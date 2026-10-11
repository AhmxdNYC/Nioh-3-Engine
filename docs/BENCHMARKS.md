# Nioh3EngineFix benchmarks

Status: shipping build **1.7.61** (2026-10-11). Defaults: frame overlap off, Extra Performance presets off (opt-in),
constant-upload fix CBN=1 CBT=1 (CBS=0, CBF=0).

This page lists only results that were re-checked against the raw captures (PresentMon CSVs, mod logs, per-session
`report.md` files). Those captures stay in the private development repository: they are large and contain machine
and save details. Specific reports can be shared on request. Evidence names below are capture folders or files there.

**The big gap:** there is no full "stock game vs 1.7.61 with default settings" benchmark yet. The results below
measure single features, or older builds. The second PC and ROG Ally smoke tests of 1.7.61 are still pending.

## Test hardware

| name | CPU | GPU | display / settings | OS | notes |
|---|---|---|---|---|---|
| **PC** | Ryzen 5 7600X (6C/12T), 32 GB DDR5-4800 | Radeon RX 9070 | 2560x1440, render 75 %, XeSS -> FSR4 (OptiScaler), FG off | Windows 11 | At these settings it is GPU-bound at about 100 fps |
| **pc2** | Ryzen 5 5600X | Radeon RX 6650 XT 8 GB | 1080p HIGH, FSR 65 % | Windows | **Not a typical PC.** The GPU sits in a chipset slot that measures about 0.8 GB/s CPU<->GPU. Treat pc2 gains as a slow-link worst case. |
| **Ally** | ROG Ally X: Ryzen Z1 Extreme (8C/16T) | Radeon 780M (shared memory) | 1080p | Bazzite + Proton (vkd3d-proton / RADV) | About 99 % GPU-bound. Roughly 15-17 fps at MAX native 1080p in Futamata. |

All three machines are AMD. Nothing has been measured on NVIDIA, Intel Arc or Intel CPUs (see section (e)).

## Method

- **fastab (October 2026 onward):** one game launch, with the feature switched live in ABBA order (off, on, on, off ...).
  Windows are usually 15-20 s (some sessions used 100 s), with a warm-up after every switch. fps comes from the mod's
  frame clock; GPU busy is sampled with PresentMon. The result is the paired difference with its standard error (SE).
- **Older sessions (2026-10-06 to 2026-10-08):** PresentMon, 20-30 s windows, idle at a shrine with a camera sweep.
  Some A/B pairs were toggled live inside one launch; others were separate launches. Separate launches of the same
  build differed by up to about 5 %, so cross-launch differences smaller than that are not results.
- **Ally loading:** the game's own loading-flag window from the mod log, title screen -> Load Game (Level 136).

## (a) The mod alone vs stock, same settings

| device | what | result | build | evidence |
|---|---|---|---|---|
| PC | Constant-upload fix (CBN+CBT, on by default) | 101.9 -> 102.5 fps, +0.55 % +-0.46 SE: **no measurable change** | 1.7.61 | `fastab/20261010-2308-rc1761/report.md` |
| PC | Same, earlier build | +0.49 % +-1.78 SE: no change | 1.7.60 | `fastab/20261010-1608-cbu2pc/report.md` |
| pc2 | Same | 38.2 -> 44.3 fps, **+15.9 %** (SE 0.06 fps, 3 pairs) | 1.7.60 | `pc2/cbu1/cbu1.log`. **Slow-link machine only**; the normal PC above showed no change. |
| Ally | Same | -0.16 % +-0.16: no change | 1.7.60 | Ally run log (CBMODE 0/6) |
| PC | Old engine patches (W1/W2 memory fix, real-time frame step, spin fixes), two on/off pairs inside one launch, no profiler | 102.2 -> 103.5 and 99.5 -> 103.2 fps (about +2.5 % on average, only two pairs); 1 % lows 79.0 -> 80.6 and 82.2 -> 82.2 | patcher scripts (before the ASI) | `results.csv` rows 10-13 |
| PC | Real-time frame step: game speed below the 120 cap | stock 0.83-0.90x (slow motion) -> **1.00x** | patcher scripts + ASI | `results.csv` rows 2-4, 10-13 |
| PC | FSR frame-gen presenter fix (F1) | about 0.13 CPU core less on average over 6 frame-gen launches (one went the other way); fps differences were within the large noise of those runs | patcher scripts only; not a 1.7.61 option | `rounds2-4.csv` (round 2) |
| Ally | GF (GPU-fence wait sleeps instead of spinning; auto-on under Proton) | main thread about 85 % -> 32 % busy; **fps unchanged** (the GPU is the limit) | 1.7.x | Ally run logs |
| PC | GHDM shadow shader (optional file, **not distributed**), frame generation off | 102.7 -> 103.0 fps, +0.37 % +-0.09 SE; two repeats gave +0.65 % and +0.64 % | 1.7.54-55 | `fastab/20261009-1648-ghdm_vc2`, `.../1702-ghdm_vc2`, `.../1717-ghdm_gp2` |
| Ally | GHDM shadow shader, USER preset | about +0.5 % | 1.7.5x | Ally run log |

Tested and **not** kept on PC: P12 worker pool, physics on 4 threads, `CPU_SPEC_CLASS` (`rounds2-4.csv`, no gain);
PP page-pool sweep (+0.13 % and +0.92 %, within noise); B (ABBA -0.11 %); `[GpuOpt] Barriers` (-7.1 % over two 20 s
windows). GF and MQ on Windows have **no valid result**: the ABBA report marked both runs invalid because fps stepped
after the first toggle. They stay off on Windows.

W6 (main-thread GPU wait) has no measured fps effect in a normal setup: in the stock Proton environment it never
engages. It is auto-on under Proton because it is harmless there.

## (b) Setup and settings: real gains, but not the mod

These are things you can change yourself. They are not part of the engine fix.

| what | result | evidence |
|---|---|---|
| PC: Windows desktop 60 Hz -> 240 Hz, new settings, FSR4, 120 cap (no FG) | 59.4 fps (locked at 60) -> 99.7 fps | `results.csv` rows 0-1 |
| PC: in-game **"FPS dynamic adjustment" OFF** at a 120 cap, stock game, nothing else changed | 74.7 fps with it ON (stuck in the 60 fps mode) vs 107.8 / 109.1 fps with it OFF. **The mod does not change this setting**; it only shows a warning, so switch it off yourself. | `bench-ab-dynon1.csv`, `bench-ab-dynoff1.csv`, `bench-ab-dynoff2.csv` |
| pc2: Resizable BAR (SAM) on in the BIOS | GPU time 27.2 -> 19.1 ms at a busy spot, but **measured fps rose only 36 -> 38 (+6 %)** because the CPU became the limit. Slow-link pc2 only, so the size on a normal PC is unknown. | ReBAR notes and mod GPU-profiler logs |
| Extra Performance **Performance** preset (opt-in; changes the image a little) | PC +3.14 % +-0.08 (102.4 -> 105.6, 1.7.61); PC +4.3 % (1.7.59 branch); Ally +3.48 % +-0.07; pc2 about 0 fps (CPU-bound), GPU -0.5 ms | `fastab/20261010-2308-rc1761/report.md`; Ally run log |
| Extra Performance **Potato** preset (opt-in; visibly coarser fog, no ambient occlusion) | PC +10.5 % +-0.1; Ally +9.57 % +-0.17; pc2 about 0 fps, GPU -1.8 ms | 1.7.59 branch, not re-run on 1.7.61 |
| PC: shorter far foliage/prop scatter (old data edit, x0.6) | 113.1 / 113.4 -> 115.1 / 114.7 fps, separate launches (inside launch noise) | `rounds2-4.csv` round 7 |

## (c) Loading times

| device | route | without the fix | with the fix | evidence |
|---|---|---|---|---|
| Ally (Proton) | title -> Load Game, Level 136, cold first launch | 96.0 s (mod installed, EP off) | 47.3 s with EP (1.7.23). Paired SR test on 1.7.25: 47.25 -> **38.45 s**. EP and SR switch on automatically under Proton. | Ally logs `1723/`, `1725/` |
| PC (Windows) | title -> Continue | no stock capture | 25.8 +-0.5 s with mod 1.7.16 (loading-flag window). LC, LW and GL1 measured no change. EP and SR are Proton-only and were never tested on Windows. | load-stats logs |

The Ally numbers have not been re-measured on 1.7.61. Of the old 118.7 s Ally window, about 25 s was the tip page
waiting for a button press, not loading.

## (d) Experimental, not shipped

| what | result | why it is not a claim |
|---|---|---|
| Frame overlap (1.7.63-fo11, experimental branch) | 60.9 -> 68.1 fps, +11.74 % +-1.54 SE, PC | Measured with an **artificial 8 ms replay delay** (an emulator), not a native gain. Not merged. An offline safety audit still fails. An earlier native test on pc2 measured about +0.4 %. 1.7.61 contains an older version of it, off by default. |
| Constant-upload modes CBS / CBF | CBS: no fps change on PC. CBF: +2.8 % +-3.1 on PC (no measurable change) | off by default; CBF is test-only |
| Other `[Patches]` / `[GpuOpt]` options marked experimental in the ini | built, not proven in game | off by default |

## (e) Untested hardware

- **NVIDIA GPUs, Intel Arc GPUs, Intel CPUs, and AMD GPUs other than the RX 9070 / RX 6650 XT / 780M: never run.**
  A read-only code review (done on 1.7.33) found nothing vendor-specific in the default features of that build, so
  the mod is *expected* to load and work. That review predates the constant-upload fix and the Extra Performance hooks.
  Expected is not measured, and gains may be larger, smaller or zero.
- On NVIDIA the game itself records effects differently (async slot 0), so CPU numbers will differ.
- Steam Deck and other Proton handhelds: expected to behave like the Ally, not measured.
- Online co-op: the frame-timing fix pauses itself in online sessions; tested offline only.
- 1.7.61 on pc2 and the Ally: smoke tests queued, results pending.
- CBN/CBT are on because the shipped ini turns them on. An ini from before 1.7.60 leaves them off.

## Numbers we no longer use

These appeared in the development README at various points.

- "+74 % fps (59.4 -> ~103)": nearly all of this came from setup (60 Hz desktop, settings, FSR4). See (b).
- "Engine patches 72 -> 108 fps (+49 %)": this came from the "FPS dynamic adjustment" setting, which the mod does not
  change. See (b).
- "Raised the 1 % lows by 10 %" and "stutter spikes 3-6 -> 0-2": from a profiler-attached run or two picked pairs.
  Other patched runs the same day had 21 and 37 spikes.
- GHDM "+3 % / +11 % with FG" (PC) and "+6.4 to +8.7 % at MAX" (Ally): older multi-launch results. Later paired tests
  gave +0.4 % (PC, without frame gen) and +0.5 % (Ally, USER preset). Those are different settings, so the old numbers
  were never re-tested like for like.
- "PP kept on PC (+2.9 %)": PP is off by default; paired re-tests showed no change.
- "B +5 % on PC": a later ABBA test gave -0.11 %. B is off.
- W6 "+7-12 % fps on the Ally": measured with a non-default `VKD3D_SWAPCHAIN_LATENCY_FRAMES=1` override. In a normal
  setup W6 never engages.
- ReBAR "about +35 % fps": calculated from GPU time on pc2. The measured fps gain was +6 %.
- "Works on NVIDIA / Intel Arc": not tested.
