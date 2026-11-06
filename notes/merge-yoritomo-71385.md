# Yoritomo + Fight now integration (71385)

Merge Ally 8e8a33d6 into cx-pc/no-fight-fix-71350 62070822, based on final34912c12. training.c auto-merged without conflicts. Only checkpoints.md, next.md, owners.md conflicted; retained both histories. No version bump. BR source unchanged from9bd72fbe, use its built dd0e5e17 file5368; new TR c0a77053 file5374.

All17 run_move_toggle_available.py harnesses passed with -UNDEBUG (assertions enabled): idle_hold, ih_alive, drill, gate_zone, move_toggle, player_hp, player_hp_entry, player_recovery, hazard_guard, slot_hold, boss_vitals, aggro, practice_stats, negki_meter, auto_stagger, autobind, auto_moves. Inherited missing arena generator and3 extracted action fixtures remain skipped; auto_moves has0 real files. Log Temp/merge71385/harness17.log. Training build exit0, buildTR.log. Updated old native_entry_test stubs for the route observer introduced by Fight now; native entry PASS. Production ownership/native-return/dedupe harness PASS, including all5 route exits and BR mutation guards. No new gameplay changes beyond the requested merge.

Offline only: no install/game. Claude tests this BR/TR pair, including real cliff/death and five-row Fight now/exit criteria in notes/no-fight-fix-71350.md. Existing installed supplied build EF960584e7 BR e1b51409 TR f68a77d1 Cam7482c03d unchanged by this worker. Public camera stays7482c03d.
