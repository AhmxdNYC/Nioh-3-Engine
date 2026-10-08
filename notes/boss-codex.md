# Encyclopedia / codex: table, UI/save path, unlocked bits (static, 2026-10-08)

Game not run, no mod code. Data: `notes\boss-codex.csv` (237 rows). Renames: `re\symbols\incoming\codex-renames.tsv`. Extraction from `data\extract\lfm` (read only). Decomp via `decomp_ro.cmd`.

## Short answer
- There is **no per-enemy "defeated" or "kill count" bit in the codex data**. The menu is `MENU_ID_AMRITA_MEMORY_ENCYCLOPEDIA` (a tab of the Amrita Memory menu: TOP / ENCYCLOPEDIA / GALLERY). Entries are gated by **unlock conditions** evaluated against story progress and mission flags, and cached as one byte per entry in the player data.
- The codex is a **people/boss lore** table, not a full bestiary: 237 rows = ~76 distinct subjects (56 CN_ character/NPC + ~28 CE_ enemy rows incl. variants), no ordinary mob species. Normal yokai/mob kill counts are not here.
- For the rematch UI the usable "boss beaten" signals are the **mission flags** named by a few rows (unlock type 3, below) plus the existing mission-clear db (`boss-fights.md` section 4). The codex adds little beyond that.

## 1. Codex table = LFM `archive_03\13\4E7EDC15EB80...` (39,028 B)
Header `00 22 04 20`, u32 count = 237 at +4, 148 B records from +8, then a NUL-terminated key-string pool at file offset 35084 (`CE_HERO`, `CN_KUNIMATSU_FIRST`, ...). Fields (u32 index in record):

| w | meaning (confidence) |
|---|---|
| 0 | key string offset into pool (high) |
| 1 | row kind: 0 = entry, 1 = variant page (CE_SHINGEN, `_TOKOYAMI`, `_ANOTHER_STYLE`), 2 = lore text page `_TEXT_n` (high) |
| 3 | codex id (unique, <10000); w5 = parent's codex id (high) |
| 4 | sequential index 1..1040 with gaps, likely the slot index (medium) |
| 7 | **name text id**, same ids as the character table; join gives char_id (high) |
| 10 | lore description text id (0x92xxxxx, group 152459569, text pages only) (medium) |
| 12 | **unlock-condition type** {1,3,5,7,8,10} (high) |
| 14 / 18 (+19=1) / 15 / 23 | condition argument for type 3 / 5 / 10 / 8 |
| 22 | story-progress gate 1000..9999 (Shingen entry 1050, boss page 1450) (medium) |
| 33..35 | floats, probably preview camera/pose (low) |

Unlock types, checked by overlap against other tables:
- **3 = mission flag** (w14): 12 of 25 `Mission::IsMissionFlag` ids in `boss-arenas.csv` match (55129 and 6634 are story flags tested by Nekomata / DLC1 scripts). Rows: CE_SHIGURUI_SOUJI 31877, CE_HANBE 6634 / HUMAN 15192, CE_SHOSETSU 46078, CN_HANZO_JR_TEXT_2..4 (55129, 57740, 39908). These are the codex "seen/defeated" gates.
- **5 = NPC/ally record** (w18, w19=1): all 21 values are keys (col 1) of `archive_03\B0\AB7C3E87...` (268 x 20 B cast table, col 3 = name text id). CE_MASTER_ITO 42166, CE_ANAYAMA 6450.
- **7 = story gate only** (w22): the large majority (108 rows), including every Shingen / Masakage / Nobuharu / Yoritomo / Yoshinobu / Hiruko page.
- **8**: value is a key (col 1) in `archive_03\5B\DA3D...` (600 x 40 B), 11/11; 4 also seen as `SetMissionFlag` ids. **10**: `archive_03\31\BD17...` (49 x 132 B) col 21, 18/29 (weak). Type-3 args also fill `archive_03\88\598A...` (146 x 28 B) col 5, 16/16. Meaning of these three tables unresolved (quest/event).
- 1 = always available (CE_HERO and a few).

char_id join: w7 -> char table `archive_03\C4\E1CBAE...` (+0xA0 name id, +0x08 char_id): 211 of 227 non-zero w7 resolve; CSV `char_ids` lists all (variants and difficulty copies share a name). Category column: CN_ = character/NPC page, CE_ = enemy page; boss human/yokai/main/side taken from `boss-names-online.csv` by English name (heuristic).

## 2. Runtime: the unlocked bit
The codex rows share the schema of the **Amrita Memory table** (`archive_03\16\7EF277D0...`, 194 x 96 B; rec+8 memory id, +0x10 category {1: 111, 5: 35, 6: 48}, +0x14 u16 key into the text table, unlock type with the same {1,5,7,8,10} vocabulary). It is `[g_DataDb(0x45B9E30)+0x198]`; the text/icon table is `[g_DataDb+0x1D0]` (rec+0xC text id, +0x2C icon). Which DataDb slot the CN/CE codex table is read from was **not located** (no direct string xrefs for MENU ids). The tab type `[menu+0x14D0]` in {1,5,6} is built by `AmritaMemoryMenuBuildList` 0x1EED1EC.
- **Bit store**: player-data object `[0x4751860]` (g_PlayerDataMgr). Hash map: FNV-1a over the 4-byte id, mask `[[pd]+0x71E0]`, buckets `[[pd]+0x71C8]` (16 B), node id at +0x10, **unlocked byte at node+0x14**. Getter `AmritaMemoryIsUnlocked` 0x60589C, node lookup 0x605ACC, setter `AmritaMemorySetUnlocked` 0xA10700 (also sets dirty byte `[[pd]+0x69E4]`). Script bindings: `Memory::SetAmritaMemory` 0x23A2CBC (set + toast), `Memory::IsAmritaMemory` 0x23A2C0C.
- Auto unlock: `AmritaMemoryAutoUnlockScan` 0x605C9C (type-5 rows via predicate 0x604C80).
- Persistence: the map lives in the user save stream; field hash/offset not traced (same open item as mission flags).

## 3. Use for the rematch UI
1. Cheapest boss-beaten check stays the mission-clear db and mission flags (`boss-fights.md` section 4). The codex gives a handful of extra flag ids (type 3 rows above).
2. To read "entry unlocked", call `AmritaMemoryIsUnlocked(g_PlayerDataMgr, id)` with the memory-table id. The CN/CE codex id -> memory id link was not proven.
3. Next: log `AmritaMemorySetUnlocked` 0xA10700 during one boss kill to see which ids fire; trace the `[0x4751860]+0x71C8` writer in the save serializer.
