# Codex handoff - 2026-10-14

## Stop state: all work paused

The user ordered: finish the current tests, save results, then pause ALL work. Both device peers confirmed PAUSED. This handoff request authorizes documentation only; it does not resume development, tests, agents or game launches. Wait for explicit user resume.

This document supersedes earlier **current-state** claims in AGENT_BRIEF, codex-progress-report and chronological trial notes. Historical evidence remains valid within its stated scope. The broader Oct12 work is indexed in [the previous consolidated handoff](handoff-codex-to-claude-2026-10-12.md).

## Did the devices actually test the game?

| Device | What actually happened in the final CPU tests | What was NOT completed |
|---|---|---|
| MAIN | User played and won Yoritomo and Ibaraki with all six reviewed CPU switches ON. Both combat intervals have CPU and frame measurements. | No controlled repeat of identical combat. No causal 10-20% CPU saving established. |
| PC2 | Reached actual Central Kyoto shrine field with bundle OFF and ON; visual/control and patch-state checks passed. OFF captured 60 seconds of CPU. | PresentMon produced no frames.csv. ON navigation/review exhausted the capture time reserve; no ON measurement. No paired CPU/FPS conclusion. |
| Ally | Game process launched for J1 OFF, then stopped during startup when Steam changed its runtime files. | No gameplay, input, CPU/FPS window, ON arm or final OFF arm. No J1 performance result. |

Do not describe the PC2/Ally attempts as successful optimization benchmarks. Do not say neither launched: both did. MAIN supplied the only completed gameplay measurements of the new combined CPU bundle.

## MAIN results and installed state

| Fight | Prior WG/LK/MW CPU | Combined CPU | Prior FPS | Combined FPS | Prior / combined p99 frame time |
|---|---:|---:|---:|---:|---:|
| Yoritomo | 71.58% | 69.49% | 86.01 | 87.45 | 15.545 / 13.803 ms |
| Ibaraki | 57.41% | 55.41% | 93.50 | 94.24 | 14.240 / 13.861 ms |

CPU is game-process utilization normalized across 12 logical CPUs. Observed reduction is about 2 percentage points, or 2.91%/3.49% relative; FPS rose 1.67%/0.79%. These are descriptive comparisons of different manual fights, durations and outcomes. Earlier Yoritomo ended in death; current fights ended in wins. Do not claim the bundle caused the entire difference or that the user's 10-20% target was reached.

Evidence root: `D:/nioh3-re/cpu-bundle-main-20261013`.
- `two-boss-summary.json`, `slice-yoritomo.json`, `slice-ibaraki.json`, `analyze-two-bosses.py`.
- `two-boss-result/` retained engine/addon logs; `metrics/` raw CPU, hardware and frames plus clock mapping.
- Actual selected intervals 64.13s/82.21s, 5609/7747 frames, QPC coverage passed. Collectors exited 0/0, errors[]. Frame-type classification unavailable; sampled game logs report FG off. Missing optional sensors remain unavailable.
- Prior comparisons: `D:/nioh3-re/manual-wg-lk-matched-20261013` (Yoritomo), `D:/nioh3-re/manual-wg-lk-20261013` (Ibaraki). Earlier MW run: `D:/nioh3-re/mw-manual-hosted-final-20261013`.

Fresh read-only check for this handoff: no Nioh3 process found. Recorded MAIN launch state is `game-exited` (old PID29304/start2026-10-13T16:45:28.1627418Z). Recording was finalized earlier. No game or settings changed for this handoff. User progress was never rolled back; final pre-test saves were hash-verified and retained, and later manual progress was preserved.

Installed files freshly hash-verified in `C:/Program Files (x86)/Steam/steamapps/common/Nioh3`:
- Engine1.7.73 combined SHA256 `09cb2e9dc72109fa2c2c0affed8dd7ef0dae0de99dfe9871fceb8ed1498f73b1`.
- Addon1.4.22-retry3 SHA256 `2c381f3c3b0293e4d2a873ae1c07151352d359c0a0e2f4d720630cf0ef13ecd2`.
- Current INI: RD/M2S/J1/WG/LK/MW=1; M1/M2=0. Other preexisting settings preserved.
- Do not install the old main-branch bin over this: it can remove newer hosted menus/features. Engine version alone does not distinguish candidate builds; use hashes.

## CPU implementation and source locations

Combined source `D:/nioh3-wt/cpu-bundle-20261013`, commit `b9cba94cc1c01b51ba91401af13cd4e4ef27662e`, hosted base592a888. Packet393 `D:/nioh3-re/candidates/cpu-bundle-b9cba94.zip`, SHA256 `73568f7a56f6a03413a6bc08bd52648cc633248b977a1c7f81d6870237678284`. Contains engine, source diff, exact INI deltas and validation; addon separate. Source was built in isolated worktrees, not merged into main merely by documenting it.

- **RD** (`a52f82a`, `D:/nioh3-wt/drain-wait-20261013`): paired renderer-drain wait/wake. Preserves full queue/in-flight predicate, reservation-gap polling and completion decrement. Uses bounded WaitOnAddress after a short spin; fallback retains stock polling. Native race/reset and actual thunk-unwind checks passed. MAIN recorded exercised waits/wakes with errors0/fallback0.
- **M2S** (`a1b5fef`, `D:/nioh3-wt/meshlet-seed-20261013`): two seed reads at493DC/49412 changed to plain loads immediately before retained locked CAS. 40,002 native cases passed. It is a small subset, not full M2.
- **J1**: existing eight job-queue load changes, enabled in the combined MAIN build. Source/ordering review supports the experiment; no independent measured J1 gain.
- **MW/WG/LK**: existing paired meshlet completion wait, wake gate and reviewed load changes retained. Earlier manual comparisons were mixed; see prior notes rather than claiming all are individually proven gains.
- **M1/full M2 remain OFF**. M1 cache omits relevant identity/state and has publication issues; real tail append87583C invalidates the old immutable-list argument. Full M2 has unresolved ordering questions beyond instruction-value equivalence.

Combined build passed Release, all260sites, 39-site overlap check, RD/MW race checks and M2S native checks. These do not replace gameplay/performance validation. See [CPU stack findings](cpu-stacks-current-2026-10-13.md), [M1/M2 audit](meshlet-cpu-audit-20261013.md), [RD design](renderer-drain-wait-candidate-2026-10-13.md) and [trial chronology](combined-cpu-trial-2026-10-13.md).

## PC2 final state and failures

Final report3171, evidence **401**, SHA256 `0489b432e84844ba0afe6c307f17158907ef905897fdeb3e50cbf1d163fdc8df`. Corrected runner397, GO3136. OFF:59.9994281wall-s,556.328125CPU-s,9.272223796 CPU cores. No frames.csv. ON reached matched field and feature proof, but less than240s remained inside its fixed14min arm; capture refused. RD/MW cumulative errors0, RD fallback0, but those are not an ON measured window.

Both arms restored six saves and all13 tracked files; game/runners/collectors absent, task16 lease free. Explicit PAUSED confirmed. Peer-local handoff: `C:/Users/User/projects/nioh3-engine/notes/handoff-pc2-2026-10-13.md`.

Earlier failures worth retaining so they are not repeated:
- Rematch3080: entry navigation timed out before FightNow. A script-scope `$entry` collision then prevented restoration; reviewed recovery390 completed exact restoration, preserved trial-exit saves and released the lease.
- CPU GO3125: inherited input helper still hardcoded old owner13024 even though outer runner used new owner9772. First key refused; no gameplay. Root review missed this inherited binding. Narrow397 corrected it; prior result stayed failed/immutable.
- Final time budget included model/tool/image-review latency. That was a test-runner limit, not evidence of a game failure or a performance regression.

Last reported peer owner9772, thread01a1362c-3f4a-7fe2-8fa6-73c9cc62e7d6, wake worker7764. These are historical identities; re-discover on explicit resume, never reuse old launch admissions.

## Ally final state and failures

Final report3177/3178, evidence **402**, SHA256 `081a9977aaef0a8e099ad3d7f2bfe9c325c5b747c49530d13d8d3c95869eba35`; original recovery evidence400 retained. J1 runner399/GO3159 launched only A1 OFF. Steam content log confirms Runtime4 update during startup, changing run/_v2-entry-point/VERSIONS. No field, input or measurement followed. This is an interrupted test, not evidence J1 failed or improved performance.

All40 entry files/saves/flags/backups and remaining62 dependency pins verified; only three documented Steam runtime changes retained. Game29194/runner28336 absent. Bounded recovery observer passed. Durable supplemental resolution preserved original failure/HOLD/guard bytes, then task17 released and holderNone verified. No Steam runtime rollback, repinning or game retry. Explicit ALL WORK PAUSED.

Source mismatch request was resolved with **398**, exact e9941b4 Git table/header/dllmain and original manifest binding installed engine5efe5951. Local evidence `D:/nioh3-re/candidates/ally-j1-e9941b4-source.zip`; no rebuild required. Original399 source and failed admission remain immutable; resume would need current runtime identity.

Ally does not have proven chat auto-wake installation. It actively polled the bridge; keep-awake and chat auto-wake are different. Last owner13902/ticks161835 is historical, not a future launch binding.

## Boss rematches, training and loading

- User's empty-arena-after-death bug reproduced in earlier MAIN Yoritomo logs. Addon kept historical fight/PASS/gate latches and arena spawn override across same-mission death. Native encounter reset semantics were not proven by those latches.
- Candidate retry3 source `D:/nioh3-wt/rematch-retry-epoch-20261013`, commit `5b8b40a09e2e7d4932f7500a0d9b41a20b9d7c58`: restores owned original native spawn on anchored confirmed death before reload, disables automatic retry warp, scopes new verification to a fresh epoch. Added actual normal Return menu with confirmation/attempt binding. Focused source/native tests passed.
- Retry3 is installed on MAIN and the two wins exercised ordinary fights/mission exits. **Neither win tested death recovery. PC2 never completed the defeat/retry/Return trial. The reported empty-arena bug is not live-verified fixed.** See [death/retry findings](yoritomo-death-retry-20261013.md).
- Training/Practice: prior PC2 evidence366/trial2844 demonstrated actual native field pause/resume, Practice shrine/control and healthy held phase2. **Summon, visible released boss/control and native return remain unaccepted.** Earlier Tick contention diagnosis and observer correction are documented in the Oct12 handoff.
- **Skipping the entire mission load is not implemented or accepted.** AutoContinueLoad skips the post-load confirmation tip; it does not eliminate loading. Earlier rematches used native mission loads (Ibaraki19.612s/Yamagata16.297s request-to-control). Do not confuse arena entry/cover progress with seamless arena-only reset.

## Other work retained, not resumed

- Ally PS05ad/PB2 vertex/combined PB2 compiler gates passed on the then-current runtime, evidence331+332/334. Those were no-draw native acceptance, not image equivalence or FPS gains. Runtime later changed; preserve provenance. Live lodu/shader performance work remains incomplete. See previous handoff for packages339/345/349 and exact limits.
- Phone graph/dashboard and Claude/Codex limits work lives in `universal-optimizer-phone-dashboard`; exact-chat relay and automatic notifications in `universal-optimizer-phone-delivery`. Older handoff records commits and tested behavior. No new UI verification performed for this document.
- Measurement tools are in `C:/Users/Ahmad Hamza/universal-optimizer/tools/bench/MEASUREMENT.md`: synchronized process CPU/QPC, frames, RAM and available GPU sensors. Use actual gameplay intervals. Mango100ms samples cannot establish individual-frame p99; absent sensors/frames are not zero values.

## Workflow to preserve if explicitly resumed

1. Re-read this stop state and obtain explicit resume before any new test or source work.
2. Preserve MAIN manual progress. Never restore an old test snapshot over user saves. Device test cleanup uses its own freshly verified entry only.
3. Reuse existing tools; avoid broad sweeps, repeated unchanged suites and long review chains. Resolve real blockers narrowly. Validate current owner through actual inherited input paths, not just outer scripts.
4. CPU/FPS comparisons need both measured arms and comparable scene/camera/settings. No benefit claim from install logs, boot, compiler success or a single baseline. No artificial RAM/PSI/noise launch gates; telemetry only. No OS/driver/hardware tuning or graphics reductions.
5. Before a future PC2 comparison, resolve missing PresentMon CSV and navigation/review budget at source; do not extend a running trial ad hoc. Before Ally, bind actual current Steam runtime rather than reverting it. These are backlog items, NOT tasks authorized by this handoff.
6. Required ACKs stay internal. No user-chat echo for every peer message. Meaningful STARTED/BLOCKED/RESULT/PAUSED milestones must update phone and Discord through existing bridge-status-sync (no model calls). Do not expose webhook/token secrets.
7. Bridge client cwd `C:/Users/Ahmad Hamza/universal-optimizer-phone-delivery/agent-bridge`; channel nioh3, agent codex. Last checked cursor3178. Read-only status is fine; do not wake peers into new work while paused. Outbox `D:/nioh3-re/status/bridge-sync/state.sqlite3` distinguishes delivery from actual test acceptance.

All local implementation/review agents finished. No new candidates, retries or tests are authorized. Keep failed evidence immutable and preserve the distinction between promising MAIN observations and missing remote validation.
