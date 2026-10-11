# Nioh 3 settings / config.xml map (static RE of Nioh3.dump.exe)

Status: complete (2026-10-06).
- Source: `re\Nioh3.dump.exe` (raw offset == RVA, ImageBase 0x7FF74F410000 at dump time).
- Scratch: `re\settings\` (strings_all.tsv, table.txt, threads.txt, *.asm disassembly via `patcher\ft_x.fsx`).
- Configs: `configs\quality-60.xml` and `configs\perf-120.xml` (not installed).

**Goal (2026-10-06): "use the GPU more"** on the test PC.
- Baseline (the test PC's settings at the time: 60 Hz desktop, FSR FG, dynamic resolution): CPU busy 10.7 ms avg / 17 ms
  p95, GPU busy 7.0 ms, at REN_RAT 65 with dynamic resolution 50-65%.
- At those settings the game was CPU-bound. The plan is to lower the CPU-side settings and spend the idle GPU on resolution
  and GPU-side quality. (At the later test settings in docs/BENCHMARKS.md, 1440p / render 75 % / FSR4 / FG off, the PC is
  GPU-bound at about 100 fps.)

## 1. How the engine handles config.xml

| What | RVA | Notes |
|---|---|---|
| Settings descriptor table | `0x45C8570` | 60 entries x 0x30 bytes: `+0 name*`, `+8 values**`, `+0x20 default idx`, `+0x24 value count`, `+0x28/+0x2C int min/max`. Static-initialised by `fn_1FE14CC`, which is also the accessor `GetConfigDesc(idx)` |
| Live settings array | `0x45BAF30` | `int[60]`, an enum *index* per key (same order as the table). Slot address = `0x45BAF30 + 4*idx` |
| Load | `fn_1FE41A0` -> `fn_1FE4244` per key | XPath `//%s/text()` on `%LOCALAPPDATA%\KoeiTecmo\NIOH3\config.xml`. Enum keys use an exact, case-sensitive strcmp against the value list. Int keys use atoi plus a min/max check. A missing or invalid value falls back to the default |
| Validate + apply | `fn_1FE6064` | Per-key side effects: see section 4 |
| Save | `fn_1FE56A8` | Writes 57 keys. **It skips REN_REZ, NVIDIA_REFLEX and INTEL_XELL**, but the loader reads all 60 |
| Menu | `fn_60627C` | References every slot except WINDOW_POS_X/Y, REN_REZ, BRIGHTNESS (SDR brightness lives on another screen), NVIDIA_REFLEX, INTEL_XELL and CPU_SPEC_CLASS |

## 2. Full key table with cost side

The "Cost side" column says where the setting mainly costs time.

| Code | Meaning |
|---|---|
| **GPU** | Raises GPU busy time (shading, pixels, bandwidth) |
| **CPU** | Raises main-thread or worker time (object count, animation, simulation, draw submission) |
| **Both** | Costs on both sides |
| `-` | Negligible |
| ! | Large cost |

Evidence comes from this dump (consumer functions, engine thread names, render-pass strings), from Nioh3Fix's own notes, and from public benchmarks (Wccftech).

The dump confirms the renderer has these GPU features, which is why most image-quality settings below are GPU-side:
- GPU culling: "GPUCulling Setup" shader.
- Hi-Z occlusion: GenerateSceneHiZ.
- Virtual shadow maps.
- LPV + probe-field GI: BuildProbeField, LPV*.
- Hardware ray tracing: HWRaytracingShaderLibrary, AORayTrace, BMRayTrace.
- Ray-marched clouds: CloudRayTraceNum.

| # | Key | Engine values (index 0..n) | Menu | Cost side | Evidence / notes |
|---|---|---|---|---|---|
| 0 | DISP_MODE | WINDOWED, BORDERLESS | Y | - | No exclusive fullscreen value. Borderless uses the desktop refresh rate (see REN_FPS) |
| 1 | MONITOR_ID | MONITOR 1..10 | Y | - | DXGI output order. Check in the menu which number is the intended monitor |
| 2 | ADAPTER_ID | ADAPTER 1..10 | Y | - | When the adapter applies, the engine sets an Intel flag (`45BBCEE`) for PCI vendor 0x8086 |
| 3 | VSYNC | OFF, ON | Y | - | |
| 4/5 | WINDOW_POS_X/Y | int | N | - | |
| 6 | SCR_REZ | 13 fixed sizes (5120x2160 ... 1280x720) | Y | GPU (output-res passes, upscaler) | Nioh3Fix replaces the 3840x2160 entry to allow custom sizes |
| 7 | **REN_REZ** | 124 resolutions | **N, hidden, never saved** | - | Derived: `fn_1FDEAA0` recomputes it from SCR_REZ x REN_RAT, so writing it does nothing |
| 8 | REN_RAT | 25..100, steps of 5 | Y | **GPU!** | Per-axis internal-resolution %. GPU time scales roughly with pixels: 7.0 ms at 65% ≈ 16.6 ms at 100% (1440p, current settings) |
| 9 | REN_FPS | **30, 60, 120 only** | Y | sets the frame budget | **Gated by refresh rate** (`fn_FBA838`/`fn_1FE402C`): 60 needs >= 58 Hz, 120 needs >= 118 Hz, otherwise it is silently lowered |
| 10 | DYNAMIC_FPS | OFF, ON | Y | - | "FPS dynamic adjustment" (variable target). OFF = hard lock. At a 120 cap, ON can stay stuck in the 60 fps tier; switching it OFF is a setting you change yourself, not a mod gain (the mod does not change it, it only warns; docs/BENCHMARKS.md section (b)) |
| 11 | HDR | OFF, ON | Y | - (GPU-) | The dump shows live HDR = OFF although the file says ON, so the display or Windows had HDR off |
| 12-16 | HDR_* luminance, saturation, brightness, contrast | int | Y | - | Ranges: MAX_LUM 500-1000, BASE_LUM 100-500, SAT 0-10, BRIGHT 0-100, CONTRAST 0-10 |
| 17 | BRIGHTNESS | 0-100 | (other menu) | - | A value over 100 is reset to 50 |
| 18 | PRESET | CUSTOM, LIGHTEST_PERFORMANCE, LIGHT_PERFORMANCE, STANDARD, HIGH_QUALITY, HIGHEST_QUALITY | Y | - | Use CUSTOM for hand-edited files |
| 19 | SHADOW_QUAL | OFF, LIGHTEST, LIGHT, STANDARD, HIGH, HIGHEST | Y | **GPU** (+ small CPU) | Virtual shadow maps + Hi-Z are GPU passes. More shadow casters adds a little CPU culling. **OFF is rejected**: the apply step turns 0 into -1 |
| 20 | ANISOTROPIC_FILTER | OFF, 2, 4, 8, 16 | Y | GPU- | Nearly free on RDNA4 |
| 21 | AMBIENT_OCCLUSION | OFF, LIGHTEST ... HIGHEST | Y | **GPU** | AORayTrace / GBufferOcclusion passes. Wccftech's third most expensive |
| 22 | GI | LIGHTEST, LIGHT, STANDARD, HIGH, VERY_HIGH, HIGHEST | Y | **GPU!!** | LPV + probe field + indirect passes (DrawDiffuseIndirect, IndirectRenderScale). Wccftech: by far the most expensive. No OFF value |
| 23 | SSR | OFF, LIGHTEST ... HIGHEST | Y | **GPU** | Screen-space pass |
| 24 | MODEL_QUAL | LIGHT, STANDARD, HIGH | Y | Both (mostly GPU) | Mesh LOD: triangles on the GPU; LOD selection is cheap on the CPU |
| 25 | MODEL_TEX_QUAL | LIGHTEST ... HIGHEST | Y | VRAM only | Free on a 16 GB card. The test settings use STANDARD, which wastes headroom |
| 26 | MODEL_DRAW | FEW, NORMAL, MANY | Y | **CPU!** | Object count and draw distance. Nioh3Fix: draw-distance increases cost "particularly on the CPU-side". Feeds the CharacterUpdate (8), StgGdtUpd (9) and StageObject/Gadget update pools |
| 27 | VOLUMETRIC | LIGHTEST, LIGHT, STANDARD, HIGH | Y | **GPU** | Ray-marched fog and clouds (CloudRayTraceNum, VFInjection shadow) |
| 28 | WIND_SENSITIVITY | OFF, LOW, NORMAL | Y | CPU (likely) | Read by game-side code (`fn_1104880/1104F50`), not render passes. Cloth/foliage wind simulation |
| 29 | VISUAL_EFFECT | LIMITED, NORMAL | Y | **Both** | CPU: EffectUpdateDisp (4) and EffectUpdateNoDisp (9) worker pools. GPU: particle overdraw. Wccftech's second most expensive ("Effects") |
| 30 | MOTION_QUALITY | LIGHT, STANDARD, HIGH | Y | **CPU** | Nioh3Fix: "distance at which distant enemy animations are displayed at full framerate". Pure animation update cost |
| 31 | LANDSCAPE_QUALITY | LIGHTEST, LIGHT, NORMAL, HIGH, ULTRA | Y | Both | Nioh3Fix: terrain tessellation and background-mesh LOD distances, "particularly CPU". Tessellation itself is GPU |
| 32 | MESHLET_QUALITY | LIGHT, STANDARD, HIGH | Y | **GPU** (+ streaming IO) | See the note below the table |
| 33 | GRASS_DENSITY | LOWEST, LOW, NORMAL, HIGH | Y | Both | Instance count: GPU vertex and overdraw, plus CPU placement and culling (`fn_31EF94`). Wccftech lists grass as costly |
| 34 | EVENT_FPS | 30, 60 | Y | - | Cutscene cap. 60 needs >= 58 Hz |
| 35 | EVENT_QUALITY | LIGHT, STANDARD | Y | Both- | Cutscenes only |
| 36 | POSTEFFECT_RESOLUTION | STANDARD, HIGH | Y | **GPU** | Read by the upscaler-side code (`fn_2998B4C/2998CE4`, which also read FSR/XeSS sharpness). Likely post at output vs internal resolution |
| 37 | MOTION_BLUR | OFF, LIGHTEST ... HIGHEST | Y | GPU- | Post pass |
| 38-44 | CHROMATIC_ABERRATION, NOISE_FILTER, VIGNETTING, DEPTH_OF_FIELD, LENS_FLARE, BLOOM, MACH_BAND_NOISE | OFF, ON | Y | GPU- (DOF and bloom a bit more) | All read by one post-chain function (`fn_39B32C`). Taste |
| 45 | COLOR_PRECISION | MIDDLE, HIGH | Y | GPU- (bandwidth) | **There is no LOW value.** HIGH reduces banding, which matters with HDR |
| 46 | SSS | OFF, ON | Y | GPU- | Skin subsurface scattering |
| 47 | DYNAMIC_RESOLUTION | OFF, 30FPS, 45FPS, 60FPS, 75FPS, 90FPS, 105FPS, 120FPS | Y | - | Highest allowed target: REN_FPS 30 -> 30FPS, 60 -> 60FPS, 120 -> 120FPS (`fn_1FE0A24`). It lowers resolution even when CPU-bound, which buys nothing here |
| 48/49 | DYNAMIC_RENDER_RATIO_MAX/MIN | 25..100, steps of 5 | Y | - | |
| 50 | RESOLUTION_SCALING | OFF, DLSS, FSR, XESS | Y | GPU (upscaler pass) | The table default (5) is out of range, so the first run picks one by GPU vendor |
| 51-53 | DLSS_/FSR_/XESS_SHARPNESS | 0.0..1.0, steps of 0.1 | Y | - | |
| 54 | FRAME_GENERATION | OFF, DLSS, FSR, **XeSS** | Y | GPU (+ latency) | Mixed case "XeSS". The exe imports libxess_fg.dll |
| 55 | DLSS_FRAME_GEN_MULI | 2X, 3X, 4X | Y | - | Misspelled in the engine; keep the spelling |
| 56 | **NVIDIA_REFLEX** | OFF, ON, ON_BOOST | **N, hidden, not saved** | latency | The loader reads it. It goes through Streamline (sl.interposer.dll), so on AMD it only works if OptiScaler/fakenvapi supplies it (-> Anti-Lag 2). Experiment only |
| 57 | **INTEL_XELL** | OFF, ON | **N, hidden, not saved** | latency | libxell.dll is imported. Intel GPUs only |
| 58 | SOUND_QUALITY | LIGHT, STANDARD | Y | CPU- | Audio synthesis threads (KTSL_SYNTH) |
| 59 | **CPU_SPEC_CLASS** | LOW, STANDARD, HIGH | **N, hidden** | none observed | First-run benchmark result. Does **not** size thread pools (4.2) |

**MESHLET_QUALITY detail.**
- **What the code does:** its consumer `fn_2353C0` uses the value (`45BAFB0`) to pick a column (+4/+8/+0xC) of a per-stage LOD table. That column multiplies 8 stage LOD/distance floats at `+0xA108..+0xA124`.
- **Why it is GPU-side:** stage geometry is meshlet-based ("MeshletCheckRequestLoad" streaming) and the renderer has a GPU-culling setup pass. Higher quality means more triangles past GPU culling plus more meshlet streaming. It does **not** add CPU draw calls the way MODEL_DRAW does.
- **Effect:** keeping it HIGH moves geometry detail onto the GPU, which is what we want.

**CPU-side settings to lower:** MODEL_DRAW, MOTION_QUALITY, WIND_SENSITIVITY, then GRASS_DENSITY / LANDSCAPE_QUALITY / VISUAL_EFFECT (those three are "Both").

**GPU-side settings to raise with spare budget:** REN_RAT, GI, SHADOW_QUAL, AO, SSR, VOLUMETRIC, POSTEFFECT_RESOLUTION, MESHLET_QUALITY, MODEL_QUAL, AF, COLOR_PRECISION, MODEL_TEX_QUAL (VRAM only).

### Strings present but dead
These look like debug keys: `MIPDOWNLEVEL`, `MIPDOWNLEVEL_PLAYER`, `MIPDOWNLEVEL_CHARA`, `MIPDOWNLEVEL_STAGE`, `MIPDOWNLEVEL_RENDERTARGET`, `DEBUG_JUMP`, `REMOVE_MAP_CLOUD`, `DISP_DAMAGE_HEAL ` (with a trailing space), `DEBUG_DAMAGE_PC`, `DEBUG_DAMAGE_ENEMY_NPC`.
They have **zero code or data references** in retail, so adding them to config.xml does nothing.

## 3. Other files, switches and environment

- **config.xml is the only config file.** The exe's strings name only `config.xml`, `steam_appid.txt` and the Savedata paths: no `.ini` or `.json`, and no debug-menu strings beyond script bindings (`Debug::Printf`, `mud::debug::*`).
- **Command line:** the only switch found is `--enable_output_dump`, which writes crash dumps to `%LOCALAPPDATA%\KoeiTecmo\NIOH3\dump\core_dump_*.dmp`. A generic GetCommandLineA parser (`fn_3223C1C`) exists; no other switch strings were found.
- **Environment:** no game-specific environment variables. The exe imports VCOMP140.dll (MSVC OpenMP), so `OMP_NUM_THREADS` / `OMP_WAIT_POLICY` apply to whatever uses OpenMP. Low priority.
- **Notable imports:**
  - Upscaling and latency: sl.interposer.dll (Streamline), libxess.dll, libxess_fg.dll, libxell.dll, amd_ags_x64.dll; FSR3 is built in.
  - Threading and timing: GetSystemCpuSetInformation + SetThreadSelectedCpuSets (the engine pins threads to CPU sets itself), timeBeginPeriod, Sleep (407 call sites), SetWaitableTimer.

## 4. Engine-level levers

### 4.1 REN_FPS is gated by refresh rate
`fn_FBA838` runs on REN_FPS / EVENT_FPS apply. It calls `fn_9AFE00`, which returns the cached refresh rate (`45BBCC4`; 60 in the dump):
- Below 58 Hz: forced to 30.
- 58-117 Hz: 60 is the maximum.
- 118 Hz and up: 120 is allowed.

So:
- **A 75 Hz monitor:** 60 is the best possible lock. Keep its desktop at **60 Hz** for this game, because 60 fps on 75 Hz judders.
- **A 144 Hz monitor:** 120 is allowed. Set its desktop to **120 Hz** for an exact cadence with VSYNC ON.
- **Patch target** for a 72/75 lock or for 120 on a 75 Hz monitor: `fn_1FE402C` (the 0x76 / 0x3A compares).

### 4.2 CPU_SPEC_CLASS does not control thread counts
- **Where it comes from:** the first-run benchmark (`fn_1C33FA8` -> `fn_1FE0054`).
  - score < 15,000,000 -> LOW
  - score < 32,000,000 -> STANDARD
  - otherwise -> HIGH
  - The 7600X got STANDARD.
- **Where it goes:** `cpuinfo+0x284` (as 1/2/3).
- **What reads it:** in the first-run auto-preset (`fn_1C33174`), LOW forces REN_FPS = 30. No other reader of the live slot exists.
- **Conclusion:** HIGH is harmless but probably does nothing (still to confirm with Ghidra xrefs to `cpuinfo+0x284`). Forcing it was tested on the PC and not kept: no gain (docs/BENCHMARKS.md).

### 4.3 Thread-pool sizing
This is the CPU-side engine lever.
- **CPU detection:** `fn_298EDA8` calls GetLogicalProcessorInformationEx and records physical (+0x270), logical (+0x274), P (+0x278) and E (+0x27C) core counts, an "is Ryzen" flag (+0x281) and an "is Zen1/Zen+" flag (+0x282).
- **Pool sizing:** `fn_1FE2FD4` sets the worker count to clamp(logical, 6, 32), which is **12** here, then sizes 74 named pools from the weight table at `0x4B722D0+0xC`. Live counts are in `re\settings\threads.txt`, e.g.:
  - CharacterUpdate 8, StgGdtUpd 9, EffectUpdateNoDisp 9, Dummy:StageGadgetUpdate 18
  - KIDS_RESOURCE 7, StageUpdate 4, PhysUpdate 4
  - KTGLGraph.Cache.UpdateTileMapping 32
- **Penalty:** with 12 or fewer logical CPUs, KIDS_RESOURCE and StageUpdate lose 3 threads each.
- **Oversubscription:** about 90 threads on 12 hardware threads. Patch candidates once ETW shows contention:
  - Trim the pools.
  - Or lift the 12-thread penalty.
- **Extra CPU knobs:** Nioh3Fix's LOD-distance floats (Character/Model/Parts/HLOD/MotionQuality multipliers). Values below 1.0 go further than MODEL_DRAW FEW / MOTION_QUALITY LIGHT. Their AOBs are in `nioh3fix-signatures.md`, so the external patcher can poke them without the unsigned ASI.

## 5. Mod #36 presets vs the test settings

Test settings (baseline): STANDARD preset, REN_RAT 65, dynamic resolution 60FPS (50-65%), FSR upscaling + FSR FG, VSYNC ON, HDR ON (inactive), DYNAMIC_FPS ON.

| Key | Test | opt36 "qualidade" (default) | opt36 "perfomace" (fps) | opt36 visual | Side | Likely impact |
|---|---|---|---|---|---|---|
| VSYNC | ON | OFF | OFF | OFF | | Tearing; judder remains unless locked |
| SCR_REZ | 2560x1440 | 2560x1440 | **1920x1080** | **1920x1080** | GPU | The fps/visual presets were made on 1080p. Ignore |
| REN_RAT | 65 | **100** | **100** | 65 | GPU | 100 = native AA, +137% pixels vs 65 |
| REN_FPS | 60 | 120 | 120 | 120 | | Only effective on a 118 Hz+ display |
| DYNAMIC_FPS | ON | OFF | OFF | ON | | |
| HDR | ON | OFF | OFF | OFF | | |
| BRIGHTNESS | 100 | 52 | 52 | 50 | | |
| SHADOW_QUAL | STANDARD | HIGH | STANDARD | STANDARD | GPU | Moderate |
| ANISOTROPIC_FILTER | 4 | 16 | 8 | 16 | GPU- | About free |
| AMBIENT_OCCLUSION | STANDARD | HIGH | **OFF** | HIGH | GPU | Moderate-high |
| GI | STANDARD | **HIGHEST** | STANDARD | VERY_HIGH | GPU!! | The heaviest knob |
| SSR | STANDARD | STANDARD | **OFF** | LIGHTEST | GPU | Moderate |
| MODEL_TEX_QUAL | STANDARD | HIGH | STANDARD | STANDARD | VRAM | |
| MODEL_DRAW | MANY | MANY | **FEW** | MANY | **CPU** | The fps preset's only real win when CPU-bound |
| VOLUMETRIC | STANDARD | HIGH | STANDARD | STANDARD | GPU | Moderate |
| WIND_SENSITIVITY | NORMAL | NORMAL | LOW | NORMAL | CPU | Small |
| LANDSCAPE_QUALITY | NORMAL | HIGH | NORMAL | NORMAL | Both | Moderate |
| GRASS_DENSITY | HIGH | HIGH | **LOW** | LOW | Both | Moderate |
| MOTION_BLUR | STANDARD | OFF | OFF | HIGHEST | GPU- | |
| CA / VIGNETTE / DOF / FLARE | ON | OFF | OFF | ON | GPU- | Taste |
| NOISE_FILTER / BLOOM / MACH_BAND_NOISE | ON / ON / ON | ON / ON / OFF | OFF / OFF / OFF | ON / ON / ON | GPU- | BLOOM OFF changes the look a lot |
| COLOR_PRECISION | MIDDLE | HIGH | MIDDLE | HIGH | GPU- | |
| SSS | ON | ON | OFF | ON | GPU- | |
| DYNAMIC_RESOLUTION | 60FPS | OFF | OFF | 60FPS | | |
| FRAME_GENERATION | FSR | OFF | OFF | OFF | | |

Verdict: all three presets mostly cut GPU work. That does little on the PC at these CPU-bound baseline settings; only the "perfomace" preset's MODEL_DRAW FEW, GRASS LOW and WIND LOW touch the bottleneck. None of them use XeSS/FSR4 or address the refresh gate.

## 6. Proposed configs

Both configs assume OptiScaler with XeSS input -> FSR4, i.e. `RESOLUTION_SCALING=XESS` with XESS_SHARPNESS 0.3; OptiScaler's RCAS can override the sharpening.

Both configs:
- Turn dynamic resolution OFF, because it would cut resolution during CPU spikes for no gain.
- Set DYNAMIC_FPS OFF for a hard lock and VSYNC ON.
- Use MODEL_TEX_QUAL HIGHEST, AF 16 and EVENT_FPS 60.

**CPU relief in both:**
- MODEL_DRAW MANY -> **NORMAL** (the largest CPU knob).
- MOTION_QUALITY HIGH -> **STANDARD** (full-rate animation only affects distant enemies, so it is barely visible).

**GPU budget model:** GPU busy is roughly proportional to pixels. 7.0 ms at 65% / 1440p implies about 16.6 ms at 100%.
- That is an upper bound: the baseline average resolution may have been below 65% because dynamic resolution ran at 50-65%.
- For a 16.7 ms lock, keep about 1.5-2 ms of slack plus the cost of the GPU-quality bumps.

### quality-60.xml: 75 Hz monitor at 2560x1440, desktop at 60 Hz, no frame generation
- **Resolution:** REN_RAT **75** (1920x1080 into FSR4, above FSR4 "Quality" at 67%). Estimated GPU time: 7.0 x (75/65)^2 ≈ **9.3 ms**, leaving about 5 ms for quality.
- **GPU-side raises:**
  - SHADOW_QUAL STANDARD->HIGH
  - AO STANDARD->HIGH
  - GI STANDARD->**HIGH**
  - SSR STANDARD->HIGH
  - VOLUMETRIC STANDARD->HIGH
  - LANDSCAPE NORMAL->HIGH
  - POSTEFFECT_RESOLUTION ->HIGH
  - COLOR_PRECISION ->HIGH
  - AF 4->16
  - TEX ->HIGHEST
- **Kept:** MESHLET HIGH, MODEL_QUAL HIGH, GRASS HIGH, WIND NORMAL, VISUAL_EFFECT NORMAL, HDR ON, all post toggles, MOTION_BLUR STANDARD.
- **Next steps once PresentMon shows GPUBusy:**
  1. If it is still under about 13 ms, raise GI to VERY_HIGH; that is where the next visible gain is.
  2. Then raise REN_RAT to 80.
  3. If it goes over 15 ms, drop GI back or set REN_RAT to 70.
- **Risk:** the CPU p95 (17 ms) is above the 16.7 ms budget. MODEL_DRAW and MOTION_QUALITY trim it; if spikes remain, set GRASS NORMAL, then apply the 4.3 patches.

### perf-120.xml: 144 Hz monitor at 1920x1080, desktop at 120 Hz (MONITOR 2, verify in the menu)
- **Lock:** REN_FPS 120 (passes the >= 118 Hz gate), VSYNC ON.
- **Frame generation:** **FRAME_GENERATION=FSR**, with a rendered base of about 60.
  - A native 8.3 ms frame is impossible with a CPU averaging 10.7 ms.
  - FG keeps the camera cadence steady at 120, which avoids the 60-120 judder.
  - **Verify in-game** that the engine halves the base rate with FG on and REN_FPS 120.
- **GPU budget:** 1080p is 56% of the 1440p pixel count.
  - REN_RAT **85** -> 1632x918 ≈ 1.50 MP, about 6.7 ms at baseline settings.
  - Add the HIGH bumps and roughly 1-2 ms of FG cost: about 10-11 ms. That is well inside a 16.7 ms base frame.
- **GPU-side:** SHADOW/AO/GI/SSR/VOLUMETRIC HIGH, POSTEFFECT HIGH, COLOR HIGH, MESHLET/MODEL HIGH.
- **Extra CPU cuts beyond quality-60** (GPU headroom matters less here than CPU stability):
  - GRASS NORMAL, LANDSCAPE NORMAL, WIND LOW.
  - MOTION_BLUR LIGHT (less smear on the interpolated frames).
  - HDR OFF (that monitor is SDR).
- **Native-120 variant** (only after CPU patches get CPU busy below about 8 ms): set `FRAME_GENERATION=OFF`, `MODEL_DRAW=FEW`, `MOTION_QUALITY=LIGHT`, `GRASS_DENSITY=LOW`, `REN_RAT=70`.

### Sweep order once the game can run
One 30 s PresentMon capture each; watch CPUBusy and GPUBusy:
1. GI up one step.
2. REN_RAT ±5.
3. MODEL_DRAW NORMAL vs MANY (CPU delta).
4. GRASS_DENSITY (CPU and GPU deltas).
5. VISUAL_EFFECT LIMITED in a heavy-effects fight.
