# Player frame meter (#22796)

Implemented (offline-tested only, NOT run in the game): a second SF6-style bar "FRAME METER  Player" under the boss bar.
Menu: Practice > Player page > "Player frame meter" ON/OFF (`[Practice] PlayerFrameMeter`, PS_PLAYER_METER = 10). Works with or without the boss meter.

## What the player already exposes (proven from old logs)
- The FRAMEPROBE hook 2B4A3C already fires for the player's attacks: `notes/frame-probe-hino-22356.log` diag line `atk == owner == player` from 2B30E2
  (same collider path as bosses: hit record H, row R, pool collider -> window open / end). So active windows, power and the GreatAttack bit are available.
- The meter algorithm (frame_meter.h) is generic: it only needs (action id, window open, unblockable) per game tick. Reused unchanged; same game-time clock.

## Code
- frame_probe_game.h: extra `g_fp[FP_PL]` entry follows `PlayerChar()`; `FpPlayerTick` reads ctrl = [[actor+0x3B0]+8] and the action id with 358528(ctrl)
  (the boss method); `FpHit` matches the player (atk == actor / AI / owner) after the boss slots.
- frame_meter_game.h: second `FmMeter g_fmp`, panels drawn stacked (boss, player).

## Untested / assumed (verify live first)
1. That 358528(playerCtrl) returns a changing action id per attack. If the bar shows one endless run or nothing, the player's action id lives elsewhere.
   Next RE step: log id per tick with `[Debug] FrameProbe 1`; compare with the player's action record (archive action files, same `act+0x20` rows as
   bosses) and with `*(ctrl+0xE8)` (FpHit reads `id = **(ctrl+0xE8)`; check it equals 358528).
2. Player hits on the training dummy / boss are all counted; projectiles, ninjutsu, spirit hits go through owner resolver 325F4C (kind 4) - unproven.
3. Startup counts to the first hit window, so a whiff still shows its windows only if the collider spawns (it does on whiffs for bosses); player cancels
   (action id change mid-swing) end the run at the cancel, no cancel-window display.
4. Overlay height/position with two panels (FrameMeterY unchanged).
