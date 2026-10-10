// frameoverlap.cpp - FO frame overlap (see frameoverlap.h, notes\renderer\frame-overlap.md section 5).
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include "frameoverlap.h"

namespace frameoverlap {

namespace {

// ---- GfxDevice layout (backend.md, frame-overlap.md section 1) ----------------------------------------------------------
constexpr uintptr_t kCtx = 0x10, kCtxListPtr = 0x590, kListPool = 0x598, kListRing = 0x5B0, kPipeStatsN = 0x10 + 0x24D4;
constexpr uintptr_t kRing = 0x3BE0, kSyncN = 0x3C18, kSyncFrame = 0x3C1C, kSyncTab = 0x3C20, kStreamSize = 0x3C28,
                    kReserve = 0x3C2C, kBase = 0x3C30, kEnd = 0x3C38, kWrite = 0x3C40, kLastKick = 0x3C48, kHigh = 0x3C50,
                    kGen = 0x3C60, kLastSubmit = 0x3C68, kSinceFlush = 0x3C74, kStats = 0x3C78, kBytes = 0x3C84,
                    kSnap = 0x3D00, kBytesMax = 0x3D88, kSubCount = 0x3E28, kPresentThr = 0x3F80, kTsPer = 0x4034,
                    kPresentParams = 0x4220, kSlotCount = 0x4240, kSyncInterval = 0x424C, kDevice = 0x4268, kQueue = 0x4270,
                    kSwap = 0x4278, kAlloc = 0x4290, kFenceEv = 0x4318, kSlotFence = 0x4320, kBB = 0x4698, kOffscreen = 0x4718,
                    kSlot = 0x4730, kTsHeap = 0x4768, kTsReadback = 0x4770, kTsCopy = 0x4778, kTsCalib = 0x4780,
                    kTsFirst = 0x4788, kTsLast = 0x478C, kTsBits = 0x4790, kSubArr = 0x74D8, kPool = 0x61A0,
                    kPresentTime = 0x7708, kPresentEv = 0x7720, kPresentPending = 0x7728, kPresentSc = 0x7730,
                    kPresentA = 0x7738, kPresentB = 0x773C;
constexpr uintptr_t kRendererNodeIdx = 0x1EE568;
constexpr uint32_t kMarkerPending = 0xAAAAAAAAu, kMarkerDone = 0xBBBBBBBBu;

template <class T>
inline T& F(uintptr_t a) {
    return *reinterpret_cast<T*>(a);
}

void (*g_log)(const char*, ...) = nullptr;
#define FLOG(...)                      \
    do {                               \
        if (g_log) g_log(__VA_ARGS__); \
    } while (0)

Engine E{};
bool g_installed = false;
std::atomic<int> g_on{0};          // live switch
std::atomic<int> g_disabled{0};    // sticky reason code (0 = fine)
std::atomic<int> g_pct{25};        // barrier position, % of last frame's node count
const char* g_disReason = "";

// device-level state, main thread unless noted
uintptr_t g_dev = 0;
bool g_init = false, g_initFailed = false;
uintptr_t g_sBase = 0, g_sMid = 0, g_sEnd = 0;  // stream halves
uint32_t g_genAtStart = 0xFFFFFFFFu;
uintptr_t g_frameStart = 0;
HANDLE g_waitEv = nullptr;   // main thread's fence waits
uint32_t g_bufCount = 0, g_mainIdx = 0;
uint64_t g_bbFrame = ~0ull;  // frame whose back-buffer index FoGetBB already chose (RenderFrameBegin can run without a Present)
bool g_lastWasFO = false;
std::atomic<uint64_t> g_presents{0};  // every GfxDevicePresent through FO's entry points (watchdog)

struct FrameRec {
    uint64_t frame;
    uint64_t fence;
    uint32_t slot;  // the frame slot the frame was recorded in (main advances +4730h at E; TAIL resolves / stores for this one)
    uint32_t pad2;
    volatile LONG fenceValid;
    LONG predValid;
    uint32_t predIdx;
    uint32_t pad;
};
FrameRec g_rec[4] = {};
uint64_t g_frame = 0;                       // last frame number given by E (FO or stock)
std::atomic<uint64_t> g_issued{0};          // last frame whose TAIL was issued
std::atomic<uint64_t> g_tailDone{0};        // last frame whose TAIL finished (replay thread)
bool g_barrierPending = false;
uint32_t g_prevGen = 0xFFu;
volatile uint8_t g_prevGen8 = 0xFF;  // read by the HandleSlotIsNotFreed stub
uint32_t g_maxIdx = 0, g_K = 0;
// sub-stream recorders captured at E
constexpr int kMaxSub = 64;

SRWLOCK g_pp = SRWLOCK_INIT;  // PagePool frame protocol lock (main: E7 / PagePoolFrameUpdate / 7D9C24; replay: PP tokens)

// stats
std::atomic<int64_t> s_noAdvance{0};
std::atomic<int64_t> s_fo{0}, s_stock{0}, s_span{0}, s_barrierTicks{0}, s_leadTicks{0}, s_tailTicks{0}, s_ppWaitTicks{0},
    s_ppWaits{0}, s_subSkip{0}, s_barrierEarly{0}, s_barrierLate{0}, s_prevGenHits{0}, s_bbChecks{0};
double g_tickMs = 0.0;

uint64_t Now() {
    LARGE_INTEGER q;
    QueryPerformanceCounter(&q);
    return (uint64_t)q.QuadPart;
}

void Disable(int code, const char* why) {
    int expected = 0;
    if (g_disabled.compare_exchange_strong(expected, code)) {
        g_disReason = why;
        FLOG("FO: OFF for this session (%s) - stock frame end from the next frame", why);
    }
}

// ---- waits --------------------------------------------------------------------------------------------------------------
void WaitTail(uint64_t n) {
    if (g_tailDone.load(std::memory_order_acquire) >= n) return;
    const uint64_t t0 = Now();
    uint64_t lastLog = t0;
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    for (unsigned spin = 0; g_tailDone.load(std::memory_order_acquire) < n; ++spin) {
        if (spin < 64) YieldProcessor();
        else Sleep(0);
        const uint64_t t = Now();
        if (t - lastLog > (uint64_t)f.QuadPart / 2) {  // 500 ms watchdog: keep waiting (the tail owns Present), but say so
            lastLog = t;
            FLOG("FO: WARNING: waiting for TAIL %llu for %.0f ms (done %llu)", (unsigned long long)n,
                 (double)(t - t0) * 1000.0 / (double)f.QuadPart, (unsigned long long)g_tailDone.load());
        }
    }
    s_barrierTicks += (int64_t)(Now() - t0);
}

// barrier B: frame N's tail done, then the sub-stream resets stock does at Present
void Barrier(bool early) {
    if (!g_barrierPending) return;
    WaitTail(g_issued.load(std::memory_order_relaxed));
    g_barrierPending = false;
    if (early) ++s_barrierEarly;
    else ++s_barrierLate;
}

void FlushAll() {
    Barrier(false);
    g_lastWasFO = false;
}

// ---- E7: KtglCache frame end (12682C) with the forward fence ------------------------------------------------------------
void CacheFrameEnd(uintptr_t pool, uintptr_t dev, uint64_t fenceValue, uintptr_t fenceDev) {
    if (!F<uint8_t>(pool + 0xD20)) return;
    const uint64_t stamp = F<uint64_t>(pool + 0xD10);
    const uintptr_t ring = pool + 0x6A8, srw = pool + 0xCC0, srw2 = pool + 0xCD0;
    const LONG v = InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(pool + 0x8D0), 0, 0);
    bool locked = false;
    if (v == 0) {
        if (F<uint32_t>(pool + 0x6B4) >= (uint32_t)~(F<uint32_t>(pool + 0x6B0) * 5u)) {
            AcquireSRWLockExclusive(reinterpret_cast<PSRWLOCK>(srw2));
            AcquireSRWLockExclusive(reinterpret_cast<PSRWLOCK>(srw));
            locked = true;
        }
    } else if (E.tryX2(srw2) & 0xFF) {
        if (E.tryX1(srw) & 0xFF) {
            E.cacheCompact(pool, dev);
            locked = true;
        } else {
            E.unlockWake(srw2);
        }
    }
    if (locked) {
        E.ringNorm(ring);
        F<uint8_t>(pool + 0xCC8) = 0;
        ReleaseSRWLockExclusive(reinterpret_cast<PSRWLOCK>(srw));
        E.unlockWake(srw2);
    }
    E.tagFence(ring, fenceDev, fenceValue);  // entry {3, 0, fenceDev+4310h, value}: the job's COPY queue Wait(fence, value)
    E.retire(ring, pool + 0x608, pool + 0xA8, pool + 0x8F0);
    E.kick(pool + 0x758);
    E.kick(pool + 0x788);
    E.kick(pool + 0x6F8);
    E.process(ring, dev, stamp, pool + 0x590, pool + 0x608);
    E.signalPair(pool + 0x728);
}

// ---- TAIL (replay thread, inside the 0x8F token) ------------------------------------------------------------------------
using PresentFn = HRESULT(STDMETHODCALLTYPE*)(void*, UINT, UINT);
using GetIdxFn = UINT(STDMETHODCALLTYPE*)(void*);
using ResolveFn = void(STDMETHODCALLTYPE*)(void*, void*, int, UINT, UINT, void*, UINT64);
using MapFn = HRESULT(STDMETHODCALLTYPE*)(void*, UINT, const D3D12_RANGE*, void**);
using UnmapFn = void(STDMETHODCALLTYPE*)(void*, UINT, const D3D12_RANGE*);
using U64PtrFn = HRESULT(STDMETHODCALLTYPE*)(void*, UINT64*);
using ResetFn = HRESULT(STDMETHODCALLTYPE*)(void*);
using SignalFn = HRESULT(STDMETHODCALLTYPE*)(void*, void*, UINT64);
template <class Fn>
inline Fn V(void* obj, size_t off) {
    return *reinterpret_cast<Fn*>(*reinterpret_cast<uintptr_t*>(obj) + off);
}

void TimestampResolve(uintptr_t dev, uint32_t slot) {
    uint32_t i = F<uint32_t>(dev + kTsFirst + slot * 16ull);
    const uint32_t last = F<uint32_t>(dev + kTsLast + slot * 16ull);
    if (i > last) return;
    const uint32_t base = slot * F<uint32_t>(dev + kTsPer);
    uint64_t* bits = F<uint64_t*>(dev + kTsBits + slot * 16ull);
    void* list = F<void*>(dev + kCtxListPtr);
    uint32_t start = 0xFFFFFFFFu, run = 0;
    auto flush = [&]() {
        const UINT a = start + base;
        V<ResolveFn>(list, 0x1B0)(list, F<void*>(dev + kTsHeap), 2, a, run, F<void*>(dev + kTsReadback), (UINT64)a * 8);
        run = 0;
        start = 0xFFFFFFFFu;
    };
    for (; i <= last; ++i) {
        const uint64_t m = 1ull << (i & 63);
        uint64_t& w = bits[i >> 6];
        if (!(w & m)) {
            if (run) flush();
        } else {
            w &= ~m;
            if (!run) start = i;
            ++run;
        }
    }
    if (run) flush();
}

// GfxFrameSlotAdvance 8BEC48 minus the parts that stay on the replay thread, run by MAIN at E (stock: main, after Present).
// The timestamp allocator 6B42B0 runs on main at record time with index slot * per + i and the slot's range / bitset, so the
// record slot must advance before frame N+1 records. Order matters for the replay thread, which resets every new command list with
// allocator +4290h[+4730h] (SynCmdListOnReset 492220): the next slot's allocator is reset (after its fence = frame N-2 done)
// BEFORE +4730h changes, so a list of frame N recorded after the switch starts on a freshly reset allocator; that allocator is
// next reset only after the fence of frame N+1 (covers frame N).
void SlotAdvanceMain(uintptr_t dev) {
    const uint32_t slot = (F<uint32_t>(dev + kSlot) + 1) % std::max(1u, F<uint32_t>(dev + kSlotCount));
    const uint64_t t0 = Now();
    E.waitFence(dev, F<uint64_t>(dev + kSlotFence + slot * 8ull), g_waitEv);  // GPU done with frame N-2
    s_leadTicks += (int64_t)(Now() - t0);
    const uint32_t first = F<uint32_t>(dev + kTsFirst + slot * 16ull), last = F<uint32_t>(dev + kTsLast + slot * 16ull);
    if (first <= last) {
        const uint32_t per = F<uint32_t>(dev + kTsPer), cnt = last - first + 1;
        const uint64_t a = (uint64_t)slot * per + first;
        D3D12_RANGE r{(SIZE_T)(a * 8), (SIZE_T)((a + cnt) * 8)};
        void* rb = F<void*>(dev + kTsReadback);
        void* p = nullptr;
        if (SUCCEEDED(V<MapFn>(rb, 0x40)(rb, 0, &r, &p)) && p) {
            memcpy(F<uint8_t*>(dev + kTsCopy) + first * 8ull, static_cast<uint8_t*>(p) + a * 8, cnt * 8ull);
            V<UnmapFn>(rb, 0x48)(rb, 0, nullptr);
        }
        void* q = F<void*>(dev + kQueue);
        V<U64PtrFn>(q, 0x80)(q, reinterpret_cast<UINT64*>(dev + kTsCalib));
        F<uint32_t>(dev + kTsFirst + slot * 16ull) = 0xFFFFFFFFu;
        F<uint32_t>(dev + kTsLast + slot * 16ull) = 0;
        memset(F<void*>(dev + kTsBits + slot * 16ull), 0, ((uint64_t)per + 0x3F & ~0x3Full) >> 3);
    }
    void* alloc = F<void*>(dev + kAlloc + slot * 8ull);
    V<ResetFn>(alloc, 0x40)(alloc);
    InterlockedExchange(reinterpret_cast<volatile LONG*>(dev + kSlot), (LONG)slot);
}

void __fastcall TailFn(void* /*{list, ctx}*/, FrameRec* r) {
    const uintptr_t dev = g_dev;
    const uint64_t t0 = Now(), q0 = E.qpc();
    if (F<uint8_t>(dev + kPresentThr)) E.spinPresent(dev);  // present N-1 retired: GetCurrentBackBufferIndex = frame N's buffer
    TimestampResolve(dev, r->slot);
    const uintptr_t p = dev + kPresentParams;
    const bool scOverride = F<uintptr_t>(p + 8) != 0;
    uintptr_t res = scOverride ? F<uintptr_t>(p + 0x10) : 0;
    if (!res) {
        void* sc = F<void*>(dev + kSwap);
        const UINT idx = V<GetIdxFn>(sc, 0x120)(sc);
        if (r->predValid) {
            ++s_bbChecks;
            if (idx != r->predIdx) {
                FLOG("FO: back-buffer prediction mismatch at frame %llu (predicted %u, real %u)", (unsigned long long)r->frame,
                     r->predIdx, idx);
                Disable(4, "back-buffer index prediction mismatch");
            }
        }
        res = F<uintptr_t>(dev + kBB + idx * 8ull);
    }
    E.transition(dev + kCtx, F<uint64_t>(res + 0x10), F<uint64_t>(res + 0x18), 0, 0, 0xFFFFFFFFull);
    const uint64_t fence = E.submitFrame(dev + kListRing, dev + kListPool, 0);
    const uint32_t slot = r->slot;
    F<uint64_t>(dev + kSlotFence + slot * 8ull) = fence;
    r->fence = fence;
    InterlockedExchange(&r->fenceValid, 1);
    E.psoMaint(dev + kCtx, slot, F<uint64_t>(dev + kSlotFence + slot * 8ull));
    // KtglCache frame end (12682C) exactly as stock: after the frame's last list, tagged with the frame's own fence. Under the PP
    // lock against main's PagePoolFrameUpdate / 7D9C24 of frame N+1 (no common engine lock; stock relied on the drain).
    AcquireSRWLockExclusive(&g_pp);
    CacheFrameEnd(dev + kPool, dev, fence, dev);
    ReleaseSRWLockExclusive(&g_pp);
    // present hand-off (stock 3D62E4 tail)
    const bool noVsyncFlag = (F<uint32_t>(p) & 1) == 0;
    const int sync = noVsyncFlag ? F<int>(dev + kSyncInterval) : 0;
    const bool flagged = !noVsyncFlag;
    const int flags = (scOverride && flagged) ? 0 : F<int>(p);
    void* sc = scOverride ? F<void*>(p + 8) : F<void*>(dev + kSwap);
    if (!F<uint8_t>(dev + kPresentThr)) {
        using Cb = void (*)();
        if (Cb cb = F<Cb>(E.slCb[0])) cb();
        const HRESULT hr = V<PresentFn>(sc, 0x40)(sc, (UINT)sync, (UINT)flags);
        if (FAILED(hr)) {
            E.removedReason(F<uint64_t>(dev + kDevice), hr);
            E.fatal(F<uint64_t>(E.gfxGlobal), hr);
        }
        if (Cb cb = F<Cb>(E.slCb[1])) cb();
        if (Cb cb = F<Cb>(E.slCb[2])) cb();
    } else {
        E.spinPresent(dev);
        F<uint8_t>(dev + kPresentPending) = 1;
        F<void*>(dev + kPresentSc) = sc;
        F<int>(dev + kPresentA) = sync;
        F<int>(dev + kPresentB) = flags;
        SetEvent(F<HANDLE>(dev + kPresentEv));
    }
    E.recRebuild(dev + kCtx);  // pipeline-statistics readback (early-outs when unused; FO falls back when used)
    E.recycle(dev + kCtx);     // CmdListRingFlushAndRecycle on SynCtx (2EFB73C)
    F<uint64_t>(dev + kPresentTime) = E.qpc() - q0;
    s_tailTicks += (int64_t)(Now() - t0);
    g_tailDone.store(r->frame, std::memory_order_release);
}

// ---- E: frame end on the main thread ------------------------------------------------------------------------------------
bool Init(uintptr_t dev) {
    if (g_init) return true;
    if (g_initFailed) return false;
    g_dev = dev;
    g_sBase = F<uintptr_t>(dev + kBase);
    g_sEnd = F<uintptr_t>(dev + kEnd);
    const uintptr_t size = g_sEnd - g_sBase;
    g_sMid = g_sBase + ((size / 2) & ~(uintptr_t)0xF);
    const uint32_t reserve = F<uint32_t>(dev + kReserve);
    if (!g_sBase || size < (4u << 20) || reserve > size / 8 || g_sEnd != g_sBase + F<uint32_t>(dev + kStreamSize)) {
        FLOG("FO: not used (stream %p..%p, size %u, reserve %u: unexpected layout)", (void*)g_sBase, (void*)g_sEnd,
             F<uint32_t>(dev + kStreamSize), reserve);
        g_initFailed = true;
        return false;
    }
    g_waitEv = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    g_init = true;
    FLOG("FO: armed: stream halves %p | %p | %p (reserve %u), sync tables %u, frame slots %u, present thread %u, K %d %%",
         (void*)g_sBase, (void*)g_sMid, (void*)g_sEnd, reserve, F<uint32_t>(dev + kSyncN), F<uint32_t>(dev + kSlotCount),
         F<uint8_t>(dev + kPresentThr), g_pct.load());
    return true;
}

void StockPresent(uintptr_t dev, uintptr_t p2) {
    FlushAll();
    ++s_stock;
    E.present(dev, p2);
    ++g_frame;
    g_rec[(g_frame + 1) & 3].predValid = 0;
}

void __fastcall FoPresent(uintptr_t dev, uintptr_t p2) {
    ++g_presents;
    if (!g_dev) g_dev = dev;
    if (p2 || dev != g_dev || !g_on.load() || g_disabled.load() || !Init(dev)) {
        StockPresent(dev, p2);
        return;
    }
    Barrier(false);  // this frame's barrier if the graph never reached K
    if (F<uint32_t>(dev + kSyncN) < 3 || F<uint32_t>(dev + kPipeStatsN) != 0) {
        Disable(5, F<uint32_t>(dev + kSyncN) < 3 ? "fewer than 3 sync-point tables" : "pipeline-statistics queries in use");
        StockPresent(dev, p2);
        return;
    }
    // stream: this frame's data must sit in one half, with room for the 0x8F token
    const uintptr_t write = F<uintptr_t>(dev + kWrite);
    const uint32_t gen = F<uint32_t>(dev + kGen);
    const uintptr_t start = gen == g_genAtStart ? g_frameStart : g_sBase;
    const bool inLo = start >= g_sBase && start < g_sMid, inHi = start >= g_sMid && start < g_sEnd;
    const uintptr_t hiEnd = inLo ? g_sMid : g_sEnd;
    if (!(inLo || inHi) || write < start || write + 64 > hiEnd || write + 64 > F<uintptr_t>(dev + kEnd)) {
        ++s_span;
        StockPresent(dev, p2);
        F<uintptr_t>(dev + kEnd) = g_sMid;  // stock reset to base: the next frame records in the low half
        return;
    }
    {  // stock does not advance the slot when the present flag bit 0 is set without a swapchain override: keep stock there
        const uintptr_t pp = dev + kPresentParams;
        if ((F<uint32_t>(pp) & 1) && !F<uintptr_t>(pp + 8)) {
            ++s_noAdvance;
            StockPresent(dev, p2);
            return;
        }
    }
    const uint64_t N = g_frame + 1;
    // final kick exactly as stock (submit = 1)
    uint64_t item[2] = {F<uint64_t>(dev + kLastKick), (uint64_t)write};
    E.ringPush(dev + kRing, item, 1, dev + kStats);
    // sync-point table tick (first half of 8BEDC8)
    {
        const uint32_t n = ++F<uint32_t>(dev + kSyncFrame);
        const uintptr_t t = F<uintptr_t>(dev + kSyncTab) + (uint64_t)(n % F<uint32_t>(dev + kSyncN)) * 0x18;
        F<uint32_t>(t + 8) = 0;
        F<uint32_t>(t + 0x10) = n;
    }
    // record slot for frame N+1 (GfxFrameSlotAdvance's main-thread part); TAIL N uses the frame's own slot
    const uint32_t frameSlot = F<uint32_t>(dev + kSlot);
    SlotAdvanceMain(dev);
    // TAIL token in its own stretch (submit = 0)
    FrameRec& r = g_rec[N & 3];
    r.frame = N;
    r.slot = frameSlot;
    r.fence = 0;
    r.fenceValid = 0;
    {
        uint32_t* w = reinterpret_cast<uint32_t*>(write);
        const uint64_t fn = (uint64_t)(uintptr_t)&TailFn, arg = (uint64_t)(uintptr_t)&r;
        w[0] = 0x8F;
        w[1] = (uint32_t)fn;
        w[2] = (uint32_t)(fn >> 32);
        w[3] = (uint32_t)arg;
        w[4] = (uint32_t)(arg >> 32);
    }
    const uintptr_t write2 = write + 20;
    g_issued.store(N, std::memory_order_relaxed);
    uint64_t item2[2] = {(uint64_t)write, (uint64_t)write2};
    E.ringPush(dev + kRing, item2, 0, dev + kStats);
    // switch halves (stock: drain, then reset to base)
    const uintptr_t nextStart = inLo ? g_sMid : g_sBase, nextEnd = inLo ? g_sEnd : g_sMid;
    const uint32_t bytes = F<uint32_t>(dev + kBytes) + (uint32_t)(write2 - start);
    F<uint32_t>(dev + kBytes) = bytes;
    F<uint32_t>(dev + kBytesMax) = std::max(F<uint32_t>(dev + kBytesMax), bytes);
    F<uintptr_t>(dev + kHigh) = std::max(F<uintptr_t>(dev + kHigh), write2);
    F<uintptr_t>(dev + kEnd) = nextEnd;
    F<uintptr_t>(dev + kWrite) = nextStart;
    F<uintptr_t>(dev + kLastKick) = nextStart;
    F<uintptr_t>(dev + kLastSubmit) = nextStart;
    g_prevGen = gen & 0xF;
    g_prevGen8 = (uint8_t)(gen & 0xF);
    F<uint32_t>(dev + kGen) = gen + 1;
    F<uint32_t>(dev + kSinceFlush) = 0;
    g_genAtStart = gen + 1;
    g_frameStart = nextStart;
    // CmdStreamEndFrame snapshot
    memcpy(reinterpret_cast<void*>(dev + kSnap), reinterpret_cast<void*>(dev + kStats), 0x88);
    memset(reinterpret_cast<void*>(dev + kStats), 0, 0x88);
    g_frame = N;
    g_rec[(N + 1) & 3].predValid = 0;
    g_barrierPending = true;
    g_lastWasFO = true;
    g_K = (uint32_t)((uint64_t)g_maxIdx * (uint32_t)std::clamp(g_pct.load(), 0, 90) / 100u);
    g_maxIdx = 0;
    ++s_fo;
}

// ---- call-site wrappers -------------------------------------------------------------------------------------------------
void __fastcall FoPresentDirect(uintptr_t dev, uintptr_t p2) {  // other GfxDevicePresent callers (init / loading paths)
    ++g_presents;
    FlushAll();
    E.present(dev, p2);
    ++g_frame;
}
void __fastcall FoPresentSpin(uintptr_t dev) {  // WaitSubmitThread 7D9B91
    if (g_lastWasFO) return;                     // TAIL N spins for present N-1 itself before it hands off present N
    E.spinPresent(dev);
}
UINT __fastcall FoGetBB(void* sc) {  // WaitSubmitThread 7D9BC7 (was call [rax+120h] = GetCurrentBackBufferIndex)
    if (g_bbFrame == g_frame) return g_mainIdx;  // second frame begin without a Present in between: same buffer
    g_bbFrame = g_frame;
    if (g_lastWasFO && g_bufCount) {
        const uint32_t idx = (g_mainIdx + 1) % g_bufCount;
        FrameRec& r = g_rec[(g_frame + 1) & 3];
        r.predIdx = idx;
        r.predValid = 1;
        g_mainIdx = idx;
        return idx;
    }
    const UINT idx = V<GetIdxFn>(sc, 0x120)(sc);
    DXGI_SWAP_CHAIN_DESC d{};
    using DescFn = HRESULT(STDMETHODCALLTYPE*)(void*, DXGI_SWAP_CHAIN_DESC*);
    if (SUCCEEDED(V<DescFn>(sc, 0x60)(sc, &d))) g_bufCount = d.BufferCount;
    g_mainIdx = idx;
    return idx;
}
void __fastcall FoPPUpdate(uintptr_t pool, uintptr_t dev) {  // 7D9BA3 PagePoolFrameUpdate
    AcquireSRWLockExclusive(&g_pp);
    E.ppUpdate(pool, dev);
    ReleaseSRWLockExclusive(&g_pp);
}
void __fastcall FoPPOps(uintptr_t pool, uintptr_t dev) {  // 7D9C07 7D9C24
    AcquireSRWLockExclusive(&g_pp);
    E.ppOps(pool, dev);
    ReleaseSRWLockExclusive(&g_pp);
}
void __fastcall FoCacheFlushBegin(uintptr_t r) {  // 4A53D0: render-size change path uses SynCtx from main -> stock situation first
    FlushAll();
    E.cacheFlushBegin(r);
}
void __fastcall FoEndFrameFlush(uintptr_t dev) {  // 2EE8B92 (device flush path) -> CmdStreamEndFrame
    FlushAll();
    E.endFrame(dev);
}
void __fastcall FoTeardownSpin(uintptr_t dev) {  // 2EEC4F9 (teardown for reset) -> SpinWaitFlagSleep0
    FlushAll();
    E.spinPresent(dev);
}
void __fastcall FoNode(uintptr_t r, uintptr_t entry) {  // Renderer vfn+130h, before every frame-graph entry
    const uint32_t idx = F<uint32_t>(r + kRendererNodeIdx);
    g_maxIdx = std::max(g_maxIdx, idx);
    if (g_barrierPending && idx >= g_K) Barrier(true);
    E.node130(r, entry);
}

// handles: HandleSlotIsNotFreed 58C434 accepts the previous generation while TAIL is pending (stock otherwise).
// MUST be machine code with the original's clobber set (rax rcx rdx r8 + flags): the engine is built with LTCG, and callers keep
// live values in r9 / r10 / r11 across this leaf call (7D9F38: r9 = handle, r10 = device, r11 = &+3C58h; 3D2BFE: r9). A C++
// replacement clobbered them -> the handle wait spun on a garbage handle (pc2 FO1, 1.7.59-fo1). HandlePoolSlotIsLive 58B76C is
// left stock: for a previous-generation handle it already answers "no kick needed".
//   rcx = handle pool {desc*, u32 gen}, rdx = handle (offset | gen << 60); al = 0 not freed (marker unreplayed), 1 freed
size_t BuildNotFreedStub(uint8_t* s, const volatile uint8_t* prevGen, const volatile uint64_t* issued, const volatile uint64_t* done) {
    size_t n = 0;
    auto B = [&](std::initializer_list<uint8_t> l) {
        for (uint8_t v : l) s[n++] = v;
    };
    auto Q = [&](uint64_t v) {
        memcpy(s + n, &v, 8);
        n += 8;
    };
    B({0x48, 0x85, 0xD2});                    // test rdx, rdx
    B({0x74, 0x00});                          // je ret1                     (patched)
    const size_t jeRet0 = n - 1;
    B({0x8B, 0x41, 0x08});                    // mov eax, [rcx+8]
    B({0x83, 0xE0, 0x0F});                    // and eax, 0Fh
    B({0x4C, 0x8B, 0xC2});                    // mov r8, rdx
    B({0x49, 0xC1, 0xE8, 0x3C});              // shr r8, 3Ch
    B({0x44, 0x3A, 0xC0});                    // cmp r8b, al
    B({0x74, 0x00});                          // je check                    (patched)
    const size_t jeCheck = n - 1;
    B({0x48, 0xB8});                          // mov rax, &prevGen
    Q((uint64_t)(uintptr_t)prevGen);
    B({0x44, 0x3A, 0x00});                    // cmp r8b, [rax]
    B({0x75, 0x00});                          // jne ret1                    (patched)
    const size_t jneRet1 = n - 1;
    B({0x48, 0xB8});                          // mov rax, &issued
    Q((uint64_t)(uintptr_t)issued);
    B({0x4C, 0x8B, 0x00});                    // mov r8, [rax]
    B({0x48, 0xB8});                          // mov rax, &tailDone
    Q((uint64_t)(uintptr_t)done);
    B({0x4C, 0x39, 0x00});                    // cmp [rax], r8
    B({0x73, 0x00});                          // jae ret1  (tailDone >= issued: nothing pending -> stock answer "freed")
    const size_t jaeRet1 = n - 1;
    const size_t check = n;
    B({0x4C, 0x8B, 0x01});                    // mov r8, [rcx]        desc
    B({0x4D, 0x8B, 0x40, 0x08});              // mov r8, [r8+8]       stream base
    B({0x48, 0xB8});                          // mov rax, 0FFFFFFFFFFFFFFFh
    Q(0x0FFFFFFFFFFFFFFFull);
    B({0x48, 0x23, 0xD0});                    // and rdx, rax
    B({0x41, 0x81, 0x3C, 0x90, 0xAA, 0xAA, 0xAA, 0xAA});  // cmp dword [r8+rdx*4], 0AAAAAAAAh
    B({0x75, 0x00});                          // jne ret1                    (patched)
    const size_t jneRet2 = n - 1;
    B({0x32, 0xC0});                          // xor al, al
    B({0xC3});                                // ret
    const size_t ret1 = n;
    B({0xB0, 0x01});                          // mov al, 1
    B({0xC3});                                // ret
    auto fix = [&](size_t at, size_t to) { s[at] = (uint8_t)(int8_t)((intptr_t)to - (intptr_t)(at + 1)); };
    fix(jeRet0, ret1);
    fix(jeCheck, check);
    fix(jneRet1, ret1);
    fix(jaeRet1, ret1);
    fix(jneRet2, ret1);
    return n;
}
using NotFreedFn = uint8_t(__fastcall*)(int64_t* p, uint64_t h);
NotFreedFn g_notFreed = nullptr;  // the stub (also called by the harness)

// PagePool token handlers on the replay thread take the PP lock
using Handler = void(__fastcall*)(uintptr_t ctx, uint8_t** cur);
constexpr uint8_t kPPToks[] = {0x82, 0x83, 0x95, 0x96, 0x97, 0x98, 0x9A};
constexpr int kNPP = (int)sizeof(kPPToks);
Handler g_ppOrig[kNPP] = {};
bool PPLock() {
    if (TryAcquireSRWLockExclusive(&g_pp)) return true;
    const uint64_t t0 = Now();
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    for (unsigned spin = 0; !TryAcquireSRWLockExclusive(&g_pp); ++spin) {
        if (spin < 64) YieldProcessor();
        else Sleep(0);
        if (Now() - t0 > (uint64_t)f.QuadPart * 2) {  // 2 s: break a would-be deadlock (run unlocked), stock frame ends from now
            Disable(6, "PP lock watchdog (replay thread waited 2 s)");
            ++s_ppWaits;
            s_ppWaitTicks += (int64_t)(Now() - t0);
            return false;
        }
    }
    ++s_ppWaits;
    s_ppWaitTicks += (int64_t)(Now() - t0);
    return true;
}
template <int I>
void __fastcall PPWrap(uintptr_t ctx, uint8_t** cur) {
    const bool locked = PPLock();
    g_ppOrig[I](ctx, cur);
    if (locked) ReleaseSRWLockExclusive(&g_pp);
}
const Handler kPPWraps[kNPP] = {&PPWrap<0>, &PPWrap<1>, &PPWrap<2>, &PPWrap<3>, &PPWrap<4>, &PPWrap<5>, &PPWrap<6>};

// ---- installation -------------------------------------------------------------------------------------------------------
uint8_t* AllocIn(uintptr_t from, uintptr_t to, size_t size) {
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
uint8_t* AllocNear(uintptr_t lo, uintptr_t hi, size_t size) {
    const uintptr_t reach = 0x7FF00000ull - (hi - lo);
    if (uint8_t* p = AllocIn(hi, hi + reach, size)) return p;
    return AllocIn(lo > reach + 0x10000 ? lo - reach : 0x10000, lo, size);
}

// write n code bytes with every other thread suspended and none stopped inside them (psomemo.cpp FrozenWrite5 scheme)
bool FrozenWrite(uint8_t* at, const uint8_t* bytes, size_t n) {
    DWORD old = 0;
    if (!VirtualProtect(at, n, PAGE_EXECUTE_READWRITE, &old)) return false;
    const DWORD self = GetCurrentThreadId(), pid = GetCurrentProcessId();
    constexpr int kMaxT = 1024;
    static DWORD tids[kMaxT];
    static HANDLE hs[kMaxT];
    static bool sus[kMaxT];
    bool written = false;
    for (int attempt = 0; attempt < 200 && !written; ++attempt) {
        int nt = 0;
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snap != INVALID_HANDLE_VALUE) {
            THREADENTRY32 te{};
            te.dwSize = sizeof(te);
            for (BOOL ok = Thread32First(snap, &te); ok && nt < kMaxT; ok = Thread32Next(snap, &te))
                if (te.th32OwnerProcessID == pid && te.th32ThreadID != self) tids[nt++] = te.th32ThreadID;
            CloseHandle(snap);
        }
        int nh = 0;
        for (int i = 0; i < nt; ++i)
            if (HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, tids[i])) hs[nh++] = h;
        for (int i = 0; i < nh; ++i) sus[i] = SuspendThread(hs[i]) != (DWORD)-1;
        bool clash = false;
        for (int i = 0; i < nh && !clash; ++i) {
            if (!sus[i]) continue;
            CONTEXT ctx{};
            ctx.ContextFlags = CONTEXT_CONTROL;
            if (!GetThreadContext(hs[i], &ctx)) continue;
            if (ctx.Rip > (uintptr_t)at && ctx.Rip < (uintptr_t)at + n) clash = true;
        }
        if (!clash) {
            memcpy(at, bytes, n);
            FlushInstructionCache(GetCurrentProcess(), at, n);
            written = true;
        }
        for (int i = 0; i < nh; ++i)
            if (sus[i]) ResumeThread(hs[i]);
        for (int i = 0; i < nh; ++i) CloseHandle(hs[i]);
        if (!written) Sleep(1 + attempt % 4);
    }
    VirtualProtect(at, n, old, &old);
    return written && memcmp(at, bytes, n) == 0;
}
bool WritePtr(uintptr_t at, uintptr_t v) {
    DWORD old = 0;
    if (!VirtualProtect((void*)at, 8, PAGE_READWRITE, &old)) return false;
    InterlockedExchange64(reinterpret_cast<volatile LONG64*>(at), (LONG64)v);
    VirtualProtect((void*)at, 8, old, &old);
    return F<uintptr_t>(at) == v;
}

struct Site {
    uint32_t rva;
    uint8_t op;        // E8 call / E9 jmp; 0xFF = 6-byte 'call [rax+120h]'
    uint32_t target;   // stock target RVA (rel32 sites)
    void* fn;          // replacement
    const char* name;
};

uintptr_t g_base = 0;
uint8_t* g_relays = nullptr;
int g_nRelay = 0;
uint8_t* Relay(void* fn) {  // mov rax, imm64 ; jmp rax
    uint8_t* r = g_relays + 16 * g_nRelay++;
    r[0] = 0x48;
    r[1] = 0xB8;
    const uint64_t a = (uint64_t)(uintptr_t)fn;
    memcpy(r + 2, &a, 8);
    r[10] = 0xFF;
    r[11] = 0xE0;
    return r;
}

// ---- lazy install / restore / watchdog --------------------------------------------------------------------------------
struct Patch {
    uint8_t* at;
    size_t n;
    uint8_t orig[16];
    uint8_t neu[16];
    const char* name;
};
struct PtrPatch {
    uintptr_t at, orig, neu;
};
Patch g_patch[16];
int g_nPatch = 0;
PtrPatch g_ptr[16];
int g_nPtr = 0;
bool g_ready = false;          // verified at boot, patches prepared
CRITICAL_SECTION g_instCs;
std::atomic<int> g_wdFired{0};

DWORD WINAPI Watchdog(LPVOID);
bool Install() {  // under g_instCs
    if (g_installed) return true;
    if (!g_ready) return false;
    int ok = 0;
    for (int i = 0; i < g_nPtr; ++i) ok += WritePtr(g_ptr[i].at, g_ptr[i].neu);
    for (int i = 0; i < g_nPatch; ++i) {
        if (FrozenWrite(g_patch[i].at, g_patch[i].neu, g_patch[i].n)) ++ok;
        else FLOG("FO: WARNING: site %s write failed", g_patch[i].name);
    }
    g_installed = true;
    if (ok != g_nPatch + g_nPtr) {
        FLOG("FO: ERROR: %d of %d patches written - FO stays OFF for this session", ok, g_nPatch + g_nPtr);
        g_disabled = 7;
        g_disReason = "partial install";
    }
    FLOG("FO: frame overlap installed (%d code sites, %d table / vtable slots)", g_nPatch, g_nPtr);
    if (HANDLE h = CreateThread(nullptr, 0, Watchdog, nullptr, 0, nullptr)) CloseHandle(h);
    return true;
}
void Restore(const char* why) {  // put every original byte / pointer back
    EnterCriticalSection(&g_instCs);
    if (g_installed) {
        for (int i = 0; i < g_nPatch; ++i) FrozenWrite(g_patch[i].at, g_patch[i].orig, g_patch[i].n);
        for (int i = 0; i < g_nPtr; ++i) WritePtr(g_ptr[i].at, g_ptr[i].orig);
        FLOG("FO: all FO patches removed (%s)", why);
    }
    LeaveCriticalSection(&g_instCs);
}
DWORD WINAPI Watchdog(LPVOID) {
    uint64_t last = g_presents.load();
    DWORD lastChange = GetTickCount();
    for (;;) {
        Sleep(250);
        const uint64_t p = g_presents.load();
        const DWORD now = GetTickCount();
        if (p != last || p == 0) {
            last = p;
            lastChange = now;
            continue;
        }
        if (now - lastChange > 5000 && !g_wdFired.exchange(1)) {
            FLOG("FO: WATCHDOG: no frame for %lu ms (switch %d, FO frames %lld, stock %lld, TAIL issued %llu done %llu, barrier %s) - "
                 "FO off for the session",
                 (unsigned long)(now - lastChange), g_on.load(), (long long)s_fo.load(), (long long)s_stock.load(),
                 (unsigned long long)g_issued.load(), (unsigned long long)g_tailDone.load(), g_barrierPending ? "pending" : "done");
            Disable(8, "watchdog: no frame for 5 s");
            if (g_tailDone.load() >= g_issued.load()) Restore("watchdog, no TAIL pending");
            else FLOG("FO: WATCHDOG: TAIL %llu still pending - patches kept so the pending frame can finish", (unsigned long long)g_issued.load());
        }
    }
}

}  // namespace

// ---- public -------------------------------------------------------------------------------------------------------------
void Start(const StartInfo& info) {
    g_log = info.log;
    if (!info.iniPath || !info.base) return;
    const int key = (int)GetPrivateProfileIntW(L"GpuOpt", L"FrameOverlap", 0, info.iniPath);
    InitializeCriticalSection(&g_instCs);
    if (key != 1 && key != 2) return;
    g_pct = std::clamp((int)GetPrivateProfileIntW(L"Tuning", L"FO_Pct", 25, info.iniPath), 0, 90);
    if (!info.knownBuild) {
        FLOG("FO: not installed (unknown build; the sites are v2.02)");
        return;
    }
    const uintptr_t b = info.base;
    g_base = b;
    auto fn = [&](uint32_t rva) { return b + rva; };
    E.present = (decltype(E.present))fn(0x3D62E4);
    E.ringPush = (decltype(E.ringPush))fn(0x25426C);
    E.subReset = (decltype(E.subReset))fn(0x253F2C);
    E.transition = (decltype(E.transition))fn(0x115E24);
    E.submitFrame = (decltype(E.submitFrame))fn(0x252758);
    E.psoMaint = (decltype(E.psoMaint))fn(0x8CDA94);
    E.spinPresent = (decltype(E.spinPresent))fn(0x7D9E90);
    E.removedReason = (decltype(E.removedReason))fn(0x1896810);
    E.fatal = (decltype(E.fatal))fn(0x18956D0);
    E.waitFence = (decltype(E.waitFence))fn(0x7D9E1C);
    E.recRebuild = (decltype(E.recRebuild))fn(0x9AFED0);
    E.recycle = (decltype(E.recycle))fn(0x2EFB73C);
    E.qpc = (decltype(E.qpc))fn(0x205530);
    E.ringNorm = (decltype(E.ringNorm))fn(0x1263F0);
    E.tryX2 = (decltype(E.tryX2))fn(0x1264F0);
    E.tryX1 = (decltype(E.tryX1))fn(0x126480);
    E.cacheCompact = (decltype(E.cacheCompact))fn(0x250650);
    E.unlockWake = (decltype(E.unlockWake))fn(0x12645C);
    E.tagFence = (decltype(E.tagFence))fn(0x126744);
    E.retire = (decltype(E.retire))fn(0x1280C8);
    E.kick = (decltype(E.kick))fn(0x126984);
    E.process = (decltype(E.process))fn(0x126A40);
    E.signalPair = (decltype(E.signalPair))fn(0x1269D4);
    E.ppUpdate = (decltype(E.ppUpdate))fn(0x252510);
    E.ppOps = (decltype(E.ppOps))fn(0x7D9C24);
    E.cacheFlushBegin = (decltype(E.cacheFlushBegin))fn(0x32FE938);
    E.endFrame = (decltype(E.endFrame))fn(0x418B94);
    E.node130 = (decltype(E.node130))fn(0x113150);
    E.gfxGlobal = fn(0x45BA588);
    E.slCb[0] = fn(0x4BD0998);
    E.slCb[1] = fn(0x4BD09A8);
    E.slCb[2] = fn(0x4BD09B0);

    const Site sites[] = {
        {0x5DAF3, 0xE9, 0x3D62E4, (void*)&FoPresent, "GfxDevicePresentVfn -> GfxDevicePresent"},
        {0x30A6931, 0xE9, 0x3D62E4, (void*)&FoPresentDirect, "30A6904 -> GfxDevicePresent"},
        {0x32FAE39, 0xE8, 0x3D62E4, (void*)&FoPresentDirect, "PostEffectLibraryInit -> GfxDevicePresent"},
        {0x32FBDBE, 0xE8, 0x3D62E4, (void*)&FoPresentDirect, "32FADF4 -> GfxDevicePresent"},
        {0x7D9B91, 0xE8, 0x7D9E90, (void*)&FoPresentSpin, "WaitSubmitThread present spin"},
        {0x7D9BA3, 0xE8, 0x252510, (void*)&FoPPUpdate, "WaitSubmitThread PagePoolFrameUpdate"},
        {0x7D9BC7, 0xFF, 0, (void*)&FoGetBB, "WaitSubmitThread GetCurrentBackBufferIndex"},
        {0x7D9C07, 0xE8, 0x7D9C24, (void*)&FoPPOps, "WaitSubmitThread 7D9C24"},
        {0x4A53D0, 0xE8, 0x32FE938, (void*)&FoCacheFlushBegin, "RenderFrameBegin RenderFrameBeginCacheFlush"},
        {0x2EE8B92, 0xE8, 0x418B94, (void*)&FoEndFrameFlush, "2EE8B70 CmdStreamEndFrame"},
        {0x2EEC4F9, 0xE8, 0x7D9E90, (void*)&FoTeardownSpin, "GfxDeviceTeardownForReset spin"},
    };
    // ---- verify everything before touching anything ----
    bool ok = true;
    for (const Site& s : sites) {
        const uint8_t* at = (const uint8_t*)(b + s.rva);
        if (s.op == 0xFF) {
            static const uint8_t k[] = {0x48, 0x8B, 0x8B, 0x78, 0x42, 0x00, 0x00, 0x48, 0x8B, 0x01, 0xFF, 0x90, 0x20, 0x01, 0x00, 0x00, 0x8B, 0xC0};
            if (memcmp(at - 10, k, sizeof k) != 0) {
                FLOG("FO: site %X (%s) bytes differ", s.rva, s.name);
                ok = false;
            }
            continue;
        }
        int32_t rel = 0;
        memcpy(&rel, at + 1, 4);
        if (at[0] != s.op || (uintptr_t)(at + 5 + rel) != b + s.target) {
            FLOG("FO: site %X (%s) bytes differ", s.rva, s.name);
            ok = false;
        }
    }
    // HandleSlotIsNotFreed 58C434 / HandlePoolSlotIsLive 58B76C: test rdx,rdx; je; mov eax,[rcx+8]; mov r8,rdx; shr r8,3Ch
    static const uint8_t kNotFreed[] = {0x48, 0x85, 0xD2, 0x74, 0x32, 0x8B, 0x41, 0x08, 0x4C, 0x8B, 0xC2, 0x49, 0xC1, 0xE8, 0x3C};
    if (memcmp((void*)(b + 0x58C434), kNotFreed, sizeof kNotFreed)) {
        FLOG("FO: HandleSlotIsNotFreed 58C434 bytes differ");
        ok = false;
    }
    const uintptr_t vt[2] = {b + 0x40D4A78 + 0x130, b + 0x3AAF600 + 0x130};
    for (uintptr_t v : vt)
        if (F<uintptr_t>(v) != b + 0x113150) {
            FLOG("FO: Renderer vtable slot +130h at %llX is not 113150", (unsigned long long)(v - b));
            ok = false;
        }
    const uintptr_t tab = b + 0x3C02220;
    static const uint32_t kPPRva[kNPP] = {0x9AFDF0, 0x9A62FC, 0x7F64B4, 0x2F0D750, 0x2F0D774, 0x891F24, 0x2F0D684};
    for (int i = 0; i < kNPP; ++i)
        if (F<uintptr_t>(tab + kPPToks[i] * 8ull) != b + kPPRva[i]) {
            FLOG("FO: token table entry %02X is not %X", kPPToks[i], kPPRva[i]);
            ok = false;
        }
    if (!ok) {
        FLOG("FO: not installed (byte / table check failed)");
        return;
    }
    const auto* dos = (const IMAGE_DOS_HEADER*)b;
    const auto* nt = (const IMAGE_NT_HEADERS64*)(b + dos->e_lfanew);
    g_relays = AllocNear(b, b + nt->OptionalHeader.SizeOfImage, 4096);
    if (!g_relays) {
        FLOG("FO: not installed (no executable page within 2 GB of the image)");
        return;
    }
    g_notFreed = (NotFreedFn)(g_relays + 2048);
    BuildNotFreedStub(g_relays + 2048, &g_prevGen8, reinterpret_cast<volatile uint64_t*>(&g_issued),
                      reinterpret_cast<volatile uint64_t*>(&g_tailDone));
    for (const Site& s : sites) {
        Patch& pt = g_patch[g_nPatch++];
        pt.at = (uint8_t*)(b + s.rva);
        pt.n = s.op == 0xFF ? 6 : 5;
        memcpy(pt.orig, pt.at, pt.n);
        uint8_t* r = Relay(s.fn);
        const int32_t rel = (int32_t)((intptr_t)r - (intptr_t)(pt.at + 5));
        pt.neu[0] = s.op == 0xFF ? (uint8_t)0xE8 : s.op;
        memcpy(pt.neu + 1, &rel, 4);
        pt.neu[5] = 0x90;
        pt.name = s.name;
    }
    {
        Patch& pt = g_patch[g_nPatch++];
        pt.at = (uint8_t*)(b + 0x58C434);
        pt.n = 12;
        memcpy(pt.orig, pt.at, 12);
        uint8_t j[12] = {0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xE0};  // mov rax, stub; jmp rax (rax is clobbered by the original)
        const uint64_t a2 = (uint64_t)(uintptr_t)g_notFreed;
        memcpy(j + 2, &a2, 8);
        memcpy(pt.neu, j, 12);
        pt.name = "HandleSlotIsNotFreed 58C434";
    }
    for (int i = 0; i < kNPP; ++i) {
        g_ppOrig[i] = (Handler)(b + kPPRva[i]);
        g_ptr[g_nPtr++] = {tab + kPPToks[i] * 8ull, b + kPPRva[i], (uintptr_t)kPPWraps[i]};
    }
    for (uintptr_t v : vt) g_ptr[g_nPtr++] = {v, b + 0x113150, (uintptr_t)&FoNode};
    FlushInstructionCache(GetCurrentProcess(), g_relays, 4096);
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    g_tickMs = 1000.0 / (double)f.QuadPart;
    g_ready = true;
    FLOG("FO: frame overlap ready (%d code sites, 7 token handlers, 2 vtable slots verified); nothing is patched until the switch "
         "is ON; FO_Pct %d (live: Tuning.FO / Tuning.FO_Pct)",
         g_nPatch, g_pct.load());
    if (key == 1) {
        EnterCriticalSection(&g_instCs);
        Install();
        LeaveCriticalSection(&g_instCs);
        g_on = g_installed ? 1 : 0;
    }
}

bool SetTuning(const char* key, const char* val, char* reply, size_t n) {
    const bool isFo = !_stricmp(key, "FO"), isPct = !_stricmp(key, "FO_Pct");
    if (!isFo && !isPct) return false;
    if (!g_ready) {
        snprintf(reply, n, "%s: FO not available ([GpuOpt] FrameOverlap = 1 or 2 at boot, v2.02, byte checks)", key);
        return true;
    }
    const int v = atoi(val);
    if (isFo) {
        if (v && !g_installed) {
            EnterCriticalSection(&g_instCs);
            Install();
            LeaveCriticalSection(&g_instCs);
        }
        g_on = v && g_installed ? 1 : 0;  // sampled at the next frame end
        snprintf(reply, n, "FO = %d%s", g_on.load(), g_disabled.load() ? " (but OFF for this session: see log)" : "");
    } else {
        if (v < 0 || v > 90) {
            snprintf(reply, n, "FO_Pct must be 0..90 (got %s)", val);
            return true;
        }
        g_pct = v;
        snprintf(reply, n, "FO_Pct = %d", v);
    }
    if (g_log) g_log("LIVE tuning: %s", reply);
    return true;
}

void StatusLine(char* buf, unsigned n, unsigned secs) {
    if (!n) return;
    buf[0] = 0;
    if (!g_installed) return;
    static int64_t p[13] = {};
    const int64_t c[13] = {s_fo, s_stock, s_span, s_barrierTicks, s_leadTicks, s_tailTicks, s_ppWaitTicks, s_ppWaits, s_subSkip,
                           s_barrierEarly, s_barrierLate, s_prevGenHits, s_bbChecks};
    int64_t d[13];
    for (int i = 0; i < 13; ++i) d[i] = c[i] - p[i], p[i] = c[i];
    const double fo = d[0] ? (double)d[0] : 1.0;
    snprintf(buf, n,
             "FO: switch %s%s | frames FO %.1f/s stock %.1f/s (span-fallback %lld) | per FO frame: barrier wait %.2f ms, GPU-lead wait "
             "%.2f ms, TAIL %.2f ms | barrier at K (node %u) %lld, at frame end %lld | PP lock waits %lld (%.2f ms) | sub-stream "
             "resets skipped %lld | prev-gen handle checks %lld | back-buffer checks %lld",
             g_on.load() ? "ON" : "off", g_disabled.load() ? " (session OFF)" : "", d[0] / (double)std::max(1u, secs),
             d[1] / (double)std::max(1u, secs), (long long)d[2], d[3] * g_tickMs / fo, d[4] * g_tickMs / fo, d[5] * g_tickMs / fo, g_K,
             (long long)d[9], (long long)d[10], (long long)d[7], d[6] * g_tickMs, (long long)d[8], (long long)d[11], (long long)d[12]);
}

#ifdef FO_TEST
void TestInstall(uintptr_t fakeBase, const Engine* e) {
    E = *e;
    g_base = fakeBase;
    g_installed = true;
    g_ready = true;
    uint8_t* page = (uint8_t*)VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    BuildNotFreedStub(page, &g_prevGen8, reinterpret_cast<volatile uint64_t*>(&g_issued), reinterpret_cast<volatile uint64_t*>(&g_tailDone));
    FlushInstructionCache(GetCurrentProcess(), page, 4096);
    g_notFreed = (NotFreedFn)page;
    g_on = 1;
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    g_tickMs = 1000.0 / (double)f.QuadPart;
}
void TestPresent(uintptr_t dev) { FoPresent(dev, 0); }
void TestNode(uintptr_t renderer) { FoNode(renderer, 0); }
uint32_t TestGetBB(void* swapchain) { return FoGetBB(swapchain); }
void TestPPUpdate(uintptr_t pool, uintptr_t dev) { FoPPUpdate(pool, dev); }
uint64_t TestNotFreed(int64_t* p, uint64_t h) { return g_notFreed(p, h); }
const uint8_t* TestStub(size_t* n) {
    static uint8_t tmp[256];
    *n = BuildNotFreedStub(tmp, &g_prevGen8, reinterpret_cast<volatile uint64_t*>(&g_issued), reinterpret_cast<volatile uint64_t*>(&g_tailDone));
    return tmp;
}
void TestTail(void* lc, void* rec) { TailFn(lc, static_cast<FrameRec*>(rec)); }
uint64_t TestTailDone() { return g_tailDone.load(); }
void TestPPHandler(uintptr_t ctx, uint8_t** cur, void (*orig)(uintptr_t, uint8_t**)) {
    g_ppOrig[0] = (Handler)orig;
    PPWrap<0>(ctx, cur);
}
uint64_t TestIssued() { return g_issued.load(); }
uint32_t TestPrevGen() { return g_prevGen; }
bool TestDisabled() { return g_disabled.load() != 0; }
#endif

}  // namespace frameoverlap
