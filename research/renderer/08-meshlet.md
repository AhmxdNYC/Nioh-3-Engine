# renderer/08-meshlet: the GPU-driven meshlet pipeline (LOD, culling, Hi-Z, streaming) + replacement design v2 (2026-10-10)

Package 08 of research/renderer/00-map.md. Static work only (C export `<re-scratch>\decomp`, 4 pool queries, DXIL in
`gpu-shaders/culling/extract` + `re/scripts_shadow/out/mpc`, exe strings). No game run. Builds on meshlet-frontend.md,
meshlet-mainthread.md, gpu-rewrite-round3.md §3, meshlet-redundant-lod.md, rdna2-meshlet-slowdown.md, rebar-fix-v3.md.
Tags: **S** static, **M** measured (earlier runs), **E** estimate, **(i)** inferred. RVAs are v2.02; `FUN_7ff74fXXXXXX` = RVA + 0x7FF74F410000.

## 0. Summary

- **One object owns everything:** `MeshletSys = renderer(+F8h) + 18D0h` (the `param_1` of every recorder). It holds the
  two per-frame scene-buffer sets, ~20 GPU buffer objects at `+16A140..+16A210`, a 32-slot per-view Hi-Z table, the culling
  parameter block at `+60h`, and the streaming manager `M = MeshletSys + 169DD0h`.
- **Per frame (S):** frame begin uploads the scene buffers (CPU, 4BBEC) and records the streaming check (MCLRC). Then each meshlet
  view runs `[ResolveLOD] -> CullBegin -> InstanceCulling -> Preculling -> draws -> CullEnd`, and a two-phase view does it again after
  its Hi-Z. All views share **one set of output buffers**, so view N's draws must run before view N+1's culling overwrites them.
  That serialisation is the biggest constraint on a replacement (§7).
- **Dispatch / barrier budget (S, matches M):** ~25-35 dispatches + ~70-90 UAV-barrier tokens per LOD resolve (9-14 levels),
  up to 15 dispatches + ~27 barrier tokens per Preculling chain. With 1.5 resolves and 3.5 chains per frame that is **~100
  dispatches + ~200 barrier tokens per frame**, each replayed one at a time by the dispatcher (backend.md: barriers are never batched).
- **Payoff of a replacement with ReBAR ON (E, §7.4):** pc2 GPU **~0.4-0.9 ms of 19-24 ms**, plus ~0.2-0.5 ms on the dispatcher
  (replay) thread, which is the critical path on pc2 with ReBAR. RX 9070: 0.2-0.3 ms. Ally: 0.4-1 ms. Effort **~9-12 weeks** (E),
  because multi-view culling also needs per-view output buffers and a detour of the draw consumer.
  **Verdict: below the ≥ 5 % bar on every device. Do not build it standalone.** Fold the cheap parts into the submission-layer work
  (00-map §5 (a)) only if that work rewrites these recorders anyway.
- **Partition trim (S, §8):** of the 11,443 SCENE rows, about **370 are meshlet** (147 core + 220 region-fill that touch MeshletSys).
  ~1,100 more are LOD / culling / terrain / grass support. The other ~9,700 region-fill rows are resource and asset glue (760),
  gameplay objects (141), tiny generated functions (5,364) and unknown code (3,336). Those should go back to res / obj.

## 1. Entry points

| RVA | name | owner | role |
|---|---|---|---|
| 4A529C | RenderFrameBegin | FRAME | calls MeshletFrameBegin |
| 5D214 | MeshletFrameBegin | 08 | per-frame setup, set swap, upload, MCLRC record, extra dispatch (§3.1) |
| 4BBEC | MeshletUpdateSceneBuffers | 08 | maps and fills the current scene-buffer set (§2.2) |
| 4AA34 / 5D1F4 | MeshletCheckRequestLoad / ...Thunk | 08 | MCLRC record (streaming check), marker `MeshletCheckRequestLoad` |
| 5DC18 → 4B104 | MeshletRequestLoadStep → MeshletRequestLoad | 08 | MGLRC record (request generation), marker `MeshletRequestLoad`, from RenderWaitForFrame 4A4F94 |
| 9811F8 → 4A2D0 | MeshletRequestJobEntry → MeshletRequestJob | 08 | CPU: readback → per-entry request rings → TS_MESHLET_REQ workers (48E90) |
| 904610 | **MeshletResolveLodMainView** (new) | unowned | main-view wrapper: reset-bit → param_10 = set+70h + permutation 1 |
| 8DA0A8 | **MeshletResolveLodClipmap** (new) | unowned | VSM-clipmap wrapper: ctx+1430h = clip<<4\|1, param_11 = 1, SM ≥ 0Ch toggles ctx+1298h+8 |
| 531948 | MeshletResolveLOD (proposed; partition LIGHT) | 03 | LOD recorder, marker `MeshletResolveLOD` + per-level `%d` (§3.2) |
| 53FD3C | MeshletInstanceCullingRecord | 08 | MIC recorder, marker `MeshletInstanceCulling` (§3.4) |
| 5301FC | MeshletPrecullingRecord | 08 | MPC recorder, marker `MeshletPreculling` (§3.5) |
| 52FCB4 | RenderCtxBeginPassTarget (partition DRAW) | 07 | binds the view's Hi-Z + rect, sets the draw-slot (§3.3) |
| 9AC33C / 52E424 / 52FB44 | MeshletCullBegin / MeshletCullBeginAlt / scene variant | - / - / DRAW | per-view block = MeshletSys + 308h / 380h / 2F0h + view·B4D0h |
| 53007C | MeshletCullEnd | 08 | Hi-Z ping-pong, resets view state, 3 UAV barriers |
| 44F8A8 | MeshletHiZBuild (proposed; partition LIGHT) | 03 | GHDM: pass 0 depth/HTILE → mip 0, pass 1 per mip (no 0x8A; transitions at bind) |
| 44E0FC / 9B1850 / 32F5DA8 | Str_GenerateSceneHiZ / GenerateShadowMapHiZ / Str_Generate2ndHiZ | 08 / - / 08 | marker wrappers around 44F8A8 |
| 1D2AAC | MeshletDrawRecord | 08 | **the VS path consumer**: EI draw from 16A178 (§3.6) |
| 32F5248 | DrawMeshletPathAux | 08 | non-VS-draw path: draw from 16A170 by view slot |
| 237044 | **MeshletSceneViewNodeRecord** (new) | 08 | scene-view node: LOD + MIC + MPC + CullEnd for one view (§4) |
| 2353C0 / 237488 | ApplyMeshletLodScales / MeshletLodThresholdGet (proposed, LIGHT) | 08 / 03 | 8 LOD distance groups (§2.4) |
| 4667C0 / 5D1B0 / 5DA48 | **MeshletResolveLodCtxGet** / **MeshletMclrcCtxGet** / MeshletExtraDispatch | unowned / 08 / 08 | shader objects by name hash 7ED01E3Fh / 4EA130F9h / 7F5143E8h via 111064 |

Graph nodes: MeshletResolveLODPass (nodes 14 / 35 / 299, 00-map). The culling runs inside the view nodes:
SceneRenderView 535188, ShadowCascadeRender 1F5C2C, VsmPassExecute 465414, LocalLightShadowmapRender 7E92B4, the secondary-view
renderer 1EB85C, 23584C / 236E14 (VSM clipmaps), 44DE78, 579E7C, 33CD0C8, 3682D2C / 3683A4C. FC markers: `MeshletResolveLOD` (levels
`0`..`13` nested), `MeshletInstanceCulling`, `MeshletPreculling`, `MeshletCheckRequestLoad`, `MeshletRequestLoad`, `GenerateSceneHiZ`,
`GenerateShadowMapHiZ`, `Generate2ndHiZ`, `GenerateSubShadowMapHiZ`, `GenerateVFInjectionShadowMapHiZ`, `GenerateReflectiveShadowMapHiZ`.

## 2. Data

### 2.1 MeshletSys (= renderer+18D0h; `param_1` of the recorders)
| offset | what |
|---|---|
| +14h (byte) | VS-draw meshlet mode on (selects the MPC draw chain and MeshletDrawRecord's indexed path) |
| +60h | **MPC parameter block** (a ShaderParam object; the MPC ctx points at it via ctx[25Eh]). Slots: +D8h 16A170 (+250h mode, +254h count), +E0h 16A178 (+278h Hi-Z mode, +27Ch flag), +128h 16A1B8, +140h 16A1C0 |
| +270h..+28Ch | Hi-Z UV rect (+270h/+278h from 981C50) and the viewport scale (+280h..+28Ch, rect / RT size) |
| +298h / +2A0h | flags (non-VS path), Hi-Z mode bits 1..4 |
| +2A4h | Hi-Z mode: 0 off, 1 phase-1, 2 phase-2 (low nibble) \| 200h |
| +2A8h / +2ACh | view draw slot: `slot*4 + 10h` (slot < 80000000h), 0 = none |
| +2E0h | Preculling count `cnt` = `param_5[+20h]` (or `+1Ch` when 169D48 = 4) |
| +2F0h + i·B4D0h, i < 32 | **per-view Hi-Z blocks** (+0 texture object, +14h valid, +B4C0h viewport rect) |
| +169CF0h / +169D04h / +169D08h | 2nd-Hi-Z state |
| +169D10h / +169D18h | current / previous Hi-Z block (ping-pong in MeshletCullEnd) |
| +169D20h..+169D2Eh | per-view bools: use-slot, slot = Fh, no-HiZ (sets MIC flag 80000000h), vs-mode, LOD-freeze ... |
| +169D3Ch | MPC phase (param_4) |
| +169D48h / +169D4Ch | MPC count-field selector (4 = `+1Ch`), previous value |
| +169D50h / 54h / 58h | MIC mode bits → ctx+1444h (20000000h / 40000000h, 10000000h / 8000000h, 4000000h / 2000000h) |
| +169D5Ch / +169D60h | non-VS draw slot / **EI command-signature base** for MeshletDrawRecord |
| +169D70h / D90h / D98h | shader-object tables (LOD / MCLRC / extra), defaults at +169DA0h / DC0h / DC8h |
| **+169DD0h** | **M = streaming manager** (meshlet-mainthread.md §1: entries +568h 32 B, map +4B0h, flags, ...) |
| +169FB0h / +16A030h | **the two scene-buffer sets** (80h each), `+16A0B0` = current, `+16A0B8` = parity |
| +16A0C0h | view / camera cbuffer source (count `+16A2ACh + 1`) |
| +16A140h .. +16A210h | GPU buffer objects (§2.3) |
| +16A23Ch | MRLOD payload capacity (nMaxMRLODPayloadNum) |
| +16A618h | "refraction pass" flag (MPC slot 9 / A gate) |
| +16AA7Ch | forced LOD bias (≥ 0 overrides param_6) |
| +16AA80h..88h | global LOD constants → ctx+1470h..1478h (vThPOE / vCustomCorrectPOE (i)) |
| +16AAA0h[v] / +16AAC0h[v] / +16AAE0h + v·10h | per-view LOD-freeze flag / capture flag / frozen camera position (vFixPositionForCalcLOD) |

Buffer object (rebar-fix-v3 §2): +8 refcount, +18h ID3D12Resource (or a pointer to it), +68h usage {0/1 DEFAULT, 2 UPLOAD, 3 READBACK},
+70h capacity, +80h offset, +90h its IB view (used by tok 0x3D).

### 2.2 Scene-buffer set (`[MeshletSys+16A0B0]`, written by 4BBEC every frame, UPLOAD heap)
| slot | element | shader name / slot | readers |
|---|---|---|---|
| +18h | 48 B per instance: 3 × float4 (world 3x4) | StructuredBuffer<float4> | MIC (ctx[25Eh]), MshLod#2 |
| +20h / +38h | prefix tables `(n+1)*4` | t22 (i) | MPC#20/#33 (+20h when 169D48 = 4) |
| +28h / +40h | 4 B per object | t21/t24 (i) | MPC#20/#33 |
| +30h | **S_INSTANCE_OFFSET** {u32 ×4} = 16 B per part | t18 / t19 | MIC, MshLod#2, MPC#27 |
| +50h | **S_DISPATCH_OFFSET** {u32 ×3} = 12 B | t20 | MIC |
| +58h | **S_PAYLOAD_TO_MRLOD** {u32 ×2} root list, padded with `FFFFFFFF00000000` | t20 (tDispatcherMRLOD) | MshLod#1 |
| +68h / +70h | VSM per-view default output / counter reset | u (param_10) | MshLod#7 (clipmap) / main resolve on reset |
| +78h | READBACK | - | CPU |

### 2.3 Persistent buffers (MeshletSys + ...) and their shader bindings (DXIL resource tables, S)
| offset | shader name (type) | written by | read by |
|---|---|---|---|
| 16A140 | page flags (ByteAddressBuffer t21) (i) | streaming | MshLod#2, MCLRC, MPC |
| 16A160 | cull / LOD codes (RWStructuredBuffer<uint> u0) | MshLod#5 (clear to 70h), MshLod#2, MIC | MIC, MPC, MCLRC |
| 16A168 | cache codes (RWSB<uint> u5; nCacheCodesPIdx) | MshLod#2 (stores 2 = keep-alive), MshLod#4, MPC | MCLRC next frame |
| 16A170 | **S_PAYLOAD_TO_MSHLTCULL** {u32 ×2} list (u2 / t23) **+ its DispatchIndirect args at byte 0** | MshLod#2 / #4 | MPC#20/#33 (tok 0x81 offset 0) |
| 16A178 | uVSDrawArguments (RWBuffer<uint> u9) | MPC#22/#23/#24/#26/#27 | #27 dispatch args (byte 4), slot A args (byte 10h), **draw EI args (MeshletDrawRecord)** |
| 16A180 | uArguments (u2 in #23/#24, u10 in #27) **+ the meshlet INDEX buffer** (buffer+90h view, tok 0x3D) | MPC#23/#24/#27 | indexed meshlet draws |
| 16A188 | uVSDrawMshltMtrOffsets (u11) | MPC#22..#31 | meshlet VS |
| 16A190 | **S_PAYLOAD_TO_MSHLTDRAW** {u32 ×2} (u3) | MPC#20/#33/#27 | meshlet VS |
| 16A198 | **S_VS_DRAW_MSHLT_INFO** {float4, u32 ×9} = 52 B (u12) | MPC#27 | meshlet VS |
| 16A1A0 | S_PAYLOAD_TO_MRLOD scratch (u4) | MshLod#1, #2, #3 | next LOD level |
| 16A1A8 / 16A1B0 | LOD level args / counters, ping-pong per level (u1 / u3) | MshLod#0 / #3 / #2 | tok 0x81 of the next level |
| 16A1B8 | non-VS draw output (slot 2 / Dh) | MPC | DrawMeshletPathAux |
| 16A1C0 | phase-2 args (ctx two-phase permutation) | MPC slot 3 | MPC tok 0x81 |
| 16A1C8 | MIC output (RWBuffer<uint> u1) | MIC | MeshletExtraDispatch at the next frame begin (ctx+1318h) |
| 16A1F8 / 16A210 | scene SRVs (G1M offset table S_G1M_OFFSET t16/t17, resource offsets S_RESOURCE_OFFSET t19) (i) | streaming / upload | LOD, MIC, MPC |

Kernel bindings (DXIL, `register|type`; numthreads):
- MshLod#0 (32): u1. MshLod#1 (32): t20 MRLOD, u4. **MshLod#2 (32): t16 S_G1M_OFFSET, t18 S_INSTANCE_OFFSET, t19 S_RESOURCE_OFFSET,
  t21 BAB, u0 codes, u1 args, u2 MSHLTCULL, u3 args, u4 MRLOD, u5 cache codes.** MshLod#3 (32): u1 u3 u4. MshLod#4 (1): u1 u2.
  MshLod#5 (1024): u0. MshLod#6 (8): u7.
- MIC#0/#2 (32): t17 S_G1M_OFFSET, t20 S_DISPATCH_OFFSET, t19 S_INSTANCE_OFFSET, t21 Buffer<uint> (VSM SAT
  tValidRectCountCumulativeMap), Hi-Z Texture2D + sampler, u0 codes, u1 out; #2 adds cbuffer MeshletInstanceCullingConstBuf. MIC#1: t20 t19 u0.
- MPC#20/#33 (32): t17 t19 t21 t22 t23 (MSHLTCULL) t24, Hi-Z + sampler, u0 u2 u3 (MSHLTDRAW) u9 u11 u13. MPC#0 (32): u2. #22 (1024): u9 u11.
  #23 (16×64): u2 u9 u11. #24 (1024): u2 u9 u11. #26 (1): u9. **#27 (1×128×8): t17 t19 t21, u3 u9 u10 u11 u12 (VS_DRAW_MSHLT_INFO) u13.**
  #28/#30 (32×16), #29 (1), #31 (16×64): u9 u11.
- MCLRC#0/#2 (64), #4 (256): t16 S_CLUSTER {u32, u32, u32[4]} = 24 B, t17 S_READRANGE {u32 off, u32 size}, t18 S_G1M_OFFSET,
  t19 S_RESOURCE_OFFSET, t20 uint, t21 BAB, u0. MCLRC#1/#3/#5: t19, u0.

### 2.4 Data formats
- **S_G1M_OFFSET** (48 B per G1M model, 12 dwords): offsets into the bindless G1M buffer (cluster records, meshlets, vertex /
  index streams, LOD error and bounds). MshLod#2 reads dwords {0, 4, 16, 28, 36, 44} (gpu-rewrite-round4).
  **S_RESOURCE_OFFSET** {u32, u32} per G1M: base and count of the streamed resource (page) range. **S_CLUSTER** 24 B, the
  streaming unit: the CPU's read-range record (`bytes / 18h` in MGLRC). **S_READRANGE** {file offset, size}.
- **LOD cluster payload** (MRLOD and MSHLTCULL, 8 B): `g1m = (p0 >> 6) & 3FFFFh`, `instance = (p0 & 3Fh) << 14 | p1 >> 18`,
  `cluster = p1 & 3FFFFh`. Root list (set+58h) = one payload per instance root cluster, padded to 8 / 32 by wave size.
- **Streaming metadata** (exe strings for the MCLRC / MGLRC cbuffers): nClusterNum, nClusterNumTable, nClusterOffTable,
  nDependG1mIdx(Table), nReadRangeOffTable, nReqFlgOffTable, nPageIndex, nModelEntryNum and the `nO2*` twins (the per-group
  indirection variant). The CPU fills them per entry: MGLRC cbuffer arrays at ctx+132Ch (clusters), +1B2Ch (entry index),
  +232Ch (request-flag offset `>> 2`), +2B2Ch (read-range offset `/ 18h`), +332Ch / +732Ch (per-group entry and cluster base).
  Up to 200h entries or 1000h groups of 32 clusters per dispatch.
- **LOD thresholds:** 8 distance groups (RADENLODBiasGroup). ApplyMeshletLodScales 2353C0 (GameUpdateTick) reads the stage KIDS
  resource 38D04576h into `+A108h..A124h`, then applies the quality tier column (`[39CAFB0]` 0/1/2), an override table and a +bias
  with a clamp at 10. MeshletLodThresholdGet 237488(view, i) feeds the 32-byte camera block (`param_5`). VSM clipmaps rescale it per
  clip (meshlet-redundant-lod.md). Shader constants: fScrDist, fThPOE / vThPOE, nForceLevel, nForceLimitLevel (= 10h from both
  wrappers), nCurLevel, nPayOffForCurrentLevel / NextLevel, nMaxMRLODPayloadNum, nCullCodesNum, fDistPerPixel.
- Cull / Hi-Z constants: vFrustumPlanes, vHiZViewport, mW2PPrev, nIsReverseZ, nEnableOcclusionCulling, fExcludeOccCullScreenRatio,
  nEnableExcludeOccCullBasedOnScreen, nEnableMergeClip, nInstCullCodeShift, nDispatchOffOff, nG1mInstOff, nG1mVisFlagsCapa,
  nMshltPrm0/1/2, nVSDrawPrm (.y = cnt), nCullMode, nCullingFlag.

## 3. CPU setup of each stage

ShaderCtx protocol (every recorder): `vtbl+68h` valid, `+88h` pass count, `+A8h(n)` select pass n (PSO), bind setters write
ctx[25Eh..28Fh], `+C8h` commit (descriptor tables / cbuffer), `+D8h(&x,&y,&z)` groups from the bound counts, `+B8h` end pass,
`+98h` / `+78h` finish. Tokens go straight into `KtglContext+3C40h`: **7E** Dispatch(x,y,z), **81** DispatchIndirect(off, bufobj),
**8A** UAV barrier(ID3D12Resource*), **3D** IB, **6E / 6F** ExecuteIndirect (non-indexed / indexed), with `+2A70h` counting
dispatches. ctx+1298h is a permutation state block: `+0 == 1` (540428) gates MIC pass 1, `+8 == 1` (52FFB0) gates the MPC
two-phase passes, and 8DA0A8 sets it for SM ≥ 0Ch.

### 3.1 Frame begin (MeshletFrameBegin 5D214, main thread)
Order: 5A4DC / 387A40 / 96A1E4 setup → **set swap** (`+16A0B8 = (p-1)&1`, `+16A0B0 = +169FB0 + p·80h`) → 93B720 (streaming
round-robin divisor M+14Ch / M+630h) → KIDS_MMM_SubReqLoad if the sub-request config changed → 5C708 (clear the two request
buffers, count·4 B each) → MeshletRegisterEntries 4BA8C → MeshletUpdateFlagsThunk 5B278 → 5D160 (re-request entries whose loaded
quality level < the view's `+74Ch`) → **MeshletUpdateSceneBuffers 4BBEC** → **MCLRC record** (5D1F4 → 4AA34: 2 passes over all
entries, ≤ 1000h groups of 256 per dispatch + tok 8A) → if M+290h > 0, **MeshletExtraDispatch** (shader 7F5143E8h, binds 16A1C8,
one tok 7E when pass count > 6).
Separately, at RenderWaitForFrame: MeshletRequestLoadStep 5DC18 → **MGLRC record** 4B104 (pass 0/1 or 2-5 by `[39CAA40]`, batches
as in §2.4, output M+370h = uRequestCmds, tok 8A). The CPU side (MeshletRequestJob 4A2D0): round-robin over entries, swap each
entry's request ring, map the readback (M+2E0h +8 / +10), fan out to TS_MESHLET_REQ workers (48E90) → MeshletChunkIssueLoad 49D5C.

### 3.2 LOD resolve (531948)
Signature (S): `(MeshletSys*, KtglContext*, ShaderCtx*, LodRequest* p4 {+24h levels, +28h, +2Ch roots, +30h cull codes},
CamBlock32* p5, int lodBias p6, int p7, int forceLimitLevel p8 (10h), float p9 (view scale, ctx+1434h), BufObj* vsmOut p10,
bool clipmapPerm p11)`. Sequence (pass n = PSO slot n = MshLod#n):
1. pass 0 ×3: Dispatch(1,1,1) on 16A170, 16A1A8, 16A1B0 (arg reset) → 3 × tok 8A;
2. pass 1: Dispatch(roots/8 or /32 by `KtglContext+60DCh` < 20h), t20 = set+58h, u4 = 16A1A0 → 8A;
3. pass 5 (if pass count > 5): Dispatch(cullCodes/1024), u0 = 16A160 → 8A;
4. levels L = 0..p4+24h−1 (marker `%d`): args = L even ? 16A1A8 : 16A1B0, counters = the other; L ≥ 1: pass 3 Dispatch(1,1,1)
   (count → groups, payload offsets `+16A23C` ping-pong) → 8A (args) + 8A (16A1A0); then 8A(counters); pass 2: **L = 0 Dispatch(roots/8|32),
   L ≥ 1 DispatchIndirect(args, 0)**, binds cbuffer 16A0C0, p10, 16A1F8, set+18h, 16A160, 16A140, set+30h, 16A210, level, 16A170,
   16A168, lodBias, p4+28h, p7, p8, 16A1A0 → 8A ×3 (+1 with p10);
5. pass 4 (if pass count > 4): Dispatch(1,1,1), u1 = 16A170, u2 = 16A168 → converts the selected count into MPC#20's indirect args.
Per resolve: 6 + 2·(L−1) + 1 dispatches, ~6 tokens 8A per level. No frustum and no Hi-Z input (gpu-rewrite-round3 §3.1).

### 3.3 View begin / end
RenderCtxBeginPassTarget 52FCB4(sys, kc, phase, slot, noHiZ, -, hizBlock): stores the Hi-Z block (+169D10h), the RT-scaled viewport
(+280h), the Hi-Z texture into +60h and its UV rect (+270h; Hi-Z on only if the block is valid and +169D25h is set), mode +2A4h,
draw slot `+2A8h = slot·4 + 10h`, and the bools +169D20h / +169D21h (slot = Fh means "no draw slot"). The wrappers pick the view
block (+2F0h / +308h / +380h + view·B4D0h) and the MPC count selector (+169D48h: 0 / 1 / 3 / 4). MeshletCullEnd 53007C releases
the Hi-Z, emits 8A on 16A170 / 16A1A0 / 16A1B8, ping-pongs +169D10h → +169D18h and resets the view state.

### 3.4 Instance culling (53FD3C)
`(sys, kc, micCtx, phase)`. ctx+1444h = mode bits | Hi-Z mode \| 200h; Hi-Z texture → ctx+12F0h (53FC14) with rect +1420h; +1448h = draw slot,
+1430h..143Ch = viewport scale; 16A1C8 → ctx+13B0h unless +169D21h. Pass 1 (when ctx+1298h+0 == 1): Dispatch(ctx groups) → 8A(16A160).
Pass 0: Dispatch(ctx groups, z = phase). Binds: cbuffer 16A0C0, 16A1F8, set+18h, set+30h, set+50h, 16A160. **1-2 dispatches, 0-1 barrier.**

### 3.5 Preculling (5301FC)
`(sys, kc, mpcCtx, phase, const u32* cntBlock, int slot, bool p7, bool perm, bool p9)`; `cnt = cntBlock[+20h | +1Ch]` (selector
+169D48h), ctx[25Eh] = sys+60h. Slot ↔ DXIL record:
| pass | record | dispatch | writes (then tok 8A on) |
|---|---|---|---|
| (2-phase) ? | phase-2 cull | EI(16A170, 0) with 16A1C0 | 16A170, 16A1C0 |
| 3 | arg reset | (1,1,1) ×1-2 | - |
| 7 | MPC#22 | cnt/1024 | 16A188, 16A178 |
| 0 | MPC#0 | ctx groups | 16A170, 16A140 |
| **1** | **MPC#20 / #33** (meshlet cull) | **EI(16A170, 0)** | 16A188, 16A178, 16A198, 16A170, 16A168 |
| 3 + 5 (2-phase) | phase-2 re-test (#20 variant) | (1,1,1) + EI(16A170, 0) | as above + 16A1A0 |
| 8 | MPC#23 | cnt/1024 | 16A178, 16A188 |
| E / F / 10 / 11 | MPC#28 / #29 / #30 / #31 (scan fix-ups) | ((cnt+63)/64+511)/512 / 1 / same / cnt/1024 | 16A178, 16A188 |
| 9 | MPC#24 | cnt/1024 | 16A178, 16A188 (binds 16A190 when VS mode) |
| B | MPC#26 | (1,1,1) | 16A178 |
| **C** | **MPC#27** (args + index build) | **EI(16A178, 4)** | 16A180, 16A178, 16A190 |
| A | refraction / main-only extra | EI(16A178, 10h) | + 8A(16A168) |
| 2 or D | non-VS path | cnt or ctx groups | 16A1B8, then 8A 16A170 |
**≤ 15 dispatches + ~27 barrier tokens per chain.** MPCS (meshlet-frontend §2) removes passes E-11 + B when cnt ≤ 1024.

### 3.6 The VS path (what the GPU draws)
DrawSubmit 1D2F60 → **MeshletDrawRecord 1D2AAC** for meshlet draw packets: in VS mode it binds the index buffer `[16A180]+90h`
(tok 3D), clears VB slot 0 (tok 3C), then emits **tok 6F (indexed EI) with command signature `[+169D60h] + 1Ch` on the argument
buffer object 16A178**. Otherwise (or for the refraction re-draw) it emits **tok 6E (non-indexed EI), signature `+169D60h + 30h`**, then
restores the IB / VB and bumps the Hi-Z use count. The meshlet VS reads 16A188 / 16A190 / 16A198 and the bindless G1M streams
(nMshltPrm0, nVSDrawPrm, drawID). DrawMeshletPathAux 32F5248 (non-VS mode): an indirect draw from 16A170 at `view slot·4`
(32F5308), or a direct draw of `(n+31)/32` (32F520C).
Per-draw argument records are 9 dwords at `u9[9i+7..9i+15]` (gpu-culling-rewrite §1), written by MPC#24 / #27.

## 4. Per-view chains

| caller | views | sequence |
|---|---|---|
| MeshletSceneViewNodeRecord 237044 (scene view node) | main, per view index < 32 | ctxs cached per node (2375A4 at node+20h / 28h / 30h / 38h, else created: LOD 4667C0, MIC 110EEC (i), MPC 110FA8 (i)) → 8 thresholds (237488) → LOD-freeze copy → **904610 → 531948** → begin (52FB44, or 1EB440 when node+44h bit 0) → MIC → MPC → CullEnd |
| SceneRenderView 535188 (DRAW) | main + sub shadow / VF-injection / reflective SM | per view: **MIC(ph 0) → MPC(ph 0) → DrawListWalk ×2 → CullEnd → Generate*HiZ → MIC(ph 1) → MPC(ph 1) → draws → CullEnd** (two-phase occlusion) |
| ShadowCascadeRender 1F5C2C | cascades / VSM tile batches | MeshletCullBegin 9AC33C or Alt 52E424 → MIC → MPC → 4× DrawListWalk → CullEnd |
| VsmClipmapRender 1F8340 → 236E14 / 23584C | VSM clipmaps | **8DA0A8 → 531948 (clipmap permutation MshLod#7, per-clip thresholds, param_10 = +416F8h[i])** → MIC → MPC → CullEnd |
| VsmPassExecute 465414, LocalLightShadowmapRender 7E92B4 | VSM pass, local lights (up to 8 MIC/MPC pairs) | begin → MIC → MPC → DrawListWalk → CullEnd |
| secondary-view renderer 1EB85C (from 1EB604) | water / 2nd view | ctx → **531948** → MIC → MPC → 4× DrawListWalk → CullEnd |
| 44DE78 | GenerateSceneHiZ view | MIC → MPC → CullEnd, Str_GenerateSceneHiZ |
pc2 dense (M): 1.5 resolves (9046AA main, 8DA16D clipmap in ~19 % of frames), 2.7-3.5 MIC/MPC chains.
**All chains write the same MeshletSys buffers (16A170..16A1B8)**: the recorders are strictly sequential on one KtglContext, and
each view's draws (MeshletDrawRecord reads the fixed `sys+16A178/16A180`) are recorded between its MPC and its CullEnd.

## 5. Streaming interaction

- Feedback loop (S): MshLod#2 stores 2 into the cache code of every visited cluster (16A168). Next frame, MCLRC (4AA34, 2 passes)
  compares the previous and current codes against S_CLUSTER / S_READRANGE / page flags and writes request flags. MGLRC (4B104)
  turns the referenced, unloaded entries into uRequestCmds (M+370h), which land in the 100 MB READBACK heap. MeshletRequestJob reads
  them two frames later and issues the chunk loads (24-byte read ranges). Page flags (16A140, t21) then make MshLod#2 stop at
  resident levels.
- Consequences for any replacement: (1) the LOD pass must keep visiting clusters outside every frustum (shadow casters and the
  keep-alive codes); (2) MshLod#5 clearing the codes every frame is part of the protocol; (3) the codes' layout is a contract with
  MCLRC. meshlet-frontend §6.1 already listed these.
- Without ReBAR the scene set and MRLOD roots are read over PCIe. That was the 40-100x anomaly (rdna2-meshlet-slowdown,
  rebar-fix-v3). **With ReBAR ON (pc2) it is gone (M):** MIC and MshLod#1/#2 dropped out of the top 100 (< ~0.1 ms each),
  MPC 0.37 → 0.09 ms. The streaming READBACK feedback stays in system memory either way (~30 KB/frame, not hot).

## 6. Terrain / grass / other SCENE rows (not meshlet)

These have their own paths and do not touch MeshletSys: GrassPlacementCull 31EF94 (CPU placement cull), ClusterLasmapCullingNode
52A9EC, GrassVertexDisplacementNode 52AF08, StaticTerrainNode (factory 25B5B84, ctor 367E3D4; PB2LS tessellated G-buffer views 46-48 / 91 / 97),
TerrainCopyToTextureAtlasNode 578AC0, GpuCullingRecord 69566C / Str_GPUCulling_Setup 298990 (separate GPU culling for
non-meshlet draws), CPU LOD (StageObjLodUpdateRange 469C0, CharacterModelLodApply 42EA40, CullEntitiesByDrawDistance 1772B4,
RenderViewCullListBuild 17FEEC). A new renderer would consume their draw lists through package 07, not this pipeline.

## 7. Replacement design v2 (updates meshlet-frontend.md §6 with exact interfaces)

### 7.1 Interface: what must be kept bit-compatible
- **Inputs (read-only):** scene set slots +18h / +30h / +50h / +58h (§2.2), 16A140 / 16A1F8 / 16A210, cbuffer source 16A0C0
  (+16A2ACh views), the view's Hi-Z block (MeshletSys+2F0h + v·B4D0h, rect +270h), 16AA7C..88, the 8 thresholds, p4/p5/p9/p11.
- **Outputs consumed outside the front-end:**
  - per view, at MeshletDrawRecord time: 16A178 (EI args at the draw slot, 9-dword records), 16A180 (+90h IB), 16A188, 16A190, 16A198;
    DrawMeshletPathAux: 16A170 at `slot·4`;
  - per frame: 16A160 / 16A168 codes (MCLRC, next frame), 16A1C8 (MeshletExtraDispatch, next frame), the param_10 VSM buffers (899B50).
- **Private (free to change):** 16A1A0 / 16A1A8 / 16A1B0, the scan scratch inside 16A178 / 16A188 below the draw records,
  16A1B8 / 16A1C0, the pass structure.
- **Detour points (all `__fastcall`, MSVC x64):**
  | RVA | detour | why |
  |---|---|---|
  | 531948 | MeshletResolveLOD | persistent LOD (M2); signature in §3.2 |
  | 5301FC | MeshletPrecullingRecord | fused scan (M3); signature in §3.5 |
  | 53FD3C | MeshletInstanceCullingRecord | multi-view (M4) |
  | 52FCB4 + 53007C | RenderCtxBeginPassTarget / MeshletCullEnd | M4: capture per-view state instead of recording at once |
  | 1D2AAC + 32F5248 (DRAW-owned) | MeshletDrawRecord / DrawMeshletPathAux | **M4 only:** point the draws at per-view output copies |
- **Token emission:** write tokens 7E / 81 / 8A directly into `KtglContext+3C40h` (bump `+2A70h` for dispatches; flush when
  `+3C74h >= +3C70h` via KtglCmdStreamFlush 254110). Own PSOs need a token the dispatcher already understands. Binding through the
  game's ShaderCtx keeps root signatures and descriptor tables valid (`vtbl+A8h` / `+C8h`), so new kernels should be **new
  permutations inside the shipped containers' root signature**, registered like the 24 MshLod#2 records (proven route: the `mpc`
  / `lodu` file swaps).

### 7.2 Pipeline (unchanged in idea, now with exact scope)
1. **M2 persistent LOD:** replace passes 0 / 1 / 5 / (3 + 2) × L / 4 with: MshLod#5 (kept), 1 init and 1 persistent traversal
   dispatch (global queue in 16A1A0, in-flight counter in 16A1A8), then MshLod#4. Same predicate code (all 24 permutations +
   MshLod#7 for the clipmap), same writes to 16A160 / 16A168 / 16A170 / param_10. Removes ~20-28 dispatches and ~60-80 barrier
   tokens per resolve.
2. **M3 fused scan:** passes 7, 8, E, F, 10, 11, 9, B → one decoupled-look-back scan + MPC#27 (kept). Removes 7 dispatches and
   ~14 barrier tokens per chain (MPCS already does part of it for cnt ≤ 1024).
3. **M4 multi-view:** batch the phase-1 MIC + MPC of all views between two graph points into one dispatch pair (thread = item × view).
   From the export: the outputs live in ONE buffer set per MeshletSys and the draws read fixed offsets. So M4 needs
   (a) per-view copies of 16A170..16A198 (or one buffer with per-view regions; sizes from the buffer objects' +70h), (b) the
   MeshletDrawRecord / DrawMeshletPathAux detours to rebase per view, and (c) deferring the views' recording, because the
   graph walks views one by one (frame-graph package 05 must say where all phase-1 views are known). Phase 2 stays per view.
4. **Barriers:** one per stage on the written set (tok 8A takes one resource; a NULL-resource 8A = global UAV barrier per backend.md,
   which also replaces 3-5 tokens).

### 7.3 Effort (estimate)
| # | milestone | effort | note |
|---|---|---|---|
| 1 | capture + offline replay harness (inputs §7.1 at region begin, outputs at CullEnd, per view) | 1 wk | gp_harness mode 5 base exists |
| 2 | persistent LOD (detour 531948, 25 permutations) + shadow check | 2 wk | clipmap permutation adds ~0.5 wk |
| 3 | fused scan (detour 5301FC passes 7..B) | 1 wk | |
| 4 | multi-view phase-1 culling + per-view buffers + draw detours | 4-5 wk | includes the buffer-set and DRAW detours |
| 5 | async LOD on a compute queue | 2 wk | only if F3 shows overlap |
| | **total** | **9-11 wk (M1-M4), 11-13 with M5** | |

### 7.4 Payoff with ReBAR ON (E unless marked)
- Without ReBAR the kernels are inflated by the PCIe anomaly (§5); with ReBAR ON the per-kernel work collapsed (M, §5). The front-end
  is now ~launch + drain bound. pc2 front-end with ReBAR ≈ RX 9070's 0.92 ms (M) × 2-2.7 (RDNA2 latency, meshlet-frontend §1.4) ≈
  **1.8-2.5 ms**, of which the structural part (≈ floor 7.6 µs × ~100 dispatches + barrier drains) is ~0.8-1.2 ms.
- **GPU:** M2 + M3 remove ~55 % of the dispatches → **pc2 ~0.4-0.7 ms**, + M4 ~0.2 ms → **≤ 0.9 ms of 19-24 ms (2-4 %)**.
  RX 9070: 0.15-0.3 ms (of ~8 ms). Ally (UMA, latency ×2-4, smaller scenes at USER): 0.4-1 ms of 30+ ms (≤ 3 %).
- **CPU (replay thread):** ~150 fewer dispatch / barrier tokens per frame. At ~1-3 µs each on the dispatcher (compute state flush +
  one ResourceBarrier per token, backend.md) that is **~0.2-0.5 ms** on the thread that bounds pc2 with ReBAR (frame 26 ms vs GPU 19,
  M). That is real but small next to the submission-layer work (a), which attacks the same thread wholesale.
- **Verdict:** **not worth 9-11 weeks** (< 5 % on every device). If package (a) (new submission layer) proceeds, the cheapest
  slice (M3 fused scan, already half-proven as MPCS, plus a NULL-resource 8A per stage) goes in with it. M2 is worth reconsidering
  only if an Ally capture shows the resolve > 1.5 ms.
- **Keep / replace for a new renderer:** keep the data model (scene sets, payload formats, codes, streaming protocol) and the
  shipped kernels. Replace only the recorders' pass structure. A new renderer consumes §7.1's outputs as its meshlet draw input.

## 8. Partition trim (answer to "which of the 9.9k region-filled functions are really meshlet")
Script: classify every SCENE row by its exported C (MeshletSys offsets `0x169Dxx` / `0x16Axxx` or meshlet names → meshlet;
renderer calls → render-other; resource and asset loader calls → res; actor / character / audio → obj; ≤ 64 B → tiny).
| class | core (d ≤ 4) | region fill (d = 50) | fill bytes |
|---|---|---|---|
| meshlet (touches MeshletSys) | 147 | **220** | 82 KB |
| LOD / culling / terrain / grass (non-meshlet render) | ~460 | ~95 | ~31 KB |
| resource / asset glue (LfmFileIdGetPath, ResHandle*, RegisterAssetHandler) | - | 760 | 329 KB |
| gameplay / objects | - | 141 | 110 KB |
| tiny (thunks, getters, generated) | 847 | 5,364 | 187 KB |
| unknown (no render evidence) | 97 | 3,336 | 937 KB |
Proposal for the next `render_map.py` run: keep the 370 meshlet rows (regenerate the list with the rule above) plus the named LOD / terrain rows. Return resource / asset (760) to RES-load and gameplay (141) to obj.
Mark the 3,336 unknown and 5,364 tiny region-fill rows as "unassigned" rather than SCENE. Ownership fixes for the core: 531948,
236E14, 237488, 44F8A8, 52F5D4.. binders (now LIGHT) and 52FCB4 (DRAW) belong in 08.

## 9. Questions for other packages
- **03 lighting:** 531948 (MeshletResolveLOD), 44F8A8 (MeshletHiZBuild), 236E14 (VsmClipmapMeshletLod), 237488
  (MeshletLodThresholdGet) and the ShaderCtx binders 52F424 / 52F5D4 / 52F614 / 52FA74 / 52FAB0 / 532868 / 532C88 are meshlet code
  that the BFS gave to LIGHT. Do you object to moving them to 08 (names proposed here, not added to symbols.tsv)?
- **07 drawlists:** 52FCB4 = RenderCtxBeginPassTarget (the per-view cull begin), 52FB44 = its scene variant. Confirm the EI command-signature table that
  `+169D60h + 1Ch / 30h` indexes (SynCtx+6E8h / 6F0h) and the 9-dword record layout.
- **05 frame-graph:** for M4, at which node are all phase-1 meshlet views of a frame known (before the first MIC)?
- **12 capture-check (FCAP1):** per frame, count tok 7E / 81 / 8A under the markers `MeshletResolveLOD` / `MeshletPreculling`
  (expected ~100 dispatches, ~200 UAV barriers), and the pc2-ReBAR front-end total (the 1.8-2.5 ms estimate in §7.4).

## Capture-check (package 12, 2026-10-10; answers the hand-off above)

Under the markers `MeshletCheckRequestLoad`, `MeshletResolveLOD*`, `MeshletInstanceCulling`, `MeshletPreculling`, `GPUCulling*`, `GenerateSceneHiZ`, `MeshletRequestLoad`, per frame:
- pc2 point 1: 32 Dispatch + 12 ExecuteIndirect, 191 barriers; pc2 point 3: 52 + 17, 280; Ally: 52 + 16, 270; PC point 1: 56 + 15, 264; PC point 3 (dense): 100 + 34, **540**. So "~100 dispatches / ~200 barriers" holds for a dense scene only; the light points are about half.
- LOD resolve: one `MeshletResolveLOD` scope per frame (two in 2 of 9 analysed frames), 10-14 level children, each 1 dispatch + 1 ExecuteIndirect and ~20 barriers (121-151 per resolve). MIC / MPC chains per frame 1-5; MPC chain 9-18 dispatch + 2-5 EI + 45-93 barriers.
- `GenerateSceneHiZ` = 7 dispatches per run; the PC runs it twice per frame (phase 2 re-test follows the first HiZ), pc2 and the Ally once.
- Counts are per frame on the replay thread (calls at 58BE1B Dispatch, 6ECFF4 / 6EB9B4 / 58BD13 ExecuteIndirect); the token numbers are for 01-backend.
