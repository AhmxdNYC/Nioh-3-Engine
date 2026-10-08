# Work deciders: settings -> pass parameters, LOD, streaming, culling, scheduling (Steam v2.02)

RE only (decomp + exe data; nothing installed, game not run). Symbols: `re\symbols\incoming\work-deciders.tsv` (new)
and `work-deciders-renames.tsv` (apply in place). Builds on `gpu-maxsettings.md` §1, `perf-coverage.md`, `shadows-culling.md`,
`meshlet-mainthread.md`, `cpu-hotspots-ally.md`. Ally config referenced below: REN_RAT 50 (960x540), SHADOW/AO/GI/SSR/VOLUMETRIC STANDARD.

## 0. Key: RenderConfig field names come from the environment schema
`ApplyGraphicsSettings` 0x39A39C (every frame) works on **RenderConfig** ("S"), which is refilled from the stage's
environment DB and then scaled or overridden per quality setting. The two schema registration functions
(0x25F15B0, 0x2629194) register each field as `lea name; mov [rsp+28h], offset; mov edx, hash`, so offset = RenderConfig
offset. `py -I re\scripts_re\renderconfig_fields.py` writes all **1,567** fields to `re\out\renderconfig_fields.tsv`
(`--off 13dc 107c` looks offsets up). Env values below are from the dumped env DB 0x396b9210 (`data\dumps`).

## 1. Quality setting -> pass parameter (what each applier really writes)

| Setting (levels) | Applier | RenderConfig field <- value per level | Env value | Same-look lever? |
|---|---|---|---|---|
| VOLUMETRIC (LIGHTEST..HIGH) | ApplyVolumetricQuality 0x39B080 | +0x13DC **VolumetricFogResolutionX**, +0x13E0 **VolumetricFogResolutionZ**: LIGHTEST 0/0 (+0x1398 ScatterScale x0 = fog off); LIGHT /2 cap 80/32; STANDARD cap 120/48; HIGH x1.5 cap 160/64. +0x7B4 **VolumetricCloudResolution = 2 at every level** | X 160, Z 64 (32 in some areas); CloudRes 1 | No: froxel X is not tied to render width (Ally STANDARD 120 froxels over 960 px = 8 px each). Authored quality steps |
| (VFSCAT grid input) | VolumetricFogScatterNodeExecute 0x14871C | cb+14D0/14D4 = RenderSystem+0x960/0x964 x view scale x dyn-res. **+0x960/964 = render width/height** (g_RenderResolution 0x45BB028, copied each frame by RenderSystemSyncRenderSize 0x12399D0), not the froxel grid. The grid is X x Y x Z above; Y not traced (r5: 160x96x48 at HIGH with env Z 32) | - | Corrects perf-coverage "copy path not traced" |
| (VCLOUD steps) | none | +0x40C **VolumetricCloudMinRaySampleNum**, +0x410 **MaxRaySampleNum**: no setting touches them; VCLOUD cost = env data + quarter-res target | 32 / 256 | No setting knob exists; only a data/memory poke (not same-look) |
| SSR (OFF..HIGHEST) | **ApplySsrQuality** 0x39B0E8 (was ApplySetting23) | +0x107C **RLRMaxRaySamples** x 0 / 0.5 / 0.75 / 1 / 1.5 / 2; +0x109C RLRResolution = 0 always; resource 0xEF3D849D +0xB8 = 1 (LIGHTEST..STANDARD), 0 (HIGH, HIGHEST) | 0 in this env (shader default?) | No |
| AO (OFF..HIGHEST) | ApplyAmbientOcclusion 0x39B20C | +0x12F4 **SSAORayNum** 0/2/3/5/7/14; +0x12F8 SSAOStepNum 2; +0x12D8 SSAOContrast 1.75; +0x12E4 SSAOQuality 0 (LIGHTEST) else 1. HIGHEST at render height <= 720: 7 rays + SSAOResolution (+0x12E8) = 1. ApplyGraphicsSettings also forces +0x12F0 SSAOUseViewDepth = 1 | 8 rays / 6 steps (overridden) | No. Note: on the Ally (540 p) HIGHEST = 7 rays like HIGH, plus SSAOResolution 1 |
| MOTION_BLUR | ApplyMotionBlurQuality 0x39B4A4 | +0x1898 **LensSimulationMotionBlur2SampleCount** 2/4/6/8/10 (absolute) | 8 | No |
| LANDSCAPE_QUALITY (LIGHTEST..ULTRA) | ApplyGraphicsSettings 0x39A758 | +0x1970 **TerrainTessellationMaxDistance** = TerrainTessMaxDistanceTable 0x3978390 {1000,1250,1500,2000,4000} (absolute, **overrides env**); +0x1978 **TerrainTessellationDistanceAdaptRate** = TerrainTessAdaptRateTable 0x3977A58 {0.3,0.35,0.4,0.5,0.8}, 0.5 and no distance write while IsEventScene/photo | 5000 / 0.5 | No (it is tessellation reach, not LOD). Renames the two "LandscapeLod*" tables |
| SHADOW_QUAL (OFF..HIGHEST) | ApplyShadowQuality 0x18A87C, ShadowQualityTable 0x3978210 (64 B rows, row = clamp(q,1,5)); ShadowQualityRowGet 0x18A9F8; ShadowRenderTargetsCreate 0x226E918 | rows 1..5: +0 -> +0xB98 **ShadowmapKeyLightResolution** 18/9/10/8/8; +0xC -> +0xB9C **ShadowmapCubicResolution** and +0xBA0 **ShadowmapPlanarResolution** 512/512/1024/1024/1024; +0x10 -> param 85A0BC7F 1024/1024/2048/2048/2048 (read by the VSM pass 0x466087); **+0x14 -> param 93BCE2C1 = VsmTilesPerFrameBudget 2/2/8/8/8** (default 16, read by 0x1F7960); +0x18 -> param 2CE47344 1/0/0/0/0 (read at 0x1F5487); +0x34 -> +0xBA8 SubShadowmapResolution 512/1024/2048/2048/2048; +0x1C..+0x30 RT hashes (HIGH = HIGHEST); +0x3C -> g_RenderGlobals+0x7598 | KeyLightRes 0, cascades start 2000, EraseFarEnd 50000 (env-only, no setting) | No |
| SHADOW_QUAL column +0x38 | **CharacterFarCullFlag 0x876EB8** (the "no reader" column) | raw SHADOW_QUAL row (unclamped): 2000/2000/2000/2000/**3000**/**7000** cm; model +0xEE x100 can override; beyond it the character's render state +0x3B3 = 1 (far flag; most likely its shadow) | - | No (HIGHEST = 70 m vs 30 m) |
| MODEL_QUAL (LIGHT..HIGH) | GetModelQualityLodBias 0x396E18 (2/1/0) -> ScaleByModelQuality 0x396D80, GetModelQualityLodDistance 0x397110, both only in MotionLodParamsBuild 0x396F54 and only when g_pLodTierProvider != 0 | character update params: three distances 800/800/1200 x 0.5/0.75/1.0 and one distance 1000/1200/1400 | - | **Consumer found** (was "not located"): MODEL_QUAL scales character LOD/motion distances; it does not touch meshlet LOD |
| MODEL_DRAW (FEW..MANY) | GetModelDrawLevelBias 0x91743C (2/1/0) -> GetMaxDrawnCharacters 0x9173F8 = floor(60 x 0.5/0.75/1) = **30/45/60** -> CharacterDrawBudgetCull 0x2814F0 | sorts active characters (key +0xAD38) and hides the farthest beyond the budget (0x6BAC04(c,1), +0xAE27) | - | No (cap is rarely reached) |
| MESHLET_QUALITY (LIGHT..HIGH) | ApplyMeshletLodScales 0x2353C0 | 8 per-category LOD scales (+0xA108..+0xA124) = param (hashes 1A2055FB, 5AC25A6D, 6FAB22AE, 8493EAEF, 997CB330, AE657B71, C34E43B2, D8370BF3; base 2.0 means "use entry[0]") x entry[1+q] of the runtime table g_pLodScaleCfg+0xA08 (7-float entries, LodScaleTableFindIndex 0x235638); cat 0 base = g+0x8BC; cats 0-3, 6 get + g+0x8C0, capped 10 | runtime data (not in the dumps) | Needs a live read of g_pLodScaleCfg+0xA08 |
| GRASS_DENSITY (LOWEST..HIGH) | GrassPlacementCull 0x31EF94 | min(GRASS,4) -> placement +0xFC; a change rebuilds the placement (0xE583A4 / 0xE44750) | ProcPlacement density maps | Density math below +0xFC not traced |
| GI | GiRenderScalesApply 0x1872C4 / GiScaleLookup 0x187A50 | gpu-maxsettings.md §1 (already exact) | - | - |

## 2. LOD selection

| Decider | RVA | Rule / constants |
|---|---|---|
| Character LOD tier count | GetLodLevelCount 0x397218 / GetLodTier 0x3971BC | 8/6/4 levels by MOTION_QUALITY (8 if no provider), 1 when IsLodForcedMinimum |
| Character LOD distances | MotionLodParamsBuild 0x396F54 | fixed 0x447A0000 (1000), 0x453B8000 (3000), 0x44BB8000 (1500) + the MODEL_QUAL-scaled ones above |
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
- VFSCAT Y dimension and depth slices: the 3D target is created from the RenderConfig mirror (RenderConfigSyncShaderParams
  0x1E2C00, dirty bits 8/0x10), not traced to the create call.
- MESHLET per-category table values (runtime, g_pLodScaleCfg+0xA08); GRASS density math below placement +0xFC.
- Names of the ktgl param hashes (85A0BC7F, 93BCE2C1, 2CE47344, the 8 meshlet LOD hashes).
