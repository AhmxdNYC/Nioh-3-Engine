# Engine rewrite options: from a D3D12 scheduler to a new renderer (feasibility, 2026-10-09)

Feasibility study only. Nothing was built, and the game was not launched on this PC. Ghidra was not run either: Nioh3.exe was
running on this PC (another agent's session), so two callees noted in C2 are still not decompiled.

Inputs:
- render-architecture.md, meshlet-frontend.md, gpu-unattributed.md, gpu-rewrite-round3/4.md, cost-vs-looks.md,
  gpu-volumetrics-rewrite.md, gpu-barriers.md, gpu-scheduling.md, invisible-gpu.md, shadows-redundancy.md,
  gpu-resolution-audit.md, fsr3-audit.md, settings*.md, frame-timing.md;
- pc2 GP2 #125 (budget125.txt);
- bridge #704/#705 (TR and PJH dropped on the PC), #732 (MSV on the Ally: +0.2-0.4 fps), #736 (POSTEFFECT_RESOLUTION
  neutral on pc2), #762 (the scheduling agent's SCHED1 hypothesis);
- **XSYS1 matched captures** #144 (PC) and #145 (pc2). I ran the cross-system agent's `tools\xsys_compare.py` over them only to
  read off numbers (section 1). The cross-system analysis itself belongs to that agent (notes/cross-system-compare.md, not
  yet pushed).

Its notes on scheduling/sync (notes/gpu-scheduling-sync.md) and VRAM were not in the repo when this was written. Where an option
overlaps their work, this note points to them and does not repeat it.

Units:
- pc2 = RX 6650 XT, 1080p HIGH, FSR 65, ~27 ms GPU / 37 fps in dense scenes. 1 ms = ~+1.4 fps (+3.8 %).
- PC = RX 9070, 8.5 ms GP2 span in XSYS1. Note the PC log reports 2560x1440 output, so the PC is NOT pixel-matched: it renders
  1.78x pixels. 0.3 ms = ~+3 % when GPU-bound.
- Ally = USER config, 28.7 ms. 1 ms = ~+3.6 %.
- Agent-day = one agent working day, including the queued A/Bs on the test machines.
- E = estimate; M = measured.

## 0. Verdict

1. **No rewrite reaches a large same-picture gain on pc2 unless the RDNA2 anomaly turns out to be scheduling.**
   - The renderer is a reasonably efficient deferred design (render-architecture §0).
   - Every same-look algorithmic lever in B and E adds up to about **3-5 ms on pc2** (37 -> ~42-44 fps), about **0.5-1 ms on
     the PC** and **1.5-3 ms on the Ally**.
   - Of that, the meshlet front-end is the only single item of 1 ms or more.
2. **XSYS1 changes the question.** At matched settings, pc2's excess over what its raw speed predicts sits in a few families:
   - meshlet LOD/culling, with an anomaly of x0.22 (4.5x relatively costlier on pc2);
   - shadow maps, x0.39;
   - async VCLOUD and fog, x0.44;
   - copies, x0.47.

   It is not spread over the frame. Single kernels blow up 40-100x per dispatch on pc2: MshLod#2 1.50 vs 0.015 ms, MIC 1.26
   vs 0.029 ms, and MshLod#1 1.66 ms, which only appears on pc2. Kernel work does not explain that. It matches the scheduling
   agent's finding (#762): MshLod#1's per-dispatch cost tracks VCLOUD's async wall time (r = 0.88), not draw count.

   **If SCHED1 confirms async contention, 2-5 ms of pc2's frame comes back from a scheduling change with zero new code.** That
   is more than any rewrite here, and it shrinks the meshlet rewrite's prize.
3. **A (a generic D3D12 scheduler) and D (a new renderer) are not worth building.** The reasons are in sections A and D.
4. **Recommended next (section 7):**
   1. The meshlet front-end, staged and gated on SCHED1 + Phase M.
   2. A cheap near-identical package of about 6 agent-days: the VSM batch-overdraw cap plus the engine's own amortisation knobs.

## 1. New data: XSYS1 family split, PC vs pc2 (same tour points 7356/52/7292/5631, GP2 MinRun 1, matched config)

| family | PC ms (% span) | pc2 ms (% span) | anomaly (1 = scales like the GPU, speed factor x0.39) |
|---|---|---|---|
| meshlet LOD / culling / HiZ | 0.58 (6.8) | **6.73 (21.3)** | **0.22** |
| G-buffer geometry | 1.75 (20.6) | 6.57 (20.8) | 0.68 |
| VCLOUD [async wall] | 0.49 | 2.91 | 0.44 |
| shadow maps | 0.30 (3.6) | 2.01 (6.4) | 0.39 (SM-Sfb skinned: 0.079 vs 0.909 ms, 0.26) |
| GI / reflection gathers | 0.57 | 1.23 | 1.20 (scales normally) |
| FSR3 | 0.35 | 1.26 | 0.71 |
| copy / resolve / clear | 0.23 | 1.25 | 0.47 |
| VFSCAT fog [async wall] | 0.22 | 1.25 | 0.45 |
| post (bloom / MB / DOF / compose) | 0.19 | 0.52 | 0.96 |

The pc2 graphics-queue excess over "PC x speed" is about 5 ms in meshlet, 1.2 ms in shadows and 0.7 ms in copies. Post is
small and scales normally, which agrees with #736.

## 2. Ranked table

| rank | option | pc2 ms (fps) | PC 9070 ms | Ally ms | effort (agent-days) | visual risk | compat risk (RDNA2/3/4, NV, Intel, Proton) | first validation experiment | verdict |
|---|---|---|---|---|---|---|---|---|---|
| 0 | *(not an option here, the gate)* SCHED1: VCLOUD / fog on the graphics queue (scheduling agent, #762) | **0 to 2-5 (E)** (37 -> up to ~45) | ~0 | ? (#503-style) | 0 (built: AS row) | none (the engine's own sync path) | per-vendor ini default | already queued on pc2 (#762); also run it on the Ally | **decides rank 1's scope** |
| 1 | **B1 meshlet front-end replacement**, staged | 1-2, +1.5 if MshLod#1 is real work; **0.5-1 if SCHED1 removes the contention** | 0.2-0.4 | 0.7-2 | 5-7 (M1 capture/replay) + 10-15 (persistent LOD) + 5 (fused scans) + 15-20 (multi-view); total 35-50 | none (set-equal outputs, shadow-run check) | persistent threads need care on Intel / RADV | Phase M probe (#692) + SCHED1, then the M1 offline replay | **pursue, gated** |
| 2 | **B4b VSM batch-overdraw cap** (shadows-redundancy S1: tiles past the 8/frame budget are cleared and drawn every frame but never marked valid or sampled) | part of 0.44 (VSM tiles) mean; bursts | ~0.05 | **0.1-0.4 mean, 3-5.7 ms movement bursts** (1 % lows) | 3-5 | **near-identical**: same pixels that frame; only streaming-feedback timing for those meshlets changes | none (CPU-side cave in 1F8340) | Ally tour with movement: 1 % low + GP marker "Batch Tiles"; streaming request log on/off; shotdiff | **pursue (package 2)** |
| 3 | **E1 engine amortisation knobs** left at "every frame" in all 62 env presets (LPVUpdateFrequency 1, SkylightAmbientUpdateFreq 1, VolumetricFogTemporalFactor 0, VolumetricCloudReprojectionBlendWeightCurrentFrame 0) | 0-0.3 (E, unmeasured) | ~0-0.05 | 0-0.3 | 1-2 (decode semantics + ASI data write) | low (LPV and sky ambient are low-frequency) | none (engine path) | decompile the readers of RenderConfig +0x1394 / +0x144 / +0x13C0 / +0x7C0, then an ABBA on pc2 + Ally with shotdiff | **pursue (package 2)** |
| 4 | **A0 existing barrier batching (`[GpuOpt] Barriers = 1`) at a GPU-bound spot** | 0-0.3 | -7 % measured CPU-bound; GPU model 0.2-0.3 | ~0 (RADV already merges) | 0.5 (built) | none | Windows only (no-op on RADV) | pc2 live ABAB at 7292 (CPU 10 ms vs GPU 27 ms, so the dispatcher cost is free there) | free test; ship only per class if positive |
| 5 | C2 GPU-time-driven dynamic resolution (feed the game's own controller GPU busy instead of frame time) | holds a target (e.g. 40-45 fps by dipping to 55-60 %); 0 at the same resolution | same | same | 3-5 | **a look change during dips** (opt-in, not "same settings") | all (Proton included) | pc2 tour: % of frames at the target + mean render scale, vs stock DRS 60FPS | offer as opt-in only |
| 6 | B2 froxel-fog temporal reuse (half the froxels per frame + history reprojection) | 0.2-0.5 frame (async wall 1.1 -> ~0.6) | 0.05-0.1 | 0.2-0.4 | 8-12 | medium (light lags behind torches and flashes; ghosting on moving lights) | all | E1's VolumetricFogTemporalFactor test first (the engine may already have the path) | only if E1 shows no native path and SCHED1 does not already hide fog |
| 7 | B3 RefProbe GI gather at half rate (checkerboard + temporal) | 0.3-0.5 | 0.2-0.3 | 0.5-1 | 10-15 | medium (GI lag, disocclusion) | all | SkipFx "stale" test in cost-vs-looks (queued) shows how visible a 1-frame-old GI is | later |
| 8 | B4a far cascade every 2nd frame for dynamic casters | 0.15-0.3 | ~0.05 | 0.1-0.2 | 5-8 | low-medium (30 Hz far character shadows) | all | shotdiff of moving enemies at a cascade-2 distance | later |
| 9 | C1 better upscaler at lower internal resolution | net 0.2-1.0, but **not the same look** (GI / SSR / AO buffers shrink with render scale) | FSR4 at 67 % instead of 75 %: ~0.7-1 | negative (FSR3 is the cheapest; XeSS measured slower) | 1-2 (blind shotdiff / flicker test, no code) | medium | FSR4 = RDNA4 only; XeSS DP4a costs more on RDNA2 | PC: OptiScaler FSR4 67 % vs FSR3 75 %, shotdiff + flicker metric | settings advice for the PC only |
| 10 | E5 VRS tier 2 (content-adaptive) on G-buffer / deferred PS | 0.3-0.8 (E, low confidence) | 0.1-0.2 | n/a or small | 8-12 | low-medium (FSR3 can amplify VRS blocks) | needs tier 2: RDNA2+, Turing+, Arc; vkd3d via fragment shading rate; off elsewhere | VRS-everything-2x2 SkipFx-style probe for the ceiling first | not now |
| 11 | B6 meshlet VS -> mesh shaders | 0.2-0.5 | ~0.1 | 0.2-0.4 | 25-35 (1616 VS records, new PSO streams) | none if exact | MS support varies (Intel, older NV, RADV NGG) | after MSV; empty-wave share from asset-audit | not worth it (MSV took the chain; Ally +0.2-0.4 fps, #732) |
| 12 | B5 one fused post compute pass | <= 0.1-0.2 | ~0.05 | 0.1-0.3 | 10-15 (exe-embedded DXBC) | low | all | none needed: #736 + XSYS1 post 0.52 ms | **not worth it** |
| 13 | A full D3D12 frame scheduler (auto reorder / merge / async re-balance / dead-work removal) | 0.1-0.5 beyond A0 | 0.1-0.3 | ~0 | 25-40 | corruption / TDR risk if a hazard is mis-inferred | **high** (per driver; vkd3d semantics) | n/a | **not worth it** |
| 14 | D new renderer back-end | <= sum of B (3-5) | <= 1 | <= 3 | 1-2 agent-YEARS | certain differences | very high (breaks on every game patch) | n/a | **never worth it here** |

## 3. A. D3D12-level frame scheduler

What such a layer could do, and what the data says about each part:
- **Barrier merge.**
  - Already built as gpuopt `Barriers = 1`: 1220 -> 434 calls/frame.
  - Measured -7 % on the PC, but **only CPU-bound** (180 fps, dispatcher +0.36 ms). The GPU-side model on the 9070 is
    0.19-0.45 ms.
  - It was never run GPU-bound on pc2. pc2 is the ideal case: CPU 10 ms against GPU 27 ms.
  - RADV already ORs consecutive barriers, so the Ally gets ~0.
  - That is row 4: a zero-code test.
- **Remove redundant barriers / transitions.**
  - The engine already skips same-state transitions at every site.
  - No redundant barrier was provable from RGP (gpu-barriers §6).
  - The barrier time is mostly dependency drains (5.51 of 6.84 ms summed on r5). Those are real.
- **Remove clears.**
  - 38 clears = 104 us on the 9070.
  - The 4096^2 cascade clear is an HTILE fast clear (render-architecture F5).
  - Nothing to gain.
- **Batch submits.** Worth <= 0.1 ms/frame (gpu-scheduling B3). Each extra ECL costs ~8 us.
- **Re-balance async vs direct automatically.**
  - A layer cannot know which dispatches are independent without tracking every resource across 21-23 lists, the copy
    queues, tile mappings and fences.
  - The engine already has a 13-slot async system with per-slot switches (AS / AS0 / AS_Slots). The useful re-balancing
    (SCHED1, AS0) goes through it with zero inference.
- **Dead-work removal.** This is the one thing only a layer can do: a resource-level trace (write never read before
  overwritten). Keep it as a **discovery tool** (gpuopt trace mode exists) for the ~0.55 ms of copies of unknown purpose
  (invisible-gpu §3). It is not a runtime optimiser.

Honest assessment: driver layers get away with this because they own the hardware queue model. A mod layer would have to
re-derive the engine's render graph from a token stream, for a 0.1-0.5 ms prize, with corruption risk on every GPU and Proton.
**Not worth building. Run A0 on pc2.**

## 4. B. Subsystem replacements

- **B1 meshlet front-end** (meshlet-frontend.md §6, designed).
  - The prize depends on two open answers:
    - Phase M: is MshLod#1 real work or an absorbed wait?
    - SCHED1: is the pc2 blow-up async contention?
  - XSYS1's 40-100x per-dispatch anomalies on pc2 point to contention: MshLod#2 is 1.50 ms on pc2 and 0.015 ms on the PC.
    A rewrite cannot remove contention, but moving the async kick can.
  - Plan: if SCHED1 is positive, build only M1-M3 (replay harness, persistent LOD, fused scans), ~20-25 agent-days for
    ~0.5-1 ms on pc2 and ~0.7-1.5 ms on the Ally (latency-bound). Otherwise build the full plan.
  - The asynchronous LOD (M5) only makes sense if SCHED1 shows that async *overlaps* on RDNA2.
- **B2 volumetrics.**
  - Clouds are already render/8 with a 1/16 Bayer temporal update plus reprojection. There is nothing left to amortise
    without changing the look. vc7 (a latency rewrite, same picture) is queued: 0.1-0.5 ms.
  - Fog (160x90x64 froxels) is recomputed every frame. Temporal reuse is the standard trick, but its artefacts are on
    moving and flickering lights, and Nioh is full of torches. VolumetricFogTemporalFactor = 0 in every preset hints that
    the engine already has a temporal blend; test that first (E1).
  - On pc2 the fog's frame cost may disappear anyway if SCHED1 moves it.
- **B3 reflection probes / GI.**
  - RefProbe is a per-pixel screen-space gather, not a probe re-capture (invisible-gpu §1), so "amortised probe updates"
    does not apply.
  - A half-rate gather with temporal reuse is possible at medium visual risk.
  - XSYS1 shows GI scales normally on RDNA2 (anomaly 1.20): no RDNA2-specific waste.
  - LPV / sky-ambient update rates are engine knobs (E1).
- **B4 shadows.**
  - The VSM page cache exists and works: static casters are cached, 8 tiles/frame, lifespan.
  - The open items:
    - the batch-overdraw cap (S1), near-identical, rank 2;
    - far-cascade half rate (rank 8);
    - viewport instancing (0.1-0.2, low feasibility).
  - The pc2 shadow anomaly (SM-Sfb skinned x0.26) sits in the same front of the frame as the meshlet blow-ups. Re-read it
    after SCHED1 before designing anything.
- **B5 post chain fused.**
  - The post family is 0.52 ms on pc2 and scales normally.
  - POSTEFFECT_RESOLUTION HIGH vs STANDARD was neutral (#736).
  - At 65 % render scale the surfaces sit in the 32 MB Infinity Cache (render-architecture §2.3).
  - Not worth it.

## 5. C. Upscaling and dynamic resolution

- **Is FSR3 at 65 % optimal on mid-range?** For RDNA2, yes as far as we can tell.
  - FSR3 is 1.26 ms on pc2 (4 %) and every pass feeds the image (fsr3-audit).
  - FSR4 is officially RDNA4-only.
  - XeSS DP4a costs more than FSR3: measured about 1 fps slower on the 780M, ~+0.8 ms.
  - Going 65 -> 60 % saves ~1.2-1.5 ms of render-res work (resolution audit: most of the frame does not scale at 65 %). An
    XeSS swap eats half of that. Worse, the GI, SSR, AO and fog-composite buffers are sized from render resolution, so the
    picture changes beyond what the upscaler restores. **Not "same look" on pc2.**
- **On the PC (RDNA4).** FSR4 through OptiScaler (XeSS input) at 67 % vs the user's 75 % saves ~0.7-1 ms, and FSR4 at 67 %
  is generally at least as clean as FSR3 at 75 %.
  - The Adrenalin FSR4 driver override probably cannot apply: the game links its own FFX fork statically (`ffxDispatch` is
    exe export ord 111; there is no FSR 3.1 DLL to upgrade).
  - This is settings advice backed by a blind shotdiff test, not a mod.
- **Ally.** FSR3 is the cheapest upscaler there and FSR4 has no native RDNA3 path. The only lever is a native-fp16 FSR3
  rebuild (fsr3-audit §5): it changes rounding, so it is the user's call ("within FSR's fp16 tolerance").
- **C2 dynamic resolution.** The game already has a controller.
  - `DynResolutionController` 5AF7F8 averages 60 frames of a time history at +0xA138.
  - 10F3330 compares the average against target x0.75 / x0.9.
  - The ratio is bounded by DYNAMIC_RENDER_RATIO_MIN/MAX.
  - The target is halved with FSR/XeSS FG + VSync.
  - It lowers resolution even when CPU-bound (settings.md), so its input is frame time, not GPU time.
  - Mod version: feed it GPU-busy time from our own timestamps, plus finer steps if the engine accepts them. Not yet
    decompiled: 5AFA40 / 5AF950 hold the step logic; Ghidra was not run while the game was up on this PC.
  - It keeps a frame-time target cleanly, but it is a look change in the dips. Opt-in only, never part of "same settings".

## 6. D. A new renderer back-end, and E. findings from the decompile

**D: never worth it here.**
- The back-end would have to reproduce:
  - 430 render-graph nodes;
  - ~7800 meshlet-VS shader records alone, plus 1600+ material shaders;
  - VSM, OBORO/LPV GI, UltraMarine water, toon edges, the thin-surface temporal filter, snow accumulation and G1E
    particles;
  - three upscalers + FG;
  - the KTGL token stream.
- If it reuses the game's shaders, it is the same renderer with a different scheduler (= A). If it rewrites them, the
  picture changes.
- The prize is capped by the architecture verdict: the same-look total is ~2.5-4.5 ms of 30, all reachable through B.
- XSYS1 shows pc2's waste is concentrated in a few families, not systemic.
- Every game patch would break it.
- Effort is agent-years, with certain visual drift.

**E: what the decompile and the data offer.**
1. **Amortisation knobs** set to every frame (E1, rank 3). These are data, not exe: env render-config fields, identical in
   all 62 presets.
2. **PS5-shaped async design.**
   - KTGL's 13 async slots ship with 0/3/4/6 on, and slot 0 is off on non-NVIDIA (AS0 built).
   - VCLOUD is kicked at graph node 4, right beside the meshlet chain (node 14).
   - On one fixed console GPU that overlap was presumably tuned. On RDNA2 PC it looks like contention (#762, XSYS1).
   - This is the most likely "console path left in the PC build", and SCHED1 tests it.
3. **No debug GPU work runs in shipping frames.**
   - DebugViewHistogramPS / DebugViewExposureMaskPS and MeshletGeometryDensityDebug have no GP2 row.
   - The FSR3 debug view is off.
   - `DebugFSR3CopyUpscaledDepth` has "Debug" in its name but feeds the post depth (it is used).
   - The KTGL perf overlay is gated (d+76F0).
4. **Unused-output work already found**, ~0.2-0.5 ms in total:
   - TOONEDGE2 runs full-screen every frame (0.17);
   - mostly-empty RefProbe group-1 dispatches (GIM built);
   - ESM at 2048^2 feeding only fog + LPV (ESM1 built);
   - the water-view culling chain with no water on screen (unproven).
5. **GI HIGHEST costs 3x VERY_HIGH and looks nearly the same.** That is a settings fact, not a mod lever, given the
   same-settings rule.
6. **Not my scope:** the ~0.9 ms/frame between-frame idle on pc2 heavy windows and the ~1.35 ms "gated" stall on the 9070.
   These belong to the scheduling/sync agent (GQ).

## 7. Recommendation

1. **Meshlet front-end (B1), gated.**
   - Do not start M1 until SCHED1 (#762) and Phase M (#692) are back.
   - SCHED1 positive (VCLOUD on DIRECT drops MshLod#1 under ~0.2 ms/dispatch): ship the AS_Slots default per vendor (that
     is the scheduling agent's call), then re-run XSYS1. Build M1-M3 only if pc2's meshlet family is still >= ~2 ms.
   - SCHED1 negative: build M1 (capture + offline replay, 5-7 agent-days), then M2 (persistent LOD), with the shadow-run
     check on all 3 machines.
2. **Near-identical package (~5-7 agent-days):** the VSM batch-overdraw cap (S1) + the E1 amortisation knobs.
   - Both are engine-native or CPU-side, work on every GPU and on Proton, and target the Ally's movement bursts and 1 % lows.
   - First experiment: decompile the E1 field readers (when the PC game is not running). Then one concurrent hands-free
     tour per machine:
     - BASE vs S1 vs E1, ABBA;
     - movement-heavy route;
     - PresentMon GPUBusy + 1 % low, GP marker "Batch Tiles";
     - shotdiff against the BASE-vs-BASE noise floor;
     - streaming request counts, for S1.
3. Free, slot it into any pc2 queue: **A0** (`Barriers = 1` live ABAB at 7292, GPU-bound).

**Not worth it, and why:**
- **A, the full scheduler:** the prize is 0.1-0.5 ms, the risk is high, and the engine's own async slots already give the
  only useful re-balancing.
- **D, the new renderer:** effort in years, certain drift, and a prize no bigger than B.
- **B5, the fused post pass:** post is 0.52 ms and resolution-neutral (#736).
- **B6, mesh shaders:** MSV already took the chain (+0.2-0.4 fps on the Ally, #732).
- **C1 as a "same look" lever on RDNA2 / RDNA3:** no better upscaler there that is also cheaper.
