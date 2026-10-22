# Frame meter / FRAMEPROBE on a second boss: Hino-enma (chr 83953, R23E5) + Reset fight boss Ki (#22356 / #22592)

2026-10-21, pc, live (Practice `train R23E5` via `Nioh3Training.cmd`, CPU stance = the boss's own AI, player infinite health, no drill).
Earlier notes: notes/frame-probe-22356.md (Yoritomo), notes/frame-fps-independence-22575.md, notes/practice-stats-22389.md.
Log extract (2 launches, 20:39-20:51): `notes/frame-probe-hino-22356.log`. Table: `py -I tools/frame_probe_summary.py notes/frame-probe-hino-22356.log --chr 83953`.
Static data: archive_01 `50/1C63D0293D7573AF46BDE49601C2705AD8B34FFC1E99B7DD3E0B395DA271E0` (the only action file with all 25 live-seen
action ids incl. 0x175C; 134 records), `tools/chr_hit_groups.py` / the per-move window code of `tools/yoritomo_move_summary.py`.
Screenshots (nioh3-engine `captures/frame-meter-20261021/`): `b4_meter_4E7D.jpg` (projectile volley, 2 hits), `b4_meter_4E6F.jpg` (fireball),
`b3_after_reset.jpg` (0x4E67, 3 hits), `b3_m3.jpg` / `b2_r1.jpg` (Reset fight).

## Bugs found and fixed
1. **Projectile hits were not counted** (Hino-enma's fireballs / volleys: 0x4E6F, 0x4E7D, 0x4E86, 0x4E54 ...). They call 2B4A3C with a type-4 object
   (`[x] & 0xF == 4`, `+0x260` = the boss actor), not the actor / AI / ctrl the probe matched. Fix: `FpOwnerChar` (frame_probe.h) = the game's own
   attacker resolver 325F4C (type 0 itself, 4 -> +0x260 (type 3 -> +0x1E0), 2 / 6 -> `[[x+0x240]+0x690]`), match kind 4. After it: 0 unmatched boss
   projectiles; the only unmatched calls left are the player and one other character (the 40 diag lines, budget now reset per boss).
2. **Zero-damage colliders showed as hits**: power 0 / 0, type 12 collider at frame 0 of the steps 0x4E20-0x4E22 / 0x3EF (static: those actions own
   no rows; the same row object is used by a non-boss character) and power 0 / 0 type 65535 rows of 0x4E73 (static rows 0/0/65535 too). The meter
   showed 'hits 1, startup 1 / total 1' on every step. Fix: `FpNoDamageRow` (power +0x10 and element +0x12 both 0) = not a window, not on the meter;
   logged as `FRAMEPROBE nodmg` (first 3 per boss). Every damaging hit seen on both bosses has power > 0.
3. **Windows that outlive their action** (chains 0x4E5F -> 0x4E60, 0x4E62 -> 0x4E63, 0x4E8C -> 0x4E8D, 0x4E6A -> 0x4E6B): the run line said
   'active 0 f' (end never seen) and the meter stopped drawing them as active at the id change. Fix: `FpRunCut` ends such windows at the action end in
   the run line (`cut at the action end N`); the meter counts every open window of the boss, also one opened by the previous action.
   Parser fix (`tools/frame_probe_summary.py`): a hitend of the previous action no longer lands on the next run's own window with the same index
   (0x4E8D had shown 'active 0-98, recovery -95').
No crash in 3 launches; no rate-limit drops; ~2 FRAMEPROBE lines/s while the boss fights (1075 lines in ~9 min).

## Hino-enma numbers (game-time frames at 60/s, median; startup = first active frame; live windows = frames since the action start)
| move | runs | live hits | startup | active windows | recovery | unblockable live (h13C bit 10) / static (+0x04 0x2000) | static own rows / windows (motion frames) |
|---|---|---|---|---|---|---|---|
| 0x4E67 | 2 | **3** | 50 | 48.6-69.6, 83.5-92.5, 125.6-137.8 | 43 | 0 / 0 | 3 rows (35/0/12), 1 hit id, 60-147 |
| 0x4E7D (volley, proj) | 6 | **2** | 54 | 53-61, 89-99 | 77 | 0 / 0 | 1 row + projectile, - |
| 0x4E6F (fireball, proj) | 16 | 1 | 75 | 73.5-77.6 | 22 | 0 / 0 | 0 own rows + projectile |
| 0x4E65 | 6 | 1 | 45 | 44.1-80.1 (36 f) | 97 | 0 / 0 | 3 rows, 45-79 |
| 0x4E60 | 5 | 1 | 23 | 22.1-32.8 | 53 | 0 / 0 | 2 rows, 23-32 |
| 0x4E88 | 4 | 1 | 30 | 29.4-45.4 | 145 | **1 / 1** | 1 row, 40-54 |
| 0x4E62 -> 0x4E63 -> 0x4E64 (grab chain, power 58 type 4) | 4-5 | 1 each | 73 / 1 / 8 | 72-80 / 0-16 / 7-10 | - / - / 92 | **1 / 0**, 1 / 1, 1 / 1 | 1 row each: 73-80, 0-14, 8-9 |
| 0x4E79 -> 0x4E63 | 1 | 1 | 73 | 71.9-77.2 | - | **1 / 0** | 3 rows, 73-92 |
| 0x4E8C -> 0x4E8D | 5 | 1 + 1 | 92 / 1 | 91.4-97.9 / 0-2.9 | - | 0 / 0 | 86-105 / 0-69 |
| 0x4E50, 0x4E5F, 0x4E61, 0x4E66, 0x4E68, 0x4E69, 0x4E6A, 0x4E6B, 0x4E78, 0x4E7F, 0x4E8A, 0x4E8B, 0x4E8E | 1-8 | 1 | 31, 34, 50, 47, 49, 7, 30, 40, 1, 72, 39, 33, 1 | see the table | | 0 / 0 | |

## Plausibility
- Hit counts: every melee move = its static row-group count or its row count (0x4E67: 3 rows of one hit id -> 3 live swings; the static 'hit id'
  count says 1, so for this boss the row count is the better predictor). Projectile moves have 0-1 own rows; their hits only come from the shots,
  which are now matched (0x4E7D = 2 volleys every run).
- Windows: live startup vs static motion frames: 0x4E65 45 vs 45, 0x4E60 23 vs 23, 0x4E62 73 vs 73, 0x4E78 1 vs 1, 0x4E88 30 vs 40 and 0x4E8C 92 vs 86
  (play-rate schedule, same effect as Yoritomo 0x4E6C, notes/frame-probe-22356.md).
- Unblockable: 0x4E63 / 0x4E64 / 0x4E88 = GreatAttack in the static file and live. **0x4E62 and 0x4E79 are 0 in the static file but their last
  window is live-flagged**: both chain into 0x4E63 (the grab hold), and the window that opens on their last frames (power 58 type 4, the same row data
  as 0x4E63) already carries the GreatAttack bit. The meter follows the live hit bit, which is what the game uses, so it shows that window purple.
- No missing hits seen: the player's infinite-health refill lines follow the matched windows; non-matched calls are the player / one other character.
- Several actors: Hino-enma is one actor plus shots; a second Training boss slot is followed by the meter only when slot A is empty (unchanged).

## Reset fight: boss Ki 'WRITE DID NOT STICK' (fixed)
- Cause: KiSetCur 2F88D0 clamps the value to `max - [K+0x88]` (K = st+0x98, so st+0x120), and the native regen (2F83A0..2F8400) stops at the same
  ceiling. That field is 0 at spawn and grows with the player's hits (live: light hits on Hino-enma -> 2.91, then 5.81; cur stuck at 1661.1 / 1664 for
  minutes; cleared by hand -> regen back to 1664). So a 'refill to 1664' read back 1661 = 'nearly full' bar.
- Fix (`BvWriteKi`, boss_vitals_game.h; pure `BvKiCeilClear`, tests/boss_vitals_test.c): when the target is above the ceiling, set the loss to 0 (the
  spawn state) first. The log line now prints the read-back and the cleared loss.
- Live (real hits, loss 5.81): `BOSSVITALS Boss A ...: Ki -> 1664.0 / 1664.0 (Set boss Ki %, Ki ceiling loss 5.8 cleared), read back 1664.0`;
  memory after: cur 1664 / max 1664, loss 0. Also with an injected loss 30 / cur 1400: read back 1664. Infinite boss Ki and Set boss Ki % use the
  same writer, so they no longer fight the ceiling either.

## State left
- Installed Training d58a8e7c (this branch), Rematch 4c48cc79 unchanged. Backup of the previous asi / ini: nioh3-engine `captures/pc/play-20261021-pre2ndboss/`.
  Nioh3Training.ini restored byte-identical (`[Debug] FrameProbe = 0`, `[Practice] FrameMeter = 1`). Game running (PID 31928), no Training session.
