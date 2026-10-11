# Work deciders: settings -> pass parameters, LOD, streaming, culling, scheduling (Steam v2.02)

RE only (decomp + exe data; nothing installed, game not run). The symbols named here are in symbols/symbols.tsv.
Builds on `gpu-maxsettings.md` §1, `perf-coverage.md`, `shadows-culling.md`,
`meshlet-mainthread.md`, `cpu-hotspots-ally.md`. Ally config referenced below: REN_RAT 50 (960x540), SHADOW/AO/GI/SSR/VOLUMETRIC STANDARD.

## 0. Key: RenderConfig field names come from the environment schema
`ApplyGraphicsSettings` 0x39A39C (every frame) works on **RenderConfig** ("S"), which is refilled from the stage's
environment DB and then scaled or overridden per quality setting. The two schema registration functions
(0x25F15B0, 0x2629194) register each field as `lea name; mov [rsp+28h], offset; mov edx, hash`, so offset = RenderConfig
offset. A helper script (`renderconfig_fields.py`, private dev repo) lists all **1,567** fields
(`--off 13dc 107c` looks offsets up). Env values below are from the dumped env DB 0x396b9210.

## 1. Quality setting -> pass parameter (what each applier really writes)

| Setting (levels) | Applier | RenderConfig field <- value per level | Env value | Same-look lever? |
|---|---|---|---|---|
| VOLUMETRIC (LIGHTEST..HIGH) | ApplyVolumetricQuality 0x39B080 | +0x13DC **VolumetricFogResolutionX**, +0x13E0 **VolumetricFogResolutionZ**: LIGHTEST 0/0 (+0x1398 ScatterScale x0 = fog off); LIGHT /2 cap 80/32; STANDARD cap 120/48; HIGH x1.5 cap 160/64. +0x7B4 **VolumetricCloudResolution = 2 at every level** | X 160, Z 64 (32 in some areas); CloudRes 1 | No: froxel X is not tied to render width (Ally STANDARD 120 froxels over 960 px = 8 px each). Authored quality steps |
| (VFSCAT grid input) | VolumetricFogScatterNodeExecute 0x14871C | cb+14D0/14D4 = RenderSystem+0x960/0x964 x view scale x dyn-res. **+0x960/964 = render width/height** (g_RenderResolution 0x45BB028, copied each frame by RenderSystemSyncRenderSize 0x12399D0), not the froxel grid. The grid is X x Y x Z above; Y not traced (r5: 160x96x48 at HIGH with env Z 32) | - | Fills in perf-coverage's "copy path not traced" |
| (VCLOUD steps) | none | +0x40C **VolumetricCloudMinRaySampleNum**, +0x410 **MaxRaySampleNum**: no setting touches them; VCLOUD cost = env data + quarter-res target | 32 / 256 | No setting knob exists; only a data/memory poke (not same-look) |
| SSR (OFF..HIGHEST) | **ApplySsrQuality** 0x39B0E8 | +0x107C **RLRMaxRaySamples** x 0 / 0.5 / 0.75 / 1 / 1.5 / 2; +0x109C RLRResolution = 0 always; resource 0xEF3D849D +0xB8 = 1 (LIGHTEST..STANDARD), 0 (HIGH, HIGHEST) | 0 in this env (shader default?) | No |
| AO (OFF..HIGHEST) | ApplyAmbientOcclusion 0x39B20C | +0x12F4 **SSAORayNum** 0/2/3/5/7/14; +0x12F8 SSAOStepNum 2; +0x12D8 SSAOContrast 1.75; +0x12E4 SSAOQuality 0 (LIGHTEST) else 1. HIGHEST at render height <= 720: 7 rays + SSAOResolution (+0x12E8) = 1. ApplyGraphicsSettings also forces +0x12F0 SSAOUseViewDepth = 1 | 8 rays / 6 steps (overridden) | No. Note: on the Ally (540 p) HIGHEST = 7 rays like HIGH, plus SSAOResolution 1 |
| MOTION_BLUR | ApplyMotionBlurQuality 0x39B4A4 | +0x1898 **LensSimulationMotionBlur2SampleCount** 2/4/6/8/10 (absolute) | 8 | No |
| LANDSCAPE_QUALITY (LIGHTEST..ULTRA) | ApplyGraphicsSettings 0x39A758 | +0x1970 **TerrainTessellationMaxDistance** = TerrainTessMaxDistanceTable 0x3978390 {1000,1250,1500,2000,4000} (absolute, **overrides env**); +0x1978 **TerrainTessellationDistanceAdaptRate** = TerrainTessAdaptRateTable 0x3977A58 {0.3,0.35,0.4,0.5,0.8}, 0.5 and no distance write while IsEventScene/photo | 5000 / 0.5 | No (it is tessellation reach, not LOD). Renames the two "LandscapeLod*" tables |
| SHADOW_QUAL (OFF..HIGHEST) | ApplyShadowQuality 0x18A87C, ShadowQualityTable 0x3978210 (64 B rows, row = clamp(q,1,5)); ShadowQualityRowGet 0x18A9F8; ShadowRenderTargetsCreate 0x226E918 | rows 1..5: +0 -> +0xB98 **ShadowmapKeyLightResolution** 18/9/10/8/8; +0xC -> +0xB9C **ShadowmapCubicResolution** and +0xBA0 **ShadowmapPlanarResolution** 512/512/1024/1024/1024; +0x10 -> param 85A0BC7F 1024/1024/2048/2048/2048 (read by the VSM pass 0x466087); **+0x14 -> param 93BCE2C1 = VsmTilesPerFrameBudget 2/2/8/8/8** (default 16, read by 0x1F7960); +0x18 -> param 2CE47344 1/0/0/0/0 (read at 0x1F5487); +0x34 -> +0xBA8 SubShadowmapResolution 512/1024/2048/2048/2048; +0x1C..+0x30 RT hashes (HIGH = HIGHEST); +0x3C -> g_RenderGlobals+0x7598 | KeyLightRes 0, cascades start 2000, EraseFarEnd 50000 (env-only, no setting) | No |
| SHADOW_QUAL column +0x38 | **CharacterFarCullFlag 0x876EB8** (the "no reader" column) | raw SHADOW_QUAL row (unclamped): 2000/2000/2000/2000/**3000**/**7000** cm; model +0xEE x100 can override; beyond it the character's render state +0x3B3 = 1 (far flag; most likely its shadow) | - | No (HIGHEST = 70 m vs 30 m) |
| MODEL_QUAL (LIGHT..HIGH) | GetModelQualityLodBias 0x396E18 (2/1/0) -> ScaleByModelQuality 0x396D80, GetModelQualityLodDistance 0x397110, both only in MotionLodParamsBuild 0x396F54 | writes block +0x94..+0xAC (800/800/1200 x 0.5/0.75/1, 1000/1200/1400) | - | **No effect in v2.02** (see addendum A5 and streaming-anim.md): CharacterJobsDispatchA/B copy only bytes 0..0x1F of the block, nothing else reads it |
| MODEL_DRAW (FEW..MANY) | GetModelDrawLevelBias 0x91743C (2/1/0) -> GetMaxDrawnCharacters 0x9173F8 = floor(60 x 0.5/0.75/1) = **30/45/60** -> CharacterDrawBudgetCull 0x2814F0 | sorts active characters (key +0xAD38) and hides the farthest beyond the budget (0x6BAC04(c,1), +0xAE27) | - | No (cap is rarely reached) |
| MESHLET_QUALITY (LIGHT..HIGH) | ApplyMeshletLodScales 0x2353C0 | 8 per-category LOD scales (+0xA108..+0xA124) = param (hashes 1A2055FB, 5AC25A6D, 6FAB22AE, 8493EAEF, 997CB330, AE657B71, C34E43B2, D8370BF3; base 2.0 means "use entry[0]") x entry[1+q] of the runtime table g_pLodScaleCfg+0xA08 (7-float entries, LodScaleTableFindIndex 0x235638); cat 0 base = g+0x8BC; cats 0-3, 6 get + g+0x8C0, capped 10 | runtime data (not in the dumps) | Needs a live read of g_pLodScaleCfg+0xA08 |
| GRASS_DENSITY (LOWEST..HIGH) | GrassPlacementCull 0x31EF94 | min(GRASS,4) -> placement +0xFC; a change rebuilds the placement (0xA483A4 / 0xA34750) | ProcPlacement density maps | See addendum A2 |
| GI | GiRenderScalesApply 0x1872C4 / GiScaleLookup 0x187A50 | gpu-maxsettings.md §1 (already exact) | - | - |

## 2. LOD selection

| Decider | RVA | Rule / constants |
|---|---|---|
| Character LOD tier count | GetLodLevelCount 0x397218 / GetLodTier 0x3971BC | 8/6/4 levels by MOTION_QUALITY (8 if no provider), 1 when IsLodForcedMinimum |
| Character LOD distances | MotionLodParamsBuild 0x396F54 | fixed 1000/3000/1500 + the MODEL_QUAL-scaled ones: **all unread** (only block bytes 0..0x1F are consumed; addendum A5) |
| Character near/far state | CharacterDistanceLodState 0x42E90C | 18 m / 20 m hysteresis -> CharacterModelLodApply 0x42EA40 |
| Placeable LOD | PlaceableDistanceLod 0xDBA70 | surface distance^2 >= 1e6 (10 m) -> far |
| Meshlet LOD | ApplyMeshletLodScales 0x2353C0 (above) + MshLod GPU passes | per-category scale x MESHLET_QUALITY column |
| Terrain | TerrainTessellationMaxDistance / AdaptRate (above); LandscapeLodScaleApply 0x31FAA0 (table all 1.0, no effect) | - |

## 3. Streaming budgets

| Budget | Where | Value |
|---|---|---|
| Texture tile requests | TextureStreamRequestTiles 0x1290B8 | <= 16 per texture per frame; free = g_RenderGlobals+0x7560 (pool capacity) - (max(+0x756C,+0x7570) + +0x7590 (this frame) + +0x7584 + +0x757C + +0x7578); per-frame cap +0x75A8 (create params +0x19C via 0x33114A4; set to **128** at 0x28A930D) |
| Page pool | PagePoolFrameUpdate 0x252510 -> PagePoolScanAll 0x2522C4; PagePoolOccupancyCheck 0x126C64 | fixed buckets x ways; >85 % occupancy heuristic |
| Meshlet streaming | MeshletMarkRequestedAddCost 0x49950, MeshletPageCountdownTick 0x5DF70 | per-bucket cost totals, u16 per-page countdown (meshlet-mainthread.md) |
| VSM tiles | VsmTilesPerFrameBudget 0x1F7960 | SHADOW_QUAL: 2 (LIGHTEST/LIGHT), 8 (STANDARD+), default 16 |

## 4. Culling thresholds

| Cull | RVA | Threshold |
|---|---|---|
| Entities by draw distance | CullEntitiesByDrawDistance 0x1772B4 / GetDrawDistanceLimit 0x177274 | g_pDrawDistanceCfg+0x80, per-entry scale 0x1771B0/0x1770C8 |
| Placeables | GetPlaceableDrawDistance 0x209EB0; PlaceableCullSmallByFrustumDistance 0xDD7AC | p+0x29C or SceneDrawCfg+0x1770; small objects AABB < 1000 on every axis |
| Characters | CharacterDrawBudgetCull 0x2814F0 (30/45/60); CharacterFarCullFlag 0x876EB8 (20/30/70 m by SHADOW_QUAL) | above |
| Shadow casters | env ShadowEraseFarEnd (+0xAF4, 50000) / cascades (+0xAFC..) | env-only; no setting |
| Meshlet precull | MeshletPrecullingRecord 0x5301FC | once per culled view or shadow face (38 sites; perf-coverage §6) |

## 5. Update scheduling
Already covered: logic over-ticking and the TG design (cpu-hotspots-ally.md §4; GameUpdateTick 0x2338B0, DynFpsGovernorTick
0x5AF4E4), SkyAmbient time slicing (43 steps), MergeNatureCloudAndPatternMap (only on sim-time change), VSM tile budget.
Nothing new found here.

## Verdict
None of these tables holds a value that is above what is visible at 1080p: every step is an authored quality change
(more froxels, rays, samples, distance). The exe exposes no knob for VCLOUD ray samples (32/256, env data). The one
Ally-relevant finding: at 540 p render height, AO HIGHEST costs the same rays as HIGH (7) plus SSAOResolution 1.

## Open
- VFSCAT Y dimension and depth slices: closed in addendum A1.
- MESHLET per-category table values: runtime only; read recipe in A4, live values in "A4 result".
- GRASS density math: level path traced (A2), the +0xFC consumer is still open.
- Param hash names: hash function found, names absent from all static sources (A3).

## Addendum (2026-10-08, second pass; static only)

### A1. Volumetric fog froxel grid: X x Y x Z
- The 3D targets are created by **VolumetricFogVolumeEnsure 0x1478F0** (called from VolumetricFogScatterNodeExecute
  0x14871C each execute) -> **VolumetricFogVolumeCreate 0x38C0C4C**: 3 x `Texture3DCreate 0x6D7090(dev, W, H, D, 1, 3, 0)`
  (history ring, node +0xAC0 -> object +0x60/+0x68/+0x70) plus a 0x150-byte buffer (+0x78).
- Inputs come from the per-view **RenderParams** copy (stack, filled by RenderParamsAssign 0x247950 from the view's
  RenderConfig). RenderParams = 0x2F4-byte view header + the env RenderConfig block, so **RenderParams+0x16D0 =
  VolumetricFogResolutionX (env 0x13DC)**, **+0x16D4 = VolumetricFogResolutionZ (env 0x13E0)**. Proof: every fog field
  NodeExecute reads maps 1:1 with this +0x2F4 shift (ScatterScale 0x168C, FarDistance 0x1690, DustScale 0x1694 ...
  CamRalativeHeightAttType 0x16F0).
- **W = X, D = Z (depth slices), H = floor(X / RenderParams+0x14C)**; +0x14C is a header field used as the view aspect
  (width/height). 16:9: HIGH 160 x 90 x 64 (Z 32 areas: 48), STANDARD **120 x 67 x 48** (32), LIGHT 80 x 45 x 32. The
  r5 capture's "160x96x48" Y is not what this code gives at 16:9 (90); likely a different aspect or a padded view.
- Recreated only when the existing texture's width (+0x48 u16) or depth (+0x4C u16) differs; an aspect-only change
  keeps the old H. VolumetricFogScatterResourcesReady 0x34684E0 pre-creates a default **160 x 90 x 64** when flag
  0x400000 is set.
- So the froxel count scales with VOLUMETRIC only (X and Z caps); it never depends on render resolution. Same verdict.

### A2. GRASS_DENSITY: where the level goes
- GrassPlacementCull 0x31EF94 is really the per-area **stage-unit load state machine** (states 1..0xF). State 6 builds a
  0x680-byte procedural-placement unit (PlacementUnitCreate 0x7B3D6C); state 7 registers it (SceneUnitRegister 0xA483A4 ->
  g_pSceneDrawCfg+0x20 list +0x1A8) and writes **unit+0xFC = min(GRASS_DENSITY, 4)**; state 1 re-checks every tick and,
  on a change, writes +0xFC, clears +0xF9 and queues a rebuild: **PlacementRebuildEnqueue 0xA34750** (g_pSceneDrawCfg
  +0x1208, 0x38-byte entries in vector +0x1A8; if the resource is not ready it sets unit+0xEA/+0xF9 instead), then
  sets dirty bytes via 0x28F42F0 ([0x45C3760]+0x268 -> +0x10 -> +0x532A1 = 1) and 0x20AD638 ([0x45B9E38]+0x148 -> +0x1C09 = 1).
- The arithmetic on +0xFC (level -> instance count/spacing) runs in the rebuild consumer, which a byte scan for
  `[reg+0FCh]` reads did not find (no reader near the unit's other fields). Data side (game-data.md §4.6): StaticGrass
  LODDensityRate 0.3, FarClip 15000, MaxBlocks 512; ProcPlacement ViewDistance / ModelInstanceNumLimitArray.
  **Still open**: the consumer of unit+0xFC (next step: a hardware watchpoint on unit+0xFC, or decompile the drain of
  g_pSceneDrawCfg+0x13B0).

### A3. Param hashes: hash function found, names not
- **kids property / RenderConfig field hash: h = sum(c[i] * 31^(i+1)) mod 2^32** (our helper `kidshash.py`).
  Matches 1,560/1,567 RenderConfig fields; an exe-wide scan finds 16,609 `lea name ... mov edx, kh(name)` registrations
  (5,983 distinct names). Other namespaces: material params h*27+c (MaterialFindParam 0x3C768C); type names (SSFog
  0x63BA44B4) use a third, unidentified hash.
- The 11 hashes (85A0BC7F, 93BCE2C1, 2CE47344 and the 8 meshlet ones) are **render-node property overrides**: the
  values in RenderConfig+0x6EC0 (32-byte entries: +0 flags, type in bits 24-27 = 4 u32 / 8 float; +4 hash; +0x18 value*)
  are written each frame by the node-apply functions from the node object's property table (obj+0x40, lookup 0x382314)
  with code defaults:
  - **ShadowNodeParamsApply 0x188C88** (from 0x13F2C4): 85A0BC7F default **4096**, 93BCE2C1 default **16**,
    2CE47344 default 0, among ~30 others; then ApplyShadowQuality overwrites the three per SHADOW_QUAL.
  - **MeshletLodNodeParamsApply 0x26ADD8** (from 0x26A1C4): the 8 meshlet LOD params default **2.0** (the "use table"
    sentinel), plus two more floats default 20000 and 200000.
- Names: **not recoverable statically.** The hashes occur only as immediates in their generated get/set stubs; no exe
  string (81k identifiers), tool dictionary (kassent/extra_props), dump, kidsrender, or shader_dx12 identifier (1.8M)
  hashes to them under kh, h*27+c, CRC32 or FNV-1a, and a 28.8M-candidate dictionary attack (shadow/meshlet/LOD words,
  1-4 tokens) found nothing. None of the extracted kidsrender graphs carries them, so shipped values = the code defaults.

### A4. Meshlet LOD per-category values: exact live read
Per frame (GameUpdateTick 0x2338B0 -> ApplyMeshletLodScales 0x2353C0), for category c = 0..7:
1. `v = param[c]` from camera 0x38D04576's RenderConfig+0x6EC0 (default 2.0, A3).
2. If `[0x45B9E30]` (g_pLodScaleCfg, 0xB10-byte singleton created by 0x26F383C/0x2704378, never a kids object) != 0 and
   its byte **+0xB08** != 0: `T = [g+0xA08]`; entry index = LodScaleTableFindIndex(T+0x20 map, c) (map: +4 empty key
   byte, vector [+8,+0x10) of 8-byte {u8 key, u32 index @+4}); entry = `[T] + 8 + index*0x1C` (7 floats), count `[[T]+4]`.
3. If v in [1.99999, 2.00001]: v = entry[0]. Category 0: v = `[0x45B9E58]+0x8BC` instead (if g_pSceneDrawCfg != 0).
4. v *= entry[1 + MESHLET_QUALITY] (0x45BAFB0: LIGHT 1, STANDARD 2, HIGH 3).
5. Categories 0-3 and 6: v += `[0x45B9E58]+0x8C0`, capped at 10.0. Result stored at (ApplyMeshletLodScales arg1, the GameUpdateTick object) +0xA108 + 4c.
**Live read**: `g=[Nioh3+0x45B9E30]`, byte g+0xB08, `T=[g+0xA08]`, `B=[T]`: u32 B+4 count, then count x 7 floats from B+8;
map `M=[T+0x20]` for key->index; also floats `[Nioh3+0x45B9E58]+0x8BC/+0x8C0`, and the 8 outputs at the caller's +0xA108.
No writer of g+0xA08 was found statically (only the ctor zero-fill 0x24B140C and the release in 0x24BCDF8), so the
table is attached at runtime by a resource load; the live read is the only source.

### A5. MODEL_QUAL: no effect (settles the conflict with streaming-anim.md)
- Every reference to 0x45BAF90 (exe-wide rip-relative scan): GetModelQualityLodBias 0x396E18, menu 0x60732F/0x608270,
  options 0x12A34FB/0x12A4486, config load/save 0x1FE0C7A..0x1FE5968, and an accessor pair 0x281F1DC (get) / 0x281F59C
  (set) that nothing calls or points to. 0x396E18's only callers are ScaleByModelQuality 0x396D80 and
  GetModelQualityLodDistance 0x397110, both called only from MotionLodParamsBuild 0x396F54.
- CharacterUpdateDispatch 0x3972C4 passes the 0xE0-byte block only to CharacterUpdateBalanceAndRun 0x399C1C, which
  passes it only to CharacterJobsDispatchA 0x31A86C (copies dwords 0..7) and CharacterJobsDispatchB 0x31A2C0 (two
  movaps = 0x20 bytes; the block pointer [rbp+50h] is loaded once). Nothing reads +0x94..+0xAC (or +0x3C).
- **This agrees with streaming-anim.md; §1/§2 above reflect it.**

### A4 result (live read on PC, 2026-10-08)
Table has 8 entries x 0x1C B; 7th dword is an int category key. e0 = 2 for all categories except cat 3 = 8 and cat 4 = 0; e1..e5 = 2,1.5,1,1,1 for every category. At MESHLET_QUALITY=2 (multiplier e3=1) result = {2,2,2,8,0,2,2,2}; scene cfg +8BC=2, +8C0=0. LIGHT would use the 1.5 entry.
