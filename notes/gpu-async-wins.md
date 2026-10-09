# Nioh 3: GPU scheduling wins - async compute (AS) and why AS0 read "neutral"

Status 2026-10-09, branch `async` (ASI 1.7.44, from main 1.7.43). Static work on the dump + vkd3d-proton source
(`D:\refs\vkd3d-proton` 2230755) + offline harnesses on this PC (RX 9070). The game was not launched.

## Plain summary

- **The engine already knows how to run compute next to graphics.** KTGL has 13 "async slots". For each, Koei Tecmo wrote the
  pass twice: on the graphics queue, and on its own COMPUTE queue with the engine's fences (the compute queue waits for the
  graphics work it needs, the graphics queue waits for the result right before it is used). Stock PC/Ally: fog, sky clouds
  and ocean use it. GPU particle/effect simulation (slot 0) uses it **only on NVIDIA**; grass displacement (11) and tree wind
  (12) never.
- **Built: AS** (`[Patches] AS`, default 0): creates the slots in `AS_Slots` at startup (default `0`; Ally `0 11 12`) and adds
  a live menu row, so one launch can A/B the stock queues against AS's. Same shaders and the engine's own fences, so the
  same picture.
- **Why AS0 (slot 0) read "neutral" before:** both tests could not show it. PC 1.7.16 was CPU-bound on purpose (CPU busy
  6.4 ms = frame, GPU busy 5.38 ms with and without AS0) and launch-to-launch noise was 5 %; in a CPU-bound frame the GPU runs
  right behind the CPU, so the graphics queue reaches the effect wait just as the compute work is submitted: nothing to hide
  it under. The Ally 1.7.5 run had the per-PSO GPU profiler on (~14 ms of stamps). A GPU overlap change needs a GPU-bound
  A/B in one launch, which AS's live row gives.
- **Measured offline on this RX 9070** (real D3D12 harness, the r5 chain shape): moving a 0.30 ms chain to a compute queue
  with the engine's fence pattern saves **0.133 ms** (44 %), because the move itself costs 0.06 ms of extra list splits and
  0.13 ms of cross-queue sync; with no graphics work to hide under (the CPU-bound case) it **costs +0.19 ms**. So: PC at the
  user's 1440p settings (GPU-bound, ~8.7 ms) **~0.13 ms per frame from slot 0 (+1-1.5 % fps)**, not the 0.25-0.3 ms the
  occupancy model predicted; slots 11 / 12 (0.02 ms of work on the PC) would be a net loss there, hence default `0`.
  Ally USER (28.7 ms GPU, power-limited): effects 0.55 + GVD 0.36 ms of low-occupancy compute -> est. 0.3-0.5 ms (1-1.5 %)
  with `0 11 12`, minus the power effect. Risk: KT switched slot 0 off for non-NVIDIA for an unknown reason (stable in two earlier launches), 11 / 12 never ran
  on PC.
- **Not built (and why):** RefProbe GI gathers on async (not slot-aware; weeks of engine surgery), ExecuteCommandLists merging
  and Signal/Wait elision (no gain on vkd3d-proton, not provably the same on Windows). Details below.

## 1. Candidates

| # | candidate | measured target | PC gain (RX 9070, 1440p, GPU-bound) | Ally gain | correctness | risk | status |
|---|---|---|---|---|---|---|---|
| 1 | **AS slot 0**: effect GPU chains to async (engine path) | r5 GFX segs 69-80: ~0.33 ms of 1x1x1 / 3x1x1 dispatches at 0-13 % occupancy (168/190/190/75-dispatch chains); Ally USER CS dfe80724 0.339 + 4924b422 0.114 + 52e987e4 0.071 + ffc7f39e 0.026 = 0.55 ms (176/120/166/128 dispatches/frame) | **0.13 ms M (offline harness)**, +1-1.5 %; CPU-bound: -0.19 ms | 0.2-0.4 ms E (0.7-1.4 %), minus power | engine's own async path; GFX waits at the first consumer (RefProbe / EffectViewPrepare join) | medium-high (KT off on non-NVIDIA) | **built (AS)** |
| 2 | **AS slots 11 / 12**: GrassVertexDisplacement, Tree2Wind | PC 10.9 + 9.3 us (r5 segs 39-40); Ally GVD 0.362 ms (3 dispatches) | **net loss** (~0.02 ms of work vs ~0.19 ms of split + sync) | 0.1-0.2 ms E | engine path; consumers wait in ViewVsmListA (1F4CF3 / 1F4D14) and MaterialTechniqueBind (3C7FB7 / 3C7FE4) | high (never shipped on PC) | **built (AS, same row)** |
| 3 | AS "-3 -4 -6": shipped fog / VCLOUD / ocean back on the graphics queue while ON | Ally async fog 1.78 + VCLOUD 1.52 ms | - | unknown sign: on a power-limited APU overlap raises power and lowers clock | engine path (stock on other configs) | low | **built (AS_Slots option)**, Ally question only |
| 4 | Research slots 5, 7, 8 (RainDrop), 9 (GBufferEdit), 10 (CombineHDR, StaticNature, MiasmaDecal, material scales, GVD part) | not attributed | unknown | unknown | engine path, producers not audited | high | boot-only via AS_Slots (no live switch) |
| 5 | RefProbe #121/#414 async | PC 0.65 ms HIGH / ~2.5 ms GI HIGHEST at 10-36 % occ.; Ally 1.26 ms | 0.25-0.6 ms | 1-3 ms E | **not slot-aware**: ReflectionProbePassRecord 542C08 only *joins* slot 0 (13ACh gate at 5443C0); a split needs RE of its bindings, root signature and states on a compute queue (RP2) | high | not built (weeks) |
| 6 | ECL merge (several lists per ExecuteCommandLists) | PC ~8 us per list boundary x 21-23; Ally gaps 1.67 ms/frame (unattributed until GQ runs) | <= 0.17 ms | unknown: vkd3d makes one vkQueueSubmit2 per ECL, but no barrier or semaphore between them (command.c 26282); only the kernel submit would go | D3D12: "decay does not occur between command lists executed together in the same ExecuteCommandLists"; the engine relies on per-list COMMON (stamp reset, vkd3d-audit B) -> not provably the same | medium | **rejected** (revisit only if GQ shows >= 0.3 ms of per-submit gaps) |
| 7 | Signal coalescing / skip satisfied queue Waits | 2 GFX Signals/frame from CacheFlushOnGfx + per-list signals | ~0 | 0 | vkd3d: a non-shared Signal is no Vulkan submit (fence worker on the last submission's timeline value, command.c 24763); a Wait whose value is already reached is dropped (`has_wait` false, 24703) | - | **rejected (nothing to gain)** |

Order to test: AS on the PC (GPU-bound), then AS on the Ally, then the Ally "-3 -4 -6" question. Nothing else here is
worth a build before the GQ gap census (gq-jt-probes.md) says where the Ally's 1.67 ms of graphics-queue gaps go.

## 2. How the engine's async slots work (static, v2.02)

- **Init** `RenderAsyncSlotsInit 2269EC4`: table `44C04D0` {u8 enable, f32, u32 id} x 13 (ships 0 3 4 6 enabled, slot 6 param
  0.5); non-NVIDIA clears slot 0 in place (226A490 `cmp [rbp+70h],10DEh; je`, loop 226A499); mask = id == i && enable
  (226A4BC); copy to `45C0460`; `AsyncSlotsCreate 32FA450` makes one COMPUTE queue + context per bit, mask at sched+1CCh
  (sched = RS+1130h); slot 0 also starts the KIDS job `KIDS_ASync_LLCulling_Effect`.
- **The only mask test** is `AsyncSlotGetCtx 52BE1C` (`mov eax,[rcx+1CCh]; bt eax,edx`; full-text scan: no inlined copy).
  It has **32 direct callers and no other reference** (no jmp / jcc / pointer / lea). Classified:
  - producers (one per switchable slot): 3D0C67 effects (0), 5F328D VCLOUD (3), 14906E fog (4), 1A25524 ocean (6),
    52B142 GVD (11), 418643 Tree2Wind (12);
  - slot-0 job / joins: 3D16E1 (job), 3D37FD EffectViewPrepare, 544634 ReflectionProbePassRecord, 194E70E (fog cold chunk),
    19CFAFC; every join is entered only when `RS+13ACh != 0`, and the only byte write of 1 is the producer's async branch
    (3D0D3A);
  - init: 32FA4CC / 32FA4EA / 32FA505; other slots: 52853, 104B54, 1063E6, 193DC34, 1947180, 1A41879, 347AF60, 3675307 (10),
    10D19C, 1A408F4 (9), 41A28E, 19A6B36, 19CB677 (8), 52C44B, 52D65F (7), 3658C11, 3658F73 (5).
- **Producer pattern** (GVD 52AF94, same for the others): `ctx = GetCtx(slot)`; NULL -> dispatches into the graphics stream
  (tok 7E); else `ctx->Begin`, `5F17C(gfx ctx)` = KtglCtxGetFence of the graphics stream (a list split) -> `3D2724(ctx, f)`
  = the async context waits on it, the same dispatches through the async variants of the shader object, then
  `KtglCtxGetFence(ctx) -> AsyncSlotAddFence 148514(sched, key, slot, f)`.
- **Consumer pattern**: `3C5D0C` peek the slot's pending fence -> `GfxWaitAsyncFence 4F8F38` (records token A9 = a queue
  Wait unless the fence is already complete) -> `4F94B4` pop. Consumers peek regardless of the mask, so a producer that
  took the graphics path leaves nothing to wait for.
- **Slot 0 in job mode** (545C44: job present): the producer records into RS+1310h.., kicks the KIDS job (3D06A0), sets
  RS+13ACh = 1; the first join (EffectViewPrepare / RefProbe / fog / 19CFAF2) finishes the batch (18F790) on the async
  context, publishes the fence and waits right there. On the GPU the overlap window is producer (r5 fab1e, ~1.3 ms) ->
  join (fab26, ~5 ms): ~3.7 ms of graphics at ~50 % occupancy, far more than the 0.33 ms chain needs - **when the GPU lags
  the CPU** (GPU-bound). CPU-bound, the compute list reaches the queue at the join just as the graphics queue does.

## 3. AS (built, ASI 1.7.44, default off)

- `[Patches] AS` 0 / 1 / 2, `[Tuning] AS_Slots` (default `0`, Ally `0 11 12`; `-N` for 3 4 6 = keep that shipped pass synchronous
  while ON). Boot (decrypt-wait loop, like AS0, refused once the live table is filled): slot 0 -> the AS0 byte 226A497
  74 -> EB; 1..12 -> the table's enable byte. Intel GPUs: slot 0 refused (hardware-compatibility.md).
- Live (`asyncslots.cpp` / `.asm`): each producer's `call 52BE1C` -> `call tramp -> caveAsK`: counts the call, runs
  `mov eax,[rcx+1CCh]`, ANDs `as_and_on` (= ~sync slots) or `as_and_off` (= ~added slots) and resumes at 52BE22. OFF =
  the stock slot set at the producers; ON = AS's. Only rax and flags change, as in the original.
- **Why a toggle is safe at any moment:** every producer section is self-contained (GetCtx, Begin, waits, dispatches,
  AddFence in one function); consumers peek the fence list, not the mask; slot 0's joins keep calling the unmasked
  GetCtx and only run when the producer itself set 13ACh, so they always find the context they need. The only state
  outside one frame is the effect double-buffer index (RS+13C0h / 13C1h), which only the async branch reads; one frame
  after a toggle may show effects from the other path's buffer at worst (skip the first second of each A/B window).
- Log: `AS: boot: async slots requested 0 11 12 -> added 0 11 12 (vendor 1002 ...)`, `AS: live switch at the producers
  effects(0) GVD(11) Tree2Wind(12) (switch ON|off ...)`, every 30 s `stats/s AS: switch ON|off, added 0 11 12, kept
  synchronous while ON - | producer calls: effects(0) N/s GVD(11) N/s Tree2Wind(12) N/s | created slots 0 3 4 6 11 12`
  plus the existing `stats/s ASYNC: 6 async compute queue(s), slots 0 3 4 6 11 12 (AS0 applied)`.
- Menu row "Async compute slots (AS)" (heading "Experimental (GPU, A/B live)"); Save writes `AS`.
- Cost when off (AS = 2): one AND + indirect jump per producer call (~6 calls per view per frame), idle queues.
- KT's async branch skips the pass for that frame if the async context's Begin (vf+10h) fails (all slot users, also the
  shipped 3 / 4 / 6): a visible one-frame drop of grass / tree wind / effects would point there.
- On vkd3d-proton (Ally) each extra D3D12 compute queue maps onto the Vulkan compute family (at most 4 Vulkan queues per
  family, device.c 2953); with more D3D12 queues than that vkd3d aliases them (it handles wait-before-signal for exactly
  that case, command.c 24703): async slots then serialise among themselves, still in parallel with graphics.

## 4. Offline proof

- `tests\test_patterns.py`: the 6 producer sites (unique patterns, `call 52BE1C`, edx = slot), all 32 callers classified (a
  new one fails the test), GetCtx bytes / resume, gate == AS0, slot table ids / enables, slot-0 join gates == dump and
  deps, caves present, FrozenWrite arrays sized. Result: see §6.
- `tests\run_asyncslots_harness.py`: the original 34 bytes of AsyncSlotGetCtx from the dump vs the cave path, 1.2M random
  scheduler states (mask, 17Ch indices incl. negative, contexts, slot 0-12, switch values): every GPR and xmm0-15 after the
  cave == the original on the same state with the narrowed mask; rax == the model; the scheduler is untouched; per-site
  counter +1. Result: see §6.
- `tests\run_async_overlap_harness.py`: this PC's real device. A 170-step chain of Dispatch(1,1,1) with UAV barriers (the
  r5 effect-chain shape) and ~3 ms of full-screen graphics: SERIAL (stock) vs ASYNC (engine pattern: GFX Signal -> compute
  Wait, chain, compute Signal; graphics; GFX Wait before the consumer) vs NOWINDOW (wait right after the producer = the
  CPU-bound case). Output bit-identical in all modes (and the chain == a CPU model). Result: see §6.

## 5. Test lines

**PC (GPU-bound; the default abba2 condition is CPU-bound and would hide it):**
```
powershell -NoProfile -ExecutionPolicy Bypass -File tools\testmatrix\abba2_pc.ps1 -Live AS -LaunchFlags none -Quads 6 -RenRat 75 -Upscaler FSR -MaxFps 240 -Input vpad -SrcRoot D:\nioh3-wt\async -RunName as_gpu
py -I tools\testmatrix\abba2_report.py captures\tm\abba2\as_gpu
```
Install the 1.7.44 ASI from `D:\nioh3-wt\async\mod\Nioh3EngineFix\bin` first; the harness writes `AS = 2` (installed,
switch off) and the default `AS_Slots = 0` (slot 0 only: 11 / 12 are a net loss on the PC). Check: report windows `gpu busy` ~= frame time (GPU-bound), `seen` =
the switch state, `stats/s AS` producer calls > 0 for effects(0). abba2_report skips its GPU-move guard for AS (a GPU-side
change). KEEP rule unchanged (median dfps >= +1 %, every quad > 0). Also look at the shot pair for effects (sparks, hits).
Expect about +1-1.5 % (0.13 ms of ~8.7); KEEP needs >= +1 %, so run -Quads 6 if the first quads straddle it.

**Ally (USER preset, GF on, the S1 spot; one launch per line):**
```
[Patches] AS = 2        [Tuning] AS_Slots = 0 11 12
[Patches] AS = 2        [Tuning] AS_Slots = -3 -4 -6
```
Per launch: 60 s settle, then ABAB 4 x 30 s with the menu row "Async compute slots (AS)" (ON, OFF, OFF, ON), MangoHud fps /
gpu_power / gpu_core_clock; skip the first 2 s after each toggle. Log must show `AS: boot ... added 0 11 12` (line 1) and
`stats/s AS: switch ...` matching each window. KEEP line 1 if fps ON > OFF in both pairs; line 2 answers whether the shipped
async fog/clouds help the power-limited APU (ON = synchronous).

## 6. Results (this branch)

All on this PC, 2026-10-09 (no game running; `%TEMP%\nioh3_measuring.lock` checked before each step).
- `py -I tests\test_patterns.py`: **ALL 225 SITES OK** (AS block OK: 6 producers, 32 classified callers, gates, deps).
- `py -I tests\run_asyncslots_harness.py`: **ALL PASSED**, 1.2M cases (context 307667, NULL 892333, of which 268712 NULL
  because the switch masked the slot). Mutation (OFF path without its AND) -> FAILED (393342): the test sees it.
- `py -I tests\run_async_overlap_harness.py 170 100` (RX 9070, W = 4 full-screen 2560x1440 draws ~2.9 ms), five runs:

  | mode | GFX span ms |
  |---|---|
  | SERIAL (stock) | 3.23 |
  | ASYNC (engine pattern) | 3.08-3.10 |
  | NOWINDOW (wait right after the producer) | 3.42-3.44 |
  | SPLIT (same 4 ECLs, one queue) | 3.30 |
  | X alone on DIRECT / COMPUTE | 0.30 / 0.29 |

  Saving 0.132-0.134 ms (44 % of X) in all five runs; list splits +0.063 ms; cross-queue sync +0.124-0.129 ms. K = 340
  (X 0.60 ms): saving 0.143 ms. X result == CPU model and bit-identical in all modes; render target identical. (The first
  runs, before the SPLIT mode changed the rotation and the clock settled, scattered -0.36..+0.15 ms: run it warm.)
- `build.bat` (with the main checkout's `tools\msvc\setup_x64.bat`): OK, `bin\Nioh3EngineFix.asi` 1.7.44.
  `menu_map.py --selftest`, `abba2_report.py --selftest`: OK.

What it means: on Windows the async move pays a fixed ~0.19 ms (splits + sync) and wins only what it hides beyond that.
The effect chain (0.33 ms) clears it; small passes do not. On vkd3d (Ally) the per-submit and cross-queue costs are
unmeasured (the GQ census would show them); the chains there are 2-3x longer, so the margin is larger.

## 7. Files

`mod\Nioh3EngineFix\src\asyncslots.{h,cpp,asm}`, dllmain.cpp (boot + Start + stats), menu.{h,cpp} (row kXAS),
`tests\asyncslots_harness.cpp` + `run_asyncslots_harness.py`, `tests\async_overlap_harness.cpp` +
`run_async_overlap_harness.py`, patches.json (sites ASp0/3/4/6/11/12 + deps AS), test_patterns.py (AS block),
`tools\testmatrix\matrix.json` (item AS, grp 11 = not in run_pc's plan), `abba2_report.py` (AS read-back, GPU items).
