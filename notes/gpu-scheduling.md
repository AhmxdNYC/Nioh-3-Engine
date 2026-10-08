# Nioh 3: GPU scheduling - async compute and submission timing

Status 2026-10-07. Static analysis and trace work only:
- the v2.02 dump `re\Nioh3.dump.exe` (Ghidra RO project, Iced scans);
- the installed `Nioh3.exe`, read-only, for the shipped .data table;
- the r5 RGP trace `captures\rgp\Nioh3_2026-10-06_19-39-58.rgp`: RX 9070, 1440p output, 1080p render, HIGH settings,
  FSR4, GPU-bound, about 10.7 ms of GPU per frame. It has SQTT for one frame plus queue events for 6 frames.

The game was not launched. Nothing was applied, and `mod\Nioh3EngineFix\src` was not touched.

Large outputs are in `D:\nioh3-re\gpusched\` (Ghidra queries `q1..q16.txt`, `*.sched_*.tsv`).

## TL;DR

| | finding | number |
|---|---|---|
| A1 | **Queues the engine creates:** one DIRECT queue (`2EE9DE4`, the GfxQueue at device +0x4270), COPY queues (`2EE9EE8` with 8 lists, `2F110A8`, `2F13350`), and **one COMPUTE queue per enabled "async slot"** (`2EF8448`) | RGP sees 3 DIRECT, 3 COMPUTE and 34 DMA queues; only GFX0, CMP35/36/37 and DMA1-3 carry work |
| A2 | **Work that already runs async:** VFSCAT volumetric fog (293 µs), ocean PFB + IGW (182 µs, 10 dispatches) and VCLOUD (one 30x17 dispatch, under 1 µs). All of it overlaps GFX; none of it fills GFX gaps | 0.49 ms of async compute vs **2.72 ms of compute still on GFX** (165 dispatch-only segments) |
| A3 | **KTGL has a full async-compute slot system** (13 slots; table at .data `0x44C04D0`). **Slots 0, 3, 4 and 6 ship enabled. The renderer switches slot 0 OFF on every non-NVIDIA GPU** (`226A490`). Slot 0 is the effect path (`KIDS_ASync_LLCulling_Effect`). The 9 other slots ship disabled for every vendor | PC dump at runtime: slots 3, 4, 6 live |
| A4 | **Movable compute with idle CUs:** (a) effect GPU chains on GFX (segments 70-80: 1x1x1 / 3x1x1 dispatches at 0-13 % occupancy, **0.32 ms**); (b) RefProbe GI gathers #121/#414 (**0.65 ms** at HIGH, about 2.5 ms at GI HIGHEST, 10-36 % occupancy) | realistic **PC -0.5…0.7 ms (5-6 %) at HIGH**, up to about -1 ms at GI HIGHEST; **Ally 0-4 %** (power-limited) |
| A5 | **Cost:** (a) is probably a **1-byte, engine-native experiment** (enable slot 0 on AMD, `patcher\async_slots.fsx`, boot race, default off). (b) is **engine surgery**, unless RefProbe sits behind one of the disabled slots | (a) hours, risk unknown; (b) weeks |
| B1 | **K1 (`CmdFlushLimit` +0x3C70) does not create ExecuteCommandLists.** All 31 K1 compare sites hand off with `submit = 0`. D3D12 lists are closed only at fence points (KTGL token 1), at 4 explicit submit sites, at ring-exhaustion splits and at frame end | 21-23 GFX lists per frame, each in its own ECL |
| B2 | **GPU idle per frame (r5, 6 frames):** 0.32 ms waiting for the CPU, and **1.35 ms with work already queued** ("gated"). About 0.17 ms of the gated time is per-list launch gaps (~8 µs each); the rest is 0.4-2.2 ms stalls in which **GFX, compute and DMA queues all start at the same instant** | the 1.07 ms of "CB-boundary idle" is mostly *not* submission timing |
| B3 | **"Submit earlier / more often" is worth ≤ 0.1 ms per frame.** Model: the same frames with every list's work arriving continuously while it is recorded, over GPU-load scales 0.3-1.0. Each extra ECL costs ~8 µs of GPU and ECL-thread CPU | not worth a code patch |
| B4 | **Built:** `patcher\k1_live.fsx` reads the engine's own per-frame stream stats (draws, kicks, frame-end drain spin, ring-full wait) and does a live K1 data write with A/B. Offline check **ALL PASSED** | CPU-side lever: drain spin 0.7-2.1 % of main, dispatcher wakes (Ally watts) |

**Bottom line**
1. **The cheapest real async gain** is to switch the vendor-disabled effect slot back on. The engine already has that path, and NVIDIA gets it.
2. **The biggest scheduling loss on the PC trace** is the ~1 ms/frame device-wide GPU stall with work already queued. It is not caused by submission timing, and its cause is still to be proven (§B.3).
3. **On the Ally** the submission-timing lever is the latency-1 waitable. That is MaxFrameLatency (ASI 1.1.0, another agent), not K1.

---

## A. Async compute

### A.1 Queues

The engine's queue creators are listed below. The RGP `QueueInfo` chunk gives the queue type (1 = universal, 2 = compute, 3 = DMA).

| creator | type | what |
|---|---|---|
| `2EE9DE4` (from `2EE9424` ← device init `2EE300C`) | DIRECT | allocator, fence, list. This is the graphics queue (device +0x4270), used by the ECL thread (`4F90DC`: ExecuteCommandLists, then Signal) |
| `2EE9EE8` | COPY | 8 copy lists |
| `2F110A8`, `2F13350` (from `2EF053C`) | COPY | upload / streaming copy contexts. `2F110A8` falls back to the GFX queue if creation fails |
| **`2EF8448`** (← `2EF8354` ← `2EE256C` ← `32FA450`) | **COMPUTE** | one per enabled async slot. Each has its own N compute allocators, M compute lists, an event, a compute root signature and CBV/SRV/UAV/sampler heaps. An optional own dispatcher backend (vtable `409A3B0`) is used when cfg +0x3D is set |
| `37761E4` | DIRECT/COMPUTE | the engine's FSR3 frame generation (AsyncComputeQueue, PresentQueue). Not used with FG off |

The RGP r5 trace shows 3 DIRECT, 3 COMPUTE and 34 DMA queues. Work runs on:
- **GFX0**: 21-23 lists per frame;
- **CMP35 / CMP36 / CMP37**: one list per frame each;
- **DMA1-3**: 1-3 lists per frame, 50-1500 µs each (streaming).

The other DMA and DIRECT queues are idle in this capture: overlay, Streamline and OptiScaler queues, plus copy contexts that have nothing to do.

### A.2 The async slot system (engine-native async compute)

**Renderer init.** `RenderAsyncSlotsInit 2269EC4` runs once when the renderer is created.
- It reads `g_AsyncSlotTable` at .data `0x44C04D0`: 13 × {u8 enable, f32 param, u32 slot id}.
- On disk, the enabled slots are **0, 3, 4 and 6**. Slot 6 has param 0.5.
- It then applies vendor rules:
  - `226A490  cmp dword [rbp+70h],10DEh ; je 226A4AD`: on every **non-NVIDIA** GPU, the entry whose slot id is 0 is disabled (`226A499` loop).
  - `226A571`: NVIDIA with a driver ≤ 0x1C92 gets all slots disabled.
- It builds the mask (id == index && enable) and copies the table to `g_AsyncSlotTableLive 0x45C0460`. That is BSS: zero until init.
- It calls `AsyncSlotsCreate 32FA450(sched = mgr+0x1130, …, &mask)`.
- **The PC dump's live copy shows slots 3, 4 and 6.** That matches the three COMPUTE queues in RGP.

**Context creation.** `AsyncSlotsCreate` creates one async context per mask bit (`AsyncComputeCtxCreate 2EE256C` → device +0x74D8[i]) and stores the mask at sched +0x1CC. For slot 0 it also starts the KIDS job `KIDS_ASync_LLCulling_Effect` (callback `3D16AC`).

**Per-subsystem pattern.** Seen in `418284` (slot 12), `B06EA0` (Fluid2DGrid), `B1BEC0` (CSPH2D):

```
ctx = AsyncSlotGetCtx(sched, SLOT)            ; 52BE1C: NULL unless the mask bit is set
if (ctx == NULL) {                            ; synchronous path, on the graphics context
    tok 0x7C (compute section) … tok 0x7E Dispatch … tok 0x7D (UAV barrier, end)
} else {                                      ; async path
    ctx->Begin (backend vf+10h) … Dispatch (vf+D0h) … End (vf+38h)
    KtglCtxGetFence(ctx, &f, 1)               ; 3D1BA4: f = ctxIndex<<56 | value
    AsyncSlotAddFence(sched, key, SLOT, f)    ; 148514: the graphics side waits on it
}
```

**Cross-queue sync** already exists end to end:
- the KTGL fence token 1 (dispatcher handler `8F0B5C`, a list boundary);
- tokens 6 and 7 for waits in the context streams;
- ECL tokens 7 Signal (`4F91C0`) and 8 Wait (`4F93C8`, written by `48F3B4`), replayed by the ExecuteCommandListsThread.

**Slot users.** These are the `AsyncSlotGetCtx` call sites with a constant slot, from `re\scripts_d3d12\slotargs.fsx`:

| slot | shipped (AMD) | users |
|---|---|---|
| 0 | **on in the table, forced off on non-NVIDIA** | `3D0AE0`, `3D16AC` (the KIDS_ASync_LLCulling_Effect job), `3D3350`, `544634`, `194E70E`, `19CFAFC`, `32FA505` |
| 3, 4, 6 | **on** (the three compute queues: VCLOUD, VFSCAT, ocean) | `5F2A88` (3), `14871C` (4), `1A2550E` (6) |
| 5 | off | `3658D34` |
| 7 | off | `52D4C4` |
| 8 | off | `419F54`, `19CB677` |
| 9 | off | `10D19C`, `1A408F4`, `32FA4CC` |
| 10 | off | 10 sites (`52853`, `104B54`, `1063E6`, `193DC34`, `1947180`, `1A41879`, `347AF60`, `3675307`, `52B727`, `32FA4EA`) |
| 11 | off | `52AF94` |
| 12 | off | `418284`: per-item dispatches plus one 3D dispatch (x/4, y/4, z/4) |

**Which slot hosts which pass** (other than 3/4/6) is not proven statically. The game has no GPU markers; only "GPUCulling" (`18D9CEC`) and the fluid markers exist. Slot 0 = effects is the strongest lead: the job name, the `LLCulling_Effect` string, and the effect-like 1x1x1 chains on GFX.

### A.3 The r5 frame, by pass

From `re\scripts_gpu\sched_overlap.py`:
- Wave-slot capacity C = 1790 waves (p99.5 of 5 µs bins; the RX 9070 maximum is about 1792).
- Average occupancy is 47 %.
- Inside GFX segments there are 4.35 ms-equivalents of free wave slots.

| GFX list (µs) | span | occupancy | content (shader group from the DXBC digest) | dependency |
|---|---|---|---|---|
| fab18 | 867-971 | 38 % | MCLRC culling, clears | previous-frame Hi-Z |
| fab1e | 1052-1777 | **19 %** | GVD / TREE2WIND / MshLod culling (dispatch → EI pairs), **segments 70-80: effect chains, 0-13 %, 0.32 ms** (168/190/190/74/75 one-to-three-group dispatches + an 87.5 µs CP-DMA copy), CLC, SM, MIC/MPC | culling feeds the very next draws; the effect chains feed G1E draws ~7 ms later |
| fab20 | 1790-2118 | 44 % | SM/SMTREE2 shadow depth, GHDM, MPC | – |
| fab22 | 2128-4059 | 50 % | **G-buffer** (PB2 / PB2TREE2 / PB2LS terrain) | – |
| fab24 | 4068-4956 | 67 % | WDO_SST shadow resolve (PS, 2048²), shadow conversion | shadow maps |
| fab26 | 4974-6241 | 54 % | CTS 80 + WET 48 (CS, 80-86 %), ESM 239+57 (PS), **RefProbe #121 35+302, #414 57+260 (CS, 10-36 %)** | G-buffer, screen shadow, probes |
| fab28 | 6495-7022 | 46 % | PB2DS local lights 210 (PS, cube shadows) ‖ **VFSCAT on CMP36** | – |
| fab2c | 7120-7625 | **18 %** | PB2UMAH 186, UMTV 71, VFS/probe apply | consumes VFSCAT and RefProbe |
| fab32 | 8029-9870 | 66 % | post, G1E effects (draws), **FSR4** (OptiScaler CS wave32) | everything before |
| fab34+ | 9888-10716 | 60-76 % | output post, copy, UI | – |

There are 165 compute-only segments on GFX (2.72 ms). Most of them are not movable:
- culling feeds indirect draws immediately;
- CTS/WET already run at 80-86 % occupancy;
- the post and FSR4 chains are serial;
- FSR4 belongs to OptiScaler, not the engine.

The movable ones are effects (A4a) and RefProbe (A4b).

### A.4 Overlap estimate

Model (`sched_overlap.py`): `packed = max(dur_W, (occ_W·dur_W + occ_X·dur_X)/C)`, upper saving = `dur_X + dur_W − packed`, realistic = upper × 0.5 for cache and bandwidth contention. Latency-bound chains get ×0.8, because they need almost no slots.

| candidate X | X (PC r5) | host window W (inputs ready → consumer) | upper | **realistic PC** | Ally at MAX (model) |
|---|---|---|---|---|---|
| **a. effect chains** (fab1e segments 70-80) | 320 µs at ~4 % | fab20-fab28 (shadow depth, G-buffer, shadows; ~4.5 ms at ~50 %) | 320 µs | **0.25-0.3 ms** | latency-bound: scales with clock, not CUs → ~0.7-1 ms → 1-1.5 % of 60 ms |
| **b. RefProbe #121/#414 at HIGH** | 655 µs at ~33 % | rest of fab26 (CTS/WET/ESM, ~610 µs) + fab28 (527 µs) ≈ 1.14 ms at ~58 %, minus VFSCAT's share (~144 slot-µs) → ~335 slot-µs free; X needs 216 | 655 µs | **0.25-0.4 ms** | – |
| b. RefProbe at **GI HIGHEST** | ~2.5 ms | same window, slot-limited: ≤ 335/0.33 ≈ 1.0 ms upper | ~1.0 ms | **0.4-0.6 ms** | 15-20 ms of RefProbe; hosts (ESM/WDO/PB2DS/CTS ×6-8 ≈ 7-9 ms) are bandwidth-bound on LPDDR5, efficiency ~0.3 → **1-3 ms (2-5 %)**, minus the power effect below |

**Total PC:** −0.5…0.7 ms at HIGH, i.e. 5-6 % of a GPU-bound 10.7 ms frame. At GI HIGHEST, up to about −1 ms.

**Ally:**
- The Ally is power-limited (25 W TDP; GPU at 1.0-1.1 GHz in MAX runs).
- Overlap raises average GPU power, so the clock falls and part of the gain is lost.
- Honest range **0-4 %**. That is far below the 3.5-4.5× gap in gpu-maxsettings.md §0.

**RADV caveats:**
- vkd3d-proton maps D3D12 compute queues onto the Vulkan compute queue family. Check that a 4th D3D12 compute queue does not serialise behind the 3 that already exist (RADV queue count).
- Async compute on RDNA3 APUs works on RADV (ACE rings).

### A.5 Designs and grades

| id | design | effort | grade |
|---|---|---|---|
| **AS0** | **Enable slot 0 on AMD:** `226A497 74 14 → EB 14` (skip the non-NVIDIA rule), written before renderer init (boot race). `patcher\async_slots.fsx arm 0` | done, default off | **easy to try; risk unknown.** KT disabled it on purpose for non-NVIDIA (driver bug, hang, or slower: unknown). Same shaders. Effects might become one frame late if the consumer reads the previous fence (must be checked in A/B) |
| AS-N | Enable slots 1, 2, 5, 7-12 (table enable byte, same boot race; `arm N`) | done, research only | **unknown subsystems, never shipped enabled on PC.** One slot per test session, RGP each time to see what moved |
| RP1 | RefProbe → async slot, if the RefProbe code is one of the slot-aware users (identify by toggling AS-N with RGP: #121/#414 move to a compute queue) | 0 code once found | depends on AS-N |
| RP2 | RefProbe → async **without** a slot-aware path. A cave in the RefProbe record function (`ReflectionProbePass` render-graph node) gets an async context (a free slot id + table enable at boot), re-records the 4 dispatches there, publishes the fence with `AsyncSlotAddFence`, and makes the consumer (fab2c apply) wait. Needs: the compute root signature of the async context to match the PSOs (layout from `2EF8448`: 14 CBV / UAV / SRV tables, unverified); UAV→SRV state transitions that are legal on a compute queue; and every binding the pass inherits from the graphics context | weeks: RE + a GPU-validated harness | **hard** |
| D2 | Generic dispatcher-level split: replay `tok 0x7C … 0x7D` compute sections into a second list on a compute queue, with Signal/Wait. Needs resource hazard tracking across queues (RGP has no resource identity; gpuopt trace mode could give it), per-queue state legality, and a GFX wait before the first consumer | months | **very hard, not recommended**. The engine already has the right abstraction (slots) |

---

## B. Submission timing

### B.1 How lists reach the GPU

The pipeline is: main thread → stream → dispatcher → list → ECL thread → queue.

1. **Main thread.** It records KTGL tokens at `KtglContext+0x3C40`. Every draw path does `inc +3C74; cmp +3C74, +3C70 (CmdFlushLimit); jnc kick`; there are 31 `cmp r32,[reg+3C70h]` sites.
2. **The kick** is `KtglCmdStreamFlush 254110(ctx, submit)`.
   - It pushes {ctx, begin, end, submit} into the dispatcher ring: items at +3BF8, capacity +3C00, write/read at +3C04/+3C08, in flight +3C10.
   - It calls `ReleaseSemaphore(+3BF0)` and does ++kicks (+3C7C).
   - **If the ring is full, the main thread spins on Sleep(0).** That wait is timed into stats +3C88.
   - **Every K1 kick passes `submit = 0`** (29 sites, `xor edx,edx`; e.g. `1D3186`).
   - `submit = 1` is used only at 4 sites:
     - `253EE0` in `UiViewRender 253C68` ("OffScreenRendering");
     - `3D2CDF` in `KtglWaitQueryOrKick 3D2C64` (a query or fence not ready);
     - `6D7FD9`;
     - `1A1786F`.
   - The frame-end kick (`CmdStreamFinalKick 418C70`) also submits.
3. **The dispatcher** (`1A7C3E0`) replays the tokens into one open list.
   - It closes and ECLs the list only on a submit item (`SynCloseCmdList 496078` → `CmdListRingSubmitUpTo 4930C8`).
   - It also closes at KTGL **fence points** (token 1, `8F0B5C`: list ring +0x5A0). Every `KtglCtxGetFence` on the graphics context is one.
   - It also splits when descriptor or upload rings run out (`945664`). That is a mid-pass split the engine already handles: lists start with ClearState, and resource state is reset to COMMON per list stamp.
4. **The ExecuteCommandListsThread** does one ExecuteCommandLists per list, plus the Signal/Wait tokens.

**Result: 21-23 GFX lists per frame.** They are cut at fence/sync points (the slot fences, query waits, UI offscreen), not at K1.

**K1 only sets how soon the dispatcher starts recording what the main thread wrote.** That in turn sets:
- how long the main thread spins in `418F40` at frame end (0.7-2.1 % of main in the CPU profiles);
- how many dispatcher wakes there are (CPU power on the Ally);
- how far behind an explicit submit can leave.

**The value** is copied once at init (`5D7023`: `+804h → +3C70h`) and only read afterwards, so a live data write is safe. It is a plain aligned dword, compared with `>=`. The engine snapshots its own per-frame stats every frame (`CmdStreamEndFrame 418B94` → +3D00):
- +3D00 draws;
- +3D04 kicks;
- +3D10 ring-full wait {total, max, n};
- +3D28 frame-end drain-spin timer.

### B.2 What the queue events say

`re\scripts_d3d12\submit_model.py`, r5, 6 frames, GFX0:

| frame | lists | recording span | CPU-late idle | gated idle (work queued) | holes > 300 µs between submits |
|---|---|---|---|---|---|
| 23124 | 22 | 3.19 ms | 0 | 435 µs | 3 (1.7 ms) |
| 23125 | 23 | 2.96 | 743 | 534 | 4 (1.9 ms) |
| 23126 | 21 | 3.19 | 0 | 565 | 1 (1.7 ms) |
| 23127 | 23 | 3.94 | 559 | **1781** (first list started 1.4 ms after its submit on an idle GPU) | 4 (2.6 ms) |
| 23128 | 21 | 4.29 | 0 | **1233** (first list queued 1.77 ms ahead; GPU idle 862 µs) | 4 (3.0 ms) |
| 23129 | 21 | 3.35 | 285 | **2611** (3rd list waited 2.25 ms) | 2 (1.9 ms) |
| **mean** | 22 | 3.5 | **317 µs** | **1345 µs** | |

- **The submission burst is real, but it is the recording itself.** Game logic runs first on the same thread. The render recording takes the last 3-4.3 ms of the CPU frame, and the engine already submits about every 200 µs inside that window.
- **The ~8 µs launch gap per list** (SQTT: 21 CB boundaries ≈ 0.17 ms) is the price of every extra ECL.
- **The gated stalls are device-wide.** In 23127, 23128 and 23129, the GFX list, the async compute lists (CMP35/37) and the DMA copy lists all start within 10-50 µs of each other, after the GPU sat idle with all of them submitted. That rules out a GFX-only cause (back buffer, swapchain).
  - Ranked hypotheses:
    1. **Paging / tile mapping.** The engine streams reserved (tiled) textures: `TextureStreamRequestTiles 1290B8`, the profile scope "KTGLGraph.Cache.UpdateTileMapping" (`2269478`), and the page-pool update at frame start (`252510`). `UpdateTileMappings` / residency operations are scheduled by VidMm and can block every queue.
    2. **Another process's GPU work / OS scheduling** (RTSS / Afterburner OSD, overlays, DWM at 240 Hz).
    3. **A cross-queue Wait** on a fence signaled late (the trace logs no Signal/Wait events).
  - **This is the largest scheduling loss in the trace: ~1 ms/frame ≈ 9 % of a GPU-bound frame.**

### B.3 Earlier / more frequent submission: model and verdict

**No-backlog model** (CPU-bound or latency-serialised case: the GPU starts each list as soon as it is submitted):
- `end_now` uses today's list boundaries.
- `end_fluid` assumes every list's work became available continuously while it was recorded (the limit of "submit more often"), plus 8 µs per extra boundary every 250 µs.

| GPU durations scaled by | 1.0 (r5) | 0.6 | 0.45 | 0.35 | 0.3 (CPU-bound PC, GPU ≈ 3 ms) |
|---|---|---|---|---|---|
| mean gain per frame | 100 µs | 122 µs | **132 µs** | 12 µs | 0 |

- **"Both near the limit" (GPU ≈ recording span, scale ~0.45): ≤ 0.13 ms/frame, about 1 %.**
- When the GPU is the bottleneck it has a backlog (r5 lag 7-9 ms), so submission timing does not matter.
- When the CPU is the bottleneck, the GPU's idle is off the critical path, because 2 frames are in flight on PC DXGI (r5 shows the next frame recorded while the GPU still runs the last one).

**Designs, with gain and risk:**

| id | change | gain | risk | status |
|---|---|---|---|---|
| **K1** | live data write of `CmdFlushLimit`, both directions (e.g. ×0.5 and ×2), read with the engine's own stats | CPU: shorter frame-end drain spin (≤ 0.7-2.1 % of main) vs fewer dispatcher wakes (watts on the Ally). GPU: ≈ 0 (no extra ECLs) | low. If the ring overflows at small N, the main thread Sleep(0)-spins; `status` shows the ring-full wait; floor N = 32 | **built: `patcher\k1_live.fsx`, default off** |
| E2 | K1 kicks also submit: `1D3186 33 D2 → B2 01` (and the other 28 sites) or a cave in `254110` that sets `submit = 1` when ≥ T µs have passed since the last submit | ≤ 0.1-0.13 ms/frame (model above), minus 8 µs GPU per extra list and ECL-thread CPU | medium. Mid-pass list splits are already exercised (`945664`), but every split adds a CB boundary and allocator/list ring use (`4920E0` may wait on a fence when the ring is exhausted) | design only. **Not recommended** |
| E3 | Submit the first segment of the frame early: one `submit = 1` after the first N draws (cave in `254110`, per-frame latch) | only in a serialised frame start; ≤ the time from render begin to the first explicit submit (unmeasured; TS probes W/B plus RGP needed) | medium | design only |
| **G1** | **Find the device-wide gate (B.2):** WPR/GPUView GPU trace (measurement only), or extend gpuopt with counters on `ID3D12CommandQueue::UpdateTileMappings` (vtbl +0x40) and `Wait` (+0x78), with timestamps per frame | up to ~1 ms/frame on the PC trace if it is engine-caused and avoidable (e.g. batching tile-mapping updates) | – | next step |
| Ally | The latency-1 waitable (input-latency.md §3): on vkd3d, render N+1 starts only after frame N is presented, so the GPU drains at every frame boundary. Lever: `[Latency] MaxFrameLatency = 2` (ASI 1.1.0, another agent). K1/E2/E3 do not address it | see input-latency.md §7 A/B | – | exists |

---

## C. A/B plans

**AS0 (PC first, then the Ally).** Same save and spot (the shrine and a combat or effects-heavy spot), FG off, GI HIGHEST.
1. Stock run: PresentMon 60 s plus an RGP capture.
2. Start `set FT_ALLOW_WRITE=1 & dotnet fsi patcher\async_slots.fsx arm 0`, then launch the game. After loading, `status` must show the runtime copy as [0; 3; 4; 6].
3. RGP: there must be a **4th compute queue**, and the 1x1x1 effect chains (r5 GFX segments 70-80) must leave GFX. `sched_overlap.py` on both captures shows the GFX time saved.
4. PresentMon ABAB: GPU busy, fps, 1 % lows. Check visually for effect lag or corruption (sparks, particles, hits).
5. Abort on a hang or a TDR: restart the game; the patch does not persist.

On the Ally the boot race runs inside Proton. The ASI owner can port it as an early .data/.text write in dllmain, which is simpler.

**AS-N (research).** Same procedure, one slot per launch. Record which passes leave GFX, so each slot gets a name. If RefProbe #121/#414 move, RP1 is found.

**K1:**
1. `dotnet fsi patcher\k1_live.fsx status 5` gives the baseline (draws, kicks, drain spin, ring-full wait).
2. `apply <limit/2>`, run `status 5`, then `apply <limit*2>`, `status 5`, then `revert`.
3. Each leg: 60 s PresentMon, CPU profile (main thread `418F40` share), on the Ally MangoHud CPU W.
4. Pick the setting with the lowest drain spin and no ring-full wait. The ASI can ship it as a data write at init.

**G1:** a WPR GPU trace (`wpr -start GPU -filemode`, 10 s in game) → GPUView. Look for paging packets or other processes' DMA packets at the frame-start gaps. Then decide whether tile-mapping batching is worth designing.

## D. Files

| file | what |
|---|---|
| `re\scripts_gpu\sched_overlap.py` | per-segment occupancy, shader groups for every PSO, free wave-slot time per CB, compute-segment list, overlap model inputs |
| `re\scripts_d3d12\submit_model.py` | per-frame CPU-late vs gated GPU idle, submit holes, no-backlog "fluid submission" model (`--gpu-scale`) |
| `re\scripts_d3d12\slotargs.fsx` | constant args at call sites (slot ids at `52BE1C` / `148514`, submit flags at `254110`, marker strings at `829BFC`) |
| `patcher\k1_live.fsx` | K1 offline check, live status from the engine's stream stats, apply/revert (default off) |
| `patcher\async_slots.fsx` | AS offline check (dump + installed exe table), status, boot-race arm, revert (default off, experiment) |
| `re\symbols\incoming\merged\20261007-140743_gpu-scheduling.tsv` | 29 symbols (merged into `symbols.tsv`) |
| `D:\nioh3-re\gpusched\` | Ghidra query outputs, r5 `sched_segments.tsv` / `sched_cbs.tsv` / `sched_summary.txt`, `qinfo.py` |

## ASI 1.7.5 AS0 integration
`[Patches] AS0 = 1` in Nioh3EngineFix.ini (default 0, also in the Ally zip) writes `226A497 74 14 -> EB 14` (patches.json site AS0, pattern-tested) from the decrypt-wait loop, i.e. before the renderer init `2269EC4`. Ordering guard: the live slot table `.bss 0x45C0460` is zero until init; if entry 1's slot id (+0x14) is non-zero the init already ran and the patch is refused (log `AS0: renderer already initialised ... NOT applied`). Success log `AS0: slot 0 forced on`. Stats: `stats/s ASYNC: N async compute queue(s), slots ...` every 30 s (stock AMD: 3 = slots 3 4 6; with AS0: 4). Risk (documented in ini/README): KT disabled slot 0 on non-NVIDIA on purpose; faster, slower, one-frame effect lag, corruption or GPU hang are possible. Ally A/B: AS0=1 for one launch.
