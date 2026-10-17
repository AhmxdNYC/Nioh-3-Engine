# Boss action ids from live logging (pc2 #9508)

Logger: `Nioh3Training` `ActionLog` (branch `pc2/action-log` 3926553, asi dd396480): every >= 40 ms, for each boss with a gauge row, calls native
`358528(state)` (= dword at current-action state+E8, FFFF when no current action; state = [[actor+3B0]+8]) and buckets per (arena chr, id).
Raw output: `Nioh3Training_actions.tsv` (chr, action id, entries, samples, ms). Log lines `ACTLOG boss ... action 0xID (prev, ran N ms)` in Nioh3Training.log.

## Takeda Shingen (chr 42450) - clean run, 5 min, boss attacks an idle invulnerable player (pc2, xworldprobe-20261016-213044)

22 distinct ids. Data: `notes/boss-action-ids-shingen-chr42450.tsv` (ms = time the id was current; entries = times it started).
Ids 0x4E21..0x4E8F (19 of them) look like the attack/move family (0x4E4E, 0x4E45, 0x4E44 ran longest); 0x0, 0x2, 0x3FA, 0x3FB, 0x1875 are not in that range
(likely idle/locomotion/hit-reaction; 0x0 ran 15 s with 1 entry = probably the idle-before-aggro). Labels need one observation per id (video/anim): NOT done.

## Caveats
- One boss clean only. Yoritomo (chr 16102) and Otoroshi runs of this batch are contaminated: a second pc2 harness instance (another slot) sent its own `fnow` commands to the same game (7B35 and 2EE9 crossed, "a load is running, refused"), so the Yoritomo file has only 14 changes and no Otoroshi data exists. Redo them alone.
- Ids are action *ids*, not names; ids are per-boss assets and must not be reused across bosses.
