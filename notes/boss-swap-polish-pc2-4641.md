# Boss arena swap: what is missing (pc2, task #4641, 2026-10-14)
Device pc2 (RX 6650 XT, 1080p, ~50 fps in field), Nioh3 v2.02, add-on 1.3.3 (file 467) + Nioh3Training (file 466/464), BossLog off.

## Measured in game (Nuribotoke R401E, base world 0xE0, same swap path as the others)
- Swap: player warped to arena, boss spawned by CreateDynamic(392871) 0.3 s after command (preloaded). Log: Nioh3Training.log FIGHTNOW/ARENA/CreateDynamic.
- BGM: none. Nioh3 audio session peak 0.00-0.01 for 8 s after the swap (same as before it).
- Boss HP bar: none seen (screens in field, boss behind camera).
- BossBattleMode never ran: add-on status after swap = fights=0 infight=0 (it hooks Mission::BossBattleMode).
- Fog gate / intro: none (no script ran; only CreateDynamic is called).
- Yoritomo / Ibaraki / Yamagata: same code path, so same gaps. Earlier run (#4626): no HP bar seen. A fresh per-boss run
  was NOT possible: fast travel from base to the Heian world crashed the game (Nioh3.exe vanished, relaunched at title 19:49,
  likely my enter script's guard or a crash restart; cause not investigated). Per-boss in-game confirmation still open.

## Why (static RE, existing notes; no new decompile)
Swap spawns the boss actor only. Everything else is mission script `sub_event_boss_battle_start` (launched through 94B04
ScriptLaunchNamed, mgr+0x40 name table), which calls: Mission::BossBattleMode(placementId) (0x23A2DCC: sets charMgr+0x26AA,
HUD boss mode, boss gauge), UI::ShowBossGauge, Sound::PlayBossBGM(hash) (wrapper 0x3BFF948; 32-bit hash constant per script,
see notes/boss-script-args.csv e.g. 0xCFAB3684), door/fog: SetDoorControlAbsoluteClose / SetReplacementStatusPlacement,
Camera::RequestScriptCamera + RTM intro. See notes/boss-bgm-events.md, boss-duelflow-script.md, boss-fights.md.
94B04 needs a live mission script owner (R9), which a field world has none of, so replaying it (as in #4572) works only inside the mission.

## Options
A. Call natives directly after CreateDynamic: BossBattleMode(boss placement) -> HP bar + boss mode; PlayBossBGM(hash from csv) -> music;
   StopBossBGM on end. Needs per-boss hash + placement id table (extractable from boss-script-args.csv). Cheapest, gives bar+BGM.
B. Fog gate: spawn/close the door gadget via the same native the script uses (SetDoorControlAbsoluteClose); needs owner mission context. Harder.
C. Intro: skip (RTM); the swap already has the boss idle with awareness range.
Not done: no code changed.
