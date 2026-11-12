# CPU burn: what keeps ~8 cores busy at ~52 fps on pc2 (#90776, untouched-map target 4, human #89754)

performance-pc/0, 2026-11-12, offline (no game). Synthesis of existing measurements; no new capture.
Input: pc2 #90721 (5600X, RX 6650 XT, 1.8.12 shipped ini + MaxFps 240, FG off, FSR 50 %): 50.9 fps, GPU 18.0 ms,
**0.151 CPU-s/frame = 7.68 cores busy**. `captures/pc2-cpucut-89928/r2rel` is on pc2 only (not in this checkout); it has
process totals, not per-thread data. Per-thread split below is from the main PC (`cpu-stacks-current-2026-10-13.md`,
7.75 cores, same total) plus pc2 SP profiles (`cpu-bound-midrange.md`) and the Ally SP study (`cpu-hotspots-ally.md`).

## Where the 7.7 cores go (main PC per-thread GetThreadTimes, 10-13; pc2 total matches)

| thread family | cores | nature |
|---|---|---|
| Job scheduler workers | 2.2-2.3 | real work (object update 1A7BF0, pool reuse 12FBDD0, bounds 71C60, skinning) + wake churn; workers block properly (`job-system.md` §1: no spin before blocking) |
| Stage/world workers | 1.0-1.15 | real work (StageObjLodUpdateRange 469C0, object lifetime) |
| Collision workers | 0.8 | real work (CollisionObjectUpdate D80B0) |
| Main game thread | 0.75-0.8 (PC) / ~0.93 (pc2, TSTATE 92-94 %) | ~40 % draw prep + **23-30 % WaitDispatcherDrainSpin 418F40** + fence/present Sleep(0) waits |
| Meshlet-request workers | 0.73-0.8 | real work + **capacity re-sum loop 49150** (15.6 % self of a worker) + Sleep(0) fence polls |
| D3D12 dispatcher | 0.5 | PSO lookup 119820, barriers, memcpy |

Honest headline: **most of the 7.7 cores is real engine work done in parallel, not spinning.** The spins/polls that
exist add up to roughly 1-1.5 cores. That is the same-look CPU cut available; more needs fewer logic ticks (TG, changes
feel) or a multi-core rewrite (not built, risky).

## Top 3 CPU burners on pc2 and the same-look fix for each

| # | burner | evidence | est. CPU now | same-look fix | est. saved | status |
|---|---|---|---|---|---|---|
| 1 | **Main-thread waits that spin**: WaitDispatcherDrainSpin 418F40 (polls DispatcherRingBusy 418F90 until the replay thread drains) + GfxWaitFenceValue 7D9E1C / SpinWaitFlagSleep0 7D9E90 Sleep(0) loops (fence + Present wait) | pc2 SP: drain spin 23.7 % of [App]Game = ~6 ms/frame; Ally: fence Sleep(0) 57.6 % of [App]Game when GPU-bound. pc2 at 52 fps is GPU-bound (18.0 of 19.6 ms), so the fence/present spin share rises like on the Ally | ~0.4-0.6 core | (a) **GF=1 + W6=1 on Windows** (built, bit-exact, shipped on Proton since 1.1.0/1.4.0; on Windows they were judged by fps only, +0.38 %, never by CPU). (b) **SN** drain nap: crashes 3/3 on #6879 (0xC0000005 at Nioh3.exe+298EA12); rerun on fixed build #6914 0edcde64 | GF+W6: ~0.3-0.5 core; SN: ~0.2-0.3 core more | GF/W6 ready (ini flip); SN needs pc2 rerun A/C/D |
| 2 | **Job-worker wake churn + locked polls** (scheduler pool) | Ally WakeStats: 14-16k wakes/s steady (~6k wasted), 40-165k/s storms for ~4 min after a load; LK/J1 locked loads on idle polls; MQ bone matrices ~1 % per scheduler thread | ~0.2-0.4 core of the 2.3 (rest is real work) | **WG + J1 + LK + MQ** on Windows (all built, bit-exact or same-result; currently 0 / MQ Proton-only) | ~0.05-0.1 core steady, ~0.3-1 core during post-load storms | built, opt-in; needs one CPU-s/frame ABBA |
| 3 | **Meshlet-request workers re-summing page-pool capacity** (MeshletRequestWorker_ListSumLoop 49150 inside 48E90) + locked bitmap loads + Sleep(0) fence poll GfxFencePollWorker 117D6C | 15.6 % self on worker 26996 (10-13 PC stacks), loop seen on other meshlet workers too | ~0.12-0.15 core | **M1** (cache the sum) + **M2** (plain loads); 117D6C is covered by GF site 2 | ~0.1 core | built, OFF; never A/B'd for CPU |

Not same-feel, listed for completeness: **TG** (logic ticks follow render rate): pc2 runs 75-83 logic ticks/s for ~38-52
rendered fps; TG measured +2.6-3.3 % fps on pc2 and ~1-1.4 cores saved on the Ally, but changes input timing (taps
held ~33 ms). Biggest single CPU lever; human decision only.

## Proposed next step (one pc2 run, ~25 min, same-look only)

Arms on the shipped 1.8.12 ini + MaxFps 240, same spawn spot, 2 x 60 s windows, ABBA, report CPU-s/frame, cores busy, fps,
1 % low (the #90721 method):
- A = shipped (GF/W6/MQ auto -> 0 on Windows)
- B = GF=1, W6=1, MQ=1, WG=1, J1=1, LK=1, M1=1, M2=1 (all built, no pixel change)
Expected: B -0.5 to -1.0 core (-7..-13 % CPU-s/frame) at flat fps. If it holds, flip the profiles.json Windows auto values
(GF/W6/MQ) and add WG/J1/LK/M1/M2 to the release profile. SN only after #6914 A/C/D passes.

## Per-thread capture recipe for pc2 (data missing for pc2 specifically)

Process totals alone cannot attribute the burn. Run once per arm, in the steady window:
1. The tool already used 10-13: `dotnet fsi patcher/cpuprof.fsx` (GetThreadTimes per thread family + stack
   samples, 12 s, max 24 threads/tick) -> gives cores per family like the table above. Raw output under `D:/nioh3-re/`.
2. ETW alternative (exact CPU samples, all threads): elevated `wpr -start CPU -start GeneralProfile -filemode`, play 60 s,
   `wpr -stop pc2-cpu-<arm>.etl`; then `wpaexporter -i pc2-cpu-<arm>.etl -profile CPUUsageSampled-ByThread` (or
   `xperf -i <etl> -a dumper`) and group by thread name ([App]Game, Scheduler*, TS_MESHLET_REQ*, collision, stage,
   SynergisticDispatcherTask). Symbolize Nioh3.exe RVAs with `re/symbols/symbols.tsv`.
3. Also log the mod's SP + WakeStats (`[Profiler]` SP=1, WakeStats=1) for the wake/s count.

## Symbols

All burners above already have names in `re/symbols/symbols.tsv` (WaitDispatcherDrainSpin, DispatcherRingBusy,
GfxWaitFenceValue, SpinWaitFlagSleep0, GfxFencePollWorker, MeshletRequestWorker_ListSumLoop, JobWaitSleepBackoff,
JobGroupWait, CollisionObjectUpdate, StageObjLodUpdateRange, ObjUpdatePerObject, ObjPoolReuseIdleForTypeIds,
BoundsFromPartObbs, DrawEmit, TextureStreamUpdate, PsoCacheBTreeFind). New rows: `re/symbols/incoming/cpu-burn.tsv`
(drain-spin loop and SN hook site labels).
