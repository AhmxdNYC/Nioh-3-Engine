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

### Follow-up: actual sampled branch and completion closure

Read-only parallel review on October13 narrows the earlier explanation. Raw
cpu-diagnostic.txt line156 records four direct samples in3FD10: three at3FD4D,
one at3FD5C. Retained-dump Capstone decoding confirms these addresses belong
to the M+5F8 byte-poll/Sleep(0) branch, not the cold SetEvent branch19279CC.
The65 attributed samples (15.258% of426 main samples) include callees; only4
(0.939%) directly land inside this function. Neither percentage is CPU savings.

The byte represents completion of the entire request job:5A8C4 sets it before
pool42 submission;9811F8 calls4A2D0 and clears it at981211 only after return.
This independently agrees with notes/frame-io-writer-closure.md. Calling this
runtime observation merely a worker wake acknowledgement was incomplete.

Exact retained-dump callsites identify the remaining latency breakdown:
4A327->3FC48 manager lock;4A3D0/4A41B->5B3AC sequential GPU readback maps;
4A592->3E5F8 CPU child fanout/join;4A60D->4A85C count recomputation;
4A637/4A659->5CEE8 cleanup/flush. The child join actively helps execute jobs,
so passive blocking there would remove useful work. Next targeted diagnosis
must distinguish queued-start, lock, readbacks and child work; the current
profile cannot identify which dominates. No patch or new live capture made.

Subsequent implementation: MW paired completion wait candidate is now built in
commit364c5e2; see [candidate behavior and tests](meshlet-completion-wait-candidate.md).
Default OFF, actual wait/wake code change, not a diagnostic-only addition.
Native race/publication tests and257 pattern sites passed; independent final
source review approved first opt-in gameplay test. Packet376 supplied to PC2
under task2972, adapter preparation ACK2969. No gameplay gain yet. The same
candidate binary must be used for OFF/ON/OFF; preserve all settings except MW.

### Remote results received through2962

Ally evidence373: manual two-boss recording complete, collectors stopped,
game left open and progress preserved. Approximate Yamagata/Ibaraki segments:
31.7/30.8FPS,8.52/8.57CPU cores,97/96.5%GPU, maximum CPU75/77C. Mango interval
averages and approximate alignment remain limitations; no optimization gain.

PC2 evidence374: four OFF/ON/ON/OFF CPU windows9.1095/8.4253/10.1826/10.3917
cores, only three completed wrappers. User movement affects comparability and
PresentMon emitted no CSV: no WG CPU-saving or FPS verdict. Following foreground
failure and initial graceful-close timeout, independent recovery PASS confirmed
13 baseline files and6 saves, user exit saves separately preserved, all owned
processes absent and task16 free. ACKs2961/2962 sent; no repeat launch authorized.

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
2934/2938. ACK/READY2942 now confirms game274917/ticks7394731 and recorder active
at backup/manual-play-20261013-011657; user told PLAY NOW. CPU/RAM1s and existing
full MangoHud/log capture active, unchanged config/mod, fresh backup, no Tour,
save rollback, input automation or game kill. Existing recorder retained instead
of restarting for372. End on user notification; stop collectors only. Clock
alignment approximate/per-framep99 unavailable. Never call send receipt an ACK.
Phone+Discord use existing automatic status sync. No Claude invocation.
Notifications2932/2934/2937/2938 independently confirmed phone_done=1/sent=1,
attempts0/errornull. Actual Ally acknowledgement is separately2942.

User-requested closure: exact main game23500 closed gracefully via CloseMainWindow; process absence verified, watcher state game-exited and lease release succeeded. No save rollback or forced termination. Receipt: D:/nioh3-re/user-telemetry-20261013/user-close-receipt.json. PC2 independently STARTED2981 the admitted MW series (not a result yet).


### Follow-up open-world combat, 02:48-02:54 EDT
User reports open-world killing only, no boss rematch. Root D:/nioh3-re/mw-manual-diagnostic-20261013. MW requested but installer SKIPPED (native census initial-list mismatch), so baseline only. Process15444, capture375.185s/3244.672CPU-s=8.648cores including startup/menu/load, collectors0/0/errors[]. Agent extracted log-selected gameplay slices83.4-87.6displayedFPS and8.89-9.69cores (74.1-80.7% of12 logicalCPUs), system95-100%, summed game3D70-74%; latest99.6s slice02:52:45-02:54:25:87.62FPS, displayedp99 23.34ms, GPUbusy8.98ms, RAM6.43GiB/VRAM9.67GiB. No temperatures/GPUclocks. No gain claim; uncontrolled manual baseline. User-finished collectors stopped, then gameclosed gracefully for already-requested correctedfixrestart, no save rollback. New session mw-manual-native-enum-20261013 staged08557e candidate; runtimeacceptance pending.


### Completed MW + hosted-menu gameplay, 03:18?03:24 EDT

The user completed two boss fights and open-world combat, then requested data collection and game closure. Game 12332 closed normally; the device watcher released its lease. All eight final save files were copied and hash-verified, with no rollback. CPU, frame and hardware collectors finalized successfully (exit 0/0, no recording errors).

Evidence: `D:/nioh3-re/mw-manual-hosted-final-20261013`. Read `gameplay-selection-and-summary.json` for exact log lines, selected intervals and caveats; five `slice-*.json` reports preserve both full fights and combat interiors after control became available. ?Controlled? in those filenames means player control was available, not a controlled A/B experiment.

| Activity | Selected duration | Game CPU cores | Game CPU / 12 logical CPUs | Displayed FPS | Displayed frame p99 |
|---|---:|---:|---:|---:|---:|
| Open world | 51.31 s | 9.506 | 79.22% | 84.86 | 24.371 ms |
| Takeda Shingen combat | 26.49 s | 9.084 | 75.70% | 86.11 | 23.353 ms |
| Minamoto no Yoritomo combat | 34.36 s | 8.840 | 73.66% | 84.68 | 16.529 ms |

Both boss clears and returns are logged. All frame sources bracket their CPU intervals. MW was actually installed and exercised: 13,493 wait rounds, 6,643 timeout returns, 35,560 wake signals, zero reported errors. Timeout returns re-enter the original completion check; they are not failed fights or fake job completion.

The prior manual open-world baseline (8.89?9.69 cores, 83.37?87.62 FPS) overlaps this run. No 10?20% CPU reduction is demonstrated. Scenes were not matched and the earlier engine was 1.7.61 versus this combined 1.7.73 build, so these are not causal A/B results. The whole-run average of 5.17 cores includes title/menu/loading time and must not be presented as a saving. Temperature and GPU-clock sensors remain unavailable.

Discord and phone completion notification 3031 delivered successfully. Combined candidate remains installed; game is closed.
