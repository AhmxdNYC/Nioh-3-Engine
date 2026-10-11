// jt.cpp - JT: job-timing / logic-join probe (see jt.h). [Profiler] JT = 0 by default. MEASUREMENT ONLY.
//
// Recording (game threads, jt.asm -> the extern "C" functions below): per job a 40-byte record in a lock-free ring (QPC
// start / end, thread, job function RVA, inner callback RVA, ParallelFor range / grain); per wait of the watched thread
// (KIDSTaskScheduler0) start / end / CV / the first game frames of its stack / the engine's logic tick; per notify of that
// CV while it sleeps the waker thread, its stack and the job it is running. Analysis + log: one background thread.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#include <intrin.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "jt.h"

extern "C" {
extern volatile uint64_t jt_waitCv;
extern volatile uint32_t jt_waitTid;
extern uint64_t jt_iatSleepSrw, jt_iatWake, jt_iatWakeAll;
void JtJobBegin(void* job, void* fn);
void JtJobEnd(void* job);
void JtWaitBegin(void* cv, uint32_t ms);
void JtWaitEnd(void* cv, uint32_t result);
void JtOnWake(void* cv, uint32_t kind);
}

namespace jt {
namespace {

// ---- v2.02 layout (notes\job-system.md, notes\whats-left.md #4; decompiles 0x111184, 0x1113B4, 0x204584, 0x1753B0) ------
constexpr uint32_t kRvaJobNode = 0x111184;   // JobNodeExecute: inner = vfunc +8 of [[job+48h]+28h] (JobInvokeTimed's callback)
constexpr uint32_t kRvaPfor = 0x204584;      // JobParallelForSplit: fn +50h, range [+70h, +74h), grain +78h
constexpr uint32_t kRvaRange = 0x1753B0;     // ObjRangeJobSplitInvoke: range [+48h, +4Ch), grain +50h
constexpr uint32_t kRvaFrameMgrPtr = 0x4759910, kOffTick = 0x610;   // G+610h: +1 per game-update tick (osd.cpp)
constexpr uint32_t kCrtLo = 0xB5E000, kCrtHi = 0xBF0000;   // statically linked CRT / STL (cv wait, _Cnd_*, mutex): skipped in stacks

void (*g_log)(const char*, ...) = nullptr;
#define JLOG(fmt, ...) do { if (g_log) g_log("JT: " fmt, __VA_ARGS__); } while (0)

uint8_t* g_base = nullptr;           // engine image for the tick counter
uintptr_t g_exeLo = 0, g_exeHi = 0;  // module whose RVAs are recorded
uint32_t g_kindNode = kRvaJobNode, g_kindPfor = kRvaPfor, g_kindRange = kRvaRange;
bool g_crtFilter = true;
wchar_t g_watchName[64] = L"KIDSTaskScheduler0";
int g_reportSec = 30, g_top = 6;
double g_usPerTick = 0;
volatile LONG g_started = 0;

inline int64_t Qpc() {
    LARGE_INTEGER q;
    QueryPerformanceCounter(&q);
    return q.QuadPart;
}
inline uint32_t Rva(const void* p) {   // 0 = null, 1 = outside the module
    const uintptr_t a = (uintptr_t)p;
    if (!a) return 0;
    return (a >= g_exeLo && a < g_exeHi) ? (uint32_t)(a - g_exeLo) : 1u;
}

// ---- thread registry ----------------------------------------------------------------------------------------------------
constexpr int kMaxThr = 256;
volatile LONG g_nThr = 0;
DWORD g_thrTid[kMaxThr];
thread_local int t_thr = -1;
int MyThr() {
    if (t_thr < 0) {
        LONG i = InterlockedIncrement(&g_nThr) - 1;
        if (i >= kMaxThr) i = kMaxThr - 1;
        else g_thrTid[i] = GetCurrentThreadId();
        t_thr = (int)i;
    }
    return t_thr;
}

// ---- rings (single producer per slot claim; the reader validates the sequence number before and after copying) ------
struct Open { void* job; int64_t t0; uint32_t fn, inner, range, grain; uint8_t kind; };
constexpr int kStack = 32;
thread_local Open t_open[kStack];
thread_local int t_depth = 0;

struct JRec { int64_t t0, t1; uint32_t fn, inner, range, grain; uint16_t thr; uint8_t depth, kind; volatile uint32_t seq; };
struct WRec { int64_t t0, t1; uint64_t cv; uint32_t site[4]; uint32_t tick, ms, res; volatile uint32_t seq; };
struct KRec { int64_t t, jobT0; uint64_t cv; uint32_t site[4]; uint32_t jobFn, jobInner; uint16_t thr; uint8_t kind, inJob; volatile uint32_t seq; };
constexpr uint32_t kJRing = 1u << 18, kWRing = 1u << 14, kKRing = 1u << 14;
JRec* g_jr = nullptr;
WRec* g_wr = nullptr;
KRec* g_kr = nullptr;
volatile LONG64 g_jHead = 0, g_wHead = 0, g_kHead = 0, g_unbalanced = 0;
thread_local WRec t_wait;

uint32_t Tick() {
    if (!g_base) return 0;
    __try {
        const uint8_t* G = *reinterpret_cast<uint8_t* const volatile*>(g_base + kRvaFrameMgrPtr);
        return G ? *reinterpret_cast<const volatile uint32_t*>(G + kOffTick) : 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

void GameFrames(uint32_t* out, int n) {   // return RVAs of the first game frames on this stack, CRT / STL frames skipped
    void* fr[40];
    const USHORT c = RtlCaptureStackBackTrace(1, 40, fr, nullptr);
    int k = 0;
    for (USHORT i = 0; i < c && k < n; ++i) {
        const uintptr_t a = (uintptr_t)fr[i];
        if (a < g_exeLo || a >= g_exeHi) continue;
        const uint32_t r = (uint32_t)(a - g_exeLo);
        if (g_crtFilter && r >= kCrtLo && r < kCrtHi) continue;
        out[k++] = r;
    }
    for (; k < n; ++k) out[k] = 0;
}

void ReadKind(Open& o, const uint8_t* j) {
    __try {
        if (o.fn == g_kindNode) {
            o.kind = 1;
            const uint8_t* d = *reinterpret_cast<uint8_t* const*>(j + 0x48);
            const uint8_t* cb = d ? *reinterpret_cast<uint8_t* const*>(d + 0x28) : nullptr;
            if (cb) o.inner = Rva((*reinterpret_cast<void* const* const*>(cb))[1]);
        } else if (o.fn == g_kindPfor) {
            o.kind = 2;
            o.inner = Rva(*reinterpret_cast<void* const*>(j + 0x50));
            o.range = (uint32_t)(*reinterpret_cast<const int32_t*>(j + 0x74) - *reinterpret_cast<const int32_t*>(j + 0x70));
            o.grain = *reinterpret_cast<const uint32_t*>(j + 0x78);
        } else if (o.fn == g_kindRange) {
            o.kind = 3;
            o.range = (uint32_t)(*reinterpret_cast<const int32_t*>(j + 0x4C) - *reinterpret_cast<const int32_t*>(j + 0x48));
            o.grain = *reinterpret_cast<const uint32_t*>(j + 0x50);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        o.kind = 0xFF;
    }
}

}  // namespace
}  // namespace jt

using namespace jt;

extern "C" void JtJobBegin(void* job, void* fn) {
    MyThr();
    const int d = t_depth++;
    if (d >= kStack) return;
    Open& o = t_open[d];
    o.job = job;
    o.fn = Rva(fn);
    o.inner = o.range = o.grain = 0;
    o.kind = 0;
    if (o.fn == g_kindNode || o.fn == g_kindPfor || o.fn == g_kindRange) ReadKind(o, (const uint8_t*)job);
    o.t0 = Qpc();
}

extern "C" void JtJobEnd(void* job) {
    const int64_t t1 = Qpc();
    if (t_depth <= 0) {
        InterlockedIncrement64(&g_unbalanced);
        return;
    }
    const int d = --t_depth;
    if (d >= kStack || !g_jr) return;
    const Open& o = t_open[d];
    if (o.job != job) InterlockedIncrement64(&g_unbalanced);
    const LONG64 idx = InterlockedIncrement64(&g_jHead) - 1;
    JRec& r = g_jr[idx & (kJRing - 1)];
    r.seq = 0;
    _WriteBarrier();
    r.t0 = o.t0;
    r.t1 = t1;
    r.fn = o.fn;
    r.inner = o.inner;
    r.range = o.range;
    r.grain = o.grain;
    r.thr = (uint16_t)t_thr;
    r.depth = (uint8_t)(d > 255 ? 255 : d);
    r.kind = o.kind;
    _WriteBarrier();
    r.seq = (uint32_t)(idx + 1);
}

extern "C" void JtWaitBegin(void* cv, uint32_t ms) {
    const DWORD le = GetLastError();
    t_wait.t0 = Qpc();
    t_wait.cv = (uint64_t)cv;
    t_wait.ms = ms;
    t_wait.tick = Tick();
    GameFrames(t_wait.site, 4);
    jt_waitCv = (uint64_t)cv;   // from here a notify of this CV takes the cave's slow path
    SetLastError(le);
}

extern "C" void JtWaitEnd(void* cv, uint32_t result) {
    const DWORD le = GetLastError();
    const int64_t t1 = Qpc();
    jt_waitCv = 0;
    if (g_wr && t_wait.cv == (uint64_t)cv) {
        const LONG64 idx = InterlockedIncrement64(&g_wHead) - 1;
        WRec& r = g_wr[idx & (kWRing - 1)];
        r.seq = 0;
        _WriteBarrier();
        r.t0 = t_wait.t0;
        r.t1 = t1;
        r.cv = t_wait.cv;
        memcpy(r.site, t_wait.site, sizeof r.site);
        r.tick = t_wait.tick;
        r.ms = t_wait.ms;
        r.res = result;
        _WriteBarrier();
        r.seq = (uint32_t)(idx + 1);
    }
    SetLastError(le);
}

extern "C" void JtOnWake(void* cv, uint32_t kind) {
    const DWORD le = GetLastError();
    const int64_t t = Qpc();
    if (g_kr) {
        const int thr = MyThr();
        const LONG64 idx = InterlockedIncrement64(&g_kHead) - 1;
        KRec& r = g_kr[idx & (kKRing - 1)];
        r.seq = 0;
        _WriteBarrier();
        r.t = t;
        r.cv = (uint64_t)cv;
        r.thr = (uint16_t)thr;
        r.kind = (uint8_t)kind;
        const int d = t_depth;
        if (d > 0 && d <= kStack) {
            const Open& o = t_open[d - 1];
            r.inJob = 1;
            r.jobFn = o.fn;
            r.jobInner = o.inner;
            r.jobT0 = o.t0;
        } else {
            r.inJob = 0;
            r.jobFn = r.jobInner = 0;
            r.jobT0 = 0;
        }
        GameFrames(r.site, 4);
        _WriteBarrier();
        r.seq = (uint32_t)(idx + 1);
    }
    SetLastError(le);
}

namespace jt {
namespace {

// ---- analysis (report thread) -------------------------------------------------------------------------------------------
struct Job { int64_t t0, t1; uint32_t fn, inner, range, grain; uint16_t thr; uint8_t depth, kind; };
struct Wait { int64_t t0, t1; uint64_t cv; uint32_t site[4]; uint32_t tick, ms, res; };
struct Wake { int64_t t, jobT0; uint64_t cv; uint32_t site[4]; uint32_t jobFn, jobInner; uint16_t thr; uint8_t kind, inJob; };

std::vector<Job> g_jobs;     // sorted by t1, last ~2 s
std::vector<Wait> g_waits;   // not analysed yet
std::vector<Wake> g_wakes;   // last ~2 s
LONG64 g_jTail = 0, g_wTail = 0, g_kTail = 0, g_jDrop = 0, g_wDrop = 0, g_kDrop = 0;
int g_watchThr = -1;         // thread index of the watched thread (it runs jobs too)

// log2 histogram of microseconds, 16 buckets per octave (~4.4 %), 0 .. ~60 s; percentiles clamped to the exact min / max
struct Hist {
    uint32_t b[288] = {};
    uint64_t n = 0;
    double lo = 0, hi = 0;
    void Add(double us) {
        const int k = us <= 0 ? 0 : std::min(287, (int)(16.0 * std::log2(1.0 + us)));
        ++b[k];
        if (!n || us < lo) lo = us;
        if (!n || us > hi) hi = us;
        ++n;
    }
    double Pct(double p) const {
        if (!n) return 0;
        if (p >= 1.0) return hi;
        const uint64_t want = (uint64_t)(p * (double)(n - 1)) + 1;
        uint64_t c = 0;
        for (int k = 0; k < 288; ++k)
            if ((c += b[k]) >= want) return std::clamp(std::exp2(((double)k + 0.5) / 16.0) - 1.0, lo, hi);
        return hi;
    }
};
inline uint64_t JKey(uint32_t fn, uint32_t inner) { return ((uint64_t)fn << 32) | inner; }
struct JobAgg {
    uint64_t n = 0, leaves = 0, leafRange = 0;
    double sumUs = 0, maxUs = 0;
    uint64_t thrMask[4] = {};
    Hist h;
    std::map<uint32_t, uint32_t> grains;
};
struct CritAgg { uint32_t n = 0, before = 0; Hist dur, startAfter, startBefore; };   // start: relative to the wait's start
struct SiteAgg {
    uint64_t n = 0, noRel = 0, relInJob = 0, timeouts = 0;
    double sumUs = 0, busyUs = 0, idleUs = 0, threadsSum = 0;
    Hist h, beforeWake;   // wait durations; for releasers outside a job: time since their last job ended
    std::map<uint16_t, uint32_t> relThr;
    std::map<std::string, uint32_t> relSite;
    std::map<uint64_t, CritAgg> crit, longest;
};
struct Window {
    int64_t q0 = 0;
    uint32_t tick0 = 0;
    uint64_t waits = 0, timeouts = 0, noRel = 0, jobs = 0;
    double blockedUs = 0, jobUs = 0;
    std::map<uint32_t, double> perTick;   // logic tick -> blocked us
    std::map<std::string, SiteAgg> sites;
    std::unordered_map<uint64_t, JobAgg> jobAgg;
    uint64_t thrSeen[4] = {};
};
Window g_win;

std::string SiteKey(const uint32_t* s) {
    char b[64];
    if (!s[0]) return "(no game frame)";
    int o = _snprintf_s(b, sizeof b, _TRUNCATE, "%X", s[0]);
    for (int i = 1; i < 3 && s[i] && o > 0 && o < (int)sizeof b - 10; ++i) o += _snprintf_s(b + o, sizeof b - (size_t)o, _TRUNCATE, " < %X", s[i]);
    return b;
}
std::string FnName(uint32_t fn, uint32_t inner) {
    char b[48];
    auto one = [](char* d, size_t n, uint32_t r) {
        if (r == 0) _snprintf_s(d, n, _TRUNCATE, "-");
        else if (r == 1) _snprintf_s(d, n, _TRUNCATE, "ext");
        else _snprintf_s(d, n, _TRUNCATE, "%X", r);
    };
    char a[16], c[16];
    one(a, sizeof a, fn);
    one(c, sizeof c, inner);
    _snprintf_s(b, sizeof b, _TRUNCATE, "%s/%s", a, c);
    return b;
}
std::vector<std::string> g_thrName;
const char* ThrName(int i) {
    if (i < 0 || i >= kMaxThr) return "?";
    if ((int)g_thrName.size() <= i) g_thrName.resize((size_t)i + 1);
    std::string& s = g_thrName[(size_t)i];
    if (s.empty() || s[0] == '?') {
        char b[64] = "?";
        using GetDescFn = HRESULT(WINAPI*)(HANDLE, PWSTR*);
        static GetDescFn gd = []() {
            HMODULE k = GetModuleHandleW(L"kernelbase.dll");
            if (!k) k = GetModuleHandleW(L"kernel32.dll");
            return k ? reinterpret_cast<GetDescFn>(GetProcAddress(k, "GetThreadDescription")) : nullptr;
        }();
        if (HANDLE h = gd ? OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, g_thrTid[i]) : nullptr) {
            PWSTR d = nullptr;
            if (SUCCEEDED(gd(h, &d)) && d) {
                int n = 0;
                for (const wchar_t* p = d; *p && n < 40; ++p) b[n++] = (*p > 32 && *p < 127) ? (char)*p : '_';
                b[n] = 0;
                if (!n) _snprintf_s(b, sizeof b, _TRUNCATE, "tid%lu", g_thrTid[i]);
                LocalFree(d);
            }
            CloseHandle(h);
        }
        if (b[0] == '?') _snprintf_s(b, sizeof b, _TRUNCATE, "?tid%lu", g_thrTid[i]);
        s = b;
    }
    return s.c_str();
}

template <class R, class O, class F>
void DrainRing(R* ring, uint32_t size, volatile LONG64& head, LONG64& tail, LONG64& drop, std::vector<O>& out, F conv) {
    const LONG64 h = head;
    if (h - tail > (LONG64)size) {
        drop += h - tail - size;
        tail = h - size;
    }
    for (; tail < h; ++tail) {
        const R& r = ring[tail & (size - 1)];
        const uint32_t want = (uint32_t)(tail + 1);
        if (r.seq != want) continue;   // still being written (or lost): skip
        _ReadBarrier();
        R c = r;   // copy, then re-check: a writer that lapped us invalidates it
        _ReadBarrier();
        if (r.seq != want) continue;
        out.push_back(conv(c));
    }
}

void AddJob(Window& w, const Job& j) {
    const double us = (double)(j.t1 - j.t0) * g_usPerTick;
    JobAgg& a = w.jobAgg[JKey(j.fn, j.inner)];
    a.n++;
    a.sumUs += us;
    a.maxUs = std::max(a.maxUs, us);
    a.h.Add(us);
    if (j.thr < 256) {
        a.thrMask[j.thr >> 6] |= 1ull << (j.thr & 63);
        w.thrSeen[j.thr >> 6] |= 1ull << (j.thr & 63);
    }
    if ((j.kind == 2 || j.kind == 3) && j.range <= j.grain) {
        a.leaves++;
        a.leafRange += j.range;
        if (a.grains.size() < 16 || a.grains.count(j.grain)) a.grains[j.grain]++;
    }
    w.jobs++;
    w.jobUs += us;
}

void Drain() {
    std::vector<Job> nj;
    DrainRing(g_jr, kJRing, g_jHead, g_jTail, g_jDrop, nj, [](const JRec& r) {
        return Job{r.t0, r.t1, r.fn, r.inner, r.range, r.grain, r.thr, r.depth, r.kind};
    });
    std::sort(nj.begin(), nj.end(), [](const Job& a, const Job& b) { return a.t1 < b.t1; });
    for (const Job& j : nj) AddJob(g_win, j);
    const size_t mid = g_jobs.size();
    g_jobs.insert(g_jobs.end(), nj.begin(), nj.end());
    std::inplace_merge(g_jobs.begin(), g_jobs.begin() + (ptrdiff_t)mid, g_jobs.end(), [](const Job& a, const Job& b) { return a.t1 < b.t1; });
    DrainRing(g_wr, kWRing, g_wHead, g_wTail, g_wDrop, g_waits, [](const WRec& r) {
        Wait w{r.t0, r.t1, r.cv, {r.site[0], r.site[1], r.site[2], r.site[3]}, r.tick, r.ms, r.res};
        return w;
    });
    DrainRing(g_kr, kKRing, g_kHead, g_kTail, g_kDrop, g_wakes, [](const KRec& r) {
        Wake k{r.t, r.jobT0, r.cv, {r.site[0], r.site[1], r.site[2], r.site[3]}, r.jobFn, r.jobInner, r.thr, r.kind, r.inJob};
        return k;
    });
}

void Prune(int64_t now) {
    const int64_t keep = (int64_t)(2.0e6 / g_usPerTick);   // 2 s
    auto j = std::lower_bound(g_jobs.begin(), g_jobs.end(), now - keep, [](const Job& a, int64_t t) { return a.t1 < t; });
    g_jobs.erase(g_jobs.begin(), j);
    g_wakes.erase(std::remove_if(g_wakes.begin(), g_wakes.end(), [&](const Wake& k) { return k.t < now - keep; }), g_wakes.end());
}

void Analyse(const Wait& w) {
    Window& W = g_win;
    const double durUs = (double)(w.t1 - w.t0) * g_usPerTick;
    SiteAgg& s = W.sites[SiteKey(w.site)];
    s.n++;
    s.sumUs += durUs;
    s.h.Add(durUs);
    W.waits++;
    W.blockedUs += durUs;
    W.perTick[w.tick] += durUs;
    const bool timeout = w.ms != INFINITE && w.res == 0;
    if (timeout) { s.timeouts++; W.timeouts++; }
    // the releaser: the last notify of this CV while the wait was in progress
    const Wake* rel = nullptr;
    for (const Wake& k : g_wakes)
        if (k.cv == w.cv && k.t >= w.t0 && k.t <= w.t1 && (!rel || k.t > rel->t)) rel = &k;
    if (!rel) {
        s.noRel++;
        W.noRel++;
    } else {
        s.relThr[rel->thr]++;
        s.relSite[SiteKey(rel->site)]++;
        if (rel->inJob) {
            s.relInJob++;
            CritAgg& c = s.crit[JKey(rel->jobFn, rel->jobInner)];
            c.n++;
            // the job's full duration: its record on the releasing thread (same start)
            auto it = std::lower_bound(g_jobs.begin(), g_jobs.end(), rel->t, [](const Job& a, int64_t t) { return a.t1 < t; });
            for (; it != g_jobs.end() && it->t1 - rel->t < (int64_t)(50000.0 / g_usPerTick); ++it)
                if (it->thr == rel->thr && it->t0 == rel->jobT0) {
                    c.dur.Add((double)(it->t1 - it->t0) * g_usPerTick);
                    break;
                }
            const double st = (double)(rel->jobT0 - w.t0) * g_usPerTick;
            if (st < 0) { c.before++; c.startBefore.Add(-st); }   // the job was already running when the wait began
            else c.startAfter.Add(st);
        } else {
            // not inside a JobWorkerPoll job: how long ago did that thread finish one?
            auto it = std::lower_bound(g_jobs.begin(), g_jobs.end(), rel->t, [](const Job& a, int64_t t) { return a.t1 < t; });
            while (it != g_jobs.begin()) {
                --it;
                if (it->thr == rel->thr) {
                    s.beforeWake.Add((double)(rel->t - it->t1) * g_usPerTick);
                    break;
                }
                if (rel->t - it->t1 > (int64_t)(100000.0 / g_usPerTick)) break;
            }
        }
    }
    // jobs of the other threads that overlap the wait
    auto it = std::lower_bound(g_jobs.begin(), g_jobs.end(), w.t0, [](const Job& a, int64_t t) { return a.t1 < t; });
    const int64_t cap = w.t1 + (int64_t)(50000.0 / g_usPerTick);
    std::vector<std::pair<int64_t, int64_t>> iv;
    uint64_t thrs[4] = {};
    const Job* longest = nullptr;
    double busy = 0;
    for (; it != g_jobs.end() && it->t1 <= cap; ++it) {
        if (it->t0 > w.t1 || (int)it->thr == g_watchThr) continue;
        const int64_t a = std::max(it->t0, w.t0), b = std::min(it->t1, w.t1);
        if (b <= a) continue;
        busy += (double)(b - a);
        iv.emplace_back(a, b);
        if (it->thr < 256) thrs[it->thr >> 6] |= 1ull << (it->thr & 63);
        if (!longest || it->t1 - it->t0 > longest->t1 - longest->t0) longest = &*it;
    }
    std::sort(iv.begin(), iv.end());
    double covered = 0;
    int64_t ce = w.t0;
    for (const auto& p : iv) {
        if (p.second <= ce) continue;
        covered += (double)(p.second - std::max(p.first, ce));
        ce = p.second;
    }
    int nthr = 0;
    for (uint64_t m : thrs) nthr += (int)__popcnt64(m);
    s.threadsSum += nthr;
    s.busyUs += busy * g_usPerTick;
    s.idleUs += std::max(0.0, durUs - covered * g_usPerTick);
    if (longest) {
        CritAgg& l = s.longest[JKey(longest->fn, longest->inner)];
        l.n++;
        l.dur.Add((double)(longest->t1 - longest->t0) * g_usPerTick);
    }
}

void ProcessWaits(int64_t now, bool all) {
    const int64_t delay = all ? 0 : (int64_t)(60000.0 / g_usPerTick);   // let jobs that overlap the wait finish
    size_t k = 0;
    for (; k < g_waits.size(); ++k) {
        if (!all && g_waits[k].t1 > now - delay) break;
        Analyse(g_waits[k]);
    }
    g_waits.erase(g_waits.begin(), g_waits.begin() + (ptrdiff_t)k);
}

template <class M>
std::vector<std::pair<typename M::key_type, uint32_t>> TopCounts(const M& m, size_t n) {
    std::vector<std::pair<typename M::key_type, uint32_t>> v(m.begin(), m.end());
    std::sort(v.begin(), v.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    if (v.size() > n) v.resize(n);
    return v;
}

void LogWindow() {
    Window& W = g_win;
    const int64_t now = Qpc();
    const double secs = (double)(now - W.q0) * g_usPerTick / 1e6;
    const uint32_t tick1 = Tick();
    uint32_t ticks = tick1 - W.tick0;
    if (!g_base || !W.tick0 || ticks > 100000) ticks = (uint32_t)W.perTick.size();
    const double tk = ticks ? (double)ticks : 1.0;
    std::vector<double> pt;
    for (const auto& kv : W.perTick) pt.push_back(kv.second);
    std::sort(pt.begin(), pt.end());
    auto pct = [&](double p) { return pt.empty() ? 0.0 : pt[std::min(pt.size() - 1, (size_t)(p * (double)(pt.size() - 1) + 0.5))]; };
    int nthr = 0;
    for (uint64_t m : W.thrSeen) nthr += (int)__popcnt64(m);
    JLOG("window %.1f s | watched %ls (tid %lu) | logic ticks %u (%.1f/s) | CV waits %llu (%.2f/tick), blocked %.3f ms/tick "
         "(%.1f %% of the window); blocked per tick p50 %.3f p90 %.3f max %.3f ms; timed out %llu, no waker seen %llu | jobs %llu "
         "(%.0f/s, %.3f ms/tick summed) on %d threads | dropped: jobs %lld, waits %lld, wakes %lld; unbalanced %lld",
         secs, g_watchName, (unsigned long)jt_waitTid, ticks, secs > 0 ? ticks / secs : 0.0, (unsigned long long)W.waits,
         (double)W.waits / tk, W.blockedUs / 1000.0 / tk, secs > 0 ? 100.0 * W.blockedUs / 1e6 / secs : 0.0, pct(0.5) / 1000.0,
         pct(0.9) / 1000.0, pt.empty() ? 0.0 : pt.back() / 1000.0, (unsigned long long)W.timeouts, (unsigned long long)W.noRel,
         (unsigned long long)W.jobs, secs > 0 ? (double)W.jobs / secs : 0.0, W.jobUs / 1000.0 / tk, nthr, g_jDrop, g_wDrop, g_kDrop,
         (long long)g_unbalanced);
    std::vector<std::pair<std::string, const SiteAgg*>> sites;
    for (const auto& kv : W.sites) sites.emplace_back(kv.first, &kv.second);
    std::sort(sites.begin(), sites.end(), [](const auto& a, const auto& b) { return a.second->sumUs > b.second->sumUs; });
    JLOG("wait sites by blocked time [return RVAs of the first 3 game frames above the STL wait, innermost first] | ms/tick | "
         "waits/tick | wait p50 / p90 / max us | released by (thread share) | waker call site (share)%s", "");
    int rank = 0;
    for (const auto& sp : sites) {
        if (++rank > g_top) break;
        const SiteAgg& s = *sp.second;
        std::string rel, rs;
        for (const auto& t : TopCounts(s.relThr, 3)) {
            char b[96];
            _snprintf_s(b, sizeof b, _TRUNCATE, "%s%s %.0f %%", rel.empty() ? "" : ", ", ThrName(t.first), 100.0 * t.second / (double)s.n);
            rel += b;
        }
        for (const auto& t : TopCounts(s.relSite, 2)) {
            char b[96];
            _snprintf_s(b, sizeof b, _TRUNCATE, "%s[%s] %.0f %%", rs.empty() ? "" : ", ", t.first.c_str(), 100.0 * t.second / (double)s.n);
            rs += b;
        }
        if (s.noRel) {
            char b[64];
            _snprintf_s(b, sizeof b, _TRUNCATE, "%sno waker %.0f %%", rel.empty() ? "" : ", ", 100.0 * (double)s.noRel / (double)s.n);
            rel += b;
        }
        JLOG("  %d [%s] | %.3f | %.2f | %.0f / %.0f / %.0f | %s | %s", rank, sp.first.c_str(), s.sumUs / 1000.0 / tk, (double)s.n / tk,
             s.h.Pct(0.5), s.h.Pct(0.9), s.h.Pct(1.0), rel.empty() ? "-" : rel.c_str(), rs.empty() ? "-" : rs.c_str());
        std::vector<std::pair<uint64_t, const CritAgg*>> cr;
        for (const auto& kv : s.crit) cr.emplace_back(kv.first, &kv.second);
        std::sort(cr.begin(), cr.end(), [](const auto& a, const auto& b) { return a.second->n > b.second->n; });
        std::string line;
        int k = 0;
        for (const auto& c : cr) {
            if (++k > 3) break;
            char b[260];
            const CritAgg& x = *c.second;
            _snprintf_s(b, sizeof b, _TRUNCATE, "%s%d) %s %.0f %% of the waits, job p50 %.0f p90 %.0f us, started before the wait in %.0f %% "
                        "(p50 %.0f us before), else p50 %.0f us after it began", line.empty() ? "" : "; ", k,
                        FnName((uint32_t)(c.first >> 32), (uint32_t)c.first).c_str(), 100.0 * x.n / (double)s.n, x.dur.Pct(0.5), x.dur.Pct(0.9),
                        x.n ? 100.0 * x.before / (double)x.n : 0.0, x.startBefore.Pct(0.5), x.startAfter.Pct(0.5));
            line += b;
        }
        const uint64_t outJob = s.n - s.noRel - s.relInJob;
        char tail[160] = "";
        if (outJob)
            _snprintf_s(tail, sizeof tail, _TRUNCATE, "%sreleaser not in a job %.0f %% (its last job ended p50 %.0f us before the wake)",
                        line.empty() ? "" : "; ", 100.0 * (double)outJob / (double)s.n, s.beforeWake.Pct(0.5));
        JLOG("     critical job (what the releasing thread was running at the wake, job fn/inner callback): %s%s", line.empty() ? "-" : line.c_str(), tail);
        std::vector<std::pair<uint64_t, const CritAgg*>> lo;
        for (const auto& kv : s.longest) lo.emplace_back(kv.first, &kv.second);
        std::sort(lo.begin(), lo.end(), [](const auto& a, const auto& b) { return a.second->n > b.second->n; });
        line.clear();
        k = 0;
        for (const auto& c : lo) {
            if (++k > 3) break;
            char b[160];
            _snprintf_s(b, sizeof b, _TRUNCATE, "%s%d) %s %.0f %% (p50 %.0f us)", line.empty() ? "" : "; ", k,
                        FnName((uint32_t)(c.first >> 32), (uint32_t)c.first).c_str(), 100.0 * c.second->n / (double)s.n, c.second->dur.Pct(0.5));
            line += b;
        }
        JLOG("     longest job overlapping the wait: %s | other job threads busy %.0f %% of the wait (%.1f threads ran jobs), no job "
             "running on any other thread %.0f %% of the wait", line.empty() ? "-" : line.c_str(),
             s.threadsSum > 0 ? 100.0 * s.busyUs / (s.sumUs * (s.threadsSum / (double)s.n)) : 0.0, s.threadsSum / (double)s.n,
             s.sumUs > 0 ? 100.0 * s.idleUs / s.sumUs : 0.0);
    }
    std::vector<std::pair<uint64_t, const JobAgg*>> js;
    for (const auto& kv : W.jobAgg) js.emplace_back(kv.first, &kv.second);
    std::sort(js.begin(), js.end(), [](const auto& a, const auto& b) { return a.second->sumUs > b.second->sumUs; });
    JLOG("jobs by total time (all threads; fn = job function RVA, inner = JobNode callback / ParallelFor body) | ms/tick | jobs/tick | "
         "p50 / p90 / max us | threads | split jobs: leaves/tick, grain (most common), mean leaf range%s", "");
    rank = 0;
    for (const auto& jp : js) {
        if (++rank > g_top * 2) break;
        const JobAgg& a = *jp.second;
        int nt = 0;
        for (uint64_t m : a.thrMask) nt += (int)__popcnt64(m);
        char split[96] = "-";
        if (a.leaves) {
            uint32_t g = 0, gn = 0;
            for (const auto& kv : a.grains)
                if (kv.second > gn) { g = kv.first; gn = kv.second; }
            _snprintf_s(split, sizeof split, _TRUNCATE, "leaves %.1f/tick, grain %u, leaf range %.1f", (double)a.leaves / tk, g,
                        (double)a.leafRange / (double)a.leaves);
        }
        JLOG("  %d %s | %.3f | %.1f | %.0f / %.0f / %.0f | %d | %s", rank, FnName((uint32_t)(jp.first >> 32), (uint32_t)jp.first).c_str(),
             a.sumUs / 1000.0 / tk, (double)a.n / tk, a.h.Pct(0.5), a.h.Pct(0.9), a.maxUs, nt, split);
    }
    g_win = Window();
    g_win.q0 = now;
    g_win.tick0 = tick1;
}

bool FindWatched() {
    using GetDescFn = HRESULT(WINAPI*)(HANDLE, PWSTR*);
    HMODULE k = GetModuleHandleW(L"kernelbase.dll");
    if (!k) k = GetModuleHandleW(L"kernel32.dll");
    auto gd = k ? reinterpret_cast<GetDescFn>(GetProcAddress(k, "GetThreadDescription")) : nullptr;
    if (!gd) return false;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    THREADENTRY32 te{};
    te.dwSize = sizeof te;
    const DWORD pid = GetCurrentProcessId();
    DWORD found = 0;
    for (BOOL ok = Thread32First(snap, &te); ok && !found; ok = Thread32Next(snap, &te)) {
        if (te.th32OwnerProcessID != pid) continue;
        HANDLE h = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, te.th32ThreadID);
        if (!h) continue;
        PWSTR d = nullptr;
        if (SUCCEEDED(gd(h, &d)) && d) {
            if (!wcscmp(d, g_watchName)) found = te.th32ThreadID;
            LocalFree(d);
        }
        CloseHandle(h);
    }
    CloseHandle(snap);
    if (!found) return false;
    jt_waitTid = found;
    for (LONG i = 0; i < std::min<LONG>((LONG)g_nThr, (LONG)kMaxThr); ++i)
        if (g_thrTid[i] == found) g_watchThr = (int)i;
    JLOG("watching %ls (tid %lu): its condition-variable waits, their wakers and every job of the KIDS pools are timed", g_watchName,
         (unsigned long)found);
    return true;
}

DWORD WINAPI ReportThread(LPVOID) {
    g_win.q0 = Qpc();
    g_win.tick0 = Tick();
    ULONGLONG lastFind = 0, lastLog = GetTickCount64();
    for (;;) {
        Sleep(200);
        const ULONGLONG ms = GetTickCount64();
        if (ms - lastFind >= 10000) {   // (re)find the watched thread: at start, and if it ever exits
            lastFind = ms;
            bool alive = false;
            if (const DWORD tid = jt_waitTid) {
                if (HANDLE h = OpenThread(SYNCHRONIZE, FALSE, tid)) {
                    alive = WaitForSingleObject(h, 0) == WAIT_TIMEOUT;
                    CloseHandle(h);
                }
            }
            if (!alive) {
                jt_waitTid = 0;
                FindWatched();
            }
        }
        if (g_watchThr < 0 && jt_waitTid)
            for (LONG i = 0; i < std::min<LONG>((LONG)g_nThr, (LONG)kMaxThr); ++i)
                if (g_thrTid[i] == jt_waitTid) g_watchThr = (int)i;
        __try {
            Drain();
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
        const int64_t now = Qpc();
        ProcessWaits(now, false);
        Prune(now);
        if (ms - lastLog >= (ULONGLONG)g_reportSec * 1000) {
            lastLog = ms;
            LogWindow();
        }
    }
#pragma warning(suppress : 4702)
    return 0;
}

void InitCommon() {
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    g_usPerTick = 1e6 / (double)f.QuadPart;
    g_jr = (JRec*)VirtualAlloc(nullptr, sizeof(JRec) * kJRing, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    g_wr = (WRec*)VirtualAlloc(nullptr, sizeof(WRec) * kWRing, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    g_kr = (KRec*)VirtualAlloc(nullptr, sizeof(KRec) * kKRing, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

}  // namespace

bool Prepare(const StartInfo& info, uint8_t* siteW, uint8_t* siteX, uint8_t* siteN1, uint8_t* siteN2) {
    g_log = info.log;
    g_base = info.base;
    if (!g_base || !siteW || !siteX || !siteN1 || !siteN2) return false;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(g_base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(g_base + dos->e_lfanew);
    g_exeLo = (uintptr_t)g_base;
    g_exeHi = g_exeLo + nt->OptionalHeader.SizeOfImage;
    auto inExe = [](const uint8_t* p) { return (uintptr_t)p >= g_exeLo && (uintptr_t)p + 8 <= g_exeHi; };
    // FF 15 disp32 (call [rip+d]) at siteW / siteX; 48 FF 25 disp32 (jmp [rip+d]) at siteN1 / siteN2
    uint8_t* sw = siteW + 6 + *reinterpret_cast<const int32_t*>(siteW + 2);
    uint8_t* sx = siteX + 6 + *reinterpret_cast<const int32_t*>(siteX + 2);
    uint8_t* s1 = siteN1 + 7 + *reinterpret_cast<const int32_t*>(siteN1 + 3);
    uint8_t* s2 = siteN2 + 7 + *reinterpret_cast<const int32_t*>(siteN2 + 3);
    if (siteW[0] != 0xFF || siteW[1] != 0x15 || siteX[0] != 0xFF || siteX[1] != 0x15 || siteN1[1] != 0xFF || siteN1[2] != 0x25 ||
        siteN2[1] != 0xFF || siteN2[2] != 0x25 || sw != sx || !inExe(sw) || !inExe(s1) || !inExe(s2) || s1 == s2 ||
        !*reinterpret_cast<void**>(sw) || !*reinterpret_cast<void**>(s1) || !*reinterpret_cast<void**>(s2)) {
        JLOG("%s", "IAT slots of the condition-variable sites look wrong - JT off");
        return false;
    }
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    const void* want[3] = {k32 ? (void*)GetProcAddress(k32, "SleepConditionVariableSRW") : nullptr,
                           k32 ? (void*)GetProcAddress(k32, "WakeConditionVariable") : nullptr,
                           k32 ? (void*)GetProcAddress(k32, "WakeAllConditionVariable") : nullptr};
    const void* have[3] = {*reinterpret_cast<void**>(sw), *reinterpret_cast<void**>(s1), *reinterpret_cast<void**>(s2)};
    for (int i = 0; i < 3; ++i)
        if (want[i] && have[i] != want[i])   // another hook in the chain is fine: we tail-jump through the slot at call time
            JLOG("note: IAT slot %d holds %p, kernel32 export is %p (hooked by another module?) - calls still go through the slot", i,
                 have[i], want[i]);
    jt_iatSleepSrw = (uint64_t)(uintptr_t)sw;
    jt_iatWake = (uint64_t)(uintptr_t)s1;
    jt_iatWakeAll = (uint64_t)(uintptr_t)s2;
    const wchar_t* ini = info.iniPath ? info.iniPath : L"";
    GetPrivateProfileStringW(L"Profiler", L"JT_Thread", L"KIDSTaskScheduler0", g_watchName, (DWORD)(sizeof g_watchName / sizeof(wchar_t)), ini);
    const int rs = (int)GetPrivateProfileIntW(L"Profiler", L"ReportSec", 30, ini);
    g_reportSec = std::clamp((int)GetPrivateProfileIntW(L"Profiler", L"JT_ReportSec", rs, ini), 5, 600);
    g_top = std::clamp((int)GetPrivateProfileIntW(L"Profiler", L"JT_Top", 6, ini), 1, 20);
    InitCommon();
    if (!g_jr || !g_wr || !g_kr) {
        JLOG("%s", "ring allocation failed - JT off");
        return false;
    }
    return true;
}

void Start() {
    if (!g_jr || InterlockedExchange(&g_started, 1)) return;
    LARGE_INTEGER a, b;
    QueryPerformanceCounter(&a);
    for (int i = 0; i < 1000; ++i) Qpc();
    QueryPerformanceCounter(&b);
    JLOG("job-timing probe on: every JobWorkerPoll job (40E4E) timed with 2 QPC reads (%.0f ns each), CV waits of %ls and their "
         "wakers recorded; report every %d s, top %d (notes/gq-jt-probes.md)",
         (double)(b.QuadPart - a.QuadPart) * g_usPerTick, g_watchName, g_reportSec, g_top);
    if (HANDLE h = CreateThread(nullptr, 0, ReportThread, nullptr, 0, nullptr)) CloseHandle(h);
}

void TestSetup(void (*log)(const char*, ...), uintptr_t exeLo, uintptr_t exeHi, uint8_t* tickBase, void** iatSleep, void** iatWake,
               void** iatWakeAll, const wchar_t* watched, uint32_t kindNode, uint32_t kindPfor) {
    g_log = log;
    g_exeLo = exeLo;
    g_exeHi = exeHi;
    g_base = tickBase;
    g_crtFilter = false;
    g_kindNode = kindNode;
    g_kindPfor = kindPfor;
    g_kindRange = 0xFFFFFFFF;
    jt_iatSleepSrw = (uint64_t)(uintptr_t)iatSleep;
    jt_iatWake = (uint64_t)(uintptr_t)iatWake;
    jt_iatWakeAll = (uint64_t)(uintptr_t)iatWakeAll;
    wcsncpy_s(g_watchName, watched, _TRUNCATE);
    InitCommon();
    g_win.q0 = Qpc();
    g_win.tick0 = Tick();
}

void TestReport() {
    if (!jt_waitTid) FindWatched();
    if (g_watchThr < 0)
        for (LONG i = 0; i < std::min<LONG>((LONG)g_nThr, (LONG)kMaxThr); ++i)
            if (g_thrTid[i] == jt_waitTid) g_watchThr = (int)i;
    Drain();
    ProcessWaits(Qpc(), true);
    LogWindow();
}

}  // namespace jt
