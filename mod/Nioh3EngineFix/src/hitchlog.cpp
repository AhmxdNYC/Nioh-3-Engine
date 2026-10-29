// hitchlog.cpp - [Debug] HitchLog: one line per game-thread frame gap over HitchLogMs. See hitchlog.h.
// Line (grep HITCH; parsed by tools\hitch_from_log.py, keep the field names stable):
//   HITCH #3 gap 812.4 ms | gt cpu 9.8 ms 1% | proc cpu 140.2 ms 0.17 cores | pf 1234 | rd 12.3 MB 45 ops | wr 0.1 MB |
//   last GF 790.1 ms end 0.4 ms ago | max GF 790.1 ms tid 1234 game | waits W3 0 L1 1 F1 0 W6 1/2.1 GF 3/790.5 DS 0/0.0 |
//   bks ecl 0.0 idle 0.3 recyc 0.0 tok95 1.2 | frame drain 0.4/0.2 full 0.0 hw 0.0 sync 0.0/0.0 | pj pso 3 miss 0 |
//   menu 0 pause 0 mode 0 act 0 | ld A 3/10 B -/- infl 1 | ram 3300/31890 MB load 89% | commit 41000/46000 MB |
//   ws 9100 MB | vram 7012/7600 MB | t 412.3 s
#include <windows.h>
#include <psapi.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include <cstdint>
#include <cstdio>
#include <cwchar>

#include "bkstats.h"
#include "hitchlog.h"
#include "psojournal.h"
#include "loadstats.h"
#include "memstats.h"
#include "menu.h"

namespace hitchlog {

volatile long g_on = 0;

namespace {

void (*g_log)(const char*, ...) = nullptr;
int64_t g_freq = 1;
int64_t g_threshTicks = 0;
int64_t g_t0 = 0;
SRWLOCK g_lock = SRWLOCK_INIT;
const char* const kStageName[kNumStages] = {"-", "W3", "L1", "F1", "W6", "GF", "DS"};

Counters g_c;  // the mod's existing wait counters (dllmain)
// longest single wait since the last game present: (ticks << 4) | stage, plus the thread that waited
volatile long long g_maxPack = 0;
volatile DWORD g_maxTid = 0;
// last wait on the game's presenting thread
volatile DWORD g_gameTid = 0;
volatile long g_lastStage = 0;
volatile long long g_lastTicks = 0, g_lastEnd = 0;

struct Prev {
    bool valid = false;
    DWORD tid = 0;
    int64_t qpc = 0;
    uint64_t thrCpu = 0, procCpu = 0;  // 100 ns
    DWORD faults = 0;
    uint64_t rdBytes = 0, rdOps = 0, wrBytes = 0;
    long w[kNumStages] = {};          // existing counters W3 L1 F1 W6 GF DS (index = Stage)
    long long wt[kNumStages] = {};    // their QPC tick totals where the mod keeps one (W6 GF DS)
    bool bk = false;                  // engine back-end cumulative waits read
    double bkMs[4] = {};              // bkstats::CumWaitsMs
    uint64_t pjSeen = 0, pjMiss = 0;  // PJ: engine PSO creates seen / handover misses
    int pjMode = 0;
};
long Rd(volatile long* p) { return p ? *p : 0; }
long long Rd(volatile long long* p) { return p ? *p : 0; }
Prev g_prev;
long g_hitches = 0;

IDXGIAdapter3* volatile g_adapter = nullptr;

uint64_t Ft(const FILETIME& f) { return (uint64_t)f.dwHighDateTime << 32 | f.dwLowDateTime; }

using CreateFactory1Fn = HRESULT(WINAPI*)(REFIID, void**);

// the system dxgi.dll (skips OptiScaler / Streamline proxies), hardware adapter with the most dedicated VRAM
DWORD WINAPI AdapterThread(void*) {
    Sleep(20000);  // well after the game made its device; never from the Present hook
    wchar_t path[MAX_PATH];
    const UINT n = GetSystemDirectoryW(path, MAX_PATH);
    if (!n || n + 10 >= MAX_PATH) return 0;
    wcscat_s(path, L"\\dxgi.dll");
    HMODULE m = GetModuleHandleW(path);
    if (!m) m = LoadLibraryW(path);
    auto create = m ? reinterpret_cast<CreateFactory1Fn>(GetProcAddress(m, "CreateDXGIFactory1")) : nullptr;
    IDXGIFactory1* f = nullptr;
    if (!create || FAILED(create(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&f))) || !f) {
        if (g_log) g_log("HitchLog: no DXGI factory - VRAM not in the hitch lines");
        return 0;
    }
    IDXGIAdapter1* best = nullptr;
    SIZE_T bestMem = 0;
    IDXGIAdapter1* a = nullptr;
    for (UINT i = 0; f->EnumAdapters1(i, &a) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 d{};
        if (SUCCEEDED(a->GetDesc1(&d)) && !(d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) && (!best || d.DedicatedVideoMemory > bestMem)) {
            if (best) best->Release();
            best = a;
            bestMem = d.DedicatedVideoMemory;
        } else {
            a->Release();
        }
    }
    f->Release();
    IDXGIAdapter3* a3 = nullptr;
    if (best && SUCCEEDED(best->QueryInterface(__uuidof(IDXGIAdapter3), reinterpret_cast<void**>(&a3))) && a3) g_adapter = a3;
    if (best) best->Release();
    if (g_log) g_log("HitchLog: VRAM source %s (%.0f MB dedicated)", g_adapter ? "found" : "not found", (double)bestMem / 1048576.0);
    return 0;
}

bool Vram(double* used, double* budget) {
    double nlU = 0, nlB = 0;
    if (memstats::Vram(used, budget, &nlU, &nlB)) return true;
    IDXGIAdapter3* a = g_adapter;
    if (!a) return false;
    DXGI_QUERY_VIDEO_MEMORY_INFO i{};
    if (FAILED(a->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &i))) return false;
    *used = (double)i.CurrentUsage / 1048576.0;
    *budget = (double)i.Budget / 1048576.0;
    return true;
}

// Nioh3Training_SessionMode: 1 Boss Match, 2 Practice; SessionActive bit 2 = summoned bosses alive (as menu.cpp TrainingMode)
void TrainingState(int* mode, int* active) {
    static int (*fMode)() = nullptr;
    static int (*fActive)() = nullptr;
    static ULONGLONG tried = 0;
    if (!fMode && (!tried || GetTickCount64() - tried > 5000)) {
        tried = GetTickCount64();
        if (HMODULE m = GetModuleHandleW(L"Nioh3Training.asi")) {
            fMode = reinterpret_cast<int (*)()>(GetProcAddress(m, "Nioh3Training_SessionMode"));
            fActive = reinterpret_cast<int (*)()>(GetProcAddress(m, "Nioh3Training_SessionActive"));
        }
    }
    *mode = fMode ? fMode() : -1;
    *active = fActive ? fActive() : -1;
}

double Ms(int64_t ticks) { return (double)ticks * 1000.0 / (double)g_freq; }

void LogHitch(int64_t now, int64_t gap, const Prev& cur, long long maxPack, DWORD maxTid) {
    ++g_hitches;
    const double gapMs = Ms(gap);
    const double gtCpu = (double)(cur.thrCpu - g_prev.thrCpu) / 1e4;
    const double prCpu = (double)(cur.procCpu - g_prev.procCpu) / 1e4;
    char waits[200];
    int w = 0;
    const bool haveTicks[kNumStages] = {false, false, false, false, g_c.w6Ticks != nullptr, g_c.gfTicks != nullptr, g_c.dsTicks != nullptr};
    for (int s = 1; s < kNumStages && w < (int)sizeof(waits); ++s) {
        const long n = cur.w[s] - g_prev.w[s];
        w += haveTicks[s] ? snprintf(waits + w, sizeof(waits) - w, "%s%s %ld/%.1f", s > 1 ? " " : "", kStageName[s], n, Ms(cur.wt[s] - g_prev.wt[s]))
                          : snprintf(waits + w, sizeof(waits) - w, "%s%s %ld", s > 1 ? " " : "", kStageName[s], n);
    }
    // engine back-end waits (bkstats: KTGL's own accumulators): cumulative delta over the gap + the last frame's snapshot
    char bk[200];
    double lf[6];
    const bool haveLf = bkstats::LastFrameWaitsMs(lf);
    if (cur.bk && g_prev.bk)
        snprintf(bk, sizeof(bk), "ecl %.1f idle %.1f recyc %.1f tok95 %.1f", cur.bkMs[0] - g_prev.bkMs[0], cur.bkMs[1] - g_prev.bkMs[1],
                 cur.bkMs[2] - g_prev.bkMs[2], cur.bkMs[3] - g_prev.bkMs[3]);
    else
        snprintf(bk, sizeof(bk), "ecl - idle - recyc - tok95 -");
    char fr[160];
    if (haveLf)
        snprintf(fr, sizeof(fr), "drain %.1f/%.1f full %.1f hw %.1f sync %.1f/%.1f", lf[0], lf[1], lf[2], lf[3], lf[4], lf[5]);
    else
        snprintf(fr, sizeof(fr), "drain - full - hw - sync -");
    char pjb[80];
    if (cur.pjMode)
        snprintf(pjb, sizeof(pjb), "pso %llu miss %llu", (unsigned long long)(cur.pjSeen - g_prev.pjSeen),
                 (unsigned long long)(cur.pjMiss - g_prev.pjMiss));
    else
        snprintf(pjb, sizeof(pjb), "pso - miss -");
    const int ls = (int)g_lastStage;
    char last[80];
    if (ls > 0 && ls < kNumStages && g_lastEnd)
        snprintf(last, sizeof(last), "%s %.1f ms end %.1f ms ago", kStageName[ls], Ms(g_lastTicks), Ms(now - g_lastEnd));
    else
        snprintf(last, sizeof(last), "- 0.0 ms end - ms ago");
    const int ms = (int)(maxPack & 15);
    char mx[80];
    if (maxPack && ms > 0 && ms < kNumStages)
        snprintf(mx, sizeof(mx), "%s %.1f ms tid %lu %s", kStageName[ms], Ms(maxPack >> 4), (unsigned long)maxTid,
                 maxTid == cur.tid ? "game" : "other");
    else
        snprintf(mx, sizeof(mx), "- 0.0 ms tid 0 -");
    int mode = -1, active = -1;
    TrainingState(&mode, &active);
    int act[2], pend[2];
    char ld[80];
    if (loadstats::LoaderQueues(act, pend)) {
        char a0[24], b0[24];
        snprintf(a0, sizeof(a0), act[0] >= 0 ? "%d/%d" : "-/-", act[0], pend[0]);
        snprintf(b0, sizeof(b0), act[1] >= 0 ? "%d/%d" : "-/-", act[1], pend[1]);
        snprintf(ld, sizeof(ld), "A %s B %s infl %d", a0, b0, loadstats::InflatesNow());
    } else {
        snprintf(ld, sizeof(ld), "A -/- B -/- infl -");
    }
    MEMORYSTATUSEX ms2{};
    ms2.dwLength = sizeof(ms2);
    const bool haveRam = GlobalMemoryStatusEx(&ms2) != 0;
    PROCESS_MEMORY_COUNTERS pmc{};
    pmc.cb = sizeof(pmc);
    const bool havePm = GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)) != 0;
    double vu = 0, vb = 0;
    char vram[48];
    if (Vram(&vu, &vb)) snprintf(vram, sizeof(vram), "%.0f/%.0f MB", vu, vb);
    else snprintf(vram, sizeof(vram), "-/- MB");
    constexpr double kMB = 1048576.0;
    g_log("HITCH #%ld gap %.1f ms | gt cpu %.1f ms %.0f%% | proc cpu %.1f ms %.2f cores | pf %lu | rd %.1f MB %llu ops | wr %.1f MB | "
          "last %s | max %s | waits %s | bks %s | frame %s | pj %s | menu %d pause %d mode %d act %d | ld %s | ram %.0f/%.0f MB load %lu%% | "
          "commit %.0f/%.0f MB | ws %.0f MB | vram %s | t %.1f s",
          g_hitches, gapMs, gtCpu, gapMs > 0 ? gtCpu * 100.0 / gapMs : 0.0, prCpu, gapMs > 0 ? prCpu / gapMs : 0.0,
          (unsigned long)(cur.faults - g_prev.faults), (double)(cur.rdBytes - g_prev.rdBytes) / kMB,
          (unsigned long long)(cur.rdOps - g_prev.rdOps), (double)(cur.wrBytes - g_prev.wrBytes) / kMB, last, mx, waits, bk, fr, pjb,
          menu::IsOpen() ? 1 : 0, menu::GamePauseReason(), mode, active, ld,
          haveRam ? (double)ms2.ullAvailPhys / kMB : -1.0, haveRam ? (double)ms2.ullTotalPhys / kMB : -1.0,
          haveRam ? (unsigned long)ms2.dwMemoryLoad : 0ul,
          haveRam ? (double)(ms2.ullTotalPageFile - ms2.ullAvailPageFile) / kMB : -1.0,
          haveRam ? (double)ms2.ullTotalPageFile / kMB : -1.0, havePm ? (double)pmc.WorkingSetSize / kMB : -1.0, vram,
          Ms(now - g_t0) / 1000.0);
}

}  // namespace

void NoteWaitSlow(int stage, int64_t t0, int64_t t1) {
    if (stage <= 0 || stage >= kNumStages) return;
    const int64_t d = t1 - t0;
    if (d < 0) return;
    const DWORD tid = GetCurrentThreadId();
    if (tid == g_gameTid) {
        g_lastStage = stage;
        g_lastTicks = d;
        g_lastEnd = t1;
    }
    const long long pack = (long long)d << 4 | stage;
    for (long long cur = g_maxPack; (pack >> 4) > (cur >> 4); cur = g_maxPack)
        if (InterlockedCompareExchange64(&g_maxPack, pack, cur) == cur) {
            g_maxTid = tid;
            break;
        }
}

void OnGamePresent(int64_t now) {
    if (!g_on || !TryAcquireSRWLockExclusive(&g_lock)) return;
    const DWORD tid = GetCurrentThreadId();
    Prev cur;
    cur.valid = true;
    cur.tid = tid;
    cur.qpc = now;
    FILETIME c, e, k, u;
    if (GetThreadTimes(GetCurrentThread(), &c, &e, &k, &u)) cur.thrCpu = Ft(k) + Ft(u);
    if (GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u)) cur.procCpu = Ft(k) + Ft(u);
    PROCESS_MEMORY_COUNTERS pmc{};
    pmc.cb = sizeof(pmc);
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) cur.faults = pmc.PageFaultCount;
    IO_COUNTERS io{};
    if (GetProcessIoCounters(GetCurrentProcess(), &io)) {
        cur.rdBytes = io.ReadTransferCount;
        cur.rdOps = io.ReadOperationCount;
        cur.wrBytes = io.WriteTransferCount;
    }
    cur.w[kW3] = Rd(g_c.w3), cur.w[kL1] = Rd(g_c.l1), cur.w[kF1] = Rd(g_c.f1);
    cur.w[kW6] = Rd(g_c.w6), cur.w[kGF] = Rd(g_c.gf), cur.w[kDS] = Rd(g_c.ds);
    cur.wt[kW6] = Rd(g_c.w6Ticks), cur.wt[kGF] = Rd(g_c.gfTicks), cur.wt[kDS] = Rd(g_c.dsTicks);
    cur.bk = bkstats::CumWaitsMs(cur.bkMs);
    pj::Stats pjs;
    pj::GetStats(pjs);
    cur.pjMode = pjs.mode;
    cur.pjSeen = pjs.seen;
    cur.pjMiss = pjs.handMiss;
    const long long maxPack = InterlockedExchange64(&g_maxPack, 0);
    const DWORD maxTid = g_maxTid;
    if (g_prev.valid && g_prev.tid == tid) {
        const int64_t gap = now - g_prev.qpc;
        if (gap > g_threshTicks) LogHitch(now, gap, cur, maxPack, maxTid);
    } else if (g_prev.valid && g_log) {
        g_log("HitchLog: game present thread %lu -> %lu", (unsigned long)g_prev.tid, (unsigned long)tid);
    }
    g_gameTid = tid;
    g_prev = cur;
    ReleaseSRWLockExclusive(&g_lock);
}

void Start(const StartInfo& si) {
    g_log = si.log;
    g_c = si.c;
    const int on = si.iniPath ? (int)GetPrivateProfileIntW(L"Debug", L"HitchLog", 1, si.iniPath) : 1;
    int ms = si.iniPath ? (int)GetPrivateProfileIntW(L"Debug", L"HitchLogMs", 100, si.iniPath) : 100;
    if (ms < 20) ms = 20;
    if (ms > 5000) ms = 5000;
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    g_freq = f.QuadPart > 0 ? f.QuadPart : 1;
    g_threshTicks = g_freq * ms / 1000;
    g_t0 = Now();
    if (g_log) g_log("HitchLog = %d, HitchLogMs = %d%s", on, ms, on ? " (one HITCH line per game-thread present gap over the threshold)" : "");
    if (!on) return;
    if (HANDLE h = CreateThread(nullptr, 0, AdapterThread, nullptr, 0, nullptr)) CloseHandle(h);
    InterlockedExchange(&g_on, 1);
}

bool Enabled() { return g_on != 0; }

}  // namespace hitchlog
