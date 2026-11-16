# Dojo rows in Boss Match: root cause per stage (#89218)

Offline only. No game launch, nothing installed. Source read: origin/cx-pc/open-world-89086 a1e91c19 (read worktree
Temp/wt-dojo89218). The checked-out branch cx-pc/open-triage is stale: its rematch.cpp has no lesson code, so its line
numbers do not match the current mod. Scripts were disassembled with re/scripts_re/lfm_disasm.py --calls from
data/extract/lfm (outputs in %TEMP%/dj89218/*.calls.txt). About 25 minutes of work. Labels: PROVEN means a log or a script
listing shows it. INFERRED means it follows from the code or script but has not been seen live.

## 1. The 9 rows and what the catalogue picks now

Dojo membership = mission names "The Way of the Warrior / Ninja / Onmyo" (cx-pc #84676). The table below is from
Nioh3BossRematch_bosses.tsv on a1e91c19.

| id | name | script file (12) | lesson col | placement | fight start (pick_fight_start) | gate (pick_gate) |
|---|---|---|---|---|---|---|
| RF5E5 | Yagyu Munenori (Novice) | A96CE28F8E68 | 1 | - | none | none |
| R6E65 | Yagyu Munenori (Adept) | 937889AF5AA9 | 0 | 5017215 | mission start 5014239, 10 m, high | none |
| R1A5E | Yagyu Munenori (Veteran) | BFC230194A06 | 0 | 5027806 | mission start 5021112, 10 m, high | none |
| RD759 | Hattori Hanzo (Novice) | E7923507DECC | 1 | - | none | none |
| RE18C | Hattori Hanzo (Adept) | 8C8BDE81D976 | 0 | 5043775 | mission start 5044637, 10 m, high | none |
| R9BE4 | Hattori Hanzo (Veteran) | 96A326DD1D0A | 0 | 5051552 | mission start 5059114, 10 m, high | none |
| R26F3 | Saito Fuku (Novice) | 23B6AC585823 | 1 | - | none | none |
| RA0A1 | Saito Fuku (Adept) | 0AD2EFD76D68 | 1 | - | none | none |
| R2826 | Saito Fuku (Veteran) | 699A2C366549 | 0 | 5084399 | mission start 5084780, 11 m, high | none |

There are two classes, and the script decides which (PROVEN, _script_is_lesson() on these 9 files):
- **Duel dojos (5):** R6E65 R1A5E RE18C R9BE4 R2826. The entry calls the teacher flow. That flow waits until the player is
  in a small start box or the teacher takes damage, then calls `Mission::BossBattleMode(teacher)` and loops on
  `Duel::IsAliveBoss`. When the teacher kneels it calls `EndMissionClearState`, then `BossBattleModeEnd`, then
  `WaitClearGadgetAccess(gadget, teacher)`, so Proceed means talking to the kneeling teacher. pick_fight_start uses
  the single start point (the len(st)==1 rule). No gate is needed.
- **Lesson dojos (4):** RF5E5 RD759 R26F3 RA0A1. The entry calls `Function::ZakoRushCameraFocus([g1,g2,g3,...],
  [x,y,z], [...])`. It runs `CreateGroupEnemy`/`IsDeathGroupEnemy` for each wave group, then `EndMissionClearState`,
  `BossBattleModeEnd`, `RequestMissionClearEffect`, `Gadget::CreatePlacementGadgetPosition(gadget, x,y,z)` and
  `UI::SetCompassDestinationIndex(x,y,z)` (this is the white dot), then `SetMissionClearFlag`/`CreateClearGadget`. The
  script **never calls BossBattleMode**. The catalogue has no boss placement for these rows (no boss-like enemy record in
  rematch-enemy-index.csv), so `mrow()` never calls pick_fight_start or pick_gate. Every launch field is "-".

Values decoded from the lesson entry arguments (PROVEN, script listing):

| row | wave groups | clear gadget / white dot (x y z) | start point used live |
|---|---|---|---|
| RF5E5 | 50001, 50002 | 76135 3555 55802 | 5001735 (76529 3555 56560) |
| RD759 | 50301, 50302 | 76135 3555 55802 | Training start (same spawn) |
| R26F3 | 50601, 50602 | 76494 1270 37765 | 5062689 (75515 1271 39273) |
| RA0A1 | 50701, 50702, 50703 | 76494 1270 37765 | 5070983 (75515 1271 39273) |

The waves start when the player enters `IsEnterBox` (76520 3700 56555, size 50/300/50; Fuku arena 75515 1390 39280), or
when an hp check fires. The start points above are inside those boxes.

## 2. Per stage

### (1) Load: works for all 9 (PROVEN)
- 1.8.12 sweep #87276 (Temp/sweep87237/c1/Nioh3BossRematch.log). RF5E5, R26F3 and RA0A1 log `SELFTEST ... PASS`
  (14.5-15.9 s, 0 warps). The landing equals the requested start. `RF5E5 had no start point in the catalogue; using
  Nioh3Training's game-data start 5001735`.
- RE18C and R9BE4 reach the fight (#74804). R6E65 had a real win (checkpoint 3:00 am 11-06). RD759 loads and is covered
  (#323, the cover drops in 16 s).
- Weak spot (INFERRED): lesson rows load only because of the Training fallback in rematch.cpp ("#65732 bug 3 ... a catalogue
  row without a start point (dojo Novice rows) takes it from Nioh3Training's arena table"). The catalogue itself holds
  nothing for these rows. That fallback is the per-class special case the universal rule should replace.

### (2) Enemy spawn: works by game design. Nothing is broken (PROVEN)
- Duel dojos: the teacher is placed at mission start (catalogue placement plus boss_pos about 4 m from the start).
- Lesson dojos: the waves are script-created groups (table above). Earlier logs show waves visible and both waves dying
  (RD759 #76208: wave 2 at 2:00:55). `lesson group 5xxx01 checked, teacher fight not started` is the first wave group
  being polled, not a failure.
- The earlier "spawn" failures were caused by tests (PROVEN by the script):
  - The forced kills in #71936 targeted 5032286 and 5038451. Those are the `enemy_group_b` actors the script holds with
    `AlgoActiveFlag F` (demonstrators). They are not wave groups 50301/50302, so killing them cannot advance
    `IsDeathGroupEnemy`.
  - The RA0A1 "forced kills did not complete" failure (#75395) has the same likely cause (INFERRED).
- Separate issue: the E00 "0 enemies + 0x2332" result is the scroll bug (0x2332 = shrine-menu pose, #76364). It is not a
  dojo cause. KeepGear was ruled out (#330, #76385).

### (3) Fight start / Proceed: the real failure, lesson rows only
- Root cause, part a (PROVEN by script, consistent with logs). Lesson scripts never call `Mission::BossBattleMode`. The
  mod's fight start (FIGHT group: BossBattleMode hook, boss bar, killid) has nothing to catch. So a lesson row never shows
  FIGHT, a bar or a win. Today that is masked by the "Lesson" label and the footer line `kLessonLine`.
- Root cause, part b (PROVEN by script plus #76459/#76524). Proceed is the game's normal clear-gadget access. The script
  creates the gadget at a position given in the script (76135 3555 55802 for Yagyu/Hanzo), not where the teacher or the
  player stands. Live tests found YES at about x 76000-76300, z 55720-55920 and NO at 75941,55615, which brackets the
  decoded point. "Depends where you stand" is the clear gadget's interaction range. It is not a mod bug.
- Duel dojos: BossBattleMode(teacher) fires once the player is inside the 1 m start box at the start point, or hits the
  teacher. Proceed = `WaitClearGadgetAccess(gadget, teacher)` at the kneeling teacher, which is normal duel behaviour
  (PROVEN script). Live: R6E65 won, RE18C and R9BE4 reach the fight. R1A5E and R2826 are not named in any live result
  I found (UNPROVEN).

### (4) Fight end
- Lesson rows: the script calls `Mission::BossBattleModeEnd()` with no matching BossBattleMode, then sets the clear flag
  when the gadget is used. Live RD759 #76208 logged `BossBattleModeEnd` and the mod gate page (end-mission box BLOCKED) on
  hold RB. Leave Boss Match returned home in 17 s with the reward. So the finish path works once the player reaches the
  gadget (PROVEN, 1 run main PC plus 1 Ally).
- What is missing (INFERRED from code): Training's `End` hook (training.cpp:84, `OnEntry("BossBattleModeEnd", &End)`)
  and the mod's win bookkeeping key off an active boss fight. An unpaired ModeEnd therefore does not count as a Boss
  Match win. That is why these rows can never become "confirmed" in the filter.
- Duel rows: the teacher kneels and is not killed (`Duel::IsAliveBoss` false, `CurrentMissionEnemyFightingSpiritLoss`).
  If the win path needs a killid/death, it could miss the end. R6E65 recorded a real win, so the ModeEnd path covers it
  (PROVEN for 1 row).

## 3. Proposed universal rule (no per-row ids)

All in tools/rematch_catalog.py. Each rule is keyed on script/mission-table data that any future mission has.

1. **Encounter kind from the script** (extends `_script_is_lesson`). While walking the entry's call graph, also record
   - `fight_signal`: "boss" if `Mission::BossBattleMode` is reached, else "waves" if `IsDeathGroupEnemy` is reached.
   - `wave_groups`: the int args of the CreateGroupEnemy/IsDeathGroupEnemy chain. For flow helpers like
     ZakoRushCameraFocus these are the entry call's first array, nonzero values below 0x10000.
   - `clear_pos`: the xyz literal handed to `Gadget::CreatePlacementGadgetPosition` /
     `UI::SetCompassDestinationIndex`. In these scripts it is the entry call's 3-float array. A general walker should
     resolve the v1.7-v1.9 parameter slots back to the entry call's arguments.
2. **pick_fight_start without a boss placement.** If `pid` is empty and the mission start table has exactly one point,
   return it as "mission start". Conf "high" if `fight_signal == "waves"` and the point is inside the script's first
   `Player::IsEnterBox`, else "medium". Distance is measured to `clear_pos` or the box instead of the boss.
   `mrow()` must call pick_fight_start/pick_arena when `pid` is empty and the script gives a box. This removes the
   Training "game-data start" fallback for these rows.
3. **pick_gate:** unchanged. The single-start-point rows need no gate. Both dojo classes start inside the IsEnterBox, so
   the box can be written as the arena box (arena_src "script IsEnterBox") for the existing re-warp check.
4. **Code (rematch.cpp / training.cpp):** when `fight_signal == waves`, treat "first wave group polled"
   (HkLesson already logs it) as the fight start. Treat `BossBattleModeEnd` with no open BossBattleMode in the Fight now
   mission as the fight end and the win. After the end, point or offer a warp to `clear_pos` so Proceed always appears.
   That replaces `kLessonLine` and the `lesson` special case with data columns.

Effect on normal rows (to be confirmed by a catalogue diff before merging):
- Rule 2 only fires when `placement_id` is "-", so the 5 duel dojos and every boss row with a placement are unchanged.
  Other rows with "-" placements and a single start point would gain a start. Run `rematch_catalog.py` before and after
  and list the rows whose stage_mission changes. The expected set is the 4 lessons plus any other no-placement mission
  (scroll E rows and X rows are built by erow/xrow and are not affected).
- Rule 4 "unpaired ModeEnd = win" could touch any normal row where the game sends ModeEnd after a death or retry. Only
  accept it when no BossBattleMode came in this Fight now and the row's `fight_signal == waves`. Normal boss rows then
  keep the current behaviour.

## 4. Live check plan (one machine, pc2 or main PC under the lead's lock)

- Build: the branch with rules 1-4 on top of open-world a1e91c19. Install the BR asi plus the regenerated
  Nioh3BossRematch_bosses.tsv. Keep the installed TR and EF.
- ini [Rematch]: `KeepGearInArenas=1` (default; ruled out), `AutoContinueLoad=1`, `VirtualPad=1`, `KeepRunningUnfocused=1`,
  `SelfTest=RD759,RA0A1,RE18C`. Reuse Temp/sweep87237/run.ps1 -Row. The self-test only proves load; the fight needs pad
  input or the pipe.
- Rows: **RD759** (2-wave lesson, white dot known), **RA0A1** (3-wave lesson, the Fuku arena, never passed), **RE18C**
  (duel control, a normal row that must not change).
- Pass lines in Nioh3BossRematch.log, per row:
  - Load: `F landed: retry point (+14h) <start> == the requested start point` with the start now coming from the
    catalogue. The line `had no start point in the catalogue` must be absent.
  - Spawn: `lesson group 50301 checked` (or 50701), then the new fight-start line for waves.
  - End: Training `BossBattleModeEnd`, then the new win line for the row, then the Proceed gate page logged on RB.
  - Leave: `resume home` with no `F FAIL` line for the row.
  - RE18C must still log the BossBattleMode FIGHT start, exactly as before.

Open (not proven): R1A5E and R2826 have no live result. Whether Training's End ignores an unpaired ModeEnd is read from
the hook only and needs confirming in the training.cpp End body.
