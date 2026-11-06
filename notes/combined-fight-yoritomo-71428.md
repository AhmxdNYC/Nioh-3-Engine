# Combined Fight now / Yoritomo validation — 71428

Branch `cx-pc/no-fight-fix-71350`. Merge `1238b6ea` joins Fight now checkpoint `62070822` and Ally hazard guards `8e8a33d6`; base `34912c12`. No conflicts in training.c; only three handoff files required combined histories. No runtime changes were needed after this merge. Harness maintenance `cbdef651` adds the missing save-veto tick stand-in, recognizes the destination-owner guard preceding the admission guard, and allows N3_MSVC_SETUP in two older tests.

All 17 Yoritomo executable harnesses passed with `-UNDEBUG`: idle_hold, ih_alive, drill, gate_zone, move_toggle, player_hp, player_hp_entry, player_recovery, hazard_guard, slot_hold, boss_vitals, aggro, practice_stats, negki_meter, auto_stagger, autobind, auto_moves. Existing missing arena generator and three raw-action fixtures remain skipped (auto_moves: zero real files).

All six Fight now suites passed: native_entry_test, native_entry_return_test, test_native_attempt, test_fight_result, test_spawn_restore, test_request_read_gates. Coverage includes real-source entry/fallback, five route exits and cleanup, dedupe isolation, 96 transaction checks, four admission callback checks, four standalone-return checks, unreadable exit, 15 map ownership checks, nine fight outcomes/save-veto scenarios, 23 restoration checks, and 86 admission checks faulting 16 snapshot reads.

Evidence on main PC: D:/nioh3-wt/yoritomo-71428-17-tests.log (initial 17-harness pass); D:/nioh3-wt/assert-tests-71428.log (assert-enabled 17 pass followed by the now-fixed read-gate fixture failure); D:/nioh3-wt/71428-*.log (final six passing suites); D:/nioh3-wt/Nioh3BossRematch-71428-final.log and Nioh3Training-71428-final.log (fresh paired builds).

No game/install. The earlier pair BR #5368 / TR #5369 remains unchanged for its queued test. The new combined pair requires the same five-row live gate described in notes/no-fight-fix-71350.md plus actual cliff/recovery, OFF death and genuine boss-kill checks. Offline results do not establish live late-native uniqueness or cliff efficacy.

Final new pair: BR 28f309ace59ba143d6109784beec344cfbe50dd079af0ab47a78be21494fdbdd, bridge #5376; TR c0a770537b12e42c29de476f63e2d66823c9780104ca42a877e4cd7a2309d3d5, bridge #5377. Both build exit 0. Install together under normal ASI names. BR runtime source is unchanged from the earlier pair; its new build hash reflects the rebuild.
