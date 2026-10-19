# Yoritomo (chr 16102) static action table: FOUND (2026-10-19, offline, game not touched)

Question: is there a static table of ALL Yoritomo action ids (all Ki / HP phases), so the Training move list (44 ids from recorded
fights) can be completed without playing? **Yes. 173 action records, 173 unique ids; all 44 recorded ids are in it.** 129 new ids
were added to `notes/boss-action-ids-all.csv` and `mod/boss_moves.h` regenerated (`tools/gen_boss_moves.py`).

## Where
- File: LFM `archive_01`, extracted at `data/extract/lfm/archive_01/FA/5FBD421BF96B8514DA056584B9507A22DC7956B1793995A113FBA714CFFA54`
  (232,452 bytes; nioh3-engine checkout). Layout: `u32 n = 16`, then 16 `(offset, size)` sub-tables.
- Sub-table 1 (offset 0x70D4, 58,128 bytes) = **173 action records of 336 (0x150) bytes**:
  - `+0x0C u16` **action id** (the value the Training ActLog / move list uses: 0x0, 0x2, 0x6B, 0x3FB, 0x4E21 ...).
  - `+0x20 u16` **motion number**: 1000-1104 = attack motions, 10-83 / 520 / 61-62 = movement, steps, turns, 30004-32101 = hit
    reactions / Ki break, 6900-6920 = unknown non-attack, 0 / 65535 = no motion. Several action ids share one motion (variants:
    e.g. motion 1040 = 0x4E68, 0x4E69, 0x4E6A, 0x4E6B, 0x4EA8-0x4EAB).
  - `+0xF0 u32` **action object hash** = an entry of CE_YORITOMO_00's `MotorCharacterSetting.CharacterActionObjectNameHashArray`.
  - Other fields (flags at +0x06, +0x10, +0x1A, chains in the middle block) not decoded. Example: record 0x4E21 has 0x4E22 in a
    later field, matching the recorded chain 0x4E21 -> 0x4E22.
- Parser: `py -I tools/chr_action_table.py <archive_01 file>` (TSV: row, action_id, motion, flags, obj_hash).

## How it was bound to Yoritomo (not guessed)
1. `notes/boss-char-assets.csv`: CE_YORITOMO_00 (char key 03e94ad8) -> loader 086ad65c -> setting DB 01be1363 (104 action objects).
2. `py -I data/tools/n3rdb.py get <Nioh3/package> root 01be1363` + `n3kod.py`: the 104 action-object hashes.
3. Scanning all of `data/extract` for those hashes as raw u32: exactly one file holds them (59 of 104), the archive_01 file above.
4. Independent check: the same file's action-id field holds **all 44 recorded Yoritomo ids** (incl. late-phase 0x4E53/0x4E54/0x4E5C/
   0x4E60/0x4E62/0x4E8A, the Ki-break 0x4E37/0x4E38/0x4E3E/0x4E40/0xFA6 of `kibreak-deathstance-9895.md`, the 0x18D1 long attack).

## What did not work (for the record)
- `Nioh3.dump.exe`: the 0x4Exx ids occur as imm32 only 0-3 times each (struct offsets); no static id table in the executable.
- Raw u32 scan of all extracted data for the recorded ids: no file. (The ids are u16 inside the 336-byte records.)
- The action-object hashes are not kidshash / ktgl hashes (constant step 0x013652DF between neighbours is no power of 31); the
  scene DB 960adb1c and NameDatabase 6bd90ddc are not in the shipped package. Not needed once the archive_01 table was found.

## New rows in the move list (129)
Kinds were assigned from the motion number only (unverified, `label_lowconf` starts with `static table:`):
- **70 attack-family rows** (motion 1000-1104) and **8 motion-69xx rows**: kind empty = shown in the move list. 53 of the 70 share
  a motion with an already recorded attack (label says `same motion as 0x....`), so they are likely phase / range / direction
  variants of known moves; 17 (motions 1073, 1081, 1100-1104) have no recorded sibling = moves never seen in the logs.
- **51 rows kind M** (movement / steps / turns, hit reactions / Ki break, no-motion system states): hidden unless "Show twitches".
Next live step (when someone plays): drill a few never-seen rows (0x4E76, 0x4E77, 0x4E78, 0x4E79, 0x4E7A, 0x4E87, 0x4E72) to name them.

## Generalises to other bosses
206 archive_01 files have the same 16-sub-table layout with 336-byte action records in the 0x4E20+ range (10-568 records each).
For any boss: setting DB action-object hashes (step 1-2 above) -> the archive_01 file containing them -> `tools/chr_action_table.py`.
