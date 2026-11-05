# Extreme HQ (Preset 5) and FSR sharpness (XSHARP) - #67772, branch pc/extreme-hq

2026-11-04, offline. The game was not run for this note. Human #67772: keep most of Extreme's gain (pc2 +29 %, Ally
+35-39 %, almost all of it from XRES 0.7) but with a near-1:1 picture. Human #67786: test Extreme on all devices and let the
human try it before the next variant is benched, so the recipe at the end waits for that.

## 1. What is built

| key | what | engine path (no draw skips) | default | in Preset 5 |
|---|---|---|---|---|
| `Preset = 5` | Extreme HQ (native res) | the rows below plus existing XGI / XCLD / XSHF | 0 | - |
| `XFOGS` 0/1/2 (new) | volumetric fog grid x0.75 / x0.5 per axis (X and Z) | S+0x13DC / 0x13E0 after ApplyVolumetricQuality. Never raised, floor 16 x 8, fog-off stays off. XFOG wins when both are set | 0 | 1 |
| `XSSR` 0/1/2 (new) | SSR at half size; 2 = also ray steps x0.5 | the RLR object (`2386FC([[4759910]+530], 0xEF3D849D)`) +0xB8 = 1. ApplySsrQuality 39B0E8 writes the same flag for SSR LIGHTEST..STANDARD every tick. S+0x107C steps x0.5 | 0 | 1 |
| `XSHARP` 0..5 (new, not a preset field) | FSR 3 RCAS strength 0.0 / 0.25 / 0.5 / 0.75 / 1.0. 0 = game | S+0x1F58, the float FsrConfigApply 2998B4C passes to the FSR 3.1 upscale dispatch. FsrConfigApply always sets enableSharpening S+0x1F54 = 1 | 0 | no (works with Preset 4) |
| XGI 3 (existing) | GI diffuse + reflection probe gathers x0.35 | GiScaleLookup cave: the engine's own sub-resolution gather, then its bilateral GI composite (RefProbe #9 7c978f3f) upsamples | - | 3 |
| XCLD 2 (existing) | clouds 64 ray-march steps | S+0x410 / 0x40C. VCLOUD already runs at render/8 with a 16-frame Bayer pattern and reprojection | - | 2 |
| XSHF 1 (existing) | sun-shadow filter 2 taps (game 4) | S+0xB44 | - | 1 |

Preset 5 runs at the native render size: no XRES and no XHALF. XHALF is left out because of the Ally strobe and brown
sky (#216) and the rock-shade flicker on pc2 (#219). Geometry, LOD, terrain, textures, particles, AO, motion blur, DOF,
bloom, shadow distance, UI and post all stay at the game's values. Fog stays on and only gets a coarser grid. XSHARP is
read after the preset and ApplyPreset never touches it, so `Preset = 4` plus `XSHARP = n` gives "Extreme, sharper". Each
piece has its own ini key and menu row (Extra performance page: "Volumetric fog grid scale", "Reflections at half size",
"Upscaler sharpening"). LiveCtl / `vpad menu set` keys: XFOGS, XSSR, XSHARP. The menu Save writes all of them.

**Upsample quality.** No new shader was written. Every reduced pass goes through a reduced-resolution path the engine
already has, plus the engine's own reconstruction:
- GI: bilateral, depth-aware composite.
- Fog: trilinear froxel lookup.
- Clouds: Bayer reprojection.
- SSR: the half-size path the engine uses at SSR STANDARD.

A bilateral upsample in the consumer pass was not needed, so none was added.

## 2. What was not done, and why

- **fp16 FSR (R1)** was never built. The notes list it as "not built, near-look, user decision" (`ally-gpu-budget.md` §5,
  `remaining-work.md` 3.8). Platform by platform:
  - pc2 (RDNA2): its min16float already runs at native fp16 (`gpu-resolution-audit.md` §5).
  - PC: runs XeSS -> OptiScaler FSR4, so the game's FSR3 does not run at all (`windows-gpu-plan.md`).
  - Ally: R1 would be worth 0.2-0.5 ms (estimate), but it needs a dxc rebuild of the FFX ACCUMULATE_SHARPEN CS plus a
    proof. That cannot be done in an ASI-only change.
- **fogpar** is a ShaderSwap bytecode file, not ASI code. The Ally already ships it as the game-file swap
  (`ally_shaders.sh apply fogpar`, release keeps it, ABBA +0.225 fps), so it is present in every Ally arm. On PC and pc2
  it needs `[ShaderSwap] Enabled = 1` plus `641524ee74b197c7d8f1535066bb2c1a.dxil` in `Nioh3EngineFix_shaders\`. That
  file is a `gpu-shaders\fog\build_fogpar.py` output, gitignored and not on this PC. ShaderSwap also turns off PL
  (pipeline library), which risks shader-compile hitches. So it is not stacked into Preset 5: it is bit-exact, worth
  only 0.05-0.2 ms, and can be added as a separate arm once the file is staged.
- **AO at reduced resolution.** HBAO already runs at half resolution with 7 rays below 721 px of render height, and it
  costs ~0 to 0.13 ms (`gpu-maxsettings.md`, `gpu-resolution-audit.md`). What S+0x12E8 means (the half/full-res flag)
  was not proven, so AO is left native. XAO (off) remains a separate key.

## 3. Expected saving per pass (from the notes; E = estimate)

| pass | Ally USER (28.7 ms, 960x540) | pc2 (RX 6650 XT, 1080p REN_RAT 65) | source |
|---|---|---|---|
| GI + reflection gathers, XGI 3 | GI family 1.77 -> about -0.6 to -0.8 E. Measured: XGI3 alone +2.1 % fps | XGI -3.1 % GPU (measured). PC XGI3 -0.34 ms, +3.6 %, look = noise | ally-gpu-budget §2, extra-performance-settings §3 |
| clouds, XCLD 2 | VCLOUD real <= 0.3 ms E. XCLD3 measured +2.5 % | PC 0 (measured) | same |
| fog grid, XFOGS 1 (x0.42 cells) | VFSCAT 1.78 async, 0.71 real -> about -0.2 to -0.4 E | XFOG1 -0.17 ms PC at x0.16 cells, so about -0.1 E | ally-gpu-gap, extra-performance-settings |
| SSR half size, XSSR 1 | 0 at USER (SSR LIGHTEST is already half). MAX 1.0 -> about -0.5 E | PBRLR 0.11 ms -> about -0.05 E | gpu-resolution-audit §1, ally-gpu-budget §3 |
| shadow filter, XSHF 1 | part of WDO_SST 2.04. Unmeasured, <= 0.2 E | PC 0 (measured) | extra-performance-settings |
| **Preset 5 total** | **about -1.0 to -1.6 ms, +4-6 % fps (E)**. The Optimized-with-effect-visuals preset measured +3.5 % | **about -0.5 to -1.0 ms GPU, +3-5 % (E)**. pc2 is often CPU-bound | |

**Honest bar.** Preset 5 does not keep "most" of Extreme's +29-39 %. Every non-resolution cut in the notes adds up to
2-3 ms at most (`big-wins-pass-rank.md`: none of the four families clears 3.74 ms). Internal resolution is the only lever
measured past 15 %. If the bench confirms this, there are two picture-close options for most of the gain:
- Arm D: Extreme + XSHARP. The FSR3 devices get sharper at the same speed.
- Arm E: Preset 0 + the Preset 5 keys + XRES 1/2 (x0.9 / x0.8) + XSHARP. Judge it by SSIM against the noise floor.

## 4. Risks

- **XSSR.** The half-size flag is a per-tick field. The engine flips it itself when the player changes SSR quality, but
  it has never been forced while SSR is at HIGH/HIGHEST in game. If the RLR targets are only resized on a settings or
  area change, the first frames could show a quarter-size or offset reflection. Check the shots of a wet / shiny surface
  in arm C. If it is wrong, bench with `XSSR = 0` (the preset row reads "custom"). XSSR 2 only scales S+0x107C, as
  ApplySsrQuality does.
- **XFOGS.** Coarser fog cells mean softer god rays. Extreme's brown sky came from fog off plus XHALF, and Preset 5 keeps
  fog on.
- **XGI 3.** Softer, slightly laggier indirect light and reflections. Measured as noise on PC, but "lighter / flatter
  haze" was noted on the Ally for XGI 2 + XCLD 2.
- **XSHARP.** Only the game's FSR3 path reads S+0x1F58. Values stay within the game's legal 0..1. Above 1.0, FFX RCAS's
  `con > 1.33` can make the `4 * lobe + 1` denominator reach 0, so it was capped at 1.0. On the Ally (FSR_SHARPNESS
  already 1.0) only levels < 1.0 change anything. PC (OptiScaler FSR4) ignores it.
- **Bytes.** Every new site is byte-checked against v2.02 (dllmain + `run_extraperf_harness.py`). Any mismatch means
  "SSR half-size flag NOT installed" in the log, and nothing else changes.

## 5. Offline checks

- `tests/run_extraperf_harness.py` ALL PASSED. It covers:
  - XFOGS: scale, floor, never raised, XFOG wins, restore.
  - XSSR: steps x0.5 + restore; the flag via a fake lookup (args, +B8 only, null manager); the dump bytes of
    39B0F7 / 2386FC / the +B8 writes.
  - XSHARP: levels, game value back, not moved by presets; the FsrConfigApply S+1F58 / S+1F54 stores and the upscaler
    vcall 39AEFB in the dump.
  - Preset 5 from the ini and through ApplyS (fields, SSR flag, no XRES).
- `tools/gen_rvas.py --check`: 211 / 211.
- testhub offline 5/5.
- `tools/bench/imgdiff.py selftest` (PSNR / SSIM).
