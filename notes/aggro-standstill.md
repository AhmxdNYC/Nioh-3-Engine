# Aggro in Stand still (#14147) + death in Training / Boss Match (#14147, #14150)

Branch `pc/aggro-standstill` (off `pc/combined-1019` 7e50eb15). Offline only: the game and the installed files were not touched.

## 1. Boss aggro in 'Stand still'
Before: `AggroTick` skipped every boss whose AI was held (`BhAiOn != 1` / `g_ih.phase`), so in Stand still (Idle hold phase 2, AI off,
drill.h forcing the move) aggro did nothing (13:08 log: only "in range" lines, `DRILL` forces from where the boss stood).

Change (aggro.h `AgDrillBoss`, only with BossAggro 1; aggro OFF = no change):
- Out of `AggroRangeCm` and between two forced moves -> walk-up: `g_dr_hold = 1` (drill.h forces nothing, restarts its gap),
  AI on through the existing drill-fallback flag `g_ih_drill_ai` (IhTick leaves it on), AI state in `g_mv_chase` (repeat does not
  replace picks), player made the current target + hate (`AgTarget`, same as the CPU stance).
- During the walk-up `IhSelect` (idle_hold.h, `g_ih_chase_t`) passes movement picks (kinds M/T/L/I, uncatalogued, < 0x4E20) and turns
  attacks / wind-ups / unclassified moves into idle for the first 6 s; after 6 s everything passes (a boss that only closes with a lunge still arrives).
- In range: AI stays on until the boss is back in idle 0x2 (max 4 s), then AI off, drill resumes ~0.5 s later.
- Before every forced drill move with aggro ON the boss is turned to the player (`AgDrillFace`: 5ECE50 yaw matrix -> rows ch+0x120,
  the same write as BossPlace/Reset boss position; position untouched). `[Training] AggroFace = 0` disables it.
- Ends cleanly on aggro off, stance change, repeat stopped, boss dead / load hold.
- Menu hint (Rematch, Boss aggro row + toggle message) now says it works in both stances.

## 2. Death in a Training arena (log 13:22:43-13:22:52)
- `PlayerNoDeath` is read from `[Summon]` (not [Training]); code default 1, repo ini 1. The installed ini has `PlayerNoDeath = 0`:
  set by the 2026-10-18 pc install of 53ee3e2 (.handoff/next.md: "PlayerNoDeath=0, SummonNoDeath=0"), probably a misread of #13197,
  which only asked SummonNoDeath (menu summons) default 0.
- With NoDeath off the HP went to 0 = the game's own death. Our Practice retry at +2 s healed and warped the player, but the native
  death flow continued: 13:22:52 GAMESTATE[2]=1 load, player at the shrine 44 m away, session boss left behind.
- Fix (training.c):
  - `[Summon] SessionNoDeath` (default 1): every g_tr session (Practice, Fight now arena, 'train') keeps native NoDeath on whatever
    PlayerNoDeath says (`TrNoDeath()`), and TrainTick re-applies it every 1 s while it is off or the player character pointer changed
    (after a load; may explain the 10-17 teleport test "PlayerNoDeath did not stop a player death"). 0 = old behaviour.
  - #14150 'Player can't die' (`[Summon] PlayerCantDie`, default 1, menu row on the Boss moves page and the Return/gate page in
    both modes, exports `Nioh3Training_Get/SetPlayerCantDie`): ON = HP at the floor -> healed in place (RecoverFull), fight goes on;
    OFF = HP floor counts as a death -> the existing in-arena round reset 2 s later (full HP, round-1 spot, boss respawned).
    Native NoDeath stays on either way, so the game's death/shrine load never runs in a session.
  - Practice retry no longer calls `IhDefault(1)` (forced Stand still): the chosen stance is kept; repeat moves and aggro are global.

## Build (2026-10-19, existing scripts: mod/Nioh3Training/build.sh, mod/Nioh3BossRematch/build.bat)
- mod/Nioh3Training/Nioh3Training.asi sha256 `fabdea54020cb158162b9d14361b900612698f94b2b37a5802f7aac4c8916af6`
- mod/Nioh3BossRematch/bin/Nioh3BossRematch.asi sha256 `47d4df4ce59727622c442caef9a5166047374025c4b80ceb5d1aaf255b4a1e46`
- Offline tests: Training run_move_toggle_tests.py (incl. new drill walk-up hold + IhSelect cases), run_owned_registry_tests.py,
  Rematch run_rematch_harness.py (77/77) all pass.

## Untested live
- Whether the AI walks up (vs. idling) with attacks turned to idle, the face write (ch+0x120) taking effect before a forced move,
  the AI-off at idle after arrival.
- NoDeath re-apply after a load, heal-in-place at the HP floor during boss combos, stance kept across the Practice retry.
- Installed ini still has PlayerNoDeath = 0; with SessionNoDeath default 1 (key absent = 1) that no longer matters.

## #14267 follow-up (user test of FABDEA54 / 47D4DF4C, 19:44-19:50)
- Aggro walk-up worked live: 19:46:42 "5.8 m ... AI on, it walks up", 19:46:44 "arrived (4.0 m)", "walk-up end, AI off again, drill resumes",
  forced move faced the player ("turned to the player (yaw 10.9)"). Same again 19:47:02. Cosmetic: ~545 "IDLEHOLD transition 0x4E64 -> idle"
  lines while walking (the AI re-picks the attack every frame, each turned into idle).
- Why 'Player can't die' OFF "did nothing": the toggle took effect at once (read every tick), but player HP stayed 2658/2658 for the whole test
  (HB lines), so the floor was never reached. And the old ON only healed at the floor, so ON and OFF looked the same until 1 HP.
  (Native NoDeath clamps at HP 1; the floor test `c <= 1 || c*100 <= m` already covers it: 19:44:51 "beaten (HP 1 / 2658)".)
- New meaning (user #14166/#14150): ON = infinite health: any HP below max is refilled at once (direct write to [[actor+0x3A0]+0x58],
  the field PlayerHp reads; after 3 writes that do not stick it falls back to Player::RecoverFull every 250 ms). OFF = normal damage; at the floor
  -> respawn in the arena after [Summon] BeatenRespawnMs (default 0 = at once; was a fixed 2 s), stance / repeat / aggro kept, Return object usable.
  Decision in src/player_hp.h (PhDecide), offline test tests/player_hp_test.c (in run_move_toggle_tests.py).
- Menu: the toggle left the Boss moves / gate lists; new page 'Practice - Player' (row "Player (infinite health: ON/OFF)") on the Practice hub
  under the arenas row, and the same row on the Return-object page (Training and Boss Match modes). Row text "Infinite health (can't die)".
- Build: Nioh3Training.asi sha256 5716971332b038888c49414bb8cd4641c0a2c158b74f04a0382e2f41c32bb1e3,
  Nioh3BossRematch.asi sha256 242ea4a12981befe1aaf793f4f44a64a5e2beead25a2910a952a7139e0915002. Offline tests + Rematch harness pass.
- Untested live: whether the HP write sticks (else RecoverFull fallback, which may also refill other things), instant respawn at the floor
  with OFF, Player page navigation / Back from the Return-object page.

## #14267b (live 20:02:00-20:02:31 on Training 57169713): walk-up let the boss use its own moves
- 0x4E43 (catalogued, kind 0 "motion 6920") started 6.5 s into a walk-up: the old filter released every pick after 6 s. REPEAT 'pick 0x4EA8 -> 0x4E52'
  came in the 'arrived, AI on until idle' part (no filter there). Distance grew 6.5 -> 8.7 m while 'walking up' (with attacks filtered the AI did
  not walk), and walk-up restarted at 5.3 / 5.9 m right after arriving at 5.0 m (no hysteresis). 2075 IDLEHOLD transition lines in 31 s.
- Fix: IhSelect during the whole walk-up and the arrived-until-idle part passes only kind 'M' rows, idle and ids < 0x4E20; every other
  id >= 0x4E20 (incl. kind 0 and uncatalogued) -> idle. Walk-up starts only beyond AggroRangeCm + 1 m, ends at AggroRangeCm. No closing by 0.5 m within
  [Training] AggroWalkTimeoutMs (6000) -> BossPlace at AggroRangeCm - 1 m from the player, facing the player. IDLEHOLD transition log <= 1 line / 2 s
  (with a suppressed count); AGGRO walk-up lines share the 200-line cap.
- Build: Nioh3Training.asi sha256 dee2e54f478f0bf1d45f95fa70477667350fcef0267bd9f7daba9c90e2879fe7 (Rematch unchanged 242ea4a1...).
- Untested live: whether the AI walks with only locomotion allowed (else the 6 s placement kicks in), BossPlace (never run live), 66C330 in BossPlace.
