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
    w.gpuFenceMs = 2.5, w.presentWaitMs = 1.0;
    CHECK(Classify(f45, w, 60, why, sizeof why) == kMixed);
    CHECK(Classify(f60, w, 60, why, sizeof why) == kCapped);
    CHECK(Classify(f60, w, 0, why, sizeof why) == kMixed);  // no known cap: no "capped"
    FrameStats few = f60;
    few.frames = 10, few.spanMs = 160;
    CHECK(Classify(few, w, 60, why, sizeof why) == kUnknown);

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
    // a tiny buffer truncates but stays terminated
    char small[64];
    const size_t sn = Json(small, sizeof small, true);
    CHECK(sn == 0 && small[0] == 0);  // too small: nothing, never a cut JSON
    char tsmall[64];
    const size_t tsn = Text(tsmall, sizeof tsmall, true);
    CHECK(tsn < sizeof tsmall && tsmall[tsn] == 0);  // the text form may be cut

    if (argc > 1) { if (FILE* o = fopen(argv[1], "wb")) { fwrite(js, 1, jn, o); fclose(o); } }
    if (argc > 2) { if (FILE* o = fopen(argv[2], "wb")) { fwrite(tx, 1, tn, o); fclose(o); } }
    if (fails) { printf("%d FAILED\n", fails); return 1; }
    printf("ALL PASSED\n");
    return 0;
}
