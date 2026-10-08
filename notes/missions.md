# Mission master tables (static, 2026-10-08; game not run, no mod code)

CSVs: `missions.csv` (130 master records), `missions-main.csv` (21 main-mission slots), `missions-areas.csv` (61 area records). Text from `D:\nioh3-re\text_en.tsv` (group 7 holds mission/story text). Generator: not committed (stdlib py, reads the three files below). All files are `lfm\archive_03`, magic `00 22 04 20`, `u32 count` at +4, records from +8, no trailing pool.

## Three tables (what the exe indexes)
| Table | File | Rows x size | Exe access |
|---|---|---|---|
| **Mission master** | `52\BD29D455...FC2` | 130 x 0x198 | `MissionDbGetRecord` 0x377B6C (index); PlacementSetsBuildFromMissionDb 0x1C1D148 / MissionScriptsBuild 0x1C1D288 walk it |
| **Main-mission list** | `BA\82A01427...5C8` | 21 x 20 | not traced; join proven by data (below) |
| **Area records** | `C0\561B7E3A...19C` | 61 x 0xC4 | `MissionRecordById` 0x285808 (hash by u16 id at +0xB8), DataMgr+0x630 (this is the "0xC4 record, type byte +0xC0" of boss-rift-ngplus.md) |

Correction: the 0xC4 table is an AREA/stage-section table (ids 10..807, positions at +0x34), not the named mission list. Its `+0xBC` byte is the **area id** that `[g_MissionMgr state +0x11]` holds (0x1FD4C00 compares them), so +0x11 is "current area", not only a scaling key.

## Mission master record (0x198 B)
| off | field (evidence) |
|---|---|
| +0x00 u32 | running offset (0,7,21,...) into an external name pool (not in file); +0x04 second offset (2362...). Unresolved |
| +0x40 u32 | flags: 0x1 always; 0x2 story/crucible (has log text); 0x20000 crucible-story variant; 0x8000x tutorial; 0x1000 linked-battle; 0x2000000 hidden wave stage; 0x400000/0x1000000 Emaki-ish |
| +0x50..+0x6C 8 x u32 | **placement-set ids** (+0x54..+0x68 = 6 resource ids, others 0 on unnamed) and +0x6C script id (duel-flow note). Names of those files: other agent |
| +0x70 u32 | **name text id** (text group 7). 33397680 = "Dummy" (internal) |
| +0x84 u32 | **clear-log / summary text id** (story digest style) |
| +0x108 u32 | unlock tier 0..6 (0 hidden/start, 1 Hamamatsu..., 6 post-game) |
| +0x110 u32 | **parent mission key** (u16 of the owning story record; equals another row's +0x158) |
| +0x114 u32 | sub reference id (310..575, 259-267 for the nine Dojo rows) |
| +0x164 u32 | hi16 = recommended-power-like number (1050..5000, same scale as area w1 1070..7200) |
| +0x17C u32 | level-like value 451 base, up to 633 |
| **+0x158 u16** | **mission key** (random-looking 16-bit, unique, NOT the 10..807 area ids) |
| +0x15A u16 | kind: 0 story slot, 1 sub/other, 2 internal stage (always unnamed) |
| +0x15C u16 | **area id** = area record `+0xBC` (150 Hamamatsu era, 177 Heian, 224 Bakumatsu, 53 Edo, 75 Hitokoto, 216 Yamatai, 142, 89, 223, 130, 171...) |
| **+0x18A u16** | **main-mission key**: equals `missions-main.csv` col 2 (BA table u32 +0x10). Present on the 20 rows that are the main slots |
No NG+/journey variant record exists: 130 rows are single; NG+ is the slot switch (boss-rift-ngplus.md). Rewards/prereqs: not separate fields found; first-clear reward bits are area-record flag bits 10/11; unlock gates are tier +0x108 and flag id (area +0xBE).

## Main-mission list (BA, 20 B)
`u32 offset | name text id | objective text id | clear-log text id (0=none) | u16 key`. 21 slots: Ceremony, Battle of Hitokoto Slope, Hamamatsu Incident, Futamata, Mikatagahara, Saigagake, Calamity's Seed, Age of Clans Begins, Beast of the Snowfield, Heian Befouled, A Cause Too Great, Ill Omen, To the Beginning, (slot 13 unnamed), A New Era, Those Left Behind, Final Hope, What Makes A Shogun, Crucible Once Again, Menace Lurking within Chokodo, Carnage of Hell Wind. This resolves boss-missions.md "web main names not found": they are text ids here. Master rows link by +0x18A (e.g. Hitokoto key 24360 -> row 34 "Escape from Hitokoto Slope"; Ill Omen 1500 -> row 51 "Dark Roots").

## Area record (0xC4 B) and type byte
`+0x00 u32 offset, +0x04 u32 (1070-7200 level-like), +0x08 u32, +0x0C u32 flags (0x4 bit2 = first-clear reward site), +0x20 u32, +0x34 3 floats position, +0xB8 u16 area record id, +0xBC u8 area group, +0xBE u16 flag id, +0xC0 u8 type, +0xC1 u8 extra (1..3)`.
Type byte +0xC0 (counts over 61): 0 hub/start (ids 10,100,101,200,300,400...), 1 normal stage (ids 150-152, 161-166, 180, 800-805 ...), 2 boss/side sub-area (153,167,168,170,181), 3 **hell / clear-map-only** (ids 701-705, area group 89; matches the 0x239FFB8 `type==3` gate). 12 = bit pattern in w3. Main/sub/twilight/emaki are NOT type values; they are master-record kind + flags + class (below).

## Classification used (missions.csv `class`)
tutorial 3 (Basic/Expanded Controls, Training Ground), prologue 2 (Fall of Edo Castle), main 20, optional-crucible-story 6, named-linked-battle 21 (rows 74-95: Four Heavenly Kings, Demonic Duels, Gathering of the Greats...; parent key links to a main; heuristic), sub-kitan 5 (4 DLC + Demon's Regimen; the other 44 kitan names live only in `31\BD17...`, 49 x 132, name id word 3, level word 4), dojo-way 9 (Way of the Warrior/Ninja/Onmyo x3), emaki 2 (Hundred Demon Realms Picture Scroll), hidden-wave-stage 17 (flag 0x2000181), unnamed-stage 23, internal-stage 21. "Twilight" not identified.

## Open
Name-pool for +0x00/+0x04; resource id -> file name (other agent); which key (+0x158 vs area id) the clear db stores (SetMissionCleared 0x1C1DB88 arg) - log one call.
