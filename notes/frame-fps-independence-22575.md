# Are the FRAME METER / FRAMEPROBE frame counts independent of fps? (#22575)

2026-10-21, pc, live in game (Train here / Yoritomo chr 16102, R7B35, `repeat A 4E6E 1` via `Nioh3Training.cmd`). Code: `mod/Nioh3Training/src/frame_probe.h`,
`frame_probe_game.h`, `frame_meter_game.h`; parser `tools/frame_probe_summary.py` (new `--wall`). Earlier numbers: notes/frame-probe-22356.md.

## What the "frame" was (before this change)
Real time: QPC ms since the action start x 0.06 (`FpFrames60`, and the meter's `g_fm_clock` = real time between pump ticks). It is **not** game time.
It equals game time only while the game runs at 1.00x. The stock engine steps the game by a fixed `1/cap` per rendered frame (notes/frame-timing.md),
so below its fps cap (30/60/120) the whole game runs in slow motion and real-time counts grow. With the Engine Fix VRR on (this PC) it runs at 1.00x.

## What it is now
Game time: per pump tick, `ticks (G+0x610 delta) x 1000 x time scale (G+0x624) / fps-for-dt (G+0x614)`, G = `[4759910]`. This is the game's own
step, so 1 f = 1/60 s of game time at any fps and any slow-down. Pause (scale 0) stops it. The log still prints the real-time value of every edge
next to it ("wall ..."). The stats line logs `game/wall` (1.00 = real speed). Pump ticks = game ticks in every arm, and the fallback was 0.

Second fix: at a 30 fps cap the game makes **84-108 colliders per 0x4E6E swing** (new ones on each 33 ms step) instead of 8. The old per-collider list
(32) and the open-window table (24) overflowed, so the second hit was lost ("hits 1"). Now the probe merges colliders into hit windows itself:
a collider joins the last window while one of its colliders is open, or within 17 ms of its end. That is the same rule as the parser and the meter.
`FP_WIN` is now 128. Hit lines are logged per window (plus the first 12 colliders), and hitend lines once per window.

## Measurements (0x4E6E, median of the clean runs (`-> 0x2`), frames since the action start)
| arm | fps (fps-for-dt) | game/wall | clock | startup | hit 1 | hit 2 | recovery | total |
|---|---|---|---|---|---|---|---|---|
| cap 120 + VRR (normal PC setup), build 1 | ~95-116 | 1.000 | game | 95.0 | 94.0-105.9 | 140.9-156.7 | 176.8 | 333.2 |
|  |  |  | wall | 95.5 | 94.5-105.9 | 141.4-156.8 | 176.8 | 333.3 |
| cap 120 + VRR, final build | ~87-100 | 1.000 | game | 96.8 | 95.8-105.7 | 140.8-156.9 | 175.8 | 332.1 |
| cap 60 (holds 60) | 60 | 1.0005 | game | 94.0 | 93.0-106.0 | 140.1-157.1 | 167.1 | 324.2 |
|  |  |  | wall | 94.8 | 93.8-106.0 | 140.8-157.0 | 167.0 | 324.0 |
| cap 30, merged build (3 clean runs) | 30 | 1.000 | game | 95 | 94-108 | 138-158 | 166 | 324 |
| **cap 120, VRR = 0 (stock engine), renders ~88 fps** | 120 | **0.73** | **game** | **97.0** | 96.0-106.0 | 141.0-157.0 | 167.0 | **324.0** (648 ticks) |
|  |  |  | **wall (old unit)** | **129.7** | 128.7-142.1 | 191.3-213.7 | 226.6 | **441.6** |

Raw: `captures/fps-22575/arm*.log` (nioh3-engine). Reproduce: `py -I tools/frame_probe_summary.py <arm log> --chr 16102 [--wall]`.

## Verdict
- **The old unit was not truthful at every fps.** It was right only when the game ran at real speed (Engine Fix VRR on, or the fps holding the cap).
  On a stock engine below the cap it inflated every number by cap/fps (here +36%: startup 130 instead of 97, total 442 instead of 324).
- **The new unit is game time and stays the same across fps.** Second hit 140-141 .. 157 and its 16 active frames at 30, 60, ~90 and stock 120.
  Total 324 at caps 30/60/120 stock. Each edge is quantized to one game step: +-0.5 f at 120, +-1 f at 60, +-2 f at 30.
  This is the same as in a fighting game that runs at a lower tick rate: the game itself only checks hits once per step.
- Real game differences that remain (not clock errors; game and wall agree in those arms):
  - With the **Engine Fix VRR (variable dt, ~90-110 fps) the action lasts ~9 f longer** (total 332-333, recovery 176 against 324 / 167 elsewhere). The
    boss AI's next pick comes ~145 ms later (REPEAT log: 5.55 s against 5.41 s). This is probably the VRR tick carry or the TG step, so it is a VRR fidelity
    issue to check in the Engine Fix, not a meter issue.
  - The first hit opens 93-97 f depending on the cap (60: 93, VRR: 94-96, stock 120: 96). The second hit and the total agree.
  - Hit-stop: the boss main motion play rate (`[a+0x40]+0x324`) never read 0 during 0x4E6E in any arm (`boss stopped 0.0 f`, logged per run), even on
    runs where the player was hit. So no boss-side hit-stop was seen. Global slow / pause (time scale) is handled by the game clock.
  - At a 30 fps cap, Repeat move often fights the boss's step motion 0x3F1 (`0x4E6E <-> 0x3F1` every tick for a while), so only 3 of 49 runs were clean.
    This is the Training Repeat behavior at 30 fps, not the meter, and it is not fixed here.
- The start-detection outlier (a forced repeat restarting; startup ~71-73 and everything shifted ~24 f) still happens at every fps (one or two per 12 runs).
  The meter shows it as is.

## State left
- Installed `Nioh3Training.asi` a4e7da0f (this change). Backup of the previous asi/ini/EngineFix ini/config.xml: `nioh3-engine/captures/pc/play-20261021-prefps/`.
  Boss Rematch 84cbf650 unchanged. config.xml (REN_FPS 120) and Nioh3EngineFix.ini (VRR = 1) are byte-identical to the backup.
- Installed ini `[Debug] FrameProbe = 0` (it was 1). The running game still has the probe on until its next launch. `[Practice] FrameMeter = 1` unchanged.
