# Same-look package: VSM redraw cap (VSMA) + the engine's update-rate knobs (2026-10-09, ASI 1.7.49)

Package 2 of notes/engine-rewrite-options.md (rank 2 = S1 VSM batch overdraw, rank 3 = E1 amortisation knobs). Built as
default-off ini keys with live menu rows (0 = off, 1 = on, 2 = installed + off), main f84a0ac, bridge file #154. Code:
mod/Nioh3EngineFix/src/samelook.{h,cpp,asm}; offline test tests/run_samelook_harness.py; hands-free A/B
tools/testmatrix/samelook_ab.ps1 + samelook_report.py.

## Verdict table

| knob | what it does (system) | measured ms | visual verdict | keep / drop |
|---|---|---|---|---|
| VSMA (VSMA_K 2, Front 2) | VSM tile path: waiting tiles past the marked + next 16 are batch-drawn on 1 visit in 2 instead of every visit | PC (RX 9070, 1440p HIGH) and pc2 (6650 XT, 1080p HIGH): **0 % of batch tiles skipped** - every visit had <= budget dirty tiles (PC 3-8, pc2 3-8 per visit; 0 past the budget), so VSMA never acted. Movement runs on-off: PC p99 -0.03 ms (se 0.05), GPU busy +0.01 (se 0.01); pc2 p99 -0.35 (se 0.37), GPU -0.12 (se 0.25) = noise. Ally: queued (#836), not back yet | nothing changed where measured (inert); by construction no unmarked tile is ever sampled | **keep default-off as an Ally candidate**; drop if the Ally also shows ~0 % skipped |
| LPVR (LPVUpdateFrequency 1 -> 4) | env field of the LPV GI volume | PC +0.010 ms (se 0.006), pc2 -0.058 (se 0.088) | shot = noise floor (PC mad 4.01 vs 4.10, pc2 4.21 vs 4.81) | **drop**: no per-frame LPV work exists to amortise |
| SKYR (SkylightAmbientUpdateFreq 1 -> 4) | env field of the sky-ambient (MakeSkyAmbient) update | PC +0.016 ms (se 0.004), pc2 +0.097 (se 0.105) | slightly above the floor on PC (mad 4.48 vs 4.10, SSIM 0.894 vs 0.906), at the floor on pc2 | **drop**: no saving; the pass is already time-sliced |
| FOGT (VolumetricFogTemporalFactor 0 -> 0.5) | froxel fog history blend weight (not a rate) | PC +0.023 ms (se 0.005), pc2 +0.122 (se 0.120) | **visible**: above the floor on both (PC 23.1 % px vs 16.4 %, pc2 27.4 % vs 22.7 %): fog history smear | **drop**: a blend weight, costs a fetch, changes the look |
| CLDR (VolumetricCloudReprojectionBlendWeightCurrentFrame 0 -> 0.5) | cloud history blend weight (not a rate) | PC +0.036 ms (se 0.025), pc2 -0.086 (se 0.106) | shot = noise floor (PC 3.80 vs 4.10, pc2 4.46 vs 4.81) | **drop**: a blend weight; the cloud ray march is fixed |

## Measurements (2026-10-09, hands-free tour A/B, tools/testmatrix/samelook_ab.ps1)

One launch per machine, tour mode (saves blocked), tourlist_cvl.txt (4 stage spawns, 1 point) x 4 passes = 16 dwells. Every
row is a live menu row switched through the VirtualPad pipe (`menu set`), so A and B run at the same spots in one launch:
- VSMA per stage load: [0,1,1,0][pass] xor (stage & 1) (each stage arrives twice on, twice off) + arrival shot;
- in each dwell: knob cycle on the static spawn view (BASE, LPVR, SKYR, FOGT, CLDR, BASE, order reversed on odd passes:
  settle 1.5 s, 3 s PresentMon window, screenshot), then 4 stick runs (2.5 s forward + 2.5 s back) with VSMA 0/1/1/0.
- Knob values: LPVR_N 4, SKYR_V 4.0, FOGT_V 0.5, CLDR_V 0.5. VSMA_K 2, VSMA_Front 2.

| machine | session | notes |
|---|---|---|
| PC (RX 9070, 2560x1440 REN_RAT 75 XeSS, HIGH, uncapped ~160 fps) | captures/pc/samelook/20261009-1324-pcsl1/samelook_report.md | 16 dwells, 49 shots, 27.8k frames per VSMA state while moving |
| pc2 (RX 6650 XT, pc2-bench 1080p HIGH FSR 65, ~27 ms GPU) | captures/pc2/samelook_157/samelook_report_tzfix.md (bridge #853 / file #157) | run by pc2; PresentMon clock re-aligned (+14 h, pm_time.py) |
| Ally | queued, bridge #836 (ASI file #154) | the main VSMA target; pending |
| PC, SHADOW_QUAL LIGHT stress (budget 2) | captures/pc/samelook/20261009-1356-pcsl2light (aborted after 2 dwells) | still 1 dirty tile per visit, VSMA 0 % skipped: stopped to free the PC |

Findings:
1. **The game's own values are confirmed in S** (first-frame log on PC and pc2): LPVUpdateFrequency 1,
   SkylightAmbientUpdateFreq 1.0, VolumetricFogTemporalFactor 0, VolumetricCloudReprojectionBlendWeightCurrentFrame 0, with
   VolumetricFogResolutionX/Z 160/64 and SSAORayNum 7 at HIGH as the offset check. The engine re-fills LPVR / SKYR / CLDR
   every frame (refilled == written) and FOGT only when the fog block reloads, so the override is written every frame.
2. **No knob saves GPU time.** All four deltas are within ~2 se of zero or slightly positive on both GPUs (the static
   spawn views are GPU-bound on pc2). FOGT and (on the PC) SKYR change the picture above the BASE-vs-BASE floor. That
   matches the decompile: FOGT / CLDR are history blend weights, not update rates; the LPV has no per-frame GPU pass; the
   sky ambient is already sliced. **E1 is not a lever: drop all four.**
3. **VSMA never had anything to do on the PC or pc2.** The stats line shows VsmClipmapRender 0.5-1.0 calls/frame but only
   3-15 clipmap visits/s with dirty tiles, and every visit had <= budget (8) dirty tiles (0 tiles past the budget, 0 skipped)
   - at HIGH and at LIGHT (budget 2: 1 tile per visit). The redraw backlog that S1 measured comes from the Ally's logs
   (GP "Batch Tiles 8" 2.7-2.9 full batches per frame at 0.6 VSM calls/frame = 36-62 tiles per call in 5 of 89 windows):
   large invalidations (most likely a sun/sky direction step that dirties every tile, or a teleport at ~30 fps), which the
   tour + stick runs did not reproduce on the fast GPUs. The movement-run p99 differences above are noise for that reason.
4. Arrival shots: the same-spawn noise floor is high (idle pose, wind, time of day; one stage has on-vs-on 33 mad), so the
   arrival comparison is inconclusive by itself; with 0 tiles skipped the VSMA-on frames were bit-for-bit the stock
   shadow path anyway.

What is still needed: the Ally run (#836). Wanted from it: the `stats/s VSMA (switch ON)` lines (how many tiles per visit
and the skipped share) and the movement p99/p99.9. If the Ally also shows ~0 % skipped in tour play, VSMA is dropped; if it
skips during its bursts, judge the shots at those spots (a missing / blurry shadow that persists after the burst = drop).


## 1. VSMA: what the engine does and what the cap changes

Decompile (v2.02, VsmClipmapRender 1F8340, one clipmap per call, called from SceneRenderView at 538F62; Ally GP: 0.1-0.6
calls with work per frame, one "Clipmap N" each):
- Clear loop: every dirty (41720 bit 0) + requested (40118 bit 2) tile of this clipmap is cleared; empty ones are marked.
- Batch loop (1F8A91..): for every such non-empty tile `budget = VsmTilesPerFrameBudget 1F7960` (8 at SHADOW_QUAL
  STANDARD..HIGHEST, 2 at LIGHTEST/LIGHT, node param 93BCE2C1) is only tested for != 0, the tile is bound
  (VsmTileBindViewport 1F96E0) and appended; every 8 tiles one ShadowCascadeRender 1F5C2C batch (culling 23584C + draws).
  **No budget cap here.**
- Per-tile loop (1F9228..): the first `budget` bound tiles get their non-batchable items drawn and are marked valid
  (149468(..,1) -> 14951C: 40118 |= 3). So with T dirty tiles a visit draws T tiles and validates 8; the rest are cleared and
  drawn again on every visit of that clipmap until their turn: T + (T-8) + (T-16) + ...
- Nothing samples a tile without bit 0 (all 356 VSM consumer variants gate each tap, shadows-redundancy S1), and the next
  visit clears it again before drawing: the extra draws are never seen as texels.
- But they are not useless (shadows-redundancy 1b): the batch PSOs write sampler feedback (texture streaming) and the batch
  culling writes meshlet cache codes / request flags. The redraws of waiting tiles are the streaming warm-up of content that
  is not on screen yet (casters behind the camera), so dropping them all (the old VBC idea) could mark tiles valid with
  coarser mips / missing meshlets that then stay until the tile's lifespan refresh = "stale/wrong" shadows. Not built.

VSMA instead (samelook.cpp):
- the first `budget` distinct tiles bound in the visit (= exactly the tiles the per-tile loop marks: same order, same
  deterministic bind, duplicates counted once like the per-tile loop's dirty test) -> stock;
- valid tiles being refreshed (bit 0 set: lifespan 10h / stamp 20h; they are sampled meanwhile) -> stock;
- the next VSMA_Front x budget waiting tiles in the engine's order (marked within the next VSMA_Front visits) -> stock, so
  their warm-up before marking is unchanged;
- every other waiting tile: drawn on visits where (visit_of_its_clipmap + tile_index) % VSMA_K == 0, so it is never skipped
  K-1+ visits in a row and the redraws spread evenly (no new burst). VSMA_First = 1 also draws each tile's first waiting
  visit (keeps every first request on time; then the first-visit spike stays as stock);
- the whole call is stock when node param 37B3FAE9 (8F041C; level data, default -1) is >= 0: the engine compares it with a
  draw count that includes the batch draws (sub-light shadow throttle 542C08).
- A skipped tile is simply not bound: the engine's own bind-failed path (nothing appended, no batch, clip+398/39C still
  written as stock). Sites: VSMA1 538F62 (per-call reset, owner thread), VSMA2 1F8AB2 (budget call: throttle lookup on the
  same param table), VSMA3 1F8AF2 (bind; samelook.asm passes rbx = tile, r14 = page manager, edi = budget; deps-checked).

Offline (tests/run_samelook_harness.py, 3 bursts of 62 tiles, budget 8): K 2 / Front 2 = -19.5 % batch tiles, peak visit 45
vs 64 tiles (-30 %); K 3 / Front 1 = -37 %, peak 34 vs 64; First = 1 = -12.8 %, peak unchanged. Invariants checked: every
marked tile drawn on its visit, valid / front tiles always drawn, no waiting tile skipped K visits in a row, K = 1 / switch
off / throttle >= 0 = the stock sequence.

Expected size: shadows-redundancy S1 put the redundant share at 0.12 ms/frame (frame-weighted) / 0.36 ms (window mean) on
the Ally and 2.8-5.7 ms in the movement-burst windows; VSMA removes roughly a third to a half of that (it keeps the warm-up
front), i.e. ~0.05-0.15 ms mean and ~1-2.5 ms of the burst windows, on the Ally. On a GPU path that does not use 1F8340
(VSM mode 6) it is inert; the stats line shows the calls/frame.

## 2. The four update-rate knobs (E1)

All four are environment RenderConfig fields (re/out/renderconfig_fields.tsv, offsets in RC; RenderParams copies at +0x2F4
for the fog block), identical in all 62 env presets. An exe-wide Ghidra operand scan (every [reg+disp] with these offsets,
both RC and RenderParams shifts) found no reader outside the field-by-field copies (RenderParamsAssign 247950,
GfxViewParamsCopyConstruct 4FC330, the env-preset blend 7124DC, RenderConfigCtor 6C1ED4): the consumers, if any, read them
through copied blocks (cbuffers / param structs), so the semantics below come from the passes that use them.
- **FOGT, VolumetricFogTemporalFactor (RC+13C0, f32, env 0).** It is a history blend weight, not a rate. The VFSCAT shader
  (gpu-shaders/fog/vf235_src.hlsl, gInjectionDesc in cbuffer VolumetricFogConstBuf, filled by 18FAD4 from the fog param
  block, param[8]; forced to -1 on a camera cut) computes every froxel every frame, then if gInjectionDesc.y > 0 samples the
  reprojected history (sVolumeSrc via gPrevW2P) and outputs cur + y * (hist - cur). A non-zero value cannot save any work
  (it adds a 3D fetch per froxel) and makes the fog lag moving lights (torches, spells). Not a same-look lever by construction.
- **CLDR, VolumetricCloudReprojectionBlendWeightCurrentFrame (RC+7C0, f32, env 0).** Also a blend weight. VCLOUD is already
  a fixed render/8 dispatch with a 1/16 Bayer temporal update + reprojection (gpu-volumetrics-rewrite.md); a weight cannot
  change the ray-march cost, only how fast new cloud samples replace history.
- **LPVR, LPVUpdateFrequency (RC+1394, u32, env 1).** No LPV inject / propagate pass appears in any GP2 capture (Ally
  S1_gp2_all, pc2 #125/#140, PC r5): the LPV volume sampled by fog / GI is not rebuilt per frame on the GPU in the shipped
  graph, so there is no per-frame work for a lower rate to remove.
- **SKYR, SkylightAmbientUpdateFreq (RC+144, f32, env 1).** The SkyAmbient node (145C60) is already time-sliced by its own
  RenderParams fields: mode RP+3C8 (0 every frame, 1 sliced while the sky object exists, 2 forced), 43 steps spread over
  RP+29C seconds (the RenderParams builder 254B48C copies source+148 = SkylightAmbientInterpolateSpan there), reset by
  SkyAmbientDetectChange when the sky changes. The pass is not in any GP2 top-100 (< ~0.03 ms).

The knobs are written into the frame's RenderConfig S (rcx of the renderer vfunc +4B8h at 39B7CA, right after
ApplyGraphicsSettings); the first frame logs the game's values and a few known fields as an offset check.

## Files
- mod/Nioh3EngineFix/src/samelook.{h,cpp,asm}, patches.json groups VSMA / RCO (+ deps), ini [Patches] VSMA LPVR SKYR FOGT
  CLDR, [Tuning] VSMA_K VSMA_Front VSMA_First LPVR_N SKYR_V FOGT_V CLDR_V.
- tests/run_samelook_harness.py + samelook_harness.cpp.
- tools/testmatrix/samelook_ab.ps1 (PC / pc2), samelook_report.py.
