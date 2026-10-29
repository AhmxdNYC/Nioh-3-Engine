// thrsched_harness.cpp - offline test of src/thrsched.cpp (TSTATE census + RTP). Not a game test.
// Builds a contention case: "[App]Game", "SynergisticDispatcherTask" and 3 "KIDSTaskScheduler%d" busy threads all pinned to ONE
// logical processor (5 runnable threads, 1 CPU), runs the real census + RTP code against this process and checks:
//   A (RTP off, 5 s window): each spinner gets ~1/5 of the CPU; [App]Game READY share >= 50 % (runnable without a CPU) and
//     its CPU 10-35 % of a core; the census rows/labels/groups are in the log.
//   B (RTP on, level ABOVE_NORMAL): [App]Game + dispatcher are raised (GetThreadPriority == 1), they take the CPU (both >= 35 %,
//     sum >= 85 %), the workers drop (each <= 10 %) and their READY share rises; the stats line names the boosted threads.
//   C (RTP off again): the previous priorities are restored (0).
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
    char b[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    std::lock_guard<std::mutex> l(g_mx);
    g_lines.push_back(b);
}
static unsigned long PresentTid() { return 0; }

static std::atomic<bool> g_stop{false};
static DWORD_PTR g_mask = 1;
static DWORD WINAPI Spin(LPVOID p) {
    SetThreadDescription(GetCurrentThread(), static_cast<const wchar_t*>(p));
    SetThreadAffinityMask(GetCurrentThread(), g_mask);
    volatile uint64_t x = 0;
    while (!g_stop.load(std::memory_order_relaxed)) x = x + 1;
    return 0;
}

static int fails = 0;
static void Check(bool c, const char* m) {
    if (!c) {
        ++fails;
        printf("FAIL: %s\n", m);
    }
}

struct RowV {
    double cpu = -1, ready = -1;
    bool found = false;
};
// parse the newest "TSTATE  <label> <tid> | cpu X % | run Y | READY Z" row for a tid among lines[from..]
static RowV Find(size_t from, DWORD tid) {
    RowV r;
    std::lock_guard<std::mutex> l(g_mx);
    char key[32];
    snprintf(key, sizeof key, " %lu | cpu", (unsigned long)tid);
    for (size_t i = from; i < g_lines.size(); ++i) {
        const std::string& s = g_lines[i];
        if (s.rfind("TSTATE  ", 0) != 0) continue;
        const size_t k = s.find(key);
        if (k == std::string::npos) continue;
        double cpu, run, rdy;
        if (sscanf_s(s.c_str() + k + strlen(key), " %lf %% | run %lf | READY %lf", &cpu, &run, &rdy) == 3) {
            r.cpu = cpu;
            r.ready = rdy;
            r.found = true;
        }
    }
    return r;
}
static size_t NLines() {
    std::lock_guard<std::mutex> l(g_mx);
    return g_lines.size();
}
static bool WaitWindow(size_t from, int maxSec) {
    for (int i = 0; i < maxSec * 10; ++i) {
        Sleep(100);
        std::lock_guard<std::mutex> l(g_mx);
        for (size_t k = from; k < g_lines.size(); ++k)
            if (g_lines[k].rfind("TSTATE group", 0) == 0) return true;
    }
    return false;
}

int main() {
    const DWORD ncpu = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    if (ncpu < 2) {
        printf("need >= 2 logical processors\n");
        return 1;
    }
    g_mask = (DWORD_PTR)1 << (ncpu >= 3 ? 1 : 0);  // not the census CPU (the last one)
    // HIGH class: the 5 spinners (13, raised 14) and the census (15) are not disturbed by normal-class load elsewhere on the PC
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    wchar_t ini[MAX_PATH];
    GetTempPathW(MAX_PATH, ini);
    wcscat_s(ini, L"thrsched_harness.ini");
    FILE* f = nullptr;
    _wfopen_s(&f, ini, L"w");
    fprintf(f, "[Profiler]\nThreadState = 1\nThreadStateUs = 2000\nThreadStateSec = 5\nThreadStateDelaySec = 0\nThreadStateTop = 4\n"
               "[Patches]\nRTP = 2\n[Tuning]\nRTP_Level = 1\n");
    fclose(f);

    const wchar_t* names[5] = {L"[App]Game", L"SynergisticDispatcherTask", L"KIDSTaskScheduler1", L"KIDSTaskScheduler2", L"KIDSTaskScheduler3"};
    HANDLE th[5];
    DWORD tid[5];
    for (int i = 0; i < 5; ++i) th[i] = CreateThread(nullptr, 0, Spin, (LPVOID)names[i], 0, &tid[i]);
    Sleep(300);

    thrsched::StartInfo si;
    si.log = &Log;
    si.iniPath = ini;
    si.base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    si.presentTid = &PresentTid;
    thrsched::Start(si);
    volatile uint32_t* sw = thrsched::RtpSwitch();
    Check(sw != nullptr && *sw == 0, "RTP = 2 installs the switch OFF");

    // A: off. The first window may start mid-way: wait for two windows, read the second.
    size_t m0 = NLines();
    Check(WaitWindow(m0, 15), "A: first census window");
    size_t m1 = NLines();
    Check(WaitWindow(m1, 15), "A: second census window");
    RowV g = Find(m1, tid[0]), w = Find(m1, tid[2]);
    printf("A (RTP off): [App]Game cpu %.1f %% READY %.1f %% | KIDSTaskScheduler1 cpu %.1f %% READY %.1f %%\n", g.cpu, g.ready, w.cpu, w.ready);
    Check(g.found && w.found, "A: rows for [App]Game and a worker");
    Check(g.cpu > 10 && g.cpu < 35, "A: [App]Game gets about 1/5 of the shared CPU");
    Check(g.ready >= 50, "A: [App]Game READY >= 50 % (5 runnable threads on 1 CPU)");
    Check(GetThreadPriority(th[0]) == 0, "A: priority untouched while off");

    // B: on
    *sw = 1;
    Sleep(600);
    Check(GetThreadPriority(th[0]) == THREAD_PRIORITY_ABOVE_NORMAL, "B: [App]Game raised to ABOVE_NORMAL");
    Check(GetThreadPriority(th[1]) == THREAD_PRIORITY_ABOVE_NORMAL, "B: dispatcher raised to ABOVE_NORMAL");
    Check(GetThreadPriority(th[2]) == 0, "B: workers untouched");
    size_t m2 = NLines();
    WaitWindow(m2, 15);
    size_t m3 = NLines();
    Check(WaitWindow(m3, 15), "B: full census window with RTP on");
    RowV g2 = Find(m3, tid[0]), d2 = Find(m3, tid[1]), w2 = Find(m3, tid[2]);
    printf("B (RTP on):  [App]Game cpu %.1f %% READY %.1f %% | dispatcher cpu %.1f %% | KIDSTaskScheduler1 cpu %.1f %% READY %.1f %%\n", g2.cpu,
           g2.ready, d2.cpu, w2.cpu, w2.ready);
    Check(g2.cpu >= 35 && d2.cpu >= 35 && g2.cpu + d2.cpu >= 85, "B: the raised threads take the CPU");
    Check(w2.cpu <= 10, "B: the worker gets <= 10 %");
    Check(g2.ready < g.ready, "B: [App]Game READY share falls");
    char line[1024];
    thrsched::RtpStatusLine(line, sizeof line);
    printf("%s\n", line);
    Check(strstr(line, "switch ON") && strstr(line, "main [App]Game") && strstr(line, "dispatch SynergisticDispatcher"), "B: stats line");

    // C: off again -> restored
    *sw = 0;
    Sleep(600);
    Check(GetThreadPriority(th[0]) == 0 && GetThreadPriority(th[1]) == 0, "C: priorities restored");

    g_stop = true;
    WaitForMultipleObjects(5, th, TRUE, 5000);
    {
        std::lock_guard<std::mutex> l(g_mx);
        bool head = false, groups = false, sets = false;
        for (const std::string& s : g_lines) {
            if (s.rfind("TSTATE: window", 0) == 0) head = true;
            if (s.rfind("TSTATE group KIDSTaskScheduler1+", 0) == 0) groups = true;
            if (s.rfind("TSTATE cpusets", 0) == 0) sets = true;
        }
        Check(head && groups && sets, "log has the window header, the worker group and the cpusets lines");
        printf("--- sample log (last window) ---\n");
        size_t start = g_lines.size() > 14 ? g_lines.size() - 14 : 0;
        for (size_t i = start; i < g_lines.size(); ++i) printf("%s\n", g_lines[i].c_str());
    }
    DeleteFileW(ini);
    printf(fails ? "FAILED (%d)\n" : "ALL PASSED\n", fails);
    return fails ? 1 : 0;
}
