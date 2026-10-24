# Universal tools proposal (cx-pc #29043, 2026-10-24)

Offline mining of nioh3-engine `tools/` and universal-optimizer `tools/` + `TECH_DEBT.md`. Already universal (do not redo): guard.py (+`leftovers`), watchdog.py, abba.py, pmstats.py, session_capture.py, playsession.py, notify, steam_autoplay, profiles.py, devices.py, protonlog.py, gate.py.

Ranked by value / effort. Device coverage: pc = RX 9070 Windows, pc2 = RX 6650 XT Windows, ally = Bazzite/Proton.

| # | tool | source in nioh3-engine | gap in universal-optimizer | effort | pc | pc2 | ally | status |
|---|---|---|---|---|---|---|---|---|
| 1 | **Sensor logger + window summary** (CPU/GPU temp, package power, clocks, GPU busy; before/after at equal fps) | `tools/sensors/lhm_log.ps1`, `tm.py sensors/ingest` (HWiNFO/RTSS CSV) | none (bench/ has PDH only: no temps or power) | S | LHM web server | LHM | hwmon + RAPL + amdgpu | **DONE** `tools/sensors/sensorlog.py` |
| 2 | **Resumable chunked bench-plan runner** (mirrored ABBA order, retry, state file, pause file, chunk) | `pinned_benchmark_lib.ps1` (`New-PBPlan`), `recover_pinned.ps1`, `final_pc.ps1`, `run_when_idle.ps1` | abba.py judges results but nothing runs/resumes a plan | M | yes | yes | yes (python) | **DONE** `tools/bench/planrun.py` |
| 3 | **Same-look screenshot A/B** against a noise shot | `tools/testmatrix/shotdiff.py`, `shotpairs.py` | none | S | yes | yes | yes | **DONE** `tools/bench/shotdiff.py` (shotpairs batching not ported) |
| 4 | **Run-when-idle + game lock wrapper** (wait for no game / no harness, take bridge gamelock, run script, always release) | `testmatrix/run_when_idle.ps1` (hard-coded `$pat`, user paths) | guard.py has leftovers/owner binding but no wait-and-lock | S | yes | yes | needs sh twin | next: generic `harness/idle_run.py`, patterns from a json list |
| 5 | **Leftover cleanup by recorded pid files** | `testmatrix/kill_leftovers.ps1` | covered by `guard.py leftovers`; only the state-dir convention differs | XS | - | - | - | drop, point docs at guard.py |
| 6 | **Steam cloud "Play anyway" OCR click with scaling/crop** | `steam_cloud_ocr/click/decision.ps1`, `tests` | `steam_autoplay.ps1/.py` exists; TECH_DEBT says OCR misses small text at 1080p | M | yes | yes (small text) | n/a | update steam_autoplay with crop/upscale OCR from steam_cloud_ocr |
| 7 | **PresentMon time-window / frame stats** | `tm.py stats`, `pm_time.py`, `process_cpu_window.py` | pmstats.py + process_cpu_window.py exist; port the BOM-only/ETW join fallback (TECH_DEBT pc2 #3578) | S | yes | yes | MangoHud | update pmstats |
| 8 | **Settings/ini audit + live key map** | `tools/settings_audit.py`, `live_keys.py`, `menu_map.py` | `release/profiles.py` lint covers keys; no source-key vs menu-map audit | M | yes | yes | yes | later |
| 9 | **Fleet update audit** (every machine on the same fleet-v2 commit, upstream set, helper running) | ad hoc `git pull`/UPDATE failures (#28945/#28981) | `agentd` + `handoff/sync.py` | S | yes | yes | yes | propose `tools/fleet/audit.py` reading fleet.json + bridge heartbeat |
| 10 | **Screenshot/report to Discord** | `tools/notify.ps1`, `pc2_notify.ps1` | `tools/notify/notify.py file` exists | XS | - | - | - | already universal; delete the nioh3 copies |
| 11 | Frame-capture/vs/xsys capture scripts | `fcap_capture.ps1`, `vs_capture.ps1`, `xsys_capture.ps1` | game-specific (Nioh GpuOpt hooks) | - | - | - | - | not universal |

## Delivered (branch `cx-pc/universal-tools-29043` in universal-optimizer, commit 27f9aaf)

- `tools/sensors/sensorlog.py`: `log` (backends `lhm` / `linux`, CSV with ISO timestamps), `summary --start --end` with headline `cpu_temp_c / gpu_temp_c / cpu_power_w / gpu_power_w` (+max). Selftest of the summary. The live backends need hardware: LHM must run elevated with the web server on (port 8085); on Linux RAPL needs read permission on `energy_uj` (root or udev rule). **Not run against real sensors yet.**
- `tools/bench/planrun.py`: `expand` (configs x presets x runs, order reversed on even runs, same as `New-PBPlan`), `run` (retry, state jsonl, torn-line safe, `--chunk`, pause file, exit 0/1/2/3), `status`. Selftest covers resume, retry, torn state line, failed arm, pause.
- `tools/bench/shotdiff.py`: moved from testmatrix with a selftest (needs numpy + Pillow).
- README rows added. Nioh3 scripts were not changed or removed (pc owns the final bench using them).

## Next (not done)

1. `harness/idle_run.py` (item 4), then switch `run_when_idle.ps1` callers.
2. Port the OCR crop/upscale to steam_autoplay (item 6) after a dialog-present test, which needs a game launch (not allowed for cx-pc).
3. Fleet audit (item 9).
