# Main PC manual gameplay and standard measurement follow-up

User requested modded manual play with CPU/GPU observation, no screen inspection.
Installed Engine1.7.73/addon1.4.21 retained; graphics/FG/cap and patches unchanged.
Fresh save/config/mod backup: `D:/nioh3-re/save-backups/20261013-005149-user-recording`.
Steam initial launch waited at failed Cloud sync; actual game23500 began00:54:48EDT.
Main device lease nioh3-pc is owned by codex-user-recording and renewed by the
existing watcher until game exit. No automatic save rollback or input. User's
manual progress is retained. Game was left open after recording.

## Evidence and measured segments

Private evidence root: `D:/nioh3-re/user-telemetry-20261013`.
Raw: metrics/process.jsonl, hardware.jsonl, frames.csv (52,394 records),
session.json, capture-result.json, engine-observed.log, boss-observed.log.
Derived: gameplay-segments.json, four named segment JSON reports,
cpu-diagnostic.txt/.tsv, cpu-symbol-join.json. No screenshots were taken/read.

| Local EDT segment | Game CPU cores / 12 logical | Displayed FPS | Displayed p99 ms | GPU 3D engine activity |
|---|---:|---:|---:|---:|
| Open world00:55:50-00:56:55 |6.57 /54.8%|111.86|12.442|92.1%|
| Boss1 00:57:19-00:58:29 |6.47 /53.9%|112.89|11.513|92.8%|
| Open world01:00:03-01:01:22 |9.71 /80.9%|82.75|25.045|67.0%|
| Boss2 01:02:30-01:03:00 |9.36 /78.0%|83.24|24.520|74.3%|

User confirmed open-world play and two boss fights. Segment boundaries use actual
BossBattleMode start/end and engine load/menu events, with margins; CPU samples
bound the actual reported intervals (63.18/69.13/78.27/28.62s). The later boss
was rematch R15FF; logged battle ended and replay mission exited, spawn override
restored. This does not resolve the separate RA4B4 death/retry bug.

Different scenes, not an A/B optimization result. Later CPU rises and GPU activity
falls: evidence of CPU-side pressure, not a proven single cause. FG config was OFF
when read; initial PresentMon did not request FrameType. One out-of-order ETW
completion was retained raw and sorted by absolute QPC in derived analysis. The
initial CPU sampler's QPC stamp followed the memory query rather than bracketing
GetProcessTimes: both clocks share QPC, but that small acquisition offset was not
bounded. The updated shared sampler fixes this for future captures. PDH reports
summed3D-engine activity, not normalized whole-GPU utilization. Temperatures,
GPU clock/power unavailable on this Windows capture; CPU frequency is PDH-reported.
The first hardware child exit1 was an owned termination on stop; raw counter rows
were already flushed, no hardware error log. New collector exits on stop.flag.

## Separate CPU diagnosis, not benchmark timing

Five-second external cpuprof sample at01:13:47EDT, interval10ms, at most12 busy
threads sampled concurrently, 5112 samples, zero read failures. It samples one
thread at a time; held-time p99<=20us/max353us. No game writes. Profile timing is
excluded from the earlier performance segments. This later scene is not assumed
identical to either boss. Process total8.47cores; family CPU counters:

- job schedulers2.482cores; main game0.869; dispatcher0.542;
- meshlet-request family1.140; collision family0.931; stage family0.825.

These are thread-family totals, not proof all their CPU is productive subsystem
work. Sampled collision worker8 landed mostly in JobWorkerThreadMain; meshlet9
showed MeshletRequestWorker, JobWorkerThreadMain and JobHelpStep. Main attributed
stacks: KtglSignalUntilAwake2(+3FD10)15.258%, DrawEmit5.869%,
SpinWaitFlagSleep0(+7D9E90)5.869%, Memcpy3.991%, TextureStreamUpdate3.052%.
Scheduler0 attributed21.831% to CndWaitSrw. These are sample shares, not measured
CPU savings. Busy-between-samples plus a wait stack only nominates a candidate;
it does not prove the wait was spinning. Existing logs report GF fallback0 and
W6 slept/spun0, so do not label all wait samples broken existing fixes.

Profiler read zero names from the commented symbols TSV; an offline exact-RVA
join against re/symbols/symbols.tsv resolved619 rows without rerunning capture.
Original profile is retained. Best next target: real WG OFF/ON comparison and
targeted scheduler/wake-path review; no new patch or10-20% saving established.

Direct read of `D:/nioh3-re/decomp/0x3fd10.c` confirms the named wake helper is
called by MeshletRequestLoadStep(+5DC18). One branch atomically reads a shared
flag and repeatedly SetEvent's the worker while bit0 is set; another polls a
byte flag. Both yield with Sleep(0) every256iterations. This explains why it is
a candidate; the acknowledgement writer and required wake/order semantics still
need review before any patch. The descriptive RE name is not a vendor symbol.

## Tooling and other devices

Universal Optimizer already had cpucores.ps1, pmstats.py, ABBA/seqtest, session
PresentMon capture; Nioh had monitor.ps1/pc2_monitor.ps1. Custom runners did not
require one combined report. Shared UO tools/bench now combines CPU/RAM/frame/
hardware capture, explicitly reports missing sensors and clock/FG limitations,
uses native QPC boundaries on Windows, and avoids per-frame p99 claims from
MangoHud interval averages. bench.ps1 uses this default; see its MEASUREMENT.md.
Validation:83 tools+75 bridge+32 nexus tests PASS (190), focused terminal-boundary
smoke, real Windows CPU/RAM/PDH and owned PresentMon/report integration PASS.
No changes to game binaries/settings were needed. No memory/noise admission gates.
Packet372 SHA ccd2acdfe5bb5ab563cd6f3b48425bcc71d47d603e3a13da511a2ca1dae494d7.
Shared UO implementation commit7f47eae. The old separate CPU measurement tool
was therefore not missing from UO; the integration into custom test runners was.

PC2 prior369: OFF only, ExitCode wrapper bug;371 fixes it. Task2937 authorizes one
fresh WG run with packet372 CPU/QPC/RAM and hardware streams, fixed20min.
PC2 ACK2939 and STARTED2941: runner17448, fixed deadline22:38:16PDT;
observed navigation then eight30s windows underway, result pending.
Ally370: OFF only, missing math import; exact import-only diff reviewed and65
manifest hashes match, clean restoration/observer. User then replaced pending
automatic WG r5 with manual play: two fights/open world, installed current mod,
normal saves, no Tour save-block, no auto inputs/termination. Task2932, reminders
2934/2938; ACK outstanding at this checkpoint. Never call send receipt an ACK.
Phone+Discord use existing automatic status sync. No Claude invocation.
Notifications2932/2934/2937/2938 independently confirmed phone_done=1/sent=1,
attempts0/errornull. This is notification delivery, not the missing Ally ACK.
