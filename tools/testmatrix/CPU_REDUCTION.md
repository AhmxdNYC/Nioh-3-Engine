# CPU reduction comparison: existing device runners

2026-10-13 update: prefer the shared UO `tools/bench` measurement bundle372
(see its `MEASUREMENT.md`), which retains this CPU sampler CLI and adds RAM,
native Windows QPC boundaries, hardware/thread telemetry and a combined report.
367 remains historical; do not overwrite old evidence. PresentMon `--qpc_time_ms`
uses `CPUStartQPCTime`; do not infer timezone corrections. User expects all
available metrics per performance window, with unavailable sensors explicit.

User target: approximately10-20% less CPU at equal or better performance, same
picture/simulation/current frame cap. This is a target, not a demonstrated gain.

Use the current device's working launch/input/restore runner, not abba2_pc.ps1
unchanged: that older FPS-oriented runner changes graphics, cap and refresh rate.
The additive process_cpu_window.py supplies the missing process-CPU measurement.
It neither launches nor controls the game. Python standard library only.

## First run: WG only

- Fresh current-entry save/config/binary backup; current exact owner/game identity,
  device lease, normal bounded exit and restoration. One launch, no automatic retry.
- Preserve resolution, upscaler, frame generation, current cap, graphics, simulation,
  affinity and other patch settings. No OS tuning. Memory telemetry only.
- Existing WG implementation is in dllmain.cpp. If not installed, stage WG=2
  (installed but initially off), preserving other keys. Confirm installation from
  the startup log and actual live menu state. ACK of a command alone is insufficient.
- Reach actual field, establish control, select one fixed camera/scene, settle45s.
  Use existing observed navigation. Keep test navigation/input outside windows.
- Two quads: OFF ON ON OFF, then OFF ON ON OFF. Eight30s windows;5s settle after
  each toggle, close overlay before measuring. Verify state for every window.
  Keep noisy intervals labelled; no prolonged quiet wait or repeated failed runs.
- Run sampler alongside existing PresentMon/MangoHud capture for each window:
  `python process_cpu_window.py --pid PID --expected-identity START --seconds 30
  --label WG-OFF-q1-a --out UNIQUE.jsonl`
  START is Windows process creation FILETIME decimal, or Linux /proc/PID/stat
  starttime ticks decimal, checked against the owned process by the caller.
  Sampler rejects identity changes/exits and preserves incomplete output on error.
- Use first/last sample timestamps to select matching frame data. Capture frame
  data before the CPU interval starts and through its end; retain raw files and
  clock mapping. Do not compare CPU30s against a longer, unmatched FPS interval.
  If exact alignment cannot be established, report that limitation, not acceptance.

## Read the result

CPU cores = delta process user+kernel seconds / elapsed wall seconds. This is
actual scheduled CPU consumption, not PresentMon MsCPUBusy. Report mean core
equivalents plus relative ON/OFF change. Divide by the same logical CPU count
only when presenting Task-Manager-style percentage; retain raw seconds.
Also report CPU seconds per presented frame, FPS, and per-frame tail times where
the collector truly has individual frames. MangoHud interval averages do not
establish individual-frame p99. Preserve FG state and label presented versus
rendered frames; do not imply generated frames are simulation work.

Compute each quad separately, comparing mean ON against mean OFF with durations
accounted for. Report both, OFF/OFF drift and raw windows; disagreement/noise or
slower FPS/worse smoothness means inconclusive/regression, not a10-20% win.
CPU savings at unchanged capped FPS are relevant; do not reject them as capped.
Linux measurement covers the named game PID only, excluding wineserver/other
Proton helpers. Include existing helper/system CPU telemetry if available; do not
claim whole-Wine-stack or package-power savings from the game PID alone.

## Delivery and scope

Send brief ACK/STARTED/RESULT or blocker; root's automatic relay handles Discord
and phone status. Digest logs locally, report numbers/evidence/restoration, avoid
repeating unchanged suites. PC2 task2880 and Ally task2881 authorize this one WG
comparison once the sampler and narrow local runner checks pass. Preserve a fixed
session deadline sufficient for navigation plus these windows; no deadline reset.
SPF and LK are follow-ups after WG results, not simultaneous changes in this run.

Validation: three focused unit tests cover multicore CPU accounting, Linux names/
ticks, identity/counter failures and immutable outputs; a real Windows query smoke
passed. No new gameplay or CPU-gain result is established by those checks.
