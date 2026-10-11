# renderer/09-volumetrics: VFSCAT fog, VCLOUD sky clouds, sky ambient, static-nature SSFog, ocean, rain, wind (package 09, 2026-10-10)

Static pass over `<re-scratch>\decomp` plus the existing notes. Game not run. Tags: **M** measured (cited note), **S** read in the C export, **?** inferred.
RVAs are image offsets (base 0x7FF74F410000). Node numbers and L-markers come from `05-frame-graph.md` (FCAP1 order). Anything FCAP1 has not confirmed is marked.

## 1. Entry points

| node (graph) | Execute | what it does | queue |
|---|---|---|---|
| NatureSky (267, `NUpdate > DrawSky`) | NatureSkyNodeExecute 293220 -> NatureSkyRender 293230 (1.4 KB) | sky body, VCLOUD, screen-space velocity | DIRECT, plus **async slot 3** for the cloud dispatch |
| (node 4) | NatureSkyRenderPatternMap 2935E4 | only `RenderSkyPatternMap` (pattern / noise map), no clouds | DIRECT |
| VolumetricFogScatter (162) | VolumetricFogScatterNodeExecute 14871C (3.2 KB) | froxel light scatter (VFSCAT) | **async slot 4** |
| StaticNature (275 / 288) | StaticNatureNodeExecute 6D7B88 -> Record 6D7B98 -> StaticNatureRenderView 108590 (6.4 KB) | SSFog apply, static-nature draws, VFSCAT join (peek slot 4) | DIRECT |
| UltraMarine (138) | UltraMarineNodeExecute 852620 -> RenderViews 852650 -> UltraMarineRecord 850FE0 | ocean (game thread `OceanThread%d` feeds it) | **async slot 6** |
| SkyAmbient (node 8) | SkyAmbientNodeExecute 145C60 | sky-ambient bake, time-sliced | DIRECT (compute) |
| RainDrop (nodes 5, 332-334) | RainDropNodeExecute 419F54 (2.4 KB) | rain / drop simulation | **async slot 8** |
| Tree2Wind | Tree2WindNodeExecute 41805C -> 418284 | wind field for vegetation | DIRECT ? |
| PrepareStaticNatureReflection | 1528B4 | static-nature reflection prepare, `CopyRectCommand` | DIRECT |

## 2. VFSCAT (VolumetricFogScatter)

**Output.** A 160x90x64 froxel scatter volume at VOLUMETRIC HIGH (= max). Sizes come from `VolumetricFogResolutionX / Z` at RenderConfig+13DC / +13E0, H = floor(W / aspect) **S**.
`VolumetricFogParamsLoad` 244BA8 fills RenderConfig+13C0..13FF from the env fog object. `RenderConfigSyncShaderParams` 1E2C00 copies it to the shader params.

**Data.**
- `VolumetricFogVolumeEnsure` 1478F0 keeps a lazy holder of 0x80 bytes (`VolumetricFogVolumeAlloc` 38C0D28).
- `VolumetricFogVolumeCreate` 38C0C4C makes **3 x Texture3D** (+60 / +68 / +70, the **history ring** for temporal reprojection) and a 0x150-byte buffer at +78. They are recreated when W or D change **S**.
- Shader: `VFSCAT-CllLpvSfbVsm` (cs 641524ee / fcc40d4b, `shader_dx12\0xa9910db8.file`), 8x8x4 groups.
- Shader inputs: VSM page flags `t16` (uploaded by VsmFlagsUpload 912A2C), cascade matrices (CascadeMatrixGet 147F60), clustered light lists, LPV, the ESM level (EsmLevelFromRes 147324), dust / wind constants.

**CPU flow (VolumetricFogScatterNodeExecute) S.**
1. View index = `(node+B13h & 7F) + (R+974h & 7FFFFFFF)`. Bail if >= 32 or `scene+180h[view]` is NULL.
2. Copy RenderParams (`RenderParamsAssign` 247950) into a 0x3F28-byte stack block. `scene+2CC` (time) and `scene+B2EC` (second time-of-day input) go to node+AD8 / ADC.
3. Gate: `FUN_46EB30` (fog enabled for this view) and `FUN_147704` (node resources). Pick the light source: LPV / local lights (`FUN_1473C8`, `FUN_14815C`) or an empty fallback.
4. Resolve the VSM objects (`ViewGetShadowObj` 533E88), the LPV (`FUN_147854`) and the cascade set (`VolumetricFogCascadeSetup` 1479E4).
5. `VolumetricFogUploadLightLists` 147F98 uploads the **light lists** into a per-frame ring of 6 buffers (3 list kinds x 2 parities; strides 0x1B0 / 0x80|0x40 / 0xD0|0x70, index vf+D4) ?.
6. If the async join is armed (`FUN_545C44` and `RS+13AC`): `KtglSignalUntilAwake`, `AsyncSlotGetCtx(RS+1130, 0)`, record, `KtglCtxGetFence`, `AsyncSlotAddFence`. This is the **slot-4 producer** (the wrapper passes id 0 here; the peek side uses 4; question for the backend package).
7. `VolumetricFogScatterRecordAsync` 90749C (249 B) sets the viewport from `g_RenderGlobals` and calls **VolumetricFogScatterSetupDispatchAsync 18FAD4**. That fills the scatter cbuffer (+234 / +238 / +23C ...) and calls `vf+60h` (async dispatch). The sync twin is 38C1084 (`vf+68h`).

**Join.**
- `StaticNatureRenderView` calls the SSFog binding helper 4F9AD8 (at 108C52).
- Gated by `RS+968h & 20000` it peeks slot 4 (`3C5D0C(RS+1130,&f,4)`) and calls `GfxWaitAsyncFence` 4F8F38. On the ECL thread that is a GPU-side `queue->Wait` (tok8). There is no CPU wait **M** (`ally-gpu-gap.md`).
- The slot-4 dispatch is recorded around node 162 but reaches the stream after node ~203 (`05-frame-graph.md` §5).

**Cost.**
- pc2: ~1.04-1.3 ms async wall.
- Ally at MAX: 1.25-1.6 ms async, 0.71 ms if run synchronously. The "join gap" is the fog running alone, so removing the Wait gains ~0 **M**.
- PC (RX 9070): ~0.29 ms, overlapped.

**Cheap win (already built).** `fogpar` = merged early-exit VSM search plus pipelined light list. The VSM clip-level search was 44 of 84 dependent round trips (52 %). Expected 0.5-1.3 ms on the Ally, ~0 on the PC (`gpu-fog-rewrite.md`).

## 3. VCLOUD (NatureSkyDrawClouds 5F2A88, 9.5 KB, 112 callees)

- Called from NatureSkyRender 293230. The marker strings in 293230 are `"NUpdate"`, `"RenderSkyPatternMap"`, `"DrawSky"`, `"DrawStaticNature"`, `"DrawScreenSpaceVelocity"`.
- Order inside 293230 **S**:
  1. PatternMap (2935E4) and `NatureSkyUpdatePatternParams` 5F1628;
  2. `NatureSkyPrepareDraw` 293D5C (resource readiness);
  3. `DrawSky` marker;
  4. cloud history pick (`node+244` toggles 0/1 per frame: a **2-deep ping-pong**);
  5. DrawClouds;
  6. `DrawScreenSpaceVelocity` (sky velocity for TAA / FSR, needs both history targets).
- DrawClouds: re-render scale `1/(1<<n)` read from the render params (the VCBayer target is render/8, 156x88 at 1248x702), cloud-range constants (0x461C4000 = 10000.0), then `KtglEmitToken70` (compute token wrapper) and a fence through slot 3.
- The compute is `cs b76874bb` (record 7 of 8 in `0x5f17ca9d.file`, `numthreads(8,8,1)`), **220 wave64 only**, so the wall is serial-latency bound, not ALU bound.
- Cost **M**: 0.5-0.7 ms in light frames, 2.1-4.4 ms in heavy spawn frames, 1.6 ms mean async on pc2 (`gpu-volumetrics-rewrite.md`).
- Cheap win built: vc7 `u / pfu` (if-convert the 4-step light march, hoist samples, prefetch), 1.5-2.0x fewer critical cycles. The Windows variants are not bit-exact (1-3 fp16 ulp); the Ally variants `p / pfp` are bit-exact.
- Next step if more is needed: a half-rate temporal update (the Bayer is already temporal).

## 4. Sky ambient, SSFog, wind, rain, ocean

- **SkyAmbient (145C60).**
  - Mode `RS+3C8`: 0 = update every frame, 1 = only when `SkyAmbientDetectChange` 1467EC fires, then time-sliced over **43 steps** (0x2B; step length = `RS+29C / 43` seconds), 2 = immediate.
  - Steps are recorded by `SkyAmbientRecordSteps` 145F6C (1.6 KB; per-step recorders 29A740..29C9E0).
  - On change it sets `RS+96C |= 4` and a flag in the sky object (+188), so GI, fog and ambient re-sample **S**. Cost is ~1/43 of a full bake per frame.
- **SSFog (StaticNatureRenderView 108590).**
  - 25.9 KB of draw record for static nature. It applies SSFog constants and textures via 4F9AD8, binds `MaterialBindS2FAcclParams` 3C4B74 and is the VFSCAT join.
  - Callers: StaticNatureNodeRecord (nodes 275 / 288), NatureSkyRender and NatureSkyDrawClouds (the sky pass reuses the view set-up).
- **RainDrop (419F54).** View index `(node+53 & 7F) + R+974`, mode `(R+95C & 3) + 1`. Uses **async slot 8** (`AsyncSlotGetCtx(RS+1130, 8)`, `AsyncSlotAddFence` 148514) on two code paths. The sim is compute, joined later by surface shaders.
- **Ocean (UltraMarine).** `UltraMarineRecord` builds a look-at view (`CameraBuildLookAtView` 244264), offsets the origin by RS+44D0, and records only if `0 < near < far` and a flag is set. Slot 6; 05 notes the overlap is poor (§5). The `OceanThread%d` heightfield producer is game code outside this partition.
- **Wind (Tree2Wind 41805C).** A 172 B shell over 418284 (2.3 KB). The wind texture feeds vegetation and the fog dust / wind constants.
- **Time of day.** It enters through `scene+2CC` (seconds) and `scene+B2EC`. Fog, clouds, sky ambient and sun / moon directions (`SkySunMoonDirections` 5EBD08) all read them, so a new renderer needs one per-frame "environment state" struct.

## 5. Costs (existing data)

| pass | pc2 (RX 6650 XT) | Ally | PC |
|---|---|---|---|
| VFSCAT | 1.04-1.3 ms async | 1.25-2 ms | 0.29 ms |
| VCLOUD | 0.5-4.4 ms async (1.6 mean) | n/a | small |
| SkyAmbient, Ocean, Rain | not isolated in GP2 | | |

## 6. New renderer: keep / replace

- **Keep:** the env-state inputs (time, sun / moon, RenderConfig fog block), the async-slot overlap idea, the 43-step sliced ambient, the cloud ping-pong history and the froxel history ring. All are 3D-texture + compute designs that port directly.
- **Cheap wins:**
  1. Move the joins: kick VFSCAT earlier, put the join after the lighting chain; kick VCLOUD before node 4 (it needs only sky params). Needs FCAP1 confirmation.
  2. VCLOUD at half update rate (checkerboard of the existing Bayer): halves a ~1.6 ms async cost for a small temporal-quality price.
  3. fogpar (built): lower VFSCAT latency, Ally only.
  4. Skip the scatter when the view has no fog volume / dust (gate 46EB30 exists; otherwise the VSM search runs at full cost).
- **Replace:** nothing here is worth a rewrite; the passes are shader-bound and the CPU parts are small (3 KB of node Execute each).
- **Questions for other packages:** slot id 0 vs 4 in the fog wrapper (backend); `FUN_14815C` / `FUN_1473C8` light-list builder semantics (lighting); graph node numbers of SkyAmbient and RainDrop (05 table).
