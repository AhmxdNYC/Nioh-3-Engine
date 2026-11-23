# How performance is measured (and why the numbers stay accurate)

For engineers joining the project. Every "+X % fps" or "-Y CPU" claim in this repo comes from the method below.
What the features are: [performance-features.md](performance-features.md). The rule book these come from, with the
incident behind each rule: `universal-optimizer/docs/lessons-nioh3.md` (sections B "A/B method" and C "Measuring on the
device") and `universal-optimizer/tools/bench/MEASUREMENT.md`.

## The devices

| device | hardware | OS | notes |
|---|---|---|---|
| PC (Main PC) | RX 9070 | Windows 11 | usually GPU-bound at the user's settings |
| PC2 | Ryzen 5 5600X + RX 6650 XT on a slow chipset PCIe slot | Windows | CPU / PCIe-bound; ReBAR on since 2026-10-10 (older PC2 PCIe tables are pre-ReBAR, check the date) |
| ROG Ally | AMD APU, shared memory | Linux / Proton | power-limited: CPU watts saved go to the GPU; no PresentMon, MangoHud instead |

A result is always **per device**: name the device, the game settings (resolution, render scale, upscaler, frame
generation, fps cap), the build (ASI sha256) and **both arms** (OFF and ON numbers). "It is faster" without those is
not a result.

## What we measure

| metric | from | used for |
|---|---|---|
| fps, frame time, 1 % low, p99 | PresentMon 2.6.0 (`tools/PresentMon.exe`) on Windows, MangoHud CSV on Linux | GPU and general patches |
| GPU busy ms per frame | PresentMon `MsGPUBusy` | GPU patches; also a **guard**: a CPU patch cannot move it |
| process CPU (cores = CPU seconds / wall seconds), CPU s per frame | `process_cpu_window.py` / `session_capture.py` (GetProcessTimes, `/proc` on Linux) | CPU-power patches at a fixed cap |
| input-to-photon latency | PresentMon `--v2_metrics` (`MsAllInputToPhotonLatency`); 240 fps phone camera on the Ally | latency patches |
| load time | the mod's `LoadStats` = **work end** (last evaluation of the load-complete predicate), not the loading screen | loading patches |
| per-pass GPU cost | the mod's `GPUProf` timestamps | finding targets only (it inflates many-draw passes ~5 %) |
| sensors (temps, power, clocks) | HWiNFO / RTSS CSV ingested by `tools/testmatrix/tm.py ingest` | context |

Frame generation: PresentMon `fps` counts generated frames, `app_fps` counts rendered ones. CPU/GPU work is judged on
rendered frames; FG is normally **off** during an A/B because it switches regimes on its own (see below).

## The A/B method

**Live A/B inside one launch (preferred).** Two launches of the *same* build differ by about 5 % (three BASE launches:
144.7 / 144.5 / 152.5 fps), which hides most real gains. So a patch with a live menu row (`2` = installed, off) is
toggled inside one launch, in mirrored blocks of four windows: `ON OFF OFF ON` (or `OFF ON ON OFF`), 20-30 s each,
5 s settle after each toggle, no camera input. A linear drift (heat, streaming) cancels out.

**Launch A/B (boot-time changes only).** One launch per arm in a mirrored order (`BASE F1 F2 F2 F1 BASE`), each F
launch compared with the BASE interpolated between its neighbours.

**Verdict** (`universal-optimizer/tools/bench/abba.py`, Nioh runner `tools/testmatrix/abba2_pc.ps1` +
`abba2_report.py`). A block is thrown out when:
- the two middle windows (same arm) differ > 1 % -> a step happened inside the block;
- the two outer windows differ > 1.5 % -> drift too large to be linear;
- a guard metric moves > 3 % between arms (`--guard gpu_ms:3`) -> the scene changed, not the patch;
- any window is cap-bound (within 1 % of the fps cap: nothing to gain), mixes profiled and clean windows, or has no
  proof the toggle happened.

Then: **KEEP** needs a median gain >= max(1 %, 2 x the measured noise), every valid block positive, p99 not worse
than 2 %. **WORSE** is the mirror. Everything else is **NEUTRAL**, which is a real verdict: the patch stays off and
its numbers are kept in `release/profiles.json`. Too few valid blocks = **INVALID**, rerun.

## Why the numbers stay accurate

1. **Proof of state for every window.** The mod logs `MENU: <label> -> ON/OFF` on each toggle and prints the live
   state in its `stats` lines every 30 s. The harness reads both back; a window without that proof is marked
   `"invalid": "<why>"` and dropped. (Once, a whole run toggled nothing: the key list went to the input tool as one
   argument and the failure was swallowed. Read-back is how that is caught now.)
2. **Test where the change can show.** The limit must be the thing the patch changes. CPU patches are measured
   CPU-bound (lower render scale, FG off) or by CPU cores at a fixed cap; GPU patches by GPU ms. At the user's own
   settings the PC is GPU-bound at 114 fps, where no CPU patch can move fps.
3. **Remove things that change on their own.** In-game frame generation flips between paused / flapping / steady and
   made fake 15-35 % "steps" with no patch change; it stays off in A/Bs. Same upscaler, same scene, same save point
   in every arm. Never compare a 30 s CPU window against a longer, unmatched fps interval (QPC timestamps align them).
4. **Gameplay only.** Menu and title frames are sliced out (`pmstats.py --slice A:B`); averaging them in once made GPU
   passes look 5-6x smaller.
5. **Evidence class on every number**: M = measured on that device, M* = measured with a profiler on (can inflate),
   E = estimate with its basis. PC numbers are never scaled into an Ally table.
6. **Thresholds above the noise floor**, computed from the run itself, not assumed.
7. **Clean machine state, restored after.** Harnesses hold the shared game lock, back up saves and `config.xml`,
   are bound to an owner PID (`universal-optimizer/tools/harness/guard.py`, `harness_guard.ps1`; leftovers are killed
   by exact PID + start time) and restore every file afterwards. They refuse to send input while a human is using the
   PC (keyboard/mouse idle and controller packet counter checked). An unrestored ini would poison the next test.
8. **Dated, per-build results.** Each result names the ASI sha256 and the date; hardware changes (e.g. PC2 ReBAR on
   since 2026-10-10) make older tables stale, and they are marked so instead of silently reused.
9. **Provisional is not shipped as proven.** `release/profiles.json` status `provisional` = recommended but the live
   A/B is still open; `release/package.py --final` refuses a release that still has one.
10. **The tools are tested too.** `abba.py --selftest`, `tools/bench/test_*.py`, and fixture tests for the capture
    collectors (fake clock / fake process) run without a game. The feature table in performance-features.md is
    generated from the ini and checked with `py -I tools/docs/gen_perf_features.py --check`, so the docs cannot drift
    from the shipped defaults.

## Running a measurement

Windows, one session capture (does not launch or close the game):

```powershell
$g = Get-Process Nioh3
py -I universal-optimizer/tools/bench/session_capture.py --pid $g.Id `
   --expected-identity $g.StartTime.ToFileTimeUtc().ToString() --seconds 30 `
   --out captures/new-session --presentmon tools/PresentMon.exe
```

Windows, full live A/B of menu rows (launch, toggles, restore; run hidden, holding the game lock):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/testmatrix/abba2_pc.ps1 -OwnerPid <your pid> -Live CBN,CBT -Quads 3 -Input vpad -DryRun
```

`-Input vpad` drives the game through the mod's virtual-pad pipe, so the PC stays usable. Drop `-DryRun` to run.
Graphics presets for the final tables: `tools/testmatrix/gfx.ps1` + `final_pc.ps1` (README in `tools/testmatrix/`).

Linux / Ally: MangoHud logging already on, then `tools/bench/passive_gameplay_capture.py` (captures only, no input)
or `process_cpu_window.py` for CPU. MangoHud rows are logger samples, not individual frames: no per-frame p99 claim.

## Worked example: CBN / CBT

Skip byte-identical per-draw constant uploads. Live rows -> in-launch `ON OFF OFF ON` quads on PC2 (CPU-bound on its
slow PCIe link, ReBAR on, FG off), state read back from `stats` lines. Result CBN +15.6 %, CBN+CBT +15.9 % fps
(38.3 -> 44.3, run CBU1 #1609); the ROG Ally (Proton/RADV) rerun #1668 showed no harm -> `kept` in
`release/profiles.json`, shipped `1`. The gain is claimed for the device where it was measured, not extrapolated.
