# Universal-optimizer port shortlist (from Nioh 3 history), 2026-10-24

Method: read Nioh `tools/` (testmatrix, rematch, tour, playsession, sensors, bench), `.handoff/{done,next,checkpoints,user-directives}.md`,
and the UO `tools/README.md`, `docs/lessons.md`, `docs/lessons-nioh3.md`, `docs/methodology.md`, `tools/harness/*`. Not verified by running anything.
UO already has (do NOT re-port): guard.py/harness_guard.ps1 (owner PID, headroom, crash limit, leftovers by exact pid), quiet.py (foreign-load
wait), watchdog.py, abba.py/seqtest.py (drift-aware A/B), pmstats.py, process_cpu_window.py, playsession.py (save backup/restore + game lock),
datafiles/filepatch.py, gate.py, steam_autoplay, agentd test-active marker, notify, runbudget, digest.

## Part 1 - new tools (rank by value/effort; effort S/M/L)

| # | Tool | Value | Effort | Port from (Nioh) | Notes |
|---|---|---|---|---|---|
| 1 | **Chunked-run runner with resume + boot-hang kill** (generic `chunkrun.py`/ps1: schedule CSV, `-Skip/-Take`, 2 attempts per chunk then stop, results row per chunk with `valid`, per-attempt log copy, hard wall watchdog, no-main-window-for-50 s boot-hang kill, restore saves on watchdog, release game lock) | very high (user HARD RULE #28530; hr build boot-hang looped for hours) | M | `captures/test127/run129.ps1` (wrapper loop, watchdog, `Bad`/`LastRow`), `tools/rematch/audit_screen_run.ps1` + `audit_capture_helpers.ps1` (`Test-AuditBossHeartbeat`, `Assert-AuditCaptureResult`, log-offset delta reader) | Strip Nioh paths/pipe names; take game exe, launch cmd, "alive" predicate, valid-row predicate as parameters. |
| 2 | **Sensor logger: LibreHardwareMonitor web JSON -> CSV** (+ slice-by-window + mean/max ingest) | high (CPU heat / power is now a headline metric; temps need one-time admin LHM, then unprivileged) | S | `tools/sensors/lhm_log.ps1` (30 lines, columns named by sensor path); ingest side `tools/testmatrix/tm.py` `sensors()`/`ingest()` (L241-319) | Fixes to apply while porting: robust number parse (current regex strips minus/units, breaks "1,234"), write header once with fixed column set (Export-Csv -Append breaks when a sensor appears late), buffer + flush, exit cleanly if :8085 is down, log the sensor name list once. UO's `windows_metrics.ps1` explicitly says "no hardware sensor provider" - this fills that gap. |
| 3 | **Unprivileged power/util sampler**: RAPL package power via `typeperf "\Energy Meter(RAPL_Package0_PKG)\Power"` (or Processor Information), per-core util, GPU Engine util, GPU temp/power via ADLX/ADL no-admin reader; same CSV schema as #2 so a run can fall back to it | high (works with no admin; CPU temp is the only gap) | S-M | RAPL command noted in `.handoff/checkpoints.md` (00:50 and 01:10 entries); ADLX reader was assigned to gxl-pc (see `.handoff/next.md` ~L2379) - check whether it landed before porting; `tools/bench/windows_metrics.ps1` in UO is the place to extend | Record "CPU power as heat proxy, temp unavailable" in the CSV header so reports do not show blank as zero. |
| 4 | **PresentMon clock-offset auto-detect** (`pm_time.py`) | high for correctness (pc2 PresentMon wrote CPUStartDateTime ~14 h off UTC -> 0 frames in every window, silently) | S | `tools/testmatrix/pm_time.py` + `test_pm_time.py` | UO `pmstats.py --slice` and `measurement_report.py` have no offset detection (grep for CPUStartDateTime found nothing). Wire into `--slice`. |
| 5 | **Config/ini swapper with validation** (`cfgswap`: backup once, never overwrite the .orig, in-place `<KEY>value</KEY>` / `key = value` edit, refuse unknown keys or levels, restore, works on a copy for tests, byte-identical round trip) | medium-high (every settings audit needs it; game silently falls back on bad values) | S-M | `tools/testmatrix/gfxlib.ps1` + `gfx.ps1` + `presets.json` shape; ini variant `tools/testmatrix/lg_ini.ps1` (set-or-insert key under section) and `tools/testmatrix/test_arm_ini.ps1` | Generic form: schema file of valid values per key; two backends (xml element, ini section). |
| 6 | **run_when_idle**: wait until no game and no other harness process matches a pattern list, take the bridge game lock, then run a script; poll 10 s with max wait | medium-high (several agents share one lock key and stepped on each other a minute after a run ended) | S | `tools/testmatrix/run_when_idle.ps1` | Replace the hard-coded pattern with a pid-file registry (guard already writes pid files: use those instead of CommandLine regex). |
| 7 | **Screenshot A/B diff with noise control** (`shotdiff.py`: A vs B judged against A vs A2 from a second stock launch; amplified diff image; crop) | medium (the same-look rule needs it; launch-to-launch TAA/FSR jitter makes a raw diff meaningless) | S | `tools/testmatrix/shotdiff.py`, `shotpairs.py` | Pillow optional like gate.py. |
| 8 | **Steam "Unable to Sync / Play anyway" dialog handler** (OCR, Windows.Media.Ocr, only when owned window in foreground, user idle, exact words, dry-run) | medium (hands-free launches stall on this dialog) | M | `tools/testmatrix/steam_cloud_click.ps1` + `steam_cloud_decision.ps1` (pure decision function, testable) + `test_steam_cloud_*.ps1` | Check first whether UO `steam_autoplay.ps1` already dismisses it. Keep decision logic separate and unit-tested as in Nioh. |
| 9 | **Display refresh raise/restore** around a run (ChangeDisplaySettingsEx flags 0, dynamic, never registry; restore in finally) | medium (a 60 Hz panel silently capped runs, run8b analysis) | S | `Restore-DisplayRefresh` and caller, described in `notes/run8b-analysis.md` L24-25; code in `tools/testmatrix/` (grep `ChangeDisplaySettingsEx`) | |
| 10 | **Report/table generator with `noise` marking and bang-for-buck verdicts** (STOCK / OPT / OPT+X per preset, delta vs STOCK marked noise when inside run spread, latency block) | medium | M | `tools/testmatrix/tm.py` `table_md`, `latency_block`, `paired_runs`; `abba2_report.py` | UO `measurement_report.py` and `abba.py` cover verdicts; port only the table/markdown layer and `--v2_metrics` latency columns (`MsAllInputToPhotonLatency`, `MsClickToPhotonLatency`, `MsPCLatency`). |
| 11 | **Hub of offline checks** (`testhub/hub.py`: catalog of reviewed lightweight tests, plan never executes, device work plan-only) | low-medium | S | `tools/testhub/hub.py` | UO has `scripts/check.ps1`; only worth it if the catalog idea is wanted. |
| 12 | **Pinned-benchmark state machine** (hash-pinned dependencies, coordinator marker with token, refuse stale-state restore, recovery script that needs ownership confirmation) | low for now (very Nioh-specific, heavy) | L | `pinned_benchmark_lib.ps1`, `recover_pinned.ps1`, `writable_save_policy.ps1` | Port only the idea: a recovery marker with a token so a crashed run cannot restore over a newer state. Skip the rest. |

## Part 2 - updates to existing UO tools

1. **`tools/harness/guard.py` + `harness_guard.ps1`: boot-hang detection.** Add `assert_booted(proc, no_window_s=50, hard_s=...)`: game process
   exists but `MainWindowHandle == 0` for 50 s counts as a crash (feeds the existing crash limit), kill by exact pid, restore ini/asi. Source:
   run129.ps1 loop around "boot hang (no window 50 s)". Today the crash limit only counts a vanished pid, so a hung boot loops until max-minutes
   (what happened with half-rate ASIs on pc, 2026-10-24).
2. **Same files: chunk awareness.** Add `Guard.chunk(name)` that records `last_finished_chunk` in the pid/state file and refuses to start a chunk
   longer than `max_chunk_min` (default 10); "same chunk failed twice -> stop, exit code 98, write reason". Pair with Part 1 #1.
3. **Never kill another owner's game.** The Nioh guard kills `Get-Process Nioh3` by name in several wrappers (run129 does
   `Get-Process Nioh3 | Stop-Process`). In UO make `Close-Game` take only registered pids from the pid file and refuse (log "foreign game") when a
   Game.exe exists that the pid file does not own and the bridge game lock is held by another agent. Add test cases to `test_guard.ps1`
   (bystander with the same image name). `leftovers` already spares live owners by pid; extend the rule to the launch/kill helper.
4. **`tools/bench/windows_metrics.ps1` / `measurement_report.py`**: gain the sensor sources from Part 1 #2/#3; report `power_w`, `cpu_temp`, `gpu_temp`,
   `gpu_power` with a `source` and `unavailable_reason` field; add `fps_per_watt` and "same fps, less power" verdicts (heat is the metric players care about:
   user #28855 "CPU frying").
5. **`tools/bench/abba.py` / `seqtest.py`: valid-run reasons.** Make every invalid window carry a machine-readable reason (menu/loading, boss never
   spawned, toggle not read back, 0 frames after clock offset, PresentMon not elevated, wrapper watchdog) and print a per-reason count in the verdict.
   Nioh lost 5/5 pc2 attempts and many pc runs to unnamed invalids (`.handoff/done.md` 2026-10-20 and 2026-10-23 entries).
6. **`tools/bench/pmsession.ps1` / `pmstats.py`**: check elevation before starting (PresentMon records nothing without it; `needs-player.md` item), and
   fail loudly on "0 frames" instead of writing an empty CSV; use `pm_time.py` offset detection on `--slice`.
7. **`playsession.py` / tour wrappers**: save snapshot uses `robocopy /MIR` restore and a timestamped folder; document that a watchdog kill must restore
   the snapshot and release the lock (the run129 watchdog branch does both by hand). Provide `playsession.py recover` for a dead wrapper.
8. **`tools/steam/steam_autoplay.ps1`**: confirm it handles the cloud-sync dialog and uses the foreground/idle gating from `steam_cloud_decision.ps1`.
9. **`agentd` test-active marker**: wrappers that set `uo-agentd\pc\test-active` must clear it in `finally` and on the watchdog path; add a TTL so a
   crashed wrapper cannot block other agents forever (run129 clears it only on the happy path).

## Part 3 - lessons worth a playbook entry (suggest `docs/playbooks/long-runs-and-sensors.md`)

- **Chunk everything (HARD RULE #28530).** One arm/phase per launch, <= ~10 min, results to disk after each chunk, resume from the last finished
  chunk, two failures of the same chunk = stop and fix the cause. Why: a single 4.5 h audit lost hours to repeated game deaths (#24318, #28530).
- **Two repeats per arm, not three** (#25964); do not repeat a whole audit per device, use a short stage-2 confirm list (#24946).
- **A build that hangs at boot loops forever unless the harness has a no-window timer.** Boot-check every new artifact on one device before benching
  (pc2 validates startup, then pc runs chunks).
- **Invalid-run taxonomy.** Boss never spawned (poll 60 s, not 8 s), combat never proved (boss idle: gate on attack-and-record proof), menu/loading
  frames in the window, FG state flapping, GPU-bound test scene for a CPU change, 60 Hz panel cap, PresentMon not elevated, PresentMon clock 14 h off,
  stock not measurable because the mod is needed to summon (Ally). "Nearly doubles" was a loading-screen artifact. Report valid/total counts always.
- **Sensors: what is possible without admin.** CPU package power via RAPL counter, per-core and GPU engine util, GPU power/temp via vendor API
  (ADLX), PresentMon frame/display latency: yes. CPU temperature: needs LibreHardwareMonitor/HWiNFO running elevated once (UAC click by the human; the
  human said no admin steps, so ask only once and only for the sensor service), then read over the LHM web server (:8085 `data.json`) unprivileged.
  Use CPU power as the heat proxy when temp is unavailable. Log thermals for a steady-state window; note the cap (equal fps at 4K 120 cap) so
  "less heat at the same fps" is the comparison.
- **Heat metric.** Judge spin-wait fixes by cores burned, package watts and temperature at equal fps, not by fps (extends the existing handheld rule).
- **Never kill by image name.** Several agents share one machine and one game lock; kill only registered pids; wait for idle with `run_when_idle`.
- **Input-less safety.** Steam dialog automation only on an owned foreground window with the user idle; never type (already in lessons, add the OCR
  pattern).
- **Screenshot A/B needs a second stock shot** to price the launch-to-launch noise.
- **Do not claim a device number until reconciled with human experience** (#26960: Ally bench ~20 fps vs 30-45 fps played): check bench settings/scene
  against the real play config before publishing.
- **Hash-pin and token-mark recovery state** so a late restore cannot overwrite a newer run (pinned_benchmark idea).

## Suggested order
1) Part 2 items 1-3 + Part 1 #1 (one chunk-runner PR, highest value). 2) Part 1 #2, #3, #4 (sensors + PresentMon clock, all S). 3) #5, #6, #7.
Everything else when needed.
