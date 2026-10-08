# Boss defeat checks: what the game records (static, 2026-10-08; game not run, no mod code)

CSV: `notes\boss-defeat-checks.csv` (boss, mission, check_kind, value_or_id, evidence; one row per boss x check, 164 rows). Names: `re\symbols\incoming\defeat-renames.tsv`. RVAs v2.02. Builds on boss-fights.md, boss-achievements.md. Confidence is in each evidence cell (high/medium/low).

## Check kinds
- `progress`: `Player::SetStoryProgress(N)` value. Stored per category record (`MissionIdCategory` 0x5591e8: <7000 main, <8000 DLC1/sub range, <9000, else); `0x215c630` fires trophies when `record+4 >= N`. Test: story progress >= N. Values are set by the boss script at defeat (`BossDefeatEventMain`, `sub_event_boss_clear_check`, `sub_event_jouka_event`).
- `flag`: mission flag id (`MissionFlagGet` 0x284944, 6 NG slots OR-ed by `MissionFlagReadAllSlots` 0x1075f50). Same id space as `Mission::SetMissionFlag`.
- `clear_map`: `g_ClearDb` = `[0x4751848]`. Two maps: **mission** (key u32 = the 16-bit id scripts pass to `Mission::IsMissionClearedMissionID`, FNV-1a, table +0x2E0, byte `node+0x38+slot`, getter `MissionClearedGet` 0x37AFC0) and **quest** (key u16, table +0x320, byte `node+0x13+slot`, getter 0x2833F0, script `Quest::IsQuestClear`). Value is prefixed `mission:` or `quest:`.
- `trophy` (extra): the trophy idx the boss script passes to `System::Trophy::RequestUnlock`; `TrophyIsUnlocked` 0x297F14C is a persistent "defeated at least once" bit for these.

## Findings
1. **Script RequestUnlock args are literal trophy idx.** Decoding the 110 scripts gives all 21 call sites (previous note said ids 7000-7004; wrong): 4 Hitokoto start, 5 Hamamatsu castle (Naotora), 6 Saigagake BossDefeatEventMain, 7 Daidairi boss_clear_check, 8 Reiju BossDefeatEventMain, 9 Imperial Palace boss_clear_check, 10/11 final script, 30/44/49 Nekomata hub (Sunekosuri/Toyo/Kusozuki), 41/42/43 Demon rtm scenes, 45 Otoroshi, 46 Murakumo, 47 Ryotan hot spring, 48 Higashiyama Kusanagi talk, 50 Edo veterans, 51-54, 61, 63 DLC1. So the main-story bosses have two redundant detectors (script + `0x215c630` polling).
2. **Hell Rising achievements (idx 51-63) are not "Hell Rising" code.** The extracted data has no DLC2 scripts (only a `S91_DLC2_SHIMABARAWORLD00OB` stage name), but the DLC1 scripts (`S90_DLC1_*`, Shadows of Edo) unlock them:
   | idx | trigger | where |
   |---|---|---|
   | 51 | boss clear of the Edo-castle DLC stage (also sets flags 545, 556) | script 710C8B93B28A `sub_event_boss_clear_check` |
   | 52 | Chokodo boss clear (SetStoryProgress 7220, flags 551/578) | 5A34CB851898 `sub_event_boss_clear_check` |
   | 53 | DLC finale, story progress 7700 after Dokanyama | 0CA8409CACB7 / 45C15A1FFB1A `after_dokanyama_npc`, `sub_event_jouka_event` |
   | 63 | same function, after `IsOverMissionModeVeryHard/SuperHard` checks (difficulty-gated finale) | same |
   | 61 | after `IsMissionFlag(576)`; flag 576 set in the Kaneiji boss clear check | 45C15A1FFB1A `BossDefeatEventParallel`, 0CA8409CACB7 |
   | 54 | Jube (Yagyu Jubei) defeated: `Character::IsDieOrDying(8073284)` then `kitan_37_after_boss_talk_jube` | 2B59B8417AAE (S90_DLC1_04_KANDA) |
   | 62 (0x3e) | C++: a kill where the victim's type word `& 0xFFFFFFF0 == 0x8600` (stat 99 +1 alongside) | `0x2185C74`, `0x1FFFEDC`, `0xAF910` |
   | 60 (0x3c) | C++: first time `0xFFBDD0` gets a positive scaled amount (kind 1/2, param-table product); gameplay stat, not a boss | `0xFFBDD0` |
   | 55-59 | not found: targets are 0 in the table (0x44B9170) and nothing calls them; probably unused or set by other DLC data not shipped here | - |
   The idx-to-title order given in boss-achievements.md for the DLC stays a guess; the concrete bosses above are from code. Stat counters (`StatSet` 0x605034) only map 8 stats to trophies 14,15,17,23,24,25,34,36 (table 0x3978EB0), none are DLC.
3. **Mission and quest ids are random 16-bit values, not 1000/2000 steps.** `archive_03\88\598A...` (146 rows x 28 B) lists mission objectives; word 5 low 16 bits is the mission id passed to `IsMissionClearedMissionID`, the high bits are the objective index (e.g. 115161 = 49625 + 65536). `archive_03\31\BD17...` (49 kitan rows x 132 B): word 21 = quest id (low16) | prerequisite quest id (high16); word 3 = name. Anchors from scripts: Matsuo script reads 12009/39461/60021 (the three Heian Befouled objectives), teacher script reads the nine "Way of the" ids 62949/28261/6750 (Warrior N/A/V), 55129/57740/39908 (Ninja), 9971/41121/10278 (Onmyo).
4. **Boss flags with high confidence:** Pride 296, Envy 297, Obsession 298, Otoroshi 528 (crows 519-521), Murakumo 529 (temps 515-518), Great Tengu 338, Ryotan hot spring 527, veterans 422/423 (both needed, ACH 50). Pride/Envy/Obsession/Otoroshi are set inside functions named `*_boss_defeat`/`sub_event_check_boss_battle_end`, so they mean "defeated", not "started".
5. **Story ladder (set at defeat):** 1050 Hitokoto, 1450 Shingen (Saigagake), 2100 Great Tengu, 2500 Yoritomo, 3520 Himiko (Calamity's Seed), 4000 antiquity done, 5000 To the Beginning, 5120 Honnoji, 5310 Yoshinobu, 6200 Hiruko, 6500 ending; DLC1 7220 Chokodo, 7700-7710 finale.
6. Not resolved (kept `unresolved` or `low` in the CSV): mission ids for Hitokoto/Futamata/Takasugi/Enenra/Princess Tsubaki/Mystery of Rajomon/Beast of the Underworld/Crucible Tour/Solitary Blade (objective texts do not name them); which of scripts 96A326DD1D0A / BFC230194A06 is Munenori vs Hanzo; the setter of flag 187 (Okita); Death Veil identity; web-name to mission-id pairs marked low are story-order guesses.

## How to use (still no mod code)
Boss "defeated" test per web boss: take the highest-confidence row in the CSV. For a persistent check prefer `clear_map quest:` (sub-missions, 49 kitan quests, exact) or `trophy`/`flag` (Demons, Otoroshi, Murakumo); for main bosses use `progress`. For a live confirmation, hook `TrophyUnlock` 0x297F2DC and `MissionFlagSet` 0x70275C during one kill and compare with the CSV.
