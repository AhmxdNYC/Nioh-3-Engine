# PC keep candidates: B, PP, GF, MQ (+ Barriers) - mechanism, overlap, safety, repeat test, recommended inis

Analysis only (2026-10-08). Inputs: `captures\tm\results_pc_1716.md`, `tools\testmatrix\results\pc_*.json|log`, notes `mainthread-drawprep.md` (B), `parallel-drawprep.md` (PP),
`cpu-hotspots-ally.md` (GF), `streaming-anim.md` (MQ), `gpu-barriers.md`. Nothing run, no src touched.

## 0. Read this first: what the matrix actually says
1. **"B" is not barrier batching.** Matrix B = the galloping group-length search at 3BC17 (draw-prep, main thread). Barrier batching is the row **Barriers**
   (gpuopt, `[GpuOpt] Barriers`): it measured **-7.1 % fps, p99 +2.4 %, cpu_ms 5.40 -> 5.76, gpu_ms 5.01 -> 4.98** (the hook costs dispatcher CPU; the PC is CPU-bound at 180 fps).
2. **B's +15.6 % is not credible as a B effect.** Mechanism cap: 3BC0C is 1.2-1.4 % of the main thread and the gallop saves 0.5-1 % of that thread (note estimate), so about +0.5 % fps.
   Observed cpu_ms 5.40 -> 4.63 (-14 %) and **gpu_ms 5.01 -> 4.22 (-16 %)**. A CPU patch cannot change PresentMon `MsGPUTime`; the GPU workload itself changed
   (view/scene/state drift after the menu key sequences). PP shows the same: gpu_ms 5.01 -> 4.59 (-8 %). Items whose gpu_ms stayed flat (MQ 5.04, PT1 5.07, DL 5.06) sit at +0.3..+0.7 %.
3. **The p99 gains of GF (-11.7 %) and MQ (-11.5 %) are one odd window each**: stand1 p99 10.42 / 10.28 ms, stand2 13.30 / 13.51 (BASE-live is 13.4). fps of both windows is equal. Not repeatable evidence.
4. **No proof the switches were on**: the shared mod log shows `PP scans 0 skipped 0`, `W6 waits 0`, `MQ calls 0` in every 30 s line through the W6/B/PP windows (MQ needs `CpuStats=1`, absent from
   the 1.7.16 ini). The per-item logs hold only `stats` lines, no `MENU: <label> -> ON` lines. The toggles are unverified for every live item.
5. Within a launch the two stand windows agree to <= 0.3 fps (0.15 %), so in-launch random noise is ~0.2 %. The large spreads are (a) launch-to-launch (~5 %: BASE 144.7 / 144.5 / 152.5, and
   BASE-live 179.8 is +24 % over the launch BASEs, i.e. a different launch state) and (b) item-to-item state drift inside the live batch (the confound above). The 5 % rule applies only across launches.

## 1. Mechanism per candidate (PC = 7600X + RX 9070, ~180 fps stand, cpu_ms ~5.4 vs gpu_ms ~5.0: CPU-bound-ish, main thread critical)
| id | thread | work removed | expected PC fps | confidence |
|---|---|---|---|---|
| B (gallop) | [App]Game draw prep (39E20 -> 3BC0C skip-group path) | binary search of ~11-15 dependent loads per skipped group becomes a gallop from the group start (~1+2 log2 g probes). Output identical for any sorted list (24,000 random tests vs original bytes, 0 diffs). Up to ~1.5x worse on that function for groups in the hundreds | +0.3..+1 % | high on size, none for +15 % |
| PP | [App]Game, render-frame begin (7D9B6C -> PagePoolFrameUpdate 252510 -> 2523FC) | full walk of the streaming page-pool table (bucket x way x segment, 64 B entries) every frame becomes every 4th frame (`PP_Period=4`); skipped frames republish the last measurement. 2523FC = 2.2-3.0 % of the main thread | +1.5..+2.3 % if main-thread bound | medium (observed +5.3 % includes drift, gpu_ms -8 %) |
| GF | [App]Game (FenceWait 7D9E1C / 7D9E66) + fence-poll worker 117D9F | `Sleep(0)` yield loop (a full-core spin: 58-66 % of main-thread samples on the Ally) becomes `SetEventOnCompletion` + `WaitForSingleObject`. Removes no frame work; frees a hardware thread and power, adds ~50-100 us wake latency | ~0 % (+-1 %); CPU power/boost better; p99 neutral to slightly worse at 200 fps | high on mechanism, low on PC gain |
| MQ | KIDSTaskScheduler workers (MatrixFromScaleQuatTrans 88780, 1.7 % of each) | scalar becomes SSE, body 2.86 -> 1.35 ns/call, bit-exact | <= +0.5 % (workers are off the critical path unless starved) | high |
| Barriers (real batching) | dispatcher thread (SynergisticDispatcher) + GPU | 1220 -> 434 ResourceBarrier API calls/frame; GPU idle -0.2..-0.3 ms only if GPU-bound | measured -7 % at CPU-bound 180 fps | measured negative |

## 2. Overlap / combined gain / ordering
- **B + PP: same thread, additive in milliseconds** (~0.5-1 % + 1.6-2.3 % of the main thread). In fps (CPU-bound): **+2..+3 %**, not the sum of the printed deltas (+15.6, +5.3).
- **GF is orthogonal in ms but shares a resource with W6.** Both sit in the same wait path (W6 = Present-flag wait 7D9EA2 inside 7D9E90, GF = fence wait 7D9E66). They turn spins into waits and free the same
  core, so do not add their gains. Effect order: GF/W6 define what the main thread does while waiting, B/PP shorten the working part. If the main thread paces the frame, GF/W6 can only lose by wake
  latency, so test them with B+PP on as well as alone.
- **MQ** is independent (workers); it adds only if a worker is on the critical path. Expect ~0.
- **Expected combined (B+PP+MQ+GF+W6) on the PC: +2..+4 % fps** (inside the 5 % launch noise, hence the paired in-launch design below); p99 about unchanged.
- **Interaction to check**: PP is the only one touching streaming state; run its visual gate with the full set on.
- Barriers stays out of the combination until its CPU cost is understood (about +0.36 ms CPU/frame on the PC).

## 3. Safety as a default
| id | PC | Ally (Proton / vkd3d-proton, 16-60 fps, GPU-bound 99 %) |
|---|---|---|
| B | Low risk, bit-tested; relies on the type-3 list being sorted (the original binary search does too). v2.02 only (pattern + deps checked). Default after the repeat test. | fps-neutral (GPU-bound), tiny power gain, same x86 code. Low value: leave OFF until the PC confirms. |
| PP | Low-medium: occupancy readers see data <= 3 frames stale (50 ms at 60 fps vs a 300 ms mip-bias cooldown); "streaming settled" machine reacts later; the `pool+B28h==0` branch is unanalysed (falls back to stock). Default only if the texture-sharpness gate passes. | At 16-30 fps 4 frames = 130-250 ms, about the 300 ms cooldown: the staleness argument weakens. fps-neutral. Use `PP_Period=2` or keep OFF. |
| GF | Low: on any failure the stock Sleep(0) round runs (counted) and the engine re-checks the fence after each return. Keep only if A/B shows >= 0 %; its win is CPU power, not fps. | **KEEP ON** (shipped since 1.4.0): removes ~0.6 core of spin, GPU clock/power budget up, fps flat as predicted. Proton's SetEventOnCompletion is a thin eventfd. |
| MQ | Low, bit-exact (2.75 M inputs, 5 MXCSR modes, aliasing fallback). Can only help; default. | 0 fps, small power. Include (same code). |
| Barriers | Medium: measured -7 %. Default OFF. | RADV already ORs consecutive barriers, so GPU gain ~0-0.1 ms; ~790 fewer vkd3d translations/frame (watts). Keep `Barriers = 2` (installed, off) for the R2a/R1 trace work. OFF. |

## 4. Repeat-test design (next PC turn)
Goal: decide KEEP for B, PP, GF, MQ and the combined "PC rec" without the state-drift confound.

**Prereqs:** `CpuStats = 1` (MQ calls); ASI 1.7.17 (or 1.7.16); `fullmod_ini_1716.ini` as baseline. Every toggle must be read back as `MENU: <label> -> ON|OFF` and the line stored in the result log
(`Invoke-MenuToggle` already checks it; persist the matched lines). An item whose counter stays 0 while ON is INVALID.

**Block A - in-process paired A/B (one launch, ~65 min), per item X in {MQ, PP, B, GF} in that order** (cheap/safe first; GF last because it changes wait/power state):
- All four OFF at start; camera parked, menu closed; 60 s warm-up. Stand windows only, **30 s** PresentMon each; no `move` window between pairs (it moves the camera).
- Order per item (8 windows): `off on on off off on on off`. Each toggle = F10 key sequence, then 5 s settle, then the window. Between items reset all to OFF and take one extra OFF window (drift sentinel).
- Record per window: fps mean, p50, p99, cpu_ms, gpu_ms, and the item's counter delta from the 30 s stats line (PP scans:skipped about 1:3; GF waits/s > 0; MQ calls/s > 0; B has no counter, so also SP-profile 3BC0C self ON vs OFF).
- **Validity gates** (pair rejected and item re-run if any fails): (a) gpu_ms ON vs OFF within 1.5 % (same scene; this is what B and PP failed); (b) counter nonzero when ON, zero when OFF;
  (c) OFF sentinels within 0.5 % of each other; (d) p99 is judged on the median of the 4 ON vs 4 OFF windows, never on one window.
- **Verdict:** delta = median(ON) - median(OFF). KEEP: delta fps >= +1 %, same sign in both ABBA halves, median p99 not worse by > 2 %. DROP: delta <= -1 % or p99 worse by > 4 %. Else NEUTRAL
  (ship B and MQ anyway if free; GF for power; hold PP unless its visual gate passes). 1 % is justified because in-launch noise is ~0.2 %; the 5 % launch noise is not used here.

**Block B - cross-launch confirmation of the combination (3 + 3 launches, ~2 h):** interleave `BASE, REC, BASE, REC, BASE, REC` (REC = B+PP+MQ+GF+W6, CpuStats=1), stand 2 x 20 s + move 20 s,
same save/spot, 60 s warm-up (`final-benchmark-protocol.md`). Accept REC if the paired difference (REC_i - BASE_i, neighbouring launches) is positive in 3/3 pairs and the mean exceeds
max(2 %, 1.5 x the BASE paired spread). With the expected +2..+4 % this may end NEUTRAL; then keep REC on the strength of Block A plus lower CPU watts.

**Mechanism proof (cheap, independent of fps):** SP profile 60 s ON vs OFF: 2523FC self down ~75 % (PP), 3BC0C self down (B), 88780 self down ~50 % (MQ), main-thread `Sleep(0)` share ~0 (GF).
**Visual gate (PP):** fast-travel twice, 20 s time-to-sharp textures ON vs OFF screenshot compare; any visible difference = DROP.
**Barriers (optional, separate):** same ABBA at a GPU-bound setting (render scale up so gpu_ms > cpu_ms); keep only if positive there and not negative at 180 fps.

## 5. Recommended inis (keys exist in 1.7.16/1.7.17)
**PC recommended (provisional until Block A/B):**
```
[Patches]  W1=1 W2=1 VRR=1 W3=1 R1=1 L1=1 WK=1 FG=1 WL=1 W6=1 GF=1 B=1 PP=1 MQ=1 PT1=0 DL=0
           AS0=0 FPC=0 AABB=0 M1=0 M2=0 J1=0 X1=0 E1=0 S1=0 IK=0 TS=0 ED=0 WG=0 TG=0 GL1=0 LC=0 LW=0 CpuStats=0 RWb=0 SPR=0
[Tuning]   PP_Period=4 GF_TimeoutMs=100 W6_SpinUs=50 W6_TimeoutMs=1
[GpuOpt]   Barriers=0 (measured -7 %) TileBatch=0 R2a=0 R2b=0 R1=0
[Latency]  MaxFrameLatency=0
[Profiler] all 0 in normal play (MemStats/SP/LoadStats/LoadGates only for test runs)
```
PT1 / DL: +0.3 / +0.4 % with flat gpu_ms, exact and low risk; enable if Block A shows >= +0.5 %. GL1/LC/LW: neutral to negative, OFF. AS0: no gain, OFF.

**Ally recommended (GPU-bound 99 %, Proton; CPU fixes are fps-neutral and buy watts, which become GPU clock):**
```
[Patches]  W1=1 W2=1 VRR=1 W3=1 W3WineSleep=1 R1=1 L1=1 L1OnWine=0 WK=1 FG=1 FGOnWine=0 WL=1 W6=1 GF=1 MQ=1
           B=0 PP=0 (or PP=1 with PP_Period=2) AS0=0, everything else 0
[Tuning]   GF_TimeoutMs=100 W6_SpinUs=50 W6_TimeoutMs=1 W3_TimeoutMs=3
[GpuOpt]   Barriers=2 (installed, switched off) TileBatch=0 R2a=0 R2b=0 R1=0
[Latency]  MaxFrameLatency=1
[Profiler] MemStats=0 SP=0 GPUProf=0 in normal play
[ShaderSwap] Enabled=0 unless a post-v1 swap is Ally-proven
Shaders:   GHDM ON (0x0104840c, +8.7 % at MAX, Ally-proven). ESM / WDO#28 / MPC#23 / toonedge2 only where an Ally A/B showed a win
           (pack: GPU W 23.1 -> 20.5 at equal fps). GI #121/#414 and fog/VCLOUD rewrites OFF (Ally 1.7.5 GP2: all replacements 0.4-0.6 fps slower; RGA does not predict RADV/ACO).
```
Rule behind both: on the Ally, shaders/GPU passes ship only where the Ally itself measured a win; CPU patches ship for watts when exact and failure-safe (GF, W6, W3, MQ) and are held when the benefit is unmeasurable (B, PP).
