# Stage-2 audit failure diagnosis and runner patch (25896)

Evidence: run-pc-20261023-103920/trace.txt and addon_launch1.log, audit-final-20261023/run-pc/out.log, screenshot s2-pc-0001-mid2.png, installed Training.log read-only, and local tools/PresentMon.exe --help (2.6.0; no capture started).

Verdicts:
- Capture failure is real: all8 runs have no PresentMon CSV. Previous runner discarded stdout/stderr and exit codes, so exact ETW error cannot be recovered. Non-elevated ETW access is the leading operational suspect, not a proven error code. Patch conservatively requires an elevated runner before game/lock/save changes; no unattended UAC auto-relaunch.
- RSXTraceSession is not the default PresentMon session name. Local binary help explicitly supports simultaneous distinct sessions. Radeon PresentMon presence alone does not prove a collision, and no Radeon process/session should be terminated. Patch uses unique NiohAudit-PID-GUID sessions and removes stop_existing_session.
- No-spawn theory is contradicted for run1: Training.log at10:41:00 am EDT starts R7B35;10:41:03 am EDT logs spawned actor, chr16102, and51712/51712 bossHP;10:41:05 am EDT releases AI. BossHP heartbeats continue through10:42:59 am EDT. Rematch addon status fights=0/fnow=idle does not reflect Training-owned swap-in state. The source layout-only change130FEF46 gives no evidence of a spawn regression. A live boss is not necessarily engaged combat; actor AI/attack activity still needs the owner to verify.
- Screenshot confirms Steam covers most of the game. Launch explicitly called VP-GiveBackFocus(before), which can return focus to Steam. Vpad still works in background, but this is an uncontrolled foreground/occlusion condition for an FPS comparison. Patch focuses only registered gamePID, verifies it before capture and around each combat pass; a focus loss invalidates/stops the batch. It never closes Steam.

Changes:
- PresentMon PID-scoped target, explicit v1 metrics for existing MsBetweenPresents parser, unique session, per-run stdout/stderr and arguments/session log.
- Immediate startup/early exit check; bounded wait; exit-code/file/schema/positive-frame validation before arithmetic. Infrastructure failure records invalid result and stops batch with nonzero exit instead of repeating8 costly failures. Cleanup only stops this run's registered capture children.
- Swap-in readiness uses only log bytes appended after the fight request, not the old last60 lines. Positive-HP heartbeat required at spawn, after warmup and within15s during sampling. Truncation fails closed. Training log archived per launch alongside add-on log.
- Foreground checks use cheap Win32 calls instead of spawning a dotnet process during sampling. Focus request is verified, not assumed successful.

Offline validation: test_audit_capture.ps1 PASS: stale/fresh/dead/ack-only/truncated heartbeat cases; capture timeout/nonzero/missing/empty/success cases; unique-session/PID/v1 arguments and no stop-existing/UAC; both production scripts parse cleanly. No game, ETW session or benchmark touched. No FPS claim or live capture proof.

Rerun: cherry-pick the commit, keep the same stage2 plan, start the runner from an elevated owner shell with -MaxRuns1 first. Inspect the new stderr/stdout and fresh Training evidence if invalid; only continue the remaining schedule after a valid first run. Keep RSX untouched. Existing invalid results are rerunnable because the runner skips only valid=1 rows.
