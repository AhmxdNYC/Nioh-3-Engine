# Current CPU call-stack findings, October 13

Completed on main game16044/start07:45:01.3177922Z with installed Engine1.7.73/MW+WG+LK unchanged. Reused existing `patcher/cpuprof.fsx`; no new detection system, injected profiler or game writes. Two12s captures at04:06:48 and04:12:33 EDT, jittered10ms/17ms, max24 busiest threads per tick.23,352 +14,880 stack samples, both zero read failures. Raw reports/TSVs, exact-RVA symbol join and comparison JSON: `D:/nioh3-re/cpu-stacks-20261013`. No user play/input or game-screen inspection was needed. Existing loaded scene was not visually reviewed; this is not combat/open-world coverage or an optimization benchmark.

## Where CPU time goes

Thread-family GetThreadTimes totals, in equivalent logical cores (not function costs):

| Family | First | Confirmation |
|---|---:|---:|
| Job scheduler workers |2.347|2.168|
| Stage/world workers |1.032|1.150|
| Collision workers |0.808|0.820|
| Main game thread |0.744|0.818|
| Meshlet-request workers |0.734|0.802|
| D3D12 command dispatcher |0.494|0.528|

First total7.75cores. Worker-family names do not imply that every cycle is spent on the named subsystem; workers also poll/execute shared jobs. Many snapshots land in ordinary native blocking waits. The profiler's automatic SPIN-WAIT SUSPECTS label is a heuristic, not evidence that a blocked wait consumed CPU. Do not multiply wall-time sample shares by thread CPU totals to invent exact function CPU costs.

## Concrete remaining work

1. **Renderer drain busy loop is still present.** Main self samples land at418F40:6.989% then7.097% of main snapshots. Exact RIP418F75 dominates, inside418F60..418F77. Export `decomp/0x418f40.c` repeatedly calls DispatcherRingBusy until empty; this is actual repeated polling, distinct from sleeping in NtWait. Existing S1 is OFF and adds PAUSE only; it is not a10-20% CPU fix. A blocking replacement needs dispatcher publication/wake closure, not arbitrary Sleep or skipping drain.
2. **Meshlet streaming repeats a capacity sum.** On worker26996,48E90 is15.622% self; its49153/49157 samples hit list-sum loop49150..4915A. Other meshlet workers also sample that loop. Prior `notes/worker-threads.md` already researched M1 caching and M2 atomic-load candidates. Installed M1/M2/J1 are OFF, LK is ON. This nominates existing candidates for a focused reviewed test; no new gain or modern cache-lifetime proof follows from this capture.
3. **Remaining active work is distributed.** Main DrawEmit1E80B0, copiesB94230/B94250, texture streaming1E7050 and stage-table updates1D84A0 appear. Dispatcher work includes PSO lookup119820, graphics binding118604, resource transitions115E24, D3D12 and driver calls. Collision workers hit D80B0 (not merely a thread-name inference); stage workers hit469C0 and object-part/lifetime helpers. Scheduler0 includes object-pool reuse12FBDD0, object updates1A7BF0 and bounds71C60. No blanket permission to skip these jobs without invalidation/behavior proof.

The earlier MW wait path still appears through normal WaitOnAddress; no wait-error or missing-work conclusion. Its prior15.258% attributed share and current5.550% are from different scenes/builds/profiles, not a measured CPU saving.

## Measurement limits and next action

External sampler briefly suspends one thread at a time. Held-time p99<=20us, maxima491/342us; delivery maxima17.248/3.977ms. Capture timings are diagnostic and excluded from FPS gain claims. Busiest24-per-tick selection means some lower-CPU threads have totals but no stacks. Symbol names are reverse-engineering annotations, and leaf/stack-scan fallbacks retain uncertainty. Aggregate stack percentages are not execution-only ETW sample weights.

Next optimization tests should target the already-known meshlet capacity redundancy and renderer-drain work separately, with process CPU and aligned frames in an unchanged scene. A main wait loop accounting for~7% of one thread's snapshots cannot by itself explain most of7-8busy cores. Current evidence does not establish a10-20% CPU reduction or a single root cause for all game load. User kept WG/LK/MW ON; no configuration change made. Game remains running, profilers exited.
