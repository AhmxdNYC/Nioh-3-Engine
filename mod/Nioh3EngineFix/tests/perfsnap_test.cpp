#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
// Offline test of the report / crash performance snapshot (perfsnap.cpp, TASK #97638). No game.
// 1. Summarize: avg fps, 1 % low, p99, the 10 s window. 2. Classify: gpu / cpu / mixed / capped / unknown.
// 3. End to end: synthetic presents through OnGamePresent with fake GF / W6 counters -> Json (crash path) is GPU-bound at ~60 fps;
//    the JSON is written to argv[1] (the runner checks it with Python's json module) and the text lines to argv[2].
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>
#include <atomic>
#include <algorithm>
#include <string>
#include "../src/perfsnap.h"
#include "../src/perfsnap_core.h"

static int fails = 0;
#define CHECK(x) do { if (!(x)) { ++fails; fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); } } while (0)
static bool Near(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

static volatile long long g_w6 = 0, g_gf = 0, g_ds = 0;
static volatile long g_l1 = 0;
static bool FakeBk(double out[4]) {
    static double t = 0;
    t += 0.5;  // 0.5 ms of ECL stall per sample
    out[0] = t, out[1] = 0, out[2] = 0, out[3] = 0;
    return true;
}

int main(int argc, char** argv) {
    using namespace perfsnap;
    static float ms[4096], scratch[4096];
    // ---- 1. Summarize ----
    for (int i = 0; i < 594; ++i) ms[i] = 16.0f;
    for (int i = 594; i < 600; ++i) ms[i] = 40.0f;
    FrameStats f = Summarize(ms, 600, 10000, scratch);
    CHECK(f.frames == 600);
    CHECK(Near(f.avgMs, 9744.0 / 600, 1e-6));
    CHECK(Near(f.avgFps, 600000.0 / 9744, 1e-6));
    CHECK(Near(f.low1Fps, 25.0, 1e-6));  // the 6 slowest of 600 = the six 40 ms frames
    CHECK(Near(f.maxMs, 40.0, 1e-6));
    CHECK(Near(f.p99Ms, 40.0, 1e-6));
    for (int i = 0; i < 3000; ++i) ms[i] = 10.0f;  // 30 s of frames: only the newest 10 s count
    f = Summarize(ms, 3000, 10000, scratch);
    CHECK(f.frames == 1000);
    CHECK(Near(f.avgFps, 100.0, 1e-6));
    ms[2999] = 0.f;  // a hole is skipped
    f = Summarize(ms, 3000, 10000, scratch);
    CHECK(f.frames == 1000 && Near(f.avgFps, 100.0, 1e-6));
    CHECK(Summarize(ms, 0, 10000, scratch).frames == 0);

    // ---- 2. Classify ----
    FrameStats f60;
    f60.frames = 600, f60.spanMs = 10000, f60.avgMs = 16.67, f60.avgFps = 60.0;
    FrameStats f45 = f60;
    f45.avgMs = 22.2, f45.avgFps = 45.0;
    WindowCost w;
    w.spanS = 10, w.frames = 600, w.gameThreadMs = 12;
    char why[160];
    w.gpuFenceMs = 7;
    CHECK(Classify(f45, w, 60, why, sizeof why) == kGpu);
    CHECK(strstr(why, "GPU") != nullptr);
    w.gpuFenceMs = 0.5, w.presentWaitMs = 0.5;
    CHECK(Classify(f45, w, 60, why, sizeof why) == kCpu);
    CHECK(strstr(why, "present thread CPU 54% of a frame") != nullptr && !strstr(why, "busiest"));  // 12 ms / 22.2 ms
    CHECK(Classify(f45, w, 60, why, sizeof why, "[App]Game", 84.5) == kCpu);  // #97775: both measures named, never one "game thread busy"
    CHECK(strstr(why, "present thread CPU 54% of a frame; busiest thread [App]Game 84% of a core") != nullptr || strstr(why, "[App]Game 85%") != nullptr);
    CHECK(!strstr(why, "game thread busy"));
    w.gpuFenceMs = 2.5, w.presentWaitMs = 1.0;
    CHECK(Classify(f45, w, 60, why, sizeof why) == kMixed);
    CHECK(Classify(f60, w, 60, why, sizeof why) == kCapped);
    CHECK(Classify(f60, w, 0, why, sizeof why) == kMixed);  // no known cap: no "capped"
    FrameStats few = f60;
    few.frames = 10, few.spanMs = 160;
    CHECK(Classify(few, w, 60, why, sizeof why) == kUnknown);

    // ---- 2b. RankThreads / CorePct (#97673) ----
    CHECK(Near(CorePct(15000000ull, 30.0), 5.0, 1e-9));   // 1.5 s CPU in 30 s = 5 % of one core
    CHECK(Near(CorePct(300000000ull, 30.0), 100.0, 1e-9));
    CHECK(CorePct(5, 0) == 0);
    {
        ThreadUse in[8];
        const double pcts[8] = {3, 80, 12, 80, 0.5, 45, 7, 1};
        for (int i = 0; i < 8; ++i) { in[i].tid = (uint32_t)(100 + i); in[i].pct = pcts[i]; snprintf(in[i].name, sizeof in[i].name, "t%d", i); }
        ThreadUse top[5];
        const int tn = RankThreads(in, 8, top, 5);
        CHECK(tn == 5);
        CHECK(top[0].tid == 101 && top[1].tid == 103);  // tie: lower tid first
        CHECK(top[2].tid == 105 && top[3].tid == 102 && top[4].tid == 106);
        CHECK(RankThreads(in, 2, top, 5) == 2 && top[0].tid == 101 && top[1].tid == 100);
        CHECK(RankThreads(in, 0, top, 5) == 0);
    }

    // ---- 2c. WindowDelta: a failed GetThreadTimes tick is a hole, never a stale value (#97734) ----
    {
        constexpr int H = 32;
        uint64_t t[H] = {};
        bool ok[H] = {};
        for (int i = 0; i <= 40; ++i) { t[i % H] = (uint64_t)i * 10000000ull; ok[i % H] = true; }  // 1 s of CPU per tick
        int old = -1;
        uint64_t d = 0;
        CHECK(WindowDelta(t, ok, H, 40, 0, 30, old, d) && old == 10 && d == 300000000ull);
        CHECK(Near(CorePct(d, 30.0), 100.0, 1e-9));
        ok[40 % H] = false;                                   // newest sample failed
        CHECK(!WindowDelta(t, ok, H, 40, 0, 30, old, d));
        ok[40 % H] = true;
        ok[10 % H] = false;                                   // the window's old end failed
        CHECK(!WindowDelta(t, ok, H, 40, 0, 30, old, d));
        // the ring entry 31 ticks back is stale after a failure: tick 41 fails, 42 must not pair with entry 41 % 32 (= tick 9's)
        ok[10 % H] = true;
        ok[41 % H] = false;
        t[41 % H] = 9ull * 10000000ull;                       // stale value from tick 9
        CHECK(!WindowDelta(t, ok, H, 41, 0, 30, old, d));
        t[42 % H] = 42ull * 10000000ull; ok[42 % H] = true;
        CHECK(WindowDelta(t, ok, H, 42, 0, 30, old, d) && old == 12 && d == 300000000ull);  // never 33 s of CPU
        CHECK(!WindowDelta(t, ok, H, 5, 5, 30, old, d));      // no history yet (old >= tick)
        t[3] = 5; ok[3] = true; t[2] = 9; ok[2] = true;
        CHECK(!WindowDelta(t, ok, H, 3, 2, 30, old, d));      // counter went backwards: invalid, not a huge unsigned value
    }

    // ---- 3. End to end ----
    StartInfo si;
    si.w6Ticks = &g_w6, si.gfTicks = &g_gf, si.dsTicks = &g_ds, si.l1 = &g_l1, si.backendWaitsMs = &FakeBk;
    ResetForTest(si);
    SetHardwareForTest("AMD Ryzen 7 7800X3D 8-Core Processor", 16, "AMD Radeon RX 9070 \"test\"", "1002:7550", "32.0.21025.1024",
                       16304, 9120, 15480, 32448, 14210);
    SetSettingsForTest("{\"SCR_REZ\":\"2560x1440\",\"REN_RAT\":\"75\",\"REN_FPS\":\"120\",\"RESOLUTION_SCALING\":\"FSR\",\"FRAME_GENERATION\":\"OFF\"}",
                       "{\"ExtraPerf\":{\"Enabled\":\"1\",\"Preset\":\"0\"},\"VRR\":{\"MaxFps\":\"120\"},\"patches_off\":[\"MW\"]}", 120, "FSR", "OFF");
    NoteBackBuffer(2560, 1440);
    LARGE_INTEGER fq, q;
    QueryPerformanceFrequency(&fq);
    QueryPerformanceCounter(&q);
    const long long tick = fq.QuadPart / 60;  // 60 fps
    long long t = q.QuadPart - 12 * fq.QuadPart;
    for (int i = 0; i < 12 * 60; ++i) {
        g_gf += tick * 40 / 100;  // the game thread waits on the GPU 40 % of every frame
        g_w6 += tick * 2 / 100;
        if (i % 2) ++g_l1;
        OnGamePresent(t);
        t += tick;
    }
    // per-thread sampler on this real process: one named thread spins ~1.5 s, the rest sleep
    std::atomic<bool> stop{false};
    std::thread busy([&] {
        using SetDescFn = HRESULT(WINAPI*)(HANDLE, PCWSTR);
        if (auto sd = reinterpret_cast<SetDescFn>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "SetThreadDescription")))
            sd(GetCurrentThread(), L"[App]BusyTest");
        volatile unsigned x = 0;
        while (!stop) ++x;
    });
    std::thread idle([&] { while (!stop) Sleep(50); });
    // #97775: five more busy threads push the presenting (main) thread, which only sleeps here, out of the top 5
    std::thread spin[5];
    for (auto& th : spin) th = std::thread([&] { volatile unsigned x = 0; while (!stop) ++x; });
    Sleep(100);
    ThreadTickForTest();  // tick 0: list + first times
    Sleep(1500);
    ThreadTickForTest();  // tick 1: the 1.5 s window
    stop = true;
    busy.join();
    idle.join();
    for (auto& th : spin) th.join();

    static char js[16384], tx[8192];
    const size_t jn = Json(js, sizeof js, true);
    const size_t tn = Text(tx, sizeof tx, true);
    printf("%s\n", js);
    printf("%s", tx);
    CHECK(jn > 200 && jn < sizeof js - 1);
    CHECK(tn > 100);
    CHECK(strstr(js, "\"bound\":\"gpu\"") != nullptr);
    CHECK(strstr(js, "\"avg_fps\":60.0") != nullptr);
    CHECK(strstr(js, "\"low1_fps\":60.0") != nullptr);
    CHECK(strstr(js, "\"gpu\":\"AMD Radeon RX 9070 \\\"test\\\"\"") != nullptr);  // escaped
    CHECK(strstr(js, "\"fps_cap\":120,\"upscaler\":\"FSR\",\"frame_gen\":\"OFF\"") != nullptr);
    CHECK(strstr(js, "\"backbuffer\":\"2560x1440\"") != nullptr);
    CHECK(strstr(js, "\"REN_RAT\":\"75\"") != nullptr);
    CHECK(strstr(js, "\"patches_off\":[\"MW\"]") != nullptr);
    CHECK(strstr(js, "\"name\":\"GPU fence wait (GF)\",\"ms\":6.6") != nullptr);  // 40 % of 16.67 ms, the top part
    CHECK(strstr(js, "\"limiter_per_frame\":0.50") != nullptr);
    CHECK(strstr(js, "engine ECL ring full") != nullptr);
    CHECK(strstr(tx, "bound: gpu") != nullptr);
    CHECK(strstr(js, "\"busiest_thread\":{\"name\":\"") != nullptr);  // six spinners tie for busiest
    {
        const char* bp = strstr(js, "\"busiest_thread\"");
        const char* cp = bp ? strstr(bp, "\"core_pct\":") : nullptr;
        const double pct = cp ? atof(cp + 11) : -1;
        printf("busy thread: %.1f %% of one core\n", pct);
        CHECK(pct > 60 && pct < 105);  // one spinning thread = about one core
    }
    CHECK(strstr(js, "\"threads\":[{") != nullptr);
    CHECK(strstr(js, "\"thread_count\":") != nullptr);
    CHECK(strstr(js, "present\",\"tid\":") != nullptr);  // #97775: the presenting thread is listed (6th entry) with the tag
    {
        int cnt = 0;
        for (const char* q2 = js; (q2 = strstr(q2, "\"core_pct\"")) != nullptr; ++q2) ++cnt;
        printf("thread entries incl. busiest_thread: %d\n", cnt);
        CHECK(cnt == 7);  // busiest_thread + 6 listed
    }
    CHECK(strstr(js, "\"mod_settings\":") != nullptr);
    CHECK(strstr(tx, "busiest ") != nullptr && strstr(tx, " / present ") != nullptr);
    // a tiny buffer truncates but stays terminated
    char small[64];
    const size_t sn = Json(small, sizeof small, true);
    CHECK(sn == 0 && small[0] == 0);  // too small: nothing, never a cut JSON
    char tsmall[64];
    const size_t tsn = Text(tsmall, sizeof tsmall, true);
    CHECK(tsn < sizeof tsmall && tsmall[tsn] == 0);  // the text form may be cut

    // ---- 4. #97850: the ini path outlives the caller's string (dllmain passes a block-local std::wstring) ----
    {
        wchar_t tmp[MAX_PATH], ini[MAX_PATH];
        GetTempPathW(MAX_PATH, tmp);
        swprintf(ini, MAX_PATH, L"%sperfsnap_test_%lu.ini", tmp, GetCurrentProcessId());
        WritePrivateProfileStringW(L"Latency", L"VrrPresent", L"1", ini);
        WritePrivateProfileStringW(L"Patches", L"MW", L"0", ini);
        auto* owner = new std::wstring(ini);
        StartInfo s4 = si;
        s4.iniPath = owner->c_str();
        ResetForTest(s4);
        std::fill(owner->begin(), owner->end(), L'X');  // scribble, then free: a kept pointer now reads garbage
        delete owner;
        auto* junk = new std::wstring(400, L'Z');       // likely reuses the freed block
        SetSettingsForTest("", "", 0, "", "");           // clear what section 3 set
        RefreshModForTest();
        static char j4[16384];
        Json(j4, sizeof j4, true);
        const char* modSec = strstr(j4, "\"mod_settings\":");
        printf("mod_settings after the caller's string was freed: %.80s\n", modSec ? modSec : "(missing)");
        CHECK(modSec && strstr(modSec, "\"Latency\":{\"VrrPresent\":\"1\"}") != nullptr);
        CHECK(strstr(j4, "\"patches_off\":[\"MW\"]") != nullptr);
        delete junk;
        DeleteFileW(ini);
    }

    // ---- 5. #97817: a renamed thread is renamed in the cache; "present" follows the presenting TID ----
    {
        using SetDescFn = HRESULT(WINAPI*)(HANDLE, PCWSTR);
        auto sd = reinterpret_cast<SetDescFn>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "SetThreadDescription"));
        HANDLE go = CreateEventW(nullptr, TRUE, FALSE, nullptr), step = CreateEventW(nullptr, FALSE, FALSE, nullptr),
               done = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        std::atomic<int> cmd{0};
        DWORD wtid = 0;
        std::thread wk([&] {
            wtid = GetCurrentThreadId();
            SetEvent(done);
            for (;;) {
                WaitForSingleObject(step, INFINITE);
                const int c = cmd;
                if (c == 1 && sd) sd(GetCurrentThread(), L"[App]Renamed");
                if (c == 2) { LARGE_INTEGER q2; QueryPerformanceCounter(&q2); OnGamePresent(q2.QuadPart); }
                SetEvent(done);
                if (c == 3) return;
            }
        });
        WaitForSingleObject(done, INFINITE);
        auto run = [&](int c) { cmd = c; SetEvent(step); WaitForSingleObject(done, INFINITE); };
        auto enumerate = [&] { for (int i = 0; i < 5; ++i) ThreadTickForTest(); };  // a list refresh runs every 5th tick
        const DWORD me = GetCurrentThreadId();
        { LARGE_INTEGER q2; QueryPerformanceCounter(&q2); OnGamePresent(q2.QuadPart); }  // main presents
        enumerate();
        const std::string n0 = ThreadNameForTest(wtid), m0 = ThreadNameForTest(me);
        run(1);  // the worker names itself after the sampler first saw it
        enumerate();
        const std::string n1 = ThreadNameForTest(wtid);
        run(2);  // the worker becomes the presenting thread
        enumerate();
        const std::string n2 = ThreadNameForTest(wtid), m2 = ThreadNameForTest(me);
        printf("worker: '%s' -> '%s' -> '%s'; main: '%s' -> '%s'\n", n0.c_str(), n1.c_str(), n2.c_str(), m0.c_str(), m2.c_str());
        CHECK(!n0.empty() && n0.find("Renamed") == std::string::npos);
        CHECK(!sd || n1 == "[App]Renamed");
        CHECK(!sd || n2 == "[App]Renamed / present");
        CHECK(m0.find("present") != std::string::npos);
        CHECK(!m2.empty() && m2.find("present") == std::string::npos);  // tag cleared when the presenting TID moved
        run(3);
        wk.join();
        CloseHandle(go), CloseHandle(step), CloseHandle(done);
    }

    if (argc > 1) { if (FILE* o = fopen(argv[1], "wb")) { fwrite(js, 1, jn, o); fclose(o); } }
    if (argc > 2) { if (FILE* o = fopen(argv[2], "wb")) { fwrite(tx, 1, tn, o); fclose(o); } }
    if (fails) { printf("%d FAILED\n", fails); return 1; }
    printf("ALL PASSED\n");
    return 0;
}
