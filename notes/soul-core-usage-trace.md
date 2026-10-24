# Soul Core usage trace (2026-10-15, this PC)

Read-only investigation. The game was not launched. Nioh3.exe PID started 12:27:03 and was still running at about 12:43.

## 1. What the logs show

Logs are in `C:\Program Files (x86)\Steam\steamapps\common\Nioh3\`: `Nioh3EngineFix.log` (last write 12:43), `Nioh3BossRematch.log` (12:41:30) and `Nioh3Training.log` (12:30:11).

**No log line identifies the Soul Core use.** Grepping `soul`, `core`, `guardian`, `item use` and `ItemUse` across all three logs finds only unrelated lines:
- `Sites CORE 13/13 resolved`. This is the add-on's "CORE" site group, not a soul-core feature.
- D3D12Core.dll paths in the OSD lines of the EngineFix log.
- `LoadStats` lines whose text includes "core(s)".

Why nothing was captured:
- `Nioh3BossRematch.ini` has `BossLog=1`, `RematchLog=1` and `InvLog=0`.
- The `EmakiItemUse_MenuHandler` hook (F1F060) is installed. It only fires for scroll-type items. The log has 0 `EmakiItemUse` or `Q7` lines.
- `InventoryAddItem` (54D324) is hooked only when `InvLog=1`, so it was off. A Soul Core use that consumes or adds an inventory item was therefore not recorded.
- No hook exists for any soul-core, guardian-equip, shortcut or item-use function other than F1F060.
- The `Player::AddGuardianSpirit*` hooks are script natives. They log only inside the boss-fight window, plus 30 s after it.
- `Nioh3Training.log` stops at 12:30:11 and contains only `BBM`/`ARENA`/`BOSSMON`/`PERC` lines.

**Only possibly related events** (unproven):
- `Nioh3BossRematch.log` has `RematchLog L5: TrophyUnlock id=17 flag=1 caller=+19E65C3 (other)` at:

| Time | `t=` |
|---|---|
| 12:40:18.842 | 787.325 |
| 12:41:12.463 | 840.945 |
| 12:41:17.937 | 846.420 |
| 12:41:30.794 | 859.277 |

- These come from the `TrophyUnlock_body` hook (`TrophyUnlock` = 0x297F2DC).
- Trophy idx 17 = PS5 trophy #18. I did not map it. The trophy table is in `notes\boss-achievements.csv`.
- If the Soul Core use was around 12:40 to 12:41, an unlock tied to it is plausible, but nothing proves it. The caller `+19E65C3` is not documented in the notes.
- The earlier log lines (12:28:49 `L7: getters`, `MissionStartFromSave`, Emaki table dump) are the mission load, not the Soul Core.

Conclusion: the exact time and item id of the Soul Core use are not in any log. A capture run needs the hooks in section 3.

## 2. What the repo and decompiles say about soul cores

**Item data**
- `notes\items.md`: category 900000000 has 119 rows.
  - They are the probable soul cores: type word w33=928, price 500, no name text.
  - The text template `{} Soul Core` (0x010d29f1) and `Mortal Soul Core` (0x012f2b15) exist in text group `17/8F/676FE6`. The `{}` is filled from the enemy-name key. Soul Core UI text is in `17/00/7317BD`.
  - Category 600000000 contains a "Soul Core Box".
- `notes\boss-loot.md` and `notes\boss-drops.md`: drop soul core item ids per boss are INFERRED (e.g. 132302), with low confidence.
- `notes\boss-rewards.md`: the first defeat guarantees a soul core, later ones are random.

**Save and persistence**
- `notes\amrita-tools.md` and `re\symbols\symbols_x64dbg.txt`:
  - `SaveWriteSoulCoreHistory` = 0xA55B94. It writes field 0x1798DF25 (soul-core Transformation history, 2000 entries x 28 B = 8000-byte array). Its callers are `SaveUserWritePlayerSection` (0x105AE20) and the read counterpart `SaveUserReadPlayerSection` (0x1044680).
  - `SaveUserReadEquipSection` = 0x1047F30 and `SaveUserWriteEquipSection` = 0x105EC60 hold equipped gear, soul cores and shortcuts (field hashes 0x54B78304, 0x6B52DB66, 0xAB0E9943, 0x36BFE4B6, 0xF3EF06E4).
  - The Transformation history means "use" is probably a transformation or fusion, which writes a history row.
- Inventory:
  - `InventoryAddItem` = 0x54D324. Its record is 0xE8 bytes (+0 u16 id, +2 base id, +0x28 serial).
  - The inventory is at global `[0x4751530]`, array +0x224A60 (stride 0xE8), count +0x23B4E0.

**Guardian spirit script natives** (symbols; no decompile in `D:\nioh3-re\decomp` for these, and a recursive search timed out)
- `ScriptBind_Player_AddGuardianSpirit` 0x23A7714, `...NoNotice` 0x23A77AC, `...WithTimer` 0x23A783C
- `DelGuardianSpirit` 0x23A7DF8
- `EquipGuardianSpirit` 0x23A7FF4
- `IsEquipGuardianSpirit` 0x23A9F70
- `HasGuardianSpirit` 0x23A8C7C
- `GuardianSpiritForceLost` 0x23A8B54 and `...ForceReturn` 0x23A8BD0
- `RecoverGuardianAction` 0x23AAC1C
- UI natives: `OpenGuardianGainNotice` 0x23B6F68 and `SetGuardianSpiritHUDVisible` 0x23B8234
- Online: `Online::PlayerGuardianSpirit` 0xADC7B0 and `...InPossessionForGimmick` 0xADC81C

**Item use and shortcuts**
- The native item-use handler found so far is `EmakiItemUse_MenuHandler` 0xF1F060 (scroll use only: reads +0x20, +0x12, +0x0F, +0x28 and calls `EmakiMissionRequest`).
- Shortcut natives: `Item::SetShortCut` 0x23A210C, `Item::FindShortCut` 0x23A191C, `Player::GetActiveItemShortcut` 0x23A8474, `Player::IsItemSetActiveShortCutSlotDirection` 0x23AA098, `UI::FixShortCutChange` 0x23B3330, `UI::SetItemShortcutHUDVisible` 0x23B82A8.

**Not found:** no symbol or note names a "use soul core", "yokai shift" or "soul core activate" function. The Cheat Engine table "V6 infinite item/soul core use" (Nexus mod 148, `notes\boss-modding-community.md`) implies the use path decrements a stack count.

## 3. Hypothesis and a log-only hook plan

**Function chain (hypothesis, unverified)**
1. The player presses the item shortcut. The HUD shortcut system (`GetActiveItemShortcut` / the shortcut slot) selects the active inventory record, and the item-use dispatcher runs.
2. The dispatcher branches on item type. For type 928 (soul core) it calls a soul-core handler, which looks up the yokai via the enemy-name key.
3. The handler applies the yokai effect (likely a guardian-action or transform state on the player) and decrements or consumes the inventory stack.
4. The handler appends a row to the soul-core history (the field that `SaveWriteSoulCoreHistory` writes).
5. Notifications and HUD follow: `UI::OpenGuardianGainNotice` or the shortcut HUD.

**Log-only hook plan** (all pass through the original; use the existing `bosslog.cpp` `OnEntry` pattern with caller RVA from `sp[0]`)
1. **Free first:** set `InvLog = 1` in `Nioh3BossRematch.ini`. `InventoryAddItem` 54D324 then logs id, record bytes, caller, function start and `via` return addresses for every add. This shows whether a soul core use adds or exchanges items.
2. Add `OnEntry` hooks for the natives `EquipGuardianSpirit`, `DelGuardianSpirit`, `HasGuardianSpirit`, `IsEquipGuardianSpirit`, `GuardianSpiritForceLost`/`ForceReturn` and `Item::SetShortCut`.
   - Move them out of the fight-window gate.
   - Dump their VM args with `FmtVmArgs`.
3. Hook the inventory remove or consume side. The callee of `InventoryAddItem`'s siblings is near 0x54CD9C, 0x54EFA0 and 0x54E574 (seen in `InvSource`). Find the stack-count decrement by walking from `[0x4751530]+0x224A60` with a `.pdata` neighbour search. Log the item id, the new count and the caller.
4. Hook the save writer `SaveWriteSoulCoreHistory` 0xA55B94 and the history array at field 0x1798DF25. Log the history count before and after, plus the caller, to identify the transform record.
5. **Discover the dispatcher without static knowledge:**
   - Add a temporary debug hook to the item-use path. Capture `RtlCaptureStackBackTrace` at the first `InventoryAddItem` or stack-decrement call during a Soul Core use.
   - Resolve each frame with `FnStart` (`RtlLookupFunctionEntry`), as `HkInvAdd` already does.
   - Then add the top 3 distinct function starts as permanent logged hooks.
6. After the log shows the chain, resolve names with the static dump (`re\Nioh3.dump.exe`, Ghidra project under `D:\nioh3-re\ghidra_proj*`) and add them to `re\symbols\symbols_x64dbg.txt`.
7. Also record a snapshot of the item record (0xE8 bytes) before and after the use, to find the fields that change.

Caveat: all RVAs above come from `symbols_x64dbg.txt` and `sites.json` for build TimeDateStamp 0x6AA7C106 (v2.02). The soul-core-specific chain in section 3 is inferred.
