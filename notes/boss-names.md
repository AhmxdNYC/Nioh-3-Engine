# Boss / enemy names: text tables and the id -> name path (static, 2026-10-08)

Game not run, mod not touched. Companion: `notes\boss-names.csv` (960 named character records: char_id, name_text_id, english_name, romaji_token, evidence), renames in `re\symbols\incoming\boss-names-renames.tsv`. Extraction used `data\extract\lfm` (nothing new written there).

## 1. Text (localization) tables = LFM `archive_17`
- 736 text files in `data\extract\lfm\archive_17\XX\<62hex>` (UTF-16LE). Format: repeated `{u32 text_id, u32 nchars_incl_NUL, UTF-16 chars + NUL}`, ids unordered, no header. Unused slots are `^13~UNUSED~`; inline markup `^01~COLOR~...^01~WHITE~`, `^FE~RUBY~..`, `<swGenderByHero(a,b)>`.
- 32 text *groups*, each = the same id set (key = first id + record count) in 23 language files (language is not in the file name or header; identify by content/script). Empty string = not translated in that file.
- Names group = **first id 7662000, 1280 records** (the "DUMMY" first entry). English file: `archive_17\27\158B1CE85DA2C4C6...` ("Ibaraki Doji"); other languages: `0B\4AE6..` ja (with RUBY), `77\D8D7..` zh, `36\85E2..` ko, `F0\0FC5..` en-variant, `6F\9794..` / `8B\DA7B..` / `B2\8569..` fr/it/pt-style "Ibaraki-doji", `9A\C578..` de "Ibaraki-Doji". Same group also holds guardian-spirit names + descriptions (ids interleave: name id then description id).
- Other groups (not needed here): 29xxxxxx/30xxxxxx quest/story strings (756 rec, first id 31696817), enemy/encyclopedia lore (666 rec, 152459569, has "Ibaraki Doji rampaged through Kyoto..."), item names (1417/1039 rec).

## 2. Character param table: char_id -> name_text_id
- `data\extract\lfm\archive_03\C4\E1CBAE0841F7ECE8...` (943,314 B). Header `00 22 04 20`, u32 count at +4 (= 1022), then **records of 920 B starting at file offset 8**. Per record: `+0x08` = **char_id** (e.g. 20680 = 0x50C8; ids ~20680..), `+0xA0` = **name_text_id** (key into the names group above; 0 = unnamed). 960/1022 records resolve to an English name. Several char ids share a name (phases/variants/difficulty copies), e.g. Ibaraki Doji = char 21175, 21188, 21213; Takeda Shingen = 20899, 22199.
- This is the same record the engine reads as actor->charParam (`[actor+0xE8]`); the in-memory copy puts name id at `+0xA0` (see below), consistent with the file layout.
- The sibling file `archive_03\13\4E7EDC15...` (148 B records, keys `CN_*`/`CE_*` like `CE_SHINGEN`, `CN_HANZO_MASANARI_TEXT_1`, first field = name text id e.g. 6130737 Tokugawa Ieyasu) is the encyclopedia / character-entry table (CN = chara name entry, CE = enemy entry). Not fully decoded.
- Two small tables (`archive_03\B0\AB7C3E87..`, `EB\86B2A73B..`) also reference name ids (player/NPC cast: Saito Dosan, Azai Nagamasa, ...).

## 3. Runtime path: boss HP bar -> name (static decompile, medium-high confidence)
1. Script `UI::ShowBossGauge(actorId[,..])` -> `0x23B8678` -> `0x1D115D4` -> **BossGaugeSetup 0x1D05824** (HudBossGauge*, up to 3 actor ids).
2. For each id: **AddBossGaugeRow 0x1CE7C6C** (skips id 0, duplicate rows, null actor/dead): row = 0x60 B, `+4` = actor id, `+8` = **name text id**. Name id = `[charMgr+0xD4]` if nonzero, else `actor->charParam([actor+0xE8])+0xA0` (normal path, flag `actor+0x885 == 0`) or `FUN_5A28D0(actor)+0x18` (alt path, a special-form record for some actors). Fallback default `0x3E994F1`.
3. BossGaugeSetup then calls **TextGetById 0x5A4B04**(row+8) -> **TextMgrLookup 0x63D9C4**(g_TextMgr, language, id): text manager `g_TextMgr` at **0x45C8450**; language index at `+0x1CE4` (must be < 0x17 = 23 languages), loaded-count `+0x1CE8`, std::map (red-black tree header at `+0x1CD0`) id -> `wchar_t*` at node `+0x28`; "^13~UNUSED~" results are skipped and `L"?"` style fallback `0x45...` (DAT 7ff752cfe5c4) returned. The string is then set into gauge element 0xF5[i] (text widget, width 0x4F) via 0x302540; positions/fonts via 0x300F44.
4. So: boss actor -> charParam record (+0xA0 name id) -> text map for the active language -> row text. To change/inspect a boss name at runtime, hook TextGetById 0x5A4B04 (1,309 callers, hot) or read row+8 in AddBossGaugeRow.

## 4. Romaji token -> English (boss-fights-table.csv `name_tokens`)
Mapping built by matching tokens to name words in the English names group (all verified to exist in the char table; details with char ids in the CSV `romaji_token` column):
ibaraki=Ibaraki Doji; shingen=Takeda Shingen; masakage=Yamagata Masakage; nobuharu=Baba Nobuharu; masanobu=Kosaka Masanobu; kagetoki=Kajiwara Kagetoki; yoshitsune=Minamoto no Yoshitsune; kumagai/naoyoshi=Kumagai Naoyoshi; toragozen=Tora Gozen; hanbe=Kanai Hanbei; shosetsu=Yui Shosetsu; chuya=Marubashi Chuya; jurobe/juuroube=Kawara Jurobei; jube=Yagyu Jubei; youshinobu=Tokugawa Yoshinobu; takasugi=Takasugi Shinsaku; souji=Okita Soji; nagakura=Nagakura Shinpachi; saigo=Saigo Takamori; kotaro=Fuma Kotaro; hanzo=Hattori Hanzo; maria=Maria; hiruko=Hiruko; kurama=Kurama Tengu; onryo=Onryoki; magatsu=Magatsu Warrior; shunobon=Shunobon; kusanagi=Kusanagi; kamaitachi, nekomata, nuribotoke, kappa = same-named yokai. Weak/guesses: `shinsen`=Shinsengumi Member, `wild`=Wild Dog.
Unmatched (mostly not names: stage/flag words) but plausible names without a table entry: ishibitsu, boshitsuki, oitekebori, masatomo, tadakatasu, goganki, nigiyakashi, abura, yodomi, tokoyo, soranaki, siroshoin, hyakuman, mikata, genshin, shigurui, horie, yusuke, tsuyoshi, tomato (likely quest/sub-mission ids or place names; try `grep -i` in the other languages' ruby text).

## 5. Caveats / next
- Language-file identification is by content; the language->path-hash mapping per group is not decoded (probably from `archive\load_inf_NN.bin`).
- char_id here is the charParam key. Boss placement ids in scripts (e.g. 8061822) are different; link is the stage placement data (other agents).
