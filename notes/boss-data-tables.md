# Boss data tables: placement id -> enemy character id (data side, 2026-10-08)

Result: **not resolvable from the extracted data.** No file we can read statically holds the mission/placement -> enemy
table. `notes\boss-map-data.csv` therefore lists script + constant placement ids with char_id empty (evidence column says why).

## What was searched (read-only, game untouched)
- All 17,086 ObjectDatabaseFile (root+system) decompressed and scanned for the script placement ids (8061822, 8063630,
  16889, 575908, 160566 ... as u32 LE): only noise (1603 etc. coincidences in 46-92 MB shader/model DBs). Big 7-8 digit ids: 0 real hits.
- LFM archives 00-19 (02/06/15 newly extracted here: G2A_PACK/DMPP animation packs, no tables). Archive roles: 05/09/14/16 = bytecode scripts,
  17 = UTF-16 text tables (hash->string), 10 = 12-byte hash-pair lookup tables, 03 = timeline/event records (u32 index headers), 01 = collision, 13 = shaders.
  None holds `Character::Create` placement records.
- Stage DBs: `data\extract\stage` is only Octree/Landscape/ProcPlacement (foliage/props). Per-stage `MotorStageSetting` (type e1d40600, 151 DBs,
  file KTID = `data\stage_names.tsv` key, e.g. S02_HEI_01_KAMOSHI = 4e0ccc15) lists ObjectDatabaseNameHashArray (21-30 DBs: octree, sound, set models, nav).
  Cross-check: **0 of those hashes are character DBs**, so enemies are not bound to stages through MotorStageSetting.
- Characters: 1,952 `MotorCharacterSetting` DBs (type d186ceeb) + `CharacterSettingLoader`; each = model name hash + model DB. Table:
  `notes\chara-setting-table.tsv` (file_ktid, obj_ktid, model_name_hash, model_db_ktid). 267 are named char-creation parts (data\sel_chara.txt); enemy names are not in any NameDatabase we have (the two NameDatabase fdata are missing from disk).
- Hash notes: file KTIDs are not kidshash or FNV/CRC of the stage name (tested); kidshash.py only matches property names.

## Where the table must be (next steps)
1. Runtime only: placement records are built in memory by `Character::Create` (0x1180740) from mission data loaded outside RDB/LFM extracts: log (placementId -> actor -> model) in a real fight.
2. Possible unexplored source: `TypeInfo::Object::Script::Code` (1,664 DBs) and `Placeable::Location` (82 DBs), `Timeline::Script` (100), MotorEventSetting (100): not decoded; the 8-digit ids may be hashes of Location object names.
3. Scripts -> stage: stage strings in script pools (boss-fights-table.csv `stages`) join to MotorStageSetting via stage_names.tsv.
