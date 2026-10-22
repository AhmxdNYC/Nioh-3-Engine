# autoframe (pc2): hit/frame algorithm validated from the game's own archive (2026-10-22)

Algorithm = `tools/yoritomo_move_facts.py` (same start frame + hit group = 1 hit; overlapping same-group windows merge). Runs identically on all 95 bound action files.
New here: `tools/lfm_extract.py` + `tools/lfm_extract_all.py` extract the archive_01 action files straight from `Nioh3/archive/archive_01.bin` (no n3rdb.py needed; LFMB entry = 32 B {off, usize, csize, zlib flag}, 16 KB zlib chunks).
Pipeline: `py -I tools/lfm_extract_all.py && py -I tools/allboss_coverage.py`.
Results (pc2, game v2.02 files): Yoritomo 0x4E6E = 2 hits (windows 96-107, 143-158, unblockable) PASS; Gaki Chief 0x4E27 = 7 hits (28 rows, 7 windows 0-28 .. 247-286) PASS;
`--check` ok; coverage TSV over all 95 files is byte-identical to the committed notes/all-bosses-move-facts-coverage.tsv.
Remaining gap for "fully automatic": boss-name -> action file binding is still the hand-written dict in allboss_coverage.py (44 boss names unbound); needs binding via the character setting DB at runtime/offline.
