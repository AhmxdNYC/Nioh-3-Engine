// WG stress harness: the real WgIndex / WgWake / WgSleep source (extracted verbatim from src\dllmain.cpp into
// wg_extract.inc) driven by a model of the engine's job pools with the v2.02 memory layout:
//   slot = pool->workers + i*458h, [slot] = WorkerThread {+8 event (auto-reset), +10h bits (bit0 = sleeping, wake me)},
//   [slot+440h] = pool, [pool+10h] = workers, [pool+34h] = id.
// Wakers: CAS-clear a sleeper's bit0 (highest index first, like 48C84) then WgWake(event, slot).
// Workers: run until idle (own queue, then steal from all), set bit0 (CAS), re-check the queues (so the STOCK model is
// stall-free), then WgSleep(event, INFINITE, pool, slot).
// Oracle: every pushed job completes; a stall (no completion for 3 s with jobs outstanding) fails the run.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <intrin.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

extern "C" uint64_t ws_iatSetEvent = 0, ws_iatWait = 0;
static BOOL(WINAPI* g_setEv)(HANDLE) = &SetEvent;
static DWORD(WINAPI* g_wait)(HANDLE, DWORD) = &WaitForSingleObject;

namespace {
#include "wg_globals.inc"
}
#include "wg_extract.inc"

struct WorkerThread {
    uint64_t pad0;
    HANDLE ev;      // +8
    volatile LONG bits;  // +10h
};
constexpr int N = 8;
alignas(64) uint8_t g_poolObj[0x100];
alignas(64) uint8_t g_slots[N * 0x458];
WorkerThread g_wt[N];
struct Q { std::mutex m; std::deque<int> d; };
Q g_q[N + 1];  // N = external submitters' queue? no: submitters push into a random worker queue
std::atomic<long long> g_pushed{0}, g_done{0};
std::atomic<bool> g_stop{false};
thread_local std::mt19937 t_rng{std::random_device{}()};

uint8_t* Slot(int i) { return g_slots + i * 0x458; }

bool Pop(int self, int& job) {
    for (int k = 0; k < N; ++k) {
        int j = (self + k) % N;
        std::lock_guard<std::mutex> g(g_q[j].m);
        if (!g_q[j].d.empty()) { job = g_q[j].d.front(); g_q[j].d.pop_front(); return true; }
    }
    return false;
}
bool AnyWork() {
    for (int j = 0; j < N; ++j) { std::lock_guard<std::mutex> g(g_q[j].m); if (!g_q[j].d.empty()) return true; }
    return false;
}
// 48C84 JobWakeOne model: highest-index sleeper, CAS-clear bit0, then the WS3 path (WgWake with the slot)
bool WakeOne() {
    for (int i = N - 1; i >= 0; --i) {
        WorkerThread* wt = &g_wt[i];
        LONG v = wt->bits;
        while (v & 1) {
            LONG o = InterlockedCompareExchange(&wt->bits, v & ~1, v);
            if (o == v) { WgWake(wt->ev, Slot(i)); return true; }
            v = o;
        }
    }
    return false;
}
void Push(int q, int job) {
    { std::lock_guard<std::mutex> g(g_q[q].m); g_q[q].d.push_back(job); }
    g_pushed++;
    WakeOne();
}
void Work(int self, int job) {
    volatile int x = 0;
    for (int k = 0; k < 200 + (job & 255) * 4; ++k) x = x + k;
    g_done++;
    if ((job % 7) == 0 && !g_stop) Push(self, job + 1);  // nested submission from a worker (like a pop-chain fan-out)
}
void WorkerMain(int self) {
    WorkerThread* wt = &g_wt[self];
    for (;;) {
        int job;
        while (Pop(self, job)) Work(self, job);
        if (g_stop && !AnyWork()) return;
        LONG v = wt->bits;
        for (;;) { LONG o = InterlockedCompareExchange(&wt->bits, v | 1, v); if (o == v) break; v = o; }
        if (AnyWork()) {  // re-check after announcing (keeps the stock model free of lost wake-ups)
            v = wt->bits;
            while (v & 1) { LONG o = InterlockedCompareExchange(&wt->bits, v & ~1, v); if (o == v) break; v = o; }
            continue;
        }
        WgSleep(wt->ev, INFINITE, g_poolObj, Slot(self));
    }
}

int Run(uint32_t enable, LONG maxPending, int seconds) {
    g_wgEnable = enable;
    g_wgMax = maxPending;
    g_wgWindowTsc = ~0ull >> 1;  // effectively infinite: correctness must not depend on the window
    g_stop = false;
    g_pushed = 0; g_done = 0; g_wgSkipped = 0; g_wgCounted = 0; g_wgStale = 0;
    std::vector<std::thread> ws;
    for (int i = 0; i < N; ++i) ws.emplace_back(WorkerMain, i);
    std::vector<std::thread> subs;
    for (int s = 0; s < 3; ++s)
        subs.emplace_back([s, seconds] {
            std::mt19937 r(1234 + s);
            const ULONGLONG end = GetTickCount64() + seconds * 1000ull;
            int job = 1 + s;
            while (GetTickCount64() < end) {
                const int burst = 1 + (int)(r() % 64);
                for (int b = 0; b < burst; ++b) { job += 3; Push((int)(r() % N), job); }
                if (r() % 4 == 0) Sleep(0);
                if (r() % 64 == 0) Sleep(1);
                // wait for this submitter's outstanding work to drain sometimes (exercises idle -> storm transitions)
                if (r() % 16 == 0) { const ULONGLONG t0 = GetTickCount64(); while (g_done < g_pushed && GetTickCount64() - t0 < 5000) Sleep(0); }
            }
        });
    // stall monitor
    int rc = 0;
    long long lastDone = -1;
    ULONGLONG lastProg = GetTickCount64();
    bool subsDone = false;
    for (;;) {
        Sleep(50);
        const long long d = g_done, p = g_pushed;
        if (d != lastDone) { lastDone = d; lastProg = GetTickCount64(); }
        if (d < p && GetTickCount64() - lastProg > 3000) {
            printf("  STALL: pushed %lld done %lld\n", p, d);
            rc = 1;
            break;
        }
        if (!subsDone) {
            bool alive = false;
            for (auto& t : subs) alive |= t.joinable() && WaitForSingleObject(t.native_handle(), 0) == WAIT_TIMEOUT;
            if (!alive) subsDone = true;
        }
        if (subsDone && d == p) break;
    }
    for (auto& t : subs) if (t.joinable()) t.join();
    g_stop = true;
    for (int i = 0; i < N; ++i) SetEvent(g_wt[i].ev);
    for (int k = 0; k < 200; ++k) { for (int i = 0; i < N; ++i) SetEvent(g_wt[i].ev); Sleep(1); bool any = false; for (auto& t : ws) any |= WaitForSingleObject(t.native_handle(), 0) == WAIT_TIMEOUT; if (!any) break; }
    for (auto& t : ws) if (WaitForSingleObject(t.native_handle(), 2000) == WAIT_OBJECT_0) t.join(); else t.detach();
    long pend = 0;
    for (auto& ps : g_wgPool) pend += ps.pending;
    printf("  WG %s max %ld: jobs %lld done %lld | skipped %lld, counted %lld, stale %lld, pending at end %ld -> %s\n",
           enable ? "ON " : "off", maxPending, (long long)g_pushed, (long long)g_done, (long long)g_wgSkipped,
           (long long)g_wgCounted, (long long)g_wgStale, pend, rc ? "FAIL" : (pend == 0 ? "OK" : "FAIL (pending drift)"));
    return rc || pend != 0;
}

int main() {
    ws_iatSetEvent = (uint64_t)(uintptr_t)&g_setEv;
    ws_iatWait = (uint64_t)(uintptr_t)&g_wait;
    *reinterpret_cast<uint8_t**>(g_poolObj + 0x10) = g_slots;
    *reinterpret_cast<int*>(g_poolObj + 0x34) = 1;
    for (int i = 0; i < N; ++i) {
        g_wt[i].ev = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        *reinterpret_cast<WorkerThread**>(Slot(i)) = &g_wt[i];
        *reinterpret_cast<uint8_t**>(Slot(i) + 0x440) = g_poolObj;
    }
    int fails = 0;
    fails += Run(0, 1, 4);
    fails += Run(1, 1, 8);
    fails += Run(1, 2, 6);
    fails += Run(1, 1, 8);
    printf(fails ? "FAILED\n" : "ALL PASSED\n");
    return fails;
}
