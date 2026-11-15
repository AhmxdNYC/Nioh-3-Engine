// fpw_harness.cpp - offline test of FPW, the file-loader poll wait in src/thrsched.cpp (#103680). Not a game test.
// A fake image (PE header with the v2.02 SizeOfImage, the real v2.02 bytes of the FileLoad.Poll / FileLoad bodies at RVA
// 116D30..117060 from tests/fpw_fixture.h, and the unlock IAT slot 38E2630) is handed to thrsched::Start with
// [Patches] FileLoadPoll = 1. Checks:
//   A install: "FPW: installed" logged; both unlock calls (116F12, 11702C) became E8 rel32 + 90 to two thunks.
//   B thunk = native unlock: calling the Poll thunk with rcx = ctx+CD0h calls the IAT slot's function with that rcx.
//   C unwind: from inside that unlock function RtlLookupFunctionEntry + RtlVirtualUnwind (the crash reporters walk) reaches the caller.
//   D not looping (produced == read-done == retired): no back-off, every call returns at once (no sleep counted).
//   E looping with progress every pass (read-done advances): no pause / yield / sleep.
//   F looping, no progress: pause first, then yields, then timer sleeps (each pass < 2 ms).
//   G FileLoad thunk: its loop condition is produced > read-done only (retired lagging does not back off).
//   H wrong bytes (a different build / another patch): "FPW: not installed", the site bytes are left unchanged.
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

int main() {
    wchar_t ini[MAX_PATH];
    GetTempPathW(MAX_PATH, ini);
    wcscat_s(ini, L"fpw_harness.ini");
    WritePrivateProfileStringW(L"Patches", L"FileLoadPoll", L"1", ini);

    // H first (Start installs once per process image; use a separate fake image).
    {
        uint8_t* bad = MakeImage(true);
        uint8_t before[6];
        memcpy(before, bad + kPollSite, 6);
        thrsched::StartInfo si;
        si.log = Log;
        si.iniPath = ini;
        si.base = (uintptr_t)bad;
        thrsched::Start(si);
        CHECK(Logged("FPW: not installed (loader poll loop bytes differ"));
        CHECK(!memcmp(before, bad + kPollSite, 6));
        printf("H wrong bytes: %s\n", Logged("FPW: not installed") ? "not installed, site unchanged" : "?");
    }
    g_lines.clear();
    uint8_t* b = MakeImage(false);
    *(void**)(b + kSlot) = (void*)&FakeUnlock;
    thrsched::StartInfo si;
    si.log = Log;
    si.iniPath = ini;
    si.base = (uintptr_t)b;
    thrsched::Start(si);
    for (auto& l : g_lines) printf("  log: %s\n", l.c_str());
    CHECK(Logged("FPW: installed"));
    CHECK(b[kPollSite] == 0xE8 && b[kPollSite + 5] == 0x90 && b[kLoadSite] == 0xE8 && b[kLoadSite + 5] == 0x90);
    Thunk tp = ThunkAt(b + kPollSite), tl = ThunkAt(b + kLoadSite);
    CHECK(tp != tl);
    printf("A install: Poll thunk %p, FileLoad thunk %p\n", (void*)tp, (void*)tl);

    void* lock = g_ctx + 0xCD0;
    long long v[10];
    // B + C + D: not looping.
    *Ctr(0) = 5, *Ctr(1) = 5, *Ctr(2) = 5, *Ctr(3) = 5;
    LARGE_INTEGER t0, t1;
    QueryPerformanceCounter(&t0);
    for (int i = 0; i < 1000; ++i) CallThunk(tp, lock);
    QueryPerformanceCounter(&t1);
    CHECK(g_unlockRcx == lock);
    bool found = false;
    for (USHORT i = 0; i < g_frames; ++i)
        if (g_stack[i] == g_caller) found = true;
    CHECK(found);
    for (USHORT i = 0; i < g_frames; ++i) printf("  frame %u: %p\n", i, g_stack[i]);
    printf("  caller %p, thunk %p\n", g_caller, (void*)tp);
    Status(v);
    CHECK(v[0] == 1000 && v[1] == 0 && v[2] == 0 && v[3] == 0);
    printf("B/C/D not looping: unlock rcx ok=%d, stack walk %u frames reaches caller=%d, 1000 passes %.1f us total, pause/yield/sleep %lld/%lld/%lld\n",
           g_unlockRcx == lock, g_frames, found, Us(t0, t1), v[1], v[2], v[3]);

    // E: looping with progress every pass.
    *Ctr(0) = 100000, *Ctr(2) = 0, *Ctr(3) = 0;
    for (int i = 0; i < 2000; ++i) {
        ++*Ctr(2);
        ++*Ctr(3);
        CallThunk(tp, lock);
    }
    long long e[10];
    Status(e);
    CHECK(e[1] == v[1] && e[2] == v[2] && e[3] == v[3]);
    printf("E progress each pass: 2000 passes, pause/yield/sleep +%lld/+%lld/+%lld\n", e[1] - v[1], e[2] - v[2], e[3] - v[3]);

    // F: looping, no progress (fence not reached: read done 10 > retired 4) until 3 timer sleeps (or 1 s).
    // Every idle pass after the first lands in exactly one bucket: pause (< SpinUs), yield (< YieldUs) or sleep. Which
    // bucket depends on wall time, so a busy PC (a game running) may skip pause / yield; the sum may not.
    *Ctr(0) = 10, *Ctr(2) = 10, *Ctr(3) = 4;
    QueryPerformanceCounter(&t0);
    int passes = 0;
    double maxPass = 0;
    long long f[10];
    do {
        LARGE_INTEGER a, c;
        QueryPerformanceCounter(&a);
        CallThunk(tp, lock);
        QueryPerformanceCounter(&c);
        maxPass = std::max(maxPass, Us(a, c));
        ++passes;
        Status(f);
        QueryPerformanceCounter(&t1);
    } while (f[3] - e[3] < 3 && Us(t0, t1) < 1e6);
    const long long fp = f[1] - e[1], fy = f[2] - e[2], fs = f[3] - e[3];
    CHECK(fs >= 3);
    CHECK(fp + fy + fs == passes - 1);
    printf("F idle: %d passes = 1 first + pause %lld, yield %lld, sleep %lld (%lld ms slept), %.1f ms, longest pass %.0f us\n", passes, fp,
           fy, fs, f[4] - e[4], Us(t0, t1) / 1000, maxPass);
    // progress again: next pass returns at once, no sleep
    ++*Ctr(3);
    QueryPerformanceCounter(&t0);
    CallThunk(tp, lock);
    QueryPerformanceCounter(&t1);
    long long g2[10];
    Status(g2);
    CHECK(g2[3] == f[3] && g2[2] == f[2]);
    printf("  progress after idle: next pass %.1f us, no back-off\n", Us(t0, t1));

    // G: FileLoad thunk, produced == read done but retired lags -> not looping for FileLoad (no back-off ever).
    *Ctr(0) = 10, *Ctr(2) = 10, *Ctr(3) = 4;
    g_unlockRcx = nullptr;
    QueryPerformanceCounter(&t0);
    do {
        CallThunk(tl, lock);
        QueryPerformanceCounter(&t1);
    } while (Us(t0, t1) < 2000);
    long long g[10];
    Status(g);
    CHECK(g_unlockRcx == lock);
    CHECK(g[5] > 0 && g[6] == 0 && g[7] == 0 && g[8] == 0);
    // FileLoad looping without progress -> backs off (same bucket rule as F)
    *Ctr(0) = 11;
    QueryPerformanceCounter(&t0);
    int lp = 0;
    long long h[10];
    do {
        CallThunk(tl, lock);
        ++lp;
        Status(h);
        QueryPerformanceCounter(&t1);
    } while (h[8] < 3 && Us(t0, t1) < 1e6);
    CHECK(h[8] >= 3);
    CHECK(h[6] + h[7] + h[8] == lp - 1);
    printf("G FileLoad: retired lagging = no back-off (%lld passes); produced > read done idle = pause %lld, yield %lld, sleep %lld\n", g[5],
           h[6], h[7], h[8]);

    DeleteFileW(ini);
    printf(g_fail ? "FAILED (%d)\n" : "ALL PASSED\n", g_fail);
    return g_fail ? 1 : 0;
}
