# Boss / enemy character assets (static, 2026-10-08)

Companion: `notes\boss-char-assets.csv` (352 non-cosmetic character ids; columns char_id, name, asset_ids_files, boss_flag_evidence, name_hint). Game not run.

## Id -> asset chain (verified)
1. **Char id = kidshash(name)**: `CharacterKeyHashArray` in `MotorCommonSetting` (root `0x6d011726`, obj 61ad16cf) equals `re\scripts_re\kidshash.py <NAME>` (CE_ONI_00 = 0xc6700dee, CS_CERBERUS_00 = 0x0d40c7d3). 2138 names (parallel arrays: Key, SettingObjectName, SettingLoaderObjectDatabaseName, SettingLoaderObjectName). Names are readable plain ASCII romaji; list also in `data\stage_names.tsv` (type `chara`; col1 = loader DB KTID).
2. Loader DB (tiny `ObjectDatabaseFile`, ~100 B, one `CharacterSettingLoader`): `SceneDatabaseResourceNameHashArray` (scene DB), `CharacterSettingObjectDatabaseResourceNameHash` (setting DB KTID), `CharacterSettingObjectNameHash`.
3. Setting DB = `MotorCharacterSetting` (+ one `Model::WorldPQ`): `CharacterActionObjectNameHashArray` (motion/action objects, count = moveset size), `CharacterActionGroupIDArray`, `CharacterModelNameHash` (WorldPQ -> `DisplaysetObjectNameHash` -> Model displayset -> ModelData/ShaderBind/RigBin etc.), `ColorVariationNum/Names`. Model/motion files live in root.rdb (ModelData, G1A, RigBin...), keyed by those hashes.
4. **No HP, boss flag, or AI id in any of these**. `MotorCharacterSetting` has none. HP/boss-gauge data is in the LFM game-table layer (`archive\archive_01`, 507 column-oriented binary tables: header u32 n=16 then n (offset,size) pairs) and placement data (`Duel::GetBossPlacementID` is a placement id, 7-digit ids like 9900154 in scripts, not a char key). Not decoded here. Root `0x9aa2c3a6.bin` (43 KB) is a hash table containing 74 of these char keys (mostly CE_ mobs); purpose unknown.

## Coverage caveat
Only 248/352 loader DBs exist in package rdb; 104 do not (incl. 18 of 27 CS_, 8 of 15 KY_). Absent => DLC/unshipped or separate layer. Also 1402 setting DBs are 2788-byte stubs with 0 actions (placeholders).

## Boss candidates (no flag found; ranking is circumstantial)
- **CS_ (27)**: only unique named set pieces, no mob species: CERBERUS, ENKO(monkey), GUHIN, HOUOU_HOU/HOUOU_OU (phoenix pair), KINSYACHI, KONGOJISHI, KURAMATENGU, KUSANAGI (+_FINAL_00/01, _NINJA, _SAMURAI), MURAKUMO, NARIKAMA, NEKO/NEKOTSUKI, SHINROKU, SHIROGARASU, SYAMICHOUROU, TENGEN; most have _00/_01 phase pairs. Settings have few/0 actions (cutscene models, or boss class loaded elsewhere). Best boss-name list available; most are absent from the rdb.
- **KY_ (15)**: boss-like yokai duplicates of CE_ mobs: BOSHITSUKI, DAIDARABOCCHI, GAKIDAISHO, GOGANKI, HIRUKO_P, JAKOTSUBA (98 actions, a full moveset), MIKOSHI, NURIBOTOKE, OTENGU, OTOROSHI, RAIJU, SHUSHINKI, TSURARAONNA, YAGYOHEI, YASHA. Likely mini-boss/ big variants.
- **CE_ named/part chars**: YOSHITSUNE (+_BLADE/_WING), YORITOMO, SHINGEN, NOBUHARU, YOSHINOBU, MASAKAGE, RYOUMENSUKUNA_00-02, IBARAKIDOJI (+_ARM), HINOENMA, ENKI, HIRUKO, SHIGURUI_BIG/SHURA (+_ARM), DAIDARABOCCHI_LD, SORANAKI_* (arm/bg/min parts), SUNEKOSURI_2ND, YOUKI_POWER/SPEED, WILDDOG, NINJA. Action count > ~1.3x CE median (82) flagged in CSV.
- **CN_ (28)** named NPCs/historical figures (KUNIMATU, SHINGEN, MUNENORI, HIMIKO, ...); some are duel opponents; CN_JUBE has 610 actions (player-like kit).
- **CP_HUMAN_<weapon>**: shared humanoid weapon movesets (2958 actions each): the base for human bosses/duelists, not bosses themselves.

## Next steps
Decode archive_01 tables (find the one with ~2k rows keyed by char key/id with HP column) and the placement data to turn candidates into confirmed boss flags; use `Chara::SetMaxHpCoefficient`/`Chara::GetLinkEnemyID` binders in the exe to find the table reader.
