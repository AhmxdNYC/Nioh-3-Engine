# The test system

**What this is.** A map of every offline test, generator check and live in-game test script in the Nioh 3 mod project:
what each covers, how to run it, and how a run decides PASS or FAIL.
**Two layers.** Offline tests (no game, no window, no input) gate every build; live scripts drive a real game through
named pipes and are used only for behaviour that cannot be faked.
**Two rules.** A candidate build must keep every untouched feature passing (compared against a base build), and no test
may need a game file you cannot rebuild from your own install.

Source cited here is the v1.8.39 tree. Paths are repo-relative and carry no line numbers. Where a file exists in that
tree but not yet in this public copy, it is marked. See
[PUBLIC_EXCLUSIONS.md](../../PUBLIC_EXCLUSIONS.md) for what never ships (game bytes, decompiled dumps, text tables) and
[CONTRIBUTING.md](../../CONTRIBUTING.md) for the decompiling side. The map of the code under test is
[docs/code-map.md](../code-map.md).

## 1. Layers at a glance

| Layer | Needs | Where | Decides PASS by |
|---|---|---|---|
| Logic and "production code against stubs" tests | a C/C++ compiler, Python | `mod/*/tests`, `mod/shared/tests`, `mod/tests` | exit code 0 plus a final `PASS` / `ALL PASSED` line |
| Generated-file freshness checks | Python | `--check` modes of the generators | exit code (committed file differs from a rebuild) |
| Dump checks (patterns and RVAs against the real image) | a decrypted image of **your own** v2.02 install, Python, sometimes `capstone` and `numpy` | `mod/Nioh3EngineFix/tests/test_patterns.py`, `mod/Nioh3BossRematch/tests/test_sites.py`, `mod/Nioh3EngineFix/tests/test_padguard.py` | each pattern matches exactly once, at the expected RVA |
| Release and installer checks | Python (and a built ASI for the quick mode) | `release/check_release.py`, `release/tests`, `mod/settings/test_catalog.py` | exit code, `SUMMARY PASS` line |
| Live in-game scripts | the game, saves you can lose, the mods' pipes | `tools/rematch`, `tools/testmatrix`, `tools/tour` | log lines the mods write (hit landed, fight started, row exited) |
| Performance A/B | real hardware, PresentMon | `tools/testmatrix` | paired statistics with a confidence interval, not pass/fail |

Almost everything in the first four layers is deliberately **not a game test**. The mods run inside `Nioh3.exe`, so the
tests extract the real production function from the source, compile it against small stubs, and drive it with fake
memory, fake clocks and fake game observations. A failing test means the logic is wrong, not that the game moved.

## 2. Toolchains and the NDEBUG trap

Two compilers are used, chosen per test. The runner picks its own; you do not choose.

* **MSVC x64** (`cl`). The EngineFix and BossRematch native harnesses (`run_*_harness.py`, `run_*_test.py`) call a
  portable MSVC environment script. Runners look for `tools/msvc/setup_x64.bat` in the checkout, then honour the
  environment variable `N3_MSVC_SETUP`, and some fall back to a fixed location on the original maintainer's disk. If you
  run from a Developer Command Prompt with `cl` on `PATH`, many runners work as they are; for the rest, point
  `N3_MSVC_SETUP` at your own `vcvars64`-style batch file. The portable MSVC tree itself is not distributed here.
* **zig cc / zig c++** (`py -m ziglang cc ...`, from the `ziglang` pip package). Used by most Nioh3Training and many
  Nioh3BossRematch tests because they compile plain C or small C++ extracts and need no Windows SDK headers. Some use
  `-target x86_64-windows-gnu`.

**The NDEBUG trap.** `zig cc -O2` (and `zig c++ -O2`) defines `NDEBUG`. Every `assert()` in an assert-based test then
compiles to nothing, and the test prints PASS while checking nothing. Rules that follow:

* Build assert-based tests with `-O0`, or keep `-O2` and add `-UNDEBUG`. Every zig invocation in the tree uses one of
  these (`-O2 -UNDEBUG` or `-O0 -UNDEBUG`). Examples: `mod/Nioh3Training/tests/run_move_toggle_tests.py`,
  `mod/Nioh3Training/tests/run_owned_registry_tests.py`, `mod/Nioh3BossRematch/tests/test_native_gate_retry.py`.
* Some `.c` tests also put `#undef NDEBUG` before `#include <assert.h>` (for example
  `mod/Nioh3Training/tests/frame_meter_test.c`), so they are safe when compiled by hand.
* MSVC equivalents are `/Od /UNDEBUG` (see `mod/Nioh3EngineFix/tests/run_holdkeygap_harness.py`). MSVC does not define
  `NDEBUG` unless you ask, but the explicit flag documents intent.
* When you add a test, grep its build line for `-O2` and make sure `-UNDEBUG` (or `-O0`) sits next to it. If you rebuild
  an old test by hand with `-O2` and no `-UNDEBUG`, do not trust the PASS.
* MSVC harnesses compile with `/W4`; some treat any compiler warning as a failure (`run_padmap_harness.py` scans the
  compile output for `warning`, `mod/tests/run_rematch_bridge_tests.py` uses `/WX`).

Other dependencies: Python 3.10 or newer (always run as `py -I`, isolated mode, so a stray `json.py` in the current
directory is never imported), `capstone` and `numpy` for dump-based disassembly checks, `git` for
`mod/settings/test_catalog.py`.

## 3. How tests are written

Four patterns cover nearly all of `mod/*/tests`:

1. **Extract-and-compile.** The `.py` file reads `src/training.c` or `src/rematch.cpp`, slices out named functions by a
   marker string (`text.index('static int ...')`), strips attributes, prepends stubs for the game and Win32, compiles and
   runs. Examples: `mod/Nioh3Training/tests/native_entry_test.py` runs the real native-entry state machine against
   scripted game observations; `mod/Nioh3BossRematch/tests/test_native_attempt.py` compiles the real admission, cleanup
   and pump bodies. Consequence: **renaming or moving a marker in the source breaks the test with a `ValueError` from
   `index()`.** That is the intended tripwire (it forces you to look at the test). Update the marker; do not delete the test.
2. **Header-only logic.** Cores live in headers (`mod/Nioh3Training/src/frame_meter.h`,
   `mod/Nioh3EngineFix/src/vpad_core.h`, `perfsnap_core.h`, `titlegate_core.h`, `updatecheck_core.h`,
   `mod/Nioh3BossRematch/src/rematch_core.h`) so a test can `#include` them directly.
3. **Real object, fake world.** Harnesses such as `mod/Nioh3EngineFix/tests/vpad_harness.cpp` link the real `.cpp` (hook
   installer, named-pipe server, IAT hook) and give it a fake 60 Hz game thread, fake IAT slots and real pipe clients.
4. **Data audits.** Tests that parse the shipped `.tsv` and `.json` tables and assert invariants, for example
   `mod/Nioh3Training/tests/arena_safety_test.py` (parses `Nioh3Training_arenas.tsv`),
   `mod/Nioh3Training/tests/fallback_roster_data_test.py`, `mod/Nioh3Training/tests/native_entry_box_data_test.py`.

## 4. Offline tests by area

Counts are approximate (files, including helpers). The public copy holds an older subset of these; the sections say when
a group is newer than the public copy.

### 4.1 Nioh3EngineFix (`mod/Nioh3EngineFix/tests`, about 190 files)

Run from `mod/Nioh3EngineFix`: `py -I tests/run_<name>.py` (most accept an optional dump path as the first argument).
Each runner builds its `*_harness.cpp` or `*_test.cpp` and ends with `ALL PASSED` or an equivalent line.

| Group | Runners and tests | What they prove |
|---|---|---|
| Hot-patch plumbing | `run_gimerge_harness.py`, `run_motionguard_harness.py`, `run_aichainguard_harness.py`, `run_assertguard_harness.py`, `run_resinfoguard_harness.py`, `run_pollfix_harness.py`, `run_readfix_harness.py`, `run_psoswap_harness.py`, `run_psomemo_harness.py` | each code cave or guard behaves on synthetic inputs; a caller that is not listed reaches the original instruction |
| Pad and input | `run_padguard_harness.py`, `run_padmap_harness.py`, `run_padlat_harness.py`, `run_pad_family_test.py`, `run_padcard_test.py`, `run_padrec_test.py`, `run_holdkeygap_harness.py`, `menu_pad_input_offline.py`, `nav_repeat_axis_offline.py`, `inputlog_test.cpp`, `inputhist_test.cpp` | list-walk guards, the map-miss cave, pad family detection, input ring and history, menu pad gestures and repeat |
| VirtualPad | `run_vpad_harness.py` (`vpad_harness.cpp`) | command parser, the real XInput IAT hook on fake slots (exclusive, merge, disarm), a real named-pipe server with several clients, keys hidden while unfocused, the `KeepRunningUnfocused` thunk run natively and disassembled |
| Menu | `run_menu_harness.py`, `run_menu_defaults_test.py`, `run_menu_prompts_test.py`, `run_menu_tabbar_test.py`, `run_btnstyle_migrate_test.py`, `run_settings_preservation.py`, `menu_*_test.cpp` | page order, the shipped-defaults header, prompts, tab bar, button-style migration, user settings are not overwritten by an update |
| Loading and streaming | `run_loadstats_harness.py`, `run_loadmem_harness.py`, `run_flp_load_harness.py`, `run_tilestats_harness.py`, `test_drainwait_native.py`, `test_meshletwait_binding.py`, `run_zinflate_test.py` | synthetic call sites and heaps; the inflate replacement against Python `zlib` (valid and corrupt streams) |
| Reports and crash | `run_crashblame_test.py`, `run_crashupload_test.py`, `run_reportsend_test.py`, `test_dxgidiag_report.py`, `run_perfsnap_test.py`, `run_applybatch_failure_audit.py` | crash attribution, report size caps, upload payloads, perf snapshot maths |
| Updater, title, misc | `run_updatecheck.py`, `run_titlegate_test.py`, `run_walkfix_default_test.py`, `run_thrsched_harness.py`, `run_timerres_harness.py`, `run_tour_harness.py`, `run_sbm_harness.py`, `run_samelook_harness.py`, `run_vrr_floor_harness.py`, `vrs_*_test.cpp` | each feature's pure logic |
| Pattern and RVA | `test_patterns.py`, `test_padguard.py`, `site_perf_check.py` | see section 5 |
| Real D3D12 device | `run_gp_harness.py`, `run_hot_harness.py` | GPU pass profiler and shader hot-swap against a real adapter. Not for hosted CI without a GPU |
| Benchmarks and witnesses | `run_cbf_bench.py`, `run_cbs_bench.py`, `run_cbvupload_recreation_witness.py` | measurements and counterexample reproducers. A witness's exit 0 means it reproduced the modelled counterexample, not that production behaviour is safe |

Test files with no runner: about 25 `*_test.cpp` and `*_harness.cpp` files (for example `inputlog_test.cpp`,
`menu_mode_test.cpp`, `loop_breaker_test.cpp`, `vrs_menu_test.cpp`, `savelib_harness.cpp`) are not referenced by name in
any `run_*.py` (unverified: a runner that globs could still pick one up). Their header comment usually gives a build line
you can run by hand. They are **not** covered by the regression suite in section 8 unless you add a runner.

Most of this directory is: the public copy holds roughly 120 of the 190 files.

### 4.2 Nioh3BossRematch (`mod/Nioh3BossRematch/tests`, about 60 files, plus `mod/tests`)

Run from `mod/Nioh3BossRematch`: `py -I tests/<name>.py`. The wrapper compiles production bodies of `src/rematch.cpp`,
`src/bosslog.cpp`, `src/main.cpp`, `src/hooks.cpp` and the headers with stubs.

| Area | Tests | What they prove |
|---|---|---|
| Whole add-on harness | `run_rematch_harness.py` (`rematch_harness.cpp`) | catalogue and verified-file merge on the real `Nioh3BossRematch_bosses.tsv`, map redirect and restore, E-row lifecycle (retry, exit, timeout), entry thunk with two handlers, chaining onto another module's hook. Also wiring checks: `build.bat` compiles every `src/*.cpp`, the shipped ini keeps `Enable`, `Pipe` and the `[Debug]` loggers at 0, the catalogue and `sites_gen.h` match their generators |
| Fight now lifecycle | `test_native_attempt.py`, `test_pump_lifecycle.py`, `test_request_read_gates.py`, `test_native_entry_gates.py`, `test_native_gate_retry.py`, `test_retired_gate_again.py`, `test_fight_result.py`, `test_verify_identity.py` | admission, cleanup, tracking, gate retry, win and loss detection, spawn identity |
| Transition cover | `run_transition_cover_tests.py` (`transition_cover_*_test.cpp`, `transition_native_read_test.cpp`) | the load-cover policy, readers and presentation tickets (`src/transition_cover.h`, `src/transition_native_read.h`) |
| Training bridge | `test_training_snapshot.py`, `test_spawn_restore.py`, `test_roster_ownership.py`, `test_catalog_trigger_links.py`, `test_selftest_hold.py`, `selftest_scroll_offline.py`, `unbeaten_menu_test.py` | what is handed to Nioh3Training, restore of spawned actors, roster ownership, in-mod self-test pacing |
| Compass and walk targets | `test_white_dot.py` | parses the add-on's `white dot` log lines; the newest line for a row wins |
| Menu bridge | `mod/tests/run_rematch_bridge_tests.py` (`rematch_menu_bridge_test.cpp`) | the ABI between the Boss Rematch UI and the EngineFix menu host; builds with `/W4 /WX` |
| Site table | `test_sites.py` | dump check, section 5 |
| Direct fixtures | `formhide4_offline.py`, `favorites_offline.py`, `press_queue_offline.py`, `page_defaults_offline.py`, `despawn_gate_offline.py`, and others ending `_offline.py` | one feature each, local buffers only |

In the current tree the BossRematch tests live under `mod/Nioh3BossRematch/tests`. The public copy still keeps an older
layout under `mod/tests`. The `*_offline.py` fixtures listed above, `test_fight_result.py`, `test_white_dot.py`,
`test_roster_ownership.py` and `test_selftest_hold.py` are.

### 4.3 Nioh3Training (`mod/Nioh3Training/tests`, about 75 files)

Training is plain C (`src/training.c` plus headers). Tests compile with zig (`-O2 -UNDEBUG` or `-O0 -UNDEBUG`).

* **Wrapped runners.** `run_move_toggle_tests.py` first checks that the generated `mod/boss_moves.h`,
  `src/move_motion.h` and `Nioh3Training_arenas.tsv` equal their generators' output (`tools/gen_boss_moves.py`,
  `tools/gen_move_motion.py`, `tools/gen_summon_rows.py`), then builds and runs about 20 `.c` tests (move veto, drill, gate
  zone, boss vitals, aggro, auto-bind, last-attack rule and so on). Also `run_move_toggle_available.py`,
  `run_owned_registry_tests.py` (native-target lifecycle, built `-Wall -Wextra -Werror`) and `run_frame_meter_draw.py`
  (style meter and frame meter drawing and counts).
* **Extract-and-compile `*_test.py`.** The native-entry family (`native_entry_test.py`, `native_entry_sequence_test.py`,
  `native_entry_return_test.py`, `native_phase_test.py`, `native_proceed_test.py`), session end rules
  (`kill_ends_session_test.py`, `despawn_ends_session_test.py`, `dead_despawn_defer_test.py`), `ki31_guard_test.py` (the
  Ki-style hook's bad-pointer guard, including a faulting pointer), `boss_match_timer_test.py`,
  `boss_match_persistence_test.py`, `roster_combat_guard_test.py`.
* **Data audits.** `arena_safety_test.py`, `fallback_roster_data_test.py`, `native_entry_box_data_test.py`.
* **Unwired `.c` tests** (no runner references them by name): `style_meter_test.c`, `fightlog_test.c`, `kb_slot_test.c`,
  `kg_world_test.c`, `player_clock_test.c`, `player_speed_test.c`, `practice_freeze_test.c`, `soul_core_swap_test.c`,
  among about 18 in total. Build by hand with the same flags, and wire them into a runner when you touch their code.
* Fixtures live in `mod/Nioh3Training/tests/fixtures`.

Many of the newer Training tests (the native-entry family, the session-end tests, the `*_test.c` files for style meter,
fight log, player clock and speed) are.

### 4.4 Shared code (`mod/shared/tests`)

`run_gdi_stroke_tests.py` builds `gdi_stroke_test.c` with MSVC: the vector text and shape renderer used by overlays.
`known_build_test.c` covers `mod/shared/known_build.h`.

## 5. Generated files and dump checks

The mods keep game addresses and tables in data files and generate headers from them. A stale header is a silent bug, so
each generator has a check mode that exits non-zero if the committed file differs from a rebuild.

| Check | Command | Guards |
|---|---|---|
| Patch table | `py -I mod/Nioh3EngineFix/tools/gen_header.py --check` | `patches.json` against `src/patches_gen.h` |
| Pattern table per mod | `py -I mod/<Mod>/tools/gen_rvas.py --check` | every fixed RVA used in that mod's source has a pattern in `rvas.json`, and `src/rvas_gen.h` is current |
| Site table | `mod/Nioh3BossRematch/tools/derive_sites.py` (exercised by `test_sites.py`) | `sites.json` against `src/sites_gen.h` |
| Menu catalogue | `py -I mod/settings/generate_catalog.py --check` | `mod/settings/catalog.json` against the menu sources |
| Profiles | `py -I release/make_profiles.py --check` | `release/profiles.json` |
| Boss catalogue | `py -I tools/rematch_catalog.py --check` | `mod/Nioh3BossRematch/Nioh3BossRematch_bosses.tsv` and its CSV against a rebuild |
| Move and arena tables | `tools/gen_boss_moves.py`, `tools/gen_move_motion.py`, `tools/gen_summon_rows.py` | compared inside `run_move_toggle_tests.py` |
| Fixed RVAs | `py -I tools/check_fixed_rvas.py` | lists every hard-coded `base + 0xRVA` that is not resolved by pattern; exit 1 unless the use is in `tools/fixed_rvas_allow.txt`. `--tests` includes test sources. `--json` output feeds `tools/aob_audit.py`, which proposes a unique byte pattern for each |

**Dump checks.** `mod/Nioh3EngineFix/tests/test_patterns.py [dump]`, `mod/Nioh3BossRematch/tests/test_sites.py [dump]` and
`mod/Nioh3EngineFix/tests/test_padguard.py [dump]` take a memory-layout dump of the decrypted v2.02 image (file offset
equals RVA). Make it from your own install with `tools/progress/dump_nioh3.py` (see
[CONTRIBUTING.md](../../CONTRIBUTING.md)). The on-disk `Nioh3.exe` keeps `.text` encrypted, so scanning it matches
nothing. Checks: build identity (PE timestamp and size) matches; every pattern matches **exactly once** in the executable
sections at the expected RVA; the dump bytes at each site equal `orig`; patch sites are instruction-aligned and do not
overlap; hook heads are whole relocatable instructions; globals resolve through their rip-relative anchor. Many harnesses
also read the dump to ground fixtures in real bytes (the runner takes the path as its first argument or has a default
location). Never commit or share the dump.

## 6. Release and settings checks

* **`mod/settings/test_catalog.py`** (no compiler, no game). Verifies the settings catalogue is the single source of truth:
  no duplicate keys; every shipped ini key has a catalogue row or is listed as `iniOnly`; each default equals the shipped
  ini; bounds and choices are sane; keys flagged `codeDefault` really differ from the shipped ini; no shipped default
  changed since the last release unless listed in `INTENDED_FLIPS`. It also diffs protected sections of
  `mod/Nioh3EngineFix/src/menu.cpp` and `mod/Nioh3BossRematch/src/rematch.cpp` (item enum, page order, ini key table, pipe
  command table) against the last release: they may **grow**, but no existing entry may be removed, renamed or
  reordered. It reads the baseline from git (`git show <RELEASE>:<path>`) through the pinned constant `RELEASE`, so it needs
  repository history. **Bump `RELEASE` to the shipped commit at every release**, or the test blocks the next one. In
  this public checkout the pin names a commit that does not exist; run it against your own history or re-pin it.
* **`release/check_release.py`.** Quick mode: ini lint (duplicate keys, repeated sections, control characters), every
  base-ini key is read by the source, the profiles check, `MOD_VERSION` from `mod/Nioh3EngineFix/src/dllmain.cpp` appears
  in the built ASI and in `MODLOG.md`, the built ini equals the source ini, and the Boss Rematch ASI really contains
  soul-core support (`--br PATH` runs only this gate). `--full` adds `test_patterns.py` and every
  `tests/run_*_harness.py` of EngineFix. `--strict` also fails on warnings. Quick mode needs a built ASI.
* **`release/tests`.** `test_install_mod.py` (installer keeps saves and settings, verifies hashes, backs up),
  `test_restore_vanilla.py` (the "Restore Vanilla Game" and "Put the mod back" shortcuts), `test_scrub_pdb.py` (PE privacy:
  debug-path scrubbing, malformed files, no overwrite). Plain `unittest`, standard library only.
  `release/test_publish_github.py` checks that the publisher refuses any repository other than the public one
 .
* **Updater.** `tools/updater/test_updater.py`, `updater_selftest.py`, `test_exe_e2e.py` (manifest validation, SHA-256
  mismatch rejection, settings preservation, rollback, no-op repeat); `verify-offline.py` in the repo root runs the staged
  payload scenario end to end with no network.

## 7. Other offline tools worth knowing

* `tools/testhub/hub.py`: a catalogue and a five-check offline suite (`py -I -B tools/testhub/hub.py run offline`).
  `plan all` prints commands for every lane without running anything. Only the offline lane is executable; native, dump,
  device and release lanes are plans.
* `tools/testmatrix`: synthetic-input tests (`test_tm_stats.py`, `test_pm_time.py`, `test_process_cpu_window.py`,
  `test_guard.ps1`, `test_pinned_benchmark.ps1`) for the statistics and harness-safety code.
* `tools/bench/test_*.py`, `tools/test_chain_hits.py`, `tools/test_hit_labels.py`, `tools/row_display_name_test.py`:
  fixture tests for analysis scripts.
* `tools/rematch/test_*.ps1`: PowerShell fixture tests for the pilot helpers (no game).
* PowerShell scripts are run as `powershell -NoProfile -ExecutionPolicy Bypass -File <script>`; most accept `-DryRun`.

## 8. Regression suite and the "untouched features must keep passing" rule

The rule: a change may alter only the behaviour it was written for. Everything else must pass exactly as it did before,
and each build is checked for that explicitly. Every candidate build gets these passes before anyone launches the game:

1. **Untouched-parts tests.** Run the tests of every mod and feature the change did *not* target, not just the ones next
   to the diff. A change to menu code must not break the pad guard or the Training session tests. Typical record of such
   a pass: "pad guard, pad map, settings preservation, menu defaults, report send all passed".
2. **Base versus candidate.** `tools/testmatrix/regsuite.py` runs the whole offline suite on a candidate tree and, with
   `--base`, on a base tree too, then compares per test:

   ```
   py -I tools/testmatrix/regsuite.py TREE [--base BASETREE] [--out DIR] [--only mod:glob ...]
   ```

   It globs `tests/*.py` for BossRematch, `tests/run_*.py` and `tests/*_offline.py` for EngineFix, and `tests/*_test.py`
   plus `tests/run_*.py` for Training. Each file runs as `py -I <file>` from the mod directory with the MSVC environment
   loaded (600 s timeout per test), one log per test, one line per test: `head PASS|FAIL <mod:file> | <last line>`.
   The MSVC setup path is a constant at the top of the file; edit it for your machine.
  
3. **Generated-file and catalogue checks** from sections 5 and 6, and a "no hard-coding" audit (below).

**How regsuite decides a single test.** It FAILS if the exit code is non-zero, or if the output contains a failure
marker (`FAIL`, `FAILED`, `Traceback`, `AssertionError`, `error C`, `error LNK`, `fatal error`, `TIMEOUT`), unless the last
non-empty line says `PASS`, `ALL PASSED` or `OK`, the exit code is 0 and there is no traceback. Lines like "0 failed" or
"fails=0" are ignored. This stops a log that merely mentions a failure from counting as one and, more importantly,
stops a failing run from ever reporting "all passed" (an earlier shell loop printed `rc=0` for failing tests).

**How the base comparison decides.** Without `--base`, exit 0 only if every test passes. With `--base`, exit 1 only for
**new failures**: tests that fail on the candidate and pass on the base. Failures that already exist on the base are
listed as pre-existing and do not block, but must be written down, not ignored. Tests present only on the candidate are
listed as "only on head". Also compare the build output with the previous build: note compiler warnings (for example "only
the two old C4456 warnings") and confirm no exported symbol, ini key or menu row disappeared (`test_catalog.py` does the
last two).

**No-hardcode audit.** Mission ids, boss ids, placement ids, coordinates, wake offsets and similar facts belong in data
files (`Nioh3BossRematch_bosses.tsv`, `Nioh3Training_arenas.tsv`, `Nioh3Training_actionsets.tsv`, `sites.json`,
`rvas.json`), not as literals in the code. A maintainers' audit script scans the C and C++ sources for such literals, and a
build must not add findings beyond the recorded baseline. **That script is not in the repository** (details unverified).
If you re-create it: it audits the tree it is run from, so run it from inside the candidate tree rather than a different
checkout, and accept a finding only if it is a behaviour constant (a design choice, an item type) and not a per-boss rule.
The in-repo part of the same discipline is `tools/check_fixed_rvas.py`.

**Release pins.** Before a release: bump `RELEASE` in `mod/settings/test_catalog.py`, confirm `MOD_VERSION` (EngineFix
`dllmain.cpp`) and `ADDON_VERSION` (BossRematch `addon.h`) match what ships, and run `release/check_release.py --strict`.

## 9. Live in-game tests

Live scripts exist because some behaviour cannot be modelled: whether the player is really inside the arena, whether a
load completes, whether a hit lands. They are slow and fragile, so each automates one bounded scenario and decides PASS
from lines the mods write to their own logs. They never use OS-level input.

**Rules for every live run** (the scripts enforce most of them):

* Run on a game you own, with saves backed up and restored byte for byte afterwards, and with nobody playing.
* Close the game before the run. Restore every ini and ASI file you touched afterwards (scripts do this in a `finally`).
* Input goes only through the VirtualPad pipe (below); nothing is sent to the foreground window.
* Kill only the process ID you launched.
* A supervisor process ID is passed in (`--owner-pid` or `-OwnerPid`). The run aborts if that process disappears, so a
  dead controller can never leave a harness driving the game. `tools/testmatrix/harness_guard.ps1` also enforces a time
  limit, minimum free commit headroom before each launch (default 18 GB), a limit of two crashes per run, and a pid file
  for cleanup. `tools/testmatrix/test_guard.ps1` is its dry-run self-test.
* Several wrappers also take a machine-wide "game lock" through private coordination tooling. Replace it with your own
  mutual exclusion if you reuse them.

### 9.1 The pipes

| Pipe | Mod and switch | Commands |
|---|---|---|
| `\\.\pipe\nioh3vpad` | Nioh3EngineFix, `[Debug] VirtualPad = 1` (exclusive: slot 0 is virtual, slots 1 to 3 report not connected) or `2` (merge with real pads). `[Debug] KeepRunningUnfocused = 1` stops the game ignoring pads while unfocused | one line in, one reply line (`ok ...` or `err ...`): `press <btn[+btn]> [ms]`, `hold`, `release <btn\|all>`, `stick L\|R x y [ms]`, `trigger L\|R v [ms]`, `key <name> [ms]`, `pad` (whole pad state in one step), `menu get\|set <KEY> [v]`, `state`, `realpads on\|off`, `arm`, `disarm`, `wait`, `log`, `ping`, `help` |
| `\\.\pipe\nioh3bossrematch` | Nioh3BossRematch, `[Remote] Pipe = 1` (default 0; local clients only) | see below |

Source: `mod/Nioh3EngineFix/src/vpad.cpp`, `vpad.h`, `vpad_core.h` (the VirtualPad pipe);
`mod/Nioh3BossRematch/src/remote.cpp` (the pipe server) and `mod/Nioh3BossRematch/src/rematch.cpp` (the command table, in
the function `Command`).

`state` on the VirtualPad pipe reports the frame counter, whether the game thinks it is active, loading, paused, the
player position, and `field=1` (player present, not loading, not paused, no game menu). `menu set KEY VALUE` runs the mod
menu's own change code on the Present thread (same log line as a key press, no navigation). Virtual keys go through the
menu's DirectInput keyboard hook and need `[Menu] Enabled = 1`. While the game is not foreground, the real keyboard state
is hidden from it, so typing in another window never reaches the game. A press lasts at least two game frames and the
reply comes after release. `VirtualPadIdleReleaseSec` (default 600) releases held inputs if the controller goes quiet.
The client is `tools/testmatrix/vpad.py` (one command, `run <file>`, `wait-pipe`, `in-game`, `title-continue`,
`selftest`; exit codes 0 ok, 1 mod said `err`, 2 no pipe, 3 owner process gone, 4 timeout) and `vpad.ps1` for PowerShell
wrappers.

**Boss Rematch pipe commands** (one per line, one reply line):

| Command | Reply or effect |
|---|---|
| `ping` | `pong Nioh3BossRematch <version> live=<0\|1>` |
| `status` | state, active row, fights, mission id, gate reason, last result, player position |
| `cover` | transition-cover state: token, phase, attempt, mission, deadlines |
| `scan`, `row <id>` | rescan the save, then status of one catalogue row |
| `start <id>` | start the row's native mission; `refused <reason>` when not eligible |
| `fight <id>` | Fight now, in the arena: native mission load when the player is elsewhere, the Training in-place arena swap when already in that world. Replies `queued <id> fight start=<n>`, `refused <reason>` or `error no row <id>` |
| `shrine <id>` | the start point alone |
| `trainscroll <id>` | the Training scroll request, for waves rows |
| `again` | the gate page's "Fight again" |
| `exit`, `leave`, `abort` | leave the running mission as the mission menu does; end a stuck arena session; abort the request |
| `open`, `scroll` | open the menu page; the scroll request |
| `hostfight [row]` | hosted Fight-now test path; needs `[Remote] HostedFightTest = 1` as well |

There are also developer commands (`scrollctl`, `spirit`, `tenshin`, `peek`) used for reverse engineering, not for tests.
Row ids come from `Nioh3BossRematch_bosses.tsv`: a route letter then the mission or index, for example `R1CBC`.

**Training command file.** `Nioh3Training.cmd` in the game folder takes one command per line, consumed on the game
thread; results go to `Nioh3Training.log`. About 120 commands (list in the header of `mod/Nioh3Training/src/training.c`),
for example `fnow <mission hex>`, `train <row or char>`, `trainstop`, `summon`, `slot`, and `rm <text>`, which forwards a
line to the Boss Rematch pipe from inside the game.

### 9.2 `tools/rematch/arena_walk_test.py` and `arena_walk_run.ps1`

**Question it answers:** does "Train in arena" really put the player *inside* the arena, with no fence, gate, wall or
wrong floor between the player and the boss?

For each Training arena row it:

1. reads rows from the installed `Nioh3Training_arenas.tsv` (rows whose kind column is `arena`; `--rows R1CBC,R15FF`
   selects some);
2. sends `fight <id>` on the Boss Rematch pipe (the same request as the menu);
3. waits up to `--load-sec` (default 90) for either the Training log line `ARENA ... CONTROL: player released at x y z`
   (in-place swap route) or `FIGHT start: [<id>]` (native load route). Neither within the limit is `NO-SESSION`;
4. waits 2.5 s for the boss AI grace period, presses RS to lock on, then for up to `--walk-sec` (default 30) holds the left
   stick forward for 900 ms and presses the attack button (`--attack`, default `X`), tailing `Nioh3Training.log`;
5. **PASS** = at least one `STYLE MOVE: ... | HIT |` line, or `landed N` with N above 0, in the Training log (it stops
   early at two hits). **FAIL** = no hit within `--walk-sec`. It also records the closest distance from
   `BOSSMON ... dist N m` lines and the player's start and last position, so a FAIL shows where the player got stuck;
6. sends `trainstop` to the Training command file, and `exit` on the Boss Rematch pipe between native-route rows.

It needs the style log: `Nioh3Training.ini [StyleMeter] AlwaysOn = 1` (the `HIT` lines come from the style meter).
`--repeat 2` runs each row twice (a native load, then an in-place swap inside that world). `--dry-run` lists the rows.
It refuses to run without `--i-own-the-game` and a nonzero `--owner-pid`. Output is
`captures/rematch/arenawalk-<stamp>.csv` with columns `row, mission, route, verdict, first_hit_s, hits, min_dist_m, start,
end`; the exit code is 0 only if every row passed.

`--learn` is a data-collection mode. For every Boss Rematch row with no recorded fight-start spot in
`notes/arena-fightstart.csv`, it sends `fight <id>`, walks forward until the log shows `BossBattleMode start - player now`,
then sends `exit`. Afterwards run `tools/harvest_fightstart.py` and `tools/auto_arena.py`, install the regenerated
`Nioh3Training_arenas.tsv`, restart the game, and run again without `--learn`.

`arena_walk_run.ps1 -Rows R1CBC` is the wrapper and the reference proof run. It closes the game, takes the lock, backs up the
save folder, copies the inis and the Boss Rematch ASI, sets `Pipe = 1` in the Boss Rematch ini, forces `VirtualPad` in the
EngineFix ini, launches through Steam, waits for the VirtualPad pipe, presses through title and Continue until `field=1`,
runs the walk test as a child process, copies logs and the CSV into a run folder, then restores inis, ASI and saves exactly,
releases the lock and relaunches the game for the player with VirtualPad off. Time limit: 14 minutes. Both files are
, as is `tools/testmatrix/vpad.py`.

### 9.3 Fight-now tests and related scripts

* **`tools/rematch/fightnow_test.ps1`.** Unattended test of "Fight now (in the arena)" over catalogue rows that have an arena
  point. Per row: `fight <id>` on the pipe, the mission loads at the boss start point, the add-on warps into the arena
  trigger and verifies. `status` reports `fnow=pass` when a `BossBattleMode` started after the warp and `fnow=fail` when none
  did within 8 s after up to three warps. Then `exit`, next row. Results go to `captures/rematch/fightnow-<stamp>.csv`.
  Options include `-Rows`, `-IncludeAtFight`, `-Retry` and `-RetryMaxSec` (timed "Fight again" runs through the pipe
  command `again`), `-WalkSec`, `-AddonAsi` (test a build other than the one in `mod/Nioh3BossRematch/bin`),
  `-PerfSet "Section/Key=Value"` (extra keys in the test EngineFix ini only) and `-DryRun`. It waits for the game to be
  closed rather than touching a running one, and restores the save folder with `robocopy /MIR`.
* **`tools/rematch/fightnow_selftest.py`.** The same idea with **no pipe and no input**, driven by the add-on itself. With
  `[Rematch] SelfTest = arena` (or a comma list of row ids) in `Nioh3BossRematch.ini`, the add-on starts Fight now on every
  row that has an arena point or boss gate and is **cleared on this save** (uncleared rows log `SELFTEST <id> SKIP` and are
  never started), waits for PASS or FAIL, leaves the mission, and ends with `SELFTEST DONE`. Related ini keys:
  `SelfTestHoldMs` (stay after the verdict, default 4000), `SelfTestPlaceAt` and `SelfTestStagedFinal` (test only). The
  Python script is the framing: it refuses if the game runs, backs up saves and touched files, installs the add-on and test
  keys, launches with a command you pass, follows the log, restores files and saves byte for byte, and writes a CSV of id,
  result and load seconds. Exit 0 = ran (read the CSV), 2 = refused, 3 = no `SELFTEST DONE` before `--timeout-min`.
  It works wherever the game can be launched, including Linux under Proton.
* **`tools/rematch/verify_all.ps1`.** Per-boss verifier. Per row: `scan`, `row <id>` (rows not cleared on the save are
  skipped), `start <id>`, wait for the mission to load, then up to `-FightWaitSec` for a `BossBattleMode` start whose
  character placements (created in the preceding 60 s) include the row's expected placement id (the boss HP bar). Then quit
  and require the add-on to report the row exited. A passing row flips `verified=1` in `Nioh3BossRematch_verified.tsv`.
  A row that needs the player to walk to the boss ends as "loaded, fight not reached" and stays unverified unless `-AcceptStart`.
* **`tools/testmatrix/white_dot.py`.** Given `Nioh3BossRematch.log` and a row id, prints the newest compass destination the
  game set (the "white dot"), used as the walk target for lesson or gadget Proceed steps instead of a per-mission
  coordinate table.
* **`tools/testmatrix/training_gate.py`** (with `training_gate_live.ps1`). Release gate for Training session fixes. Five
  cases, each scored from the three mod logs written during the case: "Train here" does not native-load; the arena gate's
  "Fight again" works after a won match; "Despawn all" ends the session and the next menu open is not forced onto the
  Practice page; a won in-place session ends when the boss body is removed; "Train in a boss arena" goes to the arena as
  Practice. `training_gate.py source` fails if a log line a case scores on was renamed or removed in the source; `plan`
  prints the steps; `score DIR` scores captured logs; `selftest` runs fixtures. **A case with no log files is `NOT RUN`,
  never PASS.** Exit 0 = all PASS, 1 = a FAIL, 2 = not run or aborted.
* **`tools/testmatrix/vpad_live.ps1`.** Smoke test of VirtualPad with the game unfocused: title to field, toggle one menu
  row and back, push the stick forward and require the player position to change, close normally, restore.
* **`tools/testmatrix/padrec.py`.** Records a real pad session to JSONL (polling XInput from its own process) and replays it
  through the `pad` command. Off unless started; the mod has no recorder.
* **`tools/tour/run_tour.ps1`.** Runs the Tour mode (EngineFix `src/tour.cpp`, `[Debug] Tour*` keys), which visits mission
  maps for stability and performance sampling. No input is sent. Mode 1 filters to unlocked or cleared missions; `-Passes 2`
  adds a leak comparison. It backs up and restores saves, captures PresentMon and mod logs, and handles bounded crashes
  and stalls. `report_tour.ps1` summarises; `-DryRun` uses `fake_game.ps1`. It cannot replace manual combat, retry, menu
  or controller checks.
* **`tools/rematch/*pilot*.ps1`, `camera_witness*`, `run_c72e_*`.** One-launch guarded pilots for specific cover and
  Training bugs, each pinned to hashes of the build under test and requiring an explicit approval receipt. They belong to a
  private approval process and are not general tools; their `test_*.ps1` fixtures run offline. Not documented further here.

### 9.4 Performance A/B (brief)

Not pass/fail. `tools/testmatrix/abba2_pc.ps1` interleaves A and B arms (ABBA order) so launch-to-launch drift cancels.
`fastab.ps1` switches menu rows and shader sets **inside one launch** through the mod's `LiveCtl` control file
(`[Debug] LiveCtl = 1`), and `fastab_report.py` reports paired deltas with a 95 % t-interval and a verdict (B better, B
worse, no difference). `live_keys.py` classifies each ini key as live (switchable without a relaunch) or relaunch;
`fastab.ps1` refuses a key that is not live. `tools/testmatrix/README.md` documents the matrix and final-phase flow. These
need PresentMon and real hardware, and the numbers depend on the machine; publish only scrubbed summaries.

## 10. Adding a test

1. Put pure logic in a header under `src/` (`*_core.h`) when you can, so the test can include it.
2. Write `tests/<name>_test.c` or `.cpp` and, if it needs a build step, a `run_<name>.py` that compiles (with `-UNDEBUG` or
   `/Od /UNDEBUG`), runs, prints a last line `PASS` or `ALL PASSED`, and exits non-zero on failure. Name it so the
   regression suite's globs find it (EngineFix: `run_*.py` or `*_offline.py`; Training: `*_test.py` or `run_*.py`;
   BossRematch: any `tests/*.py`).
3. Print real results. A run that prints `FAILED` while exiting 0 is still treated as a failure by the suite.
4. For a bug fix, add the failing case first, show it failing on the base tree, then passing on yours.
5. Do not read game files in a test unless the path is an argument or an environment variable, and the test says clearly
   when it is absent and exits non-zero for "not run". No game bytes and no extracted text tables in fixtures
   ([PUBLIC_EXCLUSIONS.md](../../PUBLIC_EXCLUSIONS.md)).
6. If you add a pipe command or a menu row, remember `mod/settings/test_catalog.py` allows growth but not removal or
   reordering of what the last release shipped.

## Open problems / known gaps

* **No hosted CI.** The public repository has no active workflow. A portable lane could start with
  `py -I -B mod/Nioh3EngineFix/tools/gen_header.py --check` and the synthetic-input harnesses (`run_loadstats_harness.py`,
  `run_loadmem_harness.py`), which do not read a dump. Anything needing a real D3D12 device (`run_gp_harness.py`,
  `run_hot_harness.py`) or the dump cannot run on a hosted runner. Not validated on a hosted runner (unverified).
* **Hard-coded environment.** Many runners assume a portable MSVC tree at `tools/msvc/setup_x64.bat`, the default Steam
  install path (`NIOH3_DIR` overrides it in `arena_walk_test.py`), or a location on the original maintainer's disk.
  `regsuite.py` and several tests hold such constants. They need parameterising before outside use.
* **`mod/settings/test_catalog.py` depends on git history** and a pinned `RELEASE` commit; it cannot pass on a fresh shallow
  or re-rooted checkout.
* **Unwired tests.** About 18 Training `.c` tests and about 25 EngineFix `*_test.cpp` and `*_harness.cpp` files have no
  runner and are skipped by the regression suite (section 4).
* **Extract-by-marker fragility.** Tests that slice production source by marker string break on harmless renames, and no
  tool lists which tests depend on which marker.
* **The no-hardcode audit script and the build-comparison helper are not in the repository**, so "no new findings" and
  "nothing removed versus the previous build" cannot be reproduced from this tree alone.
* **Menu harness link gap.** Earlier regression notes report that `run_menu_harness.py` failed to link `ui_harness.cpp`
  for lack of stubs for `CloseBossMode` and `BossModeOpen`. Unverified whether it still fails on v1.8.39.
* **Coarse hold timing.** The input log measures key "held" durations with a coarse tick (about 15.6 ms steps), so a hold
  is accurate to roughly one frame at 60 fps. A QPC-based timer is deferred.
* **Dump-dependent tests can be skipped when the dump is absent.** Treat "ran without the dump" as "not run", and re-run
  with your own dump before a release.
* **Live coverage is thin by design.** The arena walk covers only Training arena rows; Fight now self-tests cover only rows
  already cleared on the test save; the tour samples points and does not test combat, death and retry, menus, controllers
  or game-speed behaviour. Those still need a person.
* **Performance A/B results do not transfer between machines.** The harness is a method, not a result.
* The tests assert the behaviour of v1.8.39 against Steam v2.02 of the game. A game update moves every RVA; the pattern and
  fixed-RVA checks in section 5 are what will tell you how much.
