# Codex work on Nioh 3

Updated 2026-10-11. This report separates completed work, measured results, and work still pending.

## Latest checkpoint - FO98 pre-publication resource cleanup traced
- Traced pre-publication sorted resource membership, cache invalidation, typed-ring cleanup and three-list record cleanup before retirement publication. Evidence: notes/handoff-codex-fo98-prepublication-cleanup.md.
- Eight new static provenance checks PASS. Native 127B4C supplies the membership argument omitted by decomp; cleanup virtual calls and actual GPU completion remain unresolved. No runtime/lifetime/safety/performance verdict; FO remains NO-GO. Parent d66de6d; experimental 24494d5 and tested 1.7.61/FO0/default visuals unchanged. Prior passed suites not repeated.
- No local game/test at entry/checkpoint; no lock, Ghidra, build/deployment or installed ASI/INI/config/save/settings/hardware change. Prior restoration retained; controller untouched; no Claude call/wake. Phone inbox empty. Remote1767/1768/1781 pending at last known bridge1785; no fresh query.
- Next ready unit: resolve record[1] vtable+B8 and cache-object vtable+28 through construction/assignment provenance; establish submission/completion boundary, then producer closure and ring restart. Serial ownership until exit; commit/push with Codex attribution and Discord result after push. Static evidence only; use git log -1 for checkpoint commit.

## Latest checkpoint - FO98 retirement work publication traced
- Traced 25208C pending+40 enqueue and 1267A8 active+48/work-token publication via 250650 under owner+D00; scheduler 2EF0938 reaches this path. Evidence: notes/handoff-codex-fo98-work-publication.md.
- Six new static provenance checks PASS. Native 1278D8 carries a fourth context argument omitted by decomp; record[1] vtable+B8 and pre-publication helpers identify the next GPU-ordering trace. No runtime/lifetime/safety/performance verdict; FO remains NO-GO. Parent 1a65b12; experimental 24494d5 and tested 1.7.61/FO0/default visuals unchanged. Prior passed suites not repeated.
- No local game at entry/checkpoint; no test/lock, Ghidra, build/deployment or installed ASI/INI/config/save/settings/hardware change. Prior restoration retained; controller untouched; no Claude call/wake. Phone inbox empty. Remote1767/1768/1781 pending at last known bridge1785; no fresh query.
- Next ready unit: trace 12796C/127CDC/127B4C/127A80 and record[1] vtable+B8 into submission/fence ordering, then producer closure and ring restart. Serial ownership until exit; commit/push with Codex attribution and Discord result through existing notifier after push. Static evidence only; use git log -1 for checkpoint commit.

## Latest checkpoint - FO98 retirement ring rebuild traced
- Traced2EEF3B0 capacity growth and2EF01F4 FIFO ring rebuild, semaphore replacement and counter reset;2EEF950 frees the old buffer. Evidence: notes/handoff-codex-fo98-ring-rebuild.md.
- Four new static provenance checks PASS. CPU worker completion is not a GPU fence; GPU timing remains unresolved. Semaphore-creation failure branches restore saved handles; initial closed-handle suspicion corrected after following the common failure jump. No runtime/lifetime/safety/performance verdict; FO remains NO-GO. Parent3d27386; experimental24494d5 and tested1.7.61/FO0/default visuals unchanged. Prior passed suites not repeated.
- No local game at entry/checkpoint; no test/lock, Ghidra, build/deployment or installed ASI/INI/config/save/settings/hardware change. Prior restoration retained; controller untouched; no Claude call/wake. Phone inbox empty. Remote1767/1768/1781 pending at last known bridge1785; no fresh query.
- Next ready unit: trace scheduler2EF0938 and queue+48/work-token publication to connect pending+40, worker restart and actual GPU completion; then producer closure proof. Concrete vtable and map replacement invariants remain open. Serial ownership until exit; commit/push with Codex attribution and Discord result via existing notifier after push. Static evidence only; use git log -1 for checkpoint commit.

## Latest checkpoint - FO98 live-map references traced
- Traced251660 live-map insertion: stores both pointers, calls record[0] vtable+8, and lock-increments optional record[1]+8. Allocation2509EC publishes under owner+CE8; explicit caller2F03E80 confirmed. Evidence: notes/handoff-codex-fo98-live-map.md.
- Three new static provenance checks PASS. Correction: record[0] vtable+10 is release-shaped, not a proven wait; concrete vtable/GPU completion boundary still unresolved. No runtime/lifetime/safety/performance verdict; FO remains NO-GO. Parent32b8109; experimental24494d5 and tested1.7.61/FO0/default visuals unchanged. Prior passed suites not repeated.
- No local game at entry/checkpoint; no game test/lock, Ghidra, build/deployment or installed ASI/INI/config/save/settings/hardware change. Prior restoration retained; controller untouched; no Claude call/wake. Phone inbox empty. Remote1767/1768/1781 pending at last known bridge1785; no fresh query.
- Next ready unit: trace retirement admission2EEF3B0 ->2EEE310/2EF01F4 ->2EEF950 to locate GPU completion before callback dispatch; then producer closure/ring rebuild proof. Concrete vtable and map replacement invariants remain open. Serial ownership until exit; commit/push with Codex attribution and Discord result via existing notifier after push. Documentation/static evidence only; use git log -1 for checkpoint commit.

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
