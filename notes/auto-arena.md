# Automatic arena rows for every boss (user #23436 / #23447 / #23451 / #23479)

2026-10-22, branch `ally/autoarena`. Static work only: no game launch. Linux install `~/.local/share/Steam/steamapps/common/Nioh3`
(game 1.0 + Keian DLC). Per-row report: `notes/auto-arena-bosses.csv`.

## 1. Where the arena data lives (no exe addresses needed)
All of it is in the LFM data under `<game>/archive` (`archive_NN.bin` + `lfm_order_NN.bin` bundles + loose `XX/<62 hex>` files; read by
`tools/n3lfm.py` and the C port in `mod/Nioh3Training/src/arena_scan.h`).
- **id table** (`/AE/50B7...`): file id -> LFM path. Every table below is referenced by such an id.
- **mission table** (`/52/BD29...`, 130 x 0x198 B): +0x40 flags (0x41 set = not in the replay list -> `D` row), +0x50 script, +0x54 / +0x58
  enemy tables, +0x5C PlacementBox table, +0x6C start point table, +0x70 title text id, +0x158 mission id, +0x15A kind (2 = open-field region),
  +0x15C world byte.
- **enemy table** (0x138 B records): +0 placement id, +4 character id (the `Load::Character` id the mod spawns), +0x40 x y z yaw,
  +0x9C flags (**0x04 = boss record**), +0x134 record name (`..._BOSS...`, dojo `TEACHER_MISSION_*` / `*_ZAKO_*` grunts).
- **PlacementBox table** (0xA0 B): +0 id, +0x10 centre, +0x20 yaw, +0x28 size, +0x9C name (`BOSS_AREA...`).
- **start points** (0xA4 B): +0 id, +8 flags (0x40 boss retry point, 0x200 post-boss), +0xC kind (0 spawn, 906/907/909 shrine, 908 gadget),
  +0x40 x y z yaw. Used as the Fight-now start and as the WarpOtherWorld start for a cross-world switch.
- **mission script**: the arena trigger = the PlacementBox id pushed (op 0x29) right before `Player::IsEnter*Box*` / `Function::Wait_Invade*`
  / `Function::Loop_*EnterPlacementBox*` (`rematch_catalog._script_box_calls`).
- Arena point = 60 % of the way from the trigger box centre to its edge towards the chosen start point, on the boss's floor, facing the boss
  (`rematch_catalog.pick_arena`); no trigger near the boss -> the start point itself (`start`); open-field placement -> `field`.

## 2. What is automatic now
- **Generator** `tools/auto_arena.py [--game DIR] [--check] [--fresh]`: reads the game folder directly (no extracted dump, no hand list),
  finds the id / mission / chara tables at their known paths and **again by structure when a patch moves them**, indexes every
  `archive_*.bin` (DLC bundles included). Rows: (1) one `R`/`D<mission>` row per startable mission with a boss record (first `0x04` record
  named `*BOSS*`; no boss record -> the mission's only opponent: a single enemy record, or the one non-`ZAKO` record = the dojo duels);
  (2) one `S<chr>` row for every further boss character of a mission, then of every open-field region (`field`: 8 m in front of the boss
  along its own facing, start = the region start point nearest the boss); (3) `kind summon` roster rows (mission 0) for every boss character
  without an R/D row. Names: chara param table (+0x64 id, +0xA0 text id) + the English text file picked by structure; duplicates get the
  mission title / "open field". Reproduces the 43 hand-validated rows **byte for byte**; `--check` is idempotent.
- **Runtime** (`arena_scan.h`, `[ArenaSwap] AutoArena = 1` default): at table load (worker thread, 45 s after start) Training derives the
  same rows from the game files (0.2 s on Linux, structure fallback 0.6 s) and **adds every row whose mission (R/D) or boss character (S)
  the TSV lacks**, plus a roster entry (`Boss <chr>` until a TSV names it). The TSV is now cache + override. Offline check
  `mod/Nioh3Training/tests/arena_scan_test.c` = `auto_arena.py --fresh`: **131/131 rows identical** (also with the known paths disabled,
  `-DAS_TEST_NO_KNOWN`). With the committed TSV it adds 0 rows (nothing changes in today's game; a patch/DLC boss gets a row automatically).
- **Training API**: `Nioh3Training_FightNowChr(chr)` (+ pipe `fnowchr <chr>`): Fight now for a boss character = its R/D row, else its
  `S<chr>` arena/field row (pinned so the deferred fade / cross-world steps keep that row). `FindRow(mission)` ignores S rows, so the
  catalogue's `FightNow(mission)` is unchanged. Table capacity 160 -> 512 rows.

## 3. What is still manual / carried
- The committed TSV keeps (carried by row id): the names of the old rows, the CrossWorldCellSkip cell lists (column 16, measured in
  game) and its roster order; two roster bosses with no boss record anywhere (Raiju 368726, Tsurara-onna 246537) get `field` rows from a
  regular placement only because the TSV lists them. Every R/D row's boss equals what the automatic rule picks. A fresh build without the
  TSV (`--fresh`, = the runtime scan) gives 131 rows instead of 133 (those two).
- Runtime names of brand-new bosses are `Boss <chr>` (the English-text pick is a corpus heuristic; not ported). Running
  `tools/auto_arena.py` after a patch gives proper names.
- Boss Rematch menu: roster entries still call `SummonSlotChr`; a "Fight in its arena" item that calls `Nioh3Training_FightNowChr` is
  not wired (Nioh3BossRematch side).
- Not derived: boss BGM (`Nioh3Training_bgm.tsv`), cell whitelist, and anything per-move (frame data is a separate task).

## 4. The 64 + 4 summon-only bosses of the old table
Old table: 43 arena rows + 68 summon rows (4 catalogue dojo missions + 64 roster bosses). Now: **60 of 68 have an arena/field row**
(all 4 dojo rows, 56 of the 64 roster bosses); 8 stay summon-only.

| # | boss | chr | result | row id | kind | mission/region | world | source |
|---|---|---|---|---|---|---|---|---|
| 1 | Yagyu Munenori (Novice) | 696121 | arena | RF5E5 | start | 0xF5E5 | 0xDF | mission (boss kept from the committed row) |
| 2 | Hattori Hanzo (Novice) | 139791 | arena | RD759 | start | 0xD759 | 0xDF | mission (boss kept from the committed row) |
| 3 | Saito Fuku (Novice) | 631242 | arena | R26F3 | start | 0x26F3 | 0xDF | mission (boss kept from the committed row) |
| 4 | Saito Fuku (Adept) | 631242 | arena | RA0A1 | start | 0xA0A1 | 0xDF | mission (boss kept from the committed row) |
| 5 | Aggrieved Warrior | 189627 | arena | S189627 | field | 0xF722 | 0x96 | region boss record |
| 6 | All-Seeing Mage | 100898 | arena | S100898 | field | 0x1782 | 0xB1 | region boss record |
| 7 | Anayama Nobutada | 982853 | arena | S982853 | start | 0x1C7C | 0x96 | mission boss record |
| 8 | Anazawa Morihide | 38227 | arena | RBAB8 | start | 0xBAB8 | 0x35 | mission (first boss record) |
| 9 | Araki Mataemon | 60383 | **summon only** | - | - | - | - | no placement record of this character in any of the 130 shipped mission/region enemy tables (+0x54/+0x58): spawned by a script or not shipped |
| 10 | Baba Nobuharu | 482895 | arena | R1C7C | start | 0x1C7C | 0x96 | mission (first boss record) |
| 11 | Berenguela the Whimsical | 245775 | arena | S245775 | field | 0x4431 | 0x35 | region boss record |
| 12 | Blood-Spurting Ogin | 314396 | arena | S314396 | field | 0xF722 | 0x96 | region boss record |
| 13 | Bloodedge Demon | 14625 | arena | S14625 | start | 0x61A2 | 0x96 | mission boss record |
| 14 | Daidara Bocchi | 966334 | **summon only** | - | - | - | - | no placement record of this character in any of the 130 shipped mission/region enemy tables (+0x54/+0x58): spawned by a script or not shipped |
| 15 | Demon of Envy | 765992 | arena | S765992 | start | 0xA0CB | 0x59 | mission boss record |
| 16 | Demon of Obsession | 510357 | arena | S510357 | start | 0xA0CB | 0x59 | mission boss record |
| 17 | Demon of Pride | 810814 | arena | RA0CB | start | 0xA0CB | 0x59 | mission (first boss record) |
| 18 | Demon Slayer | 566824 | **summon only** | - | - | - | - | no placement record of this character in any of the 130 shipped mission/region enemy tables (+0x54/+0x58): spawned by a script or not shipped |
| 19 | Denrinbo Raikei | 972886 | **summon only** | - | - | - | - | no placement record of this character in any of the 130 shipped mission/region enemy tables (+0x54/+0x58): spawned by a script or not shipped |
| 20 | Enenra | 488671 | arena | S488671 | start | 0x0034 | 0x96 | mission boss record |
| 21 | Enki | 109203 | arena | S109203 | field | 0x8289 | 0xE0 | region boss record |
| 22 | Fabricated Warrior | 200413 | arena | S200413 | start | 0xB3FE | 0x35 | mission boss record |
| 23 | Gaki | 986563 | arena | S986563 | field | 0x1BA2 | 0xB1 | region boss record |
| 24 | Gaki Chief | 141706 | arena | S141706 | field | 0x1371 | 0x96 | region boss record |
| 25 | Gozuki | 665148 | arena | R6351 | start | 0x6351 | 0xE0 | mission (first boss record) |
| 26 | Great Tengu | 269750 | arena | RC72E | start | 0xC72E | 0x8E | mission (first boss record) |
| 27 | Green Shadow's Blade | 804607 | arena | S804607 | field | 0x14B9 | 0x96 | region boss record |
| 28 | Hattori Takeo | 240225 | arena | S240225 | field | 0x8289 | 0xE0 | region boss record |
| 29 | Hijikata Toshizo | 223221 | arena | S223221 | start | 0x7C85 | 0xE0 | mission boss record |
| 30 | Hiruko (Human) | 745563 | arena | R0C12 | start | 0x0C12 | 0xD8 | mission (first boss record) |
| 31 | Hiruko (Yokai) | 306399 | arena | S306399 | start | 0x0C12 | 0xD8 | mission boss record |
| 32 | Ishikawa Goemon | 334318 | **summon only** | - | - | - | - | no placement record of this character in any of the 130 shipped mission/region enemy tables (+0x54/+0x58): spawned by a script or not shipped |
| 33 | Jakotsu-baba | 947280 | arena | R0034 | arena | 0x0034 | 0x96 | mission (first boss record) |
| 34 | Joro-gumo | 96976 | arena | RAC1F | start | 0xAC1F | 0xB1 | mission (first boss record) |
| 35 | Kajiwara Kagetoki | 724394 | arena | R9A25 | start | 0x9A25 | 0xB1 | mission (first boss record) |
| 36 | Kanai Hanbei (Yokai) | 96759 | arena | R19EA | arena | 0x19EA | 0x35 | mission (first boss record) |
| 37 | Karasu Tengu | 970966 | arena | S970966 | field | 0xDFE0 | 0xE0 | region boss record |
| 38 | Kawara Jurobei | 602366 | arena | RD84E | start | 0xD84E | 0x35 | mission (first boss record) |
| 39 | Kondo Isami | 128741 | arena | S128741 | start | 0x7C85 | 0xE0 | mission boss record |
| 40 | Kosaka Masanobu | 274906 | arena | S274906 | start | 0x0F7F | 0x96 | mission boss record |
| 41 | Kyokotsu | 536709 | arena | S536709 | field | 0x1371 | 0x96 | region boss record |
| 42 | Marubashi Chuya | 381593 | arena | S381593 | start | 0x19EA | 0x35 | mission boss record |
| 43 | Mezuki | 492278 | arena | S492278 | start | 0xB24D | 0x59 | mission boss record |
| 44 | Mori Gentatsu | 626327 | arena | S626327 | start | 0xBAB8 | 0x35 | mission boss record |
| 45 | Nagakura Shinpachi | 614441 | arena | R7C85 | start | 0x7C85 | 0xE0 | mission (first boss record) |
| 46 | Naito Masatoyo | 502003 | arena | R0B0B | start | 0x0B0B | 0x96 | mission (first boss record) |
| 47 | Ninja Yoki | 597530 | arena | S597530 | field | 0x0F6A | 0x96 | region boss record |
| 48 | Nuppeppo | 904088 | arena | S904088 | field | 0x14B9 | 0x96 | region boss record |
| 49 | One-eyed Oni | 482715 | arena | S482715 | field | 0x1BA2 | 0xB1 | region boss record |
| 50 | Onmoraki | 799359 | arena | RE7BD | arena | 0xE7BD | 0xE0 | mission (first boss record) |
| 51 | Onryoki | 264763 | arena | R61A2 | start | 0x61A2 | 0x96 | mission (first boss record) |
| 52 | Raiju | 368726 | arena | S368726 | field | 0x0F6A | 0x96 | regular placement (no boss record) |
| 53 | Saito Hajime | 745986 | arena | S745986 | start | 0x7C85 | 0xE0 | mission boss record |
| 54 | Shishido Kagetoshi | 966721 | **summon only** | - | - | - | - | no placement record of this character in any of the 130 shipped mission/region enemy tables (+0x54/+0x58): spawned by a script or not shipped |
| 55 | Taigen Sessai | 198738 | **summon only** | - | - | - | - | no placement record of this character in any of the 130 shipped mission/region enemy tables (+0x54/+0x58): spawned by a script or not shipped |
| 56 | Takashima Shuhan | 524713 | **summon only** | - | - | - | - | no placement record of this character in any of the 130 shipped mission/region enemy tables (+0x54/+0x58): spawned by a script or not shipped |
| 57 | Takeda Shingen (Human) | 466641 | arena | D1C95 | start | 0x1C95 | 0x96 | mission (its only opponent, no boss record) |
| 58 | Tesso | 97403 | arena | S97403 | field | 0x4431 | 0x35 | region boss record |
| 59 | Tokugawa Yoshinobu | 268880 | arena | S268880 | arena | 0x1DEA | 0xE0 | mission boss record |
| 60 | Tsurara-onna | 246537 | arena | S246537 | field | 0x1BA2 | 0xB1 | regular placement (no boss record) |
| 61 | Usui Sadamitsu | 287588 | arena | S287588 | field | 0x09E7 | 0xB1 | region boss record |
| 62 | Waira | 115893 | arena | S115893 | field | 0x1BA2 | 0xB1 | region boss record |
| 63 | Yamada Shinryuken | 613533 | arena | S613533 | start | 0xBAB8 | 0x35 | mission boss record |
| 64 | Yamagata Masakage (Yokai) | 598115 | arena | R0F7F | start | 0x0F7F | 0x96 | mission (first boss record) |
| 65 | Yamainu | 410613 | arena | S410613 | field | 0xDFE0 | 0xE0 | region boss record |
| 66 | Yasha | 560912 | arena | S560912 | field | 0x1782 | 0xB1 | region boss record |
| 67 | Yoki | 999088 | arena | S999088 | field | 0x14B9 | 0x96 | region boss record |
| 68 | Yui Shosetsu | 428785 | arena | S428785 | arena | 0xB3FE | 0x35 | mission boss record |

arena 60 summon-only 8 of 68

**Why the 8 cannot get one**: their character ids (Araki Mataemon 60383, Daidara Bocchi 966334, Demon Slayer 566824, Denrinbo Raikei
972886, Ishikawa Goemon 334318, Shishido Kagetoshi 966721, Taigen Sessai 198738, Takashima Shuhan 524713) occur in **no** 0x138-byte
placement record of any LFM file (all 8349 entries scanned, not only the 130 missions' tables), and in no mission script. The ids appear
only in the chara param table, motion/asset bundles (`G2A_`/`TB1G` files), a video and per-character param tables: they are spawned by a
system outside the mission tables (Crucible/Eternal Rift wave data, scripted events) or not placed at all in 1.0. Getting arenas for them
needs RE of that spawner (which data feeds `Character::Create` for these ids at runtime): a BossLog Create trace in their fight, then the
generator can read that source. Until then they stay summon-only (summon works for them, verified live #22984).

Also new beyond the old table: **19 more boss characters** found in the data now have rows + roster entries (DLC field bosses
Ichimokuren, Itsumade, Mujina, Nozuchi, Jinnosuke the Shadow Slicer, Iron-Fisted Rogue, Rattan-Clad Ninja Oni, Robust Spear Samurai, Ooka
Genzaburo; second character ids of Kondo Isami, Hijikata Toshizo, Okita Soji, Hiruko, Nuribotoke, Tsurara-onna, Kajiwara Kagetoki, Ninja
Yoki, Saito Hajime, Marubashi Chuya) and **28 new mission rows** (e.g. R0034, R0B0B, R0F7F, R7C85, R61A2, RBAB8, RD84E; two are not
real fights: D7F16 = the new-game prologue variant, RA19A = the Picture Scroll test record; harmless, only reachable by id).

## 5. Needs a Windows build + in-game test
- Build: `mod/Nioh3Training/build.sh` (zig cc builds clean here; only the existing `g_noForceHide` warning).
- Log check: `ARENASCAN: ... rows from the game data, 0 not in the TSV added` and `arena table: N bosses` (133) on start.
- `fnowchr <chr>` for: one `S` mission row (e.g. 274906 Kosaka Masanobu, 0x0F7F), one `field` row in the current world (e.g. 97403 Tesso,
  world 0x35) and one cross-world `field` row (WarpOtherWorld with a region start point id is new: unverified that the region start ids
  are accepted as world start points).
- `field` rows are untested: the 8 m-in-front point may sit inside a wall/cliff for some bosses (the boss is pulled to BossDistance; the
  player drop is +1 m). Open-field bosses already beaten/alive in the region may coexist with the summoned copy.
- Wire a "Fight in arena" item for roster bosses in Nioh3BossRematch (calls `Nioh3Training_FightNowChr`, falls back to summon when 0).
