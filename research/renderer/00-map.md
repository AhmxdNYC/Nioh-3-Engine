# renderer/00-map: module map, coverage, parallel plan, runtime capture, new-renderer feasibility (Phase 0, 2026-10-10)

Scope: the whole Nioh 3 (Steam v2.02) rendering engine. This note is the index for the `research/renderer/` packages; each package
note owns the detail. Static work only, no game launched on this PC. Tags: **M** measured, **S** static (dump / decompile), **E** estimate.

## 0. Summary

- **Render module size (S):** 54,295 functions / 13.0 MB of code (25 % of the exe's 52.7 MB). The evidence-backed core is 5,932
  functions / 1.9 MB: named render entry points plus their call / vtable closure. The rest is code in render-labelled regions of
  `docs/code-map.md` that no walk from a named entry point reaches (mostly vtable / callback targets, generated parameter code and
  KIDS type glue).
- **Coverage (S + M):** **~17 % of the render module's code bytes are understood** (real name + high/medium confidence or cited in a
  note), **49 % of the core's bytes**, and 3.9 % of its functions by count. Runtime cost is far better covered: **≥ 99 % of
  profiled render-thread CPU time** is in named functions (perf-coverage.md). On the GPU side, all **430 render-graph nodes / 70 node
  types** are identified with their Execute entry points (render-nodes.md, render-architecture.md §2). The hot paths are known; the cold
  breadth is not.
- **Work plan:** 4 packages started first (backend, materials, lighting, post). 8 more are laid out in §3, split by `partition.tsv`
  so that no two own the same function: frame-graph, resources, drawlists, meshlet, volumetrics, effects, ui, capture-check.
- **Runtime capture (built):** the FC logger (`[Profiler] FrameCap`, default 0, commit 89b7e52/5281c1e) records one
  frame's full D3D12 stream per tour dwell. FCAP1 captures from pc2, the Ally and the PC (RDNA4) are analysed in renderer/12-capture-check.md (§4).
- **Feasibility headline (§5):** (a) **a new D3D12 submission layer is the only level whose payoff matches the measured wall.**
  On pc2 with ReBAR the frame is 26 ms against 19 ms of GPU work, because the single dispatcher (replay) thread is the critical path.
  Parallel or cheaper replay is estimated (E) at up to **~+30 % (38 → ~50 fps) on pc2**, ~0 on the GPU-bound Ally. pc2 is a slow-link
  worst case (~0.8 GB/s chipset slot, see docs/BENCHMARKS.md), so this is not a typical-PC projection. It needs
  an estimated 4-8 weeks of work, medium-high risk, and can ship incrementally as an ASI. (b) A new frame graph reusing the game's shaders is
  only worth doing in pieces (meshlet front-end: an estimated 2-3.5 ms GPU on pc2 in dense scenes). (c) A fully new renderer is not worth it:
  years of work for a ceiling of about the same CPU gain as (a) plus 3-5 ms GPU.

## 1. Method and tools

- `re\scripts_renderer\render_map.py` (stdlib + numpy, ~3 s, reuses `re\scripts_re\regions.py`):
  - Functions are the `.pdata` starts (211k after folding chained entries).
  - Edges are direct E8/E9 calls plus ctor→vtable-slot edges: a function that `lea`s a vtable gets an edge to every slot (54.5k edges).
  - Seeds are the symbols.tsv names matching a subsystem regex, plus about 100 hand-picked entry RVAs from the notes.
  - A multi-source BFS (depth ≤ 4) assigns each function to the nearest seed's subsystem. It does not enter hubs (in-degree ≥ 60)
    or service / non-render regions; those edges are counted as dependencies.
  - Finally, unreached functions in render regions are filled in by their region label.
  - Run: `py -I re\scripts_renderer\render_map.py` (table), `--sub KEY --list N` (roots), `--partition re\scripts_renderer\partition.tsv`.
- `re\scripts_renderer\partition.tsv` is the **ownership file** of the work plan, with columns rva, subsystem, depth (0 = seed,
  1-4 = closure, 50 = region fill), size, region, name, package. Regenerate it only deliberately, because names
  added to symbols.tsv move seeds (and so move ownership).
- **Bulk C export (done for this note, see §1.1):** all 54,295 partition functions are in `<re-scratch>\decomp\<rva>.c`
  (`targets.txt` = the partition in depth order; the previous list is kept as `targets_base_20261008.txt`).
  `re\scripts_renderer\export_renderer.ps1` runs it in 12-minute chunks, so the read-only copy's mutex is free between chunks.
  It then retries timeouts at 600 s and runs `apply_names.py`. For renderer work, **grep the export first**:
  `rg -l "pattern" <re-scratch>\decomp`. Use `decomp_pool.cmd` (4 parallel read-only copies) only for `asm:` / `x:` queries.
- Caveat: indirect calls through runtime-filled tables are seen only through a ctor/vtable `lea`. That covers render-node Execute,
  KTGL token handlers in table 3C02220 and job callbacks. Region-fill membership is "mostly render" (hold-out: 79 % right at the
  coarse class for low-confidence regions, code-map.md).

### 1.1 Export run (2026-10-10 00:29-00:44)
- Targets: **54,294 partition functions**. 5,158 were already exported, so **52,621 were new**.
- Main pass: one Ghidra instance on the read-only copy, threads = 6, timeout 120 s. It exported 52,621 in **272 s** (~200
  functions/s), with 2 failed and 14 timeouts.
- Retry pass: threads = 2, timeout 600 s, 279 s. It recovered 4 more. Among them is **PostEffectApply 6DA118 (28.6 KB, now ok)**.
  **4 timeouts remain**; use `decomp_pool.cmd asm:` for those.
- `apply_names.py` took 4 min (9,553 names, 18,764 files rewritten).
- **Total wall time 15 min.** `<re-scratch>\decomp` now holds 57,775 ok functions (renderer + the earlier 5k).
- For anything the export does not answer, use `<re-scratch>\decomp_pool.cmd` (4 read-only copies, parallel) instead of `decomp_ro.cmd`.

## 2. Module map

Counts: **all** = partition functions (core + region fill); **core** = seed + closure only; **real** = symbols.tsv name that is not an
auto/RTTI/placeholder name; **understood** = real name with high/medium confidence or cited in a note. Notes = the notes citing most
of the subsystem's functions (hex RVA tokens).

| subsystem (package) | all funcs / KB | core funcs / KB | real names (all) | understood: all bytes / core bytes | main notes |
|---|---|---|---|---|---|
| FRAME: frame / render-graph build & pass scheduling (frame-graph) | 212 / 181 | 212 / 181 | 45 | 58 % / 58 % | render-architecture, render-nodes, perf-coverage, mainthread-drawprep, frame-timing |
| BACKEND: D3D12 device, queues, KTGL token stream, dispatcher replay, barriers, fences, descriptors, root sigs (backend) | 4,931 / 1,493 | 495 / 127 | 294 | 8 % / 54 % | d3d12-submission, vkd3d-audit, gpu-scheduling, pc-submit-overhead, gpu-barriers, cpu-bound-midrange, **renderer/backend.md** |
| RESOURCE: heaps, upload rings, mapping, residency, tiles, texture streaming, RTs (resources) | 235 / 61 | 235 / 61 | 128 | 65 % / 65 % | root-cause-vram-streaming, rebar-fix-v3, ui-mainthread, vkd3d-audit, stutter, gpu-tiles |
| PSO: PSO / shader / material system (materials) | 6,694 / 1,574 | 260 / 94 | 138 | 7 % / 51 % | stutter-pso, boot-shader-compile, stutter, gpu-materials-trim, **renderer/materials.md** |
| DRAW: views, draw lists, CPU draw prep (drawlists) | 120 / 109 | 120 / 109 | 22 | 69 % / 69 % | mainthread-drawprep, parallel-drawprep, ally-drawprep-2, gfx-submission (docs) |
| SCENE: meshlets, GPU culling, LOD, Hi-Z, terrain / grass (meshlet) | 11,443 / 1,792 | 1,555 / 162 | 191 | 5 % / 41 % | meshlet-frontend, meshlet-mainthread, meshlet-redundant-lod, rdna2-meshlet-slowdown, work-deciders, gpu-culling-rewrite |
| LIGHT: deferred lighting, shadows (VSM, cascades, local), GI / probes, SSR, AO (lighting) | 4,932 / 1,001 | 726 / 320 | 329 | 22 % / 56 % | shadows-culling, shadows-redundancy, gi-highest-audit, gpu-gi-rewrite, **renderer/lighting.md** |
| VOLNAT: volumetric fog, clouds / sky, weather, water / ocean (volumetrics) | 7,041 / 2,829 | 776 / 316 | 371 | 41 % / 42 % | gpu-volumetrics-rewrite, gpu-fog-rewrite, gpu-async-wins, perf-coverage |
| FX: effects / particles / decals (effects) | 5,613 / 1,312 | 458 / 214 | 176 | 8 % / 43 % | particles, effects-animation, gpu-async-wins |
| POST + PRESENT: post chain, FSR3 / XeSS / DLSS / FG, present, pacing (post) | 7,479 / 1,683 | 702 / 208 | 175 | ~8 % / ~45 % | fsr3-audit, frame-generation, gpu-post-*, vrr-present, frame-timing, input-latency, **renderer/post.md** |
| UI: screen layout, glyphs, UI quads, fades (ui) | 5,595 / 943 | 393 / 133 | 239 | 9 % / 38 % | ui-mainthread, docs/subsystems/ui.md, osd-design |
| **total** | **54,295 / 12,983** | **5,932 / 1,930** | **2,108 (3.9 %)** | **17 % / 49 %** | |

### 2.1 Per subsystem: entry points, data, dependencies

The RVAs are entries. `n` = functions reachable inside the subsystem (depth 4) from that entry.

**FRAME: frame / render-graph build and pass scheduling.**
- Entries:
  - FrameRenderViews 113204 (graph walker, per-view loop) and RenderNodeExecutePerView 10F060.
  - RenderNodeExecuteIfPrepared 367BD44.
  - RenderFrameBegin 4A529C (thunk 577E50, post 577EBC), RenderWaitForFrame 4A4F94, RenderViewSetupCommon 415080 (n 90).
  - RenderFrameSettingsApply 39B570, ApplyGraphicsSettings 39A39C, RenderParamsAssign 247950.
  - Graph and node types:
    - RenderGraphNodeFactory 25B337C and RenderGraphNodeRegisterType 2449040.
    - RenderGraphDataRegisterType 2448FB0 and CompoundRenderGraphTableRegisterType 243E624.
    - RenderNodeBaseCtor 3451994.
- Data:
  - Graph data `system 0x102dd706.kidsrender`: 430 nodes; dump in data/dumps.
  - Node vtable: +8 Execute, +10 ResourcesReady, +30 Release. 70 node types, 216 shader objects, 483 data objects; registry builder 26F5D84.
  - RenderTaskNode {+4 flags, +20 typeinfo, +30 task (vtbl[1] = execute), +38 next, +40 stats}.
  - Render-settings block RS = [r+F8]: +960/964 render size, +974 view base.
  - View gate: node byte & 7F + RS+974 < 0x20.
- Not known yet: how BeginCheckSkip / EndCheckSkip decide per view; how node order maps to fence / list cuts; graph build at stage load.
- Depends on: the jobs system (FrameTask vtables), GameMain frame pacing (frame-timing.md) and the scene views (DRAW).

**BACKEND (detail: `renderer/backend.md`).**
- Entries:
  - Device and swapchain: CreateD3D12Device 2EE300C (via sl.interposer), GfxDeviceTeardownForReset 2EEC4C8, RecreateSwapchainDevice 2EEA690.
  - Root signatures and queries: SynCtxCreateSignaturesAndQueries 2EFEA14.
  - Async compute: AsyncSlotsCreate 32FA450, AsyncComputeCtxCreateQueue 2EF8448.
  - Recording: KtglCmdStreamFlush 254110 (record side), DispatcherRingPush 25426C.
  - Replay thread: SynergisticDispatcherThreadEntry 2EEDFA0 → SynergisticDispatcherLoop 1A7C3E0, token table 3C02220 (185 handlers),
    WaitDispatcherDrainSpin 418F40.
  - Submit thread: ExecuteCommandListsThreadEntry 2F144B4 → 1A75970 (table 3BD4DF0: 9 tokens, ECL 4F9214 / Signal 4F91C0 / Wait 4F93C8).
  - Per-draw replay work: SynFlushGfxDescTable 1188AC, SynUavBarrier 1139D4, SynUploadCbvTable 118E28, SynTokFencePoint 8F0B5C.
- Data:
  - GfxDevice: +10 SynCtx, +3BE0 DispatcherRing, +4268 device, +4270 GfxQueue, +4278 swapchain, +4290 allocators,
    +4320 slot fences, +61A0 PagePool, +7720.. present-thread block.
  - SynCtx: +580 CmdList, +5A0 list ring, +6E8/6F0 ExecuteIndirect signatures, +B24 dirty flags, +D70 CB shadow,
    +1118 SRV bind cache, +1DB0 UAV cache.
  - KtglContext: +3C40 stream pointer, +3C70 flush limit, +25xx state shadows.
  - DispatcherRing {+10 semaphore, +18 32-byte items, +24 write, +28 read, +30 in flight}.
- Depends on: jobs (thread creation), Streamline / AGS, and every subsystem through the token stream.

**RESOURCE: GPU resources and memory.**
- Entries:
  - Heaps and resources: GpuMemCreatePlacedResource 62C8E0, GpuMemCreateCommittedResource 62C4DC,
    GpuMemCreateHeapWithPriority 2F1165C, TextureStagingAllocDedicated 9C2EE0.
  - Texture upload: GpuMemUploadTextureAndUnmap 62F86C, GpuMemMapAndUploadTexture 62ED60.
  - Texture mapping: TextureMap 69A36C, TextureMapSubresource 69A494, TextureUploadRect 699EC8.
  - Buffers: GpuBufferMap 4FD84 / GpuBufferUnmap 4FBD4, GpuBufferEnsureWidth 4E2F8.
  - Upload ring: UploadRingAllocate 1190C0, UploadHeapRing vtable 409A748.
  - Texture streaming: TextureStreamUpdate 1E7050, PagePoolFrameUpdate.
  - Tiles: RenderFrameBeginCacheFlush 32FE938 (tile mappings on DIRECT), copy-queue creators 2EE9EE8 / 2F110A8 / 2F13350.
- Data:
  - GpuBuffer {+30 staging, +58 size, +64 CPU-visible, +68 map mode, +70/78 map range}.
  - PagePool (GfxDevice+61A0) {+B2C used, +D10 frame stamp, +D20 enabled}.
  - TexStreamCtx {+67A8.. request lists, +74E0 block allocator, +7560/7590/75A8 tile budgets}.
  - RT table `system 0x93fb3280` (KTGL PixelFormat enum; the DXGI map is still not found).
- Not known yet: how the RT / transient memory is allocated (aliasing or not); residency (MakeResident / Evict users); descriptor-heap
  allocators (shared with backend).
- Depends on: Stream·RDB resource loading (res region), the ReBAR / UPLOAD heap placement (rebar-fix-v3).

**PSO (detail: `renderer/materials.md`).**
- Entries:
  - Graphics PSOs: PsoGetOrCreateGfx 119670, PsoCacheBTreeFind 119820 (3.1 ms/frame of dispatcher on pc2), SynFlushPsoAndTopology 118758.
  - Compute PSOs: PsoComputeCacheBTreeFind 58C144.
  - PSO lists: PsoListLoad 3483E04, PsoListPreloadJob 34835D4, PsoListResolveGfx 34837DC, PreloadPsoBootLoad 1101B90,
    PsoPreloadGfxCreate 2EFE440.
  - Materials: MaterialTechniqueBind 3C77F4, MaterialUnbindTechnique 3C5D64.
  - Shader constants: ShaderConstWriteStream 1D7340 (tokens 2B/2C/37/38), ShaderConstFlush 1DF880.
- Depends on: RES (shader_dx12 containers), DRAW (per-draw bind) and BACKEND (PSO create on the dispatcher).

**DRAW: views, draw lists, CPU draw prep.**
- Entries:
  - SceneRenderView 535188 (per view, 53 KB stack frame, n 61).
  - DrawListWalk 39E20, DrawListGroupLength 3BC0C.
  - DrawEmit 1E80B0 (17.5 % incl of main), DrawEmitSubPasses 1DC2F0, DrawSubmit 1D2F60, DrawStateCommit 1DAF6C (160 callers),
    DrawStateFlushDirty 1E0B64.
  - ModelDrawSubmeshApplyMaterialScales 5103C, StageSlotTableUpdate2578 / 2568 1D8720 / 1D84A0.
- Data:
  - View {+3FF0 draw-list headers (32 B), +4328 shadow root, +4460 VSM tile list}.
  - DrawPacket {+C flags, +14..20 args of token 6A, +28/2C state keys}.
  - Draw-list items grouped by object+18.
- Depends on: Obj·Stage (scene objects), animation (skinning matrices), PSO / materials, BACKEND tokens.

**SCENE: meshlets, culling, LOD, terrain / grass.**
- Entries:
  - MeshletFrameBegin 5D214 (n 96), MeshletUpdateSceneBuffers 4BBEC, MeshletUpdateFlagsThunk 5B278 / MeshletUpdateFlagsPre 5B484, MeshletRequestJob 4A2D0 / 9811F8.
  - Culling recorders: MeshletPrecullingRecord 5301FC (11-stage MPC chain), MeshletInstanceCullingRecord 53FD3C, MeshletResolveLODPass (graph
    nodes 14 / 35 / 299).
  - LOD: MeshletLodNodeApply 26A1C4, LodScaleCfg* 26F383C / 2704378 / 24BCDF8.
  - Grass: GrassPlacementCull 31EF94.
- Data:
  - MeshletManager (= GfxFrameContext+169DD0) {+11C capacity, +1AC state flags, +4B0 slot map, +4C0 active count, +568 entries
    (32 B), +830 texture slots}.
  - GfxFrameContext +16A0B8 upload parity.
- Depends on: streaming (meshlet loads, page flags), Obj·Stage, VSM (valid-rect SAT), Hi-Z.
- The 9.9k region-filled functions here are the least certain part of the map (low-confidence region, many tiny generated functions).

**LIGHT (detail: `renderer/lighting.md`).**
- Entries:
  - Reflection probes / GI: ReflectionProbePassNodeExecute 54294C → Record 542C08 (n 169).
  - Lights: LightPassExecute 7E919C.
  - Local-light shadows: LocalLightShadowmapRender 7E92B4.
  - Deferred shading: ClusteredDeferredShading2PassExecute 545CC4 → Impl 54762C.
  - Sun shadows (VSM): VsmPassNodeExecute 465384 / VsmPassExecute 465414, VsmClipmapRender 1F8340.
  - ShadowNodeApplyIfEnabled 13F2C4.
  - Render scales: GiRenderScalesApply 1872C4.

**VOLNAT: volumetrics, sky, weather, water.**
- Entries:
  - Sky and clouds: NatureSkyNodeExecute 293220 → NatureSkyRender 293230 (n 199), NatureSkyDrawClouds 5F2A88 (VCLOUD async slot 3).
  - Static nature: StaticNatureNodeExecute 6D7B88 → Record 6D7B98, StaticNatureRenderView 108590 (SSFog).
  - Water: UltraMarineNodeExecute 852620 → RenderViews 852650 → UltraMarineRecord 850FE0.
  - Ambient: SkyAmbientNodeExecute 145C60.
  - Weather: RainDropNodeExecute 419F54.
  - Fog grid: froxel setup 14871C, VolumetricFogScatterPass node (vtable 40EBA10, async slot 4).
- Depends on: weather / time-of-day game state (scene+2CC time), ESM / VSM (LIGHT), async slots (BACKEND).
- 2.8 MB of this subsystem is region fill from the low-confidence "Nature·Ocean" region. Ocean simulation (OceanThread%d) is game
  code that feeds it.

**FX: effects, particles, decals.**
- Entries:
  - Effect manager: EffectManagerFrameUpdate 39E910 (n 91), EffectManagerUpdateInstances 15FC80, EffectManagerUpdateEmitters 15E658,
    EffectCreate 4CE90C, EffectSystemCreate 2EA8744.
  - Render nodes: OldEffectNodeExecute 14FAD8, EffectRenderNodeExecute 3D0AE0 (ClusterLightCulling slot), EffectViewRecordSync 365AA98.
  - Decals: GBufferEditMiasmaDecalNodeExecute 937D68.
  - GPU particles: GpuParticleMgr vtable 4099A58.
- Depends on: game scripts (ScriptBind_Effect_*), animation (attach points), async slot 0 (off on AMD).

**POST + PRESENT (detail: `renderer/post.md`).**
- Entries:
  - Post chain: PostEffectLibraryInit 32FAE8C, PostEffectApply 6DA118 (n 200; it times out in the decompiler at 30 s, so the bulk
    export retries it at 600 s), HdrOutputNodeExecute 6D8928.
  - Upscalers: Fsr3UpscalerExecute 18D8898, XessExecute 18D66E0, DlssExecute 6B64D8.
  - Frame generation: FfxFi* 377xxxx.
  - Present: GfxDevicePresent 3D62E4, PresentThreadLoop 1A82298.
  - Latency: SlReflexSleep 6B8E28.
  - Frame timing: FrameDtSetup 5AD5F8, DynFps governor 5AF410.

**UI.**
- Entries:
  - Screen layout: StaticScreenLayoutNodeCtor 3451798 / UiViewRender 253C68 (= StaticScreenLayout Execute).
  - UI walk: UiWalkLayer 7D63E4, UiRenderPath 7D622C, UiTextFrame 254440, UiRasterizePath 30DDD18.
  - UiQuadBuild 1CD690: 2 ms/frame on pc2, a read-modify-write of a mapped UPLOAD buffer.
  - Glyphs: glyph atlas (GlyphAtlas struct, 20 fields).
  - Fades: SSFadeSaturationNodeExecute 927DF8.
- Depends on: ktgl ScreenLayout / menu logic (non-render), text tables.

### 2.2 Coupling inside the render module (direct calls between subsystems, top)

| edge | calls |
|---|---|
| SCENE → PSO | 1,497 |
| PSO → BACKEND | 862 |
| VOLNAT → SCENE / PSO / BACKEND | 764 / 729 / 572 |
| SCENE → FX | 712 |
| PSO → SCENE | 621 |

The material / draw path (SCENE, DRAW → PSO → BACKEND tokens) is the spine. Every node type calls into it. That is why level (b)
below costs more than it looks: a new frame graph still has to reproduce per-node material binding.

## 3. Parallel work plan

How to contribute a package note: each package covers the functions whose `partition.tsv` package column names it, so two
packages never name the same function (a function owned by another package goes into your note as an open question for it).
New names go into symbols.tsv under the package's `# renderer/<name>` block and must be unique. A package note lives at
`research/renderer/<file>` and has these sections: entry points, data structures (offsets), per-frame flow, what the GPU gets
(checked against FCAP1 captures where available), dependencies, and a keep / replace verdict for a new renderer. Every claim
about a pass names its graph node(s) and FC marker.

| # | package | status | scope: entry RVAs (+ every partition row of the package) | questions to answer | output | symbol block |
|---|---|---|---|---|---|---|
| 01 | backend | note published | BACKEND rows: 1A7C3E0, 2EEDFA0, 3C02220 table, 254110, 86F760, 1A75970, 418F40, 2EE300C, 32FA450 | full token table semantics, ring / fence protocol, list cutting, descriptor heaps, root signatures, async slots, what blocks the replay (ResourceBarrier, SRW 7F64DC, ECL ring) | renderer/backend.md | # renderer/backend |
| 02 | materials | note published | PSO rows: 119670, 119820, 58C144, 3483E04, 3C77F4, 1D7340 | PSO key layout, cache, material → technique → PSO, cbuffer layouts, shader containers | renderer/materials.md | # renderer/materials |
| 03 | lighting | note published | LIGHT rows: 54294C, 545CC4, 465384, 7E92B4, 7E919C, 1872C4 | VSM / cascades / local shadows, clustered deferred, probes / GI, SSR, AO | renderer/lighting.md | # renderer/lighting |
| 04 | post | note published | POST + PRESENT rows | — | renderer/post.md | # renderer/post |
| 05 | frame-graph | note published | FRAME rows (212): 113204, 10F060, 367BD44, 4A529C, 415080, 39A39C, 39B570, 247950, 25B337C, 2449040, 26F5D84 + node vtables | graph data format and build (kidsrender load); walker semantics (Compound, BeginCheckSkip / EndCheckSkip, ProfileEvent); view gating and the RS block; per-node Execute / Record contract; where list cuts and fences fall relative to nodes; which nodes go to which queue; the full node → pass table (extend render-architecture §2.2 with entry RVAs) | renderer/05-frame-graph.md | # renderer/frame-graph |
| 06 | resources | note published | RESOURCE rows (235): 62C8E0, 62C4DC, 2F1165C, 1190C0, 409A748 vtbl, 4FD84, 69A36C, 1E7050, PagePool, 32FE938, copy queues 2EE9EE8 / 2F110A8 / 2F13350 | heap types and placement (UPLOAD / DEFAULT / ReBAR), RT allocation and aliasing, the RT table (0x93fb3280) and the KTGL PixelFormat → DXGI map, upload ring sizes and recycling, residency / eviction, tile mappings, texture streaming request → copy queue → ready | renderer/06-resources.md | # renderer/resources |
| 07 | drawlists | note published | DRAW rows (120): 535188, 39E20, 3BC0C, 1E80B0, 1DC2F0, 1D2F60, 1DAF6C, 1E0B64, 5103C, 1D8720 / 1D84A0 | View struct; draw-list kinds and item layout; how objects enter lists (scene → view); per-draw record path down to tokens 6A / 2B / 66; state shadowing; what a new renderer would need as **scene input** (the handoff contract) | renderer/07-drawlists.md | # renderer/drawlists |
| 08 | meshlet | note published | SCENE rows (11.4k; core 1,555): 5D214, 4BBEC, 5B278, 4A2D0, 5301FC, 53FD3C, 26A1C4, LodScaleCfg*, 31EF94 | MeshletManager / scene-buffer layout; LOD resolve and culling chains (recorders + kernels, dispatch / barrier counts); Hi-Z; streaming feedback; terrain / grass; which of the 9.9k region-filled functions are really meshlet (trim the partition) | renderer/08-meshlet.md | # renderer/meshlet |
| 09 | volumetrics | note published | VOLNAT rows: 293220 / 293230, 5F2A88, 14871C, 6D7B88, 108590, 852620, 850FE0, 145C60, 419F54, fog node 40EBA10 | VCLOUD / VFSCAT data flow and async join points; sky / ambient; water views; rain / snow / wind nodes; time-of-day inputs | renderer/09-volumetrics.md | # renderer/volumetrics |
| 10 | effects | note published | FX rows: 39E910, 15FC80, 15E658, 4CE90C, 14FAD8, 3D0AE0, 365AA98, 937D68, GpuParticleMgr 4099A58 | effect instance → render path; GPU particle simulation chains (slot 0); decals; effect lights into clustered lists | renderer/10-effects.md | # renderer/effects |
| 11 | ui | note published | UI rows: 253C68, 254440, 7D61A8 / 7D622C / 7D63E4, 1C0FFC, 1CD690, 30DDD18, glyph atlas, 3451798, 927DF8 | UI render path and batching; glyph atlas; the mapped-UPLOAD read-modify-write (2 ms pc2); FG HUD-less path; what a new renderer must keep (UI as an overlay pass) | renderer/11-ui.md | # renderer/ui |
| 12 | capture-check | note published | no functions; FCAP1 zips from pc2 / Ally + a PC (RDNA4) run | per device: queues, ECL order, fences, per-marker draws / dispatches / barriers, PSO sets, RT formats; diff against render-architecture §2 and frame_graph.py; RDNA2 vs RDNA3 / vkd3d vs RDNA4 differences; caller RVAs → token handlers (hand to 01) | renderer/12-capture-check.md | none |

Suggested order: 05 frame-graph, 07 drawlists, 08 meshlet first, because the feasibility of (a)
and (b) depends on them. Then 06 resources, 09 volumetrics, 10 effects, 11 ui. 12 uses the FCAP1 captures.

## 4. Runtime capture: FCAP1

**Logger: FC.** `[Profiler] FrameCap = 0` by default. Code: `mod/Nioh3EngineFix/src/fcap.h`, included by `gpuprof.cpp`; hooks in
gpuopt.cpp, osd.cpp and psoswap.cpp. Commit 89b7e52; clean build 5281c1e.
- It reuses gpuopt's command-list vtable thunks, which already cover every D3D12 list method on both list classes and under
  vkd3d-proton. In a capture window FC arms every list slot in gp's hook mask. Outside the window only the GPUProf = 1 hook set is
  live (Close / Reset / Begin / EndEvent), which costs nothing.
- One 128-byte record per call:
  - caller RVA (the return address, read from the thunk frame = the dispatcher's token handler);
  - register args, plus stack args 5-7 (regs[10..12]);
  - list type;
  - a per-method decode: PSO shader set (psoswap map), marker text, RTV / DSV handle values, clear colour / depth, root constants,
    IB / VB views, copy locations.
- Barriers: one row per D3D12_RESOURCE_BARRIER, from gpuopt's pass-through path (needs `[GpuOpt] Barriers = 0`).
- Queue rows:
  - ExecuteCommandLists, one row per list, from osd's hook (needs the OSD on);
  - queue Signal / Wait / UpdateTileMappings, hooked by FC on the queue vtable at its first ECL.
- Resource rows: one per resource first seen (GetDesc: size, format, flags, GPU VA).
- Presents bound the window: `FrameCap_Frames = 2`, so one whole frame plus the overlap.
- Trigger: tour dwell start + `FrameCap_TourDelay` frames (tour.cpp → gp::LogWindow). At the dwell end the file is renamed
  `Nioh3EngineFix_fcap_<n>_<tour tag>.csv`. Without a tour the trigger is `FrameCap_AtSec`.
- Requires `GPUProf = 0`: FC refuses to run otherwise, because the GP2 sampler owns the hook mask.
- Exceptions switch the capture off; the hooks then pass through.
- Verified in game by the FCAP1 runs on pc2, the Ally and the PC (renderer/12-capture-check.md). Built clean, `test_patterns.py` 252/252 OK.
  (`run_gp_harness.py` fails to link on memstats::OnBind / BindsWanted. That failure predates FC: the harness was not updated for 1.7.48.)

**Bug found in the first run (pc2 FCAP1):**
- `dllmain` verifies the GP code sites only when `GPUProf` or `GPUProf_Markers` is set. With `FrameCap = 1` alone, gp therefore had
  no `codeOk`, gpuopt installed no command-list hooks, and all 5 captures hold queue rows only (0 list calls).
- The Ally's run (vkd3d-proton) was empty for the same reason, not a vkd3d problem; it was re-run with the key below.
  Its tour does not arm after Load Game, so it uses the timed trigger (FrameCap_AtSec / EverySec / Max).
- **Fix: add `Profiler.GPUProf_Markers=1`.** It is in `fcap_capture.ps1` and the spec below. No rebuild was needed. Later source
  builds also log a warning in this case.

**What the queue rows already show** (pc2, RDNA2 Windows; one frame between the first two presents of each capture, 4 tour points):
- **DIRECT: 19-23 ExecuteCommandLists per frame, 1 list each.** That matches the 21-23 of render-architecture §2.1.
  - 17-21 come from the ECL thread's handler at **4F9114** (+ Signal at 4F9127, so every DIRECT list gets its own fence Signal).
  - 2 come from another module (ra ffffffff: the Streamline / FFX present path).
  - DIRECT Waits: 7-8 per frame, at **8F4583** x3 and **4F9323** x3 (ECL-thread Wait token) + 2 foreign.
- **COMPUTE: 4-6 ECLs per frame on 2-3 distinct compute queues** (async slots 3 / 4 / 6: VCLOUD, VFSCAT, ocean), all from **4F925F**
  with Signal 4F908B, and 1-2 COMPUTE Waits. This confirms the async-slot model.
- **COPY: 3-4 distinct copy queues active.**
  - Streaming submits at **4DB03**, **5C305** and **892E04**, each followed by a Signal.
  - **UpdateTileMappings runs on a COPY-type queue at 290FFF: 2-87 calls per frame.** render-architecture §2.1 puts the frame-begin
    tile updates on DIRECT (CacheFlushOnGfx). Tile updates run on copy queues as well; that is for packages 06 / 12 to reconcile.
  - The first capture (60 s timer, before the tour armed) shows 49 COPY ECLs + 54 tile maps in one frame: the load-in burst.

**Digest:** `py -I re\scripts_renderer\fcap_digest.py "<dir>\Nioh3EngineFix_fcap_*.csv" --device X` prints:
- ECL batches per queue, in order;
- the fence / tile-map op sequence;
- per marker path: draws / indexed / dispatch / EI / barriers / copies / clears / PSO sets / RT sets;
- the top PSO shader sets;
- render targets with size and format;
- call sites per method.

**Spec FCAP1** (the same on every device; matched to XSYS1 so it lines up with the GP2 data in cross-system-compare.md):
- config `configs/xsys-1080-high-fsr65.xml`: 1080p, custom HIGH, REN_RAT 65, FSR 3.1, FG off, VSync off;
- tour points 7356 / 52 / 7292 / 5631 (`tools/tour/tourlist_xsys.txt`), spawn point only, 1 pass, 40 s dwell;
- capture 300 frames into each dwell, 2 presents per capture;
- ini keys `Profiler.GPUProf=0; GPUProf_Markers=1; FrameCap=1; FrameCap_Frames=2; FrameCap_TourDelay=300; FrameCap_Max=8; FrameCap_MaxEvents=600000;
  GpuOpt.Barriers=0; VRR.MaxFps=240`.

Each device runs it as follows:
- pc2 (RDNA2): `tools\fcap_capture.ps1 -Device pc2 -RunTour tools\tour\pc2_run_tour.ps1 -Asi <fcap1 build 5281c1e> -WaitFree`.
- Ally (RDNA3, vkd3d-proton): its XSYS1 harness with the keys above.
- PC (RDNA4): `tools\fcap_capture.ps1 -Asi <fcap1 build 5281c1e> -WaitFree` (package 12).

What to check against the decompiled frame graph:
1. The DIRECT list count (21-23 / frame) and where the cuts fall.
2. Async-compute kick / join fences for slots 3 / 4 / 6 (VCLOUD, VFSCAT, ocean) and the copy-queue streaming lists.
3. The per-node marker sequence vs the 430-node order.
4. The meshlet chain's dispatch / barrier counts (~100 / ~250).
5. The barrier count per frame (1360 on r5).
6. RT formats vs the RT table (closes the PixelFormat → DXGI gap).
7. The caller RVA per D3D12 method vs the 185-entry token table.
8. RDNA2 vs RDNA3 vs RDNA4: identical streams (engine-side) or vendor / driver branches (e.g. async slot 0 off on AMD,
   vkd3d differences).

## 5. New-renderer feasibility: first honest outline

The wall to beat on mid-range (pc2, R5 5600X + RX 6650 XT, ReBAR on, cpu-bound-midrange.md): **GPU busy 19.1 ms, frame 25.8 ms (M)**.
- [App]Game is 92-94 % busy. 6.1 ms of it is WaitDispatcherDrainSpin 418F40, which waits for the replay thread. The rest, ~19.7 ms,
  is draw prep, UI and logic glue.
- The dispatcher does ~20 ms of replay per frame. Of that, ~6 ms is blocked:
  - ResourceBarrier inside the driver: 7.5 %;
  - the SRW lock of SynSharedLockedUpdate 7F64DC: 6.1 %;
  - the ECL ring being full: 5.5 %.
- Its compute shares are: PSO B-tree lookups ~4 ms, constant-blob memcpy 1.8 ms, driver CBV / descriptor / upload work ~3.9 ms.
- Main-thread savings alone are absorbed by the drain (W1B: +0.1 %). The PCM memo gave only +0.5 %.
- So the replay **architecture** binds, not any single function.

| level | what is replaced | understanding required | effort (E) | risk | payoff |
|---|---|---|---|---|---|
| **(a) new D3D12 submission layer** behind the engine | the replay side: SynergisticDispatcherLoop + the 185 token handlers + list / ECL management. The engine keeps recording KTGL tokens. | (1) Token-stream format and every handler's D3D12 effect (backend package; FCAP1 caller RVAs map handlers ↔ calls). (2) Resource-state tracking and barrier rules. (3) The ring / fence / ECL protocol, async slots, present hand-off. (4) Descriptor / CBV management (SynUploadCbvTable). (5) FFX / Streamline interplay on the same lists. | 4-8 weeks of work to an A/B-able ASI | medium-high: state bugs show as corruption or device removal; vkd3d differences; must keep the engine's async fences | **Only level aimed at the measured wall.** Options:<br>- record N stream segments on N threads, each with its own SynCtx snapshot and list (segments already start at flush points);<br>- PSO handles resolved at record time, or a direct-mapped cache instead of the B-tree;<br>- one root-constant / bindless path instead of CreateConstantBufferView per draw;<br>- no SRW contention.<br>Estimate (E): if the replay drops below the main thread's ~19.7 ms, pc2 could go from 38 to ~50 fps (+30 %, the "main-thread ceiling" in cpu-bound-midrange.md §6). pc2 is the slow-link worst case (docs/BENCHMARKS.md), not a typical PC. The PC (RX 9070) gains where it is CPU-bound. The Ally is GPU-bound: ~0. |
| **(b) new frame graph** reusing the game's shaders and materials | the graph walker + node Execute / Record → our own pass scheduler (ordering, queue placement, barrier planning, transient aliasing), recording through (a) or the existing tokens | everything in (a), plus every node type's inputs / outputs, shader-object parameters and cbuffer layouts (70 node types, 216 shader objects), view gating, the draw-list / material path the nodes call (2.2: SCENE → PSO is the spine) | 3-6 months of work | high: per-node look parity; the long tail of 70 node types | GPU-side, same look: the meshlet front-end restructure 2-3.5 ms on pc2 in dense scenes (render-architecture §4.1); async scheduling 0-1.5 ms either sign; fusion ≤ 0.4 ms. CPU: little beyond (a). **Do it in pieces:** the meshlet front-end (nodes 14 / 35 / 299 + MPC / MIC recorders) is a self-contained sub-graph. |
| **(c) fully new renderer** fed by engine scene data | all of §2 except the game-side producers: we consume views / draw lists, MeshletManager, G1M models, materials, lights, effects, UI, nature systems | the scene handoff contract (07 + 08 + 10 + 11 + volumetrics inputs), 1600+ material shaders (rewrite or reuse), lighting / GI / VSM / volumetrics algorithms, post / FSR / FG, UI | years | very high: look parity is not reachable without the game's shaders, so this becomes (b) | about the same CPU gain as (a); GPU ≤ 3-5 ms same-look (engine-rewrite-options.md: no hidden 5-10 ms); unbounded only if the look may change. **Not worth it.** |

Recommendation:
1. Finish the backend + frame-graph + drawlists packages first. They are exactly the understanding (a) needs.
2. Then prototype (a) as a default-off ASI mode. Smallest useful step: two replay threads on alternating stream segments, each with its own SynCtx copy and command list, submitted in order. The ECL thread already serializes submission.
3. Measure it on pc2 with the CPU harness, the `cpu_live_sched` ABBA in cpu-bound-midrange.md.
4. Keep (b) to the meshlet sub-graph, gated on the SCHED / XSYS results.
5. This agrees with engine-rewrite-options.md (A: "a generic D3D12 scheduler is not worth building", D: "never"). That verdict was
   about GPU scheduling before ReBAR moved pc2 to the CPU wall. **For the CPU wall, a new submission layer is the lever.**

## 6. Files

- `re/scripts_renderer/render_map.py`, `partition.tsv`, `fcap_digest.py`, `export_renderer.ps1`;
- `tools/fcap_capture.ps1`;
- `mod/Nioh3EngineFix/src/fcap.h` (+ hooks in gpuprof / gpuopt / osd / psoswap);
- `captures/fcap/` (local builds, not committed).

All of these live in the private development repository.


