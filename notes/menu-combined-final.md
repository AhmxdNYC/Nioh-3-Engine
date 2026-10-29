# pc/menu-combined-final (2026-10-18)

Base pc/training-combined f7fed86 (leave-arena escape, first-page menu) + pc/gate-modes 6f66c07 (incl. gate-always-present dee39d7)
+ cherry-pick autoretry-always-on 858df8b + the unfinished restyle from worktree agent-af9fc5dc (menu_style.h, hosted side panel 58 cols, EngineFix panel look).
Merge conflict: rematch.cpp item ids (LeaveArena and FightOpts both -49): FightOpts moved to -50.

User items applied (#13257/#13274/#13279/#13293/#13296):
- 'Rematch' -> 'Boss Matches' in every user-visible title, row, footer and toast (pipe replies / ini keys / logs unchanged).
- Practice page: Train here (where I am) / Train in a boss arena / Training Mods / Summon a boss where I am / Boss Matches.
- Section headers ("-- Name --" rows from rematch.cpp ItemGroup, drawn by EngineFix as gold headers with a rule) on Boss Matches,
  Return gate, Training Mods, Summon and Boss moves pages.
- Boss move page: 'Repeating move' row (A / Left stops) + 'Gap between repeats' slider; Right on a move picks it (drill).
- Boss Match rules (kill / finisher toggles) also on the Summon page, Training Mods and the Training Return gate.
- Settings: catalog groupLabels (plain group names, kConfigGroupLabels), CamelCase keys split into words, help text says
  'Saved at once; restart the game to apply' (auto-save) instead of the stale 'save to keep it'.
- Menu pause: Nioh3Training MenuPauseThread ([Training] MenuPause = 1).

Offline only (not installed, game not launched). Harnesses passing: run_menu_harness, run_rematch_bridge_tests (stale fixture fixed:
UiOpenAt stub), run_rematch_harness, run_transition_cover_tests, run_owned_registry_tests, move_integration_harness (cl /W4, updated
for the new Practice layout), test_pump_lifecycle / sites / spawn_restore / training_snapshot / verify_identity, generate_catalog --check.
Failing the same way on base f7fed86 (not touched): Nioh3Training run_move_toggle_tests (fixture lacks g_nsp/g_sp_actor),
run_settings_preservation (build.bat not on PATH), settings/test_catalog (uncovered ini keys), test_native_attempt / test_request_read_gates
(missing ReturnInfo / XwPending / CoverTick in their extracted sources).

## TECH_DEBT
- Menu pause is not live-tested. It reuses the F9 freeze stop counter (G+0x590+12, proven live #9658) under its own flag and is
  skipped while a load / swap / return / fade / arena settle / summon runs. Unknowns: the Training game-thread pump may not tick
  while paused, so rows that need a game tick (Idle stance request, Remove summoned bosses) take effect when the menu closes;
  real-time timers (HP-0 window, finisher kill timeout) keep running during the pause. Off switch: [Training] MenuPause = 0.
- Section headers are extra rows: the legacy (non-hosted) Rematch overlay shows them as plain "-- Name --" text.

## pc/menu-fixes-1 (ally #89 smoke on c3ab21b: 3 bugs)
- (1) Boss attacked ~1.5 s after the cover although stance = IDLE: BhTick switched the AI back on at grace end (66982C) and Idle stance
  only re-took it at the boss's next native idle (0x2) once IhEligible passed, so the boss attacked first. Fix: IhAdoptBossHold (idle_hold.h):
  at grace end, when the Training session wants Idle (default pending or request), the AI stays off and Idle stance owns the hold
  (phase 2, owns_ai). Round respawns / Train here bosses (SpawnTick, slot < 0, Idle wanted) get the same BOSSHOLD from frame one.
  CPU stance (Left) / session end restore the AI as before. No release on hit (hits play reactions; Left = CPU).
- (2) Boss A was 'empty': only menu slot summons registered g_slot_*; the arena boss (ArenaTick CreateBoss) also used a stale g_watch_chr.
  Now the Training session boss (arena boss and round respawns) becomes Boss A in WatchTick when slot A is free (no SummonPassiveMs hold),
  g_watch_chr = g_ar.chr. Summon page defaults to slot B when A holds it and B is free. New export Nioh3Training_BossName(chr, buf, cap)
  names bosses with no move list. Move lists: kN3Moves (notes/boss-action-ids-all.csv, pc2 #70) has 11 bosses only; every other boss
  shows 'Boss moves - <name> (none recorded yet)' and needs an ACTLOG capture first.
- (3) Arena / Train here / Summon rows printed the name with %.34s / %.40s: now the full name; the hosted panel word-wraps at 58 cols.
- Test fix: idle_hold_test expected a 10 s acquisition timeout, code is 20 s (failed on base too). Added adoption cases.

## pc/menu-fixes-2 (user #13529 / #13548, live log 11:47-11:59 on c3ab21b)
- (A) Gate floating: root cause in GateGroundY. The cross-world arena hold froze the player at the warp pose (y 15111) for ~9 s; those trail
  samples have the same x/z as the later standing ones (15016), so the 'nearest 3' pick tied and kept the frozen ones ('y 15111 -> 15111').
  Now the lowest sample within 500 u of the gate (+ the player's feet when near). Gate opened no menu: with ReturnGateAlways the Practice boss is
  alive all session and every own open path (pad A, stand-on, mask) was idle-only (!boss); only the native gadget interact was left, out of reach
  of the floating gadget (no GATE interact line in the log). Now in a Practice session or while Stand still holds the boss: tap A at the gate
  (ReturnGateRadius, |dy| < 250) opens our page (#13548 single click), and the face buttons are hidden from the game there (EngineFix lease;
  a held button stays hidden until released = hold-cancel), so the native hold-interact / mission menus never open.
- (B) Start x3 / Esc x3 / menu key open the current mode's page (EngineFix OpenModePage: Nioh3Training_SessionMode 2 or summoned bosses ->
  Practice, 1 -> Boss Matches, else the first page). B returns to the first page.
- (C) Return gate page has a Back row (gate is a base page now: Back -> host first page); long pages pin the Back row below the scrolled list.
- (D) Stance words: 'Stand still (idle)' / 'Fight normally' (+ 'Stand still + drill'). Repeat in Stand still: Idle hold keeps the AI off, so
  the AI never picked and the repeat never fired. New drill.h: while Stand still holds the boss and a repeat is set, the move is forced
  (66C4A8) whenever the boss is back at idle and the gap has passed. Fallback: 3 forces that do not start -> AI on for the drill only.
  Test: tests/drill_test.c (in run_move_toggle_tests.py; move_toggle_test still fails as on base).
- Not merged: pc/menu-fixes-3 (control-hint icons) had only uncommitted work at commit time.
- Needs in-game check: gate on the floor in a cross-world arena; tap A opens 'Return gate - Practice'; holding A opens no native box;
  Start x3 in Practice / Boss Match; drill plays the move with the AI off (log 'DRILL ... forced', or the fallback line).

## Repeat controls redo (user #13618, shot-13618)
- Root cause: offline the arm path works (Nioh3Training_SetMoveRepeat arms all 181 editable entries incl. 144 unlabelled, OFF moves too, Stand still drill
  reads the same MvRepeatId). The live log had no 'REPEAT' line because the user never armed one: Right on an OFF move only switched it ON
  (repeat needed old > 0, i.e. a second Right), A on a move never repeats, and the only feedback was the footer (cleared on every Up/Down).
- Now on a move row: A = ON / OFF (OFF also stops its repeat), Right = ALWAYS repeat (OFF -> ON first), Left = stop. Top rows: 'Repeat selected move'
  (A repeats the last move a button was pressed on, else the first ON move) and 'Repeating move: 0x4E30 <name>   Boss does 0x4E30 every 1.25 s,
  stands still between' (Fight normally: 'Boss uses 0x4E30 each time it attacks (gap 1.25 s)'). Move row value 'REPEAT' = amber pill (menu_style.h).
  2 s toast on arm / stop / failure (with reason). Logs: 'Rematch: REPEAT arm (...)' / 'arm FAILED' / 'stop', Training 'REPEAT chr .. ON (.., Stand still|Fight
  normally, move ON|OFF, hooks n)' and 'REPEAT .. refused: <reason>'.
- Tests: move_toggle_test fixture fixed (g_nsp / g_sp_* / ActIdOf, -fms-extensions) and extended with repeat arming; move_integration_harness covers
  Right-on-OFF arming, Left stop, Repeat selected move, REPEAT pill, top-row text, toasts. Needs in-game check: drill plays with the menu closed.
