# EF per-hook / per-key OFF vs ON table (#104257, gx-pc, 2026-11-15)

Desk study, no code change, no game. Tree: v1.8.18 ship build #7695 (merge 2578d264) + FPW gate (origin/gx-pc/fpw-gate-104135 428b3430), because origin/main (b918b0a9) still carries the v1.8.17 EF (it lacks FileLoadPrio, FileLoadPoll, KidstsCap, FpwDefaultV). Sources: .handoff/next.md, done.md, checkpoints.md, notes/*.md and bridge RESULTs #103799 #104107 #104161 #104197. Key cross-check method: patches.json site groups vs shipped ini keys vs catalog.json settings vs string literals in mod/Nioh3EngineFix/src (script kept on gx-pc at C:/Users/Ahmad Hamza/Temp/fpw104135/keys.py, not in the repo).

Columns: default = shipped Nioh3EngineFix.ini value; ini / cat = key present in the shipped ini / mod/settings/catalog.json; mode = tools/testmatrix/live_keys.json class (live = one-launch LiveCtl A/B, relaunch = one launch per arm); evidence = A/B (key-isolated OFF vs ON on a device), bundle (only measured together with other keys), offline (harness, bench or GPU-ms model), verdict (result recorded, no figure), never, n/a (diagnostic, parameter or crash guard: no OFF/ON number expected). Device names as in the source; pc = Main PC.

## Counts
- Rows: 170 keys ([Patches] + [ExtraPerf] + [GpuOpt] + [Perf] + [Latency] + [VRR] + [Memory]; [Tuning] parameters left out).
- Isolated A/B number: 42. Bundle only: 29. Offline only: 8. Verdict recorded without a number: 5. Never measured: 76. n/a: 10.
- Patch site groups in patches.json: 79 (79); 25 of them have no ini key of their own (switched by another key, listed below).

## Table

| key | what it does | default | ini | cat | mode | evidence | best OFF vs ON number (device, verdict) | source |
|---|---|---|---|---|---|---|---|---|
| Patches.AABB | Character bounding boxes computed in SSE, bit-identical | 1 | Y | Y | live | A/B | Ally fps 35.9 -> 31.6 (n=1, GPU-bound, no gain); pc2 fps 47.84 -> 47.37 (noise, CPU -0.5 core); pc2 AABB+DQ 49.2 -> 49.3 DROP | #37074, #37025, #68394 |
| Patches.AQ | Async archive reads on worker threads | 0 | Y | Y | relaunch | A/B | pc -4.9 (unit not stated), OFF; Ally no A/B | default-on-candidates-20261023.md:75 |
| Patches.AQThreads | AQ async archive reads helper thread count (sub-key of AQ) | 4 | Y | Y | relaunch | never | never measured (only AQ itself: pc -4.9, unit not stated) | default-on-candidates-20261023.md:75 |
| Patches.AS | Opt-in live switch for async-compute slots (GPU effects) | 0 | Y | Y | live | never | never measured (ABAB plan only, no result) | gpu-async-wins.md:132 |
| Patches.AS0 | Keep the extra async-compute effects queue on AMD GPUs | 0 | Y | Y | relaunch | verdict | verdict only (no figure): PC neutral / no gain, stays OFF | pc-keep-candidates.md:114 |
| Patches.B | Draw-list group-length search gallops instead of full binary search | 1 | Y | Y | live | verdict | verdict only: PC -0.1 % (dropped), no A/B detail | hardware-compatibility.md:73, optimization-shortlist-20261021.md:16 |
| Patches.BPO | Caches body-part rotations per key on game-update thread | 1 | Y | Y | live | never | never measured alone (bundle only: Main PC AABB+DQ+RD 118 -> 122 did not include BPO; Ally bench "not run") | done.md:215 |
| Patches.CBF | Per-draw constant ring in system memory, batch copy | 0 | Y | Y | relaunch | offline | offline: PC +0.17 ms GPU; pc2 copy bench +9.6 ms GPU, would lose; OFF | handoff-2026-10-10-1710.md:11 |
| Patches.CBN | Skip unchanged constant-buffer uploads | 1 | Y | Y | relaunch | A/B | with CBT: pc2 fps 38.3 -> 44.3 (+15.9 %) KEEP; Main PC CBN alone 83.2 -> 86.4 fps (+3.8 %, noisy) | #93358, next.md:2620 |
| Patches.CBS | D3D12 replay thread constant-buffer copy path (CBS/CBN/CBT) | 0 | Y | Y | relaunch | never | never measured (only install/log checks) | handoff-codex-step1.md:19 |
| Patches.CBT | CBN/CBT skip redundant constant-buffer uploads | 1 | Y | Y | relaunch | bundle | bundle only (CBN+CBT): pc2 fps 48.5 -> 51.1 (+5.5 %, noise) KEEP; earlier pc2 38.3 -> 44.3 (+15.9 %) | done.md:561, #93358 |
| Patches.CLDR | Cloud history blend weight 0 -> 0.5 | 0 | Y | Y | live | offline | GPU ms PC +0.036 (se 0.025), pc2 -0.086 (se 0.106), noise, DROP | same-look-package.md:16 |
| Patches.CP | Collision workers prefetch the next colliders to cut stalls | 1 | Y | Y | live | bundle | bundle only: CP+SPF+DL Ally fps OFF 29.43 -> ON 29.78 (+1.2%, noise, GPU-bound), keep off | done.md:149 |
| Patches.CpuStats | Count MQ calls and DL skips for the stats line | 0 | Y | Y | relaunch | n/a | n/a (diagnostic counters) | - |
| Patches.DL | Collision update skips a discarded part-map search | 0 | Y | Y | live | bundle | bundle only (CP+SPF+DL): Ally fps 29.43 -> 29.78 (+1.2 %, noise, keep off) | done.md:149, default-on-candidates-20261023.md:63 |
| Patches.DQ | Frame-retry spin re-checks after 200 us, not blind 1 ms | 1 | Y | Y | live | bundle | bundle removal x3 quads: without DQ CPU -4.0 % / +0.7 %, 1% low +1.3 / +0.4 fps -> split, KEEP by rule; lows slightly better without DQ (Main PC 4-core 60 cap, EF d7cb9d69, bundle quad ABBA (A = RD,M2S,MW,J1,P1E7,DQ on; B = this key off)) | #104161, #103934 |
| Patches.DS | Main-loop 1 ms retry spin sleeps on a high-resolution timer | 1 | Y | Y | live | A/B | Main PC 60 cap CPU 5.27 -> 4.98 cores (-5.5 %), 1% low +4.5 % KEEP; Ally 4.1 -> 3.34 cores PASS; PC2 neutral (8.27 = 8.27) | #96036, #96341, #96365 |
| Patches.E1 | Hidden effects updated every E1_N-th pass with scaled time step | 1 | Y | Y | live | A/B | Main PC fps -2.1/-1.7/-1.2/-1.4 % (within noise), Ally ~0, DROP | #205, next.md:4385 |
| Patches.ED | Effect-update workers with empty lists not woken or waited | 1 | Y | Y | live | never | never measured | next.md:2482 |
| Patches.EP | Reuse kernel events for archive reads | -1 | Y | Y | relaunch | A/B | Ally load 98.9 -> 50.1 s KEEP; PC fps 93.3 -> 91.0 (-2.4 %, noise) | loading-ally-fixed.md:493, next.md:2634 |
| Patches.ESM1 | Exponential shadow map at 1024 instead of 2048 | 1 | Y | Y | relaunch | never | never measured alone (only inside pure-perf bundle +29.8 % PC; later closed/neutral) | response-to-nioh3fix-review-3645.md:171, optimization-shortlist-20261021.md:16 |
| Patches.FG | FSR frame-gen presenter sleeps on timer instead of spinning | 1 | Y | Y | live | bundle | bundle only: PC pure-perf vs stock 75.84 -> 98.46 fps (+29.8%, FG off, ~60 keys); FG-on noise | response-to-nioh3fix-review-3645.md:171 |
| Patches.FGC | Guard the frame-gen copy job against a null texture crash | 0 | Y | Y | relaunch | n/a | n/a (crash guard) | - |
| Patches.FGOnWine | Under Wine/Proton frame generation runs only if 1 | 0 | Y | Y | relaunch | never | never measured | settings-audit-plan.md:147 |
| Patches.FOGT | Froxel fog history blend weight 0 -> 0.5 | 0 | Y | Y | live | offline | GPU ms PC +0.023 (se 0.005), pc2 +0.122 (se 0.120); look changed, DROP | same-look-package.md:15 |
| Patches.FPC | Per-frame cache for material parameter lookups | 1 | Y | Y | live | never | never measured (built, A/B pending) | parallel-drawprep.md:18 |
| Patches.FileLoadPoll | File-load loader threads sleep instead of spinning (FPW) | 1 | Y | Y | - | A/B | Main PC 4-core loader 0.30 -> 0.04 cores, game -2%, Continue 69 -> 18 s PASS; PC2 12-core B fps lower (not a pass) | #104047, #103997 |
| Patches.FileLoadPrio | File-load threads BELOW_NORMAL with few CPUs (FLP) | 0 | Y | Y | - | bundle | with KidstsCap (FK) vs A, Main PC 4-core: hitches >=250 ms 77 -> 4, Boss Match load 27.4 -> 12.8 s, walk fps 32 -> 49 PASS (not isolated from KidstsCap) | #103256, #102935 |
| Patches.GF | Wait on a GPU fence event instead of Sleep(0) spin | -1 | Y | Y | live | bundle | bundle only (P arm GF W6 MQ WG J1 LK M1 M2): pc2 7.97 vs 7.71 cores, no CPU cut | #92569 |
| Patches.GIMerge | Merge two GI gather passes, one per stencil light group | 0 | Y | Y | relaunch | A/B | PC GPU-opt screen -10.3 % / -2.9 % (loss), stays off | default-on-candidates-20261023.md:76 |
| Patches.GIMergeQualify | GIMerge qualify threshold (sub-key of GIMerge) | 60 | Y | Y | relaunch | never | never measured | - |
| Patches.GL1 | Prefetch mission-thread task-list entries 3 ahead | 1 | Y | Y | relaunch | verdict | verdict only (no figure): PC neutral to negative, OFF; loads 0 gain | pc-keep-candidates.md:114, loading-bottleneck.md:36 |
| Patches.IK | Read the pad at logic-frame start for fresher input | 1 | Y | Y | live | never | never measured (offline estimate: 4-9 ms input latency cut) | input-latency.md:52 |
| Patches.J1 | Idle job workers poll queues with plain loads | 1 | Y | Y | relaunch | bundle | bundle removal: without J1 CPU +4.0 / +3.3 % -> J1 EARNS, KEEP (Main PC 4-core 60 cap, EF d7cb9d69, bundle quad ABBA (A = RD,M2S,MW,J1,P1E7,DQ on; B = this key off)); Ally AABB+J1 -4 % fps | #103509, done.md:176 |
| Patches.KidstsCap | Size job pools for the CPUs in the affinity mask | 0 | Y | Y | - | bundle | with FileLoadPrio (FK) vs A, Main PC 4-core: hitches >=250 ms 77 -> 4, BM 27.4 -> 12.8 s, walk fps 32 -> 49 PASS; KidstsCap alone worse | #103256, #102935 |
| Patches.L1 | Frame limiter sleeps on high-res timer instead of spinning | 1 | Y | Y | live | never | never measured OFF/ON (only L1_MarginUs tuning: margin 300 FAIL, cap miss 59.71, CPU -1.7%) | #93702 |
| Patches.L1OnWine | Let L1 run under Wine/Proton | 0 | Y | Y | relaunch | bundle | bundle only (DS + L1OnWine=1, DQ=0): Ally 4.12 -> 3.37 cores (-18 %) at 30 cap, power not lower | #96130 |
| Patches.LC | Resource-load managers keep 16 requests in flight, not 10 | 0 | Y | Y | relaunch | never | never measured (written off for loads by analysis) | loading-bottleneck.md:100, pc-keep-candidates.md:114 |
| Patches.LK | 18 locked atomic loads in worker code become plain loads | 1 | Y | Y | relaunch | bundle | bundle only (J1/WG/LK/MW): Main PC fps +2.2% noise, CPU -0.4%, OFF; Takeda MW -> MW+WG+LK 86.11 -> 83.05 fps | #95941, manual-cpu-gpu-2026-10-13.md:211 |
| Patches.LPVR | LPV GI volume update every 1 -> 4 frames | 0 | Y | Y | live | offline | GPU ms PC +0.010 (se 0.006), pc2 -0.058 (se 0.088), noise, DROP | same-look-package.md:13 |
| Patches.LW | Loader Sleep(1) polls wait on an event instead | 1 | Y | Y | relaunch | A/B | PC load 25.3/26.2 s base -> 26.4/25.7 s ON, write-off (no gain) | loading-bottleneck.md:35 |
| Patches.LoadOnlyCpuFixes | Run GF, W6 and RWb only while a level loads | 0 | Y | Y | relaunch | never | never measured (pair still open) | remaining-work.md:14 |
| Patches.M1 | Meshlet workers stop re-summing page-pool capacity | 0 | Y | Y | live | bundle | bundle only (P arm): pc2 7.97 vs 7.71 cores, no gain; OFF | #92569 |
| Patches.M2 | Meshlet workers read page bitmaps with plain loads | 1 | Y | Y | relaunch | A/B | pc2 fps +1.3 %, 1% low -2.4 %, inside a 30 % spread; no gain | response-to-nioh3fix-review-3645.md:161 |
| Patches.M2S | Completion-CAS seed reads only, startup-only | 0 | Y | Y | - | bundle | bundle removal x2 quads: without M2S CPU +5.1 % / -3.6 %, 1% low -8.8 / +0.7 fps -> KEEP (mixed) (Main PC 4-core 60 cap, EF d7cb9d69, bundle quad ABBA (A = RD,M2S,MW,J1,P1E7,DQ on; B = this key off)) | #104107, #103706 |
| Patches.MA | 25 main-thread locked atomic loads become plain loads | 1 | Y | Y | relaunch | A/B | PC run7 vs interpolated base +1.43 % / -2.48 %, noisy, DROP | run7-analysis.md:13 |
| Patches.MPCS | Skip MPC tail in MeshletPreculling (needs mpc shader file) | 0 | Y | Y | relaunch | never | never measured (offline estimate 0.05-0.1 ms on pc2) | meshlet-frontend.md:26 |
| Patches.MQ | Skeleton bone matrices in SSE, bit-identical | -1 | Y | Y | live | A/B | PC fps +0.3..+0.7% with flat gpu_ms, DROP; offline: 2.86 -> 1.35 ns/call | pc-keep-candidates.md:11, streaming-anim.md:144, #92569 |
| Patches.MW | Paired meshlet completion wait | 0 | Y | Y | relaunch | bundle | bundle removal: CPU change under 2 %, fps 59.5 both -> dropped (Main PC 4-core 60 cap, EF d7cb9d69, bundle quad ABBA (A = RD,M2S,MW,J1,P1E7,DQ on; B = this key off)); older J1/WG/LK/MW bundle +2.2 % fps noise | #103799, #103738, #95941 |
| Patches.P12 | Keep 3 extra worker threads on CPUs with <= 12 threads | 0 | Y | Y | relaunch | A/B | no fps gain on Ryzen 5 7600X; Ally screen +36.8 % but s2 -5.3 % (contradictory), off | default-on-candidates-20261023.md:58 |
| Patches.P1E7 | Main thread prefetches next texture's streaming object | 1 | Y | Y | live | bundle | bundle removal: CPU 5.19 (on) vs 5.04 (off) cores, one outlier window, no clear signal (Main PC 4-core 60 cap, EF d7cb9d69, bundle quad ABBA (A = RD,M2S,MW,J1,P1E7,DQ on; B = this key off)) | #104197 |
| Patches.PCM | Memoise pipeline-cache lookups on the replay thread | 1 | Y | Y | live | A/B | pc2 fps 52.7 -> 53.6 (noise), cores 9.33 -> 9.39, OFF; ABBA x4 38.53 -> 38.73 (+0.5 %) | #96035, cpu-bound-midrange.md:101 |
| Patches.PF | Trace-based prefetch of archive reads during loads | 0 | Y | Y | relaunch | A/B | Ally -33.5 (load screen, unit not stated), stays off | default-on-candidates-20261023.md:75 |
| Patches.PFLeadMB | PF prefetch lead size (sub-key of PF) | 768 | Y | Y | relaunch | never | never measured | - |
| Patches.PFMatchPct | PF match threshold (sub-key of PF) | 50 | Y | Y | relaunch | never | never measured | - |
| Patches.PFThreads | PF helper thread count (sub-key of PF) | 4 | Y | Y | relaunch | never | never measured | - |
| Patches.PFTraces | PF trace count (sub-key of PF) | 4 | Y | Y | relaunch | never | never measured | - |
| Patches.PJ | PSO journal + title-screen prewarm | 0 | Y | Y | relaunch | A/B | PC 0 hitches, no change, DROP; pc2 hitches OFF 2-5 vs ON 9, DROP | done.md:486, done.md:491 |
| Patches.PJH | PSO prewarm before first use, replayed from journal | 0 | Y | Y | live | A/B | Main PC frames >33 ms 0 OFF / 0 ON, costs 8 CPU-s at boot, DROP; offline route: runtime PSO creates 81 -> 43 | #96157, stutter-pso.md:133 |
| Patches.PJ_KeepMax | Most prewarmed PSOs kept for the hand-over (sub-key of PJ) | 4096 | Y | Y | relaunch | never | never measured | - |
| Patches.PJ_MaxMB | PJ journal size cap in MB (sub-key of PJ) | 64 | Y | Y | relaunch | never | never measured | - |
| Patches.PJ_QuietMs | Quiet time before PSO prewarm (sub-key of PJ) | 3000 | Y | Y | relaunch | never | never measured | - |
| Patches.PL | Boot pipeline library saved across launches | 0 | Y | Y | relaunch | verdict | verdict only (no figure): PC no frame-time gain, DROP | perf-opportunities-pc-20261015.md:54 |
| Patches.PL_MaxMB | Largest PSO library saved (sub-key of PL) | 1024 | Y | Y | relaunch | never | never measured | - |
| Patches.PP | Streaming page-pool sweep only every PP_Period-th frame | 1 | Y | Y | live | A/B | PC 8.98 -> 8.72 ms (+2.9 %) KEEP; Ally avg fps 11.7 -> 13.7, 1% low 5.0 -> 7.4 (noisy) | catchup-2026-10-21.md:90, done.md:508 |
| Patches.PT1 | Texture tile requests take pool lock once per texture | 1 | Y | Y | live | A/B | PC fps +0.3% with flat gpu_ms (gpu_ms 5.07), below +0.5% bar | pc-keep-candidates.md:114 |
| Patches.R1 | Frame-latency wait uses a 2 ms timeout, not a busy poll | 0 | Y | Y | live | A/B | Ally fps +0.26 % (noise), GPU 98-99 % busy, DROP as perf item | #67028 |
| Patches.RC | Read cache serves small reads from RAM blocks | 0 | Y | Y | relaunch | never | never measured | loading-ally-fixed.md:518 |
| Patches.RCKB | Read-cache block size (sub-key of RC) | 64 | Y | Y | relaunch | never | never measured | loading-ally-fixed.md:415 |
| Patches.RCMB | Read cache size in MB (sub-key of RC) | 64 | Y | Y | relaunch | never | never measured | - |
| Patches.RCMaxReadKB | Largest read served by the read cache (sub-key of RC) | 64 | Y | Y | relaunch | never | never measured | - |
| Patches.RD | Paired renderer-drain wait (not render distance) | 0 | Y | Y | - | bundle | bundle removal: CPU change under 2 %, fps 59.5 both -> dead weight, dropped from bundle (Main PC 4-core 60 cap, EF d7cb9d69, bundle quad ABBA (A = RD,M2S,MW,J1,P1E7,DQ on; B = this key off)) | #103799, #103509 |
| Patches.RPA | RenderParamsAssign copies with 16-byte moves | 1 | Y | Y | live | never | never measured (offline trace: 0.14 ms/frame cost on PC) | done.md:563 |
| Patches.RTP | Raise Windows priority of render-path threads | 0 | Y | Y | live | A/B | pc2 4-core ABBA x2 fps +0.1 % NEUTRAL, keep off | cpu-bound-midrange.md:20 |
| Patches.RWb | Resource-state spin wakes worker once per 256 spins | 1 | Y | Y | relaunch | never | never measured alone (only inside pure-perf bundle) | response-to-nioh3fix-review-3645.md:171 |
| Patches.S1 | PAUSE in the main thread's end-of-frame spin | 1 | Y | Y | relaunch | never | never measured (pc2 MB/SN/S1 A/B tasked #89862, no figure found) | next.md:5995 |
| Patches.SH | Open archive handles synchronous (Wine/Proton only) | 0 | Y | Y | relaunch | A/B | Ally s2 +10 % (noise), superseded by SR, off | default-on-candidates-20261023.md:75 |
| Patches.SKYR | Sky-ambient update frequency 1 -> 4 | 0 | Y | Y | live | A/B | PC GPU +0.016 ms (se 0.004), pc2 +0.097 ms (se 0.105), no saving, DROP | same-look-package.md:14 |
| Patches.SPF | Stage-update worker prefetches placeables 4 ahead | 1 | Y | Y | live | bundle | bundle only (CP+SPF+DL): Ally fps 29.43 -> 29.78 (+1.2 %), noise, keep off | done.md:149 |
| Patches.SPR | Measurement only: time runtime PSO creates | 0 | Y | Y | relaunch | n/a | n/a (measurement key) | - |
| Patches.SR | Read-fix: redirect game file reads, fewer Wine overheads | -1 | Y | Y | relaunch | A/B | Ally load 47.25 -> 38.45 s (ABBA, 1.7.25) KEEP | remaining-work.md:14 |
| Patches.TG | Logic tick cap follows rendered fps (1/30 s floor) | 0 | Y | Y | live | A/B | pc2 CPU 66.59 -> 46.42 % (-30 % relative), fps about +3 %; needs human OK | #16098/#16104, #21911 |
| Patches.TR | Steady timer resolution around pad-thread Sleeps | 0 | Y | Y | live | verdict | verdict only (no figure): listed dropped/neutral | handoff-2026-10-10-0125.md:49 |
| Patches.TS | Timestamp probes, measurement only | 0 | Y | Y | live | n/a | n/a (measurement tool) | - |
| Patches.TSY | Raise the TrySync worker thread count (stock 3) | 0 | Y | Y | relaunch | never | never measured (RE says more threads would not help) | load-request-re-92324.md:21 |
| Patches.VRR | Variable-dt frame timing; real speed below the cap | 1 | Y | Y | relaunch | bundle | bundle only: pure-perf PC fps 75.84 -> 98.46 (+29.8 %, FG off), not isolated | response-to-nioh3fix-review-3645.md:171 |
| Patches.VRROnline | What VRR does in an online/co-op session | -1 | Y | Y | relaunch | never | never measured (install/log checks only, co-op test still needed) | remaining-work.md:70 |
| Patches.VSMA | Fewer redundant virtual-shadow-map tile redraws | 0 | Y | Y | live | offline | offline: PC and pc2 0 % of tiles skipped (never over budget); Ally A/B dropped | same-look-package.md:12, #92137 |
| Patches.W1 | Zero-fill loop on GPU upload memory as rep stosd | 1 | Y | Y | relaunch | bundle | bundle only (pure-perf bundle); profile share 11-22 % main thread | next.md:6185 |
| Patches.W1B | Glyph padding clear uses plain stores on main thread | 1 | Y | Y | live | A/B | pc2 CPU-bound ABBA x3: fps +0.1%, no gain; offline: 120 -> 25 ns per row | cpu-bound-midrange.md:18 |
| Patches.W2 | Quad writer without read-modify-write on upload memory | 1 | Y | Y | relaunch | bundle | bundle only (pure-perf): PC fps 75.84 -> 98.46 (+29.8 %) | response-to-nioh3fix-review-3645.md:171 |
| Patches.W3 | Window message pump sleeps instead of spinning a core | 1 | Y | Y | live | offline | offline profile estimate 0.25-0.6 core saved; no OFF/ON A/B | cpu-profile.md:417 |
| Patches.W3WineSleep | Wine: sleep 1 ms on empty message queue | 1 | Y | Y | relaunch | never | never measured | - |
| Patches.W6 | Present wait sleeps on an event instead of spinning | -1 | Y | Y | live | A/B | Ally: ~0.75 core freed, GPU MHz +37 %; bundle pc2 7.97 vs 7.71 cores, no cut | cpu-hotspots-ally.md:139, #92569 |
| Patches.WG | Wake gate: skip second wake-one while a worker is waking | 1 | Y | Y | live | bundle | bundle only (J1/WG/LK/MW): Main PC fps +2.2% noise, CPU -0.4%, OFF; Takeda MW -> MW+WG+LK 86.11 -> 83.05 fps; Ally never run | #95941, manual-cpu-gpu-2026-10-13.md:211 |
| Patches.WK | Skip worker wake-ups when no job queue has work | 1 | Y | Y | live | never | never measured (Ally counter only: 0.3 % skipped) | worker-power.md:4 |
| Patches.WL | Close previous frame's latency wait handle (leak fix) | 1 | Y | Y | live | never | never measured (leak fix, not perf) | memory-leaks.md:167 |
| Patches.WakeStats | Count job-worker wakes (measurement only) | 0 | Y | Y | live | n/a | n/a (measurement key) | cpu-hotspots-ally.md:25 |
| Patches.WorkerCap | Handhelds only: cap job thread pool size at N | 0 | Y | Y | relaunch | never | never measured | gpu-savings-round2.md:81 |
| Patches.X1 | Pad scan skips XInput slots known to be empty | 0 | Y | Y | live | never | never measured | - |
| ExtraPerf.Enabled | Master switch for look-changing extra-performance options | 1 | Y | Y | relaunch | n/a | n/a (master switch; see Preset / X* rows) | - |
| ExtraPerf.Preset | Preset 0-5 bundles of extra-perf keys (changes the look) | 0 | Y | Y | live | A/B | pc2 fps 49.6 -> 64.3 (+29.5%, Preset 4 Extreme vs baseline, not shippable); Main PC Preset 3 +0.2% | #67701, done.md:1045 |
| ExtraPerf.RenderDistance | Render distance scale 0.5 to 1.0 | 1.00 | Y | Y | - | A/B | Ally fps +0.9 % / +2.5 % (0.5 vs 1.0); PC +0.7 % / +9.7 % (one launch each) | #67659, #67745 |
| ExtraPerf.XAO | Ambient occlusion off (changes the look) | 0 | Y | Y | live | A/B | PC fps +1.8 % +- 0.2 (GPU -0.19 ms); opt-in | extra-performance-settings.md:68 |
| ExtraPerf.XCLD | Cloud ray-march steps 256 -> 128/64/32 | 0 | Y | Y | live | offline | bench: fps -0.2% +- 0.1 (no gain), DROP | extra-performance-settings.md:63 |
| ExtraPerf.XFOG | Volumetric fog grid size, 3 = fog off | 0 | Y | Y | live | A/B | PC XFOG=3 fps +2.8 % +- 0.2, GPU -0.26 ms, glow gone | extra-performance-settings.md:66 |
| ExtraPerf.XFOGS | Fog grid scale 75 % / 50 % per axis | 0 | Y | Y | - | bundle | bundle only: Extreme HQ pc2 fps -3.6 % (GPU +0.67 ms), DROP | #67999 |
| ExtraPerf.XGI | GI/reflection gather resolution scale | 0 | Y | Y | live | never | never measured | - |
| ExtraPerf.XHALF | Half-rate GI, reflection and cloud passes (hidden key) | (none) | N | N | - | A/B | pc2 off 66.5 vs GI-only 65.0 / refl-only 66.0 fps, no speed, OFF; Ally bundle +6.1 % but strobes | #81085, #216 |
| ExtraPerf.XHALF_Force | Force half-rate GI/reflection update (hidden key) | (none) | N | N | - | never | never measured (fix cost ~2 fps pc2 68 -> 66) | next.md:78 |
| ExtraPerf.XHALF_Mask | XHALF pass mask (hidden key) | (none) | N | N | - | never | never measured | - |
| ExtraPerf.XHALF_Period | Frames between XHALF refreshes (hidden key) | (none) | N | N | - | never | never measured | - |
| ExtraPerf.XHALF_Run | XHALF run length (hidden key) | (none) | N | N | - | never | never measured | - |
| ExtraPerf.XLOD | Meshlet LOD scale, coarser far meshes | 0 | Y | Y | live | A/B | Ally fps -0.4%, PC +3.8% not significant (changes look) | #89788 |
| ExtraPerf.XPOST | Motion blur + depth of field off (2 = bloom too) | 0 | Y | Y | live | A/B | PC XPOST=1 +1.8 % +- 0.1 (GPU -0.17 ms); XPOST=2 +6.7 % +- 4 | extra-performance-settings.md:69 |
| ExtraPerf.XPRT | GPU particle groups 100 % to off | 0 | Y | Y | live | never | never measured | - |
| ExtraPerf.XRES | Render resolution scale (boot-time, hidden key) | (none) | N | N | - | A/B | Main PC fps 109 -> 116 (+6%, 0.9 scale) DROP; pc2 50 -> 53 (+7%) | next.md:4490, #68698 |
| ExtraPerf.XSHARP | FSR sharpening strength override | 0 | Y | Y | - | never | never measured | next.md:4464 |
| ExtraPerf.XSHD | Sun shadow distance scale | 0 | Y | Y | live | never | never measured (estimate under 1 ms) | #27737 |
| ExtraPerf.XSHF | Shadow filter taps 4 -> 2 or 1 | 0 | Y | Y | live | never | never measured | - |
| ExtraPerf.XSSR | Cheaper screen-space reflections | 0 | Y | Y | - | bundle | bundle only: pc2 0 % inside the Extreme HQ P5 bundle | #68226 |
| ExtraPerf.XTER | Terrain tessellation distance and rate scale | 0 | Y | Y | live | offline | profile: XTER2 (x0.4) PC GPU -0.06 ms | map-radius-rank-27701.md:15 |
| GpuOpt.AnyCaller | GpuOpt barrier hook on any caller (diagnostic) | 0 | Y | Y | relaunch | never | never measured | - |
| GpuOpt.Barriers | D3D12 barrier batching | 2 | Y | Y | live | A/B | PC Barriers=1 vs 0 -7 % fps, rejected; Ally stack 30.2 -> 30.5 fps, not KEEP | render-architecture.md:122, done.md:166 |
| GpuOpt.CbvMirror | Per-draw constant buffers mirrored to VRAM (no ReBAR) | 0 | Y | Y | relaunch | A/B | CbvMirror=2: GPUBusy p50 +0.9..1.0 ms worse, no win, dropped | root-cause-vram-streaming.md:204 |
| GpuOpt.CbvMirrorAllSites | CBV mirror on all sites (sub-key of CbvMirror) | 0 | Y | Y | relaunch | never | never measured | - |
| GpuOpt.CbvMirrorComputeCopy | Compute-shader copy for the CBV mirror (sub-key) | 1 | Y | Y | relaunch | never | never measured | - |
| GpuOpt.CbvMirrorMB | CbvMirror ring size (sub-key) | 128 | Y | Y | relaunch | never | never measured | - |
| GpuOpt.CbvMirrorQueue | CBV mirror on its own COPY queue with fence waits | 0 | Y | Y | relaunch | A/B | pc2 +15 ms per frame with queue=1 (ini comment, 1.7.52), do not use | Nioh3EngineFix.ini comment |
| GpuOpt.CbvMirrorSites | Which constant buffers the mirror copies (sub-key) | 1 | Y | Y | relaunch | A/B | pc2 mirror copies +10 to +36 ms (loss) | user-settings.md:83 |
| GpuOpt.DeferThreads | Thread names whose D3D12 calls are deferred (Barriers) | SynergisticDispatcher | Y | Y | relaunch | bundle | bundle only: Barriers family PC -7 % fps, keep off | hardware-compatibility.md:87 |
| GpuOpt.DropDuplicateUav | Drop UAV barrier already covered in same batch | 1 | Y | Y | relaunch | bundle | bundle only: Barriers family, Main PC -7% (CPU-bound) | hardware-compatibility.md:87 |
| GpuOpt.DropUndoPairs | Drop A->B, B->A barrier pairs | 0 | Y | Y | relaunch | bundle | bundle only (Barriers family): PC -7 % | hardware-compatibility.md:87 |
| GpuOpt.FrameOverlap | Overlap frame N replay tail/Present with N+1 recording | 0 | Y | Y | relaunch | never | never measured with a number (listed closed: FO) | handoff-codex-step2.md:8, optimization-shortlist-20261021.md:16 |
| GpuOpt.InstallDelaySec | Seconds before GpuOpt hooks install | 20 | Y | Y | relaunch | n/a | n/a (timing parameter) | - |
| GpuOpt.MaxBatch | Maximum barriers per batch (sub-key) | 64 | Y | Y | relaunch | never | never measured | pc-submit-overhead.md:121 |
| GpuOpt.MergeAcrossBinds | State setters do not split a barrier batch | 1 | Y | Y | relaunch | bundle | bundle only: Barriers family PC -7 % fps, keep off | hardware-compatibility.md:87 |
| GpuOpt.R1 | Drops read-to-read barriers (superset states) | 0 | Y | Y | live | bundle | bundle only (R2a+R1): Ally +4% fps once, PC -7%; Proton-only | next.md:4372 |
| GpuOpt.R2a | Proton/vkd3d barrier rewrites (Ally) | 0 | Y | Y | live | A/B | Ally fps +0.08 % (noise), DROP; earlier +4 % (28.8 -> 29.9) did not reproduce | #67028, done.md:163 |
| GpuOpt.R2b | Narrow waits for UAV-buffer barriers (Proton) | 0 | Y | Y | live | never | never measured (never run on Ally) | optimization-shortlist-20261021.md:45 |
| GpuOpt.SceneBufMirror | Mirrors per-frame scene buffers to VRAM on no-ReBAR PCs | 0 | Y | Y | relaunch | A/B | PC2 fps +1.8%, GPU -1.2% -> stays OFF | #96199 |
| GpuOpt.SceneBufMirrorQueue | Queue for the scene-buffer mirror (sub-key) | 1 | Y | Y | relaunch | never | never measured | - |
| GpuOpt.TileBatch | Merge consecutive UpdateTileMappings calls | 0 | Y | Y | relaunch | bundle | bundle only: Barriers family PC -7 % fps, keep off | hardware-compatibility.md:87 |
| GpuOpt.TraceDelaySec | Barrier trace start delay (diagnostic) | 30 | Y | Y | relaunch | n/a | n/a (diagnostic parameter) | - |
| GpuOpt.TraceRecords | Write a barrier trace CSV (diagnostic) | 0 | Y | Y | relaunch | n/a | n/a (diagnostic) | - |
| Perf.VrsCeiling | VRS ceiling (off / 2x2 / 4x4 / selective) | 0 | Y | Y | - | A/B | Main PC fps 109 -> 123 (+13 %, GPU 9.2 -> 7.2 ms); pc2 50 -> 59 (+19 %) but heavy glitter artifacts | #252, #251 |
| Perf.VrsGBufferMotion1x1 | Exclude whole motion-writing MRT draw from VRS | 0 | Y | Y | - | never | never measured | - |
| Perf.VrsKeepCharacters | Characters stay at 1x1 shading under VRS | 1 | Y | Y | - | never | never measured | - |
| Perf.VrsLayouts | VRS mode 3 render-target layout bits | 7 | Y | Y | - | never | never measured | - |
| Perf.VrsRate | VRS mode 3 coarse rate 2x2/1x2/2x1 | 0 | Y | Y | - | never | never measured | - |
| Perf.VrsSkipSkinned | Skinned meshes excluded from coarse shading | 1 | Y | Y | - | never | never measured | - |
| Latency.MaxFrameLatency | Frames the game may queue ahead of the GPU | -1 | Y | Y | live | never | never measured | - |
| Latency.VrrPresent | Fix stuck SyncInterval 1 for independent flip on VRR PCs | 0 | Y | Y | relaunch | never | never measured (estimate only: ~7 ms latency) | plain-summary-2026-10-09.md:55 |
| VRR.Max120 | VRR 120 tier upper limit | 120 | Y | Y | relaunch | never | never measured | - |
| VRR.Max30 | VRR 30 tier upper limit | 30 | Y | Y | relaunch | never | never measured | - |
| VRR.Max60 | VRR 60 tier upper limit | 60 | Y | Y | relaunch | never | never measured | - |
| VRR.MaxFps | Fps cap (120 stock, 144/165/240, 0 unlocked) | 120 | Y | Y | relaunch | n/a | n/a (cap setting, not a hook) | run7-analysis.md:77 |
| VRR.MenuQuantize | Quantize menu hold/repeat timers | 0 | Y | Y | relaunch | never | never measured | - |
| VRR.Min120 | VRR 120 tier lower limit (48 Hz) | 48 | Y | Y | relaunch | never | never measured | response-to-nioh3fix-review-3645.md:28 |
| VRR.Min30 | VRR 30 tier lower limit | 20 | Y | Y | relaunch | never | never measured | - |
| VRR.Min60 | VRR 60 tier lower limit | 24 | Y | Y | relaunch | never | never measured | - |
| VRR.Tier120 | VRR 120 tier on/off | 1 | Y | Y | relaunch | never | never measured | - |
| VRR.Tier30 | VRR 30 tier on/off | 1 | Y | Y | relaunch | never | never measured | - |
| VRR.Tier60 | VRR 60 tier on/off | 1 | Y | Y | relaunch | never | never measured (A/B only proposed) | review-response-nioh3fix-20261029.md:139 |
| Memory.LM | Load-memory sweeper drops dead heap pages | 0 | Y | Y | relaunch | never | never measured (estimate 3-6 s on loads, default off) | remaining-work.md:53 |
| Memory.LM_BudgetUs | Max us a heap lock is held per bin walk (sub-key of LM) | 1000 | Y | Y | relaunch | never | never measured | - |
| Memory.LM_Force | Skip the memory sweeper working-set self-test | 0 | Y | Y | relaunch | never | never measured | load-memory.md:22 |
| Memory.LM_MinKB | LM minimum block (sub-key of LM) | 256 | Y | Y | relaunch | never | never measured | - |
| Memory.LM_PeriodMs | Heap walk period (sub-key of LM) | 1000 | Y | Y | relaunch | never | never measured | - |
| Memory.LM_SliceMB | Memory sweeper slice size (sub-key of LM) | 128 | Y | Y | relaunch | never | never measured | - |

## Key mismatches

### In code, not in the shipped ini or catalog
- ExtraPerf.XHALF: menu writes it (menu.cpp:1701); not in shipped ini or catalog. Users cannot see or reset it from the ini/catalog; add it (default 0 / -1) or document it as hidden.
- ExtraPerf.XRES: menu writes it (menu.cpp:1702); not in shipped ini or catalog. Users cannot see or reset it from the ini/catalog; add it (default 0 / -1) or document it as hidden.
- ExtraPerf.XHALF_Force: read by skipfx.cpp:459; ini/catalog lack it. Users cannot see or reset it from the ini/catalog; add it (default 0 / -1) or document it as hidden.
- ExtraPerf.XHALF_Mask: read by skipfx.cpp:468; ini/catalog lack it. Users cannot see or reset it from the ini/catalog; add it (default 0 / -1) or document it as hidden.
- ExtraPerf.XHALF_Period: read by skipfx.cpp:489; ini/catalog lack it. Users cannot see or reset it from the ini/catalog; add it (default 0 / -1) or document it as hidden.
- ExtraPerf.XHALF_Run: read by skipfx.cpp:492; ini/catalog lack it. Users cannot see or reset it from the ini/catalog; add it (default 0 / -1) or document it as hidden.
- Patches.DSDefaultV: one-time migration marker (DS default 0 -> 1) (intentionally hidden, fine).
- Patches.FpwDefaultV: one-time migration marker (FileLoadPoll 0 -> 1, #104185) (intentionally hidden, fine).
- Patches.JT: alias read of Profiler.JT for the abba2 harness (intentionally hidden, fine).

### In the shipped ini / catalog, not read by code
- none: every shipped ini key in these sections is read (as a literal or through a key table), and ini == catalog for all of them.

### Tested in a branch build, not in the shipped build
- Patches.SN, Patches.MB: ASI #6879 only; Main PC no gain, default OFF (#91640). tools/testmatrix/live_keys.json still lists Patches.SN; the build does not read it.
- Patches.DN: pc2 #144 DN=0 vs 1, CPU heat and fps unchanged (done.md); not in the build.

### In the ship tree (#7695 + FPW gate), not on origin/main (v1.8.17 EF)
- Patches.FileLoadPoll, Patches.FileLoadPrio, Patches.FpwDefaultV, Patches.KidstsCap, Tuning.FileLoadPoll_MaxCpus, Tuning.FileLoadPoll_SleepUs, Tuning.FileLoadPoll_SpinUs, Tuning.FileLoadPoll_YieldUs, Tuning.FileLoadPrio_MaxCpus.

### Patch site groups without their own ini key (switched by)
- AC: General.AutoContinueLoad (hooked by autocont.cpp)
- BN: boot-notice skip (bootnotice.cpp)
- CBU: CBN/CBT (cbvupload.cpp)
- EPF: ExtraPerf (XPRT)
- EPL: ExtraPerf (XLOD)
- GIS: ExtraPerf (XGI)
- XRS: ExtraPerf.XRES
- RCO: SKYR/FOGT/CLDR/LPVR + ExtraPerf
- RDD: ExtraPerf.RenderDistance
- RDP: ExtraPerf.RenderDistance
- RDC: ExtraPerf.RenderDistance
- RDF: ExtraPerf.RenderDistance
- FGP: retired (never installed)
- FP: FnProbe (fnprobe.cpp)
- GP: Profiler GpuProf / markers
- LG: LoadGates probe (loadstats.cpp)
- MLP: MeshletProbe (mlprobe.cpp)
- PM: Profiler pmode log
- RP: ReadProbe / EP (readprobe.cpp)
- SBM: GpuOpt.SceneBufMirror (scenemirror.cpp)
- VP: KeepRunningUnfocused (vpad.cpp)
- VRRMENU: VRR + VRR.MenuQuantize
- W6W: W6
- WC: WorkerCap / KidstsCap
- WS: WakeStats or WG

### live_keys.json entries that are LiveCtl aliases, not ini keys
- VRR.Enabled, Tuning.W3_UseSleep, Tuning.FO, Tuning.RPF, Tuning.CBS/CBN/CBT/CBMODE/CBF/CBF_QUEUE/CBS_FENCE/CBALL/CBSTATS: no ini key of that name in the build (live switch names).

## Caveats
- Numbers were collected by grep over the notes; a key whose only result sits in an unindexed bridge message may show "never measured". Short names (B, S1, TR, PP) were checked by hand for false hits.
- "bundle" numbers do not isolate the key; most of the 1.7.x pure-perf bundle (+29.8 % PC, #3645 review) is in that column.
