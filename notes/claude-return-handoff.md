# User-controlled return to Claude - 2026-10-11

Codex continuation remains enabled. **Do not call or wake Claude. Only the user resumes Claude after the ownership transfer below.** User phone instruction1786 requires keeping this file, STATUS top, the Codex ledger and progress report current at every checkpoint.

Latest automation correction2026-10-11: the installing lead's2milliondaily cap stopped ready work after the first worker. User reported the stall; monitor stopped cleanly with no active worker, and daily caps now default null (optional user settings). Actual usage remains tracked and the single-writer/game guards remain. Tooling regression13checksPASS; restart follows repository checks/commits, then lead-active release. Rendering-first priority is unchanged. Earlier cap-hold observations below are historical and superseded by this correction.

## Latest checkpoint - FO epoch witnesses complete; emitter ownership next
- Completed actual-code epoch/lifecycle audit on isolated codex-fo10; experimental **24494d5 committed/pushed** with Codex attribution. **12 new witnesses / 0 failures, zero compiler warnings**; prior eleven controls not rerun. Evidence and limits: notes/handoff-codex-fo98-epoch.md and branch tests/run_frameoverlap_98_epoch_audit.py. Production source unchanged; FO remains NO-GO.
- Deterministic witnesses: stamping precedes publication; duplicate/main suppression while queued; identical pointers across use epochs; FlushAll leaves a late TAIL record pending; actual Restore clears installed state with pending record despite the watchdog TAIL predicate. Reset/restored routing are explicitly modeled; no real object locks, retirement or GPU proof. Both hook slots use the verified 2F06AB4 compare/write/stamp fixture; 7C50B4 internals remain unverified.
- Main parent82b0235; tested1.7.61/FO0 and default visuals preserved. No local game at entry/checkpoint, no game lock acquired or game test started; installed ASI/INI/config/saves and prior restoration untouched. No shipping build, deployment, settings/hardware change or FPS claim. Phone inbox empty at entry/checkpoint. Controller read-only enabled/working monitor45016/worker42456; serial ownership until exit, no controller writes or Claude call/wake. Remote1767/1768/1781 pending at last known bridge1785, not freshly queried.
- Next ready unit: trace 12B0D0/12AF40 and 7C50B4 lock/update paths, then 0x98 consumer and PagePool retirement ownership. Establish admission, epoch linearization and resource lifetime before choosing a combined queue/space/closure repair. No count-only or second-flush-only safety patch. Discord result via existing notifier after main checkpoint push.

## Verified repository and build state

- Main audit checkpoint: `a68a2c4` pushed, documentation only. Tested engine remains **1.7.61**, integrated at the earlier step1 checkpoint; defaults FO0 / ExtraPerf0, CBN1 CBT1 CBS0 CBF0. ASI SHA256 `4734152ACAFDC13FA417C23EE60C8F6579B023D7F93FE73A763D862446A7AD00`. This handoff adds documentation after a68a2c4; use `git log -1` for its own documentation commit.
- Experimental branch/worktree: `codex-fo10` / `D:/nioh3-wt/codex-fo10`, **24494d5 pushed** (new epoch witnesses; production source remains3b89c04). Includes startup availability repair after audit406b19d. Existing fo11 code1a84a3a / ASI1.7.63-fo11 SHA256 `D315D0570B29700EC222E7CDB78C3EFE4303AC6BF0FD67BC8C98A705EA6F8E63`. Not merged, deployed anew, or released. Original fo-efxlag worktree untouched.
- Audit406b19d compiled actual FO code in isolation: **11 checks / 0 failures, zero compiler warnings**, reproducing hazards. This is negative failure evidence, **not release safety PASS**. Reproducer `mod/Nioh3EngineFix/tests/run_frameoverlap_98_audit.py` in the experimental worktree; evidence/limitations `notes/handoff-codex-fo98-audit.md`. Discord result sent via existing notifier.
- Earlier main:255 patterns /36 harnesses/full release PASS0errors3knownwarnings; combined PC3179frames PASS. Earlier fo11:20combinedtransitions +test14x3 /6299frames PASS; all restoration hashes match. Details: `notes/handoff-codex-step1.md`, `notes/handoff-codex-step2b.md`. The fo11 +11.74% +/-1.54SE result is an 8ms replay-delay **emulator** ABBA, not a native performance claim. Main PC A/B numbers and limits are in the progress report.

## Known failures and risks

FO is still NO-GO for shipping: queue overflow reintroduces off-main writes and reorders records; Flush98 can cross active logical stream ends; late TAIL emissions can remain queued after flush-then-barrier. Emitter availability readiness failure repaired3b89c04; remaining lifecycle hazards still block release. Volatile queue count has unlocked accesses. Stream drain/reset cannot execute while holding the producer queue lock; prove overflow, bounds, producer closure, epoch/GPU ordering and object lifetime together. Eleven negative controls intentionally assert these existing hazards; convert them when repairing the implementation.

Initial fo10 failed the twentieth return to stock and required termination after graceful close stalled. Fo11 subsequently passed the targeted transition run but retained one shutdown-only engine thread-join watchdog. Do not describe that log as wholly watchdog-free. No new game test occurred in this audit checkpoint, and no runtime use-after-free was proven by the late-record model.

## External gates and restoration

Last bounded bridge check cursor1785: no new remote results. Current steering user1786 acknowledged at final phone checkpoint; it provides no device result. Main pc2 smoke **1767** and Ally smoke **1768**, kit223, remain pending. Native pc2 FO5 **1781**, kit225, runs only after1767 PASS, then HOLD. Exact branch plan `notes/fo5-pc2-test.md`; no replay-delay emulator on native pc2. One Ally session only; do not restart Claude or its watcher. RC packaging waits for both main smokes; FO also needs safety repair and native gates. Rematch full validation and co-op/VRR gates need player participation where documented.

No game was running at entry or final audit checkpoint. No game lock acquired because no game was launched. Audit did not alter installed ASI, INI, saves, config, visuals, hardware or settings. Prior runtime tests' verified restoration is recorded in step1/step2b; it was not repeated here. Pre-existing untracked captures are left alone. Completed topics committed explicitly, no production source edits outstanding.

## Ownership and the user's pause sequence

Read-only controller status during this checkpoint: enabled=true, phase=working, monitor PID5504, active worker PID27908; `lead-active` absent. This serial worker owns the source until its final checkpoint/exit. That is a timestamped observation, not a promise that the PID will remain current. The worker does not change controller state or pause continuation on its own.

When the user actually wants Claude to resume:

1. Send **/stop** in the phone app's **Nioh 3 - Codex / PC Codex** chat. It pauses new workers while the active worker/test finishes safely. Do not force-kill a game or worker.
2. Use **/status**, or read-only `py -I "C:/Users/Ahmad Hamza/universal-optimizer/tools/codex-autowake/runner.py" status`. Wait until continuation is paused, phase is no longer working and worker_pid is null/exited. Confirm no Nioh3 process or owned test lock remains; inspect any interrupted-worker condition before taking ownership.
3. The user/manual lead creates `%LOCALAPPDATA%/nioh3-codex/lead-active` as the documented ownership hold, only after the worker has finished. Keep Codex paused while Claude edits. Do not remove its hold or enable either auto-wake during concurrent editing.
4. Only then the user resumes Claude and supplies this file, AGENT_BRIEF, newest STATUS and the ledger. User priorities supersede old AGENT_BRIEF no-launch/bridge/Claude-attribution wording. Keep unproven work isolated and use Codex attribution for Codex commits; Claude owns any subsequent changes it makes.

Controller instructions: `C:/Users/Ahmad Hamza/universal-optimizer/tools/codex-autowake/README.md`. No pause/hold/state change was executed by this checkpoint.

## Exact next independent unit

Use the latest checkpoint above: trace original emitter/consumer ownership before a combined repair; new epoch witnesses are complete24494d5. Lifecycle contract is complete; startup repair3b89c04 remains verified. User pause sequence above remains required before Claude resumes.
