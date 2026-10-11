// sp.cpp - in-process sampling profiler (see sp.h). Measurement only.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <vector>

#include "sp.h"

namespace sp {
namespace {

void (*g_log)(const char*, ...) = nullptr;
#define SLOG(...) \
    do {                           \
        if (g_log) g_log(__VA_ARGS__); \
    } while (0)

struct Cfg {
    int intervalUs = 1000, workers = 1, reportSec = 30, topN = 25, stackBytes = 2048, startDelaySec = 20;
    int focus = 0;  // 1.7.57: 1 = KIDSTaskScheduler0 (logic) gets its own group and, with the dispatcher, is sampled every tick
} g_cfg;

// 1.7.36 Tour: windows follow the tour's dwells (sampling only inside a dwell, one report per dwell)
volatile LONG g_ext = 0, g_sampling = 0, g_reqReset = 0, g_reqReport = 0;
char g_winTag[64] = {};

// ---- Nioh3.exe image ------------------------------------------------------------------------------------------------
uintptr_t g_base = 0, g_end = 0;
struct Range {
    uintptr_t lo, hi;
};
std::vector<Range> g_exec;                  // executable sections of Nioh3.exe
const RUNTIME_FUNCTION* g_pdata = nullptr;  // sorted by BeginAddress
size_t g_pdataN = 0;

bool InExec(uintptr_t a) {
    for (const Range& r : g_exec)
        if (a >= r.lo && a < r.hi) return true;
    return false;
}

void InitImage(uintptr_t base) {
    g_base = base;
    auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    auto nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    g_end = base + nt->OptionalHeader.SizeOfImage;
    const IMAGE_SECTION_HEADER* s = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i)
        if (s[i].Characteristics & IMAGE_SCN_MEM_EXECUTE)
            g_exec.push_back({base + s[i].VirtualAddress, base + s[i].VirtualAddress + std::max(s[i].Misc.VirtualSize, s[i].SizeOfRawData)});
    const IMAGE_DATA_DIRECTORY& ex = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    if (ex.VirtualAddress && ex.Size) {
        g_pdata = reinterpret_cast<const RUNTIME_FUNCTION*>(base + ex.VirtualAddress);
        g_pdataN = ex.Size / sizeof(RUNTIME_FUNCTION);
    }
}

// start RVA of the .pdata entry containing rva (a function or a chained fragment of one); rva itself when none
uint32_t FuncStart(uint32_t rva) {
    size_t lo = 0, hi = g_pdataN;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (g_pdata[mid].BeginAddress <= rva) lo = mid + 1;
        else hi = mid;
    }
    if (lo == 0) return rva;
    const RUNTIME_FUNCTION& f = g_pdata[lo - 1];
    return rva < f.EndAddress ? f.BeginAddress : rva;
}

// does a call instruction end right before ret (a return address into Nioh3.exe code)?
bool IsReturnAddress(uintptr_t ret) {
    if (!InExec(ret) || !InExec(ret - 7)) return false;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(ret);
    if (p[-5] == 0xE8) return true;                                // call rel32
    if (p[-6] == 0xFF && (p[-5] & 0x38) == 0x10) return true;      // call [mem] modrm+disp32 (incl. FF 15 rip)
    if (p[-2] == 0xFF && (p[-1] & 0xF8) == 0xD0) return true;      // call reg
    if (p[-3] == 0x41 && p[-2] == 0xFF && (p[-1] & 0xF8) == 0xD0) return true;  // call r8-r15
    if (p[-3] == 0xFF && (p[-2] & 0xF8) == 0x50) return true;      // call [reg+disp8]
    if (p[-7] == 0xFF && (p[-6] & 0xF8) == 0x90) return true;      // call [reg+disp32] (after a REX byte)
    return false;
}

// ---- modules (for RIP outside Nioh3.exe) ----------------------------------------------------------------------------
struct Mod {
    uintptr_t lo, hi;
    char name[48];
};
std::vector<Mod> g_mods;  // append-only, so indices stay stable for the counts
using EnumModsFn = BOOL(WINAPI*)(HANDLE, HMODULE*, DWORD, LPDWORD);
using ModInfoFn = BOOL(WINAPI*)(HANDLE, HMODULE, void*, DWORD);
EnumModsFn g_enumMods = nullptr;
ModInfoFn g_modInfo = nullptr;
struct ModInfo {
    LPVOID base;
    DWORD size;
    LPVOID entry;
};

void RefreshModules() {
    if (!g_enumMods || !g_modInfo) return;
    HMODULE mods[1024];
    DWORD need = 0;
    if (!g_enumMods(GetCurrentProcess(), mods, sizeof(mods), &need)) return;
    const DWORD n = std::min<DWORD>(need / sizeof(HMODULE), 1024);
    for (DWORD i = 0; i < n && g_mods.size() < 1000; ++i) {
        ModInfo mi{};
        if (!g_modInfo(GetCurrentProcess(), mods[i], &mi, sizeof(mi))) continue;
        const uintptr_t lo = reinterpret_cast<uintptr_t>(mi.base);
        bool known = false;
        for (const Mod& m : g_mods)
            if (m.lo == lo) known = true;
        if (known) continue;
        Mod m{lo, lo + mi.size, {}};
        wchar_t path[MAX_PATH] = {};
        GetModuleFileNameW(mods[i], path, MAX_PATH);
        const wchar_t* leaf = wcsrchr(path, L'\\');
        leaf = leaf ? leaf + 1 : path;
        WideCharToMultiByte(CP_UTF8, 0, leaf, -1, m.name, sizeof(m.name) - 1, nullptr, nullptr);
        g_mods.push_back(m);
    }
}

constexpr uint32_t kNoMod = 0xFFFF;
uint32_t ModIndex(uintptr_t a) {
    for (size_t i = 0; i < g_mods.size(); ++i)
        if (a >= g_mods[i].lo && a < g_mods[i].hi) return (uint32_t)i;
    return kNoMod;
}

// ---- threads ----------------------------------------------------------------------------------------------------------
enum Group : int { kMain = 0, kKidsTS, kKidsTs2, kDispatch, kAppOther, kLogic, kNumGroups };
const char* kGroupName[kNumGroups] = {"main [App]Game", "KIDSTaskScheduler", "KIDSTS", "SynergisticDispatcher", "[App] other",
                                      "logic KIDSTaskScheduler0"};
const char* kGroupShort[kNumGroups] = {"main", "kidsTS", "kidsts", "dispatch", "app", "logic"};
// groups sampled every tick (Focus = 1: main + logic + dispatcher; else main only)
bool EveryTick(int g) { return g == kMain || (g_cfg.focus && (g == kLogic || g == kDispatch)); }

struct Thr {
    DWORD tid;
    HANDLE h;
    int group;
};
std::vector<Thr> g_thr;
using GetDescFn = HRESULT(WINAPI*)(HANDLE, PWSTR*);
GetDescFn g_getDesc = nullptr;

int Classify(const wchar_t* d) {
    if (!wcscmp(d, L"[App]Game")) return kMain;
    if (g_cfg.focus && !wcscmp(d, L"KIDSTaskScheduler0")) return kLogic;
    if (!wcsncmp(d, L"KIDSTaskSchedul", 15)) return kKidsTS;
    if (!wcsncmp(d, L"KIDSTS", 6)) return kKidsTs2;
    if (!wcsncmp(d, L"Synergistic", 11)) return kDispatch;
    if (!wcsncmp(d, L"[App]", 5)) return kAppOther;
    return -1;
}

void RefreshThreads() {
    // drop exited threads
    for (size_t i = 0; i < g_thr.size();) {
        DWORD code = 0;
        if (!GetExitCodeThread(g_thr[i].h, &code) || code != STILL_ACTIVE) {
            CloseHandle(g_thr[i].h);
            g_thr.erase(g_thr.begin() + (ptrdiff_t)i);
        } else {
            ++i;
        }
    }
    if (!g_getDesc) return;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    THREADENTRY32 te{};
    te.dwSize = sizeof(te);
    const DWORD pid = GetCurrentProcessId(), self = GetCurrentThreadId();
    for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
        if (te.th32OwnerProcessID != pid || te.th32ThreadID == self) continue;
        bool known = false;
        for (const Thr& t : g_thr)
            if (t.tid == te.th32ThreadID) known = true;
        if (known) continue;
        HANDLE q = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, te.th32ThreadID);
        if (!q) continue;
        int grp = -1;
        PWSTR d = nullptr;
        if (SUCCEEDED(g_getDesc(q, &d)) && d) {
            grp = Classify(d);
            LocalFree(d);
        }
        CloseHandle(q);
        if (grp < 0 || (!EveryTick(grp) && !g_cfg.workers)) continue;
        HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_LIMITED_INFORMATION, FALSE, te.th32ThreadID);
        if (h) g_thr.push_back({te.th32ThreadID, h, grp});
    }
    CloseHandle(snap);
}

// ---- counts -----------------------------------------------------------------------------------------------------------
// key: group (8 bits) | kind (8) | module index (16) | function RVA (32)
//   kind 0: RIP in Nioh3.exe, fn = its .pdata start
//   kind 1: RIP in another module (or none), fn = first game caller found on the stack
//   kind 2: RIP elsewhere and no game caller on the copied stack
constexpr size_t kSlots = 1 << 15;
struct Slot {
    uint64_t key;
    uint32_t n;
};
Slot* g_tab = nullptr;
uint64_t g_total[kNumGroups] = {}, g_inGame[kNumGroups] = {}, g_failed = 0;

void Count(uint64_t key) {
    size_t i = (size_t)((key * 0x9E3779B97F4A7C15ull) >> 49) & (kSlots - 1);
    for (size_t probe = 0; probe < kSlots; ++probe, i = (i + 1) & (kSlots - 1)) {
        if (g_tab[i].n == 0) {
            g_tab[i].key = key;
            g_tab[i].n = 1;
            return;
        }
        if (g_tab[i].key == key) {
            ++g_tab[i].n;
            return;
        }
    }
}

uint64_t Key(int group, int kind, uint32_t mod, uint32_t rva) {
    return ((uint64_t)group << 56) | ((uint64_t)kind << 48) | ((uint64_t)(mod & 0xFFFF) << 32) | rva;
}

std::vector<uint8_t> g_stack;

void SampleOne(const Thr& t) {
    alignas(16) CONTEXT ctx{};
    ctx.ContextFlags = CONTEXT_CONTROL;
    SIZE_T got = 0;
    if (SuspendThread(t.h) == (DWORD)-1) {
        ++g_failed;
        return;
    }
    // only kernel calls between Suspend and Resume: no heap, no loader lock
    const BOOL okCtx = GetThreadContext(t.h, &ctx);
    if (okCtx) {
        SIZE_T want = g_stack.size();
        for (int tries = 0; tries < 3 && want >= 64; ++tries, want /= 4)
            if (ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<LPCVOID>(ctx.Rsp), g_stack.data(), want, &got)) break;
            else got = 0;
    }
    ResumeThread(t.h);
    if (!okCtx) {
        ++g_failed;
        return;
    }
    ++g_total[t.group];
    const uintptr_t rip = ctx.Rip;
    if (rip >= g_base && rip < g_end) {
        ++g_inGame[t.group];
        Count(Key(t.group, 0, 0, FuncStart((uint32_t)(rip - g_base))));
        return;
    }
    const uint32_t mod = ModIndex(rip);
    for (SIZE_T off = 0; off + 8 <= got; off += 8) {
        uintptr_t v;
        memcpy(&v, g_stack.data() + off, 8);
        if (v >= g_base && v < g_end && IsReturnAddress(v)) {
            Count(Key(t.group, 1, mod, FuncStart((uint32_t)(v - g_base))));
            return;
        }
    }
    Count(Key(t.group, 2, mod, 0));
}

void Report(int seconds) {
    std::vector<Slot> all;
    for (size_t i = 0; i < kSlots; ++i)
        if (g_tab[i].n) all.push_back(g_tab[i]);
    std::sort(all.begin(), all.end(), [](const Slot& a, const Slot& b) { return a.n > b.n; });
    for (int g = 0; g < kNumGroups; ++g) {
        if (!g_total[g]) continue;
        int threads = 0;
        for (const Thr& t : g_thr) threads += t.group == g;
        SLOG("SP %s: %llu samples in %d s from %d thread(s), RIP in Nioh3.exe %.1f %% (the rest: system/driver code, "
             "attributed to the calling game function)",
             kGroupName[g], (unsigned long long)g_total[g], seconds, threads, 100.0 * g_inGame[g] / g_total[g]);
        int shown = 0;
        for (const Slot& s : all) {
            if ((int)(s.key >> 56) != g) continue;
            if (shown++ >= g_cfg.topN) break;
            const int kind = (int)((s.key >> 48) & 0xFF);
            const uint32_t mod = (uint32_t)((s.key >> 32) & 0xFFFF), rva = (uint32_t)s.key;
            const double pct = 100.0 * s.n / g_total[g];
            const char* mn = mod < g_mods.size() ? g_mods[mod].name : "unix/other";
            if (kind == 0) SLOG("SP %-8s %5.1f %%  fn 0x%X", kGroupShort[g], pct, rva);
            else if (kind == 1) SLOG("SP %-8s %5.1f %%  [%s] <- fn 0x%X", kGroupShort[g], pct, mn, rva);
            else SLOG("SP %-8s %5.1f %%  [%s] (no game caller found)", kGroupShort[g], pct, mn);
        }
    }
    if (g_failed) SLOG("SP: %llu samples failed (thread gone or not suspendable)", (unsigned long long)g_failed);
    memset(g_tab, 0, sizeof(Slot) * kSlots);
    memset(g_total, 0, sizeof(g_total));
    memset(g_inGame, 0, sizeof(g_inGame));
    g_failed = 0;
}

DWORD WINAPI SamplerThread(LPVOID) {
    Sleep((DWORD)g_cfg.startDelaySec * 1000);
    g_tab = static_cast<Slot*>(VirtualAlloc(nullptr, sizeof(Slot) * kSlots, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!g_tab) {
        SLOG("SP: out of memory - profiler off");
        return 0;
    }
    g_stack.resize((size_t)g_cfg.stackBytes);
    HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, 0x00000002 /*CREATE_WAITABLE_TIMER_HIGH_RESOLUTION*/, TIMER_ALL_ACCESS);
    SLOG("SP: sampling every %d us (%s timer), workers %s, report every %d s, %zu .pdata entries%s",
         g_cfg.intervalUs, timer ? "high-resolution" : "Sleep", g_cfg.workers ? "on (round-robin)" : "off",
         g_cfg.reportSec, g_pdataN, g_cfg.focus ? ", Focus: [App]Game + KIDSTaskScheduler0 + dispatcher every tick" : "");
    LARGE_INTEGER f, t0, now;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t0);
    LONGLONG lastRefresh = 0;
    size_t rr = 0;
    bool loggedThreads = false;
    for (;;) {
        QueryPerformanceCounter(&now);
        if (g_ext) {  // Tour windows
            if (InterlockedExchange(&g_reqReport, 0)) {
                SLOG("SP window %s", g_winTag);
                Report((int)((double)(now.QuadPart - t0.QuadPart) / (double)f.QuadPart + 0.5));
                t0 = now;
            }
            if (InterlockedExchange(&g_reqReset, 0)) {
                memset(g_tab, 0, sizeof(Slot) * kSlots);
                memset(g_total, 0, sizeof(g_total));
                memset(g_inGame, 0, sizeof(g_inGame));
                g_failed = 0;
                t0 = now;
            }
            if (!g_sampling) {
                Sleep(5);
                continue;
            }
        }
        if (lastRefresh == 0 || now.QuadPart - lastRefresh > f.QuadPart * 10) {
            RefreshThreads();
            RefreshModules();
            lastRefresh = now.QuadPart;
            if (!loggedThreads && !g_thr.empty()) {
                int c[kNumGroups] = {};
                for (const Thr& t : g_thr) ++c[t.group];
                SLOG("SP: threads found - main %d, KIDSTaskScheduler %d, KIDSTS %d, dispatcher %d, other [App] %d, logic (Focus) %d", c[kMain],
                     c[kKidsTS], c[kKidsTs2], c[kDispatch], c[kAppOther], c[kLogic]);
                loggedThreads = c[kMain] > 0;
            }
        }
        // the main thread (+ logic + dispatcher with Focus) every tick, plus one other thread round-robin
        for (const Thr& t : g_thr)
            if (EveryTick(t.group)) SampleOne(t);
        if (g_thr.size() > 1) {
            for (size_t k = 0; k < g_thr.size(); ++k) {
                const Thr& t = g_thr[(rr + k) % g_thr.size()];
                if (!EveryTick(t.group)) {
                    SampleOne(t);
                    rr = (rr + k + 1) % g_thr.size();
                    break;
                }
            }
        }
        const double el = (double)(now.QuadPart - t0.QuadPart) / (double)f.QuadPart;
        if (!g_ext && el >= g_cfg.reportSec) {
            Report((int)el);
            QueryPerformanceCounter(&t0);
        }
        if (timer) {
            LARGE_INTEGER due;
            due.QuadPart = -(LONGLONG)g_cfg.intervalUs * 10;
            if (SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE)) WaitForSingleObject(timer, 100);
            else Sleep(1);
        } else {
            Sleep(std::max(1, g_cfg.intervalUs / 1000));
        }
    }
}

}  // namespace

void SetExternalWindows(bool on) { InterlockedExchange(&g_ext, on ? 1 : 0); }
void BeginWindow() {
    InterlockedExchange(&g_reqReset, 1);
    InterlockedExchange(&g_sampling, 1);
}
void EndWindow(const char* tag) {
    InterlockedExchange(&g_sampling, 0);
    snprintf(g_winTag, sizeof(g_winTag), "%s", tag ? tag : "");
    InterlockedExchange(&g_reqReport, 1);
}

void Start(const StartInfo& info) {
    g_log = info.log;
    if (!info.iniPath || !info.base) return;
    auto I = [&](const wchar_t* key, int def) { return (int)GetPrivateProfileIntW(L"Profiler", key, def, info.iniPath); };
    if (!I(L"SP", 0)) return;
    g_cfg.intervalUs = std::clamp(I(L"IntervalUs", g_cfg.intervalUs), 200, 100000);
    g_cfg.workers = I(L"Workers", g_cfg.workers);
    g_cfg.reportSec = std::clamp(I(L"ReportSec", g_cfg.reportSec), 5, 600);
    g_cfg.topN = std::clamp(I(L"TopN", g_cfg.topN), 5, 200);
    g_cfg.stackBytes = std::clamp(I(L"StackBytes", g_cfg.stackBytes), 256, 16384) & ~7;
    g_cfg.startDelaySec = std::clamp(I(L"StartDelaySec", g_cfg.startDelaySec), 0, 600);
    g_cfg.focus = I(L"Focus", 0) == 1 ? 1 : 0;
    InitImage(info.base);
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    HMODULE kb = GetModuleHandleW(L"kernelbase.dll");
    g_enumMods = reinterpret_cast<EnumModsFn>(GetProcAddress(k32, "K32EnumProcessModules"));
    g_modInfo = reinterpret_cast<ModInfoFn>(GetProcAddress(k32, "K32GetModuleInformation"));
    g_getDesc = reinterpret_cast<GetDescFn>(GetProcAddress(kb ? kb : k32, "GetThreadDescription"));
    if (!g_getDesc) g_getDesc = reinterpret_cast<GetDescFn>(GetProcAddress(k32, "GetThreadDescription"));
    if (!g_getDesc) {
        SLOG("SP: GetThreadDescription not available - can't find the game's threads, profiler off");
        return;
    }
    if (HANDLE h = CreateThread(nullptr, 0, SamplerThread, nullptr, 0, nullptr)) {
        SetThreadPriority(h, THREAD_PRIORITY_ABOVE_NORMAL);  // so a busy CPU doesn't skew the sampling interval
        CloseHandle(h);
        SLOG("SP: profiler armed (starts after %d s)", g_cfg.startDelaySec);
    }
}

}  // namespace sp
