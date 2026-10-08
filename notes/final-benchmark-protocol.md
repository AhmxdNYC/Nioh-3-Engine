# Final benchmark protocol (PC + ROG Ally X)

Run at the very end, with the user present, after the test matrix (`tools\testmatrix`) has produced the per-device KEEP set.
Steam is one account: PC and Ally run one after the other (`py -m uo_bridge.gamelock acquire nioh3` / `release`). Game-side only: no TDP, boost,
governor, env-var or gamescope changes; the same OS/driver/power settings for every run of a device.

## What is compared
| config | contents |
|---|---|
| STOCK | game without the mod (no .asi, original shaders). A mod-loaded ALL-OFF run is also kept as the overlay-cost control. |
| ALL-KEPT | ASI with the device's KEEP flags (matrix verdict `BEST`, or `BEST_lowrisk` if BEST regressed) + the KEEP shaders |
Each config is run at two graphics presets: **MAX** (everything on its top level, 1080p on the Ally) and the **user's preset** (note its values in the result header).
Frame generation OFF unless the item under test needs it (record it). Same upscaler setting in all runs.

## Fixed conditions (write them into every result file)
Same save, same spots (A: fixed camera, nothing moving = STAND; B: the same walking/camera route = MOVE; C: the same combat encounter = COMBAT), same time of day,
game version v2.02, driver version, fps cap (120 on PC; on the Ally the cap the user plays at), no other apps, Steam overlay off, device on mains power
(PC) / the user's normal power mode (Ally, recorded, never changed by us). Warm-up: play 60 s in the level before the first window; do not restart between windows
of one run. Order inside a block: STOCK, ALL-KEPT, STOCK, ALL-KEPT, ... (interleaved) so thermal drift hits both equally. Let the device idle 2 min if temps are high.

## Per run (one launch)
1. Launch, load the save (timed: **LOAD1** = launch click to controllable in the level).
2. STAND 20 s at spot A (2 windows), MOVE 20 s on route B (1 window), COMBAT 25 s at C (1 window; scripted fight or the same enemy, record deaths/rerolls: invalid if the fight differed).
3. **One level load, timed**: take the same door/teleport to the next stage and back; **LOAD2** = trigger to controllable, in seconds, plus the number of frames above 40 ms in the 12 s after it (hitch count).
4. Wait 35 s in the level so the mod prints a 30 s stats window; keep the `stats` lines.
5. Quit via the menu (no kill) so the log is flushed.

## Repetitions and statistics
3 runs per config x preset (fresh launch each), so STOCK/ALL-KEPT at MAX and at the user's preset = 4 blocks x 3 runs per device. Per window take avg fps, 1 % low
(1000 / p99 frame time), p99 ms. Per run take the median of its stand windows for STAND, the single value for MOVE/COMBAT. Report the median of the 3 runs and the
min-max range. A difference counts as real only if it is larger than the range of the control (STOCK vs STOCK). Drop a run if the character left gameplay, the
load route differed, or a capture is shorter than 90 % of its length.

## Metrics
| metric | PC (PresentMon `tools\PresentMon.exe`, 20 s timed) | Ally (MangoHud + measure.sh) |
|---|---|---|
| fps avg / 1 % low / p99 frame ms | MsBetweenDisplayChange (fallback MsBetweenPresents); `tools\testmatrix\tm.py stats` | MangoHud frametime log |
| GPU busy | MsGPUBusy | gpu_load % |
| CPU busy / power | MsCPUBusy | cpu_power W (read-only), core load |
| GPU W and MHz | optional (HWiNFO/driver log) | gpu_power W, gpu_core_clock MHz (mean over the window) |
| efficiency | fps per GPU W when power is logged | fps per GPU W; W/MHz at equal fps |
| load seconds | LOAD1, LOAD2 from stopwatch/log timestamps | same |
| hitches | frames > 40 ms in the 12 s after LOAD2 | same |
| input latency (optional) | `TS = 1` line `stats latency` | same |
| mod counters | `stats/s ...` lines from the log, each 30 s | same |

PC capture command per window: `PresentMon.exe --process_name Nioh3.exe --output_file <csv> --timed 20 --terminate_after_timed --stop_existing_session --no_console_stats`.
`tools\testmatrix\run_pc.ps1 -Preset MAX|USER` automates STAND + MOVE (+ boot-to-gameplay time) per item; the COMBAT window and LOAD2 need the user (or a `-CombatScript`).

## Output
`notes\final-benchmark-results.md`: one table per device and preset (rows STOCK, ALL-KEPT, delta %), the per-item KEEP/DROP list from the matrix, GPU W/MHz, load seconds,
hitch counts, screenshots for `shot` items. Then update the README (kept/dropped verdict per device) and post the summary to Discord (webhook pasted by the user in-session, never stored).

## User additions (2026-10-08, 04:00) - these extend everything above
Start only when decompilation + optimization are done. Everything measured in gameplay.
1. **Every mod setting and every game graphics level** on both devices: each preset (LOW/MID/HIGH/MAX, and per-setting levels where cheap) x {STOCK, mod ALL-KEPT}, plus each mod flag alone (`tools\testmatrix`).
2. **Re-test every dropped experiment** (shader rewrites GI #121/#414, fog, toonedge2, esm, wdo28, mpc, mshlod, trvs/pbvs, post shaders; R2a/R2b/R1; AS0; LM; RWb/WG; GL1/LC/LW) on both devices.
3. **Input latency** in 3 configs: STOCK (game alone), OPTIMIZED (mod, FG off), OPTIMIZED + FG. PC: PresentMon 2 `MsAllInputToPhotonLatency`/`MsClickToPhotonLatency` + `MsPCLatency`. Ally: no software tool reads end-to-end latency on Linux, so a 240 fps phone slow-motion camera filming button press -> screen change (>= 20 presses per config, median), plus the present-to-display part from MangoHud frametimes.
4. **Tools people use** (alongside our own logs): PC = RTSS/MSI Afterburner overlay + log, Task Manager CPU/GPU %, HWiNFO (CPU/GPU temps, power, clocks); Ally = MangoHud (fps, frametime, CPU/GPU %, temps, power, clocks) and the Bazzite/Steam performance overlay. Record temps for every run.
5. **Best bang-for-buck settings** per device (PC 7600X + RX 9070 1440p; ROG Ally X RC72LA 1080p): per graphics setting, the fps cost of each level vs its visual change (screenshots) -> a recommended preset per device with the mod; FG listed separately (never counts toward the goal).
6. **Stability runs**: the user plays for a long session per device with the recommended setup; log crashes/hangs (mod log + game exit code).
