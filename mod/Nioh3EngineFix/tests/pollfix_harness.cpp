// pollfix_harness.cpp - offline test of PollFix (#108505) in src/thrsched.cpp. Not a game test.
// Same fake image as fpw_harness.cpp (real v2.02 FileLoad.Poll / FileLoad bytes, tests/fpw_fixture.h), run on the FULL
// process affinity with [Patches] FileLoadPoll = 0 and PollFix = 1. Checks:
//   P1 install: "PollFix: ON" logged, both unlock calls retargeted, no FPW gate line (FileLoadPoll = 0 is independent).
//   P2 any CPU count: idle looping Poll passes back off (pause/yield/sleep) on all CPUs (FPW alone would be stock above 6).
//   P3 progress: read-done advancing every pass = no back-off.
//   P4 wait-on-work: FileLoad idles in a long timer wait (SleepUs 5000); a Poll pass that advances read-done ends that
//      wait at once (median resume < 1500 us vs 5000 us timer) and the stats count the wake.
//   P5 not looping: no back-off, unlock called with the same rcx.
//   P6 PollFix = 0 + FileLoadPoll = 0 (separate process run, arg "off"): nothing installed, nothing logged.
// Prints ALL PASSED on success.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "../src/thrsched.h"
#include "fpw_fixture.h"

static std::vector<std::string> g_lines;
static void Log(const char* fmt, ...) {
    char b[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    g_lines.push_back(b);
}
static bool Logged(const char* s) {
    for (auto& l : g_lines)
        if (l.find(s) != std::string::npos) return true;
    return false;
}
static int g_fail = 0;
#define CHECK(c)                                                   \
    do {                                                           \
        if (!(c)) {                                                \
            printf("FAIL line %d: %s\n", __LINE__, #c);            \
            ++g_fail;                                              \
        }                                                          \
    } while (0)

constexpr uint32_t kSize = 0x52FB000, kSlot = 0x38E2630, kPollSite = 0x116F12, kLoadSite = 0x11702C;

static uint8_t* MakeImage(bool corrupt) {
    uint8_t* b = (uint8_t*)VirtualAlloc(nullptr, kSize, MEM_RESERVE, PAGE_NOACCESS);
    if (!b) return nullptr;
    VirtualAlloc(b, 0x1000, MEM_COMMIT, PAGE_READWRITE);
    VirtualAlloc(b + 0x116000, 0x2000, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    VirtualAlloc(b + (kSlot & ~0xFFFu), 0x1000, MEM_COMMIT, PAGE_READWRITE);
    auto* dos = (IMAGE_DOS_HEADER*)b;
    dos->e_magic = IMAGE_DOS_SIGNATURE;
    dos->e_lfanew = 0x80;
    auto* nt = (IMAGE_NT_HEADERS64*)(b + 0x80);
    nt->Signature = IMAGE_NT_SIGNATURE;
    nt->FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
    nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
    nt->OptionalHeader.SizeOfImage = kSize;
    const char* h = kFpwFixHex;
    for (size_t i = 0; h[2 * i] && h[2 * i + 1]; ++i) {
        char t[3] = {h[2 * i], h[2 * i + 1], 0};
        b[kFpwFixRva + i] = (uint8_t)strtoul(t, nullptr, 16);
    }
    if (corrupt) b[kLoadSite + 6] = 0x90;  // mov eax,[rsi+1Ch] no longer there
    return b;
}

// Fake ctx: SRW lock at +CD0h, ring counters 6BC (produced) / 6C0 / 6C4 (read done) / 6C8 (retired).
alignas(16) static uint8_t g_ctx[0x1000];
static uint32_t* Ctr(int i) { return (uint32_t*)(g_ctx + 0x6BC + 4 * i); }
static void* g_unlockRcx = nullptr;
static USHORT g_frames = 0;
static void* g_stack[32];
static void* volatile g_caller = nullptr;
static void WINAPI FakeUnlock(void* lock) {
    g_unlockRcx = lock;
    // Manual unwind (RtlLookupFunctionEntry + RtlVirtualUnwind, as crashblame.cpp / crashreport.cpp walk a crash stack).
    CONTEXT c;
    RtlCaptureContext(&c);
    g_frames = 0;
    for (int i = 0; i < 8 && c.Rip; ++i) {
        g_stack[g_frames++] = (void*)c.Rip;
        DWORD64 imgBase = 0;
        RUNTIME_FUNCTION* rf = RtlLookupFunctionEntry(c.Rip, &imgBase, nullptr);
        if (!rf) {  // leaf
            c.Rip = *(DWORD64*)c.Rsp;
            c.Rsp += 8;
            continue;
        }
        void* hd = nullptr;
        DWORD64 est = 0;
        RtlVirtualUnwind(UNW_FLAG_NHANDLER, imgBase, c.Rip, rf, &c, &hd, &est, nullptr);
    }
}
using Thunk = void (*)(void*);
static Thunk ThunkAt(uint8_t* site) {
    int32_t r;
    memcpy(&r, site + 1, 4);
    return (Thunk)(site + 5 + r);
}
__declspec(noinline) static void CallThunk(Thunk t, void* rcx) {
    g_caller = _ReturnAddress();
    t(rcx);
}
static void Status(long long* v) {  // Poll passes, pause, yield, sleep, ms, FileLoad passes, pause, yield, sleep, ms
    char buf[1024];
    thrsched::FlpStatusLine(buf, sizeof buf);
    const char* p = strstr(buf, "FPW: Poll passes");
    memset(v, 0, 10 * sizeof(long long));
    if (p)
        sscanf_s(p, "FPW: Poll passes %lld (pause %lld, yield %lld, sleep %lld = %lld ms) | FileLoad passes %lld (pause %lld, yield %lld, sleep %lld = %lld ms)",
               &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7], &v[8], &v[9]);
}
static double Us(LARGE_INTEGER a, LARGE_INTEGER b) {
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    return (b.QuadPart - a.QuadPart) * 1e6 / f.QuadPart;
}

static void PfWakes(long long* w) {  // wakes to Poll, ended a wait, wakes to FileLoad, ended a wait
    char buf[1024];
    thrsched::FlpStatusLine(buf, sizeof buf);
    memset(w, 0, 4 * sizeof(long long));
    if (const char* p = strstr(buf, "PollFix ON: wakes"))
        sscanf_s(p, "PollFix ON: wakes to Poll %lld (ended a wait %lld), to FileLoad %lld (ended a wait %lld)", &w[0], &w[1], &w[2], &w[3]);
}

static uint8_t* g_lock = g_ctx + 0xCD0;
static Thunk g_tp = nullptr, g_tl = nullptr;
static volatile LONG g_stop = 0;
static volatile LONG64 g_loadPasses = 0;
static LARGE_INTEGER g_lastLoadPass{};
// FileLoad thread: native loop shape = keep calling its thunk while produced > read done.
static DWORD WINAPI FileLoadThread(LPVOID) {
    while (!g_stop) {
        CallThunk(g_tl, g_lock);
        QueryPerformanceCounter(&g_lastLoadPass);
        InterlockedIncrement64(&g_loadPasses);
    }
    return 0;
}

int main(int argc, char** argv) {
    wchar_t ini[MAX_PATH];
    GetTempPathW(MAX_PATH, ini);
    wcscat_s(ini, L"pollfix_harness.ini");
    DeleteFileW(ini);
    const bool off = argc > 1 && !strcmp(argv[1], "off");
    WritePrivateProfileStringW(L"Patches", L"FileLoadPoll", L"0", ini);
    WritePrivateProfileStringW(L"Patches", L"FpwDefaultV", L"1", ini);
    WritePrivateProfileStringW(L"Patches", L"PollFix", off ? L"0" : L"1", ini);
    WritePrivateProfileStringW(L"Tuning", L"FileLoadPoll_SleepUs", L"5000", ini);
    DWORD_PTR pm = 0, sysm = 0;
    GetProcessAffinityMask(GetCurrentProcess(), &pm, &sysm);
    int nAll = 0;
    for (int i = 0; i < 64; ++i) nAll += (sysm >> i) & 1;
    uint8_t* b = MakeImage(false);
    *(void**)(b + kSlot) = (void*)&FakeUnlock;
    uint8_t before[6];
    memcpy(before, b + kPollSite, 6);
    thrsched::StartInfo si;
    si.log = Log;
    si.iniPath = ini;
    si.base = (uintptr_t)b;
    thrsched::Start(si);
    for (auto& l : g_lines) printf("  log: %s\n", l.c_str());
    if (off) {
        bool any = false;
        for (auto& l : g_lines) any |= l.find("FPW") != std::string::npos || l.find("PollFix") != std::string::npos;
        CHECK(!any);
        CHECK(!memcmp(before, b + kPollSite, 6));
        printf("P6 PollFix = 0 + FileLoadPoll = 0: %s\n", any ? "something installed (BAD)" : "nothing installed, site unchanged");
        DeleteFileW(ini);
        printf(g_fail ? "FAILED (%d)\n" : "ALL PASSED\n", g_fail);
        return g_fail ? 1 : 0;
    }
    CHECK(Logged("PollFix: ON") && Logged("wake events ok"));
    CHECK(!Logged("FPW: installed") && !Logged("FPW: process affinity"));
    CHECK(b[kPollSite] == 0xE8 && b[kPollSite + 5] == 0x90 && b[kLoadSite] == 0xE8 && b[kLoadSite + 5] == 0x90);
    g_tp = ThunkAt(b + kPollSite), g_tl = ThunkAt(b + kLoadSite);
    printf("P1 install on %d CPUs with FileLoadPoll = 0: PollFix ON, no FPW gate line\n", nAll);

    long long v[10], e[10];
    // P5: not looping
    *Ctr(0) = 5, *Ctr(1) = 5, *Ctr(2) = 5, *Ctr(3) = 5;
    for (int i = 0; i < 1000; ++i) CallThunk(g_tp, g_lock);
    Status(v);
    CHECK(g_unlockRcx == g_lock);
    CHECK(v[0] == 1000 && v[1] == 0 && v[2] == 0 && v[3] == 0);
    printf("P5 not looping: 1000 passes, unlock rcx ok=%d, pause/yield/sleep %lld/%lld/%lld\n", g_unlockRcx == g_lock, v[1], v[2], v[3]);
    // P3: progress every pass
    *Ctr(0) = 100000, *Ctr(2) = 0, *Ctr(3) = 0;
    for (int i = 0; i < 2000; ++i) {
        ++*Ctr(2);
        ++*Ctr(3);
        CallThunk(g_tp, g_lock);
    }
    Status(e);
    CHECK(e[1] == v[1] && e[2] == v[2] && e[3] == v[3]);
    printf("P3 progress each pass: pause/yield/sleep +%lld/+%lld/+%lld\n", e[1] - v[1], e[2] - v[2], e[3] - v[3]);
    // P2: idle looping on ALL CPUs backs off
    *Ctr(0) = 10, *Ctr(2) = 10, *Ctr(3) = 4;
    LARGE_INTEGER t0, t1;
    QueryPerformanceCounter(&t0);
    int passes = 0;
    long long f[10];
    do {
        CallThunk(g_tp, g_lock);
        ++passes;
        Status(f);
        QueryPerformanceCounter(&t1);
    } while (f[3] - e[3] < 3 && Us(t0, t1) < 2e6);
    CHECK(f[3] - e[3] >= 3);
    CHECK((f[1] - e[1]) + (f[2] - e[2]) + (f[3] - e[3]) == passes - 1);
    printf("P2 idle on %d CPUs: %d passes = pause %lld, yield %lld, sleep %lld in %.1f ms\n", nAll, passes, f[1] - e[1], f[2] - e[2], f[3] - e[3],
           Us(t0, t1) / 1000);

    // P4: FileLoad on its own thread waits for read done (produced 20 > read done 10). Poll passes advance read done
    // once every ~20 ms; each advance must end FileLoad's 5 ms timer wait at once (wake), not after the timer.
    *Ctr(0) = 20, *Ctr(1) = 20, *Ctr(2) = 10, *Ctr(3) = 10;
    HANDLE th = CreateThread(nullptr, 0, FileLoadThread, nullptr, 0, nullptr);
    CHECK(th != nullptr);
    // latency, not scheduling noise: a game or build running on the same PC must not decide this check
    SetThreadPriority(th, THREAD_PRIORITY_TIME_CRITICAL);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    Sleep(50);  // FileLoad reaches its long timer waits
    long long w0[4];
    PfWakes(w0);
    std::vector<double> lat;
    for (int r = 0; r < 9; ++r) {
        Sleep(20);
        const LONG64 passes0 = g_loadPasses;
        LARGE_INTEGER a;
        QueryPerformanceCounter(&a);
        ++*Ctr(2);  // Poll's read-done step advanced
        ++*Ctr(3);
        CallThunk(g_tp, g_lock);  // Poll's per-pass unlock sees the move and wakes FileLoad
        LARGE_INTEGER c;
        do QueryPerformanceCounter(&c);
        while (g_loadPasses == passes0 && Us(a, c) < 20000);
        lat.push_back(Us(a, c));
    }
    InterlockedExchange(&g_stop, 1);
    *Ctr(2) = *Ctr(0);  // let the loop condition drop
    WaitForSingleObject(th, 2000);
    CloseHandle(th);
    long long w1[4];
    PfWakes(w1);
    std::sort(lat.begin(), lat.end());
    const double med = lat[lat.size() / 2];
    CHECK(med < 1500);
    CHECK(w1[2] - w0[2] >= 5 && w1[3] - w0[3] >= 5);
    printf("P4 wait-on-work: FileLoad resumes %.0f us (median of 9, min %.0f, max %.0f) after Poll moves read done, timer 5000 us; "
           "wakes to FileLoad +%lld, ended a wait +%lld\n", med, lat.front(), lat.back(), w1[2] - w0[2], w1[3] - w0[3]);
    char buf[1024];
    thrsched::FlpStatusLine(buf, sizeof buf);
    printf("  stats: %s\n", buf);
    DeleteFileW(ini);
    printf(g_fail ? "FAILED (%d)\n" : "ALL PASSED\n", g_fail);
    return g_fail ? 1 : 0;
}
