# renderer/capture-check: the decompiled renderer model against FCAP1 frame captures (package 12, 2026-10-10)

Scope: every structural claim in render-architecture.md section 2, renderer/backend.md, lighting.md, post.md and materials.md
that a D3D12 call stream can confirm or refute. Verdicts: **confirmed**, **contradicted**, **unverifiable** (the capture cannot see it),
**refined** (right in outline, wrong in a detail that matters for a rewrite).

## 0. Data and method

| device | GPU / driver / OS | source | captures |
|---|---|---|---|
| pc2 | RX 6650 XT (RDNA2), Windows D3D12 | pc2 FCAP1 capture, 2026-10-09 (re-run with GPUProf_Markers=1) | 5 (1 load-in + 4 tour points), 2 presented frames each |
| Ally | Ryzen Z1 Extreme (RDNA3, RADV PHOENIX), Proton 11 / vkd3d-proton | Ally FCAP1 capture, 2026-10-10, static Futamata spot, not the tour points | 4, 2 frames each |
| PC | RX 9070 (RDNA4), Windows D3D12 | FCAP1 capture run for this package with `tools\fcap_capture.ps1` and the FC build, 2026-10-10 | 5 (1 load-in + 4 tour points), 2 frames each |

All three run the same ASI (1.7.58, FC build 5281c1e) and the XSYS1 config (1080p output, HIGH, REN_RAT 65, FSR 3.1, FG off).
FC logged 0 dropped records everywhere. In each file the first present is the capture arm point, so **frame 1 (between present 1 and 2) is the
analysed frame**; frame 0 is partial. Tooling: scratch scripts (`ana.py`, `lists.py`, `chain.py`, `post.py`, `hashes.py`) read the CSV
directly; they are not in the repo. Everything below can be re-derived from the CSV columns `t_us, tid, kind, ltype, list, call, ra, info`.

What the capture **can** see: the replay thread's D3D12 call stream with timestamps and caller RVAs, GFX markers (about 100 BeginEvent per frame), PSO shader
digests (vs / ps / cs MD5), viewports, RT counts, dispatch group counts, every ExecuteCommandLists / Signal / Wait / UpdateTileMappings, and the
desc of every resource touched by a barrier / copy / clear. What it **cannot** see: the render graph node ids (only 20 of the ~430 nodes carry a marker),
which RTV is which resource (CreateRenderTargetView is not hooked), and GPU time.

## 1. Headline findings

1. **Render resolution is 1280x720, not 1248x702**, on both pc2 and the Ally at REN_RAT 65 (every G-buffer / HiZ / SSR viewport is 1280x720; VCLOUD dispatches
   20x12x1 groups, not 20x11). render-architecture, gpu-resolution-audit, lighting, cross-system-compare and gpu-volumetrics-rewrite all quote 1248x702.
   All render-res byte / wave / group figures built on it are ~5 % low (pixel count 921,600 vs 876,096).
2. **The DIRECT waits at 8F4583 are not async joins.** They wait on COPY-queue fences (streaming submits at 892E19 / 5C315). The async-compute joins are the 4F9323 waits.
   backend.md section 4.1 and 00-map section 4 label 8F4548 / 8F4583 as "the async joins".
3. **Async compute is recorded late and submitted in the last burst.** The VCLOUD dispatch is replayed at the DrawSky marker (node 267), not at the NatureSky update (node 4);
   every COMPUTE list reaches its queue in burst 1 at ~89 % of the frame, together with the DIRECT lists. There is no early kick on this build.
4. **Post chain order is wrong in post.md section 2.** Real order: FSR3 -> motion blur -> bloom -> compose -> exposure CS -> compose (CA variant) -> heat haze -> exposure / histogram -> UI.
   post.md has exposure first and MB after bloom.
5. **No UpdateTileMappings on DIRECT, ever** (0 of 79+5+1+2+17 calls); all run on a COPY queue from 290FFF. render-architecture 2.1 / P0 / F10 are wrong; backend.md 4.1 already says so.
6. **Meshlet chain size is scene-bound, not a constant ~100 / ~250**: 32-52 dispatch + 12-17 ExecuteIndirect and 191-280 barriers on the pc2 / Ally points, 56-100 dispatch + 15-34 EI and 264-540 barriers on the PC (which also runs HiZ twice per frame; the quoted "14 dispatches" is 2 x 7).
7. The two-burst DIRECT submission, the 18-27 lists per frame, the single engine graphics root signature, the G-buffer shape (7 RTs + D32S8), the marker order and the shader-digest ids are **confirmed on all three GPUs**; the engine stream is identical across RDNA2 / RDNA3-RADV / RDNA4 except for the few device rows in section 8.

## 2. Backend submission (backend.md section 4 / 4.1, render-architecture 2.1)

| # | claim | verdict | evidence |
|---|---|---|---|
| B1 | 21-23 DIRECT ECL per frame, one list each, every list followed by its own Signal | **confirmed** | pc2 18-25 per frame over 8 frames (cap2 frame 1: 19); Ally 18-19; PC 21-27. Each from 4F9114 (PC: the `ecl` rows show ra ffffffff because the OSD hook sits under another layer on that machine; the Signal rows still show 4F9127), Signal from 4F9127, tid = the ECL thread. 2 more per frame from `ra ffffffff` (FFX / present path; at the start of the frame window = the previous present). |
| B2 | submission arrives in **two bursts** at the end of the CPU frame (~89 % and ~99 %); nothing between | **confirmed** | pc2 cap2: burst 1 = 10 lists in 0.3 ms at 23.13-23.42 of 25.92 ms (89 %), burst 2 = 7 lists in 0.2 ms at 25.66-25.88 (99.4 %); first ECL of the frame at 0.4 ms is the previous present's FFX list. Ally: 10 lists at 33.12-33.20 of 37.56 (88 %), 8 at 37.26-37.46 (99 %). PC (9.58 ms frame): 13-17 lists at 7.91-8.03 (82-84 %), 8 at 9.40-9.54 (98-99.6 %). Replay (calls on the dispatcher thread) runs 3.9-25.7 ms, so the GPU sees frame N only after 89 % of its replay. |
| B3 | what falls in each burst | **refined** (new) | burst 1 = every list up to and including the `App_CopyMRLRScene` list (P0..P13 start) **plus all COMPUTE lists**; burst 2 = from `EffectMergeShrinkBuffer` on (effects tail, FSR3, post, UI, VideoOut). On the Ally burst 2 starts one list earlier (`CustomHighlightPass`). |
| B4 | lists are cut at fence / async points, "not at K1" | **confirmed** | cuts on pc2 cap2 fall at: after `MeshletCheckRequestLoad`; before `RenderSkyPatternMap`; between `MeshletPreculling`+G-buffer ph1 and `GPUCulling Setup`; before / after `GenerateSceneHiZ`; before `App_CopyMRLROcnScene`; between `DrawSky` and `DrawScreenSpaceVelocity`; before `EffectMergeShrinkBuffer`; before `DebugFSR3CopyUpscaledDepth_`; before `OffScreenRendering`; before `VideoOut`. The G-buffer phase 1 is **one list** (537 draws + 156 ExecuteIndirect, 195 PSO sets, 219 barriers). Same cut set on the Ally. |
| B5 | COMPUTE: 4-6 ECL per frame on 2-3 queues, from 4F925F, Signal 4F908B, preceded by a Wait token | **confirmed** | pc2 4, 4, 6, 6 per frame, 2-3 queues; Ally 4 per frame (2-3 queues); PC 3, 3, 5, 5 on 2-3 queues. Each queue = a 0-dispatch list, Signal, optional Wait on a DIRECT fence (4F908B), then the work list, Signal. |
| B6 | async slot 3 VCLOUD: 1 dispatch, kicked at node 4, joined at node 267 | **contradicted (position)** | `cs=b76874bb` (VCLOUD) is replayed at 22.80 ms, 0.05 ms after the `DrawSky` marker (22.75), not near the NatureSky update (~4.2 ms). Groups 20x12x1. Its queue has **no Wait on DIRECT** in the frame (free-running, temporal), and DIRECT waits on its fence (4F9323) one list **after** the DrawSky list (before the `DrawScreenSpaceVelocity` list). |
| B7 | async slot 4 VFSCAT: 160x90x64, kicked at node 162, joined at the fog / deferred apply | **refined** | `cs=641524ee` replayed at 21.90 ms (right after `GenerateSceneHiZ`, 21.78). Groups 20x12x16 = 160x90x64 at 8x8x4: size **confirmed**. The compute work list waits on the DIRECT Signal of the G-buffer ph1 list (v=...05), so it cannot start before burst 1 anyway; DIRECT waits on it before the list that starts at `App_CopyMRLROcnScene`. |
| B8 | async slot 6 (ocean) | **unverifiable** here | a third compute queue exists in pc2 cap4/5 and on the Ally, but its dispatches carry no marker or known digest. |
| B9 | DIRECT Waits 7-8 per frame: 8F4583 x3 and 4F9323 x3 (+2 foreign) | **confirmed (counts)**, **contradicted (meaning)** | pc2 per frame: 8F4583 x3, 4F9323 x2-3, foreign x2. The 8F4583 fences are signalled by the COPY queues (`892e19`, `5c315`), the 4F9323 fences by COMPUTE (`4f908b`). So GfxWait 8F4548 = "DIRECT waits for streaming uploads", **not** async joins. backend.md 4.1 and 00-map 4 should swap the attribution. |
| B10 | COPY: 3-4 queues, streaming ECL at 4DB03 / 5C305 / 892E04 | **confirmed** | 4-5 distinct COPY queues; 892E04 = 3 ECL per frame (matches "892D88 3 per frame"), 5C305 = 2-4, 4DB03 = 3-16 per frame in steady state and 146 in the load-in burst. Each followed by a Signal at 4DB1B / 5C315 / 892E19. |
| B11 | frame-begin UpdateTileMappings on DIRECT (CacheFlushOnGfx, token 4 + 2 Signals) | **contradicted** | 0 on DIRECT in all 14 captures (pc2, Ally, PC). Every call is `ltype 3` (COPY), ra 290FFF (CacheTileBatchFlushDirect). pc2: 1 per steady frame, 17 in a streaming frame, 79 in the load-in frame; Ally 0; PC 0-10. The COPY queue (ra 290B91) **Waits on the DIRECT fence** (Signal 4F9127), so DIRECT -> COPY ordering exists, but as a COPY wait. |
| B12 | ECL ring never full, ECL thread = tid of 4F9114 | **confirmed** (structure) | one tid for all 4F9114 / 4F925F / 4F9323 / 4F908B rows (15116 on pc2, 1792 on the Ally); the dispatcher tid (8352) makes every list call. |
| B13 | per-frame list calls: Close ~28, Reset ~28 | **confirmed** | Close 22-31 per frame (pc2), 27-28 (Ally); Reset equal; SetDescriptorHeaps and Set*RootSignature once per list (32-40). |
| B14 | caller RVA of every D3D12 method inside the dispatcher's token handlers | **confirmed**, handed to 01 | see section 7. |

## 3. Render-graph order (render-architecture 2.2; this section checks 2.2 only, 05-frame-graph.md has the node-level view)

Markers present per frame, in replay order, identical on pc2 and the Ally: `MeshletCheckRequestLoad`, `RenderSkyPatternMap`, `MeshletResolveLOD` (+ children 0..N),
`Cascade 0/1/2` (x2), `MeshletInstanceCulling`, `MeshletPreculling`, `GPUCulling Setup`, `GPUCulling`, `GenerateSceneHiZ`, [Ally: `MeshletInstanceCulling` +
`MeshletPreculling` again], `GFM_TextureClear`, `App_CopyMRLROcnScene`, `NUpdate > DrawSky`, `DrawScreenSpaceVelocity`, `GFM_TextureClear`, `App_CopyMRLRScene`, `EffectMergeShrinkBuffer`,
`CustomHighlightPass`, `FSR3:SaveOpaqueColor`, `FSR3` (+ 9 FFX passes), `DebugFSR3CopyUpscaledDepth_`, `OffScreenRendering`, `VideoOut`, `MeshletRequestLoad`.
The G-buffer, SSAO, ESM, GI, deferred shading, SSR, water and transparents have **no marker** (they are inside the "(no marker)" bucket: 1130 draws, 146 dispatch, 173 EI on pc2 cap2).

| # | claim | verdict | evidence |
|---|---|---|---|
| G1 | P0 frame begin: MeshletCheckRequestLoad first | **confirmed** | 3.9 ms, first marker. |
| G2 | P2 LOD resolve (node 14) before shadows; children 0..13 = 14 levels | **confirmed** | MeshletResolveLOD at 4.57 ms before Cascade 0 (4.98). Level count per frame: 11, 10, 14, 10 (pc2), 13 (Ally); max 14 confirmed, light scenes stop earlier. |
| G3 | P4 shadows (VSM + cascades) before P5 culling | **confirmed** for cascades; VSM **unverifiable** | Cascade 0-2 at 4.98-5.60, MIC / MPC at 7.60. VSM pages have no marker. Cascade viewports: 4096x2048 (cascade 0, 1) and 2048x2048 (cascade 2) into a 4096x4096 depth. |
| G4 | P5 main-view culling ph1 then P6 G-buffer ph1 | **confirmed** | MIC 7.60, MPC 7.61-9.0, then 7-RT draws at 1280x720 from 7.75 to 20.4 (MPC's marker scope is still open; the G-buffer is the unmarked work under it). |
| G5 | P7 Hi-Z + phase 2 after the G-buffer | **confirmed** (order), **refined** (counts) | `GPUCulling Setup` = 9 draws down a 512 -> 2 pyramid (20.41), `GPUCulling` 20.45, `GenerateSceneHiZ` **7 dispatches** per run (21.78). render-architecture says "GHDM-FAST, 14 dispatches": **device dependent**: pc2 and the Ally run HiZ **once** per frame (7); the PC (RDNA4) runs it **twice** (14: after the phase-1 MIC / MPC chain at 5.85 ms and after `GPUCulling` at 7.20 ms). Cause not isolated (same ASI, same keys that matter). A second MIC + MPC chain after HiZ (phase 2) is visible on the Ally (marker at 31.10 after HiZ 31.08) and on the PC (5.88 after HiZ 5.85); MIC / MPC chains per frame: pc2 1, 3, 2, 5; Ally 2; PC 2, 4, 4, 5, consistent with "2.7-6 chains". |
| G6 | P8 GBufferEdit after P7 | **unverifiable / possibly contradicted** | the GBufferEdit 4-RT copy shader `7c5acaaf` is first used at 20.28 ms, i.e. **before** `GPUCulling Setup` (20.41) and `GenerateSceneHiZ` (21.78); last use 22.74. Same order on the PC (6.45 vs GPUCulling Setup 6.55) and the Ally. Two GBufferEdit nodes exist (121-145 and 215 / 337), so the first use may be a different node. Needs node ids from 05. |
| G7 | P10 VolumetricFogScatter after P9 GI; P13 DrawSky (267) after the deferred passes | **confirmed** | VFSCAT 21.90 < DrawSky 22.75 < thin-surface temporal filter `449cf0ae` (node 270) 23.25. |
| G8 | App_CopyMRLROcnScene (water) before DrawSky | **confirmed** | 22.48 < 22.75. |
| G9 | P14 transparents / effects, EdgePass, Silhouette / Highlight (383-387) | **unverifiable** except `CustomHighlightPass` | marker at 23.68 after `App_CopyMRLRScene`, before `FSR3:SaveOpaqueColor` (23.81). TOONEDGE2 shaders (a127c98a / acad776d / 7bb9d36d) were never seen as PS digests in any of the 14 captures on any device (edge pass not drawn in these scenes). |
| G10 | P15: FSR3 save-opaque -> upscaler -> post at output res | **confirmed** | FSR3:SaveOpaqueColor 23.81, FSR3 24.46-24.60, post draws all at 1920x1080 or its mips. |
| G11 | FSR3 pass sequence | **confirmed** | GENERATE_REACTIVE, PREPARE_INPUTS, LUMA_PYRAMID (2 dispatches), SHADING_CHANGE_PYRAMID, SHADING_CHANGE, PREPARE_REACTIVITY, LUMA_INSTABILITY, ACCUMULATE_SHARPEN, RCAS. post.md (b)/(c) order holds: reactive first. The 28 embedded CS: 10 dispatches here. |
| G12 | P16 UI: StaticScreenLayout / OldEffect at output res after Apply, then VideoOut | **confirmed** | `OffScreenRendering` (44-108 draws at 1920x1080, a 3840x3240 BGRA8 offscreen target, 64x64 glyph / mask targets) 25.08-25.59, `VideoOut` 25.66 (1 draw), then the next frame's `MeshletRequestLoad`. |
| G13 | tile-map / clear / copy bucket: ~30 copies, ~38 RT clears per frame | **confirmed** | copies 30-74 per frame (pc2), 105 (Ally incl. streaming copies in the list); RTV clears 36 per frame, UAV clears ~25 per frame. |
| G14 | draws+dispatches per frame, "heavy" >= 1600 | **confirmed** | pc2 cap2-5 per frame draws 1217 / 1538 / 2042 / 1404 plus dispatches 188 / 252 / 288 / 291 and EI 185 / 205 / 266 / 514. |
| G15 | 430-node order | **unverifiable** | 20 of the 430 nodes have a marker; GFX markers cannot separate the other ~410. 05 should add markers or use the PSO digest sequence. |

## 4. Meshlet front-end (render-architecture F1, 08-meshlet.md hand-off)

Counted per analysed frame under the marker paths `MeshletCheckRequestLoad`, `MeshletResolveLOD*`, `MeshletInstanceCulling`, `MeshletPreculling`, `GPUCulling*`, `GenerateSceneHiZ`, `MeshletRequestLoad`:

| capture | dispatch | ExecuteIndirect | UAV / other barriers | MIC / MPC chains |
|---|---|---|---|---|
| pc2 cap2 | 32 | 12 | 191 | 1 / 1 |
| pc2 cap4 | 52 | 17 | 280 | 2 / 2 |
| Ally cap2 | 52 | 16 | 270 | 2 / 2 |
| PC cap2 | 56 | 15 | 264 | 2 / 2 (HiZ x2 = 14 dispatches) |
| PC cap4 | 100 | 34 | 540 | 4 / 4 (2 resolves, HiZ x2) |

| # | claim | verdict | evidence |
|---|---|---|---|
| M1 | ~100 dependent dispatches per frame | **refined** (32-100, scene-bound) | pc2 / Ally: 32-52 Dispatch + 12-17 ExecuteIndirect = 44-69 launches. PC cap4 (dense scene, 2 resolves, 4 chains, HiZ x2): 100 + 34 = 134. ~100 is a heavy-scene figure, not a constant. |
| M2 | ~250 barriers per frame | **confirmed** (light) / **exceeded** (dense) | 191-280 on pc2 / Ally, 264 and 540 on the PC. |
| M3 | MPC chain 11 stages, MIC 2 per view | **confirmed** | MeshletPreculling = 9-18 dispatches + 2-4 EI per chain in the same frame; MIC 1-2. |
| M4 | 1360 barriers / frame total (r5) | **confirmed** | PC (the same RX 9070) 1203-1532 per frame, ~1370 mean over the 4 tour points; pc2 1112-1540, Ally 1092-1104 (single-element `ResourceBarrier` calls, `Barriers = 0`). |
| M5 | LOD resolve ~30 stages per resolve, 1.4-2 resolves per frame | **refined** | one MeshletResolveLOD scope per frame, 11-14 level children x (1 dispatch + 1 EI) + ~20 barriers per level (121-151 barriers). The "1.4-2 resolves" is not visible; the capture shows 1 per frame. |

## 5. Lighting (lighting.md) and PSO / material binding (materials.md section 8)

| # | claim | verdict | evidence |
|---|---|---|---|
| L1 | G-buffer = 7 colour targets + D32S8 | **confirmed** (pc2, Ally, PC) | `OMSetRenderTargets` with 7 RTVs (8 per frame, 1280x720) and a D32S8 depth (`R32G8X24_TYPELESS` 1280x720); two 8-RT sets also appear (extra target; possibly velocity / GI). A 640x360 D32S8 exists (half-res depth for AO / GI). |
| L2 | cascade atlas 4096^2, 3-4 cascades, every frame | **confirmed**, format new | 4096x4096 **R16_TYPELESS (D16)**; 3 cascades; VSM pool not touched by any barrier / clear in the window (sparse, no page work this frame). |
| L3 | key-light ESM 2048^2 R32F | **unverifiable** | no 2048x2048 R32 resource was touched. 2048x2048 R8 / R11G11B10F (4 mips, RT+UAV), RGBA16 RT and a D16 depth with 4 mips are. |
| L4 | VFSCAT 160x90x64 froxels | **confirmed** | 20x12x16 groups. |
| L5 | VCLOUD 220 wave64 (156x88 px) | **contradicted** | 20x12x1 = 240 groups (160x90 px = 1280x720 / 8). |
| L6 | stencil-classed ClusteredDeferredShading2Pass x9 | **unverifiable** | no marker, and RTV -> resource mapping is not captured. Full-screen 1280x720 draws between GenerateSceneHiZ and App_CopyMRLROcnScene: ~24 (cap2). |
| L7 | top-down depth 1024^2 | **confirmed** | two 1024x1024 R32_TYPELESS RT resources; 1024x1024 viewports at 20.54. |
| L8 | one graphics root signature for every classic PSO (GfxDevice+6E8h) | **confirmed** | `SetGraphicsRootSignature`: 32 calls, **1** distinct pointer (once per list); +2 from the FFX / present module (PC: +2 +2 from two foreign layers, the engine's pointer still used 34 times). Compute: 1 distinct pointer for 40 calls on pc2 and the PC; **3** on the Ally (35 + 4 + 4: the extra two are FFX-owned layouts, ra other module). |
| L9 | PSO sets far below draws inside G-buffer passes | **confirmed** | pc2 G-buffer ph1 list: 537 draws + 156 EI -> 195 PSO sets (0.28 per launch); PC: 675 draws + 101 EI -> 185 (0.24). Frame: 804 PSO sets for 1217 draws + 188 dispatch + 185 EI (0.5). The Ally issues ~1.3 PSO sets per draw (1777 / 1334): its 1039 dispatches carry 1044 PSO sets in one list. |
| L10 | SetPipelineState only from SynFlushPsoAndBindings (118604) | **confirmed** | graphics SetPipelineState ra = 118737 x1235 (inside 118604..); compute ra = 58BFEF x371, 119C95 x6. |
| L11 | per-draw CBV table via SynUploadCbvTable 118E28 | **confirmed** | SetGraphicsRootDescriptorTable ra 119094 x4211, 118D68 x2006, 118B4D x494 per 2 frames = ~3.4k tables for ~1.2k draws (2.8 per draw). |
| L12 | shader digests are the engine's DXIL / DXBC digests | **confirmed** | all 15 ids quoted in post.md (MB, bloom, compose, exposure, heat haze) appear as vs / ps / cs MD5 prefixes in the PSO sets on all three devices; so do VFSCAT / VCLOUD / GBufferEdit / thin-surface ids from render-architecture. |
| L13 | pass-to-pass shader order P8 < P10 < P13 | **confirmed** | pc2: 7c5acaaf (GBufferEdit) 20.28 < VFSCAT 21.90 < VCLOUD 22.80 < 449cf0ae 23.25; PC 6.45 < 7.30 < 7.73 < 8.00; Ally 30.03 < 31.40 < 32.58 < 33.31. |

## 6. Post chain (post.md section 2) and render-target formats

Measured order of the post draws that follow the FSR3 RCAS (PS digest prefix, viewport), pc2 cap2; the Ally matches except as noted:

| step | digest | viewport | post.md name |
|---|---|---|---|
| 1 | a2bd0e86 / 5fccb795 (DrawIndexed) | 1920x1080 | upscaled-depth copy (`DebugFSR3CopyUpscaledDepth_`) |
| 2 | 615daafd (VS d8fc9faf) | 1920x1080 | MB velocity decode |
| 3 | 76a3c427, a955f6c3 x2 | 960x540, 480x270, 240x135 | MB neighbour max / pyramid |
| 4 | f11afe97, ee6f48b2 | 34x19 | MB tile max |
| 5 | 4e13e236 | 1920x1080 | MB gather |
| 6 | 7139b08f (VS c5909527) | 960x540 | bloom bright pass |
| 7 | b5b4638c | 480x270 | listed in post.md as an *upsample* |
| 8 | 17149dca x6 (Ally x4) | 240x135 .. 7x4 (Ally .. 30x16) | bloom 4-tap down |
| 9 | 18a4c2bb x7 (Ally x5) | 15x8 .. 960x540 | bloom 9-tap blur on the way up |
| 10 | 6a305be4 (VS d21b4b07) | 1920x1080 | compose (light) |
| 11 | CS d7040592, 6ee82656, a1712355 | 1920x1080 | exposure CS (post.md step 2) |
| 12 | 04c1052d (pc2 cap2, 5) or e6efc8ff (pc2 cap3, 4; Ally) | 1920x1080 | compose, CA variant |
| 13 | f3596c47, 9ea0865d (VS d3e89be9) | 1920x1080 | heat haze |
| 14 | d787dbd0, CS 41393a1d | 2x1 | exposure histogram / eye adaptation resolve |
| 15 | `OffScreenRendering` ff5fcc12 / 31f54894 / 5747cd78 ... | 1920x1080 (+64x64, 3840x3240, 1440x810) | UI |

| # | claim | verdict | evidence |
|---|---|---|---|
| P1 | upscaler always first in Apply | **confirmed** | FSR3 24.46-24.60 precedes every post draw. |
| P2 | post order: exposure -> bloom -> MB -> DOF -> flare -> compose | **contradicted** | measured MB -> bloom -> compose -> exposure CS -> compose (CA) -> heat haze -> histogram resolve. |
| P3 | compose is one pass: 6a305be4 / 660f2f60 **or** the CA variants | **contradicted** | both a light compose (6a305be4) and a CA compose (04c1052d or e6efc8ff) run in the same frame, in that order, with the exposure CS between them. 660f2f60 never seen. The CA digest follows the tour point, not the GPU: points 1 and 4 use 04c1052d, 2 and 3 use e6efc8ff on both pc2 and the PC; the Ally spot uses e6efc8ff. |
| P4 | bloom: 4-tap down x4, 9-tap blur, upsample ab11dcd7 x5 | **contradicted (detail)** | down x6, blur x7 on both Windows GPUs (pc2 RDNA2, PC RDNA4, every capture); down x4, blur x5 on the Ally (Proton, every capture), so it is an OS / profile branch, not GPU or scene; `ab11dcd7` never seen; `b5b4638c` is a single pass at 480x270 early in the chain. |
| P5 | MB2 chain order velocity decode -> tile max -> neighbour max -> gather | **refined** | measured decode -> neighbour max (3 pyramid steps) -> tile max (34x19) -> gather. |
| P6 | post runs at output res (POSTEFFECT_RESOLUTION HIGH) | **confirmed** | bloom starts at 960x540 = 1920x1080 / 2; MB gather and compose at 1920x1080. |
| P7 | DOF, lens flare / star, radial blur | **unverifiable** | none of 51dd7477 .. e7318142 / 796e2cdd / b94af32d / 278e3bad appears (features off or not in view). |
| P8 | toon EdgePass (TOONEDGE2) full-screen every frame | **contradicted** at these points | a127c98a / acad776d / 7bb9d36d absent from all FCAP1 frames of the three devices; render-architecture F6 ("full-screen every frame, 0.17 ms") overstates it. |
| P9 | UI after Apply at output res, VideoOut gamma copy last | **confirmed** | see G12. |

### 6.1 Render-target formats actually seen (DXGI numbers decoded; identical on pc2 and the Ally)

Closes part of the "PixelFormat -> DXGI gap" in render-architecture section 1. Surfaces are those touched by a barrier / clear / copy in the window (so a sample, not the whole RT table).

| size | format | count | likely role (by size / usage; the RTV -> resource map is not captured) |
|---|---|---|---|
| 4096x4096 | R16_TYPELESS (D16) | 1 | cascade atlas |
| 2048x2048 | R16_TYPELESS D16 (4 mips) / RGBA16 / R11G11B10F UAV / R8 UAV | 1 each | shadow / ESM class surfaces |
| 1280x720 | R32G8X24_TYPELESS (D32S8) | 2 | main depth + prev depth |
| 1280x720 | **RGBA16_TYPELESS** | **10 RT + 4 RT|UAV** | G-buffer / scene surfaces at 8 B (render-architecture assumes mostly 4 B) |
| 1280x720 | BGRA8_TYPELESS | 3 (+1 UAV, +2 five-slice) | AB / NS / RS class (4 B), two 5-slice arrays |
| 1280x720 | R11G11B10F | 5 (+1 UAV, +1 five-slice) | RR/EM array and scene-class 4 B targets |
| 1280x720 | RG16_TYPELESS | 1 | velocity (4 B) |
| 1280x720 | R32_TYPELESS | 2 RT + 1 UAV + 1 DS | linear depth / HiZ / SSAO |
| 1920x1080 | RGBA16_TYPELESS | 1 | FSR3 output (RGBA16F, 16.6 MB: **confirmed**) |
| 1920x1080 | R11G11B10F | 3 RT + 1 UAV | post / bloom work at output res |
| 1920x1080 | BGRA8 (87) x3, BGRA8_TYPELESS x1, R32_TYPELESS DS | | back buffer (PC: 2560x1440 BGRA8, the monitor mode) + UI + upscaled depth (D32F, no stencil) |
| 3840x3240 | BGRA8_TYPELESS | 1 | UI offscreen atlas (new) |
| 1024x1024 | R32_TYPELESS RT | 2 | top-down depth |
| 640x360 / 960x540 / 480x270 | RGBA16, R8, RG16, R11G11B10F, D32S8 | | half-res AO / GI, bloom, MB |

So: velocity RG16 and the 4 B AB / NS / RS class are **confirmed**; scene HDR is **R11G11B10F or RGBA16** (post.md and render-architecture disagree and the capture holds both at render res);
**14 distinct 8-byte surfaces at render res** were touched (pc2; PC and Ally the same), vs ~1-2 assumed in the traffic estimate (render-architecture 2.3). The 50 MB / 25-30 MB write-set estimate is therefore a lower bound.

## 7. Caller RVA -> token handler (hand-off to 01-backend)

Top caller RVAs per method (pc2 cap4, per two frames; the Ally shows the same RVAs). These are the replay thread's token handlers; the mapping to token numbers is for 01.

| method | callers (count) |
|---|---|
| SetGraphicsRootDescriptorTable | 119094 (4211), 118D68 (2006), 118B4D (494), 4935A3 (32) |
| DrawIndexedInstanced | 116835 (1996), 7A73A5 (92), 87A556 (6) |
| DrawInstanced | 6BE188 (341) |
| SetPipelineState | 118737 (1235), 58BFEF (371), 119C95 (6) |
| IASetVertexBuffers | 11636C (1383), 7A7371 (92), 116484 (66), 494F8E (24) |
| IASetIndexBuffer | 114A9C (1228), 7A7386 (92), 114678 (70), 494FD3 (15) |
| SetComputeRootDescriptorTable | 49586C (370), 495C2B (364), 49555C (250), 4953E1 (23) |
| OMSetStencilRef | 696E6E (834), 494EF4 (32) |
| OMSetRenderTargets / RSSetViewports / RSSetScissorRects | 1182C1 / 11834A / 1183C2 (389 each) |
| ExecuteIndirect | 6ECFF4 (184), 6EB9B4 (166), 58BD13 (24) |
| Dispatch | 58BE1B (361), 119BF2 (6) |
| Begin / EndEvent | 113F51 / 11401B (100 each) |
| CopyBufferRegion | 7A5F02 (44), 7A5DA5 (36), 7F6620 (2), 7F68AE (1) |
| ClearRenderTargetView / ClearUAVUint / ClearDSV | 45935F (72) / 7B1A1F (50) / 4593DF (16) |
| Close / Reset / SetDescriptorHeaps / Set*RootSignature | 490DAF + 3D1A63 / 49224F + 3D18CC / 493500 + 3D1911 / 49352E, 493517 |
| ClearState | 490DA6 (33) |
| DiscardResource / EndQuery | 1A7C467 (24 / 4, other module = Streamline) |
| ResolveQueryData | 3D6447 (2) |

Observations for 01 (callers only; token numbers not derived here): graphics draw-state calls cluster at 114678-1187xx (SynFlushGfxStateForDraw 1180D0 / SynFlushPsoAndBindings 118604 range);
58BE1B (Dispatch) and 58BD13 (ExecuteIndirect) are adjacent, so one compute-record routine; 6BE188 is the only DrawInstanced (non-indexed) caller (341 per 2 frames); 7A73xx (92 per 2 frames)
is a separate small draw path that also owns CopyBufferRegion 7A5Fxx; Dispatch 119BF2 (6) and PSO 119C95 (6) are a second, rare compute entry beside 58BE1B / 58BFEF.

## 8. PC (RDNA4) run and device differences

All three devices were captured with the same ASI and config; the engine stream is the same. Per-device numbers (analysed frame of each tour-point capture; Ally = its 4 spot captures):

| metric | pc2 (RDNA2, Win) | Ally (RDNA3 RADV, Proton / vkd3d) | PC (RDNA4, Win) |
|---|---|---|---|
| frame (present to present) | 24-28 ms | 37-41 ms | 9.6-12.8 ms |
| render res / output | 1280x720 / 1920x1080 | 1280x720 / 1920x1080 | 1280x720 / 1920x1080 (back buffer 2560x1440) |
| DIRECT ECL / frame | 18-25 | 18-19 | 21-27 |
| COMPUTE ECL / queues | 4-6 / 2-3 | 4 / 2-3 | 3-5 / 2-3 |
| COPY ECL / frame (steady) | 5-20 | 5 | 5 (ra field ffffffff) |
| UpdateTileMappings / frame | 1-17 (79 load-in) | 0 | 0-10 |
| first DIRECT burst at % of frame | 89 % | 88 % | 82-84 % |
| draws / frame | 1217-2042 | 1334 | 1400-2066 |
| dispatches / frame (1x1x1 share) | 188-291 (54 %) | 1039 (58 %) | 192-333 (50-55 %) |
| ExecuteIndirect / frame | 185-514 | 287 | 329-785 |
| PSO sets / frame | 804-1164 | 1777-1811 | 865-1264 |
| barriers / frame | 1112-1540 | 1092-1104 | 1203-1532 |
| GenerateSceneHiZ runs / frame | 1 | 1 | 2 |
| meshlet chain dispatch + EI | 44-69 | 68 | 71-134 |
| bloom down / blur levels | 6 / 7 | 4 / 5 | 6 / 7 |
| engine graphics root sigs (foreign) | 1 (+1) | 1 (+1) | 1 (+2 layers) |
| compute root sigs | 1 | 3 | 1 |

Reading it:
- **Engine-side identical** (a new renderer can treat the three as one stream): marker order, list cuts, the two bursts, ECL / Signal / Wait callers (4F9114, 4F9127, 4F925F, 4F908B, 8F4583, 4F9323, 290FFF), 7-RT G-buffer + D32S8, 1280x720 render res, the MB -> bloom -> compose -> exposure -> compose -> haze -> UI post order, no TOONEDGE draw, no DOF.
- **RDNA4 PC differs** in: two HiZ runs per frame (4 MIC / MPC chains in dense scenes, phase 2 after the first HiZ), an earlier first burst (82 % vs 89 %: the replay is ~3x faster relative to the CPU frame tail), one more foreign graphics root-signature layer, and `ra ffffffff` on `ecl` rows (a hook-layer artifact of that machine's setup, not a game difference).
- **Proton / RADV Ally differs** in: no tile mappings in any captured frame (cause not isolated: the cache copy path not exercised, or vkd3d-proton handling of sparse mappings), 2 extra compute root signatures, 4 / 5 bloom levels (a profile branch), ~1.3 PSO sets per draw vs ~0.5, and 3 compute queues in the frame. Its 1039 dispatches (599 of them 1x1x1, all from 58BE1B, in one list before the shadow cascades) are the GPU effect-simulation chain on DIRECT; the same 1x1x1 chain is 100-184 per frame on pc2 / PC (render-architecture F2): shape confirmed, size scene-dependent.
- No RDNA-specific branch in the **call stream** was found: the same shader digests, PSO order and barrier counts (within ~3 %) on RDNA2 and RDNA4. Vendor tuning in the engine therefore lives in shader permutations or the settings profile, not in D3D12 call differences. RADV vs AMD Windows: only the items listed above.

Run notes (PC): a first attempt ended at the title with `Tour: not installed (code at 2239600 differs)`: another installed ASI mod hooks MissionRequestById 2239600, the tour's own hook site. The capture run disabled that mod for its duration. The tour harness should skip or warn when another ASI patches 2239600.

## 9. Open items

- Re-run G6 (GBufferEdit position) and G15 against the node ids per marker in 05-frame-graph.md, with the PSO-digest sequence as a cross-key.
- RTV / DSV handle -> resource: hook `CreateRenderTargetView` / `CreateDepthStencilView` (and `CreateUnorderedAccessView` for UAV clears) in FC to name the RT sets; this would close L1, L6 and the scene-HDR format question.
- Add markers (or a cheap "node id" BeginEvent) around the 410 unmarked nodes for a node-exact check.
- Heavier-scene capture (>= 3000 draws+dispatches) for M1; the XSYS spawn points are medium.
- Ally in a tour-point scene (its tour does not arm after Load Game) for a like-for-like RDNA3 comparison of dispatch counts; the Ally's unmarked 977 dispatches (vs 146) in its Futamata scene are a scene difference until proven otherwise.
