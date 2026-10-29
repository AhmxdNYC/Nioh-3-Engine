// crashreport.cpp - see crashreport.h.
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "advapi32.lib")
#include "crashreport.h"

#include <windows.h>
#include <bcrypt.h>
#include <psapi.h>
#include <shellapi.h>
#include <dxgi.h>
#include <intrin.h>
#include <cwchar>
#include <stdlib.h>
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>

namespace crashreport {
namespace {

constexpr int kStateSlots = 32, kKeyLen = 24, kValLen = 96, kLogLines = 48, kLogLen = 200;

struct StateSlot { char key[kKeyLen]; char val[kValLen]; };
StateSlot g_state[kStateSlots];
volatile LONG g_stateUsed = 0;
char g_logRing[kLogLines][kLogLen];
volatile LONG g_logHead = 0;

void (*g_log)(const char*, ...) = nullptr;
HMODULE g_self = nullptr;
uintptr_t g_base = 0;
bool g_known = false, g_dump = false;
wchar_t g_dir[MAX_PATH * 2];
char g_shaText[1536];      // "Nioh3X.asi sha256 ... size ..." lines, computed once at Start
DWORD g_startTick = 0;
char g_fatalTitle[160];
char g_url[300] = "https://nioh3-mod-feedback.vercel.app";  // [Report] Url (placeholder: the feedback site)
bool g_urlOpen = false;
char g_lastSnapshot[MAX_PATH * 2];

// hand-off to the helper thread (own stack): filled by the faulting thread, consumed by Writer()
HANDLE g_evGo = nullptr, g_evDone = nullptr;
struct Req {
    char reason[160];
    EXCEPTION_POINTERS ep;      // pointers only valid while the faulting thread waits
    bool hasEp = false;
    bool snapshot = false;      // menu Report row: files are Nioh3EngineFix-report-*, title says so
    DWORD tid = 0;
    EXCEPTION_RECORD erCopy;
    CONTEXT ctxCopy;
} g_req;
volatile LONG g_inReport = 0;
volatile LONG g_written = 0;
LPTOP_LEVEL_EXCEPTION_FILTER g_prevFilter = nullptr;

void CopyTrunc(char* dst, size_t cap, const char* src) {
    size_t i = 0;
    for (; src && src[i] && i + 1 < cap; ++i) dst[i] = src[i] == '\n' || src[i] == '\r' ? ' ' : src[i];
    dst[i] = 0;
}

std::string Sha256File(const wchar_t* path, unsigned long long* sizeOut) {
    std::string out;
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return out;
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE h = nullptr;
    unsigned long long total = 0;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0 && BCryptCreateHash(alg, &h, nullptr, 0, nullptr, 0, 0) >= 0) {
        static unsigned char buf[1 << 16];
        DWORD n = 0;
        while (ReadFile(f, buf, sizeof(buf), &n, nullptr) && n) { BCryptHashData(h, buf, n, 0); total += n; }
        unsigned char dig[32];
        if (BCryptFinishHash(h, dig, 32, 0) >= 0) {
            char hex[65];
            for (int i = 0; i < 32; ++i) snprintf(hex + i * 2, 3, "%02x", dig[i]);
            out = hex;
        }
    }
    if (h) BCryptDestroyHash(h);
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    CloseHandle(f);
    if (sizeOut) *sizeOut = total;
    return out;
}

void ComputeShas() {  // Start-time, never from the crash path
    g_shaText[0] = 0;
    static const wchar_t* kNames[] = {L"Nioh3EngineFix.asi", L"Nioh3Training.asi", L"Nioh3BossRematch.asi"};
    size_t used = 0;
    for (const wchar_t* nm : kNames) {
        std::wstring p = std::wstring(g_dir) + nm;
        unsigned long long sz = 0;
        std::string sha = Sha256File(p.c_str(), &sz);
        if (sha.empty()) continue;
        int n = snprintf(g_shaText + used, sizeof(g_shaText) - used, "  %ls  sha256 %s  (%llu bytes)\n", nm, sha.c_str(), sz);
        if (n > 0) used += (size_t)n;
        if (used + 200 >= sizeof(g_shaText)) break;
    }
}

const char* ModuleOf(void* addr, uintptr_t* offOut, char* name, size_t cap) {
    HMODULE m = nullptr;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)addr, &m) && m) {
        wchar_t w[MAX_PATH];
        DWORD n = GetModuleFileNameW(m, w, MAX_PATH);
        const wchar_t* b = w;
        for (DWORD i = 0; i < n; ++i) if (w[i] == L'\\' || w[i] == L'/') b = w + i + 1;
        snprintf(name, cap, "%ls", b);
        *offOut = (uintptr_t)addr - (uintptr_t)m;
        return name;
    }
    snprintf(name, cap, "?");
    *offOut = (uintptr_t)addr;
    return name;
}

struct Out {
    HANDLE f = INVALID_HANDLE_VALUE;
    void P(const char* fmt, ...) {
        char b[1024];
        va_list ap;
        va_start(ap, fmt);
        int n = vsnprintf(b, sizeof(b), fmt, ap);
        va_end(ap);
        if (n <= 0) return;
        if (n >= (int)sizeof(b)) n = sizeof(b) - 1;
        DWORD w = 0;
        WriteFile(f, b, (DWORD)n, &w, nullptr);
    }
    void W(const char* d, size_t n) { DWORD w = 0; if (n) WriteFile(f, d, (DWORD)n, &w, nullptr); }
};

const char* ExcName(DWORD c) {
    switch (c) {
        case 0xC0000005: return "ACCESS_VIOLATION";
        case 0xC00000FD: return "STACK_OVERFLOW";
        case 0xC0000374: return "HEAP_CORRUPTION";
        case 0xC0000409: return "STACK_BUFFER_OVERRUN/FAIL_FAST";
        case 0xC000001D: return "ILLEGAL_INSTRUCTION";
        case 0xC0000094: return "INT_DIVIDE_BY_ZERO";
        case 0xC0000096: return "PRIVILEGED_INSTRUCTION";
        case 0xC0000006: return "IN_PAGE_ERROR";
        case 0xC000001E: return "INVALID_LOCK_SEQUENCE";
        case 0xE06D7363: return "C++_EXCEPTION";
        case 0x80000003: return "BREAKPOINT";
        default: return "";
    }
}

// ---- report extras (Linux test #ab2e42d4): system specs, mod / Training / Boss Rematch settings, summoned bosses -------------
// Specs port the Boss Rematch specs.txt (pc/report-specs 96611268): no user/computer name, paths, serials or network ids.
wchar_t g_iniPath[MAX_PATH * 2];
char g_redact[3][260];             // USERNAME / COMPUTERNAME / USERPROFILE (UTF-8), replaced by <redacted> in copied settings
char g_specs[4096];
volatile LONG g_specsReady = 0;

std::string Utf8(const wchar_t* w) {
    char b[1024];
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, b, sizeof(b), nullptr, nullptr);
    return n > 0 ? std::string(b) : std::string();
}
std::string Specs() {
    std::string o;
    HMODULE nt = GetModuleHandleW(L"ntdll.dll");
    using WineVer = const char*(__cdecl*)();
    using WineHost = void(__cdecl*)(const char**, const char**);
    WineVer wv = nt ? (WineVer)GetProcAddress(nt, "wine_get_version") : nullptr;
    WineHost wh = nt ? (WineHost)GetProcAddress(nt, "wine_get_host_version") : nullptr;
    if (wv) {
        const char* v = wv();
        o += "  OS family:     Linux / SteamOS (Wine/Proton)\r\n  Wine version:  " + std::string(v ? v : "unknown") + "\r\n";
        if (wh) { const char* sys = nullptr; const char* rel = nullptr; wh(&sys, &rel); o += "  Host kernel:   " + std::string(sys ? sys : "?") + " " + (rel ? rel : "?") + "\r\n"; }
    } else {
        o += "  OS family:     Windows\r\n";
    }
    using RtlGV = LONG(WINAPI*)(OSVERSIONINFOW*);
    RtlGV rgv = nt ? (RtlGV)GetProcAddress(nt, "RtlGetVersion") : nullptr;
    OSVERSIONINFOW vi{};
    vi.dwOSVersionInfoSize = sizeof(vi);
    if (rgv && rgv(&vi) == 0) o += "  Windows build: " + std::to_string(vi.dwMajorVersion) + "." + std::to_string(vi.dwMinorVersion) + "." + std::to_string(vi.dwBuildNumber) + " (reported)\r\n";
    {
        wchar_t prod[128] = {}, disp[64] = {};
        DWORD n1 = sizeof(prod), n2 = sizeof(disp);
        RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"ProductName", RRF_RT_REG_SZ, nullptr, prod, &n1);
        RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"DisplayVersion", RRF_RT_REG_SZ, nullptr, disp, &n2);
        std::string pn = Utf8(prod);
        const size_t w10 = pn.find("Windows 10");
        if (w10 != std::string::npos && vi.dwBuildNumber >= 22000) pn.replace(w10, 10, "Windows 11");  // registry still says 10 on 11
        if (prod[0]) o += "  Product:       " + pn + (disp[0] ? " " + Utf8(disp) : std::string()) + "\r\n";
    }
    int r[4] = {0};
    char brand[49] = {0};
    __cpuid(r, 0x80000000);
    if ((unsigned)r[0] >= 0x80000004u) for (int i = 0; i < 3; i++) { __cpuid(r, 0x80000002 + i); memcpy(brand + 16 * i, r, 16); }
    std::string b(brand);
    size_t a = b.find_first_not_of(' ');
    b = a == std::string::npos ? "unknown" : b.substr(a);
    while (!b.empty() && b.back() == ' ') b.pop_back();
    SYSTEM_INFO si{};
    GetNativeSystemInfo(&si);
    o += "  CPU:           " + b + " (" + std::to_string(si.dwNumberOfProcessors) + " logical cores)\r\n";
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) o += "  RAM:           " + std::to_string(ms.ullTotalPhys >> 20) + " MiB total, " + std::to_string(ms.ullAvailPhys >> 20) + " MiB available\r\n";
    int gpus = 0;
    if (HMODULE dx = LoadLibraryW(L"dxgi.dll")) {
        using CF = HRESULT(WINAPI*)(REFIID, void**);
        CF cf = (CF)GetProcAddress(dx, "CreateDXGIFactory1");
        IDXGIFactory1* f = nullptr;
        if (cf && SUCCEEDED(cf(__uuidof(IDXGIFactory1), (void**)&f)) && f) {
            IDXGIAdapter1* ad = nullptr;
            for (UINT i = 0; i < 8 && f->EnumAdapters1(i, &ad) == S_OK; i++) {
                DXGI_ADAPTER_DESC1 d{};
                ad->GetDesc1(&d);
                if (!(d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
                    char ids[64];
                    snprintf(ids, sizeof(ids), "vendor 0x%04X device 0x%04X", d.VendorId, d.DeviceId);
                    o += "  GPU " + std::to_string(++gpus) + ":         " + Utf8(d.Description) + " (" + ids + "), " + std::to_string((unsigned long long)d.DedicatedVideoMemory >> 20) + " MiB VRAM\r\n";
                    LARGE_INTEGER drv{};
                    if (SUCCEEDED(ad->CheckInterfaceSupport(__uuidof(IDXGIDevice), &drv)))
                        o += "    driver:      " + std::to_string(HIWORD(drv.HighPart)) + "." + std::to_string(LOWORD(drv.HighPart)) + "." + std::to_string(HIWORD(drv.LowPart)) + "." + std::to_string(LOWORD(drv.LowPart)) + "\r\n";
                }
                ad->Release();
            }
            f->Release();
        }
        // dxgi.dll stays loaded (the game uses it anyway); no FreeLibrary from a report thread
    }
    if (!gpus) o += "  GPU:           unavailable\r\n";
    o += "  Display:       " + std::to_string(GetSystemMetrics(SM_CXSCREEN)) + "x" + std::to_string(GetSystemMetrics(SM_CYSCREEN)) + "\r\n";
    return o;
}
DWORD WINAPI SpecsThread(LPVOID) {  // cache for crash reports (never enumerate DXGI from a crash path)
    Sleep(20000);
    const std::string s = Specs();
    snprintf(g_specs, sizeof(g_specs), "%s", s.c_str());
    InterlockedExchange(&g_specsReady, 1);
    return 0;
}

// Training / Boss Rematch state through their exports. SEH-guarded: plain C types only in this function.
void TrainingState(char* out, size_t cap) {
    out[0] = 0;
    size_t used = 0;
    auto add = [&](const char* fmt, auto... args) {
        if (used + 1 >= cap) return;
        int n = snprintf(out + used, cap - used, fmt, args...);
        if (n > 0) used += (size_t)n < cap - used ? (size_t)n : cap - used - 1;
    };
    HMODULE tr = GetModuleHandleW(L"Nioh3Training.asi");
    HMODULE br = GetModuleHandleW(L"Nioh3BossRematch.asi");
    if (!tr) add("  Nioh3Training.asi: not loaded\r\n");
    if (tr) {
        auto ver = (int (*)())GetProcAddress(tr, "Nioh3Training_Version");
        auto mode = (int (*)())GetProcAddress(tr, "Nioh3Training_SessionMode");
        auto act = (int (*)())GetProcAddress(tr, "Nioh3Training_SessionActive");
        auto cnt = (int (*)())GetProcAddress(tr, "Nioh3Training_SlotCount");
        auto chr = (uint32_t (*)(int))GetProcAddress(tr, "Nioh3Training_SlotChr");
        auto name = (int (*)(uint32_t, char*, int))GetProcAddress(tr, "Nioh3Training_BossName");
        auto vit = (int (*)(int, uint64_t*, uint64_t*, float*, float*, int*))GetProcAddress(tr, "Nioh3Training_BossVitalsGet");
        __try {
            const int m = mode ? mode() : -1, a = act ? act() : -1;
            add("  Training version %d, session %s, active bits %d (1 session, 2 summoned boss alive)\r\n", ver ? ver() : -1,
                m == 1 ? "Boss Match" : m == 2 ? "Practice" : m == 0 ? "none" : "?", a);
            const int n = cnt ? cnt() : 0;
            int alive = 0;
            for (int s = 0; s < n && s < 8; ++s) {
                const uint32_t c = chr ? chr(s) : 0;
                if (!c) continue;
                ++alive;
                char nm[96] = "?";
                if (name) name(c, nm, (int)sizeof(nm));
                uint64_t hc = 0, hm = 0;
                float kc = 0, km = 0;
                int cfg[4] = {0, 0, 100, 100};
                const int live = vit ? vit(s, &hc, &hm, &kc, &km, cfg) : -2;
                add("  summoned boss %c: %s (chr 0x%08X) HP %llu/%llu Ki %.0f/%.0f live %d, infHP %d infKi %d HP%% %d Ki%% %d\r\n", 'A' + s, nm, c,
                    (unsigned long long)hc, (unsigned long long)hm, kc, km, live, cfg[0], cfg[1], cfg[2], cfg[3]);
            }
            if (!alive) add("  summoned bosses: none (%d slots)\r\n", n);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            add("  (Training state read faulted)\r\n");
        }
    }
    if (!br) add("  Nioh3BossRematch.asi: not loaded\r\n");
    if (br) {
        auto ver = (const char* (*)())GetProcAddress(br, "Nioh3BossRematch_Version");
        auto inm = (int (*)())GetProcAddress(br, "Nioh3BossRematch_InMatch");
        __try {
            const char* v = ver ? ver() : nullptr;
            add("  Boss Rematch version %s, in match %d\r\n", v ? v : "?", inm ? inm() : -1);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            add("  (Boss Rematch state read faulted)\r\n");
        }
    }
}

// Settings file without comments / blank lines; user / computer names and the profile path are redacted.
std::string SettingsText(const wchar_t* path) {
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return "  (missing)\r\n";
    std::string raw(512 * 1024, 0);
    DWORD n = 0;
    ReadFile(f, raw.data(), (DWORD)raw.size(), &n, nullptr);
    CloseHandle(f);
    raw.resize(n);
    if (raw.size() >= 2 && (unsigned char)raw[0] == 0xFF && (unsigned char)raw[1] == 0xFE) {  // UTF-16 ini
        std::wstring w((const wchar_t*)(raw.data() + 2), (raw.size() - 2) / 2);
        int m = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
        std::string u((size_t)m, 0);
        WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), u.data(), m, nullptr, nullptr);
        raw = u;
    }
    std::string out;
    size_t pos = 0, lines = 0;
    while (pos < raw.size()) {
        size_t e = raw.find('\n', pos);
        if (e == std::string::npos) e = raw.size();
        std::string l = raw.substr(pos, e - pos);
        pos = e + 1;
        while (!l.empty() && (l.back() == '\r' || l.back() == ' ' || l.back() == '\t')) l.pop_back();
        size_t a = l.find_first_not_of(" \t\xEF\xBB\xBF");
        if (a == std::string::npos || l[a] == ';' || l[a] == '#') continue;
        for (const char* id : g_redact) {
            if (!id[0] || strlen(id) < 3) continue;
            for (size_t k; (k = l.find(id)) != std::string::npos;) l.replace(k, strlen(id), "<redacted>");
        }
        out += "  " + l.substr(a) + "\r\n";
        if (++lines >= 4000) { out += "  (truncated)\r\n"; break; }
    }
    return out.empty() ? "  (empty)\r\n" : out;
}

void WriteReport(const char* reason, const EXCEPTION_RECORD* er, const CONTEXT* ctx, DWORD faultTid, bool snapshot) {
    const wchar_t* kind = snapshot ? L"report" : L"crash";
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t path[MAX_PATH * 2], latest[MAX_PATH * 2];
    swprintf(path, MAX_PATH * 2, L"%sNioh3EngineFix-%s-%04u%02u%02u-%02u%02u%02u.txt", g_dir, kind, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    swprintf(latest, MAX_PATH * 2, L"%sNioh3EngineFix-%s-latest.txt", g_dir, kind);
    Out o;
    o.f = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (o.f == INVALID_HANDLE_VALUE) return;

    o.P(snapshot ? "Nioh 3 mod state snapshot (menu Report)\r\n======================================\r\n" : "Nioh 3 mod crash report\r\n=======================\r\n");
    o.P("time:      %04u-%02u-%02u %02u:%02u:%02u (local)\r\n", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    o.P("reason:    %s\r\n", reason);
    o.P("uptime:    %lu s since mod start\r\n", (unsigned long)((GetTickCount() - g_startTick) / 1000));
    o.P("process:   pid %lu, crashing thread %lu\r\n", GetCurrentProcessId(), faultTid);
    o.P("exe base:  0x%llx, known build (v2.02): %s\r\n", (unsigned long long)g_base, g_known ? "yes" : "NO");
    o.P("\r\n-- mod build --\r\n%s", g_shaText[0] ? g_shaText : "  (no sha available)\r\n");
    {
        o.P("\r\n-- system --\r\n");
        // Snapshot (menu Report): fresh specs on this helper thread. Crash: only the cached copy (no DXGI from a crash path).
        std::string s = snapshot ? Specs() : std::string(g_specsReady ? g_specs : "  (not collected yet)\r\n");
        o.W(s.data(), s.size());
        char tr[2048];
        TrainingState(tr, sizeof(tr));
        o.P("\r\n-- Training session / summoned bosses --\r\n");
        o.W(tr, strlen(tr));
    }

    if (er) {
        char nm[64];
        uintptr_t off = 0;
        ModuleOf(er->ExceptionAddress, &off, nm, sizeof(nm));
        o.P("\r\n-- exception --\r\ncode:      0x%08lX %s\r\n", er->ExceptionCode, ExcName(er->ExceptionCode));
        o.P("address:   0x%llx = %s+0x%llx\r\n", (unsigned long long)(uintptr_t)er->ExceptionAddress, nm, (unsigned long long)off);
        if ((er->ExceptionCode == 0xC0000005 || er->ExceptionCode == 0xC0000006) && er->NumberParameters >= 2) {
            const char* k = er->ExceptionInformation[0] == 0 ? "read" : er->ExceptionInformation[0] == 1 ? "write" : "execute";
            o.P("access:    %s of 0x%llx\r\n", k, (unsigned long long)er->ExceptionInformation[1]);
        }
        o.P("flags:     0x%lX, %lu parameters\r\n", er->ExceptionFlags, er->NumberParameters);
    }
    if (ctx) {
        o.P("\r\n-- registers --\r\n");
        o.P("RIP %016llx RSP %016llx RBP %016llx EFLAGS %08lx\r\n", ctx->Rip, ctx->Rsp, ctx->Rbp, ctx->EFlags);
        o.P("RAX %016llx RBX %016llx RCX %016llx RDX %016llx\r\n", ctx->Rax, ctx->Rbx, ctx->Rcx, ctx->Rdx);
        o.P("RSI %016llx RDI %016llx R8  %016llx R9  %016llx\r\n", ctx->Rsi, ctx->Rdi, ctx->R8, ctx->R9);
        o.P("R10 %016llx R11 %016llx R12 %016llx R13 %016llx\r\n", ctx->R10, ctx->R11, ctx->R12, ctx->R13);
        o.P("R14 %016llx R15 %016llx\r\n", ctx->R14, ctx->R15);

        o.P("\r\n-- stack (module+offset; unwound with RtlVirtualUnwind) --\r\n");
        CONTEXT c = *ctx;
        for (int i = 0; i < 40 && c.Rip; ++i) {
            char nm[64];
            uintptr_t off = 0;
            ModuleOf((void*)c.Rip, &off, nm, sizeof(nm));
            o.P("#%02d 0x%016llx %s+0x%llx\r\n", i, c.Rip, nm, (unsigned long long)off);
            DWORD64 imgBase = 0;
            RUNTIME_FUNCTION* rf = RtlLookupFunctionEntry(c.Rip, &imgBase, nullptr);
            if (!rf) {
                if (!c.Rsp) break;
                c.Rip = *(DWORD64*)c.Rsp;  // leaf frame
                c.Rsp += 8;
            } else {
                void* hd = nullptr;
                DWORD64 ef = 0;
                RtlVirtualUnwind(UNW_FLAG_NHANDLER, imgBase, c.Rip, rf, &c, &hd, &ef, nullptr);
            }
            if (c.Rip < 0x10000) break;
        }
    }

    o.P("\r\n-- last mod state --\r\n");
    LONG used = g_stateUsed;
    if (used > kStateSlots) used = kStateSlots;
    for (LONG i = 0; i < used; ++i)
        if (g_state[i].key[0]) o.P("  %-24s %s\r\n", g_state[i].key, g_state[i].val);
    if (!used) o.P("  (none published)\r\n");

    o.P("\r\n-- last log lines (oldest first) --\r\n");
    LONG head = g_logHead;
    for (int i = 0; i < kLogLines; ++i) {
        const char* l = g_logRing[(head + i) % kLogLines];
        if (l[0]) o.P("  %s\r\n", l);
    }

    {
        struct { const char* title; std::wstring path; } files[] = {
            {"mod settings (Nioh3EngineFix.ini)", g_iniPath[0] ? std::wstring(g_iniPath) : std::wstring(g_dir) + L"Nioh3EngineFix.ini"},
            {"Training settings (Nioh3Training.ini)", std::wstring(g_dir) + L"Nioh3Training.ini"},
            {"Boss Rematch settings (Nioh3BossRematch.ini)", std::wstring(g_dir) + L"Nioh3BossRematch.ini"},
        };
        for (auto& f : files) {
            o.P("\r\n-- %s, comments removed --\r\n", f.title);
            const std::string t = SettingsText(f.path.c_str());
            o.W(t.data(), t.size());
        }
    }

    o.P("\r\n-- loaded modules --\r\n");
    HMODULE mods[512];
    DWORD need = 0;
    if (K32EnumProcessModules(GetCurrentProcess(), mods, sizeof(mods), &need)) {
        DWORD cnt = need / sizeof(HMODULE);
        if (cnt > 512) cnt = 512;
        for (DWORD i = 0; i < cnt; ++i) {
            wchar_t w[MAX_PATH];
            MODULEINFO mi{};
            if (!GetModuleFileNameW(mods[i], w, MAX_PATH)) continue;
            K32GetModuleInformation(GetCurrentProcess(), mods[i], &mi, sizeof(mi));
            o.P("  0x%016llx %8lx  %ls\r\n", (unsigned long long)(uintptr_t)mi.lpBaseOfDll, mi.SizeOfImage, w);
        }
    }
    o.P("\r\n(end of report)\r\n");
    FlushFileBuffers(o.f);
    CloseHandle(o.f);
    CopyFileW(path, latest, FALSE);

    if (g_dump && er && ctx) {  // opt-in minidump of the crashed state
        wchar_t dmp[MAX_PATH * 2];
        swprintf(dmp, MAX_PATH * 2, L"%sNioh3EngineFix-crash-%04u%02u%02u-%02u%02u%02u.dmp", g_dir, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        if (HMODULE dh = LoadLibraryW(L"dbghelp.dll")) {
            using MDW = BOOL(WINAPI*)(HANDLE, DWORD, HANDLE, int, void*, void*, void*);
            if (MDW fn = (MDW)GetProcAddress(dh, "MiniDumpWriteDump")) {
                HANDLE df = CreateFileW(dmp, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (df != INVALID_HANDLE_VALUE) {
                    struct { DWORD ThreadId; EXCEPTION_POINTERS* ExceptionPointers; BOOL ClientPointers; } mei = {faultTid, &g_req.ep, FALSE};
                    fn(GetCurrentProcess(), GetCurrentProcessId(), df, 0x00000001 /*MiniDumpWithDataSegs*/ | 0x00000004 /*WithHandleData*/ | 0x00001000 /*WithThreadInfo*/, &mei, nullptr, nullptr);
                    CloseHandle(df);
                }
            }
        }
    }
    if (snapshot) { snprintf(g_lastSnapshot, sizeof(g_lastSnapshot), "%ls", path); }
    if (g_log) g_log("CRASHREPORT: wrote %ls (%s)", path, reason);
}

DWORD WINAPI Writer(LPVOID) {  // fresh stack: survives a stack overflow in the faulting thread
    for (;;) {
        if (WaitForSingleObject(g_evGo, INFINITE) != WAIT_OBJECT_0) return 0;
        WriteReport(g_req.reason, g_req.hasEp ? &g_req.erCopy : nullptr, g_req.hasEp ? &g_req.ctxCopy : nullptr, g_req.tid, g_req.snapshot);
        InterlockedExchange(&g_written, 1);
        SetEvent(g_evDone);
    }
}

// Hands one report to the helper thread and waits for it (max waitMs). Re-entrancy / second faults: only the first reports.
void Submit(const char* reason, EXCEPTION_POINTERS* ep, DWORD waitMs, bool snapshot = false) {
    if (!g_evGo || InterlockedCompareExchange(&g_inReport, 1, 0) != 0) return;
    CopyTrunc(g_req.reason, sizeof(g_req.reason), reason);
    g_req.snapshot = snapshot;
    g_req.tid = GetCurrentThreadId();
    g_req.hasEp = ep && ep->ExceptionRecord && ep->ContextRecord;
    if (g_req.hasEp) {
        g_req.ep = *ep;
        g_req.erCopy = *ep->ExceptionRecord;
        g_req.ctxCopy = *ep->ContextRecord;
        g_req.ep.ExceptionRecord = &g_req.erCopy;
        g_req.ep.ContextRecord = &g_req.ctxCopy;
    }
    SetEvent(g_evGo);
    WaitForSingleObject(g_evDone, waitMs);
    InterlockedExchange(&g_inReport, 0);
}

LONG WINAPI Filter(EXCEPTION_POINTERS* ep) {
    char why[160];
    DWORD c = ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : 0;
    snprintf(why, sizeof(why), "unhandled exception 0x%08lX %s", c, ExcName(c));
    Submit(why, ep, 8000);
    if (g_prevFilter) return g_prevFilter(ep);
    return EXCEPTION_CONTINUE_SEARCH;
}

void __cdecl OnSignalAbort(int) {
    Submit("abort() / SIGABRT (CRT abort path)", nullptr, 8000);
    TerminateProcess(GetCurrentProcess(), 3);
}
void __cdecl OnTerminate() {
    Submit("std::terminate (unhandled C++ exception / noexcept violation)", nullptr, 8000);
    TerminateProcess(GetCurrentProcess(), 3);
}
void __cdecl OnInvalidParam(const wchar_t*, const wchar_t*, const wchar_t*, unsigned, uintptr_t) {
    Submit("CRT invalid parameter handler", nullptr, 8000);
    TerminateProcess(GetCurrentProcess(), 0xC000000D);
}
void __cdecl OnPurecall() {
    Submit("CRT pure virtual call", nullptr, 8000);
    TerminateProcess(GetCurrentProcess(), 3);
}

// "Fatal error" popup path: the engine's fatal handler (18956D0) and the crash box sit in a MODAL dialog on the main thread, so
// the process is alive and no exception reaches the filter. A watcher thread looks for a visible dialog window (#32770) of this
// process once a second and writes one report per distinct dialog title, quoting the title and the static text.
BOOL CALLBACK EnumWnd(HWND w, LPARAM lp) {
    DWORD pid = 0;
    GetWindowThreadProcessId(w, &pid);
    if (pid != GetCurrentProcessId() || !IsWindowVisible(w)) return TRUE;
    wchar_t cls[64] = {};
    GetClassNameW(w, cls, 64);
    if (wcscmp(cls, L"#32770") != 0) return TRUE;
    wchar_t title[160] = {}, text[256] = {};
    GetWindowTextW(w, title, 160);
    for (HWND ch = GetWindow(w, GW_CHILD); ch; ch = GetWindow(ch, GW_HWNDNEXT)) {
        wchar_t cc[32] = {};
        GetClassNameW(ch, cc, 32);
        if (wcscmp(cc, L"Static") == 0) {
            wchar_t t[200] = {};
            GetWindowTextW(ch, t, 200);
            if (t[0] && !text[0]) wcsncpy_s(text, t, _TRUNCATE);
        }
    }
    snprintf((char*)lp, 400, "%ls | %ls", title, text);
    return FALSE;
}

DWORD WINAPI PopupWatcher(LPVOID) {
    char seen[400] = {};
    for (;;) {
        Sleep(1000);
        char cur[400] = {};
        EnumWindows(EnumWnd, (LPARAM)cur);
        if (!cur[0] || strcmp(cur, seen) == 0) continue;
        strcpy_s(seen, cur);
        char why[480];
        snprintf(why, sizeof(why), "error popup shown: %s", cur);
        for (char* p = why; *p; ++p) if (*p < 32) *p = ' ';
        CopyTrunc(g_fatalTitle, sizeof(g_fatalTitle), cur);
        SetState("last_popup", g_fatalTitle);
        Submit(why, nullptr, 8000);
    }
}

// ---- forced-crash hook (pc tests the three report paths with this) ---------------------------------------------------------
volatile int g_testSink = 0;
#pragma warning(push)
#pragma warning(disable : 4717)  // recursive on all paths: intended
int __declspec(noinline) Recurse(int n) {
    volatile char pad[1024];
    pad[0] = (char)n;
    return Recurse(n + 1) + pad[0];
}
#pragma warning(pop)

struct TestArgs { int mode; int delaySec; };
DWORD WINAPI TestThread(LPVOID p) {
    TestArgs a = *(TestArgs*)p;
    delete (TestArgs*)p;
    Sleep((DWORD)a.delaySec * 1000);
    if (g_log) g_log("CRASHREPORT: CrashTest=%d firing now", a.mode);
    SetState("crash_test", "firing");
    switch (a.mode) {
        case 1: *(volatile int*)nullptr = 1; break;                                      // access violation (write to 0)
        case 2: g_testSink = Recurse(0); break;                                          // stack overflow
        case 3: abort(); break;                                                          // abort() path
        case 4: MessageBoxW(nullptr, L"Fatal error (CrashTest 4: simulated engine fatal popup)", L"Nioh 3", MB_OK | MB_ICONERROR); break;
        case 5: throw 42;                                                                // no handler on this thread: std::terminate
        case 6: { wchar_t b[2]; wcscpy_s(b, 2, L"too long"); } break;                    // invalid parameter handler
    }
    return 0;
}

}  // namespace

bool Snapshot(char* pathOut, unsigned cap) {
    if (pathOut && cap) pathOut[0] = 0;
    if (!g_evGo) return false;
    g_lastSnapshot[0] = 0;
    InterlockedExchange(&g_written, 0);
    Submit("manual snapshot (menu Report row)", nullptr, 10000, true);
    if (!g_lastSnapshot[0]) return false;
    if (pathOut && cap) snprintf(pathOut, cap, "%s", g_lastSnapshot);
    return true;
}
const char* ReportUrl() { return g_url; }
bool ReportOpenWanted() { return g_urlOpen; }
bool CopyReportUrl() {
    const size_t n = strlen(g_url) + 1;
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, n);
    if (!h) return false;
    if (void* p = GlobalLock(h)) { memcpy(p, g_url, n); GlobalUnlock(h); } else { GlobalFree(h); return false; }
    bool ok = false;
    if (OpenClipboard(nullptr)) {
        EmptyClipboard();
        ok = SetClipboardData(CF_TEXT, h) != nullptr;
        CloseClipboard();
    }
    if (!ok) GlobalFree(h);
    return ok;
}
void OpenReportUrl() {
    wchar_t w[320];
    MultiByteToWideChar(CP_UTF8, 0, g_url, -1, w, 320);
    ShellExecuteW(nullptr, L"open", w, nullptr, nullptr, SW_SHOWNORMAL);
}
void OpenReportFolder(const char* path) {
    wchar_t windows[MAX_PATH]{};
    const UINT count = GetWindowsDirectoryW(windows, MAX_PATH);
    if (!count || count >= MAX_PATH) return;
    wchar_t explorer[MAX_PATH + 16];
    swprintf(explorer, MAX_PATH + 16, L"%s\\explorer.exe", windows);
    wchar_t target[MAX_PATH * 2]{};
    // Snapshot stores the wide path through narrow %ls, which is the ANSI code page, not UTF-8.
    if (path && path[0]) MultiByteToWideChar(CP_ACP, 0, path, -1, target, MAX_PATH * 2);
    if (!target[0] && g_dir[0]) wcsncpy_s(target, g_dir, _TRUNCATE);
    if (!target[0]) return;
    const DWORD attr = GetFileAttributesW(target);
    const bool folder = attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
    wchar_t args[MAX_PATH * 2 + 32];
    swprintf(args, MAX_PATH * 2 + 32, folder ? L"\"%s\"" : L"/select,\"%s\"", target);
    ShellExecuteW(nullptr, L"open", explorer, args, nullptr, SW_SHOWNORMAL);
}

void SetState(const char* key, const char* value) {
    if (!key) return;
    char k[kKeyLen];
    CopyTrunc(k, sizeof(k), key);
    LONG n = g_stateUsed;
    if (n > kStateSlots) n = kStateSlots;
    for (LONG i = 0; i < n; ++i)
        if (strcmp(g_state[i].key, k) == 0) { CopyTrunc(g_state[i].val, kValLen, value ? value : ""); return; }
    LONG idx = InterlockedIncrement(&g_stateUsed) - 1;
    if (idx >= kStateSlots) return;
    CopyTrunc(g_state[idx].val, kValLen, value ? value : "");
    CopyTrunc(g_state[idx].key, kKeyLen, k);
}

void SetStateF(const char* key, const char* fmt, ...) {
    char b[kValLen];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof(b), fmt, ap);
    va_end(ap);
    SetState(key, b);
}

void NoteLog(const char* line) {
    LONG i = (InterlockedIncrement(&g_logHead) - 1) % kLogLines;
    if (i < 0) i += kLogLines;
    CopyTrunc(g_logRing[i], kLogLen, line);
    for (size_t n = strlen(g_logRing[i]); n && g_logRing[i][n - 1] == ' '; --n) g_logRing[i][n - 1] = 0;
}

void Start(const StartInfo& info) {
    g_log = info.log;
    g_self = (HMODULE)info.selfModule;
    g_base = info.base;
    g_known = info.knownBuild;
    int on = info.iniPath ? (int)GetPrivateProfileIntW(L"Debug", L"CrashReport", 1, info.iniPath) : 1;
    int test = info.iniPath ? (int)GetPrivateProfileIntW(L"Debug", L"CrashTest", 0, info.iniPath) : 0;
    int delay = info.iniPath ? (int)GetPrivateProfileIntW(L"Debug", L"CrashTestDelaySec", 30, info.iniPath) : 30;
    g_dump = info.iniPath && GetPrivateProfileIntW(L"Debug", L"CrashDump", 0, info.iniPath) != 0;
    if (info.iniPath) {
        wchar_t wu[300] = {};
        GetPrivateProfileStringW(L"Report", L"Url", L"", wu, 300, info.iniPath);
        if (wu[0]) WideCharToMultiByte(CP_UTF8, 0, wu, -1, g_url, sizeof(g_url), nullptr, nullptr);
        g_urlOpen = GetPrivateProfileIntW(L"Report", L"Open", 0, info.iniPath) != 0;
    }
    if (!on) {
        if (g_log) g_log("CRASHREPORT: off ([Debug] CrashReport = 0)");
        return;
    }
    DWORD n = GetModuleFileNameW(g_self, g_dir, (DWORD)(sizeof(g_dir) / sizeof(g_dir[0])));
    while (n && g_dir[n - 1] != L'\\' && g_dir[n - 1] != L'/') --n;
    g_dir[n] = 0;
    g_startTick = GetTickCount();
    ComputeShas();
    if (info.iniPath) wcsncpy_s(g_iniPath, info.iniPath, _TRUNCATE);
    {
        const wchar_t* keys[3] = {L"USERNAME", L"COMPUTERNAME", L"USERPROFILE"};
        for (int i = 0; i < 3; ++i) {
            wchar_t v[260] = {};
            DWORD len = GetEnvironmentVariableW(keys[i], v, 260);
            g_redact[i][0] = 0;
            if (len && len < 260) WideCharToMultiByte(CP_UTF8, 0, v, -1, g_redact[i], sizeof(g_redact[i]), nullptr, nullptr);
        }
        // USERPROFILE before USERNAME would leave nothing to match: redact the longest first
        if (strlen(g_redact[2]) > strlen(g_redact[0])) { char t[260]; strcpy_s(t, g_redact[0]); strcpy_s(g_redact[0], g_redact[2]); strcpy_s(g_redact[2], t); }
    }
    g_evGo = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    g_evDone = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!g_evGo || !g_evDone) return;
    if (HANDLE h = CreateThread(nullptr, 256 * 1024, Writer, nullptr, 0, nullptr)) CloseHandle(h);
    if (HANDLE h = CreateThread(nullptr, 64 * 1024, PopupWatcher, nullptr, 0, nullptr)) CloseHandle(h);
    if (HANDLE h = CreateThread(nullptr, 256 * 1024, SpecsThread, nullptr, 0, nullptr)) CloseHandle(h);
    g_prevFilter = SetUnhandledExceptionFilter(Filter);
    signal(SIGABRT, OnSignalAbort);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);  // our handler reports; no CRT message box in front of it
    std::set_terminate(OnTerminate);
    _set_invalid_parameter_handler(OnInvalidParam);
    _set_purecall_handler(OnPurecall);
    SetState("crashreport", "armed");
    if (g_log) g_log("CRASHREPORT: armed (dir %ls, dump %d, test %d)", g_dir, g_dump ? 1 : 0, test);
    if (test > 0 && test <= 6) {
        if (delay < 0) delay = 0;
        if (HANDLE h = CreateThread(nullptr, 0, TestThread, new TestArgs{test, delay}, 0, nullptr)) CloseHandle(h);
        if (g_log) g_log("CRASHREPORT: CrashTest=%d scheduled in %d s", test, delay);
    }
}

}  // namespace crashreport
