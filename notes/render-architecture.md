# Render architecture audit: frame graph, inefficiencies, algorithms, ranking (2026-10-09)

Scope: the whole frame, not single shaders. Target: pc2 (RX 6650 XT, RDNA2 gfx1032, 32 CU, 32 MB Infinity Cache, 1080p HIGH,
FSR 65 = 1248x702 render, ~30 ms GPU in real levels). Cross-checks: the RX 9070 RGP trace r5 (gpu-profile.md, 1080p render)
and the Ally (ally-gpu-budget.md). Static work on this PC; the game was not launched here.

Tools (new): `re\scripts_gpu\frame_graph.py` (render graph + RT table -> ordered pass list with the textures each node binds,
sizes, formats, bytes), `re\scripts_gpu\arch_budget.py` (GP2 rows -> architecture families, heavy vs light windows).

## 0. Verdict (short)

**The architecture is reasonably efficient except for one block.** The deferred / post / upscale chain, shadows, GI and
volumetrics are each either necessary work or already amortized (VSM page cache, 1/16 temporal clouds, async fog/clouds,
half-res GI/SSR, GPU-driven meshlets with two-phase occlusion). At 65 % render scale the screen-space passes are **not
bandwidth-bound**: a 4-byte render-res target is 3.5 MB and fits the 6650 XT's 32 MB Infinity Cache, so fusing full-screen
passes or slimming intermediates recovers tenths of a ms at most.

**The one architectural inefficiency is the GPU-driven meshlet front-end** (LOD resolve + per-view instance/meshlet culling +
Hi-Z): ~100 dependent compute dispatches and ~250 UAV barriers per frame on the graphics queue. In the real-level windows on
pc2 it costs **6.5 ms of a 29.9 ms frame (22 %)**, against 0.92 ms of 10.7 ms (8.6 %) on the RX 9070 and 0.58 ms in pc2's
light windows. It scales ~11x with scene density while ordinary passes scale 2-3x: it is latency/dependency-bound, exactly the
"many cheap passes collectively inefficient" pattern. The same-picture fix is algorithmic (persistent LOD traversal, single-pass
scans, multi-view batched culling, async overlap): **2-3.5 ms recoverable on pc2 in dense scenes, but weeks of engine-level
work**, not an ASI patch. Section 4 / 5.

Second open question: **async compute may contend rather than overlap** on RDNA2 (VCLOUD wall time 0.46 -> 2.51 ms from light to
heavy windows with a fixed dispatch). Queued as a zero-code A/B (section 6).

## 1. Data and caveats

- **pc2 GP2 tour (#126 raw log, Phase A, 13 heavy + 21 light windows).** The tour mixes spawn points in real levels (25-34 ms
  GPU; PresentMon GPUBusy 30-32 ms at the dwell, e.g. 00:54:26-00:54:38 p50 frame 32.2 ms, GPUBusy 30.6 ms) with warp points,
  several of which sit outside the level (8-14 ms). The 23.2 ms "mean span" of the #125 table is an average of both and
  represents neither. This note uses **heavy = windows with >= 1600 draws+dispatches/frame** (mean span 29.9 ms) as the
  gameplay frame; it matches the human play session (#441: GPUBusy 26-29 ms, 37 fps).
  - **Tour report bug:** `tour_report.md` prints fps = 120 / p50 8.3 ms for every point, while PresentMon in the same dwell shows
    31-33 ms frames. The tour's frame counter is not the presented frame rate (reported to the tour owner; do not use its fps).
- GP2 `MinRun = 4` bills short PSO runs to the first PSO of the interval (gpu-rewrite-round3.md section 1): family totals are
  right, per-kernel splits inside the meshlet chain are not. The running VCLOUD A/B (#509) uses MinRun = 1 and will give the split.
- gpu-unattributed.md (310c71a): the "4 ms unattributed" is mostly accounting (+4.89 ms sub-cut rows, -3.07 ms async rows counted
  serially); ~2.15 ms of the DIRECT queue is outside stamped spans with whole-GPU idle <= 0.27 ms, so >= 1.8 ms is busy
  (list-head barriers / transitions, or DIRECT waiting on async); GP2 overhead ~+0.4 ms/frame. **The frame is GPU-saturated, not
  idle-bound.** Per-transition cost and async contention are therefore real levers, not bookkeeping.
- RT formats: the RT table (system 0x93fb3280) stores KTGL's own PixelFormat enum; the DXGI mapping table was not located
  (searched the dump; the PSO path 9290D8 already receives DXGI bytes). Bytes/pixel in this note are inferred from usage
  (02 = 4 B R32F-class: ESM, prev depth; 03 = 8 B HDR scene; 04 = 16 B exposure 2x1; depth 'l' = D32S8 ~5 B; others 4 B) and
  can be off by 2x per target. Traffic numbers are upper-bound screen-space estimates (one full read/write per binding).

## 2. Reconstructed frame graph

Source: the main render graph `system 0x102dd706.kidsrender` (430 nodes; the root graphs 0x05c37956 / 0xce279af4 / 0xd813e276
are the same). Node order = execution order of the graph walker (113204, render-nodes.md). BeginScene/EndScene nodes set the
render-target scope; `BeginCheckSkip`/`EndCheckSkip` gate optional blocks per view/feature. Per-node detail (every bound texture,
size, format code, bytes): `py -I re\scripts_gpu\frame_graph.py --md`.

### 2.1 Queues

| queue (D3D12) | created by | carries per frame | sync with DIRECT |
|---|---|---|---|
| DIRECT (GfxQueue, device +4270h) | 2EE9DE4 | 21-23 command lists (cut at fence points, not at K1), everything below unless noted | - |
| COMPUTE slot 3 (CMP35) | AsyncSlotsCreate 32FA450 | VCLOUD ray march `b76874bb` (1 dispatch, render/8 = 156x88 px, 220 wave64) | kicked at node 4 (NatureSky update phase, producer 5F328D), joined at node 267 (NatureSky draw = DrawSky marker) |
| COMPUTE slot 4 (CMP36) | same | VFSCAT froxel scattering `641524ee` (160x90x64 at HIGH, ~14k waves) | kicked at node 162 (producer 14906E), joined at the fog/deferred apply (P11-P13) |
| COMPUTE slot 6 (CMP37) | same | ocean PFB / IGP / BWVM (NUpdate) | joined at App_CopyMRLROcnScene (water) |
| COMPUTE slot 0 | (off on AMD/Intel; AS item) | effect GPU simulation chains (on DIRECT instead: ~0.3 ms of 1x1x1 dispatches) | joins EffectViewPrepare / RefProbe |
| COPY x3+ | 2EE9EE8, 2F110A8, 2F13350 | texture streaming (DMA1-3, 50-1500 us lists), tile-mapping cache (UpdateTileMappings) | CPU-polled; frame-begin tile updates go on DIRECT (CacheFlushOnGfx 2EF0938 x2: token 4 + Signal) |

### 2.2 DIRECT queue, in order (pc2 heavy-window ms from GP2/GP1; r5 = RX 9070 us; traffic = render-res bytes touched)

| phase | graph nodes | engine code / GP1 marker | reads | writes | pc2 heavy ms | r5 us | traffic est. | barriers / sync |
|---|---|---|---|---|---|---|---|---|
| P0 frame begin | - | RenderFrameBegin 4A529C: CacheFlushOnGfx x2 (UpdateTileMappings + 2 Signals), ~32 ClearUAVUint (streaming, 1284E8), MeshletCheckRequestLoad / RequestLoad | streaming feedback | page flags, request lists | 0.27 + 0.07 (+ clears in copy bucket) | 57 | buffers, <1 MB | list head: tile maps (paging op) |
| P1 setup | 2-13 | UltraMarine prep, SDF, NatureSky update (**VCLOUD kick**), RainDrop, ClusterDecalCulling x2 (slot 7 off), SkyAmbient, GVD, Tree2Wind (slots 11/12 off), ComputeSkinning, SimulationMeshUpdate | depth (prev), GBufferOC | skinned VBs, wind/grass buffers | ~0.3 (GVD 0.16) | ~60 | small | UAV barriers per dispatch |
| **P2 LOD resolve** | **14** MeshletResolveLODPass (shader 0x7ed01e3f) | marker MeshletResolveLOD, children "0".."13" = LOD levels; MshLod#0/#1 instance pass, then per level MshLod#3 (1x1x1 arg init) -> MshLod#2 (ExecuteIndirect dispatch, one thread per cluster payload) | instance / G1M / page-flag buffers (bindless), camera cbuffer | per-level payload lists (atomics), cache codes | **3.24 incl** (1.46 instance pass + 0.77 level 0 + 0.35 + 0.21 + 0.13 + ...); 1.4-2.0 resolves/frame | 225 | buffers only (tens of MB of pointer-chasing loads) | **~30 dependent stages per resolve, UAV barrier after each** |
| P3 top-down depth | 15-23 | view ViewID 5 into `c45d20d9` 1024^2 depth; SnowDeform; TerrainCopyToTextureAtlas | - | 1024^2 depth | small | - | 4 MB | - |
| **P4 shadows** | 24 VirtualShadowMapPass (465384/465414), 25-28 cascade atlas, 29 LightPass | VirtualShadowmap / Clipmap / Batch Tiles; Cascade 0-2; GenerateShadowMapHiZ; MIC/MPC with `tValidRectCountCumulativeMap` | VSM page table, valid-rect SAT, Hi-Z | VSM pool `a6861e5b` 4096^2 x16 (sparse, only invalid pages), cascade atlas `ea36c27d` 4096^2 (**full depth clear every frame**, HTILE fast clear), local-light planar/cube arrays | 2.02 (SM 0.62, cascades ~0.25 each, ESM 0.21, VSM tiles 0.44) | 232 draws + 63 culling + 36 HiZ | atlas clear 67 MB nominal (metadata only) | two-phase occlusion: draw -> Hi-Z -> redraw rejects |
| **P5 main-view culling ph1** | 30-35 (35 = MeshletResolveLODPass, cull-only: MIC 0xdd38b728 + MPC 0x49826c5b) | MeshletInstanceCulling, MeshletPreculling (11-dispatch MPC chain), cluster decal / lasmap culling | prev-frame Hi-Z, LOD lists | draw args, index stream | (MIC + MPC across all views) **2.6** | 120 | buffers | 11 dependent stages + 2-5 UAV barriers each (5301FC) |
| **P6 G-buffer ph1** | 36-100 | views 39/40, 46-48 (terrain), **62-64 main G-buffer**, 69-71, 76-78, 83-86, 91/97 StaticTerrain (PB2LS tessellated), 95-99, OldEffect 98 | velocity (prev), refraction depth, material textures | **AB `8b18ce44` (4 B), NS `7e94c315` (4 B), RS `521c7ded` 5-slice (4 B), RR/EM `58d0a6f2` 5-slice, velocity `62590470`, depth+stencil `e13ddb4f`** | **6.33** (whole G-buffer family incl. ph2; 663 draws: ~129 meshlet EI groups x2 phases + 533 direct) | 1630 | ~50 MB bound at 702p (57 B/px if every slice is written; MRT max 8 so a draw binds a subset) | RT/DSV transitions per scope |
| P7 Hi-Z + ph2 | 101-120 | MeshletGenerateHiZMap (GenerateSceneHiZ = GHDM-FAST, 14 dispatches), phase-2 views 105-117 (rejects re-tested) | depth | Hi-Z mips, G-buffer | 0.15 (+ ph2 draws in P6 figure) | 81 + | 4-5 MB | depth -> SRV transition |
| P8 G-buffer edits | 121-145 | GBufferEdit (4-RT copy `7c5acaaf` AB/NS/RS0/RS1 -> GBufferEdit*, then decals / snow / screen map), PrepareAccumulatedSnow, **DeferredIndirectPass 134** (sun shadow resolve WDO#14 -> mask, WDO#28 blur -> DirectOcclusion), SDF, Miasma decal, Ripple | G-buffer, CSM atlas, VSM, sub shadow maps | GBufferEdit copies (4 x 3.5 MB), direct-occlusion slice | deferred/shadow resolve family 1.16; decals 0.24 | 224 copy + 120 + 367 | ~30 MB | copy = hazard copy (read original while editing live) |
| P9 AO / ESM / GI | 146-160 | SSAO (HBAO lin-depth, half-res, main, blur, temporal), **DeferredIndirectPass 149** (ESM 2048^2 gen + 7-tap V/H blur, LPV inject/propagate), **ReflectionProbePass 150** (#121 diffuse x2 stencil groups, #414 specular x2), Denoiser 151, OBOROExport 159 | G-buffer, depth, OBORO probe field, SSR result | AO, ESM 2048^2 R32F (16.8 MB), RR array slices | GI family 1.35 (RefProbe 0.80, GFM 0.25, HBAO 0.13, SSR 0.12); ESM 0.21 | 707 + 297 | ESM 2 x 16.8 MB; GI ~25 MB | group-1 -> group-2 drain (3 barriers) |
| P10 light culling + fog kick | 161-162 | ClusterLightCulling (= EffectRenderNodeExecute slot; effects join), **VolumetricFogScatter -> async slot 4** | depth, light lists, ESM, VSM, LPV | froxel volume (async) | (fog async 1.27 wall) | 293 async | froxels 160x90x64 x 8 B = 7.4 MB | GFX Signal -> CMP36 Wait |
| P11 deferred shading | 163-189 | ClusteredDeferredShading2Pass x9 (167-186: light groups / stencil classes, 54762C), SnowScattering, SSSS | G-buffer (AB/NS/RS/RR/D1), shadow arrays, AO, GI | scene `064b746b` (4 B) + 2 extra targets (184-186) | in deferred family 1.16 | 210 + ~250 | each pass reads ~15-25 MB, writes 3.5-7 MB | stencil-masked, same pixels read per light group |
| P12 forward / 2nd view | 190-220 | ForwardScreenMap, OldEffect (effects), views 197/198, PrepareStaticNatureReflection, HiZ 203, terrain 206, DeferredIndirectPass 209, deferred 212/217, GBufferEdit 215, CombineHDR 219 | | scene HDR | effects 1.22 (G1E ~0.6) | 280-394 | | effects joins (slot 0 when on) |
| P13 SSR / water / sky | 221-300 | MergeRLR2 x3, CopyColorRenderTarget 235 (scene -> `f07c5979` render/2 refraction copy), UltraMarine/StaticNature (SSFOG), water-view RefProbe 243-249 + DeferredIndirect 252, **NatureSky 267 = DrawSky: VCLOUD join + sky draw**, PrepareThinSurfaceBSDF 270 (temporal filter `449cf0ae`), RealtimeLocalReflection 290 (SSR trace, mips), RefProbe 292, **MeshletResolveLODPass 299 (water-view culling, Flags 2)** | scene, depth, velocity, prev RLR | RLR, refraction, sky | water 0.25 (+0.35 async), sky draw 0.13, SSFOG 0.14, PBRLR 0.12 | ~700 | ~60 MB | **DIRECT waits on VCLOUD at 267** |
| P14 transparents / water / effects | 300-398 | PrepareThinSurfaceBSDF 300, forward views 302-311, UltraMarineEffectGBufferEdit, water views 318-327, RainDrop x3, GBufferEdit 337, RefProbe 338/348, deferred 340-346 (water), effect views 354-374 (G1E particles), **EdgePass 370/376 (TOONEDGE2, full-screen every frame)**, Silhouette/Highlight 383-387, PatternedOcclusion 395 | | scene HDR | TOONEDGE2 0.17, Highlight 0.08 | ~500 | ~40 MB | |
| P15 upscale + post | 399-409 | SSFadeSaturation, CopyColorRenderTarget 406, HdrOutput 408, **PostEffect::Apply 409**: FSR3 (prepare inputs/reactivity, luma pyramid, accumulate+sharpen, RCAS) then post at OUTPUT res (POSTEFFECT_RESOLUTION HIGH): bloom down/up, motion blur, DOF, compose; CustomDepthUpscale | scene, depth, velocity, history | 1920x1080 color (RGBA16F 16.6 MB), history | FSR3 1.37, post 1.05 | 810 + ~600 | ~150 MB at output res | serial chain |
| P16 UI / output | 410-429 | OldEffect, Fade, CopyColorRenderTarget 417/422/427 (SelfTiming 2/4/3), StaticScreenLayout 418/424 (AMBSL UI quads), HdrOutput 421/428 (VideoOut gamma copy) | | back buffer | UI/output 0.69 | ~300 | ~30 MB | present transition |

Copies / resolves / clears (GP2 bucket, heavy windows): **2.25 ms, 30/frame**, but the bucket also bills up to 3 preceding draws
(MinRun 4). Known real copies: GBufferEdit 4-RT copy (P8), CopyColorRenderTarget 235/406/417/422/427, MRLR scene copies
(App_CopyMRLRScene 0.036 + OcnScene 0.022), FSR SaveOpaqueColor 0.016, VideoOut gamma copy (output res, 0.07-0.11), ~38 RT clears
(r5: 104 us total). The MinRun = 1 run splits the rest.

### 2.3 Memory traffic (estimate) and why it is not the bottleneck

- Render-res surfaces: 1248x702 x 4 B = 3.5 MB; the G-buffer scope binds AB + NS + RS(5) + RR(5) + velocity + D32S8 = ~50 MB if
  every slice were written (a draw binds <= 8 RTs; the 5-slice arrays are shared between material data, emissive and the GI
  output, so ~25-30 MB is the realistic write set).
- Bounds: summing every graph node's bound screen-space surfaces (frame_graph.py; shadow/probe pools excluded) gives 5.9 GB, but
  that counts skipped blocks (most `BeginCheckSkip` views do not run every frame) and binds that touch nothing (GBufferEdit nodes
  without edit draws). The executed full-screen work is ~60 passes per frame (GP2 heavy rows) x 10-20 MB = **~0.6-1.2 GB per
  frame**, i.e. 20-35 GB/s at 30 fps against 280 GB/s DRAM, with most render-res working sets (10-25 MB) inside the 32 MB IC.
- What fusion could actually remove is the *redundant* re-reads: the 9 ClusteredDeferredShading2Pass nodes re-read G-buffer
  surfaces, but each is stencil-masked to its light group, so the overlap is a fraction of 8 x 15 MB; worst case (all DRAM) 0.4 ms,
  realistic 0.1-0.2 ms. Post at output res: bloom down/up targets are small mips; compose is already one pass.
- The big byte movers are sparse: shadow-map pools (VSM 4096^2 x16 physical pages, only invalid tiles), material textures in the
  G-buffer pass, and probe/OBORO fields. None of them is an intermediate that fusion could remove.
- **Pass fusion for bandwidth is a <= 0.2-0.4 ms lever at 65 % render scale**, smaller than the noise of most launch A/Bs.

## 3. Inefficiency findings (architecture level)

| # | category (user's list) | finding | size on pc2 (heavy) | status |
|---|---|---|---|---|
| F1 | dependency chains that serialize work | **Meshlet front-end**: LOD resolve (~30 stages/resolve, 1.4-2 resolves/frame) + per-view MIC + 11-stage MPC chain (2.7-6 chains/frame) + Hi-Z (14 dispatches) + streaming feedback. Every stage is a dispatch followed by 2-5 single-resource UAV barriers (5301FC records tok 0x8A per written buffer). ~100 dispatches + ~250 barriers, most 1x1x1 or a few groups. **6.49 ms = 21.7 % of the frame; 0.58 ms in light windows; 0.92 ms (8.6 %) on the 9070.** | 6.5 ms | engine-level fix only (section 4.1) |
| F2 | low-occupancy dispatches | same chains: MshLod#3 / MPC#29 are `numthreads(1,1,1)` or 1-group; effect chains 1x1x1 (0.3 ms on DIRECT because slot 0 is off on AMD) | part of F1; effects ~0.3 | kernel rewrites exist (mpc/mshlod, proven, never measured on RDNA2); AS slot 0 built |
| F3 | async work that contends rather than overlaps | VCLOUD wall 0.46 ms (light) -> 2.51 ms (heavy) for a fixed 220-wave dispatch; VFSCAT 0.76 -> 1.27. DIRECT waits on VCLOUD at DrawSky (node 267) and on fog at its apply. gpu-unattributed: >= 1.8 ms of busy-but-unstamped DIRECT time, one candidate being DIRECT waiting on async. Sign of the net effect on RDNA2 is unknown. | 0-1.5 ms either way | **A/B queued (section 6, Q1)** |
| F4 | copies that exist only for layout / hazard reasons | GBufferEdit 4-RT hazard copy (edits read the original while writing the live G-buffer; SSR reads the copies); refraction scene copy 235 (render/2); late CopyColorRenderTarget 406/417/422/427 (timed copies for UI/FG/blur consumers) | copy bucket 2.25 incl. absorbed draws; real copies probably <= 0.5 | needs the MinRun = 1 split; gpu-copies-postres found no dead copy with FG off |
| F5 | redundant clears | 4096^2 cascade atlas depth cleared every frame (node 26, Z = 1): an HTILE fast clear, ~free; 38 RT clears = 104 us on the 9070, no duplicates found (invisible-gpu 4c) | <= 0.1 | not worth it |
| F6 | passes whose output is unused / occluded | TOONEDGE2 EdgePass full-screen every frame (0.17 ms) whether or not any stencil-8 outline pixel exists; RefProbe light-group-1 dispatches mostly empty (GIM built); ESM at 2048^2 feeds only fog + LPV (ESM1 built); water-view culling chain (node 299) runs whenever the stage has water, also when none is on screen (unproven) | 0.1-0.3 total | GIM / ESM1 built; TOONEDGE2 skip needs a GPU-side test |
| F7 | full-screen passes that read the same pixels | ClusteredDeferredShading2Pass x9 + SnowScattering + SSSS each re-read the G-buffer under stencil classes; post chain at output res (bloom down/up, MB, compose) | deferred family 1.16, post 1.05 | merging saves bytes that hit IC anyway: <= 0.2-0.3 ms |
| F8 | G-buffer layout / size | 14 surfaces incl. two 5-slice arrays (RS, RR/EM); the RR array doubles as the GI output; per-pixel up to ~57 B bound. Fat by deferred standards, but write cost is in the 6.3 ms G-buffer family mostly as VS work (meshlet VS chain, MSV) not ROP/bandwidth | <= 0.2 from slimming | not worth a material-system rewrite |
| F9 | barrier / transition overhead | 1360 barriers/frame (r5), single-element ResourceBarrier calls; batching was **measured -7 % on PC** (Barriers=1, rejected) and is a no-op on RADV; the remaining cost is dependency drains (F1) | - | respected: not re-proposed |
| F10 | frame-boundary idle | ~0.9 ms/frame between frames in heavy windows (gpu-unattributed); frame-begin tile-mapping updates on DIRECT (gpu-tiles) | 0.9 | other agent (GQ / TileStats) |

## 4. Better algorithms for the expensive systems (same look)

### 4.1 Geometry front-end (meshlet LOD + culling) - the one big lever
What it is: per frame, MshLod resolves a cluster LOD per instance with a level-by-level hierarchical traversal (one
dispatch + argument init + barrier per level, ~14 levels), then every view (VSM, main ph1, main ph2, water) runs instance
culling (MIC, Hi-Z footprint + VSM valid-rect SAT) and an 11-stage meshlet culling + prefix-sum + argument compaction chain
(MPC #22 -> #0 -> #20/#33 -> #23 -> #28 -> #29 (numthreads 1) -> #30 -> #31 -> #24 -> #26 -> #27).

Same-picture alternatives (all keep the per-cluster / per-meshlet predicates, so the visible set is identical; output order is
already non-deterministic because of atomic appends):
1. **Persistent-thread LOD traversal** (one dispatch, a global work queue with atomics, Nanite-style): replaces ~28 of ~30
   stages per resolve. Expected: most of MshLod's per-level fixed cost + the level-0 tail; **1.3-2.5 ms on pc2 heavy**, ~0.1 ms
   light, larger on the Ally (latency x2-4). Risk: persistent threads need forward-progress care (fine on AMD/NVIDIA, check
   Intel/RADV).
2. **Single-pass decoupled-look-back scan** for MPC #23..#26 (the `mpc` file already does it inside one group when cnt <= 1024,
   but the 5 follow-up dispatches + barriers stay): one dispatch per chain instead of six. 0.2-0.5 ms heavy.
3. **Multi-view batched culling**: VSM + main-ph1 (and water) culling share inputs (LOD lists, previous Hi-Z) and could run as
   one chain with a view index per thread: 2.7-6 chains -> 1-2. 0.5-1.0 ms heavy.
4. **Async overlap** of the LOD resolve (depends on camera + instances, not on this frame's depth): kick it on a compute queue at
   frame begin against P1/P3/P4 graphics, or one frame early against the previous frame's post/UI tail (needs double-buffered
   payloads: node 299 still reads them late in the frame). 1-2 ms heavy if the CUs are free; the engine has no async variant of
   these shader objects (MshLod is not slot-aware), so it needs new root signatures/contexts.
- Combined realistic: **2-3.5 ms on pc2 dense scenes (7-12 %)**. Difficulty: new kernels + rewriting the recorders (node 14
  Execute, MeshletPreculling 5301FC, MeshletInstanceCulling 53FD3C) + a GPU diff harness (gpu-shaders/culling/sim.py covers the
  kernels, not a recorder change). Weeks, high risk. Not attempted in this session.

### 4.2 Materials / G-buffer (6.3 ms heavy)
Already GPU-driven (one ExecuteIndirect per material group and phase). The VS dependent-load chain is the measured cost on the
Ally (75 % VS) -> MSV (other agent, queued #505). A visibility-buffer redesign would remove G-buffer overdraw/bandwidth but means
rewriting 1600+ material shaders: out of scope, and section 2.3 shows bandwidth is not what binds. Verdict: no architectural lever
beyond MSV.

### 4.3 Volumetrics (VCLOUD 2.5 + VFSCAT 1.3 ms async wall)
VCLOUD is already render/8 with 1/16 Bayer temporal amortization and reprojection; VFSCAT is a 160x90x64 froxel grid with ESM.
Both are the efficient algorithms for the look (no cheaper same-look algorithm: lower froxel/cloud resolution changes the
picture). Open lever is **scheduling** (F3): whether the async queue helps or hurts on RDNA2, and kicking fog earlier (inputs ready
after node 149/161, today kicked at 162 and joined shortly after). Shader-level work: vc7 / fogpar (other agent, A/B running).

### 4.4 Lighting / GI / reflections (1.35 + 1.16 ms)
RefProbe gathers are per-pixel screen-space passes at 0.6/0.5 scale (not probe re-captures), deferred lighting is clustered,
SSR is half-res at STANDARD. Async GI (RP2) would hide 0.25-0.6 ms on the 9070 but needs RefProbe on a compute queue (not
slot-aware; weeks). Verdict: efficient; only scheduling levers.

### 4.5 Shadows (2.0 ms)
Sun = VSM with a page cache + valid-rect culling (static geometry costs nothing per frame); per-frame shadow draws are dynamic
casters (skinned characters) across 3 cascades. Viewport-array instancing (draw each caster once into all cascades) would save
~0.1-0.2 ms GPU and CPU time; low feasibility. Verdict: efficient.

### 4.6 Post chain + upscale (1.05 + 1.37 + 0.69 ms)
FSR3 passes all feed the image; post at output res is the HIGH setting's look. Fusing bloom down/up + compose saves bandwidth
that is not binding at 1080p (each RGBA16F target 16.6 MB, IC-resident in pieces). Verdict: efficient.

## 5. Ranked candidates (pc2 = RX 6650 XT 1080p HIGH FSR65, heavy real-level frame ~30 ms)

| rank | change | ms recoverable pc2 heavy / light; Ally; PC 9070 | difficulty | visual risk | HW compatibility | evidence | validation plan |
|---|---|---|---|---|---|---|---|
| 1 | Meshlet front-end restructure: persistent LOD traversal + single-pass MPC scan + multi-view batched culling (+ async LOD) | **2-3.5 / ~0.2; Ally 1-3 (E, latency x2-4); 9070 0.3-0.5** | very high (weeks: new kernels + 3 recorder rewrites) | none if predicates kept (set-equal; order already atomic) | all D3D12 SM6; persistent threads need care on Intel / RADV | GP2 heavy 6.49 ms (21.7 %), light 0.58; r5 0.92 ms; 100 dispatches / ~250 barriers | GPU diff harness on the kernels, RGP/GP2 MinRun 1, shotdiff + LOD-pop check while moving |
| 2 | Async volumetrics scheduling (sync vs async per vendor; later: earlier fog kick) | **unknown sign, 0-1.5 / ~0**; Ally = #503-style question; 9070 ~0 | test: zero code (AS row); ship: ini default per vendor | none (engine's own sync path) | per vendor (Windows RDNA2 vs RDNA3/4 vs NVIDIA vs RADV) | VCLOUD wall x5.5 heavy vs light; >= 1.8 ms busy-unstamped DIRECT (gpu-unattributed) | **Q1 queued**: in-launch ABAB, PresentMon GPUBusy |
| 3 | Culling-chain stage cut: `mpc` + `mshlod` files (proven bit-exact, never measured on RDNA2) | **0.2-0.5 / ~0.03**; Ally 0.1-0.25; 9070 0.06 | low (files exist) | none (bit-exact chain proof, 95 + 2926 scenes) | all (DXIL validator 1.5; RADV untested) | MPC family 1.6-3.5 ms heavy on pc2 (x7-15 vs r5) | **Q2 queued**: pc_gpu_session -Items mpc,mshlod -GP2MinRun 1 |
| 4 | MSV meshlet-VS scalarization (other agent) | 0.15-0.4; Ally 0.4-1.0 | built | none (proven) | WaveOps SM6.0; Ally NGG caveat | round3 | #505 (queued) |
| 5 | Copy/clear removal after attribution | 0-0.5 (unknown until MinRun 1) | low-medium | none if unused | all | 2.25 ms bucket incl. absorbed draws | read #509 MinRun 1 rows, then decide per copy |
| 6 | RefProbe GI on async (RP2) | 0.3-0.5; Ally 1-3 at HIGHEST | very high | none | all | gpu-scheduling A4 | RGP + A/B |
| 7 | Effects slot 0 async (AS, built) | 0.1-0.3 (GPU-bound) | done | none (engine path) | not Intel; KT disabled on AMD for an unknown reason | offline harness 0.13 ms | AS PC/Ally A/B (queued elsewhere) |
| 8 | TOONEDGE2 skip when no outline pixel; GIM; ESM1 | 0.05-0.2 each | low-medium | none / ESM1 slight | all | GP2 rows | GIM/ESM1 queued (#473); TOONEDGE2 needs a GPU-side test |
| 9 | Merge deferred light-group / post full-screen passes | <= 0.2-0.3 | high | none | all | section 2.3: not bandwidth-bound | not recommended |
| 10 | G-buffer slimming (5-slice arrays) | <= 0.2 | very high (all materials) | none | all | section 2.3 | not recommended |
| - | Barrier batching | **-7 % on PC (measured)** | - | - | - | gpu-barriers.md | rejected, not re-proposed |

**Explicit verdict.** Outside the meshlet front-end, the Nioh 3 renderer is a reasonably efficient modern deferred design for
this hardware class: every other family is <= 6.7 % of the dense-scene frame on pc2, the expensive systems already use the
amortized algorithms (VSM page cache, temporal clouds, froxel fog on async, half-res GI/SSR, GPU-driven meshlets with two-phase
occlusion), and at 65 % render scale the screen-space chain is not bandwidth-bound, so pass fusion and buffer slimming recover
little. Of the ~30 ms dense-scene frame, a realistic same-look architectural total is **~2.5-4.5 ms (8-15 %)**, and ~2-3.5 ms of
that is one engine-level project (the meshlet front-end). There is no further hidden 5-10 ms.

## 6. A/B tests queued on pc2

- **Q1 - async volumetrics: contend or overlap (zero new code).** ASI >= 1.7.44 (bridge #131 1.7.45b is fine). Base ini = rc
  default + `[Patches] AS = 2`, `[Tuning] AS_Slots = -3 -4`, `[Profiler] GPUProf = 0`. With the live row "Async compute slots (AS)"
  ON, VCLOUD (slot 3) and VFSCAT (slot 4) take the engine's synchronous path on the graphics queue; OFF = stock async. In-launch
  quads at a GPU-bound spot (the save's load point, 1080p HIGH FSR 65, MaxFps 240):
  `tools\testmatrix\pc2_abba2_pc.ps1 -Live AS -LaunchFlags none -Quads 6 -RenRat 65 -Upscaler FSR -MaxFps 240 -Input vpad -BaseIniFile C:\nioh3-re\as_sync_base.ini -RunName as_sync_vol`.
  Proof it ran: `stats/s AS: switch ON, added -, kept synchronous while ON 3 4`. Decision: sync faster in every quad (>= 1 %) ->
  async contends on RDNA2 Windows -> ship `AS_Slots = -3 -4` for that class (per-vendor default); equal -> async is free, no change;
  async faster -> the engine's design is right and F3 closes. Shotdiff of the sky (same shaders, same picture expected).
- **Q2 - culling-chain kernel files on RDNA2.** Bridge file with `0x29ee0902.file` (mpc, sha256 3947f3d7...) and
  `0x8d9282e6.file` (mshlod, 214ac371...) -> `gpu-shaders\replacement\`, verify sha; then
  `tools\testmatrix\pc2_pc_gpu_session.ps1 -Phases B -Items mpc,mshlod -Reps 2 -GP2MinRun 1 -Tag cull1 -UserConfig <#441 config>`.
  Metric: PresentMon GPUBusy (primary), GP2 MPC / MshLod rows (MinRun 1 also gives the per-kernel split the round-4 work needs),
  shotdiff. Expected 0.2-0.5 ms where the chain is heavy, ~0 at a light spawn.

Queued 2026-10-09 05:31: **bridge #564** ([QUEUE] pc2, after pc2's existing queue); files: **bridge #138**
`nioh3-cull-mpc-mshlod.zip` (the two culling files + README). No ASI or harness change was needed: Q1 uses the AS live row
built in 1.7.44 (asyncslots.cpp: `AS_Slots = -N` keeps slot N synchronous while ON), Q2 uses the existing `mpc` / `mshlod`
items of pc_gpu_session.ps1. Results are appended here when they arrive.

Why no new ASI patch this round: the only ASI-sized architectural cut found (skip MPC #28-#31/#26 when cnt <= 1024, 5 dispatches
+ their UAV barriers per chain) still needs proof that the CPU `cnt` equals the cbuffer's `nVSDrawPrm.y`
(gpu-culling-rewrite.md section 3; re-read of MeshletPreculling 5301FC this session: the count reaches the GPU through the buffer-view
setter 52F8EC and the compute-context commit vf+C8h, cbuffer source still not located). Shipping it unproven could drop meshlets.
Q2's MinRun = 1 rows will show whether those 5 stages are worth the proof work on RDNA2.

## 7. Files
- `re\scripts_gpu\frame_graph.py` - render graph -> ordered nodes with bound textures (size, format code, MB); `--md` table.
- `re\scripts_gpu\arch_budget.py` - GP2 rows -> families for heavy / light windows, with the GP1 markers of the same windows.
- Inputs: `data\dumps\kgk_0x102dd706.kidsrender.txt`, `data\dumps\0x93fb3280.kidsobjdb.txt`, `captures\pc2\phaseA\tour\logs\run01.log`,
  `captures\pc2\phaseA\tour\presentmon.csv`.

## Capture-check (package 12, 2026-10-10; full table in renderer/12-capture-check.md)

FCAP1 frame captures on pc2 (RDNA2), the Ally (RADV / vkd3d-proton) and the PC (RX 9070), 14 captures x 2 frames. Contradictions and refinements of this note:
- **Render resolution is 1280x720** at REN_RAT 65 (all device captures; VCLOUD 20x12x1 groups, not 20x11), not 1248x702. Every render-res size / byte figure here is ~5 % low.
- **2.1 queues / P0 / F10: UpdateTileMappings never runs on DIRECT.** All calls are on a COPY queue from 290FFF (0-79 per capture); the COPY queue waits on the DIRECT fence instead.
- **2.1 async slots:** the VCLOUD dispatch is replayed at the `DrawSky` marker (node 267), not at node 4, and its queue has no wait on DIRECT; VFSCAT is replayed right after `GenerateSceneHiZ`. All COMPUTE lists reach their queue in the first of the two end-of-frame bursts (~82-89 % of the frame), so the early kick has no effect on this build. DIRECT joins both after the DrawSky list (4F9323 waits).
- **2.2 P7:** `GenerateSceneHiZ` is 7 dispatches per run; the PC runs it twice (14), pc2 / Ally once. **F1:** the meshlet chain is 32-100 dispatch + 12-34 ExecuteIndirect and 191-540 barriers depending on scene, not a fixed ~100 / ~250.
- **2.3 / section 1 formats:** 14 distinct render-res RGBA16 (8 B) surfaces were touched vs ~1-2 assumed; the cascade atlas is 4096x4096 D16 (R16_TYPELESS); depth is R32G8X24 (D32S8) at render res and half res; velocity RG16; the scene HDR is R11G11B10F or RGBA16 (not resolvable without RTV -> resource mapping). Full list in 12-capture-check section 6.1.
- **F6:** TOONEDGE2 shaders appear in none of the 18 captured frames.
- Confirmed: 18-27 DIRECT lists per frame, 4-6 COMPUTE lists on 2-3 queues, ~1360 barriers per frame (PC 1203-1532), the phase order P0 < P2 < P4 < P5 < P6 < P7 < P13 (DrawSky) < FSR3 < post < UI, 7-RT G-buffer + D32S8, ~50 % of dispatches are 1x1x1 (F2).
