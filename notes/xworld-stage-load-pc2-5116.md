# Cross-world stage load experiment (pc2, task #5116, 2026-10-15)
Device pc2 (RX 6650 XT, 1080p, ~44-50 fps), Nioh3 v2.02, save continues in field world 0xE0, test Nioh3Training.asi (arena-swap-4431 source + `rdump`, `stgreq`, oworld keep arg; diff notes/xworld-5116-training.diff), saves backed up/restored byte for byte. Single samples, no repeats. Harness: tools/rematch/xworld_probe.ps1 (scenario files). Captures: captures/rematch/xworldprobe-20261015-{061451,062220,062727,063352,063910,064559}.

## 1. Real stage ids, one native Yoritomo fight (R7B35, add-on `fight`, run 061451)
Request world 0xB1, start/retry point 374664 (0x5B908), main cell 67253, centre 216993 15020 221781. Timeline from request: +0.1 s registry 3161 -> 10 (world switched, player at start), +4.2 s stage state 4, +9 s state 5 (main cell + 8 cells), +20 s 16-17 cells, loadms 28.97 s (add-on), BossBattleMode at +49.5 s (intro). Loaded cell set of the arena (identical in all 0xB1 runs): 67253 221265 285287 301780 320919 329181 364049 500328 525291 584691 625536 780040 820705 833442 889691 974198 983784. Base world 0xE0 cell 239285 (15 cells).

## 2. Method A: MissionRequestRegion 22399B8 (WarpOtherWorld, mission 0), `oworld 0xB1 374664`
prm built as the VM wrapper (1BF7D68 init, prm[7]=0, prm[0xA]=0, prm[0xE]=1).
- keep=0 (run 062220): world switch +1.0 s (player at Yoritomo start), stage state 5 at +10.8 s, registry 10 -> 2692 at +19.6 s, 2702 (settled) at +24.6 s.
- keep=1 prm[2]=1 KeepLoadedStage (run 062727): switch +1.0 s, state 4 +6.3 s, state 5 seen by +23 s, registry 2692 at +30.7 s, settled +36 s. NOT faster (this is a different world, nothing to keep; slower in this single sample).
- Arena usable (run 064559): oworld, then `arena 16102 ...` (warp into arena + CreateDynamic): boss spawned and control returned 9.9 s after the oworld command (arena cmd issued +5.6 s; arena ready 4.1 s later); Yoritomo alive, HP bar shown, walks (screenshot arena_after_oworld.png). Boss is passive in this build (owner=player, known, ally FIX 1). Background field actors (2692 registry) keep loading until ~+28 s but are not needed.

## 3. Method B: arena-stage-only FB0530(stageMgr+0x500, id, flag, 0) from world 0xE0 (run 063910)
Static (runtime dump 063352/d4, 062727/dumps): FB0530 = wrapper -> 3A0534(sub, stageId, 0, 0, flagByte). stageId is a CELL id (compared against list entries at sub+0x390 and passed to cell streamers 1FB460C/1FB4738 on sub+0x40), the 3rd arg byte is a flag, NOT a world. The stage mgr is bound to its world byte (sub+0): the request streams a cell of the CURRENT world.
Measured: `stgreq 0 67253` and `stgreq 0xB1 67253` in 0xE0: no crash, call returns in 0 ms, [45B9E58]+760h stays 3 (IsStageLoadEnd already true = no signal), world stays 0xE0, player position unchanged, loaded count 15 -> 16 (an E0 cell 67253 slot, not the Heian arena). Not usable: cannot bring the arena of another world.
NOTE A40A30 (Load::IsStageLoadEnd) is a script native (takes the VM context in rcx): do not call it directly, poll [45B9E58]+760h == 3.

## Verdict
Only a world switch brings another world's arena. Cheapest working path = Method A + live arena spawn: boss fightable ~10 s after the request (native fight-now: ~29 s load, 49 s to BossBattleMode). Remaining cost is the world request itself (state 5 at ~+9-11 s); skipping the 2692-actor field registry (+20-28 s) is already hidden because the fight starts at ~+10 s. Next: prm[3..] / world-field placement filter would only help the background; a boss-only world request is not needed for the first 10 s.
