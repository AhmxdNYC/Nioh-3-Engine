# Cross-world boss load: bottleneck (pc2, answer to #7847)

Status: NOT PROVEN yet. No clean disk/CPU trace of a real cross-world run.

Evidence so far (n=1 prewarm run, Shingen R15FF, 2026-10-15 23:02 PDT, captures/rematch/xworldprobe-20261015-230009/training.log):
- world 0x96 landed at +12.5 s after the request; stage ready at +12.5 s
- cells stuck at 5-6 loaded of 9-12 for ~15 s while "placeable loads in flight" rose 1 -> 3080
- area-object gate only passed via CrossWorldAreaObjSkipMs 500 at +13.0 s
=> looks bound by the object queue, not disk. Hypothesis only.

Counters attempts:
- 23:13 and 23:15 runs: add-on pipe connect timed out, no cross-world command ran. The sampler (disk / CPU) only caught game boot. Not usable for this question.
- 03:01 PDT (10-16) run: landed +19.9 s, claimed at +18.3 s (captures/rematch/xworldprobe-20261016-025820). Load time varies 13-20 s run to run.
- Blocked: another slot's live game (pid 3272) on this PC.

Proposed change to test (hypothesis): during prewarm, cap or prioritise the placeable object loads needed at the arena cell, so the area-object gate passes earlier.

Default settings candidate: commit 7144a86 on pc2/faster-load, [ArenaSwap] in Nioh3Training.ini:
CrossWorld=1, CrossWorldEarlySwap=1, CrossWorldBlockCells=2, CrossWorldAreaObjSkipMs=500, CrossWorldPrewarm=2, CrossWorldPrewarmHoldMs=120000, CrossWorldCellSkip=2 (rows with a whitelist only).
Evidence for the prewarm part: n=2 each, 2.2 / 3.3 s ON vs 33.2 / 30.5 s OFF (27 s menu dwell, Shingen). Menu-open auto-trigger not yet tested.

## Update (pc2 #8256, 2026-10-16 07:40 PDT) - where the cold wait really goes (Shingen R15FF, no prewarm)
Logs: captures/rematch/xworldprobe-20261015-201624 (CellSkip 2) and -200555 (CellSkip log-only), both cold fnow, n=2.
- +0.7 s .. +6.9 s (6.2 s): mission sequencer state 4, LoadIsLoadEnd gates 0x110 = area-objects (16) + G9 (256). The OLD world 0xE0 is unchanged (15 cells, 11 loaded) and holds ~870 placeable loads in flight. CrossWorldAreaObjSkipMs never touches state 4.
- +6.9 .. +9 s: new world states 0-3.
- +9 .. +13.7 s: state 4, 6 cells, 1 loaded -> 5 (cell streaming, ~4.7 s). Stage ready +13.7 s.
- New-world area-objects gate (state 5): passes via CrossWorldAreaObjSkipMs 500 -> only 0.5 s. ~3100 placeable loads still queued, they keep streaming.
=> The object queue that costs time is the OLD world's, in state 4 (6.2 s), not the target world's (0.5 s).
Change: [ArenaSwap] CrossWorldOldAreaObjSkipMs = N (default 0) passes that gate after N ms in state 4. Branch pc2/oldao-trim 3d0911a. A-B-A-B (OFF/ON 300 ms, prewarm 0, CellSkip 0) queued, results pending.

## Result of the old-world trim test (pc2, Shingen R15FF cold fnow, prewarm 0, CellSkip 0, 2026-10-16 07:35-08:04 PDT)
Click -> arena swap (command to visible), n=2 OFF / n=1 ON (the second ON run was cancelled: another slot started the game):
- OFF (CrossWorldOldAreaObjSkipMs 0): 19.5 s, 21.1 s   (captures/rematch/xworldprobe-20261016-073559, -075334)
- ON  (CrossWorldOldAreaObjSkipMs 300): 18.2 s         (xworldprobe-20261016-080012)
The trim works as designed: the new world starts at +2.9 s instead of +6.9 s (state-4 gate passed at +985 ms). But the new world's last cell then stalls from +9.4 s to +17.4 s (OFF: cells finish ~4.7 s after their start), because the ~870 old-world placeable loads (up to ~3100 after the swap) are still streaming on the same loader pool (3 threads). Total is conserved: the wait moved, it did not go away. Gain ~1.3-2.9 s, n=1 ON, not significant.
Conclusion: the cold wait is bound by the placeable load queue throughput (loader pool), not by a gate. To literally skip it the placeable loads must be cancelled or never queued (hook the queue sites D97E8/D9EC7 in PlaceableCollisionUpdate D8E00 during a cross-world switch). Not built yet.
