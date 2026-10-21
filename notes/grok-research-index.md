# Grok research index (agents gx-pc, gxl-pc lead, gxl-pc2, gxl-ally; 2026-10-20..21)

All Grok work is offline/notes only. No Grok-authored files exist in this repo's notes/. Sources are .handoff/ plus a side worktree.
Bridge files: C:/Users/Ahmad Hamza/AppData/Local/uo-bridge/blobs/<sha256> (ids resolved via bridge.sqlite3 table `files`).
Grok pause write-up (read first): .handoff/gxl-pc-pause-22241.md. Also .handoff/done.md (rows 2026-10-20/21), checkpoints.md (lines ~827-1047), owners.md (~515-523), next.md (~1729-1806).

## 1. Picture-safe optimizations (PAUSED 10:49 am EST Oct 21, #22241)
- 73 picture-safe speedups in build, 20 on in live ini (MW W1 W2 VRR W3 R1 L1 WK FG W6 B J1 GF WG LK CP MQ PT1 DL PJ; RD and M2S on but not in build). Result #21931/#22098. Detail: bridge file 1277 (n3-ini-21931.json, blob sha de3c0196...), pause file.
- 10 graphics-API hooks (PJ PJH PL VrrPresent Barriers R2a TileBatch GIMerge CbvMirror MaxFrameLatency) might copy to the next Katana game; 54 hook Nioh 3 addresses and would need re-finding. Bridge file 1271 (n3-look-hooks-21918.txt, blob fcbf7094...). Result #21925.
- Measured: 13 of 64 have a number; only CBN gains >2% (pc2 only: 38.3 -> 44.3 fps, CPU 25.84 -> 22.29 ms; main PC +0.49%, Ally -0.16%). Barriers on RX 9070 PC about 7% worse.
- Status: unproven for next game; no injector built; no switch changed. Grok verdict: "unlikely without looking at the next game; lessons shorten the search".
- Grok next: wait for human "resume". Named, not run: Ally on-vs-off test of R2a and R1.
- Related repo notes: notes/stutter-pso.md, notes/tg-30hz-locate.md, notes/mainthread-logic.md (s8), notes/untried-optimizations-2026-10-15.md.

## 2. Shader warmup PJ / journal (file #1281)
- 64 MB journal pj_win_dgpu_sm68.bin = 67,108,773 bytes, written 6:56 pm EST Oct 11, in C:/Program Files (x86)/Steam/steamapps/common/Nioh3/Nioh3EngineFix_pj/ (exists, verified). Bridge files 1273 (n3-journal-21912.json) and 1281 (n3-journal-22017.json, blob 75018c1f...) are just restatements.
- PJ default 0 in [Patches]; live ini =1; code mod/Nioh3EngineFix/src/psojournal.cpp (tracked, in Oct 19 build).
- Hitch to beat 167 ms; 6 of 6 slow gameplay frames were shader compiles. TG 30 Hz: about 3% gain (not 26%), changes tap timing ~33 ms, on in this PC's ini.
- Status: on-vs-off pair never run (unproven). Next: journal run then prewarm run, same mission/boss. Checkpoints lines ~1010-1026.

## 3. Yoritomo (chr 16102) / boss attack tables (STOPPED 11:04 pm EST Oct 20, #21839)
- Summary files: .handoff/yoritomo-training-stop-21839.md (bridge 1270), .handoff/yoritomo-search-paused-21591.md.
- Numbers: 173 moves, 88 attacks, 6 sequences (4E53>4E54, 4E74>4EB7, 4E7A>4E8D, 4E7C>18D1, 4E93>4E5B, 4EB8>4EB9). 11,541 / 39,230 are branch-path counts, not sequences.
- 42 of 88 attacks have a choice link; 13 have one next move, 29 are choices (2 to 8 next moves); 46 stand alone. Detail: sections "Follow-up choice links", "One target or many" in attack-values (below).
- 14 bosses, 1784 moves, 280 attacks, 0 chains; 4 bosses (Shingen, Masakage, Mezuki, Okita) have the attack flag on 0 rows -> 14 bosses / 0 stored attack strings (per `boss_sequences.py` / sorter). Sorter not wired into menu.
- 34 attacks set unnamed byte +0x115, 54 clear (split 2 starts / 12 alone / 16 choices / 4 chains); not read by attack code, not an unblockable flag. done.md ~line 316-319; next.md ~1729.
- Hit counts: none stored. Hitbox table 182x192 rows, 4E68 owns rows 92-94 (not 3 hits). 221 HP was the training refill, not damage (damage builds 30/25/40).
- Frame search: 0 of 88 store startup/active/recovery; frame mark on 64 of 88 (~frame 14) unproven; motion 1040 = 221 frames, not a meter. File: notes/yoritomo-frame-fields.md.
- Details live in the SIDE WORKTREE, branch gxl-pc/move-sort-20999 (head cb65b0cf), NOT in this checkout:
  C:/Users/Ahmad Hamza/nioh3-sort-20999/notes/yoritomo-attack-values.md (43 KB, main detail),
  .../notes/yoritomo-frame-fields.md, yoritomo-action-fields.md/.csv, yoritomo-move-table-search.md,
  .../notes/yoritomo-sequences.txt (2.3 MB, 39,230 paths), .../notes/move-sort-20999/{move-sort.csv,report.txt,summary.json},
  .../tools/{sort_boss_moves.py,boss_sequences.py,test_sort_boss_moves.py}.
  Also branches cx-pc/yoritomo-native-14818, cx-pc/boss-action-record-13519 (Codex, not Grok).
- Status: paused/stopped, findings offline-proven, sequences "not a proven string". Grok next: do NOT launch practice fight or hunt frames/grabs/unblockable unless human asks. Open leads: command dword choosing hitbox slot; who writes u16 damage scale if gate opens.

## 4. Training search
- Same as section 3: practice block test asked then stopped, never launched (#21834/#21839). Menu unchanged.

## Could not locate
- Bridge ids are in the sqlite, not as named files (open blobs by sha256).
- Gxl-pc2 #21337 and gxl-ally #21336 results: dropped #21350; only in owners.md/next.md lines.
- No Grok notes/ file for optimizations beyond pause file; per-hook measured numbers (the 13 of 64) not found in a standalone file.
- Existing overview: notes/catchup-2026-10-21.md section 5.
