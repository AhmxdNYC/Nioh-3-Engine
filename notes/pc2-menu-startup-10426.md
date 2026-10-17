# PC2 startup exit: offline diagnosis (#10426)

Verdict: no root cause proven. No game process, game lock, or installed files were
changed by this investigation. The captured launch failed before a window; it is
not evidence that the Training moves page crashes with no session.

## Evidence

- `captures/rematch/xworldprobe-20261017-011932/addon_launch0.log`: all site groups
  resolve, transition anchors match, RightsCheck call retarget succeeds, and arena
  tables merge. The final line is the Training table merge inside `Prepare()`.
- The next operations register Character::Create/BossBattleMode callbacks and
  install FIGHT hooks. `ui::Start()` is later, after `StartRematchPump()` returns.
  The move page and its chr=0 title branch are not called in this startup path.
  Missing later logs alone do not prove that the fault was on this worker thread.
- `training.log` contains only the probe-loaded line. Training's Worker first
  sleeps **45 seconds**, before installing hooks or loading move preferences.
  The probe-loaded line therefore does not implicate the move preference loader.
- EngineFix's game-folder log identifies **1.7.55**, and progresses through site
  resolution after the final Rematch line. Its earlier absence in Rematch's boot
  log means load order, not a missing dependency. Full compatibility remains
  unproven for this launch. No new EngineFix export is required by c8d2a3c.
- Harness stdout reports no window after 180 seconds, then restores six save files
  byte-for-byte and restores game-folder files. It does not record an exception
  address or process exit status. No matching Application Error event or local
  Nioh crash dump was found during this investigation. Queued kernel reports
  refer to older dump filenames and are not proof of this launch's cause.

## Offline checks

Current `pc2/move-toggle` source includes subsequent idle-pause work; the captured
Training binary is the original test artifact and differs from the newer local
Training binary. Do not silently substitute one when reproducing the failure.

- Added explicit zero-session tests: no image base, no active session, no boss;
  publishing identity installs zero hooks, and all 202 chr=0 action queries and
  setters reject the unknown boss. Catalogue has no chr=0 rows. Static inspection
  of c8d2a3c's moves page yields only Back and its start-session title for chr=0.
- Existing tests pass: 181 editable actions, 21 protected entries, boss ownership,
  selection/commit veto, INI persistence, deferred install and prologue mismatch.
- Compiled and ran the actual Rematch hooks offline using the existing synthetic
  executable-memory harness: **63 checks, 0 failures**, including chained entry
  hooks and pump retarget. This does not reproduce concurrent game startup.
  Zig emitted the already documented intrinsic declaration warning.

## Next step for the game owner

Capture process exit code and a crash dump/exception address on the exact original
pair before attributing the exit to the menu. If unavailable, compare the same
EngineFix/config with (1) original Rematch only and (2) original pair; retain all
logs. Startup breadcrumbs immediately before/after each FIGHT InstallSlot can
narrow the unresolved interval. Keep each case offline/infinite-health as usual.
This branch predates current gate/HP-0 fixes and is not the full player build.

Original captured Training SHA256:
`f0b3944147c5cee63e9901b86added3b084a1e5c0bc2cbc47d66b7cbf037860f`.
Rematch artifact read during investigation SHA256:
`c34bba8564a4aa52c7ad47d557d5f8737d8a1b10debe6724acca49a9c9976b22`.
Build paths/dependencies: `notes/boss-move-menu-10183.md` and bridge reply #10738.

## Follow-up: owner isolation artifacts

The owner's `captures/pc2-isolation-10454/stdout.txt` now records startup PASS
(window alive 15 seconds) for both Rematch OFF and Rematch ON on PC2 with EngineFix
and Training present. These are startup checks, not performance measurements.
`start.ps1` passes scratch-9702/arm3/Nioh3Training.asi to both arms; its SHA256 is
`1792e991435718649f396864c8d8f9dbacf77e91ab3c1047c3ab3b989c879fa0`, different
from the failing menu Training `f0b39441...`. Consequently these checks support
Rematch startup with the alternate Training build only; they do not reproduce or
clear the exact original pair. The 15-second window hold also does not guarantee
coverage of Training's delayed 45-second worker initialization.

Next owner check: use the captured original Training artifact with the same
Rematch, retain exit code/exception dump on failure, and keep successful startup
observation beyond 45 seconds from module load. No companion game access used.
The zero-session and full move/idle lifecycle offline suite was rerun and passed.

## Latest original-Training retry and diagnostic build (#10454)

The owner retry `captures/rematch/xworldprobe-20261017-013441` reached the
field (`title -> Continue -> field: True`), then failed its add-on pipe connection.
Training loaded at 01:35:18 local and installed hooks at 01:36:03, confirming its
45-second delay was passed. Rematch still ends at the arena merge, without ready
or pipe startup. This run does not reproduce the original pre-window exit and
does not complete the requested ON/OFF/ON move test. The harness restored six
save files byte-for-byte. No companion worker touched the game.

Added startup-only breadcrumbs before callback registration, before FIGHT group
installation, and around each entry-hook installer (including failure reason).
Built successfully with the existing intrinsic declaration warning. Artifact:
`mod/Nioh3BossRematch/bin/Nioh3BossRematch-startup-10454.asi`, SHA256
`0f63ef5d417f1f0b20d30667d982dabcf53f793038cc2836d02b5a7c57109d13`.
This is diagnostic only, not a crash fix. Owner next: use this Rematch with the
captured original Training and unchanged EngineFix/config, observe beyond 45 s,
retain the last Startup line and process exit code (dump if an actual crash).
Only after add-on readiness proceed with the move ON/OFF/ON checks.
