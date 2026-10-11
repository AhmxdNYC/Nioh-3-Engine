# Codex work on Nioh 3

Updated 2026-10-11. This report separates completed work, measured results, and work still pending.

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

Uploaded smoke kit **bridge file 223** and queued pc2 **1767** and Ally **1768**. Both device results remain pending at the last checked bridge cursor, **1781**. Release-candidate packaging is held until both pass. Discord result card posted.

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

Uploaded candidate kit **bridge file 225** and queued native pc2 test **1781**, conditional on smoke **1767** passing first. This branch is **not merged or released**. Before shipping it needs native pc2 validation and further audits of queue-full behavior, command-stream bounds, and unavailable-hook handling. Those audits have started; no additional fix has been completed.

Evidence: `notes/handoff-codex-step2.md`, `notes/handoff-codex-step2b.md`, branch `notes/fo5-pc2-test.md`, and `captures/pc/fastab/20261011-0019-fo11fix/report.md`.

## Tour reporting fix

Fixed a reporting failure caused by PowerShell 5 treating Python's normal stderr clock-alignment diagnostic as a terminating error. Added `tools/tour/report_tour.ps1` and connected it to the tour runner. It captures output, restores the caller's error policy, and still rejects an actual nonzero Python exit.

Recovered the existing fo11 tour report offline. Verified both the successful path and a missing-log failure; the caller's Stop policy remained intact. The tour harness passed all 44 checks. This changed tooling, with no game launch, new ASI, or performance claim.

Committed and pushed on main, currently `e416b61`. Evidence: `notes/handoff-codex-tour-report.md` and `captures/tour-report-check.log`. Discord result card posted.

## Current work and remaining gates

The user has now authorized taking over all remaining tasks and building Codex continuation and phone-chat support. That supersedes the earlier stop-after-each-step instruction for future work. Claude Code has not been called, and its auto-wake remains disabled.

- **In progress:** a separate Codex continuation controller and a Codex chat entry in the existing phone app, in `universal-optimizer`. Initial code is being written; neither is claimed tested or enabled yet.
- **Pending device results:** pc2 and Ally main-build smokes, followed by conditional native pc2 frame-overlap testing.
- **Next independent engine work:** finish frame-overlap safety audits; investigate small replay-thread wins with byte-exact proofs and measured A/B tests. Existing graphics PSO memoization is not a new candidate; compute PSO memoization is still unbuilt, and descriptor caching needs lifetime/mutation proof.
- **Later release work:** reconcile the remaining-work ledger, device matrix, final stock-versus-kept benchmarks, longer stability runs, profiles, documentation, and release packaging. Rematch full validation and multiplayer checks have external/player requirements.

No new hardware, driver, power, or operating-system performance settings were changed. No claim is made that the project is finished.
