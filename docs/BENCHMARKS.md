# Nioh3EngineFix benchmarks

Status: shipping build **1.7.61** (2026-10-11). Defaults: frame overlap off, Extra Performance presets off (opt-in),
constant-upload fix CBN=1 CBT=1 (CBS=0, CBF=0).

This page lists only results with a capture, log or dated note behind them. Evidence paths point into the private
development repository, where the raw captures (PresentMon CSVs, logs, screenshots) live; they are too large and too
personal (save data, machine details) to publish, but specific reports can be shared on request. Some older numbers in earlier READMEs are not repeated here. See "Numbers we no longer use" at the end.

**The big gap:** there is no full "stock game vs 1.7.61 with default settings" benchmark yet. The results below
measure single features, or older builds. The pc2 and ROG Ally smoke tests of 1.7.61 have not been run yet.

## Test hardware

| name | CPU | GPU | display / settings | OS | notes |
|---|---|---|---|---|---|
| **PC** | Ryzen 5 7600X (6C/12T), 32 GB DDR5-4800 | Radeon RX 9070 | 2560x1440, render 75 %, XeSS -> FSR4 (OptiScaler), FG off | Windows 11 | At these settings it is GPU-bound at about 100 fps (GPU busy ~9.8 ms per frame) |
| **pc2** | Ryzen 5 5600X | Radeon RX 6650 XT 8 GB | 1080p HIGH, FSR 65 % | Windows | **Not a typical PC.** The GPU sits in a B550 chipset slot that measures about 0.8 GB/s CPU<->GPU, and the card cannot be moved. Treat pc2 gains as a slow-link worst case. |
| **Ally** | ROG Ally X: Ryzen Z1 Extreme (8C/16T) | Radeon 780M (shared memory, 8 GB of 24 GB) | 1080p 120 Hz, 25 W | Bazzite + Proton (vkd3d-proton / RADV) | About 99 % GPU-bound. Roughly 15-17 fps at MAX native 1080p in Futamata. |

All three machines are AMD. Nothing has been measured on NVIDIA, Intel Arc or Intel CPUs (see section (e)).

## Method

- **fastab (Oct 2026 onward):** one game launch, with the feature switched live in ABBA order (off, on, on, off ...).
  Each window is 15-20 s, with a 5 s warm-up after every switch. fps comes from the mod's frame clock. GPU busy is
  sampled with PresentMon. The result is the paired difference with its standard error (SE). The PC used stage 7292
  (Mirage Fortress spawn) or stage 7356 (open sky). A screenshot pair is compared against the same session's A-vs-A
  noise. Each session has a `report.md`.
- **Older sessions (2026-10-06 to 2026-10-08):** separate launches at a shrine, idle with a camera sweep, PresentMon,
  20-30 s each. Launch-to-launch noise was about 5 % (`captures/tm/results_pc_1716.md`). A difference smaller than
  that, measured between two launches, is not a result.
- **Ally loading:** the game's own loading flag (58A) window from the mod log, title -> Continue (Level 136).

## (a) The mod alone vs stock, same settings

| device | what | result | build | evidence |
|---|---|---|---|---|
| PC | Constant-upload fix (CBN+CBT, on by default) | 101.9 -> 102.5 fps, +0.55 % +-0.46 SE: **no measurable change** | 1.7.61 | `captures/pc/fastab/20261010-2308-rc1761/report.md` |
| PC | Same, earlier build | +0.49 % +-1.78 SE: no change | 1.7.60 | `captures/pc/fastab/20261010-1608-cbu2pc/report.md` |
| pc2 | Same | 38.2 -> 44.3 fps, **+15.9 %** (SE 0.06 fps, 3 pairs) | 1.7.60 | `captures/pc2/cbu1/cbu1.log`. **Slow-link machine only**; a normal x16 PC showed no change (row 1). |
| Ally | Same | -0.16 % +-0.16: no change | 1.7.60 | `notes/handoff-2026-10-10-1710.md` (bridge #1668; raw log not in the repo) |
| PC | GHDM shadow shader (optional file) | 102.7 -> 103.0 fps, +0.37 % +-0.09 SE; two repeats gave +0.65 % and +0.64 % (within noise) | 1.7.54-55 | `captures/pc/fastab/20261009-1648-ghdm_vc2/report.md`, `.../20261009-1702-ghdm_vc2/`, `.../20261009-1717-ghdm_gp2/` |
| Ally | GHDM shadow shader, USER preset | about +0.5 % | 1.7.5x | `notes/backlog-small-wins.md` (bridge #1178; raw log not in the repo) |
| PC | Old engine patches (W1/W2 memory fix, real-time frame step, spin fixes), no profiler attached, 2 back-to-back pairs | 102.2 -> 103.5 and 99.5 -> 103.2 fps (about +2.5 %); 1 % lows 79.0 -> 80.6 and 82.2 -> 82.2 | patcher scripts (pre-ASI) | `captures/results.csv` rows 10-13 |
| PC | Real-time frame step: game speed below the 120 cap | stock 0.83-0.90x (slow motion) -> **1.00x** | patcher scripts + ASI | `captures/results.csv` rows 2-4, 10-13 |
| PC | FSR frame-gen presenter fix (F1) | about 0.13 CPU core less on average over 6 launches (one launch went the other way); fps unchanged | patcher scripts | `captures/rounds2-4.csv` (round 2) |
| Ally | W6/GF wait fixes | main thread far less busy; **fps unchanged** (the GPU is the limit) | 1.7.x | `STATUS.md` line 170 |

Tested and **not** kept on PC (no gain, or a loss): P12 worker pool, physics on 4 threads, `CPU_SPEC_CLASS`
(`captures/rounds2-4.csv`); PP page-pool sweep (+0.13 % and +0.92 %, both within noise; `.../20261009-1648-ghdm_vc2/`
and `.../1702-ghdm_vc2/`); B, GF, MQ (ABBA about 0; `STATUS.md` line 256); `[GpuOpt] Barriers` -7.1 % in one window
(`captures/tm/results_pc_1716.md`).

## (b) Setup and settings advice: real gains, but not the mod

These are things you can change yourself. They are not part of the engine fix.

| what | result | evidence |
|---|---|---|
| PC: Windows desktop 60 Hz -> 240 Hz, new settings, FSR4, 120 cap (no FG) | 59.4 fps (locked at 60) -> 99.7 fps. This is the source of the old "+74 %" headline. | `captures/results.csv` rows 0-1 |
| PC: turn the in-game **"FPS dynamic adjustment" OFF** at a 120 cap | stock 72.3 / 73.3 fps with it ON (stuck in the 60 fps mode) vs 107.9 / 106.9 with it off. The old patcher scripts switched it off; **the 1.7.61 mod only shows a warning**, so you must switch it off yourself. With the setting holding 120 mode, stock reached 110.0 fps on another run. | `captures/results.csv` rows 20, 22-25; `mod/Nioh3EngineFix/src/osd.cpp` line 579 |
| pc2: Resizable BAR (SAM) on in the BIOS | GPU time 32.5 -> 24.0 ms at spawn (from the mod's GPU profiler). At a busier save spot GPU time fell 27.2 -> 19.1 ms, but **measured fps rose only 36 -> 38 (+6 %)** because the CPU became the limit. Measured on the slow-link pc2, so the size on a normal PC is unknown. | `notes/user-settings.md` lines 66-74; `notes/rebar-fix-v3.md` |
| Extra Performance **Performance** preset (opt-in; changes the image a little) | PC +3.14 % +-0.08 (102.4 -> 105.6, 1.7.61); PC +4.3 % (1.7.59 branch); Ally +3.48 % +-0.07; pc2 about 0 fps (CPU-bound), GPU -0.5 ms | `captures/pc/fastab/20261010-2308-rc1761/report.md`; `notes/extra-performance-settings.md` section 3 |
| Extra Performance **Potato** preset (opt-in; visibly coarser fog, no AO) | PC +10.5 % +-0.1; Ally +9.57 % +-0.17; pc2 about 0 fps, GPU -1.8 ms | `notes/extra-performance-settings.md` section 3 (1.7.59 branch, not re-run on 1.7.61) |
| PC: shorter far foliage/prop scatter (old panel data edit, x0.6) | 113.1 / 113.4 -> 115.1 / 114.7 fps, separate launches (inside launch noise) | `captures/rounds2-4.csv` round 7 |

## (c) Loading times

| device | route | stock | with the mod | evidence |
|---|---|---|---|---|
| Ally (Proton) | title -> Continue, Level 136 | 98.9 s | 50.1 s with EP (1.7.24); **38.45 s** with EP + SR (1.7.25, ABBA: 47.25 -> 38.45 s). Both switch on automatically under Proton. | `captures/ally/1723/`, `captures/ally/1725/`, `notes/loading-ally-fixed.md`, `MODLOG.md` line 540 |
| PC (Windows) | title -> Continue | 25.8 +-0.5 s (loading-flag window); warm title-to-game 13.5-15 s | **no change**: EP, SR, LC, LW and GL1 all measured neutral or worse on Windows | `notes/loading-bottleneck.md`, `captures/tm/results_pc_1716.md`, `STATUS.md` line 254 |

The Ally numbers are from 1.7.24-1.7.25 and have not been re-measured on 1.7.61. Of the old 118.7 s Ally window, about
25 s was the tip page waiting for a button press, not loading (`notes/loading-ally-fixed.md` header).

## (d) Experimental, not shipped

| what | result | why it is not a claim | evidence |
|---|---|---|---|
| Frame overlap (1.7.63-fo11, branch `codex-fo10`) | 60.9 -> 68.1 fps, +11.74 % +-1.54 SE, PC | Measured with an **artificial 8 ms replay delay** (an emulator), not a native gain. Not merged. An offline audit still fails (queue overflow, stream bounds). An earlier verdict put the native ceiling at about +0.4 % on pc2. | `captures/pc/fastab/20261011-0019-fo11fix/report.md`; `notes/handoff-codex-step2b.md`; `STATUS.md` line 73 |
| Constant-upload modes CBS / CBF | no fps change on PC; CBF off by default because of pc2's slow link | not default | `captures/pc/fastab/20261010-1333-cbu1pc/report.md` |
| PJ, DQ, P1E7, GIMerge, wdo14, AABB, MA | built, not proven in game | default off | `README.md` "What the game does wrong" table |

## (e) Untested hardware

- **NVIDIA GPUs, Intel Arc GPUs, Intel CPUs, AMD GPUs other than the RX 9070 / RX 6650 XT / 780M: never run.**
  `notes/hardware-compatibility.md` is a read-only code review. It found nothing vendor-specific in the default
  features, so the mod is *expected* to load and work. Expected is not measured, and gains may be larger, smaller or zero.
- On NVIDIA the game itself runs effect recording differently (async slot 0), so CPU numbers will differ
  (`notes/hardware-compatibility.md`).
- Steam Deck and other Proton handhelds: expected to behave like the Ally, not measured.
- Online co-op: the frame-timing fix pauses itself in online sessions; tested offline only.
- 1.7.61 on pc2 and the Ally: smoke tests queued (bridge #1767 / #1768), results pending.

## Numbers we no longer use

These appeared in earlier READMEs. `claims-audit.md` gives file and line for each.

- "+74 % fps (59.4 -> ~103)": nearly all of this came from setup (60 Hz desktop, settings, FSR4). The mod's share
  was about +2.5 % (section a).
- "Engine patches 72 -> 108 fps (+49 %)": this came from the Dynamic FPS game setting, which 1.7.61 does not change.
- "Raised the 1 % lows by 10 %" and "stutter spikes 3-6 -> 0-2": from a profiler-attached run or two picked pairs.
  Other patched runs the same day had 21 and 37 spikes.
- GHDM "+3 % / +11 % with FG" (PC) and "+6.4 to +8.7 % at MAX" (Ally): older multi-launch results. The paired
  re-tests give +0.4 % (PC) and +0.5 % (Ally).
- "PP kept on PC (+2.9 %)": PP is off by default; one-launch re-tests showed no change.
- W6 "+7-12 % fps on the Ally": later measurements show the Ally fps unchanged.
- ReBAR "about +35 % fps": calculated from GPU time on pc2. The measured fps gain was +6 %.
- "Works on NVIDIA / Intel Arc": not tested.
