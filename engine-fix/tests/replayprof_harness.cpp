// replayprof_harness.cpp - offline test of src/replayprof.cpp + src/replayprof.asm (RPF replay-thread profiler).
// Built and run by tests/run_replayprof_harness.py. Not a game test.
//   1. vtable thunks: arguments (4 registers + 8 stack slots), return values and the caller's stack reach the original
//      exactly, on the replay thread and on other threads; only replay-thread calls are counted; a hooked method calling
//      another hooked method is counted once (outer); per (handler, group) attribution.
//   2. token thunks: a fake token table driven by a fake dispatcher loop with the engine's call shape (hl_loop, asm): the
//      replay thread is learned from the loop's return address, per-op counts / inclusive / self time (nested sub-range
//      replay), tokens replayed on another thread are counted but not timed, the frame token gives the frame count.
//   3. waits: the loop's own wait (hl_wait) is idle; a wait inside a handler is attributed to the handler.
//   4. Switch(false) restores every table / vtable / import slot (stock pointers back, nothing counted), Switch(true) again.
//   5. Report() reproduces the known per-frame values; probe cost per token / per D3D12 call (on vs off) is printed.
#define NOMINMAX
#include <windows.h>
#include <intrin.h>

#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "../src/replayprof_detail.h"
#include "../src/replayprof_names.h"

using namespace replayprof;
using namespace replayprof::detail;

extern "C" {
void hl_loop(void** table, void* ctx, uint8_t** cursor);
void hl_loop_ret();
DWORD hl_wait(void** slot, HANDLE h, DWORD ms);
void hl_wait_ret();
extern volatile uint32_t rp_tid;
bool RpForwards(const void* slotValue, const void* want);
}

static std::vector<std::string> g_lines;
static void Log(const char* fmt, ...) {
    char b[2048];
    va_list a;
    va_start(a, fmt);
    vsnprintf(b, sizeof b, fmt, a);
    va_end(a);
    g_lines.push_back(b);
    printf("  log: %s\n", b);
}
static int fails = 0;
static void Check(bool c, const char* m) {
    if (!c) {
        ++fails;
        printf("FAIL: %s\n", m);
    } else {
        printf("ok: %s\n", m);
    }
}

static double g_qpcUs = 0;
static uint64_t Qpc() {
    LARGE_INTEGER q;
    QueryPerformanceCounter(&q);
    return (uint64_t)q.QuadPart;
}
static void SpinUs(double us) {
    const uint64_t t0 = Qpc();
    while ((double)(Qpc() - t0) * g_qpcUs < us) YieldProcessor();
}

// ---- fake "command list" class -----------------------------------------------------------------------------------------
constexpr int kSlotBarrier = 26, kSlotDraw = 13, kSlotCopyTex = 16, kSlotRootTable = 32, kSlotClose = 9, kSlotExecInd = 59;
constexpr int kVtN = 81;
struct FakeList {
    void** vt;
};
static void* g_vt[kVtN];
static std::atomic<int> g_calls{0};
static uint64_t g_lastSum = 0;
static FakeList g_list;

// 12 integer arguments: this + 3 registers + 8 on the stack (the most the thunk copies)
static uint64_t __stdcall M12(FakeList* self, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6,
                              uint64_t a7, uint64_t a8, uint64_t a9, uint64_t a10, uint64_t a11) {
    ++g_calls;
    const uint64_t s = (self == &g_list ? 1 : 0) + a1 * 3 + a2 * 5 + a3 * 7 + a4 * 11 + a5 * 13 + a6 * 17 + a7 * 19 + a8 * 23 +
                       a9 * 29 + a10 * 31 + a11 * 37;
    g_lastSum = s;
    return s;
}
static void __stdcall MSpin(FakeList*, double us) {  // ResourceBarrier slot stand-in: burns `us` (xmm1 argument)
    ++g_calls;
    SpinUs(us);
}
static void __stdcall MNested(FakeList* self, double us) {  // calls another hooked method on the same list
    ++g_calls;
    using Fn = void(__stdcall*)(FakeList*, double);
    ((Fn)self->vt[kSlotBarrier])(self, us);
    SpinUs(us);
}
static void __stdcall MNop(FakeList*) { ++g_calls; }
static ULONG __stdcall MRef(FakeList*) { return 1; }
static HRESULT __stdcall MQi(FakeList*, const IID&, void** o) {
    *o = nullptr;
    return E_NOINTERFACE;
}

using SpinFn = void(__stdcall*)(FakeList*, double);
using NopFn = void(__stdcall*)(FakeList*);
using M12Fn = uint64_t(__stdcall*)(FakeList*, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t,
                                   uint64_t, uint64_t, uint64_t);

// ---- fake token table ----------------------------------------------------------------------------------------------------
// token = u32 { op | arg << 16 }, handlers advance 4 bytes. Ops used:
//   95 frame marker | 10 spin arg us | 11 D3D: barrier spin arg us + 1 draw (M12) | 12 D3D nested (MNested arg us)
//   90 sub-range: replays the next `arg` tokens through the table itself (like 115750) | 20 wait 2 ms inside the handler
//   any other: no-op
static void* g_table[kTokens];
static void* g_stock[kTokens];
static void** g_wfsoSlot = nullptr;
static void* g_wfsoImport = nullptr;
static HANDLE g_ev = nullptr;
static std::atomic<int> g_tokCalls{0};

static void HSpin(void*, uint8_t** cur) {
    const uint32_t t = *(uint32_t*)*cur;
    *cur += 4;
    ++g_tokCalls;
    SpinUs((double)(t >> 16));
}
static void HD3D(void*, uint8_t** cur) {
    const uint32_t t = *(uint32_t*)*cur;
    *cur += 4;
    ++g_tokCalls;
    ((SpinFn)g_list.vt[kSlotBarrier])(&g_list, (double)(t >> 16));
    ((M12Fn)g_list.vt[kSlotDraw])(&g_list, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11);
}
static void HNested(void*, uint8_t** cur) {
    const uint32_t t = *(uint32_t*)*cur;
    *cur += 4;
    ++g_tokCalls;
    ((SpinFn)g_list.vt[kSlotCopyTex])(&g_list, (double)(t >> 16));
}
static void HSub(void* c, uint8_t** cur) {
    const uint32_t t = *(uint32_t*)*cur;
    *cur += 4;
    ++g_tokCalls;
    const uint32_t n = t >> 16;
    for (uint32_t i = 0; i < n; ++i) {
        const uint16_t op = *(uint16_t*)*cur;
        ((void (*)(void*, uint8_t**))g_table[op])(c, cur);  // through the table, like 115750
    }
}
static void HWait(void*, uint8_t** cur) {
    *cur += 4;
    ++g_tokCalls;
    ((DWORD(WINAPI*)(HANDLE, DWORD))*g_wfsoSlot)(g_ev, 2);  // through the import, times out after 2 ms
}
static void HNop(void*, uint8_t** cur) {
    *cur += 4;
    ++g_tokCalls;
}

static std::vector<uint32_t> Frame() {
    return {0x95,          0x10 | 300u << 16, 0x11 | 200u << 16, 0x12 | 100u << 16, 0x90 | 2u << 16, 0x10 | 100u << 16,
            0x01,          0x20,              0x01,              0x01};
}
// per frame (us): 10: 300 + 100 (inside 90) = 2 calls; 11: barrier 200 + draw; 12: nested 100 + 100 (inner barrier);
// 90: incl >= 100 (child 10 + 01); 20: 2000 wait; 01 x3 no-op (one inside 90)

static void RunFrames(int frames, HANDLE idleEv, int idleMs) {
    for (int f = 0; f < frames; ++f) {
        std::vector<uint32_t> s = Frame();
        uint8_t* cur[2] = {(uint8_t*)s.data(), (uint8_t*)(s.data() + s.size())};
        hl_loop(g_table, nullptr, cur);
        hl_wait(g_wfsoSlot, idleEv, (DWORD)idleMs);  // the loop's own wait (idle)
    }
}

int main() {
    LARGE_INTEGER qf;
    QueryPerformanceFrequency(&qf);
    g_qpcUs = 1e6 / (double)qf.QuadPart;
    g_ev = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    // fake vtable
    for (int i = 0; i < kVtN; ++i) g_vt[i] = (void*)&MNop;
    g_vt[0] = (void*)&MQi;
    g_vt[1] = g_vt[2] = (void*)&MRef;
    g_vt[kSlotBarrier] = (void*)&MSpin;
    g_vt[kSlotCopyTex] = (void*)&MNested;
    g_vt[kSlotDraw] = (void*)&M12;
    g_vt[kSlotExecInd] = (void*)&M12;
    g_list.vt = g_vt;
    void* stockVt[kVtN];
    memcpy(stockVt, g_vt, sizeof stockVt);

    // fake table
    for (int i = 0; i < kTokens; ++i) g_table[i] = (void*)&HNop;
    g_table[0x10] = (void*)&HSpin;
    g_table[0x11] = (void*)&HD3D;
    g_table[0x12] = (void*)&HNested;
    g_table[0x90] = (void*)&HSub;
    g_table[0x20] = (void*)&HWait;
    memcpy(g_stock, g_table, sizeof g_stock);
    // fake import slot
    g_wfsoImport = (void*)&WaitForSingleObject;
    g_wfsoSlot = &g_wfsoImport;

    Check(GroupOf(kKindList, "ResourceBarrier") == kGBarrier && GroupOf(kKindList, "DrawIndexedInstanced") == kGDraw &&
              GroupOf(kKindList, "SetGraphicsRootDescriptorTable") == kGRoot && GroupOf(kKindList, "Close") == kGLife &&
              GroupOf(kKindList, "ClearState") == kGLife && GroupOf(kKindList, "ClearRenderTargetView") == kGCopy &&
              GroupOf(kKindList, "ResolveQueryData") == kGQuery && GroupOf(kKindList, "IASetVertexBuffers") == kGState &&
              GroupOf(kKindList, "SetPipelineState") == kGPso && GroupOf(kKindList, "Release") == kGCom &&
              GroupOf(kKindDevice, "CopyDescriptorsSimple") == kGDescCopy &&
              GroupOf(kKindDevice, "CreateConstantBufferView") == kGViewCreate &&
              GroupOf(kKindDevice, "CreateSampler") == kGViewCreate && GroupOf(kKindDevice, "CreateCommittedResource") == kGDevOther &&
              GroupOf(kKindQueue, "ExecuteCommandLists") == kGEcl && GroupOf(kKindQueue, "Signal") == kGQueueSync &&
              GroupOf(kKindFence, "GetCompletedValue") == kGFence && GroupOf(kKindResource, "Map") == kGMap &&
              GroupOf(kKindResource, "GetGPUVirtualAddress") == kGResOther,
          "call groups by method name");
    int nn = 0;
    const char* const* ln = KindNames(kKindList, nn);
    Check(nn == 81 && !strcmp(ln[kSlotBarrier], "ResourceBarrier") && !strcmp(ln[kSlotDraw], "DrawIndexedInstanced") &&
              !strcmp(ln[kSlotCopyTex], "CopyTextureRegion") && !strcmp(ln[kSlotExecInd], "ExecuteIndirect"),
          "list slot names (d3d12.h order)");

    TestSetup((uintptr_t)g_table, (uintptr_t)&hl_loop_ret, (uintptr_t)&hl_wait_ret, g_wfsoSlot, 1);
    Check(HookVtable("list", kKindList, g_vt, kVtN) == 0, "vtable hooked as set 0");
    Check(HookVtable("list", kKindList, g_vt, kVtN) == 0, "same vtable again -> same set");
    Check(g_vt[kSlotBarrier] != (void*)&MSpin, "vtable slot points at the thunk");

    // thunks pass through before the replay thread is known (rp_on = 0 here too)
    g_calls = 0;
    uint64_t r = ((M12Fn)g_vt[kSlotDraw])(&g_list, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11);
    const uint64_t want = 1 + 3 + 10 + 21 + 44 + 65 + 102 + 133 + 184 + 261 + 310 + 407;
    Check(r == want && g_calls == 1, "pass-through: 12 arguments + return value");

    Switch(true);
    Check(g_table[0x10] != g_stock[0x10] && *g_wfsoSlot != (void*)&WaitForSingleObject, "Switch(1): table + import slot replaced");
    // replay thread: learned from the loop's return address
    const int kFrames = 20;
    std::thread rt([&] { RunFrames(kFrames, g_ev, 3); });
    rt.join();
    Counters c = Read();
    Check(c.tid != 0, "replay thread learned from the loop call site");
    Check(c.n[0x95] == (uint64_t)kFrames && c.n[0x10] == 2ull * kFrames && c.n[0x01] == 3ull * kFrames && c.n[0x90] == (uint64_t)kFrames,
          "per-op counts (incl. tokens replayed inside the sub-range handler)");
    // timing (TSC -> us via QPC calibration)
    const uint64_t t0 = __rdtsc(), q0 = Qpc();
    SpinUs(20000);
    const double usPerTick = (double)(Qpc() - q0) * g_qpcUs / (double)(__rdtsc() - t0);
    auto US = [&](uint64_t ticks) { return (double)ticks * usPerTick / kFrames; };
    auto MAT = [&](int op, int g, int w) { return c.mat[((size_t)op * kGroups + g) * 2 + w]; };
    printf("  per frame: 10 incl %.0f us, 11 incl %.0f, 12 incl %.0f, 90 incl %.0f child %.0f, 20 incl %.0f, idle %.0f us (%llu)\n",
           US(c.incl[0x10]), US(c.incl[0x11]), US(c.incl[0x12]), US(c.incl[0x90]), US(c.child[0x90]), US(c.incl[0x20]),
           US(c.idle), (unsigned long long)c.idleN);
    Check(US(c.incl[0x10]) >= 400 && US(c.incl[0x10]) < 480, "op 10 inclusive ~400 us/frame");
    Check(US(c.incl[0x90]) >= 100 && std::fabs(US(c.child[0x90]) - US(c.incl[0x90])) < 15,
          "sub-range handler: child time = nested handlers (self ~0)");
    Check(c.child[0x10] == 0, "leaf handler has no child time");
    Check(MAT(0x11, kGBarrier, 1) == (uint64_t)kFrames && US(MAT(0x11, kGBarrier, 0)) >= 200 && US(MAT(0x11, kGBarrier, 0)) < 240,
          "barrier call attributed to op 11 (~200 us)");
    Check(MAT(0x11, kGDraw, 1) == (uint64_t)kFrames && g_lastSum == want, "draw call with 12 args attributed to op 11, args intact");
    Check(MAT(0x12, kGCopy, 1) == (uint64_t)kFrames && MAT(0x12, kGBarrier, 1) == 0 && US(MAT(0x12, kGCopy, 0)) >= 200,
          "nested hooked call counted once (outer CopyTextureRegion, ~200 us)");
    Check(MAT(0x20, kGWait, 1) == (uint64_t)kFrames && US(MAT(0x20, kGWait, 0)) >= 1500, "wait inside a handler -> op 20 wait");
    Check(c.idleN == (uint64_t)kFrames && US(c.idle) >= 2500, "loop's own wait -> idle");
    Check(c.outer > 0 && US(c.outer) >= US(c.incl[0x10]) + US(c.incl[0x11]) + US(c.incl[0x20]),
          "outermost handler time covers the top-level handlers");
    Check(c.id[(0 * kSlots + kSlotBarrier) * 2 + 1] == (uint64_t)kFrames && c.id[(0 * kSlots + kSlotCopyTex) * 2 + 1] == (uint64_t)kFrames,
          "per-method counts (replay thread only)");

    // other thread: counted (tokens) but not timed; D3D calls pass through uncounted
    const uint64_t barBefore = c.id[(0 * kSlots + kSlotBarrier) * 2 + 1];
    std::thread ot([&] {
        std::vector<uint32_t> s = Frame();
        uint8_t* cur[2] = {(uint8_t*)s.data(), (uint8_t*)(s.data() + s.size())};
        hl_loop(g_table, nullptr, cur);
        g_lastSum = 0;
        ((M12Fn)g_vt[kSlotExecInd])(&g_list, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11);
    });
    ot.join();
    Counters c2 = Read();
    Check(c2.other[0x95] == 1 && c2.other[0x10] == 2 && c2.n[0x95] == c.n[0x95], "other thread: tokens counted, not timed");
    Check(c2.id[(0 * kSlots + kSlotBarrier) * 2 + 1] == barBefore && g_lastSum == want,
          "other thread: D3D calls pass through uncounted, args intact");

    // report
    g_lines.clear();
    Counters z = {};
    const double msPerTick = usPerTick / 1000.0;
    const double sec = 1.0;  // pretend the 20 frames took 1 s: wall 50 ms/f
    Report(z, c, msPerTick, sec, 30.0, 10, 10, &Log);
    bool head = false, top = false, grp = false, api = false;
    for (auto& l : g_lines) {
        if (l.rfind("RPF: 1.0 s, 20 frames", 0) == 0 && l.find("wall 50.00") != std::string::npos) head = true;
        if (l.find("RPF tok  1 20 ") == 0 || l.find("RPF tok  1 ") == 0) top = true;
        if (l.rfind("RPF groups", 0) == 0 && l.find("barrier") != std::string::npos && l.find("wait (WFSO)") != std::string::npos)
            grp = true;
        if (l.find("RPF api  1 list.") == 0) api = true;
    }
    Check(head && top && grp && api, "Report: header (20 frames, wall 50 ms/f), handler top, groups, api top");

    // Switch(false): everything back, nothing counted
    Switch(false);
    bool same = memcmp(g_table, g_stock, sizeof g_stock) == 0 && memcmp(g_vt, stockVt, sizeof stockVt) == 0 &&
                *g_wfsoSlot == (void*)&WaitForSingleObject;
    Check(same, "Switch(0): table, vtable and import slot restored byte-identical");
    Counters c3 = Read();
    std::thread rt2([&] { RunFrames(3, g_ev, 1); });
    rt2.join();
    Counters c4 = Read();
    Check(c4.n[0x95] == c3.n[0x95] && c4.idleN == c3.idleN && c4.other[0x95] == c3.other[0x95], "off: nothing counted");
    Switch(true);
    Check(g_table[0x10] != g_stock[0x10] && g_vt[kSlotBarrier] != (void*)&MSpin, "Switch(1) again re-installs");

    // probe cost on this thread (made the replay thread): trivial tokens, and tokens making one trivial D3D call, on vs off
    Switch(false);
    g_stock[0x30] = g_table[0x30] = (void*)+[](void*, uint8_t** cur) {
        *cur += 4;
        ((NopFn)g_list.vt[kSlotRootTable])(&g_list);
    };
    rp_tid = GetCurrentThreadId();
    {  // SetPipelineState redundancy + Forwards (gpuopt watch)
        Switch(true);
        using PsoFn = void(__stdcall*)(FakeList*, uintptr_t);
        const Counters p0 = Read();
        FakeList other;
        other.vt = g_vt;
        ((PsoFn)g_vt[25])(&g_list, 0x1234);
        ((PsoFn)g_vt[25])(&g_list, 0x1234);   // redundant
        ((PsoFn)g_vt[25])(&other, 0x1234);    // other list: new
        ((PsoFn)g_vt[25])(&other, 0x1234);    // redundant
        ((NopFn)g_vt[10])(&other);            // Reset
        ((PsoFn)g_vt[25])(&other, 0x1234);    // after Reset: new
        ((PsoFn)g_vt[25])(&other, 0x5678);    // other PSO: new
        const Counters p1 = Read();
        Check(p1.psoRedN - p0.psoRedN == 2 && p1.id[25 * 2 + 1] - p0.id[25 * 2 + 1] == 6, "redundant SetPipelineState: 2 of 6");
        Check(RpForwards(g_vt[kSlotBarrier], (void*)&MSpin) && !RpForwards(g_vt[kSlotBarrier], (void*)&MNop) &&
                  !RpForwards((void*)&MSpin, (void*)&MSpin),
              "RpForwards: our thunk in front of the wanted method");
    }
    auto costRun = [&](bool on, uint32_t tok) {
        Switch(on);
        const int N = 200000;
        std::vector<uint32_t> s(N, tok);
        double best = 1e30;
        for (int rep = 0; rep < 7; ++rep) {
            uint8_t* cur[2] = {(uint8_t*)s.data(), (uint8_t*)(s.data() + s.size())};
            const uint64_t a = Qpc();
            hl_loop(g_table, nullptr, cur);
            best = std::min(best, (double)(Qpc() - a) * g_qpcUs * 1000.0 / N);
        }
        return best;
    };
    const double tOff = costRun(false, 0x01), tOn = costRun(true, 0x01), dOff = costRun(false, 0x30), dOn = costRun(true, 0x30);
    printf("  probe cost: token %.1f ns (off %.1f, on %.1f); token + 1 D3D12 call %.1f ns (off %.1f, on %.1f) -> per call %.1f ns\n",
           tOn - tOff, tOff, tOn, dOn - dOff, dOff, dOn, (dOn - dOff) - (tOn - tOff));
    Check(tOn - tOff < 200 && dOn - dOff < 400, "probe cost per token / per call is bounded");
    Switch(false);
    Check(memcmp(g_table, g_stock, sizeof g_stock) == 0 && memcmp(g_vt, stockVt, sizeof stockVt) == 0, "final restore");
    printf("%s\n", fails ? "FAILED" : "ALL PASSED");
    return fails ? 1 : 0;
}
