# Practice Yoritomo self-kill candidate (#69414 / #69424)

2026-11-05 12:28 pm EST. Device cx-pc, offline only. Base origin/pc/repeat-last-move-fix 61d07b05 (TR e8ba9b52). Public source f60c56f9, TR 04781485.

## Evidence and diagnosis

Bridge #69414 reports two Repeat-OFF deaths on pc2. File #5140 is a 4 KB excerpt, not the full logs: both runs show native 0x3F1 -> 0x4E3B then HP 0; run 2 has HP 63402/63402 1.5 seconds beforehand. Full logs requested in #69442. The catalogue already marks chr16102 0x4E3B/4E3C unsafe (prior #14655); motion 30400 leads to 30401. The old guard refuses forced repeats only. Native select/commit neither consult that unsafe classification nor automatically install for Fight with Repeat OFF.

DEATHBLOW diag `ours 1` means the actor is in the spawned registry. It is a read-only diagnostic, not a kill request. `done 0` and absence of a preceding KILL/DEATHBLOW skip in the excerpts do not support blaming the finisher completion path. BossTick only invokes its HP-floor kill after HP <= 1. The exact native instruction that zeros HP and whether the reaction passes the action hook remain unproven; this is a narrowly scoped prevention candidate, not a confirmed live fix. Opus CLI read-only review independently identified the unguarded commit and lazy hook installation.

## Change

Move selection (including rerolls) returns native idle 2 for the known death pick. Commit vetoes it with native invalid action FFFF before writes. Conditions: registered spawned Yoritomo, matching AI controller, not player or explicit ForceKillActor target, current action exactly 3F1, readable HP > 1 and <= maximum, native death flag clear. No HP writes, resurrection, story changes or blanket death-reaction block. Other reactions, actual HP-floor/dead actors and cleanup pass unchanged. Hooks are installed for spawned Yoritomo even with Repeat OFF. SELF KILL evidence is logged as `SELFKILL guard` with actor, HP and rejected ID (first20 then every100). It must appear in the live retest to show this path was caught.

Repeat gap logging suppresses idle->idle and emits at most one actual attack veto per gap. Sequence and single repeat use the same limiter. Public 1.8.6 has the same native exposure; Repeat-last additions are not necessary for the bug. Public gameplay reproduction is not claimed.

## Validation

- mod/Nioh3Training/build.bat exit 0, two existing unused-symbol warnings.
- run_move_toggle_available.py exit 0: 14 harness executables; 5061 catalogue entries, 4998 toggle/veto cases; new actual select/commit regressions cover both IDs, full/partial HP, HP0/1, death flag, unreadable HP, player/foreign/unowned actors, non-idle reactions and explicit cleanup.
- 495 idle gap calls emit zero logs; repeated attack picks emit one per gap. Existing repeat, leap continuation, aggro, idle and vitals tests pass.
- Original full runner cannot load missing tools/gen_summon_rows.py (pre-existing). Available runner explicitly skips that generation check and raw action-data fixtures (zero extracted files). No game launched or installation changed.
- ASI SHA256: 2de742cd25ebe3c3b84f8c391f413e1129ed68bdbbd6dc5ac73a22b6a1bf7fbc.

## Lead retest

Install only this Training ASI over #253 setup. Plain Fight, Repeat OFF, infinite player HP ON: reproduce native 3F1 -> 4E3B request and require SELF KILL guard evidence with boss HP remaining live. Then damage/kill Yoritomo normally and confirm death/amrita still happen; repeat Last3 and confirm gap logs are bounded. If he dies without guard evidence, collect full logs and trace the direct native reaction/force-transition path; do not call this fixed. pc owns queueing this in-game test. Installed main-PC build unchanged: EF85ebffdc, TR0e30328a, BR4c219306, Cam7482c03d.
