# Codex continuation ledger

Owner: one serial Codex lead. User authorized all remaining work and Codex auto-continuation/phone chat on 2026-10-11. Claude must remain paused; its auto-wake stays disabled. Read AGENT_BRIEF.md, this ledger, and only the relevant newest STATUS entries. Original ordered brief: notes/codex-handoff-prompt.md; its old stop-after-each-step/no-Codex-auto-wake wording is superseded by the user's explicit continuation request.

## Latest checkpoint - FO98 retirement pool return traced
- Traced251B5C/251CB4 key removal and251B88 free-range insertion/coalescing under owner+CE8. Optional532C64 releases object+8 reference and calls vtable+18 at zero. Evidence: notes/handoff-codex-fo98-pool-return.md.
- Nine new static provenance checks PASS; record[0] comes from owner+38 map entry+8, but its vtable+10 CPU/GPU wait target remains unresolved. No runtime/lifetime/safety or performance verdict; FO remains NO-GO. Parent94b359f; experimental24494d5 and tested1.7.61/FO0/default visuals unchanged. Prior passed suites not repeated.
- No local game at entry/checkpoint; no game test/lock, Ghidra, build/deployment or installed ASI/INI/config/save/settings/hardware change. Prior restoration retained; controller untouched; no Claude call/wake. Phone inbox empty. Remote1767/1768/1781 pending at last known bridge1785; no fresh query.
- Next ready unit: trace insertion into owner+38 live map to resolve record[0] vtable+10 and CPU/GPU wait semantics; then producer closure/ring rebuild proof. Serial ownership until exit; commit/push with Codex attribution and Discord result via existing notifier after push. This checkpoint is documentation/static evidence only; use git log -1 for its commit.


## Latest checkpoint - FO98 retirement callback traced
- Traced descriptor/context2EEF6D4 ->2EEFEB0 -> callback2EEF8F4 -> consumer1A757B8. Callback replaces the worker flag argument; consumer calls record vtable+10, invokes pool helper under owner+CE8 exclusive, clears24bytes and releases capacity. Evidence: notes/handoff-codex-fo98-callback.md.
- Five new static provenance checks PASS; vtable GPU-wait, helper ownership, ring restart and producer closure remain unproven. No runtime/lifetime/safety or performance verdict; FO remains NO-GO. Parent65cdf23; experimental24494d5 and tested1.7.61/FO0/default visuals unchanged. Prior passed suites not repeated.
- No local game at entry/checkpoint; no game test/lock, Ghidra, build/deployment or installed ASI/INI/config/save/settings/hardware change. Prior restoration retained; controller untouched; no Claude call/wake. Phone inbox empty. Remote1767/1768/1781 pending at last known bridge1785; no fresh query.
- Next ready unit: trace251B5C pool-return behavior and resolve record[0] vtable+10 from enqueue provenance, then GPU/producer closure and ring rebuild ordering. Serial ownership until exit; commit/push with Codex attribution and Discord result via existing notifier after push.


## Latest checkpoint - FO98 event and worker protocol traced
- Traced queue event/worker protocol: queue+10 is a manual-reset completion event; thread handle is queue+0. Native worker1A71FDC calls descriptor callback with context and queue+18, then signals completion.1266FC/126A14 atomically set/clear a bit. Auto-reset wake probes can consume a signal. Evidence: notes/handoff-codex-fo98-event-worker.md.
- Eight new static provenance checks PASS; no runtime/lifetime/safety or performance verdict. FO remains NO-GO; prior passed suites not repeated. Parent c38358f; experimental24494d5 and tested1.7.61/FO0/default visuals unchanged.
- No local game at entry/checkpoint; no test/lock/build/deployment or installed ASI/INI/config/save/settings/hardware change. Prior restoration retained; controller untouched; no Claude call/wake. Phone1802 acknowledged, no new steering. Remote1767/1768/1781 pending at last known bridge1785; no fresh query.
- Next ready unit: follow2EEF6D4 descriptor/context into2EEFEB0 to identify retirement callback and queue+18 bit contract, then consumer/GPU ordering. ResList capacity,D20 transitions and emitter caller modes remain open. Serial ownership until exit; commit/push with Codex attribution and Discord result via existing notifier after push.


## Ready work, in priority order

**Continuation correction (2026-10-11):** user asked why work had stopped. The installing lead's unrequested daily cap caused it; that default is removed. Daily caps are now optional null values, with actual usage still recorded. Monitor stopped cleanly, no active worker, temporary lead-active protects this update; after tooling checks and commits the lead restarts and releases ownership. Next worker must perform the concrete rendering-first unit below, not merely rewrite status. Keep each unit narrow, checkpoint its actual evidence, and select the next ready unit. Phone1791 asks for a webhook update on the chosen work; the lead will post after successful restart. No new Claude API calls.

**Latest user-delegated decision (2026-10-11): rendering stability and release readiness first, to make Claude's return easy.** This supersedes the training-priority wording in earlier phone item1789. Training mode remains a short researched design/inventory, with no live spawner implementation yet. Finish the bounded FO availability/safety repairs and their proofs, then reconcile renderer/device gates and select a small justified renderer candidate. Keep tested main usable and notes/claude-return-handoff.md current; do not launch an expansive refactor.

1. **Repair experimental FO availability fail-closed**, worktree D:/nioh3-wt/codex-fo10. Completed audit: notes/handoff-codex-fo98-audit.md and branch tests/run_frameoverlap_98_audit.py (11 negative controls reproduce hazards; not safety PASS). Emitter-slot/range guard and focused startup proof completed3b89c04 (18casesPASS, pre-fix control catches bug); queue-count synchronization can follow if its publication contract is proven. Startup availability fix now implemented/pushed3b89c04; combined queue/lifecycle repair remains unimplemented. No new game launch merely to repeat passed tests. Relevant sources frameoverlap.cpp/.h; notes/renderer/frame-overlap.md sections8.6/8.7; notes/handoff-codex-step2b.md.
   - Historical attributed Claude API review: notes/claude-api-fo-review.md is incomplete and has unproven claims; the lead assessment separates those from evidence. Current user instruction forbids calling Claude or enabling its auto-wake. No further API assistance; never recover credentials from chat logs.
   - Fixed-size deferred0x98 queue full currently falls back to the original emitter on the replay thread: unsafe race returns. Fail-closed behavior needs proof.
   - Flush98 writes12bytes per command without a stream-space guard. Holding the queue lock while endFrame drains replay would deadlock; release/ordering must be proven. E.endFrame418B94 invokes418C70 drain/reset; raw dump RVA=raw offset, static decomp D:/nioh3-re/decomp/0x418c70.c. No Ghidra while Nioh3.exe runs.
   - Startup unavailable-emitter readiness hazard CLOSED3b89c04, actual-code tested; do not repeat that passed suite without changes. Do not hide a failure or discard required commands.
   - Audit reproduced overflow shared-stream mutation/reordering, stream boundary canary damage and late TAIL records left pending after flush-then-barrier. Also volatile queue-count unlocked access is a C++ data race. Repair overflow/bounds/producer closure together; do not block replay for main, drop commands, or drain while holding the queue lock. See the audit's precise model limits and lifecycle proof requirements.
   - Changes require meaningful harness proof, patterns255 + check_release --full before a shared build, then lock-owned runtime tests if justified. No merge/release of FO until native pc2 and safety gates pass.
   - Epoch/lifecycle audit now complete24494d5: 12 new witnesses, zero failures/compiler warnings. Actual FlushAll/Restore retain pending records; reset/routing modeled. Next independent unit is caller-mode/consumer/retirement ownership tracing; startup and prior audit suites remain passed and are not repeated.

2. **Reconcile remaining work against current evidence.** notes/remaining-work.md is dated Oct8 and has stale owners/results. Read relevant sections narrowly and mark built/kept/dropped/pending with evidence. Produce an actionable per-device list and update this ledger; do not redo completed old work.
3. **Small replay-thread wins** from original brief step3. Existing CBU/CBT and graphics PCM are already built; compute PSO memo at58C0D8/58C144 remains unbuilt. SRV cache needs mutation/lifetime proof. One byte-exact candidate at a time, offline proof then PC measurement and conditional pc2 ABBA. No FPS claim without measured evidence.
4. **Final validation/package:** reconcile matrix8.1, stock versus kept8.2 and per-device defaults, stability/soaks, README/user docs matching actual release. Game-side changes only; no driver/hardware/power/Proton environment tuning. Package RC only once remote main smokes pass.

## Waiting for external results

- Main1.7.61 smoke: pc2 queue1767 and Ally1768, kit223. Latest bounded bridge check through1785: only known user/Codex phone exchanges after1781, no new remote results. Ally watcher is stopped; do not invoke Claude to wake it. One Ally session only.
- Native pc2 FO5 queue1781, kit225, only AFTER1767PASS. Exact plan branch notes/fo5-pc2-test.md. No emulator delay on pc2. Capture transition stress, soak and four-pair A/Bs, faults/counters, config/save restoration; then HOLD.
- Rematch full validation, co-op/VRR tests: need player involvement where the documented plan requires it. Independent engine work continues while these wait. Never pretend these are complete.

## Completed; do not repeat without new evidence

- FO deferred-command offline audit completed2026-10-11: 11checks/0failures reproduce current hazards against actual experimental code; zero compiler warnings. NO-GO remains, no production fix/shared build/game test/performance claim. notes/handoff-codex-fo98-audit.md. Negative controls are not release safety gates; convert them when repairing the code.
- Main1.7.61 merges cbv-upload+extra-perf;255patterns,36harnesses,full release PASS0errors3knownwarnings. PC smoke combined3179framesPASS; constant-upload neutral+0.55%±0.46SE; opt-in Performance+3.14%±0.08SE. DefaultsFO0/ExtraPerf0, CBN1CBT1CBS0CBF0. ASI SHA4734152ACAFDC13FA417C23EE60C8F6579B023D7F93FE73A763D862446A7AD00. notes/handoff-codex-step1.md.
- FO initial1.7.62NO-GO preserved.1.7.63-fo11 fixes EfxLag async2->1 starvation with staged merge. PC20combinedtransitions+test14x3PASS6299frames. Emulator+11.74%±1.54SE is NOT a native-device gain. No gameplay watchdog, but one shutdown-only thread-join watchdog retained. Natural close;8saves+ASI/INI/config hashesmatch. SHA D315D0570B29700EC222E7CDB78C3EFE4303AC6BF0FD67BC8C98A705EA6F8E63. notes/handoff-codex-step2b.md.
- Main tour stderr/report helper fixed; recovered existing tour report; missing-log failure/error-policy and44tourharnesschecksPASS. notes/handoff-codex-tour-report.md.
- User progress document notes/codex-progress-report.md written/pushed2b778c7. Keep it current when milestones change.

## Automation and ownership

Controller tools: C:/Users/Ahmad Hamza/universal-optimizer/tools/codex-autowake. Runtime state/inbox: %LOCALAPPDATA%/nioh3-codex. Phone project **Nioh 3 — Codex** routes [TO-CODEX]; /status,/stop,/resume are local controls. Monitor polls bridge with no model turns, launches one fresh bounded worker only when ready, waits on external gates, backs off failures and records actual CLI usage. Daily caps are optional user-configured numbers; defaults null keep ready work moving. Never write controller state from a worker; only update this ledger/STATUS and return the schema checkpoint.

Before a human/manual lead edits while automation owns the repo: /stop, wait for current worker to finish, then create runtime lead-active. Current setup retains lead-active until the installing lead finishes its handoff. No parallel game launch, source writer, Claude call, or auto-wake toggle. Commit/push completed topics with Co-Authored-By: OpenAI Codex <noreply@openai.com>; Discord result cards use existing tools/notify/notify.py, never read/print/write its secret.

## Return handoff requirement (user1786)

Continue independently with Codex continuation enabled; do not call/wake Claude. At every checkpoint update STATUS top, this ledger, notes/codex-progress-report.md and notes/claude-return-handoff.md with commits, verified evidence, failures/risks, pending queues, restoration/ownership state and exact next steps. User transfers ownership by /stop, waiting for active worker/test closure, then creating the documented lead-active hold before personally resuming Claude. Worker never changes controller state. Main audit checkpoint a68a2c4; experimental406b19d pushed. Current read-only observation enabled/working worker27908/monitor5504, no lead-active; this serial worker retains ownership until exit. No game running.

## Latest checkpoint

2026-10-11, after first worker exit: audit406b19d and main handoffae77b38 pushed. Negative-controls11PASS reproduce unsafe overflow, stream overrun and late-producer hazards; no production repair or new game launch. Read-only controller confirmed enabled/ready with worker_pid=null. Reported first-worker usage:2,571,176input,2,466,304cached input,19,100output; total counted2,590,276. This exceeded the configured2,000,000daily total-token cap, so the next automatic worker waits for daily rollover. Cached input remains included in this cap; do not bypass or claim it is free. Manual lead took a temporary lead-active hold while recording the rendering-first decision, to be released after committing these docs. No concurrent source writer or game.

2026-10-11 02:56 New York: bounded FO audit complete; actual code negative controls reproduce overflow/stream-bounds/late-producer hazards (11checks/0failures, no compiler warnings). Static availability and queue-count synchronization failures recorded. No production fix, ASI/game/config changes, game launch, Claude call or controller-state write. Phone inbox empty at entry/checkpoints; no game running. Bridge through1785 has no new remote results. Main1.7.61/FO0 and native gates unchanged. Experimental audit406b19d committed/pushed with Codex attribution; Discord result sent through existing notifier. Next concrete independent unit: fail-closed startup availability repair + focused proof in codex-fo10, then queue/drain lifecycle repair. STATUS top and audit note carry this ready checkpoint.

Phone steering1786 acknowledged at final checkpoint; progress report and clean Claude-return handoff updated. Next work remains the independent FO availability repair; no user pause or Claude resumption requested/executed.
