# Battle Scrolls (shrine menu): entry table, unlock, start path (static, 2026-10-08)

Static only (LFM text + param tables, `decomp_ro.cmd`, v2.02 RVAs, base 0x7FF74F410000). Game not run, no mod code. Table: `notes\battle-scrolls.csv` (all 130 mission records). Names: `re\symbols\incoming\battlescroll-renames.tsv`. Generators: `re\scripts_boss\bs_text_en.py` (English text dump), `bs_mission_table.py` (CSV), `bs_add_boss.py` (boss column). Extracted LFM archives 02/06/15 were added to `data\extract\lfm` (ignored by git).

## 1. What "Battle Scroll" is internally
- In-game text (LFM `archive_17`, group `71d12e04`): *"Battle Scrolls via the Shrine menu will allow you to play completed main missions and unlocked sub missions"*; *"Battle Scroll allows you to replay completed missions or undertake Battle Scroll-exclusive missions"*; tutorials are playable "through Battle Scroll"; later *"Hundred Demon Realms Picture Scroll has been added to the Shrine menu's Battle Scroll options"*. The menu icon tag is `^05~EMAKI~`.
- So Battle Scroll = **shrine item 3** with two halves: (a) **mission replay** (`MENU_ID_MISSION_REPLAY`, menu `[g_MenuMgr=0x45C84A8]+0x4E0`) over the mission table below, and (b) **Picture Scroll / Emaki** missions (`[g_MenuMgr]+0x4E8`, single generic mission 0xCC96; covered in `boss-rematch-paths.md` §1). The story bosses are all in (a).

## 2. The entry table = the mission table (found; it was missing in `boss-missions.md`)
- File: LFM `archive_03` entry 398, path `/52/BD29D45533CFACBFA2D6EC80A3B9F40E07CBF9183727358B0B3C3ABE3E4FC2`, 53,048 B (zlib 11,248 B at archive offset 0x227000; table slot at file 0x31E0). Header `00 22 04 20`, u32 count = 130, records 0x198 B from +8.
- In memory: `[[g_DataMgr=0x45B9E30]+0]` = the same bytes; id->index map at `[[0x45B9E30]]+0x20`. `MissionRecordGet(dm, id)` **0x377B38** (124 callers) returns `base+8+idx*0x198`. Proven by code: `Mission::IsTrainingMission` compares the current mission with 0x6F24 = record 25 "Training Ground" `+0x158`.
- Record fields (offsets from record start; confirmed in code = C, from data = D):

| off | meaning |
|---|---|
| +0x00 / +0x04 | sequence / key (D) |
| +0x40 u32 | flags (C): bit0, 0x40, 0x4000 = never listed; 0x20000 = allowed online; 0x1000000 = Emaki-capable; bit1 tested by shrine item 4 |
| +0x70 / +0x84 | name / clear-summary text id (group `b1a7e301` or quest group `b127ad07`) (D) |
| +0x110 u16 | parent main-mission id (sub missions) (D) |
| +0x14C / +0x14E u16 | menu category / order (D) |
| +0x158 u16 | **mission id** (C; clear-db key, e.g. Crucible Manifested = 0x34) |
| +0x15A u16 | kind 0 main / 1 sub / 2 system-dummy (C) |
| +0x160 u8 | era tab: 1 Rift, 2 Sengoku, 3 Heian, 4 Yamatai, 5 Edo finale, 6 Bakumatsu, 7 Edo post, 8 Keian/DLC (C compares it to the menu era) |
| +0x164 / +0x166 u16 | main-story marker / story-progress value (C in 0x215A57C) |
| +0x178/+0x17A/+0x17C u16 | recommended level per journey (D) |
| +0x17E u16 | era-progress needed to list (C) |

## 3. List of fights Battle Scrolls covers (62 listable rows; full detail in the CSV)
Filter used by `MissionSelectListBuild` 0x378674: flags `& (1|0x40|0x4000) == 0`, kind 0/1, progress >= `+0x17E`, `MissionUnlockedGet` true, era matches. Rows passing (boss from the game's own summary/objective text; "web" = `boss-names-online.csv`):
- **Sengoku**: Escape from Hitokoto Slope - Yamagata Masakage; Crucible Manifested - Jakotsu-baba; Mirage Fortress - Baba Nobuharu; A Fiery Invasion - Takeda Shingen (human); Tiger's Cause - Shingen (yokai); subs: Ryotan Temple - Ryomen Sukuna; Haunting Caves - Hino-enma; Takeda's Four Heavenly Kings; The Demon's Regimen - Hattori Hanzo; Princess Tsubaki, Vengeful Blade, Horie's Fleeting Petals, Origin of the Haunting Caves, Groans in the Valley (web: Kamaitachi).
- **Heian**: Sacred Mountain Defiled - Great Tengu; Black Feather Sanctuary - Otoroshi; Subterfuge at the Abandoned Shrine - Kajiwara Kagetoki; The Oni of Rajomon - Ibaraki Doji; Frozen City, Blazing Tree - Minamoto no Yoritomo; subs: The Mystery of Rajomon; Beneath a Pure White Flag (web: Tomoe, Yoshitsune, Yoichi, Kajiwara).
- **Yamatai / Edo finale**: The Spirit Tree Village; Dark Roots - Hiruko; Light of Hope - Tokugawa Kunimatsu; A Sinister End - Hiruko (+Death Veil); prologue The Fall of Edo Castle - Kunimatsu.
- **Bakumatsu**: The Sweet Sound of Strings - Takasugi Shinsaku; Accursed Temple - possessed Buddha (web: Nuribotoke); Scion of the Gods - Tokugawa Yoshinobu; subs: Beast of the Underworld; Kiyomizu Defiled - Onmoraki; Solitary Blade - Shinsengumi; Drawn to Yokai Incense; Crucible Tour; Nobunaga's Blade; Shinsengumi: Best of Five.
- **Edo post-game (Battle-Scroll-style rushes)**: Shadows of the Three Oni; Demonic Duels; Within Time Everlasting; Rampant Temptation; Supernatural Shrieks; **Gathering of the Greats** (rush of past bosses).
- **Keian / DLC**: A Fortress in Ruins - Yagyu Jubei; The Hollow Schoolhouse - Kanai Hanbei; Mountain Keep in the Storm's Eye - Yui Shosetsu; Guardian of Technique; Ghosts at Hirakawa Gate; Disaster Unfolds; Keian Dreams; Keian Noble Bouts (+Giant Toad).
- **Eternal Rift**: Way of the Warrior/Ninja/Onmyo x3 - Yagyu Munenori / Hattori Hanzo / Saito Fuku. Tutorials (Basic/Expanded Controls, Training Ground) also pass, matching the tip text.
- **Present but excluded** (flag 0x40): *Becoming Champion of the East* (Honda Tadakatsu duel), *Triple Triumph* (Minamoto no Yoshitsune duel), *Yagyu Principles* (Yagyu Jubei, Marubashi Chuya); flag bit0: *The Battle of Hitokoto Slope* (first-play variant), 17 dummies, generic 0xCC96 (Emaki) and 0xA19A.
- **Not missions at all** (so no Battle Scroll entry): open-field/region bosses and story beats outside a mission record (web "Those Left Behind"/Enenra, "A New Era", "Final Hope" are only field chapter text), Crucible/small-hell bosses, field bosses, the three secret Demon ikiryo. For these the Emaki path (single placement per record) is the only scroll-style route.

## 4. Menu, unlock checks, start path
- Shrine item visibility `ShrineItemVisible` **0x1D9FBD8**: item 3 (Battle Scroll) shown iff **mission flag 0xAB** (`MissionFlagGet` 0x284944). This is the "after Crucible Manifested" unlock. Enable `ShrineItemEnabled` 0x1D9B374: offline = inventory count < 2000.
- Select `ShrineTopMenu_Update` 0x1E1E258, item 3: if `FeatureUnlocked(8)` (0x605300, bitset `[0x4B6A170]+8`) **and** mission flag **0x217** -> Battle Scroll top menu `[g_MenuMgr]+0x490` (`BattleScrollTop_Select` 0x1DAFE64: option 0 replay, option 1 Emaki); else straight to replay `MenuOpenMissionReplay` 0x1DE28E8(owner,0,6) -> 0x1DDA258 -> `MissionReplayMenu_Setup` **0x1DC8424** (stores mission id `+0x24F4`, journey, era byte from record `+0x160`, then vtbl+0x380 builds the list). (The rematch note writes 0x1DD8424; the correct RVA is 0x1DC8424.)
- Per-mission state, clear db `g_ClearDb=[0x4751848]`: `MissionUnlockedGet` 0x909218 (map `+0x2E0`, byte `+0x18`), `MissionClearedGetSlot` 0x702B64 (map `+0x2C8`, `+0x38+slot`, = `Mission::IsMissionAlreadyClear`), cleared bit `+0x24+slot` set by `MissionClearBitSet` 0x217F9B0. List rows get status 4 when cleared.
- Start: the replay menu feeds the shared mission-start queue (`MissionRequestById` 0x2239600 / `MissionRequestEnqueue` 0x223953C, see `boss-rematch-paths.md` §2-3); the fight is the whole mission script, not a boss-only arena. Picture Scroll fights go through `EmakiMissionRequest` 0x10DBED0 (mission 0xCC96).
- Not pinned: the exact vtbl+0x380 list builder of the replay menu. `MissionSelectListBuild` 0x378674 has exactly the filter above and is the best candidate (callers 0x1E84184, 0x9C4E48); confirm by a log hook when the menu opens.

## 5. Mod options (solo/offline only; never in co-op/online)
1. **Unlock the menu early - memory, game setter (low risk).** `MissionFlagSet(g_MissionMgr, 0xAB, 1, 0)` (0x70275C) shows Battle Scroll; add 0x217 + `FeatureUnlocked` bit 8 for the Picture Scroll half. Revert = set flag back to 0 (it is per journey slot and saved, so revert before saving if a clean save matters).
2. **Unlock individual entries early - memory read-hook (medium).** Hook `MissionUnlockedGet` 0x909218 / `MissionClearedGetSlot` 0x702B64 to return true for chosen ids **only while the replay menu is open** (0x23F627C `UI::IsOpenMissionReplayMenu` state); never write the clear db. Wider gating is risky: both getters drive story logic.
3. **List the excluded duels (Tadakatsu, Yoshitsune, Jubei/Chuya) - memory data patch (low-medium).** After load, clear bit 0x40 in `+0x40` of the mission records 0xD22D, 0xB3F8, 0xFB96 (find by `MissionRecordGet`), restore the byte on exit. Whether their scripts run cleanly from the replay path is untested.
4. **Data edit (byte-exact revert possible, more work).** The table is in LFM, which LooseFileLoader does **not** cover. Edit = back up `archive\archive_03.bin` (7,139,328 B), patch the decompressed 53,048 B table, recompress in the same chunk format (`u32 size + zlib`, see `n3rdb.py decompress_assets`), write it at 0x227000 if it fits the slot (next entry offset) and update the csize in the 32-B table slot at 0x31E0; revert = restore the backup (or Steam verify). Same outcome as option 3 but persistent and offline; no checksum on LFM was seen.
5. **Adding missing bosses.** Not possible through this table: a record only names a mission id whose script and stage spawn the boss; the 17 dummy records have no script behind them. Field/Crucible/secret bosses need the Emaki route (`boss-rematch-paths.md` §1: patch one Emaki record's boss placement in memory before `EmakiMissionRequest`).

## 6. Open items
1. Log-confirm the replay list builder (hook 0x378674 and vtbl+0x380 of `[g_MenuMgr]+0x4E0`) and which getter (unlocked vs cleared) gates main vs sub rows.
2. Which script/mission sets flags 0xAB and 0x217, and who sets `FeatureUnlocked` bit 8.
3. Bosses for the sub missions whose text names no boss (Princess Tsubaki, Vengeful Blade, Horie, Origin of the Haunting Caves, Mystery of Rajomon, Nobunaga's Blade): needs the BossLog run (ASI 1.7.18 `[Debug] BossLog`).
4. Verify that LFM loose files in `archive\XX\` override the bundle (would make option 4 a drop-in file instead of an archive edit).
