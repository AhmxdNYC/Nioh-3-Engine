# Boss duel flow: where the boss id comes from (static, 2026-10-08)

Static only (decomp_ro, extracted LFM). Game not run, mod untouched. RVAs v2.02.

## 1. The shared flow
- Each mission script in `lfm\archive_09` carries its own compiled copy of the `Function::*` library. `Function::DuelCommonFlow` / `DuelCommonFlowAnyRange` start (script 17B9D09013790E34..., offset 0x6d44) with:
  `local7 = Duel::GetBossPlacementID()` (no script args), then `IsKneelDownFlag(local7)`, ..., `IsAliveBoss`, `Mission::BossBattleMode`.
- Correction to boss-fights.md s.1 format note: call operands (0x1b/0x1f/0x3b) are the symbol **id** (1-based, 2nd field of the sym table), not the index. Decoding with the index shifts every name by one.
- So the boss is chosen by the native, not by the script.

## 2. Duel::GetBossPlacementID (0x2399A48) -> BossPlacementIdForArea (0x1C16DC0)
- Arg = u32 at `[[[vmctx+8]+8]+0x90]+0x120`, i.e. **the mission id**: `Mission::GetMissionID` (binding 0xA46580) returns exactly the same field. So `GetBossPlacementID() == BossPlacementIdForMission(GetMissionID())`.
- 0x1C16934 `PlacementSetFindById`: walks list A of the placement manager `[0x47098E0]` (shared_ptr pairs) and returns the set whose `+0x118 == id` and whose 6 resources (+0x10,+0x38,+0x60,+0x88,+0xB0,+0xD8) are loaded (0xC92FC0).
- Set `+0xB0` = **enemy placement table**: resource data = `{u32 magic 0x20042200, u32 count}` + count x 0x138-byte records (0x2BE574 index, 0x2F7378 count), then a string pool.
- Result = `id` of the **first record (table order) with `byte[+0x9C] & 4`**. Fallback (no such record): scan `[[0x45B9E30]+0x668]` 0xD8 records vs `[0x45B9E30]+0x8D0` table (`u16 +0x60 == id`), returns 0x3A.
- Duel::IsAliveBoss (0x2399AD8) uses the same id -> ActorFindById (0x135B78) -> `0x7E11F0(actor)==0`.
- 0x1C2448C is the third caller (event/BGM path keyed by `u16 +0x60` in the 100-byte table at `[0x45B9E30]+0x8D0`).

## 3. Enemy placement table on disk (lfm\archive_03, magic 00 22 04 20)
Same layout as runtime. Record 0x138: `+0 u32 placementId`, `+4 u32 characterId`, `+8 u32 packed` (0x1f37xx...), `+0x9C u8 flags` (bit 0x4 = boss candidate; final bosses also have 0x8/0x10/0x20), `+0x134 u32 name offset` into the trailing NUL-separated pool (names like `CH08_MAIN03_DOKANYAMA_MIDDLE_BOSS_KUMAGAI`).

## 4. Mission record -> placement set (the per-mission record)
- Mission DB = `lfm\archive_03\52\BD29D45533CFACBF...` (magic 00 22 04 20, 130 records x 0x198; accessor 0x377B6C `MissionDbGetRecord`, root `*[0x45B9E30]`).
- 0x1C1D148 builds list A: for each mission record, a placement set from the 6 u32 ids at `+0x54..+0x68` and the u16 at `+0x158`; 0x1C1D288 builds list B (mission scripts) from `+0x6C` (script id, looked up via hashmap `[0x475A1A0]`, 0x3F5C30).
- ~~Open~~ Resolved in §7: the ids are LFM **file ids** looked up in a shipped id table, not hashed. Correction: rec `+0x6C` is NOT the script (it is a 7th table); the mission script is rec `+0x50`.

## 5. The mapping rule (offline)
`boss(mission) = first record r (file order) of the mission's enemy table with (r[+0x9C] & 4)`; `boss_char_id = r[+4]`, `boss_placement_id = r[+0]`, name = pool string at `r[+0x134]`.
- Validated on scripts that also pass an explicit id to `Mission::BossBattleMode`: 35A880867F1E (1161567/1162241/1160051), 679CBE96AA72 (20127), E00A59553306 (563857) are all flag-0x4 records in their tables. 59 scripts: the computed first-boss id itself is a constant in the script ("high").
- Because the record->file join is open, the CSV picks the script's enemy table by content: the table containing the most of the script's int32 constants (opcode 0x29), excluding the shared table EE903C7C. Confidence: high = boss pid appears in the script; medium = >=3 constants matched; low = 1-2; none = 28 scripts (the trivial `DuelCommonFlow()` skeleton, no placement constants: needs the record join or the live log).
- Caveats: world tables (`CHxx_WORLD_*`) hold many flag-0x4 records (open-world bosses); for a mission run the engine uses the mission's own set, so "low" rows matched to a world table may be wrong. Superseded by §7 / `boss-map-all.csv` (deterministic join, mission id included).
- Defeat flag: not from this path. BossBattleModeEnd/SetMissionClearFlag clear the running mission (boss-fights.md s.4); no per-boss flag id is read by GetBossPlacementID.

## 6. Next
1. ~~Close the record -> file join~~ done (§7).
2. Optional: log `BossPlacementIdForMission` args/returns once in a live fight to confirm one row, and decode the fallback (DataMgr+0x668) for the 24 missions with no flag-4 record.

## 7. Resolution: file id -> LFM entry, boss for every mission (static, 2026-10-08)
**File id -> LFM path (exe).** `LfmFileIdMapBuild` 0x29481E8 creates `g_LfmFileIdMap` `[0x475A1A0]`, and `LfmFileIdTableLoad`
0x29489D8 loads the LFM entry named by the exe string `AE_50B70535D0EDD195...` = `archive_08\AE\50B70535D0EDD19523951019C533295915AE7D4F65FE37E351E09D3858A8C5`.
Format: `u32 0, u32 n (8,778)`, then n x `{u32 fileId, i32 rel}`; `entry+rel` is the ASCII path `XX/<62 hex>` (the LFM/loose name).
Each `{fileId -> row}` is inserted with `FileIdMapInsert` 0x2947CC4; `FileIdMapFind` 0x3F5C30 returns the row (or -1) and
`LfmFileIdGetPath` 0x3F5BA0 (407 callers) returns the path string. Coverage: 4,823 of the 4,824 LFM entries (all but the table
itself) plus 3,953 loose `archive\XX\` files. So there is no hash to invert: the table is the join.

**Mission record -> set (exe).** `PlacementSetsBuildFromMissionDb` 0x1C1D148 builds a set only if all 6 ids at `+0x54..+0x68`
exist in the map (0xE2E720 -> FileIdMapFind). `PlacementSetCtor` 0x1B779E8 stores the ids at set `+0x100..+0x114` and
**`u16 rec+0x158` at set `+0x118`**, so the mission id that `GetMissionID` / `BossPlacementIdForMission` compares is the
missions.md "mission key" (`+0x158`). The 0x4B8 loader puts ids[0] in its first slot (+0x470/flag +0x4A0), which
`PlacementSetBindSlots` 0xCC7830 binds to set `+0xB0`: **the enemy table is rec `+0x54`**. Data agrees: only `+0x54` files have
0x138-byte named records; `+0x58..+0x68` are other 0x20042200 tables (often empty), `+0x6C` holds 0xA4-byte position records
(list B, 0x1C1D288, previously misread as scripts), and `+0x50` is the mission's bytecode script (archive_09; 110/110 unique).

**Result** (`notes\boss-map-all.csv`, generator `re\scripts_re\boss_map_all.py`, read-only):
- 130 records: 110 shipped (each = one archive_09 script, unique mission id); 20 rows (17 unnamed-stage, 3 internal) carry
  ids 0x2C000-0x31000 that are not in the id table, so their set is never built (no script either).
- 86 missions have a boss (first `+0x9C & 4` record of the `+0x54` table); 45 of them hold more than one candidate (multi-boss
  fights, phase 2 records: the first is what GetBossPlacementID returns).
- 24 missions have no flag-4 record (11 with an empty enemy table: wave/tutorial stages that spawn from script; 13 bossless
  tables: 4 dojo Novice/Adept, tutorials, hidden-wave, unnamed): GetBossPlacementID takes the DataMgr fallback there.
- Old duelflow CSV vs the join: high 58/59 agree. The one high conflict (B44697EFB296) has both ids as script constants;
  the join gives its own table (DUEL_MASTER_NPC_002_BOSS 2024153, mission 52826). 14 low/medium rows were the wrong table
  (e.g. dojo "Way of" missions now resolve to TEACHER_MISSION_* bosses; Mirage Fortress -> CH02_MAIN03_FUTAMATA_BOSS0001 84220).
- The 28 skeleton scripts: 12 now have a boss (8 DUEL_MASTER_NPC_* hidden-wave duels, DUEL_QUEST_07_TOGOKU_BOSS,
  PARALLEL_102_HANZO, PARALLEL_114_ENEMY_WB5_01, EMAKI_TEST_ENEMY_00); 16 have no flag-4 record (fallback path).
- Symbols: `re\symbols\incoming\placement-renames.tsv` (15 new, 4 in-place description fixes incl. 0x1C1D288 rename).
