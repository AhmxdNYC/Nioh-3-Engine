# Hit-label accuracy: Yoritomo (chr 16102), Training, 2026-10-27 (human #41677/#41681/#41687)

Real game on pc. Installed: Training ASI 958464 B (6:57 pm), EngineFix 1.7.85, chain-aware TSV
`Nioh3Training_action_records.tsv` sha256 5ddf6861a610.

## How the run was done
- Driven through the mod menu with real keyboard SendInput (`D:/nioh3-wt/acc/kb.ps1`, a copy of the clips job's `kb.ps1`).
  Path: F10 > Practice > Reset fight > Training Bosses > Boss A > `<hit group>` > move > Repeat ON > close.
  Stance: Stand still + repeating a move, gap 1.25 s, Aggro on me ON, player infinite health ON. Run 2 also had boss infinite Ki ON.
- The frame meter was OFF (Display Overlays > Frame meter and Player frame meter). The INI was restored afterwards.
- Picks: 3 random moves per group, `random.seed(41677)` over the menu index.
- Counting: real HP-damage calls on the player, one `IFRAMEHP ... raw N` line per hit (`[Debug] IFrameProbe = 1`). Each
  rep runs from the forced move's ACTLOG start to the next action. Hits that land during a follow-up attack started by
  the move are counted separately as chain hits.
- **Log cap found:** run 1 counted `TRAIN infinite health: hit of N` / `(refilled)` lines. Those stop after 20 / 50
  lines per session (training.c:4267 / 1425), so later zeros in run 1 were not real. The game was restarted with
  IFrameProbe = 1 (cap 1500) and the affected moves were measured again. Calibration: 0x4E59 gave the same 738 + 757 pair in both runs.
- Reps with 0 hits after the boss drifted out of range are listed as "whiff" and left out of the per-move number.
- Count tool: `D:/nioh3-wt/acc/count3.py`. Session log: `D:/nioh3-wt/acc/Nioh3Training-acc.log`.

## Results
| Menu label | Move | TSV hit_count (damage per hit) | Player hits taken when drilled alone (reps) | Follow-up chain seen (extra hits) | Verdict |
|---|---|---|---|---|---|
| 1 hit | 0x4E59 | 1 (175\|180) | **2** every rep (7 + 10 reps, 738 + 757) | 0x4E5E (+0) | under-labeled: a '1 hit' move lands 2 |
| 1 hit | 0x4E7C | 1 (0, zero_power) | 0 (7) | sometimes 0x4E42 (+0/1) | over-labeled: its only collider has power 0 |
| 1 hit | 0x4E42 | 1 (100) | 0 (10 run 1 + 5 rerun) | 0x4E24 / 0x4E43 | over-labeled: hit window present but never connects when drilled |
| 2 hits | 0x4E69 | 2 (125/200) | **2** (14 reps; 2 reps landed 1; run 1, before the log cap) | none | accurate |
| 2 hits | 0x4EA2 | 2 (155/0, zero_power) | **1** (8) | none | over-labeled: 2nd hit is power 0 |
| 2 hits | 0x4E81 | 2 (90/125) | 0 (30) | none when drilled | over-labeled: no hit window in the drilled action (0.78 s wind-up only) |
| 3 hits | 0x4E8E | 3 (150/100/140) | **1** (6 of 7; one rep 2) | none | over-labeled |
| 3 hits | 0x4E95 | 3 (225/150/150) | **1** (5 of 6) | none | over-labeled |
| 3 hits | 0x4EA8 | 3 | **3** (3 of 5; others 1, 2) | none | accurate |
| 4 hits | 0x4E64 | 4 (200\|245/175/115/175) | **2** (6, then 3 whiffs) | none | over-labeled |
| 4 hits | 0x4EA0 | 4 (120/120/155/0) | **1** (5 + 3 whiffs) | sometimes 0x4EA2 (+0/1) | over-labeled |
| 4 hits | 0x4EA1 | 4 (120/120/155/0) | **1** (3 + 2 whiffs) | sometimes 0x4EA2 (+0/1) | over-labeled |
| 5 hits | 0x4E5C | 5 (140/150/150/205/0) | **1** alone (7) | 0x4E60 > 0x4E62 in 7/9 reps, **total 3** (once 2) | over-labeled (5 vs 1 alone / 3 with chain) |
| 5 hits | 0x4E6C | 5 (210/235/175/115/175) | **2** (6) | none | over-labeled |
| 5 hits | 0x4EAC | 5 (170/185/175/115/175) | **2** (3), 8 colliders | none | over-labeled |
| Unblockable [hits unknown] | 0x4E50 | 0 | 0 (8), no hit window | none | label is honest ('unknown'); move does nothing when drilled |
| Unblockable [hits unknown] | 0x4E6A | 0 | 0 (6), no hit window | none | same |
| Unblockable [hits unknown] | 0x4EAF | 0 | 0 (4), no hit window | none | same |
| Unblockable [2 hits, unblockable] (extra) | 0x4EAE | 2 (170/200) | **2** (3) | none | accurate |
| Other attacks (hits unknown) | 0x4E5B | - | 0 (4), no hit window | none | n/a (no number shown) |
| Other attacks (hits unknown) | 0x4E65 | - | 0 (5), no hit window | none | n/a |
| Other attacks (hits unknown) | 0x4EB8 | - | 0 (5) | 0x4EB9 every rep (+0) | n/a |

## Per-label verdict
- **1 hit** (3 moves): **inaccurate**. 0x4E59 is labelled 1 and lands 2 on every rep, which the request defines as the
  algorithm being inaccurate. The TSV stores it as one hit with damage `175|180` (a '|' joined pair); in game those are
  two separate HP hits (738, 757). The other two land 0 when drilled (0x4E7C has power 0; 0x4E42 never connects).
- **2 hits** (3): mixed. 0x4E69 is accurate. 0x4EA2 lands 1 because its 2nd hit is power 0. 0x4E81 lands 0.
- **3 hits** (3): mostly over-labeled. 0x4EA8 = 3 (accurate); 0x4E8E and 0x4E95 land 1.
- **4 hits** (3): over-labeled. Real counts 2 / 1 / 1. 0x4EA0 and 0x4EA1 include a trailing power-0 hit (`.../0`).
- **5 hits** (3): over-labeled. Real counts are 1 (3 with its chain) / 2 / 2.
- **Unblockable** (3 random): all '[hits unknown]', 0 when drilled (no hit window). The extra 0x4EAE '[2 hits]' is accurate.
- **Other attacks** (hits unknown): 0 when drilled. No number is claimed.

## What this points at (not changed in this run)
1. A `|` pair in damage_per_hit (e.g. `175|180`, `200|245`) is counted as **one** hit, but in game it lands as **two**. That gives the under-label.
2. Zero-power rows (`/0`, zero_power = 1) are counted as hits. They should not be.
3. Many 3/4/5-hit rows count more colliders than hits that connect: a sweep of several colliders hits the player once.
   The real count is closer to the number of damage events (one per collider group) than to the number of colliders.
4. Some moves are follow-ups (0x4E5C chains to 0x4E60 > 0x4E62; 0x4EA0/0x4EA1 chain to 0x4EA2). Drilled alone they
   land fewer hits than in a real fight. This matches the earlier 0x4E4E/0x4E55 finding.

Clips: `D:/nioh3-wt/acc/clip1-4E59-1hit-label-takes-2.mp4` (34 s) and `clip2-4E5C-5hit-label-1-alone-3-with-chain.mp4` (36 s).

## Part B (Save files menu)
The installed EngineFix (1.7.85, `[SaveFiles] Probe=1 Enabled=0`) contains the Save files strings ('Back up after Quit',
'Queued COPY slot%d'). But at the title screen the mod menu has no Save files row on any page (screenshot
`D:/nioh3-wt/acc/B-title-modmenu-no-savefiles.png`). It is not reachable in the installed build, so the video was skipped.
