# Boss achievements -> trigger chain (static, 2026-10-08; game not run)

Steam list: AppID 3681010, 64 achievements. Story descriptions are spoiler-hidden on Steam; filled from PS5 lists (keengamer, powerpyx, insider-gaming). Table: `notes\boss-achievements.csv`. Names: `re\symbols\incoming\boss-ach-renames.tsv`. RVAs v2.02.

## Unlock code
- Steam side: `STEAMUSERSTATS_INTERFACE_VERSION013` via `SteamInternal_ContextInit`. `0x1fdc0f0` (idx < 0x40) calls ISteamUserStats vtbl+0x30 (SetAchievement) with name `ACH_%03d` (fmt `0x3a9ae24`, prefix `0x3a9adc0`), number from table `0x44B9170` (64 x 12 B {achNumber, flag, progressTarget}); idx == ACH number. `0x1fdc3b0` = SetStat for `ACH_%03d_PROGRESS` (vtbl+0x18).
- Game side: `TrophyUnlock(idx, flag)` = `0x297f2dc` (19 callers), `TrophyIsUnlocked` = `0x297f14c`, `0x297f3f8` resyncs all 64 to Steam. Script API `System::Trophy::RequestUnlock` (wrapper `0x23b1ed0`) exists but only one stage script uses it (ids 7000-7004, not bosses). Boss trophies are C++-polled after a mission result: `0x1c3413c` -> **`0x215c630`**.
- Index order = PS5 trophy order minus 1. Checked via progress targets: idx2=10 (level 10), idx3=100, idx13/14/15=10, idx21=13 (masters), idx27=38 (Chijiko), idx38=26 (bases), idx40=22 (crucibles).

## Chain
| idx | Achievement | Boss/target | Trigger |
|---|---|---|---|
| 6 | The Old Tiger Departs | Takeda Shingen | main-mission progress >= 2000 |
| 7 | Unbreakable Cause | Minamoto no Yoritomo | >= 2500 |
| 8 | The Queen's Eyes Open | Himiko | >= 3500 |
| 9 | What Must Be Done | Tokugawa Yoshinobu | >= 5300 |
| 10 | Delusion Destroyed | Hiruko | >= 6200 |
| 11 | As Shogun | ending | >= 6500 |
| 4, 5 | Wanderer in Time, Guiding Mirror | story events | >= 1000, >= 1153 |
| 41/42/43 | Kunimatsu's Pride / Envy / Obsession | Demon of Pride / Envy / Obsession | mission flag 0x128 / 0x129 / 0x12A |
| 45 | Ruckus in the Sanctuary | Otoroshi | mission flag 0x210 |
| 46 | Pain, Pain, Go Away | Murakumo | mission flag 0x211 |
| 47 | Distant Memories | - | mission flag 0x20F |
| 50 | Nothing Left to Learn | - | flags 0x1A6 and 0x1A7 |
| 20/21 | Masterful Pupil / A God Among Men | masters (13) | per-master bit via `0x217f6c4`; counter target 13 |

Story thresholds: `0x215c630` reads a per-category record (`0x5591e8`: id <7000 main, <8000 sub, <9000, else; record via `0x107b604`) and fires when its value >= N, so N behaves as a monotone cleared main-mission id (1000, 1153, 2000, 2500, 3500, 5300, 6200, 6500). The boss per threshold is from the trophy descriptions and era order (S01_SEN, S02_HEI, S03_KOD, S04_BAK), not from code. Flags: `0x1075f50(obj, out, flagId)` ORs the 6 NG-slot bytes at `[[mgr+8]+0x130+slot*0x400+flagIdx]`, the same array `Mission::SetMissionFlag` writes (boss-fights.md section 4).

## Not resolved
- Hell Rising DLC idx 51-63 (The One-Eyed Master, Yagyu Shinkage Style Virtuoso, Devoted to Treason, Regrets Unsettled, One Thing After Another, Noble Bout, Friend of the Keian Nigitama, Savior of Keian, Paranormal Purifier, Aid Offered, Keian's Enlightenment, Taken in Stride, The Best Defense); their callers (0x3c from `0xffbdd0`; 0x3e from `0x1fffedc`, `0x2185c74`, `0xaf910`) not traced; idx-to-name order for the DLC is a guess.
- Main-mission id -> stage name; Senji Toyo (idx 44) caller; master ids.
- Practical use: hook `0x297f2dc` (idx in ecx) to log the exact moment and flag state of each boss kill; the flag ids 0x128-0x12A, 0x20F-0x211 are confirmed boss/quest completion flags for rematch re-arm.
