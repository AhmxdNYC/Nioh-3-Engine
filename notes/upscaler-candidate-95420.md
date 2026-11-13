# Better upscaler at lower internal res: history + candidate (task #95420, 2026-11-13, offline)

performance-pc/0. No game launched. Builds on `upscaler-swap-re.md` (hook points, per-device table, 5-step plan), `fsr3-audit.md`,
`fsr-post-re-91962.md`, `engine-rewrite-options.md` row 9, `drs-pc-test-123.md`.

## 1. What was tried and why it stopped
| try | result | why it stopped |
|---|---|---|
| OptiScaler 0.9.4 XeSS inputs -> FSR4 on Main PC (10-06) | FSR4 0.81 ms at 1440p (gpu-profile) | worked; never A/B'd against FSR3 at a lower REN_RAT |
| `Fsr4Update=false` (10-24, pc-crash-tracker) | set to isolate the DXGI popup | popup root cause later found elsewhere (OptiScaler ~5 %, dxgi-device-removed-rootcause.md) but the key was **never put back** |
| XeSS DP4a on Ally | 33.3 vs 34.2 fps (-1 fps) | slower than FSR3 on RDNA3 |
| DRS target 40 on PC (#123) | -5.7 %, inside noise | PC already under budget; engine never lowered the ratio |
| XRES 0.9/0.7 (shipped key, #67206) | PC +6 % (CPU cap ~118), pc2 +7 % | ships; not "same look" below 0.9 (GI/SSR/AO buffers shrink) |
| FSR3 fp16 ACCUMULATE rebuild (#91962) | est 0.2-0.5 ms Ally | near-look, needs human OK; not built |
| FSR4 INT8 on pc2/Ally (plan step 3) | **never measured** | blocked by optimization pause #70960 |

## 2. New finding (offline, verified from files)
- Game-dir `OptiScaler.ini` still has `Fsr4Update=false`; its own comment says "Enables updating of FSR3.X to FSR4, default true
  for RDNA4". So since 10-24 the Main PC's XeSS->OptiScaler path has most likely run **FSR 3.1 from the SDK DLL, not FSR4** (U until
  an arm's OptiScaler log shows it).
- Main PC `config.xml` right now: `RESOLUTION_SCALING=FSR`, `REN_RAT=50`, `REN_FPS=60` (test config, not the user's 75 + XeSS). On
  `FSR` the game's static FFX fork runs and OptiScaler is idle (it only takes XeSS inputs).
- `Fsr4ForceEnableInt8` exists in this OptiScaler build, so the pc2/Ally INT8 probe is config only.

## 3. Candidate (same look = FSR4 at a lower internal res matches FSR3 at the higher one)
No new code is needed for the first proof; the candidate is a per-device config:
- **PC (RDNA4):** `RESOLUTION_SCALING=XESS` + OptiScaler `Fsr4Update=true`, REN_RAT 75 -> 65.
- **pc2 / Ally (RDNA2/3):** same + `Fsr4ForceEnableInt8=true`; REN_RAT 65 -> 55 (pc2), 50 -> 40 (Ally). Stop if INT8 costs more than
  the render ms saved (~1.3 ms pc2, 1-2 ms Ally).
- An EngineFix FSR3->FFX4 bridge (plan step 4, 3-5 agent-days) only if the config arms win AND users must not need OptiScaler.

## 4. Harness (built, dry run OK)
`tools/testmatrix/upscaler_ab.ps1`: arms `NAME:FSR|XESS:RENRAT:off|fsr4|int8`, ABBA order, one abba2_pc launch per arm (-Arms BASE,
-Live none, vpad, MaxFps 240), OptiScaler logging per arm so each arm proves which upscaler ran; config.xml + OptiScaler.ini backed
up and restored. Default PC order F3@75 F4@75 F4@65 F4@65 F4@75 F3@75 (~6 launches, ~20 min). Also fixed `menu_map.py` (abba2
crashed on `hudhide::Installed()` in the current menu.cpp). Look check: shots per arm (fastab `-Shots` style) + human eye.

## 5. Live ABBA, Main PC (#95480, 2026-11-13 5:52-7:08 am EST, performance-pc/0)
RX 9070, 1440p, FG off, REN_FPS 120 for the run, EF ba089162 BR 8076cdd6 TR bcc4783e, OptiScaler 0.9.4, abba2 BASE x3 windows of 20 s, vpad, unfocused.
| L | arm | fps (s1/s2/s3) | p99 ms | GPU busy ms | CPU frame ms | Nioh3 CPU % (12 thr) |
|---|---|---|---|---|---|---|
| 1 | F3@75 FSR, Fsr4Update off | 113.2 / 111.4 / 118.3 | 11.8-12.8 | 6.31 / 6.31 / 6.28 | 8.0-8.6 | n/a (sidecar not yet built) |
| 2 | F4@75 XeSS->Opti fsr4 | 109.6 / 109.1 / 110.9 | 12.4-12.6 | 6.24 | 8.7-8.9 | 65.7 |
| 3 | F4@65 | 107.0 / 111.2 / 111.0 | 11.8-12.1 | 5.75 | 8.4-9.0 | 66.7 |
| 4 | F4@65 | 111.6 / 111.7 / 111.9 | 11.6-12.1 | 5.77 | 8.5 | 65.5 |
| 5 | F4@75 | 102.4 / 101.2 / 103.0 | 13.0 | 6.28 | 9.5-9.7 | 65.6 |
| 6 | F3@75 | 107.0 / 105.3 / 112.2 | 11.8-12.5 | 6.30 | 8.4-9.0 | 67.6 |
- **Verdict:** CPU-bound (CPU frame > GPU busy). F4@65 cuts GPU 0.53 ms (-8.5 %) with **no fps gain** here; F4@75 costs the same GPU as F3@75.
  Fps drift between launches (101-118) is CPU-side noise.
- **FSR4 unproven:** info log shows `CheckForGPU RDNA4: true, Fsr4Update: true` + FSR4ModelSelection hooks, no model-load line; equal GPU cost
  to FSR3 at 75 is suspicious (FSR4 0.81 ms in 10-06 gpu-profile). Next: one debug-log launch (not measured) to confirm the model loads.
- **Invalid run dropped:** OptiScaler LogLevel 1 wrote 566k lines in 4.5 min (~20/frame) = +1.8 ms CPU frame (96-102 fps). Harness now LogLevel 2.
- **Screenshots:** none. The game is unfocused during measuring; screen grab = desktop, PrintWindow/EnumWindows by PID = no visible window. Same-look check still open.
- Quiet guard: background 1.3-2.4 cores (steamwebhelper, Medal, Discord) kept the default threshold waiting 13 min; ran with QG_MAX_CORES=2.5 for all arms.
- Restored: config.xml cff22b4 (FSR / REN_RAT 50 / REN_FPS 60), OptiScaler.ini 7dcdeb5b, saves byte-identical, lock 381 released 7:10 am.
- Worth it only where GPU-bound: pc2 / Ally INT8 arm (performance-ally already tasked by pc/0).
