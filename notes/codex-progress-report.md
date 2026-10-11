# Codex work on Nioh 3

Updated 2026-10-11. This report separates completed work, measured results, and work still pending.

## Latest checkpoint - FO epoch witnesses complete; emitter ownership next
- Completed actual-code epoch/lifecycle audit on isolated codex-fo10; experimental **24494d5 committed/pushed** with Codex attribution. **12 new witnesses / 0 failures, zero compiler warnings**; prior eleven controls not rerun. Evidence and limits: notes/handoff-codex-fo98-epoch.md and branch tests/run_frameoverlap_98_epoch_audit.py. Production source unchanged; FO remains NO-GO.
- Deterministic witnesses: stamping precedes publication; duplicate/main suppression while queued; identical pointers across use epochs; FlushAll leaves a late TAIL record pending; actual Restore clears installed state with pending record despite the watchdog TAIL predicate. Reset/restored routing are explicitly modeled; no real object locks, retirement or GPU proof. Both hook slots use the verified 2F06AB4 compare/write/stamp fixture; 7C50B4 internals remain unverified.
- Main parent82b0235; tested1.7.61/FO0 and default visuals preserved. No local game at entry/checkpoint, no game lock acquired or game test started; installed ASI/INI/config/saves and prior restoration untouched. No shipping build, deployment, settings/hardware change or FPS claim. Phone inbox empty at entry/checkpoint. Controller read-only enabled/working monitor45016/worker42456; serial ownership until exit, no controller writes or Claude call/wake. Remote1767/1768/1781 pending at last known bridge1785, not freshly queried.
- Next ready unit: trace 12B0D0/12AF40 and 7C50B4 lock/update paths, then 0x98 consumer and PagePool retirement ownership. Establish admission, epoch linearization and resource lifetime before choosing a combined queue/space/closure repair. No count-only or second-flush-only safety patch. Discord result via existing notifier after main checkpoint push.

## Completed integration: main build 1.7.61

Merged Claude's existing `cbv-upload` constant-upload fix and `extra-perf` presets into main. These features were already developed before the takeover; Codex integrated, validated, built, and smoke-tested them. Fixed instruction-length handling in FnProbe and stale harness dependencies exposed by the full checks.

Defaults remain CBN=1, CBT=1, CBS=0, CBF=0, Extra Performance disabled, and FrameOverlap=0. Quality-changing presets are opt-in.

Validation passed: all 255 pattern sites, the constant-upload recorded-trace and Extra Performance harnesses, and the full release check including all 36 harnesses. The release check reported zero errors and three known warnings.

PC smoke results, three ABBA pairs:

| Comparison | Average FPS | Measured change |
|---|---:|---:|
| Constant-upload CBMODE 0 → 6 | 101.9 → 102.5 | +0.55% ±0.46% SE; neutral |
| Performance preset XPRE 0 → 1 | 102.4 → 105.6 | +3.14% ±0.08% SE |

The combined settings ran 3,179 frames without a test failure. Sampled screenshots stayed within the A/A animation variation. The game closed and saves, configuration, INI, and ASI were restored and verified.

The earlier **pc2 +15.9%** constant-upload result belongs to the inherited branch, not a new Codex measurement. That machine's slow chipset PCIe link makes it an unusual test case; it does not establish the same gain on a normal x16 connection.

Uploaded smoke kit **bridge file 223** and queued pc2 **1767** and Ally **1768**. Both device results remain pending at the last checked bridge cursor, **1785**. Release-candidate packaging is held until both pass. Discord result card posted.

Evidence: `notes/handoff-codex-step1.md`, `captures/step1-release-final.log`, and `captures/pc/fastab/20261010-2308-rc1761/report.md`.

## Experimental frame-overlap work

Used isolated worktree `D:/nioh3-wt/codex-fo10`, branch `codex-fo10`; main stays on the tested 1.7.61 build with frame overlap disabled.

The initial **1.7.62-fo10** attempt was a **NO-GO**. Its unsafe control reproduced the replay-thread fault; the deferred arm later hung on the twentieth return to stock rendering. Offline checks alone did not establish runtime safety. Captured and reported the failure; no performance claim was made. One hung test required termination after graceful shutdown failed; restoration was verified.

Investigated the transition hang and built **1.7.63-fo11**. The corrected effect-release transition merges the older list into the current staged list, preventing repeated async depth changes from starving copy-context releases. Added model and hook-harness coverage for repeated toggles, GPU ordering, exactly-once release, an empty staged list, and flush behavior.

PC follow-up passed twenty combined transitions and three additional transition-test cycles: 47 acknowledgements, 44 advancing dwell windows, and 6,299 frames. The sampled deferred-command counters ended with no pending commands or queue-full events. No replay fault, hang, or watchdog occurred during gameplay.

Measured with an **8 ms game-side replay-delay emulator**, three ABBA pairs:

| Comparison | Average FPS | Measured change |
|---|---:|---:|
| Frame overlap off → on; effect lag held on | 60.919 → 68.068 | +11.74% ±1.54% SE |

This is an emulator result, **not a native PC or pc2 gain**. Screenshot samples showed no obvious corruption and stayed within sampled A/A variation; this does not prove every scene is identical.

The game exited naturally and restoration hashes matched. A shutdown-only watchdog appeared after the tour finished, in the engine's shutdown thread join; this diagnostic is retained rather than presented as a completely clean shutdown log.

Uploaded candidate kit **bridge file 225** and queued native pc2 test **1781**, conditional on smoke **1767** passing first. This branch is **not merged or released**. Before shipping it needs native pc2 validation and further audits of queue-full behavior, command-stream bounds, and unavailable-hook handling. The bounded audit is complete: experimental406b19d reproduces overflow, stream-boundary damage and late-TAIL queue retention in 11 negative controls / 0 failures, with zero compiler warnings. Startup unavailable-hook readiness is repaired3b89c04; queue-count synchronization remains. This is failure evidence, not release safety PASS; only the bounded startup availability repair has been completed. See notes/handoff-codex-fo98-audit.md.

Evidence: `notes/handoff-codex-step2.md`, `notes/handoff-codex-step2b.md`, branch `notes/fo5-pc2-test.md`, and `captures/pc/fastab/20261011-0019-fo11fix/report.md`.

## Tour reporting fix

Fixed a reporting failure caused by PowerShell 5 treating Python's normal stderr clock-alignment diagnostic as a terminating error. Added `tools/tour/report_tour.ps1` and connected it to the tour runner. It captures output, restores the caller's error policy, and still rejects an actual nonzero Python exit.

Recovered the existing fo11 tour report offline. Verified both the successful path and a missing-log failure; the caller's Stop policy remained intact. The tour harness passed all 44 checks. This changed tooling, with no game launch, new ASI, or performance claim.

Tour tooling fix committed and pushed as `e416b61`. Evidence: `notes/handoff-codex-tour-report.md` and `captures/tour-report-check.log`. Discord result card posted.

## Current work and remaining gates

The user has now authorized taking over all remaining tasks and building Codex continuation and phone-chat support. That supersedes the earlier stop-after-each-step instruction for future work. Claude Code has not been called, and its auto-wake remains disabled.

- **Built and tested:** a separate Codex continuation controller and **Nioh 3 — Codex / PC Codex** entry in the existing phone app, in `universal-optimizer`. Ten controller checks passed, along with repository checks, CLI authentication smoke, live phone-message routing and transcript redaction. The scheduled monitor is enabled; the installing lead held source ownership through its setup handoff. The current serial worker owns its bounded unit until checkpoint/exit. It uses fresh serial workers, a small task ledger, idle waiting, failure cooldowns, and optional user-configured daily caps. The original unrequested daily cap stalled continuation; removed after the user reported it. Usage remains tracked. No measured token-saving percentage is claimed.
- **Historical API assistance (no further calls permitted):** authenticated the user-provided Claude API credential in memory and built a bounded read-only API reviewer. An incomplete FO review is saved and explicitly attributed in `notes/claude-api-fo-review.md`, with the lead's corrections. Setup hit hidden-reasoning and Windows pipe-encoding issues; both are documented and corrected. No key is retained, no Claude Code call occurred, and no engine fix follows from the review alone.
- **Pending device results:** pc2 and Ally main-build smokes, followed by conditional native pc2 frame-overlap testing.
- **Next independent engine work:** fail-closed FO emitter availability repair with focused startup proof, then overflow/bounded-drain/late-producer closure repair; investigate small replay-thread wins with byte-exact proofs and measured A/B tests. Existing graphics PSO memoization is not a new candidate; compute PSO memoization is still unbuilt, and descriptor caching needs lifetime/mutation proof.
- **Later release work:** reconcile the remaining-work ledger, device matrix, final stock-versus-kept benchmarks, longer stability runs, profiles, documentation, and release packaging. Rematch full validation and multiplayer checks have external/player requirements.

No new hardware, driver, power, or operating-system performance settings were changed. No claim is made that the project is finished.

## Latest checkpoint and return ownership

Main audit documentation checkpoint a68a2c4 and experimental audit406b19d pushed with Codex attribution; Discord result posted. Main remains the tested1.7.61 engine. No new game launch, production repair, deployment, settings change, performance claim or controller-state write. No game running and prior runtime restoration remains documented/verified. Remote queues1767/1768/conditional1781 remain pending through bridge1785.

User phone steering1786: continue independent work and keep the return to Claude clean. Codex continuation stays enabled; do not call/wake Claude. Current worker holds source ownership until it exits. Before Claude resumes, the user sends /stop, waits for the active worker to finish safely and confirms paused/no active worker/no game, then creates the documented lead-active ownership hold. Only the user resumes Claude. Exact commits, evidence, risks, queues, restoration and pause/next-step instructions: notes/claude-return-handoff.md.
