# Boss duel flow: where the boss id comes from (static, 2026-10-08, WIP)

Static only (decomp_ro, extracted LFM). Game not run, mod untouched. RVAs v2.02.

## 1. The shared flow
- Each mission script in `lfm\archive_09` carries its own compiled copy of the `Function::*` library. `Function::DuelCommonFlow` / `DuelCommonFlowAnyRange` start (script 17B9D09013790E34..., offset 0x6d44) with:
  `local7 = Duel::GetBossPlacementID()` (no script args), then `IsKneelDownFlag(local7)`, ..., `IsAliveBoss`, `Mission::BossBattleMode`.
- Correction to boss-fights.md s.1 format note: call operands (0x1b/0x1f/0x3b) are the symbol **id** (1-based, 2nd field of the sym table), not the index. Decoding with the index shifts every name by one.
- So the boss is chosen by the native, not by the script.

## 2. Duel::GetBossPlacementID (0x2399A48) -> BossPlacementIdForArea (0x1C16DC0)
- Arg = u32 at `[[[vmctx+8]+8]+0x90]+0x120`: an id stored on the running script's host object (not a script argument).
- 0x1C16934 `PlacementSetFindById`: walks list A of the placement manager `[0x47098E0]` (shared_ptr pairs) and returns the set whose `+0x118 == id` and whose 6 resources (+0x10,+0x38,+0x60,+0x88,+0xB0,+0xD8) are loaded (0xC92FC0).
- Set `+0xB0` = **enemy placement table**: resource data = `{u32 magic 0x20042200, u32 count}` + count x 0x138-byte records (0x2BE574 index, 0x2F7378 count), then a string pool.
- Result = `id` of the **first record (table order) with `byte[+0x9C] & 4`**. Fallback (no such record): scan `[[0x45B9E30]+0x668]` 0xD8 records vs `[0x45B9E30]+0x8D0` table (`u16 +0x60 == id`), returns 0x3A.
- Duel::IsAliveBoss (0x2399AD8) uses the same id -> ActorFindById (0x135B78) -> `0x7E11F0(actor)==0`.
- 0x1C2448C is the third caller (event/BGM path keyed by `u16 +0x60` in the 100-byte table at `[0x45B9E30]+0x8D0`).

## 3. Enemy placement table on disk (lfm\archive_03, magic 00 22 04 20)
Same layout as runtime. Record 0x138: `+0 u32 placementId`, `+4 u32 characterId`, `+8 u32 packed` (0x1f37xx...), `+0x9C u8 flags` (bit 0x4 = boss candidate; final bosses also have 0x8/0x10/0x20), `+0x134 u32 name offset` into the trailing NUL-separated pool (names like `CH08_MAIN03_DOKANYAMA_MIDDLE_BOSS_KUMAGAI`).
