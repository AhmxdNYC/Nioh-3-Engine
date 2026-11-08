# Hardcode pass, refreshed on pc/final-187-79191 7144aa26 (2026-11-07)

Offline read of the sources on 7144aa26 (parent list: gx-pc/hardcode-76111 on 556af33d). No shipped code changed. Paths are under mod/. The boss list stays a file: Nioh3BossRematch_bosses.tsv (lesson, story_mission and cells are columns already). Generated headers (native_*_gen.h, fallback_rosters_gen.h, rvas_gen.h, sites_gen.h) are build output, not rules, unchanged: they stay.

## Move onto data the game or the catalogue already has

1. Dojo already in the room. BossRematch/src/rematch.cpp:2490 and transition_cover.h:38 (PreplacedRoute). Mission 0x1A5E, start 5021112, placement 5027806, one row. MOVE: mark the row from native_no_boss_mode_gen.h's generator (or a tsv flag), delete both literal copies. Same as before, not changed by the merge.
2. Two entrances. Now MOVED already: transition_cover.h:37 BranchRoute reads native_branch_entrances_gen.h:8 (0x1DEA, flag 187 -> alt placement 4097963). STAYS as generated build output; the old literal is gone. Nothing left to do.
3. Forced arena cells. Training/src/training.c:2813 kForceCells (0x15FF 3 cells, 0x2EE9 6 cells), used at :2833 only when the row has no cells. MOVE: fill tsv column 16 for those 2 missions, delete the array.
4. World 0xB1 cells. training.c:2581 kCellsB1 (17 cells), used at :2704 and :2884 when the row has none. MOVE: same fix, store on the rows.
5. Scroll mission number 0xCC96. Mostly moved: bosslog.cpp:667 and rematch.cpp:1521/3909 now read the row's story_mission (rcore::ScrollMission, rematch_core.h:184). STILL LITERAL: rematch.cpp:1155 (wave veto check) and rematch.cpp:2643 (is-scroll-run check). MOVE: compare against ScrollMission of the picked row. Low risk.
6. Wake table, 20 rows. rematch_core.h:562 kWaveWakeSteps[20] = {4,1,0,0,2,0,0,9,1,3,2,0,0,0,0,0,0,1,1,0}, indexed by parsing "E00".."E19" in WaveWakeStep (:563). Values are measured offsets from pc2 #78351, Ally #78283/#78389, not game data. MOVE the numbers to a tsv column (wake_step) on the 20 E rows, so a re-measure is a file edit and the id parsing goes; keep the offset list WaveNudgeAt (:541) in code. Until a measure for a row exists, 0 = old walk (correct fallback). Only E07 (step 9) and E00 (step 4) are expensive to get wrong.
7. Training no-waves skip. rematch_core.h:577 kWaveTrainWarps = 2 (warp at the wake spot twice, then give control), rematch.cpp:1157 VetoWaveRequest skips Emaki::RequestWaveBattle when g_scrollNoWaves is set. STAYS: it is behaviour (a design choice), not a per-row rule. Only its 0xCC96 check is item 5.

## Keep

8. Practice lock. rematch.cpp:1411 PracticeRow, :1422 PracticeReturnGate, 180 s cover at transition_cover.h:19. Row R1CBC, mission 0x1CBC, placement 65092, start 67064. STAYS until a catalogue practice column exists; 180 s is the measured Yamagata load, others get 90 s.
9. Five scroll item types. rematch.cpp:879 KindOf and :880 IsScroll (0x1E82, 0x516D, 0xE604, 0xDD82, 0xD523). STAYS: item types, not bosses; no item-category read yet. Also CountBagItem(0x1E82) in the new empty-bag path (same id).
10. Lesson sentence. rematch.cpp:3566 kLessonLine still says "hold RB", one sentence for all lesson rows, used at :5176, :5182, :5873. STAYS as one line; the lesson bit is a column. Do not add a sentence per row.
11. Favorites (new). rematch.cpp ~:511-:600 (FavPath, kFavBoss 'B' / kFavTrain 'T', file Nioh3BossRematch_favorites.txt next to the mod, atomic temp+replace). STAYS: a user file of row ids, no game data, no per-boss rule. Id check rcore::FavIdOk refuses id-less rows.
12. VRS tuning page (new). EngineFix/src/vrs_menu.h:7 six rows (Mode, Layouts, Rate, Skip, Keep, Reset), menu_pages.h:45-48 private subpage pVrsTune and row slots, Supported() needs query ok and Tier >= 1 (vrs_menu.h:4), layout names :53. STAYS: UI of a settings page; nothing per-boss or per-mission. The Tier 1 gate is a D3D12 capability read, not a literal about the game.

## Result
Move: 1, 3, 4, 5 (two literals left), 6 (new, the 20-row table). Already moved: 2. Stay: 7-12. Fresh finding vs the old list: the 20-row wake table and the two surviving 0xCC96 literals.
