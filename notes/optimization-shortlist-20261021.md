# Look-preserving optimization shortlist (2026-10-21)

Desk study only. The game was not launched or touched. Nothing in the build or ini was changed.

Read: `.handoff/untried-ideas.md`, `.handoff/gxl-pc-pause-22241.md`, `notes/untried-optimizations-2026-10-15.md`,
`cpu-hotspots-ally.md`, `cpu-profile.md`, `stutter-pso.md`, `frame-timing.md`, `gpu-maxsettings.md`, `fps-unlock-research.md`,
`pc-keep-candidates.md`, `backlog-small-wins.md`, `grok-research-index.md`, `whats-left.md` (WG/IATS rows), and the
`.handoff/done.md` result rows 2026-10-15 to 2026-10-21 (there is no `board/` directory in the repo; done.md is the result log).

Filter: no resolution, scaler, effect, shadow, GI or quality cuts. No OS or driver tuning (ReBAR, TDP). So GI1-P1..P3, MOTION_BLUR/SSR
downgrades, ESM 1024 and similar from `gpu-maxsettings.md` s.4 are excluded. File-swap shader rewrites are also excluded because
past replacements ran 0.4 to 0.6 fps slower on the Ally.

## Where the evidence stands (so nothing here is re-tested blindly)
- Only 13 of 64 hooks have an OFF/ON number; only CBN gains over 2 percent (pc2 38.3 to 44.3 fps; PC +0.5, Ally -0.2). CBN is already 3/3 devices, not listed.
- Closed or negative: Barriers on PC (-7 percent), PL, B, GL1, LC, AS0, W6/PT1/DL, ESM1, MSV, FO frame overlap, CellSkip, NearArenaOnly, Ally AABB+J1 stack (not KEEP).
- Noise: about 4 percent run to run on PC, about 0.8 percent on Ally in a fixed spot, about 1 fps in real Ally combat. Anything under 2 percent needs ABBA-ABBA.
- Rules from untried-ideas.md: same fight both arms, gameplay gate, verdict needs all 3 devices (or the one valid device, stated).

## Ranked list (most worth doing first; cheapest flagged CHEAP)

### 1. Pacing census of the 1% low from existing PresentMon CSVs (CHEAP, zero game time)
- Source: `untried-optimizations-2026-10-15.md` #3, `perf-opportunities-pc-20261015.md` #3, `stutter.md`.
- Evidence: PC average about 96-99 fps against a 1% low of 44-53 fps. The CPU bundle raised the 1% low 20 percent while the average moved 3.3 percent. This gap is the largest unexplained PC cost; Ally 1% low is 15-18 against 29-30 average.
- Risk to picture: none (analysis only).
- Test plan: run `re/scripts_stutter/pc_spikes.py` over the Ibaraki and Yoritomo CSVs from PC, PC2 and Ally. Label the slowest 1 percent of frames CPU, GPU or WAIT and bucket them against SPR creates, stage loads and streaming. Output decides which of items 2, 3, 6 is worth a launch. Metric: share of spike frames per class per device.
- Devices: all three, from files already on disk.

### 2. PJ/PJH shader warmup, true OFF/ON on a boss route (CHEAP on PC)
- Source: `stutter-pso.md` s.1, s.4; `gxl-pc-pause-22241.md` (pair never run); `untried-ideas.md` #3.
- Evidence: measured prewarm of 15,732 PSOs in 2.7 s, runtime creates 81 to 43, zero creates over 1 ms in 4 launches, but on a route with no hitch to cure. The old pc2 run had 94 frames over 100 ms against 39; 6 of 6 slow gameplay frames were shader compiles (worst 167 ms; boss-kill hitches 102-448 ms). Average fps effect expected about 0; the win is the 0.1 and 1 percent lows. Never A/B'd on any device.
- Risk to picture: none (same descriptors and shaders). Cost: 64 MB journal, one low-priority boot thread.
- Test plan: Fight-now route to the same boss (Yoritomo R7B35), arms PJH=2 vs PJH=1 (journal pass first), cold/warm/off = 3 launches per device minimum, `tools/pj_hitch.py`. Metrics: frames over 50 and over 100 ms, worst frame, p99.9, SPR creates over 1 ms inside the boss window. Pass: at least 50 percent fewer frames over 100 ms and p99 not worse.
- Devices: PC first, PC2 second (lowest hitch tolerance), Ally last (PSO hitches not seen there).

### 3. TG logic tick governor: finish the PC arm, decide on the PC2/Ally split
- Source: `cpu-hotspots-ally.md` s.4, `tg-30hz-locate.md`, `cpu-bound-midrange.md`, `done.md` 2026-10-20 rows.
- Evidence: PC2: fps +2.6 to +3.3 percent (three runs), CPU 66.59 to 46.42 percent (-30.29 percent, 4 of 6 windows accepted, fights not matched). Ally: four valid 60 s intervals, CPU-seconds/s 6.347 off vs 6.485 on (+2.2 percent), no benefit. PC: no both-arm pair (it is on in this PC's ini). The remembered 26 percent is a CPU estimate, not fps.
- Risk to picture: none. Behaviour: simulation step about 1/30 s, a tap must be held about 33 ms, packet about 6 ms older. Needs a user decision for boss fights.
- Test plan: PC ABBA-ABBA, same boss fight with MaxFps 240, arms TG=0 vs TG=1, matched combat window (scripted attack loop), 60 s each after 30 s settle. Metrics: process CPU, fps, 1% low, plus a game-time frame counter on one scripted attack to prove timing unchanged. On Ally, only re-run with a power metric (CPU W, GPU MHz) since fps is GPU-bound.
- Devices: PC (missing), then Ally with watts; PC2 only needs a matched-fight repeat.

### 4. R2a and R1 barrier fixes on the Ally (Proton only, so Ally is the only valid device)
- Source: `untried-optimizations-2026-10-15.md` #8, `gxl-pc-pause-22241.md` ("named, not run"), `vkd3d-audit.md` B4-B6, `done.md` 2026-10-16.
- Evidence: R2a Ally ABBA +4 percent (28.8 to 29.9 fps, 1% low 15.3 to 17.5), GPU-bound 97.5 percent. R2a is forced off on native Windows, so PC and PC2 cannot confirm it. R1 and R2b were never run on the Ally. The R2a+AABB+J1+Barriers stack gave only +0.3 fps, so R2a may be eaten by the stack.
- Risk to picture: low (barrier rewrite, same output; run `shotdiff`). Medium risk of a hang if a barrier is wrongly dropped.
- Test plan: Ally ABBA-ABBA, Ibaraki Doji 1080p, arms R2a alone, R1 alone, R2a+R1, stack OFF; one extra launch with TraceRecords=200000 GPUProf=2 to confirm the hooks fire (all 20 old logs print R2a=0 R1=0). Metrics: fps, 1% low, GPU ms per frame. Keep at +0.1 fps or better in both pairs with shotdiff passing.
- Devices: Ally only. Battery hold applies (done.md 2026-10-20): charger only.

### 5. AABB: close the PC arm, repeat PC2
- Source: `untried-ideas.md` #1, `handoff-codex-cpu-ready-aabb.md`, `done.md` 2026-10-15/17.
- Evidence: PC2 +6.3 percent once, later one 60 Hz pair 45.95 vs 45.87 fps (no gain) and a logic bench -0.71 ms/tick (-6.9 percent scheduler, 1% low 33.5 to 38.0, n=1). Ally neutral to -4 percent (pooled about 0). PC: failed run, no data. Output bit-exact offline (6 of 6 runs, 900k comparisons).
- Risk to picture: none (bit-exact).
- Test plan: PC and PC2 ABBA-ABBA at the abba2 scene, MaxFps 240, arms AABB=0 vs AABB=2 only (no J1). Metrics: Scheduler0 ms per tick from the SP profile, CPU %, fps, 1% low. Keep if scheduler share drops and fps at least 0 with p99 within 2 percent.
- Devices: PC (missing), PC2 (repeat to 2 quads). Ally already neutral; skip.

### 6. Isolate RD, M2S, MW, J1 (and P1E7, DQ) one switch at a time
- Source: `untried-ideas.md` #6, `cpu-next-candidates-2026-10-13.md`, `combined-cpu-trial-2026-10-13.md`, `renderer-drain-wait-candidate-2026-10-13.md`.
- Evidence: only the bundle was measured (PC +3.3 percent fps, 1% low +20 percent, CPU -10 percent; Ally -3 percent fps while CPU -7 points). Targets are large on paper: 418F40 is about 7 percent of main-thread self samples (23-24 percent on PC2, 6 ms/frame); 48E90 is 15.6 percent of its worker. RD and M2S are on in the live ini but not in the build, so what they do now is unknown. The Ally -3 percent suggests one switch there hurts.
- Risk to picture: none; risk is cross-thread ordering (crash or hang), reviewed offline only.
- Test plan: bundle-minus-one ABBA-ABBA on PC first (each of RD, M2S, MW, J1 removed in turn), then Ally for the harmful switch. Metrics: CPU %, fps, 1% low; keep a switch if CPU is at least 3 percent lower or fps at least +1 percent with p99 within 2 percent.
- Devices: PC, then PC2 (recorder fix is a prerequisite per open.md), then Ally for no-harm.

### 7. Ally wake and power items never run: WG, DS, W3_TimeoutMs=3, LK (IATS/RL are README designs only)
- Source: `cpu-hotspots-ally.md` s.5, s.10, TL;DR rows 3-7; `whats-left.md` row 13.
- Evidence: WakeStats 14-16k wakes/s at MAX, 23.5k/s at USER, about 6k wasted, storms of 40-165k/s early in a level. Estimated 0.05-0.15 core steady and 0.3-1 core in storms; every 1 W freed is about 4-5 percent GPU clock (`gpu-maxsettings.md` s.4 row 1) on a GPU-bound handheld. WG is on in the live ini yet "built, never Ally-run". W3_TimeoutMs=3 is an ini-only change (about 0.07 core).
- Risk to picture: none. Risk: a missed wake stalls a worker (WG); watch for hitches.
- Test plan: Ally ABBA at USER, fixed spot, charger on, arms WG=0 vs 2 and W3_TimeoutMs 1 vs 3 (one at a time). MangoHud metrics: CPU W, GPU W, GPU MHz, fps, plus the WG skipped counter and wakes/s. Keep if CPU W drops with fps not lower.
- Devices: Ally; PC2 later as a CPU-bound check for WG.

### 8. KL_Div=4 (back-end segment hand-off) on PC and a stable Ally spot
- Source: `backlog-small-wins.md`, `untried-ideas.md` #5.
- Evidence: PC2 +1.2 to +1.3 percent twice (SE 0.02-0.04). Ally KLA1 inconclusive (scene got heavier mid-run; KL4 -5.9 +- 1.5, KL2 +1.0 +- 3.8). PC never run. Small gain; listed because it is nearly free.
- Risk to picture: none intended (same draws, different hand-off cadence); branch renderer-backend, default off.
- Test plan: PC and Ally ABBA-ABBA, KL_Div=0 vs 4, fixed standing spot, 60 s each. Ally needs at least 4 pairs; reject any pair where GPU ms differs over 1.5 percent between arms.
- Devices: PC (missing), Ally (redo); PC2 done.

### 9. PP page-pool throttle with a texture time-to-sharp check
- Source: `untried-ideas.md` #4, `parallel-drawprep.md`, `pc-keep-candidates.md` s.3 and s.4b.
- Evidence: PagePoolFrameUpdate 2523FC is 2.2-3.0 percent of the PC main thread, 1.27 percent of Ally main. Estimated +1.5 to +2.3 percent if main-thread bound. The old "+3 percent" sweep is not valid (matrix toggles never fired, FG regime changed mid-run, pc-keep-candidates s.4b), so it is effectively unmeasured.
- Risk to picture: low to medium. Occupancy readers see data up to 3 frames stale; textures could sharpen later. Only item here touching streaming state, so it needs the visual gate.
- Test plan: PP_Period=1 vs 4 (Ally 2), ABBA-ABBA at a fixed spot with the PP counter nonzero when ON; then fast-travel twice per arm and compare time-to-sharp with screenshots. Any visible difference = DROP.
- Devices: PC and PC2 first (CPU bound), Ally last (fps-neutral expected, power only).

### 10. PC2 dispatcher critical path (SRW lock 7F6904, UAV barrier 1139D4, ECL ring 86F760)
- Source: `cpu-bound-midrange.md` s.0, `pc-submit-overhead.md`, `untried-optimizations-2026-10-15.md` #4.
- Evidence: PC2 main thread waits on the replay thread (418F40 about 6 ms/frame). Dispatcher is 78 percent busy and about 19 percent blocked: 7.5 percent driver barrier, 6.1 percent shared SRW lock, 5.5 percent ring backpressure. Called the only route to about 50 fps on PC2. PCM memo gave only +0.5 percent.
- Risk to picture: none intended; risk is submission ordering and locks (Barriers on PC measured -7 percent, so fewer calls can cost CPU).
- Test plan: step 1 only: one deep-stack SP profile (Dispatcher focus) at the PC2 spawn to name the SRW holder. Only if one site dominates, ABBA on that site with CPU ms per frame, fps, 1% low.
- Devices: PC2 first; PC dispatcher is 0.5 cores so low value there.

## Not ranked (parked behind the above)
- PB2 meshlet VS uniform-descriptor fast path (`material-pb2-next-opportunities.md`): E 0.3-1.0 ms Ally, but earlier shader replacements lost 0.4-0.6 fps and the Ally rebuild tool is missing.
- Meshlet readback pair (`frame-io-next-20261011.md`) and compute-constant CBVs on PC2 (`rdna2-meshlet-slowdown.md`): high build cost; CBN/CBT may already cover the second.
- LM load-memory sweeper and the MC memcpy size probe (`untried-ideas.md` #7, #9): load-time and 0.47 ms respectively; run when a device is idle.
- Ally RADV `cswave32`: look-identical but a driver/launch option, excluded by the no-driver-tuning rule.

## Cheapest first
1. Item 1 (offline). 2. Item 2 on PC (one route, three launches). 3. PC arms of items 5 and 8 (existing bench scripts, no new code). Items 4 and 7 are cheap per run but Ally-only and charger-only.
Suggested order: 1, 2, 5 (PC), 3 (PC), 6 (PC), 8 (PC), then 4 and 7 on the Ally, then 9, 10.
