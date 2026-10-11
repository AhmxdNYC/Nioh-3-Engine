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
#include <cstdarg>
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
// Back-buffer anchor (fo7): the last REAL index and the frame it belongs to, {frame << 8 | idx}. Written by TAIL N (the index it
// read for frame N after present N-1 retired) and by FoGetBB whenever it reads the real index (frame begin after a stock frame).
// An FO frame begin predicts idx(anchor) + (F - frame(anchor)): re-anchored every frame, so one present that does not rotate the
// chain (fo6pc: predicted 0, real 2 after 3556 good frames) costs at most two mispredicted frames instead of drifting forever.
std::atomic<uint64_t> g_bbAnchor{~0ull};
DWORD g_mainTid = 0, g_replayTid = 0;  // for the watchdog's thread dump

struct FrameRec {
    uint64_t frame;
    uint64_t fence;
    uint32_t slot;  // the frame slot the frame was recorded in (main advances +4730h at E; TAIL resolves / stores for this one)
    uint32_t pad2;
    volatile LONG fenceValid;
    LONG predValid;
    uint32_t predIdx;
    uint8_t forceOff;  // test (Tuning.FO_Test 1 / 2): TAIL of this frame switches FO off for the session
    uint8_t forced;    // test (Tuning.FO_Test 2 / 4): the prediction of this frame was deliberately wrong
    uint16_t pad;
    // back-buffer handshake for THIS frame (fo7): 0 = nothing yet; 1 = main began the frame with predIdx (TAIL of the previous
    // frame must verify it before this frame replays); 2 = TAIL of the previous frame published realIdx first (main uses it)
    volatile LONG bbState;
    uint32_t realIdx;
    uint32_t pflags;  // present params snapshot taken at E (flags u32 at +4220h, sync interval +424Ch): TAIL presents with these
    int psync;
};
FrameRec g_rec[4] = {};
void ClearRec(uint64_t f) {
    FrameRec& r = g_rec[f & 3];
    r.predValid = 0;
    r.forceOff = 0;
    r.forced = 0;
    r.realIdx = 0;
    InterlockedExchange(&r.bbState, 0);
}
uint64_t g_frame = 0;                       // last frame number given by E (FO or stock)
std::atomic<uint64_t> g_issued{0};          // last frame whose TAIL was issued
std::atomic<uint64_t> g_tailDone{0};        // last frame whose TAIL finished (replay thread)
bool g_barrierPending = false;
uint32_t g_prevGen = 0xFFu;
volatile uint8_t g_prevGen8 = 0xFF;  // read by the HandleSlotIsNotFreed stub
uint32_t g_maxIdx = 0, g_K = 0;
// sub-stream recorders captured at E
constexpr int kMaxSub = 64;
// Sub-streams (GfxDevice+74D8h[+3E28h], recorder = sub+60h when sub+55h; recorder +14h reserve, +18h base, +20h end, +28h write,
// +30h last kick). Stock resets every active recorder at Present (253F2C -> 2543A0: write = last kick = base + per-frame stats),
// so a sub-stream never reaches its own wrap path in practice. FO cannot reset at E (frame N's 0x90 tokens still reference the
// data until TAIL N) and must not reset later (effect workers append for frame N+1 before the barrier). So each recorder is split
// in two halves like the main stream: at E the recorder switches to its other half (+18h / +20h moved, then the stock 2543A0
// reset), the old half stays intact until TAIL N is done (always true at the next E). PC fo4pc / fo5pc: without this the
// sub-streams filled after 260 frames and the never-used wrap path replayed overwritten data (AV in 119B54).
struct SubHalves {
    uintptr_t rec, lo, mid, hi;
};
SubHalves g_subH[kMaxSub] = {};
int g_subHN = 0;
std::atomic<int64_t> s_subSwitch{0};
std::atomic<int64_t> s_subCross{0};
// 1 = switched; 0 = a recorder too small to split (FO off); -1 = a recorder's frame data crosses its middle (stock frame end)
int SubStreamsSwitchPass(uintptr_t dev, bool apply);
int SubStreamsSwitch(uintptr_t dev) {
    const int r = SubStreamsSwitchPass(dev, false);  // check every recorder before switching any
    return r == 1 ? SubStreamsSwitchPass(dev, true) : r;
}
int SubStreamsSwitchPass(uintptr_t dev, bool apply) {  // main, at E (workers idle, as at stock's Present reset)
    const uintptr_t arr = F<uintptr_t>(dev + 0x74D8);
    if (!arr) return 1;
    const uint32_t cnt = std::min<uint32_t>(F<uint32_t>(dev + 0x3E28), kMaxSub);
    for (uint32_t i = 0; i < cnt; ++i) {
        const uintptr_t sub = F<uintptr_t>(arr + i * 8ull);
        if (!sub || !F<uint8_t>(sub + 0x55)) continue;
        const uintptr_t rec = F<uintptr_t>(sub + 0x60);
        if (!rec) continue;
        SubHalves* h = nullptr;
        for (int k = 0; k < g_subHN; ++k)
            if (g_subH[k].rec == rec) h = &g_subH[k];
        if (!h) {  // first sight: the recorder is still stock-sized; split it
            if (g_subHN >= kMaxSub) return 0;
            const uintptr_t lo = F<uintptr_t>(rec + 0x18), hi = F<uintptr_t>(rec + 0x20);
            const uint32_t reserve = F<uint32_t>(rec + 0x14);
            if (hi <= lo || (hi - lo) / 2 < 8ull * reserve + 4096) {
                FLOG("FO: sub-stream %u recorder %p too small to split (%llu bytes, reserve %u) - no overlap", i, (void*)rec,
                     (unsigned long long)(hi - lo), reserve);
                return 0;
            }
            h = &g_subH[g_subHN++];
            *h = {rec, lo, lo + (((hi - lo) / 2) & ~(uintptr_t)0xF), hi};
            FLOG("FO: sub-stream %u recorder %p split %p | %p | %p (reserve %u)", i, (void*)rec, (void*)h->lo, (void*)h->mid, (void*)h->hi,
                 reserve);
        }
        const uintptr_t w = F<uintptr_t>(rec + 0x28), b0 = F<uintptr_t>(rec + 0x18);
        if (b0 < h->mid && w > h->mid) {  // this frame's data [base, write) crosses the middle (first FO frame after stock
            if (++s_subCross <= 4)        // frames: full extent) - either half may still hold unreplayed data: stock frame end
                FLOG("FO: sub-stream %u data %p..%p crosses its middle %p - stock frame end (no overlap this frame)", i, (void*)b0, (void*)w,
                     (void*)h->mid);
            return -1;
        }
        if (!apply) continue;
        const bool inLo = w <= h->mid && b0 < h->mid;
        F<uintptr_t>(rec + 0x18) = inLo ? h->mid : h->lo;
        F<uintptr_t>(rec + 0x20) = inLo ? h->hi : h->mid;
        E.subReset(sub);  // stock 253F2C -> 2543A0: write = last kick = the new base, per-frame stats rolled
        ++s_subSwitch;
    }
    return 1;
}


SRWLOCK g_pp = SRWLOCK_INIT;  // PagePool frame protocol lock (main: E7 / PagePoolFrameUpdate / 7D9C24; replay: PP tokens)

// stats
std::atomic<int64_t> s_noAdvance{0};
std::atomic<int64_t> s_fo{0}, s_stock{0}, s_span{0}, s_barrierTicks{0}, s_leadTicks{0}, s_tailTicks{0}, s_ppWaitTicks{0},
    s_ppWaits{0}, s_subSkip{0}, s_barrierEarly{0}, s_barrierLate{0}, s_prevGenHits{0}, s_bbChecks{0};
std::atomic<int64_t> s_bbMiss{0}, s_stockOff{0}, s_stockOther{0}, s_stockSpin{0}, s_direct{0};
double g_tickMs = 0.0;

// ---- live test actions (Tuning.FO_Test; notes\renderer\frame-overlap.md section 7 fo7) ---------------------------------------
// 1 = session OFF from the next TAIL (replay thread), the path fo6pc took; 2 = deliberately wrong back-buffer prediction at the
// next FO frame begin AND session OFF at that frame's TAIL (fo6pc exactly); 3 = the main thread stalls 5.6 s at the next FO frame
// end -> the watchdog fires (session OFF + every patch removed) -> stock; 4 = wrong prediction only (must be tolerated);
// 5 / 6 = switch off / on (as live Tuning.FO); 9 = re-arm (TEST ONLY: clears the session OFF / watchdog state, reinstalls,
// switch ON); 100 = self-test sequence 5 6 4 1 9 2 9 3 9 with a frames-advance check after each step.
std::atomic<int> g_tForceOff{0}, g_tMispredict{0}, g_tStall{0}, g_wdSuppress{0};

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

// ---- back buffer: verified BEFORE the next frame replays (fo7) ----------------------------------------------------------------
// PC fo6pc / fo7pc: a frame recorded into a back buffer that is not the swapchain's current one removes the D3D12 device within two
// frames (ID3D12Resource::Map -> DXGI_ERROR_DEVICE_REMOVED in 4DF64 -> the engine's fatal handler 18956D0 sits in a modal error
// box on the main thread; the tile job waits in the same handler holding the cache SRW +CF8h shared = fo6pc's "hang"). So a
// predicted index is never allowed to reach the GPU unverified: frame N+1's tokens replay only after TAIL N, and TAIL N, after
// present N has retired, reads the REAL index of frame N+1. Main either uses it (TAIL N was first) or began N+1 with a prediction
// that TAIL N now checks; a wrong prediction is fixed before anything of N+1 replays by presenting the current buffer again (it is
// in PRESENT state and holds an older frame: one repeated image) until the chain stands on the predicted buffer.
using U32PtrFn = HRESULT(STDMETHODCALLTYPE*)(void*, UINT*);
std::atomic<int64_t> s_bbResync{0}, s_bbExtraPresents{0}, s_bbExact{0};

UINT CurrentIdx(void* sc) { return V<GetIdxFn>(sc, 0x120)(sc); }

// one present of the current back buffer (replay thread, TAIL), retired before it returns
void ExtraPresent(uintptr_t dev, void* sc, const FrameRec* r) {
    const int flags = (int)(r->pflags & ~1u);  // never DXGI_PRESENT_TEST: it must rotate the chain
    ++s_bbExtraPresents;
    if (!F<uint8_t>(dev + kPresentThr)) {
        const HRESULT hr = V<PresentFn>(sc, 0x40)(sc, 0, (UINT)flags);
        if (FAILED(hr)) FLOG("FO: extra present failed (%08lX)", (unsigned long)hr);
        return;
    }
    E.spinPresent(dev);
    F<uint8_t>(dev + kPresentPending) = 1;
    F<void*>(dev + kPresentSc) = sc;
    F<int>(dev + kPresentA) = 0;
    F<int>(dev + kPresentB) = flags;
    SetEvent(F<HANDLE>(dev + kPresentEv));
    E.spinPresent(dev);
}

// TAIL N, after present N was handed off: publish / verify frame N+1's index
void VerifyNext(uintptr_t dev, void* sc, const FrameRec* r) {
    if (F<uint8_t>(dev + kPresentThr)) E.spinPresent(dev);  // present N retired: the index now is frame N+1's
    FrameRec& n = g_rec[(r->frame + 1) & 3];
    UINT idx = CurrentIdx(sc);
    if (g_tMispredict.load()) {  // test: let main predict first, so the injected wrong prediction is caught here (never rendered)
        for (int i = 0; i < 2000 && n.bbState == 0; ++i) Sleep(0);
    }
    n.realIdx = idx;
    const LONG prev = InterlockedCompareExchange(&n.bbState, 2, 0);
    if (prev == 1) {  // main began N+1 with a prediction: it must be right before N+1 replays
        if (n.predIdx != idx) {
            const int64_t k = ++s_bbResync;
            const UINT before = idx;
            UINT pc0 = 0;
            V<U32PtrFn>(sc, 0x88)(sc, &pc0);
            int presents = 0;
            for (; presents < 6 && idx != n.predIdx; ++presents) {
                ExtraPresent(dev, sc, r);
                idx = CurrentIdx(sc);
            }
            UINT pc1 = 0;
            V<U32PtrFn>(sc, 0x88)(sc, &pc1);
            if (k <= 8 || (k & (k - 1)) == 0)
                FLOG("FO: back-buffer resync #%lld before frame %llu replays: predicted %u, real %u%s -> %d extra present(s) -> %u (%s) | "
                     "DXGI present count %u -> %u | buffers %u | present flags %08X sync %d, present thread %u",
                     (long long)k, (unsigned long long)(r->frame + 1), n.predIdx, before, n.forced ? " (test: forced)" : "", presents, idx,
                     idx == n.predIdx ? "matches" : "STILL WRONG", pc0, pc1, g_bufCount, r->pflags, r->psync, F<uint8_t>(dev + kPresentThr));
            if (idx != n.predIdx) Disable(11, "back-buffer chain could not be brought to the predicted index");
            n.realIdx = idx;
        }
    } else {
        ++s_bbExact;
    }
    g_bbAnchor.store(((r->frame + 1) << 8) | (idx & 0xFF), std::memory_order_release);
}

void __fastcall TailFn(void* /*{list, ctx}*/, FrameRec* r) {
    const uintptr_t dev = g_dev;
    g_replayTid = GetCurrentThreadId();
    const uint64_t t0 = Now(), q0 = E.qpc();
    if (F<uint8_t>(dev + kPresentThr)) E.spinPresent(dev);  // present N-1 retired (normally already by TAIL N-1)
    TimestampResolve(dev, r->slot);
    void* sc = F<void*>(dev + kSwap);  // FO frames never carry a swapchain / resource override (E sends those to the stock path)
    const UINT idx = CurrentIdx(sc);
    if (r->predValid) {
        ++s_bbChecks;
        if (idx != r->predIdx) {  // cannot happen after VerifyNext (TAIL N-1 checked it): the frame is already on the GPU
            ++s_bbMiss;
            FLOG("FO: ERROR: frame %llu was recorded for back buffer %u but the current one is %u - FO off for the session",
                 (unsigned long long)r->frame, r->predIdx, idx);
            Disable(4, "back buffer differs from the verified index");
        }
    }
    const uintptr_t res = F<uintptr_t>(dev + kBB + idx * 8ull);
    if (r->forceOff) Disable(10, "test: forced by Tuning.FO_Test (from the TAIL, as fo6pc)");
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
    // present hand-off (stock 3D62E4 tail) with the params E snapshotted (E keeps TEST presents and overrides on the stock path)
    const int sync = r->psync;
    const int flags = (int)r->pflags;
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
    VerifyNext(dev, sc, r);
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

// Put the device back in the exact stock situation before a stock GfxDevicePresent (every stock frame end: switch off, session
// OFF, fallbacks, the other Present callers):
//  - TAIL of the last FO frame done (barrier),
//  - present of the last FO frame retired when the present thread is used: stock GfxDevicePresent reads GetCurrentBackBufferIndex
//    (3D6474) BEFORE its own present spin, relying on WaitSubmitThread's spin at frame begin - which FO skipped for the frame
//    that follows an FO frame. Without this the first stock Present after an FO frame read the index while present N was still
//    being presented on the present thread (fo7 finding),
//  - main stream end and sub-stream recorders back to their full extent (FO confined them to a half).
void PrepareStock(uintptr_t dev) {
    const bool afterFO = g_lastWasFO;
    FlushAll();
    if (dev != g_dev) return;
    if (afterFO && F<uint8_t>(dev + kPresentThr)) {
        ++s_stockSpin;
        E.spinPresent(dev);
    }
    if (g_init && F<uintptr_t>(dev + kEnd) == g_sMid) F<uintptr_t>(dev + kEnd) = g_sEnd;  // FO frames use one half each
    for (int k = 0; k < g_subHN; ++k) {  // full extent again; stock's Present resets write to the base after its drain
        F<uintptr_t>(g_subH[k].rec + 0x18) = g_subH[k].lo;
        F<uintptr_t>(g_subH[k].rec + 0x20) = g_subH[k].hi;
    }
}

// last frame ends, for the replay-fault report (main thread writes, the fault handler reads)
struct EndRec {
    uint64_t frame;
    uintptr_t start, write, tail;
    uint32_t fo, gen;
};
EndRec g_ends[8] = {};
volatile LONG g_endN = 0;
void NoteEnd(uint64_t frame, uintptr_t start, uintptr_t write, uintptr_t tail, uint32_t fo, uint32_t gen) {
    const LONG i = InterlockedIncrement(&g_endN);
    g_ends[i & 7] = {frame, start, write, tail, fo, gen};
}

void StockPresent(uintptr_t dev, uintptr_t p2) {
    PrepareStock(dev);
    ++s_stock;
    if (dev == g_dev) NoteEnd(g_frame + 1, 0, F<uintptr_t>(dev + kWrite), 0, 0, F<uint32_t>(dev + kGen));
    E.present(dev, p2);
    ++g_frame;
    ClearRec(g_frame + 1);
}

void __fastcall FoPresent(uintptr_t dev, uintptr_t p2) {
    ++g_presents;
    if (!g_dev) g_dev = dev;
    g_mainTid = GetCurrentThreadId();
    if (g_tStall.load() && g_lastWasFO && g_on.load() && !g_disabled.load()) {
        // test (Tuning.FO_Test 3 / 7 / 10): a main-thread stall at an FO frame end -> the watchdog fires (session OFF, every patch
        // restored while this thread sleeps in FO code, outside every patched byte); afterwards this frame ends on the stock path.
        // 7 = the same stall with the watchdog suppressed (is a stall alone harmful?), 10 = 8 s (main wakes after the restore)
        const int m = g_tStall.exchange(0);
        const DWORD ms = m == 3 ? 8000 : 5600;
        if (m == 2) g_wdSuppress = 1;
        FLOG("FO: test: main thread stalls %lu ms at the end of frame %llu%s", (unsigned long)ms, (unsigned long long)(g_frame + 1),
             m == 2 ? " (watchdog suppressed)" : " (watchdog test)");
        Barrier(false);
        Sleep(ms);
        g_wdSuppress = 0;
    }
    if (p2 || dev != g_dev || !g_on.load() || g_disabled.load() || !Init(dev)) {
        if (p2 || dev != g_dev) ++s_stockOther;
        else ++s_stockOff;
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
    const uintptr_t pp = dev + kPresentParams;
    {  // stock does not advance the slot when the present flag bit 0 is set without a swapchain override: keep stock there.
       // A swapchain / resource override (+4228h) presents elsewhere: the main chain would not rotate -> stock frame end too.
        if ((F<uint32_t>(pp) & 1) && !F<uintptr_t>(pp + 8)) {
            ++s_noAdvance;
            StockPresent(dev, p2);
            return;
        }
        if (F<uintptr_t>(pp + 8)) {
            ++s_stockOther;
            StockPresent(dev, p2);
            return;
        }
    }
    const uint64_t N = g_frame + 1;
    // sub-streams: switch every active recorder to its other half (frame N's 0x90 data stays until TAIL N); a recorder too small
    // to split -> stock frame end (it resets them) and FO off
    if (const int ss = SubStreamsSwitch(dev); ss != 1) {
        if (ss == 0) Disable(9, "a sub-stream is too small to split");
        StockPresent(dev, p2);
        return;
    }
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
    r.forceOff = (uint8_t)(r.forceOff || g_tForceOff.exchange(0));  // (FO_Test 2 marks it at frame begin, 1 here)
    r.pflags = F<uint32_t>(pp);  // TAIL presents with what E saw (bit 0 / TEST is excluded above)
    r.psync = F<int>(dev + kSyncInterval);
    ClearRec(N + 1);  // the next frame's record, BEFORE the TAIL token exists: TAIL N publishes frame N+1's index into it
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
    NoteEnd(N, start, write, write, 1, gen);
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
    g_barrierPending = true;
    g_lastWasFO = true;
    g_K = (uint32_t)((uint64_t)g_maxIdx * (uint32_t)std::clamp(g_pct.load(), 0, 90) / 100u);
    g_maxIdx = 0;
    ++s_fo;
}

// ---- call-site wrappers -------------------------------------------------------------------------------------------------
void __fastcall FoPresentDirect(uintptr_t dev, uintptr_t p2) {  // other GfxDevicePresent callers (init / loading paths)
    ++g_presents;
    ++s_direct;
    PrepareStock(dev);
    E.present(dev, p2);
    ++g_frame;
    ClearRec(g_frame + 1);
}
void __fastcall FoPresentSpin(uintptr_t dev) {  // WaitSubmitThread 7D9B91
    if (g_lastWasFO) return;                     // TAIL N spins for present N-1 itself before it hands off present N
    E.spinPresent(dev);
}
UINT __fastcall FoGetBB(void* sc) {  // WaitSubmitThread 7D9BC7 (was call [rax+120h] = GetCurrentBackBufferIndex)
    // a second frame begin without a frame end in between keeps the predicted buffer. ONLY on the predicted path: g_frame is FO's own
    // count of frame ends - once the watchdog restored 5DAF3 (FoPresent) it stops, and a cached index then went stale while the
    // chain rotated (fo7pc-selftest4/5: frames rendered into a compositor-owned buffer -> device removed, 887A002B ACCESS_DENIED)
    if (g_lastWasFO && g_bbFrame == g_frame) return g_mainIdx;
    g_bbFrame = g_frame;
    const uint64_t F1 = g_frame + 1;  // the frame being begun
    const uint64_t a = g_bbAnchor.load(std::memory_order_acquire);
    if (g_lastWasFO) {
        // frame N = F1-1 is an FO frame: present N has not happened yet (TAIL N does it). TAIL N publishes the real index of F1
        // after present N retired; if it did already, use it. Otherwise predict from the newest real index (one rotation per frame
        // since) and leave it to TAIL N to verify - and fix - before anything of F1 replays (VerifyNext).
        FrameRec& r = g_rec[F1 & 3];
        uint32_t idx = 0;
        if (r.bbState == 2 || !g_bufCount) {
            while (r.bbState != 2) Sleep(0);  // (no buffer count yet: wait for TAIL N - never on the hot path)
            idx = r.realIdx;
        } else {
            const bool anchored = a != ~0ull && (a >> 8) < F1 && F1 - (a >> 8) <= 3;
            idx = anchored ? (uint32_t)(((a & 0xFF) + (F1 - (a >> 8))) % g_bufCount) : (g_mainIdx + 1) % g_bufCount;
            const int m = g_tMispredict.load() ? g_tMispredict.exchange(0) : 0;  // test (FO_Test 2 / 4)
            if (m) {
                idx = (idx + 1) % g_bufCount;
                r.forced = 1;
                if (m == 2) r.forceOff = 1;
            }
            r.predIdx = idx;
            if (InterlockedCompareExchange(&r.bbState, 1, 0) == 2) {  // TAIL N was faster: take the real one
                idx = r.realIdx;
                if (m) {  // the test needs a frame where main predicts first: try again next frame
                    r.forced = 0;
                    r.forceOff = 0;
                    g_tMispredict = m;
                }
            } else if (m) {
                FLOG("FO: test: frame %llu begins with a deliberately wrong back-buffer index %u%s", (unsigned long long)F1, idx,
                     m == 2 ? " + session OFF at its TAIL (the fo6pc sequence)" : "");
            }
        }
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
    g_bbAnchor.store((F1 << 8) | (idx & 0xFF), std::memory_order_release);
    return idx;
}
std::atomic<int> g_ppWaitTail{2};  // Tuning.FO_PPWait: 2 = B6 waits for TAIL N (default), 1 = B4 + B6, 0 = no wait (unsafe)
void __fastcall FoPPUpdate(uintptr_t pool, uintptr_t dev) {  // 7D9BA3 PagePoolFrameUpdate
    if (g_ppWaitTail.load() & 1) Barrier(true);  // FO_PPWait: 1 = B4 + B6 wait for TAIL N, 2 = B6 only
    AcquireSRWLockExclusive(&g_pp);
    E.ppUpdate(pool, dev);
    ReleaseSRWLockExclusive(&g_pp);
}
void __fastcall FoPPOps(uintptr_t pool, uintptr_t dev) {  // 7D9C07 7D9C24
    if (g_ppWaitTail.load()) Barrier(true);  // FO_PPWait 1 or 2
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
std::atomic<int> g_replayDelayUs{0};  // test: Tuning.FO_ReplayDelayUs - replay-thread lag like a CPU-bound pc2 (busy wait)
template <int I>
void __fastcall PPWrap(uintptr_t ctx, uint8_t** cur) {
    if constexpr (I == 0) {  // tok 0x82 = start of every frame's PagePool ops, i.e. near the start of each frame's replay
        if (const int us = g_replayDelayUs.load()) {
            const uint64_t t0 = Now(), wait = (uint64_t)((double)us / (g_tickMs * 1000.0));
            while (Now() - t0 < wait) YieldProcessor();
        }
    }
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

uintptr_t g_base = 0, g_imgSize = 0x5000000;
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

const char* Where(uintptr_t a, char* buf, size_t n) {
    if (a >= g_sBase && a < g_sEnd) {
        snprintf(buf, n, "main stream %s half +%llX", a < g_sMid ? "LOW" : "HIGH", (unsigned long long)(a - (a < g_sMid ? g_sBase : g_sMid)));
        return buf;
    }
    for (int k = 0; k < g_subHN; ++k)
        if (a >= g_subH[k].lo && a < g_subH[k].hi) {
            snprintf(buf, n, "sub-stream recorder %d %s half +%llX (recorder base %p end %p write %p)", k, a < g_subH[k].mid ? "LOW" : "HIGH",
                     (unsigned long long)(a - (a < g_subH[k].mid ? g_subH[k].lo : g_subH[k].mid)), F<void*>(g_subH[k].rec + 0x18),
                     F<void*>(g_subH[k].rec + 0x20), F<void*>(g_subH[k].rec + 0x28));
            return buf;
        }
    snprintf(buf, n, "outside the main stream and the split sub-streams");
    return buf;
}
int Unwind(CONTEXT* c, uintptr_t* frames, int max);
std::atomic<int> g_vehLogged{0};
LONG CALLBACK ReplayFault(PEXCEPTION_POINTERS ep) {
    CONTEXT* c = ep->ContextRecord;
    __try {
        if (!g_base || ep->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION) return EXCEPTION_CONTINUE_SEARCH;
        const uintptr_t ret = *reinterpret_cast<uintptr_t*>(c->Rsp);
        bool atCall = ret == g_base + 0x1A7C467;
        if (!atCall && GetCurrentThreadId() != g_replayTid) return EXCEPTION_CONTINUE_SEARCH;
        uintptr_t dispRbp = atCall ? c->Rbp : 0;
        if (!atCall && c->Rip >= g_base + 0x1A7C3E0 && c->Rip < g_base + 0x1A7C4C0) {  // in the dispatcher loop itself (e.g. the
            dispRbp = c->Rbp;                                                            // table read of a garbage opcode)
            atCall = true;
        }
        if (!atCall) {  // a fault deeper in a token handler: unwind to the dispatcher frame (its rbp holds the cursor pair)
            CONTEXT u = *c;
            uintptr_t fr[40];
            for (int k = 0; k < 40; ++k) {
                if (Unwind(&u, fr, 2) < 2) break;  // one step: fr[1] = the caller's rip, u = the caller's context
                if (u.Rip == g_base + 0x1A7C467) {
                    dispRbp = u.Rbp;
                    atCall = true;
                    break;
                }
            }
        }
        if (g_vehLogged.exchange(1)) return EXCEPTION_CONTINUE_SEARCH;
        FLOG("FO: REPLAY FAULT on tid %lu: code %08lX at %p (return %p%s) | FO frame %llu, TAIL issued %llu done %llu, session %s",
             GetCurrentThreadId(), (unsigned long)ep->ExceptionRecord->ExceptionCode, (void*)c->Rip, (void*)ret,
             atCall ? " = dispatcher token call 1A7C463" : "", (unsigned long long)g_frame, (unsigned long long)g_issued.load(),
             (unsigned long long)g_tailDone.load(), g_disabled.load() ? "OFF" : "on");
        if (atCall) {
            const uintptr_t rbp = dispRbp;
            const uintptr_t cur = F<uintptr_t>(rbp - 0x30), pair = F<uintptr_t>(rbp - 0x28), beg = F<uintptr_t>(rbp - 0x18),
                            end = F<uintptr_t>(rbp - 0x10), ctx = F<uintptr_t>(rbp - 0x20);
            char w1[256], w2[256], w3[256];
            FLOG("FO:   token %04X at %p [%s] | pair %p (%s) -> end %p | ring item ctx %p begin %p [%s] end %p [%s]",
                 F<uint16_t>(cur), (void*)cur, Where(cur, w1, sizeof w1), (void*)pair, pair == rbp - 0x18 ? "main stream item" : "redirected (sub-stream)",
                 F<void*>(pair + 8), (void*)ctx, (void*)beg, Where(beg, w2, sizeof w2), (void*)end, Where(end, w3, sizeof w3));
            char hex[600];
            size_t o = 0;
            for (int k = -48; k < 48 && o + 12 < sizeof hex; k += 4)
                o += (size_t)snprintf(hex + o, sizeof hex - o, "%s%08X", k == 0 ? " >" : " ", F<uint32_t>(cur + k));
            FLOG("FO:   bytes around the cursor:%s", hex);
            // the first pointer-looking qwords of the token payload (texture / resource objects for binds): their first bytes
            for (int k = 4, shownp = 0; k < 64 && shownp < 3; k += 4) {
                const uintptr_t q = F<uintptr_t>(cur + k);
                if (q < 0x10000 || q > 0x7FFFFFFFFFFFull || (q & 7)) continue;
                o = 0;
                for (int m = 0; m < 64 && o + 20 < sizeof hex; m += 8) o += (size_t)snprintf(hex + o, sizeof hex - o, " %016llX", F<unsigned long long>(q + m));
                FLOG("FO:   payload +%d -> %p:%s", k, (void*)q, hex);
                ++shownp;
            }
        }
        FLOG("FO:   main stream: write %p last kick %p end %p gen %u | halves %p %p %p", F<void*>(g_dev + kWrite), F<void*>(g_dev + kLastKick),
             F<void*>(g_dev + kEnd), F<uint32_t>(g_dev + kGen), (void*)g_sBase, (void*)g_sMid, (void*)g_sEnd);
        for (int k = 0; k < g_subHN; ++k)
            FLOG("FO:   sub-stream recorder %d: split %p | %p | %p, now base %p end %p write %p last kick %p high %p", k, (void*)g_subH[k].lo,
                 (void*)g_subH[k].mid, (void*)g_subH[k].hi, F<void*>(g_subH[k].rec + 0x18), F<void*>(g_subH[k].rec + 0x20),
                 F<void*>(g_subH[k].rec + 0x28), F<void*>(g_subH[k].rec + 0x30), F<void*>(g_subH[k].rec + 0x38));
        const LONG n = g_endN;
        for (LONG i = n - 7; i <= n; ++i) {
            const EndRec& e = g_ends[i & 7];
            if (!e.frame) continue;
            char w4[256];
            FLOG("FO:   frame end %llu %s gen %u: data %p..%p [%s]%s", (unsigned long long)e.frame, e.fo ? "FO" : "stock", e.gen, (void*)e.start,
                 (void*)e.write, Where(e.write, w4, sizeof w4), e.fo ? " + TAIL token" : "");
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

DWORD WINAPI Watchdog(LPVOID);
bool g_wdStarted = false;
bool Install() {  // under g_instCs
    if (g_installed) return true;
    if (!g_ready) return false;
    int ok = 0;
    for (int i = 0; i < g_nPtr; ++i) ok += WritePtr(g_ptr[i].at, g_ptr[i].neu);
    // GfxDevicePresentVfn 5DAF3 (FoPresent, index 0) is the switch that creates FO frames: written LAST, so no FO frame can end
    // before FO's frame-begin hooks (present spin, back-buffer index, PP lock) are in place (fo7pc-selftest6: installed first,
    // an FO frame ended while WaitSubmitThread still read the index the stock way -> next frame in a non-current buffer)
    for (int i = g_nPatch - 1; i >= 0; --i) {
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
    if (!g_wdStarted) {
        g_wdStarted = true;
        if (HANDLE h = CreateThread(nullptr, 0, Watchdog, nullptr, 0, nullptr)) CloseHandle(h);
        AddVectoredExceptionHandler(1, &ReplayFault);  // logs a replay-thread fault (pc2 fo2: invisible behind the crash box)
    }
    return true;
}
void Restore(const char* why) {  // put every original byte / pointer back (only with the session OFF and no TAIL pending)
    EnterCriticalSection(&g_instCs);
    if (g_installed) {
        // GfxDevicePresentVfn 5DAF3 (FoPresent, index 0) restored LAST: until then every frame end still goes through FO's stock
        // path (PrepareStock: present retired, full stream / sub-stream extents) and its frame-begin hooks see consistent frame
        // numbers; the session is OFF, so it creates no FO frame in between
        for (int i = g_nPatch - 1; i >= 0; --i) FrozenWrite(g_patch[i].at, g_patch[i].orig, g_patch[i].n);
        for (int i = g_nPtr - 1; i >= 0; --i) WritePtr(g_ptr[i].at, g_ptr[i].orig);
        g_installed = false;
        FLOG("FO: all FO patches removed (%s)", why);
    }
    LeaveCriticalSection(&g_instCs);
}

// ---- hang diagnostics: every game thread's stack (watchdog) -------------------------------------------------------------
// Each thread is suspended only for GetThreadContext, then resumed; the unwind (RtlVirtualUnwind over the modules' .pdata) runs
// on the resumed thread's stack: a hung thread's stack does not move, a running one may give a garbage tail (SEH-guarded).
size_t ModName(uintptr_t a, char* out, size_t n, uintptr_t* base) {
    HMODULE m = nullptr;
    *base = 0;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)a, &m) || !m)
        return (size_t)snprintf(out, n, "?");
    wchar_t w[MAX_PATH];
    const DWORD k = GetModuleFileNameW(m, w, MAX_PATH);
    const wchar_t* s = w;
    for (DWORD i = 0; i < k; ++i)
        if (w[i] == L'\\' || w[i] == L'/') s = w + i + 1;
    size_t j = 0;
    for (; *s && *s != L'.' && j + 1 < n && j < 24; ++s) out[j++] = (char)(*s < 128 ? *s : '_');
    out[j] = 0;
    *base = (uintptr_t)m;
    return j;
}
int Unwind(CONTEXT* c, uintptr_t* frames, int max) {  // plain C: SEH only
    int n = 0;
    __try {
        frames[n++] = c->Rip;
        for (; n < max;) {
            DWORD64 imageBase = 0;
            PRUNTIME_FUNCTION fe = RtlLookupFunctionEntry(c->Rip, &imageBase, nullptr);
            if (!fe) {  // leaf
                c->Rip = *reinterpret_cast<DWORD64*>(c->Rsp);
                c->Rsp += 8;
            } else {
                void* hd = nullptr;
                DWORD64 ef = 0;
                RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, c->Rip, fe, c, &hd, &ef, nullptr);
            }
            if (!c->Rip) break;
            frames[n++] = c->Rip;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return n;
}
void DumpThreads(const char* why) {
    const DWORD self = GetCurrentThreadId(), pid = GetCurrentProcessId();
    static DWORD tids[1024];
    int nt = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    THREADENTRY32 te{};
    te.dwSize = sizeof(te);
    for (BOOL ok = Thread32First(snap, &te); ok && nt < 1024; ok = Thread32Next(snap, &te))
        if (te.th32OwnerProcessID == pid && te.th32ThreadID != self) tids[nt++] = te.th32ThreadID;
    CloseHandle(snap);
    const uintptr_t pool = g_dev ? g_dev + kPool : 0;
    auto W = [&](uintptr_t off) -> unsigned long long { return pool ? (unsigned long long)F<uint64_t>(pool + off) : 0ull; };
    FLOG("FO: thread dump (%s): %d threads; main %lu, replay %lu | lock words: FO PP %llX, cache +CC0h %llX +CD0h %llX +CE8h %llX "
         "+CF8h %llX +D00h %llX +D08h %llX | present pending %u, TAIL issued %llu done %llu",
         why, nt, g_mainTid, g_replayTid, (unsigned long long)(uintptr_t)g_pp.Ptr, W(0xCC0), W(0xCD0), W(0xCE8), W(0xCF8), W(0xD00),
         W(0xD08), g_dev ? F<uint8_t>(g_dev + kPresentPending) : 0u, (unsigned long long)g_issued.load(),
         (unsigned long long)g_tailDone.load());
    if (g_dev && F<void*>(g_dev + kDevice)) {
        using HrFn = HRESULT(STDMETHODCALLTYPE*)(void*);
        void* d = F<void*>(g_dev + kDevice);
        FLOG("FO: D3D12 GetDeviceRemovedReason = %08lX", (unsigned long)V<HrFn>(d, 0x128)(d));  // ID3D12Device slot 37
    }
    if (g_dev)
        FLOG("FO: dispatcher ring: cap %u write %u read %u in flight %u | stream write %p last kick %p end %p gen %u",
             F<uint32_t>(g_dev + 0x3C00), F<uint32_t>(g_dev + 0x3C04), F<uint32_t>(g_dev + 0x3C08), F<uint32_t>(g_dev + 0x3C10),
             F<void*>(g_dev + kWrite), F<void*>(g_dev + kLastKick), F<void*>(g_dev + kEnd), F<uint32_t>(g_dev + kGen));
    // main and replay first (pc2 fo2: the replay thread fell behind an 80-line cap), then every thread with a game frame
    for (int i = 0, j = 0; i < nt; ++i)
        if (tids[i] == g_mainTid || tids[i] == g_replayTid) std::swap(tids[i], tids[j++]);
    int shown = 0;
    for (int i = 0; i < nt && shown < 400; ++i) {
        HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, tids[i]);
        if (!h) continue;
        CONTEXT c{};
        c.ContextFlags = CONTEXT_FULL;
        bool got = false;
        if (SuspendThread(h) != (DWORD)-1) {
            got = GetThreadContext(h, &c) != 0;
            ResumeThread(h);
        }
        CloseHandle(h);
        if (!got) continue;
        uintptr_t fr[28];
        const int n = Unwind(&c, fr, 28);
        bool game = tids[i] == g_mainTid || tids[i] == g_replayTid;
        for (int k = 0; k < n && !game; ++k) game = g_base && fr[k] >= g_base && fr[k] < g_base + g_imgSize;
        if (!game) continue;  // driver / OS worker threads without a game frame
        char line[2400];
        size_t o = (size_t)snprintf(line, sizeof line, "FO:  tid %lu%s:", tids[i],
                                    tids[i] == g_mainTid ? " (main)" : tids[i] == g_replayTid ? " (replay)" : "");
        for (int k = 0; k < n && o + 48 < sizeof line; ++k) {
            char mn[32];
            uintptr_t mb = 0;
            if (g_base && fr[k] >= g_base && fr[k] < g_base + g_imgSize) {
                o += (size_t)snprintf(line + o, sizeof line - o, " %llX", (unsigned long long)(fr[k] - g_base));
                continue;
            }
            ModName(fr[k], mn, sizeof mn, &mb);
            o += (size_t)snprintf(line + o, sizeof line - o, " %s+%llX", mn, (unsigned long long)(fr[k] - mb));
        }
        FLOG("%s", line);
        ++shown;
    }
}

// frames rendered: the engine's sync-point frame counter +3C1Ch (stock GfxFrameRecordTick 8BEDC8 and FO's E both advance it once
// per frame) - still counts after the watchdog removed every FO entry point; before the first frame end, FO's own count
uint64_t FrameCount() { return g_dev ? (uint64_t)F<volatile uint32_t>(g_dev + kSyncFrame) + 1 : g_presents.load(); }

DWORD WINAPI Watchdog(LPVOID) {
    uint64_t last = FrameCount();
    DWORD lastChange = GetTickCount();
    int dumps = 0;  // thread dumps in the current no-frame episode
    for (;;) {
        Sleep(250);
        const uint64_t p = FrameCount();
        const DWORD now = GetTickCount();
        if (p != last || p == 0 || g_wdSuppress.load()) {
            if (dumps && p != last) FLOG("FO: WATCHDOG: frames again");
            last = p;
            lastChange = now;
            dumps = 0;
            continue;
        }
        if (now - lastChange > 5000 && !g_wdFired.exchange(1)) {
            FLOG("FO: WATCHDOG: no frame for %lu ms (switch %d, FO frames %lld, stock %lld, TAIL issued %llu done %llu, barrier %s) - "
                 "FO off for the session",
                 (unsigned long)(now - lastChange), g_on.load(), (long long)s_fo.load(), (long long)s_stock.load(),
                 (unsigned long long)g_issued.load(), (unsigned long long)g_tailDone.load(), g_barrierPending ? "pending" : "done");
            DumpThreads("watchdog");
            dumps = 1;
            Disable(8, "watchdog: no frame for 5 s");
            if (g_tailDone.load() >= g_issued.load()) Restore("watchdog, no TAIL pending");
            else if (g_vehLogged.load())
                FLOG("FO: WATCHDOG: TAIL %llu can never finish: the replay thread FAULTED (REPLAY FAULT above) and sits in the engine's crash "
                     "handler - not recoverable from FO; patches kept", (unsigned long long)g_issued.load());
            else FLOG("FO: WATCHDOG: TAIL %llu still pending - patches kept so the pending frame can finish", (unsigned long long)g_issued.load());
        } else if (now - lastChange > 15000 && dumps == 1) {  // still nothing 15 s on: a second dump shows what is really stuck
            DumpThreads("watchdog +15 s");
            dumps = 2;
        }
    }
}

// ---- Tuning.FO_Test (live test actions + self-test) -------------------------------------------------------------------------
void Rearm() {  // TEST ONLY: back to "installed + ON" after a session OFF / watchdog
    g_on = 0;  // no FO frame while (re)installing
    EnterCriticalSection(&g_instCs);
    g_disabled = 0;
    g_disReason = "";
    g_wdFired = 0;
    Install();
    LeaveCriticalSection(&g_instCs);
    g_on = g_installed ? 1 : 0;
    FLOG("FO: test: re-armed (installed %d, switch %d)", (int)g_installed, g_on.load());
}
void TestAction(int v) {
    switch (v) {
    case 1: g_tForceOff = 1; break;
    case 2: g_tMispredict = 2; break;
    case 3: g_tStall = 1; break;
    case 7: g_tStall = 2; break;
    case 10: g_tStall = 3; break;
    case 11: {  // the watchdog's restore in groups while rendering, a frames check after each group: which restore breaks it?
        g_on = 0;
        Sleep(500);
        Disable(13, "test: staged restore");
        struct G {
            const char* name;
            int p[3];
            int q[7];
        };
        static const G groups[] = {{"rare sites 4A53D0 2EE8B92 2EEC4F9", {8, 9, 10}, {-1}},
                                   {"other Present callers 30A6931 32FAE39 32FBDBE", {1, 2, 3}, {-1}},
                                   {"Renderer vtable slots (FoNode)", {-1}, {7, 8, -1}},
                                   {"token table PP wrappers 82 83 95 96 97 98 9A", {-1}, {0, 1, 2, 3, 4, 5, 6}},
                                   {"PagePoolFrameUpdate / 7D9C24 sites 7D9BA3 7D9C07", {5, 7, -1}, {-1}},
                                   {"present spin 7D9B91 + back-buffer index 7D9BC7", {4, 6, -1}, {-1}},
                                   {"HandleSlotIsNotFreed 58C434 stub", {11, -1}, {-1}},
                                   {"GfxDevicePresentVfn 5DAF3 (FoPresent)", {0, -1}, {-1}}};
        for (const G& g : groups) {
            EnterCriticalSection(&g_instCs);
            for (int k = 0; k < 3 && g.p[k] >= 0; ++k) FrozenWrite(g_patch[g.p[k]].at, g_patch[g.p[k]].orig, g_patch[g.p[k]].n);
            for (int k = 0; k < 7 && g.q[k] >= 0; ++k) WritePtr(g_ptr[g.q[k]].at, g_ptr[g.q[k]].orig);
            LeaveCriticalSection(&g_instCs);
            const uint64_t f0 = FrameCount();
            Sleep(2000);
            const uint64_t df = FrameCount() - f0;
            FLOG("FO: test: staged restore: %s -> %llu frames in 2 s%s", g.name, (unsigned long long)df, df < 30 ? " <- FRAMES STOPPED" : "");
            if (df < 30) break;
        }
        g_installed = false;
        break;
    }
    case 8: {  // the watchdog's own path (session OFF + every patch restored) while the game renders normally, no stall
        g_on = 0;
        Sleep(500);  // stock frames: no TAIL pending
        Disable(12, "test: watchdog path without a stall");
        if (g_tailDone.load() >= g_issued.load()) Restore("test: watchdog path without a stall");
        break;
    }
    case 4: g_tMispredict = 4; break;
    case 5: g_on = 0; break;  // = live Tuning.FO 0
    case 6: g_on = g_installed && !g_disabled.load() ? 1 : 0; break;
    case 9: Rearm(); break;
    default: break;
    }
}
std::atomic<int> g_selftest{0};
DWORD WINAPI SelfTest(LPVOID) {
    struct Step {
        int action;
        const char* what;
    };
    static const Step steps[] = {{5, "live switch off -> stock"},
                                 {6, "live switch on -> overlap"},
                                 {4, "wrong back-buffer prediction -> caught and resynced before the frame replays"},
                                 {1, "session OFF from the TAIL"},
                                 {9, "re-arm"},
                                 {2, "wrong prediction (resynced) + session OFF at its TAIL (fo6pc sequence)"},
                                 {9, "re-arm"},
                                 {11, "watchdog restore in groups while rendering"},
                                 {9, "re-arm (reinstall)"},
                                 {8, "watchdog path (session OFF + all patches restored at once) while rendering, no stall"},
                                 {9, "re-arm (reinstall)"},
                                 {3, "5.6 s main stall -> watchdog -> main wakes during the restore -> stock"},
                                 {9, "re-arm (reinstall)"},
                                 {7, "5.6 s main stall, watchdog suppressed"},
                                 {9, "re-arm"},
                                 {10, "8 s main stall -> watchdog restores during the stall -> stock"},
                                 {9, "re-arm (reinstall)"}};
    int fails = 0;
    FLOG("FO: selftest: start (%d steps, each must keep frames coming)", (int)(sizeof steps / sizeof steps[0]));
    if (!g_on.load() || g_disabled.load()) Rearm();
    for (const Step& s : steps) {
        const int64_t fo0 = s_fo.load(), st0 = s_stock.load();
        const int64_t miss0 = s_bbMiss.load(), rs0 = s_bbResync.load();
        TestAction(s.action);
        // let the action take effect (the watchdog step needs > 5 s of stall + the restore)
        Sleep(s.action == 3 ? 9000 : s.action == 7 ? 8000 : s.action == 10 ? 11000 : s.action == 11 ? 500 : 1500);
        const uint64_t p1 = FrameCount();
        Sleep(3000);
        const uint64_t frames = FrameCount() - p1;
        const int64_t dfo = s_fo.load() - fo0, dst = s_stock.load() - st0;
        bool ok = frames >= 60;  // rendering continues (>= 20 fps over 3 s)
        switch (s.action) {
        case 4: ok = ok && s_bbResync.load() > rs0 && !g_disabled.load() && dfo >= 60; break;  // caught + fixed before replay
        case 1: ok = ok && g_disabled.load() != 0 && dst >= 60; break;
        case 2: ok = ok && s_bbResync.load() > rs0 && g_disabled.load() != 0 && dst >= 60; break;
        case 3:
        case 10:
        case 11:
        case 8: ok = ok && g_disabled.load() && !g_installed; break;  // pure stock engine afterwards (FO's entry points gone)
        case 7: ok = ok && !g_wdFired.load(); break;
        case 5: ok = ok && dst >= 60; break;
        case 6:
        case 9: ok = ok && g_installed && !g_disabled.load() && dfo >= 60; break;
        default: break;
        }
        ok = ok && s_bbMiss.load() == miss0;  // no frame ever rendered into a non-current back buffer
        if (!ok) ++fails;
        const bool stop = !ok;
        FLOG("FO: selftest: step %d (%s): %s - %llu frames in 3 s, FO frames +%lld, stock +%lld, resyncs +%lld, wrong back buffer +%lld, "
             "session %s, patches %s",
             s.action, s.what, ok ? "PASS" : "FAIL", (unsigned long long)frames, (long long)dfo, (long long)dst,
             (long long)(s_bbResync.load() - rs0), (long long)(s_bbMiss.load() - miss0), g_disabled.load() ? "OFF" : "on",
             g_installed ? "installed" : "removed");
        if (stop) break;  // a failed step leaves nothing meaningful to test after it
    }
    if (fails) FLOG("FO: selftest: FAILED (%d steps)", fails);
    else FLOG("FO: selftest: ALL PASS");
    g_selftest = 0;
    return 0;
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
    g_replayDelayUs = std::clamp((int)GetPrivateProfileIntW(L"Tuning", L"FO_ReplayDelayUs", 0, info.iniPath), 0, 50000);  // test only
    // B6 (7D9C24 PagePool page ops) waits for TAIL N by default: overlapping it with frame N's replay corrupted / freed data frame N
    // still replays (fo8 lag tests); 0 = unsafe overlap (diagnosis only), 1 = B4 + B6 wait
    g_ppWaitTail = std::clamp((int)GetPrivateProfileIntW(L"Tuning", L"FO_PPWait", 2, info.iniPath), 0, 2);
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
    g_imgSize = nt->OptionalHeader.SizeOfImage;
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
        if (s.op == 0xFF) {
            // 6-byte 'call [rax+120h]': NOP first, then the call, so the return address stays at+6 = the original next
            // instruction. (fo7pc: 'call; nop' returned to at+5; the watchdog's restore wrote the original bytes back while the
            // main thread was inside FoGetBB, which then returned into the last byte of the restored instruction -> garbage
            // executed -> device removed.)
            const int32_t rel = (int32_t)((intptr_t)r - (intptr_t)(pt.at + 6));
            pt.neu[0] = 0x90;
            pt.neu[1] = 0xE8;
            memcpy(pt.neu + 2, &rel, 4);
        } else {
            const int32_t rel = (int32_t)((intptr_t)r - (intptr_t)(pt.at + 5));
            pt.neu[0] = s.op;
            memcpy(pt.neu + 1, &rel, 4);
        }
        pt.name = s.name;
        // every patched call must return exactly to at+n (never into bytes a later restore rewrites)
        const size_t callAt = pt.neu[0] == 0xE8 ? 0 : pt.neu[1] == 0xE8 ? 1 : 99;
        if (pt.neu[0] != 0xE9 && callAt + 5 != pt.n) {
            FLOG("FO: not installed (site %s: patched call would return inside the patch)", s.name);
            return;
        }
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
    const bool isFo = !_stricmp(key, "FO"), isPct = !_stricmp(key, "FO_Pct"), isTest = !_stricmp(key, "FO_Test");
    if (!_stricmp(key, "FO_PPWait")) {  // 0 = PagePool frame begin overlaps frame N's replay (UNSAFE when the replay lags), 1 = B4 + B6
        g_ppWaitTail = std::clamp(atoi(val), 0, 2);  // wait for TAIL N (default), 2 = B6 only
        snprintf(reply, n, "FO_PPWait = %d", g_ppWaitTail.load());
        if (g_log) g_log("LIVE tuning: %s", reply);
        return true;
    }
    if (!_stricmp(key, "FO_ReplayDelayUs")) {  // test only: replay-thread lag (us per frame, busy wait in the 0x82 handler)
        g_replayDelayUs = std::clamp(atoi(val), 0, 50000);
        snprintf(reply, n, "FO_ReplayDelayUs = %d%s", g_replayDelayUs.load(), g_installed ? "" : " (active once FO is installed)");
        if (g_log) g_log("LIVE tuning: %s", reply);
        return true;
    }
    if (!isFo && !isPct && !isTest) return false;
    if (!g_ready) {
        snprintf(reply, n, "%s: FO not available ([GpuOpt] FrameOverlap = 1 or 2 at boot, v2.02, byte checks)", key);
        return true;
    }
    const int v = atoi(val);
    if (isTest) {  // edge-triggered: the same value written again (fastab re-sends the whole state) does nothing
        static int last = 0;
        const bool edge = v != last;
        last = v;
        if (edge && v == 100) {
            if (!g_selftest.exchange(1)) {
                if (HANDLE h = CreateThread(nullptr, 0, SelfTest, nullptr, 0, nullptr)) CloseHandle(h);
                else g_selftest = 0;
            }
        } else if (edge && v) {
            TestAction(v);
        }
        snprintf(reply, n, "FO_Test = %d%s", v, edge && v ? " (started)" : "");
    } else if (isFo) {
        if (v && !g_installed && !g_disabled.load()) {
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
    constexpr int kN = 22;
    static int64_t p[kN] = {};
    const int64_t c[kN] = {s_fo, s_stock, s_span, s_barrierTicks, s_leadTicks, s_tailTicks, s_ppWaitTicks, s_ppWaits, s_subSwitch.load(),
                           s_barrierEarly, s_barrierLate, s_prevGenHits, s_bbChecks, s_bbMiss, s_stockOff, s_stockOther, s_noAdvance,
                           s_stockSpin, s_direct, s_bbExact, s_bbResync, s_bbExtraPresents};
    int64_t d[kN];
    for (int i = 0; i < kN; ++i) d[i] = c[i] - p[i], p[i] = c[i];
    const double fo = d[0] ? (double)d[0] : 1.0;
    snprintf(buf, n,
             "FO: switch %s%s | frames FO %.1f/s stock %.1f/s (switch/session off %lld, TEST present %lld, span %lld, other device "
             "%lld; other Present callers %lld; present spins before stock %lld) | per FO frame: barrier wait %.2f ms, GPU-lead wait "
             "%.2f ms, TAIL %.2f ms | barrier at K (node %u) %lld, at frame end %lld | PP lock waits %lld (%.2f ms) | sub-stream "
             "half switches %lld | prev-gen handle checks %lld | back buffer: checks %lld, real index first %lld, prediction resynced "
             "%lld (extra presents %lld), WRONG %lld (session %lld)",
             g_on.load() ? "ON" : "off", g_disabled.load() ? " (session OFF)" : "", d[0] / (double)std::max(1u, secs),
             d[1] / (double)std::max(1u, secs), (long long)d[14], (long long)d[16], (long long)d[2], (long long)d[15], (long long)d[18],
             (long long)d[17], d[3] * g_tickMs / fo, d[4] * g_tickMs / fo, d[5] * g_tickMs / fo, g_K, (long long)d[9], (long long)d[10],
             (long long)d[7], d[6] * g_tickMs, (long long)d[8], (long long)d[11], (long long)d[12], (long long)d[19],
             (long long)d[20], (long long)d[21], (long long)d[13], (long long)s_bbMiss.load());
}

#ifdef FO_TEST
static void TestLog(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    printf("    log: ");
    vprintf(fmt, ap);
    putchar('\n');
    va_end(ap);
}
void TestInstall(uintptr_t fakeBase, const Engine* e) {
    E = *e;
    g_log = &TestLog;
    g_base = fakeBase;
    InitializeCriticalSection(&g_instCs);
    g_installed = true;
    g_ready = true;
    g_wdStarted = true;
    if (HANDLE h = CreateThread(nullptr, 0, Watchdog, nullptr, 0, nullptr)) CloseHandle(h);
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
void TestPresentSpin(uintptr_t dev) { FoPresentSpin(dev); }
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
void TestAct(int v) { TestAction(v); }
bool TestInstalled() { return g_installed; }
void TestWatchdogSuppress(int on) { g_wdSuppress = on; }
bool TestWatchdogFired() { return g_wdFired.load() != 0; }
int64_t TestMisses() { return s_bbMiss.load(); }
int64_t TestResyncs() { return s_bbResync.load(); }
int64_t TestExactFirst() { return s_bbExact.load(); }
bool TestPPFree() {
    if (!TryAcquireSRWLockExclusive(&g_pp)) return false;
    ReleaseSRWLockExclusive(&g_pp);
    return true;
}
#endif

}  // namespace frameoverlap
