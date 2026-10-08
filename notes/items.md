# Item master table decode (static, 2026-10-08)

Game not run. Script: `py -I re\scripts_re\items_dump.py` (stdlib, read-only) -> `D:\nioh3-re\items.tsv` (3,362 rows, off-repo) and `notes\items-boss-named.csv` (45 rows).
Text: `D:\nioh3-re\text_en.tsv/text_ja.tsv` (see text-tables.md). Extends boss-loot.md section 3.

## Source table
`data\extract\lfm\archive_03\2E\D4E1DB3B...79` (1,398,600 B). Header `00220420`, u32 count at +4 = **3362**, then **3362 fixed records of 104 u32 (416 B) starting at file offset 8**, no tail. (boss-loot.md's "stride varies per family" is wrong: every family is 416 B; unused words are 0.)

## Record fields (word index inside record)
| w | meaning | confidence |
|---|---|---|
| 25 | **category/type code** (43 distinct; see below). Not unique per item | high |
| 26 | **name text id** (key in text group; group = id>>22: 3 armor, 4 consumable/material/misc, 5 weapon, 11 skill, 36 notes) | high |
| 27 | description text id (equals name id for placeholder rows) | high |
| 84 | **unique per-record hash** (u32, 3362 distinct) = best item key; this is what other tables would reference (not proven) | medium |
| 0, 1 | monotone offsets (~12-35 apart) into two side string/key pools (not found; probably item symbol names) | medium |
| 34 | price/value (weapons 600-1810, armor 700-1500, materials 5000) | medium |
| 33 | type flag word (896 gear, 897 talisman/mat, 907/911/929 skills/scrolls, 928 soul core) | low |
| 36/37/38 | gear: 70/15/n weapons, 48/15/n armor; w38 looks like base stat (weapon 11-48, armor 7-75). Rarity/level NOT confirmed | low |
| 44 | flags/rarity-like bitfield (weapons 2035..20484, armor 2049/4097/6145) | low |
| 64 | max carry/stack (9999 mats, 99 consumables, 1 gear) | medium |
| 2,3,4,5,20,94,96,97 | misc; w2/w3=0xFFFFFFFF for most | - |
Placeholders: name id 17844063 / 18269809 = text "DUMMY" (deleted/reserved slots; 520 rows, mostly in weapon/armor categories). `no_text` column = no English name. Soul-core rows (cat 900000000) and cat 1700000000 have name id 0.

## Categories (w25) -> counts (total 3362; names resolve in English for 2,6xx)
| w25 | n | label | note |
|---|---|---|---|
| 100000200..100001700 | 559+3 | weapons | per weapon class (x100): 02 katana, 03 dual katana, 04 spear, 05 axe, 06 kusarigama, 07 odachi, 08 tonfa, 09 hatchets, 10 switchglaive, 11 splitstaff, 12 cestus, 13 tachi, 14 dual tachi/odachi?, 15 talons, 16/17 hoko&shield/sword&shield (class names from samples, tentative) |
| 200010000/20000/30000 | 18/11/10 | bow / matchlock / cannon | |
| 301..305 000000 | 198/157/145/154/152 | armor head/body/hands/waist/feet | |
| 306000000 | 102 | accessories (omamori, medicine case, netsuke charms) | |
| 400000000 | 116 | consumables (Elixir, Power Pill, Spirit Stone) | |
| 500000000 | 195 | materials (yokai drops: Karasu Tengu Feather...) | |
| 600000000 | 179 | key items, scroll boxes, Soul Core Box | |
| 700000000 | 245 | talismans, ninjutsu/onmyo scrolls, Summoning Seal: {} | |
| 800000000 | 7 | ammo | |
| 900000000 | 119 | **probable soul cores**: no name text, w33=928, price 500, 116 rows unique hash; yokai name must come from elsewhere (template `{} Soul Core` text id 0x010d29f1 exists but no row references it) | hypothesis |
| 1000000000 | 209 | skills (g11/g31) | |
| 1100000000(+100) | 499 | appearance/collection copies of weapon/armor names (w33 907/939/11) | hypothesis |
| 1200000000 / 1300000000 | 53 / 32 | martial-art / ninjutsu-onmyo skill entries + handbooks | |
| 1400000000 | 40 | ninja tools (Healing Pill, Salt, Smoke Ball) | |
| 1600000000 | 89 | skill-slot placeholders ("SKILL NAME DUMMY") | |
| 1700000000 | 20 | unnamed, w33=0, price 800 (unknown) | |
| 1800000000 | 5 | Hundred Demon Realms picture scrolls | |
| 1900000000 | 48 | readable documents/diaries (g36) | |
| 0 | 5 | misc (Gunpowder/Shrapnel Bomb) | |

## Not in this table
- **Guardian spirits** (names in text g1/g17) and per-yokai **soul core** names: no row carries them. Soul cores are template + enemy name; the 119 unnamed rows are the candidate keys.
- Enemy -> reward link: no enemy id in any item field. Join key to try: w84 hash or row index against the reward-set decode.

## Boss-named items
`notes\items-boss-named.csv`: 45 rows, 26 boss atoms (from boss-names-online.csv; split on `; ( ) &`, whole-word case-insensitive match against English name). Cols: boss_atom, idx, cat_code, category, name_id, desc_id, name_en, rec_hash. Examples: Ibaraki Doji's Odachi / Thorns, Ryomen Sukuna's Axe/Sword/Bow, Kusanagi Helmet/Netsuke Charm, Great Tengu Blade/Feather, Waira Claw, Enki Pelt, Demon of Pride's Horn, Takeda Shingen portrait. Humans (Hanzo, Honda, Yoritomo...) have no item named after them; boss-signature gear is named otherwise. The `appearance` category duplicates names of weapons/armor.
