// thrsched.cpp - TSTATE thread-state census + RTP render-thread priority (see thrsched.h).
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#include <intrin.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <unordered_map>
#include <vector>

#include "thrsched.h"
#include "gamerva.h"

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

// ======================================================================================================================
// FLP file-loader priority (#102935): on a few-CPU process (affinity <= FileLoadPrio_MaxCpus) the two KTGLGraph.Cache
// file loader threads (FileLoad + FileLoad.Poll, a yield/poll loop) take ~1.2 of 4 cores in walk hitches while the
// disk reads < 18 MB/s and [App]Game waits for a core (notes/walk-hitch-102842.md). BELOW_NORMAL lets the main and
// render threads run first; the loaders still get every idle cycle. Re-checked every second (affinity can change).
// #103349: lowered during a level load, 6 normal workers on 4 CPUs starved the loaders and Continue never finished.
// So the loaders are lowered ONLY in the field: while the engine level-load window G+58Ah is set (the same byte
// loadstats / loadonly / prefetch read) or unreadable, they keep their original priority; a load seen while they are
// lowered restores them on the next 250 ms tick. Every lower / restore is logged with its reason.
// #85: G+58Ah stays 0 through boot and the title screen (no level load yet), so FLP lowered the loaders 1 s after
// start while the game was still streaming its boot data (crash #85 hit 14 s in, 4 CPUs). Nothing to fix there (no walk
// hitch before a level is loaded): the loaders are lowered only after the first level load has been seen (G+58Ah = 1).
// ======================================================================================================================
constexpr uint32_t kFlpGameMainPtr = 0x4759910, kFlpGm58A = 0x58A;
int g_flpMaxCpus = 6;
volatile LONG g_flpSets = 0, g_flpRestores = 0, g_flpThreads = 0;
volatile LONG g_flpActive = 0;
struct Lowered {
    DWORD tid = 0;
    HANDLE h = nullptr;
    int orig = THREAD_PRIORITY_NORMAL;
    bool lowered = false;
    bool poll = false;  // KTGLGraph.Cache.FileLoad.Poll (else FileLoad)
};

bool FlpSafeRead(uintptr_t a, void* out, size_t n) {
    if (a < 0x10000) return false;
    __try {
        memcpy(out, (const void*)a, n);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
// 1 = level load in progress, 0 = not loading, -1 = unreadable (treated as loading: never lower blind).
int FlpLoading() {
    const uint32_t r = Grva(kFlpGameMainPtr);
    if (!g_base || !r) return -1;
    uintptr_t gm = 0;
    uint8_t f = 1;
    if (!FlpSafeRead(g_base + r, &gm, sizeof(gm)) || gm < 0x10000 || !FlpSafeRead(gm + kFlpGm58A, &f, 1)) return -1;
    return f ? 1 : 0;
}

int AffinityCpus() {
    DWORD_PTR pm = 0, sm = 0;
    if (!GetProcessAffinityMask(GetCurrentProcess(), &pm, &sm)) return 64;
    int n = 0;
    for (DWORD_PTR m = pm; m; m &= m - 1) ++n;
    return n;
}

DWORD WINAPI FlpThread(LPVOID) {
    std::vector<Lowered> lw;
    std::unordered_map<DWORD, int> tries;  // tid -> name queries (threads get their name shortly after creation)
    int lastCpus = -1, lastLoad = -2;
    bool seenLoad = false;  // #85: boot / title (no level load yet) is not the field
    for (unsigned tick = 0;; ++tick) {
        const int cpus = AffinityCpus();
        const bool few = cpus <= g_flpMaxCpus;
        if (cpus != lastCpus) TLOG("FLP: process affinity %d CPUs -> file loader priority %s", cpus, few ? "BELOW_NORMAL in the field" : "unchanged");
        lastCpus = cpus;
        const int load = FlpLoading();
        if (load == 1) seenLoad = true;
        if (load != lastLoad)
            TLOG("FLP: level load %s", load == 1 ? "in progress" : load < 0 ? "flag unreadable" : seenLoad ? "done (field)" : "none yet (boot / title, loaders unchanged)");
        lastLoad = load;
        const bool on = few && load == 0 && seenLoad;
        const char* why = !few ? "affinity above cap" : load == 1 ? "level load in progress" : load < 0 ? "load flag unreadable" : !seenLoad ? "boot / title" : "field";
        // Thread discovery once a second; the priority decision below runs every 250 ms tick.
        if (tick % 4 != 0) {
            for (Lowered& l : lw)
                if (!on && l.lowered) {
                    if (SetThreadPriority(l.h, l.orig)) InterlockedIncrement(&g_flpRestores);
                    TLOG("FLP: restore %s tid %lu to %d (%s)", l.poll ? "FileLoad.Poll" : "FileLoad", (unsigned long)l.tid, l.orig, why);
                    l.lowered = false;
                }
            g_flpActive = on ? 1 : 0;
            Sleep(250);
            continue;
        }
        // Both loaders found: no more full thread snapshots; only check that they still live (GetExitCodeThread).
        bool haveLoad = false, havePoll = false;
        for (size_t i = 0; i < lw.size();) {
            DWORD ec = 0;
            if (GetExitCodeThread(lw[i].h, &ec) && ec != STILL_ACTIVE) {
                CloseHandle(lw[i].h);
                lw.erase(lw.begin() + i);
                continue;
            }
            (lw[i].poll ? havePoll : haveLoad) = true;
            ++i;
        }
        HANDLE snap = (haveLoad && havePoll) ? INVALID_HANDLE_VALUE : CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snap != INVALID_HANDLE_VALUE) {
            const DWORD pid = GetCurrentProcessId();
            std::vector<DWORD> alive;
            THREADENTRY32 te{};
            te.dwSize = sizeof(te);
            for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
                if (te.th32OwnerProcessID != pid) continue;
                const DWORD tid = te.th32ThreadID;
                alive.push_back(tid);
                bool have = false;
                for (const Lowered& l : lw) have |= l.tid == tid;
                if (have) continue;
                int& t = tries[tid];
                if (t >= 5) continue;
                ++t;
                HANDLE q = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, tid);
                if (!q) continue;
                PWSTR d = nullptr;
                bool match = false, poll = false;
                if (SUCCEEDED(g_getDesc(q, &d)) && d) {
                    if (*d) t = 5;  // named: decided
                    poll = !wcscmp(d, L"KTGLGraph.Cache.FileLoad.Poll");
                    match = poll || !wcscmp(d, L"KTGLGraph.Cache.FileLoad");
                    if (match) TLOG("FLP: file loader thread %ls tid %lu found (priority %d)", d, (unsigned long)tid, GetThreadPriority(q));
                    LocalFree(d);
                }
                CloseHandle(q);
                if (!match) continue;
                Lowered l;
                l.tid = tid;
                l.h = OpenThread(THREAD_SET_LIMITED_INFORMATION | THREAD_QUERY_LIMITED_INFORMATION, FALSE, tid);
                if (!l.h) continue;
                l.orig = GetThreadPriority(l.h);
                l.poll = poll;
                lw.push_back(l);
            }
            CloseHandle(snap);
            for (size_t i = 0; i < lw.size();) {
                if (std::find(alive.begin(), alive.end(), lw[i].tid) == alive.end()) {
                    CloseHandle(lw[i].h);
                    lw.erase(lw.begin() + i);
                } else ++i;
            }
            for (auto it = tries.begin(); it != tries.end();)
                it = std::find(alive.begin(), alive.end(), it->first) == alive.end() ? tries.erase(it) : std::next(it);
        }
        for (Lowered& l : lw) {
            const int cur = GetThreadPriority(l.h);
            if (cur == THREAD_PRIORITY_ERROR_RETURN) continue;
            if (on) {
                if (!l.lowered) l.orig = cur;
                const int want = std::min(l.orig, (int)THREAD_PRIORITY_BELOW_NORMAL);  // never raise a thread
                if (cur != want && SetThreadPriority(l.h, want)) {
                    InterlockedIncrement(&g_flpSets);
                    TLOG("FLP: lower %s tid %lu %d -> %d (%s)", l.poll ? "FileLoad.Poll" : "FileLoad", (unsigned long)l.tid, cur, want, why);
                }
                l.lowered = true;
            } else if (l.lowered) {
                if (SetThreadPriority(l.h, l.orig)) InterlockedIncrement(&g_flpRestores);
                TLOG("FLP: restore %s tid %lu to %d (%s)", l.poll ? "FileLoad.Poll" : "FileLoad", (unsigned long)l.tid, l.orig, why);
                l.lowered = false;
            }
        }
        g_flpThreads = (LONG)lw.size();
        g_flpActive = on ? 1 : 0;
        Sleep(250);
    }
}

// ======================================================================================================================
// FPW file-loader poll wait (#103680, [Patches] FileLoadPoll = 0 | 1, default 0). notes/fileload-poll-wait-103680.md.
// KTGLGraph.Cache.FileLoad.Poll (body 116D30) and FileLoad (body 116F80) loop with NO yield at all until the request
// ring drains: Poll re-runs while produced [ctx+6BCh] > read-done [6C4h] or read-done > retired [6C8h] (the retire
// step polls the GPU fence), FileLoad while produced > read-done (it waits on Poll). Each pass ends with the shared
// unlock of the SRW lock ctx+CD0h (call [IAT] at 116F12 / 11702C, rcx = ctx+CD0h). FPW retargets those two calls to a
// thunk that unlocks exactly as before and then, ONLY when the loop is about to run again AND no ring counter moved
// since the previous pass, backs off: pause for FileLoadPoll_SpinUs (20), then SwitchToThread until
// FileLoadPoll_YieldUs (200), then a high-resolution timer wait of FileLoadPoll_SleepUs (250) per pass. Any counter
// change = progress = the next pass runs at once. The native loop still makes every decision; the thunk never
// changes a counter, the lock or the loop condition, so the result is identical, only the idle passes give the CPU
// away. Patched in place: one 6-byte instruction each (atomic / spin-parked write, as autocont.cpp).
// ======================================================================================================================
constexpr uint32_t kFpwPollFn = 0x116D30, kFpwPollSite = 0x116F12;  // FileLoad.Poll body / its per-pass unlock call
constexpr uint32_t kFpwLoadFn = 0x116F80, kFpwLoadSite = 0x11702C;  // FileLoad body / its per-pass unlock call
constexpr uint32_t kFpwLockOff = 0xCD0, kFpwCtrOff = 0x6BC;           // ctx+CD0h SRW lock; 6BC/6C0/6C4/6C8 ring counters
volatile LONG g_fpwOn = 0;
bool g_fpwInstalled = false;
int g_fpwSpinUs = 20, g_fpwYieldUs = 200, g_fpwSleepUs = 250;
LARGE_INTEGER g_fpwQpf{};
uint8_t* g_fpwPage = nullptr;
RUNTIME_FUNCTION g_fpwRf[2]{};
struct FpwSpin {
    DWORD tid = 0;
    bool idle = false;  // the previous pass looped
    uint32_t c[4] = {};
    LONGLONG t0 = 0;
    HANDLE timer = nullptr;  // high-resolution waitable timer (INVALID_HANDLE_VALUE = not available)
};
FpwSpin g_fpwS[2];  // 0 = FileLoad.Poll, 1 = FileLoad (one thread each)
volatile LONG64 g_fpwPass[2] = {}, g_fpwPause[2] = {}, g_fpwYield[2] = {}, g_fpwSleep[2] = {}, g_fpwSleptUs[2] = {};

void FpwBackoff(uint8_t* lock, int k) {
    if (!g_fpwOn) return;
    const volatile uint32_t* c = reinterpret_cast<const volatile uint32_t*>(lock - kFpwLockOff + kFpwCtrOff);
    const uint32_t v[4] = {c[0], c[1], c[2], c[3]};
    const bool again = k == 0 ? (v[0] > v[2] || v[2] > v[3]) : v[0] > v[2];  // the native loop condition
    FpwSpin& s = g_fpwS[k];
    const DWORD tid = GetCurrentThreadId();
    if (s.tid != tid) {
        s.tid = tid;
        s.idle = false;
    }
    InterlockedIncrement64(&g_fpwPass[k]);
    if (!again) {
        s.idle = false;
        return;
    }
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    if (!s.idle || v[0] != s.c[0] || v[1] != s.c[1] || v[2] != s.c[2] || v[3] != s.c[3]) {
        s.idle = true;  // first looping pass or progress since the last one: run the next pass at once
        memcpy(s.c, v, sizeof(v));
        s.t0 = now.QuadPart;
        return;
    }
    const LONGLONG us = (now.QuadPart - s.t0) * 1000000 / g_fpwQpf.QuadPart;
    if (us < g_fpwSpinUs) {
        for (int i = 0; i < 16; ++i) YieldProcessor();
        InterlockedIncrement64(&g_fpwPause[k]);
        return;
    }
    if (us < g_fpwYieldUs || g_fpwSleepUs <= 0) {
        SwitchToThread();
        InterlockedIncrement64(&g_fpwYield[k]);
        return;
    }
    if (!s.timer) {
        s.timer = CreateWaitableTimerExW(nullptr, nullptr, 0x00000002 /*HIGH_RESOLUTION*/, TIMER_ALL_ACCESS);
        if (!s.timer) s.timer = INVALID_HANDLE_VALUE;
    }
    LARGE_INTEGER due;
    due.QuadPart = -(LONGLONG)g_fpwSleepUs * 10;
    if (s.timer != INVALID_HANDLE_VALUE && SetWaitableTimer(s.timer, &due, 0, nullptr, nullptr, FALSE)) {
        WaitForSingleObject(s.timer, 50);
        LARGE_INTEGER t1;
        QueryPerformanceCounter(&t1);
        InterlockedIncrement64(&g_fpwSleep[k]);
        InterlockedAdd64(&g_fpwSleptUs[k], (t1.QuadPart - now.QuadPart) * 1000000 / g_fpwQpf.QuadPart);
    } else {
        SwitchToThread();
        InterlockedIncrement64(&g_fpwYield[k]);
    }
}
void FpwBackoffPoll(uint8_t* lock) { FpwBackoff(lock, 0); }
void FpwBackoffLoad(uint8_t* lock) { FpwBackoff(lock, 1); }

uint8_t* FpwAllocIn(uintptr_t from, uintptr_t to, size_t size) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    const uintptr_t g = si.dwAllocationGranularity;
    MEMORY_BASIC_INFORMATION mbi;
    for (uintptr_t a = (from + g - 1) & ~(g - 1); a < to;) {
        if (!VirtualQuery((void*)a, &mbi, sizeof(mbi))) break;
        const uintptr_t rb = (uintptr_t)mbi.BaseAddress, re = rb + mbi.RegionSize;
        if (mbi.State == MEM_FREE) {
            const uintptr_t c = (rb + g - 1) & ~(g - 1);
            if (c >= from && c + size <= re && c + size <= to)
                if (void* p = VirtualAlloc((void*)c, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE)) return (uint8_t*)p;
        }
        if (re <= a) break;
        a = re;
    }
    return nullptr;
}
uint8_t* FpwAllocNear(uintptr_t lo, uintptr_t hi, size_t size) {
    const uintptr_t reach = 0x7FF00000ull - (hi - lo);
    if (uint8_t* p = FpwAllocIn(hi, hi + reach, size)) return p;
    return FpwAllocIn(lo > reach + 0x10000 ? lo - reach : 0x10000, lo, size);
}
// One interlocked 8- or 16-byte compare-exchange; else park the first 2 bytes on "jmp $", write the tail, then the head
// (same scheme as autocont.cpp). The site is one instruction, so no thread can be inside it.
bool FpwPatchAtomic(uint8_t* p, const uint8_t* bytes, size_t len) {
    const uintptr_t a = (uintptr_t)p, w8 = a & ~uintptr_t(7), w16 = a & ~uintptr_t(15);
    const bool fit8 = a + len <= w8 + 8, fit16 = a + len <= w16 + 16;
    if (!fit8 && !fit16) return false;
    void* win = fit8 ? (void*)w8 : (void*)w16;
    const size_t wlen = fit8 ? 8 : 16;
    DWORD old = 0;
    if (!VirtualProtect(win, wlen, PAGE_EXECUTE_READWRITE, &old)) return false;
    bool ok = false;
    if (fit8) {
        volatile LONG64* q = (volatile LONG64*)w8;
        const LONG64 cur = *q;
        LONG64 nw = cur;
        memcpy((uint8_t*)&nw + (a - w8), bytes, len);
        ok = InterlockedCompareExchange64(q, nw, cur) == cur;
    } else {
        volatile LONG64* q = (volatile LONG64*)w16;
        alignas(16) LONG64 cur[2] = {q[0], q[1]};
        LONG64 nw[2] = {cur[0], cur[1]};
        memcpy((uint8_t*)nw + (a - w16), bytes, len);
        ok = _InterlockedCompareExchange128(q, nw[1], nw[0], cur) != 0;
    }
    VirtualProtect(win, wlen, old, &old);
    FlushInstructionCache(GetCurrentProcess(), win, wlen);
    return ok;
}
bool FpwPatch(uint8_t* p, const uint8_t* bytes, size_t len) {
    if (FpwPatchAtomic(p, bytes, len)) return true;
    static const uint8_t spin[2] = {0xEB, 0xFE};
    uint8_t orig[2] = {p[0], p[1]};
    if (len < 3 || !FpwPatchAtomic(p, spin, 2)) return false;
    DWORD old = 0;
    if (!VirtualProtect(p + 2, len - 2, PAGE_EXECUTE_READWRITE, &old)) {
        FpwPatchAtomic(p, orig, 2);
        return false;
    }
    memcpy(p + 2, bytes + 2, len - 2);
    VirtualProtect(p + 2, len - 2, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, len);
    return FpwPatchAtomic(p, bytes, 2);
}
// Masked compare (0x100 = any byte).
bool FpwMatch(const uint8_t* p, const uint16_t* pat, size_t n) {
    for (size_t i = 0; i < n; ++i)
        if (pat[i] < 0x100 && p[i] != pat[i]) return false;
    return true;
}
constexpr uint16_t W = 0x100;
// From site - 3: mov rcx,r15; call [IAT unlock]; ... both back-edges "ja" (targets checked separately).
const uint16_t kFpwPollPat[] = {0x49, 0x8B, 0xCF, 0xFF, 0x15, W, W, W, W, 0x8B, 0x85, 0xC4, 0x06, 0x00, 0x00, 0x48, 0x8D, 0xBD, 0xA8,
                                0x06, 0x00, 0x00, 0x4C, 0x8D, 0xB5, 0xB8, 0x07, 0x00, 0x00, 0x39, 0x85, 0xBC, 0x06, 0x00, 0x00, 0x0F,
                                0x87, W, W, W, W, 0x48, 0x8D, 0xBD, 0xA8, 0x06, 0x00, 0x00, 0x4C, 0x8D, 0xB5, 0xB8, 0x07, 0x00, 0x00,
                                0x3B, 0x85, 0xC8, 0x06, 0x00, 0x00, 0x0F, 0x87, W, W, W, W};
// From site - 3: mov rcx,rbx; call [IAT unlock]; mov eax,[rsi+1Ch]; cmp [rsi+14h],eax; ja (loop head)
const uint16_t kFpwLoadPat[] = {0x48, 0x8B, 0xCB, 0xFF, 0x15, W, W, W, W, 0x8B, 0x46, 0x1C, 0x39, 0x46, 0x14, 0x77, 0x96};
const uint8_t kFpwPollLea[] = {0x4C, 0x8D, 0xB9, 0xD0, 0x0C, 0x00, 0x00};  // fn+23h lea r15,[rcx+CD0h]
const uint8_t kFpwLoadLea[] = {0x48, 0x8D, 0x99, 0xD0, 0x0C, 0x00, 0x00};  // fn+25h lea rbx,[rcx+CD0h]
const uint8_t kFpwLoadRing[] = {0x48, 0x8D, 0xB1, 0xA8, 0x06, 0x00, 0x00};  // fn+3Ah lea rsi,[rcx+6A8h]

// Thunk: sub rsp,28h; mov [rsp+20h],rcx; mov rax,[slot ptr]; call [rax] (the native unlock); mov rcx,[rsp+20h];
// call [backoff]; add rsp,28h; ret. 34 bytes, unwind = UWOP_ALLOC_SMALL 28h after a 4-byte prolog.
size_t FpwThunk(uint8_t* t, const void* slotVar, const void* fnVar) {
    static const uint8_t code[] = {0x48, 0x83, 0xEC, 0x28, 0x48, 0x89, 0x4C, 0x24, 0x20, 0x48, 0x8B, 0x05, 0, 0, 0, 0, 0xFF,
                                   0x10, 0x48, 0x8B, 0x4C, 0x24, 0x20, 0xFF, 0x15, 0, 0, 0, 0, 0x48, 0x83, 0xC4, 0x28, 0xC3};
    memcpy(t, code, sizeof(code));
    const int32_t d1 = (int32_t)((const uint8_t*)slotVar - (t + 16)), d2 = (int32_t)((const uint8_t*)fnVar - (t + 29));
    memcpy(t + 12, &d1, 4);
    memcpy(t + 25, &d2, 4);
    return sizeof(code);
}

// Verifies both sites, builds the thunks, retargets the two calls. Logs one line either way.
bool FpwInstall() {
    const uint32_t rPf = Grva(kFpwPollFn), rPs = Grva(kFpwPollSite), rLf = Grva(kFpwLoadFn), rLs = Grva(kFpwLoadSite);
    if (!rPf || !rPs || !rLf || !rLs) {
        TLOG("FPW: not installed (loader poll loop not found on this build)");
        return false;
    }
    uint8_t* pf = (uint8_t*)(g_base + Grva(kFpwPollFn));
    uint8_t* ps = (uint8_t*)(g_base + Grva(kFpwPollSite));
    uint8_t* lf = (uint8_t*)(g_base + Grva(kFpwLoadFn));
    uint8_t* ls = (uint8_t*)(g_base + Grva(kFpwLoadSite));
    auto relTarget = [](const uint8_t* jcc6) {  // 0F 87 rel32
        int32_t r;
        memcpy(&r, jcc6 + 2, 4);
        return jcc6 + 6 + r;
    };
    bool ok = false;
    __try {
        ok = FpwMatch(ps - 3, kFpwPollPat, sizeof(kFpwPollPat) / sizeof(kFpwPollPat[0])) &&
             FpwMatch(ls - 3, kFpwLoadPat, sizeof(kFpwLoadPat) / sizeof(kFpwLoadPat[0])) &&
             !memcmp(pf + 0x23, kFpwPollLea, sizeof(kFpwPollLea)) && !memcmp(lf + 0x25, kFpwLoadLea, sizeof(kFpwLoadLea)) &&
             !memcmp(lf + 0x3A, kFpwLoadRing, sizeof(kFpwLoadRing)) &&
             relTarget(ps + 0x20) == pf + 0x50 && relTarget(ps + 0x3A) == pf + 0x50 &&  // both Poll back-edges -> loop head
             ls + 6 + 8 - 0x6A == lf + 0x50;                                           // FileLoad back-edge -> loop head
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        ok = false;
    }
    if (!ok) {
        TLOG("FPW: not installed (loader poll loop bytes differ - another patch or build)");
        return false;
    }
    g_fpwPage = FpwAllocNear(g_base, g_imgEnd, 4096);
    if (!g_fpwPage) {
        TLOG("FPW: not installed (no executable page within 2 GB of the image)");
        return false;
    }
    // Page: 0x000 Poll thunk, 0x040 FileLoad thunk, 0x080 unwind info, 0x100 data (IAT slot addresses, backoff fns).
    uint8_t* t = g_fpwPage;
    void** data = reinterpret_cast<void**>(t + 0x100);
    int32_t d;
    memcpy(&d, ps + 2, 4);
    data[0] = ps + 6 + d;  // Poll's unlock IAT slot
    memcpy(&d, ls + 2, 4);
    data[1] = ls + 6 + d;  // FileLoad's unlock IAT slot
    data[2] = reinterpret_cast<void*>(&FpwBackoffPoll);
    data[3] = reinterpret_cast<void*>(&FpwBackoffLoad);
    const size_t n0 = FpwThunk(t, &data[0], &data[2]);
    const size_t n1 = FpwThunk(t + 0x40, &data[1], &data[3]);
    static const uint8_t unwind[8] = {0x01, 0x04, 0x01, 0x00, 0x04, 0x42, 0x00, 0x00};  // v1, prolog 4, 1 code: ALLOC_SMALL 28h
    memcpy(t + 0x80, unwind, sizeof(unwind));
    g_fpwRf[0] = {0x000, (DWORD)n0, 0x80};
    g_fpwRf[1] = {0x040, (DWORD)(0x40 + n1), 0x80};
    FlushInstructionCache(GetCurrentProcess(), t, 0x200);
    if (!RtlAddFunctionTable(g_fpwRf, 2, reinterpret_cast<DWORD64>(t))) {
        TLOG("FPW: not installed (thunk unwind registration failed)");
        return false;
    }
    auto callTo = [](uint8_t* site, uint8_t* thunk, uint8_t out[6]) {  // E8 rel32 + 1-byte nop replaces FF 15 disp32
        const intptr_t r = (intptr_t)thunk - (intptr_t)(site + 5);
        if (r < INT32_MIN || r > INT32_MAX) return false;
        const int32_t r32 = (int32_t)r;
        out[0] = 0xE8;
        memcpy(out + 1, &r32, 4);
        out[5] = 0x90;
        return true;
    };
    uint8_t bp[6], bl[6];
    if (!callTo(ps, t, bp) || !callTo(ls, t + 0x40, bl)) {
        TLOG("FPW: not installed (thunk out of rel32 reach)");
        RtlDeleteFunctionTable(g_fpwRf);
        return false;
    }
    uint8_t origP[6];
    memcpy(origP, ps, 6);
    if (!FpwPatch(ps, bp, 6)) {
        TLOG("FPW: not installed (Poll site write failed)");
        RtlDeleteFunctionTable(g_fpwRf);
        return false;
    }
    if (!FpwPatch(ls, bl, 6)) {
        FpwPatch(ps, origP, 6);  // keep both or neither
        TLOG("FPW: not installed (FileLoad site write failed; Poll site restored)");
        RtlDeleteFunctionTable(g_fpwRf);
        return false;
    }
    return true;
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

    // FLP ([Patches] FileLoadPrio = 0 | 1, [Tuning] FileLoadPrio_MaxCpus = 6): see FlpThread
    if (I(L"Patches", L"FileLoadPrio", 0) == 1) {
        g_flpMaxCpus = std::clamp(I(L"Tuning", L"FileLoadPrio_MaxCpus", 6), 1, 64);
        if (!g_getDesc) TLOG("FLP: GetThreadDescription not available - off");
        else if (HANDLE h = CreateThread(nullptr, 0, FlpThread, nullptr, 0, nullptr)) {
            CloseHandle(h);
            TLOG("FLP: installed (KTGLGraph.Cache.FileLoad + .Poll BELOW_NORMAL in the field only, not during a level load, while the process affinity has <= %d CPUs)", g_flpMaxCpus);
        }
    }

    // FPW ([Patches] FileLoadPoll = 0 | 1, [Tuning] FileLoadPoll_SpinUs / _YieldUs / _SleepUs): see FpwBackoff
    if (I(L"Patches", L"FileLoadPoll", 0) == 1) {
        g_fpwSpinUs = std::clamp(I(L"Tuning", L"FileLoadPoll_SpinUs", 20), 0, 10000);
        g_fpwYieldUs = std::clamp(I(L"Tuning", L"FileLoadPoll_YieldUs", 200), g_fpwSpinUs, 100000);
        g_fpwSleepUs = std::clamp(I(L"Tuning", L"FileLoadPoll_SleepUs", 250), 0, 5000);
        QueryPerformanceFrequency(&g_fpwQpf);
        if (g_fpwQpf.QuadPart > 0 && FpwInstall()) {
            g_fpwInstalled = true;
            g_fpwOn = 1;
            TLOG("FPW: installed (FileLoad.Poll 116F12 + FileLoad 11702C idle passes back off: pause %d us, yield to %d us, then %d us "
                 "timer waits; any ring progress runs the next pass at once)", g_fpwSpinUs, g_fpwYieldUs, g_fpwSleepUs);
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

void FlpStatusLine(char* buf, unsigned n) {
    if (!n) return;
    buf[0] = 0;
    int o = 0;
    if (g_flpThreads || g_flpSets) o = snprintf(buf, n, "FLP: %s | loader threads %ld, lowered %ld, restored %ld", g_flpActive ? "active" : "idle", g_flpThreads, g_flpSets, g_flpRestores);
    if (g_fpwInstalled && o >= 0 && o < (int)n) {
        // Totals since boot: passes, then the idle passes that paused / yielded / slept (and the measured sleep time).
        snprintf(buf + o, n - o, "%sFPW: Poll passes %lld (pause %lld, yield %lld, sleep %lld = %lld ms) | FileLoad passes %lld (pause %lld, yield %lld, sleep %lld = %lld ms)",
                 o ? " || " : "", g_fpwPass[0], g_fpwPause[0], g_fpwYield[0], g_fpwSleep[0], g_fpwSleptUs[0] / 1000, g_fpwPass[1],
                 g_fpwPause[1], g_fpwYield[1], g_fpwSleep[1], g_fpwSleptUs[1] / 1000);
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
