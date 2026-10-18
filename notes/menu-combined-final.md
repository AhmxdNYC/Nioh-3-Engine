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
