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
