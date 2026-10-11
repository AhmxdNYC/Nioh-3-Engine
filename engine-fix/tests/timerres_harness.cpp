// timerres_harness.cpp - offline test of src/timerres.cpp (TR). Not a game test.
// This exe imports WINMM timeBeginPeriod / timeEndPeriod itself, so timerres::Start(base = this exe) hooks OUR import slots,
// exactly as it hooks Nioh3.exe's. Every call below goes through the IAT (normal import calls).
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <timeapi.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <algorithm>
#include "../src/timerres.h"

static int g_fail = 0;
static void Check(bool c, const char* msg) {
    if (!c) { std::printf("FAIL: %s\n", msg); ++g_fail; }
}
static void Log(const char* fmt, ...) {
    char b[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    std::printf("  log: %s\n", b);
}
using Ntq_t = LONG(NTAPI*)(PULONG, PULONG, PULONG);
static ULONG CurRes() {
    static Ntq_t q = reinterpret_cast<Ntq_t>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryTimerResolution"));
    ULONG a = 0, b = 0, c = 0;
    if (q) q(&a, &b, &c);
    return c;  // 100 ns units
}
// Sleep(1) duration in us, median of n
static double SleepUs(int n) {
    LARGE_INTEGER f, a, b;
    QueryPerformanceFrequency(&f);
    std::vector<double> v;
    for (int i = 0; i < n; ++i) {
        QueryPerformanceCounter(&a);
        Sleep(1);
        QueryPerformanceCounter(&b);
        v.push_back((b.QuadPart - a.QuadPart) * 1e6 / f.QuadPart);
    }
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

int main() {
    wchar_t ini[MAX_PATH];
    GetTempPathW(MAX_PATH, ini);
    wcscat_s(ini, L"timerres_harness.ini");
    WritePrivateProfileStringW(L"Patches", L"TR", L"2", ini);  // installed, switch off
    const double s0 = SleepUs(16);  // fresh process: nothing in it asked for a timer period yet
    std::printf("  Sleep(1) median before anything in this process asks a period: %.0f us\n", s0);

    void* pBegin0 = reinterpret_cast<void*>(&timeBeginPeriod);  // address of the import thunk target as seen now
    (void)pBegin0;
    timerres::StartInfo si;
    si.log = &Log;
    si.iniPath = ini;
    si.base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    timerres::Start(si);
    volatile uint32_t* sw = timerres::Switch();
    Check(sw != nullptr, "TR = 2: hooks installed");
    if (!sw) { std::printf("cannot continue\n"); return 1; }
    Check(*sw == 0, "TR = 2: switch off");

    char line[512];
    // 1. switch off: calls pass through, counted, nothing skipped
    for (int i = 0; i < 100; ++i) { Check(timeBeginPeriod(1) == TIMERR_NOERROR, "off: begin passes"); Check(timeEndPeriod(1) == TIMERR_NOERROR, "off: end passes"); }
    timerres::StatusLine(line, sizeof line, 1);
    std::printf("  %s\n", line);
    Check(strstr(line, "timeBeginPeriod 100/s timeEndPeriod 100/s (period asked 1 ms), skipped 0/0") != nullptr, "off: 100 calls counted, 0 skipped");
    Check(strstr(line, "hold none") != nullptr, "off: no hold");
    Check(strstr(line, "Sleep(1) here: median") != nullptr, "status line has the Sleep(1) probe");

    // 2. switch on: hold 1 ms, game pairs skipped (they return NOERROR), resolution stays fine between pairs
    *sw = 1;
    timerres::Sync();
    const ULONG resOn = CurRes();
    std::printf("  system timer with the hold: %.3f ms\n", resOn / 1e4);
    Check(resOn <= 10000, "on: system timer <= 1 ms while held");
    for (int i = 0; i < 100; ++i) { Check(timeBeginPeriod(1) == TIMERR_NOERROR, "on: begin returns NOERROR"); Check(timeEndPeriod(1) == TIMERR_NOERROR, "on: end returns NOERROR"); }
    timerres::StatusLine(line, sizeof line, 1);
    std::printf("  %s\n", line);
    Check(strstr(line, "skipped 100/100") != nullptr, "on: 100 pairs skipped");
    Check(strstr(line, "hold 1 ms") != nullptr, "on: hold 1 ms");
    const double s1 = SleepUs(40);
    std::printf("  Sleep(1) median with the hold: %.0f us\n", s1);
    Check(s1 < 3000, "on: Sleep(1) < 3 ms (1 ms granularity)");

    // 3. switch flips between a begin and its end, on two threads: every end matches its own begin (no stray end, no leak)
    {
        // begin skipped (on), switch off, end must be skipped too (TLS depth), later pairs pass
        Check(timeBeginPeriod(1) == TIMERR_NOERROR, "flip1: begin");
        *sw = 0;
        Check(timeEndPeriod(1) == TIMERR_NOERROR, "flip1: end of a skipped begin is skipped (NOERROR, not NOCANDO)");
        // begin passed (off), switch on, end must pass (refcount balanced in winmm)
        Check(timeBeginPeriod(1) == TIMERR_NOERROR, "flip2: begin passes");
        *sw = 1;
        Check(timeEndPeriod(1) == TIMERR_NOERROR, "flip2: end passes (balances the real begin)");
    }
    // 4. 4 threads x 20000 pairs while the switch flips 200 times: all calls NOERROR, counts add up
    {
        std::atomic<int> bad{0};
        std::atomic<bool> stop{false};
        std::vector<std::thread> th;
        for (int t = 0; t < 4; ++t)
            th.emplace_back([&] {
                for (int i = 0; i < 20000; ++i) {
                    if (timeBeginPeriod(1) != TIMERR_NOERROR) ++bad;
                    if (timeEndPeriod(1) != TIMERR_NOERROR) ++bad;
                }
            });
        std::thread flipper([&] { for (int i = 0; i < 200 && !stop; ++i) { *sw = (i & 1) ? 0u : 1u; Sleep(0); } });
        for (auto& x : th) x.join();
        stop = true;
        flipper.join();
        Check(bad == 0, "stress: every begin/end returned NOERROR (no unbalanced end)");
        timerres::StatusLine(line, sizeof line, 1);
        std::printf("  stress: %s\n", line);
    }
    // 5. switch off at the end: hold released; the process asks nothing any more
    *sw = 0;
    timerres::Sync();
    timerres::StatusLine(line, sizeof line, 1);
    Check(strstr(line, "hold none") != nullptr, "off again: hold released");
    // the real winmm refcount for period 1 must be 0 now: one more end must fail with NOCANDO (went through: switch off)
    Check(timeEndPeriod(1) == TIMERR_NOCANDO, "winmm refcount balanced: an extra end is refused");

    // 6. cost per hooked call (switch on, skipped path)
    *sw = 1;
    timerres::Sync();
    LARGE_INTEGER f, a, b;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&a);
    for (int i = 0; i < 100000; ++i) { timeBeginPeriod(1); timeEndPeriod(1); }
    QueryPerformanceCounter(&b);
    const double nsPair = (b.QuadPart - a.QuadPart) * 1e9 / f.QuadPart / 100000;
    *sw = 0;
    timerres::Sync();
    QueryPerformanceCounter(&a);
    for (int i = 0; i < 2000; ++i) { timeBeginPeriod(1); timeEndPeriod(1); }
    QueryPerformanceCounter(&b);
    const double nsPairStock = (b.QuadPart - a.QuadPart) * 1e9 / f.QuadPart / 2000;
    std::printf("  cost per begin+end pair: skipped %.0f ns, stock (switch off, real winmm) %.0f ns\n", nsPair, nsPairStock);
    Check(nsPair < 2000, "skipped pair < 2 us");

    DeleteFileW(ini);
    std::printf(g_fail ? "FAILED (%d)\n" : "ALL PASSED\n", g_fail);
    return g_fail ? 1 : 0;
}
