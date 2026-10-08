// loadstats.cpp - LoadStats: per-level-load measurement ([Profiler] LoadStats = 0 by default). See loadstats.h and
// notes\loading.md. Measurement only: every hook calls the original with the original arguments and returns its result.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "loadstats.h"

namespace loadstats {
namespace {

// ---- v2.02 addresses (RVAs). Sources: notes\loading.md §2, re\symbols\incoming\loading.tsv ----
constexpr uint32_t kGameMainPtr = 0x4759910;   // g_GameMainPtr
constexpr uint32_t kGmStopFlag58A = 0x58A;     // level-load window byte (Pause::SetFlag index 2)
constexpr uint32_t kGmFrameCounter = 0x610;    // +1 per rendered frame
constexpr uint32_t kGmLoaderCap1 = 0x4D;       // boot flag: loader in-flight cap 1 when set, else 10 (1FEE80 / 1FF3A8)
constexpr uint32_t kStagePtr = 0x45B9E58;      // stage/scene system; +760h == 3 -> Load::IsStageLoadEnd (A40A30)
constexpr uint32_t kStageState = 0x760;
constexpr uint32_t kReadyWaitAcc = 0x45C84A4;  // float: LoadIsLoadEnd 3B3424 ready-wait accumulator (> 120 -> timeout)
constexpr uint32_t kEventScenePtr = 0x45CE800; // IsEventScene DA7C0: obj && (state 8 || 11..16) && [obj+1Ch]
constexpr uint32_t kIatSleep = 0x38E2338;
constexpr uint32_t kIatReadFile = 0x38E23B8;

// Loader managers: A = ResourceLoadIdlePoll 1FEE28 (job 1343720), B = 1FF320 (job 13438B0). Both keep the manager in r14.
struct MgrLayout { uint32_t pend0, pend1, actBeg, actEnd; };
constexpr MgrLayout kMgr[2] = {{0x200, 0x210, 0x230, 0x238}, {0x1E0, 0x1F0, 0x218, 0x220}};

enum Kind : uint8_t { kMgrSleep, kInflate, kPso, kRead };
struct Site {
    uint32_t rva;
    Kind kind;
    uint32_t expect;  // E8: callee RVA; FF 15: IAT slot RVA
    uint8_t arg;      // kMgrSleep: 0 A idle, 1 A no-progress, 2 B idle; kInflate: wrapper 0..2; kPso: 0 gfx rt, 1 gfx preload,
                      // 2 compute rt, 3 compute preload; kRead: 0 archive layer, 1 other
};
constexpr Site kSites[] = {
    {0x1FEE68, kMgrSleep, kIatSleep, 0},   {0x1FF18C, kMgrSleep, kIatSleep, 1},   {0x1FF371, kMgrSleep, kIatSleep, 2},
    {0x258A5A, kInflate, 0x420654, 0},     {0x4201F5, kInflate, 0x420654, 0},     {0x420605, kInflate, 0x420654, 0},
    {0x324BD61, kInflate, 0x324D064, 1},   {0x324C409, kInflate, 0x324D064, 1},   {0xA099DD, kInflate, 0xA09A40, 2},
    {0x324C372, kInflate, 0xA09A40, 2},    {0x324C4A1, kInflate, 0xA09A40, 2},
    {0x11970A, kPso, 0x9290D8, 0},         {0x2EFE47C, kPso, 0x9290D8, 1},        {0x119AE1, kPso, 0x58C2B0, 2},
    {0x119D86, kPso, 0x58C2B0, 2},         {0x58BF92, kPso, 0x58C2B0, 2},         {0x58C07A, kPso, 0x58C2B0, 2},
    {0x2EFE1F2, kPso, 0x58C2B0, 3},
    {0x311D17, kRead, kIatReadFile, 0},    {0x312DD2, kRead, kIatReadFile, 0},    {0x31305A, kRead, kIatReadFile, 0},
    {0x3130D6, kRead, kIatReadFile, 0},    {0x740392, kRead, kIatReadFile, 1},    {0x1FD9CA3, kRead, kIatReadFile, 1},
    {0x32562BE, kRead, kIatReadFile, 1},
};
constexpr uint32_t kInflTargets[3] = {0x420654, 0x324D064, 0xA09A40};
constexpr uint32_t kPsoTargets[2] = {0x9290D8, 0x58C2B0};

void (*g_log)(const char*, ...) = nullptr;
uintptr_t g_base = 0;
int g_mode = 0;
bool g_on = false;
double g_qpcMs = 0;  // ms per QPC tick

// ---- counters (monotonic since start; windows use snapshots) ----
struct Counters {
    std::atomic<uint64_t> readCalls[2], readBytes[2], readTicks[2], readAsync;
    std::atomic<uint64_t> inflChunks, inflIn, inflOut, inflTicks, inflFail;
    std::atomic<uint64_t> psoN[4], psoTicks[4], psoMaxTicks;
    std::atomic<uint64_t> mgrSleep[3];
};
Counters g_c;
struct Snap {
    uint64_t readCalls[2], readBytes[2], readTicks[2], readAsync;
    uint64_t inflChunks, inflIn, inflOut, inflTicks, inflFail;
    uint64_t psoN[4], psoTicks[4];
    uint64_t mgrSleep[3];
    uint64_t cpu100ns;  // process kernel + user
    int frames;
    uint64_t qpc;
};
std::atomic<int> g_inflNow{0}, g_inflMax{0};
std::atomic<uintptr_t> g_mgr[2];
constexpr int kTidSlots = 128;
std::atomic<DWORD> g_tids[kTidSlots];
std::atomic<int> g_nTids{0};

uint64_t Qpc() {
    LARGE_INTEGER q;
    QueryPerformanceCounter(&q);
    return (uint64_t)q.QuadPart;
}
void AtomicMax(std::atomic<uint64_t>& a, uint64_t v) {
    uint64_t c = a.load(std::memory_order_relaxed);
    while (v > c && !a.compare_exchange_weak(c, v, std::memory_order_relaxed)) {}
}
void AtomicMaxI(std::atomic<int>& a, int v) {
    int c = a.load(std::memory_order_relaxed);
    while (v > c && !a.compare_exchange_weak(c, v, std::memory_order_relaxed)) {}
}
void NoteThread() {
    const DWORD t = GetCurrentThreadId();
    for (int i = 0, h = (int)((t * 2654435761u) >> 25) & (kTidSlots - 1); i < 8; ++i, h = (h + 1) & (kTidSlots - 1)) {
        DWORD cur = g_tids[h].load(std::memory_order_relaxed);
        if (cur == t) return;
        if (cur == 0) {
            if (g_tids[h].compare_exchange_strong(cur, t)) { g_nTids.fetch_add(1, std::memory_order_relaxed); return; }
            if (cur == t) return;
        }
    }
}

// ---- hooks (same arguments in, same result out) ----
using InflFn = uint64_t (*)(void*, uint32_t, uint64_t*, const void*, uint32_t);
template <int W>
uint64_t HkInflate(void* dst, uint32_t cap, uint64_t* outLen, const void* src, uint32_t csize) {
    NoteThread();
    AtomicMaxI(g_inflMax, g_inflNow.fetch_add(1, std::memory_order_relaxed) + 1);
    const uint64_t t0 = Qpc();
    const uint64_t r = reinterpret_cast<InflFn>(g_base + kInflTargets[W])(dst, cap, outLen, src, csize);
    const uint64_t dt = Qpc() - t0;
    g_inflNow.fetch_sub(1, std::memory_order_relaxed);
    g_c.inflChunks.fetch_add(1, std::memory_order_relaxed);
    g_c.inflTicks.fetch_add(dt, std::memory_order_relaxed);
    g_c.inflIn.fetch_add(csize, std::memory_order_relaxed);
    if (r & 0xFF) {
        if (outLen) g_c.inflOut.fetch_add(*outLen, std::memory_order_relaxed);
    } else {
        g_c.inflFail.fetch_add(1, std::memory_order_relaxed);
    }
    return r;
}

using PsoFn = void* (*)(void*, void*, void*, void*);
template <int K>  // 0 gfx runtime, 1 gfx preload, 2 compute runtime, 3 compute preload
void* HkPso(void* a, void* b, void* c, void* d) {
    const uint64_t t0 = Qpc();
    void* r = reinterpret_cast<PsoFn>(g_base + kPsoTargets[K / 2])(a, b, c, d);
    const uint64_t dt = Qpc() - t0;
    g_c.psoN[K].fetch_add(1, std::memory_order_relaxed);
    g_c.psoTicks[K].fetch_add(dt, std::memory_order_relaxed);
    AtomicMax(g_c.psoMaxTicks, dt);
    return r;
}

using ReadFn = BOOL(WINAPI*)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
template <int C>
BOOL WINAPI HkRead(HANDLE h, LPVOID buf, DWORD n, LPDWORD got, LPOVERLAPPED ov) {
    const ReadFn real = *reinterpret_cast<ReadFn*>(g_base + kIatReadFile);
    const uint64_t t0 = Qpc();
    const BOOL r = real(h, buf, n, got, ov);
    const uint64_t dt = Qpc() - t0;
    g_c.readCalls[C].fetch_add(1, std::memory_order_relaxed);
    g_c.readBytes[C].fetch_add(n, std::memory_order_relaxed);
    g_c.readTicks[C].fetch_add(dt, std::memory_order_relaxed);
    if (ov) g_c.readAsync.fetch_add(n, std::memory_order_relaxed);
    return r;
}

// Reached from the Sleep thunk: rcx = ms (unchanged), rdx = r14 (the manager), r8d = site id.
void MgrSleep(DWORD ms, void* mgr, uint32_t id) {
    g_mgr[id < 2 ? 0 : 1].store((uintptr_t)mgr, std::memory_order_relaxed);
    g_c.mgrSleep[id].fetch_add(1, std::memory_order_relaxed);
    (*reinterpret_cast<decltype(&::Sleep)*>(g_base + kIatSleep))(ms);
}

// ---- patching ----
uint8_t* g_thunks = nullptr;
size_t g_thunkUsed = 0;
constexpr size_t kThunkPage = 4096;

// First free, granularity-aligned hole in [from, to) (VirtualQuery walk).
uint8_t* AllocIn(uintptr_t from, uintptr_t to) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    const uintptr_t g = si.dwAllocationGranularity;
    MEMORY_BASIC_INFORMATION mbi;
    for (uintptr_t a = (from + g - 1) & ~(g - 1); a < to;) {
        if (!VirtualQuery((void*)a, &mbi, sizeof(mbi))) break;
        const uintptr_t rb = (uintptr_t)mbi.BaseAddress, re = rb + mbi.RegionSize;
        if (mbi.State == MEM_FREE) {
            const uintptr_t c = (rb + g - 1) & ~(g - 1);
            if (c >= from && c + kThunkPage <= re && c + kThunkPage <= to)
                if (void* p = VirtualAlloc((void*)c, kThunkPage, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE))
                    return (uint8_t*)p;
        }
        if (re <= a) break;
        a = re;
    }
    return nullptr;
}

// A page every rel32 from inside [lo, hi) can reach: above the image first (closest), else below it.
uint8_t* AllocNear(uintptr_t lo, uintptr_t hi) {
    const uintptr_t reach = 0x7FF00000ull - (hi - lo);
    if (uint8_t* p = AllocIn(hi, hi + reach)) return p;
    return AllocIn(lo > reach + 0x10000 ? lo - reach : 0x10000, lo);
}

uint8_t* NewThunk(const uint8_t* pre, size_t preLen, const void* target) {
    const size_t need = (preLen + 14 + 15) & ~size_t(15);
    if (g_thunkUsed + need > kThunkPage) return nullptr;
    uint8_t* t = g_thunks + g_thunkUsed;
    if (preLen) memcpy(t, pre, preLen);
    static const uint8_t jmp[6] = {0xFF, 0x25, 0, 0, 0, 0};  // jmp qword [rip+0]
    memcpy(t + preLen, jmp, 6);
    memcpy(t + preLen + 6, &target, 8);
    g_thunkUsed += need;
    return t;
}

// Writes len bytes at p with ONE interlocked 8- or 16-byte compare-exchange; false (nothing written) otherwise.
bool PatchAtomic(uint8_t* p, const uint8_t* bytes, size_t len) {
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

// Fallback for an instruction that straddles a 16-byte boundary: (1) atomically turn its first two bytes into
// "jmp $" (EB FE), so a thread arriving now spins on it, (2) write the tail, (3) atomically write the final first two
// bytes. A thread that already decoded the old instruction runs the old, complete instruction once.
bool PatchSpin(uint8_t* p, const uint8_t* bytes, size_t len) {
    static const uint8_t spin[2] = {0xEB, 0xFE};
    uint8_t orig[2] = {p[0], p[1]};
    if (len < 3 || !PatchAtomic(p, spin, 2)) return false;
    DWORD old = 0;
    if (!VirtualProtect(p + 2, len - 2, PAGE_EXECUTE_READWRITE, &old)) {
        PatchAtomic(p, orig, 2);
        return false;
    }
    memcpy(p + 2, bytes + 2, len - 2);
    VirtualProtect(p + 2, len - 2, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, len);
    return PatchAtomic(p, bytes, 2);
}

bool Patch(uint8_t* p, const uint8_t* bytes, size_t len) { return PatchAtomic(p, bytes, len) || PatchSpin(p, bytes, len); }

bool RelFits(const uint8_t* from, const uint8_t* to) {
    const intptr_t d = (intptr_t)to - (intptr_t)from;
    return d >= INT32_MIN && d <= INT32_MAX;
}

const void* HookFor(const Site& s) {
    switch (s.kind) {
        case kInflate:
            return s.arg == 0 ? (const void*)&HkInflate<0> : s.arg == 1 ? (const void*)&HkInflate<1> : (const void*)&HkInflate<2>;
        case kPso:
            switch (s.arg) {
                case 0: return (const void*)&HkPso<0>;
                case 1: return (const void*)&HkPso<1>;
                case 2: return (const void*)&HkPso<2>;
                default: return (const void*)&HkPso<3>;
            }
        case kRead: return s.arg == 0 ? (const void*)&HkRead<0> : (const void*)&HkRead<1>;
        default: return (const void*)&MgrSleep;
    }
}

// Verifies one site and installs it. Returns 0 ok, 1 bytes differ (other patch / other build), 2 no room / not atomic.
int InstallSite(const Site& s) {
    uint8_t* p = (uint8_t*)(g_base + s.rva);
    const bool isCall = s.kind == kInflate || s.kind == kPso;
    if (isCall) {
        if (p[0] != 0xE8) return 1;
        int32_t rel;
        memcpy(&rel, p + 1, 4);
        if ((uintptr_t)(p + 5 + rel) != g_base + s.expect) return 1;
        uint8_t* t = NewThunk(nullptr, 0, HookFor(s));
        if (!t || !RelFits(p + 5, t)) return 2;
        const int32_t nrel = (int32_t)((intptr_t)t - (intptr_t)(p + 5));
        uint8_t nb[5] = {0xE8, 0, 0, 0, 0};
        memcpy(nb + 1, &nrel, 4);
        return Patch(p, nb, 5) ? 0 : 2;
    }
    if (p[0] != 0xFF || p[1] != 0x15) return 1;
    int32_t rel;
    memcpy(&rel, p + 2, 4);
    if ((uintptr_t)(p + 6 + rel) != g_base + s.expect) return 1;
    uint8_t pre[9];
    size_t preLen = 0;
    if (s.kind == kMgrSleep) {
        const uint8_t movRdxR14[3] = {0x49, 0x8B, 0xD6};
        memcpy(pre, movRdxR14, 3);
        pre[3] = 0x41;  // mov r8d, imm32
        pre[4] = 0xB8;
        const uint32_t id = s.arg;
        memcpy(pre + 5, &id, 4);
        preLen = 9;
    }
    uint8_t* t = NewThunk(pre, preLen, HookFor(s));
    if (!t || !RelFits(p + 5, t)) return 2;
    uint8_t nb[6] = {0xE8, 0, 0, 0, 0, 0x90};  // call thunk; nop  (the call returns to the nop, then p+6 as before)
    const int32_t nrel = (int32_t)((intptr_t)t - (intptr_t)(p + 5));
    memcpy(nb + 1, &nrel, 4);
    return Patch(p, nb, 6) ? 0 : 2;
}

// ---- safe reads of game memory from the sampler thread ----
bool SafeCopy(void* dst, const void* src, size_t n) {
    __try {
        memcpy(dst, src, n);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
template <class T>
bool Rd(uintptr_t a, T& v) {
    return a > 0x10000 && SafeCopy(&v, (const void*)a, sizeof(T));
}

uint64_t ProcCpu() {
    FILETIME c, e, k, u;
    if (!GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u)) return 0;
    return ((uint64_t)k.dwHighDateTime << 32 | k.dwLowDateTime) + ((uint64_t)u.dwHighDateTime << 32 | u.dwLowDateTime);
}

Snap TakeSnap(int frames) {
    Snap s{};
    for (int i = 0; i < 2; ++i) {
        s.readCalls[i] = g_c.readCalls[i].load();
        s.readBytes[i] = g_c.readBytes[i].load();
        s.readTicks[i] = g_c.readTicks[i].load();
    }
    s.readAsync = g_c.readAsync.load();
    s.inflChunks = g_c.inflChunks.load();
    s.inflIn = g_c.inflIn.load();
    s.inflOut = g_c.inflOut.load();
    s.inflTicks = g_c.inflTicks.load();
    s.inflFail = g_c.inflFail.load();
    for (int i = 0; i < 4; ++i) {
        s.psoN[i] = g_c.psoN[i].load();
        s.psoTicks[i] = g_c.psoTicks[i].load();
    }
    for (int i = 0; i < 3; ++i) s.mgrSleep[i] = g_c.mgrSleep[i].load();
    s.cpu100ns = ProcCpu();
    s.frames = frames;
    s.qpc = Qpc();
    return s;
}

constexpr double kMB = 1024.0 * 1024.0;

// "<label> 38.2 s: frames 812 (47.0 ms) | CPU 6.9 cores | read 700 MB (sync 120 ms) | inflate 3.50 s, 92000 ch, 640->1500 MB
//  | PSO g 780/12 c 90/3 in 9.80 s"
void LogSpan(int n, const char* label, const Snap& a, const Snap& b) {
    const double sec = (double)(b.qpc - a.qpc) * g_qpcMs / 1000.0;
    if (sec <= 0) return;
    const int fr = b.frames - a.frames;
    const double rd = (double)(b.readBytes[0] - a.readBytes[0] + b.readBytes[1] - a.readBytes[1]) / kMB;
    const double rdSyncMs = (double)(b.readTicks[0] - a.readTicks[0] + b.readTicks[1] - a.readTicks[1]) * g_qpcMs;
    const double inflS = (double)(b.inflTicks - a.inflTicks) * g_qpcMs / 1000.0;
    uint64_t pn = 0, pt = 0;
    for (int i = 0; i < 4; ++i) {
        pn += b.psoN[i] - a.psoN[i];
        pt += b.psoTicks[i] - a.psoTicks[i];
    }
    g_log("LoadStats #%d %s %.1f s: frames %d (%.1f ms) | CPU %.2f cores | read %.0f MB (in-call %.0f ms) | inflate %.2f s, "
          "%llu ch, %.0f->%.0f MB | PSO %llu (gfx %llu+%llu pre, cmp %llu+%llu pre) in %.2f s",
          n, label, sec, fr, fr > 0 ? sec * 1000.0 / fr : 0.0, (double)(b.cpu100ns - a.cpu100ns) / 1e7 / sec, rd, rdSyncMs,
          inflS, (unsigned long long)(b.inflChunks - a.inflChunks), (double)(b.inflIn - a.inflIn) / kMB,
          (double)(b.inflOut - a.inflOut) / kMB, (unsigned long long)pn, (unsigned long long)(b.psoN[0] - a.psoN[0]),
          (unsigned long long)(b.psoN[1] - a.psoN[1]), (unsigned long long)(b.psoN[2] - a.psoN[2]),
          (unsigned long long)(b.psoN[3] - a.psoN[3]), (double)pt * g_qpcMs / 1000.0);
}

// ---- window state (sampler thread only, except the status line) ----
struct Window {
    bool open = false;
    int n = 0;
    Snap s0{}, sStage{}, sLastTick{};
    bool haveStage = false;
    double tStage = -1, tReady0 = -1, tReady1 = -1, tLastBusy = -1, tFirstBusy = -1;
    float accMax = 0;
    uint64_t samples = 0, evSamples = 0, busy[2] = {0, 0}, actSum[2] = {0, 0}, atCap[2] = {0, 0}, pendSum[2] = {0, 0};
    int cap = 10;
};
Window g_w;
std::atomic<int> g_statusLoad{0};
std::atomic<int> g_statusAct{0};
std::atomic<uint64_t> g_statusT0{0};

int ReadFrames(uintptr_t gm) {
    int f = 0;
    Rd(gm + kGmFrameCounter, f);
    return f;
}

void OpenWindow(uintptr_t gm) {
    g_w = Window{};
    g_w.open = true;
    static int s_n = 0;
    g_w.n = ++s_n;
    g_inflMax.store(g_inflNow.load());
    uint8_t c1 = 0;
    Rd(gm + kGmLoaderCap1, c1);
    g_w.cap = c1 ? 1 : 10;
    g_w.s0 = TakeSnap(ReadFrames(gm));
    g_w.sLastTick = g_w.s0;
    g_statusT0.store(g_w.s0.qpc);
    g_statusLoad.store(1);
    g_log("LoadStats #%d: 58A set (loader in-flight cap %d, GameMain+4Dh = %u)", g_w.n, g_w.cap, (unsigned)c1);
}

void CloseWindow(uintptr_t gm) {
    const Snap e = TakeSnap(ReadFrames(gm));
    const Window& w = g_w;
    const double sec = (double)(e.qpc - w.s0.qpc) * g_qpcMs / 1000.0;
    const int fr = e.frames - w.s0.frames;
    const double ss = w.samples ? 1.0 / (double)w.samples : 0.0;
    auto busyS = [&](int m) { return (double)w.busy[m] * 0.01; };
    auto actAvg = [&](int m) { return w.busy[m] ? (double)w.actSum[m] / (double)w.busy[m] : 0.0; };
    auto capPct = [&](int m) { return w.busy[m] ? 100.0 * (double)w.atCap[m] / (double)w.busy[m] : 0.0; };
    g_log("LoadStats #%d: %.1f s | frames %d (%.1f ms) | CPU %.2f cores | stage ready %+.1f s | loaders busy %.1f/%.1f s "
          "(first %+.1f, last %+.1f), in-flight avg A %.1f B %.1f of %d (at cap A %.0f %% B %.0f %%), pending avg A %.1f B %.1f"
          " | ready-wait %+.1f..%+.1f s (acc max %.1f%s) | event scene %.1f s",
          w.n, sec, fr, fr > 0 ? sec * 1000.0 / fr : 0.0, (double)(e.cpu100ns - w.s0.cpu100ns) / 1e7 / (sec > 0 ? sec : 1),
          w.tStage, busyS(0), busyS(1), w.tFirstBusy, w.tLastBusy, actAvg(0), actAvg(1), w.cap, capPct(0), capPct(1),
          (double)w.pendSum[0] * ss, (double)w.pendSum[1] * ss, w.tReady0, w.tReady1, (double)w.accMax,
          w.accMax > 120.0f ? " = TIMEOUT" : "", (double)w.evSamples * 0.01);
    const double rdA = (double)(e.readBytes[0] - w.s0.readBytes[0]) / kMB, rdO = (double)(e.readBytes[1] - w.s0.readBytes[1]) / kMB;
    const double inflS = (double)(e.inflTicks - w.s0.inflTicks) * g_qpcMs / 1000.0;
    g_log("LoadStats #%d io: read %.0f MB archive (%llu calls, %.0f MB overlapped, %.0f ms in-call) + %.0f MB other (%llu calls, "
          "%.0f ms) | inflate %.2f CPU-s, %llu chunks, %.0f -> %.0f MB, %llu failed, max %d parallel, %d threads so far | "
          "PSO gfx %llu rt + %llu preload, cmp %llu rt + %llu preload, %.2f CPU-s, max %.1f ms | mgr Sleep(1) A idle %llu, "
          "A no-progress %llu, B idle %llu",
          w.n, rdA, (unsigned long long)(e.readCalls[0] - w.s0.readCalls[0]), (double)(e.readAsync - w.s0.readAsync) / kMB,
          (double)(e.readTicks[0] - w.s0.readTicks[0]) * g_qpcMs, rdO,
          (unsigned long long)(e.readCalls[1] - w.s0.readCalls[1]), (double)(e.readTicks[1] - w.s0.readTicks[1]) * g_qpcMs,
          inflS, (unsigned long long)(e.inflChunks - w.s0.inflChunks), (double)(e.inflIn - w.s0.inflIn) / kMB,
          (double)(e.inflOut - w.s0.inflOut) / kMB, (unsigned long long)(e.inflFail - w.s0.inflFail), g_inflMax.load(),
          g_nTids.load(), (unsigned long long)(e.psoN[0] - w.s0.psoN[0]), (unsigned long long)(e.psoN[1] - w.s0.psoN[1]),
          (unsigned long long)(e.psoN[2] - w.s0.psoN[2]), (unsigned long long)(e.psoN[3] - w.s0.psoN[3]),
          (double)(e.psoTicks[0] + e.psoTicks[1] + e.psoTicks[2] + e.psoTicks[3] - w.s0.psoTicks[0] - w.s0.psoTicks[1] -
                   w.s0.psoTicks[2] - w.s0.psoTicks[3]) * g_qpcMs / 1000.0,
          (double)g_c.psoMaxTicks.exchange(0) * g_qpcMs, (unsigned long long)(e.mgrSleep[0] - w.s0.mgrSleep[0]),
          (unsigned long long)(e.mgrSleep[1] - w.s0.mgrSleep[1]), (unsigned long long)(e.mgrSleep[2] - w.s0.mgrSleep[2]));
    if (w.haveStage) {
        LogSpan(w.n, "split [set->stage ready]", w.s0, w.sStage);
        LogSpan(w.n, "split [stage ready->clear]", w.sStage, e);
    }
    g_statusLoad.store(0);
    g_w.open = false;
}

void SampleMgr(int m, double t) {
    const uintptr_t mg = g_mgr[m].load(std::memory_order_relaxed);
    if (!mg) return;
    uint64_t p0 = 0, p1 = 0, b = 0, e = 0;
    if (!Rd(mg + kMgr[m].pend0, p0) || !Rd(mg + kMgr[m].pend1, p1) || !Rd(mg + kMgr[m].actBeg, b) ||
        !Rd(mg + kMgr[m].actEnd, e))
        return;
    const uint64_t act = e >= b ? (e - b) >> 4 : 0;
    if (p0 > (1u << 24) || p1 > (1u << 24) || act > 4096) return;  // implausible -> layout differs, ignore the sample
    g_w.pendSum[m] += p0 + p1;
    if (p0 + p1 + act) {
        ++g_w.busy[m];
        g_w.actSum[m] += act;
        if ((int)act >= g_w.cap) ++g_w.atCap[m];
        if (g_w.tFirstBusy < 0) g_w.tFirstBusy = t;
        g_w.tLastBusy = t;
    }
    if (m == 0) g_statusAct.store((int)act);
}

DWORD WINAPI Sampler(void*) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);  // keep the 10 ms cadence under load
    for (;;) {
        Sleep(10);
        uintptr_t gm = 0;
        if (!Rd(g_base + kGameMainPtr, gm) || !gm) continue;
        uint8_t f = 0;
        if (!Rd(gm + kGmStopFlag58A, f)) continue;
        if (!g_w.open) {
            if (f) OpenWindow(gm);
            continue;
        }
        if (!f) {
            CloseWindow(gm);
            continue;
        }
        const double t = (double)(Qpc() - g_w.s0.qpc) * g_qpcMs / 1000.0;
        ++g_w.samples;
        uintptr_t st = 0;
        int ss = -1;
        if (Rd(g_base + kStagePtr, st) && st) Rd(st + kStageState, ss);
        if (!g_w.haveStage && ss == 3) {
            g_w.haveStage = true;
            g_w.tStage = t;
            g_w.sStage = TakeSnap(ReadFrames(gm));
        }
        float acc = 0;
        if (Rd(g_base + kReadyWaitAcc, acc) && acc > 0.0f && acc < 1e6f) {
            if (g_w.tReady0 < 0) g_w.tReady0 = t;
            g_w.tReady1 = t;
            if (acc > g_w.accMax) g_w.accMax = acc;
        }
        uintptr_t ev = 0;
        int evState = 0, evOn = 0;
        if (Rd(g_base + kEventScenePtr, ev) && ev && Rd(ev, evState) && Rd(ev + 0x1C, evOn) && evOn &&
            (evState == 8 || (unsigned)(evState - 11) <= 5))
            ++g_w.evSamples;
        SampleMgr(0, t);
        SampleMgr(1, t);
        if (g_mode >= 2 && g_w.samples % 100 == 0) {
            const Snap s = TakeSnap(ReadFrames(gm));
            const Snap& a = g_w.sLastTick;
            g_log("LoadStats tick #%d %+.0f s: frames %d, CPU %.2f cores, A act %d, read %.1f MB, inflate %.0f ms, PSO %llu, "
                  "stage %d, acc %.1f",
                  g_w.n, t, s.frames - a.frames, (double)(s.cpu100ns - a.cpu100ns) / 1e7 / ((double)(s.qpc - a.qpc) * g_qpcMs / 1000.0),
                  g_statusAct.load(), (double)(s.readBytes[0] + s.readBytes[1] - a.readBytes[0] - a.readBytes[1]) / kMB,
                  (double)(s.inflTicks - a.inflTicks) * g_qpcMs,
                  (unsigned long long)(s.psoN[0] + s.psoN[1] + s.psoN[2] + s.psoN[3] - a.psoN[0] - a.psoN[1] - a.psoN[2] - a.psoN[3]),
                  ss, (double)acc);
            g_w.sLastTick = s;
        }
    }
}

}  // namespace

bool Start(const StartInfo& info) {
    g_log = info.log;
    g_base = info.base;
    g_mode = (int)GetPrivateProfileIntW(L"Profiler", L"LoadStats", 0, info.iniPath);
    if (g_mode <= 0 || !g_log || !g_base) return false;
    if (!info.knownBuild) {
        g_log("LoadStats: refused (unknown build; addresses are v2.02)");
        return false;
    }
    LARGE_INTEGER fq;
    QueryPerformanceFrequency(&fq);
    g_qpcMs = 1000.0 / (double)fq.QuadPart;
    const auto* dos = (const IMAGE_DOS_HEADER*)g_base;
    const auto* nt = (const IMAGE_NT_HEADERS64*)(g_base + dos->e_lfanew);
    g_thunks = AllocNear(g_base, g_base + nt->OptionalHeader.SizeOfImage);
    if (!g_thunks) {
        g_log("LoadStats: no executable page within 2 GB of the image - hooks off, polling only");
    }
    int ok = 0, differ = 0, skipped = 0;
    char bad[512] = {0};
    size_t bl = 0;
    for (const Site& s : kSites) {
        const int r = g_thunks ? InstallSite(s) : 2;
        if (r == 0) {
            ++ok;
            continue;
        }
        (r == 1 ? differ : skipped)++;
        if (bl < sizeof(bad) - 16) bl += (size_t)snprintf(bad + bl, sizeof(bad) - bl, " %X%s", s.rva, r == 1 ? "(bytes)" : "(atomic)");
    }
    HANDLE th = CreateThread(nullptr, 0, &Sampler, nullptr, 0, nullptr);
    if (th) CloseHandle(th);
    g_on = th != nullptr;
    g_log("LoadStats: mode %d, %d/%d sites hooked, %d differ, %d skipped%s%s", g_mode, ok, (int)(sizeof(kSites) / sizeof(kSites[0])),
          differ, skipped, bl ? " -" : "", bad);
    return g_on;
}

bool Enabled() { return g_on; }

const char* StatusLine(char* buf, unsigned n) {
    if (!buf || !n) return "";
    buf[0] = 0;
    if (!g_on) return buf;
    if (!g_statusLoad.load()) {
        snprintf(buf, n, "LS: idle");
        return buf;
    }
    const double t = (double)(Qpc() - g_statusT0.load()) * g_qpcMs / 1000.0;
    snprintf(buf, n, "LS: load %.1f s, A %d, %.0f MB, PSO %llu", t, g_statusAct.load(),
             (double)(g_c.readBytes[0].load() + g_c.readBytes[1].load()) / kMB,
             (unsigned long long)(g_c.psoN[0].load() + g_c.psoN[1].load() + g_c.psoN[2].load() + g_c.psoN[3].load()));
    return buf;
}

}  // namespace loadstats
