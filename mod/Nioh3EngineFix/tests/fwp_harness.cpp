// fwp_harness.cpp - offline check of FWP (#101247, src/framewaitpark.h): the policy behind caveFwp.
// A worker thread clears bit 0 of a fake job after D us (busy-timed, like the game's job worker); the caller runs
// FwpRound in mode 2 (stock shape: kick per poll, Sleep(0) every 256th) and mode 1 (kick once + timer park).
// Checks: FwpRound returns only once bit 0 is clear; mode 1 kicks far less; caller CPU cycles per wait drop for waits
// longer than the spin window; prints the added wake latency (return time - clear time) so the cost is visible.
// Build + run: py -I tests/run_fwp_harness.py
#include "../src/framewaitpark.h"
#include <cstdio>
#include <vector>
#include <algorithm>

static volatile LONG g_testKicks = 0;
static void TestKick(void*) { InterlockedIncrement(&g_testKicks); }

struct Job { volatile LONG flags; void* holder; };
alignas(64) static uint8_t g_obj[0x200];
static Job g_job;
static volatile LONG64 g_clearAt = 0;   // QPC when the worker cleared bit 0
static volatile LONG g_go = 0, g_delayUs = 0, g_quit = 0;
static int64_t g_f = 0;

static int64_t Now() { LARGE_INTEGER q; QueryPerformanceCounter(&q); return q.QuadPart; }

static DWORD WINAPI Worker(void*) {
    for (;;) {
        while (!g_go) { if (g_quit) return 0; YieldProcessor(); }
        const int64_t until = Now() + g_f * g_delayUs / 1000000;
        while (Now() < until) YieldProcessor();
        InterlockedAnd(&g_job.flags, ~1L);
        g_clearAt = Now();
        g_go = 0;
    }
}

struct Result { double cyclesMedian, latUsMedian, latUsP95; LONG kicks; int early; };

static Result Run(int mode, int delayUs, int reps) {
    g_fwpMode = mode;
    std::vector<double> cyc, lat;
    int early = 0;
    const LONG k0 = g_testKicks;
    HANDLE self = GetCurrentThread();
    for (int r = 0; r < reps; ++r) {
        g_job.flags = 1;
        g_delayUs = delayUs;
        ULONG64 c0 = 0, c1 = 0;
        QueryThreadCycleTime(self, &c0);
        g_go = 1;
        FwpRound(g_obj);
        const int64_t back = Now();
        QueryThreadCycleTime(self, &c1);
        if (g_job.flags & 1) ++early;  // returned while still busy (only allowed via the 250 ms bound)
        while (g_go) YieldProcessor();
        cyc.push_back((double)(c1 - c0));
        lat.push_back((double)(back - g_clearAt) * 1e6 / (double)g_f);
    }
    std::sort(cyc.begin(), cyc.end());
    std::sort(lat.begin(), lat.end());
    return {cyc[cyc.size() / 2], lat[lat.size() / 2], lat[lat.size() * 95 / 100], g_testKicks - k0, early};
}

int main() {
    LARGE_INTEGER q; QueryPerformanceFrequency(&q); g_f = q.QuadPart; g_fwpQpf = g_f;
    g_fwpTimer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!g_fwpTimer) { printf("FAIL no high-resolution timer\n"); return 1; }
    g_fwpKick = TestKick;
    g_job.holder = &g_job;
    *reinterpret_cast<Job**>(g_obj + 0x1C8) = &g_job;
    HANDLE w = CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);
    int fails = 0;
    const int delays[] = {5, 50, 300, 1000, 3000};
    printf("%-8s %-6s %14s %12s %12s %10s\n", "delay_us", "mode", "cycles_median", "lat_us_med", "lat_us_p95", "kicks/wait");
    for (int d : delays) {
        const int reps = d >= 1000 ? 100 : 300;
        Result s = Run(2, d, reps), p = Run(1, d, reps);
        printf("%-8d %-6s %14.0f %12.1f %12.1f %10.1f\n", d, "stock", s.cyclesMedian, s.latUsMedian, s.latUsP95, (double)s.kicks / reps);
        printf("%-8d %-6s %14.0f %12.1f %12.1f %10.1f\n", d, "park", p.cyclesMedian, p.latUsMedian, p.latUsP95, (double)p.kicks / reps);
        if (s.early || p.early) { printf("FAIL delay %d: returned while bit 0 still set (stock %d, park %d)\n", d, s.early, p.early); ++fails; }
        if (d >= 1000 && !(p.cyclesMedian < s.cyclesMedian * 0.5)) { printf("FAIL delay %d: park does not halve caller cycles\n", d); ++fails; }
        if (d >= 300 && !(p.kicks * 10 < s.kicks)) { printf("FAIL delay %d: park kicks not 10x fewer\n", d); ++fails; }
    }
    // job pointer null / bit already clear: returns at once, no kick needed for correctness
    g_job.flags = 0; g_fwpMode = 1;
    FwpRound(g_obj);
    *reinterpret_cast<Job**>(g_obj + 0x1C8) = nullptr;
    FwpRound(g_obj);
    printf("PASS null job / clear bit returns\n");
    g_quit = 1; WaitForSingleObject(w, 1000);
    if (fails) { printf("%d FAILURES\n", fails); return 1; }
    printf("ALL PASSED\n");
    return 0;
}
