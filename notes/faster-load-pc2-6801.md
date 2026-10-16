# Faster cross-world Fight now (pc2, task 6801, 2026-10-15)
Device: pc2 (Win11, 16 GB, Nioh3 + EngineFix 1.7.74 #558 + Rematch 1.4.21 #559 + Training from pc/toggles-merge 8430966 + this branch). Route: field in world 0xE0 -> Takeda Shingen R15FF (world 0x96), Rematch pipe `fight`. Times after the world request, cold:

| arm | world switched | stage ready | arena swap | boss spawn | cover fades in |
|---|---|---|---|---|---|
| A baseline | 5.7 | 11.0 | 11.7 | 12.0 | 27.4 (control back 28.7) |
| B AreaObjSkip 500 | 5.4 | 10.2 | 11.0 | 11.3 | 25.6 (control 28.0) |
| C B + Fade PlaceableWaitMs 1500 | 5.4 | 10.3 | 11.0 | 11.4 | 25.8 (control 28.0) |
| F C + CrossWorldLoaderGateSkip 1 | 5.6 | 10.8 | 11.5 | 11.8 | 27.4 (control 27.6) |
| H prewarm 2, 10 s dwell | - | landed +10.2 | +0.7 after press | +1.0 | 17.3 after press |
| J prewarm 2, 27 s dwell | - | landed +12.2 | +0.0 after press | +0.3 | **1.0 after press** |

Findings
- The cover is the game's control lock: sequencer state 5 (MissionLoadStep) -> 6 (223C53C) -> 8 -> 0xF. State 5 waits LoadIsLoadEnd; probe bits show gate G9 (3B3424 -> 46518()&&3B2970(): loader [475A420] +0x248/+0x249 busy) holds it from stage-ready (+11 s) to +23.6 s. Skipping G9 (jne 75 6E at 3B3523, `CrossWorldLoaderGateSkip=1`) ends state 5 at +15.2 s, but state 6 then waits until +24 s: 1C1C61C walks a list until every node has +0x138 == 8 (actors constructed behind ~3000 queued placeable loads). Skipping that wait (jne 74 4F at 223C55A) hangs the game at state 0xF (tried once, removed). AreaObjSkip alone does nothing (G9 and state 6 are the limiters).
- Only prewarm helps: `CrossWorldPrewarm = 2` (+ `CrossWorldPrewarmHoldMs = 120000`) does the real WarpOtherWorld when the menu opens; Fight now claims it. 27 s of menu time -> 1.0 s cover, boss spawns 0.3 s after the press, 60 s combat (boss hit the player 2658 -> 485 HP). Shorter dwell: cover = 27.5 s - dwell.
- Tested via Rematch pipe + Training.cmd `xpre 0x15FF` (menu-open Preload not driven by a real menu). With the default HoldMs 30000 a 35 s dwell is cancelled and returned (pipe `fight` then refuses `loading`).
- Initial boot to field (title -> Continue) 54 s; not changed.
- Memory: game working set 7.4 GB during the cross-world swap; no OOM on pc2 (16 GB).
Evidence: captures/rematch/fl{A-base,B-aoskip,C-aoskip-pw,E-probe,F-lgs,G-lgs3,H-pre35,I-pre10,J-pre27}-*/training.log; driver tools/rematch/fl_test.ps1 (tools/ is gitignored).

## Final recommended INI (pc2, 2026-10-15; [ArenaSwap] of Nioh3Training.ini)
```
CrossWorld = 1
CrossWorldEarlySwap = 1
CrossWorldBlockCells = 2
CrossWorldAreaObjSkipMs = 500
CrossWorldPrewarm = 2
CrossWorldPrewarmHoldMs = 120000
CrossWorldCellSkip = 2      ; only effective for rows with a whitelist (tsv col 16; Yoritomo 0xB1 built in, Shingen R15FF added)
```
Evidence: prewarm 2, 27 s menu dwell, Shingen, n=2 each: ON cover 2.2 s / 3.3 s vs OFF 33.2 / 30.5 s (captures/rematch/n2C-20261015-194618, n2A-20261015-193415). Needs the Training asi to read CrossWorldPrewarm=2 (stock Training_installed.asi read 0 -> invalid ON arm earlier).
CellSkip (cold, no menu dwell, Shingen): CellSkip=2 with whitelist 193960,333645,439542,633740,929786 -> swap 14.4 s vs 19.0 s log-only (n=1 each, captures/rematch/xworldprobe-20261015-201624 vs -200555); arena + boss HP bar fine. Helps only when the menu dwell is short.
