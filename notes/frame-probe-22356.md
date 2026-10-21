# Boss frame data from a live log-only probe (#22356 / #22310)

2026-10-21, pc, live in game (Train here / Yoritomo chr 16102, R7B35, menu Repeat move driven via `Nioh3Training.cmd`: `train R7B35`, `repeat A <id> 1`).
Code: `mod/Nioh3Training/src/frame_probe.h` (pure, `tests/frame_probe_test.c`), `frame_probe_game.h` (hook + pump tick). Parser: `tools/frame_probe_summary.py`.
Log extract (3 game runs, 16:44-16:58): `notes/frame-probe-22356.log`. Reproduce the table:
`py -I tools/frame_probe_summary.py notes/frame-probe-22356.log --csv <nioh3-engine>/notes/yoritomo-move-summary.csv [--runs]`.
Ini: `Nioh3Training.ini [Debug] FrameProbe` (repo default 0; read at start; 0 = no hook installed).

## How it measures
- Hook: hit-record setup `2B4A3C` (notes/yoritomo-hit-source-22305.md). Its entry is `test rcx,rcx / je rel32`, so the detour is at **`2B4A45`** (12 bytes
  `48 8B C4 48 89 58 10 4C 89 48 20 55`, checked with the 9 bytes before them; fail closed). A naked stub saves the caller's `rbx` = the attack collision
  object when the caller is `2B30A0` (return `2B30E2`), checked by `[obj+0xB0] == H`.
- **The attacker in `rdx` is the actor itself** (match kind 1 in every boss line), not `actor+0x3B0`. The first build compared `actor+0x3B0` and matched nothing.
- One `hit` line per collider creation. A window ends when the collision object leaves its pool slot (`[45C6F38]+0x20+id*8`, id found by scan).
  0x4E6E creates 8 colliders per swing (5 hitbox rows; 2 rows make 2 colliders each), so the parser merges overlapping windows into one move hit.
- Timing: QPC ms from the action-id change (seen by the per-frame pump or by the hook, whichever is first) -> 60 fps frames = ms x 0.06.
  The game ran at about 97 fps, so each edge has about +-1 frame of jitter. A start that is detected late or early (a forced repeat restarting) shifts
  startup but not recovery. The medians below ignore the two outlier runs (0x4E6E 72 f, 0x4E6C 121 f).

## Numbers (median, range in brackets; frames at 60 fps; startup = first active frame, 1-based)
| move | runs | hits | startup | active windows (frames since action start) | recovery | total | power | GreatAttack bit | static CSV windows |
|---|---|---|---|---|---|---|---|---|---|
| **0x4E6E** (unblockable double) | 18 | **2** (all 18) | **95** (94.9..96.4) | **94.0-105.7 (11.6 f)** and **141.2-156.9 (15.5 f)** | **177** (174..178) | 333 | 50 / 55 | 1 | 96-106, 143-157 |
| 0x4E6C (blockable variant, same motion 1050) | 5 | 2 (all 5) | 64 | 63.3-75.2 (11.9 f) and 110.5-125.8 (15.4 f) | 138 | 264 | 42 / 47 | 0 | 96-106, 143-157 |
| **0x4E4E** (1 hit) | 22 | **1** (21 of 22) | **49** (48.7..50.1) | **48.3-53.5 (5.1 f)** | 93 to idle | 146 | 40 | 0 | 52-58 |
| **0x4E50** (GreatAttack flag) | 9 | **0** (all 9) | - | none of its own | - | 141 | - | - | none (owns 0 rows) |
| 0x4E53 (what 0x4E50 sometimes chains into) | 3 | 2 | 53 | 51.6-56.5 (4.9 f) and 67.0-74.2 (7.2 f) | 12, then 0x4E54 | 86 | 30 / 42 | 0 | 54-80 |

## Plausibility
- **0x4E6E = 2 hits in every one of 18 runs**, with power 50 then 55 and hit bit 10 (GreatAttack) set. That is exactly the two row groups of the static data.
  The player's two HP drops (infinite-health refill lines, for example 2658->2250 and ->2159) come about 17 ms after each window opens.
  Spread across repeats: about 1 frame on every edge. **Consistent.**
- 0x4E6E live windows (94-106, 141-157) match the static `active_windows` column (96-106, 143-157) to within 2 frames.
  The static column is not a frame meter by itself, though. 0x4E6C has the **same** static windows (same motion) but runs about 31 frames earlier live.
  Its play-rate schedule (`sched_a`) is different. So the static frames are motion frames, and live ms (or static frames run through the play-rate
  schedule) give the real game frames. 0x4E4E also opens about 3 frames before its static 52.
- 0x4E50 has **no hit window of its own** in 9 runs, which fits the static data (it owns 0 hitbox rows). When the drill lets it continue, it goes into
  0x4E53, which hits twice (power 30, 42; GreatAttack bit clear). In most drill runs it goes back to idle after 141 f. Caveat: hits that take the
  other two callers (`1179220`, `23F2760`) with a non-actor attacker are not counted for a boss. The first 40 unmatched calls per launch are logged as
  `FRAMEPROBE diag`, and all of those came from world objects before the boss existed.

## Frame meter recipe (what the numbers mean)
startup = first window open frame; active = each merged window (open-end); recovery = next action - last window end. For a frame meter, the recovery
frames of a forced drill run end when the boss goes back to idle (0x2). In a real fight, the next action can cancel them earlier.

## State left / cost
- Installed `Nioh3Training.asi` = this branch's build (sha256 218d2fd0...). Backup of the pre-probe asi/ini: `nioh3-engine/captures/pc/play-20261021-preprobe/`.
  Boss Rematch asi unchanged (9836fe20...). Installed ini: `[Debug] FrameProbe = 1`.
- Cost: boss hook calls are about 1 to 2 per second while a boss attacks, and 0 to 3 per second in the field. Each call does a few pointer compares,
  plus a 4000-entry pool scan per boss collider. The pump does 4 slot reads per frame. The FPS overlay showed 97-99 at the shrine with it on, but no
  A/B was measured. The log gets about 40 lines per boss move while drilling (capped at 40 per second and 30000 per launch), plus one stats line per
  10 s when the call count changed. Left ON because the cost is not noticeable.
