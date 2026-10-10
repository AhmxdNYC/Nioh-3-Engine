// thrsched.cpp - TSTATE thread-state census + RTP render-thread priority (see thrsched.h).
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <unordered_map>
#include <vector>

#include "thrsched.h"

namespace thrsched {
namespace {

void (*g_log)(const char*, ...) = nullptr;
#define TLOG(...)                      \
    do {                               \
        if (g_log) g_log(__VA_ARGS__); \
    } while (0)

uintptr_t g_base = 0, g_imgEnd = 0;
unsigned long (*g_presentTid)() = nullptr;

using NtQsiFn = LONG(NTAPI*)(ULONG, PVOID, ULONG, PULONG);
using NtQitFn = LONG(NTAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG);
using GetDescFn = HRESULT(WINAPI*)(HANDLE, PWSTR*);
NtQsiFn g_ntQsi = nullptr;
NtQitFn g_ntQit = nullptr;
GetDescFn g_getDesc = nullptr;

// ---- thread names and roles -------------------------------------------------------------------------------------------
enum Role : int {
    kGame, kLogic, kDispatch, kEcl, kPresent, kMission, kDrawThr,  // single critical threads (rows always shown)
    kWorkers, kKidsts, kEffect, kAppOther, kOther,                 // groups (summed)
    kNumRoles
};
constexpr int kNumCritical = kWorkers;
const char* const kRoleName[kNumRoles] = {"main [App]Game", "logic KIDSTaskScheduler0", "dispatch SynergisticDispatcher",
                                          "ECL thread", "present thread", "[App]MissionThread", "[App]GameModeDrawThread",
                                          "KIDSTaskScheduler1+", "KIDSTS*", "[App]Effect*", "other [App]*", "unnamed/other"};

int RoleOfName(const wchar_t* d) {
    if (!d || !*d) return kOther;
    if (!wcscmp(d, L"[App]Game")) return kGame;
    if (!wcscmp(d, L"KIDSTaskScheduler0")) return kLogic;
    if (!wcsncmp(d, L"Synergistic", 11)) return kDispatch;
    if (wcsstr(d, L"ExecuteCommandList") || wcsstr(d, L"CommandListSubmit")) return kEcl;
    if (!wcscmp(d, L"[App]MissionThread")) return kMission;
    if (!wcscmp(d, L"[App]GameModeDrawThread")) return kDrawThr;
    if (!wcsncmp(d, L"KIDSTaskSchedul", 15)) return kWorkers;
    if (!wcsncmp(d, L"KIDSTS", 6)) return kKidsts;
    if (!wcsncmp(d, L"[App]Effect", 11)) return kEffect;
    if (!wcsncmp(d, L"[App]", 5)) return kAppOther;
    return kOther;
}

// name of a thread (UTF-8, "" when it has none); start = its Win32 start address
bool QueryThread(DWORD tid, char* name, size_t n, uintptr_t* start, int* role) {
    name[0] = 0;
    *start = 0;
    *role = kOther;
    HANDLE h = OpenThread(THREAD_QUERY_INFORMATION, FALSE, tid);
    if (!h) h = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, tid);
    if (!h) return false;
    PWSTR d = nullptr;
    if (g_getDesc && SUCCEEDED(g_getDesc(h, &d)) && d) {
        *role = RoleOfName(d);
        WideCharToMultiByte(CP_UTF8, 0, d, -1, name, (int)n - 1, nullptr, nullptr);
        name[n - 1] = 0;
        LocalFree(d);
    }
    if (g_ntQit) {
        PVOID sa = nullptr;
        if (g_ntQit(h, 9 /*ThreadQuerySetWin32StartAddress*/, &sa, sizeof(sa), nullptr) >= 0) *start = reinterpret_cast<uintptr_t>(sa);
    }
    CloseHandle(h);
    return true;
}

void FormatStart(uintptr_t a, char* buf, size_t n) {
    if (!a) snprintf(buf, n, "-");
    else if (a >= g_base && a < g_imgEnd) snprintf(buf, n, "+%llX", (unsigned long long)(a - g_base));
    else {
        HMODULE m = nullptr;
        char path[MAX_PATH] = {};
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCSTR>(a), &m) &&
            GetModuleFileNameA(m, path, MAX_PATH)) {
            const char* leaf = strrchr(path, '\\');
            snprintf(buf, n, "%s+%llX", leaf ? leaf + 1 : path, (unsigned long long)(a - reinterpret_cast<uintptr_t>(m)));
        } else {
            snprintf(buf, n, "%llX", (unsigned long long)a);
        }
    }
}

// ======================================================================================================================
// TSTATE census
// ======================================================================================================================
struct Cfg {
    int us = 100000, sec = 30, top = 14, delay = 20;
} g_cfg;

// SYSTEM_PROCESS_INFORMATION / SYSTEM_THREAD_INFORMATION (x64 layouts, stable since Windows Vista)
constexpr size_t kPiNext = 0x00, kPiThreads = 0x04, kPiUser = 0x28, kPiKernel = 0x30, kPiPid = 0x50, kPiSize = 0x100;
constexpr size_t kTiKernel = 0x00, kTiUser = 0x08, kTiTid = 0x30, kTiPrio = 0x38, kTiBase = 0x3C, kTiCsw = 0x40,
                 kTiState = 0x44, kTiReason = 0x48, kTiSize = 0x50;

enum Bucket : int { kRun, kReadyPre, kReadyWake, kSleep, kWaitObj, kSusp, kWaitOther, kNumBuckets };

int BucketOf(uint32_t state, uint32_t reason) {
    switch (state) {
        case 2:  // Running
        case 3:  // Standby (picked to run next on a processor)
            return kRun;
        case 1:  // Ready
        case 7:  // DeferredReady
            return (reason == 30 || reason == 31 || reason == 32 || reason == 38) ? kReadyPre : kReadyWake;
        case 5:  // Waiting
            if (reason == 4 || reason == 11) return kSleep;     // (Wr)DelayExecution: Sleep / timer sleep
            if (reason == 5 || reason == 12) return kSusp;      // (Wr)Suspended (SP's own suspends land here)
            if (reason == 6 || reason == 13 || reason == 37 || reason == 15 || reason == 14 || reason == 21 || reason == 0 || reason == 7)
                return kWaitObj;                                 // WaitFor* / condvar & WaitOnAddress (alert) / IOCP / keyed event
            return kWaitOther;
        default:
            return kWaitOther;  // initialized / transition / terminated
    }
}

struct Thr {
    char name[64] = {};
    int role = kOther;
    uintptr_t start = 0;
    bool queried = false;
    int nameTries = 0;
    uint64_t cpu0 = 0, cpu1 = 0;  // kernel + user (100 ns) at window start / latest
    uint32_t csw0 = 0, csw1 = 0;
    uint32_t n[kNumBuckets] = {};
    uint32_t seen = 0;
    int prio = 0, basePrio = 0;
    bool inWindow = false;
};
std::unordered_map<DWORD, Thr> g_thr;
std::vector<uint8_t> g_buf;
uint32_t g_snaps = 0;
std::vector<uint32_t> g_costUs;
uint64_t g_proc0 = 0, g_proc1 = 0;
bool g_procSeen = false;
DWORD g_selfTid = 0;

bool Snapshot() {
    ULONG need = 0;
    for (int tries = 0; tries < 4; ++tries) {
        const LONG st = g_ntQsi(5 /*SystemProcessInformation*/, g_buf.data(), (ULONG)g_buf.size(), &need);
        if (st >= 0) return true;
        if (st != (LONG)0xC0000004 /*STATUS_INFO_LENGTH_MISMATCH*/) return false;
        g_buf.resize(std::max<size_t>(need + 256 * 1024, g_buf.size() * 2));
    }
    return false;
}

void Ingest() {
    const DWORD pid = GetCurrentProcessId();
    const uint8_t* p = g_buf.data();
    for (;;) {
        uintptr_t epid;
        memcpy(&epid, p + kPiPid, 8);
        if ((DWORD)epid == pid) {
            uint32_t nt;
            memcpy(&nt, p + kPiThreads, 4);
            int64_t pu, pk;
            memcpy(&pu, p + kPiUser, 8);
            memcpy(&pk, p + kPiKernel, 8);
            g_proc1 = (uint64_t)(pu + pk);
            if (!g_procSeen) {
                g_proc0 = g_proc1;
                g_procSeen = true;
            }
            const uint8_t* t = p + kPiSize;
            for (uint32_t i = 0; i < nt; ++i, t += kTiSize) {
                uintptr_t tid64;
                memcpy(&tid64, t + kTiTid, 8);
                const DWORD tid = (DWORD)tid64;
                if (tid == g_selfTid) continue;
                int64_t k, u;
                uint32_t csw, st, rs;
                int32_t pr, bp;
                memcpy(&k, t + kTiKernel, 8);
                memcpy(&u, t + kTiUser, 8);
                memcpy(&csw, t + kTiCsw, 4);
                memcpy(&st, t + kTiState, 4);
                memcpy(&rs, t + kTiReason, 4);
                memcpy(&pr, t + kTiPrio, 4);
                memcpy(&bp, t + kTiBase, 4);
                Thr& r = g_thr[tid];
                const uint64_t cpu = (uint64_t)(k + u);
                if (!r.inWindow) {
                    r.inWindow = true;
                    r.cpu0 = cpu;
                    r.csw0 = csw;
                }
                r.cpu1 = cpu;
                r.csw1 = csw;
                ++r.n[BucketOf(st, rs)];
                ++r.seen;
                r.prio = pr;
                r.basePrio = bp;
            }
            return;
        }
        uint32_t next;
        memcpy(&next, p + kPiNext, 4);
        if (!next) return;
        p += next;
    }
}

void RefreshNames() {
    for (auto& kv : g_thr) {
        Thr& r = kv.second;
        if (!r.inWindow) continue;
        // the engine names some threads after they start: retry an unnamed one a few times
        if (r.queried && (r.name[0] || r.nameTries >= 5)) continue;
        ++r.nameTries;
        r.queried = QueryThread(kv.first, r.name, sizeof(r.name), &r.start, &r.role) || r.queried;
    }
}

struct Row {
    double cpu = 0;         // cores
    double sh[kNumBuckets] = {};  // shares 0..1 (thread-equivalents when summed)
    double csw = 0;
    int threads = 0;
};

void AddRow(Row& g, const Thr& r, double secs) {
    g.cpu += (double)(r.cpu1 - r.cpu0) / 1e7 / secs;
    g.csw += (double)(r.csw1 - r.csw0) / secs;
    for (int b = 0; b < kNumBuckets; ++b) g.sh[b] += (double)r.n[b] / (double)std::max(1u, g_snaps);  // share of the window's snapshots
    ++g.threads;
}

void LogThread(const char* label, DWORD tid, const Thr& r, double secs) {
    Row x;
    AddRow(x, r, secs);
    char st[96];
    FormatStart(r.start, st, sizeof st);
    TLOG("TSTATE  %-30s %6lu | cpu %5.1f %% | run %5.1f | READY %5.1f (preempt %4.1f) | sleep %5.1f | wait %5.1f | susp %4.1f | other %4.1f | "
         "csw %6.0f/s | prio %d/%d | start %s",
         label, (unsigned long)tid, 100 * x.cpu, 100 * x.sh[kRun], 100 * (x.sh[kReadyPre] + x.sh[kReadyWake]), 100 * x.sh[kReadyPre],
         100 * x.sh[kSleep], 100 * x.sh[kWaitObj], 100 * x.sh[kSusp], 100 * x.sh[kWaitOther], x.csw, r.prio, r.basePrio, st);
}

void LogCpuSets(DWORD tid, const char* label) {
    HANDLE h = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, tid);
    if (!h) return;
    ULONG ids[64] = {}, cnt = 0;
    const BOOL ok = GetThreadSelectedCpuSets(h, ids, 64, &cnt);
    PROCESSOR_NUMBER ideal{};
    const BOOL okI = GetThreadIdealProcessorEx(h, &ideal);
    char sets[200] = "none (any CPU)";
    if (ok && cnt) {
        int o = snprintf(sets, sizeof sets, "%lu:", cnt);
        for (ULONG i = 0; i < cnt && i < 16 && o < (int)sizeof sets - 8; ++i) o += snprintf(sets + o, sizeof sets - o, " %lu", ids[i]);
    }
    TLOG("TSTATE cpusets %-30s %6lu | selected CPU sets %s | ideal processor %d", label, (unsigned long)tid, sets, okI ? (int)ideal.Number : -1);
    CloseHandle(h);
}

void Report(double secs, const std::vector<uint32_t>& cost) {
    FILETIME fi, fk, fu;
    static uint64_t sI = 0, sK = 0, sU = 0;
    static bool sSys = false;
    double sysBusy = -1;
    if (GetSystemTimes(&fi, &fk, &fu)) {
        auto Q = [](const FILETIME& f) { return ((uint64_t)f.dwHighDateTime << 32) | f.dwLowDateTime; };
        const uint64_t i = Q(fi), k = Q(fk), u = Q(fu);
        if (sSys && (k + u) > (sK + sU)) sysBusy = 1.0 - (double)(i - sI) / (double)((k - sK) + (u - sU));  // kernel time includes idle
        sI = i; sK = k; sU = u; sSys = true;
    }
    const DWORD ncpu = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    std::vector<uint32_t> c = cost;
    std::sort(c.begin(), c.end());
    const uint32_t p50 = c.empty() ? 0 : c[c.size() / 2], mx = c.empty() ? 0 : c.back();
    // per role
    Row roles[kNumRoles];
    const DWORD ptid = g_presentTid ? (DWORD)g_presentTid() : 0;
    double readySum = 0;
    for (auto& kv : g_thr) {
        const Thr& r = kv.second;
        if (!r.inWindow) continue;
        int role = r.role;
        if (ptid && kv.first == ptid && role == kOther) role = kPresent;  // the unnamed Present thread
        AddRow(roles[role], r, secs);
        if (r.seen) readySum += (double)(r.n[kReadyPre] + r.n[kReadyWake]) / (double)std::max(1u, g_snaps);
    }
    TLOG("TSTATE: window %.0f s, %u snapshots (every %d us, cost p50 %u us max %u us, %zu KB) | CPU: system %.1f %% of %lu logical (%.2f busy), "
         "game process %.2f cores | game threads runnable without a CPU (READY): %.2f thread-equivalents | present tid %lu",
         secs, g_snaps, g_cfg.us, p50, mx, g_buf.size() / 1024, sysBusy < 0 ? -1.0 : 100 * sysBusy, (unsigned long)ncpu, sysBusy < 0 ? -1.0 : sysBusy * ncpu,
         (double)(g_proc1 - g_proc0) / 1e7 / secs, readySum, (unsigned long)ptid);
    TLOG("TSTATE  role / thread                     tid | cpu %% of one core | running %% | READY %% (preempted / quantum end) | Sleep %% | "
         "waiting on objects / condvars %% | suspended (SP) %% | other %% | context switches | priority cur/base | Win32 start");
    // critical threads, one row each (a role may have several threads, e.g. two named the same: all are shown)
    static bool s_setsLogged = false;
    for (int role = 0; role < kNumCritical; ++role) {
        bool any = false;
        for (auto& kv : g_thr) {
            const Thr& r = kv.second;
            if (!r.inWindow) continue;
            int rr = r.role;
            if (ptid && kv.first == ptid && rr == kOther) rr = kPresent;
            if (rr != role) continue;
            LogThread(kRoleName[role], kv.first, r, secs);
            if (!s_setsLogged) LogCpuSets(kv.first, kRoleName[role]);
            any = true;
        }
        if (!any && role == kPresent && ptid) TLOG("TSTATE  %-30s %6lu | (the present thread is also a named thread above)", kRoleName[role], (unsigned long)ptid);
    }
    // busiest other threads
    std::vector<std::pair<double, DWORD>> top;
    for (auto& kv : g_thr) {
        const Thr& r = kv.second;
        if (!r.inWindow) continue;
        int rr = r.role;
        if (ptid && kv.first == ptid && rr == kOther) rr = kPresent;
        if (rr < kNumCritical) continue;
        top.push_back({(double)(r.cpu1 - r.cpu0), kv.first});
    }
    std::sort(top.begin(), top.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    for (int i = 0; i < (int)top.size() && i < g_cfg.top; ++i) {
        const Thr& r = g_thr[top[i].second];
        char lab[64];
        snprintf(lab, sizeof lab, "%s", r.name[0] ? r.name : "(unnamed)");
        LogThread(lab, top[i].second, r, secs);
    }
    // groups
    for (int role = kNumCritical; role < kNumRoles; ++role) {
        const Row& g = roles[role];
        if (!g.threads) continue;
        TLOG("TSTATE group %-24s %3d threads | cpu %.2f cores | running %.2f | READY %.2f (preempted %.2f) | Sleep %.2f | waiting %.2f | "
             "suspended %.2f | other %.2f thread-equivalents | csw %.0f/s",
             kRoleName[role], g.threads, g.cpu, g.sh[kRun], g.sh[kReadyPre] + g.sh[kReadyWake], g.sh[kReadyPre], g.sh[kSleep],
             g.sh[kWaitObj], g.sh[kSusp], g.sh[kWaitOther], g.csw);
    }
    if (!s_setsLogged) {
        HANDLE hp = GetCurrentProcess();
        ULONG cnt = 0;
        const BOOL ok = GetProcessDefaultCpuSets(hp, nullptr, 0, &cnt);
        DWORD_PTR pm = 0, sm = 0;
        GetProcessAffinityMask(hp, &pm, &sm);
        int restricted = 0, total = 0;
        for (auto& kv : g_thr) {
            if (!kv.second.inWindow || kv.second.role != kWorkers) continue;
            ++total;
            HANDLE h = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, kv.first);
            if (!h) continue;
            ULONG ids[64], n = 0;
            if (GetThreadSelectedCpuSets(h, ids, 64, &n) && n) ++restricted;
            CloseHandle(h);
        }
        TLOG("TSTATE cpusets: process default CPU sets %lu (%s), process affinity %llX of %llX | KIDSTaskScheduler1+ with selected CPU sets: %d of %d",
             cnt, ok ? "none = all CPUs" : "set", (unsigned long long)pm, (unsigned long long)sm, restricted, total);
        s_setsLogged = true;
    }
}

DWORD WINAPI CensusThread(LPVOID) {
    Sleep((DWORD)g_cfg.delay * 1000);
    g_selfTid = GetCurrentThreadId();
    // not pinned: pc2 CPU1 (1.7.58 first build) pinned it to the last CPU, which was [App]Game's ideal processor, and the
    // ~16 ms snapshots there distorted the main thread's profile. HIGHEST so it samples on time when every core is busy.
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    g_buf.resize(2 * 1024 * 1024);
    HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, 0x00000002 /*HIGH_RESOLUTION*/, TIMER_ALL_ACCESS);
    TLOG("TSTATE: census every %d us (%s timer), report every %d s, top %d other threads, sampler at HIGHEST priority (not pinned)",
         g_cfg.us, timer ? "high-resolution" : "Sleep", g_cfg.sec, g_cfg.top);
    LARGE_INTEGER f, t0, now, a, b;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t0);
    LONGLONG lastNames = 0;
    for (;;) {
        QueryPerformanceCounter(&a);
        if (Snapshot()) {
            Ingest();
            ++g_snaps;
            QueryPerformanceCounter(&b);
            if (g_costUs.size() < 100000) g_costUs.push_back((uint32_t)((b.QuadPart - a.QuadPart) * 1000000 / f.QuadPart));
        }
        QueryPerformanceCounter(&now);
        if (now.QuadPart - lastNames > f.QuadPart * 2) {
            RefreshNames();
            lastNames = now.QuadPart;
        }
        const double el = (double)(now.QuadPart - t0.QuadPart) / (double)f.QuadPart;
        if (el >= g_cfg.sec) {
            RefreshNames();
            Report(el, g_costUs);
            // next window: drop threads that were not seen, keep names
            for (auto it = g_thr.begin(); it != g_thr.end();) {
                Thr& r = it->second;
                if (!r.seen) {
                    it = g_thr.erase(it);
                    continue;
                }
                r.inWindow = false;
                r.seen = 0;
                memset(r.n, 0, sizeof r.n);
                ++it;
            }
            g_snaps = 0;
            g_costUs.clear();
            g_procSeen = false;
            QueryPerformanceCounter(&t0);
        }
        if (timer) {
            LARGE_INTEGER due;
            due.QuadPart = -(LONGLONG)g_cfg.us * 10;
            if (SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE)) WaitForSingleObject(timer, 1000);
            else Sleep(1);
        } else {
            Sleep((DWORD)std::max(1, g_cfg.us / 1000));
        }
    }
}

// ======================================================================================================================
// RTP render-thread priority
// ======================================================================================================================
volatile uint32_t g_rtpSwitch = 0;
bool g_rtpInstalled = false;
int g_rtpLevel = THREAD_PRIORITY_ABOVE_NORMAL;
struct Boost {
    DWORD tid = 0;
    HANDLE h = nullptr;
    int role = kOther;
    int orig = THREAD_PRIORITY_NORMAL;
    bool boosted = false;
    bool gone = false;
};
std::vector<Boost> g_boost;
SRWLOCK g_boostLock = SRWLOCK_INIT;
volatile LONG g_rtpResets = 0, g_rtpSets = 0, g_rtpRestores = 0;
std::unordered_map<DWORD, int> g_rtpRole;  // tid -> role (cache; unnamed threads re-queried a few times)
std::unordered_map<DWORD, int> g_rtpTries;

void RtpDiscover() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    const DWORD pid = GetCurrentProcessId();
    const DWORD ptid = g_presentTid ? (DWORD)g_presentTid() : 0;
    std::vector<DWORD> alive;
    THREADENTRY32 te{};
    te.dwSize = sizeof(te);
    for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
        if (te.th32OwnerProcessID != pid) continue;
        const DWORD tid = te.th32ThreadID;
        alive.push_back(tid);
        int role;
        auto it = g_rtpRole.find(tid);
        if (it != g_rtpRole.end() && (it->second != kOther || g_rtpTries[tid] >= 5)) role = it->second;
        else {
            char nm[64];
            uintptr_t st;
            QueryThread(tid, nm, sizeof nm, &st, &role);
            g_rtpRole[tid] = role;
            ++g_rtpTries[tid];
        }
        if (ptid && tid == ptid && role == kOther) role = kPresent;
        if (role != kGame && role != kDispatch && role != kEcl && role != kPresent) continue;
        bool have = false;
        for (const Boost& b : g_boost)
            if (b.tid == tid && !b.gone) have = true;
        if (have) continue;
        HANDLE h = OpenThread(THREAD_SET_LIMITED_INFORMATION | THREAD_QUERY_LIMITED_INFORMATION, FALSE, tid);
        if (!h) continue;
        Boost b;
        b.tid = tid;
        b.h = h;
        b.role = role;
        b.orig = GetThreadPriority(h);
        g_boost.push_back(b);
        TLOG("RTP: render thread %s tid %lu found (priority %d)", kRoleName[role], (unsigned long)tid, b.orig);
    }
    CloseHandle(snap);
    for (Boost& b : g_boost)
        if (!b.gone && std::find(alive.begin(), alive.end(), b.tid) == alive.end()) {
            b.gone = true;
            CloseHandle(b.h);
            b.h = nullptr;
        }
}

void RtpApply(bool on) {
    for (Boost& b : g_boost) {
        if (b.gone || !b.h) continue;
        const int cur = GetThreadPriority(b.h);
        if (cur == THREAD_PRIORITY_ERROR_RETURN) continue;
        if (on) {
            if (!b.boosted) b.orig = cur;
            const int want = std::max(b.orig, g_rtpLevel);  // never lower a thread the engine already raised
            if (cur >= want) continue;                       // already there (or raised further by someone else)
            if (b.boosted) InterlockedIncrement(&g_rtpResets);  // it was ours and got lowered again (the engine)
            if (SetThreadPriority(b.h, want)) {
                b.boosted = true;
                InterlockedIncrement(&g_rtpSets);
            }
        } else if (b.boosted) {
            if (SetThreadPriority(b.h, b.orig)) InterlockedIncrement(&g_rtpRestores);
            b.boosted = false;
        }
    }
}

DWORD WINAPI RtpThread(LPVOID) {
    LARGE_INTEGER f, now;
    QueryPerformanceFrequency(&f);
    LONGLONG lastFind = 0, lastApply = 0;
    uint32_t last = 2;  // force a first apply
    for (;;) {
        QueryPerformanceCounter(&now);
        const uint32_t sw = g_rtpSwitch ? 1u : 0u;
        const bool find = now.QuadPart - lastFind > f.QuadPart * 2;
        if (find || sw != last || now.QuadPart - lastApply > f.QuadPart) {
            AcquireSRWLockExclusive(&g_boostLock);
            if (find) {
                RtpDiscover();
                lastFind = now.QuadPart;
            }
            RtpApply(sw != 0);
            ReleaseSRWLockExclusive(&g_boostLock);
            if (sw != last && last != 2) TLOG("RTP: switch %s", sw ? "ON" : "off");
            last = sw;
            lastApply = now.QuadPart;
        }
        Sleep(50);
    }
}

}  // namespace

void Start(const StartInfo& info) {
    g_log = info.log;
    g_presentTid = info.presentTid;
    if (!info.iniPath || !info.base) return;
    g_base = info.base;
    {
        auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(info.base);
        auto nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(info.base + dos->e_lfanew);
        g_imgEnd = info.base + nt->OptionalHeader.SizeOfImage;
    }
    auto I = [&](const wchar_t* sec, const wchar_t* key, int def) { return (int)GetPrivateProfileIntW(sec, key, def, info.iniPath); };
    HMODULE nt = GetModuleHandleW(L"ntdll.dll");
    HMODULE kb = GetModuleHandleW(L"kernelbase.dll"), k32 = GetModuleHandleW(L"kernel32.dll");
    g_ntQsi = nt ? reinterpret_cast<NtQsiFn>(GetProcAddress(nt, "NtQuerySystemInformation")) : nullptr;
    g_ntQit = nt ? reinterpret_cast<NtQitFn>(GetProcAddress(nt, "NtQueryInformationThread")) : nullptr;
    g_getDesc = reinterpret_cast<GetDescFn>(GetProcAddress(kb ? kb : k32, "GetThreadDescription"));
    if (!g_getDesc && k32) g_getDesc = reinterpret_cast<GetDescFn>(GetProcAddress(k32, "GetThreadDescription"));

    const int rtp = I(L"Patches", L"RTP", 0);
    if (rtp == 1 || rtp == 2) {
        const int lvl = std::clamp(I(L"Tuning", L"RTP_Level", 1), 1, 2);
        g_rtpLevel = lvl == 2 ? THREAD_PRIORITY_HIGHEST : THREAD_PRIORITY_ABOVE_NORMAL;
        if (!g_getDesc) TLOG("RTP: GetThreadDescription not available - off");
        else if (HANDLE h = CreateThread(nullptr, 0, RtpThread, nullptr, 0, nullptr)) {
            CloseHandle(h);
            g_rtpSwitch = rtp == 1 ? 1u : 0u;
            g_rtpInstalled = true;
            TLOG("RTP: installed, switch %s, level %s ([App]Game, SynergisticDispatcherTask, the Present thread, a named ECL thread)",
                 rtp == 1 ? "ON" : "off", lvl == 2 ? "HIGHEST" : "ABOVE_NORMAL");
        }
    }

    if (I(L"Profiler", L"ThreadState", 0) == 1) {
        if (!g_ntQsi) {
            TLOG("TSTATE: NtQuerySystemInformation not available - off");
            return;
        }
        g_cfg.us = std::clamp(I(L"Profiler", L"ThreadStateUs", g_cfg.us), 1000, 100000);
        g_cfg.sec = std::clamp(I(L"Profiler", L"ThreadStateSec", I(L"Profiler", L"ReportSec", 30)), 5, 600);
        g_cfg.top = std::clamp(I(L"Profiler", L"ThreadStateTop", g_cfg.top), 0, 60);
        g_cfg.delay = std::clamp(I(L"Profiler", L"ThreadStateDelaySec", I(L"Profiler", L"StartDelaySec", 20)), 0, 600);
        if (HANDLE h = CreateThread(nullptr, 0, CensusThread, nullptr, 0, nullptr)) {
            CloseHandle(h);
            TLOG("TSTATE: thread-state census armed (starts after %d s)", g_cfg.delay);
        }
    }
}

volatile uint32_t* RtpSwitch() { return g_rtpInstalled ? &g_rtpSwitch : nullptr; }

void RtpStatusLine(char* buf, unsigned n) {
    buf[0] = 0;
    if (!g_rtpInstalled || !n) return;
    int o = snprintf(buf, n, "RTP: switch %s, level %s | sets %ld, restores %ld, re-applied after an outside change %ld | threads:",
                     g_rtpSwitch ? "ON" : "off", g_rtpLevel == THREAD_PRIORITY_HIGHEST ? "HIGHEST" : "ABOVE_NORMAL", g_rtpSets, g_rtpRestores,
                     g_rtpResets);
    AcquireSRWLockShared(&g_boostLock);
    for (const Boost& b : g_boost) {
        if (b.gone || o >= (int)n - 64) continue;
        o += snprintf(buf + o, n - o, " %s %lu (prio %d, was %d)", kRoleName[b.role], (unsigned long)b.tid, b.h ? GetThreadPriority(b.h) : 0, b.orig);
    }
    ReleaseSRWLockShared(&g_boostLock);
    if (o > 0 && o < (int)n && buf[o - 1] == ':') snprintf(buf + o, n - o, " none found yet");
}

}  // namespace thrsched
