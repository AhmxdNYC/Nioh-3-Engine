# tools\testmatrix

Two phases share `tm.py` (stats / verdicts) and `matrix.json`:

1. **Matrix** (`run_pc.ps1`, `matrix.json`, `menu_map.*`): one mod flag / shader per run vs BASE -> KEEP / DROP verdicts (`summary_<dev>.csv`, `verdict_<dev>.md`). Unchanged.
2. **Final phase** (this section; protocol in `notes\final-benchmark-protocol.md`): every graphics preset and setting level x {STOCK, OPTIMIZED, OPTIMIZED+FG}, input latency, sensors, README tables.

Run PowerShell scripts as `powershell -NoProfile -ExecutionPolicy Bypass -File <script> ...` (this machine blocks unsigned scripts). Everything has `-DryRun`.

## Files (final phase)
| file | what |
|---|---|
| `presets.json` | valid levels per config.xml key (from `notes\settings-cost.md`), named presets (`MAX HIGH MID LOW A_MAX_MINUS_INVISIBLE B_BALANCED C_FLOOR`, `USER` = config as found), sweep groups, visual-grade table for bang-for-buck |
| `gfxlib.ps1` / `gfx.ps1` | preset switcher: backup + in-place `<KEY>value</KEY>` edit + restore. Refuses unknown keys/levels (the game silently falls back on an unknown value). `-Config <path>` works on a copy |
| `final_pc.ps1` | the driver: cells = run x preset x config (+ sweep), PresentMon windows, latency window, crash-safe restore, game lock |
| `tm.py` (new commands) | `sensors`, `ingest`, `latency`, `table`; `stats` now also returns latency columns + `app_fps` |

## Graphics switcher
```
gfx.ps1 -Action List | Show -Preset HIGH | SweepPlan -Sweep cheap
gfx.ps1 -Action Apply -Preset MID [-DryRun]            # backup (state\config.xml.orig, never overwritten) + write
gfx.ps1 -Action Apply -Preset MAX -Set GI=HIGH,REN_RAT=85
gfx.ps1 -Action Restore
```
Only graphics keys are written (never VSYNC, HDR, upscaler, REN_FPS, DYNAMIC_*); `PRESET` is set to `CUSTOM`. Close the game first (it rewrites config.xml on exit).
Per-setting sweep = each level of a key once on top of the base preset (MAX); `-Sweep cheap` is the 13 keys `settings-cost.md` ranks, `-Sweep ALL` every key (about 100 cells).

## The three configs
| config | mod | FRAME_GENERATION |
|---|---|---|
| STOCK | `Nioh3EngineFix.asi` renamed `.stockoff`, original shaders/swaps, ini untouched | OFF |
| OPTIMIZED | matrix KEEP set (`tm.py best`, `-KeepSet low` = BEST_lowrisk) incl. KEEP shaders/swaps | OFF |
| OPTIMIZED+FG | as OPTIMIZED | FSR (config.xml; PC only, FG crashes the game on Proton so never on the Ally) |

OptiScaler / `dxgi.dll` / `dinput8.dll` stay in place in every config (same upscaler in all runs); STOCK means "without our mod".

## Capturing
```
final_pc.ps1 -DryRun -Presets USER,MAX -Runs 3 -Sweep cheap          # plan, per-cell config.xml diff, time estimate, PresentMon check
final_pc.ps1 -Presets USER,MAX -Runs 3 [-Sweep cheap -SweepConfig OPTIMIZED] [-CombatScript x.ps1] [-PcLatency]
final_pc.ps1 -ReportOnly -Hwinfo D:\logs\hwinfo.csv [-Rtss D:\logs\rtss.csv]   # ingest sensors, write tables
```
Per run: STAND x2, MOVE, COMBAT (with `-CombatScript`), LATENCY (30 s, 25 scripted presses). Results `results\final_pc_<cfg>_<preset>[_SW-key=val]_r<n>.json`; resume skips finished cells (`-Force` redoes). Order is run -> preset -> config (interleaved). LOAD2 and hitch count are manual: fill `load2_s` / `hitches` in the json.

### PresentMon and latency
`tools\PresentMon.exe` is **2.6.0** (checked: has `--v2_metrics`, input tracking on by default, `--track_pc_latency` beta). No upgrade needed for AllInput/Click-to-photon. Latency windows use `--v2_metrics`; columns read by `tm.py stats`: `MsAllInputToPhotonLatency`, `MsClickToPhotonLatency`, `MsPCLatency`, `MsDisplayLatency` (median, p95, sample count per window). Caveats:
* `MsPCLatency` is filled only if the game emits PC Latency markers (Reflex/PCL, `sl.pcl.dll` is shipped); `-PcLatency` adds the flag, a blank column is a valid answer.
* The presses are synthetic (`mouse_event`/`keybd_event`); if a window reports 0 samples, ETW is not seeing them: repeat with real presses or an external macro device. Samples are only counted on frames an input actually reached.
* With FG, `fps shown` counts generated frames; `app_fps` = rendered frames. Read latency next to `app_fps`.
* Ally (Linux) has no such tool: 240 fps phone camera, 20+ presses per config (protocol).

### Sensor logs (CPU/GPU %, temps, power, clocks)
Start HWiNFO (sensors window -> Logging CSV) and/or RTSS/Afterburner logging by hand before the session, same PC clock; keep logging on until the end. `ingest` slices the log by each window's `t0/t1` and stores mean + max in the window json (`sens.hwinfo`, `sens.rtss`). Column matching is by header text (HWiNFO: `Total CPU Usage`, `GPU Core Load`, `GPU Temperature`, `GPU Hot Spot`, `CPU (Tctl/Tdie)`, `CPU Package Power`, `GPU Power`, `GPU Clock`, `Core Clocks (avg)`); any CSV with a `Time`/`Timestamp` column and similar headers works (RTSS benchmark CSV, an Afterburner log exported to CSV; the native `.hml` is not parsed). `py -I tm.py sensors <csv> [--start ISO --end ISO]` shows which columns were picked. Task Manager has no log: note its CPU/GPU % by eye or use the HWiNFO columns.

## Outputs
`py -I tm.py table results pc` (or `-ReportOnly`) writes `results\results_table_pc.md`, ready to paste into the README:
* per preset: rows STOCK / OPTIMIZED / OPTIMIZED+FG with fps [min-max], 1 % low, p99, move/combat fps, GPU/CPU ms and %, temps, W, fps per GPU W, delta vs STOCK (marked `noise` when inside the run spread);
* bang-for-buck per config: sweep cells vs the MAX cell: ms saved, fps gain, visual grade (`presets.json` `visual`), ms saved per grade, verdict (`FREE LUNCH` grade 0 and faster, `good`, `trade-off`, `costly look`, `noise`);
* the latency block (`tm.py latency` writes `latency_pc.md` alone): STOCK vs OPTIMIZED vs OPTIMIZED+FG with delta vs STOCK.

## Needs installing / starting (nothing was installed)
HWiNFO64 (sensors + CSV logging) and RTSS / MSI Afterburner (overlay + log) are **not** installed or started by these scripts; Windows 11 Task Manager already exists. PresentMon 2.6.0 is present. Python (`py`) and .NET (`dotnet fsi`, via `patcher\common.ps1`) are used as before. Optional: a 240 fps phone camera for the Ally latency rig.

## Tested (dry, no game)
`gfx.ps1` Apply/Restore on a copy of config.xml (byte-identical after restore), `final_pc.ps1 -DryRun`, and `tm.py ingest/table/latency` on synthetic PresentMon + HWiNFO fixtures. Not yet exercised against real captures: the first real run should check the sensor column picks and that `MsAllInputToPhotonLatency` has samples.
