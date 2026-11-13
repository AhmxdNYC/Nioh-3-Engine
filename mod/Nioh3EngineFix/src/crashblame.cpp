// crashblame.cpp - see crashblame.h.
#include "crashblame.h"

#include <psapi.h>
#include <cstdarg>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cwctype>

#pragma comment(lib, "version.lib")

namespace crashblame {
namespace {

wchar_t g_winDir[MAX_PATH], g_sysDir[MAX_PATH], g_gameDir[MAX_PATH];
size_t g_winLen = 0, g_gameLen = 0;
volatile LONG g_init = 0;

void Init() {
    if (g_init) return;
    UINT n = GetWindowsDirectoryW(g_winDir, MAX_PATH - 2);
    if (n && n < MAX_PATH - 2) { if (g_winDir[n - 1] != L'\\') { g_winDir[n++] = L'\\'; g_winDir[n] = 0; } g_winLen = n; }
    GetSystemDirectoryW(g_sysDir, MAX_PATH - 40);
    DWORD m = GetModuleFileNameW(nullptr, g_gameDir, MAX_PATH);
    if (m && m < MAX_PATH) {
        wchar_t* s = wcsrchr(g_gameDir, L'\\');
        if (s) { s[1] = 0; g_gameLen = wcslen(g_gameDir); }
    }
    InterlockedExchange(&g_init, 1);
}

bool PrefixI(const wchar_t* s, const wchar_t* p, size_t n) { return n && _wcsnicmp(s, p, n) == 0; }

bool EndsWithI(const wchar_t* s, const wchar_t* suf) {
    size_t a = wcslen(s), b = wcslen(suf);
    return a >= b && _wcsicmp(s + a - b, suf) == 0;
}

const wchar_t* BaseName(const wchar_t* path) {
    const wchar_t* b = path;
    for (const wchar_t* p = path; *p; ++p) if (*p == L'\\' || *p == L'/') b = p + 1;
    return b;
}

void Company(const wchar_t* path, char* out, size_t cap) {
    out[0] = 0;
    DWORD h = 0, sz = GetFileVersionInfoSizeW(path, &h);
    if (!sz || sz > (1u << 20)) return;
    void* buf = malloc(sz);
    if (!buf) return;
    if (GetFileVersionInfoW(path, 0, sz, buf)) {
        struct Tr { WORD lang, cp; }* tr = nullptr;
        UINT tl = 0;
        wchar_t key[64];
        if (VerQueryValueW(buf, L"\\VarFileInfo\\Translation", (void**)&tr, &tl) && tr && tl >= sizeof(Tr))
            swprintf(key, 64, L"\\StringFileInfo\\%04x%04x\\CompanyName", tr->lang, tr->cp);
        else
            wcscpy_s(key, L"\\StringFileInfo\\040904b0\\CompanyName");
        wchar_t* v = nullptr;
        UINT vl = 0;
        if (VerQueryValueW(buf, key, (void**)&v, &vl) && v && vl) {
            char a[96] = {};
            WideCharToMultiByte(CP_UTF8, 0, v, -1, a, (int)sizeof(a) - 1, nullptr, nullptr);
            size_t L = strlen(a);
            while (L && (a[L - 1] == ' ' || a[L - 1] == '.')) a[--L] = 0;
            if (L >= cap) { a[cap - 1] = 0; }
            strcpy_s(out, cap, a);
        }
    }
    free(buf);
}

// Windows unload trace (ntdll keeps the last unloaded modules; Win8+ has the Ex query, Wine may have neither).
struct UnloadEv {
    PVOID base;
    SIZE_T size;
    ULONG seq, stamp, sum;
    WCHAR name[32];
    ULONG ver[2];
};
using GetTraceFn = PVOID(NTAPI*)();
using GetTraceExFn = VOID(NTAPI*)(PULONG*, PULONG*, PVOID*);

bool UnloadTrace(const BYTE** arr, ULONG* count, ULONG* elem) {
    HMODULE nt = GetModuleHandleW(L"ntdll.dll");
    if (!nt) return false;
    auto fn = reinterpret_cast<GetTraceFn>(GetProcAddress(nt, "RtlGetUnloadEventTrace"));
    auto fx = reinterpret_cast<GetTraceExFn>(GetProcAddress(nt, "RtlGetUnloadEventTraceEx"));
    const BYTE* a = nullptr;  // Ex gives the live array (*pt); the old call returns a static buffer that stays empty (checked on 26200)
    ULONG c = 64, e = (ULONG)sizeof(UnloadEv);
    if (fx) {
        PULONG pe = nullptr, pc = nullptr;
        PVOID pt = nullptr;
        __try {
            fx(&pe, &pc, &pt);
            if (pe && *pe >= offsetof(UnloadEv, ver) && *pe <= 512) e = *pe;
            if (pc && *pc && *pc <= 1024) c = *pc;
            if (pt) a = *static_cast<const BYTE* const*>(pt);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    } else if (fn) {
        a = static_cast<const BYTE*>(fn());  // pre-Win8 / Wine: the only query there is
    }
    if (!a) return false;
    *arr = a;
    *count = c;
    *elem = e;
    return true;
}

bool UnloadAt(ULONG i, const BYTE* arr, ULONG elem, UnloadEv& ev) {
    __try {
        memcpy(&ev, arr + (size_t)i * elem, elem < sizeof(UnloadEv) ? elem : sizeof(UnloadEv));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool FindUnloaded(uintptr_t a, Owner& out) {
    const BYTE* arr = nullptr;
    ULONG cnt = 0, elem = 0;
    if (!UnloadTrace(&arr, &cnt, &elem)) return false;
    ULONG bestSeq = 0;
    bool found = false;
    for (ULONG i = 0; i < cnt; ++i) {
        UnloadEv ev{};
        if (!UnloadAt(i, arr, elem, ev) || !ev.base || !ev.size) continue;
        uintptr_t b = (uintptr_t)ev.base;
        if (a < b || a >= b + ev.size) continue;
        if (found && ev.seq < bestSeq) continue;
        found = true;
        bestSeq = ev.seq;
        wchar_t nm[33] = {};
        memcpy(nm, ev.name, sizeof(ev.name));
        WideCharToMultiByte(CP_UTF8, 0, nm, -1, out.name, (int)sizeof(out.name) - 1, nullptr, nullptr);
        out.off = a - b;
    }
    if (found) out.kind = kUnloaded;
    return found;
}

bool ReadQ(uintptr_t a, uint64_t& v) {
    __try {
        v = *reinterpret_cast<const volatile uint64_t*>(a);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Is r a return address, i.e. is there a call instruction ending right before it?
bool AfterCall(uintptr_t r) {
    unsigned char b[8];
    __try {
        memcpy(b, reinterpret_cast<const void*>(r - 8), 8);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    // b[8 - k] is the byte k before r
    if (b[3] == 0xE8) return true;                                         // call rel32
    if (b[2] == 0xFF && (b[3] == 0x15 || (b[3] & 0xF8) == 0x90)) return true;  // call [rip+d32] / [r+d32]
    if (b[6] == 0xFF && (b[7] & 0xF8) == 0xD0) return true;                // call reg
    if (b[5] == 0xFF && ((b[6] & 0xF8) == 0x50 || b[6] == 0x14)) return true;  // call [r+d8] / [sib]
    if (b[6] == 0xFF && (b[7] & 0xF8) == 0x10) return true;                // call [reg]
    if (b[4] == 0xFF && b[5] == 0x54) return true;                         // call [sib+d8]
    return false;
}

struct Mod {
    uintptr_t base;
    DWORD size;
    int kind;  // -1 not classified yet
    int hits;
    char name[64];
    char company[48];
};
constexpr int kMaxMods = 512;
Mod g_mods[kMaxMods];  // report helper thread only (one report per process)
int g_modN = 0;

void LoadModules() {
    g_modN = 0;
    HMODULE hm[kMaxMods];
    DWORD need = 0;
    if (!K32EnumProcessModules(GetCurrentProcess(), hm, sizeof(hm), &need)) return;
    DWORD n = need / sizeof(HMODULE);
    if (n > (DWORD)kMaxMods) n = kMaxMods;
    for (DWORD i = 0; i < n; ++i) {
        MODULEINFO mi{};
        if (!K32GetModuleInformation(GetCurrentProcess(), hm[i], &mi, sizeof(mi))) continue;
        Mod& m = g_mods[g_modN++];
        m.base = (uintptr_t)mi.lpBaseOfDll;
        m.size = mi.SizeOfImage;
        m.kind = -1;
        m.hits = 0;
        m.name[0] = m.company[0] = 0;
    }
}

Mod* ModAt(uintptr_t a) {
    for (int i = 0; i < g_modN; ++i)
        if (a >= g_mods[i].base && a < g_mods[i].base + g_mods[i].size) return &g_mods[i];
    return nullptr;
}

void Fill(Mod& m) {
    if (m.kind >= 0) return;
    Owner o;
    Classify(reinterpret_cast<const void*>(m.base), o, true);
    m.kind = o.kind;
    strcpy_s(m.name, o.name);
    strcpy_s(m.company, o.company);
}

struct Buf {
    char* p;
    size_t cap, n = 0;
    void P(const char* fmt, ...) {
        if (!p || n + 1 >= cap) return;
        va_list ap;
        va_start(ap, fmt);
        int k = vsnprintf(p + n, cap - n, fmt, ap);
        va_end(ap);
        if (k < 0) { p[n] = 0; return; }
        n += (size_t)k < cap - n ? (size_t)k : cap - n - 1;
    }
};

bool IsSys(int k) { return k == kWindows || k == kDriver; }
bool IsOther(int k) { return k == kProxy || k == kGameFolder || k == kOtherProgram; }

void OwnerText(const Owner& o, char* out, size_t cap) {
    if (o.kind == kNoModule) { snprintf(out, cap, "0x%llx (no module)", o.off); return; }
    snprintf(out, cap, "%s+0x%llx [%s%s%s]", o.name, o.off, KindText(o.kind), o.company[0] ? ", " : "", o.company);
}

}  // namespace

const char* KindText(Kind k) {
    switch (k) {
        case kOurs: return "our mod";
        case kGame: return "the game";
        case kWindows: return "Windows";
        case kDriver: return "device driver";
        case kProxy: return "proxy DLL in the game folder (mod loader/injector)";
        case kGameFolder: return "game-folder DLL (game file or another mod)";
        case kOtherProgram: return "another program's DLL (overlay/tool)";
        case kUnloaded: return "unloaded before the crash";
        default: return "no module";
    }
}

void Classify(const void* addr, Owner& out, bool withCompany) {
    Init();
    out = Owner{};
    out.off = (unsigned long long)(uintptr_t)addr;
    HMODULE m = nullptr;
    if (!addr || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                     static_cast<LPCWSTR>(addr), &m) || !m) {
        if (addr) FindUnloaded((uintptr_t)addr, out);
        return;
    }
    out.off = (unsigned long long)((uintptr_t)addr - (uintptr_t)m);
    wchar_t path[MAX_PATH * 2] = {};
    DWORD n = GetModuleFileNameW(m, path, MAX_PATH * 2);
    if (!n) { strcpy_s(out.name, "?"); out.kind = kOtherProgram; return; }
    const wchar_t* base = BaseName(path);
    WideCharToMultiByte(CP_UTF8, 0, base, -1, out.name, (int)sizeof(out.name) - 1, nullptr, nullptr);
    if (m == GetModuleHandleW(nullptr)) out.kind = kGame;
    else if (_wcsnicmp(base, L"Nioh3", 5) == 0 && EndsWithI(base, L".asi")) out.kind = kOurs;
    else if (PrefixI(path, g_winDir, g_winLen)) {
        wchar_t low[MAX_PATH * 2];
        size_t i = 0;
        for (; path[i] && i + 1 < MAX_PATH * 2; ++i) low[i] = (wchar_t)towlower(path[i]);
        low[i] = 0;
        out.kind = wcsstr(low, L"\\driverstore\\") ? kDriver : kWindows;
    }
    else if (PrefixI(path, g_gameDir, g_gameLen) && !wcschr(path + g_gameLen, L'\\')) {
        out.kind = kGameFolder;
        if (g_sysDir[0] && !EndsWithI(base, L".asi")) {
            wchar_t sys[MAX_PATH * 2];
            swprintf(sys, MAX_PATH * 2, L"%s\\%s", g_sysDir, base);
            if (GetFileAttributesW(sys) != INVALID_FILE_ATTRIBUTES) out.kind = kProxy;
        }
    } else if (PrefixI(path, g_gameDir, g_gameLen)) out.kind = kGameFolder;  // subfolder of the game folder
    else out.kind = kOtherProgram;
    if (withCompany && out.kind != kWindows) Company(path, out.company, sizeof(out.company));
}

int Analyze(const EXCEPTION_RECORD* er, const CONTEXT* ctx, char* text, size_t textCap, char (*shortLines)[120], int maxShort) {
    Init();
    Buf o{text, textCap};
    if (text && textCap) text[0] = 0;
    LoadModules();

    const void* crashAddr = er ? er->ExceptionAddress : ctx ? reinterpret_cast<const void*>(ctx->Rip) : nullptr;
    Owner c0;
    Classify(crashAddr, c0, true);
    const bool exec = er && (er->ExceptionCode == 0xC0000005 || er->ExceptionCode == 0xC0000006) && er->NumberParameters >= 2 &&
                      er->ExceptionInformation[0] == 8;

    // unwound frames
    constexpr int kFrames = 40;
    Owner fr[kFrames];
    int nfr = 0;
    if (ctx) {
        CONTEXT c = *ctx;
        for (int i = 0; i < kFrames && c.Rip; ++i) {
            Classify(reinterpret_cast<const void*>(c.Rip), fr[nfr++], false);
            DWORD64 imgBase = 0;
            RUNTIME_FUNCTION* rf = RtlLookupFunctionEntry(c.Rip, &imgBase, nullptr);
            if (!rf) {
                uint64_t r = 0;
                if (!c.Rsp || !ReadQ((uintptr_t)c.Rsp, r)) break;
                c.Rip = r;
                c.Rsp += 8;
            } else {
                void* hd = nullptr;
                DWORD64 ef = 0;
                __try {
                    RtlVirtualUnwind(UNW_FLAG_NHANDLER, imgBase, c.Rip, rf, &c, &hd, &ef, nullptr);
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    break;
                }
            }
            if (c.Rip < 0x10000) break;
        }
    }
    // nearest caller that is not the game / Windows / a driver, and the first non-Windows frame
    int iFirstNonSys = -1, iMod = -1, iOurs = -1;
    for (int i = 0; i < nfr; ++i) {
        const int k = fr[i].kind;
        if (iFirstNonSys < 0 && !IsSys(k)) iFirstNonSys = i;
        if (iMod < 0 && i > 0 && (k == kOurs || IsOther(k))) iMod = i;
        if (iOurs < 0 && k == kOurs) iOurs = i;
    }

    // raw stack scan: return addresses into non-Windows modules (catches frames the unwinder skips: hooks, trampolines)
    int scanned = 0;
    const Mod* rawFirst = nullptr;  // nearest-to-the-top return address into our mod or another mod/program
    if (ctx && ctx->Rsp) {
        MEMORY_BASIC_INFORMATION mbi{};
        uintptr_t lo = (uintptr_t)ctx->Rsp & ~(uintptr_t)7, hi = lo + 16 * 1024;
        if (VirtualQuery(reinterpret_cast<void*>(lo), &mbi, sizeof(mbi))) {
            uintptr_t end = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
            if (end < hi) hi = end;
        }
        for (uintptr_t a = lo; a + 8 <= hi; a += 8) {
            uint64_t v = 0;
            if (!ReadQ(a, v)) break;
            ++scanned;
            Mod* m = ModAt((uintptr_t)v);
            if (!m) continue;
            Fill(*m);
            if (IsSys(m->kind) || !AfterCall((uintptr_t)v)) continue;
            ++m->hits;
            if (!rawFirst && (m->kind == kOurs || IsOther(m->kind))) rawFirst = m;
        }
    }

    // verdict
    char who[200] = {}, a0[160], a1[160];
    OwnerText(c0, a0, sizeof a0);
    const Owner* caller = iMod >= 0 ? &fr[iMod] : nullptr;
    if (caller) OwnerText(*caller, a1, sizeof a1); else strcpy_s(a1, "-");
    const char* label = "unknown";
    if (c0.kind == kOurs) {
        label = "OUR MOD";
        snprintf(who, sizeof who, "OUR MOD: crashed inside %s+0x%llx", c0.name, c0.off);
    } else if (IsOther(c0.kind)) {
        label = "ANOTHER MOD/PROGRAM";
        snprintf(who, sizeof who, "ANOTHER MOD/PROGRAM: crashed inside %s%s%s%s", c0.name, c0.company[0] ? " (" : "", c0.company,
                 c0.company[0] ? ")" : "");
    } else if (c0.kind == kUnloaded || c0.kind == kNoModule) {
        const Owner* f = nullptr;  // who jumped there: the first real frame above
        for (int i = 1; i < nfr && !f; ++i) if (!IsSys(fr[i].kind) && fr[i].kind != kNoModule && fr[i].kind != kUnloaded) f = &fr[i];
        const bool goneOurs = c0.kind == kUnloaded && _strnicmp(c0.name, "Nioh3", 5) == 0;
        label = f && f->kind == kOurs ? "OUR MOD" : f && IsOther(f->kind) ? "ANOTHER MOD/PROGRAM" : goneOurs ? "OUR MOD"
              : c0.kind == kUnloaded ? "ANOTHER MOD/PROGRAM (unloaded)" : "unknown";
        if (c0.kind == kUnloaded)
            snprintf(who, sizeof who, "%s: ran code of %s after it was unloaded; called from %s", label, c0.name, f ? f->name : "unknown");
        else
            snprintf(who, sizeof who, "%s: %s 0x%llx, no code there (hook/trampoline or bad pointer); called from %s", label,
                     exec ? "jumped to" : "crashed at", c0.off, f ? f->name : "unknown");
    } else if (IsSys(c0.kind)) {
        const Owner* f = iFirstNonSys >= 0 ? &fr[iFirstNonSys] : nullptr;
        label = !f ? "Windows/driver" : f->kind == kOurs ? "OUR MOD" : IsOther(f->kind) ? "ANOTHER MOD/PROGRAM" : f->kind == kGame ? "the game" : "unknown";
        snprintf(who, sizeof who, "%s: inside %s (%s), called from %s", label, c0.name, KindText(c0.kind), f ? f->name : "nothing outside Windows");
    } else if (c0.kind == kGame) {
        if (caller && caller->kind == kOurs) {
            label = "OUR MOD (likely)";
            snprintf(who, sizeof who, "OUR MOD (likely): game code %s+0x%llx called from our %s+0x%llx", c0.name, c0.off, caller->name, caller->off);
        } else if (caller) {
            label = "ANOTHER MOD/PROGRAM (likely)";
            snprintf(who, sizeof who, "ANOTHER MOD/PROGRAM (likely): game code %s+0x%llx called from %s", c0.name, c0.off, caller->name);
        } else {
            label = "the game";
            snprintf(who, sizeof who, "THE GAME: %s+0x%llx, no mod on the unwound stack", c0.name, c0.off);
        }
    }
    if (!who[0]) snprintf(who, sizeof who, "unknown: %s", a0);
    if (rawFirst && !strstr(who, rawFirst->name) && (!strncmp(who, "THE GAME", 8) || !strncmp(who, "the game", 8) || !strncmp(who, "unknown", 7))) {
        const size_t L = strlen(who);
        snprintf(who + L, sizeof(who) - L, "; raw stack has %s (%s)", rawFirst->name, KindText((Kind)rawFirst->kind));
    }

    o.P("verdict:   %s\r\n", who);
    o.P("           (best guess from addresses; a mod can also break the game by changing its data earlier)\r\n");
    o.P("crash at:  %s\r\n", a0);
    if (er && (er->ExceptionCode == 0xC0000005 || er->ExceptionCode == 0xC0000006) && er->NumberParameters >= 2 &&
        er->ExceptionInformation[1] >= 0x10000) {
        Owner t;
        Classify(reinterpret_cast<const void*>(er->ExceptionInformation[1]), t, false);
        if (t.kind != kNoModule) { char tt[160]; OwnerText(t, tt, sizeof tt); o.P("target:    0x%llx is %s\r\n", (unsigned long long)er->ExceptionInformation[1], tt); }
    }
    if (iFirstNonSys > 0) { char t[160]; OwnerText(fr[iFirstNonSys], t, sizeof t); o.P("first frame outside Windows/drivers: #%02d %s\r\n", iFirstNonSys, t); }
    o.P("nearest mod/program caller: %s\r\n", caller ? a1 : "none on the unwound stack");
    o.P("our mods on the unwound stack:");
    int ours = 0;
    for (int i = 0; i < nfr; ++i) if (fr[i].kind == kOurs) o.P("%s #%02d %s+0x%llx", ours++ ? "," : "", i, fr[i].name, fr[i].off);
    o.P("%s\r\n", ours ? "" : " none");
    o.P("other mods/programs on the unwound stack:");
    int oth = 0;
    for (int i = 0; i < nfr; ++i) if (IsOther(fr[i].kind) || fr[i].kind == kUnloaded) o.P("%s #%02d %s+0x%llx", oth++ ? "," : "", i, fr[i].name, fr[i].off);
    o.P("%s\r\n", oth ? "" : " none");
    o.P("return addresses in the top %d KB of the crashing stack (finds hook frames the unwinder skips):", scanned * 8 / 1024);
    int hm = 0;
    for (int i = 0; i < g_modN; ++i)
        if (g_mods[i].hits) o.P("%s %s x%d [%s]", hm++ ? "," : "", g_mods[i].name, g_mods[i].hits, KindText((Kind)g_mods[i].kind));
    o.P("%s\r\n", hm ? "" : " none outside Windows");

    // unloaded modules
    o.P("modules unloaded before the crash (Windows unload trace, newest first):");
    const BYTE* arr = nullptr;
    ULONG cnt = 0, elem = 0;
    char unl[118] = {};
    size_t unlN = 0;
    if (UnloadTrace(&arr, &cnt, &elem)) {
        int shown = 0;
        ULONG lastSeq = 0xFFFFFFFFu;
        for (int pass = 0; pass < 24; ++pass) {  // newest first: pick the highest sequence below the previous one
            ULONG best = 0, bi = 0xFFFFFFFFu;
            for (ULONG i = 0; i < cnt; ++i) {
                UnloadEv ev{};
                if (!UnloadAt(i, arr, elem, ev) || !ev.base || ev.seq >= lastSeq) continue;
                if (bi == 0xFFFFFFFFu || ev.seq > best) { best = ev.seq; bi = i; }
            }
            if (bi == 0xFFFFFFFFu) break;
            lastSeq = best;
            UnloadEv ev{};
            UnloadAt(bi, arr, elem, ev);
            wchar_t nm[33] = {};
            memcpy(nm, ev.name, sizeof(ev.name));
            char a[72] = {};
            WideCharToMultiByte(CP_UTF8, 0, nm, -1, a, (int)sizeof(a) - 1, nullptr, nullptr);
            o.P("\r\n  %s  0x%016llx %8llx", a, (unsigned long long)(uintptr_t)ev.base, (unsigned long long)ev.size);
            ++shown;
            int k = snprintf(unl + unlN, sizeof(unl) - unlN, "%s%s", unlN ? ", " : "", a);
            if (k > 0 && unlN + (size_t)k < sizeof(unl)) unlN += (size_t)k;
        }
        o.P("%s\r\n", shown ? "" : " none");
    } else {
        o.P(" (not available on this system)\r\n");
    }

    // every non-Windows module, classed
    o.P("non-Windows modules loaded (class, company):\r\n");
    for (int i = 0; i < g_modN; ++i) {
        Fill(g_mods[i]);
        if (IsSys(g_mods[i].kind)) continue;
        o.P("  %-36s %s%s%s\r\n", g_mods[i].name, KindText((Kind)g_mods[i].kind), g_mods[i].company[0] ? ", " : "", g_mods[i].company);
    }

    // short lines for the upload
    int ns = 0;
    if (shortLines && maxShort > 0) {
        snprintf(shortLines[ns++], 118, "WHO: %s", who);
        if (ns < maxShort) {
            char t[118] = {};
            size_t tn = 0;
            for (int i = 0; i < g_modN; ++i) {
                if (!g_mods[i].hits) continue;
                int k = snprintf(t + tn, sizeof(t) - tn, "%s%s x%d", tn ? ", " : "", g_mods[i].name, g_mods[i].hits);
                if (k < 0 || tn + (size_t)k >= sizeof(t)) break;
                tn += (size_t)k;
            }
            snprintf(shortLines[ns++], 118, "WHO stack: ours %s; raw: %s", ours ? "yes" : "no", tn ? t : "none");
        }
        if (ns < maxShort && unlN) snprintf(shortLines[ns++], 118, "WHO unloaded: %s", unl);
        if (ns < maxShort) {
            char t[118] = {};
            size_t tn = 0;
            for (int i = 0; i < g_modN; ++i) {
                const int k0 = g_mods[i].kind;
                if (k0 != kProxy && k0 != kOtherProgram && !(k0 == kGameFolder && strstr(g_mods[i].name, ".asi"))) continue;
                int k = snprintf(t + tn, sizeof(t) - tn, "%s%s%s", tn ? ", " : "", g_mods[i].name, k0 == kProxy ? "(proxy)" : "");
                if (k < 0 || tn + (size_t)k >= sizeof(t)) break;
                tn += (size_t)k;
            }
            if (tn) snprintf(shortLines[ns++], 118, "WHO mods: %s", t);
        }
    }
    (void)label;
    (void)iOurs;
    return ns;
}

}  // namespace crashblame
