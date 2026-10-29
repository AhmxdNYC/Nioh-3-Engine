// timerres.cpp - TR: hold the 1 ms timer resolution once, skip the game's per-loop timeBeginPeriod/timeEndPeriod.
// See timerres.h and notes\game-logic-audit.md section 3.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <climits>
#include <cstdio>
#include <cstring>
#include "timerres.h"

namespace timerres {
namespace {

using Period_t = UINT(WINAPI*)(UINT);
using NtQueryTimerResolution_t = LONG(NTAPI*)(PULONG coarsest, PULONG finest, PULONG current);

constexpr UINT kPeriod = 1;            // ms; what the game's HID threads request (g_HidTimerPeriod 44B68F0 = 1 in v2.02)
constexpr UINT kTimerrNoError = 0;     // TIMERR_NOERROR (mmsystem.h not included)

void (*s_log)(const char*, ...) = nullptr;
Period_t s_begin = nullptr, s_end = nullptr;  // the original slot values (winmm or a hook chained before us)
volatile uint32_t s_enable = 0;
volatile LONG s_held = 0;                     // 1 while our own timeBeginPeriod(kPeriod) is active
SRWLOCK s_lock = SRWLOCK_INIT;
DWORD s_tls = TLS_OUT_OF_INDEXES;             // per thread: game begins skipped and not yet ended
bool s_installed = false;
NtQueryTimerResolution_t s_ntQuery = nullptr;
// counters (monotonic; the stats line prints deltas)
volatile LONG s_nBegin = 0, s_nEnd = 0, s_nSkipB = 0, s_nSkipE = 0, s_nHolds = 0;
volatile LONG s_minAsked = LONG_MAX;

void** FindImport(uint8_t* base, const char* dll, const char* fn) {
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    const IMAGE_DATA_DIRECTORY& dd = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dd.VirtualAddress) return nullptr;
    for (auto d = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dd.VirtualAddress); d->Name; ++d) {
        if (_stricmp(reinterpret_cast<const char*>(base + d->Name), dll) != 0 || !d->OriginalFirstThunk) continue;
        auto oft = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + d->OriginalFirstThunk);
        auto ft = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + d->FirstThunk);
        for (; oft->u1.AddressOfData; ++oft, ++ft)
            if (!IMAGE_SNAP_BY_ORDINAL64(oft->u1.Ordinal) &&
                !strcmp(reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + oft->u1.AddressOfData)->Name, fn))
                return reinterpret_cast<void**>(&ft->u1.Function);
    }
    return nullptr;
}

bool Swap(void** slot, void* hook, void** orig) {
    DWORD old, t;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) return false;
    *orig = InterlockedExchangePointer(slot, hook);
    VirtualProtect(slot, sizeof(void*), old, &t);
    return *orig != nullptr;
}

void NoteAsked(UINT p) {
    LONG cur = s_minAsked;
    while ((LONG)p < cur) {
        const LONG seen = InterlockedCompareExchange(&s_minAsked, (LONG)p, cur);
        if (seen == cur) break;
        cur = seen;
    }
}

UINT WINAPI HkBegin(UINT p) {
    InterlockedIncrement(&s_nBegin);
    NoteAsked(p);
    Sync();
    if (s_held && p >= kPeriod) {  // our hold already gives this period (or a finer one): nothing to change
        TlsSetValue(s_tls, reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(TlsGetValue(s_tls)) + 1));
        InterlockedIncrement(&s_nSkipB);
        return kTimerrNoError;
    }
    return s_begin(p);
}

UINT WINAPI HkEnd(UINT p) {
    InterlockedIncrement(&s_nEnd);
    const uintptr_t d = reinterpret_cast<uintptr_t>(TlsGetValue(s_tls));
    if (d) {  // the matching begin on this thread was skipped: skip the end too (balanced across switch flips)
        TlsSetValue(s_tls, reinterpret_cast<void*>(d - 1));
        InterlockedIncrement(&s_nSkipE);
        Sync();
        return kTimerrNoError;
    }
    const UINT r = s_end(p);
    Sync();
    return r;
}

}  // namespace

void Sync() {
    if (!s_installed) return;
    const LONG want = s_enable ? 1 : 0;
    if (s_held == want) return;
    AcquireSRWLockExclusive(&s_lock);
    if (s_held != want) {
        if (want) {
            if (s_begin(kPeriod) == kTimerrNoError) {
                s_held = 1;
                InterlockedIncrement(&s_nHolds);
            }
        } else {
            s_end(kPeriod);
            s_held = 0;
        }
    }
    ReleaseSRWLockExclusive(&s_lock);
}

void Start(const StartInfo& info) {
    s_log = info.log;
    const int mode = info.iniPath ? (int)GetPrivateProfileIntW(L"Patches", L"TR", 0, info.iniPath) : 0;
    if (mode <= 0) return;
    auto base = reinterpret_cast<uint8_t*>(info.base);
    void** sb = FindImport(base, "WINMM.dll", "timeBeginPeriod");
    void** se = FindImport(base, "WINMM.dll", "timeEndPeriod");
    if (!sb || !se || !*sb || !*se) {
        if (s_log) s_log("TR: Nioh3.exe has no WINMM timeBeginPeriod/timeEndPeriod import slot - not installed");
        return;
    }
    s_tls = TlsAlloc();
    if (s_tls == TLS_OUT_OF_INDEXES) {
        if (s_log) s_log("TR: TlsAlloc failed - not installed");
        return;
    }
    // the originals are known before the slots point at the hooks, so a game thread can never see a null original
    s_begin = reinterpret_cast<Period_t>(*sb);
    s_end = reinterpret_cast<Period_t>(*se);
    void* ob = nullptr;
    void* oe = nullptr;
    if (!Swap(se, reinterpret_cast<void*>(&HkEnd), &oe) || !Swap(sb, reinterpret_cast<void*>(&HkBegin), &ob)) {
        if (s_log) s_log("TR: import slot write failed - not installed");
        if (oe) Swap(se, oe, &oe);
        return;
    }
    s_begin = reinterpret_cast<Period_t>(ob);
    s_end = reinterpret_cast<Period_t>(oe);
    if (HMODULE nt = GetModuleHandleW(L"ntdll.dll"))
        s_ntQuery = reinterpret_cast<NtQueryTimerResolution_t>(GetProcAddress(nt, "NtQueryTimerResolution"));
    s_enable = mode == 1 ? 1u : 0u;
    s_installed = true;
    Sync();
    if (s_log)
        s_log("TR: game timeBeginPeriod/timeEndPeriod hooked (HID reader threads); switch %s (%s)", s_enable ? "ON" : "off",
              s_enable ? (s_held ? "1 ms held for the session, the game's own pairs are skipped" : "hold FAILED - calls pass")
                       : "calls pass, counters only");
}

volatile uint32_t* Switch() { return s_installed ? &s_enable : nullptr; }

void StatusLine(char* buf, unsigned n, unsigned secs) {
    if (!n) return;
    buf[0] = 0;
    if (!s_installed) return;
    Sync();
    static LONG pB = 0, pE = 0, pSB = 0, pSE = 0;
    const LONG b = s_nBegin, e = s_nEnd, sb = s_nSkipB, se = s_nSkipE;
    const unsigned s = secs ? secs : 1;
    char res[96] = "system timer ?";
    ULONG coarse = 0, fine = 0, cur = 0;
    if (s_ntQuery && s_ntQuery(&coarse, &fine, &cur) >= 0)
        snprintf(res, sizeof res, "system timer %.3f ms (finest %.3f, coarsest %.3f)", cur / 1e4, fine / 1e4, coarse / 1e4);
    // what a game thread's Sleep(1) costs in THIS process right now (per-process granularity since Windows 10 2004):
    // 8 samples on the stats thread, <= ~130 ms at 15.6 ms granularity
    LARGE_INTEGER f, a, z;
    QueryPerformanceFrequency(&f);
    double sl[8], mx = 0;
    for (double& v : sl) {
        QueryPerformanceCounter(&a);
        Sleep(1);
        QueryPerformanceCounter(&z);
        v = (double)(z.QuadPart - a.QuadPart) * 1e6 / (double)f.QuadPart;
        if (v > mx) mx = v;
    }
    for (int i = 1; i < 8; ++i)  // insertion sort for the median
        for (int j = i; j > 0 && sl[j] < sl[j - 1]; --j) { const double t = sl[j]; sl[j] = sl[j - 1]; sl[j - 1] = t; }
    snprintf(buf, n, "TR: game timeBeginPeriod %ld/s timeEndPeriod %ld/s (period asked %ld ms), skipped %ld/%ld per s | switch %s, "
             "hold %s | %s | Sleep(1) here: median %.0f us, max %.0f us",
             (long)((b - pB) / (LONG)s), (long)((e - pE) / (LONG)s), s_minAsked == LONG_MAX ? -1L : (long)s_minAsked,
             (long)((sb - pSB) / (LONG)s), (long)((se - pSE) / (LONG)s), s_enable ? "ON" : "off", s_held ? "1 ms" : "none", res,
             (sl[3] + sl[4]) / 2, mx);
    pB = b; pE = e; pSB = sb; pSE = se;
}

}  // namespace timerres
