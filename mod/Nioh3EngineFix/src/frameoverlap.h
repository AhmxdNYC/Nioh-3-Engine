// frameoverlap.h - FO: overlap frame N's D3D12 replay with frame N+1's recording (1.7.59-fo1, default off).
// notes\renderer\frame-overlap.md (Stage 1 lifetime audit, Stage 2 design section 5).
//
// Stock frame end (GfxDevicePresent 3D62E4, main thread): final kick, SPIN until the replay thread
// (SynergisticDispatcherTask) has replayed the whole frame (~6 ms on pc2), reset the token stream, then on the main thread:
// timestamp resolve, back buffer -> PRESENT, close + submit the list, KtglCache frame end, Present, frame-slot advance.
// FO instead:
//   E  (main, frame end N): wait the GPU for frame N-2 (stock CPU lead), KtglCache frame end with a FORWARD fence (the ASI's
//      own ID3D12Fence, value N, signalled on the GFX queue by TAIL N after the frame's last list), sync-point table advance,
//      final kick as stock, then a 0x8F token {TailFn, frame record} in its own stretch, then switch the stream to its OTHER
//      HALF (no drain) and return.
//   TAIL (replay thread, inside that 0x8F token): the rest of stock GfxDevicePresent (resolve, PRESENT barrier, submit,
//      signal the forward fence, PSO-cache maintenance, present hand-off, frame-slot advance) - everything that touches
//      SynCtx / the slot index / the allocators stays on the replay thread.
//   B  (main, barrier, frame N+1): before the first frame-graph entry whose node index >= FO_Pct % of the last frame's
//      count (Renderer vfn+130h), and at the latest at the next E: wait TAIL N done, reset the sub-streams. After B the main
//      thread is in the stock situation.
//   Frame begin (WaitSubmitThread 7D9B6C call sites): no present-pending spin, predicted back-buffer index (verified by
//      TAIL), PagePoolFrameUpdate / 7D9C24 under the PP lock; the replay thread takes the PP lock in the PagePool token
//      handlers 82 83 95 96 97 98 9A.
//   Handles (HandleSlotIsNotFreed 58C434, replaced by a register-exact machine-code stub: LTCG callers keep r9-r11 live across
//      it): a handle of the previous generation is "not freed" while TAIL N is pending and its 0xA7 marker is still unreplayed.
//   Back-buffer index (fo7): a frame recorded for a non-current back buffer REMOVES THE DEVICE on the PC (fo6pc / fo7pc), so no
//      prediction reaches the GPU unverified. TAIL N waits for present N to retire and publishes frame N+1's real index; main
//      uses it when TAIL N was first, else predicts (newest real index + one rotation per frame) and TAIL N verifies before N+1
//      replays; a wrong prediction is fixed by presenting the current (PRESENT-state, older) buffer until the chain matches.
//   Stock frame end after an FO frame (switch off, session OFF, fallbacks): wait the pending present first (stock Present reads
//      GetCurrentBackBufferIndex before its own present spin), stream / sub-stream extents back to full.
// Nothing is patched until the switch first goes ON (FrameOverlap = 1 at boot, or live Tuning.FO = 1 with FrameOverlap = 2).
// Watchdog: no frame (engine frame counter +3C1Ch) for 5 s -> thread dump, FO off for the session, all patches restored (if no
// TAIL is pending); a second dump at 15 s. Live Tuning.FO_Test (tests): 1 session OFF from the TAIL, 2 wrong prediction + OFF,
// 3 watchdog (5.6 s stall), 4 wrong prediction, 5 / 6 switch off / on, 7 stall with the watchdog suppressed, 8 the
// watchdog's restore while rendering, 9 re-arm, 10 8 s stall, 100 self-test of all of them.
// Fallbacks (stock path for the frame, or FO off for the session): switch off, frame data spanning both stream halves,
// sync tables < 3, timestamp / pipeline-statistics queries in use, back-buffer prediction mismatch, render-size change and
// device reset / teardown / other Present callers (wait TAIL first, then stock).
//   [GpuOpt] FrameOverlap = 0 | 1 | 2   (0 = nothing installed; 1 = installed + ON; 2 = installed, switch OFF)
//   [Tuning] FO_Pct = 25 (live 'Tuning.FO_Pct'), live switch 'Tuning.FO = 0|1'. "stats/s FO:" line every 30 s.
#pragma once
#include <cstddef>
#include <cstdint>

namespace frameoverlap {

// ---- engine interface (v2.02 RVAs; the harness injects fakes) ----------------------------------------------------------
struct Engine {
    void (*present)(uintptr_t dev, uintptr_t p2);                                                    // GfxDevicePresent 3D62E4
    void (*ringPush)(uintptr_t ring, const uint64_t* item, uint64_t submit, uintptr_t stats);       // DispatcherRingPush 25426C
    void (*subReset)(uintptr_t sub);                                                                 // 253F2C
    void (*transition)(uintptr_t ctx, uint64_t res, uint64_t st, uint64_t a, uint64_t b, uint64_t c);  // SynTransitionResource 115E24
    uint64_t (*submitFrame)(uintptr_t ring, uintptr_t pool, uint64_t f);                             // GfxSubmitFrameCmdList 252758
    void (*psoMaint)(uintptr_t ctx, uint64_t slot, uint64_t fence);                                  // 8CDA94
    void (*spinPresent)(uintptr_t dev);                                                              // SpinWaitFlagSleep0 7D9E90
    void (*removedReason)(uint64_t device, int64_t hr);                                              // 1896810
    void (*fatal)(uint64_t gdev, int64_t hr);                                                        // 18956D0
    void (*waitFence)(uintptr_t dev, uint64_t v, void* ev);                                          // GfxWaitFenceValue 7D9E1C
    void (*recRebuild)(uintptr_t ctx);                                                               // RenderFrameRecordRebuild 9AFED0
    void (*recycle)(uintptr_t ctx);                                                                  // 2EFB73C (list-ring recycle)
    uint64_t (*qpc)();                                                                               // QpcNow 205530
    // KtglCache frame end 12682C, rebuilt here with a forward fence
    void (*ringNorm)(uintptr_t ring);                                                                // 1263F0
    uint64_t (*tryX2)(uintptr_t srw);                                                                // 1264F0
    uint64_t (*tryX1)(uintptr_t srw);                                                                // 126480
    void (*cacheCompact)(uintptr_t pool, uintptr_t dev);                                             // 250650
    void (*unlockWake)(uintptr_t srw);                                                               // 12645C
    void (*tagFence)(uintptr_t ring, uintptr_t dev, uint64_t v);                                     // 126744
    void (*retire)(uintptr_t ring, uintptr_t a, uintptr_t b, uintptr_t c);                           // 1280C8
    void (*kick)(uintptr_t ev);                                                                      // 126984
    void (*process)(uintptr_t ring, uintptr_t dev, uint64_t stamp, uintptr_t a, uintptr_t b);        // 126A40
    void (*signalPair)(uintptr_t ev);                                                                // 1269D4
    void (*ppUpdate)(uintptr_t pool, uintptr_t dev);                                                 // PagePoolFrameUpdate 252510
    void (*ppOps)(uintptr_t pool, uintptr_t dev);                                                    // 7D9C24
    void (*cacheFlushBegin)(uintptr_t r);                                                            // RenderFrameBeginCacheFlush 32FE938
    void (*endFrame)(uintptr_t dev);                                                                 // CmdStreamEndFrame 418B94
    void (*node130)(uintptr_t r, uintptr_t entry);                                                   // 113150
    uintptr_t gfxGlobal;                                                                             // &g_GfxDevice 45BA588
    uintptr_t slCb[3];                                                                               // 4BD0998 / 9A8 / 9B0
};

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;
    uintptr_t base = 0;       // Nioh3.exe image base
    bool knownBuild = false;  // v2.02 only
};
void Start(const StartInfo& info);
bool SetTuning(const char* key, const char* val, char* reply, size_t n);  // FO, FO_Pct
void StatusLine(char* buf, unsigned n, unsigned secs);

#ifdef FO_TEST
// harness entry points: the engine functions are injected, the state machine is the real one
void TestInstall(uintptr_t fakeBase, const Engine* e);
void TestPresent(uintptr_t dev);
void TestNode(uintptr_t renderer);
uint32_t TestGetBB(void* swapchain);
void TestPresentSpin(uintptr_t dev);  // WaitSubmitThread's present spin (7D9B91)
void TestPPUpdate(uintptr_t pool, uintptr_t dev);
uint64_t TestNotFreed(int64_t* p, uint64_t h);
const uint8_t* TestStub(size_t* n);
void TestTail(void* lc, void* rec);
void TestPPHandler(uintptr_t ctx, uint8_t** cur, void (*orig)(uintptr_t, uint8_t**));
uint64_t TestIssued();
uint32_t TestPrevGen();
uint64_t TestTailDone();
bool TestDisabled();
void TestAct(int v);          // Tuning.FO_Test actions 1 2 3 4 9
bool TestInstalled();         // false after the watchdog restored every patch
void TestWatchdogSuppress(int on);  // for frames that are slow on purpose
bool TestWatchdogFired();
int64_t TestMisses();         // frames rendered into a non-current back buffer (must stay 0)
int64_t TestResyncs();        // wrong predictions fixed before the frame replayed
int64_t TestExactFirst();     // TAIL published the real index before the frame began
bool TestPPFree();            // FO's PP lock is not held
#endif

}  // namespace frameoverlap
