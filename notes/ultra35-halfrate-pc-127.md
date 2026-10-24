# Ultra Performance 35% and half-rate reuse on pc (board #127)

Setup: pc, RX 9070, 2560x1440 XeSS, FG off, REN_FPS 120, Yoritomo R7B35 fight-now, 30 s warmup + 60 s PresentMon, order OFF ON ON OFF, FG off, saves/ASI/INI/config.xml backed up and restored (hash-verified).
Harness: tools/rematch/audit_screen_run.ps1 driven by captures/test127/run127.ps1 (installs the ASI itself, 11 min watchdog, startup-log gates per run).

## Test 1: Ultra Performance 35% (file 1772, sha256 5709E939...1123, build 1.7.73-xlod4-ultra35-27751)
OFF = UltraPerformance 0 with fixed REN_RAT 50 (DYNAMIC_RESOLUTION OFF), ON = UltraPerformance 1. Log gate per run: "ULTRA35 ... requested=0/1 result=0/1". Same ASI, only the ini key differs.

| run | arm | avg fps | 1% low | GPU active ms (PresentMon msGPUActive) | game speed (log) | valid |
|---|---|---|---|---|---|---|
| ultra-001 | OFF | 91.81 | 58.72 | 7.77 | ~1.00x | yes |
| ultra-002 | ON | 91.20 | 56.75 | 6.94 | ~1.00x | yes (first launch hung 11 min, watchdog killed it, redone) |
| ultra-003 | ON | 87.30 | 55.38 | 7.33 | ~1.00x | yes |
| ultra-004 | OFF | 88.86 | 59.54 | 8.16 | ~1.00x | yes |

OFF mean 90.34 fps (11.07 ms), 1% low 59.13, GPU 7.97 ms. ON mean 89.25 fps (11.20 ms), 1% low 56.07, GPU 7.14 ms.
ON vs OFF: avg fps -1.2 % (+0.13 ms frame time), 1% low -5.2 %, GPU active -0.83 ms (-10 %). Repeat spread is 3-4 %, so the fps/1% change is inside noise; no fps gain. The game is CPU/pacing bound here (GPU active 7-8 ms vs 11 ms frame time), so cutting GPU work does not raise fps on this PC at this scene.
Render dimensions: unverified (not sampled). The only hint is the GPU drop of about 0.8 ms, which matches the 50 to 35 % pixel-count step scaled from the 75 to 50 % step (controls at REN_RAT 75 ran 9.3-10.5 ms GPU). config.xml read back after the game closed still said REN_RAT 50 in ON runs (the game rewrites it on exit, or the early write did not take), so treat the 35 % as unproven. Power: not recorded.
Verdict: no fps benefit on pc; do not ship as a performance option for pc (may still help GPU-bound Ally, untested here).

## Test 2: half-rate HRS / HRG / HRC (file 1780, sha256 BE2FFBF0...0922): NOT MEASURABLE, game hangs at boot
The 1780 ASI (1.7.61 base, log "HalfRate: installed ... HRS 0 HRG 0 HRC 0") never lets Nioh3 create a window: the game process sits at ~9 GB, frame counter never advances ("engine frame counter did not advance within 300094 ms"), no render. Tried 6 launches with the baseline ini (all three keys 0): all hung. The installed 1.7.73 ASI with the same ini boots and runs fine (control, below), so the ini/config are not the cause.
Port check: eccb0650 cherry-picked onto the 1.7.73-drs source (cx-pc/drs-matched-27690, menu rows dropped, ini keys only), built locally (sha256 5815E404...F236, not pushed): it hangs the same way in 6 launches. A 1.7.73-drs/1.7.73 build without half-rate boots at ~88-93 fps with the identical ini. So the halfrate entry hooks (halfrate.cpp Start, FrozenWrite of 4 call sites) block boot on pc even with every key at 0.
Control (no half-rate, installed 1.7.73 ASI, hr baseline ini): 87.72 and 92.58 fps, 1% low 59.15 / 68.63, GPU active 10.5 / 9.3 ms at REN_RAT 75.
Result for HRS, HRG, HRC: no OFF vs ON data. Next step is a fix by gx-pc (suspect the thread-freeze patch at boot plus the near code page), not another test.

## Restore status
Game ASI sha 04F8D822..., ini A4F221F3..., config.xml 8C345A0B... and the save folder are all back to the pre-test hashes; test-active marker removed; no Nioh3 process left.
Raw: captures/test127/ (results-pc-ultra.csv, results-pc-ctrl.csv, results-pc-hr.aborted*.csv, logs/, wrapper.log), PresentMon CSVs in captures/audit-screen/run-pc-20261023-22*/ and -23*/.
