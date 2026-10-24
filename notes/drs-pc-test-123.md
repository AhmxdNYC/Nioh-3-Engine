# DRS target 40 A/B on pc (board #123)

Build: Nioh3EngineFix-1.7.73-drs-27690.asi (sha256 01f54b79...cede14), same ASI both arms, only [DynamicResolution] Enabled 0/1 (NativeTargetFPS=40).
Harness: tools/rematch/audit_screen_run.ps1 (Yoritomo R7B35 fight-now, 30 s warmup, 60 s PresentMon), FG off, VRR on, RX 9070, 2560x1440 XeSS.
Game config for both arms: DYNAMIC_RESOLUTION=60FPS, REN_FPS=120, REN_RAT=75, ratio min 35 / max 75 (max = REN_RAT, so the baseline picture equals the earlier bench).
Order OFF, ON, ON, OFF.

| run | arm | avg fps | 1% low | frames |
|---|---|---|---|---|
| 0101 | OFF | 87.58 | 69.77 | 5254 |
| 0102 | ON | 80.62 | 57.61 | 4836 |
| 0103 | ON | 77.37 | 57.12 | 4641 |
| 0104 | OFF | 79.93 | 61.38 | 4795 |

OFF mean 83.76 fps (11.96 ms), ON mean 78.99 fps (12.66 ms): ON -5.7 % / +0.70 ms, but the two OFF repeats differ by 9 %, so this is inside noise.
Counters (30 s lines): hook installed in all runs; ON overrides == calls (e.g. 23313/23313), OFF overrides 0; nativeMode=3, lastNative target 60.0,
boundsIndex 2..10; ratio index stayed 10 (75 %, 1920x1080) in every ON sample, OFF showed 10 with one 9 sample. The engine never lowered the ratio, so
target 40 had nothing to do: at ~80 fps the GPU is under the 60 fps budget already. No measurable gain; DRS target 40 does not help on pc in this scene.
Game speed ~1.0x (log OSD, 0.96-1.00 averages incl. load/title samples). No deaths/loads.
Raw: captures/drs-123/ (results-pc-drs3.csv, logs2/, logs3/, plan/). An earlier attempt ran without the DRS ASI installed (runner only validates -PerfAsi) and was discarded (captures/drs-123/invalid-v1).
