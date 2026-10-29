# Human duelists: which action rows the game uses (pc2 #23909/#23911, offline, 2026-10-22)

Tool: `py -I tools/auto_movelist.py --chr <id>` / `--sweep` (output `notes/auto-movelist/summary.tsv`, `moves.tsv`). Game not launched;
the 367 archive_01 action files were extracted read-only (`--extract`).

## What is loaded for a human boss (from the autobind sweep logs = spawn-time memory reads, run4/run5)
The runtime row scan (autobind_game.h) stops after 6 rows without a known action-object hash, so on the big shared files it sees only a
slice (that is why humans matched 36-62 %). Replaying the scan inside every file (same start action id, same distinct-hash count, same
best / runner-up %) identifies each slice uniquely:
- base: `CP_HUMAN_COMMON` 95A50CA1 (767 rows; ids 0x0-0x40x system / reactions) for almost every duelist;
- one weapon file: KATANA C36664C0 (Kondo, Kosaka, Green Shadow's Blade, Munenori, Saito Fuku), NITOU 29D407C5 (Hijikata, Anayama, Araki,
  Hattori Takeo), YARI B46AEA89 (Naito, Marubashi), TACHI 55B17131 (Nagakura, Saito Hajime), NAGIGAMA 7922CF24 (Anazawa, Usui),
  ONO 9721535F (Taigen, Takashima), TEKOU F8306B0A (Demon Slayer, Goemon), KAMA_NINJA 5AB52298 (Shishido, Yamada), TEONO_NINJA (Mori),
  TEONO 21C36CBA (Kajiwara), SUDE (Tsurara-onna), unowned 3684288C (All-Seeing Mage), 70D20493 (Berenguela);
- small extras: other CP_HUMAN_COMMON parts (E595C355, 1DD4D996, 7DB7EE8B/EC38D185 Hanzo), DB_NOUMIN_F_00 (Aggrieved Warrior).

## Rule (found in the data)
- Action ids are partitioned between these files. Over all 27 multi-file human sets only 11 system ids occur in two loaded files
  (0x0, 0x2, 0x61, 0x41A, 0xC51, 0x1035, 0x6AE, 0x3EE9-0x3EEC); 7 of them have the same action-object hash (+0xF0) in both files
  (= the same action object), and none of the 11 is an attack. => the boss's move list = the union of its loaded files' rows.
- The game picks rows by action-object hash (setting DB `CharacterActionObjectNameHashArray` -> row +0xF0, proven for Yoritomo in
  yoritomo-action-fields.md), so the files are not "overridden"; for the 4 differing duplicate ids the tool keeps the more specific
  (later) file. INFERRED, and it changes no attack / hit / sequence fact.
- Consequence: humans with the same weapon file get the same move list (Kondo = Kosaka = Green Shadow's Blade). Which of those moves a
  given duelist's AI actually uses is not in the action files (AI script); not derived.

## Limits
- The file set per boss comes from the blocks the sweep happened to see (the boss's current action must land in a file); a file that
  never held the current action is missing (e.g. Munenori: no 95A50CA1 slice seen). The runtime path should read whole file images
  (auto_moves_game.h header search) instead of the hash slice.
- Ogin, Denrinbo, Kawara: their weapon slices were 0-1 rows (scan saw nothing) -> only the common base resolved.
