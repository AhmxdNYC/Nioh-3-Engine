// crashupload.cpp - see crashupload.h.
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "version.lib")
#include "crashupload.h"
#include "inputlog.h"

#include <windows.h>
#include <bcrypt.h>
#include <winhttp.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cwchar>

namespace crashupload {
namespace {

constexpr size_t kCap = 64 * 1024;       // record cap (server cap is the same)
constexpr int kOtherLogLines = 40;      // per other mod log
constexpr int kMaxPending = 20;         // oldest dropped beyond this

void (*g_log)(const char*, ...) = nullptr;
wchar_t g_dir[MAX_PATH * 2];            // folder of the .asi, trailing backslash
wchar_t g_rootDir[MAX_PATH * 2];        // <dir>Nioh3CrashReports\ (trailing backslash)
wchar_t g_pendDir[MAX_PATH * 2];        // <root>pending\ (trailing backslash)
wchar_t g_url[512] = L"https://nioh3-mod-feedback.vercel.app/api/crash";
char g_bypass[128];
char g_id[33];
char g_ver[48];
bool g_upload = true, g_known = false;
int g_test = 0;
uintptr_t g_base = 0;
DWORD g_startTick = 0;
volatile LONG g_started = 0, g_sending = 0;
DWORD g_lastExcTick = 0;  // tick of the last record that had an exception (0 = none)

// redaction table (UTF-8), longest first; filled at Start
struct Redact { char from[300]; const char* to; size_t len; };
Redact g_red[5];
int g_redN = 0;

// facts gathered once at Start on the info thread (JSON fragments, already scrubbed)
char g_gameJson[768];
char g_modsJson[4096];
char g_loadersJson[512];
char g_osJson[320];
volatile LONG g_infoReady = 0;

void Logf(const char* fmt, ...) {
    if (!g_log) return;
    char b[700];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    g_log("%s", b);
}

char Lower(char c) { return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c; }
bool StartsI(const char* s, const char* p) {
    for (; *p; ++s, ++p) if (!*s || Lower(*s) != Lower(*p)) return false;
    return true;
}
bool IsDigit(char c) { return c >= '0' && c <= '9'; }

// ---- scrubber -------------------------------------------------------------------------------------------------------------
// in -> out (cap incl. NUL). Replaces the redaction table entries (case-insensitive), user-folder paths and Steam ids.
void Scrub(const char* in, char* out, size_t cap) {
    size_t o = 0;
    auto put = [&](const char* s) { while (*s && o + 1 < cap) out[o++] = *s++; };
    const char* p = in ? in : "";
    while (*p && o + 1 < cap) {
        bool hit = false;
        for (int i = 0; i < g_redN && !hit; ++i)
            if (g_red[i].len >= 3 && StartsI(p, g_red[i].from)) { put(g_red[i].to); p += g_red[i].len; hit = true; }
        if (hit) continue;
        // ...\Users\<name>  /Users/<name>  \home\<name>  /home/<name>  (drive letter before it dropped too)
        const char* tok = nullptr;
        if (StartsI(p, "\\users\\") || StartsI(p, "/users/")) tok = p + 7;
        else if (StartsI(p, "\\home\\") || StartsI(p, "/home/")) tok = p + 6;
        if (tok) {
            if (o >= 2 && out[o - 1] == ':' && ((out[o - 2] | 0x20) >= 'a' && (out[o - 2] | 0x20) <= 'z')) o -= 2;
            put("<user>");
            p = tok;
            while (*p && *p != '\\' && *p != '/' && *p != '"' && *p != '\'' && *p != '\r' && *p != '\n') ++p;
            continue;
        }
        // 17-digit Steam id (7656119...), not part of a longer digit run
        if (*p == '7' && (p == in || !IsDigit(p[-1])) && StartsI(p, "7656119")) {
            int n = 0;
            while (IsDigit(p[n])) ++n;
            if (n == 17) { put("<steamid>"); p += 17; continue; }
        }
        out[o++] = *p++;
    }
    out[o] = 0;
}

// ---- tiny JSON writer over a fixed buffer (no heap: usable from a crash) ---------------------------------------------------
struct J {
    char* b;
    size_t n, cap;
    bool over = false;
    void raw(const char* s) {
        size_t l = strlen(s);
        if (n + l + 1 > cap) { over = true; return; }
        memcpy(b + n, s, l);
        n += l;
        b[n] = 0;
    }
    void f(const char* fmt, ...) {
        char t[512];
        va_list ap;
        va_start(ap, fmt);
        int k = vsnprintf(t, sizeof t, fmt, ap);
        va_end(ap);
        if (k > 0) raw(t);
    }
    // scrub, then escape; invalid UTF-8 bytes become '?'
    void str(const char* s, size_t maxLen = 400) {
        static char sc[2048];
        Scrub(s, sc, maxLen + 1 < sizeof sc ? maxLen + 1 : sizeof sc);
        raw("\"");
        const unsigned char* q = (const unsigned char*)sc;
        while (*q && !over) {
            unsigned char c = *q;
            char e[8];
            if (c == '"' || c == '\\') { e[0] = '\\'; e[1] = (char)c; e[2] = 0; raw(e); ++q; continue; }
            if (c < 0x20) { snprintf(e, sizeof e, "\\u%04x", c); raw(e); ++q; continue; }
            if (c < 0x80) { e[0] = (char)c; e[1] = 0; raw(e); ++q; continue; }
            int len = (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 0;
            bool ok = len > 0;
            for (int i = 1; ok && i < len; ++i) ok = (q[i] & 0xC0) == 0x80;
            if (!ok) { raw("?"); ++q; continue; }
            memcpy(e, q, (size_t)len);
            e[len] = 0;
            raw(e);
            q += len;
        }
        raw("\"");
    }
};

// ---- small helpers --------------------------------------------------------------------------------------------------------
void ToUtf8(const wchar_t* w, char* out, int cap) {
    out[0] = 0;
    if (w && WideCharToMultiByte(CP_UTF8, 0, w, -1, out, cap, nullptr, nullptr) <= 0) out[0] = 0;
}

const char* BaseName(const char* p) {
    const char* b = p;
    for (; *p; ++p) if (*p == '\\' || *p == '/') b = p + 1;
    return b;
}

void ModuleOf(const void* addr, char* name, size_t cap, uintptr_t* off) {
    HMODULE m = nullptr;
    if (addr && GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)addr, &m) && m) {
        wchar_t w[MAX_PATH];
        DWORD k = GetModuleFileNameW(m, w, MAX_PATH);
        w[k < MAX_PATH ? k : MAX_PATH - 1] = 0;
        char u[MAX_PATH * 3];
        ToUtf8(w, u, sizeof u);
        snprintf(name, cap, "%s", BaseName(u));
        *off = (uintptr_t)addr - (uintptr_t)m;
        return;
    }
    snprintf(name, cap, "?");
    *off = (uintptr_t)addr;
}

const char* ExcName(DWORD c) {
    switch (c) {
        case 0xC0000005: return "ACCESS_VIOLATION";
        case 0xC00000FD: return "STACK_OVERFLOW";
        case 0xC0000374: return "HEAP_CORRUPTION";
        case 0xC0000409: return "STACK_BUFFER_OVERRUN";
        case 0xC000001D: return "ILLEGAL_INSTRUCTION";
        case 0xC0000094: return "INT_DIVIDE_BY_ZERO";
        case 0xC0000096: return "PRIVILEGED_INSTRUCTION";
        case 0xC0000006: return "IN_PAGE_ERROR";
        case 0xE06D7363: return "CPP_EXCEPTION";
        case 0x80000003: return "BREAKPOINT";
        default: return "";
    }
}

bool Sha256Prefix(const wchar_t* path, char* hex9, unsigned long long* size) {
    hex9[0] = 0;
    *size = 0;
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE h = nullptr;
    bool ok = false;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0 && BCryptCreateHash(alg, &h, nullptr, 0, nullptr, 0, 0) >= 0) {
        static unsigned char buf[1 << 16];
        DWORD n = 0;
        while (ReadFile(f, buf, sizeof buf, &n, nullptr) && n) { BCryptHashData(h, buf, n, 0); *size += n; }
        unsigned char d[32];
        if (BCryptFinishHash(h, d, 32, 0) >= 0) {
            snprintf(hex9, 9, "%02x%02x%02x%02x", d[0], d[1], d[2], d[3]);
            ok = true;
        }
    }
    if (h) BCryptDestroyHash(h);
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    CloseHandle(f);
    return ok;
}

// ---- facts gathered at Start ----------------------------------------------------------------------------------------------
void BuildOs() {
    HMODULE nt = GetModuleHandleW(L"ntdll.dll");
    using WineVer = const char*(__cdecl*)();
    using WineHost = void(__cdecl*)(const char**, const char**);
    WineVer wv = nt ? (WineVer)GetProcAddress(nt, "wine_get_version") : nullptr;
    WineHost wh = nt ? (WineHost)GetProcAddress(nt, "wine_get_host_version") : nullptr;
    using RtlGV = LONG(WINAPI*)(OSVERSIONINFOW*);
    RtlGV rgv = nt ? (RtlGV)GetProcAddress(nt, "RtlGetVersion") : nullptr;
    OSVERSIONINFOW vi{};
    vi.dwOSVersionInfoSize = sizeof vi;
    if (rgv) rgv(&vi);
    char t[256];
    if (wv) {
        const char* sys = nullptr;
        const char* rel = nullptr;
        if (wh) wh(&sys, &rel);
        snprintf(t, sizeof t, "Wine %s on %s %s (reports Windows %lu.%lu.%lu)", wv() ? wv() : "?", sys ? sys : "?", rel ? rel : "?",
                 vi.dwMajorVersion, vi.dwMinorVersion, vi.dwBuildNumber);
    } else {
        snprintf(t, sizeof t, "Windows %lu.%lu.%lu", vi.dwMajorVersion, vi.dwMinorVersion, vi.dwBuildNumber);
    }
    J j{g_osJson, 0, sizeof g_osJson};
    j.str(t, 240);
}

void BuildGame() {
    wchar_t exe[MAX_PATH * 2] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH * 2);
    char u[MAX_PATH * 3];
    ToUtf8(exe, u, sizeof u);
    DWORD stamp = 0, img = 0;
    if (g_base) {
        auto* dos = (IMAGE_DOS_HEADER*)g_base;
        if (dos->e_magic == IMAGE_DOS_SIGNATURE) {
            auto* nth = (IMAGE_NT_HEADERS64*)(g_base + dos->e_lfanew);
            if (nth->Signature == IMAGE_NT_SIGNATURE) { stamp = nth->FileHeader.TimeDateStamp; img = nth->OptionalHeader.SizeOfImage; }
        }
    }
    unsigned long long fsize = 0;
    WIN32_FILE_ATTRIBUTE_DATA fa{};
    if (GetFileAttributesExW(exe, GetFileExInfoStandard, &fa)) fsize = ((unsigned long long)fa.nFileSizeHigh << 32) | fa.nFileSizeLow;
    char fver[64] = "";
    DWORD dummy = 0, vsz = GetFileVersionInfoSizeW(exe, &dummy);
    static unsigned char vbuf[16384];
    if (vsz && vsz <= sizeof vbuf && GetFileVersionInfoW(exe, 0, vsz, vbuf)) {
        VS_FIXEDFILEINFO* ffi = nullptr;
        UINT l = 0;
        if (VerQueryValueW(vbuf, L"\\", (void**)&ffi, &l) && ffi)
            snprintf(fver, sizeof fver, "%u.%u.%u.%u", HIWORD(ffi->dwFileVersionMS), LOWORD(ffi->dwFileVersionMS), HIWORD(ffi->dwFileVersionLS), LOWORD(ffi->dwFileVersionLS));
    }
    J j{g_gameJson, 0, sizeof g_gameJson};
    j.raw("{\"exe\":");
    j.str(BaseName(u), 80);
    j.f(",\"stamp\":\"0x%08lX\",\"image_size\":%lu,\"file_size\":%llu,\"known\":%s,\"version\":", stamp, img, fsize, g_known ? "true" : "false");
    j.str(fver, 60);
    j.raw("}");
}

void BuildMods() {
    J j{g_modsJson, 0, sizeof g_modsJson};
    j.raw("[");
    wchar_t pat[MAX_PATH * 2];
    swprintf(pat, MAX_PATH * 2, L"%s*.asi", g_dir);
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    int count = 0;
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            if (count >= 24) break;
            wchar_t full[MAX_PATH * 2];
            swprintf(full, MAX_PATH * 2, L"%s%s", g_dir, fd.cFileName);
            char sha[9];
            unsigned long long size = 0;
            Sha256Prefix(full, sha, &size);
            char nm[MAX_PATH * 3];
            ToUtf8(fd.cFileName, nm, sizeof nm);
            char ver[64] = "";
            HMODULE m = GetModuleHandleW(fd.cFileName);
            if (_wcsicmp(fd.cFileName, L"Nioh3EngineFix.asi") == 0) snprintf(ver, sizeof ver, "%s", g_ver);
            else if (m && _wcsicmp(fd.cFileName, L"Nioh3BossRematch.asi") == 0) {
                auto fn = (const char* (*)())GetProcAddress(m, "Nioh3BossRematch_Version");
                const char* v = fn ? fn() : nullptr;
                if (v) snprintf(ver, sizeof ver, "%s", v);
            } else if (m && _wcsicmp(fd.cFileName, L"Nioh3Training.asi") == 0) {
                auto fn = (int (*)())GetProcAddress(m, "Nioh3Training_Version");
                if (fn) snprintf(ver, sizeof ver, "%d", fn());
            }
            j.raw(count ? ",{\"name\":" : "{\"name\":");
            j.str(nm, 80);
            j.f(",\"size\":%llu,\"sha\":\"%s\",\"loaded\":%s,\"ver\":", size, sha, m ? "true" : "false");
            j.str(ver, 40);
            j.raw("}");
            ++count;
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    j.raw("]");
    if (j.over) snprintf(g_modsJson, sizeof g_modsJson, "[]");

    J l{g_loadersJson, 0, sizeof g_loadersJson};
    l.raw("[");
    static const wchar_t* kProxies[] = {L"dinput8.dll", L"winmm.dll", L"version.dll", L"dsound.dll", L"d3d12.dll", L"dxgi.dll",
                                        L"winhttp.dll", L"xinput1_3.dll", L"nvngx.dll", L"OptiScaler.dll"};
    int lc = 0;
    for (const wchar_t* p : kProxies) {
        wchar_t full[MAX_PATH * 2];
        swprintf(full, MAX_PATH * 2, L"%s%s", g_dir, p);
        if (GetFileAttributesW(full) == INVALID_FILE_ATTRIBUTES) continue;
        char nm[64];
        ToUtf8(p, nm, sizeof nm);
        l.raw(lc++ ? "," : "");
        l.str(nm, 40);
    }
    l.raw("]");
}

// ---- install id -----------------------------------------------------------------------------------------------------------
bool IsHex32(const char* s) {
    int n = 0;
    for (; s[n]; ++n) if (!((s[n] >= '0' && s[n] <= '9') || (s[n] >= 'a' && s[n] <= 'f'))) return false;
    return n == 32;
}
void LoadInstallId(const wchar_t* ini) {
    wchar_t w[80] = {};
    if (ini) GetPrivateProfileStringW(L"Report", L"InstallId", L"", w, 80, ini);
    char a[80];
    ToUtf8(w, a, sizeof a);
    for (char* p = a; *p; ++p) *p = Lower(*p);
    if (IsHex32(a)) { memcpy(g_id, a, 33); return; }
    unsigned char r[16] = {};
    if (BCryptGenRandom(nullptr, r, 16, BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) {
        LARGE_INTEGER q;
        QueryPerformanceCounter(&q);
        for (int i = 0; i < 16; ++i) r[i] = (unsigned char)((q.QuadPart >> (i % 8 * 8)) ^ (GetTickCount() >> (i % 4)) ^ (i * 37));
    }
    for (int i = 0; i < 16; ++i) snprintf(g_id + i * 2, 3, "%02x", r[i]);
    if (ini) {
        wchar_t wid[40];
        MultiByteToWideChar(CP_UTF8, 0, g_id, -1, wid, 40);
        WritePrivateProfileStringW(L"Report", L"InstallId", wid, ini);
    }
}

// ---- reading log tails (crash path: fixed buffers) -------------------------------------------------------------------------
// Fills lines[] (each NUL-terminated, max 200 chars) with the last maxLines lines of dir+name. Returns the count.
int TailLines(const wchar_t* name, char (*lines)[208], int maxLines) {
    wchar_t p[MAX_PATH * 2];
    swprintf(p, MAX_PATH * 2, L"%s%s", g_dir, name);
    HANDLE f = CreateFileW(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return 0;
    static char buf[16384 + 1];
    LARGE_INTEGER sz{};
    GetFileSizeEx(f, &sz);
    LARGE_INTEGER pos;
    pos.QuadPart = sz.QuadPart > 16384 ? sz.QuadPart - 16384 : 0;
    SetFilePointerEx(f, pos, nullptr, FILE_BEGIN);
    DWORD got = 0;
    ReadFile(f, buf, 16384, &got, nullptr);
    CloseHandle(f);
    buf[got] = 0;
    // walk backwards collecting lines
    int count = 0;
    long end = (long)got;
    static int starts[512], lens[512];
    while (end > 0 && count < maxLines && count < 512) {
        long e = end;
        while (e > 0 && (buf[e - 1] == '\n' || buf[e - 1] == '\r')) --e;
        long s = e;
        while (s > 0 && buf[s - 1] != '\n') --s;
        if (s == 0 && pos.QuadPart > 0) break;  // first (partial) line of the window
        if (e > s) { starts[count] = s; lens[count] = (int)(e - s); ++count; }
        end = s;
    }
    for (int i = 0; i < count; ++i) {  // oldest first
        int k = count - 1 - i;
        int l = lens[k] < 200 ? lens[k] : 200;
        memcpy(lines[i], buf + starts[k], (size_t)l);
        lines[i][l] = 0;
    }
    return count;
}

// A jsonl event line is embedded as an object only if it is structurally sane (balanced, strings closed, plain text).
bool SaneJsonObject(const char* s) {
    size_t n = strlen(s);
    if (n < 2 || s[0] != '{' || s[n - 1] != '}') return false;
    int depth = 0;
    bool inStr = false;
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x20 || c >= 0x80) return false;
        if (inStr) {
            if (c == '\\') { if (++i >= n) return false; continue; }
            if (c == '"') inStr = false;
            continue;
        }
        if (c == '"') inStr = true;
        else if (c == '{' || c == '[') ++depth;
        else if (c == '}' || c == ']') { if (--depth < 0) return false; }
    }
    return depth == 0 && !inStr;
}

// ---- record ---------------------------------------------------------------------------------------------------------------
char g_rec[kCap + 1];
char g_ef[48][208];
char g_other[3][kOtherLogLines][208];
int g_otherN[3];
char g_events[32][208];
int g_eventsN = 0;

size_t BuildRecord(const char* reason, const EXCEPTION_RECORD* er, const CONTEXT* ctx, int efN, int otherLines) {
    J j{g_rec, 0, sizeof g_rec};
    SYSTEMTIME st;
    GetLocalTime(&st);
    char code[16] = "0x00000000", mod[96] = "", off[24] = "0x0";
    uintptr_t o = 0;
    if (er) {
        snprintf(code, sizeof code, "0x%08lX", er->ExceptionCode);
        ModuleOf(er->ExceptionAddress, mod, sizeof mod, &o);
        snprintf(off, sizeof off, "0x%llx", (unsigned long long)o);
    }
    DWORD stamp = 0, img = 0;
    if (g_base) {
        auto* dos = (IMAGE_DOS_HEADER*)g_base;
        auto* nth = (IMAGE_NT_HEADERS64*)(g_base + dos->e_lfanew);
        if (dos->e_magic == IMAGE_DOS_SIGNATURE && nth->Signature == IMAGE_NT_SIGNATURE) { stamp = nth->FileHeader.TimeDateStamp; img = nth->OptionalHeader.SizeOfImage; }
    }
    j.f("{\"schema\":1,\"t\":\"%04u-%02u-%02uT%02u:%02u:%02u\",\"mod\":\"EngineFix\",\"ver\":", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    j.str(g_ver, 40);
    j.f(",\"event\":\"crash\",\"id\":\"%s\",\"fields\":{\"code\":\"%s\",\"module\":", g_id, code);
    j.str(mod, 80);
    j.f(",\"offset\":\"%s\",\"game_stamp\":\"0x%08lX\",\"game_size\":%lu,\"reason\":", off, stamp, img);
    j.str(reason ? reason : "", 120);
    j.raw("}");
    j.raw(",\"exc_name\":");
    j.str(er ? ExcName(er->ExceptionCode) : "", 40);
    if (er && (er->ExceptionCode == 0xC0000005 || er->ExceptionCode == 0xC0000006) && er->NumberParameters >= 2) {
        const char* k = er->ExceptionInformation[0] == 0 ? "read" : er->ExceptionInformation[0] == 1 ? "write" : "execute";
        j.f(",\"access\":\"%s 0x%llx\"", k, (unsigned long long)er->ExceptionInformation[1]);
    }
    j.raw(",\"stack\":[");
    if (ctx) {
        CONTEXT c = *ctx;
        for (int i = 0; i < 24 && c.Rip; ++i) {
            char nm[96];
            uintptr_t fo = 0;
            ModuleOf((void*)c.Rip, nm, sizeof nm, &fo);
            char fr[160];
            snprintf(fr, sizeof fr, "%s+0x%llx", nm, (unsigned long long)fo);
            if (i) j.raw(",");
            j.str(fr, 150);
            DWORD64 imgBase = 0;
            RUNTIME_FUNCTION* rf = RtlLookupFunctionEntry(c.Rip, &imgBase, nullptr);
            if (!rf) {
                if (!c.Rsp || IsBadReadPtr((void*)c.Rsp, 8)) break;
                c.Rip = *(DWORD64*)c.Rsp;
                c.Rsp += 8;
            } else {
                void* hd = nullptr;
                DWORD64 ef = 0;
                RtlVirtualUnwind(UNW_FLAG_NHANDLER, imgBase, c.Rip, rf, &c, &hd, &ef, nullptr);
            }
            if (c.Rip < 0x10000) break;
        }
    }
    j.raw("]");
    const bool info = g_infoReady != 0;
    j.raw(",\"game\":");
    j.raw(info && g_gameJson[0] ? g_gameJson : "{}");
    j.raw(",\"mods\":");
    j.raw(info && g_modsJson[0] ? g_modsJson : "[]");
    j.raw(",\"loaders\":");
    j.raw(info && g_loadersJson[0] ? g_loadersJson : "[]");
    j.raw(",\"os\":");
    j.raw(info && g_osJson[0] ? g_osJson : "\"\"");
    j.f(",\"uptime_s\":%lu,\"test\":%d", (unsigned long)((GetTickCount() - g_startTick) / 1000), g_test);
    j.raw(",\"last\":[");
    for (int i = 0, w = 0; i < g_eventsN; ++i) {
        static char sc[260];
        Scrub(g_events[i], sc, sizeof sc);
        if (!SaneJsonObject(sc)) continue;
        if (w++) j.raw(",");
        j.raw(sc);
    }
    j.raw("],\"input\":[");
    {
        static char tail[20 * 1024];
        inputlog::WriteTail(tail, (int)sizeof tail, inputlog::kCrashTail);
        if (tail[0]) j.raw(tail);
    }
    j.raw("]");
    j.raw(",\"log\":{\"EngineFix\":[");
    for (int i = 0; i < efN; ++i) { if (i) j.raw(","); j.str(g_ef[i], 200); }
    j.raw("]");
    static const char* kNames[3] = {"BossRematch", "Training", "CamKiProbe"};
    for (int m = 0; m < 3; ++m) {
        if (!g_otherN[m]) continue;
        j.f(",\"%s\":[", kNames[m]);
        int n = g_otherN[m], first = n > otherLines ? n - otherLines : 0;
        for (int i = first; i < n; ++i) { if (i > first) j.raw(","); j.str(g_other[m][i], 200); }
        j.raw("]");
    }
    j.raw("}}");
    return j.over ? 0 : j.n;
}

bool WriteAll(const wchar_t* path, const char* data, size_t n) {
    HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD w = 0;
    BOOL ok = WriteFile(f, data, (DWORD)n, &w, nullptr) && w == n;
    FlushFileBuffers(f);
    CloseHandle(f);
    return ok != FALSE;
}

void EnsureDirs() {
    CreateDirectoryW(g_rootDir, nullptr);
    CreateDirectoryW(g_pendDir, nullptr);
}

// ---- sending --------------------------------------------------------------------------------------------------------------
// POST body to g_url. Returns the HTTP status (0 = network failure).
int Post(const char* body, size_t n, DWORD timeoutMs) {
    URL_COMPONENTS u{};
    u.dwStructSize = sizeof u;
    wchar_t host[256] = {}, path[1024] = {};
    u.lpszHostName = host;
    u.dwHostNameLength = 256;
    u.lpszUrlPath = path;
    u.dwUrlPathLength = 1024;
    if (!WinHttpCrackUrl(g_url, 0, 0, &u)) return 0;
    int status = 0;
    HINTERNET s = WinHttpOpen(L"Nioh3EngineFix-crash/1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET c = nullptr, r = nullptr;
    if (s) {
        WinHttpSetTimeouts(s, (int)timeoutMs, (int)timeoutMs, (int)timeoutMs, (int)timeoutMs);
        c = WinHttpConnect(s, host, u.nPort, 0);
    }
    if (c) r = WinHttpOpenRequest(c, L"POST", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                  u.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
    if (r) {
        wchar_t hdr[400];
        if (g_bypass[0]) swprintf(hdr, 400, L"Content-Type: application/json\r\nx-vercel-protection-bypass: %hs\r\n", g_bypass);
        else swprintf(hdr, 400, L"Content-Type: application/json\r\n");
        if (WinHttpSendRequest(r, hdr, (DWORD)-1L, (void*)body, (DWORD)n, (DWORD)n, 0) && WinHttpReceiveResponse(r, nullptr)) {
            DWORD code = 0, sz = sizeof code;
            if (WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &code, &sz, WINHTTP_NO_HEADER_INDEX))
                status = (int)code;
            char sink[512];
            DWORD got = 0;
            while (WinHttpReadData(r, sink, sizeof sink, &got) && got) {}
        }
    }
    if (r) WinHttpCloseHandle(r);
    if (c) WinHttpCloseHandle(c);
    if (s) WinHttpCloseHandle(s);
    return status;
}

// Sends one pending file. true = finished with it (sent or rejected), false = keep for a retry.
bool SendFile(const wchar_t* full, const wchar_t* name, DWORD timeoutMs, bool* accepted) {
    *accepted = false;
    HANDLE f = CreateFileW(full, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    static char body[kCap + 1];
    DWORD got = 0;
    ReadFile(f, body, kCap, &got, nullptr);
    CloseHandle(f);
    if (!got) { DeleteFileW(full); return true; }
    const int st = Post(body, got, timeoutMs);
    if (st >= 200 && st < 300) {
        DeleteFileW(full);
        *accepted = true;
        Logf("CRASHUPLOAD: sent %ls (HTTP %d)", name, st);
        return true;
    }
    if (st >= 400 && st < 500 && st != 401 && st != 403 && st != 408 && st != 429) {  // the server will never take it
        wchar_t rej[MAX_PATH * 2], dst[MAX_PATH * 2];
        swprintf(rej, MAX_PATH * 2, L"%srejected", g_rootDir);
        CreateDirectoryW(rej, nullptr);
        swprintf(dst, MAX_PATH * 2, L"%s\\%s", rej, name);
        MoveFileExW(full, dst, MOVEFILE_REPLACE_EXISTING);
        Logf("CRASHUPLOAD: %ls rejected (HTTP %d), moved to rejected\\", name, st);
        return true;
    }
    Logf("CRASHUPLOAD: %ls not sent (HTTP %d), kept for retry", name, st);
    return false;
}

int SendAll(DWORD timeoutMs) {
    if (!g_upload || InterlockedCompareExchange(&g_sending, 1, 0) != 0) return 0;
    wchar_t pat[MAX_PATH * 2];
    swprintf(pat, MAX_PATH * 2, L"%scrash-*.json", g_pendDir);
    WIN32_FIND_DATAW fd;
    int ok = 0, tries = 0;
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            wchar_t full[MAX_PATH * 2];
            swprintf(full, MAX_PATH * 2, L"%s%s", g_pendDir, fd.cFileName);
            bool acc = false;
            const bool done = SendFile(full, fd.cFileName, timeoutMs, &acc);
            if (acc) ++ok;
            if (!done) break;          // offline or server down: stop this round, retry later
            if (++tries >= 10) break;  // a few per round
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    InterlockedExchange(&g_sending, 0);
    return ok;
}

int CountPending(bool prune) {
    wchar_t pat[MAX_PATH * 2];
    swprintf(pat, MAX_PATH * 2, L"%scrash-*.json", g_pendDir);
    WIN32_FIND_DATAW fd;
    int n = 0;
    wchar_t oldest[MAX_PATH] = {};
    FILETIME ot{0xFFFFFFFF, 0xFFFFFFFF};
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            ++n;
            if (CompareFileTime(&fd.ftLastWriteTime, &ot) < 0) { ot = fd.ftLastWriteTime; wcsncpy_s(oldest, fd.cFileName, _TRUNCATE); }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    if (prune && n > kMaxPending && oldest[0]) {
        wchar_t full[MAX_PATH * 2];
        swprintf(full, MAX_PATH * 2, L"%s%s", g_pendDir, oldest);
        DeleteFileW(full);
        return CountPending(true);
    }
    return n;
}

int g_delayMs = 20000;

DWORD WINAPI InfoThread(LPVOID) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    Sleep(3000);  // let the other mods finish loading so their version exports answer
    BuildOs();
    BuildGame();
    BuildMods();
    InterlockedExchange(&g_infoReady, 1);
    return 0;
}

DWORD WINAPI SenderThread(LPVOID) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
    Sleep((DWORD)g_delayMs);
    const int start = CountPending(true);
    if (!start) { Logf("CRASHUPLOAD: nothing pending"); return 0; }
    Logf("CRASHUPLOAD: %d crash record(s) pending, sending", start);
    for (int round = 0;; ++round) {
        SendAll(10000);
        if (!CountPending(false)) { Logf("CRASHUPLOAD: pending queue empty"); return 0; }
        Sleep(round < 10 ? 60000 : 300000);
    }
}

void OnCrashImpl(const char* reason, const EXCEPTION_RECORD* er, const CONTEXT* ctx, const char* const* ring, int ringN, unsigned budget) {
    // The engine's own "Fatal error" popup (or abort) right after an exception we already recorded is the same crash: one record.
    if (!er && g_lastExcTick && GetTickCount() - g_lastExcTick < 120000) {
        Logf("CRASHUPLOAD: follow-up (%.60s) of the recorded crash, no second record", reason ? reason : "");
        return;
    }
    if (er) g_lastExcTick = GetTickCount() | 1;
    int efN = 0;
    for (int i = 0; ring && i < ringN && efN < 48; ++i) {
        if (!ring[i] || !ring[i][0]) continue;
        snprintf(g_ef[efN], sizeof g_ef[efN], "%s", ring[i]);
        size_t l = strlen(g_ef[efN]);
        while (l && (g_ef[efN][l - 1] == '\n' || g_ef[efN][l - 1] == '\r' || g_ef[efN][l - 1] == ' ')) g_ef[efN][--l] = 0;
        ++efN;
    }
    static const wchar_t* kLogs[3] = {L"Nioh3BossRematch.log", L"Nioh3Training.log", L"Nioh3CamKiProbe.log"};
    for (int m = 0; m < 3; ++m) g_otherN[m] = TailLines(kLogs[m], g_other[m], kOtherLogLines);
    g_eventsN = TailLines(L"Nioh3ModEvents.jsonl", g_events, 32);
    size_t n = 0;
    static const int kTry[3] = {kOtherLogLines, 15, 0};
    for (int lines : kTry) {
        n = BuildRecord(reason, er, ctx, efN, lines);
        if (n) break;
    }
    if (!n) n = BuildRecord(reason, er, ctx, 0, 0);
    if (!n) return;
    EnsureDirs();
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t name[96], full[MAX_PATH * 2], tmp[MAX_PATH * 2];
    swprintf(name, 96, L"crash-%04u%02u%02u-%02u%02u%02u-%04lx.json", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
             (unsigned long)(GetTickCount() & 0xFFFF));
    swprintf(full, MAX_PATH * 2, L"%s%s", g_pendDir, name);
    swprintf(tmp, MAX_PATH * 2, L"%s%s.tmp", g_rootDir, name);
    if (!WriteAll(tmp, g_rec, n) || !MoveFileExW(tmp, full, MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileW(tmp);
        Logf("CRASHUPLOAD: could not write the crash record");
        return;
    }
    Logf("CRASHUPLOAD: crash record %ls (%u bytes)", name, (unsigned)n);
    if (budget && g_upload) {
        bool acc = false;
        SendFile(full, name, budget, &acc);
    }
}

}  // namespace

void Start(const StartInfo& info) {
    if (InterlockedExchange(&g_started, 1)) return;
    g_log = info.log;
    g_base = info.base;
    g_known = info.knownBuild;
    g_test = info.testMode;
    g_startTick = GetTickCount();
    snprintf(g_ver, sizeof g_ver, "%s", info.modVersion ? info.modVersion : "");
    DWORD n = GetModuleFileNameW((HMODULE)info.selfModule, g_dir, MAX_PATH * 2);
    while (n && g_dir[n - 1] != L'\\' && g_dir[n - 1] != L'/') --n;
    g_dir[n] = 0;
    swprintf(g_rootDir, MAX_PATH * 2, L"%sNioh3CrashReports\\", g_dir);
    swprintf(g_pendDir, MAX_PATH * 2, L"%spending\\", g_rootDir);
    g_delayMs = info.startDelayMs;
    if (info.iniPath) {
        g_upload = GetPrivateProfileIntW(L"Report", L"CrashUpload", 1, info.iniPath) != 0;
        wchar_t w[512] = {};
        GetPrivateProfileStringW(L"Report", L"CrashUrl", L"", w, 512, info.iniPath);
        if (w[0]) wcsncpy_s(g_url, w, _TRUNCATE);
        wchar_t b[128] = {};
        GetPrivateProfileStringW(L"Report", L"CrashBypass", L"", b, 128, info.iniPath);
        ToUtf8(b, g_bypass, sizeof g_bypass);
        g_delayMs = (int)GetPrivateProfileIntW(L"Report", L"CrashSendDelayMs", (UINT)info.startDelayMs, info.iniPath);
        if (g_delayMs < 0) g_delayMs = 0;
    }
    EnsureInstallId(info.iniPath);
    {  // redaction table, longest first
        struct { const wchar_t* env; const char* to; } src[3] = {{L"USERPROFILE", "<user>"}, {L"USERNAME", "<user>"}, {L"COMPUTERNAME", "<pc>"}};
        g_redN = 0;
        wchar_t gd[MAX_PATH * 2];
        wcsncpy_s(gd, g_dir, _TRUNCATE);
        size_t gl = wcslen(gd);
        if (gl && (gd[gl - 1] == L'\\' || gd[gl - 1] == L'/')) gd[gl - 1] = 0;
        ToUtf8(gd, g_red[g_redN].from, sizeof g_red[0].from);
        g_red[g_redN].to = "<game>";
        g_red[g_redN].len = strlen(g_red[g_redN].from);
        ++g_redN;
        for (auto& s : src) {
            wchar_t v[300] = {};
            DWORD l = GetEnvironmentVariableW(s.env, v, 300);
            if (!l || l >= 300) continue;
            ToUtf8(v, g_red[g_redN].from, sizeof g_red[0].from);
            g_red[g_redN].to = s.to;
            g_red[g_redN].len = strlen(g_red[g_redN].from);
            if (g_red[g_redN].len >= 3) ++g_redN;
        }
        for (int i = 0; i < g_redN; ++i)  // longest first (the profile path before the bare name)
            for (int k = i + 1; k < g_redN; ++k)
                if (g_red[k].len > g_red[i].len) { Redact t = g_red[i]; g_red[i] = g_red[k]; g_red[k] = t; }
    }
    if (HANDLE h = CreateThread(nullptr, 128 * 1024, InfoThread, nullptr, 0, nullptr)) CloseHandle(h);
    if (HANDLE h = CreateThread(nullptr, 128 * 1024, SenderThread, nullptr, 0, nullptr)) CloseHandle(h);
    char u[600];
    ToUtf8(g_url, u, sizeof u);
    Logf("CRASHUPLOAD: armed (upload %d, url %s, bypass %s, id %.8s..., send scan in %d ms)", g_upload ? 1 : 0, u, g_bypass[0] ? "set" : "none", g_id, g_delayMs);
}

void OnCrash(const char* reason, const _EXCEPTION_RECORD* er, const _CONTEXT* ctx, const char* const* logRing, int ringLines, unsigned sendBudgetMs) {
    if (!g_started) return;
    __try {
        OnCrashImpl(reason, (const EXCEPTION_RECORD*)er, (const CONTEXT*)ctx, logRing, ringLines, sendBudgetMs);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

int SendPendingNow() { return SendAll(10000); }
int PendingCount() { return CountPending(false); }
void ScrubForTest(const char* in, char* out, unsigned cap) { Scrub(in, out, cap); }
const wchar_t* PendingDir() { return g_pendDir; }
const char* InstallId() { return g_id; }
const char* EnsureInstallId(const wchar_t* iniPath) {
    static SRWLOCK lock = SRWLOCK_INIT;
    AcquireSRWLockExclusive(&lock);
    if (!g_id[0]) LoadInstallId(iniPath);
    ReleaseSRWLockExclusive(&lock);
    return g_id;
}

}  // namespace crashupload
