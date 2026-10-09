// jt_harness.cpp - JT (job-timing probe) without the game: the real src/jt.cpp + src/jt.asm caves.
//   * a watched thread named "KIDSTaskScheduler0" runs 60 "logic ticks": it kicks 3 jobs to 3 worker threads
//     ("KIDSTaskScheduler1..3") and waits for them through caveJtWait (the patched STL wait) on a condition variable;
//   * the workers run the jobs through JtCallJob (= the patched JobWorkerPoll site: rbx = job, rdx = fn, call caveJtJob);
//     job A spins 1 ms (plain), job B 3 ms (a fake ParallelFor leaf: range 4, grain 4), job C 7 ms (a fake JobNode whose
//     callback vtable +8 is NodeCallbackC). The last job to finish notifies through caveJtWake1 (the patched notify_one);
//   * a worker also waits through caveJtWait itself (fast path: not recorded).
// Checks: the cave's register contract (rcx = job inside the job, rbx preserved, return value), 60 waits found with the
// tick counter, the releasing thread = the one that ran job C, critical job = C (fn/inner), longest overlapping job = C,
// ParallelFor leaf / grain statistics, nothing dropped.  Build + run: py -I tests/run_jt_harness.py
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

#include "../src/jt.h"

extern "C" {
uint64_t JtCallJob(void* job, void* fn);
extern uint64_t jth_rbxAfter;
BOOL caveJtWait(PCONDITION_VARIABLE cv, PSRWLOCK lock, DWORD ms, ULONG flags);
void caveJtWake1(PCONDITION_VARIABLE cv);
void caveJtWakeAll(PCONDITION_VARIABLE cv);
}

static std::vector<std::string> g_lines;
static SRWLOCK g_logLock = SRWLOCK_INIT;
static void Log(const char* fmt, ...) {
    char b[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    AcquireSRWLockExclusive(&g_logLock);
    printf("%s\n", b);
    g_lines.push_back(b);
    ReleaseSRWLockExclusive(&g_logLock);
}

static void Spin(double ms) {
    LARGE_INTEGER f, a, b;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&a);
    do QueryPerformanceCounter(&b); while ((double)(b.QuadPart - a.QuadPart) * 1000.0 / (double)f.QuadPart < ms);
}

// ---- fake jobs (layouts as in jt.cpp's ReadKind) -------------------------------------------------------------------------
struct alignas(16) FakeJob {
    uint8_t raw[0x100];
    int which;   // 0 A, 1 B, 2 C
};
static volatile LONG g_left = 0;
static SRWLOCK g_lk = SRWLOCK_INIT;
static CONDITION_VARIABLE g_cv = CONDITION_VARIABLE_INIT;
static volatile LONG g_done = 0;
static volatile LONG g_badRcx = 0, g_badRbx = 0, g_badRet = 0;
static DWORD g_tidC = 0;

static void FinishOne() {
    if (InterlockedDecrement(&g_left) == 0) {
        AcquireSRWLockExclusive(&g_lk);
        g_done = 1;
        ReleaseSRWLockExclusive(&g_lk);
        caveJtWake1(&g_cv);   // the STL's notify_one after `add rcx,8`
    }
}
extern "C" __declspec(noinline) uint64_t JobPlain(FakeJob* j) {
    if (j->which != 0) InterlockedIncrement(&g_badRcx);
    Spin(1.0);
    FinishOne();
    return 0xA0A0;
}
static volatile int g_sink = 0;
extern "C" __declspec(noinline) void PforBody() { g_sink = 2; }
extern "C" __declspec(noinline) uint64_t JobPfor(FakeJob* j) {
    if (j->which != 1) InterlockedIncrement(&g_badRcx);
    Spin(3.0);
    FinishOne();
    return 0xB0B0;
}
extern "C" __declspec(noinline) void NodeCallbackC() { g_sink = 3; }
static void* g_cbVtbl[4] = {nullptr, (void*)&NodeCallbackC, nullptr, nullptr};
static void* g_cbObj[2] = {(void*)g_cbVtbl, nullptr};
static uint8_t g_desc[0x40];
extern "C" __declspec(noinline) uint64_t JobNode(FakeJob* j) {
    if (j->which != 2) InterlockedIncrement(&g_badRcx);
    g_tidC = GetCurrentThreadId();
    Spin(7.0);
    FinishOne();
    return 0xC0C0;
}

// ---- tiny worker pool (not hooked: plain Win32 CV) --------------------------------------------------------------------------
struct Slot {
    SRWLOCK lk = SRWLOCK_INIT;
    CONDITION_VARIABLE cv = CONDITION_VARIABLE_INIT;
    FakeJob* job = nullptr;
    void* fn = nullptr;
    bool quit = false;
};
static Slot g_w[3];
static const wchar_t* kNames[3] = {L"KIDSTaskScheduler1", L"KIDSTaskScheduler2", L"KIDSTaskScheduler3"};

static DWORD WINAPI Worker(LPVOID p) {
    const int i = (int)(intptr_t)p;
    SetThreadDescription(GetCurrentThread(), kNames[i]);
    Slot& s = g_w[i];
    for (;;) {
        AcquireSRWLockExclusive(&s.lk);
        while (!s.job && !s.quit) {
            if (i == 0) caveJtWait(&s.cv, &s.lk, INFINITE, 0);   // a non-watched thread through the cave: fast path
            else SleepConditionVariableSRW(&s.cv, &s.lk, INFINITE, 0);
        }
        if (s.quit) { ReleaseSRWLockExclusive(&s.lk); return 0; }
        FakeJob* j = s.job;
        void* fn = s.fn;
        s.job = nullptr;
        ReleaseSRWLockExclusive(&s.lk);
        const uint64_t want = j->which == 0 ? 0xA0A0 : j->which == 1 ? 0xB0B0 : 0xC0C0;
        if (JtCallJob(j, fn) != want) InterlockedIncrement(&g_badRet);
        if (jth_rbxAfter != (uint64_t)(uintptr_t)j) InterlockedIncrement(&g_badRbx);
    }
}
static void Kick(int i, FakeJob* j, void* fn) {
    AcquireSRWLockExclusive(&g_w[i].lk);
    g_w[i].job = j;
    g_w[i].fn = fn;
    ReleaseSRWLockExclusive(&g_w[i].lk);
    WakeConditionVariable(&g_w[i].cv);
}

static uint8_t* g_fakeBase = nullptr;
static FakeJob g_jobs[3];
static DWORD WINAPI Logic(LPVOID) {
    SetThreadDescription(GetCurrentThread(), L"KIDSTaskScheduler0");
    Sleep(300);   // the harness calls TestReport -> FindWatched after the names are set
    uint8_t* G = *(uint8_t**)(g_fakeBase + 0x4759910);
    for (int f = 0; f < 60; ++f) {
        ++*(volatile uint32_t*)(G + 0x610);
        g_done = 0;
        g_left = 3;
        // C on worker (f % 3), the others round-robin: the releaser moves between threads
        Kick((f + 0) % 3, &g_jobs[2], (void*)&JobNode);
        Kick((f + 1) % 3, &g_jobs[0], (void*)&JobPlain);
        Kick((f + 2) % 3, &g_jobs[1], (void*)&JobPfor);
        AcquireSRWLockExclusive(&g_lk);
        while (!g_done) caveJtWait(&g_cv, &g_lk, INFINITE, 0);   // the patched STL wait
        ReleaseSRWLockExclusive(&g_lk);
        Spin(0.5);   // the rest of the "logic frame"
    }
    return 0;
}

int main() {
    int fails = 0;
    auto check = [&](bool c, const char* m) { printf("%s %s\n", c ? "OK  " : "FAIL", m); fails += !c; };
    HMODULE self = GetModuleHandleW(nullptr);
    auto* dos = (IMAGE_DOS_HEADER*)self;
    auto* nt = (IMAGE_NT_HEADERS*)((uint8_t*)self + dos->e_lfanew);
    const uintptr_t lo = (uintptr_t)self, hi = lo + nt->OptionalHeader.SizeOfImage;
    g_fakeBase = (uint8_t*)VirtualAlloc(nullptr, 0x4800000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    uint8_t* G = (uint8_t*)VirtualAlloc(nullptr, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    *(uint8_t**)(g_fakeBase + 0x4759910) = G;
    static void* iatSleep = (void*)&SleepConditionVariableSRW;
    static void* iatWake = (void*)&WakeConditionVariable;
    static void* iatWakeAll = (void*)&WakeAllConditionVariable;
    const uint32_t rvaNode = (uint32_t)((uintptr_t)&JobNode - lo), rvaPfor = (uint32_t)((uintptr_t)&JobPfor - lo);
    jt::TestSetup(&Log, lo, hi, g_fakeBase, &iatSleep, &iatWake, &iatWakeAll, L"KIDSTaskScheduler0", rvaNode, rvaPfor);
    // fake job layouts: JobNode: [job+48h] = desc, [desc+28h] = callback object, [[obj]+8] = NodeCallbackC
    for (int i = 0; i < 3; ++i) g_jobs[i].which = i;
    *(void**)(g_jobs[2].raw + 0x48) = g_desc;
    *(void**)(g_desc + 0x28) = g_cbObj;
    // ParallelFor: +50h body, [+70h, +74h) range, +78h grain
    *(void**)(g_jobs[1].raw + 0x50) = (void*)&PforBody;
    *(int32_t*)(g_jobs[1].raw + 0x70) = 100;
    *(int32_t*)(g_jobs[1].raw + 0x74) = 104;
    *(uint32_t*)(g_jobs[1].raw + 0x78) = 4;

    HANDLE w[3];
    for (int i = 0; i < 3; ++i) w[i] = CreateThread(nullptr, 0, Worker, (LPVOID)(intptr_t)i, 0, nullptr);
    HANDLE lg = CreateThread(nullptr, 0, Logic, nullptr, 0, nullptr);
    Sleep(150);
    jt::TestReport();   // finds the watched thread (named by now); the window so far is empty
    g_lines.clear();
    WaitForSingleObject(lg, 30000);
    Sleep(100);
    jt::TestReport();
    for (int i = 0; i < 3; ++i) {
        AcquireSRWLockExclusive(&g_w[i].lk);
        g_w[i].quit = true;
        ReleaseSRWLockExclusive(&g_w[i].lk);
        WakeConditionVariable(&g_w[i].cv);
    }
    WaitForMultipleObjects(3, w, TRUE, 5000);

    char nodeName[64], pforName[64];
    snprintf(nodeName, sizeof nodeName, "%X/%X", rvaNode, (unsigned)((uintptr_t)&NodeCallbackC - lo));
    snprintf(pforName, sizeof pforName, "%X/%X", rvaPfor, (unsigned)((uintptr_t)&PforBody - lo));
    unsigned ticks = 0, waits = 0;
    double blocked = -1, critPct = -1, critP50 = -1, longPct = -1;
    long long dj = -1, dw = -1, dk = -1, unb = -1;
    unsigned long long noWaker = 99;
    bool relOk = false, critOk = false, longOk = false, pforOk = false;
    char relName[64];
    DWORD relTid = g_tidC;
    (void)relTid;
    for (const std::string& l : g_lines) {
        const char* s = l.c_str();
        if (!strncmp(s, "JT: window ", 11)) {
            if (const char* p = strstr(s, "logic ticks ")) sscanf_s(p, "logic ticks %u", &ticks);
            if (const char* p = strstr(s, "CV waits ")) sscanf_s(p, "CV waits %u", &waits);
            if (const char* p = strstr(s, "blocked ")) sscanf_s(p, "blocked %lf ms/tick", &blocked);
            if (const char* p = strstr(s, "no waker seen ")) sscanf_s(p, "no waker seen %llu", &noWaker);
            if (const char* p = strstr(s, "dropped: jobs ")) sscanf_s(p, "dropped: jobs %lld, waits %lld, wakes %lld; unbalanced %lld", &dj, &dw, &dk, &unb);
        }
        if (!strncmp(s, "JT:   1 [", 9)) relOk = strstr(s, "KIDSTaskScheduler") != nullptr;
        if (strstr(s, "critical job (") && strstr(s, "1) ")) {
            const char* p = strstr(s, "1) ");
            critOk = strncmp(p + 3, nodeName, strlen(nodeName)) == 0;
            sscanf_s(p + 3 + strlen(nodeName), " %lf %% of the waits, job p50 %lf", &critPct, &critP50);
        }
        if (strstr(s, "longest job overlapping the wait: 1) ")) {
            const char* p = strstr(s, "1) ") + 3;
            longOk = strncmp(p, nodeName, strlen(nodeName)) == 0;
            sscanf_s(p + strlen(nodeName), " %lf %%", &longPct);
        }
        if (strstr(s, pforName) && strstr(s, "leaves 1.0/tick, grain 4, leaf range 4.0")) pforOk = true;
    }
    (void)relName;
    printf("parsed: ticks %u, waits %u, blocked %.3f ms/tick, no waker %llu, critical %s %.0f %% p50 %.0f us, longest %.0f %%, dropped %lld/%lld/%lld, unbalanced %lld\n",
           ticks, waits, blocked, noWaker, critOk ? nodeName : "(other)", critPct, critP50, longPct, dj, dw, dk, unb);
    check(g_badRcx == 0, "caveJtJob: every job got rcx = job (the displaced mov rcx,rbx)");
    check(g_badRbx == 0, "caveJtJob: rbx (the job) preserved across the cave");
    check(g_badRet == 0, "caveJtJob: the job's return value comes back in rax");
    check(ticks == 60, "60 logic ticks from the engine tick counter (G+610h)");
    check(waits == 60, "60 waits of the watched thread recorded (the worker's own cave wait took the fast path)");
    check(blocked >= 6.0 && blocked <= 12.0, "blocked ~7 ms per tick (job C's 7 ms spin)");
    check(noWaker == 0, "every wait has its waker (notify_one through the cave)");
    check(relOk, "the releasing threads are named (KIDSTaskScheduler1..3)");
    check(critOk && critPct >= 90 && critP50 >= 6500 && critP50 <= 8000, "critical job = the 7 ms JobNode (fn/inner callback), p50 ~7000 us");
    check(longOk && longPct >= 90, "longest job overlapping the wait = the JobNode");
    check(pforOk, "ParallelFor leaf: 1 leaf/tick, grain 4, range 4, body RVA from job+50h");
    check(dj == 0 && dw == 0 && dk == 0 && unb == 0, "nothing dropped, job begin/end balanced");
    printf(fails ? "FAILED (%d)\n" : "ALL PASSED\n", fails);
    return fails;
}
