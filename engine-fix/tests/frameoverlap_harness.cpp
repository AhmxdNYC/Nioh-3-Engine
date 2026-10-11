// frameoverlap_harness.cpp - offline test of FO (src/frameoverlap.cpp, built with FO_TEST) against a fake engine:
// a fake GfxDevice, a fake replay thread that replays the token stream (sequence-checked: any token the main thread
// overwrites before it was replayed breaks the sequence), a fake present thread, fake sub-streams, a real D3D12 device +
// queue + the ASI's forward fence. Run by tests/run_frameoverlap_harness.py. Must end ALL PASSED.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>
#include <initializer_list>
#include <vector>

#include "../src/frameoverlap.h"

using namespace frameoverlap;

static int g_fails = 0;
static void Check(bool c, const char* m) {
    printf("  [%s] %s\n", c ? "ok" : "FAIL", m);
    if (!c) ++g_fails;
}
template <class T>
static T& F(uintptr_t a) {
    return *reinterpret_cast<T*>(a);
}

// ---- fake device -------------------------------------------------------------------------------------------------------
static std::vector<uint8_t> g_devMem(0x8000), g_stream(8u << 20), g_renderer(0x1EE600), g_syncTab(3 * 0x18);
static uintptr_t dev, sbase, send_;
constexpr uint32_t kReserve = 4096;
static uintptr_t g_sub[2], g_subRec[2];
static std::vector<uint8_t> g_subMem(4 * 0x100), g_subBuf(2 * 65536);

// ---- fake replay thread ------------------------------------------------------------------------------------------------
struct Item {
    uint64_t b, e, submit;
};
static std::mutex g_mx;
static std::deque<Item> g_q;
static std::atomic<int> g_pending{0};
static std::atomic<bool> g_stop{false}, g_pause{false};
static std::atomic<uint64_t> g_seqReplayed{0};
static std::atomic<int> g_seqErrors{0}, g_tails{0}, g_submits{0};
static std::atomic<int> g_inPP{0}, g_ppViolations{0}, g_ppCalls{0};
static uint64_t g_seqWritten = 0;

static uint32_t g_subSeqW[2] = {}, g_subSeqR[2] = {};
static std::atomic<int> g_subErr{0};
// engine locks of the KtglCache (g+61A0h): +CF8h shared by tok 0x95 (7F64DC) and the tile job 492310, +CC0h / +CD0h exclusive in
// the KtglCache frame end (12682C: blocking acquire on the v == 0 path, try-acquire otherwise)
static PSRWLOCK CacheLock(uintptr_t off) { return reinterpret_cast<PSRWLOCK>(dev + 0x61A0 + off); }
static void PPOrig(uintptr_t, uint8_t**) {  // a PagePool token handler on the replay thread
    if (g_inPP.exchange(1)) ++g_ppViolations;
    ++g_ppCalls;
    AcquireSRWLockShared(CacheLock(0xCF8));
    Sleep(0);
    ReleaseSRWLockShared(CacheLock(0xCF8));
    g_inPP = 0;
}
static std::atomic<bool> g_jobPause{false};
static std::atomic<int> g_jobRuns{0};
static void TileJobThread() {  // 492310's tail: +CF8h shared around the update list
    while (!g_stop) {
        if (g_jobPause) {
            Sleep(1);
            continue;
        }
        AcquireSRWLockShared(CacheLock(0xCF8));
        Sleep(0);
        ReleaseSRWLockShared(CacheLock(0xCF8));
        ++g_jobRuns;
        Sleep(1);
    }
}
static void ReplayThread() {
    void* lc[2] = {nullptr, nullptr};
    while (!g_stop) {
        Item it{};
        {
            std::lock_guard<std::mutex> l(g_mx);
            if (!g_q.empty()) {
                it = g_q.front();
                g_q.pop_front();
            }
        }
        if (!it.e) {
            Sleep(0);
            continue;
        }
        while (g_pause) Sleep(1);
        uint32_t* p = reinterpret_cast<uint32_t*>(it.b);
        uint32_t* e = reinterpret_cast<uint32_t*>(it.e);
        unsigned n = 0;
        while (p < e) {
            const uint32_t op = *p & 0xFFFF;
            if (op == 0x8F) {
                const uint64_t fn = p[1] | (uint64_t)p[2] << 32, arg = p[3] | (uint64_t)p[4] << 32;
                p += 5;
                reinterpret_cast<void(__fastcall*)(void*, void*)>(fn)(lc, (void*)arg);
                ++g_tails;
            } else if (op == 0x06) {
                const uint64_t s = p[1];
                if (s != g_seqReplayed + 1) ++g_seqErrors;
                g_seqReplayed = s;
                p += 2;
                if (++n % 50 == 0) {
                    uint8_t* cur = nullptr;
                    TestPPHandler(0, &cur, &PPOrig);
                }
                if (n % 8 == 0) {  // replay is slower than recording: a backlog builds up like on pc2
                    LARGE_INTEGER t0, t;
                    QueryPerformanceCounter(&t0);
                    do QueryPerformanceCounter(&t);
                    while (t.QuadPart - t0.QuadPart < 300);
                }
            } else if (op == 0x90) {  // sub-stream segment {recorder, begin, end}: replay its (sequence-numbered) data
                const uint32_t ri = p[1];
                const uint32_t* sb = reinterpret_cast<const uint32_t*>(p[2] | (uint64_t)p[3] << 32);
                const uint32_t* se = reinterpret_cast<const uint32_t*>(p[4] | (uint64_t)p[5] << 32);
                for (; sb < se; ++sb) {
                    if (*sb != g_subSeqR[ri] + 1) ++g_subErr;
                    g_subSeqR[ri] = *sb;
                }
                p += 6;
            } else if (op == 0xA7) {
                p[1] = 0xBBBBBBBBu;
                p += 2;
            } else {
                ++g_seqErrors;
                break;
            }
        }
        if (it.submit) ++g_submits;
        --g_pending;
    }
}
static void FakeRingPush(uintptr_t, const uint64_t* item, uint64_t submit, uintptr_t) {
    ++g_pending;
    std::lock_guard<std::mutex> l(g_mx);
    g_q.push_back({item[0], item[1], submit});
}
static void WaitIdle() {
    while (g_pending.load()) Sleep(0);
}

// ---- fake swapchain / present thread -----------------------------------------------------------------------------------
static std::atomic<uint32_t> g_scIdx{0};
static std::atomic<uint32_t> g_lastResolvedSlot{99};
static std::atomic<int> g_presents{0}, g_skipNext{0};
struct FakeCom {
    void** vt;
};
static void* g_scVt[64];
static FakeCom g_sc{g_scVt};
static UINT STDMETHODCALLTYPE ScGetIdx(void*) { return g_scIdx.load(); }
static std::atomic<int> g_noRotate{0};
static HRESULT STDMETHODCALLTYPE ScPresent(void*, UINT, UINT) {
    ++g_presents;
    if (g_noRotate.load() > 0) {  // injected: presents that do not rotate the chain (e.g. an occluded window)
        --g_noRotate;
        return S_OK;
    }
    if (g_skipNext.exchange(0)) g_scIdx = (g_scIdx + 2) % 3;  // injected: the next frame's index is not (i+1)%3
    else g_scIdx = (g_scIdx + 1) % 3;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE ScLastPresentCount(void*, UINT* c) {
    *c = (UINT)g_presents.load();
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE ScDesc(void*, DXGI_SWAP_CHAIN_DESC* d) {
    memset(d, 0, sizeof *d);
    d->BufferCount = 3;
    return S_OK;
}
// timestamps: main allocates at record time in slot +4730h (6B42B0); TAIL must resolve exactly that slot's range
constexpr uint32_t kPer = 64;
static uint64_t g_tsBits[3][1] = {};
static std::atomic<int> g_resolves{0}, g_resolveBad{0};
static std::atomic<uint32_t> g_expectSlotForFrame[4096] = {};
static void* g_listVt[64];
static FakeCom g_list{g_listVt};
static void STDMETHODCALLTYPE ListResolve(void*, void*, int type, UINT start, UINT num, void*, UINT64 off) {
    ++g_resolves;
    if (type != 2 || num != 1 || off != (UINT64)start * 8 || start % kPer != 5) ++g_resolveBad;
    g_lastResolvedSlot = start / kPer;
}
static void* g_rbVt[16];
static FakeCom g_rb{g_rbVt};
static uint8_t g_rbMem[3 * kPer * 8] = {};
static HRESULT STDMETHODCALLTYPE RbMap(void*, UINT, const D3D12_RANGE*, void** pp) {
    *pp = g_rbMem;
    return S_OK;
}
static void STDMETHODCALLTYPE RbUnmap(void*, UINT, const D3D12_RANGE*) {}
static std::vector<uint8_t> g_tsCopy(3 * kPer * 8);
static void* g_allocVt[16];
static FakeCom g_alloc{g_allocVt};
static std::atomic<int> g_allocResets{0};
static HRESULT STDMETHODCALLTYPE AllocReset(void*) {
    ++g_allocResets;
    return S_OK;
}
static std::atomic<int> g_presentMs{2};  // longer than the fake drain in the switch phases (the in-game ratio)
static void PresentThread() {
    while (!g_stop) {
        if (WaitForSingleObject(F<HANDLE>(dev + 0x7720), 20) != WAIT_OBJECT_0) continue;
        void* sc = F<void*>(dev + 0x7730);
        Sleep(g_presentMs.load());  // a real Present blocks for milliseconds (frame latency / vblank); the drain is ~0.3 ms
        ScPresent(sc, 0, 0);
        F<volatile uint8_t>(dev + 0x7728) = 0;
    }
}

// ---- fake engine functions ---------------------------------------------------------------------------------------------
static std::atomic<uint64_t> g_fenceCounter{0};
static std::atomic<int> g_waitBad{0}, g_waits{0}, g_tagBad{0}, g_tags{0}, g_subResets{0}, g_subResetEarly{0}, g_stockPresents{0},
    g_ppMainViolations{0};
static uint64_t g_lastTag = 0;

static std::atomic<int> g_stockIdxPending{0};
static void StockPresent(uintptr_t d, uintptr_t) {  // stock GfxDevicePresent 3D62E4, in its real order
    uint64_t it[2] = {F<uint64_t>(d + 0x3C48), F<uint64_t>(d + 0x3C40)};  // CmdStreamEndFrame: final kick, drain, reset to base
    FakeRingPush(0, it, 1, 0);
    WaitIdle();
    F<uintptr_t>(d + 0x3C40) = F<uintptr_t>(d + 0x3C30);
    F<uintptr_t>(d + 0x3C48) = F<uintptr_t>(d + 0x3C30);
    F<uint32_t>(d + 0x3C60) += 1;
    for (int i = 0; i < 2; ++i) F<uintptr_t>(g_subRec[i] + 0x28) = F<uintptr_t>(g_subRec[i] + 0x18);  // 253F2C
    const uint32_t s = F<uint32_t>(d + 0x4730);
    F<uint32_t>(d + 0x4788 + s * 16) = 0xFFFFFFFFu;  // timestamp resolve of the slot
    F<uint32_t>(d + 0x478C + s * 16) = 0;
    g_tsBits[s][0] = 0;
    // 3D6474: GetCurrentBackBufferIndex BEFORE the present spin (stock relies on WaitSubmitThread's spin at frame begin)
    if (F<volatile uint8_t>(d + 0x7728)) ++g_stockIdxPending;
    (void)ScGetIdx(&g_sc);
    const uint64_t fence = ++g_fenceCounter;  // GfxSubmitFrameCmdList
    F<uint64_t>(d + 0x4320 + s * 8) = fence;
    AcquireSRWLockExclusive(CacheLock(0xCD0));  // 12682C (v == 0 path: blocking)
    AcquireSRWLockExclusive(CacheLock(0xCC0));
    ReleaseSRWLockExclusive(CacheLock(0xCC0));
    ReleaseSRWLockExclusive(CacheLock(0xCD0));
    while (F<volatile uint8_t>(d + 0x7728)) Sleep(0);  // SpinWaitFlagSleep0, then hand present N to the present thread
    F<uint8_t>(d + 0x7728) = 1;
    F<void*>(d + 0x7730) = &g_sc;
    SetEvent(F<HANDLE>(d + 0x7720));
    F<uint32_t>(d + 0x4730) = (s + 1) % 3;  // GfxFrameSlotAdvance: slot ++, wait its fence, 8BEDC8, allocator reset
    if (F<uint64_t>(d + 0x4320 + ((s + 1) % 3) * 8ull) > g_fenceCounter.load()) ++g_waitBad;
    F<uint32_t>(d + 0x3C1C) += 1;
    ++g_allocResets;
    ++g_stockPresents;
}
static void FakeSubReset(uintptr_t sub) {
    if (TestTailDone() < TestIssued()) ++g_subResetEarly;
    const uintptr_t rec = F<uintptr_t>(sub + 0x60);
    F<uintptr_t>(rec + 0x28) = F<uintptr_t>(rec + 0x18);
    ++g_subResets;
}
static void FakeTransition(uintptr_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {}
static uint64_t FakeSubmitFrame(uintptr_t, uintptr_t, uint64_t) { return ++g_fenceCounter; }
static void FakePsoMaint(uintptr_t, uint64_t, uint64_t) {}
static void FakeSpinPresent(uintptr_t d) {
    while (F<volatile uint8_t>(d + 0x7728)) Sleep(0);
}
static void FakeRemoved(uint64_t, int64_t) {}
static void FakeWaitFence(uintptr_t, uint64_t v, void*) {
    ++g_waits;
    if (v > g_fenceCounter.load()) ++g_waitBad;  // a wait for a fence value nobody submitted would hang the real game
}
static void FakeNoop1(uintptr_t) {}
static uint64_t FakeQpc() {
    LARGE_INTEGER q;
    QueryPerformanceCounter(&q);
    return (uint64_t)q.QuadPart;
}
static uint64_t FakeTry(uintptr_t srw) { return TryAcquireSRWLockExclusive(reinterpret_cast<PSRWLOCK>(srw)) ? 1 : 0; }
static void FakeUnlockWake(uintptr_t srw) { ReleaseSRWLockExclusive(reinterpret_cast<PSRWLOCK>(srw)); }
static void FakeNoop2(uintptr_t, uintptr_t) {}
static void FakeTag(uintptr_t, uintptr_t fdev, uint64_t v) {  // stock tag: {dev+4310h, the frame's own fence}, after its last list
    ++g_tags;
    if (fdev != dev || v <= g_lastTag || v > g_fenceCounter.load()) ++g_tagBad;
    g_lastTag = v;
}
static void FakeRetire(uintptr_t, uintptr_t, uintptr_t, uintptr_t) {}
static void FakeProcess(uintptr_t, uintptr_t, uint64_t, uintptr_t, uintptr_t) {}
static void FakePP(uintptr_t, uintptr_t) {
    if (g_inPP.exchange(1)) ++g_ppMainViolations;
    Sleep(0);
    g_inPP = 0;
}
static void FakeNode(uintptr_t, uintptr_t) {}

// ---- main-thread recording emulation -----------------------------------------------------------------------------------
static void Emit(uint32_t n, uint32_t nodes, uint32_t kickEvery) {
    for (uint32_t i = 0; i < n; ++i) {
        if (F<uintptr_t>(dev + 0x3C38) - F<uintptr_t>(dev + 0x3C40) < kReserve) {  // stock wrap: marker, kick, wait, reset
            uint64_t it[2] = {F<uint64_t>(dev + 0x3C48), F<uint64_t>(dev + 0x3C40)};
            FakeRingPush(0, it, 1, 0);
            WaitIdle();
            F<uintptr_t>(dev + 0x3C40) = F<uintptr_t>(dev + 0x3C48) = F<uintptr_t>(dev + 0x3C30);
            F<uint32_t>(dev + 0x3C60) += 1;
        }
        uint32_t* w = F<uint32_t*>(dev + 0x3C40);
        w[0] = 0x06;
        w[1] = (uint32_t)++g_seqWritten;
        F<uintptr_t>(dev + 0x3C40) += 8;
        if ((i + 1) % kickEvery == 0) {
            uint64_t it[2] = {F<uint64_t>(dev + 0x3C48), F<uint64_t>(dev + 0x3C40)};
            FakeRingPush(0, it, 0, 0);
            F<uintptr_t>(dev + 0x3C48) = F<uintptr_t>(dev + 0x3C40);
        }
        if (nodes && i % (n / nodes) == 0) {
            F<uint32_t>((uintptr_t)g_renderer.data() + 0x1EE568) = i / (n / nodes);
            if (TestInstalled()) TestNode((uintptr_t)g_renderer.data());
        }
    }
}
static int g_bbMismatchMain = 0;
static std::vector<uint32_t> g_recSlots;
static std::atomic<int> g_framesDone{0};
static void Emit(uint32_t n, uint32_t nodes, uint32_t kickEvery = 64);
static void SubWrite(int i, uint32_t dwords) {  // an effect worker appends to sub-stream i, main records the 0x90 token
    uint32_t* w = F<uint32_t*>(g_subRec[i] + 0x28);
    uint32_t* const b0 = w;
    for (uint32_t k = 0; k < dwords; ++k) *w++ = ++g_subSeqW[i];
    F<uintptr_t>(g_subRec[i] + 0x28) = (uintptr_t)w;
    if (F<uintptr_t>(dev + 0x3C38) - F<uintptr_t>(dev + 0x3C40) < kReserve) Emit(0, 0, 64);  // (no-op: Emit handles wraps)
    uint32_t* t = F<uint32_t*>(dev + 0x3C40);
    const uint64_t bb = (uint64_t)(uintptr_t)b0, ee = (uint64_t)(uintptr_t)w;
    t[0] = 0x90;
    t[1] = (uint32_t)i;
    t[2] = (uint32_t)bb;
    t[3] = (uint32_t)(bb >> 32);
    t[4] = (uint32_t)ee;
    t[5] = (uint32_t)(ee >> 32);
    F<uintptr_t>(dev + 0x3C40) += 24;
}
static uint32_t g_earlySubDwords = 0;  // > 0: workers also write this much BEFORE the barrier (effects do, every frame)
static void Frame(uint32_t tokens, uint32_t nodes = 100) {
    const bool fo = TestInstalled();  // false after the watchdog restored every patch: the pure stock engine
    // WaitSubmitThread 7D9B6C: present spin (FO skips it after an FO frame), PagePoolFrameUpdate, back-buffer index
    if (fo) {
        TestPresentSpin(dev);
        TestPPUpdate(dev + 0x61A0, dev);
        TestGetBB(&g_sc);
    } else {
        while (F<volatile uint8_t>(dev + 0x7728)) Sleep(0);
        FakePP(dev + 0x61A0, dev);
        (void)ScGetIdx(&g_sc);
    }
    {  // 6B42B0 on main: slot = +4730h at record time
        const uint32_t s = F<uint32_t>(dev + 0x4730);
        F<uint32_t>(dev + 0x4788 + s * 16) = std::min(F<uint32_t>(dev + 0x4788 + s * 16), 5u);
        F<uint32_t>(dev + 0x478C + s * 16) = std::max(F<uint32_t>(dev + 0x478C + s * 16), 5u);
        g_tsBits[s][0] |= 1ull << 5;
        g_recSlots.push_back(s);
    }
    if (g_earlySubDwords)
        for (int i = 0; i < 2; ++i) SubWrite(i, g_earlySubDwords);  // before the barrier (frame N may still be replaying)
    Emit(tokens / 2, nodes);
    for (int i = 0; i < 2; ++i) SubWrite(i, 16);
    Emit(tokens - tokens / 2, 0);
    if (fo) TestPresent(dev);
    else StockPresent(dev, 0);
    ++g_framesDone;
}

int main() {
    // real D3D12 device + DIRECT queue (forward fence)
    ID3D12Device* d3d = nullptr;
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), (void**)&d3d))) {
        printf("no D3D12 device\n");
        return 2;
    }
    D3D12_COMMAND_QUEUE_DESC qd{};
    ID3D12CommandQueue* q = nullptr;
    d3d->CreateCommandQueue(&qd, __uuidof(ID3D12CommandQueue), (void**)&q);

    dev = (uintptr_t)g_devMem.data();
    sbase = (uintptr_t)g_stream.data();
    send_ = sbase + g_stream.size();
    F<uint32_t>(dev + 0x3C28) = (uint32_t)g_stream.size();
    F<uint32_t>(dev + 0x3C2C) = kReserve;
    F<uintptr_t>(dev + 0x3C30) = sbase;
    F<uintptr_t>(dev + 0x3C38) = send_;
    F<uintptr_t>(dev + 0x3C40) = sbase;
    F<uintptr_t>(dev + 0x3C48) = sbase;
    F<uintptr_t>(dev + 0x3C58) = dev + 0x3C28;
    F<uint32_t>(dev + 0x3C18) = 3;
    F<uintptr_t>(dev + 0x3C20) = (uintptr_t)g_syncTab.data();
    F<uint8_t>(dev + 0x3F80) = 1;  // present thread
    F<uint32_t>(dev + 0x4240) = 3;
    F<void*>(dev + 0x4268) = d3d;
    F<void*>(dev + 0x4270) = q;
    F<void*>(dev + 0x4278) = &g_sc;
    static uint8_t res[3][0x20];
    for (int i = 0; i < 3; ++i) {
        F<uintptr_t>(dev + 0x4698 + i * 8) = (uintptr_t)res[i];
        F<void*>(dev + 0x4290 + i * 8) = &g_alloc;
        F<uint32_t>(dev + 0x4788 + i * 16) = 0xFFFFFFFFu;
        F<uint64_t*>(dev + 0x4790 + i * 16) = g_tsBits[i];
        F<uint32_t>(dev + 0x478C + i * 16) = 0;
    }
    F<HANDLE>(dev + 0x7720) = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    F<uint8_t>(dev + 0x61A0 + 0xD20) = 1;  // KtglCache enabled
    F<uint32_t>(dev + 0x61A0 + 0x6B0) = 1;
    F<uint32_t>(dev + 0x61A0 + 0x8D0) = 1;  // KtglCache frame end takes the try-lock path (+CD0h / +CC0h)
    // sub-streams
    for (int i = 0; i < 2; ++i) {
        g_sub[i] = (uintptr_t)g_subMem.data() + i * 0x100;
        g_subRec[i] = (uintptr_t)g_subMem.data() + 0x200 + i * 0x100;
        F<uint8_t>(g_sub[i] + 0x55) = 1;
        F<uintptr_t>(g_sub[i] + 0x60) = g_subRec[i];
        F<uintptr_t>(g_subRec[i] + 0x18) = (uintptr_t)g_subBuf.data() + i * 65536;
        F<uintptr_t>(g_subRec[i] + 0x20) = (uintptr_t)g_subBuf.data() + i * 65536 + 65536;
        F<uintptr_t>(g_subRec[i] + 0x28) = F<uintptr_t>(g_subRec[i] + 0x18);
    }
    static uintptr_t subArr[2] = {g_sub[0], g_sub[1]};
    F<uintptr_t>(dev + 0x74D8) = (uintptr_t)subArr;
    F<uint32_t>(dev + 0x3E28) = 2;
    g_scVt[8] = (void*)&ScPresent;
    g_scVt[12] = (void*)&ScDesc;
    g_scVt[17] = (void*)&ScLastPresentCount;
    g_scVt[36] = (void*)&ScGetIdx;
    g_allocVt[8] = (void*)&AllocReset;
    g_listVt[54] = (void*)&ListResolve;
    g_rbVt[8] = (void*)&RbMap;
    g_rbVt[9] = (void*)&RbUnmap;
    F<void*>(dev + 0x590) = &g_list;
    F<void*>(dev + 0x4770) = &g_rb;
    F<uint8_t*>(dev + 0x4778) = g_tsCopy.data();
    F<uint32_t>(dev + 0x4034) = kPer;

    static uintptr_t gfxGlobal = 0, cbNull = 0;
    Engine e{};
    e.present = &StockPresent;
    e.ringPush = &FakeRingPush;
    e.subReset = &FakeSubReset;
    e.transition = &FakeTransition;
    e.submitFrame = &FakeSubmitFrame;
    e.psoMaint = &FakePsoMaint;
    e.spinPresent = &FakeSpinPresent;
    e.removedReason = &FakeRemoved;
    e.fatal = &FakeRemoved;
    e.waitFence = &FakeWaitFence;
    e.recRebuild = &FakeNoop1;
    e.recycle = &FakeNoop1;
    e.qpc = &FakeQpc;
    e.ringNorm = &FakeNoop1;
    e.tryX2 = &FakeTry;
    e.tryX1 = &FakeTry;
    e.cacheCompact = &FakeNoop2;
    e.unlockWake = &FakeUnlockWake;
    e.tagFence = &FakeTag;
    e.retire = &FakeRetire;
    e.kick = &FakeNoop1;
    e.process = &FakeProcess;
    e.signalPair = &FakeNoop1;
    e.ppUpdate = &FakePP;
    e.ppOps = &FakePP;
    e.cacheFlushBegin = &FakeNoop1;
    e.endFrame = &FakeNoop1;
    e.node130 = &FakeNode;
    e.gfxGlobal = (uintptr_t)&gfxGlobal;
    e.slCb[0] = e.slCb[1] = e.slCb[2] = (uintptr_t)&cbNull;
    TestInstall(0x10000, &e);
    std::thread rt(ReplayThread), pt(PresentThread), jt(TileJobThread);
    std::atomic<const char*> phase{"setup"};
    std::thread hang([&] {  // a frame that never ends = the in-game hang: fail loudly instead of blocking the test run
        int last = -1;
        DWORD since = GetTickCount();
        while (!g_stop) {
            Sleep(200);
            const int f = g_framesDone.load();
            if (f != last) {
                last = f;
                since = GetTickCount();
            } else if (GetTickCount() - since > 20000) {
                printf("  [FAIL] HANG: no frame completed for 20 s in phase '%s' (CF8h %llX CC0h %llX CD0h %llX, FO PP free %d, "
                       "present pending %u)\nFAILED (hang)\n",
                       phase.load(), F<unsigned long long>(dev + 0x61A0 + 0xCF8), F<unsigned long long>(dev + 0x61A0 + 0xCC0),
                       F<unsigned long long>(dev + 0x61A0 + 0xCD0), (int)TestPPFree(), F<uint8_t>(dev + 0x7728));
                fflush(stdout);
                ExitProcess(3);
            }
        }
    });
    char rep[256];
    // the stock situation after a switch: nothing of FO left behind (locks, TAIL, stream / sub-stream extents, present state)
    auto StockClean = [&](const char* what) {
        WaitIdle();
        g_jobPause = true;
        Sleep(20);
        const bool locks = F<uint64_t>(dev + 0x61A0 + 0xCF8) == 0 && F<uint64_t>(dev + 0x61A0 + 0xCC0) == 0 &&
                           F<uint64_t>(dev + 0x61A0 + 0xCD0) == 0 && TestPPFree();
        g_jobPause = false;
        char m[256];
        snprintf(m, sizeof m, "%s: no lock held (cache +CF8h / +CC0h / +CD0h, FO PP)", what);
        Check(locks, m);
        snprintf(m, sizeof m, "%s: no TAIL pending", what);
        Check(TestTailDone() >= TestIssued(), m);
        snprintf(m, sizeof m, "%s: main stream end and sub-stream recorders at full extent", what);
        Check(F<uintptr_t>(dev + 0x3C38) == send_ && F<uintptr_t>(g_subRec[0] + 0x20) - F<uintptr_t>(g_subRec[0] + 0x18) == 65536 &&
                  F<uintptr_t>(g_subRec[1] + 0x20) - F<uintptr_t>(g_subRec[1] + 0x18) == 65536,
              m);
        snprintf(m, sizeof m, "%s: stock Present never read the back-buffer index while a present was pending", what);
        Check(g_stockIdxPending == 0, m);
        snprintf(m, sizeof m, "%s: token sequence intact", what);
        Check(g_seqErrors == 0 && g_seqReplayed == g_seqWritten, m);
    };

    printf("phase 1: 40 overlapped frames, FO_Pct 25\n");
    SetTuning("FO_Pct", "25", rep, sizeof rep);
    for (int f = 0; f < 40; ++f) Frame(6000);
    WaitIdle();
    const int tails1 = g_tails.load();
    Check(g_seqErrors == 0, "no token overwritten before replay, all tokens replayed in order");
    Check(g_seqReplayed == g_seqWritten, "every recorded token replayed");
    Check(tails1 >= 38, "TAIL ran for the overlapped frames (first frame may take the span fallback)");
    Check(TestTailDone() == TestIssued(), "last TAIL done");
    Check(!TestDisabled(), "no fallback to session-off (back-buffer prediction held)");
    Check(g_waitBad == 0 && g_waits > 0, "GPU-lead / slot waits only for submitted fence values");
    Check(g_tagBad == 0 && g_tags == tails1, "KtglCache tag per FO frame in TAIL = {dev+4310h, the frame's submitted fence}");
    Check(g_subResets >= 2 * tails1, "sub-streams switch halves at every FO frame end (stock 2543A0 reset on the other half)");
    Check(F<uintptr_t>(g_subRec[0] + 0x20) - F<uintptr_t>(g_subRec[0] + 0x18) == 32768, "sub-stream recorder works on a half");
    Check(g_ppViolations == 0 && g_ppMainViolations == 0 && g_ppCalls > 0, "PagePool handlers never run concurrently with main PP work");
    Check(g_allocResets >= tails1, "frame-slot allocator reset at every FO frame end (main, before the slot switch)");
    Check(g_resolves >= tails1 - 1 && g_resolveBad == 0, "timestamps: TAIL resolved index 5 of a slot range every frame");
    Check(!TestDisabled(), "timestamp queries in use do not switch FO off");

    printf("phase 2: a frame that wraps in the high half and then runs past the middle -> stock frame end, then FO again\n");
    if (F<uintptr_t>(dev + 0x3C40) < sbase + g_stream.size() / 2) Frame(6000);  // start the test frame in the high half
    const int stock0 = g_stockPresents;
    TestWatchdogSuppress(1);  // this frame is seconds long on purpose
    Frame(1200000);  // 9.6 MB: wraps at the end of the high half, continues from the base past the middle
    TestWatchdogSuppress(0);
    for (int f = 0; f < 5; ++f) Frame(6000);
    WaitIdle();
    Check(g_stockPresents > stock0, "spanning frame took the stock path");
    Check(g_seqErrors == 0 && g_seqReplayed == g_seqWritten, "token sequence intact across the fallback");
    Check(g_tails.load() >= tails1 + 4, "overlap resumed after the fallback");

    printf("phase 3: live switch off / on\n");
    g_presentMs = 60;
    SetTuning("FO", "0", rep, sizeof rep);
    const int t3 = g_tails, s3 = g_stockPresents;
    for (int f = 0; f < 5; ++f) Frame(6000);
    Check(g_tails == t3 && g_stockPresents == s3 + 5, "switch off: stock frame ends only");
    StockClean("live switch off");
    g_presentMs = 2;
    SetTuning("FO", "1", rep, sizeof rep);
    for (int f = 0; f < 5; ++f) Frame(6000);
    WaitIdle();
    Check(g_tails >= t3 + 4, "switch on: overlap again");
    Check(g_seqErrors == 0 && g_seqReplayed == g_seqWritten, "token sequence intact across switching");

    printf("phase 4: previous-generation handle while TAIL is pending\n");
    TestGetBB(&g_sc);
    TestPPUpdate(dev + 0x61A0, dev);
    Emit(1000, 0);
    // 418E74: 0xA7 marker + 0xAAAAAAAA; handle = dword offset of the AAAA word | generation << 60
    uint32_t* w = F<uint32_t*>(dev + 0x3C40);
    w[0] = 0xA7;
    w[1] = 0xAAAAAAAAu;
    F<uintptr_t>(dev + 0x3C40) += 8;
    const uint64_t off = ((uintptr_t)(w + 1) - sbase) / 4;
    const uint64_t h = off | (uint64_t)(F<uint32_t>(dev + 0x3C60) & 0xF) << 60;
    g_pause = true;
    TestPresent(dev);
    Check((TestPrevGen() & 0xF) == (h >> 60), "handle generation = previous generation after E");
    Check(TestNotFreed(reinterpret_cast<int64_t*>(dev + 0x3C58), h) == 0, "pending TAIL + unreplayed marker -> not freed (stock would say freed)");
    g_pause = false;
    WaitIdle();
    Check(TestNotFreed(reinterpret_cast<int64_t*>(dev + 0x3C58), h) == 1, "after replay -> freed");

    printf("phase 4b: the HandleSlotIsNotFreed stub keeps r9 / r10 / r11 (LTCG callers keep live values there)\n");
    {
        size_t sn = 0;
        const uint8_t* sb = TestStub(&sn);
        FILE* f = nullptr;
        if (!fopen_s(&f, "build/fotest/notfreed_stub.bin", "wb") && f) {
            fwrite(sb, 1, sn, f);
            fclose(f);
        }
        uint8_t* code = (uint8_t*)VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        size_t k = 0;
        auto B = [&](std::initializer_list<uint8_t> l) {
            for (uint8_t v : l) code[k++] = v;
        };
        auto Q = [&](uint64_t v) {
            memcpy(code + k, &v, 8);
            k += 8;
        };
        uint8_t* stub = code + 1024;
        memcpy(stub, sb, sn);
        B({0x49, 0xB9});
        Q(0x0102030405060708ull);  // mov r9, ...
        B({0x49, 0xBA});
        Q(0x1111111111111111ull);  // mov r10, ...
        B({0x49, 0xBB});
        Q(0xA5A5A5A5A5A5A5A5ull);  // mov r11, ...
        B({0x48, 0x83, 0xEC, 0x28});  // sub rsp, 28h
        B({0x48, 0xB8});
        Q((uint64_t)(uintptr_t)stub);  // mov rax, stub
        B({0xFF, 0xD0});               // call rax
        B({0x48, 0x83, 0xC4, 0x28});   // add rsp, 28h
        B({0x4C, 0x89, 0xC8});         // mov rax, r9
        B({0x4C, 0x31, 0xD0});         // xor rax, r10
        B({0x4C, 0x31, 0xD8});         // xor rax, r11
        B({0xC3});
        FlushInstructionCache(GetCurrentProcess(), code, 4096);
        using W = uint64_t(__fastcall*)(int64_t*, uint64_t);
        const uint64_t want = 0x0102030405060708ull ^ 0x1111111111111111ull ^ 0xA5A5A5A5A5A5A5A5ull;
        bool same = true;
        const uint64_t hs[3] = {0, h, (h & 0x0FFFFFFFFFFFFFFFull) | (uint64_t)((F<uint32_t>(dev + 0x3C60) + 7) & 0xF) << 60};
        for (uint64_t hh : hs) same = same && ((W)code)(reinterpret_cast<int64_t*>(dev + 0x3C58), hh) == want;
        Check(same, "r9 / r10 / r11 unchanged across the stub");
        // stock equivalence when nothing is pending: same answer as the original's logic for current / other generations
        int diff = 0;
        uint32_t* mk = reinterpret_cast<uint32_t*>(sbase + 4096);
        for (int i = 0; i < 64; ++i) {
            mk[0] = (i & 1) ? 0xAAAAAAAAu : 0xBBBBBBBBu;
            const uint32_t gcur = F<uint32_t>(dev + 0x3C60) & 0xF;
            const uint64_t g = (i & 2) ? gcur : (gcur + 1 + i % 5) & 0xF;
            const uint64_t hh = (4096 / 4) | g << 60;
            const uint64_t stock = (g == gcur && mk[0] == 0xAAAAAAAAu) ? 0 : 1;
            if ((TestNotFreed(reinterpret_cast<int64_t*>(dev + 0x3C58), hh) & 0xFF) != stock) ++diff;
        }
        Check(diff == 0, "stub = stock answer when no TAIL is pending (64 cases)");
    }

    phase = "5";
    printf("phase 5: a present that rotates the chain by 2, and a forced wrong prediction: caught by TAIL N and resynced before "
           "frame N+1 replays (never rendered into a non-current back buffer)\n");
    {
        const int64_t m0 = TestMisses(), r0 = TestResyncs();
        const int t5 = g_tails;
        g_skipNext = 1;
        for (int f = 0; f < 6; ++f) Frame(6000);
        WaitIdle();
        const int64_t r1 = TestResyncs();
        Check(r1 - r0 >= 1, "chain jump: the prediction was resynced");
        TestAct(4);
        for (int f = 0; f < 6; ++f) Frame(6000);
        WaitIdle();
        Check(TestResyncs() - r1 >= 1, "forced wrong prediction: resynced");
        Check(TestMisses() == m0, "no frame rendered into a non-current back buffer");
        Check(!TestDisabled() && g_tails >= t5 + 10, "FO stays on");
        Check(g_seqErrors == 0 && g_seqReplayed == g_seqWritten, "token sequence intact");
        printf("  (TAIL published the real index before the frame began: %lld frames)\n", (long long)TestExactFirst());
    }

    phase = "6";
    printf("phase 6: presents that do not rotate the chain: resync gives up -> FO off for the session, stock frames continue\n");
    for (int f = 0; f < 4; ++f) Frame(6000);
    WaitIdle();
    g_noRotate = 12;
    for (int f = 0; f < 6; ++f) Frame(6000);
    WaitIdle();
    g_noRotate = 0;
    Check(TestDisabled(), "chain stuck: session OFF");
    {
        const int s6 = g_stockPresents;
        for (int f = 0; f < 5; ++f) Frame(6000);
        Check(g_stockPresents == s6 + 5, "stock frame ends after the session OFF");
    }
    StockClean("after the stuck-chain OFF");

    phase = "7";
    g_presentMs = 60;
    printf("phase 7: mid-session session OFF from the TAIL (Tuning.FO_Test 1, the fo6pc path) with the engine locks busy\n");
    TestAct(9);
    for (int f = 0; f < 8; ++f) Frame(6000);
    {
        const int t7 = g_tails, s7 = g_stockPresents;
        Check(!TestDisabled() && t7 > 0, "re-armed: overlapping");
        TestAct(1);
        for (int f = 0; f < 10; ++f) Frame(6000);
        Check(TestDisabled(), "session OFF");
        Check(g_tails >= t7 + 1 && g_stockPresents >= s7 + 8, "the TAIL-issued OFF ends in stock frames that keep coming");
    }
    StockClean("after the TAIL-issued OFF");

    phase = "8";
    printf("phase 8: wrong prediction + session OFF at the same TAIL (Tuning.FO_Test 2, exactly fo6pc)\n");
    TestAct(9);
    for (int f = 0; f < 8; ++f) Frame(6000);
    {
        const int64_t m8 = TestMisses(), r8 = TestResyncs();
        const int s8 = g_stockPresents;
        TestAct(2);
        for (int f = 0; f < 10; ++f) Frame(6000);
        Check(TestDisabled() && TestResyncs() == r8 + 1 && TestMisses() == m8, "wrong prediction resynced, then session OFF");
        Check(g_stockPresents >= s8 + 8, "stock frames keep coming");
    }
    StockClean("after the fo6pc sequence");

    phase = "9";
    g_presentMs = 2;
    printf("phase 9: watchdog (Tuning.FO_Test 3: 5.6 s main stall) -> session OFF + patches restored -> pure stock frames\n");
    TestAct(9);
    for (int f = 0; f < 8; ++f) Frame(6000);
    {
        TestAct(3);
        const int s9 = g_stockPresents;
        for (int f = 0; f < 6; ++f) Frame(6000);  // the first one stalls 5.6 s inside FO
        Check(TestWatchdogFired() && TestDisabled(), "watchdog fired, session OFF");
        Check(!TestInstalled(), "every patch restored (the stalled frame was not inside a TAIL)");
        Check(g_stockPresents >= s9 + 6, "frames continue on the stock engine");
    }
    StockClean("after the watchdog");

    phase = "9b";
    printf("phase 9b: partial restore - GfxDevicePresentVfn already stock, frame-begin hooks still patched: the back-buffer index "
           "must stay the real one every frame\n");
    TestAct(9);
    for (int f = 0; f < 4; ++f) Frame(6000);
    SetTuning("FO", "0", rep, sizeof rep);
    for (int f = 0; f < 2; ++f) Frame(6000);
    {
        int stale = 0;
        for (int f = 0; f < 6; ++f) {
            TestPresentSpin(dev);
            TestPPUpdate(dev + 0x61A0, dev);
            const uint32_t got = TestGetBB(&g_sc);
            if (got != g_scIdx.load()) ++stale;
            Emit(6000, 0);
            StockPresent(dev, 0);  // FoPresent is no longer called (its site was restored first)
            ++g_framesDone;
        }
        while (F<volatile uint8_t>(dev + 0x7728)) Sleep(0);
        Check(stale == 0, "FoGetBB returned the current back buffer on every frame without FoPresent");
    }

    phase = "11";
    printf("phase 11 (pc2 fo2 ordering): switch on after stock frames while the replay lags; effect workers write early and the "
           "first FO frame's sub-stream data crosses the recorder's middle -> that frame must end stock, nothing overwritten "
           "before replay\n");
    TestAct(9);
    SetTuning("FO", "0", rep, sizeof rep);
    for (int f = 0; f < 3; ++f) Frame(6000);
    WaitIdle();
    {
        const int e0 = g_subErr.load(), s0 = g_stockPresents;
        SetTuning("FO", "1", rep, sizeof rep);
        g_earlySubDwords = 9000;  // 36 KB per recorder > its 32 KB half: the first FO frame crosses the middle
        g_pause = true;           // the replay lags: nothing of this frame is replayed until the next frame has begun
        std::thread([] {
            Sleep(150);
            g_pause = false;
        }).detach();
        Frame(6000);
        g_earlySubDwords = 600;   // later frames: workers write before the barrier while frame N still replays
        for (int f = 0; f < 8; ++f) Frame(60000);
        g_earlySubDwords = 0;
        WaitIdle();
        Check(g_subErr.load() == e0, "no sub-stream data overwritten before it was replayed");
        Check(g_stockPresents >= s0 + 1, "the crossing frame ended on the stock path");
        Check(g_tails >= 1 && !TestDisabled(), "overlap continues after it");
    }

    phase = "10";
    printf("phase 10: re-arm after the watchdog (reinstall) -> overlap again\n");
    {
        TestAct(9);
        const int t10 = g_tails;
        for (int f = 0; f < 8; ++f) Frame(6000);
        WaitIdle();
        Check(TestInstalled() && !TestDisabled() && g_tails >= t10 + 6, "reinstalled, overlapping");
        SetTuning("FO", "0", rep, sizeof rep);
        for (int f = 0; f < 3; ++f) Frame(6000);
    }
    StockClean("live switch off after re-arm");
    Check(g_jobRuns > 0, "tile-job thread exercised +CF8h shared throughout");

    char sl[1024];
    StatusLine(sl, sizeof sl, 30);
    printf("  %s\n", sl);
    g_stop = true;
    rt.join();
    pt.join();
    jt.join();
    hang.join();
    printf("presents %d (stock %d), tails %d, submits %d, PP handler calls %d\n", g_presents.load(), g_stockPresents.load(),
           g_tails.load(), g_submits.load(), g_ppCalls.load());
    printf(g_fails ? "FAILED (%d)\n" : "ALL PASSED\n", g_fails);
    return g_fails ? 1 : 0;
}
