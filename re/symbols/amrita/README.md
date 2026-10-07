# re\symbols\amrita: name lists taken from amrita.tools (2026-10-06)

Source: the public web apps at https://amrita.tools (builds., effects., scrolls., weapons., experimental.amrita.tools).
They were fetched as text/JSON and parsed with a literal parser; none of their code was run. Game data in
them is from the 2026-09-02 build (patch 2.01), so a few rows may differ from Steam v2.02. Write-up: `notes\amrita-tools.md`.

**These are not KTIDs.** Gameplay records (items, gear effects, skills, soul cores) use **16-bit param ids**
(the u16 at item+0 and effect-slot+0 in the save). They do not appear in `rdb_list.tsv`, `all_properties.csv` or
`kidstypeinfodb.yml`. Use them for the save format and the LFM param tables, not for the RDB/KOD symbol DB.
The one 32-bit hash list (`save_field_hashes.tsv`) holds field-name hashes from the user-save serializer. They are
immediates in Nioh3.exe, and the functions that use them are in `re\symbols\incoming\amrita.tsv`. Their source
strings are unknown: a dictionary attack with the engine property hash (h = sum c_i*31^(i+1), checked against 5,977 known
property names) over our 26k-word vocabulary found only chance collisions.

| file | rows | key | what |
|---|---|---|---|
| `save_field_hashes.tsv` | 22 | u32 field hash | user-save (SAVEDATA.BIN) section/field hashes, layouts, exe read/write sites |
| `items.tsv` | 2784 | u16 item id | item names (weapons, armor, consumables, scrolls); `gear_raw` = amrita's unlabelled per-gear tuple |
| `gear_effects.tsv` | 3604 | u16 effect id (hex, as amrita shows it) | gear/scroll/grace effect names, display format (`{}`, `<dot>`), `pct_x10` = value is tenths, description |
| `transformation_effects.tsv` | 176 | u16 effect id | effects that Soul Core Transformation offers; `history_mode` gated/bypass/mixed (soul-core history check) |
| `skills.tsv` | 1100 | u16 skill id | skill names; order/cost/category (C/S/N = Common/Samurai/Ninja)/mastery/tier where known |
| `soul_cores.tsv` | 118 | u16 item id | soul core yang/yin skills and costs |
| `guardians.tsv` | 24 | guardian id | guardian spirit names |
| `equip_slots.tsv` | 12 | slot index | equipment slot index to name (0-3 melee, 4-8 armor, 9-10 accessories, 13 ranged) |
| `armor_sets.tsv` | 90 | set name | set membership (item ids) and set bonuses |
| `enemies.tsv` | 142 | enemy ids | enemy names and ids (from scrolls.amrita.tools/api/enemies; ids are up to ~1e6, not u16) |

Columns are tab-separated, UTF-8, LF, one header row.
