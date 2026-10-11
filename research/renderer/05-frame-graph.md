# renderer/05-frame-graph: how the render graph is built and run each frame (package 05, 2026-10-10)

Nioh3.exe Steam v2.02, RVAs (Ghidra base 0x7FF74F410000). Tags: **S** static (decompile / disassembly / vtable dump of `re\Nioh3.dump.exe`),
**M** measured (pc2 FCAP1, 5 captures x 2 frames, XSYS config, 1280x720 render res; an earlier run of the same tour has no
command-list calls; the Ally captures are near-empty). Inputs: render-architecture.md §2 (phase table), render-nodes.md (node chains),
backend.md (tokens, rings, barriers), materials.md §7 (shader / PSO reuse interface). No game launched. Symbols: block `# renderer/frame-graph`.

## 0. Answer

1. **It is not a compiled render graph. It is an authored, linear node list that is walked once per frame** (S). The graph is a KIDS
   asset (`*.kidsrender`, magic `_RGK`): 430 nodes for the main graph `0x102dd706`. Each record is `{type hash, id, hdr=(1, next)}` plus
   typed fields (render-target / shader *name hashes*, ViewID, flags). The engine builds no dependency DAG. There is no topological sort,
   no pass culling, no resource-lifetime analysis at runtime, no barrier planning and no queue assignment. Execution order = file order.
2. **Per-frame entry (S):** `MainLoopFrame 1A74130 -> FrameStep 3D1F20 -> FrameStepRender 112DD8`. FrameStepRender calls
   `Renderer->vfn+110h` (RenderFrameBegin 4A529C). It then walks the list, calling `FrameRenderViews 113204` once per node. That function
   is the *per-entry execute*: optional marker, QPC stats, then `node->vfn+8 Execute`. After the walk it calls `vfn+118h`
   (RenderWaitForFrame 4A4F94, frame end) and `vfn+120h` (GfxDevicePresentVfn 5DA94 -> GfxDevicePresent 3D62E4, the backend's drain +
   submit + present). One thread ([App]Game) records the whole graph into the KTGL token stream, and the dispatcher replays it (backend.md).
3. **Control flow is data-driven and decided per node (S):**
   - `BeginCheckSkip` / `EndCheckSkip` push / pop a skip bit (Renderer+968h bit 31) with 49 condition types. Every Execute checks the bit
     and its own view slot; the walker does not skip anything.
   - `Compound` and `RenderGraph` nodes run sub-graphs with a re-based view index (Renderer+974h).
   - `ProfileEvent` emits markers. `BeginScene` / `EndScene` push / pop render-target scopes on the device.
   - render-nodes.md put these five on the shared base vtable 40C9C48 with a `_purecall` Execute. **That is wrong** (§2.2): each one has
     its own vtable and Execute.
4. **Resources are persistent KIDS objects referenced by name hash, with one exception: transient aliasing (S + M).**
   - RTs live in the system objdb `0x93fb3280` (769 RT objects) and are created at load / resize.
   - Objects placed in a `VideoMemoryKeeper` heap carry a *first-use global node index* (+138h). VideoMemoryKeeper is a KIDS type: factory
     259F5AC = GetResourceAllocationInfo + heap.
   - Before each node, the walker hook 113150 looks for keeper objects whose index equals the current node index. For each one it emits
     **token 0x9D, an ALIASING barrier with before = NULL, then token 0x9F, DiscardResource**.
   - FCAP1 confirms this: **exactly 12 aliasing barriers + 12 DiscardResource per frame** in every captured frame, on 1280x720 and
     640x360 targets. This is the only barrier the graph schedules.
5. **All other barriers are generated lazily by the dispatcher** at bind time. SynTransitionResource 115E24 stores the state in the
   resource; UAV barriers come from tokens 0x8A / 0x7D (backend.md §3). Per frame (M): 780-970 transitions, 300-950 UAV barriers,
   12 aliasing.
6. **Queue assignment is hard-wired in node code, not in the graph (S + M).**
   - DIRECT runs everything except three async-slot passes, which node Executes record into async-slot contexts: ocean (UltraMarine
     node 138, slot 6), VFSCAT (VolumetricFogScatter 162, slot 4) and VCLOUD (NatureSky 267, slot 3).
   - The COPY queues carry streaming / tile-cache work only.
   - **Measured overlap is poor for two of the three slots** (§5). Each async list overlaps exactly one DIRECT list before the DIRECT
     queue waits. For VFSCAT that list has 14-37 draws; for VCLOUD, 27-46 draws.
7. **Node order validated against FCAP1 (§5):** the 26 engine markers per frame fall in graph order, and the phase table of
   render-architecture §2.2 holds, with three corrections:
   - The main-view phase-2 / Hi-Z block (nodes 101-120) did not run in any captured frame. The only `GenerateSceneHiZ` is node 203's.
   - The VCLOUD dispatch is recorded at node 267, not node 4.
   - The VFSCAT dispatch reaches the GPU after the second-view culling (~node 203), not at node 162.
8. **Plug-in verdict (§7):** the cleanest seams are:
   - (a) **token 0x8F**, a native callback with the engine's command list on the replay thread, emitted from a node;
   - (b) the **empty `Compound` slot node 399** (CompoundRenderGraphTable 0xbe4baa94, keys / graph files empty), right before the post chain;
   - (c) **per-node vtable +8 replacement** for whole passes; the meshlet front-end (237044 / 44DE78) is the one worth it;
   - (d) a **re-authored `_RGK` file** (same node types, different order / gating) for data-only reorders.

## 1. Entry points (S)

| RVA | name | role |
|---|---|---|
| 1A74130 | MainLoopFrame | [App]Game frame loop; calls FrameStep |
| 3D1F20 | FrameStep | Takes `host`, the KIDS render host (render-nodes.md calls it RenderCtx `r`). Swaps the scene snapshot (pending host+138h -> current host+140h; calls `vfn+18h` on the new one). Reads the active graph (host+128h, list head at +28h) and the parity byte host+1E1h, then calls FrameStepRender |
| 112DD8 | FrameStepRender | `R = host+F8h` (the Renderer). `R->vfn+110h(host, parity, scene)` = frame begin; false skips the frame. Per entry: `R->vfn+130h(entry)` (113150, transient activation), `FrameRenderViews(entry, host, parity, scene)`, `R->vfn+70h` after every 4th entry that returned true (`ret 0` on PC: a kick slot used on other platforms), `R->vfn+138h` (AA3980: `++R+1EE568h`, the global node index). Then `vfn+118h`, `vfn+120h` |
| 113204 | FrameRenderViews (= per-entry execute) | If entry+4 < 0 and the skip bit is clear (`R->vfn+F8h` = GfxCtxFlag968Sign 1134CC): BeginEvent with the node name or `"Type:0x%08x,Name:0x%08x"` (`R->vfn+58h` 1D29B4 -> GfxBeginMarker; end `vfn+60h` 113838 -> token 0xA6). Then `node->vfn+8(node, host, parity, scene)`. If entry+40h is set: QPC duration into a 10-slot ring + min / max (spinlock) |
| 32FEE04 | (sub-list walker) | The same loop for a sub-graph's list; used by the Compound and RenderGraph Executes |
| 4A529C / 577E50 | RenderFrameBegin / RenderFrameBeginThunk | `vfn+110h` of Renderer vtables 40D4A78 / 3AAF600: latency markers + Streamline queries, WaitSubmitThread 7D9B6C, MeshletFrameBegin 5D214, RenderFrameBeginCacheFlush 32FE938, **R+1EE568h = 0** (node index), transient-set prep 94541C(R+1EE530h, ...) per active view |
| 4A3334 -> 4A4F94 | RenderWaitForFrame | `vfn+118h`: per-view snapshots into Renderer+13D0h, MeshletRequestLoadStep 5DC18, debug overlay, end-frame virtuals R vfn+188h..+250h |
| 5DA94 | GfxDevicePresentVfn | `vfn+120h` -> GfxDevicePresent 3D62E4 (backend.md §1) |
| 25BA3E8 / 25BA540 | (graph file loaders) | `_RGK` / `_TGK` (same container): check magic, version `"0000"`, header 20h; allocate 28h + 16*count + data. 3249B30 builds the record index `{record*, ordinal}` (records 4-aligned, size at +8) |
| 26F5D84 | (type registry builder) | Registers the 70 render-node types (RegisterAssetHandler 3246E9C); factory = handler-table slot 22 |

**Per-node `Execute` contract (S, all 70 types):** `bool Execute(Node* n, Host* host, u32 parity, Scene* sc)`.
- The return value means "did work"; it only feeds the every-4th kick counter.
- `+10h ResourcesReady` is polled at load (ResourceIsReady 1A6030). `+30h` is Release.
- Types with deferred preparation (the denoisers 4109CA0 / 4109CE8) use RenderNodeExecuteIfPrepared 367BD44: `if (vfn+40h() == 1) vfn+38h()`.

## 2. Data structures (S)

### 2.1 Graph file and runtime entry
- **`_RGK` header (0x20 B):** magic, `"0000"`, header size 20h, ?, **u32 node count at +10h**, data size; then the records.
- **Records:** each is a KIDS object: type hash, object id, `hdr = (1, next ordinal)`, then the typed fields printed by
  `data/dumps/kgk_*.kidsrender.txt`. The main graph is a pure chain 0 -> 1 -> ... -> 429. Fields name everything by hash:
  `*TextureObjectNameHash`, `*ShaderObjectNameHash`, `ViewID`, `CheckSkipType`, `Flag`, `KTGLShaderRenderStateOffset`.
- **Runtime entry** (one per node, a KIDS object handle):
  - `+04 i32 flags` (bit 31 = per-node GPU marker);
  - `+20h TypeDesc*` (+0Ch type hash, +10h name hash);
  - `+28h name obj` (string at obj + [obj+14h]);
  - **`+30h Node*`**, **`+38h next`**, `+40h stats*`.
- **Node object:**
  - `+0 vtable`, 7 slots: dtor, Execute, ResourcesReady, 3 defaults AA0140/AA0130/AA0120, Release.
  - `+20h u32`, packed per-node byte fields: bits 0-6 = CheckSkipType (BeginCheckSkip), bits 8-11 = MirrorID, **bits 24-30 = ViewID**.
  - Compound: +20h sub-graph handle, +28h CompoundRenderGraphTable handle, +30h ViewIDOffset (-1 = keep), +34h key.
  - RenderGraph: +20h / +28h lazily loaded sub-graph handles, one per view slot in the table at 3FDC278 = {0, 6}.

### 2.2 Structural node types (corrects render-nodes.md)
| type | ctor | vtable | Execute | semantics |
|---|---|---|---|---|
| BeginCheckSkip | 345E4F4 | 40EBCB8 | 153014 (3.6 KB) | Pushes `R+968h>>31` to `R+AD4h[R+AC0h++]`. View = ((n+20h>>24)&7Fh) + (R+974h & 7FFFFFFFh). Sets bit 31 if the view is absent or if condition `n+20h & 7Fh` (0..30h) fails. The conditions read view / scene state: 2Bh/2Ch/2Fh/30h test R+96Ch bits 400000h/800000h/10000000h/20000000h, 2Dh tests sc+2B0h->+2Ch bit 24, 6 and 0Bh test R+968h bits. Two types also record the water / MRLR scene copies (`App_CopyMRLROcnScene`, `App_CopyMRLRScene` markers). It never clears the bit, so nesting works |
| EndCheckSkip | 345E600 | 40EBC80 | 1114B8 | Pops: `bit31 = R+AD4h[--R+AC0h]`; returns false |
| Compound | 345E584 | 40EBC48 | 969F80 | If the sub-graph resolved: optionally sets R+974h = ViewIDOffset (restored after) and walks its list (32FEE04). The table is resolved at load, by key (345970C) |
| RenderGraph | 345EA84 | 40EBC10 | 3465B04 | For view slots {0, 6} present in the scene: loads the sub-graph by file hash (once), sets `R+974h = slot \| 80000000h`, walks it. If both ran, `R+96Ch \|= 2` (multi-view). Used by the wrapper graph 0x4c54a89c (sub-graph 0x024496ee) |
| ProfileEvent | 345EA3C | 40EB818 | 3465ACC | n+20h bit 1: GfxEndMarker; bit 0: GfxBeginMarker(MarkerString). The main graph does not use it |
| BeginScene | 35578C4 | 40F9028 | 1115B0 | Pushes a 268h-byte pass record on the device (GfxDevice+2538h, depth +2544h): RT / DSV objects resolved from the node's hashes (or from the parent scope's), then the set-RT tokens |
| EndScene | 3557A7C | 40F9098 | 1114FC | Pops the pass record (skip-gated) |

`B93638` (`_purecall`) remains only in the base vtable 40C9C48 (RenderNodeBase). RenderNodeBaseCtor 3451994 and the shared dtor 32545D0 set it
before each derived ctor overrides it.

### 2.3 Renderer (host+F8h; vtables 40D4A78 and 3AAF600) and the scene snapshot
| offset | field |
|---|---|
| R+70h | GfxDevice (token write ptr +3C40h, markers on +2A62h, pass records +2538h / +2544h) |
| R+968h | frame flags: **bit 31 = skip active**; bit 0, bit 3, bit 10 (400h, View) and bit 20 (RLR) are set by nodes. Copied from the RenderParams block by RenderParamsAssign 247950 (ViewSetting node) |
| R+96Ch | feature flags read by CheckSkip types (bit 1 multi-view, bit 2 SkyAmbient, 4000000h, 8000000h SSAO, ...) |
| R+974h | view base for the current (sub-)graph; bit 31 = set by RenderGraph |
| R+AC0h / R+AD4h | CheckSkip stack depth / u32 stack |
| R+1EE530h | transient set (embedded object): +18h / +1Ch counts, +30h `VideoMemoryKeeper*` array; R+1EE558h float 1.0 |
| R+1EE568h | **global node index** (reset in RenderFrameBegin; ++ after every entry, sub-graph entries included) |
| sc+180h | `View*[32]`; a NULL slot disables every node whose view index points at it |
| sc+2B0h | per-frame settings block (CheckSkip type 2Dh, RenderFrameBegin) |

**View gating**, shared by almost all Executes:
- `idx = ((n+20h >> 24) & 7Fh) + (R+974h & 7FFFFFFFh)`. Some types keep the ViewID byte at +5Bh, +14Bh or +B13h instead.
- The node runs only if `idx < 32 && sc+180h[idx] != NULL`; otherwise it returns false.
- The 58 `View` nodes (Execute 10F060 -> SceneRenderView 535188, drawlists package 07) select which draw lists a scope renders.

## 3. Node type -> Execute table (70 types; 65 used by `0x102dd706`)

Hash = type hash (the RegisterAssetHandler argument). Execute is vtable +8. `?` = not resolved here. Sources: the symbols.tsv render-node
rows, plus the vtables dumped for this note (structural types, ComputeSkinning, SimulationMeshUpdate, MeshletResolveLODPass,
MeshletGenerateHiZMap, BeginScene, EndScene, OBOROExport).

| type | hash | n | nodes in 0x102dd706 | factory | vtable | Execute | Ready |
|---|---|---|---|---|---|---|---|
| ViewSetting | 0x5F0A8446 | 1 | 1 | 25B9248 | 4035858 | 4A4DF8 | 3468120 |
| UltraMarine | 0x1872E683 | 3 | 2, 138, 240 | 25B7064 | 410A498 | 852620 | ? |
| SignedDistanceField | 0xEBFE38E8 | 4 | 3, 136, 253, 341 | 25B8ED8 | 410A1C0 | 10C240 | 3689104 |
| NatureSky | 0x1709F9D7 | 2 | 4, 267 | 25ADE2C | 40EC750 | 293220 | 346F864 |
| RainDrop | 0xEBA1DCF4 | 4 | 5, 332, 333, 334 | 25B17F0 | 40EC868 | 419F54 | 346FBA4 |
| ClusterDecalCulling | 0x44B44454 | 6 | 6, 7, 31, 32, 33 ... 199 | 25A54A4 | 4109900 | 52BAFC | 3672224 |
| SkyAmbient | 0xCAD70BC8 | 1 | 8 | 25B48AC | ? | 145C60 | ? |
| Player | 0x7AEB0B64 | 1 | 9 | 25B7418 | 4051C70 | 90BCA4 | 3468090 |
| GrassVertexDisplacement | 0x9BA40B40 | 1 | 10 | 25AB6F4 | 40EBAB8 | 52AF08 | 3467984 |
| Tree2Wind | 0x1FCCFA95 | 1 | 11 | 25B6DD4 | 40EBA80 | 41805C | 3467F74 |
| ComputeSkinning | 0x3963FB49 | 1 | 12 | 25A7928 | 40EB9D8 | 1F2F78 | 3467960 |
| SimulationMeshUpdate | 0x9A0B24CE | 1 | 13 | 25B4770 | 40EB9A0 | 1F2C10 | 3467F2C |
| MeshletResolveLODPass | 0xCDE9D67D | 3 | 14, 35, 299 | 25ADCA8 | 40EB968 | 237044 | 3467BA8 |
| BeginCheckSkip | 0x5C8B904B | 52 | 15, 36, 50, 55, 60 ... 420 | 25A4F78 | 40EBCB8 | 153014 | - |
| BeginScene | 0xE2C0442A | 52 | 16, 25, 37, 43, 44 ... 410 | 25A506C | 40F9028 | 1115B0 | 3557E0C |
| ClearScene | 0xCD713E46 | 17 | 17, 26, 38, 45, 52 ... 423 | 25A52D4 | 40F9060 | 8236D8 | 3557E10 |
| View | 0xFC64DCB6 | 58 | 18, 19, 27, 30, 39 ... 416 | 25B76EC | 4109FB0 | 10F060 | 368027C |
| EndScene | 0x1F7C5CF8 | 52 | 20, 28, 41, 49, 53 ... 419 | 25A93CC | 40F9098 | 1114FC | AAD1A0 |
| EndCheckSkip | 0x86A8B2FD | 52 | 21, 42, 54, 59, 66 ... 429 | 25A9374 | 40EBC80 | 1114B8 | - |
| SnowDeform | 0xE76D8639 | 1 | 22 | 25B49F0 | 40EC8D8 | 44BE2C | 346FCAC |
| TerrainCopyToTextureAtlas | 0x46A0BFBC | 1 | 23 | 25B632C | 40EBB68 | 578AC0 | 3467F50 |
| VirtualShadowMapPass | ? | 1 | 24 | 25B9DA8 | 410A5F8 | 465384 | 368BD8C |
| LightPass | 0xFA05A5EA | 1 | 29 | 25AC0DC | 4109F40 | 7E919C | 367FDB0 |
| ClusterLasmapCulling | 0x8764D2C9 | 1 | 34 | 25A586C | 41098C8 | 52A9EC | 3672320 |
| StaticTerrain | 0x1C8F048A | 5 | 48, 91, 97, 127, 206 | 25B5B84 | 4109F78 | 7D1810 | 367FFF0 |
| OldEffect | 0xC3A7FC79 | 16 | 98, 195, 196, 306, 321 ... 426 | 25AE634 | 4035890 | 14FAD8 | 3557748 |
| MeshletGenerateHiZMap | 0x3E493DAB | 2 | 102, 203 | 25ADB38 | 40EB850 | 44DE78 | 3467AF8 |
| GBufferEdit | 0xA82D6D40 | 7 | 122, 131, 140, 215, 258 ... 337 | 25AA4E0 | 4109A90 | 10C22C | 36737B8 |
| PrepareAccumulatedSnow | 0x4BC3E66F | 1 | 125 | 25B0C2C | 40EC7F8 | 8F45F0 | 346FA38 |
| DeferredIndirectPass | 0x93FFBE0F | 5 | 134, 149, 209, 252, 340 | 25A7CB4 | 4109FF8 | 149A94 | 3684780 |
| GBufferEditMiasmaDecal | 0x4D4677CB | 1 | 143 | 25A9AD8 | 4109AE8 | 937D68 | 3675D60 |
| Ripple | 0xC8A6C3C1 | 1 | 145 | 25AAFBC | 4109A50 | 1031E4 | 3673A5C |
| SSAO | 0x406CB6FF | 1 | 147 | 25B34C4 | 410A1F8 | 15612C | 3688C18 |
| ReflectionProbePass | ? | 7 | 150, 153, 243, 246, 292 ... 348 | 25B2AC0 | 410A428 | 54294C | 368A9E4 |
| ReflectionProbeDenoiser | ? | 2 | 151, 244 | 25B2850 | 4109CE8 | 367BD44 | 367BEA4 |
| RaytracingReflection | 0x7A7524F4 | 2 | 154, 247 | 25B1C70 | 410A588 | 9BA390 | AB0C30 |
| RaytracingReflectionDenoiser | 0xB965282D | 2 | 155, 248 | 25B1B74 | 4109D30 | 9BA3A0 | AB0B50 |
| MergeRaytracingReflection | 0x5D1E1136 | 2 | 156, 249 | 25ADA4C | 410A550 | 9BA380 | AB0C10 |
| OBOROExport | 0xE05BF624 | 4 | 159, 255, 294, 343 | 25AE3A0 | 410A630 | 90EC18 | 368BF18 |
| ClusterLightCulling | 0x8C60743D | 1 | 161 | 25A5B6C | 4109970 | 3D0AE0 | 36723F4 |
| VolumetricFogScatter | 0x3C0512C3 | 1 | 162 | 25B9F84 | ? | 14871C | ? |
| ClusteredDeferredShading2Pass | 0xF103DB00 | 12 | 167, 172, 175, 177, 178 ... 346 | 25A5E6C | 4109BD0 | 545CC4 | 3679B64 |
| SnowScattering | 0xCDEC80D8 | 2 | 169, 211 | 25B4C24 | 40EC8A0 | 90E718 | 346FDF0 |
| SSSS | 0xA033E931 | 1 | 188 | 25B3D1C | 4109B28 | 981390 | 367A0C0 |
| ForwardScreenMap | 0x0A2D845C | 1 | 193 | 25A98B8 | 4109A10 | 945550 | 3673708 |
| PrepareStaticNatureReflaction | 0xFD1176C6 | 1 | 202 | 25B1058 | 40EC7C0 | 1528B4 | 346FB00 |
| CombineHDR | 0x88959D48 | 1 | 219 | 25A75A0 | 40EBAF0 | 10579C | 34678E0 |
| MergeRLR2Pass | 0xE1FE052E | 4 | 226, 234, 284, 297 | 25ACC08 | 410A0A8 | 9A6444 | 3687154 |
| CopyColorRenderTarget | 0x97299F9C | 5 | 235, 406, 417, 422, 427 | 25A7A64 | 4035820 | 8493A4 | 3557E34 |
| StaticNature | 0x197E61C6 | 4 | 241, 262, 275, 288 | 25B4F4C | 40EC6E0 | 6D7B88 | 346FEE8 |
| PrepareDownSampleStencilTexture | 0xF8FC9D1F | 1 | 265 | 25B0EE0 | 40EBBA0 | 953560 | 3467C58 |
| PrepareThinSurfaceBSDF | 0xC41F73B1 | 2 | 270, 300 | 25B12A8 | 410A070 | 453ACC | 3687500 |
| RealtimeLocalReflectionPass | 0xAC078655 | 1 | 290 | 25B1D84 | 410A0E0 | 295C44 | 368765C |
| UltraMarineEffectGBufferEdit | ? | 1 | 313 | 25B6FBC | ? | 95E34C | ? |
| EdgePass | 0x9F649B3F | 2 | 370, 376 | 25A8DEC | 410A310 | 5EFE20 | 3689D78 |
| SilhouettePass | 0x8CBBC1DA | 2 | 383, 385 | 25B4474 | 410A268 | 916F24 | 368A124 |
| HighlightPass | 0x29A0304C | 2 | 384, 387 | 25ABB84 | 410A2D8 | 918F40 | 3689EFC |
| PatternedOcclusion | 0xA22E66AF | 1 | 395 | 25AF218 | 410A150 | 95E0A8 | 3688B78 |
| Compound | 0xD72D4D3E | 1 | 399 | 25A776C | 40EBC48 | 969F80 | - |
| SSFadeSaturation | 0x65508E9F | 1 | 401 | 25B3934 | 40EB930 | 927DF8 | 3467D6C |
| HdrOutput | 0x28916AD0 | 3 | 408, 421, 428 | 25AB8B0 | 40EB7D8 | 6D8928 | 3467A08 |
| Apply | ? | 1 | 409 | ? | 40EC640 | 945350 | 346D49C |
| Fade | 0x952CC20D | 1 | 413 | 25A9474 | 40EC608 | 294C04 | 346D440 |
| StaticScreenLayout | 0x48570415 | 2 | 418, 424 | 25B55E4 | 40EAF20 | 253C68 | 34538DC |
| ApplySSAOPass | 0xEA12CD84 | 0 | - | 25A4BB4 | 410A188 | 3688234 | 3688A8C |
| ClusteredDeferredShadingPass | 0xFBD368B4 | 0 | - | 25A6B34 | 4109B98 | 3677264 | 3679E14 |
| DrawSprite | 0x8DBC11DB | 0 | - | 25A89E0 | 4109820 | 3671988 | 3671C18 |
| Fluid | 0x842CCDC7 | 0 | - | 25A960C | 40EC718 | 346EC7C | 346F79C |
| MergeRLRPass | 0x84E77AE2 | 0 | - | 25AD3C8 | 410A038 | 3685BD0 | 368733C |
| OutlineAdditionPass | 0xE25493E2 | 0 | - | 25AEF94 | 410A2A0 | 3689664 | 368A080 |
| ProfileEvent | 0x8392F933 | 0 | - | 25B1688 | 40EB818 | 3465ACC | B93638 |
| ReflectionPass | 0xB06B232D | 0 | - | 25B25D0 | 410A118 | 3685BE4 | 3687848 |
| RenderGraph | 0xC10C91F9 | 0 | - | 25B337C | 40EBC10 | 3465B04 | - |
| Script | 0xC69D954D | 0 | - | 25B413C | 40EBA48 | 3465C50 | 3467E68 |
| TileLightCulling | 0x8B4804A1 | 0 | - | 25B644C | 4109938 | 3672030 | 3672568 |
| TiledDeferredShadingPass | 0x6E817DF7 | 0 | - | 25B66BC | 4109B60 | 3678CB4 | 367A288 |

## 4. Per-frame flow, resources, barriers, queues (S)

**Build (load time).**
1. The `_RGK` file is loaded (25BA3E8 + 3249B30), and the KIDS object system instantiates one node per record through the type's factory
   (slot 22). Factories read the typed fields (PropTableReadParamsBatch16 2F3D08) and resolve every name hash to a KIDS object handle:
   RT (system objdb 0x93fb3280), shader object, view object, sub-graph. They then call the type's ctor.
2. The list is linked in `hdr` order. ResourcesReady gates readiness.
3. Nothing is compiled. A different graph is a different file. The game ships 20 graph files (`data/dumps/kgk_*`), from 4 nodes (ViewSetting + ClearScene + SSSS)
   to 431. FrameStep swaps the active one through host+128h.

**Run (every frame).**
1. RenderFrameBegin resets the node index and does the frame-begin work.
2. For node k = 0..N (sub-graph nodes included, in place):
   - `R->vfn+130h` activates the transient objects whose first use is k (aliasing barrier + discard tokens);
   - the per-entry execute records tokens;
   - k++.
3. RenderWaitForFrame, then Present.

All recording is on the main thread, in node order. The only parallelism is the dispatcher replaying earlier stretches (backend.md),
plus the jobs some nodes start for their own draw lists (drawlists / effects packages).

**Resource declarations and lifetimes.**
- Nodes declare reads / writes only implicitly: by the RT hashes in their fields, and by what their code binds.
- Lifetimes are "whole session" for objdb RTs.
- For keeper-placed RTs they are "from first-use node index to the next object in the same keeper". The keeper slices one heap, and each
  activation is an aliasing barrier with `pResourceBefore = NULL`, meaning any previous occupant.
- FCAP1 (M), every frame: 12 activations.
  - 1280x720 targets: 3 with format 0x5A, 7 with format 0x09.
  - 640x360 targets: 2 with format 0x09, one of them with 6 mips. These formats are DXGI 90 / 9, B8G8R8A8 / R16G16B16A16 typeless, if the
    FC `res` column holds DXGI codes.
  - They fall in five places: 2 at the end of the G-buffer, 4 around clustered deferred shading 167-186, 2 in the second-view G-buffer
    (~205), 2 near the second-view deferred block (~252), 2 before the edge / highlight passes (~370-384).
- **Question for 06 (resources):** no `VideoMemoryKeeper` object appears in the dumped objdbs. Where are the keepers and their RT
  members defined, and who writes the first-use index (+138h)?

**Barriers.** The graph schedules none apart from the activations above. The dispatcher transitions on bind, from state tracked in the
resource object; it never batches, and buffers decay at list boundaries (backend.md §3). Consequences for a new frame graph:
- the engine has **no pass-level barrier batching** to reuse;
- a native pass must leave engine resources in the state the tracker believes they are in (or update the tracker).

**List cuts and fences relative to nodes (M, pc2 fcap_4 frame 0; backend.md §2 has the cut causes).** 26 DIRECT lists per frame (18-28
over all captures). Engine cuts fall at:
- frame begin (L4-L6: MeshletCheckRequestLoad, clears);
- after node 35 (L7: setup, LOD resolve, shadows, culling ph1);
- after the G-buffer + GBufferEdit + DeferredIndirect 134 (L8, ~1000 draws);
- at every async kick and join;
- at post (L21-L22), UI (L23) and output (L24).

Each async slot cuts the DIRECT list twice: the work list is closed and submitted, and then a join token comes before a later list.

**Queues (M).** Per frame:
- DIRECT: 18-28 ECLs, all from EclSubmitAndSignal (4F9114).
- COMPUTE: 4-6 ECLs, two lists per slot (an empty list carrying the Wait / Signal, then the work list).
- DIRECT waits:
  - 2-3 async joins through ECL token 2 (EclWait 4F9323), on the slot fences;
  - 3 waits through GfxWait 8F4583, on the streaming COPY queue's fence (892E19 signals), at frame begin and end.
  - Note for 01: backend.md §4.1 calls the 8F4548 waits the async joins. By fence object they are the COPY-queue waits; the 4F9323
    waits are the joins.
- COPY: streaming only.

## 5. Validation against pc2 FCAP1 (script logic in this note's §8)

Graph phase (render-architecture §2.2) -> what the capture shows, in order (fcap_4 frame 0; the other 9 frames have the same structure):

| DIRECT list | markers / shader families (gp2_hash_index names) | graph nodes |
|---|---|---|
| L4 | MeshletCheckRequestLoad, MCLRC / MGLRC, depth clear | RenderFrameBegin (before node 0) |
| L5-L6 | 1 + 2 draws, S2FP | P0/P1 setup |
| L7 | WaveManager, AreaManager (UltraMarine 2); RenderSkyPatternMap (NatureSky 4 update); GVD 10; TREE2WIND 11; **MeshletResolveLOD + "0".."13"** (14); CBC / CLC; Cascade 0-2 x2 (24, 29); MIC + MPC (35) | 2-35 |
| L8 | PB2 / PB2TREE2 / PB2EYEC / STD / PB2LS G-buffer (~750 DrawIndexed + 230 ExecuteIndirect, **0 dispatches**), 2 x alias+discard, PB2DECAL + copy (GBufferEdit 122), WDO (DeferredIndirect 134) | 36-134 |
| (C) L9/L10 | PFB, IGN, IGP, IGW (+ BWVM) ocean FFT chain, 10-14 dispatches | async slot 6, recorded by UltraMarine 138 |
| L11 | DYM, UltramarineCaustics (CTS), UltramarineWet (WET), GPUCulling Setup / GPUCulling, HBAO (SSAO 147), ESM (DeferredIndirect 149), RefProbe + PB2DUS (150-153) | 138-159 |
| L12 | PB2DS x5 (ClusteredDeferredShading2Pass 167-186) with 4 alias+discard, ARPSSSShader (SSSS 188), **GenerateSceneHiZ** (GHDM, node 203), MIC + MPC (second-view culling) | 167-203 |
| L13 | copy, GFM_TextureClear, GFM / GFM-D | ~202-205 |
| (C) L14/L15 | VFSCAT 641524ee + fcc40d4b | async slot 4 |
| L16 | DEPTH / SM depth for view 2, 2 alias+discard, PB2 second-view G-buffer, RefProbe, WDO (209), PB2DS (212/217), App_CopyMRLROcnScene, UltramarineRender (240), UMTV, SRIVER (StaticNature 241) | 197-241 |
| L17 | RefProbe 243, 2 alias+discard, WDO 252, PB2DECAL (GBufferEdit 258/259), **NUpdate > DrawSky** (NatureSky 267, SKY2PLN) | 243-267 |
| (C) L18/L19 | VCLOUD 5f17ca9d / 42ed52ed (b76874bb), 1 dispatch, **no Wait on DIRECT** | async slot 3, recorded inside node 267 |
| L20 | DrawScreenSpaceVelocity, GFM (PrepareThinSurfaceBSDF 270), PB2DS 272, App_CopyMRLRScene, SSFOG (StaticNature 275/288), GFM-Hiz, PBRLR (290) | 270-292 |
| L21 | PB2MRLR (MergeRLR2 297), 2 alias+discard, CustomHighlightPass, TOONEDGE2 (EdgePass 370/376), forward PB2, FSR3:SaveOpaqueColor, G1E effects (OldEffect), EffectMergeShrinkBuffer, **FSR3** passes (Apply 409) | 297-409 |
| L22 | DebugFSR3CopyUpscaledDepth_, motion blur, bloom, compose, tonemap, heat haze (Apply 409, output res) | 409 |
| L23 | OffScreenRendering, AMBSL (StaticScreenLayout 418 / 424) | 417-424 |
| L24 | VideoOut (HdrOutput 421 / 428), MeshletRequestLoad | 427-428, RenderWaitForFrame |

Result: **the observed order matches the graph order end to end.** Corrections to render-architecture §2.2 / render-nodes:
1. **Nodes 101-120 (main-view Hi-Z + phase-2) did not run** in any of the 10 captured frames.
   - The G-buffer list has 0 dispatches in fcap_4 (6-28 elsewhere, all decal / G-buffer-edit work).
   - Each frame has exactly one `GenerateSceneHiZ`, which sits after deferred shading, i.e. node 203 (second-view block).
   - The main view's occlusion therefore uses the previous frame's Hi-Z (single phase). Node 102's BeginCheckSkip condition is the gate;
     its type is in the dump.
2. **Async kicks are not where the producer nodes sit in the graph.**
   - VCLOUD is recorded in node 267 (inside `NUpdate > DrawSky`), not node 4. Node 4 only does `RenderSkyPatternMap`.
   - VFSCAT's dispatch reaches the stream after node ~203. VolumetricFogScatterNodeExecute 14871C records into the slot-4 context
     (AsyncSlotGetCtx 52BE1C, VolumetricFogScatterRecordAsync 90749C, AsyncSlotAddFence 148514). The replay order shows the slot context
     is drained and kicked later.
   - **Question for 09 (volumetrics) / 01 (backend):** where does the slot stream get replayed, and can the kick move up to node 162?
3. **Overlap windows (M).** ECL order is: [DIRECT list before the kick] -> C(wait) C(work) -> exactly one DIRECT list -> DIRECT Wait.
   - Ocean overlaps the whole G-buffer list (~1000 draws): good.
   - VFSCAT overlaps L13 (14-37 draws, 0-17 dispatches).
   - VCLOUD overlaps L17 (27-46 draws).
   - On pc2, VFSCAT costs ~1.3 ms and VCLOUD 0.5-2.5 ms (render-architecture §2.1), so DIRECT likely idles at the joins. This matches the
     "async contends rather than overlaps" question there. The join position is the consumer's AsyncSlotAddFence. Moving the joins later,
     or the kicks earlier, is the cheap same-look experiment.

## 6. Dependencies

- **01 backend:** the token stream and every replay-side effect (state shadowing, barriers, list cuts, ECL ring, async slot replay).
  The frame graph only records.
- **07 drawlists:** `View` node Execute 10F060 -> SceneRenderView 535188 is where the scene snapshot's draw lists enter. sc+180h views are
  the scene input contract.
- **08 meshlet:** MeshletResolveLODPass (Execute MeshletSceneViewNodeRecord 237044; nodes 14 / 35 / 299), MeshletGenerateHiZMap 44DE78 (102 / 203), MeshletFrameBegin / RequestLoadStep.
- **06 resources:** objdb RTs, VideoMemoryKeeper heaps (259F5AC), the transient set R+1EE530h.
- **09 / 03 / 04:** async slot producers and joins (UltraMarine, VFSCAT, NatureSky); light / post node Executes.

## 7. Plugging in a new frame graph while reusing the game's shaders

What any replacement must honour (S):
- the Execute contract and gating: skip bit R+968h, view index rule, sc+180h;
- the global node index, if transient RTs are touched;
- BeginScene / EndScene pass records, if it records inside a scope;
- the resource-state tracker (resource objects) and the SynCtx shadow state: a native D3D12 pass must restore or dirty it
  (**question for 01:** which SynCtx dirty bits must a 0x8F callback set?).

Options, cheapest first:

| level | how | reuses | cost / risk |
|---|---|---|---|
| Data-only graph edit | ship a modified `_RGK` (same types and fields): reorder blocks, change CheckSkipType / ViewID, drop nodes; or fill CompoundRenderGraphTable 0xbe4baa94 to insert a sub-graph at node 399 | everything | days; limited to existing node types; async kick / join placement is in code, not data |
| Native pass at a node boundary | hook one node's Execute (vtable +8 patched in memory by the ASI, never on disk) to emit **token 0x8F {fn, arg}**; `fn` gets the engine's ID3D12GraphicsCommandList at exactly that replay position | engine RTs (by KIDS handle -> resource), engine root signatures (materials.md §7.3), DXBC from `shader_dx12` (materials.md §7.1-2) | 1-2 weeks; state-restore rules (above) are the risk |
| Replace a pass family | swap the Executes of a node type (e.g. MeshletResolveLODPass (MeshletSceneViewNodeRecord 237044) + MeshletGenerateHiZMap 44DE78 + the culling chain) with native implementations behind 0x8F callbacks, keeping their outputs (draw args, index stream, Hi-Z) byte-compatible for the G-buffer View nodes | everything downstream unchanged | 00-map level (b): 2-3.5 ms GPU on pc2 in dense scenes; weeks; needs 08's buffer layouts |
| New graph executor | replace FrameStepRender's walk (112DD8) with a planner that knows each node's reads / writes (from the hash fields), batches barriers, places async kicks / joins by dependency, and still calls the node Executes | all node code | months; little gain unless combined with the backend rewrite (00-map level a), because recording is not the bottleneck and barriers are emitted by the replay, not the graph |

**Verdict.** Keep the walker and the node set. They cost ~0 CPU (render-nodes.md: Executes have ~0 self time), and the authored order
is correct. The measurable frame-graph-level wins are placement wins:
- move the VFSCAT / VCLOUD joins later, or their kicks earlier (§5.3);
- check why main-view phase-2 culling (101-120) is gated off (§5.1);
- possibly batch the 12 activation barriers.

Pass replacement belongs to the meshlet package. A new graph executor only pays together with a new backend.

## 8. Method and files
- Decompiles: `<re-scratch>\decomp` (export); 3249B30, 25BA540, 1114B8, 113838 and AA3820 / AA3980 through `decomp_pool.cmd`. Vtables:
  `re\scripts_gpu\qdump.py` / `findptr.py` (raw offset == RVA).
- Graph: `data/dumps/kgk_0x102dd706.kidsrender.txt` (+ 19 other graphs); objdb `data/dumps/0x93fb3280.kidsobjdb.txt`.
- FCAP1: the pc2 capture of 2026-10-09. Per-frame tables come from the FC CSV:
  - `kind` (call / barrier / ecl / wait / signal / res);
  - `a0` of barrier rows = D3D12 barrier type (0 transition, 1 aliasing, 2 UAV);
  - `info` = PSO stage hashes, labelled with `<re-scratch>\gp2_hash_index.json`.
  The scripts were inline one-offs; re-derive with the column meanings above.

## 9. Proposed names for functions other packages own (partition.tsv owner in brackets)

| RVA | proposed name | evidence |
|---|---|---|
| 153014 | BeginCheckSkipNodeExecute [BACKEND/50] | vtable 40EBCB8 +8; §2.2 |
| 969F80 | CompoundNodeExecute [SCENE/50] | vtable 40EBC48 +8 |
| 3465B04 | RenderGraphNodeExecute [VOLNAT/50] | vtable 40EBC10 +8 |
| 3465ACC | ProfileEventNodeExecute [VOLNAT/50] | vtable 40EB818 +8 |
| 32FEE04 | RenderGraphSubListWalk [POST/50] | callers 969F80 / 3465B04; same loop as FrameStepRender |
| 345E4F4 / 345E600 / 345E584 / 345EA3C | BeginCheckSkipNodeCtor / EndCheckSkipNodeCtor / CompoundNodeCtor / ProfileEventNodeCtor [VOLNAT/50] | set vtables 40EBCB8 / 40EBC80 / 40EBC48 / 40EB818 |
| 25BA3E8 / 25BA540 | RgkGraphFileLoad / TgkGraphFileLoad [VOLNAT/50] | magic `_RGK` / `_TGK` |
| 113394 / 113448 / 113490 | TransientActivateAtNode{Set,List,Obj} [BACKEND/50] | same test `obj[0x27] == nodeIndex` -> 860C5C |
| 5F204 | KtglEmitAliasBarrierBlob [UI/50] | tokens 0x72 + 0x9D |
| 1D29B4 | RendererBeginEventVfn [SCENE/50] | Renderer vtable +58h |
| 4A3334 | RendererEndFrameVfn [PSO/50] | Renderer vtable +118h -> RenderWaitForFrame |
| 244EE60 | VideoMemoryKeeperRegisterType [LIGHT/50] | string "VideoMemoryKeeper" |
| 259F5AC | VideoMemoryKeeperFactory [BACKEND/50] (now GpuMemGetResourceAllocationInfo) | handler-table slot 22 of 3B5B838; creates the heap object 40EB360 and adds it to R+1EE530h via 32F1FF4 |
| 32F1FF4 | TransientSetAddKeeper [BACKEND/50] | caller 259F5AC |
| 44DE78 | MeshletGenerateHiZMapNodeExecute [SCENE/50] | vtable 40EB850 +8 |
| 1F2F78 / 1F2C10 | ComputeSkinningNodeExecute / SimulationMeshUpdateNodeExecute [LIGHT/50] | vtables 40EB9D8 / 40EB9A0 +8 |
| 90EC18 | OBOROExportNodeExecute [POST/50] | vtable 410A630 +8 |

113204 keeps its old name FrameRenderViews (it is FRAME-owned, but many notes cite it); it is the per-entry execute, not a view loop.
