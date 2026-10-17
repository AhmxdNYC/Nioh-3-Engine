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

## Yoritomo (chr 16102) and Otoroshi (chr 371449), 2026-10-16 pc2
Same logger, ~5 min vs idle player, one harness at a time. Yoritomo: 13 ids (12 in 0x4E24-0x4E7B). Otoroshi: 13 ids (0x4E33-0x4EEE plus 0x18C7). Unlabelled. Data: boss-action-ids-yoritomo-chr16102.tsv, boss-action-ids-otoroshi-chr371449.tsv.

## Per-id table (times used, average duration, damage) - pc2 #9713

Columns: times used = `entries` (times the id started in ~5 min vs an idle player); avg ms = `ms`/`entries` (sampled ~47 ms, so +-47 ms); hits on player / dmg per hit = **not measured**: in these runs the player HP stays at full (log heartbeat `player HP 2658/2658` all run, invulnerable idle player) and the logger does not record damage events, so no id can be tied to a hit. Needs a run with a vulnerable player + a damage-event hook. Ids 0x0/0x2 are idle/other, not moves. Labels: none yet (screenshot pass #9702 in progress by another pc2 slot).


### Takeda Shingen

| id | times used | avg ms | total ms | hits on player | dmg/hit |
|---|---|---|---|---|---|
| 0x0 | 1 | 15235 | 15235 | n/m | n/m |
| 0x2 | 3 | 786 | 2358 | n/m | n/m |
| 0x4E64 | 10 | 1947 | 19472 | n/m | n/m |
| 0x4E7E | 10 | 2260 | 22609 | n/m | n/m |
| 0x1875 | 1 | 5672 | 5672 | n/m | n/m |
| 0x3FB | 1 | 1219 | 1219 | n/m | n/m |
| 0x4E45 | 15 | 2175 | 32626 | n/m | n/m |
| 0x4E4E | 12 | 3589 | 43077 | n/m | n/m |
| 0x4E41 | 1 | 2766 | 2766 | n/m | n/m |
| 0x4E44 | 11 | 2660 | 29267 | n/m | n/m |
| 0x4E2D | 14 | 1710 | 23951 | n/m | n/m |
| 0x4E2E | 14 | 1896 | 26547 | n/m | n/m |
| 0x4E2F | 14 | 238 | 3345 | n/m | n/m |
| 0x4E8F | 1 | 1000 | 1000 | n/m | n/m |
| 0x4E80 | 13 | 1135 | 14765 | n/m | n/m |
| 0x4E57 | 13 | 334 | 4344 | n/m | n/m |
| 0x4E86 | 13 | 200 | 2609 | n/m | n/m |
| 0x4E87 | 13 | 170 | 2219 | n/m | n/m |
| 0x4E88 | 13 | 294 | 3829 | n/m | n/m |
| 0x4E56 | 13 | 1561 | 20295 | n/m | n/m |
| 0x3FA | 18 | 1604 | 28889 | n/m | n/m |
| 0x4E21 | 1 | 47 | 47 | n/m | n/m |

### Minamoto no Yoritomo

| id | times used | avg ms | total ms | hits on player | dmg/hit |
|---|---|---|---|---|---|
| 0x0 | 1 | 8719 | 8719 | n/m | n/m |
| 0x2 | 2 | 258 | 516 | n/m | n/m |
| 0x4E30 | 1 | 1078 | 1078 | n/m | n/m |
| 0x4E31 | 1 | 1125 | 1125 | n/m | n/m |
| 0x4E32 | 1 | 47 | 47 | n/m | n/m |
| 0x4E6E | 1 | 5531 | 5531 | n/m | n/m |
| 0x4E24 | 1 | 937 | 937 | n/m | n/m |
| 0x4E25 | 1 | 1688 | 1688 | n/m | n/m |
| 0x4E26 | 1 | 47 | 47 | n/m | n/m |
| 0x4E55 | 1 | 1031 | 1031 | n/m | n/m |
| 0x4E59 | 1 | 1453 | 1453 | n/m | n/m |
| 0x4E5E | 1 | 703 | 703 | n/m | n/m |
| 0x4E64 | 1 | 2578 | 2578 | n/m | n/m |
| 0x4E7B | 1 | 703 | 703 | n/m | n/m |

### Otoroshi

| id | times used | avg ms | total ms | hits on player | dmg/hit |
|---|---|---|---|---|---|
| 0x0 | 1 | 625 | 625 | n/m | n/m |
| 0x2 | 2 | 1218 | 2437 | n/m | n/m |
| 0x4E36 | 1 | 2578 | 2578 | n/m | n/m |
| 0x4E4E | 1 | 93 | 93 | n/m | n/m |
| 0x18C7 | 1 | 4829 | 4829 | n/m | n/m |
| 0x4EEB | 1 | 844 | 844 | n/m | n/m |
| 0x4E6C | 1 | 1219 | 1219 | n/m | n/m |
| 0x4EED | 1 | 422 | 422 | n/m | n/m |
| 0x4E33 | 1 | 1500 | 1500 | n/m | n/m |
| 0x4E4F | 1 | 890 | 890 | n/m | n/m |
| 0x4E50 | 1 | 3188 | 3188 | n/m | n/m |
| 0x4EEE | 1 | 1312 | 1312 | n/m | n/m |
| 0x4EEA | 1 | 141 | 141 | n/m | n/m |

### Yamagata Masakage (R1CBC)

| id | times used | avg ms | total ms | hits on player | dmg/hit |
|---|---|---|---|---|---|
| 0x0 | 1 | 906 | 906 | n/m | n/m |
| 0x2 | 2 | 1101 | 2203 | n/m | n/m |
| 0x4E7A | 1 | 750 | 750 | n/m | n/m |
| 0x4E7C | 1 | 2532 | 2532 | n/m | n/m |
| 0x4E70 | 1 | 656 | 656 | n/m | n/m |
| 0x4E72 | 1 | 1031 | 1031 | n/m | n/m |
| 0x4E88 | 2 | 2601 | 5203 | n/m | n/m |
| 0x4E84 | 2 | 1125 | 2250 | n/m | n/m |
| 0x4E85 | 2 | 750 | 1500 | n/m | n/m |
| 0x4E86 | 2 | 773 | 1547 | n/m | n/m |
| 0x4E87 | 2 | 1383 | 2766 | n/m | n/m |
| 0x4E5C | 2 | 2484 | 4968 | n/m | n/m |
| 0x4E48 | 2 | 727 | 1454 | n/m | n/m |
| 0x4E49 | 2 | 750 | 1500 | n/m | n/m |
| 0x4E4A | 2 | 3656 | 7312 | n/m | n/m |
| 0x4E66 | 2 | 1453 | 2906 | n/m | n/m |
| 0x4E68 | 2 | 2062 | 4125 | n/m | n/m |
| 0x4E98 | 1 | 2438 | 2438 | n/m | n/m |
| 0x4E99 | 1 | 2531 | 2531 | n/m | n/m |
| 0x4E29 | 2 | 609 | 1219 | n/m | n/m |
| 0x4E2B | 2 | 586 | 1172 | n/m | n/m |
| 0x4E40 | 1 | 47 | 47 | n/m | n/m |
| 0x4E34 | 1 | 1171 | 1171 | n/m | n/m |
| 0x4E2C | 1 | 1125 | 1125 | n/m | n/m |
| 0x4E2E | 1 | 47 | 47 | n/m | n/m |
| 0x4E52 | 1 | 1735 | 1735 | n/m | n/m |

### Hino-enma (R23E5)

| id | times used | avg ms | total ms | hits on player | dmg/hit |
|---|---|---|---|---|---|
| 0x0 | 1 | 1016 | 1016 | n/m | n/m |
| 0x2 | 1 | 1969 | 1969 | n/m | n/m |
| 0x4E29 | 1 | 1078 | 1078 | n/m | n/m |
| 0x4E75 | 2 | 937 | 1875 | n/m | n/m |
| 0x4E2A | 2 | 47 | 94 | n/m | n/m |
| 0x4E7F | 1 | 2391 | 2391 | n/m | n/m |
| 0x4E7D | 2 | 4406 | 8812 | n/m | n/m |
| 0x4E81 | 2 | 1148 | 2297 | n/m | n/m |
| 0x4E78 | 2 | 2226 | 4453 | n/m | n/m |
| 0x4E53 | 1 | 2672 | 2672 | n/m | n/m |
| 0x4E87 | 1 | 516 | 516 | n/m | n/m |
| 0x4E88 | 1 | 3281 | 3281 | n/m | n/m |
| 0x4E6F | 2 | 1687 | 3375 | n/m | n/m |
| 0x4E70 | 2 | 750 | 1500 | n/m | n/m |
| 0x4E62 | 2 | 1312 | 2625 | n/m | n/m |
| 0x4E63 | 2 | 281 | 562 | n/m | n/m |
| 0x4E64 | 2 | 1711 | 3422 | n/m | n/m |
| 0x4E5F | 3 | 656 | 1970 | n/m | n/m |
| 0x4E60 | 3 | 812 | 2436 | n/m | n/m |
| 0x4E61 | 2 | 1969 | 3938 | n/m | n/m |
| 0x4E67 | 2 | 2937 | 5875 | n/m | n/m |
| 0x4E73 | 1 | 891 | 891 | n/m | n/m |
| 0x175C | 1 | 6719 | 6719 | n/m | n/m |
| 0x4E8C | 1 | 1641 | 1641 | n/m | n/m |
| 0x4E8D | 1 | 46 | 46 | n/m | n/m |
| 0x4E8E | 1 | 2250 | 2250 | n/m | n/m |
| 0x4E65 | 1 | 2859 | 2859 | n/m | n/m |
| 0x4E50 | 1 | 2109 | 2109 | n/m | n/m |
| 0x4E8A | 1 | 375 | 375 | n/m | n/m |

### Ippon-Datara (R25E4)

| id | times used | avg ms | total ms | hits on player | dmg/hit |
|---|---|---|---|---|---|
| 0x0 | 1 | 0 | 0 | n/m | n/m |

### Ibaraki Doji (REA75)

| id | times used | avg ms | total ms | hits on player | dmg/hit |
|---|---|---|---|---|---|
| 0x0 | 1 | 3016 | 3016 | n/m | n/m |
| 0x2 | 4 | 70 | 280 | n/m | n/m |
| 0x4E62 | 2 | 843 | 1687 | n/m | n/m |
| 0x4E2C | 1 | 4734 | 4734 | n/m | n/m |
| 0x4E30 | 2 | 2297 | 4594 | n/m | n/m |
| 0x4E28 | 3 | 453 | 1360 | n/m | n/m |
| 0x4E26 | 2 | 1875 | 3750 | n/m | n/m |
| 0x4E5F | 1 | 797 | 797 | n/m | n/m |
| 0x4E21 | 1 | 938 | 938 | n/m | n/m |
| 0x4E68 | 1 | 46 | 46 | n/m | n/m |
| 0x4E27 | 1 | 1782 | 1782 | n/m | n/m |
| 0x4E2D | 1 | 2578 | 2578 | n/m | n/m |
| 0x4E3F | 1 | 2297 | 2297 | n/m | n/m |
| 0x4E31 | 1 | 610 | 610 | n/m | n/m |
