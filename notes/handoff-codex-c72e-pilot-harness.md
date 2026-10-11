# C72E guarded pilot harness checkpoint

2026-10-11. Tools-only branch `codex/c72e-pilot-harness`, worktree
`D:/nioh3-wt/c72e-pilot-harness`, based on main `03122f3`. No live launch, bridge
call, installed write, ASI/source build, Ghidra, Claude or game input performed.
User recording has finished; preserve CURRENT progress, never a pre-recording save.

## Commands and boundaries

Read-only plan (default; no output directory/guard/lock/file mutations):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File D:\nioh3-wt\c72e-pilot-harness\tools\rematch\run_c72e_native_pilot.ps1 -DryRun
```

Root-only future one-arm pilot, after current proof jobs finish and guard headroom
is available; replace owner PID with the live coordinator process:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File D:\nioh3-wt\c72e-pilot-harness\tools\rematch\run_c72e_native_pilot.ps1 -Run -Arm B -OwnerPid <root-pid>
```

Optional original-shrine control: separate invocation with `-Arm A`, one launch
and its own NEW entry snapshot. Do not run both concurrently. The script never
relaunches, kills an unowned process or forcibly kills its owned game.

Both ASIs are pinned to the current installed binaries: EngineFix1.7.32 SHA256
`dd8b5739062e364dfc708bd1f0f89dfc2b97477d66ae1fbfdfb6f50b0d4f2891`;
BossRematch1.4.0 `02d763e76037365e8814820fc1ef9d3d713710435f63099d04d617e05966d4d1`.
The external kit is `D:/nioh3-re/rematch-pilots/C72E-native334140-20261011`.
B catalogue hash is `4025b610a6babf9e7d3b18f3b3af2aab69c1edc5f2a0baf4519d30ce71895c24`.
The installed catalogue/arena tables and both ASIs are never changed. Only the
two user INIs receive a reversible overlay. Warp/Spawn/Capture/SelfTest/Tour are
off; Catalog/Arenas use absolute external paths, with a fresh empty Arenas file.
Existing gate538668029 remains armed. The pipe queues `fight RC72E` exactly once,
then one native `exit` after observing the fight.

Existing session helpers provide INI merging and shared game locks; existing
harness_guard provides PID/start-time ownership, deadline and headroom. Both
`nioh3-pc` and shared PC/Ally `nioh3` locks are held. Guard's forced-kill function
is overridden locally to graceful close only. If the game remains alive or
restoration fails, the run retains its recovery manifest/guard file and does not
restore underneath a running game. Never start a second launch to recover it.

## Progress protection and evidence

After locks and before either INI changes, copy the CURRENT full save tree to a
new external directory and verify relative paths, lengths and SHA256. Reject
reparse roots/children. Hold read-only FileShare.Read handles on every existing
file: reads remain allowed, overwrite/delete/replace is denied. New files remain
possible, so after graceful exit release the handles, restore missing/changed
files, remove only individually verified added files, and verify the exact tree
twice. No recursive delete/move, Steam setting toggle or cloud approval gate.
Remote Steam Cloud state is not independently verified; local current-entry
preservation is concrete. Real-game loading compatibility with these sharing
locks is untested and a potential early trial failure, followed by cleanup.

Original Engine1.7.32 source commit `350911e` autocont.cpp/h verifies
`[General] AutoContinueLoad=1` closes post-load tip screens through their native
confirmation path. Installed binary contains that key in UTF16. It does NOT
automate initial title Continue. Root manually selects Continue once; this
checkpoint deliberately supplies no title input automation or engine upgrade.

`events.jsonl` records pipe status and screenshot times. `first-observed.json`
requires a seen load, C72E, requested retry334140 (A335814), player/loading/pause
proxy, target proximity within2m horizontal/3m vertical, and `fnow=armed` before
the gate request. Later BossBattleMode/script destination cannot substitute for
this first landing. The correct fight placement333550 is checked separately.
Screenshots capture the monitor containing the owned game before the request,
at the first observed player, and at BossBattleMode, without focusing or sending
input. Root must keep the game visible and inspect them; other windows may be
included. For exact first-controllable-frame evidence, record video beginning
before the fight request. The ASI lacks a control-enabled flag and frame-boundary
telemetry: every automated result retains
`instant_verdict=UNCONFIRMED_CONTROL_NOT_EXPOSED`. Existing F PASS/zero warps are
never accepted as instant entry proof.

## Verification and next chat

Completed: **20 fixture checks PASS**, A/B read-only plans PASS with requested
starts335814/334140, installed INI/catalogue hashes unchanged, no game running,
PowerShell parsing PASS and `git diff --check` clean. No live path executed.

Light fixtures cover fresh backup, actual Windows read/write/delete sharing,
added/missing/changed save restoration, path escape/tampered-backup rejection,
pre-load/paused/wrong-mission/wrong-position/post-gate telemetry, nonfinite data,
duplicate INI keys and PowerShell parsing. Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File D:\nioh3-wt\c72e-pilot-harness\tools\rematch\test_c72e_pilot.ps1
```

No native candidate runtime validation yet. Before a future live pilot the new
coordinator must review this tools-only checkpoint, finish unrelated full-fog
proof jobs, retain the same pinned ASIs and inspect current game/process state.
Death/retry acceptance and exact first-controllable visual acceptance remain
separate future work. The script-tuple spawn override alternative remains gated
by the known SpawnRestore ownership hazard; this harness performs no such writes.

## Recovery abort fix - 2026-10-11 successor

Reproduced the inherited Guard-Fail failure in an isolated child PowerShell:
cleanup threw, exit99 followed, but guard.json was deleted. The shared abort
also called Kill-Registered after cleanup, allowing a second close without a
subsequent restoration attempt. The pilot now overrides Guard-Fail locally:
cleanup runs once; only verified Finish-C72 completion permits record removal;
failed cleanup retains ownership metadata, result and held shared locks. No
second close/kill occurs on that path. Shared harness behavior is unchanged.

Verification: test_c72e_abort.ps1 imports the actual Guard-Fail and Finish-C72
functions without executing the launcher. Four child-process cases PASS:
graceful-close failure, a remaining foreign game, tampered-backup restoration
failure, and successful cleanup. Assertions cover exit99, one cleanup/close,
owner PID/start metadata, save bytes, no restoration while a game remains,
INI restoration order and lock release only after successful restoration.
The existing 20 save-sharing/restore/telemetry/parser fixtures also PASS.
Both A/B plans pass with pinned installed ASIs and catalog hashes. No live
launch, installed-file edit, hold change or controller input occurred.

Live gates still open: title Continue, save-lock load compatibility, continuous
first-controllable/combat evidence, and exact exit/restoration. The automated
pilot still cannot establish actual combat or first control from status alone;
its immediate exit after BossBattleMode is not playable-training acceptance.

## Optional candidate binary transaction - 2026-10-11

The runner now accepts optional `-EngineCandidate <external-ASI>` with
`-EngineCandidateSha256 <64-hex>` and/or `-RematchCandidate <external-ASI>` with
`-RematchCandidateSha256 <64-hex>`. Both parameters of each pair are required.
Default original binary pins remain unchanged; originals must match those pins
even when a candidate is selected. DryRun validates candidates and displays
original/runtime hashes without creating output, acquiring leases or staging.
The selected engine still must contain AutoContinueLoad; that string check is
not a proof of candidate behavior. Title input and one-launch policy are unchanged.

After fresh entry-save protection, both original ASIs are copied and hashed in
`entry-binaries` before either candidate can be staged. `recovery.json` contains
owner PID/start, exact original/candidate hashes, destinations and backups.
Each stage intent is persisted before replacement. Staging copies to a unique
same-directory temporary file, validates it, rechecks destination ownership,
then uses atomic NTFS File.Replace; there is no overwrite fallback. A partial
two-file stage is recoverable because the record exists before the first write.

Finish-C72 closes the owned game and rejects any remaining Nioh3 process before
restoring saves, INIs and binaries. Binary restoration accepts only original
bytes or the candidate hash of an entry with recorded stage intent. Missing,
foreign or tampered bytes/backup cause failure and retain the guard, PC lease,
binary recovery record and backups. Successful restoration also retains the
binary record as a receipt, with complete=true. No automatic recovery launcher
or cancellation assumption has been added.

The lease is now **nioh3-pc only**, per the user's explicit multi-device testing
authorization. Other device coordination and cloud settings are untouched.
Hash checks plus the cooperative PC lease do not provide an OS compare-and-swap
against an unrelated concurrent writer between the check and File.Replace.
Full power-loss durability is not proven. Live Steam delayed-launch/cancellation
uncertainty remains a separate blocker; no live test was performed here.

Verification commands (run from this worktree):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/rematch/test_c72e_binaries.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/rematch/test_c72e_abort.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/rematch/test_c72e_pilot.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/rematch/run_c72e_native_pilot.ps1 -DryRun
```

Results: seven new actual transaction/Finish-C72 fixtures PASS (success, partial
two-file stage, restore failure, foreign binary, close failure, preflight hash
rejection, changed candidate after preflight); four existing isolated abort
cases PASS; twenty existing save/telemetry/parser checks PASS; original-pin B
DryRun PASS. All mutations were in new temporary fixture directories.

Candidate B DryRun also PASS against root-reviewed private build9eeb174 using:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/rematch/run_c72e_native_pilot.ps1 -DryRun -EngineCandidate 'D:\nioh3-wt\codex-settings-menu-v4\mod\Nioh3EngineFix\bin\Nioh3EngineFix.asi' -EngineCandidateSha256 '6890c0605e3346d915f7f2263337716ea1ddad6c0b4ea36efebb2c4d4aa8e32d' -RematchCandidate 'D:\nioh3-wt\codex-settings-menu-v4\mod\Nioh3BossRematch\bin\Nioh3BossRematch.asi' -RematchCandidateSha256 '50484e3b0e39c5621fb7b829122f92380db017fd87d64ed0e2f96a1acb3b9b07'
```

## Native intro observation and explicit control review

The retained191415-5360 trace actually has a pre-intro player/loading0 at
19:16:49.131, retry334140, fnowarmed, position207629,6025,142678. The old helper
discarded it because current native-attempt gate text was not in its legacy
allowlist. Later BossBattleMode333550 occurred during loading1; the old loop
broke immediately and closed before the intro ended.

The corrected observer retains native-start, earliest player proxy and boss
evidence independently. Battle during loading never completes observation;
current loading0/player must be seen again before root review. Before-gate and
target-proximity facts remain measured, without requiring them to open review.
Pending gate masks pause/session read-validity; this is not a control flag.

Root now has bounded90seconds (ReviewSeconds15..180) to provide explicit visual
arena/boss/control-response evidence. After the review request appears, use:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/rematch/publish_c72e_control_review.ps1 -OutDir <run-dir> -OwnerPid <root-pid> -VisibleArena -VisibleBoss -ControlResponse -Evidence '<actual visual and control-response evidence>'
```

Receipt nonce and owner/runner/game PID+start identities must match. No receipt
means no native return command and no control confirmation. No timing proof is
inferred: instant_verdict remains UNCONFIRMED_CONTROL_NOT_EXPOSED. The old
LANDING_PROXY_AND_BOSS_OBSERVED status additionally retains its before_gate and
near_target requirements; a later first proxy with root-confirmed control uses
ROOT_CONFIRMED_NATIVE_ENTRY_FIRST_CONTROL_TIME_UNKNOWN instead.

Before the one return, require4seconds continuous same mission/retry player0-load
telemetry (bounded15seconds). This is explicitly a return observation proxy:
the present pipe does not expose AttemptReturnGate or ExitGate independently;
the native wrapper remains authoritative. No retries or manual control input
were added by this change.

Verification: test_c72e_intro_observation.ps1226 ordered checks PASS, including
the actual retained trace (no post-intro evidence exists) followed by labeled
synthetic continuations, root receipt mutation rejects and return-proxy resets.
Existing20 save/telemetry/parser,43 admission and18 identity/finalization checks
PASS. No new live test or installed change was performed by the author.
