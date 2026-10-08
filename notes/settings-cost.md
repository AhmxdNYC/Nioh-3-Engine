# Settings cost table: predicted cost per graphics setting (for the final bang-for-buck search)

Analysis only (2026-10-08). Game not run. Input: `final-benchmark-protocol.md` (User additions item 5), `work-deciders.md`, `render-nodes.md`,
`gpu-maxsettings.md`, `settings.md`, `gpu-copies-postres.md`, `shadows-redundancy.md`, `captures/ally/gp2_L{1,2}_full.csv`,
`captures/sweep/report.csv`. Nothing here is a new measurement.

Legend: **M** = measured (source named), **E** = estimate (model/scaling), **measure** = unknown, the final run must find it.

## 0. Read this first: how far the existing numbers can be trusted

- **Ally GP2 (`gp2_L1/L2_full`, 175 logs, 34 windows):** per-PSO ms per frame. GP2 stamps inflate the frame (span 30.3 ms vs 16.7 real), so
  use the numbers **relatively**. The graphics config of the 175 runs is not recorded in the log (it printed only FG OFF + FSR; rendered 59.9 fps).
  Treat those costs as "the user's Ally preset, scene-dependent", not as MAX. The only MAX Ally data is v71 (16.2 fps rendered, CPU-bound logging, no GP2).
- **No GP2 capture exists per setting level on the Ally.** Every per-level Ally number below is E (PC r5 RGP x 6-8, or pixel/count ratios), except where marked M.
- **PC settings sweep (`captures/sweep/report.csv`, single run per setting, shrine idle, 1440p out / 1080p render, baseline = HIGH):**
  baselines drifted 109.4 / 112.7 / 110.1 fps and GPU 8.66-8.78 ms. Almost every "cheaper" setting shows *lower* fps (-1 to -7 %), which is
  drift/reload noise, not an effect. **Only two GPU deltas exceed noise: MOTION_BLUR STANDARD->OFF -0.35 ms and POSTEFFECT_RESOLUTION HIGH->STANDARD -0.39 ms**
  (MODEL_QUAL -0.39 is also in the data but the decompile proves MODEL_QUAL has no consumer, so that one is noise). The sweep SSIM column (0.40 everywhere) is unusable. The sweep cannot rank CPU effects.
- **PC frame at HIGH:** CPU 8.7-9.3 ms, GPU 8.7 ms, 109 fps (M). It is CPU-bound by a hair and **GPU-balanced**: any GPU setting that adds
  more than ~0.3 ms makes the PC GPU-bound. GI HIGHEST (+1.8 ms E) would do exactly that.
- **Ally GPU model at MAX native 1080p: ~55-80 ms (E, `gpu-maxsettings.md` §2)**; a 60 fps target at MAX is not reachable by settings-neutral work. The search is
  therefore "which levels to drop first", dominated by REN_RAT, GI and fixed-size passes.

## 1. Measured pass costs used below

| Pass family | Ally GP2 ms/frame (L2 all-off / L1 all-on, mean of 34 windows) | PC RGP us (r5, HIGH, 1080p render) | Setting that drives it |
|---|---|---|---|
| FSR3 upscaler (accumulate 3.66-3.69, prepare-inputs 0.77, reactivity 0.89, luma-instab 0.71, pyramid 0.34+0.18, rcas 0.19, gen-reactive 0.16) | **6.9-7.0** (M, ~33 % of the table; user's native-AA config?) | not in r5 (FSR4/XeSS) | REN_RAT, RESOLUTION_SCALING |
| RefProbe GI gathers (#121 diffuse, #414 reflection, + extras) | **2.4-2.5** window mean (M); last 6 windows **#121 3.1 + #414 3.0 = 6.1** (M, `ally-replacement-ab.md`) | 338 + 317 + 53 water = **707** (HIGH) | GI |
| VFSCAT froxel fog (async) | **1.07-1.32** (M) | 293 (async) | VOLUMETRIC |
| VCLOUD (async) | 0.45-0.47 (M) | 55 | none (CloudRes 2 at every level) |
| SSFOG | 0.15-0.16 (M) | - | VOLUMETRIC (partly) |
| VSM + shadow maps (SM-*, Batch Tiles 8, cascades) | **1.8-2.0**, bursty 0.15-9.3 (M, `shadows-redundancy.md`); SMTREE2 0.24 | SM draws 232 | SHADOW_QUAL |
| WDO (shadow-mask blur, 3 PSOs) | 0.65-0.70 + 0.17 (M) | 367 us (at 1080p) | SHADOW_QUAL (indirect) |
| HBAO + GFM/GFM-D blur | HBAO 0.51-0.54, GFM 0.19, GFM-D 0.13 (M) | HBAO draws 0.4 us each | AMBIENT_OCCLUSION |
| SSR: PBRLR + PB2MRLR merge | 0.28 + 0.32-0.36 = **~0.6** (M) | 75 + 117 | SSR |
| Terrain PB2LS (tessellated) | 0.26-0.29 (M) | 155 | LANDSCAPE_QUALITY |
| G1E particles | 0.19-0.23 (M) | 394 (shrine) | VISUAL_EFFECT |
| Grass / trees (PB2TREE2, TREE2WIND) | 0.37 + 0.17 + 0.14 (M, not all grass) | - | GRASS_DENSITY (share unknown) |
| Water / wet (UltramarineRender, UMAH, Caustics) | ~0.3-1.1 per window (M) | - | scene, GI |
| Post (tonemap/compose, bloom chain, AMBSL UI) | tonemap 0.11, AMBSL 0.37 (M) | output-res post ~535 | POSTEFFECT_RESOLUTION, bloom/DOF/MB |
| Motion blur | **not isolated** (the "motion blur" label in the gp2 CSV is a mislabel for the FFX shaders) | MB OFF -0.35 ms GPU (sweep M, noisy) | MOTION_BLUR |

## 2. Per setting: levels, what changes, cost, visual change

Ranking key used in section 3. "Ally ms" are E unless marked M. "CPU?" = moves main-thread/worker cost.

| Setting (config.xml key) | Levels | Passes / systems changed per level (source) | Cost evidence | Visual change per level (plain words) | CPU? |
|---|---|---|---|---|---|
| **REN_RAT** (+ RESOLUTION_SCALING, sharpness) | 25..100 step 5 | Internal resolution per axis; FSR3 prepare/accumulate/rcas; all render-res passes scale with pixels (`settings.md` 8) | PC: 7.0 ms at 65 % ~ 16.6 ms at 100 % (E, 1440p, `settings.md`). Ally FSR3 chain alone 6.9 ms (M) | Softer image below 100 %; FSR hides most of it at 70-85 % | no |
| **GI** | LIGHTEST, LIGHT, STANDARD, HIGH, VERY_HIGH, HIGHEST (no OFF) | Probe-GI gather render scales (Indirect / Reflection / IndirectOcn / ReflectionOcn): 0.3/0.4/0.5/0.6/0.7/1.0, 0.3/0.4/0.5/0.5/0.5/1.0, 0.2/0.25/0.3/0.35/0.4/1.0, 0.3/0.4/0.5/0.6/0.7/1.0. LIGHTEST also skips some work (`gpu-maxsettings.md` 1) | PC E: STANDARD 589 us, HIGH 707, VERY_HIGH 847, **HIGHEST 2511**. Ally E: 3.5-4.7 / 4.2-5.7 / 5.1-6.8 / **15-20 ms**. Ally M window mean 2.4 (preset unknown) | LIGHTEST-STANDARD: blotchier / lower-res bounce light and reflections; HIGH->VERY_HIGH: tiny; **VERY_HIGH->HIGHEST: nearly invisible (low-frequency light, denoised) but 3x cost** | no |
| **SHADOW_QUAL** | OFF (rejected -> acts as LIGHTEST), LIGHTEST, LIGHT, STANDARD, HIGH, HIGHEST | Row table: key-light param 18/9/10/8/8; cube/planar maps 512/512/1024/1024/1024; VSM param 1024/1024/2048/2048/2048; **VSM tiles per frame 2/2/8/8/8** (default 16); sub-shadowmap 512/1024/2048/2048/2048; character far-shadow cull 20/20/20/30/70 m (HIGHEST adds flag 0x7598) (`work-deciders.md` 1) | Ally M: VSM 1.8-2.0 ms at tile budget 8 (STANDARD); bursts 5-9 ms when the camera turns. PC: sweep HIGH->STANDARD within noise. HIGHEST vs HIGH: **measure** (E 0.0-0.3) | LIGHTEST/LIGHT: low-res and slow-to-fill shadows (pop-in on camera turns, blurry edges); STANDARD->HIGH: key-light and RT hashes only, small; HIGH->HIGHEST: character shadows reach 70 m instead of 30 m | small (caster culling, tile marking) |
| **VOLUMETRIC** | LIGHTEST, LIGHT, STANDARD, HIGH | Froxel grid X x Z (cap): LIGHTEST fog off; LIGHT 80x45x32; STANDARD 120x67x48; HIGH 160x90x64 (`work-deciders.md` A1). VCLOUD unchanged | Froxel count 115k / 386k / 922k. Ally VFSCAT 1.07-1.32 ms (M, at STANDARD, async). E: HIGH ~2.4x STANDARD, LIGHT ~0.3x (async hides part) | LIGHT: coarser, chunkier god-rays and fog banding; LIGHTEST: fog and light shafts vanish (big look change); HIGH->STANDARD: finer fog detail only | no |
| **AMBIENT_OCCLUSION** | OFF, LIGHTEST..HIGHEST | HBAO rays 0/2/3/5/7/14; Ally 540p HIGHEST = 7 rays + res 1. One full-screen draw/frame at every level (`render-nodes.md`) | Ally M: HBAO 0.51-0.54 + GFM 0.32 total. Level steps: E <0.1 ms (draws masked, 0.4 us on PC). OFF saves up to ~0.8 ms | OFF: flat, floating contact shadows; LIGHTEST-STANDARD: grainier AO; HIGH->HIGHEST: invisible | no |
| **SSR** | OFF, LIGHTEST..HIGHEST | Ray steps x0/0.5/0.75/1/1.5/2; half-size resource at LIGHTEST..STANDARD | Ally M: PBRLR + merge ~0.6 ms total; OFF saves up to that; HIGH->HIGHEST E 0.2-0.3 | OFF: no wet-floor/glossy reflections; lower levels: shorter, noisier reflections; HIGH->HIGHEST: slightly longer rays | no |
| **MOTION_BLUR** | OFF, LIGHTEST..HIGHEST | Post sample count 2/4/6/8/10 at output resolution | PC sweep M: STANDARD->OFF **-0.35 ms** (noisy). Ally E: 1-2 ms (output-res post, x0.56 pixels x6-8 throughput) | Fewer samples = grainy blur streaks; OFF = crisp motion. Taste setting | no |
| **POSTEFFECT_RESOLUTION** | STANDARD, HIGH | Whole post chain (bloom, exposure, compose, ~8 targets) at render size vs output size (`gpu-copies-postres.md`) | PC sweep M: HIGH->STANDARD **-0.39 ms** (1440p out / 1080p render). No effect at native res (output = render). Ally: only when REN_RAT < 100; E 0.3-0.8 | STANDARD: post is done at render size then re-upscaled, so final image can look softer (not proven: SSIM column broken) | no |
| **LANDSCAPE_QUALITY** | LIGHTEST, LIGHT, NORMAL, HIGH, ULTRA | Terrain tess. max distance 1000/1250/1500/2000/4000 and adapt rate 0.3/0.35/0.4/0.5/0.8; background-mesh LOD distance | Ally M: PB2LS 0.26-0.29 ms (NORMAL-ish). ULTRA vs HIGH: **measure** | Lower: terrain pops/loses curvature at distance; ULTRA: smoother distant ground | yes ("particularly CPU", Nioh3Fix; not isolated) |
| **VISUAL_EFFECT** | LIMITED, NORMAL | LIMITED refuses "optional" effects (`EffectCreate`); EffectUpdateDisp/NoDisp worker pools | Ally M: G1E 0.19-0.23 ms. PC: sweep CPU change within noise | LIMITED: fewer sparks/dust/ambient particles | yes (effect pools) |
| **GRASS_DENSITY** | LOWEST, LOW, NORMAL, HIGH | min(level,4) -> placement unit +0xFC; triggers a placement rebuild (consumer of +0xFC not found) | Ally M: trees+wind 0.67 ms total, grass share unknown. **measure** | Sparser ground cover | yes (placement/cull) |
| **MODEL_DRAW** | FEW, NORMAL, MANY | Drawn-character cap 30 / 45 / 60 (`CharacterDrawBudgetCull`) | Only bites with >30 characters; PC sweep within noise. **measure in a crowd** | Distant enemies disappear earlier | yes |
| **MOTION_QUALITY** | LIGHT, STANDARD, HIGH | Character LOD tier count 8/6/4 (`GetLodLevelCount`); one analysis calls the effect dead | PC sweep within noise; **measure in a crowd** | Far enemies animate at lower rate | yes |
| **MESHLET_QUALITY** | LIGHT, STANDARD, HIGH | 8 category LOD scales x table entry; live table (`live_reads_2026-10-08.md`): **STANDARD and HIGH multipliers are both 1.0, LIGHT 1.5** | Ally/PC: **measure**; STANDARD==HIGH is a free equivalence (M, PC read) | LIGHT: coarser geometry LOD up close; STANDARD vs HIGH: identical by table | streaming IO |
| **MODEL_QUAL** | LIGHT, STANDARD, HIGH | **No consumer in v2.02** (`work-deciders.md` A5) | zero | none | no |
| **SSS** | OFF, ON | Skin subsurface scatter in lighting | **measure** (E <0.3) | OFF: waxy/flat skin | no |
| **COLOR_PRECISION** | MIDDLE, HIGH | Wider HDR targets (S+0x2148) | PC sweep: 0. Ally (bandwidth-bound): **measure**, E 0.2-0.8 | HIGH: less banding in dark gradients | no |
| **ANISOTROPIC_FILTER** | OFF, 2, 4, 8, 16 | Sampler anisotropy in G-buffer | PC sweep: 16->8 within noise. Ally: **measure** (E <0.3) | Floor/wall textures blur sooner at grazing angles at 2-4x | no |
| **DEPTH_OF_FIELD, BLOOM, LENS_FLARE, CHROMATIC_ABERRATION, NOISE_FILTER, VIGNETTING, MACH_BAND_NOISE** | OFF, ON | Post chain (`fn_39B32C`) | PC sweep: DOF/BLOOM off within noise. Ally: **measure** as one group | Taste: DOF blur, glow, flare, fringing | no |
| **WIND_SENSITIVITY** | OFF, LOW, NORMAL | Cloth/foliage wind simulation | PC sweep within noise. **measure CPU** | Less cloth and foliage sway | yes (likely) |
| **MODEL_TEX_QUAL** | LIGHTEST..HIGHEST | Texture mip budget | VRAM only; Ally X shares system RAM: **measure** load time and hitches | Texture sharpness | no |
| **SOUND_QUALITY** | LIGHT, STANDARD | Audio synthesis threads | CPU-, **measure** | Audio detail | small |

Not graphics-cost settings (leave alone): DISP_MODE, VSYNC, HDR*, REN_FPS, DYNAMIC_*, FRAME_GENERATION (separate list, never counts), EVENT_*.

## 3. Rank: ms saved per visible change

"Visible change" is graded 0 (invisible), 1 (A/B only), 2 (noticeable), 3 (obvious). Rank = biggest ms saved at the least grade. All ms are E unless M.

### ROG Ally X (1080p, GPU-bound; CPU settings are fps-neutral there)

| Rank | Step | Ms saved (Ally) | Visible grade | Note |
|---|---|---|---|---|
| 1 | GI HIGHEST -> HIGH | **10-14** (E) | 0-1 | Only if the user's Ally preset is HIGHEST. The single biggest lever; skip if already <= HIGH |
| 2 | REN_RAT down (100 -> 85 -> 75 -> 65) | pixel-proportional; FSR3 chain 6.9 ms is partly output-res (M total) | 1-2 | Biggest overall; sweep 3 values, FSR hides much |
| 3 | MOTION_BLUR HIGH/STANDARD -> OFF or LIGHTEST | 1-2 (E; PC M 0.35) | 1 (taste) | Cheap look change, matches measured sweep effect |
| 4 | POSTEFFECT_RESOLUTION -> STANDARD (only if REN_RAT < 100) | 0.3-0.8 (E; PC M 0.39) | 1-2 | May soften the final image: check in a still |
| 5 | VOLUMETRIC STANDARD -> LIGHT | ~0.7-1.0 of 1.1-1.3 async (E) | 2 | Chunkier fog; HIGH->STANDARD is 0.5-1.7 saved at grade 1 |
| 6 | GI STANDARD -> LIGHT/LIGHTEST | 1-2 (E) | 2 | Look gets noticeably flatter |
| 7 | SHADOW_QUAL STANDARD -> LIGHT | 0.5-1.5, bursts up to 3-5 (E; M: VSM 1.8-2.0) | 2 | Mostly kills the camera-turn spikes: VSM tile budget 8 -> 2 trades hitch for pop-in |
| 8 | SSR / AO OFF | ~0.6 / ~0.8 (M totals) | 3 | Not worth it: high visible cost per ms. Levels below HIGHEST cost <0.1 each |
| 9 | VISUAL_EFFECT LIMITED, LANDSCAPE one lower, GRASS one lower | 0.1-0.3 each (M pass sizes) | 1-2 | Small on the GPU; measure only if the Ally turns out CPU-limited at the final preset |
| 10 | MESHLET HIGH -> STANDARD | 0 (live table identical) | 0 | Free if it is truly a no-op: still confirm with fps |
| - | MODEL_QUAL, aniso, SSS, COLOR_PRECISION, post toggles | ~0-0.3 each | 0-1 | Group-test; leave at MAX unless the group moves fps |

### PC (RX 9070, 1440p, CPU main-thread bound, GPU balanced)

GPU-side steps buy nothing until the CPU time is lowered. Order is "what moves CPU, then what keeps GPU below CPU".

| Rank | Step | Effect | Visible grade | Note |
|---|---|---|---|---|
| 1 | GI HIGHEST -> HIGH (only if user runs HIGHEST) | GPU -1.8 ms (E): the difference between GPU 10.5 ms (GPU-bound, ~95 fps) and 8.7 ms | 0-1 | Needed to stay CPU-bound at MAX otherwise |
| 2 | MODEL_DRAW MANY -> NORMAL/FEW | CPU, only in crowds; **measure** | 1-2 | Sweep shrine test cannot see it |
| 3 | VISUAL_EFFECT LIMITED | CPU effect pools, G1E; **measure** in combat | 1-2 | |
| 4 | WIND_SENSITIVITY LOW, GRASS one lower, LANDSCAPE one lower | CPU, **measure** | 1 | The "Both" group in `settings.md` |
| 5 | MOTION_QUALITY LIGHT | CPU, **measure** in a crowd; may be dead | 1 | |
| 6 | SHADOW_QUAL one lower | CPU (caster cull) + GPU 0.1-0.3; **measure** | 1-2 | |
| 7 | MOTION_BLUR OFF, POSTEFFECT_RESOLUTION STANDARD | GPU only: -0.35 / -0.39 ms (M); no CPU benefit | 1 | Only helps if the PC becomes GPU-bound at the chosen preset |
| - | Everything GPU-only at HIGH (SSR, AO, VOLUMETRIC, MESHLET, ANISO, SSS) | ~0 fps (CPU-bound) | - | Keep at MAX |

Settings that move CPU cost: MODEL_DRAW, MOTION_QUALITY (disputed), WIND_SENSITIVITY, VISUAL_EFFECT, GRASS_DENSITY, LANDSCAPE_QUALITY, SHADOW_QUAL (small), MESHLET (streaming), SOUND_QUALITY. None of them has a clean number: the shrine sweep is not a combat/crowd scene.

## 4. Candidate presets to verify (2-3 per device)

All start from the device's "recommended mod config" (ALL-KEPT). FG off. Each preset is compared with MAX and the user's preset.

### ROG Ally X (1080p, goal: highest stable fps with the least visible loss)
- **A "MAX minus the invisible":** GI HIGH (if MAX), SSR HIGH, MOTION_BLUR HIGH, SHADOW HIGH, LANDSCAPE HIGH, MESHLET STANDARD; everything else MAX. REN_RAT 100 with the upscaler on. Expected: fps gain is almost all from GI; E-checks the "free lunch" claim.
- **B "Balanced":** A + REN_RAT at the value from the sweep that crosses the target, GI HIGH, VOLUMETRIC STANDARD, MOTION_BLUR OFF/LIGHTEST, POSTEFFECT STANDARD (if REN_RAT < 100), AO/SSR STANDARD or HIGH.
- **C "Floor":** B + GI STANDARD, VOLUMETRIC LIGHT, SHADOW LIGHT (watch pop-in), VISUAL_EFFECT LIMITED, GRASS/LANDSCAPE one lower. Shows how much fps the look cost buys.

### PC (7600X + RX 9070, 1440p, goal: keep CPU the only limiter at the highest look)
- **A "MAX look, GPU-safe":** all MAX except GI HIGH, MOTION_BLUR and POSTEFFECT unchanged. Expected: CPU-bound, same fps as full MAX if GI HIGHEST would have gone GPU-bound.
- **B "CPU-lean":** A + MODEL_DRAW NORMAL, VISUAL_EFFECT LIMITED, WIND LOW, GRASS/LANDSCAPE one lower.
- **C (optional) "CPU-floor":** B + MOTION_QUALITY LIGHT, MODEL_DRAW FEW, SHADOW one lower. Only keep if B showed a CPU move.

## 5. Measurement plan (fewest combinations)

The protocol's item 5 asks per-level fps for every setting; this plan cuts it to one-factor-at-a-time off an anchor and skips settings with no consumer or no expected effect. FG off, same save, same STAND spot + one COMBAT window per candidate.

**Stage 0: anchor.** MAX on each device (3 runs; STOCK vs ALL-KEPT is already in the protocol). Record frame ms, GPU busy %, CPU busy, GPU W/MHz. The Ally anchor sets the "GPU-bound at 99 %" claim.

**Stage 1: GPU sweep on the Ally (one factor at a time from MAX; 3 interleaved STAND windows each, anchor re-measured after every 4 steps to catch drift):**

| Setting | Levels to run | Why |
|---|---|---|
| REN_RAT | 100, 85, 75, 65 | biggest lever and what FSR hides |
| GI | HIGHEST (anchor), HIGH, STANDARD | the model's 3x step; STANDARD gives the lower slope |
| SHADOW_QUAL | HIGH, STANDARD, LIGHT | plus one COMBAT window for the tile-budget spikes |
| VOLUMETRIC | HIGH, STANDARD, LIGHT | async cost is hard to model |
| MOTION_BLUR | OFF | one level; separate cost from taste |
| POSTEFFECT_RESOLUTION | STANDARD (with REN_RAT 75) | only when upscaling |
| SSR, AO | OFF only | verify they are <1 ms; then keep HIGHEST/HIGH |
| LANDSCAPE, GRASS, VISUAL_EFFECT | one level lower each | confirm the small-GPU assumption |

About 20 configurations x 3 windows. Screenshot each (static, no HUD) for the visual column.

**Stage 2: group tests (one run each, all in one go; split only if the group moves fps):** group "free" = MODEL_QUAL LIGHT, MESHLET STANDARD, ANISO 4, SSS OFF, COLOR_PRECISION MIDDLE, DOF/bloom/flare/CA/noise/vignette/mach OFF. If the group is within the control range, drop all of it from the search.

**Stage 3: PC CPU sweep (COMBAT crowd + STAND, 3 runs each, CPU ms is the metric, not fps):**
MODEL_DRAW FEW/NORMAL, VISUAL_EFFECT LIMITED, WIND LOW/OFF, GRASS one lower, LANDSCAPE one lower, MOTION_QUALITY LIGHT, SHADOW one lower. About 8 configurations. GI HIGHEST vs HIGH on PC (1 pair) to prove GPU-bound at HIGHEST.

**Stage 4: verify candidate presets** (section 4) with the full protocol (3 runs, STOCK vs ALL-KEPT, screenshots for the visual claim) and the stability session (item 6).

**Leave at MAX without testing:** MODEL_QUAL (no consumer), MESHLET (STANDARD == HIGH; test only to confirm), MODEL_TEX_QUAL (VRAM; test only for load hitches), SOUND_QUALITY (negligible), ANISO, SSS, COLOR_PRECISION (go in Stage 2 only), AO and SSR levels (cost <0.1 ms per step; only OFF is tested).

**Open items the data above cannot answer (decide in Stage 0/1):** the true graphics config of the 175 GP2 captures; whether the user's Ally preset has GI at HIGHEST; REN_RAT sensitivity of the 6.9 ms FSR3 chain; SHADOW HIGHEST vs HIGH; LANDSCAPE ULTRA vs HIGH; GRASS GPU share. A GP2 capture (GPUProf=2) at MAX native and one at GI HIGH would turn the E numbers in sections 2-3 into M numbers; per-window ms are relative because stamps cost ~14 ms.
