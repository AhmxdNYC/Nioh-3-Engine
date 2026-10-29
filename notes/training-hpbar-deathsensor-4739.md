# Boss HP bar + death sensor, live on Ally (bridge 4739)

2026-10-14, Ally (Proton, 1920x1080 fullscreen, gamescope). Build: Nioh3Training with owner fallbacks (OwnerWaitMs 4000, OwnerActivate 1), BossMode=1, BossBgm=1.
Run /tmp/own-4717 (OFF-training.log, s4/s7/yB1/yB2 screenshots).

## Yoritomo R7B35 (world 0xB1, fnow, no mission load)
- Warp+spawn: command -> control 13.0 s cold (boss model load), owner 9528 (Rajomon placement) found immediately, no owner wait needed.
- Boss spawned, hostile: hits for 390-424, player 2658 -> 490 -> 100 HP. yB2.png.
- HP gauge: NOT shown in any screenshot (yB1 before contact, s7/yB2 in the fight). ShowBossGauge(0x10002538) resolves the owner placement 9528 (Ibaraki's), not the spawn; matches training-live-spawn-4318.md row 3b. Not fixed: needs gauge bound to the spawned actor.
- BBM log: 'no registered wrapper (dynamic boss): HUD/mode steps only' (as on pc2).

## Death sensor false positive: CONFIRMED
Player died at +46.7 s: BBM log 'alive sensor 0 -> -1' (boss actor left the actor table when the player died/respawned), +1.5 s later 'boss gone/dead' ->
RequestBossBattleBgmEndState + BossBattleModeEnd although the boss was alive.
Fix (training.c BossTick): sensor -1 (actor gone) -> 'actor gone, not dead': StopBossBGM + BossBattleModeEnd + HideBossGauge, no victory end state. Only sensor 0 -> 1 with the actor present counts as dead.
