# GPU resolution / pass-scale audit (2026-10-09, static; game not run)

Target: pc2 = RX 6650 XT, 1080p output, REN_RAT 65 (render 1248x702), HIGH, FSR, ~37 fps (27 ms, GPU-bound).
Inputs: gpu-profile (r5 RGP, RX 9070, 1440p out / 1080p render = 10.64 ms), ally-gpu-budget (GP2), gpu-maxsettings, gpu-copies-postres,
gpu-post-exe/runtime, invisible-gpu, settings-cost, `data/dumps` (env/render-graph dumps), `re/Nioh3.dump.exe` + `D:\nioh3-re\decomp`.
The pc2 RDNA2 GP2 table (bridge #467, file #125, 23 gameplay windows, 23.2 ms stamped span) arrived late; section 5 uses it. Section 1 ms
columns are **E** = r5 us x pixel ratio x ~3 (9070 -> 6650 XT) and should be replaced by the pc2 table where a row exists.
Pixel ratios vs r5: render-res passes x0.42, output-res passes x0.56, fixed-size passes x1.

## 0. Headline
- **At 65 % render scale most of the frame does not scale with REN_RAT.** r5 split (9070): render-res pixel work ~4.0 ms, geometry ~0.95,
  fixed-size ~1.44 (ESM, shadow maps, GHDM, fog, culling, particles), output-res post ~0.4-0.5, barrier tails / tiny segments / idle ~2.3.
  After scaling to 702p the fixed + geometry + tail block is ~5.3 of ~6.8 ms. pc2's top rows agree (section 5): copies, VCLOUD, VFSCAT,
  Meshlet LOD/culling, shadow-map draws are all resolution-independent.
- **No pass runs at output res that could trivially run at render res with an identical picture.** The engine already has the switch
  (POSTEFFECT_RESOLUTION), and SSAO/SSR/GI/fog have their own sub-resolution knobs (section 2).
- **Built (default OFF): `[Patches] ESM1`**, a 3-byte patch that renders the directional ESM at 1024^2 instead of 2048^2 (3.1).

## 1. Pass -> resolution -> GPU ms -> can it go lower
r5 us = RX 9070. pc2 ms = E unless section 5 gives a measured value. "Fixed" = independent of REN_RAT.

| Pass (shader group) | Runs at | r5 us | pc2 ms | Lower / earlier with same picture? |
|---|---|---|---|---|
| G-buffer geometry (PB2*, trees, terrain) | render 1248x702 | ~1630 | ~2.0 E | no (vertex-bound part is fixed); M1 scalarization is the known idea |
| Depth min/max GHDM | fixed | 82 (was 504) | 0.14 M | done |
| Shadow-map draws SM-/DEPTH-/VSM | fixed (VSM 2048, 8 tiles/frame) | 232 | ~1.1 M (rows 6, 25, 47, 58, 61, 71) | tile budget is quality-table driven; lowering = pop-in |
| WDO#14 / WDO#28 sun-shadow resolve + mask blur | render | 120 + 367 | ~0.5 M | wdo28 latency rewrite built, pc2 A/B pending |
| **ESM blur V+H** | **fixed 2048^2 (162,901 waves)** | **239 + 57** | 0.18 M (row 29) | 1024^2, feeds only fog + LPV -> ESM1 (3.1); measured share is smaller than my x3 guess |
| RefProbe GI gathers + HBAO/Hiz helpers | render x GI scale (0.6 / 0.5 at HIGH) | 707 | 0.58 + 0.15 + 0.12 M | not rate-limitable (section 5) |
| SSR (PBRLR) | render, full-size at HIGH | 75 + 117 | 0.11 M | SSR STANDARD = half-size (menu) |
| HBAO | render 702 < 721: 7 rays + half res already | 0.4 us masked | ~0.12 M | minimal at this scale |
| VFSCAT fog froxels (async) | fixed grid 160x90x64 at HIGH, not tied to REN_RAT | 293 | 1.04 M, async | VOLUMETRIC STANDARD = -58 % froxels (menu); shader belongs to the rewrite agent |
| VCLOUD (async) | fixed | 55 | 1.60 M, async | owned by the shader agent |
| Transparents / G1E particles | render | 280-394 | ~0.4 M | VISUAL_EFFECT LIMITED only |
| FSR3 ACCUMULATE_SHARPEN + 7 helper passes | render -> output | FSR4 810 | 0.81 + ~0.6 M | all feed the image (fsr3-audit); FSR3 precision lever already native on RDNA2 |
| RCAS | output | 35 | n/a (fused in FSR3 accumulate_sharpen) | FSR_SHARPNESS 0, visible |
| CustomDepthUpscale / 169 us exe copy / bloom chain / compose / histogram | output at HIGH, render at STANDARD | ~535 + 66 | ~0.8 M (rows 24, 27, 44, 64, 67) | POSTEFFECT_RESOLUTION=STANDARD (3.2) |
| Motion blur | output | -0.35 ms on/off | 0.28 M (rows 35, 41) | MOTION_BLUR STANDARD (menu) |
| UI / AMBSL / ExeUI | output | ~150 | ~0.6 M | none |
| Culling MPC / MIC / MshLod / MCLRC | fixed | 336 | ~3.5 M | shader agent |
| (copy / resolve / clear) | mixed | see 5 | 1.73 M | section 5 |

## 2. Hidden knobs (game-side only)
- **Env render-config params** (`data/dumps/0x396b9210.kidsobjdb.txt`, identical in all 73 presets; game archive data, not exe or config.xml):
  `VolumetricFogResolutionX/Z` 160/64 (VOLUMETRIC overrides via S+13DC/13E0), `VolumetricFogCustomSMResolution` 1024, `ShadowmapPlanarResolution`
  1024, `ShadowmapCubicResolution` 256, `ReflectiveShadowmapSize` 256, `VolumetricCloudResolution` 1, `TerrainDeformBufferResolution` 1024,
  `UltraMarineGlobalHeightMapSize` 1280, `SSAOResolution` 0, `RLRResolution` 0, `ThinSurfaceBSDFResolution` -1, `SuperSamplingEnable` False.
  Already at or below what HIGH visibly needs; no proven delivery route for archive edits; not pursued. The ESM size is not an env key (3.1).
- **config.xml**: hidden keys (REN_REZ, NVIDIA_REFLEX, INTEL_XELL, CPU_SPEC_CLASS) cost no GPU; MIPDOWNLEVEL_* / DEBUG_* strings have no references.
- **AO threshold**: HIGHEST uses 14 rays only at render height >= 721; at 702p it is already 7 rays + half-res.
- **Shadow table** (0x3978210, 64 B rows): STANDARD/HIGH/HIGHEST have identical RT hashes, 2048 sub-shadowmap, 8 VSM tiles; HIGH vs STANDARD differ only
  in key-light param 8 vs 10 and flag bytes. Cheaper shadow rows (LIGHT: 1024 maps, 2 tiles) pop in.
- **Menu equivalents with GPU cost**: POSTEFFECT_RESOLUTION, VOLUMETRIC, SSR, MOTION_BLUR (STANDARD each); all change the picture somewhat.

## 3. Top candidates

### 3.1 ESM 2048^2 -> 1024^2 (BUILT: `[Patches] ESM1 = 1`, default 0, next launch)
- **Site.** `EsmLevelFromRes` RVA 0x147324 (leaf, callers 149DE9 DeferredIndirectPass, 147407, 1948DB7 VolumetricFogScatter). Reads RS+16D8 (copied from
  RS+27B8 at 57E73D), returns the index into the 5-entry `ExponentialShadowmapTextureObjectNameHash` array [256, 512, 1024, 2048, 4096]:
  >=0x800 -> 3, >=0x400 -> 2, >=0x200 -> 1, >0 -> 0, else -1. (invisible-gpu's "index = quality-1" was wrong: the size is what is compared.)
- **Patch.** RVA 0x14733E `0F 9D C0` (setge al) -> `31 C0 90` (xor eax,eax; nop): index 2 (RT 0xad650905, 1024^2) for any size >= 1024.
  Unique pattern; `tests/test_patterns.py` ALL 231 SITES OK; `release/check_release.py` PASS; ASI builds. Group `ESM1`, ini `[Patches] ESM1`, no menu item.
- **Look / risk.** ESM feeds volumetric-fog in-scattering and LPV indirect shadowing (low frequency): slightly softer fog shafts. Unproven: the ESM
  generation pass might take a texel-size constant from RS+16D8; if so the blur would be mis-scaled. The A/B must shotdiff a fog-heavy spot.
- **Expected.** r5: ESM V 239 + H 57 us, ~4x fewer texels -> **-0.22 ms on the 9070**. pc2 measures the whole ESM row at only 0.18 ms/frame
  (3.2 draws), so the **RDNA2 gain is ~0.10-0.14 ms (<0.6 %)**, not the 0.6 ms my first x3 estimate gave. Small; ship only if the shotdiff is clean.

### 3.2 POSTEFFECT_RESOLUTION = STANDARD (config only)
- Measured -0.39 ms on the 9070 (sweep, 1440p out). pc2 post rows (24, 27, 44, 64, 67, 35, 41) sum ~0.8 ms at 1080p out; STANDARD works on 0.42 of
  the pixels: **E -0.3 to -0.45 ms (~1.5 %)**, plus the 169 us 3-layer copy if it drops out.
- Risk: post targets at render size = a second softening after the upscaler. Hybrid (bloom/histogram at render size, compose at output) needs the
  PostEffect3 per-target size-mode table (array lVar23 + idx*0x24, mode bytes +8/+9; flag read at 0x6DBB6D), not located. Not built.

### Not worth it
HBAO merge (engine already merges), CustomDepthUpscale -> copy (<= 0.06 ms, risky), env map-size edits (no delivery route).

## 4. pc2 A/B (hands-free vpad)
`pc2_abba2_pc.ps1 -Input vpad -BaseIniFile <ini>`: A = base, B = base + `[Patches] ESM1 = 1` (rebuild or copy the ASI; bin is gitignored). Metric: PresentMon
GPUBusy, shotdiff at a fog spot, GP2 row `47ef1c9b` (ESM) should fall ~4x. Second pair: config POSTEFFECT_RESOLUTION STANDARD vs HIGH.

## 5. pc2 RDNA2 GP2 follow-up (file #125, 1080p HIGH, 23.2 ms span)
- **(copy / resolve / clear) = 1.73 ms, 27.3 runs/frame (rank 1).** Not directly attributable: GP2 closes an interval only after `GPUProf2_MinRun` (4)
  draws and bills up to 3 preceding draws to the copy bucket (ally-gpu-budget 1, `gpuprof.cpp` 369-379), so 27 runs x up to 3 draws means much of the
  1.73 ms is ordinary small draws, not copies. Named copy shaders are already separate rows: ExeUI_AlphaFadeQuad 0.127, ExeUI_SmoothMaskDiscard 0.065,
  ExeUI_TintedQuad 0.100, VideoOut copy 0.068 (output res, 0.3 %). The known engine copy sources are CopyColorRenderTarget (8493B4), ClearScene (8236D8),
  the 4-target GBufferEdit copy (SSR reads GBufferEditNS/RS), FSR3 SaveOpaqueColor (0.057 Ally) and the depth copy; gpu-copies-postres already found no
  dead pass with FG OFF. **Verdict: nothing proven redundant; removal needs attribution first.** Do not guess-patch.
  Next step (pc2, one launch): `[Profiler] GPUProf=2 GPUProf2_MinRun=1` so each copy/clear/resolve is its own row with its resource names; then judge per row
  (output-res copies shrink only via POSTEFFECT_RESOLUTION STANDARD).
- **FSR3 ACCUMULATE_SHARPEN 0.81 ms** (output-res compute): every FSR pass feeds the image, no flag removes a pass, no double sharpening (fsr3-audit).
  Only lever is FSR_SHARPNESS 0 (sharpen fused in this CS; the saving is small and visible) or a lower REN_RAT; min16float is native fp16 on RDNA2.
- **RefProbe cs 0.58 ms (10.7 dispatches)**: cannot be amortized or rate-limited invisibly. The gathers are per-pixel screen-space passes for the current
  camera (G-buffer in, uOutTex1 out, no time parameter, no temporal interleave; invisible-gpu 1). Skipping a frame would show stale GI on motion.
  The only exact reduction is merging the empty light-group-1 dispatches (GIM, A1 in gi-highest-audit; 0.04-0.08 ms at HIGH) or lowering the GI level
  (scales 0.6/0.5 -> 0.5/0.5 at STANDARD, picture change).
- Meshlet LOD/culling (MshLod 0.81+0.45+0.20, MIC 0.53, MPC 1.0) and volumetrics (VFSCAT 1.04, VCLOUD 1.60) are the shader-rewrite agent's.
