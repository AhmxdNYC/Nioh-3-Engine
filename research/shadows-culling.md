# Shadow-map caching and visibility/culling: what the engine already does, and what is left

See also `shadows-redundancy.md`: the second shadow occlusion phase re-draws only meshlet items (hypothesis H1 / saving A2
below are false), and the Ally runs the CPU VSM tile path 1F8340 (mode 5) with a tile budget of 8 at STANDARD and above,
whose batch loop draws past that budget (S1, not provably safe to skip).

Status 2026-10-07. Static work only: the dumped Nioh3.exe (Steam v2.02), the Ghidra export `<re-scratch>\decomp` plus
read-only decompiles, the r5 RGP trace of 2026-10-06 (RX 9070, HIGH, 1920×1080 render, 10.7 ms GPU) cross-checked against
a second trace from another session, the env DB dump `0x396b9210.kidsobjdb`, and the r5–r9 CPU profiles. The game was not
run; nothing was written to it. Scripts and the live marker tool (`gpu_markers.fsx`) are in the private dev repo (§8).

## 0. Bottom line

- **The engine already caches shadows and culls well. The "static shadow cache" asked for in topic A already exists in
  three forms, so building one gains about nothing.**
  - **Sun: a virtual shadow map (VSM) with a page cache.** Only invalidated pages ("valid rects") get re-rendered. The
    meshlet culling takes `tValidRectCountCumulativeMap` and `tHiZ`, so cached pages cost no geometry. A per-frame tile
    budget applies (node default 16, env param 0x93bce2c1; ApplyShadowQuality sets 8 at STANDARD..HIGHEST and 2 at
    LIGHTEST/LIGHT, shadows-redundancy.md). The script API
    `Graphics::SetVirtualShadowUpdateTilesPerFrameBlackOut` very likely drives the same budget; its binding to that
    param was not traced. The env sets `ShadowmapType 6`.
  - **Two-phase occlusion culling for shadow maps.** The first pass draws, then `GenerateShadowMapHiZ` builds a Hi-Z, then
    the second pass draws (env flag `ShadowMapOcculusionCullingEnable = True`).
  - **A static/dynamic split of the cascade lists.** Per view, list A sits in slots 2–5 and list B in slots 11–14, and
    there is an optional baked "ShadowTree" for static geometry. In the sampled env `BakedShadowMode` is 0.
- **What is rendered into shadow maps every frame is almost entirely dynamic: ~200 direct draws of characters.**
  - In r5: 92 skinned `SM-Sfb#5`, 11 skinned `DEPTH-Sfb`, 15 skinned/morph `SM-CSfb`/`SM-CM0Sfb`, and 75 position-only
    `SM-CmdReSfb` (very likely pre-deformed character parts, see §2.3).
  - Plus 10 meshlet indirect draws of wind-animated trees (`SMTREE2`).
  - The same counts appear in a capture from another session, so this is steady per-frame work, not a burst of
    tile updates.
  - Cost: **~0.23 ms PC GPU** (≈1.4–1.8 ms on the Ally) and **≤3–4 % of the PC main thread**. That is the upper bound for
    *any* shadow caching. A cache cannot remove dynamic casters, so the realistic gain is ≈0.
- **Culling (topic B): GPU-driven and two-phase for every meshlet view.** The main view, the VSM/shadow view and the water
  view each run their own culling chain with a Hi-Z. The main thread does no culling at all (frustum and distance culling
  run on the KIDS job threads). What is left:
  1. **The culling pipeline's latency.**
     - 4 × (MIC + an 11-dispatch MPC chain), a 9-level MshLod chain (27 dispatches), MCLRC/CBC/CLC/GFM, plus 4 Hi-Z
       builds.
     - **≈0.92 ms of PC GFX time = 8.6 % of the GPU frame.** Earlier notes counted 0.34 ms, because they used busy time
       and not segment time.
     - It is a chain of dependent, mostly 1×1×1 dispatches separated by barriers, so it is latency-bound. **On the Ally it
       is probably 1.5–3 ms.**
     - The DXIL shows true dependencies: hierarchical prefix sums, one of them a single-thread `numthreads(1,1,1)` loop.
       So barriers cannot simply be dropped.
  2. **Direct (non-meshlet) draws are never occlusion-culled.** That is 533 direct draws in main-view phase 1, including
     262 tiny, non-instanced, alpha-tested draws.
  3. **The main thread records all 129 meshlet material groups twice** (phase 1 and phase 2), whether or not phase 2 has
     anything to draw.
- **Largest structural savings found.** None of them is a small, safe patch:

| # | Saving | PC | Ally | Feasibility / risk |
|---|---|---|---|---|
| B1 | Single-pass scans instead of the 6-step MPC chain (×4 views) + MshLod persistent traversal | 0.1–0.25 ms GPU | **0.4–1.2 ms GPU** (est.) | shader rewrite of 6–8 kernels + dispatch/barrier recording change; needs a GPU diff harness. Same look. Low–medium feasibility |
| B4 | Instance the 262 tiny non-instanced alpha-tested draws (AtB0/AtB3 …Ipthr…) | **~3–5 % main thread** (≈0.3–0.45 ms) | ~0.03 core | new instanced VS + engine batching. Same look. Low feasibility; identify the objects first |
| B2 | Skip the phase-2 re-record of empty meshlet groups | ~2–3 % main thread, ~0.08 ms GPU | ~0.3–0.6 ms GPU (empty ExecuteIndirects at 1 GHz) | the CPU cannot know a group is empty; needs GPU predication / count-buffer redesign. Low feasibility |
| B3 | CPU Hi-Z-readback occlusion for direct draws | 1–4 % main thread + GPU (scene-dependent) | 0.3–2 ms GPU in dense scenes | near-same look (1–2 frames of disocclusion latency). Hard |
| A2 | (void) dynamic casters drawn in both shadow occlusion phases → draw them once | 0 | 0 | **H1 is false** (§2.4): phase 2 draws only meshlet items |
| A3 | Draw each character once into all cascades/pages (viewport-array instancing) | 1.5–2.5 % main thread | small | new VS variants + cascade-loop change. Low feasibility |

- **Expected totals (estimates).**
  - No candidate has a cheap patch path (A2 is void, see §2.4).
  - Everything else needs engine-level work (a total of several % of the main thread and 1–3 ms of Ally GPU in theory).
  - Same-look wins on the order of the GI/ESM/WDO shader work are **not** here.
- **Built:** `gpu_markers.fsx` (private dev repo).
  - It turns on the engine's **own** debug markers (two data bytes, default off), so one RGP/PIX capture shows "Cascade
    %d", "VirtualShadowmap", "Clipmap %d", "Tile %d %d", "MeshletPreculling", "GenerateShadowMapHiZ" and the render-graph
    node names.
  - That settles every per-pass count this note had to infer.
  - Offline: **ALL PASSED**.

## 1. How shadows are rendered (Steam v2.02 RVAs)

### 1.1 Call structure
```
113204 render-task-graph node  ──(marker = node name if [ctx+2A62])
 └ 535188 SceneRenderView(sceneR=param_1, ..., base=param_3)
     shadow object  S = view+4328h → +20h → [0]   (ViewGetShadowObj 533E88)   S+18h = cascade count, vtbl+0x88/0x90 begin/end cascade
     ├ cascade loop 1: for c < S+18h:  S+914h=0.5/1.0 → ShadowSetDepthRange 86B8A4 (dirty-checked)  ;  "Cascade %d"
     │     └ ShadowCascadeRender 1F5C2C(..., cascade c)                                      (return site 1F6313 in cpuprof)
     ├ if base+16B5F6 && !base+16B5F2 && (cascades culled in loop 1) :
     │     GenerateShadowMapHiZ 9B1850 → loop 2 over the cascades again (S+10h=0, second occlusion phase)
     └ VsmClipmapRender 1F8340 (only when the VSM mode at base+1AC678 == 5: CPU per-tile path)
           "VirtualShadowmap" → "Clipmap %d" → "Clear Tile %d %d" / "Batch Tiles %d" / "Tile %d %d" → 1F5C2C per tile/batch
 VSM render node (mode != 5)  465384 → VsmPassExecute 465414: page manager base+16C640, clipmap entries view+4730h
     (0x150 B each, VsmClipmapSetup 32FF074), MeshletInstanceCulling / MeshletPreculling / DrawListWalk
```

### 1.2 ShadowCascadeRender 1F5C2C (per cascade, or per VSM tile batch)
- Draw lists live at `view+3FF0h` (array of 32-byte list headers, count `view+3FF8h`). The list count is DrawListCount
  535080 (a locked read of `+0Ch`, clamped by `+08h`).
  - **List A** (ViewShadowListA 1F691C): slots **2–5**.
  - **List B** (ViewShadowListB 1F68FC): slots **11–14**.
  - VSM alternates (ViewVsmListA/B 1F4B84 / 894344): slots 15–18 / 19–22.
- Meshlet GPU culling is recorded only when list A or B carries meshlet work and the view is not "skip culling"
  (`state+FDh`). It goes through MeshletCullBegin 9AC33C / 52E424, MeshletInstanceCullingRecord 53FD3C ("MIC"),
  MeshletPrecullingRecord 5301FC ("MPC" chain), then MeshletCullEnd 53007C (tokens 0x8A = UAV barriers).
- **Baked path.** If `param_25 && (base2+EA0h >> cascade) & 1`, list A is not walked. Instead a render object
  (`param_17`) draws its meshes with the cascade matrix CascadeMatrixGet 147F60 (`S + (0x25+c)*0x40`). This is very
  likely the baked/sparse ShadowTree (`BakedShadowMode`, `BakedShadowEnableCascadeNum`; the per-cascade bitmask
  matches), which is **0 in the sampled env**. The link was inferred from names and shapes, not traced.
- Otherwise:
  - list A goes through DrawListWalk twice (two sub-passes);
  - **list B is walked whenever `param_26 == 0` and it is non-empty**;
  - `param_28` adds the three per-view extra renderers (view+42D0h/4308h/4310h: trees/grass-type systems).

### 1.3 VSM (CPU tile path 1F8340, for completeness)
- Each tile is `view+4460h → +10h[i]`, 16 bytes `{clipmap|flags, x, y, …}`. The page table is
  `pm = base+16C640` (`pm+400E4h` width, `+400E8h` height).
  - VsmTileDirty 1F8168: `pm+41720h` bit 1.
  - VsmTileRequested 1F8094: `pm+40118h` bit 4.
  - **VsmTileIsEmpty 1F80C8**: the tile's caster stamp is 0. Empty tiles are cleared and marked valid without drawing.
  - VsmTileBindViewport 1F96E0 / VsmTileMarkRendered 149468.
- **Only tiles that are dirty, requested and non-empty are drawn, and at most `VsmTilesPerFrameBudget 1F7960` of them
  become valid per frame** (the "Batch Tiles" loop draws past the budget: shadows-redundancy.md S1). The budget is env
  param 0x93bce2c1 (type 0x4000000), node default **16**; ApplyShadowQuality sets 8 at STANDARD..HIGHEST and 2 at
  LIGHTEST/LIGHT.
- "Batch Tiles" mode packs up to `base+1EE340h = 8` tiles into one 1F5C2C call.
- In mode 6 (the env sampled on the PC) this CPU tile loop returns early. The VSM node 465414 renders instead, GPU-driven:
  `MPC#20/#33` read `tValidRectCountCumulativeMap` and `tHiZ`. The Ally captures run this CPU tile path (mode 5;
  shadows-redundancy.md).

### 1.4 Settings and data that shape the shadow work
- **`ApplyShadowQuality` 18A87C** (table 3978210, `gpu-maxsettings.md` §1):
  - sizes per tier 512/512/1024/1024/1024 (S+B9C/BA0), 1024/1024/2048/2048/2048 (→ 4653B4), 512/1024/2048/2048/2048
    (S+BA8);
  - "2/2/8/8/8" (→ 8FFD28);
  - HIGH and HIGHEST share all RTs.
  - HIGHEST clears `g+7598h`. That flag gates two things:
    - per-object render flag 0x20 for objects whose vcall+128h is true (RenderObjSetFlagMask 381A8);
    - in DrawEmit 1E80B0 (~line 1377), the per-frame texture-streaming stamp loop `5F6E1C` for those objects. At ≤HIGH
      those objects skip it; **at HIGHEST they are stamped.** So SHADOW HIGHEST is extra CPU texture-stream bookkeeping
      plus flag 0x20, not more shadow geometry.
- **Main view DB:** `ShadowmapWidth/Height 4096` (the cascade atlas: 4 rects of 2046² in the env), `PlaneShadowmap 1024`,
  `CubeShadowmap 256`.
- **Env `0x396b9210`** (all 62 presets):
  - `ShadowmapType 6`, `ShadowMapOcculusionCullingEnable True`, `BakedShadowMode 0`, `ShadowCascadeStart 2000`
    (2Start/3Start 0);
  - `ShadowEraseFarStart/End 25000/50000`, `MaxShadowCasterNum 1000`, `MaxSubShadowCasterNum 100`;
  - `ShadowmapPlanarResolution 1024`, `Cubic 256`, `VirtualShadowmapKeyLightResolution 0` (= code default),
    `VirtualShadowMapTileLifeSpan` not set (code default).

## 2. What the GPU actually does for shadows (r5, PC, HIGH)

### 2.1 Window and cost
Shadow section 1646–1978 µs. Exact GFX segment time, from `rgp_families.py` and `segments.csv`:

| Part | Segments | µs |
|---|---|---|
| Shadow-map draws (SM + DEPTH + SMTREE2) | 85, 86, 101, 102, 113–115 | **232** |
| VSM/shadow culling chain #1 (MIC + 11 × MPC) | 87–98 | 63 |
| Shadow Hi-Z (GHDM-FAST + 7 × GHDM mips) | 103–112 | 36 |
| Resolve/filter (outside this window): WDO#14 120, WDO#28 367, ESM 297 | — | 784 (replacement shader work: `invisible-gpu.md`, `gpu-savings-round2.md`) |

### 2.2 Draw composition
From `rgp_groups.py`. The VS input signatures come from the extracted DXIL.

| Group | Draws | VS inputs | Meaning |
|---|---|---|---|
| `SM-Sfb#5` (+ PS `SM-B0I2mIptrSfb#8`) | 92 | POSITION, BLENDWEIGHT×2, BLENDINDICES×2, TEXCOORD | skinned, alpha-tested |
| `SM-CmdReSfb#0/#2` | 38 + 37 | **POSITION only**, 64-byte cbuffer | position-only re-draw (see 2.3) |
| `SM-CSfb#5`, `SM-CM0Sfb#5`, `SM-Mdb5tltot#5` | 8 + 7 + 8 | skin + BINORMAL/COLOR/extra streams | skinned/morph (cloth, hair) |
| `DEPTH-Sfb#1` (PS `SM-SfbUh#12`) | 11 | skinned | |
| `SMTREE2-…Mshlt…` | 10 (5 ExecuteIndirect pairs) | meshlet | wind-animated trees |
| `SM-Sfb#7` | 1 | | |

- The second trace (another session, same spot) has the same groups with the same counts: SM-Sfb#5 92, DEPTH 11, CmdRe#0
  38, CmdRe#2 33. **So this is the steady per-frame load.**
- There is **no static world geometry among the per-frame shadow draws**. The meshlet world (`PB2…Mshlt`, 129 groups in
  the main view) appears in shadows only as the 10 culled SMTREE2 indirect draws. That is what a working page cache plus
  GPU culling looks like.

### 2.3 What `SM-CmdReSfb` is (inference)
- It is a position-only VS with one 4×4 matrix.
- DrawEmit looks up `"WritePositionBuffer"` / `"ControlPointMeshDeformation"` per draw. So deformed characters very
  likely write their skinned positions once (G-buffer pass), and later passes draw the deformed buffer with a
  position-only shader.
- The CmdRe draws interleave with the skinned CM0/CSfb draws of the same objects.
- **Treat them as dynamic.** A marker capture (§4) would confirm it: they should sit under character-named nodes.

### 2.4 Two occlusion phases, and hypothesis H1 (false)
- The sequence is: segs 85/86 (direct draws only) → culling chain #1 → segs 101/102 (SMTREE2 + direct) → shadow Hi-Z →
  segs 113–115 (mostly skinned SM-Sfb#5).
- In 1F5C2C, list B is walked in **both** cascade loops (loop 2 passes the same `param_26` as loop 1).
- **H1:** the dynamic list B is drawn in both shadow occlusion phases into the same depth target. The second draw writes
  identical depth (same geometry and matrix, LESS_EQUAL), so it is redundant.
- Against H1: segs 85/86 and 101/102 have *similar but not identical* draw sets (c2ad: 17 vs 14, 7970: 6 vs 3). So they
  could also be two different cascades.
- **H1 is false** (shadows-redundancy.md S2). The second cascade loop (1F5C2C call at 5389BB) sets ctx+FCh = 1, and
  DrawListWalk 39E20 then walks only items with `obj+80h != 0`, i.e. the GPU-culled meshlet items of the two-phase
  re-test. Direct draws (lists A/B, characters) are drawn once, in phase 1, so the similar direct-draw sets of segs
  85/86 and 101/102 are not a phase-2 duplicate (most likely different cascades). A2 is void.

### 2.5 Topic A as asked: "re-render a far cascade only when the light or frustum moves a texel, or a caster inside moved"
- **Already done, in a stronger form.**
  - VSM pages are invalidated per page, not per cascade, and are budgeted per frame.
  - Dynamic casters are drawn into the pages they touch.
  - Static meshlet geometry is drawn only into invalid rects (MPC `tValidRectCountCumulativeMap`).
- Time-of-day sun movement: the page cache has to invalidate on light-direction change. The engine handles that today:
  `VirtualShadowMapTileLifeSpan`, plus a tile budget that spreads re-renders over frames.
- Wind: trees are SMTREE2 and are re-culled every frame.
- Destruction and physics: dynamic list B.
- A second cache on top would add memory and risk for ~0 gain. **Not recommended.**

## 3. Visibility and culling (topic B)

### 3.1 Who culls what
| Stage | Where | Granularity | Applies to |
|---|---|---|---|
| Distance + frustum | PlaceableVisibilityUpdateRange DDC70 / CullEntitiesByDrawDistance 1772B4 / CharacterFarCullFlag 876EB8 / GrassPlacementCull 31EF94, all on KIDS job threads (task graph RegisterScenePlaceable ×7, 32 units) | object | everything, main view and shadow lists (per-cascade lists A/B) |
| Custom occluders | `[App]CustomOccluderRayCastThread` (CPU ray casts) against `OccluderUnit` lists | object | only stages with occluder units: 13 of 152 non-empty (`invisible-gpu.md` §2) |
| Meshlet LOD | MshLod chain (9 levels × [MshLod#3 1×1×1 → MshLod#2 ExecuteIndirect dispatch]) | meshlet cluster | meshlet geometry |
| Instance culling | MIC#0 (56 groups) | instance | meshlet geometry, per view |
| Meshlet culling + draw-arg build | MPC chain: #22 → #0 → #20/#33 (frustum + Hi-Z + VSM valid rects, writes `uCullCodes`, `uToMshltDrawPayloads`) → #23 → **#28 → #29 → #30 → #31 → #24 → #26** (prefix sums / argument compaction) → #27 (index stream + per-material args) | meshlet | meshlet geometry, per view |
| Hi-Z | GHDM (min/max depth chain, our GHDM-FAST) | per view | main view (after phase 1), VSM/shadow, water view; GFM-Hiz |
| Two-phase | phase 1 = culled against the previous Hi-Z, draw; build Hi-Z; phase 2 = re-test the rejects, draw | | main view (chains #2/#3 at 1996/3978 µs), shadow (#1 + GenerateShadowMapHiZ + loop 2), water view (#4 at 6870 µs) |

### 3.2 Counts per frame (r5)
- **Draw events:** 1354 direct (1175 DrawIndexed + 179 DrawInstanced), 278 + 138 indirect events, 1081 dispatches.
- **Main view:**
  - phase 1: **533 direct** draws + **129** meshlet ExecuteIndirect groups;
  - phase 2: **129** groups again (api30 + api31 pairs);
  - meshlet geometry ≈ 0.61 ms of GPU, direct PB2 draws ≈ 1.16 ms.
- **Direct PB2 draws by vertex group:**

| Group | Draws | GPU | Note |
|---|---|---|---|
| `PB2-AtB3DiGbiIpthrPkSfbVl#0` | 220 | 88 µs (0.4 µs each) | POSITION/NORMAL/UV/TANGENT + PSIZE pivot, 284-byte per-draw cbuffer, **no SV_InstanceID** → non-instanced alpha-tested small objects |
| `PB2-AtB0…Ipthr…` | 29 + 13 + small | | same kind |
| `PB2-AlepmeDiGbiPkSfbVl#0` | 78 | 514 µs | rigid, heavy |
| `PB2-AlepmeDiGbiOcPkRia#1/#0` | 72 + 21 | 311 µs | **skinned characters** |
| PB2LS terrain | 4 | 155 µs | |

- **How the main-thread percentages in this note are estimated.**
  - DrawListWalk is 30–33 % inclusive in r5–r9, and DrawEmit (once per draw) is 17–19 %.
  - About 990 items per frame go through the walk: 508 PB2 direct, 202 SM/DEPTH direct, 258 meshlet groups (129 × 2
    phases), 10 SMTREE2, plus a few more.
  - A class of draws is costed by its share of those items: ≈3 µs per item at a ~9 ms CPU frame. So 262 tiny draws ≈
    4–5 %, 129 phase-2 groups ≈ 2–3 %, and all shadow items (≈21 %) ≈ 3–4 %.
  - The sampler attributes 2–7 % of DrawListWalk *self* to the shadow call site 1F6313, consistent with short shadow
    lists.
  - These are proportional estimates, not measurements. A cpuprof with markers on cannot split them either; an A/B of
    any change can.
- Meshlet counts (how many meshlets survive) are not in the trace: they live in GPU argument buffers. With markers on,
  the ExecuteIndirect wave counts per marker give it.

### 3.3 Answers to the topic-B questions
- **Occlusion results feed shadows?** Yes. Shadows have their own Hi-Z (GenerateShadowMapHiZ) and occlusion phase, and
  the VSM uses valid-rect culling. The env enables `ShadowMapOcculusionCullingEnable`.
- **Occlusion results feed the main view?** Yes, two-phase, for meshlet geometry. **Direct draws get no occlusion
  culling in any pass.**
- **Hidden objects still drawn or prepped on the main thread?**
  - GPU-culled meshlets cost the CPU only one ExecuteIndirect per material group per phase (129 × 2).
  - Frustum-visible but occluded **direct** draws are fully prepped (DrawEmit ≈ 3 µs each) and drawn (early-Z rejects
    their pixels).
  - Frustum-visible meshlet objects also get per-object streaming bookkeeping (4BBEC part loop, TextureStreamUpdate
    1E7050) whether occluded or not. That keeps residency warm and is not a same-look target.
- **Culling granularity:**
  - CPU: per object (distance/frustum), per list (cascades A/B), per pass (25 DrawListWalk call sites filter groups by
    pass; skipped groups → 3BC0C);
  - GPU: per instance (MIC), per meshlet (MPC), per LOD cluster (MshLod), per VSM page (valid rects).

### 3.4 The GPU culling cost (exact GFX segment time, includes drain tails)

| Family | n | µs | of which drain tail |
|---|---|---|---|
| MPC (4 chains × 11) | 44 | 369 | 68 |
| MshLod (1 chain, 27) | 27 | 225 | 54 |
| GHDM / GHDM-FAST (4 Hi-Z builds) | 25 | 121 | 7 |
| MCLRC + MGLRC | 4 | 57 | 4 |
| GFM / GFM-Hiz | 18 | 48 | 16 |
| MIC | 4 | 47 | 15 |
| CBC + CLC, GVD, DYM | 9 | 54 | 14 |
| **Total** | | **≈921 µs = 8.6 % of the 10.7 ms frame** | 178 |

- The chains are latency-bound: 1×1×1 dispatches of 2–14 µs, each closed by a barrier.
  - Per chain (MIC + MPC, exact segment sums): VSM/shadow 63 µs, main phase 1 120 µs, main phase 2 81 µs, water view
    152 µs. The final MPC#27 argument/index build carries most of the variance: 54 µs in main phase 1.
  - MshLod: 225 µs. Hi-Z builds: 36 / 37 / 50 µs.
- The Ally runs them at 1.0–2.7 GHz with RDNA3 barrier/launch latency, so a latency factor of ×2–4 applies, not the
  ×6–8 throughput factor. **Estimated 1.5–3 ms per frame on the Ally.** This is the biggest same-look GPU target in this
  topic. It needs an Ally RGP to pin down (§5).
- **Why barriers can't simply be dropped** (DXIL of the MPC kernels):
  - #28 computes block sums over `uVSDrawMshltMtrOffsets` (stride 3) and `uVSDrawArguments` (stride 9).
  - #29 is **`numthreads(1,1,1)`**: one thread walks the blocks serially and writes running totals.
  - #30, #31, #24 and #26 read and write the same two buffers.
  - These are dependent scan stages, so the UAV barriers are required.
  - The fix is algorithmic: one decoupled-look-back scan (or a wave-prefix scan in one 1024-thread group) replacing
    #28→#26, and a persistent-thread traversal for MshLod. That means new kernels and changing the recorded dispatch
    sequence in MeshletPreculling 5301FC, plus validation on a GPU.

## 4. Built: engine markers on demand (`gpu_markers.fsx`, private dev repo)

The engine ships a full debug-marker path that is off at boot:

| Side | Flag | What happens when it is set |
|---|---|---|
| Record ([App]Game) | byte `g+2A62h`, `g = [Nioh3+45BA588]` (KtglContext) | GfxBeginMarker 1D3AD8 formats the name (vsnprintf), derives a colour and writes tok 0xA5 + string. GfxEndMarker 113CA4 and the inline end sites write tok 0xA6. 25 code sites test the byte; none writes it |
| Replay (SynergisticDispatcherTask) | byte `SynCtx+542h` = `g+552h` | tok 0xA5 SynTokBeginEvent 114024 → SynBeginEvent 113E64 → the marker interface `[4BD09C0]` if set (tok 0x8E), else `ID3D12GraphicsCommandList::BeginEvent` (vtbl+1C8h, UTF-16). tok 0xA6 113FC8 → EndEvent (vtbl+1D0h). tok 0xA0 A96D70 is the engine's own setter of this byte |

Markers then exist for every render-graph node (113204 @11383C) and inside SceneRenderView, the cascade loops, the VSM
(clipmap/tile/batch), meshlet culling and Hi-Z.

```
dotnet fsi gpu_markers.fsx offline         # dump checks: ALL PASSED
dotnet fsi gpu_markers.fsx dry             # live, read-only: g, both flags, cmd-list vtable outside the image, iface slot
$env:FT_ALLOW_WRITE=1
dotnet fsi gpu_markers.fsx on              # replay byte, then record byte = 1; wait ~1 s, then capture
dotnet fsi gpu_markers.fsx status          # 10 x 100 ms: does anything reset them?
dotnet fsi gpu_markers.fsx off             # or: revert (restores the bytes saved by `on`)
```

- **Look:** unchanged. Markers carry no render state.
- **Cost:** CPU per marker (format + token, plus MultiByteToWideChar + BeginEvent on the dispatcher). Use it only for
  captures.
- **Risk:** a toggle in mid-frame can leave one Begin/End pair unbalanced on one command list for one frame; the release
  runtime does not validate this.
- **Unverified:**
  - (a) whether the AMD driver surfaces D3D12 `BeginEvent` as RGP user markers (`rgp_markers.py` already decodes
    USER_EVENT). PIX and RenderDoc do show them.
  - (b) the engine passes its colour as BeginEvent `Metadata` (0xFFrrggbb), not the PIX "0 = UTF-16" code. Some tools may
    show the colour instead of the name.

## 5. Measurement plan (settles the open numbers)
1. **PC, markers on, RGP at the r5 shrine spot.** Get per-marker draw/EI counts and µs for "Cascade 0..3" in both loops,
   "VirtualShadowmap"/"Clipmap"/"Tile", the four MPC chains (which view each one belongs to) and the
   render-graph node of the 262 `AtB*Ipthr` draws (which system they are; B4).
2. **The same in a dense area** (village or interior). Measures direct-draw occlusion opportunity (B3): count direct draws
   whose pixels fully fail early-Z, using RGP per-draw waves.
3. **Ally RGP at MAX native 1080p** (`gpu-maxsettings.md` §8 recipe), markers on. Measures the real culling-chain cost
   (B1) and the shadow-draw cost on the 780M.
4. Optional: the PIX "Timing" capture on the PC with markers (exact names, if RGP drops them).

## 6. Designs (not built), with grades

| id | What | How | Same look? | Feasibility | Gain |
|---|---|---|---|---|---|
| A2 | Draw dynamic list B once across the two shadow occlusion phases | — | — | **void: H1 is false** (§2.4) | 0 |
| A3 | One draw per dynamic caster for all cascades/pages | SV_ViewportArrayIndex instancing (VS variant) + cascade-loop rewrite | yes | low | 1.5–2.5 % PC main |
| B1 | Single-pass scans for MPC #28–#26; persistent MshLod traversal | replacement kernels + new dispatch/barrier sequence in 5301FC; GPU diff harness required | yes | low–medium | PC 0.1–0.25 ms; **Ally 0.4–1.2 ms** |
| B1b | Run the MshLod chain on async compute (it depends on the camera, not on this frame's depth) | cross-queue fence in the render graph | yes | low | Ally up to ~0.5–0.9 ms overlapped |
| B2 | Avoid recording phase-2 groups that are empty | GPU-written group mask + SetPredication / one ExecuteIndirect with a count buffer per PSO | yes | low | 2–3 % PC main; Ally 0.3–0.6 ms |
| B3 | CPU occlusion for direct draws | copy a 64×32 Hi-Z mip to a readback buffer each frame; in DrawListWalk skip items whose reprojected AABB is behind it **and** that were hidden last frame | near (1–2 frame pop) | low–medium | 1–4 % PC main + GPU, scene-dependent |
| B4 | Instance the tiny alpha-tested direct draws | batch consecutive same-mesh/material items; instanced VS reading a per-instance cbuffer array | yes | low | 3–5 % PC main |
| B5 | Pre-bucket the visible list per pass (25 walks → direct pass ranges) | build per-pass index once per frame | yes | medium | 1–2 % PC main (E) |
| — | VSM tile budget / lifespan, shadow distances | data/script knobs | **no** (fewer updates or shorter shadows) | — | — |
| — | A cascade cache on top of the VSM page cache | — | — | not useful | ≈0 |

## 7. Symbols
symbols.tsv holds the functions, labels and fields above (ShadowCascadeRender, VsmClipmapRender, the VSM tile
predicates, list getters, culling recorders, the marker path and tokens, the env/flag fields).

## 8. Files (private dev repo)
| File | What |
|---|---|
| `gpu_markers.fsx` | marker switch: offline / dry / status / on / off / revert (default off) |
| `rgp_groups.py` | RGP events + pso_map → per-shader-group counts/µs; optional filtered timeline |
| `rgp_window.py` | RGP time-window dump with group names (runs collapsed) |
| `rgp_families.py` | direct / indirect / dispatch counts per shader family, meshlet split |
| `q1.txt`, `q2.txt` | read-only Ghidra decompiles (cascade/VSM helpers, VSM node) |
| DXIL extracts | SM/PB2 vertex shaders (`vs_*.ll`), MPC chain kernels (`MPC_*.ll`) |
