# CrossWorldForceCells OFF/ON, cold Fight now, no prewarm (pc2, task #8298)
Device: pc2 (16 GB RAM). Build: bridge file #639 Nioh3Training.asi (sha 0aa527b9, pc/fnow-visible 4f68c49), perf asi 60198, Rematch_good.asi.
Settings both arms: CrossWorldPrewarm=0, CrossWorldCellSkip=0, CrossWorldAreaObjSkipMs=500, CrossWorld=1, CrossWorldEarlySwap=1; only CrossWorldForceCells differs (OFF=0, ON=1).
Method: cold game start -> Continue -> `fnow <row>` through the pipe (no menu dwell), 45 s wait. Visible = "arena swap in the new world +X ms after the request"; control = visible + ~0.5 s ("ARENA +515 ms CONTROL").
Other slots also run the game on this PC; several earlier attempts were invalid and re-run, so contention is possible.

| Row | Arm | Run 1 visible | Run 2 visible | mean |
|---|---|---|---|---|
| Shingen R15FF | OFF | 15.0 s (081703) | 23.2 s (090541) | 19.1 |
| Shingen R15FF | ON  | 13.5 s (085450) | 13.1 s (090147) | 13.3 |
| Otoroshi R2EE9 | OFF | 25.1 s (084722) | 16.9 s (091250) | 21.0 |
| Otoroshi R2EE9 | ON  | 40.0 s (084213) | 30.4 s (090923) | 35.2 |

XFORCE lines: Shingen ON: "stage 4 seq 5 ... 0/5 arena cells admitted" at +5.4/5.6 s (the 5 whitelist cells were not admitted at that point). Otoroshi ON: "3/6 arena cells admitted [67253 584691 820705 ->1; 364049 484693 525291 ->0]" at +4.3 s; old-world area-object wait skipped at +656 ms.
Verdict: Shingen ON ~13.3 s vs OFF 15.0/23.2 (n=2 each, high OFF variance, not clearly better); Otoroshi ON is WORSE (30-40 s vs 17-25 s). ForceCells is not a win as built. Folders: captures/rematch/xworldprobe-20261016-*.
