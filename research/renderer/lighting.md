# renderer/lighting: lighting model, shadows, GI / probes, AO, volumetric inputs (package 03, 2026-10-10)

Static work only (Steam v2.02, RVAs on image base 0x7FF74F410000). The game was not launched. Sources: the main render graph
`data/dumps/kgk_0x102dd706.kidsrender.txt` (node numbers below are that graph's walk order = execution order), the RT table
`0x93fb3280.kidsobjdb.txt` through `re/scripts_gpu/frame_graph.py --md`, `<re-scratch>\decomp`, the decompiled shader sources in
`gpu-shaders/fog/vf235_par_src.hlsl` and `gpu-shaders/gi/out/rp121_fast.hlsl`, and the measured costs of the earlier notes
(shadows-culling, shadows-redundancy, same-look-package, gi-highest-audit, gpu-gi-rewrite, gpu-volumetrics-rewrite,
cost-vs-looks, gpu-resolution-audit, invisible-gpu, render-architecture, ally-gpu-budget, settings-cost). Scope follows
`00-map.md` (package 03, LIGHT rows). Sky / clouds / froxel fog are package 09 (volumetrics); only their lighting inputs are
here. SSR is listed only as an input of the reflection gather.

Cost columns: **r5** = RX 9070 RGP trace (1080p render, HIGH, 10.7 ms GPU frame), **pc2** = RX 6650 XT GP2 heavy windows (1080p
HIGH FSR 65 = 1248x702 render, ~30 ms frame), **Ally** = ROG Ally GP2 / GP markers at its own settings (~STANDARD, 540p-ish).
M = measured, E = estimate. Markers: "FC marker" = the engine's own debug marker (node name when `[ctx+2A62]` is set, plus the
named markers "Cascade %d", "VirtualShadowmap", "Clipmap %d", "Batch Tiles %d", "Tile %d %d", "GenerateShadowMapHiZ",
"DrawSky"); `patcher\gpu_markers.fsx` / `[Profiler] FrameCap` turn them on.

## 0. Summary

- **Hybrid deferred renderer.** A fat MRT G-buffer (7 color targets + D32S8), then screen-space passes write sun shadow, AO and
  GI *into G-buffer slices*, then up to 12 stencil-classed full-screen **ClusteredDeferredShading2Pass** draws (graph nodes
  167-186, 212, 217, 272, 346) resolve all lights per pixel from **clustered light lists** (log-z froxel clusters built on the
  GPU by node 161 ClusterLightCulling). Forward paths exist only for transparents / water / effects (other packages).
- **Sun / moon = "parallel lights".** One key light (Light0) plus up to 8 "sub" parallel lights. The key light has **two shadow
  systems at once**: a **virtual shadow map** (16 clipmaps, 4096²x16 sparse page pool, page cache, static geometry only) and a
  **cascaded atlas** (4096², 3-4 cascades, every frame, dynamic casters + trees). Both are resolved once per frame into a
  screen-space "DirectOcclusion" mask (WDO passes, nodes 134/208) that the deferred passes read.
- **Point / spot lights** (incl. effect lights) live in clustered lists `tCll*`; their shadows are cube arrays (128/256) and
  planar arrays (512/1024), rendered by LightPass (node 29) with GPU meshlet culling.
- **GI = OBORO probe field** (octahedral depth + SH irradiance + radiance probes, DDGI-like), gathered per pixel at 0.6 / 0.5
  render scale by the RefProbe compute shaders (nodes 150 / 153, plus water-view 243 / 246 / 292), denoised (151), bilaterally
  upsampled into the G-buffer RR array. LPV / RSM inject / propagate shaders exist but run no per-frame GPU work.
- **Ambient**: sky ambient (node 8, time-sliced, ~0 cost), per-material ambient scalers, probe GI. **AMBSL is not ambient light**:
  it is the StaticScreenLayout UI quad shader (render-nodes.md), so it is out of scope here.
- **AO**: HBAO (node 147, half-res, one merged draw per stage), written into the G-buffer; plus an SDF occlusion term (node 3).
- **Cost (pc2 heavy, ~30 ms frame):** shadows 2.02 ms, deferred + sun-shadow resolve 1.16, GI family 1.35 (RefProbe 0.80),
  ESM 0.18-0.21, AO 0.12-0.13 (+ generic Gaussian GFM 0.25), plus the async volumetric walls VFSCAT 1.04 / VCLOUD 1.60.
  Lighting + shadows + GI ≈ 4.7 ms of graphics-queue time (16 %), ≈ 7 ms with the volumetrics.
- **New-renderer verdict (section 9, all estimates):** a same-look reimplementation could save **≈1.3-2.3 ms on pc2 (4-8 %),
  ≈0.8-1.1 ms on the RX 9070 (8-10 %), ≈2.5-5 ms on the Ally (E)**, mostly from the sun-shadow resolve, the GI gather layout, the ESM and the
  cascade/VSM split. Lighting is not where a new renderer gets its big win; the meshlet front-end (render-architecture F1) is.

## 1. G-buffer (graph nodes 61-64 main view, 68-71 terrain class, 205 second view)

Main-view MRT (BeginScene node 61, `Flag 'F'`, 7 RTs, `ColorRenderTargetSurfaceID [0,0,0,0,2,1,0]`), all 1248x702 at pc2 render
size. Formats are KTGL `PixelFormat` codes (a table scan over the dump for a DXGI mapping table with RGBA8 / R32F / RGBA16F /
D32 / D32S8 at those indices found none; bytes are inferred from usage).

| RT | hash | KTGL fmt | B/px | engine names (node params) | content (from names + consumers) |
|---|---|---|---|---|---|
| 0 | 8b18ce44 | 01 | 4 | GBufferAB, GBufferAlbedo, SrcAlbedo | albedo; AO pass 147 renders into this scope (AO lands in AB, inference) |
| 1 | 7e94c315 | 41 'A' | 4 | GBufferNS, GBufferNormal, Normal | normal (+ spec/flags); read by every light / GI / cluster pass |
| 2 | 521c7ded slice 0 | 01 | 4 x5 | GBufferRS, GBufferRoughness | roughness / material params |
| 3 | 58d0a6f2 slice 0 | 6B 'k' | 4 x5 | GBufferEM / GBufferRR / ReflectionProbeTarget / MergeTarget | emissive (slice 0); the same array later receives GI + reflection results (RR) |
| 4 | 521c7ded slice 2 | 01 | | GBufferRS[2] | material params 2 |
| 5 | 58d0a6f2 slice 1 | 6B | | GBufferRR[1] | |
| 6 | 62590470 | 69 'i' | 4 | GBufferVl, GBufferVelocity | velocity (FSR, temporal GI/AO) |
| DS | e13ddb4f | 6C 'l' | ~5 | GBufferD1, OpaqueDepth, SceneDepth | depth + stencil (D32S8 class); stencil = light group / material class |

- The 5-slice arrays: `521c7ded` (RS) is also bound as GBufferOC (SDF occlusion, node 3), GBufferAdditional (OBOROExport 159) and
  **DirectOcclusion = slice 3** (written by DeferredIndirectPass 134/208, BeginScene 208 SurfaceID 3). `58d0a6f2` (RR/EM) is
  the GI / reflection output. The terrain class (node 68) writes RS slices 0/1/2/4 instead.
- `GBufferMaterialIDTable 0x2c7443d2` on every deferred pass maps a material ID to a shading class.
- Edits: GBufferEdit nodes (122, 131, 140, 143, 215, 258, 259) copy AB/NS/RS/EM to GBufferEdit* (hazard copy) and draw decals,
  snow, miasma; PrepareAccumulatedSnow 125. Detail in materials.md.
- Size: ~50 MB bound at 702p (57 B/px if every slice written), ~25-30 MB realistic write set (render-architecture §2.3); not
  bandwidth-bound at 65 % render scale.

## 2. Pass order (main view) with inputs, outputs, CPU entry, cost

| node(s) / FC marker | pass | reads | writes | CPU (RVA) | r5 µs | pc2 ms | Ally ms |
|---|---|---|---|---|---|---|---|
| 3 SignedDistanceField | SDF occlusion ("SDFOCC") | RS (GBufferOC) | RS slice | (package 09/other) | - | small | in GI family 1.77 |
| 8 SkyAmbient | sky ambient SH, **43 time-sliced steps** over `SkylightAmbientInterpolateSpan` s | sky | ambient buffer | SkyAmbientNodeExecute 145C60, SkyAmbientRecordSteps 145F6C, SkyAmbientDetectChange 1467EC, SkyAmbientUpdateParams 1464A8 | <30 | <0.03 | ~0 |
| 24 VirtualShadowMapPass / "VirtualShadowmap", "Clipmap %d", "Batch Tiles %d" | sun VSM page render (static list A + trees/grass) | page table, valid-rect SAT, Hi-Z | VSM pool `a6861e5b` 4096²x16 (f4E) | VsmPassNodeExecute 465384 → VsmPassExecute 465414 (mode 6, PC) / VsmClipmapRender 1F8340 (mode 5, Ally) | part of 232 + 63 cull | 0.44 tiles | 1.8-2.0 (bursty 0.15-9.3), Batch Tiles 8 self 1.2-1.4 |
| 26-28 ClearScene + View / "Cascade %d", "GenerateShadowMapHiZ" | cascade atlas (dynamic casters), two-phase occlusion | lists A/B, Hi-Z | KeyLightShadowmap `ea36c27d` 4096² (full HTILE clear each frame); VFInjectionShadowMap (ESM source); ShadowmapOnlyOBOROLight `21f704cb` 2048² | SceneRenderView 535188 cascade loops (5384AA / 5389BB) → ShadowCascadeRender 1F5C2C, GenerateShadowMapHiZ 9B1850 | SM draws 232, Hi-Z 36 | SM 0.62, cascades ~0.25 each | cascades 0.15-0.2 each, Hi-Z 0.17 |
| 29 LightPass | point / spot (+ESM) shadow maps | caster lists | TCubicShadowmapArray 128/256, TPlanarShadowmapArray 512/1024, ESM twins, DynamicPlanarShadowmap 1024 | LightPassExecute 7E919C → LocalLightShadowmapRender 7E92B4 | inside the 232 (SM-CM0Sfb) | in SM 0.62 | small |
| 30 View | RSM capture view (OBORO relight, event-driven) | VSM | RSM color / normal / albedo / depth 512..8192 | (view) | ~0 per frame | ~0 | ~0 |
| 134 DeferredIndirectPass (shader 0x3e4ccfb5 / "DeferredNoSubShadowDOPass" 0xcdc8f8d5) | **sun-shadow resolve → DirectOcclusion**: WDO#14 resolve (cascades + sub shadow + VSM with `tVirtualShadowMapFlags`), WDO#28 depth-aware vertical blur | D1, NS, AB, KeyLight atlas, SubShadowMap, VSM, RSM | RS slice 3 | DeferredIndirectPassExecute 149A94 → Impl 149AC4 (→ 14B740, VsmFlagsUpload 912A2C) | 120 + 367 | ~0.5 (WDO row 0.80 in cost-vs-looks) | 0.65-0.70 + 0.17 |
| 147 SSAO | HBAO: lin-depth (full), point downsample (half), main (1 merged draw, `nSwitchRef[N]` per stencil group), blur H/V, temporal | D1, NS, velocity, PrevDepth | AB scope | SSAONodeExecute 15612C, ApplySSAOPassExecute 3688234, ApplyAmbientOcclusion 39B20C (rays 2/3/5/7/14) | ~0 (masked) | 0.12-0.13 | 0.51-0.54 |
| 149 DeferredIndirectPass (ESMShaderObjectHash 0x1d2050e8) | **ESM** generation + 7-tap separable blur (H#9, V#4); LPV inject/propagate shaders bound but no GPU work seen | KeyLight atlas | ESM `303038e6` 2048² (f02) at HIGH | Impl 149AC4, **EsmLevelFromRes 147324** | 57 + 239 | 0.18-0.21 | in shadow 1.38 |
| 150 / 153 ReflectionProbePass, 151 Denoiser, 159 OBOROExport | **GI gather**: #121 diffuse (8 corners x 3 levels, SH + oct depth Chebyshev), #414 reflection (oct radiance + SSR `sRLRResult`), each dispatched once per stencil light group [1,2]; #9 bilateral upsample, #15 resample | D1, NS, RS, VSM, KeyLight, OBORO field, velocity, prev depth/normal | RR array `58d0a6f2` | ReflectionProbePassNodeExecute 54294C → ReflectionProbePassRecord 542C08 (→ 1F40BC); GiRenderScalesApply 1872C4 / GiScaleLookup 187A50 | 338 + 317 (+53 water) | 0.58 CS / 0.80 family | 2.4-2.5 mean, up to 6.1 (#121 3.1 + #414 3.0); #9 0.28, #15 0.11 |
| 161 ClusterLightCulling (shader 0x4759a648) | builds the clustered light / decal / lasmap lists | OpaqueDepth, Normal | `tCllLightIndices` etc. | EffectRenderNodeExecute 3D0AE0 (effects record at this slot too); CPU light list: `[App]LLSCLocalLightListSetupWorker` job (string at 22760A0) | small | not in top rows | not in top rows |
| 162 VolumetricFogScatter (async slot 4) | froxel scattering (package 09) — lighting inputs listed in §6 | ESM, cascade, VSM, cube/planar + their ESMs, LPV, clusters | froxel volume 160x90x64 | VolumetricFogScatterNodeExecute 14871C, VolumetricFogScatterSetupDispatchAsync 18FAD4 | 293 async | 1.04 wall | 1.07-1.32 |
| 163-188 ClusteredDeferredShading2Pass x9 (+ SnowScattering 169, SSSS 188) | **deferred lighting** per stencil class (see §3) | D1, NS, RS, AB, RR, cube/planar shadow arrays, IES LUT, clusters | Scene `064b746b`; SSS split targets `392d5d99` / `ed123b4f` (f03) | ClusteredDeferredShading2PassExecute 545CC4 → Impl 54762C; MaterialBindIndexedRampParams 3C3518 | ~210 + ~250 (local lights w/ cube shadows: 12 draws, 213) | 1.16 incl. WDO | 2.04 (PB2DS 0.33, PB2DUS 0.61-0.98, PB2MRLR 0.32) |
| 208-217 second view, 243-252 / 272 water view, 290-294 SSR + RefProbe | repeats of 134 / 150 / CDS2 for other views | | | same entries | water GI 23-53 | water 0.25 | GI Ocn rows 0.12 / 0.06 |
| 267 NatureSky = "DrawSky" | VCLOUD join; sky / cloud lighting uses sun + moon + sky-ambient table + transmittance LUT | | | NatureSkyNodeExecute 293220 (package 09) | 55 async | 1.60 wall | 0.45-1.52 |

## 3. Deferred lighting (ClusteredDeferredShading2Pass)

- **Variants in the graph** (each behind a `BeginCheckSkip` type that skips it when the class is absent):
  167 base (0xed86d02b); 172 / 175 accumulated snow (TranslucentSpecular + SnowScattering inputs); 177 / 178 refraction (RefrDepth);
  184-186 subsurface split (write separate diffuse `392d5d99` and specular `ed123b4f` RGBA16F for SSSS 188); 212 second view;
  217 decal-after-snow (AB = GBufferDecalAB `876b41c8`); 272 ocean; 346 water view. Each binds the same G-buffer; each is a
  stencil-masked full-screen draw (VS "WDO_SST", PS families PB2DS / PB2DUS / PB2MRLR).
- **Per-pixel light loop**: key light from the DirectOcclusion slice (no shadow-map taps for the sun here), sub parallel lights,
  then the cluster list: `tCllLightIndices / Positions / Attributes / ExAttributes` and spot twins `tCllSpt*` (layout in §5), with
  shadows from `TCubicShadowmapArray` (point, 44 comparison samples / 33 cube in the measured PS 772c50c1) and
  `TPlanarShadowmapArray` (spot), IES profiles (`IESLut`, `sIES` Texture2DArray, `nIESIndex`, `IESAttSlpScl`).
- **Shading features** (name lookups through the technique interface vf+F8h in Impl 54762C, so each is an optional permutation
  block): LightMap, VolumetricFog (fog applied inside the deferred pass), OuterTextureLight (projected / cookie lights),
  ThinSurfaceBSDF, PB2HairHighlight, SubPointLightShadows, SubSpotLightShadows, SnowScat, EmiMap, ShadowMask, OcclusionColor
  (`fOccCol`), dualLobe (dual-lobe specular, `dualLobeDTermScaler`), ReflectionProbeLighting (`probeGlobalDiffuseILScaler /
  SpecScaler`, MaterialBindReflectionProbeLightingParams 3C3F7C), Caustics, HatchingShadow, and the **IndexedRamp** toon ramp
  (`IRAFade, IRDFade, IRLocalLightParam, IRShadowFade, rampHHLtDir, rampSDOccFade, maskRotCS ...`, MaterialBindIndexedRampParams
  3C3518). Per-material ambient scale: MaterialBindPb2AmbientScaleParams 3C5914 (`vAmbDifScl / vAmbSpcScl`).
- Legacy path, not in the shipped main graph: TileLightCulling 3672030 (record 365B2D8) + TiledDeferredShadingPass 3678CB4 and
  ClusteredDeferredShadingPass (v1) 3677264. Only the small graph `0x7d27d0cb` (a 1-view preview graph) uses the tiled pair.

## 4. Shadows

### 4.1 Key light (sun or moon)
- Direction: SkySunMoonDirections 5EBD08 (sun from time of day, moon with axial tilt + phase). The key light can be the sun or
  the moon; up to 8 extra parallel "sub" lights (`IsNotDrawParallelSubLight` 8 bytes, `MaxParallelLightNum`, SubShadowMap
  4-slice arrays 512/1024/2048/4096, `SubShadowmapResolution` RC+BA8 = 2048 at HIGH).
- **VSM (static).** 16 clipmaps (`mW2S_VirtualSM[16]`, `vClipOriginOffset[16]`, `nValidClipmapFlag`), clipmap level per sample =
  `max(log2(max(2·dist / (2^nClipLevelMin · 0.5), 1)), nClipmapLevelBias)` (VFSCAT decompile), i.e. distance-from-camera
  clipmaps like UE5. Physical pool `a6861e5b` 4096²x16 is a reserved / tiled resource (UpdateTileMappings at frame begin). Page
  manager `pm = base+16C640` (struct rows `VsmPageMgr::*`): per-tile flags uploaded every frame as `tVirtualShadowMapFlags`
  (VsmFlagsUpload 912A2C) and tested by every consumer per tap; dirty + requested + uncached tiles are rendered, budget
  VsmTilesPerFrameBudget 1F7960 = 8 (STANDARD..HIGHEST) / 2 (LIGHT), lifespan VsmTileLifespan 77924, readback VsmReadbackApply
  466238 → VsmTileAgeAdvance 79074. **Only static list A + tree / grass renderers draw into it**; static geometry costs nothing per
  frame once cached. PC (ShadowmapType 6) runs the GPU-driven path 465414 (MPC reads `tValidRectCountCumulativeMap`, `tHiZ`);
  the Ally runs the CPU tile path 1F8340 (mode 5), where "Batch Tiles" redraws waiting tiles past the budget (S1, VSMA).
- **Cascades (dynamic).** Atlas `ea36c27d` 4096² (env: 4 rects of 2046²; 3 cascades seen in traces: "Cascade 0-2"), plus a
  `NoBias` alias. `ShadowCascadeStart 2000`, erase 25000-50000. Per cascade: ShadowSetDepthRange 86B8A4, CascadeMatrixGet 147F60
  (`S+(0x25+c)*0x40`), ShadowCascadeRender 1F5C2C walks list A (slots 2-5) twice and list B (11-14), with GPU meshlet culling
  (MIC / MPC) and a second occlusion phase after GenerateShadowMapHiZ. Per frame this is ~200 skinned / position-only character
  draws + 10 SMTREE2 indirect draws (shadows-culling §2.2). Optional baked ShadowTree path (BakedShadowMode 0 in all presets).
- **Resolve.** WDO#14 samples cascades, sub shadow maps and the VSM (with the tile-flag gate) at render resolution → mask;
  WDO#28 is a depth-aware 1-D vertical Gaussian of that mask (2 serialized round trips per tap; replacement built,
  invisible-gpu §3a). Output = DirectOcclusion (RS slice 3), consumed by the deferred passes and by water / second views.
- Settings: ApplyShadowQuality 18A87C (table 0x3978210; HIGH = HIGHEST maps), ShadowNodeApplyIfEnabled 13F2C4 →
  ShadowNodeParamsApply 188C88 (node defaults: key-light res 4096, budget 16 → quality row 8), ShadowRenderTargetsCreate 226E918,
  DisplaySettingsWatcher 1350210 (RT rebuild). Script: `Graphics::SetVirtualShadowUpdateTilesPerFrameBlackOut`.

### 4.2 Local lights
- LightPass node 29 (`LightGroupID 8`, `IsNotDrawParallelLight0 1`): LightPassExecute 7E919C walks the visible local lights;
  type 2 = point (6 faces, cube), type 3 = spot (one planar view); a per-light flag (light+BBh / +DBh bit 10h) picks a
  cached path, and a counter compare (`local_9ec < local_850`) looks like a per-frame update budget (inference).
  LocalLightShadowmapRender 7E92B4 (44.9 KB) records "PointLight" / "SpotLight" / "PointLightESM" / "SpotLightESM" with 8 rounds
  of MeshletInstanceCulling + MeshletPreculling + DrawListWalk; per-item filters LocalLightShadowDrawFilterA/B 367C990 / 367CE58.
- Targets: `TCubicShadowmapArray` 128² or 256² (env `ShadowmapCubicResolution 256`), `TPlanarShadowmapArray` 512² / 1024²
  (`ShadowmapPlanarResolution 1024`), each allocated in steps of 16 entries up to 256 (the graph lists every size; the engine
  binds one); ESM twins (`TCubicExponential*`, `TPlanarExponential*`) feed the fog; `DynamicPlanarShadowmap` 1024².
- Sub-light shadow throttle: node param 37B3FAE9 (default -1) compared with a draw count in ReflectionProbePassRecord 542C08.

### 4.3 ESM
- 5-entry array [256, 512, 1024, 2048, 4096] (`ExponentialShadowmapTextureObjectNameHash`, = the VFInjectionShadowMap RTs);
  index from EsmLevelFromRes 147324 (RS+16D8, ≥ 0x800 → 3 = 2048² at HIGH). Generated from the key-light atlas in node 149,
  blurred 7-tap H+V. **Consumers: froxel fog in-scattering and the (dormant) LPV only.** `[Patches] ESM1` (built, default off)
  forces 1024².

## 5. Light data on the GPU (from the decompiled VFSCAT #235 and RefProbe #121)

- **Clusters**: `$Globals nClstPtch[2]` (cluster pitch x/y), `vClstZPrm` (slice = `max(0, z·log2(x·viewZ + y) + w)`, log-z
  slicing), `vCllPkPrm[2]` (packing). Buffers: `tCllLightIndices` (uint), `tCllLightPositions` (float4), `tCllLightAttributes`
  (float3), `tCllLightExAttributes` (uint), `tCllSptLightAttributes` (float3), `tCllSptLightExAttributes` (uint). The fog shader
  dedups lights per group in LDS (`ldsAppliedLightIndices`, 16 per thread).
- **Parallel light** `slVFParallelLightInfo`: common {vMapDesc, vColorScale, vEcc}, {vDir, vDiffuse}, vAttr, 4 cascade
  world-to-shadow matrices `mW2S..mW2S4`, split distances `vSegs`, `mUVS`. Point `slVFPointBasicLightInfo` {common, vW2S};
  spot `slVFSpotBasicLightInfo` {common, vW2S[4]}; per-light scatter attributes `sPointScatterAttr / sSpotScatterAttr`.
- **LPV**: `mTrsW2LPV[4]` / `mTrsW2LPVSR[4]` (4 cascades), `nLPV3DDepth`, stored as `Texture2DArray sLPVBuffer`, `fSHBlendRegion`.
- **OBORO probes** (RefProbe): `gv_probefield_org`, `gv_probefield_pitch_size(_rv)`, `gv_tiledoct_pitch_octsize_rv`,
  `gv_probe_scale_offset`, `gv_lod_param` (3 levels), `oct_probe_info`, `oct_ref_table`, `sOctDepthSource` (+ radiance /
  normal / mask for #414), `slSHCoeff`, `sky_occ`. Stencil gate `gv_mask_param`; no `gv_time_param` use (no temporal interleave).

## 6. GI, probes, ambient, AO

- **OBORO** (static reflection probe field: `OboroStaticReflectionProbeField / Block / Entity / Manager`, `OboroHeightMap`,
  `OBOROStaticResourceBinaryFile`). Probe radiance is updated **event-driven** (script API `Graphics::OboroForceRadianceUpdate`,
  `ReflectionProbe::SetForceRadianceUpdate`, RSM capture "OBORO:Expand PrimitiveList for RSM Capture"), never per frame in the
  traces (probe-maintenance records #7 / #13 / #14 / #29 / #101 at ≤ 0.13 per frame). Per frame only the screen gathers run.
- **Gather scales** (GiRenderScalesApply 1872C4 every frame): Indirect 0.3/0.4/0.5/**0.6**/0.7/1.0, Reflection
  0.3/0.4/0.5/**0.5**/0.5/1.0, plus Ocn twins; GI HIGHEST = 1.0 = ×2.8 / ×4 pixels (PC 2.5 ms, Ally 15-20 ms E).
- **Empty waves**: each gather is dispatched over the whole grid once per stencil light group; light group 1 and the water-view
  dispatches are 95-99 % early-out waves (gi-highest-audit; GIM merge built, default off).
- **Rewrites built**: rp121 / rp414 latency rewrites (bit-exact), measured *slower* on the Ally under RADV for rp121
  (+0.9 ms); PC neutral. Not a lever on this hardware without a measured win.
- **LPV / RSM**: DeferredIndirectPass carries `LPVInjection 0x4b96639c` / `LPVPropagation 0x89998e3c` and RSM arrays, env
  `LPVUpdateFrequency 1`, but no LPV pass appears in any GP2 capture; changing the rate measured 0 (same-look-package LPVR).
- **Sky ambient**: already time-sliced (SKYR knob measured 0).
- **AO**: HBAO as above; GFM / GFM-D are generic separable Gaussians (not AO). HBAO merge is already done by the engine.

## 7. Volumetric lighting inputs (consumers in package 09)

- VFSCAT `641524ee` ("VFSCAT-CllLpvSfbVsm"): key light with 4 cascades (`sParallelShadowMap`, `mW2S*`, `vSegs`), VSM
  (`sVirtualShadow` + flags, clipmap selection above), cluster lights with `sSpotShadowMap` / `sPointShadowMapArray` (ESM twins),
  IES (`sIES`), LPV (`sLPVBuffer`), history `sVolumeSrc` (temporal blend weight `gInjectionDesc.y` = VolumetricFogTemporalFactor,
  0 in all presets). Grid 160x90x64 at HIGH (ApplyVolumetricQuality 39B080).
- VCLOUD (sky, async slot 3): sun + moon light march (4 steps each), sky-ambient table, transmittance LUT.

## 8. Data structures (offsets; struct rows in `# renderer/lighting`)

| struct | field | meaning |
|---|---|---|
| View | +3FF0 draw-list headers (32 B), +4328 shadow root (ViewGetShadowObj 533E88), +4460 VSM tile list (16 B tiles), +4730 VSM clipmap entries (0x150 B) | |
| ShadowObj S | +18 cascade count, vtbl+88/+90 begin/end cascade, +914 depth-range mode (0.5 / 1.0), +940 + c·40h cascade matrices | |
| VsmPageMgr pm (= base+16C640) | +40040 frame slot, +40044 forced mask, +400D8 / +400F8 clipmap counts, +400E4 / +400E8 tiles W / H, +400FC base level, +40100 first clipmap, +40108 GPU flag buffers, +40118 tile flags, +401C0 level records (140h), +41718 caster stamp, +41720 state, +41728 age | shadows-redundancy §3 |
| RenderConfig S | +144 SkylightAmbientUpdateFreq, +7C0 cloud reprojection weight, +B98..+BA8 shadow sizes, +12D8..+12F8 SSAO, +1394 LPVUpdateFrequency, +13C0 fog temporal factor, +13DC/+13E0 fog res | renderconfig_fields.tsv |
| render-graph nodes | LightPass (1204 B), DeferredIndirectPass (930 B), ClusteredDeferredShading2Pass (844 B), ClusterLightCulling (129 B), VolumetricFogScatter (909 B): field lists in the kidsrender dump | |

## 9. Keep / replace for a new renderer (same look), with estimated savings

Savings are against the measured costs above; "E" = estimate. Each item keeps the engine's inputs (same lights, same probes,
same shadow content) and changes only how the pass computes them.

| # | part | today | same-look reimplementation | look risk | r5 saved | pc2 saved | Ally saved (E) |
|---|---|---|---|---|---|---|---|
| L1 | sun-shadow resolve (WDO#14 + WDO#28) | resolve PS + separate 1-D blur PS with 2 dependent round trips per tap | one compute pass: resolve into LDS tile with apron, depth-aware separable blur in LDS (or the built WDO28 4-tap batch) | none (same weights / predicates) | 0.2-0.3 of 0.49 | 0.3-0.5 of ~0.8 | 0.4-0.6 of ~0.85 |
| L2 | GI gathers (RefProbe #121 / #414) | full-grid dispatch per stencil group, 95-99 % empty waves for group 1 and water; per-pixel probe search 3 levels x 8 corners with dependent loads | (a) stencil tile classification + one indirect dispatch per class; (b) resample the probe field once per frame into a camera clipmap volume (SH + visibility moments) so the per-pixel gather is a few filtered fetches | (a) none; (b) low: visibility test becomes per-voxel, slight leak differences | 0.25-0.4 of 0.71 | 0.3-0.5 of 0.80 | 1-2 of 2.4 (several ms at GI HIGHEST) |
| L3 | ESM 2048² + 7-tap H/V blur, only for fog / LPV | 296 µs r5 | 1024² ESM (ESM1) or sample fog sun shadow straight from the cascades / VSM per froxel (fog is low frequency) | low (slightly softer shafts) | 0.15-0.22 | 0.10-0.15 | 0.1-0.3 |
| L4 | key light with two shadow systems (VSM static + 3-cascade atlas dynamic) | ~200 character draws per frame into each cascade, separate Hi-Z / two phases, atlas clear | draw each dynamic caster once into all cascades (viewport-array / amplification instancing), or move dynamic casters into VSM pages with a per-page dynamic overlay; keep the page cache | none | 0.05-0.1 | 0.1-0.3 (of SM 0.62 + cascades) | 0.1-0.4 |
| L5 | VSM CPU tile path redraw (Ally only) | waiting tiles past the budget redrawn every visit | GPU-driven page path (mode 6) everywhere; budget applied to drawing, warm-up kept as request flags | none | 0 | 0 | 0.05-0.15 mean, 1-2.5 in bursts |
| L6 | deferred lighting: up to 12 stencil-classed full-screen passes re-reading the G-buffer | 9 executed per frame typical | one tiled compute pass with per-tile material classification (uber-shader per class), SSS split kept | none | 0.05-0.1 | 0.1-0.3 | 0.2-0.5 |
| L7 | HBAO chain (lin-depth, downsample, main, blur x2, temporal) | 5 small passes | one GTAO-style compute pass with LDS depth + temporal | low | ~0 | 0.03-0.06 | 0.15-0.25 |
| L8 | fog froxel lighting (VFSCAT, async) | every froxel every frame | real temporal amortisation (half the slices / checkerboard per frame + reprojection); the FOGT knob is only a blend weight | low-medium (lag on fast lights) | ~0.1 | 0.3-0.5 (wall) | 0.5-0.9 |
| L9 | shadow / scene Hi-Z builds | GHDM chains (7-14 dispatches) | single-pass downsampler (SPD) | none | 0.02-0.05 | 0.05-0.1 | 0.1 |
| keep | sky ambient (sliced), OBORO probe updates (event-driven), VSM page cache + valid-rect culling, clustered light lists, LPV (dormant: drop the code, keep the data path off) | already amortised | — | — | 0 | 0 | 0 |

**Totals (estimates, not additive with other packages):** r5 **≈0.8-1.1 ms of 10.7 (8-10 %)**, pc2 **≈1.3-2.3 ms of ~30 (4-8 %)**, Ally
**≈2.5-5 ms (E)** at its settings (more at GI HIGHEST, where L2 dominates). The safest large items are L1 and L2(a) (exact);
L2(b), L3 and L8 change the computation and need shot-diff validation. Caveats: Ally numbers are r5 x 6-8 or GP2 means; the RADV
compiler has already turned one "faster" GI shader into +0.9 ms, so every Ally figure must be measured on RADV before it counts.

## 10. Open items
- DXGI formats of the G-buffer (KTGL enum mapping not found; a PSO-desc capture or the FCAP1 frame capture would settle it).
- DeferredIndirectPassExecuteImpl 149AC4 sub-recorders (decompiled with `decomp_pool.cmd`): 3680D20 lazily creates the
  node+380h sub-object (gate 14B224), 3680DA4 updates it, 3682D2C / 3683A4C are twin per-target recorders called in a loop with
  3754150, 368117C binds the "Illustration" feature (`ilaScale`), 36823E0 / 3681DD0 run last. Which of them is DO resolve / ESM
  gen / ESM blur is still not tied to a shader hash; names in the symbol block are low confidence.
- **FCAP1 (pc2, 5 captures) cannot confirm the pass order yet**: each CSV holds only 160-360 records
  (ExecuteCommandLists, fence ops, tile mappings, resource descs); no command-list calls or markers were recorded, so the digest's
  Passes table is empty. The order in section 2 is the render graph's walk order plus the r5 RGP segment order; re-check it
  against an FCAP capture that records calls with `GPUProf_Markers = 1`.
- LightPass per-frame update budget and the light+BBh / +DBh bit 10h semantics (static-cache vs dynamic) need a decompile of
  7E92B4 (export timed out) or a marker capture.
- Which stencil class each CDS2 node draws (needs FCAP1 per-node draw counts).

## Capture-check (package 12, 2026-10-10; table in 12-capture-check.md sections 3 and 5)

- **Confirmed:** G-buffer = 7 colour RTs + D32S8 at render res (`OMSetRenderTargets` 7 RTVs, R32G8X24_TYPELESS depth); cascades (3, viewports 4096x2048 / 2048x2048 into a 4096x4096 depth) run before MIC / MPC and the G-buffer; VFSCAT dispatch is 20x12x16 groups = 160x90x64; VFSCAT < VCLOUD (DrawSky) < thin-surface temporal filter (449cf0ae, node 270) in replay order on all three devices.
- **Render res in these captures is 1280x720**, so VCLOUD is 20x12 = 240 groups.
- **New format facts:** cascade atlas depth is 4096x4096 R16_TYPELESS (D16); half-res depth 640x360 D32S8; top-down depth 1024x1024 R32 colour RT; the key-light ESM 2048^2 R32F was not touched in any capture window (unverifiable).
- **Unverifiable:** the nine ClusteredDeferredShading2Pass draws, node 134 / 149 / 150 passes and SSAO have no marker, and the RTV -> resource mapping is not captured; the GBufferEdit copy shader 7c5acaaf is first used before `GPUCulling Setup` (node placement vs 101-120 needs 05-frame-graph node ids).
