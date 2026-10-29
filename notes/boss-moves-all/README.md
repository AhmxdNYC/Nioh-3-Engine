# Move facts for every character with an action file (user #22709, offline)

Tool: `py -I tools/boss_move_summary.py` (boss-agnostic wrapper of `tools/yoritomo_move_facts.py`; same algorithm: hit windows merged by
group id, unblockable = action row +0x04 bit 0x2000, hitbox rows via sub-table 0, active-frame windows). Outputs here:
`summary.tsv` (one row per character), `moves-per-character.tsv` (every attack / unblockable move: hits, windows, spawns, how).
Game not touched.

## Coverage
- kN3Moves (mod/boss_moves.h) chr values: 11 = Yoritomo 173, Masakage 132, Ippon-Datara 90, Otoroshi 141, Nuribotoke 105, Red Kappa 113,
  Shingen 136, Takasugi 139, Kamaitachi 169, Hino-enma 134, Ibaraki Doji 14 (stub, no action file). Menu facts (kN3MoveFacts) exist for the first 10.
- Action files bound on disk: 94 characters in boss-action-files-22709.csv + Ippon-Datara = 95 files, 11127 action rows,
  2760 attacks (attack block OR timed hit window OR unblockable), hits 1/2/3/4+/unknown = 1271/417/102/128/842, unblockable 333,
  attacks with no own hitbox rows 330, attacks that spawn projectiles 411. 83 characters have >= 1 attack, 53 have >= 1 unblockable.
- Catalogue bosses (moves/attacks/unblockable): Yoritomo 173/88/19, Shingen 136/40/13, Hino-enma 134/37/11, Masakage 132/26/8,
  Ippon-Datara 90/34/7, Otoroshi 141/41/1, Nuribotoke 105/25/1, Red Kappa 112/9/0, Takasugi 139/63/14, Kamaitachi 169/63/12.
  Other bosses with data (no Training chr id yet): Ryomen Sukuna 149/15, Nobuharu 182/87/10, Mikoshi 191/73, Yoshitsune 174/51/19,
  Otengu 132/56/18, Enki 128/44/12, Yoshinobu 176/42/7, Himiko 98/14, Munenori 20/1, ... (see summary.tsv).

## No data (and why), from boss-char-assets.csv
- 104 characters have no setting DB in the package (loader id absent): CS_ set pieces (Cerberus, Kusanagi, Murakumo, Guhin, Kongojishi...),
  KY_ cutscene duplicates (Otoroshi, Nuribotoke_01, Raiju...), CE_Masakage_00 (the real one is CN_MASAKAGE, bound).
- 105 have a setting DB that is a 0-action stub (Ibaraki Doji CE_IBARAKIDOJI_00, CE_HIRUKO_00, Shiei_01, Ryomen Sukuna_02, Gaki general KY,
  Great Tengu KY, Yasha KY, generic bandit/samurai/etc.): moveset lives in another character / not in this file.
- 49 have actions listed in the setting but no archive_01 file matched their action hashes: CN_JUBE (610), CE_RYOUMENSUKUNA_01 (150),
  CE_YOUKI_POWER/SPEED (290/402), CE_NINJA (303), CP_HUMAN_* weapon sets (2958, shared, not per-boss), CE_TENGU/NUREONNA/SKELETON ...
  Not bound = hashes not found in the extracted files (unextracted layer or different array); needs the binder run on a wider extract.
- Bosses by name with no bound file: Yagyu Jubei, Ibaraki Doji, Tomoe Gozen, Honda Tadakatsu, Hattori Hanzo (only CN_HANZO_MASANARI: 56 rows,
  0 attacks), Fuma Kotaro, Kajiwara, most human duelists (they use shared CP_HUMAN_* sets).

## Menu code path check
`N3MoveCat` / `N3MoveHitTag` (move_cats.h) are keyed only on `N3MoveFactsBoss(chr)` and `N3MoveFactFind`: no Yoritomo assumption.
Yoritomo-only spots are chr-guarded and harmless: Ki-break note text (move_cats.h `chr != 16102u`), hidden 1100-1103 combo heads
(training_bosses_menu.h SeqHead). One deliberate difference in tools/gen_boss_move_facts.py: Yoritomo's `attack` = attack block only
(kept so its output stays byte-identical); other bosses also count a timed hit window / unblockable as an attack.
