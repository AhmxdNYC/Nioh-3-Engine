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
- Mission DB = `lfmrchive_03*\BD29D455...` (magic 00 22 04 20, 130 records x 0x198; accessor 0x377B6C `MissionDbGetRecord`, root `*[0x45B9E30]`).
- 0x1C1D148 builds list A: for each mission record, a placement set from the 6 u32 ids at `+0x54..+0x68` and the u16 at `+0x158`; 0x1C1D288 builds list B (mission scripts) from `+0x6C` (script id, looked up via hashmap `[0x475A1A0]`, 0x3F5C30).
- Open: these ids are resource ids; the LFM file name is a 256-bit hash whose input is unknown (sha256/sha3/blake2s of the id in 9 encodings: 0 hits over 1,033 ids), so the record -> file join is not yet offline. Ids are not in `load_inf_NN.bin` (that holds sizes).

## 5. The mapping rule (offline)
`boss(mission) = first record r (file order) of the mission's enemy table with (r[+0x9C] & 4)`; `boss_char_id = r[+4]`, `boss_placement_id = r[+0]`, name = pool string at `r[+0x134]`.
- Validated on scripts that also pass an explicit id to `Mission::BossBattleMode`: 35A880867F1E (1161567/1162241/1160051), 679CBE96AA72 (20127), E00A59553306 (563857) are all flag-0x4 records in their tables. 59 scripts: the computed first-boss id itself is a constant in the script ("high").
- Because the record->file join is open, the CSV picks the script's enemy table by content: the table containing the most of the script's int32 constants (opcode 0x29), excluding the shared table EE903C7C. Confidence: high = boss pid appears in the script; medium = >=3 constants matched; low = 1-2; none = 28 scripts (the trivial `DuelCommonFlow()` skeleton, no placement constants: needs the record join or the live log).
- Caveats: world tables (`CHxx_WORLD_*`) hold many flag-0x4 records (open-world bosses); for a mission run the engine uses the mission's own set, so "low" rows matched to a world table may be wrong. The mission id itself is not yet in the CSV.
- Defeat flag: not from this path. BossBattleModeEnd/SetMissionClearFlag clear the running mission (boss-fights.md s.4); no per-boss flag id is read by GetBossPlacementID.

## 6. Next
1. Find the resource-id -> LFM-name function (callers of 0x3F5C30 / the `[0x475A1A0]` map builder) to close the record -> file join; then all 130 missions map deterministically, including the 28 skeleton scripts.
2. Or log `BossPlacementIdForMission` args/returns once in a live fight to confirm.
