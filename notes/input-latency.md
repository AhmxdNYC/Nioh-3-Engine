# Input latency: controller to photons, and what can be cut without changing the image

Nioh 3 Steam v2.02. Static work only (dump, Ghidra export, existing PresentMon/MangoHud captures). The game was not run.
Target: a real 60 fps on the ROG Ally (Proton, vkd3d-proton, gamescope, 120 Hz panel, in-game VSync ON, cap 60,
frame generation OFF). PC numbers are a bonus.
Script: `patcher\latency_live.fsx` (PC, live; offline check passes). Helpers: `re\scripts_latency\`.
Symbols: `re\symbols\incoming\input-latency.tsv`.

## TL;DR

1. **The input sample is stale before the game reads it.** The pad is polled when the input worker is kicked at the
   **end** of logic frame N (`UpdateDispatchB` 5AB6CF). The game reads it at the **start** of logic frame N+1
   (`FrameEarlyUpdate` → 5ACAAA wait → `GameModeUpdateMain`). In between sit the rest of FrameEnd, the **QPC frame
   limiter** (FrameEnd 5AD292), and every logic-thread wait (FrameSync and the render-graph nodes). At a 60 cap with
   CPU headroom that gap is about the limiter slack: **~5–9 ms on average**. It is pure waste.
   **Fix IK**: kick the poll right before the engine's own pad-stage wait at 5ACAAA instead. Still one poll per frame,
   no rendering change. It is built in `latency_live.fsx` (default off) and designed for the ASI (§6.1).
2. **Where the limiter sits:** it runs after the game update and before the frame is handed to rendering. So it also
   (very likely) delays the commit to the render thread. That costs a second ~3–7 ms, after input is consumed.
   **LM / LS** (wait at logic frame start, Reflex style) would remove it, but it changes pacing. Measure first: the
   TS probes in `latency_live.fsx` show it (`L->W`).
3. **The waitable-object wait (R1 site) is at render begin, on the render thread, after the logic frame.** The game
   creates the swapchain with `FRAME_LATENCY_WAITABLE_OBJECT` and **never calls SetMaximumFrameLatency**. I scanned
   every swapchain vtable call: there is no vtbl+F8h call. So the max latency is the default **1**.
   - **On vkd3d-proton** the waitable is released only after `vkWaitForPresentKHR`, i.e. when the previous frame has
     been **presented**. With latency 1 the render thread can't start recording frame N+1 until frame N is on screen.
     CPU recording and GPU work cannot overlap across frames.
   - That gives the lowest latency per frame, but it probably **costs fps** on the Ally. Ally frame times cluster
     at vblank multiples (16.7 / 25 / 33 / 50 ms), with GPU load of only 81–94 %.
   - `[Latency] MaxFrameLatency = 2` is therefore **a throughput lever, not a latency lever**. Use 1 if a real 60
     holds at 1. Use 2 if 60 only holds with the overlap. A/B it (§7).
   - **vkd3d caveat for the ASI's live menu item:** vkd3d's SetMaximumFrameLatency only ever *adds* permits to the
     semaphore. Going back down (2 → 1, or "game") changes the reported value but keeps the extra permit until the
     swapchain is recreated, so the queue stays at 2. For a clean A/B on the Ally, set the value in the ini and
     restart (or toggle FG/resolution to rebuild the swapchain).
   - `VKD3D_SWAPCHAIN_LATENCY_FRAMES=1` should change almost nothing, because the app's latency-1 waitable is the
     tighter limit. That is a testable prediction.
4. **Engine frames in flight:** after each Present the render thread waits for the GPU fence of the frame slot it is
   about to reuse (`GfxFrameSlotAdvance` 8BEC48, BufferCount = 3 slots, i.e. frame N-2). The latency-1 waitable is
   tighter, so this wait only matters if the waitable is off. Not a lever.
5. **Present thread / 7D9B6C (P2):** at render begin the render thread waits for the previous `Present()` call to
   return. Under vkd3d, Present is non-blocking unless its internal 3-frame throttle trips. With latency 1 the
   previous present was already *displayed* before render begin, so this wait is ~0. P2 gains ≤0.3 ms and is
   high-risk. Skip it.
   - W6 (ASI 1.1.0) does this right: a short spin, then WaitOnAddress, with a waker at the present thread's flag
     clear (1A822D9). That keeps the wake immediate, so W6 is latency-neutral. A plain `Sleep(1)` would not be:
     it adds 0.5–1 ms+ per frame whenever Present is slow.
6. **Ranked same-look reductions for the Ally at a held 60:**

| rank | id | what | est. saving (avg) | risk | status |
|---|---|---|---|---|---|
| 1 | **IK** | input kick at logic-frame start (5ACAAA) instead of end (5AB6CF) | **4–9 ms** (more when render-bound: FrameSync waits also move before the sample) | low | live script built (default off); ASI design §6.1 |
| 2 | **LM/LS** | pace the logic frame at its *start* (sleep before IK), not between update and commit | **3–7 ms** if the commit follows the limiter (to be confirmed with TS `L->W`) | medium: frame-to-frame jitter of the update moves into present timing | design §6.2, after TS data |
| 3 | **VS0** | in-game VSync OFF while gamescope stays tear-free (gamescope composites at vblank; "Allow tearing" OFF) | 0–8 ms (no FIFO queue, render start no longer vblank-quantised); possibly more fps | low-medium: pacing must be checked by eye at 60 on 120 Hz | user setting, A/B |
| 4 | **MFL** | `MaxFrameLatency` 1 vs 2 | 1 = lowest latency per frame; 2 = may be needed to *reach* 60 (+0–1 frame only when the GPU is the bottleneck) | low (API value) | other agent's ASI key; A/B §7 |
| 5 | **D** | 1 ms blind retry quantum in the render main loop (1A744E4 → 0.2 ms) | ~0.4 ms when the render thread waits for the logic commit | low | existing design (mainthread-drawprep.md D) |
| 6 | 120 Hz | keep the panel at 120 Hz (a v4 Ally run shows 60 Hz quantisation) | ~4 ms vblank wait + ~4 ms scanout vs 60 Hz | none | user setting |
| – | P2 | don't wait for the previous Present at render begin | ≤0.3 ms | high | not recommended |
| – | K3→2 | engine frame slots 3 → 2 | 0 while the waitable governs | medium | not recommended |

**IK and LM together: about 7–15 ms off a ~55–65 ms chain (≈ 12–25 %), with identical rendering.**

---

## 1. The pipeline (threads and hand-offs)

```
[App]InputUpdateWorker   poll pads (XInput / DInput / HID) when kicked, set "pad stage done", then mouse/kb
KIDSTaskScheduler0       LOGIC frame (kidstask graph: FrameSync, KeyTrigger, Behavior::Update, Physics, ...,
(logic thread)           ApplicationCallback, Register lights/primitives, CommitToNextRender)
   FrameMgr_FrameBegin 5AC664   G+7D8 = frame start ns
     FrameEarlyUpdate 5AC9EC
       5ACAAA  InputWorkerWaitPadStage (<=1 ms)          <- pad sample CONSUMED (site C)
       564F40 -> 5651D0 -> GameModeUpdateMain 39E4E4 (InputWorkerJoin 5AB7E0, player/character update)
   ... (other nodes)
   FrameTask_Update 5ACEE8
     FrameDtSetup 5AD5F8 -> UpdateDispatchB 5AB644
       5AB6CF  InputWorkerKick 5AB7B0                    <- pad SAMPLED for the NEXT frame (site K)
     FrameEnd 5ACF80: per-scene updates, LIMITER 5AD292..5AD2BB (QPC spin, L1 = timer sleep), clock
   Register* nodes, CommitToNextRender                 (order relative to the limiter: see §2)
[App]Game (render thread) MainLoopFrame 1A74130: FrameStep 3D1F20, 1 ms blind retry when no new frame (D)
   RenderFrameBegin 4A529C
     4A52D4  QpcNow (site W)
     4A5313..4A5357 if view flag: h = swapchain->GetFrameLatencyWaitableObject(); wait(h)   (R1 site)
     4A535E  latency markers / xellSleep (site B)
     7D9B6C  wait previous Present() returned (7D9E90), GetCurrentBackBufferIndex
   record draws (SceneRenderView 535188 ...), dispatcher thread replays to D3D12
   GfxDevicePresent 3D62E4: drain dispatcher, submit, SetEvent -> present thread (site P),
     GfxFrameSlotAdvance 8BEC48: slot = (slot+1)%3, wait GPU fence of that slot (frame N-2)
Present thread 1A82298   swapchain->Present(SyncInterval = VSYNC 0/1, flags)   (SL proxy -> vkd3d / DXGI)
GPU                      work of frame N is submitted in a 2.5–3.2 ms burst at the end of recording (RGP r5)
display                  PC: DWM/flip.  Ally: vkd3d FIFO (VSync ON) -> gamescope -> panel
```

Evidence for the order on the logic thread:
- FrameBegin writes the frame-start time G+7D8.
- The CPU-time monitor 5AF410 runs inside FrameDtSetup and measures `now − G[7D8]` = 8.2–8.8 ms at 60 fps
  (frame-timing.md §4). It does **not** include the limiter wait, while G+7D8 deltas are a paced 16.67 ms.
- So the cycle is FrameBegin → … → DtSetup (kick) → FrameEnd (limiter) → … → next FrameBegin (consume). The limiter
  always lies between the sample and its consumer.

## 2. Logic thread: when input is sampled and when it is used

| step | where | stock | with IK |
|---|---|---|---|
| sample | `InputWorkerKick` 5AB7B0: join previous run, reset pad-stage event 45C39B0, set worker event 45C3818, pending (45BA541) = 1. The worker runs `PadUpdateIfMgr` (XInputGetState etc.) at once | called from **5AB6CF** (end of frame N, inside `if (G+646h)`) | called from the cave at **5ACAAA** (start of frame N+1, same `+646h` gate), then wait up to `cap` ms |
| consume | 5ACAAA `InputWorkerWaitPadStage` (≤1 ms), then `GameModeUpdateMain` joins the whole worker (5AB7E0, KtglEventWait 45C38B8) | the stage finished long ago, so the wait returns at once | waits for the fresh poll (~50–300 µs; Wine adds futex hops) |
| age at consumption | | rest of FrameEnd + limiter slack + post-limiter nodes + FrameSync wait + pre-callback nodes ≈ **T − U_cb** at a cap with headroom (T = 16.67 ms, U_cb = FrameBegin→DtSetup ≈ 8 ms on the PC menu, est. 8–11 ms Ally gameplay) | **poll time only (~0.1–0.3 ms)** |

Button edges: the pad manager derives pressed/released from consecutive polls. IK keeps exactly one poll per logic
frame, so edges behave as in stock. The live script latches the switch once per frame at 5ACAAA, so switching IK on
or off never skips a poll or polls twice. One exception: the very frame IK is switched on polls twice. A press
landing in that frame can read as "held" instead of "pressed", so toggle it while idle.

**Where the limiter sits relative to the hand-off to rendering.** The KTGL frame graph (`0xbd13a52e.kidstask`,
game-data.md §4.2) ends with Register lights/primitives → `CommitToNextRender`. Both the game update and the limiter
are inside the application callbacks, which come before those nodes. So the commit very likely comes **after** the
limiter, and the limiter slack is paid **twice**: once as input age (fixed by IK) and once between the update and
the commit (LM/LS). This is not fully proven statically: Ghidra has no functions for the KTGL task-type methods. The
TS probes settle it. `L->W` (limiter exit → next render begin) of ≲1–2 ms with no waitable wait means the commit
follows the limiter.

When the render side is the bottleneck (GPU or the waitable wait), the logic thread blocks in FrameSync instead of
the limiter. FrameSync is node 1, so it also sits between the stock sample and its use. IK removes that part too.

## 3. Render side, queue depth, presentation

- **Swapchain** (`DxgiCreateSwapChain` 2EE5A38): legacy `IDXGIFactory::CreateSwapChain` through sl.interposer.
  - BufferCount = 3, Flags = config (0x842: ALLOW_MODE_SWITCH | FRAME_LATENCY_WAITABLE_OBJECT | ALLOW_TEARING).
  - QI to IDXGISwapChain3 (GfxDevice+4278h), then `MakeWindowAssociation(hwnd, 3)`.
  - `ResizeBuffers` in `RecreateSwapchainDevice` 2EEA690 keeps BufferCount (GfxDevice+4240h).
- **SyncInterval** = VSYNC config (0/1) via GfxDevice+424Ch (`DisplaySettingsWatcher` 1350210). No SyncInterval 2.
  At 60 fps on 120 Hz the 60 comes from the engine limiter, not from VSync.
- **SetMaximumFrameLatency is never called.** `re\scripts_latency\vscan.fsx 4278 F8,...` finds only
  GetFrameLatencyWaitableObject (vtbl+108h at 4A5338) and GetCurrentBackBufferIndex (vtbl+120h) on the swapchain
  pointer.
  - DXGI: waitable max latency defaults to 1.
  - vkd3d-proton (`libs/vkd3d/swapchain.c`, master, read 2026-10-07): `frame_latency = 1` for waitable swapchains,
    3 otherwise. SetMaximumFrameLatency only ever **adds** permits. The app-visible semaphore and the internal one
    are both released in the wait worker **after `vkWaitForPresentKHR`** returns, i.e. when the image was presented.
    `VKD3D_SWAPCHAIN_LATENCY_FRAMES` (default 3) sets only the internal throttle in Present.
    SyncInterval 0 → IMMEDIATE (fallback MAILBOX/FIFO). ≥1 → FIFO.
- **The waitable wait** is gated by a per-view byte (`[rsp+2564h]`, filled by 247708). The PC profiles show it
  active: ~32 % of [App]Game samples in this poll at the shrine before R1.
  - R1 turns the poll into `WaitForSingleObject(h, 2 ms)` in a loop. The exit condition is the same, so R1 is
    latency-neutral.
  - **Position:** after the logic frame has consumed input and committed. So it is not a Reflex-style
    "sleep before input". It holds back the render thread only; the logic result waits meanwhile.
- **Ally consequence (vkd3d semantics):** render(N+1) cannot start until frame N is presented.
  - Cycle ≥ recording + GPU (minus the ~3 ms end-of-recording overlap) + wait for the next vblank.
  - At 120 Hz that quantises to 16.7 / 25 / 33 ms.
  - The Ally MangoHud logs fit this: v2 peaks at 16 / 22–24 / 32 ms; v4 (a 60 Hz session) peaks at 16.7 / 33 / 50 ms
    with GPU load only 81 %.
  - It is not proof: FIFO back-pressure alone also snaps to vblanks. The MFL A/B (§7) decides.
- **Engine frames in flight:** `GfxFrameSlotAdvance` 8BEC48 → `GfxWaitFenceValue` 7D9E1C. After Present(N) the
  render thread waits for the GPU fence of slot (N+1)%3, i.e. frame N-2 complete. That allows 2 frames of GPU queue.
  The latency-1 waitable is stricter.
- **Present → display (PC, measured, PresentMon medians):**

| capture | mode | MsBetweenPresents | MsRenderPresentLatency (GPU done after Present) | **MsUntilDisplayed** |
|---|---|---|---|---|
| bench-final4-default-nofg60 | 60 cap, VSync ON (SI 1), Composed: Flip, 240 Hz | 16.65 | 8.36 | **25.76** |
| bench-fix-stock1/2 | ~63 fps, VSync ON, composed | 15.7 | 4.6–5.1 | **21.4–21.7** |
| bench-cap60-stock, bench-G/H/I-60cap | 60 cap, SI 0, Independent Flip (VRR) | 16.64 | 6.0–9.6 | **6.2–9.7** (≈ GPU done) |
| bench-final5 / dyn-stock | ~110 fps, VSync ON, composed | 9.0 | 4.7 | 14.3–14.6 |

  With VSync ON (composed flip) every frame waits about **one more frame interval after the GPU has finished**.
  VSync OFF + VRR (independent flip) shows it at GPU completion. **On the PC that single setting is worth ~16 ms at
  60 fps.** Inside the VRR range there is no tearing, so it is the same look.

## 4. Latency budget at a 60 cap, VSync ON (one frame, button → mid-screen photons)

T = 16.67 ms; v = refresh period (Ally 120 Hz: 8.33 ms). "est." = derived from the structure plus PC profiles,
not measured on the Ally. Ranges, typical value in brackets.

| # | stage | thread / where | PC 7600X + 9070, 240 Hz, VSync ON | Ally (est.) stock | Ally IK | Ally IK + LM |
|---|---|---|---|---|---|---|
| 0 | button → XInput state (USB/HID; Ally: Steam Input / InputPlumber virtual pad → Wine xinput HID thread) | platform | 1–2 | 1–4 (2.5) | same | same |
| 1 | wait for the next poll (once per logic frame, uniform 0..T) | input worker | 0–16.7 (8.3) | 0–16.7 (8.3) | same | same |
| 2 | poll → consumed by the update | logic thread | T − U_cb ≈ 7–10 (8) | 5–9 (7) | **0.1–0.3** | 0.1–0.3 |
| 3 | update → commit to render (update, scene steps, limiter remainder, register nodes) | logic thread | ≈ T − pre-callback nodes ≈ 12–16 (14) | 13–16 (15) | 13–16 (15) | **U + R ≈ 9–13 (11)** |
| 4 | commit → render begin (1 ms retry quantum D; waitable wait; previous-Present wait) | render thread | 0.5–3 (1) | 0.5–8 (3) | same | same |
| 5 | render recording until the GPU starts (lists go out in a burst over the last ~3 ms) | render + dispatcher | 5–7 (6) | 8–11 (9) | same | same |
| 6 | GPU work of the frame | GPU | 9–11 (10) | 11–14 (12) | same | same |
| 7 | GPU done → on screen (PC: measured MsUntilDisplayed − MsRenderPresentLatency; Ally: next 120 Hz vblank + gamescope latch) | display path | **~17** (VSync ON composed) / ~0.2 (VSync OFF + VRR) | 0–8.3 + ~1 (5) | same | same |
| 8 | scanout to mid-screen (v/2) | panel | 2.1 | 4.2 (8.3 at 60 Hz) | same | same |
| | **total, button → photons** | | **~67** (~51 with VSync OFF + VRR) | **~66** | **~59** | **~55** |

Notes:
- Row 1 is the cost of a 60 Hz simulation and can't be removed without running the logic faster.
- Rows 2–3 are the engine-side waste: the limiter slack, paid twice.
- Rows 4–7 are the swap-chain path.
- The Ally column assumes a held 60. If the Ally is stuck at 30–40 fps, every row from 3 on scales with the frame
  interval: ~85–100 ms total. That is why getting to a real 60 (MFL, settings) matters more than any single patch.

## 5. Interactions

- **VRR (variable-dt) patch:**
  - At a held 60 it is exactly stock: the measured fps equals the tier, scale is 1.0, the limiter runs at the tier.
  - Below the cap there is no limiter slack, so IK only removes the FrameSync part and LM/LS has nothing to remove.
  - Neutral for latency. It also keeps game speed honest under the vblank-quantised frame times above.
- **VSync at 60 on a 120 Hz panel:**
  - Each frame is held for 2 refreshes and a present waits at most one 8.33 ms refresh (16.7 at 60 Hz).
  - The engine limiter (QPC) and the panel clock drift against each other. Under FIFO a frame finishing near a vblank
    alternates between 1 and 3 refresh holds. That is judder, and up to 8 ms of latency jitter.
  - With the latency-1 waitable on vkd3d the render start is phase-locked to presentation, which avoids the drift.
    So MFL 1 also helps pacing when it can hold 60.
- **R1 / L1:** timer/timeout versions of the stock waits with the same exit conditions. Latency-neutral, apart from
  L1 timer jitter, which the 1 ms margin covers. L1 is off under Wine by default.
- **W6 (ASI 1.1.0):** spin + WaitOnAddress with a waker at 1A822D9, so latency-neutral (TL;DR 5). Under Wine,
  WaitOnAddress/WakeByAddress are futex-backed. Check the `W6 average wait` stat against stock.
- **MaxFrameLatency (ASI 1.1.0):** see the vkd3d caveat in TL;DR 3. Under vkd3d, lowering it live doesn't take
  effect until the swapchain is rebuilt.
- **Co-op:**
  - IK changes only *when* the local pad is read. One read per frame and the network messages are unchanged, so
    it is safe online.
  - LM/LS changes only when the logic frame starts. The step size is unchanged, so it is safe too. But keep it off
    in co-op until tested, like VRR.

## 6. Patch designs for the ASI

### 6.1 IK (recommended, low risk)

Sites. Patterns derived from the dump by `re\scripts_latency\derive_lat_patterns.fsx` (same algorithm as the mod's
derive tool, unique in the executable sections):

```json
{ "id": "IK",  "group": "IK", "rva": "0x5AB6CF", "len": 5, "orig": "E8 DC 00 00 00", "kind": "call", "target": "hookInputKickEnd", "call_at": 0, "reads": "callee_rel32@1",
  "pattern": "48 8B CF E8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 8B 0D ?? ?? ?? ?? 48 8B D7 E8 ?? ?? ?? ??", "offset": 8 },
{ "id": "IKc", "group": "IK", "rva": "0x5ACAAA", "len": 5, "orig": "E8 69 F5 2B 00", "kind": "call", "target": "hookPadStageWait", "call_at": 0, "reads": "callee_rel32@1",
  "pattern": "48 8B 13 48 8B CF E8 ?? ?? ?? ?? E8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ??", "offset": 11 }
```

The mod also needs `InputWorkerKick` = target of the IK rel32 (5AB7B0) and `KtglEventWaitFor` 86C040. 86C040 can be
read from the rel32 at 86C018+0x1E, or found by pattern. It also needs the pad-stage event 45C39B0, which is a RIP
displacement at 86C018+0x17 (`lea rcx,[rel 45C39B0h]`).

```cpp
// both hooks run on KIDSTaskScheduler0 (the logic thread); no locking needed
static uint8_t g_ikOn = 0, g_ikLatched = 0; static uint64_t g_ikCapMs = 2;
extern "C" void hookInputKickEnd() {                // was: call InputWorkerKick (end of logic frame)
    if (!g_ikLatched) origInputKick();              // IK active this frame -> the kick already happened at frame start
}
extern "C" void hookPadStageWait() {                // was: call InputWorkerWaitPadStage (start of logic frame)
    uint8_t on = g_ikOn, use = on | g_ikLatched;    // switching off still does one fresh poll here
    g_ikLatched = on;
    if (!use) { origWaitPadStage(); return; }
    origInputKick();                                // join previous run (already joined) + reset + kick
    uint64_t d = g_ikCapMs;
    if (!KtglEventWaitFor(padStageEvent, &d)) ++g_ikTimeouts;   // al = 0 on timeout; game proceeds as stock would
}
```

- Ini: `[Latency] InputLateKick = 1`, `InputPadWaitCapMs = 2`.
- Menu: live switch. Co-op: safe.
- Cost: the poll moves onto the logic thread's critical path (~0.1–0.3 ms, more under Wine contention). Irrelevant
  at a cap with slack.
- Watch: the `IK wait timeouts` counter (should be ~0), menu key repeat, hot-plug (unchanged: the 2 s probe is inside
  the poll).

### 6.2 LM / LS (after TS data; medium risk)

Goal: pace the logic frame at its start, so update → commit runs back to back and the input is sampled right after
the pacing wait.

- **LM (simple):** when armed, the existing VRR `caveLim` (P4, 5AD860) passes 1000 to SetLimiterFps, so the stock
  limiter no longer waits. `hookPadStageWait` first waits until `lastStart + 1/cap` (high-resolution timer to −1 ms,
  then spin, as in L1; under Wine `Sleep` to −1.5 ms then spin), then does IK.
  - The VRR frame-start deltas (G+7D8) stay paced. The dt is unchanged at a held cap.
  - Risk: update-time jitter (ΔU, typically ≤1–2 ms) now reaches the commit and the present. Near a vblank boundary
    under FIFO that can turn into an occasional 1/3-refresh hold.
- **LS (adaptive, Reflex-like):** wait instead until `predictedRenderReady − EMA(C→W) − margin(1.5 ms)`.
  - Uses the TS stamps: B (render ready after the waitable) and C→W per frame.
  - This also removes the commit-waits-for-render gap when the render thread/GPU is the bottleneck (FrameSync case).
  - Clamp the wait to [0, T/2]. Keep the stock limiter as the hard cap.
- Gate both on TS evidence: `L->W` small (commit right after the limiter) and `C->L` ≫ `U`.

### 6.3 TS (timestamp chain, measurement only)

Same six sites as `latency_live.fsx` (patterns printed by `derive_lat_patterns.fsx`). **TSb: use the R1-independent
pattern** `85 C0 75 ?? 48 8D 54 24 40 E8 ?? ?? ?? ?? E8 ?? ?? ?? ?? E8 ?? ?? ?? ?? E8 ?? ?? ?? ??` (offset 9, unique).
The derived one contains R1j's `75 EF`.

Each probe appends `{QPC, id}` to a lock-free ring. Once a second, log medians of K→C, C→L, L→W, W→B, B→P and P→P.
On vkd3d add **B(n+1) − K(n)**: with latency 1, B is released by `vkWaitForPresentKHR`, so B(n+1) ≈ the moment
frame n was presented. Then sample → presented − scanout is read **without any hardware**; add v/2 for mid-screen.

## 7. How to measure on the Ally (no special hardware)

1. **Phone slow-motion (240 fps, 4.17 ms per video frame).**
   - Ally on a table, phone filming the screen and the right hand.
   - Use an action with a one-frame visual response:
     - the cursor step in the options list (d-pad down);
     - in gameplay, the first frame of camera motion on a right-stick flick.
   - Count video frames from the button visibly bottoming out (or the stick starting to move) to the first changed
     pixel row near the screen middle.
   - 20–30 presses per setting, report the median. Expect ~55–70 ms stock; IK should move the median by ~5–8 ms.
     That is 1–2 video frames, so it needs about 20+ samples to see.
2. **In-engine chain (ASI TS, §6.3).** It gives K→C (IK effect), the logic → render hand-off, the waitable wait, and
   on vkd3d the frame-to-present time per frame. MangoHud's frametime log gives the present cadence on the side.
3. **A/B matrix** (same spot, 60 s each, held 60 if possible). Record fps, GPU load, TS medians and the phone median.

| run | MaxFrameLatency | VKD3D_SWAPCHAIN_LATENCY_FRAMES | in-game VSync | IK | expectation |
|---|---|---|---|---|---|
| A | (stock = 1) | 3 | ON | off | baseline |
| B | 1 | **1** | ON | off | ≈ A (prediction: app waitable already 1) |
| C | **2** | 3 | ON | off | fps ↑, GPU load → ~99 % if the latency-1 serialisation was capping fps; per-frame latency +0..1 frame when GPU-bound |
| D | best of A/C | 3 | **OFF** (gamescope tearing OFF) | off | present → display shorter; check pacing by eye |
| E | best | best | best | **on** | K→C drops to ~0.2 ms; phone median −5…−8 ms |

4. **PC** (`latency_live.fsx`, game running, in a mission):

```
dotnet fsi patcher\latency_live.fsx offline          :: anytime: site bytes vs dump, cave disassembly
dotnet fsi patcher\latency_live.fsx dry              :: game running: live bytes
set FT_ALLOW_WRITE=1
dotnet fsi patcher\latency_live.fsx apply            :: cave + 6 sites, all switches OFF
dotnet fsi patcher\latency_live.fsx on TS
dotnet fsi patcher\latency_live.fsx stats 20         :: stock budget
dotnet fsi patcher\latency_live.fsx on IK
dotnet fsi patcher\latency_live.fsx stats 20         :: IK budget (K->C should collapse)
dotnet fsi patcher\latency_live.fsx timeline 80      :: event order: confirms K..C..L..W..B..P per frame
dotnet fsi patcher\latency_live.fsx revert
```

   Plus PresentMon for Present → displayed (`MsUntilDisplayed`). Offline check: `OFFLINE: ALL PASSED`
   (6 sites match the dump, call targets verified, 531-byte cave). No branch in .text lands inside any of the
   6 sites (Iced sweep). No overlap with patches.json or the other live scripts.

## 8. Open items

- Prove the logic graph order (FrameSync / ApplicationCallback / CommitToNextRender relative to the limiter) from TS
  data. The KTGL task-type methods (descriptor vtables 3B285D0 FrameSync, 3B2DBA0 CommitToNextRender, 3B28750
  ApplicationCallback with params "Timing"/"ViewObjectNameHash") have no Ghidra functions.
- Is the waitable-wait view flag (`[rsp+2564h]` via 247708) also set on the Ally? The TS `W->B` time shows it.
- vkd3d-proton anti-lag (`VK_AMD_anti_lag`) exists in vkd3d but needs an app-side API. The game's Reflex path goes
  through Streamline/NVAPI, which is not available on AMD/Proton without extra layers. Not pursued.
- Steam Input / InputPlumber poll rate on Bazzite (row 0) was not measured.

## 9. Files

- `patcher\latency_live.fsx`: IK + TS live patch (offline/dry/apply/on/off/cap/status/stats/timeline/revert).
- `re\scripts_latency\vscan.fsx`: find vtable calls on an object loaded from a field (`vscan.fsx 4278 F8,108`).
- `re\scripts_latency\peek.fsx`: dump qwords of the dump as RVAs.
- `re\scripts_latency\derive_lat_patterns.fsx`: AOB patterns for the IK/TS sites (prints patches.json rows).
- `re\scripts_latency\branch_into_sites.fsx`: proves no branch lands inside the 6 patch sites.
- `re\symbols\incoming\input-latency.tsv`: 15 rows.
