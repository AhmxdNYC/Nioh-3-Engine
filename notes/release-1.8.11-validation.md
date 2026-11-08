# 1.8.11 offline validation

85/103 runners pass; 18 baseline failures/unavailable checks. Testhub 5/5 PASS.

Baseline is 87dcdc89. Lead #83369 accepts matching pre-existing failures.

New soul-core/camera suites and rematch 357/357 pass; packaged updater fake-release test and full-install integrity pass.

Runtime source differs from fbec06ad only in MOD_VERSION. Item29 fixture and site/version.json also updated.

Remaining checks:
- formhide4_offline.py: Fixture extracts later unrelated code without Windows/polling stubs; same ULONGLONG/RdB compile errors on 87dcdc89 (tested via Zig fallback).
- test_sites.py: Ten ambiguous diagnostic probe-anchor resolutions, identical on 87dcdc89.
- empty_spawn_replay.py: 18 synthetic checks pass; recorded Temp/h72299/BossRematch.log missing on both releases (exit 2).
- entry_cover_test.py: Missing g_entry_scroll fixture global on both releases.
- native_proceed_test.py: Missing ScrollGateLive fixture stub on both releases.
- run_move_toggle_available.py: Missing HOOK_SET fixture stub on both releases.
- run_move_toggle_tests.py: Missing tools/gen_summon_rows.py dependency on both releases.
- test_catalog.py: Generated settings catalog stale on both releases.
- run_fnprobe_harness.py: Native thunk and decoder checks pass; dump buffer bounds fail on both releases.
- run_gp_harness.py: Missing osd::GameFrames linker stub on both releases.
- run_hot_harness.py: Missing osd::GameFrames linker stub on both releases.
- run_pj_harness.py: Missing skipfx::IniWants linker stub on both releases.
- run_psomemo_harness.py: Native checks pass; outdated PCM ini documentation assertion fails on both releases.
- run_r2_harness.py: Missing osd::GameFrames linker stub on both releases.
- run_settings_preservation.py: Outdated MoveFile source assertion fails on both releases.
- run_skipfx_harness.py: Seven obsolete XHALF-on expectations fail on both releases after 1.8.10 intentional default-off fix.
- run_thrsched_harness.py: Not run: harness refuses while game is running on both releases; companion does not control game.
- run_timerres_harness.py: Native checks pass; old SkipDebugString ini documentation assertion fails on both releases.

W1B wrapper limit was 90 seconds; direct built-native rerun exit 0. No game touched. PC, pc2 and Ally live gates are pending.
