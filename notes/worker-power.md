# Worker-thread wakes and CPU power (static analysis, 2026-10-07)

Goal: cut total CPU work on the ROG Ally (25 W shared budget) so the GPU gets more watts. Ally data (ASI 1.1.0, W6 on,
lightest preset, FG off, standing): main thread ~23 %, CPU still ~15.3 W, WK counter ~27k "wakes"/s, 0.3 % skipped.
Everything below is from static analysis (decompile + Ghidra); none of it is measured yet.

## What the 27k/s really is
- WK counts executions of the wake path at 40A4C, not SetEvent calls. That path only signals when a sleeping worker
  exists, and five other wake paths bypass WK entirely.
- Wake policy = "wake one on first pop": JobGroupWait 3E7CC wakes on the first pop per wait; JobWaitSleepBackoff 207200
  (101 call sites) wakes on every pop, so waits written as 207200 loops can wake once per job.
- Each woken worker that finds work wakes another: a fan-out of J jobs wakes up to min(J-1, pool width) workers; each
  chain usually ends with one wasted wake (the last worker arrives after the queue drained, worse under Proton's slower
  wakes). WK's 0.3 % skip rate fits: when WK checks, work is still queued.
- Model: ~450 wake checks per 60 Hz tick ~= fan-outs x pool width (WorkerCap 10 -> pools 6-8 wide).

## Wake sources (estimates), ranked
| # | Source | RVAs | est. wakes/s |
|---|---|---|---|
| 1 | Pop-path chains | 40A4C -> SetEvent 40B0A | up to ~27k, 30-50 % wasted (~5-12k/s) |
| 2 | Push + explicit wake-one per submission | JobPushToPool 31A514 then JobWakeOneInPool 48B9C (43 sites) / JobWakeOne 48C84 (20) | 20-60/tick ~ 1-4k |
| 3 | ResourceLoad manager idle poll | job 1343720 -> 1FEE28: Sleep(1) loop at 1 kHz; while loads in flight also wakes a worker per ms | ~1k timer + up to 1k worker |
| 4 | "Signal until acknowledged" loops | 3FD10 (each frame from MeshletRequestLoadStep 5DC18), 3EB38 | 1-5k SetEvent syscalls/s (CPU, no extra threads) |
| 5 | EffectDisp kick + join of empty lists | EffectDispKick 3D6F14 (call 3D6F46), EffectDispJoin 3D6F60 (call 3D6F8B); empty list checked in EffectWorkerRunList 3D83BC | up to ~0.8k at a shrine |
| 6 | Task-graph job wake | 40D2F (job creator 3249D0C) | < 0.5k |
| 7 | Already handled | W3 ~900/s, L1 ~50/s | small |

Rough cost ~10 us CPU per wake under ntsync -> halving the wakes may return ~0.5-1 W (unmeasured).

## Plan for the ASI (all default OFF, live switches, Ally A/B)
1. MEASURE first: counters for real SetEvents at 40B0A, 40D2F, 48C6B, 48D06, 48DEC, 48E3C; worker sleeps per pool at
   1A6BC35 (WaitForSingleObject); wake-check caller split (3FE14 / 403A0 / 207200) from 40800's return address.
   Wasted wakes ~= sleeps - checks from 3FE14.
2. Lowest risk: EffectDisp skip - don't kick/join a Disp worker whose list is empty at kick time (remember the skip so
   the join matches). Output unchanged.
3. Stricter WK gate at 40A4C: skip the wake when queued jobs <= (woken-not-yet-running workers) + reserve. A woken but
   not running worker is counted in the pool idle count (+30h) without bit0 set. Reserve 0 = wake only as many as
   queued jobs; reserve 1 also drops the last wasted wake of each chain. Optional per-pool cap on awake workers.
4. Lower priority: thin the signal-until-ack SetEvent loops (3FD10/3EB38); back off the ResourceLoad idle poll.

Ally A/B: same scene, CPU W, GPU MHz, fps, p99, and the new wake counters, toggling each fix live.
