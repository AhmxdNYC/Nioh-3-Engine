# Boss loot research (LOOT angle, 2026-10-08)

Static analysis of extracted data only. Game not run. Confidence: names/ids HIGH, loot linkage LOW (see "Not resolved").
Output: `notes\boss-loot.csv` (268 enemy rows). Helper scripts were scratch (not kept); everything below is reproducible with plain struct parsing.

## 1. English text (archive_17, "message" files)
- Format: stream of records `u32 key, u32 nchars(incl NUL), UTF-16LE text`. Files are per language x category; ~1400 files.
- English files: `17/8F/676FE6` = item names+descriptions (897 strings, keys 0x01xxxxxx, name then desc); `17/27/158B1C` = character / guardian-spirit / enemy names (1274, keys 0x00xxxxxx). Soul Core UI text in `17/00/7317BD`.
- Soul Core items are templates: `{} Soul Core` (key 0x010d29f1) with `{}` = yokai name, plus `Mortal Soul Core` (0x012f2b15, for humans, "Soul Fusion" fodder). So there is NO per-boss soul core item name in text; the boss name comes from the enemy-name key.

## 2. Enemy master table (enemy char_id -> name) = HIGH confidence
`data\extract\lfm\archive_03\B0\AB7C3E87...A8` (5368 B): header `00220420, count=268`, rows of 20 B `u32[5]`:
`[0]=index into a variable-length side table (cumulative, ~19-25 apart), [1]=enemy char id, [2]=family/group seq (e.g. 119/147 = DLC & late yokai), [3]=English name key (158B1C), [4]=flags (0,1,256,257,3)`.
Includes every boss seen: Honda Tadakatsu 59966, Saika Magoichi 25843, Oda Nobunaga 52284, Nue 53401, Giant Toad 49736, Shuten Doji 24686, Otakemaru 55750, Hattori Hanzo 55 970/11280, Takeda Shingen 32150/51399, Tokugawa Ieyasu 4766, Ibaraki Doji 26518, Hiruko 7116/5035, Himiko 52834, Tokugawa Kunimatsu 41535 ... (all in CSV). Names carrying `^01~RED~` are named mini-bosses (Juzo, Taigen Sessai, Adachi the Beheader, Kondo/Hijikata/Saito, Goemon, ...).
Flags hypothesis (unverified): bit0 = yokai (has Soul Core), 256 = human/mortal, 257 = yokai in main-game family, 0 = human boss/NPC. CSV column `soul_core_item_inferred` is derived from that guess only.
Also: `archive_03\77\0490...` (24200 B) = 112-row named-enemy table, 0xD8-byte rows, [char id @+0x18, name key @+0x1C, type-name key @+0x20] -> e.g. Juzo (0x993c) "Bloodedge Demon". `archive_03\EB\86B2...` (12-byte rows) and `7D\101C...` are related name/id side tables.

## 3. Item parameter table (name key -> item record) = HIGH confidence for layout
`archive_03\2E\D4E1DB3B...79` (1.4 MB). Record stride 416 B (104 u32) for the item family at 0x796xx-0x...: `w1=category (300000000 key items, 400000000 consumables, 500000000 materials, 600000000 weapons, 1100000000 bows)`, `w2=name key`, `w3=desc key`, `w4`=type tag, `w50`=per-item hash (u32, referenced nowhere else), `w80/w81`=sequential counters, `w0xF2`(u16)=a per-item group id. Different item families use other strides (armour/talismans). No field in a record holds an enemy id.
Boss/yokai-named material drops found by name (item -> evidently from that enemy): Nurikabe Fragment (Nurikabe 17501), Oni-bi Core (36511), Kamaitachi's Front Teeth (16182), Shuten Doji's Red Hair (24686), Waira Claw (64752), Ippon-Datara Fang, Namahage Horn, Karasu Tengu Feather, Kappa Shell / Red Kappa Shell, Mujina's Scarf, Hateful Spine, Wheel of Ice and Fire, Gyuki Wing Membrane, Kiryoki Mane, Konaki-Jiji Hair, Large Tsuchigumo Claw, White Tiger Fur, Nightmare Tusk, Tate Eboshi Horn, Giant Toad Oil, Nuppeppo Liver, Bakegani Pincers, Sanmyo Blade Fragment, Infernal Nail, Enki Pelt, Fox Spirit's Pelt/Tail. Boss-signature gear: Yoichi's Bow (Nasu no Yoichi 63077), Ame-no-Murakumo Tsurugi, Yata Mirror, Yasakani Magatama, Kusanagi Tsurugi, plus named armours (Sanada's Crimson, Kojuro's, Hero's...). CSV column `drop_items_by_name_match_inferred` = pure first-word string match, treat as a hint (false positives e.g. "One-Eyed Dragon's Armour").

## 4. Not resolved (honest)
- The enemy -> drop table (which item a given enemy id drops, Soul Core drop rate, first-clear/clear-reward items) was NOT located. Item records carry no enemy id, and enemy rows carry no item id; neither the item `w50` hash, `w80/w81`, nor key 0x01xxxxxx appears in any other extracted file (searched all LFM except >30 MB blobs). `02_clear_reward*` / `10_07_reward_icon` UI layouts exist (screen layout names), so reward data is probably in the LFM mission/stage scripts (archive_09 bytecode) or in the unextracted >30 MB archive_02/15 blobs, referenced by hash of an item symbol name.
- Next step for whoever continues: hash the item symbol name strings (kidshash.py variants) and look for those u32s next to enemy ids in archive_09 mission scripts and archive_02/15; or hook the runtime item-grant function (not done here, no game launch).
