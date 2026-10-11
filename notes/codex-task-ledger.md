# Codex continuation ledger

Owner: one serial Codex lead. User authorized all remaining work and Codex auto-continuation/phone chat on 2026-10-11. Claude must remain paused; its auto-wake stays disabled. Read AGENT_BRIEF.md, this ledger, and only the relevant newest STATUS entries. Original ordered brief: notes/codex-handoff-prompt.md; its old stop-after-each-step/no-Codex-auto-wake wording is superseded by the user's explicit continuation request.

## Ready work, in priority order

1. **Audit experimental FO deferred-command safety**, worktree D:/nioh3-wt/codex-fo10, branch codex-fo10 ddabf46. Start with static code and existing harness, no new game launch merely to repeat passed tests. Relevant sources frameoverlap.cpp/.h; notes/renderer/frame-overlap.md section8.6; notes/handoff-codex-step2b.md. No fix from this audit has been implemented yet.
   - Additional attributed Claude API review: notes/claude-api-fo-review.md. It is incomplete and has unproven claims; read the lead assessment before using its suggestions. User authorized API assistance, but no credential is retained. Only use a privately supplied process credential; never recover keys from chat logs. Continue without API assistance if none is available; never call Claude Code.
   - Fixed-size deferred0x98 queue full currently falls back to the original emitter on the replay thread: unsafe race returns. Fail-closed behavior needs proof.
   - Flush98 writes12bytes per command without a stream-space guard. Holding the queue lock while endFrame drains replay would deadlock; release/ordering must be proven. E.endFrame418B94 invokes418C70 drain/reset; raw dump RVA=raw offset, static decomp D:/nioh3-re/decomp/0x418c70.c. No Ghidra while Nioh3.exe runs.
   - Start logs deferred emitter unavailable but can still mark ready: audit fail-closed hook/byte availability. Do not hide a failure or discard required commands.
   - Changes require meaningful harness proof, patterns255 + check_release --full before a shared build, then lock-owned runtime tests if justified. No merge/release of FO until native pc2 and safety gates pass.
2. **Reconcile remaining work against current evidence.** notes/remaining-work.md is dated Oct8 and has stale owners/results. Read relevant sections narrowly and mark built/kept/dropped/pending with evidence. Produce an actionable per-device list and update this ledger; do not redo completed old work.
3. **Small replay-thread wins** from original brief step3. Existing CBU/CBT and graphics PCM are already built; compute PSO memo at58C0D8/58C144 remains unbuilt. SRV cache needs mutation/lifetime proof. One byte-exact candidate at a time, offline proof then PC measurement and conditional pc2 ABBA. No FPS claim without measured evidence.
4. **Final validation/package:** reconcile matrix8.1, stock versus kept8.2 and per-device defaults, stability/soaks, README/user docs matching actual release. Game-side changes only; no driver/hardware/power/Proton environment tuning. Package RC only once remote main smokes pass.

## Waiting for external results

- Main1.7.61 smoke: pc2 queue1767 and Ally1768, kit223. Latest bridge check through1781: no new messages. Ally watcher is stopped; do not invoke Claude to wake it. One Ally session only.
- Native pc2 FO5 queue1781, kit225, only AFTER1767PASS. Exact plan branch notes/fo5-pc2-test.md. No emulator delay on pc2. Capture transition stress, soak and four-pair A/Bs, faults/counters, config/save restoration; then HOLD.
- Rematch full validation, co-op/VRR tests: need player involvement where the documented plan requires it. Independent engine work continues while these wait. Never pretend these are complete.

## Completed; do not repeat without new evidence

- Main1.7.61 merges cbv-upload+extra-perf;255patterns,36harnesses,full release PASS0errors3knownwarnings. PC smoke combined3179framesPASS; constant-upload neutral+0.55%±0.46SE; opt-in Performance+3.14%±0.08SE. DefaultsFO0/ExtraPerf0, CBN1CBT1CBS0CBF0. ASI SHA4734152ACAFDC13FA417C23EE60C8F6579B023D7F93FE73A763D862446A7AD00. notes/handoff-codex-step1.md.
- FO initial1.7.62NO-GO preserved.1.7.63-fo11 fixes EfxLag async2->1 starvation with staged merge. PC20combinedtransitions+test14x3PASS6299frames. Emulator+11.74%±1.54SE is NOT a native-device gain. No gameplay watchdog, but one shutdown-only thread-join watchdog retained. Natural close;8saves+ASI/INI/config hashesmatch. SHA D315D0570B29700EC222E7CDB78C3EFE4303AC6BF0FD67BC8C98A705EA6F8E63. notes/handoff-codex-step2b.md.
- Main tour stderr/report helper fixed; recovered existing tour report; missing-log failure/error-policy and44tourharnesschecksPASS. notes/handoff-codex-tour-report.md.
- User progress document notes/codex-progress-report.md written/pushed2b778c7. Keep it current when milestones change.

## Automation and ownership

Controller tools: C:/Users/Ahmad Hamza/universal-optimizer/tools/codex-autowake. Runtime state/inbox: %LOCALAPPDATA%/nioh3-codex. Phone project **Nioh 3 — Codex** routes [TO-CODEX]; /status,/stop,/resume are local controls. Monitor polls bridge with no model turns, launches one fresh bounded worker only when ready, waits on external gates, backs off failures, records actual CLI usage and enforces between-worker daily caps. Never write controller state from a worker; only update this ledger/STATUS and return the schema checkpoint.

Before a human/manual lead edits while automation owns the repo: /stop, wait for current worker to finish, then create runtime lead-active. Current setup retains lead-active until the installing lead finishes its handoff. No parallel game launch, source writer, Claude call, or auto-wake toggle. Commit/push completed topics with Co-Authored-By: OpenAI Codex <noreply@openai.com>; Discord result cards use existing tools/notify/notify.py, never read/print/write its secret.

## Latest checkpoint

2026-10-11: controller10checksPASS; repository tools/bridge/nexus checks, header sync, ABBA and Windows game-guard selftestsPASS. CLI read-only auth smokePASS. Live phone route1784 [TO-CODEX] /status answered1785; explicit current Codex transcript mirrored, API-key redaction scanPASS. Scheduled task installed, continuation enabled, installing lead still holds source ownership until final release. Main-build remote smokes still pending. No new FO audit fix or performance prototype has been completed. First worker should finish the static FO safety audit and produce a concrete proof/fix or a well-supported blocked finding, then select the next independent ready task.
