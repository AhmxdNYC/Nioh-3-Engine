#pragma once
// framewaitpark.h - FWP (#101247): the policy behind caveFwp, header-only so tests/fwp_harness.cpp runs the same code.
// Included exactly once per binary (definitions, not declarations).
#include <windows.h>
#include <intrin.h>
#include <cstdint>

// FWP (#101247, note 480): RenderWaitForFrame's job spin 4A4F1C polls bit 0 of [job] (job = [obj+1C8h]), calls the
// kick 3EB04 (SetEvent on the job's worker event) on EVERY poll and Sleep(0)s only every 256th. caveFwp replaces that
// cold block: kick once, poll with pause for FWP_SpinUs, then sleep on an own high-resolution timer in FWP_SleepUs
// steps (re-kicking after each sleep) until bit 0 clears. The game's own recheck at 4A4F3B still decides; nothing is
// skipped. FWP = 2 is measure-only: the stock loop (kick per poll, Sleep(0) every 256th), timed.
volatile LONG g_fwpCalls = 0, g_fwpParks = 0, g_fwpKicks = 0, g_fwpBounded = 0;
volatile LONG64 g_fwpTicks = 0, g_fwpMaxTicks = 0;
int g_fwpMode = 0, g_fwpSpinUs = 200, g_fwpSleepUs = 100;
bool g_fwpOn = false;
HANDLE g_fwpTimer = nullptr;
volatile LONG g_fwpTid = 0;
int64_t g_fwpQpf = 0;
void (*g_fwpKick)(void*) = nullptr;
static inline bool FwpBusy(uint8_t* obj) {
    const volatile LONG* job = *reinterpret_cast<const volatile LONG* volatile*>(obj + 0x1C8);
    return job && (*job & 1);
}
static inline void FwpKick(uint8_t* obj) {
    void* job = *reinterpret_cast<void* volatile*>(obj + 0x1C8);
    if (job && g_fwpKick) { g_fwpKick(job); InterlockedIncrement(&g_fwpKicks); }
}
extern "C" void FwpRound(uint8_t* obj) {
    LARGE_INTEGER t0, t1;
    QueryPerformanceCounter(&t0);
    const int64_t f = g_fwpQpf > 0 ? g_fwpQpf : 10000000;
    const int64_t bound = f / 4;  // 250 ms: return to the native loop anyway (it re-enters here if still busy)
    InterlockedIncrement(&g_fwpCalls);
    const LONG tid = (LONG)GetCurrentThreadId();
    bool own = g_fwpTid == tid;
    if (!own && !g_fwpTid) own = InterlockedCompareExchange(&g_fwpTid, tid, 0) == 0;
    if (g_fwpMode == 2 || !g_fwpTimer || !own) {
        // stock shape: kick per poll, Sleep(0) after 256 polls (the timer object belongs to one thread only)
        for (;;) {
            FwpKick(obj);
            int i = 0;
            while (++i < 256 && FwpBusy(obj)) FwpKick(obj);
            if (!FwpBusy(obj)) break;
            Sleep(0);
            QueryPerformanceCounter(&t1);
            if (t1.QuadPart - t0.QuadPart > bound) { InterlockedIncrement(&g_fwpBounded); break; }
        }
    } else {
        FwpKick(obj);
        const int64_t spinEnd = t0.QuadPart + f * g_fwpSpinUs / 1000000;
        for (;;) {
            for (int i = 0; i < 32; ++i) _mm_pause();
            if (!FwpBusy(obj)) break;
            QueryPerformanceCounter(&t1);
            if (t1.QuadPart >= spinEnd) break;
        }
        while (FwpBusy(obj)) {
            LARGE_INTEGER due;
            due.QuadPart = -(int64_t)g_fwpSleepUs * 10;
            if (!SetWaitableTimer(g_fwpTimer, &due, 0, nullptr, nullptr, FALSE)) Sleep(0);
            else WaitForSingleObject(g_fwpTimer, 2);
            InterlockedIncrement(&g_fwpParks);
            if (!FwpBusy(obj)) break;
            FwpKick(obj);
            QueryPerformanceCounter(&t1);
            if (t1.QuadPart - t0.QuadPart > bound) { InterlockedIncrement(&g_fwpBounded); break; }
        }
    }
    QueryPerformanceCounter(&t1);
    const int64_t d = t1.QuadPart - t0.QuadPart;
    InterlockedAdd64(&g_fwpTicks, d);
    LONG64 m = g_fwpMaxTicks;
    while (d > m && InterlockedCompareExchange64(&g_fwpMaxTicks, d, m) != m) m = g_fwpMaxTicks;
}
