# Boss scripts -> missions (static, 2026-10-08, game not run, no mod code)

CSV: `notes\boss-missions.csv` (104 rows = every script of `boss-fights-table.csv`; columns script, mission_id, mission_name_en, region, type, evidence).

## Result in one paragraph
There is **no numeric mission-definition table that links to the scripts**. 61 of 104 scripts are joined by the stage-id strings in their own string pool (`S01_SEN_04_HITOKOTO`, ...); `mission_id` in the CSV is that stage id. 43 have no stage string (Edo scripts carry only `S00_EDO_ALL`; 42 carry nothing, the stage is passed as data) and stay unresolved or hint-only. `mission_name_en` is the **web mission name** (join key to `boss-per-mission-online.csv`) chosen by place name, not read from a game table. Confidence: stage string = high for place, medium for which web mission it is; Edo and hint-only rows = low.

## What the game data does contain (so you know what is NOT there)
- Text = LFM `archive_17` (UTF-16, `{u32 id, u32 nchars, text}`, 23 languages per group; see `boss-names.md` section 1). Param tables = `archive_03` (magic `00 22 04 20`, u32 rows at +4, rows from +8).
- **Sub-mission (kitan) list**: `archive_03\31\BD17B1A6...3201F30111F`, 49 rows x 132 B from +8. Word 3 = English name text id (Fated Land, Kodama Seeker, Insatiable Hunger, ... rows 39-48 are DLC: Left Behind, Stagnant Shogunate, Waterside Temptation, Agony of the Mito Tokugawa, Savior's Light, Keian Noble Bouts, Way of the Yagyu, Light of a Million Koku, Yagyu Principles, Flames of Vengeance). Word 4 = level (60/180). Words 0-2 are other ids (not matched to a script). It has no script reference and no region field.
- Sub-mission objective/log text: one `archive_03` table per quest (5-45 rows, 179 B) pointing into a 486-record quest-log text group (first id ~125.8M).
- **Story digest table** `archive_03\55\DCA3752B...D0EE38813D79`: 74 rows x 88 B from +8: word 1 = event id (1193...), word 2 = title text id, word 3 = summary text id, word 16 = chapter 1..9 (ch9 = DLC: Rodrigo, Yui Shosetsu, Amphibian Ambush, Alchemist's Fate). Titles such as "Oni at Ryotan Temple", "The Battle of Mikatagahara", "Mount Kurama Defiled", "The Oni of Rajomon, Ibaraki Doji", "Okita Soji at Mibu Temple" are story beats, not the 16 main mission names.
- The 16 main-mission names used by the web ("The Battle of Hitokoto Slope", "Calamity's Seed", ...) were **not found in any English text group** (searched Hitokoto, Slope, Hamamatsu Incident, Futamata, Calamity, Ill Omen, Final Hope, Those Left Behind). Main missions' names may live in a group/encoding not yet identified, or in the exe/UI layouts. 3 of the web sub names do exist as text (Desecrated Hot Spring, Shinsengumi Scheme, Headaches) -> web sub-mission names are real in-game names.
- Script path hash (`archive\XX\<62hex>`) is not sha256/sha3/blake2s of any string in the scripts or common path forms; the script -> mission link is made by the engine from data, not discoverable by name.
- No stage-name hash (`data\stage_names.tsv`, h = sum(c*31^(i+1))) appears as a u32 in any archive_03 table: missions are not keyed by stage hash there.

## Per-script rules used
1. Primary stage = first `S0x_*` string that is a mission stage (not `_BASE/_WORLD/_COMMON`). Region from family: S00_EDO Edo 1622, S01_SEN Warring States 1572, S02_HEI Heian 1190, S03_KOD Antiquity 247, S04_BAK Bakumatsu 1864, S90_DLC1 DLC1 (Shadows of Edo, ch9).
2. Place -> web mission: HITOKOTO -> Hitokoto Slope; HAMAMATSU/_JOU/YOUGASHIDOU -> Hamamatsu Incident; FUTAMATA -> Futamata Castle; MIKATA -> Mikatagahara; SAIGA -> Saigagake; KURAMA -> The Age of Clans Begins; RAJOMON/MATUOTAISYA -> Heian Befouled; DAIDAIRI -> A Cause Too Great; REIJU -> Calamity's Seed / Ill Omen; SHIMABARAYUKAKU -> A New Era; HONNOJI -> Those Left Behind; GOSHO -> Final Hope; MIBUDERA/NISHIHONGANJI/HORIE -> the matching sub-missions.
3. `type` is main/sub from the web list or `main/sub` when the place hosts both; `field` = open-world stage; `dlc` = S90_DLC1.
4. Rows with no stage string: `evidence` lists hint tokens from user-function names (e.g. `wave_battle_shingen/masakage/masanobu/masatomo/nobuharu` = Takeda boss rush; `wave_battle_shosetsu/chuya/hanbe/juuroube/naoyoshi` = DLC rebel rush; `kaneiji`, `rodorigo` = DLC1; `kitan_NN` tags). Many of these (size 51500/46926/51302) are template-identical scripts.

## Caveats / next
- Where a place hosts both a main and sub mission (Hamamatsu, Kamo, Horikawa, Higashiyama), the main name is guessed; the kitan tags in `boss-fights-table.csv` (`kitan_01..46`) look like sub-mission numbers but do not match row order of the 49-row table (DLC tags 37-46 vs table rows 39-48 suggest an offset of 2; unproven).
- To resolve the rest: log `Mission::*` / stage load at runtime in a real fight (mission id is held by the mission manager at `g_MissionMgr`), or decode the script-start table that maps a script to its stage (loaded via `load_inf_NN.bin`, not parsed).
