// flp_load_harness.cpp - offline test of the FLP file-loader priority rule in src/thrsched.cpp (#103349 / #103350).
// Not a game test. A fake image (PE header + the G pointer slot at base+4759910h) and a fake G object whose byte +58Ah is
// the engine level-load window. Two sleeping threads named KTGLGraph.Cache.FileLoad / .Poll, process affinity 2 CPUs
// (<= FileLoadPrio_MaxCpus 6). Checks:
//   A loading=1 from the start: loaders keep priority 0 (never lowered during a load).
//   B loading=0 (field): both lowered to BELOW_NORMAL (-1), "lower ... (field)" logged.
//   C loading=1 again: both restored to 0 within 600 ms (250 ms tick), "restore ... (level load in progress)" logged.
//   D G pointer unreadable (0): stays 0 ("load flag unreadable"), never lowered blind.
//   E field again -> lowered; then process affinity back to all CPUs (> 6, when the PC has them) -> restored.
// Prints ALL PASSED on success.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "../src/thrsched.h"

static std::mutex g_mx;
static std::vector<std::string> g_lines;
static void Log(const char* fmt, ...) {
    char b[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    std::lock_guard<std::mutex> l(g_mx);
    g_lines.push_back(b);
}
static bool Logged(const char* a, const char* b = nullptr) {
    std::lock_guard<std::mutex> l(g_mx);
    for (const std::string& s : g_lines)
        if (s.find(a) != std::string::npos && (!b || s.find(b) != std::string::npos)) return true;
    return false;
}
static unsigned long PresentTid() { return 0; }
static std::atomic<bool> g_stop{false};
static DWORD WINAPI Idle(LPVOID p) {
    SetThreadDescription(GetCurrentThread(), static_cast<const wchar_t*>(p));
    while (!g_stop.load()) Sleep(20);
    return 0;
}
static int fails = 0;
static void Check(bool c, const char* m) {
    printf("%s: %s\n", c ? "ok  " : "FAIL", m);
    if (!c) ++fails;
}
static bool WaitPrio(HANDLE a, HANDLE b, int want, int ms) {
    for (int i = 0; i < ms / 10; ++i) {
        if (GetThreadPriority(a) == want && GetThreadPriority(b) == want) return true;
        Sleep(10);
    }
    return GetThreadPriority(a) == want && GetThreadPriority(b) == want;
}

int main() {
    const DWORD ncpu = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    DWORD_PTR pm = 0, sm = 0;
    GetProcessAffinityMask(GetCurrentProcess(), &pm, &sm);
    if (ncpu < 2) { printf("need >= 2 logical processors\n"); return 1; }
    // fake image: reserve up to the G slot, commit the header page and the slot page
    const size_t kSlot = 0x4759910, kSize = kSlot + 0x1000;
    uint8_t* img = (uint8_t*)VirtualAlloc(nullptr, kSize, MEM_RESERVE, PAGE_NOACCESS);
    VirtualAlloc(img, 0x1000, MEM_COMMIT, PAGE_READWRITE);
    VirtualAlloc(img + (kSlot & ~0xFFFull), 0x2000, MEM_COMMIT, PAGE_READWRITE);
    auto dos = (IMAGE_DOS_HEADER*)img;
    dos->e_magic = IMAGE_DOS_SIGNATURE;
    dos->e_lfanew = 0x40;
    auto nt = (IMAGE_NT_HEADERS64*)(img + 0x40);
    nt->Signature = IMAGE_NT_SIGNATURE;
    nt->OptionalHeader.SizeOfImage = (DWORD)kSize;
    static uint8_t G[0x1000] = {};
    volatile uint8_t* flag = G + 0x58A;
    volatile uintptr_t* slot = (volatile uintptr_t*)(img + kSlot);
    *slot = (uintptr_t)G;
    *flag = 1;

    wchar_t ini[MAX_PATH];
    GetTempPathW(MAX_PATH, ini);
    wcscat_s(ini, L"flp_load_harness.ini");
    FILE* f = nullptr;
    _wfopen_s(&f, ini, L"w");
    fprintf(f, "[Patches]\nFileLoadPrio = 1\n[Tuning]\nFileLoadPrio_MaxCpus = 6\n");
    fclose(f);

    DWORD_PTR two = 0;
    for (DWORD_PTR m = pm; m && __popcnt64(two) < 2; m &= m - 1) two |= m & (~m + 1);
    SetProcessAffinityMask(GetCurrentProcess(), two);
    HANDLE ld = CreateThread(nullptr, 0, Idle, (LPVOID)L"KTGLGraph.Cache.FileLoad", 0, nullptr);
    HANDLE po = CreateThread(nullptr, 0, Idle, (LPVOID)L"KTGLGraph.Cache.FileLoad.Poll", 0, nullptr);
    Sleep(200);

    thrsched::StartInfo si;
    si.log = &Log;
    si.iniPath = ini;
    si.base = (uintptr_t)img;
    si.presentTid = &PresentTid;
    thrsched::Start(si);

    Sleep(2500);
    Check(GetThreadPriority(ld) == 0 && GetThreadPriority(po) == 0, "A: loading=1 -> loaders keep priority 0");
    Check(Logged("FLP: file loader thread KTGLGraph.Cache.FileLoad tid") && Logged("FLP: file loader thread KTGLGraph.Cache.FileLoad.Poll"), "A: both loaders found");
    Check(Logged("FLP: level load in progress"), "A: load logged");

    *flag = 0;
    Check(WaitPrio(ld, po, THREAD_PRIORITY_BELOW_NORMAL, 1500), "B: field -> both BELOW_NORMAL");
    Check(Logged("FLP: lower FileLoad tid", "(field)") && Logged("FLP: lower FileLoad.Poll tid", "(field)"), "B: lower logged with reason");

    *flag = 1;
    const ULONGLONG t0 = GetTickCount64();
    const bool restored = WaitPrio(ld, po, 0, 600);
    printf("     restore took %llu ms\n", GetTickCount64() - t0);
    Check(restored, "C: load starts -> both restored to 0 within 600 ms");
    Check(Logged("FLP: restore FileLoad tid", "(level load in progress)"), "C: restore logged with reason");

    *slot = 0;
    *flag = 0;
    Sleep(1500);
    Check(GetThreadPriority(ld) == 0 && GetThreadPriority(po) == 0, "D: flag unreadable -> never lowered");
    Check(Logged("FLP: level load flag unreadable"), "D: unreadable logged");

    *slot = (uintptr_t)G;
    Check(WaitPrio(ld, po, THREAD_PRIORITY_BELOW_NORMAL, 1500), "E: field again -> lowered");
    if (__popcnt64(pm) > 6) {
        SetProcessAffinityMask(GetCurrentProcess(), pm);
        Check(WaitPrio(ld, po, 0, 1500), "E: affinity above cap -> restored");
        Check(Logged("FLP: restore", "(affinity above cap)"), "E: restore logged (affinity above cap)");
    } else printf("     E affinity step skipped (%d CPUs)\n", (int)__popcnt64(pm));

    char line[512];
    thrsched::FlpStatusLine(line, sizeof line);
    printf("%s\n", line);
    g_stop = true;
    printf("--- FLP log ---\n");
    {
        std::lock_guard<std::mutex> l(g_mx);
        for (const std::string& s : g_lines)
            if (s.rfind("FLP", 0) == 0) printf("%s\n", s.c_str());
    }
    DeleteFileW(ini);
    printf(fails ? "FAILED (%d)\n" : "ALL PASSED\n", fails);
    return fails ? 1 : 0;
}
