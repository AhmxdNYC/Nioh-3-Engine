# Combined CPU trial — 2026-10-13

User requested testing all reviewed CPU optimizations together on MAIN and PC2. This supersedes the M2S-only PC2 queue. No performance improvement is established yet.

## Exact build and settings

Isolated source `D:/nioh3-wt/cpu-bundle-20261013`, commit `b9cba94cc1c01b51ba91401af13cd4e4ef27662e`, based on hosted `592a888`. Integrates reviewed RD `a52f82a` and M2S `a1b5fef`, preserving MW and hosted menus. Existing J1 included. Engine SHA256 `09cb2e9dc72109fa2c2c0affed8dd7ef0dae0de99dfe9871fceb8ed1498f73b1`.

Packet393 `D:/nioh3-re/candidates/cpu-bundle-b9cba94.zip`, SHA256 `73568f7a56f6a03413a6bc08bd52648cc633248b977a1c7f81d6870237678284`. Release build, all260sites, 39 relevant nonoverlapping sites, RD races/unwind, MW races and M2S40,002 native cases passed. These are source/native checks, not gameplay evidence.

Merge only `[Patches] RD/M2S/J1/WG/LK/MW=1; M1/M2=0` into current INI. M1 and full M2 remain excluded due documented correctness gaps; see meshlet CPU audit. Existing CPU fixes and visual settings retained. Engine still reports1.7.73; identify by hash. Pair with retry3 addon SHA256 `2c381f3c3b0293e4d2a873ae1c07151352d359c0a0e2f4d720630cf0ef13ecd2` (source5b8b40a, packet384). Rematch death recovery remains live-unverified.

## MAIN

Root `D:/nioh3-re/cpu-bundle-main-20261013`. Previous16044 session closed normally by exact PID/start check. Its installed files/logs retained under previous-session. Final saves copied and hash-verified before staging; NO save rollback. `stage-launch.ps1` installs exact engine/addon, changes only listed INI keys and invokes existing start_recording plus synchronized UO session_capture. CPU/RAM/GPU and PresentMon frames recorded together; optional unavailable sensors explicit. Memory is telemetry only. Startup-only or menu samples must not be described as gameplay measurements.

Existing user instruction not to inspect MAIN game screen remains respected. MAIN session is available for manual gameplay; preserve user progress. PC2 supplies the controlled same-location comparison. MAIN manual scenes are not deterministic A/B evidence.

## PC2

Previous rematch3080 timed out before FightNow: no death/retry/Return tested. Cleanup failed before restoration because top-level `$entry` stage assignment overwrote the script-scope save manifest. Persisted backup remains valid. Exact recovery390 reviewed and authorized in3099; preserves exit saves and original failure record, restores verified current-entry files, writes durable proof before releasing lease. Recovery result pending at this checkpoint; do not reuse stale admission.

3107 requests existing-runner same-binary RD/M2S/J1 OFF then ON, with WG/LK/MW ON in both and M1/M2 OFF. Fresh entry/ownership proof and source delta needed after recovery. Actual field, matched location/camera,30s settle+60s capture, CPU+frames+GPU/RAM. Local guarded inputs/image review remain PC2-local. RAM/noise telemetry only; no artificial gates or OS tuning. No PC2 combined launch claimed until STARTED arrives.

Discord/phone delivery for3099/3107 verified in bridge-sync SQLite: both sent and phone_done, zero errors. Routine peer ACKs stay internal.

## Runtime checkpoint

### Two real fights recorded — result

User completed Yoritomo and Ibaraki. Both native boss-end and mission-clear events recorded. Capture finalized cleanly (both collectors exit0, errors[]); MAIN29304 remains running and user saves are preserved. Root `two-boss-result/` retains both logs; `analyze-two-bosses.py`, `two-boss-summary.json`, and slice JSONs reproduce the comparison. QPC-aligned combat intervals exclude loading and postfight travel.

| Fight | Prior WG/LK/MW CPU | Combined CPU | Prior FPS | Combined FPS | Prior/current p99 frame ms |
|---|---:|---:|---:|---:|---:|
| Yoritomo |71.58%|69.49%|86.01|87.45|15.545 /13.803|
| Ibaraki |57.41%|55.41%|93.50|94.24|14.240 /13.861|

CPU normalized across12logical CPUs. Relative reductions2.91% and3.49%, roughly2percentage points each. Descriptive improvement only: manual movement, combat phases, duration and outcome differ; earlier Yoritomo ended in death. Not controlled proof or10–20% gain. Current requested UTC intervals17:04:40–17:05:45 and17:09:01–17:10:24, actual64.13s/82.21s and5609/7747frames. FrameType unavailable but sampled game log reportsFGoff. RD/MW errors0; RD fallback0. No current death cycle, so no acceptance of retry fix. Both mission loads still occur; no arena-only loading claim.

RESULT3123 sent to automatic Discord/phone. PC2 successor9772 acknowledged transfer3115 and supplied source395 for controlled OFF/ON; independent review active, no PC2 combined launch yet. Ally source-only J1 preparation396 awaiting installed-build table binding.

MAIN29304 start16:45:28.1627418Z, watcher3616, collector parent31212, PresentMon27316 and PDH25836. Six runtime patch checks passed and retained in `runtime-ready.json`: RD2sites, M2S2, J1eight, MWtwo, WGinstalled and LK18. Addon log confirms1.4.22-retry3. RD startup rounds/wakes active, errors0/fallback0; this proves execution, not measured benefit. User can play; recording runs until game exits, stop.flag or7200s. Frame-type classification unavailable (existing explicit compatibility fallback); CPU/QPC/frames retained. No screenshot inspection on MAIN.

PC2 reported3106 recovery390 PASS: six saves/all13 files restored, exit saves preserved, original failure retained, lease16 released.3113 explicitly confirms combined393/3107 received but NOT STARTED: user there requested handoff and a new chat. Task is in `C:/Users/User/projects/nioh3-engine/notes/handoff-pc2-2026-10-13.md`;3114 tells successor to acknowledge and bind fresh exact owner before preparing existing comparison runner. Do not imply PC2 test is already running. MAIN ready milestone3112 sent to automatic Discord/phone sync.

PC2 launch authorization3125 issued after independent source395 review PASS (D:/nioh3-re/reviews/pc2-combined-395/lead-source-review.json). Exact template changes only authority ID; owner9772 and fresh entry pins remain fixed. Approved existing OFF/ON command,30min total/14min arms/no second below12min. Actual STARTED confirmation pending; review delay was coordinator-side.

PC2 GO3125 launched15644 but first input refused stale owner13024 in inherited helper; no key/field/measurement.3132 reports exact six-save/13file restoration and free lease. Review missed hardcoded helper owner despite its unchanged pin. Root reviewed correction397 SHA8d8a0f8427c2e3a28b13ec5771fe2cea85fcdfd980eee09a0efc5d43d4654288: helper exact owner9772/start binding, local path/hash247afb78, sourcepins2b0c7c72, unchanged current-entry/metrics, actual narrow diff. New GO issued for exact397; fresh output pc2-combined393-3132-reviewed, prior failed result immutable. Existing budgets and restoration unchanged.

PC2 corrected GO3136 actually STARTED3139: OFF game4056/start2026-10-13T10:45:37.0325487-07:00, runner9732, series20188; normal navigation underway, no measurement result yet. Ally followup3135 was waiting on root source binding. Resolved3142 with source-only packet398 SHAc4ce589de73f847a28eeaf21ca9a978f7276465246e0f527b1e4b3b17e207122: exact e9941b4 Git patches.json a4be4524/headerfb8e8269/dllmainc24cf4d5, mod treebd56ba710, original candidate manifest and retained engine5efe5951 hash verified. No rebuild/reproducibility claim. Ally directed to finish narrow J1 OFF/ON/OFF runner; no Ally launch yet.

User requests PAUSE ALL WORK after current tests. MAIN recording already complete; current PC2 OFF/ON and assigned Ally J1 remain in scope, then cleanup/evidence/handoff only.3151/3152 require peers to ACK and end RESULT+PAUSED. No new test rounds after failures. OFF PC2 measured59.9994s CPU but missing frames.csv; do not claim FPS comparison or rerun solely for missing data. Ally399 final source review underway as last preparation for assigned series.

Both peers ACK pause condition3154/3155. Final Ally399 narrow review PASS (exact source/entry/dependencies/current input owner, unchanged-save cleanup, runtime eight-site proof); GO issued for one existing0/1/0 series, then PAUSED even if incomplete. No additional test rounds or revisions after failure. Agent rematch_fast_restart completed final review and paused; all other local agents already complete.

FINAL peer reports: PC2 RESULT401/3171 SHA0489b432e84844ba0afe6c307f17158907ef905897fdeb3e50cbf1d163fdc8df: OFF59.9994281s CPU9.272223796cores, no frames.csv; ON matched field but insufficient remaining240s reserve, no measurement. No paired CPU/FPS conclusion. All6saves/13pins restored, lease free, ALL WORK PAUSED. Ally399/GO3159 stopped before input/field/measurement because Steam updated Runtime4 during startup (content-log-confirmed). Independent400 audit all40files/flags/backups exact, game/runner absent; only3runtime hashes changed, bounded recovery observerPASS. Root reviewed400 and authorized bookkeeping-only durable supplemental resolution, preserved failed receipts/hold/guard, lease release then PAUSED; no runtime revert or new test. Neither device produced a completed optimization comparison.
