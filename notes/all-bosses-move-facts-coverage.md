# Move-facts algorithm: coverage over every boss / character with action data (#22709/#22746, 2026-10-22)

Offline, read-only. Game not touched. Method: `tools/yoritomo_move_facts.py` (hit windows + unblockable bit, see `notes/yoritomo-hits-and-unblockable.md`).
Driver: `tools/allboss_coverage.py` -> full table `notes/all-bosses-move-facts-coverage.tsv` (columns: boss, char, file found, moves, moves with hits,
unknown '?', zero-hit, unblockable, max hits, >8 hits, moves with overlapping windows, moves whose count is an upper bound (`hits_upper_bound`, section 3), bad windows, status).
File binding: `notes/boss-action-files-22709.csv` (94 characters have an archive_01 action file; Ippon-Datara added from `tools/gen_boss_move_facts.py`).
Boss list: unique names in `notes/boss-master.csv` (script + online rows). "Moves" = actions with an attack block, own hitbox rows or the unblockable bit.

## Headline
- **The tool ran without error or assertion on all 95 of 95 bound action files** (94 CSV + Ippon-Datara): 3,437 moves, no parse failure, no
  window with end < start, no file that is not the 16-sub-table layout.
- **Named bosses: 37 mapped names (36 sure, 1 guess) of ~81 boss names in boss-master have data; 36 of the 37 pass all sanity checks, 1 WARN** (> 8 hits:
  Takeda Shingen 0x1875 / 0x187B = 9, a real flurry, see 4). The other ~44 boss names have no action file bound (section 2).
- Another 58 non-boss characters (mobs, arms, NPC) also have files and run clean except > 8 hits on Shigurui Big (3 moves, 12) and Yamanba (2, 12 / 17):
  all of those are separate start frames spread over 130-270 frames (flurries), not double-counted capsules.
- Sanity totals over all 95 files: 7 moves with > 8 hits (was 17 before the hit-key fix of section 3), 0 bad windows, 7 files with 0 moves (see 2).

## 1. Table (named bosses; moves / with >=1 hit / '?' unknown / unblockable / max hits)
Yoritomo 94/41/34/19/3 | Takeda Shingen 67/32/0/13/9 WARN | Hino-enma 44/33/3/11/4 | Yamagata Masakage 31/23/3/8/3 | Ippon-Datara 35/34/0/7/2 |
Otoroshi 75/25/5/1/5 | Nuribotoke 26/12/0/1/3 | Red Kappa 9/9/0/0/2 | Takasugi Shinsaku 66/41/15/14/6 | Kamaitachi 74/33/21/12/2 |
Baba Nobuharu 97/43/33/10/4 | Mikoshi Nyudo 81/18/55/0/7 | Great Tengu 57/41/7/18/3 | Yoshitsune 55/45/5/19/5 | Enki 50/39/5/12/6 |
Tokugawa Yoshinobu 100/28/7/7/3 | Ryomen Sukuna 16/4/8/9/8 | Himiko 19/1/5/0/1 | Hiruko 18/12/0/0/2 | Honda Tadakatsu 32/26/4/11/5 |
Hattori Hanzo (Masanari file) 5/0/0/0/0 | Okita Soji 43/34/0/4/5 | Yagyu Munenori 1/1/0/0/1 | Tokugawa Kunimatsu 49/37/6/3/4 |
Jakotsu-baba 33/28/0/5/3 | Kasha 42/42/0/4/5 | Kyokotsu 24/17/0/0/8 | Mezuki 37/34/0/9/5 | Nuppeppo 20/18/1/2/7 | Onmoraki 85/51/0/4/3 |
Onryoki 34/31/0/6/4 | Tsurara-Onna 40/15/11/5/7 | Yasha 23/20/0/0/5 | Gaki 22/18/0/2/2 | Gaki Chief 48/22/2/0/7 |
Joro-gumo (CE_ARACHNE) 47/23/5/0/3 | One-eyed Oni (guess CE_HITOTUME) 16/9/0/0/2.
Files exist for all 37 (data file found = y). Rows for the 58 other characters are in the TSV.

## 2. Bosses with no usable data
- **~44 boss names have no bound action file**: Anazawa Morihide, Aggrieved Warrior, Blood-Spurting Ogin, Cavern Ascetic, Crucible Wraith, Daidara Bocchi,
  Death Veil, Demon Slayer, Demons of Envy / Obsession / Pride, Enenra, Fuma Kotaro, Giant Toad, Gozuki, Hattori Takeo, Hijikata Toshizo, Ibaraki Doji,
  Ito Ittosai, Kajiwara Kagetoki, Kanai Hanbei, Kawara Jurobei, Kondo Isami, Kosaka Masanobu, Kumagai Naoyoshi, Kusanagi, Maeda Mitsutaka, Magatsu Warrior,
  Nagakura Shinpachi, Naito Masatoyo, Nakane Masamori, Nasu no Yoichi, Ninja Yoki, Okiku, Otazu no Kata, Saito Fuku, Saito Hajime, Sawamura Yasusuke,
  Shinran, Sugitani Zenjubo, Takahashi Deishu, Tesso, Tomoe Gozen, Waira, Yagyu Jubei, Yoki.
  Why: only 94 characters' setting DBs list action hashes that match an archive_01 file. Ibaraki Doji's setting DB lists 0 action hashes
  (`boss-move-facts-all-bosses-22709.md`). For the human duelists the likely cause is a shared generic moveset (CP_HUMAN_KATANA / TONFA_NINJA / TEONO_NINJA
  are bound, but which duelist uses which was not established, so they are NOT assigned) - unverified. Several others (Cerberus, Kusanagi, Murakumo, CS_*
  stubs) have setting DBs with 0 actions (2788-byte stubs).
- **Bound file but 0 moves**: CE_JIGOKU_SHITAI_00, CE_KASYA_WHEEL_00, CE_SHIKIGAMI_00, CN_HISAHIDE_HEIFUKU, CN_TOYO, CM_CHONIN_F_00, DB_NOUMIN_F_00
  (no attack data; non-fighters or attacks owned by another file).
- **Partly usable**: Mikoshi Nyudo (55 of 81 moves '?'), Baba Nobuharu (33 of 97), Kamaitachi (21 of 74), Takasugi (15 of 66), Yoritomo (34 of 94): moves with no own
  hitbox rows or spawned objects. These are shown as '?', not guessed. Himiko has hits for 1 of 19 moves, Hattori Hanzo for 0 of 5 (those moves are
  zero-power or untimed rows).

## 3. Simultaneous rows: fixed with the game's own hit key (2026-10-22)
Before: rows of different groups (or group-0 rows of different power) live at the same frames were counted as separate hits; 241 of 3,437 moves
were upper bounds (Gaki Chief 0x4E27 = 14, Tsurara-Onna 0x4E67 = 9).
**Code evidence.** RVA `0x2B5B98` is the hit-group getter of a hitbox row: byte `+0xA5`, and `0xFF` -> (qword `+0x00` bit 17 ? 1 : 0) + (bit 18 ? 2 : 0)
(bit-17 path `0x197AD88`). Its caller `0x78EADF..0x78EB74` walks every hitbox slot of an action (`0x2E9940` count, `0x2E98F0` row), builds the key
**(start frame `+0x08`, hit group)** and puts it in a map: an existing key keeps the **max** of power `+0x10` / element `+0x12`, a new key adds an
entry; the loop at `0x78EBFB` then **sums** power / element over the keys. So the game itself treats all rows with the same start frame and hit group as
one hit (2+ capsules / body spheres of one swing) and different keys as different hits. ('hits = distinct hit ids' alone is not usable: 1,700 of
2,178 moves with timed rows have hit group 0 everywhere; Hino-enma 0x4E67 is 3 swings of group 0.)
**Rule now in `tools/yoritomo_move_facts.py`:** (1) rows -> keys (start, group), power = max; (2) keys of the same group that overlap or touch merge
(the sweep rule, 0x4E8E; group 0 also needs equal power), as before. Hits = merged windows.
Results: Yoritomo unchanged (0x4E6E 2, 0x4E68 3, 0x4E6C 2, `--check` passes); Hino-enma 0x4E67 3; Gaki Chief 0x4E27 14 -> **7**; Tsurara-Onna
0x4E67 9 -> **5**; Kamaitachi 0x4E53 / 0x4E6B / 0x4E92 / 0x4E93 2 -> 1 (the only change in `mod/boss_move_facts.h`). Shingen 0x1875 stays 9.
**Still an upper bound** (column `hits_upper_bound` = 1; `simul_moves` in the TSV): 136 moves (46 on named bosses: Shingen 7, Kunimatsu 7, Okita 6,
Onryoki 5, Mezuki 3, Tsurara-Onna 3, Gaki Chief 3, ...) where two counted windows with different keys are live at the same frame, e.g. Shingen 0x4E46
(groups 1 and 2, both 77-90, element 40) or Mezuki 0x4E35 (group 0, starts 68 / 69, power 50 / 48). The game counts them as two keys, but whether both
connect on the player is spatial; the frame meter can settle it.

## 4. Plausibility spot checks (3 bosses besides Yoritomo)
1. **Hino-enma 0x4E67** (motion 1031): windows 60-80, 95-103, 137-148 -> 3 hits, not unblockable. Matches the 3-hit count that the Training log
   comparison in `boss-move-facts-all-bosses-22709.md` reports for the same move (window start 60 vs meter startup 50 is the known unexplained offset).
2. **Takasugi 0x187F / 0x1891** (motion 1201 / 1202): 6 hits each, windows 3-4, 47, 57, 71, 101, 206 with one big final hit (power 46 / 27) and four equal
   small hits (4 / 6-7). That shape is a flurry plus finisher, and the two siblings agree in hit frames and count, as expected for variants of one motion.
3. **Kamaitachi** siblings 0x4E5A / 0x4E5B (motions 1051 / 1052) give identical windows (50-102, 93-101); 0x4E53 and 0x4E56 (motion 1002) give
   identical windows 32-40. 0x4E53's two rows are the same start frame and hit group with powers 55 / 50: one hit (power 55) by the section 3 key.
Also: Shingen 0x1875 = 9 windows (50, 105, 122, 138, 146, 158, 164, 171, 218): one opening hit, 7 small hits, one finisher; power 9 / 3 / 45 looks like
a multi-hit flurry, so 9 is plausible; the sibling 0x187B has the same frames. Shingen's 13 unblockable moves and Takasugi's 14 are high but come from the
same action-row bit as Yoritomo's; the bit is not separately proven in game for other bosses (as stated in the earlier note).

## Verdict
The algorithm runs on every available boss file (95/95) with no structural failure and gives sibling-consistent counts. Counts are exact up to the 136 flagged
upper-bound moves (section 3) and the Yoritomo-proven assumption "one window = one hit". About 44 boss names have no file, so "every boss" is not possible from the
current data.
