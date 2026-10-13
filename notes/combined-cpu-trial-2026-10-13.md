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

MAIN29304 start16:45:28.1627418Z, watcher3616, collector parent31212, PresentMon27316 and PDH25836. Six runtime patch checks passed and retained in `runtime-ready.json`: RD2sites, M2S2, J1eight, MWtwo, WGinstalled and LK18. Addon log confirms1.4.22-retry3. RD startup rounds/wakes active, errors0/fallback0; this proves execution, not measured benefit. User can play; recording runs until game exits, stop.flag or7200s. Frame-type classification unavailable (existing explicit compatibility fallback); CPU/QPC/frames retained. No screenshot inspection on MAIN.

PC2 reported3106 recovery390 PASS: six saves/all13 files restored, exit saves preserved, original failure retained, lease16 released.3113 explicitly confirms combined393/3107 received but NOT STARTED: user there requested handoff and a new chat. Task is in `C:/Users/User/projects/nioh3-engine/notes/handoff-pc2-2026-10-13.md`;3114 tells successor to acknowledge and bind fresh exact owner before preparing existing comparison runner. Do not imply PC2 test is already running. MAIN ready milestone3112 sent to automatic Discord/phone sync.
