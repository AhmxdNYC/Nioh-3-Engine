# Eternal Rift / NG+ (Shogun's Journey): what the exe says (static, 2026-10-08)

Static only (decomp_ro, strings.tsv, script name-table dump). Game not run, no mod code. RVAs v2.02. Names: `re\symbols\incoming\rift-renames.tsv`.

Summary: **no Rift floor / boss-id table was found** (no string in the exe, nothing in extracted LFM/RDB text; the data is a binary param table we have not located). What was found is the NG+ state model. Answer to "what does NG+ reset": **nothing is cleared; each journey level has its own mission-flag block and clear-db byte** (slot), and NG+ switches the slot.

## 1. Journey-level state: `[[0x47514D0]+8]` (g_MissionMgr state)
| off | use (evidence) |
|---|---|
| +0x10 u8 | **current journey level 1..5**. Slot selector kind 6 = "use +0x10" (0x285448, 0x284944, 0x2853FC). 0x37A434 gates a boss aura on `+0x10 > 2`. |
| +0x11 u8 | enemy-scaling row key (0x469C0 picks the row via 0x285AC0: row.byte0xE == +0x11 and row.level <= `[+0x408 + slot*4]`) |
| +0x24 i32, +0x28 i32 | mission mode. `+0x24==2 && +0x28!=0` (0x285D38) => **slot 0**, a separate world outside the NG cycle. Candidate for the Rift/Crucible-like mode; unverified. |
| +0x44 u8 | **highest unlocked journey level**. `System::UnlockNextMissionMode` (0x23B1F48): `if(+0x44 < +0x10) +0x44 = +0x10; 0x1C28B84()`. |
| +0x45 u8 | pending next level, set by 0x2180AEC (`+0x44 = max(+0x44, +0x45-1)`; level>=2 sets mission flag 0x7F, level>=3 sets flag 400/0x190 through MissionFlagSet 0x70275C; then 0x22877D8(level)). |
| +0x130 + slot*0x400 | mission flag bytes (0/1, 0xFF = definition default). 6 slots. |
| +0x408 + slot*4 | per-slot ints (level limit read by 0x285AC0) |

## 2. Slot function (0x285448; same logic inlined in 0x284944 and 0x2853FC)
`sel` = {char kind; int mode; ...}. kind==6 -> kind=[+0x10]; mode==0 -> mode=[+0x24]. **mode==2 -> slot 0; else kind 1..5 -> slot 1..5.** Used by `MissionFlagGet` (0x284944: `byte[+0x130 + slot*0x400 + flagIdx]`, flagIdx from 0x284A5C) and the clear db.

## 3. What NG+ resets
Nothing. Changing `+0x10` makes the game read a fresh 0x400-byte flag block and a fresh per-slot clear byte, so every mission and boss looks undefeated; older levels stay intact. For a rematch feature: **flip the slot, do not clear flags.** Caveats: changing it mid-mission changes every flag/clear/scaling read, so do it only at a menu or stage reload, offline.

Clear db (`g_ClearDb=[0x4751848]`, data at +8):
- map +0x2C8: flag byte at entry+0x24, bit0 = cleared per slot (0x217F9B0, 0x217F908 write it)
- map +0x428: per-mission record, bytes at +0x1B/+0x23/+0x2B/+0x33/+0x43 plus slot; +0x53/+0x54 first-clear reward flags
- map +0x590: key u16 id, FNV-1a basis 0xCBF29CE484222325, byte at entry+0x12+slot (getter 0x107BB78)
- map +0x2E0: 4-byte id variant (getter 0x37AFC0)
- `SetMissionCleared` = **0x1C1DB88(missionId)**, the body of `Mission::SetMissionClearFlag` (0x23A582C): sets all of the above, rewards, retry points, level-dependent score.
- **Un-clear routine:** `Hell::ResetClearMapOnlySmallHell` wrapper 0x239FFB8 -> **0x219FEB4(id, 0)** -> `0x217E884(db, id, 0, notify)`; the wrapper only acts when mission record type byte `+0xC0 == 3`. `SetClearMapOnlySmallHell` 0x23A00F0 is the same with 1. `IsClear...` 0x239FAA0 reads 0x2845C8.

## 4. Record layouts found
- Mission table `DataMgr(_DAT_7ff7539c9e30)+0x630`: 0xC4 B records, hash lookup by u16 mission id (0x285808). +0xC flag bits (bit10/11 = first-clear reward), +0xBC area, +0xC0 type (0, 1, 3; 3 = hell/small-hell clear-map-only), +0x30 position. Running mission object holds the id at +0x120.
- Table at DataMgr +0x600: 0x84 B records, key u16 id; byte +0x79 bit3 gates slot use (0x283568, 0x284228).
- Table at DataMgr +0xA88: 0x34 B records (0x1C1425C), +0x2C u16 = hell id; active index = `[charMgr]+0x2BEF`. `Emaki::IsClearWaveBattle` = 0x107BB78(clearDb, 0xFFFF). That is Battle Scrolls (other agent); listed because Rift probably reuses wave/hell machinery.
- Rewards: `Mission::GetGameClearRewardNormal/Hard` = 0x2280FA0(1 or 2) -> 0x2280C84(0x4472, 0xCD6F) / (0x7730, 0x2641).

## 5. Rift / Mugen leads (unresolved)
- Script API `Mugen::IsAllMugenBossesDead` (wrapper 0x23E4228) and `Mugen::GetPeriodicDamageAreaAppearTimeFrame` exist. "Mugen" (eternal) is the likely internal name of a Rift/boss-rush mode. Only one LFM script mentions it (`data\extract\lfm\archive_09\E1\67012D4464864CD049CF5F816CA324E606E7FBF1935E71596EA5F89A470521`, user function `sub_event_tut20_mugen_enemy`, uses Hell::* calls). `archive_09\F9\5A3A552EA629567F2D607C04894087D152BEF1885C4D164842F849CDA61205` has `VOICE_FLAG_RIFT/RIFT2/RIFT3` (stage S03_KOD_01_REIJU). Neither gives a floor or boss list.
- No "Rift", "Shogun", "Heir", "Journey" string in the exe; UI text lives in the text DB, which is not extracted.
- Script native table: `{name*, fn*}` x 1072 at 0x397E770 (read from `re\Nioh3.dump.exe`; wrapper RVAs for all names were dumped, e.g. Hell::* 0x239Fxxx, Emaki::* 0x239Axxx, Mission::* 0x23A2xxx..0x23A6xxx).
- Next steps: (1) log-only hooks on 0x2180AEC, 0x1C1DB88, 0x217E884 and Mugen wrappers during a real Rift entry to capture ids; (2) find the Mugen object (wrapper 0x23E4228 reads it) and its floor list; (3) find the DataMgr loader for +0x600/+0x630/+0xA88 (typeinfo `QuestProgressDatabase`, `BaseParamDatabase`) and dump its LFM source file.

## 6. Use for boss rematch
- Whole-game fresh bosses: set `[[0x47514D0]+8]+0x10` to another unlocked level (each level has its own flags and clears). Needs `+0x44 >= level` (use 0x2180AEC). Risk: scaling row (+0x11, +0x408) and save-level meaning change; offline only.
- Single boss: `0x217E884(db, missionId, 0, 0)` un-clears; `MissionFlagSet` (0x70275C) for flags.
