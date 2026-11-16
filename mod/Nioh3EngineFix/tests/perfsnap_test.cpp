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
// CPU-share lower bounds depend on machine load (a busy PC gives a spinner less than a core): warn, do not fail,
// unless PERFSNAP_STRICT_TIMING=1. Upper bounds stay hard checks (#108540).
static int timingWarns = 0;
#define TIMING_CHECK(x) do { if (!(x)) { ++timingWarns; fprintf(stderr, "WARN timing line %d: %s\n", __LINE__, #x); } } while (0)
static bool Near(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

static volatile long long g_w6 = 0, g_gf = 0, g_ds = 0;
static bool g_fakeMenu = false;
static bool FakeMenu() { return g_fakeMenu; }
static volatile long g_l1 = 0;
static bool FakeBk(double out[4]) {
    static double t = 0;
    t += 0.5;  // 0.5 ms of ECL stall per sample
    out[0] = t, out[1] = 0, out[2] = 0, out[3] = 0;
    return true;
}


// #98375: the settings table gate on an exe that fails the build-identity check (same table at the v2.02 RVAs).
static void TableGateTest() {
    const size_t kDesc = 0x45C8570, kLive = 0x45BAF30, kSize = 0x4600000;
    uint8_t* img = (uint8_t*)VirtualAlloc(nullptr, kSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    CHECK(img != nullptr);
    if (!img) return;
    const uintptr_t b = (uintptr_t)img, e = b + kSize;
    const char* names[] = {"DISP_MODE", "VSYNC", "SCR_REZ", "REN_RAT", "REN_FPS", "DYNAMIC_FPS", "PRESET", "SHADOW_QUAL",
                           "AMBIENT_OCCLUSION", "GI", "SSR", "MODEL_QUAL", "MODEL_DRAW", "VOLUMETRIC", "VISUAL_EFFECT",
                           "MOTION_QUALITY", "LANDSCAPE_QUALITY", "MESHLET_QUALITY", "GRASS_DENSITY", "POSTEFFECT_RESOLUTION",
                           "DYNAMIC_RESOLUTION", "RESOLUTION_SCALING", "FRAME_GENERATION", "DLSS_FRAME_GEN_MULI", "CPU_SPEC_CLASS"};
    char* str = (char*)img + 0x1000;
    const char* vnames[4] = {"OFF", "LOW", "MID", "HIGH"};
    uintptr_t* vlist = (uintptr_t*)(img + 0x8000);  // one shared 4-entry value list
    for (int v = 0; v < 4; ++v) {
        strcpy(str, vnames[v]);
        vlist[v] = (uintptr_t)str;
        str += 48;
    }
    for (int i = 0; i < 25; ++i) {
        strcpy(str, names[i]);
        *(uintptr_t*)(b + kDesc + i * 0x30) = (uintptr_t)str;
        *(uintptr_t*)(b + kDesc + i * 0x30 + 8) = (uintptr_t)vlist;
        *(int*)(b + kDesc + i * 0x30 + 0x20) = 1;  // table default
        *(int*)(b + kDesc + i * 0x30 + 0x24) = 4;
        *(int*)(b + kLive + 4 * i) = i % 4;
        str += 48;
    }
    CHECK(perfsnap::SettingsTableFoundForTest(b, e));            // stripped-DRM copy: table found
    CHECK(perfsnap::SettingsValuesVerifiedForTest(b, e));        // and its live indexes are trusted
    CHECK(!perfsnap::SettingsTableFoundForTest(b, b + kDesc));  // image too short for the table: never read past it
    // #98560: index array zeroed (moved away): names still found, values reported as unverified, not as "every key = OFF"
    for (int i = 0; i < 25; ++i) *(int*)(b + kLive + 4 * i) = 0;
    CHECK(perfsnap::SettingsTableFoundForTest(b, e));
    CHECK(!perfsnap::SettingsValuesVerifiedForTest(b, e));
    // ...but a game whose own defaults are all index 0 really can be all zero: verified (idx 0 is a valid setting)
    for (int i = 0; i < 25; ++i) *(int*)(b + kDesc + i * 0x30 + 0x20) = 0;
    CHECK(perfsnap::SettingsValuesVerifiedForTest(b, e));
    for (int i = 0; i < 25; ++i) *(int*)(b + kDesc + i * 0x30 + 0x20) = 1;
    // index array moved onto other data: one out-of-range index -> unverified
    for (int i = 0; i < 25; ++i) *(int*)(b + kLive + 4 * i) = i % 4;
    *(int*)(b + kLive + 4 * 7) = 1234;
    CHECK(perfsnap::SettingsTableFoundForTest(b, e));    // 24 of 25 still pass the name gate
    CHECK(!perfsnap::SettingsValuesVerifiedForTest(b, e));
    *(int*)(b + kLive + 4 * 7) = 3;
    // value list pointer outside the image, or its entry not a name: that key is not a hit
    uint8_t* outside = (uint8_t*)VirtualAlloc(nullptr, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    for (int i = 0; i < 6; ++i) *(uintptr_t*)(b + kDesc + i * 0x30 + 8) = (uintptr_t)outside;
    CHECK(!perfsnap::SettingsTableFoundForTest(b, e));  // 19 hits left
    for (int i = 0; i < 6; ++i) *(uintptr_t*)(b + kDesc + i * 0x30 + 8) = (uintptr_t)vlist;
    CHECK(perfsnap::SettingsTableFoundForTest(b, e));
    const uintptr_t keep = vlist[2];
    vlist[2] = (uintptr_t)(img + 0x9000);  // points at zero bytes: empty, not a name
    CHECK(!perfsnap::SettingsTableFoundForTest(b, e));  // keys at index 2 drop out (6 of 25 -> 19 left)
    vlist[2] = keep;
    VirtualFree(outside, 0, MEM_RELEASE);
    for (int i = 0; i < 6; ++i) *(int*)(b + kLive + 4 * i) = 99;   // 19 valid keys left: below the bar
    CHECK(!perfsnap::SettingsTableFoundForTest(b, e));
    memset(img + kDesc, 0xCC, 60 * 0x30);                          // another build: garbage at the place
    CHECK(!perfsnap::SettingsTableFoundForTest(b, e));
    VirtualFree(img, 0, MEM_RELEASE);
}

int main(int argc, char** argv) {
    TableGateTest();
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
    {  // #98471 freeze causes: the player-report #70 freeze (100 ms, game thread idle, 12 MB read on 8 busy threads) = loading
        HitchIn h;
        h.gapMs = 100, h.gtCpuMs = 6, h.procCores = 7.5, h.rdMb = 12.0, h.gfMs = 0, h.w6Ms = 0, h.pso = 0;
        CHECK(ClassifyHitch(h, 16) == kHLoading);
        CHECK(!strcmp(HitchCauseName(ClassifyHitch(h, 16)), "loading"));
        HitchIn m = h; m.menu = true; CHECK(ClassifyHitch(m, 16) == kHMenu);
        m = h; m.pause = 1; CHECK(ClassifyHitch(m, 16) == kHMenu);
        m = h; m.rdMb = 0; m.pause = 3; CHECK(ClassifyHitch(m, 16) == kHLoading);  // engine loading flag
        m = h; m.gfMs = 60; CHECK(ClassifyHitch(m, 16) == kHGpu);
        m = h; m.w6Ms = 55; CHECK(ClassifyHitch(m, 16) == kHPresent);
        m = h; m.pso = 2; CHECK(ClassifyHitch(m, 16) == kHShader);
        m = h; m.gtCpuMs = 80; CHECK(ClassifyHitch(m, 16) == kHGameCpu);
        m = h; m.rdMb = 0; m.ldPend = 4; CHECK(ClassifyHitch(m, 16) == kHLoading);
        m = h; m.rdMb = 0; m.pf = 30000; CHECK(ClassifyHitch(m, 16) == kHMemory);
        m = h; m.rdMb = 0; CHECK(ClassifyHitch(m, 16) == kHStall);     // 7.5 cores of 16: under half
        m = h; m.rdMb = 0; m.procCores = 9; CHECK(ClassifyHitch(m, 16) == kHOtherCpu);
        m = h; m.rdMb = 0; m.procCores = 0.4; CHECK(ClassifyHitch(m, 16) == kHStall);
        CHECK(!strcmp(HitchCauseName(99), "unexplained_wait"));
        CHECK(DumpsSize("{\"a\":1,\"b\":2}", 13) == 16);
    }
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
        TIMING_CHECK(pct > 60);  // one spinning thread = about one core (less when the PC is busy)
        CHECK(pct < 105);
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
    // ---- #98471: every freeze. Before any NoteHitch / threshold: the block says HitchLog is off ----
    CHECK(strstr(js, "\"hitch_log\":\"off\"") != nullptr);
    SetHitchThresholdMs(100);
    {
        // one freeze over a window where six threads spin: the sampler names them on its next tick
        std::atomic<bool> stop2{false};
        std::thread sp2[3];
        for (auto& th : sp2) th = std::thread([&] { volatile unsigned x = 0; while (!stop2) ++x; });
        Sleep(50);
        for (int i = 0; i < 5; ++i) ThreadTickForTest();  // one of these re-lists the threads (every 5th tick)
        LARGE_INTEGER a0, a1;
        QueryPerformanceCounter(&a0);
        Sleep(1200);
        QueryPerformanceCounter(&a1);
        HitchIn h;
        h.qpcStart = a0.QuadPart, h.qpcEnd = a1.QuadPart, h.tS = 412.3, h.gapMs = 1200, h.gtCpuMs = 5, h.procCores = 3.1;
        h.rdMb = 12.3, h.rdOps = 45, h.pf = 1234, h.gfMs = 0, h.w6Ms = 2, h.pso = 0, h.ldAct = 3, h.ldPend = 10, h.infl = 1;
        h.ramLoad = 89;
        NoteHitch(h);
        CHECK(!strcmp(HitchThreadsForTest(0), ""));  // not filled before the sampler's next tick
        ThreadTickForTest();
        stop2 = true;
        for (auto& th : sp2) th.join();
        printf("freeze threads: %s\n", HitchThreadsForTest(0));
        CHECK(strchr(HitchThreadsForTest(0), '%') != nullptr);
        {   // three spinners: the busiest named thread is about one core
            const char* pc = strchr(HitchThreadsForTest(0), '%');
            const char* sp = pc;
            while (sp > HitchThreadsForTest(0) && sp[-1] != ' ') --sp;
            const double v = atof(sp);
            TIMING_CHECK(v > 50);
            CHECK(v < 110);
        }
        // 2000 more freezes of every kind: the totals keep all of them, only 64 keep details
        for (int i = 0; i < 2000; ++i) {
            HitchIn k;
            k.qpcStart = a1.QuadPart, k.qpcEnd = a1.QuadPart + 1, k.tS = 413 + i, k.gapMs = 100 + (i % 9) * 100;
            k.gtCpuMs = (i % 9 == 4) ? k.gapMs : 1;
            k.gfMs = (i % 9 == 1) ? k.gapMs : 0;
            k.w6Ms = (i % 9 == 2) ? k.gapMs : 0;
            k.pso = (i % 9 == 3) ? 1 : 0;
            k.rdMb = (i % 9 == 5) ? 8 : 0;
            k.pf = (i % 9 == 6) ? 40000 : 0;
            k.procCores = (i % 9 == 7) ? 12 : 0.2;
            k.menu = (i % 9 == 0);
            NoteHitch(k);
        }
        CHECK(HitchKeptForTest() == 64);
        static char jh[16384], th2[16384];
        const size_t jhn = Json(jh, sizeof jh, false);
        const size_t thn = Text(th2, sizeof th2, false);
        CHECK(thn > 0 && thn < sizeof th2);
        printf("snapshot text with 64 freeze lines: %zu bytes\n", thn);
        printf("perf JSON with 2001 freezes: %zu bytes (json.dumps <= %zu)\n", jhn, DumpsSize(jh, jhn));
        CHECK(jhn > 0 && DumpsSize(jh, jhn) <= 7900);
        CHECK(strstr(jh, "\"hitch_summary\":{\"threshold_ms\":100,\"count\":2001,") != nullptr);
        CHECK(strstr(jh, "\"over_1s\":") != nullptr && strstr(jh, "\"kept\":64}") != nullptr);
        for (int c = 0; c < kHCauses; ++c) {
            char key[64];
            snprintf(key, sizeof key, "\"%s\":[", HitchCauseName(c));
            CHECK(strstr(jh, key) != nullptr);  // all nine causes counted
        }
        CHECK(strstr(jh, "\"loading\":[223,") != nullptr);  // 222 reads + the first freeze
        CHECK(strstr(jh, "\"hitches\":[{\"n\":2001,") != nullptr);  // newest first
        const char* inr = strstr(jh, "\"hitches_in_report\":");
        const int inN = inr ? atoi(inr + 20) : -1;
        printf("detailed freezes in the report: %d\n", inN);
        CHECK(inN >= 20 && inN <= 64);  // ~1.5 KB of base perf here: most of the budget holds freezes
        CHECK(strstr(th2, "freezes this session (frame gap over 100 ms): 2001,") != nullptr);
        CHECK(strstr(th2, "freeze causes: menu_or_pause ") != nullptr);
        CHECK(strstr(th2, "#2001 t ") != nullptr);
        CHECK(strstr(th2, "newest 64 freezes") != nullptr && strstr(th2, "#1938 t ") != nullptr);  // all kept, in the text
        printf("%.900s\n", strstr(th2, "  freezes this session"));
        if (argc > 3) { if (FILE* o = fopen(argv[3], "wb")) { fwrite(jh, 1, jhn, o); fclose(o); } }
        // the 1200 ms freeze with its threads is in the totals (max) even though its detail left the ring
        CHECK(strstr(jh, "\"max_ms\":1200.0") != nullptr);
    }
    // ---- #98365: a report taken with the menu open describes the 10 s before it opened ----
    {
        StartInfo s5 = si;
        s5.menuOpen = &FakeMenu;
        ResetForTest(s5);
        LARGE_INTEGER q5;
        QueryPerformanceCounter(&q5);
        const long long t60 = fq.QuadPart / 60, t20 = fq.QuadPart / 20;
        long long t5 = q5.QuadPart - 30 * fq.QuadPart;
        for (int i = 0; i < 12 * 60; ++i) { OnGamePresent(t5); t5 += t60; }  // gameplay at 60 fps
        g_fakeMenu = true;
        for (int i = 0; i < 15 * 20; ++i) { OnGamePresent(t5); t5 += t20; }  // 15 s of menu at 20 fps
        static char j5[16384], t5x[8192];
        Json(j5, sizeof j5, false);
        Text(t5x, sizeof t5x, false);
        CHECK(strstr(j5, "\"avg_fps\":60.0") != nullptr);
        CHECK(strstr(j5, "\"window\":\"before_menu_open\"") != nullptr);
        CHECK(strstr(t5x, "before the mod menu opened") != nullptr);
        // #98578: the menu report (frozen=1) must be valid JSON; the runner json.loads it and runs the site check
        if (argc > 4) { if (FILE* o = fopen(argv[4], "wb")) { fwrite(j5, 1, strlen(j5), o); fclose(o); } }
        Json(j5, sizeof j5, true);  // a crash describes the moment it happened
        CHECK(strstr(j5, "\"avg_fps\":20.0") != nullptr && !strstr(j5, "before_menu_open"));
        g_fakeMenu = false;
        for (int i = 0; i < 12 * 60; ++i) { OnGamePresent(t5); t5 += t60; }
        g_fakeMenu = true;  // opens again: a fresh copy
        OnGamePresent(t5);
        Json(j5, sizeof j5, false);
        CHECK(strstr(j5, "\"avg_fps\":60.0") != nullptr && strstr(j5, "before_menu_open") != nullptr);
        g_fakeMenu = false;
        OnGamePresent(t5 + t60);
        Json(j5, sizeof j5, false);
        CHECK(!strstr(j5, "before_menu_open"));
        if (argc > 5) { if (FILE* o = fopen(argv[5], "wb")) { fwrite(j5, 1, strlen(j5), o); fclose(o); } }  // frozen=0 shape
        ResetForTest(si);
    }
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
    if (timingWarns) {
        const char* st = getenv("PERFSNAP_STRICT_TIMING");
        printf("WARN %d timing check(s) below range (CPU busy?)\n", timingWarns);
        if (st && *st == '1') fails += timingWarns;
    }
    if (fails) { printf("%d FAILED\n", fails); return 1; }
    printf("ALL PASSED\n");
    return 0;
}
