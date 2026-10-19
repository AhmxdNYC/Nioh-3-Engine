# Yoritomo (chr 16102): where are hits / damage / Ki damage? (2026-10-19, offline, game not touched)

Question: do the static data hold Yoritomo's per-action attack values (hit count, damage, Ki damage, hit reaction, hitbox),
linked to action ids 0x4E21..0x4EB9? Source: the archive_01 file `FA/5FBD421B...` (see `yoritomo-move-table-search.md`,
`yoritomo-action-fields.md`). New CSV columns are in `notes/yoritomo-action-fields.csv`, made by `tools/chr_action_table.py --csv`.

## Verdict
- **Damage / Ki damage per action: NO, not linkable statically.** Sub-table 2 (182 x 192 B) is very likely the attack
  hitbox + attack-value table, but **nothing in this file indexes it**. Every u16 field of sub-tables 1, 3, 4, 5, 7 and 8
  was tested across all 374 family files ("max value < sub-table 2 row count, and close to it"). No field passes. So the
  action-to-hitbox link is outside this file, most likely in the motion data (motion number `+0x20`). Not decoded.
- **Hit count per action: NO.** No event class or field marks the three known multi-hit moves the same way (see the
  cross-check below).
- **"Is this an attack" flag: YES (strong).** The tail block `+0x136..+0x14A` (column `phase_tail`) is set on 88 of 96
  attack-motion records and on 0 of 77 others. The twitches 0x4E30/0x4E31/0x4E32 have none. This says a record is an attack. It
  does not give damage.
- **Projectile / spawned-object hits: located (structure proven, values not).** Event ids `20000+N` are **sub-table 7
  row N**. They are **not** action ids, which corrects `yoritomo-action-fields.md`. 0x4E74 (a known repeated hitter) has
  **no** local events. Its only attack content is spawns 1 and 2 at frame 226, so its hits come from spawned objects.
- Other archives: tables with rows keyed by 0x4E73/0x4E74/0x4E75 exist in archive_02 (48-B and 12-B rows) and in other
  archive_01 files. But ids 0x4E21+ are reused by **every** character, and none of these tables was bound to Yoritomo
  (no CE_YORITOMO hash in them). They are not usable as Yoritomo data without a binding.

## Proven (offline, numerically)
| claim | evidence |
|---|---|
| Local event id (< 1000) = **sub-table 5 row** (93 x 208 B, event definitions) | max local id < sub5 count in **354/354** family files (318 within 3 of the end). Yoritomo: max 92, sub5 = 93 rows |
| Event id 20000+N = **sub-table 7 row N** (51 x 336 B, spawned objects / bullets) | max N < sub7 count in **212/212** files (185 tight). Yoritomo: N 1..50, sub7 = 51 rows. Row 0 = default |
| Sub-table 7 `+0x110..+0x11A` = up to 6 **sub-table 8 row** indexes (30 x 80 B) | max < sub8 count in 94/94, 88/88, 66/66 ... files. The only cross-table index found besides the two above |
| Action `+0x2A/+0x2C/+0x2E` (57/58/59, 60-64, 72) = **sub-table 4 group keys** (81 x 48 B) | sub4 `+0x00` holds exactly these keys (57, 58, 59, 60-65, 71, 72, 74) at group starts. Groups list bone ids (202-262) + radii, so this is the body collision (hurtbox / pushbox) set. It is not damage. Sub5 events 41-44 and 47 switch it (`+0x50` = 57/58/72/59/60) |
| Sub-table 3 = AI selection rules, not damage | twitches 0x4E30/0x4E31 (20016/20017) have many sub3 rows (same layout as attacks). The rows hold small weights / conditions (+0x10 = 34/35/53/54/149) and no damage-like values |
| `phase_tail` (+0x134 + 6k: (x, type, frame) x4) = attack-only | 88/96 attacks vs 0/77 others. Type 1 occurs in every attack (0x4E68 1@51, 0x4E6C 1@83, 0x4E74 1@77), type 5 early (frame 12-15) |

## Guessed (not verified)
- **Sub-table 2 = hitbox + attack values.** Rows come in groups. Each group has a first row with `+0x28 = 22` and offsets
  (80, ±30, 60), which looks like a weapon-attached capsule. It is followed by rows with `+0x28 = 0/1`, a bone id at `+0x08`
  (52-243) and xyz offsets at `+0x2A..+0x2E` (body spheres). Value candidates: `+0x12` (16-55; maybe attack power or motion
  value), `+0x14` (0-20), and the pair `+0x1C/+0x1E` (40/90, 52/160, 108/360, 135/450, 300/300). `+0x1E` is about 2-3.3 x
  `+0x1C`, so the pair may be HP / Ki damage. `+0x10` is 0/15/20/25/60/70/180 (180 only on the 145-152/163-165 group,
  which has 300/300; maybe an explosion or grab). None of this can be tied to an action without the link.
- **Sub5 event classes** (column `local_event_decode`: `id:class<value>@frame+len`):
  - `r` (sub5 `+0x32`, for example 21/22/23 = 125/150/35): probably a play-rate % window. It also appears on movement
    records (8 of them), so it is not a hit marker.
  - `m` (`+0x34` = 0x1A00/0x1A0A, values `+0x3C`/`+0x64`, for example 250/300 ... -700/500): probably root-motion /
    homing distance windows.
  - `s` (switch the collision set, or a hash).
  - `p` (`+0x8C` value).
- Sub-table 7 rows 1-4 (0x4E74 uses 1 and 2, 0x4E8D uses 3 and 4): `+0x10` = 5000 (speed?), `+0x18/+0x1A` = 500/1000
  (life / range?), `+0x48` = 140/130. Sub8 links 0/4/19 and 0/4/27. Sub8 rows hold values such as (500,100,40,10) and
  (0,5,10,120,120). These are maybe the bullet hit params. Not decoded.

## Cross-check against known facts
| action | known | local events (sub5 class) | spawns | phase_tail |
|---|---|---|---|---|
| 0x4E68 | hits repeatedly | r125/r150/r35/r125 + m x3 (frames 20, 46, 89) + p80 | - | 5@15 7@28 1@51 |
| 0x4E6C | hits repeatedly | 39/40 (class o), s57 x3, s-hash x2, s125 | - | 5@13 1@83 0@92 6@112 |
| 0x4E74 | hits repeatedly | **none** | sub7 1, 2 @226 | 5@12 0@22 1@77 3@113 |
| 0x4E30 | twitch, no damage | 14 (s1841) only | - | none |
| 0x4E31/0x4E32 | twitch, no damage | none | - | none |

Result: no common "hit" event class across the three multi-hitters, so the hit windows are not in the event slots.
Together with "nothing indexes sub-table 2", hit timing and damage most likely come from the motion file of each `+0x20`
motion number. Only "attack vs not" separates them cleanly (`has_attack_block`).

## New CSV columns (`yoritomo-action-fields.csv`)
`local_event_decode`, `n_ev_r`, `n_ev_m`, `spawns_sub7` (`N(s8:a,b,c)@frame`), `phase_tail` (`type@frame`),
`has_attack_block`. The existing column `event_action_links` holds **sub7 spawn ids**, not action ids.

## Next (needs data not in this file, or the game)
1. Motion files for numbers 1040/1050/1090: search them for u16 values < 182 at frames 50/89/98/116 (0x4E68) to find the
   sub-table 2 link. That would give hit count + damage statically.
2. Live: read player HP before and after one drilled 0x4E68 hit and match it to the sub2 `+0x12`/`+0x1C` candidates.
