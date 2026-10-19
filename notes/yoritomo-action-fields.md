# Yoritomo (chr 16102) action records: field decode (2026-10-19, offline, game not touched)

Source: archive_01 file `FA/5FBD421B...` (see `yoritomo-move-table-search.md`). Output: `notes/yoritomo-action-fields.csv`
(173 rows), made by
`py -I tools/chr_action_table.py <file> --csv --log "<Steam>/Nioh3/Nioh3Training.log" 16102`.
Checked against: the Training log ACTLOG lines for 16102 (162 transitions with `ran N ms`). The log has **no** MOVE lines and
**no** player-HP-loss lines for Yoritomo, so damage and hit counts could not be checked against logged data.

## Verdict
- Names: **no**. The `+0xF0` action-object hash is in `MotorCharacterSetting.CharacterActionObjectNameHashArray` (setting DB
  01be1363, 170/173 records), but the objects behind it live in scene DB 960adb1c, and the NameDatabase 6bd90ddc is listed in
  the RDB index (`root/0xad80d5d0.fdata`) but that fdata is **not in the installed package** (153 fdata files, no 0xad80d5d0).
  No static path to names. `+0x11C` is a second 32-bit hash (62 distinct values, same count as `+0xF0`); it is not in the setting DB.
- Next action: **yes, proven** (`+0xE0`).
- Hits / damage / Ki damage: **no**. No field matches; damage is probably in sub-table 2 (182 x 192 B) or set by AI/script,
  and how actions link to it is not decoded.
- Durations: **not in the record**. No u16 field correlates with logged ran-ms (best r = 0.44 over 25 actions). Length
  comes from the motion file.

## Container (proven: the header counts match the sub-table sizes exactly)
Sub-table 0 is a header. From word 2 on it holds a (count, record size) pair per sub-table: 1 = 173x336 (actions),
2 = 182x192, 3 = 503x128, 4 = 81x48, 5 = 93x208, 6 = 2x16, 7 = 51x336, 8 = 30x80, 9 = 5x128, 10-13 empty, 14 = 2x128, 15 = 151x16.
- **Sub-table 3 is keyed by action id** at +0x00 (133 distinct keys; every key except 0xFFFF is an action id in sub-table 1;
  up to 16 rows per action). Probably per-action rules or branches. The row count is in CSV column `sub3_rules`. The rest
  of the row is not decoded, and the AI combo links seen in the log (0x4E60->0x4E62, 0x4E4E->0x4E52, 0x4E53->0x4E54) do
  **not** appear as u16 anywhere in sub-tables 1-15 except as the keys. So combo choice lives in the AI script, not here.
- Sub-table 6 (2 rows) = (587, 0x4E3F) and (588, 0x4E45). Sub-tables 2/4/5/7/8 are not decoded.

## Action record (336 B) fields
| off | field | status | evidence |
|---|---|---|---|
| +0x0C u16 | action id | PROVEN | all 44 logged ids |
| +0x0E u16 | FFFF except 0x4E22 (0xFFA6) | unknown | |
| +0x20 u16 | motion number | PROVEN-ish | families match move kinds; variants share motions |
| +0x40 16 x (u16 id, u16 frame, u16 len) | **event slots** (empty = FFFF,0,FFFF) | layout PROVEN, meaning GUESSED | frames rise inside a move (0x4E68: ids 21/22/23/21 at frames 50/89/98/116) |
|  | id < 1000 | local event (hitbox / effect / armour?) | GUESS | up to ~92; sub-table 5 has 93 rows (unverified index) |
|  | id 10000-10600 | shared event (SE / VFX?) | GUESS | the same ids repeat across many moves (10001/10005 pair, 10250-10255 set) |
|  | id 20000+ | **an action id** (same number space as +0x0C / sub3 keys) | number space PROVEN, role GUESSED | 0x4EB4 -> 0x4E46..0x4E49 at frame 54; 0x4E78/79/8C/67/70 -> 0x4E38..0x4E3C. Probably a spawned helper's action or a branch target at that frame. Column `event_action_links` |
| +0xA0 5 x (u16 value, start, end) | schedule A | GUESSED (play-rate % or move speed) | 0x4E74: 130@0, 150@43, 250@109, 400@155, 125@210 (frames rise) |
| +0xBE 5 x (u16 value, start, end) | schedule B | GUESSED (tracking / turn rate?) | steps 0x4E71: 400@4, 330@8, 260@12, 180@16 (falls off) |
| +0xE0 u16 | **next action on finish** | PROVEN | matches every logged non-AI successor: 0x4E21->0x4E22, 0x4E5C->0x4E60 (3/3), 0x4E7B->0x4EA7 (3/3), 0x4E30->0x4E31. 2 = idle, self = looping middle part (0x4E22, 0x4E25, 0x4E31, 0x4E3C ...). 0x3F1 = alt idle for 0x4E81-0x4E88 / 0x3F0 group |
| +0xE4,+0xE6 u16 | always 1 | constant | |
| +0xEA,+0xEC u16 | attack-only pair (158-250) | unknown (not duration) | only set on attacks and a few steps |
| +0xF0 u32 | action object hash | PROVEN | setting DB 01be1363 array |
| +0x11C u32 | second hash | unknown | 62 distinct |
| +0x24 u16 | attack-only value 60-175 | GUESS: AI reach / range | only on attacks |
| +0x34 u16 | 1002-1075 on 40 attacks | GUESS: alternative / follow motion number | motion-number range |
| +0x2A/+0x2C/+0x2E u16 | 57/58/59 (or 60-64, 72) | unknown (shared table indexes) | |
| +0x00,+0x02,+0x04,+0x06,+0x10,+0x18,+0x1A,+0x1C,+0x110-+0x114 | bit flags | not decoded | value sets differ by kind: e.g. +0x06 bit 0x4000 + 0x08/0x10 only on movement/system states; +0x116 = 0x400 only on hit reactions; +0x10 = 0x0906 on 87/96 attacks |
| +0x130-+0x14E | attack-only small ints (pairs) | unknown | only filled on attacks; exported raw as `tail130` |

## CSV columns
`row, action_id, motion, kind_by_motion, next_action, loops, obj_hash, hash_11c, flags*` (raw), `atk_param24, alt_motion34,
atk_paramEA, atk_paramEC, n_events, n_local_events, n_common_events, events` (`slot:id@frame+len`), `event_action_links, sched_a,
sched_b` (`value@start-end`), `tail130` (raw), `sub3_rules`, plus from the log: `log_n, log_ms_median, log_next` (observed
successors with counts), `next_action_matches_log`.

## Next (not done)
- Damage / hits: find the link from action to sub-table 2 (182 rows, fields look like attack values: 20/10/5, 150, 525,
  90/160 ...) or to sub-table 5 (93 rows). Test: is the local event id an index into sub-table 5?
- Live check (needs the game): log player HP before and after one drilled move, then match the damage to sub-table 2 values.
