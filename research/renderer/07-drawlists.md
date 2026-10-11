# renderer/07-drawlists: from scene objects to KTGL draw tokens (package 07, 2026-10-10)

Scope: the DRAW rows of `re\scripts_renderer\partition.tsv` (views, draw lists, CPU draw prep). Static only: the
`<re-scratch>\decomp` export, 2 `decomp_pool` batches, the dump (`re\Nioh3.dump.exe`, job vtables) and the existing
profiles. Tags: **[dec]** read in a decompile, **[inf]** inferred, **[M]** measured in an earlier note.
Symbols: block `# renderer/drawlists` in `re\symbols\symbols.tsv` (2 functions, 20 struct fields).

## 0. Answer

1. **Handoff point.** The game hands the renderer **DrawObjs** (0x98-byte "display sets": 19 typed slot pointers, the
   runtime form of one G1M mesh group). It does not hand over a scene graph. Object-update jobs on the KIDS workers
   (ObjUpdatePerObject 1A7BF0 → ObjUpdateWorkerCall 1202B0 → ObjUpdateWorkerBody 121FC0) cull each object against
   each view and **append 16-byte DrawRecs `{DrawObj*, u32 sortKey}` to the view's DrawLists** with a lock-free CAS
   on the list count. The append helper is 39A80, which the partition files under BACKEND by region fill. Everything
   after that runs on the main thread [dec]. §2, §3.1.
2. **Lists.** `View+3FF0h` is an array of 32-byte DrawList headers:
   - 0/1: the main view's two lists;
   - 2-5: cascade A; 6-9: a second per-cascade set; 10: an extra shadow list; 11-14: cascade B;
   - 15-18 / 19-22: VSM A/B.

   Local lights own their own list headers under `View+4028h` (shadow mappers). Trees, grass and similar systems
   are per-view "extra renderers" at +42D0/+4308/+4310 [dec]. Sorting is a ViewParam choice
   (Opaque/Alpha/ShadowPrimitiveSortMethod, ParallelSortPrimitiveUnitNum 128). List mode 3 means "contiguous by
   DrawObj+18h", the per-pass material table [dec]. The sort runs on the KIDS worker (§6 Q1).
3. **Per pass.** SceneRenderView 535188 is entered once per graph *View* node, through RenderNodeExecutePerView
   10F060. It calls DrawListWalk 39E20 on up to 25 sites: main G-buffer, stencil and highlight layers, `Cascade %d`.
   Each walk picks **one material per DrawObj by pass slot** (`DrawObj+18h[slot]`) and filters by material params.
   For each group of equal `+18h` it binds the technique once and then runs every technique pass over the group
   (DrawEmitSubPasses 1DC2F0 is pass-major; DrawEmitObjectMajor 5EAD78 runs when material+12E8h bit 0 is set).
   DrawEmit 1E80B0 runs once per DrawObj per pass. §3.2.
4. **Per draw.** DrawEmit walks the DrawObj's 0x40-byte DrawPackets. Bitfields in each packet select the index
   buffer, vertex layout, vertex streams, raster state word, constant block, texture table and param block from the
   DrawObj slots. Each one is compared against a shadow copy in KtglContext and emitted only when it changed:
   tokens 3D / 3B / 3C / 14 / 13 / 17 / 85 / 10. Then come constants and textures (materials.md §5), and finally
   DrawSubmit 1D2F60 emits **6A** {4 args from packet+14h..+20h}, or the meshlet path 1D2AAC. §3.3.
5. **CPU cost [M].** The PC main thread (r5-r9) spends 35 % inclusive in SceneRenderView, 30-33 % in DrawListWalk
   and 17-19 % in DrawEmit (self 6.1-6.8 %), which is about 3 µs per walked item at a 9 ms frame. On the Ally,
   DrawEmit costs 0.99 ms per frame (130 ns per draw), the walk plus group length 0.59 ms, the state-commit family
   about 1.1 ms and the memcpy into the stream about 1.06 ms. All of it is bound by cache misses. §4.
6. **To the backend.** Tokens go into `KtglContext+3C40h`. Every draw does `++3C78h` and `++3C74h`, and at
   `CmdFlushLimit` (+3C70h) KtglCmdStreamFlush 254110 passes the stretch to the dispatcher (backend.md §1-2). The draw
   lists never reach the replay thread; only tokens with inline constants and object pointers do. §5.
7. **Scene interface for a new renderer:** §7. Per view: camera, frustum and sort parameters plus N typed lists of
   `{DrawObj*, key}`. Per DrawObj: geometry (IB, layout, streams, packets), a material per pass slot, a constant
   block, a texture table, a raster-state word, an optional meshlet record and flags. **Verdict:** keep the
   DrawObj/DrawList contract as the input. Replace the single-threaded walker and emitter only together with a new
   backend, because state shadowing and per-draw side effects tie them to the one token stream (parallel-drawprep §4).

## 1. Entry points

| RVA | name | role |
|---|---|---|
| 57DB88 | SceneViewBuildDrawLists (not in the partition) | per frame, from SceneViewBuildTask_Run 2E33A4. Loops over views (`scene+158h`, mask `scene+3BCA0h`), pushes 3 KIDS jobs (vtables 3909D80 / 390A108 / 3909000, Run = 4674EC / 50284C / 828928), JobGroupWait, then per view ViewComputeShadowWorkMasks 534A98 and 1F737C (VSM tile bits) |
| 121FC0 | ObjUpdateWorkerBody (workers) | per object: bounds → per-view visibility and distance; appends its DrawObjs to view list 0/1 (main) and, per cascade bit, to lists 2+c / 11+c / 15+4k+c (shadow, key `(depth+100)*100`), plus light lists via `View+4028h` |
| 39A80 | (BACKEND row, unnamed) | DrawList append: CAS-reserves n slots on `L+0Ch`, writes item pointers, sets `L+18h` bit 0 if any DrawObj+80h ≠ 0 |
| 535080 | DrawListCount | `v = L+0Ch` (locked read); `v < 0 → min(-(v)-1, L+08h)` |
| 534A98 | **ViewComputeShadowWorkMasks** | cascade masks `View+3FFC/4000/4004/4008h`, flag `+4010h`; records nothing |
| 535188 | SceneRenderView | one graph View node: per bucket / layer / cascade → DrawListWalk (25 sites), meshlet culling records (MIC / MPC), highlight / stencil layers |
| 39E20 | DrawListWalk | groups, material per pass, filters, technique bind / unbind (3C77F4 / 3C5D64, materials) |
| 3BC0C | DrawListGroupLength | group end: binary search when `L+10h == 3`, else linear (gallop patch B: off by default, no measured gain in the ABBA test, see docs/BENCHMARKS.md) |
| 1DC2F0 / 5EAD78 | DrawEmitSubPasses / **DrawEmitObjectMajor** | pass-major / object-major loops over `Material vtbl+88h` passes (BeginPass +A8h, EndPass +B8h) |
| 1E80B0 | DrawEmit | per DrawObj per pass: packet loop, state shadow compares, tokens, params, DrawSubmit |
| 1D2F60 | DrawSubmit | token 6A; instanced / alternate forms via 1D42D8, 8ABA8C, 2F5EA88; non-indexed via 1D2870, `"Meshlet"` → MeshletDrawRecord 1D2AAC, DrawMeshletPathAux |
| 1DAF6C / 1E0B64 / 1E55D4 / 1E5854 / 1D8720 / 1D84A0 | state-commit family | program, permutation, slot tables, resource binds (materials.md §5 owns the semantics) |
| 5103C | ModelDrawSubmeshApplyMaterialScales | per-draw material constants (`vDifScl` …), materials.md §5 |
| 425B0 | ObjCountValidSlots | object entry table walk (`o+340h`, flags `o+4Ch`) |

## 2. Data structures

**View** (one per graph view; scene + 158h; [dec] unless marked):

| off | meaning |
|---|---|
| +3FF0 / +3FF8 | DrawList headers (32 B) / count. Slots in §0.2 |
| +3FFC, +4000, +4004, +4008 | cascade work masks (534A98) |
| +4010 | any shadow work (lists 2..14 non-empty, extra renderers, VSM tiles) |
| +4018 / +401C | sort-key bias / scale: main-list `key = u32((bias + clamp(d)) * scale)` (121FC0) |
| +4028 (+4030 cap, +4034 count) | shadow mappers: `{u64 hdr (kind = bits 60..63; 1 = light), obj*}`. Each obj has its own DrawList headers (`obj+10h → 2 lists`, inline `+18h`, count `+100h`) and class bits `+108h` (bits 8..12) |
| +42D0 / +4308 / +4310 / +42E8 / +42F0 | per-view extra renderers asked "has work for cascade k" (trees, grass, …; shadows-culling §1.2) |
| +4328 / +4330 | shadow root (ViewGetShadowObj 533E88) |
| +4460 / +4480 / +4354 | VSM tile list (lighting.md) |
| +4498 / +44A0 | meshlet draw-batch list (DrawBatchListComputePoolSizes 42154), package 08 |
| +4730 | VSM clipmap entries (0x150 B) |

**DrawList** (32 B): `+0 DrawRec** items`, `+8 i32 capacity`, `+C i32 count` (open = `~n`), `+10 u32 mode`
(3 = grouped by DrawObj+18h), `+18 u32 flags` (bit 0 = meshlet items present).

**DrawRec** (16 B, per-frame pool `{base, u32 used, u32 cap}` passed to 121FC0): `{DrawObj* obj, u32 key, u32 0}`.
The walker and 3BC0C read `**items[i]` = obj.

**DrawObj / display set** (0x98 B = 19 slot pointers). Each slot points at a typed value
`{u32 ?, u32 type | count<<8, data*}`; `DrawEmit` checks the type code before use. A model's DrawObjs are contiguous
(stride 0x98). The slot ↔ G1MG section column is [inf] from materials.md §2.1.

| slot (off) | type | used as | ~G1MG |
|---|---|---|---|
| 0 (+00) | 0D | index buffers; packet+0Ch bits 27..30 → tok **3D** SetIndexBuffer | 0x10007 |
| 1 (+08) | 0E | vertex layouts; packet+10h bits 0..3 → tok **3B** | 0x10005 |
| 2 (+10) | 0F / 1F | vertex streams (1F = 0x18-B stream sets); packet+10h bits 4..11 / 12..15 → tok **3C** per slot | 0x10004 |
| 3 (+18) | 10 | **Material* per pass slot** (DrawListWalk `param_15`; slot 10 = SSS, 0x12 = LightMap) and the group key | per-pass tables (materials §2.3) |
| 4 (+20) | 11 | per-packet object table (packet+8h bits 28..31) | - |
| 5 (+28) | 12 | 0x50-B constant blocks (packet+10h bits 20..23) → 1CC278 (material param block) | 0x10003 [inf] |
| 6 (+30) | 13 | 0xCC-B texture tables `{u16 n, 12-B entries}` (bits 24..27) → 1FE6F0 into material+1100h.. | 0x10002 |
| 7 (+38) | 14 | 16-B texture-pack refs `{pack*, base}` (bits 28..31); entry ready = `pack+28h[i]+68h ≠ 0` | G1T |
| 8 (+40) | 15 | hash-tagged param blocks (hash 09C8B32Eh read by the walker); packet+0Ch low 13 bits | - |
| 9, 10 (+48, +50) | 04 | 0x10-B entry arrays (packet+8h / +4h low 10 bits); slot 9 count % 3 ≠ 0 flags a non-triangle set | - |
| 11 (+58) | 16 | **DrawPackets** (0x40 B each; count 0 → no draw) | 0x10008 |
| 12 (+60) | 0A | u64 raster-state words (packet+12h low 4 bits) → tok **14** (bits 4..7), **13** (bits 0..3), **17** + depth bias 45C3C4 (bits 16..63), **85** (bits 9..10) | - |
| 13, 14 (+68, +70) | 1C / 1D | per-draw resource objects (flags `+0Ch`) | - |
| 15 (+78) | 1E | GPU buffer pair for written positions (`"WritePositionBuffer"`, pass 0x1B) | - |
| 16 (+80) | 20 | **meshlet record** (non-null = GPU-culled item; `DrawState+FCh / +FDh` select meshlet-only / non-meshlet) | package 08 |
| 17, 18 (+88, +90) | 21 / 22 | pre / post draw callbacks (globals 0x4BD0B90 / B88), flag byte | - |

**DrawPacket** (0x40 B): `+0Ch` flags (bit 31 indexed, bits 23..26 sub-mode), `+10h` / `+12h` selectors, `+14h..+20h`
the four token-6A dwords, `+24h` instance count (> 0 → instanced token variants), `+28h` state key (0xBBBB55 =
none), `+38h` optional sub-object (its own pass loop in DrawSubmit).

**DrawState** (walker / emitter state, `param_1` of 39E20 and 1E80B0): `[0]+70h` = KtglContext, `[1]` default state
word, `[2]` / `[3]` / `[4]` current IB / layout / stream set, `[6]..[9]` current constant / texture-table pointers,
`[0Fh]` current state word, `+A4h` bound stream count, `+FCh` / `+FDh` meshlet filters, `+FEh` walk active,
`+F7h / F9h / FBh / B6h / AEh` per-walk switches.

**KtglContext shadows written here** (names owned by backend; the meanings are new):

| off | shadows |
|---|---|
| +2550 | IB (tok 3D) |
| +2558 | vertex layout (tok 3B) |
| +25B8 | tok 14 state |
| +25BC | raster mode (tok 13) |
| +26AB | polygon-offset enable (tok 17) |
| +29A3 | flag tok 85 |
| +25E4..+25EC | 3-byte stencil state (tok 10, setter 5350A0, 71 callers) |
| +2780 / +2880 / +2900 [slot] | VB per slot (tok 3C) |

## 3. Per-frame flow

### 3.1 Scene → view (workers, then main)
```
frame task graph: BeginRegisterPrimitive → 7× RegisterScenePlaceableByListContainer (32 units) → EndRegisterPrimitive → CommitToNextRender   (game-data §4.2)
SceneViewBuildTask_Run 2E33A4 → SceneViewBuildDrawLists 57DB88 (per view in mask scene+3BCA0h)
  jobs 4674EC / 50284C / 828928 (KIDS) ; JobGroupWait
  object-update jobs: ObjUpdateJobRoot 182000 → … → ObjUpdatePerObject 1A7BF0 → 1202B0 → ObjUpdateWorkerBody 121FC0
      bounds (ObbToAabb, part OBBs) → per view: distance / frustum / cascade bits
      → 39A80 append {DrawObj*, key} × n DrawObjs into View lists (CAS on L+0Ch), L+18h meshlet bit
  per view: ViewComputeShadowWorkMasks 534A98, 1F737C (VSM tile bits)
```
Culling done before the lists (shadows-culling §3.1): distance / frustum per object on workers (PlaceableVisibility*,
CullEntitiesByDrawDistance, custom occluders). Occlusion runs on the GPU only, and only for meshlet items (MIC / MPC,
two-phase Hi-Z). Direct draws get no occlusion culling.

The order of the registration nodes and 57DB88 within a frame is not traced; the sort is covered in §6 Q1.

### 3.2 View → walks (main thread)

| graph node(s) (render-architecture §2.2) | FC marker | lists walked |
|---|---|---|
| 26-28 ClearScene + View | `Cascade %d`, `GenerateShadowMapHiZ` | 2+c (twice, two sub-passes), 11+c, 6+c / 10 when set, extra renderers. Phase 2 walks only items with DrawObj+80h (meshlet) |
| 24 VirtualShadowMapPass | `VirtualShadowmap`, `Clipmap %d` | VSM 15-22 (via VsmPassExecute 465414, a DrawListWalk caller) |
| 29 LightPass | node name | per-light lists under View+4028h (LocalLightShadowmapRender 7E92B4, filters 367C990 / 367CE58) |
| 62-64 main G-buffer ph1, 105-117 ph2 | node names; `WithHighlight`, `CustomHighlightPass`, `HighlightStencilOnly_Character/Player/Enemy`, `SpecificStencil_Character` | main lists 0/1, re-walked per stencil / highlight layer; ph2 = meshlet groups only |
| 302-311 forward views, 318-327 water views, 15-23 top-down depth (ViewID 5) | node names | the same View lists with other pass slots / filters [inf] |

Inside each walk, per group, the filters are:
- the pass slot `param_15`, with fallback `param_50` when the material table is shorter;
- SSS (`"SSSTransmit"` → slot 10);
- refraction pre-pass / refraction modes 0-6 (material+12D8h bits);
- `"IndexedRim"`, `"LightMap"` (slot 0x12);
- name prefix `CNS-` (include / exclude switches);
- name `O?L` on slot 0x0F;
- the two-sidedness bit `material+12D8h+4 & 0x100` against `param_16`;
- meshlet-only / non-meshlet (`DrawState+FC / FD`).

A rejected group is skipped with 3BC0C. An accepted group gets MaterialTechniqueBind, then
`passes = Material vtbl+88h`, then 1DC2F0 or 5EAD78, then MaterialUnbindTechnique.

### 3.3 One draw (DrawEmit 1E80B0 → DrawSubmit 1D2F60)
For each packet of the DrawObj, in this order, every step only when the selected value differs from the
KtglContext shadow:
1. Tok 3D IB.
2. Tok 3B layout.
3. State word → tok 14 / 13 / 17 (+ depth-bias values) / 85.
4. Vertex streams → tok 3C per slot (unused slots cleared).
5. Constant block → 1CC278.
6. Texture table + pack → 1FE6F0 / texture binds.
7. Param blocks → DrawBindMaterialParams 1E6664.
8. Material dirty flush (1DF880 → 1E0B64 / 1D7340: tokens 2B / 2C / 37 / 38, 29) and 0x66 blobs (242EB8 / 242FC4).
9. **DrawSubmit**: tok **6A** `{hdr | topology<<16, packet+14h, +18h, +1Ch, +20h}`, ++draw counters, flush at the
   limit. If the material has `"Meshlet"` it takes MeshletDrawRecord 1D2AAC (ExecuteIndirect path, package 08)
   instead.

Side effects outside the stream on every draw:
- a material frame stamp;
- the refcounted meshlet pointer at ctx+1A80h;
- residency flags;
- the draw counters.

These are why the emit cannot simply be split across threads (parallel-drawprep §4).

## 4. CPU cost per frame [M]

| item | PC (r5-r9, % of main thread) | Ally (ms / frame, 216 windows, 34 fps) |
|---|---|---|
| SceneRenderView incl. | ~35 % (53 KB frame hides callers) | - |
| DrawListWalk incl. / self | 30-33 % / 1.8 % | 0.36 (self) |
| DrawListGroupLength | 1.2-1.4 % (without patch B) | 0.23 |
| DrawEmit incl. / self | 17-19 % / 6.1-6.8 % | 0.99 = 130 ns per draw |
| state-commit family | ≈ 8-10 % self | ≈ 1.1 |
| memcpy into stream / upload (B94230 / B94250) | ≈ 5 % | 1.06 |
| technique bind 3C77F4 incl. | 6.7 % | (S2F 3C4B74 0.10) |

- Items walked on the PC r5: ~990 per frame, about 3 µs each. They are 508 PB2 direct, 202 shadow / depth direct and
  258 meshlet groups (129 × 2 phases). GPU events: 1354 direct draws.
- Workers: object update + list append (1A7BF0 / 121FC0) is 17-21 % of Scheduler0 (mainthread-logic.md). It is off the
  critical path on the PC.
- Sources: mainthread-drawprep §1, parallel-drawprep §1, shadows-culling §3.2, ally-drawprep-2 §1.

## 5. What the backend receives
Only the token stream (backend.md §2). Per draw that is 0-8 state tokens (most are filtered out), constant tokens with
the data copied inline, and one 6A (or 69 / 6B / 6C / 6D for the non-indexed and instanced forms). The token payloads
hold engine object pointers: IB / VB / layout / texture objects. Their lifetime past the frame is backend §6's audit
item. Draw lists, DrawRecs and DrawObjs are main-thread-only and are reset every frame (TypedSlotTableResetAll 15BAE8
zeroes the View list headers). No FCAP1 check yet: per-marker draw counts in FCAP1 should match §4's item counts
(capture-check, package 12).

## 6. Open questions / hand-offs
- **Q1 (sorting; see 13-main-submit.md §2):** the lists are sorted on the KIDS worker inside 57DB88 (ViewSortAllDrawLists
  18D3AAC -> DrawListDedupeAndSort 3B3B4 -> e.g. DrawListSortByMaterialKey 54A94C + RadixSortRecsByKeyAtC 54AB78), not on
  the main thread. Still open: the sort site of main lists 0/1, and which View-node parameters select SceneRenderView's
  bucket and pass slot (`param_12` pass kind, `param_15` slot, `param_40` refraction mode).
- **Q2 (01 backend):** 39A80 is the DrawList append (suggested name DrawListAppendRecords). Proposed KtglContext
  shadow names are in §2. 5350A0 sets the stencil state (tok 10).
- **Q3 (08 meshlet):** the DrawObj+80h record layout, and how MeshletDrawRecord / DrawMeshletPathAux map list groups
  to ExecuteIndirect (129 groups × 2 phases).
- **Q4 (02 materials):** the roles of Material vtbl +88 (pass count), +A8 / +B8 (begin / end pass), +128 (override),
  +110 (name), and of flags +12D8h / +12E8h bit 0 (object-major).
- **Inferred, not proven:** that slots 0/1 are the opaque / alpha pair, and the G1MG mapping of slots 4, 5, 8, 9, 10.

## 7. Minimal scene interface for a new renderer (the handoff contract)
Per frame, per view:
- `ViewDesc {view / proj (and previous), frustum, sort bias / scale, viewport, flags}`;
- `lists[kind]` of `{DrawObj*, u32 key}`. Kinds: main opaque / alpha, cascade c (A / B / extra), VSM clip k, and per
  light (face).

Per DrawObj (read-only for the frame):
- **geometry**: index buffer, vertex layout, vertex streams, packets `{indexed, start, count, base, instances}`;
- **materials[passSlot]** → technique + constant block + texture table (materials.md §7 owns how to bind them);
- the raster-state word;
- skinning / written positions (ComputeSkinning node, slot 15);
- an optional meshlet record (package 08);
- filter flags: meshlet, SSS, refraction, CNS, highlight / stencil class, two-sided.

Plus the systems that never enter DrawLists: extra renderers (trees / grass), effects (10), UI (11), sky / volumetrics
(09).

A new renderer can consume exactly this after the append and before SceneRenderView. That point is the cleanest
cut: game logic and culling stay as they are. What it must reproduce is §3.2's pass-slot selection and filters, and
the per-packet state of §3.3. Recording order may change only if the state is fully re-bound per segment.

## 8. Files
- `research/renderer/07-drawlists.md` (this note).
- symbols.tsv, block `# renderer/drawlists` (includes 534A98).

## Main-thread submission (2026-10-10)

The end-to-end main-thread path (walk -> emit -> tokens -> kick), the sort, the stream / kick-ring fields, the UI layer
build, the meshlet scene-buffer sync wait and the ranked CPU-ms candidates are in **13-main-submit.md**.

## Capture-check (package 12, 2026-10-10)

Per-marker draw counts from FCAP1 (pc2 / PC): per frame 1217-2068 draws (+ 185-785 ExecuteIndirect) on the replay thread; the G-buffer phase 1 is a single list (537 draws pc2, 675 PC). Cascades: 33-42 draws each for cascade 0/1, 211 for cascade 2 at pc2 point 1 (2 instances per frame). A like-for-like check against section 4's per-view item counts needs the view -> marker map from 05-frame-graph (the G-buffer views have no marker); not done.
