# Return to where I was from a cross-world Fight now arena (1.7.90) - root cause + patch

Evidence: captures/ally-59602/Nioh3Training.rc1.log (08:21, 08:29), same file 08:00 (non-arena return).

## What the log shows
- 08:21:16.737 / 08:29:53.300 `RETURN: world 0x4B -> home 0xB1 ... (WarpOtherWorld)`; at +60 s `world now 0xB1, stage 5, load 1`
  (stage init state 5, GAMESTATE[2]=1 = the game's load/control cover still set). `home world reached` only at 08:27:34 (+377 s)
  and 08:33:14 (+201 s). So the home-world load is NOT hung, it takes 2-6 minutes. RhTick lifted the black screen at 60 s
  (phase 3 left when XrTick starts its `watch`) and then 90 s timeout -> player sees a world without player/HUD/control.
- Even the non-arena return 08:00 (0x96 -> 0xB1) took 55 s.
- The Fight now trip into the same worlds takes ~5 s to state 5 and passes the loader at 13 s (08:28:33 -> 08:28:52).

## Root cause
Fight now issues its WarpOtherWorld inside an XSTREAM window (XwStart -> XsStart, training.c ~1991): stage +0x11F1 block, cell load
cap, loader gate patch, and h_aor (3B3A28 hook, ~1773) reporting the area-objects gate as passed after CrossWorldAreaObjSkipMs (500 ms)
once the target world is current and mission seq state is 5/8 (`!g_xs.on` => hook is a no-op). The two return paths (RhTick phase 2,
XwCancelTick) call 22399B8 with NO XsStart, so the big home world (0xB1, mission EA75 area objects) waits for every placeable load
(LoadIsLoadEnd gate 0x10 "area objs") at the stock cell cap. Nothing arena related stays set: BossModeEnd runs before the request,
TrainStop clears the session, no PSKIP/XSTREAM state is active (log has no XSTREAM lines after 08:21:16).
Second defect: RhTick lifts the loading cover at 60 s of game time (`g_rh.phase==3` only waited while `!g_xr.watch`) and gives up at 90 s.
Third (from 1d2b9b34, dropped in RC2): in the arena the mission reads 0 so `mis != g_home.mission` aborts the return ("mission changed").

## Patch (training.c)
1. RhTick phase 2 and XwCancelTick: `g_ps_row = 0; XsStart("RETURN"/"XCANCEL", home world); g_xs.maxMs = 180000;` right before 22399B8.
   XsTick already closes the window when the world has streamed (XsEnd) or at maxMs (new field, default 60000 unchanged for Fight now).
2. RhTick phase 3: hold the black cover while `g_xr.active` (put-back pending) up to 190 s from phase 2; total RETURN timeout 200 s in phase 3 (90 s otherwise).
3. Re-add the mission-0 exception, keyed on `g_rh.arena` (captured at request time as `g_tr.active && g_tr.arena`, because phase 1 ends the session) and `CurWorld() != g_home.world`.
Same-world arena returns are untouched (plain Warp path).

## Test plan for ally
Cross-world Fight now (R1CBC from home 0xB1/0x4B) -> Return: expect `XSTREAM RETURN: start` ... `area-objects gate passed`,
`XWORLD return: home world 0xB1 reached` within ~10-20 s (like Fight now), `player put back`, then `RETURN done (back where the player was)`
with player/HUD/control. If it is still slow, the XSTREAM lines will show which gate (0x..) holds; the 3B3A28 hook needs `CrossWorldAreaObjSkipMs 500` (ini default).
Unproven: no in-game run; the gate attribution is inferred from the Fight now logs and the missing XSTREAM window, not seen directly.
