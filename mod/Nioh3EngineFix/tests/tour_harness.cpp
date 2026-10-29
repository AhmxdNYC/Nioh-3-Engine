// tour_harness.cpp - offline tests of the Tour mode (src/tour_core.h + the thunks of src/tour.cpp). Not a game test.
// Built and run by tests\run_tour_harness.py. Prints "ALL PASSED" on success.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "../src/tour.h"
#include "../src/tour_core.h"

// ---- stubs for the modules tour.cpp talks to (not exercised here) -------------------------------------------------------
namespace gp {
void SetExternalWindows(bool) {}
void LogWindow(const char*) {}
}  // namespace gp
namespace sp {
void SetExternalWindows(bool) {}
void BeginWindow() {}
void EndWindow(const char*) {}
}  // namespace sp
namespace memstats {
bool Vram(double*, double*, double*, double*) { return false; }
}  // namespace memstats
namespace menu {
int GamePauseReason() { return 0; }
}  // namespace menu

static int g_fail = 0, g_checks = 0;
#define CHECK(c, ...)                                  \
    do {                                               \
        ++g_checks;                                    \
        if (!(c)) {                                    \
            ++g_fail;                                  \
            printf("FAIL line %d: ", __LINE__);        \
            printf(__VA_ARGS__);                       \
            printf("\n");                              \
        }                                              \
    } while (0)

using namespace tour;

// ---- fake callees for the thunks ------------------------------------------------------------------------------------------
static volatile long g_tickCalls = 0, g_saveCalls = 0, g_backupCalls = 0;
static volatile double g_sink = 0;
static void TickFn() {  // clobbers xmm registers on purpose
    ++g_tickCalls;
    double a = 1.25;
    for (int i = 0; i < 16; ++i) a = std::sqrt(a * 3.1 + i) / 1.7;
    g_sink = a;
}
__declspec(noinline) static uint64_t FakeTick(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e) {
    return a * 3 + b * 5 + c * 7 + d * 11 + e * 13;
}
__declspec(noinline) static double FakeTickD(double a, double b, double c, double d) { return a * 2 + b * 3 + c * 5 + d * 7; }
__declspec(noinline) static uint64_t FakeSave(uint64_t a, uint64_t b, uint64_t c, uint64_t d, DWORD* err) {
    ++g_saveCalls;
    if (err) *err = 5;
    return (a ^ b ^ c ^ d) & 0xFFFFFFFFFFFFFF00ull;  // al = 0: "write failed"
}
__declspec(noinline) static uint64_t FakeBackup(uint64_t, uint64_t, uint64_t, uint64_t) {
    ++g_backupCalls;
    return 0x55;
}

static void TestList() {
    int bad = -1;
    const std::string t =
        "# header\n"
        "7356   # Escape from Hitokoto Slope\n"
        "\n"
        "0x34   # hex key 52\n"
        "7292@100, 200 ,300\n"
        "7292@1.5,2.5,3.5   # extra\n"
        "99999  # too big\n"
        "abc\n"
        "52@1,2\n"
        "5631@4,5,6\n"
        "7356\n";
    const std::vector<Stage> v = ParseTourList(t, &bad);
    CHECK(bad == 3, "bad lines %d (want 3)", bad);
    CHECK(v.size() == 5, "stages %zu (want 5)", v.size());
    if (v.size() == 5) {
        CHECK(v[0].key == 7356 && v[0].label == "Escape from Hitokoto Slope" && v[0].extra.empty(), "stage 0");
        CHECK(v[1].key == 52 && v[1].label == "hex key 52", "stage 1 key %u label %s", v[1].key, v[1].label.c_str());
        CHECK(v[2].key == 7292 && v[2].extra.size() == 2 && v[2].extra[0].x == 100 && v[2].extra[0].z == 300 &&
                  std::fabs(v[2].extra[1].y - 2.5f) < 1e-6f,
              "stage 2 (@ points start a stage and join it)");
        CHECK(v[3].key == 5631 && v[3].extra.size() == 1, "stage 3");
        CHECK(v[4].key == 7356 && v[4].extra.empty(), "stage 4 (a key may repeat)");
    }
    CHECK(ParseTourList("", &bad).empty() && bad == 0, "empty list");
    CHECK(ParseTourList("7356", &bad).size() == 1, "no trailing newline");
}

static void TestProgress() {
    Progress p = ParseProgress("L 0 0 7356\nA 0 0 7356 ok\nB 0 0 0\nD 0 0 0\nB 0 0 1\n");
    CHECK(p.crashPts.size() == 1 && p.crashPts[0] == std::make_tuple(0, 0, 1), "crash point after B without D");
    CHECK(p.donePts.count({0, 0, 0}) && p.donePts.count({0, 0, 1}), "done + crashed points skipped");
    CHECK(p.doneStages.empty() && p.crashLoads.empty(), "stage 0 not done, no load crash");
    p = ParseProgress("L 0 0 7356\nA 0 0 7356 ok\nB 0 0 0\nD 0 0 0\nS 0 0\nL 0 1 52\n");
    CHECK(p.crashLoads.size() == 1 && p.crashLoads[0] == std::make_pair(0, 1), "load crash");
    CHECK(p.doneStages.count({0, 0}) && p.doneStages.count({0, 1}), "stage 0 done, stage 1 skipped after its load crash");
    CHECK(p.crashPts.empty(), "no point crash");
    p = ParseProgress("L 0 2 7292\nA 0 2 7292 stall\nL 0 3 1\nA 0 3 1 locked\n");
    CHECK(p.doneStages.count({0, 2}) && p.doneStages.count({0, 3}) && p.crashLoads.empty(), "stall / locked stages are done");
    p = ParseProgress("L 0 0 1\r\nA 0 0 1 ok\r\nB 0 0 2\r\nC 0 0 2\r\nB 0 0 3\r\nD 0 0 3\r\nE\r\n");
    CHECK(p.finished && p.oldCrashPts.size() == 1 && p.crashPts.empty() && p.donePts.count({0, 0, 2}), "C line + E, CRLF");
    p = ParseProgress("");
    CHECK(p.lines == 0 && !p.finished && p.crashPts.empty(), "empty progress");
    // pass 2 resume: points of pass 1 do not count for pass 2
    p = ParseProgress("B 0 0 0\nD 0 0 0\nS 0 0\nL 1 0 7356\nA 1 0 7356 ok\nB 1 0 0\n");
    CHECK(p.donePts.count({1, 0, 0}) && p.crashPts.size() == 1 && !p.doneStages.count({1, 0}), "second pass crash");
}

static void TestFps() {
    std::vector<Pos> c = {{1, 0, 0}, {10, 0, 0}, {5, 0, 0}, {10, 0, 1}};
    std::vector<int> k = FarthestPoints({0, 0, 0}, c, 2, 0);
    CHECK(k.size() == 2 && k[0] == 3 && k[1] == 2, "fps order");
    k = FarthestPoints({0, 0, 0}, c, 3, 6);
    CHECK(k.size() == 1 && k[0] == 3, "fps min distance");
    k = FarthestPoints({0, 0, 0}, c, 0, 0);
    CHECK(k.empty(), "fps k 0");
    k = FarthestPoints({0, 0, 0}, {}, 3, 0);
    CHECK(k.empty(), "fps no candidates");
    CHECK(!PosOk({0, 0, 0}) && PosOk({1, 2, 3}) && !PosOk({NAN, 0, 1}) && !PosOk({2e7f, 0, 1}), "PosOk");
}

static void TestStats() {
    std::vector<double> ms(196, 10.0);
    for (int i = 0; i < 4; ++i) ms.push_back(30.0);
    const FrameStats s = ComputeFrameStats(ms);
    CHECK(s.frames == 200 && std::fabs(s.avgMs - 10.4) < 1e-9 && s.p50 == 10.0 && s.p99 == 30.0 && s.maxMs == 30.0 && s.over33 == 0,
          "stats: frames %d avg %.3f p50 %.1f p99 %.1f max %.1f", s.frames, s.avgMs, s.p50, s.p99, s.maxMs);
    std::vector<double> one = {40.0};
    const FrameStats s1 = ComputeFrameStats(one);
    CHECK(s1.p99 == 40.0 && s1.over33 == 1 && std::fabs(s1.avgFps - 25.0) < 1e-9, "stats single frame");
    std::vector<double> none;
    CHECK(ComputeFrameStats(none).frames == 0, "stats empty");
}

static void TestSavePath() {
    CHECK(IsSavePath(L"C:\\Users\\X\\AppData\\Local\\KoeiTecmo\\NIOH3\\Savedata\\76561190000000000\\SAVEDATA00\\SAVEDATA.BIN"), "save");
    CHECK(IsSavePath(L"C:\\Users\\X\\AppData\\Local\\KoeiTecmo\\NIOH3\\Savedata_backup\\1\\BACKUP.BIN"), "save backup");
    CHECK(IsSavePath(L"c:/users/x/appdata/local/koeitecmo/nioh3/SAVEDATA/1/x.tmp"), "forward slashes, case");
    CHECK(!IsSavePath(L"C:\\Users\\X\\AppData\\Local\\KoeiTecmo\\NIOH3\\config.xml"), "config.xml is not a save");
    CHECK(!IsSavePath(L"C:\\Games\\Nioh3\\Nioh3EngineFix_tour\\progress.txt"), "mod files");
    CHECK(!IsSavePath(nullptr) && !IsSavePath(L""), "null / empty");
}

static void TestThunks() {
    uint8_t* page = (uint8_t*)VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    uint8_t* page2 = (uint8_t*)VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    TestThunkSet t, t2;
    CHECK(page && page2 && TestBuildThunks(page, (void*)&FakeTick, (void*)&FakeSave, (void*)&FakeBackup, &TickFn, &t),
          "build thunks");
    if (!t.tick) return;
    // tick: integer args incl. a stack arg pass through; tick function runs once per call
    using Tick5 = uint64_t (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
    const long c0 = g_tickCalls;
    const uint64_t want = FakeTick(1, 22, 333, 4444, 55555);
    CHECK(((Tick5)t.tick)(1, 22, 333, 4444, 55555) == want && g_tickCalls == c0 + 1, "tick thunk int args");
    for (uint64_t i = 0; i < 1000; ++i)
        if (((Tick5)t.tick)(i, ~i, i * 7, i ^ 0x1234, 1ull << (i % 63)) != FakeTick(i, ~i, i * 7, i ^ 0x1234, 1ull << (i % 63))) {
            CHECK(false, "tick thunk mismatch at %llu", (unsigned long long)i);
            break;
        }
    CHECK(g_tickCalls == c0 + 1001, "tick fn called every pass (%ld)", g_tickCalls - c0);
    // tick with float args (xmm0-3 preserved across the C++ tick function)
    CHECK(TestBuildThunks(page2, (void*)&FakeTickD, (void*)&FakeSave, (void*)&FakeBackup, &TickFn, &t2), "build thunks 2");
    using TickD = double (*)(double, double, double, double);
    CHECK(((TickD)t2.tick)(1.5, -2.25, 3.125, 1e9) == FakeTickD(1.5, -2.25, 3.125, 1e9), "tick thunk xmm args");
    // TestBuildThunks re-pointed the globals at page2: use t2's block/counters for the save thunks below
    using Save5 = uint64_t (*)(uint64_t, uint64_t, uint64_t, uint64_t, DWORD*);
    using Back4 = uint64_t (*)(uint64_t, uint64_t, uint64_t, uint64_t);
    DWORD err = 77;
    long s0 = g_saveCalls, b0 = g_backupCalls;
    CHECK(*t2.block == 1, "block on after build");
    uint64_t r = ((Save5)t2.sb1)(1, 2, 3, 4, &err);
    CHECK((r & 0xFF) == 1 && err == 0 && g_saveCalls == s0 && *t2.sb1Count == 1, "SB1 blocked: al 1, err 0, writer not called");
    r = ((Save5)t2.sb1)(1, 2, 3, 4, nullptr);
    CHECK((r & 0xFF) == 1 && g_saveCalls == s0 && *t2.sb1Count == 2, "SB1 blocked with a null err pointer");
    r = ((Back4)t2.sb2)(9, 8, 7, 6);
    CHECK((r & 0xFF) == 1 && g_backupCalls == b0 && *t2.sb2Count == 1, "SB2 blocked");
    *t2.block = 0;
    err = 77;
    r = ((Save5)t2.sb1)(0x100, 0x200, 0x400, 0x800, &err);
    CHECK(r == FakeSave(0x100, 0x200, 0x400, 0x800, nullptr) && err == 5 && g_saveCalls == s0 + 2 && *t2.sb1Count == 2,
          "SB1 off: the original runs with all five args");
    r = ((Back4)t2.sb2)(1, 2, 3, 4);
    CHECK(r == 0x55 && g_backupCalls == b0 + 1 && *t2.sb2Count == 1, "SB2 off: the original runs");
    // cost of the tick thunk around an empty tick (informational)
    LARGE_INTEGER f, q0, q1;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&q0);
    for (int i = 0; i < 200000; ++i) ((Tick5)t.tick)(i, 2, 3, 4, 5);
    QueryPerformanceCounter(&q1);
    printf("tick thunk + tick fn: %.1f ns per call\n", (double)(q1.QuadPart - q0.QuadPart) * 1e9 / f.QuadPart / 200000.0);
    VirtualFree(page, 0, MEM_RELEASE);
    VirtualFree(page2, 0, MEM_RELEASE);
}

int main() {
    TestList();
    TestProgress();
    TestFps();
    TestStats();
    TestSavePath();
    TestThunks();
    printf("%d checks, %d failed\n", g_checks, g_fail);
    if (g_fail) return 1;
    printf("ALL PASSED\n");
    return 0;
}
