# Eternal Rift and the Mugen natives (static, 2026-10-08)

Static only (decomp_ro, native table in `re\Nioh3.dump.exe`, `re\scripts_re\lfm_disasm.py`, existing notes). Game not run, no mod code. Names: `re\symbols\incoming\mugen-renames.tsv`. Confidence: high on the Mugen stubs, medium on the Rift data layout.

## Verdict
1. **`Mugen::*` has no C++ implementation behind it.** Two natives exist; both are constant-return stubs. This confirms `boss-rematch-paths.md` ("unused stub") and **corrects `boss-rift-ngplus.md` section 5**, which called the wrapper a Rift lead that "reads one bool". It reads nothing.
2. **There is no Rift spawner, floor table, boss pool or own defeated-tracker.** "Eternal Rift" is an *era/region tab* of the ordinary mission table. Its content is 9 normal sub missions plus 3 tutorials. Bosses are fixed per mission by the mission script and stage placements; tiers differ by recommended level.
3. So the Rift cannot host arbitrary bosses. Use the Emaki route (section 5).

## 1. The Mugen natives
Native table `{name*, fn*}` (16 B entries) at 0x397E770:

| entry | name string | wrapper | body |
|---|---|---|---|
| 0x397E120 | 0x3BF8C60 `Mugen::IsAllMugenBossesDead` | **0x23E4228** | ArgFrame ctor 0x1399B0(param); `v=0`; 0x1398E0(frame,&v) pushes a u8 return; returns **false** always |
| 0x397E130 | 0x3BF8C80 `Mugen::GetPeriodicDamageAreaAppearTimeFrame` | **0x23E41CC** | same shape, pushes int via 0x2E5A80 with value **0** |

Neither wrapper dereferences any global or object (compare `Mission::IsTrainingMission` 0x23E0094, which reads the current mission id and compares with 0x6F24). Only xrefs are the native-table entries (data refs 0x4FE7354 / 0x4FE7360). strings.tsv has no other "Mugen" string. The mode these were written for (periodic damage area plus all-bosses-dead check) was cut or never shipped; stubs remain for script-API compatibility.

## 2. Scripts that call Mugen::*
**None.** Raw scan of all 4,824 files under `data\extract\lfm` for "ugen": exactly one file matches, `archive_09\E1\67012D44...0521`, and only as the *string* `sub_event_tut20_mugen_enemy` in its string pool (offset 0x56CF, tutorial-event id string, i.e. tutorial 20 "infinite dummy enemy"). `lfm_disasm.py --calls/--stmts` shows no CALL_NATIVE to either Mugen native and no function of that name. The earlier note's "user function ... uses Hell::*" was a misread of that pool string.

## 3. What the "Rift" actually is (data)
Mission table = LFM `archive_03` entry 398 (130 x 0x198 B; see `battle-scrolls.md` section 2 and `battle-scrolls.csv`). Rift rows have era `+0x160 == 1`. Decoded rows (csv rows 23-25, 119-127):

| mission id | name | boss/content | unlock_prog +0x156 | rec lv +0x178/17A/17C |
|---|---|---|---|---|
| 0x8D87 (36231) | Basic Controls | tutorial | 0 | 1 |
| 0x8487 (33927) | Expanded Controls | tutorial | 0 | 1 |
| 0x6F24 (28452) | Training Ground | dummy area (`IsTrainingMission`) | 0 | 1 |
| 0xF5E5 62949 / 0x6E65 28261 / 0x1A5E 6750 | Way of the Warrior Novice/Adept/Veteran | Yagyu Munenori | 1153 / 2000 / 5000 | 165,385,465 / 209,429,509 / 257,477,557 |
| 0xD759 55129 / 0xE18C 57740 / 0x9BE4 39908 | Way of the Ninja N/A/V | Hattori Hanzo | same | same |
| 0x26F3 9971 / 0xA0A1 41121 / 0x2826 10278 | Way of Onmyo N/A/V | Saito Fuku | same | same |

The 9 training rows: kind sub, menu_cat 3, region `+0x15C` 0xDF (tutorials 0x32), story_prog `+0x166` 6500. "Floors" = 3 series x 3 tiers. The record has no encounter list; `unlock_prog` (1153/2000/5000) gates the tier and `rec_lv` sets scaling.

Boss pool and selection: fixed by mission -> script -> placements. The Munenori duel script is `C6DA46D668EC` (placement list file `archive_03 E9/C7BBAA...` = `PARALLEL_126_BOSS_001..008`): it runs `Mission::BossBattleMode(placementId)` in turn for six placements (1268863, 1268341, 1269826, 1264363, 1264386, 1265215), then `Mission::SetMissionClearFlag`. Chara-table ids per boss (hp varies per phase): Munenori 22638/22669/22700/22731/23277, Hanzo 22762/22799/22836/22873/23230/23249/23290, Fuku 22910/22935/23419/23426 (`boss-enemy-params.csv`). No tier-specific boss id was found; tiers appear to reuse the script and differ by level (unverified which mission loads which variant).

## 4. Rewards and defeated-tracking (all generic mission machinery)
- Defeat state = the normal clear db (`g_ClearDb`, per journey slot; `Mission::IsMissionClearedMissionID`). No Rift-specific tracker. Re-fight = Battle Scroll mission replay (`battle-scrolls.md`).
- Hub script `C34725FB97C6` (functions `teacher_placement_check`, `kunimatsu_shadow_placement_check`) reads all 9 clears. Per teacher it plays talk events (52607 Fuku, 34045 Munenori, 27840 Hanzo; stages 2-6 keyed to each tier clear), sets mission flags 364/365/366 from the three Veteran clears and, **per Veteran clear, creates a loot placement** with `Item::CreateItemPlacement`: 247459 (Fuku Vet 0x2826), 245171 (Munenori Vet 0x1A5E), 241262 (Hanzo Vet 0x9BE4). These match the Smithing texts the web lists for Veteran.
- Novice/Adept: generic first-clear mission rewards (clear-db +0x53/+0x54) and teacher text pages `CN_MUNENORI_TEXT_2..4`, `CN_HANZO_JR_TEXT_2..4`, `CN_OFUKU_OLD_TEXT_2..4` whose unlock flag id equals the mission id (`boss-codex.csv`).

## 5. Could the Rift host arbitrary bosses?
No. There is no boss list, spawner or floor index to feed. Changing a Rift mission's boss means editing its script/placements (LFM; loose files do not override, see `battle-scrolls.md` section 5.4). Alternative rematch paths stay as in `boss-rematch-paths.md`: (1) Emaki mission 0xCC96 with a patched record `+0x10` placement, (2) mission replay / start queue, (3) un-hide excluded duels. The three teachers are already refightable through mission replay.

## 6. Corrections to earlier notes
- `boss-rift-ngplus.md` section 5: Mugen is a stub, not a Rift lead; "no Rift floor table" is by design (Rift = era-1 rows above). Its NG+ slot findings stand.
- `battle-scrolls-online.md`: 3 teachers x 3 tiers confirmed by data.

## Open items
1. Which script/stage each tier loads (Novice vs Veteran Munenori) and where recommended level is applied.
2. First-clear reward grant for these rows (clear-db +0x53/+0x54 path, 0x1C1DB88).
