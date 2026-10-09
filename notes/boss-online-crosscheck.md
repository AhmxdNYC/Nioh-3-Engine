# Nioh 3 boss list: online cross-check vs boss-master.csv

Date: 2026-10-08. Detail CSV: boss-online-crosscheck.csv (87 rows).
Our side: boss-master.csv, 90 unique names (several are parse junk, see section 3).

Sources actually read: GamingBolt "All 81 Bosses" (80 names enumerated by the page fetch; headline says 81 bosses over 109 battles),
PowerPyx boss guide (20 main + 3 secret), GuruGamer secret bosses, Fextralife wiki (Bosses / Sub Missions / Myths / Hell Rising pages, via summarised fetch),
FuzionByte and Gematsu (DLC), futurefive / TechRadar reviews. Blocked or empty: mapmaster.io (403), nioh.fandom (402), Game8 (no boss page found),
GameTrek (not surfaced this pass). Reddit not surfaced by search.

Naming note: the web calls the first DLC **Hell Rising** (released 2026-08-19, Edo 1651, Keian era). Our data labels it
"Shadows of Edo" / "DLC1". Web search found no "Shadows of Edo" anywhere. The second DLC "Bloody Insurrection" is announced, not released.
Patches up to 2.02.0 (2026-09-17): no patch-added boss found online besides the Hell Rising content (1.03.03 only added an NPC ally for the first boss).
NG+ (Shogun's Journey / Enlightened One's Journey) exclusive bosses: none documented online; they are difficulty tiers re-running the same bosses.

## 1. Full online list, story order

Online total: 87 distinct = 80 GamingBolt + Daidara Bocchi (Fextralife) + 6 Hell Rising names (Yagyu Jubei, Giant Toad, Tesso, Anazawa Morihide, Mori Gentatsu, Yamada Shinryuken).
GamingBolt claims 81, so one name was missed by the fetch (uncertain; could be Daidara Bocchi). Per-row detail is in the CSV.

Main story (PowerPyx order, 20 fights):
1 Yamagata Masakage (Escape from Hitokoto Slope, 1572); 2 Jakotsu-baba / Yakotsu-baba (Crucible Manifested); 3 Baba Nobuharu (Mirage Fortress);
4 Yamagata Masakage mounted (A Fiery Invasion); 5 Takeda Shingen human (A Fiery Invasion); 6 Takeda Shingen monster (Tiger's Cause);
7 Great Tengu (Sacred Mountain Defiled, 1190); 8 Otoroshi (Black Feather Sanctuary); 9 Ibaraki Doji (The Oni of the Rajomon); 10 Kajiwara Kagetoki (Subterfuge at the Abandoned Shrine);
11 Minamoto no Yoritomo (Frozen City, Blazing Tree); 12 Kusanagi (Spirit Tree Village, 247); 13 Hiruko (Dark Roots);
14 Tokugawa Kunimatsu (Light of Hope, 1622); 15 Takasugi Shinsaku (Sweet Sound of Strings, 1864); 16 Enenra (Those Left Behind); 17 Nuribotoke (Accursed Temple);
18 Okita Soji + Tokugawa Yoshinobu (Scion of the Gods); 19 Death Veil (A Sinister End); 20 Hiruko final form (A Sinister End).

Secret / post-game (PowerPyx, GuruGamer): Demon of Pride (Iinoya), Demon of Envy (Hachijo), Demon of Obsession (Nijo Castle / Mibu Temple), plus Bloodedge Demon (dynamic purple-grave Revenant boss; GuruGamer counts it as a 4th secret boss).

Sub / field / master (GamingBolt table):
- Warring States: Aggrieved Warrior, Anayama Nobutada, Blood-Spurting Ogin, Cavern Ascetic, Fuma Kotaro, Gaki Chief, Green Shadow's Blade, Hino-Enma, Honda Tadakatsu, Ippon-Datara, Ito Ittosai, Kosaka Masanobu, Naito Masatoyo, Magatsu Warrior, Nuppeppo, Otazu no Kata, Sugitani Zenjubo, Taigen Sessai, Yoki, Daidara Bocchi, Hattori Hanzo.
- Heian: All-Seeing Mage, Demon Slayer, Minamoto no Yoshitsune, Nasu no Yoichi, Tomoe Gozen, Shinran, Tsurara-Onna, Usui Sadamitsu, Waira, Yasha, Ryomen Sukuna.
- Bakumatsu: Enki, Hijikata Toshizo, Kondo Isami, Nagakura Shinpachi, Saito Hajime, Hattori Takeo, Ishikawa Goemon, Joro-gumo, Karasu Tengu, Onmoraki, Raiju, Red Kappa, Sawamura Yasusuke, Shishido Kagetoshi, Takahashi Deishu, Takashima Shuhan, Yamainu.
- Edo masters (Eternal Rift): Hattori Hanzo, Saito Fuku, Yagyu Munenori.
- Multi-era repeats: Gaki, Kamaitachi, Kasha, Kyokotsu, Mezuki, Ninja Yoki, One-eyed Oni, Onryoki.

Hell Rising DLC (1651 Keian): Yagyu Jubei (A Fortress in Ruin; confirmed first boss), Giant Toad and Tesso (new yokai per Fextralife, boss status unconfirmed), Anazawa Morihide / Mori Gentatsu / Yamada Shinryuken (Keian Noble Bouts; from a search-engine summary only, uncertain). Online sources give no full DLC boss roster (Fextralife "New Hell Rising Bosses" section was empty), so our DLC data is likely more complete than the web.

## 2. MISSING from boss-master (online, not in ours): 14

- Taigen Sessai (master, Lake Hamana). Our file notes it as web-only with no mission link, and no row.
- Anayama Nobutada: present only as a broken fragment "Baba Nobuharu (Anayama Nobutada" (comma-split parse); needs its own row.
- Bloodedge Demon (GuruGamer / GamingBolt).
- All-Seeing Mage, Green Shadow's Blade, Karasu Tengu, Raiju, Yamainu, Usui Sadamitsu, Ishikawa Goemon, Shishido Kagetoshi, Takashima Shuhan.
- Hell Rising: Mori Gentatsu, Yamada Shinryuken. We hold them only as placement keys (DUEL_QUEST_40_GENTATSU / _SHINRYUKEN, CH08_..._40_SHINRYUKEN), not as named bosses; the names come from a search summary (uncertain). Yamainu likewise exists only as an internal key (..._QUEST_ENEMY_YAMAINU).

## 3. Ours but not found online

Likely newer-than-web (DLC) names, unverified but not contradicted: Kawara Jurobei (Keian Dreams), Kanai Hanbei (The Hollow Schoolhouse), Kumagai Naoyoshi (Mountain Keep in the Storm's Eye), Maeda Mitsutaka, Nakane Masamori (hidden-wave stages).
Anazawa Morihide and Giant Toad / Tesso have only thin web mentions (see above).
Possibly internal/generic Crucible enemies: Crucible Wraith (Memories in the Blade), Gozuki (Crucible Tour, To the Beginning). Not named in any source read; uncertain.
Parse junk, not bosses (from comma-splitting in the online rows): "10 yokai)", "3 yokai waves", "Gaki Chief (", "Hattori Hanzo met as mid-mission fights)", "Baba Nobuharu (Anayama Nobutada", "generic Emaki/Picture Scroll mission 0xCC96", "yokai", "Onmyo mages". Clean these.
Duplicate casing: Hino-Enma / Hino-enma are one boss.
Region mismatch to check: our file puts Demon of Pride under "Shadows of the Three Oni" (Edo 1622 post-game); three web sources put it at Iinoya, 1572. Uncertain which is right.

## 4. Spelling mismatches

- Jakotsu-baba (ours, GamingBolt) vs Yakotsu-baba (PowerPyx)
- Kajiwara Kagetoki (ours) vs Kaiwara Kagetoki (PowerPyx typo)
- Ishikawa Goemon (correct) vs Ishakawa Goemon (GamingBolt typo)
- Joro-gumo (ours) vs Joro-Gumo / Jorogumo
- Hino-Enma vs Hino-enma (both in ours)
- Okita Soji vs Okita Souji; Hijikata Toshizo vs Toshizo with macron
- Hattori Takeo: GamingBolt writes "Shinsengumi Member Hattori Takeo"
- Missions: ours "The Oni of Rajomon" vs web "The Oni of the Rajomon"; ours "A Fortress in Ruins" vs web "A Fortress in Ruin"
- DLC name: web "Hell Rising" vs ours "Shadows of Edo"
