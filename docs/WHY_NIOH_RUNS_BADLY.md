# Why Nioh 3 performs badly, and what the mod does about it

Plain-language summary of everything measured so far. Every number below is quoted from a repository note (named in the
"Source" column or line); nothing here is new measurement. "Not measured" means no note has a number. Gains are fps unless
stated. SE is the standard error of paired A/B runs. Evidence quality varies: single pairs are marked.

> Public copy: the "Source" names refer to the maintainers' notes. The ones that are published live in
> [`research/`](../research/) (see `research/SOURCE_MAP.tsv`); the rest are private working notes. Rows marked
> *historical claim* quote an older note whose number was later retracted; [BENCHMARKS.md](BENCHMARKS.md) and the
> final bench in section 2 are the numbers to use.

Devices:

- **PC**: Ryzen 5 7600X + RX 9070, Windows 11, 1440p (XeSS input -> OptiScaler FSR4).
- **PC2**: Ryzen 5 5600X + RX 6650 XT, Windows, 1080p. Its GPU sits behind the B550 chipset at about 0.8 GB/s CPU<->GPU, so it is a slow-link test machine, not a typical mid-range PC (STATUS.md, 2026-10-10 entry #1736).
- **ROG Ally (X)**: Z1 Extreme + Radeon 780M, Bazzite (Linux) + Proton Experimental + vkd3d-proton, 25 W shared CPU/GPU power (notes/ally-report-2026-10-06.md).

## 1. Why it runs badly (the short version)

1. **The Ally is GPU- and power-bound.** GPU busy 87-98 %, socket power pinned at the 25 W limit, GPU clocks throttled; the main thread is about 96 % busy but its savings do not become fps because every CPU watt saved goes to the GPU instead (ally-report-2026-10-06.md; ally-gpu-budget.md). Reaching 60 fps at MAX is estimated 3.5-4.5x away on the GPU, and all same-look items together are worth roughly 5-12 ms (gpu-maxsettings.md s0; gpu-savings-round2.md).
2. **The biggest single GPU cost is a setting, not a bug.** GI = HIGHEST sets all four probe-GI gather scales to 1.0: about 2.5 ms on the PC vs 0.71 ms at HIGH, about +11-14 ms on the Ally (gpu-maxsettings.md s0).
3. **On the PC at 1440p with frame generation, it is GPU-bound** (GPU busy 90-92 %, real rate about 106 fps before FG), so CPU fixes are fps-neutral there (session-2026-10-08-perf.md s0, s4). At lower settings or on weaker CPUs the engine is main-thread/dispatcher bound.
4. **PC2 is CPU-bound by a render hand-off wall.** The main thread spends about 6.1 ms per frame (23-24 % of its time) spinning at frame end until the D3D12 replay (dispatcher) thread has drained; the dispatcher is 78 % busy. Main-thread savings are absorbed by that wait (W1B +0.1 %) (cpu-bound-midrange.md s0, s2).
5. **A wasteful job system.** About 140 worker threads wake up to 10k times per second, 3-5 cores of activity, the largest power sink on the Ally (ally-report-2026-10-06.md). On the PC the process uses 8.6-9.4 logical cores while mostly spinning (session-2026-10-08-perf.md s1).
6. **Hitches are first-use PSO (pipeline) compiles**, not memory or streaming: 247 runtime PSO creates in one session, 25 of at least 1 ms, worst 153 ms; boss-kill frames of 98-167 ms (session-2026-10-08-perf.md s0.2; stutter-pso.md s1).
7. **Loads are slow and mostly not I/O.** Load windows are dominated by waiting on engine readiness gates after the resource managers have drained (about 60 % of the 25.8 s PC window), not by disk, decompression or PSO work (loading-bottleneck.md s2). The Ally is slower mainly because Proton makes small I/O and event operations far more expensive (see s3).
8. **Windows presentation can waste frames.** On some launches the game presents with SyncInterval 1, turning VRR independent flip into composed flip: about +7 ms display latency and 8-20 % of presented frames never shown (vrr-present.md; session-2026-10-08-perf.md s0.4).

## 2. Final benchmark (default stack vs mod), 2026-10-24

Source: docs/final-bench/report.md and README.md. Yoritomo fight, two chunks per arm. "Default" = stock, "Pure-perf" = same-picture optimizations only, "Full" = optimized visuals.

| Device / OS | FG | Default | Pure-perf | Full |
|---|---|---|---|---|
| PC, Windows | off | 75.84 | 98.46 | 98.78 |
| PC, Windows | on | 141.42 | 144.41 | 151.58 |
| PC2, Windows | off | 41.72 | 46.87 | 47.32 |
| PC2, Windows | on | 77.07 | 73.59 | 74.30 |
| Ally, Linux/Proton | off | 34.63 | 32.58 | 39.02 |
| Ally, Linux/Proton | on (displayed) | 42.06 | 44.65 | 52.38 |

Caveats stated in the source: PC2 with FG on is better in stock than in both mod arms; the Ally chunks vary widely (for example FG-off Ally full chunks 33.06 and 44.97), so the Ally rows are noisy; the Ally "stock" arm is the mod loaded with every optimization switch off; the real pre-FG Ally rate is about half the displayed number. 1% low (stock / pure-perf / full): PC FG off 60.52 / 74.62 / 73.32; PC2 FG off 29.18 / 33.44 / 33.63; Ally FG off 29.79 / 26.44 / 24.39.

## 3. Windows vs Linux (Proton/Bazzite): what differs

- **Proton makes small kernel-object and I/O operations expensive.** CreateEvent+CloseHandle pair: 0.57 us on Windows vs 84 us under Proton; 1 KB overlapped read with per-read event: 4.0 us vs about 55 us (pc-vs-ally-load.md s1). That is why the loading fixes EP (pooled events) and SR (sync reads) are on automatically under Proton and are not needed on Windows (predicted gain on PC: at most 0.3 s of one thread, about 0 s wall).
- **Window-message thread spins under Wine** (returns after about 40 us, about 24k wakes/s with a wineserver round trip each). Fixed by W3WineSleep (ally-report-2026-10-06.md).
- **Frame generation crashes under Proton** from a game bug (a missing texture check in the FidelityFX copy; vkd3d-proton #2845). The mod has a guard (FGC) and a "safe FG" toggle that uses the engine's own FG pause (fg-proton-fix.md; written as not run in game at the time of that note, the Ally FG-on result in s2 is later).
- **Some fixes are Windows-only**: VrrPresent (ignored under Proton), barrier batching (no-op on RADV), the R2a/R2b barrier items are Wine-only (backlog and gpu-barriers notes).
- **Some shader rewrites that help on one GPU hurt on the other**: GI and fog rewrites lost on RADV/ACO (Ally) while GI was neutral on PC (s4).
- **Same zip for both**; settings that differ by OS are -1 (auto) in the ini (mod/Nioh3EngineFix/README.md).

## 4. Optimizations tried

Verdict key: **Worked** = measured gain of at least about 2 % fps, or large load/CPU-power gain, on at least one device. **Small** = real but at most about 1-2 %. **Failed** = measured neutral or negative. **Maybe** = built, not yet measured or inconclusive. Device and OS in the third column (Win = Windows, Lin = Linux/Proton).

### 4.1 CPU, scheduling, power

| Optimization | Verdict | Measured number, device, OS | Source |
|---|---|---|---|
| W6: main thread waits for Present with WaitOnAddress instead of Sleep(0) spin | Worked | Ally Lin, FG off: main thread 98 % -> 23 % CPU, GPU clock 1064 -> 1450 MHz, +7-12 % fps (*historical claim*, retracted in BENCHMARKS.md) | mod README |
| W3 message-thread sleep / W3WineSleep | Worked (power) | Fixes a 26-62 % core spin; Wine variant removes about 24k wakes/s. No fps number recorded | mod README; ally-report-2026-10-06.md |
| W1/W2 zero-fill and quad writer no longer read-modify-write on upload memory | Worked (CPU) | Was 11-22 % of main thread. No separate fps number. W1B glyph-padding variant on PC2 Win: +0.1 % fps | mod README; cpu-bound-midrange.md |
| R1, L1, FG presenter sleeps | Worked (power) | FG presenter about 0.2 core less, no fps change (PC) | mod README |
| P12 (keep worker pools at 10 and 7 threads on 6-core/12-thread CPUs) | Small/unknown | No A/B number in the notes read | mod README |
| VRR variable-dt timing | Worked (correctness) | Below the cap, stock slows the whole game (46 fps at a 60 cap = 77 % speed); VRR keeps 1.00x. Not a speed-up of fps | mod README |
| Constant-buffer upload fix (CBMODE 0 -> 6; CBN/CBT/CBS) | Worked on PC2 only | PC2 Win +16.50 % +/-0.16 SE (earlier CBN +15.9 %); PC +0.55 % +/-0.46 (neutral); Ally Lin +0.19 % +/-0.19 (neutral). PC2 gain is specific to its 0.8 GB/s GPU link | README.md; STATUS.md 2026-10 entries |
| PP page-pool sweep every 4th frame | Small | PC Win +2.9 % (*historical claim*, retracted in BENCHMARKS.md); Ally not run | mod README (Which settings); .handoff/untried-ideas.md |
| B draw-list search, GF, MQ | Failed | PC Win: "gave nothing" | mod README; perf-opportunities-pc-20261015.md |
| TG (logic ticks follow render rate; behaviour change, opt-in) | Small, behaviour change | PC2 Win +2.6 % / +3.1 % / +3.3 % in three ABBA sets (SE 0.03-0.1) | cpu-bound-midrange.md |
| RTP (render threads above normal) | Failed | PC2 Win +0.1 % | cpu-bound-midrange.md |
| PCM (PSO cache memo on dispatcher) | Small | PC2 Win +0.5 % (SE 0.03), 98.3 % hits; default off | cpu-bound-midrange.md |
| Barrier batching (`GpuOpt Barriers=1`) | Failed | PC Win -7 % measured when CPU-bound; GPU-bound model 0.2-0.3 ms; stays off | gpu-barriers.md; .handoff/untried-ideas.md; perf-opportunities (not pursued) |
| Frame overlap FO | Failed (NO-GO) | PC2 Win +0.4 % (SE 0.02), PC Win +0.5 %. Emulator A/B with lag emulation +11.74 % +/-1.54 SE is "emulator result only, not native". fo10 hung on 20th transition | backlog-small-wins.md; STATUS.md |
| KL_Div (segment hand-off every 16 draws) | Maybe | PC2 Win +1.2-1.3 % twice; Ally Lin inconclusive (-5.9 % +/-1.5 at KL 4, +1.0 % +/-3.8 at KL 2) | backlog-small-wins.md |
| Combined CPU bundle (RD, M2S, J1, WG, LK, MW) | Small | PC Win Ibaraki, 4 arms: 96.3 -> 99.5 fps, 1% low 44.6 -> 53.4, CPU 7.06 -> 6.34 cores (arm noise about 4 %). Fights: Yoritomo 86.01 -> 87.45 fps, CPU 71.58 % -> 69.49 %; Ibaraki 93.50 -> 94.24 fps, CPU 57.41 % -> 55.41 % (descriptive, not controlled). PC2 pair: -3.97 % CPU, +0.21 % fps, later marked INVALID | perf-opportunities-pc-20261015.md; combined-cpu-trial-2026-10-13.md; cpu-ab-pc2-3578.md |
| WG/CP/SPF/LK/ED on Ally | Worked (power only) | Ally Lin: fps-neutral, saves CPU power | mod README |
| X1, E1, S1, M1/M2, AABB, DQ, P1E7, JT/JG/BPO | Maybe | AABB: best estimate 0.5-0.6 ms per logic tick, one PC2 pair 0.71 ms, "not KEEP". BPO 0.12-0.25 ms E. DQ/P1E7 listed as dropped. M1/full M2 excluded for correctness gaps | .handoff/checkpoints.md 2026-10-28 11:08; perf-opportunities-pc-20261015.md; untried-ideas.md |
| Async compute slot 0 (AS) | Maybe/Small | Offline harness on RX 9070: 0.133 ms saved (about +1-1.5 %); costs +0.19 ms when CPU-bound; slots 11/12 net loss | gpu-async-wins.md |
| Skip OutputDebugString, timer resolution (TR), SkipDebugString | Worked (cleanup) | OutputDebugStringA 91/s -> 0.1/s (Ally); TR neutral on PC | backlog-small-wins.md |

### 4.2 GPU shaders and rendering (same picture)

| Optimization | Verdict | Measured number, device, OS | Source |
|---|---|---|---|
| GHDM depth-reduction shader replacement | Small (original figures retracted) | *Historical claim:* original A/B: +3 % fps no FG and about +11 % with FG on PC Win; README: +6.4 to +8.7 % on Ally Lin. Later fastab re-check: +0.5 % Ally (USER preset), +0.4 % PC no FG. Mixed evidence | gpu-shader-replacement.md; windows-gpu-plan.md; mod README; backlog-small-wins.md |
| fogpar (VFSCAT early-exit only) | Small | Ally Lin +0.15 to +0.30 fps. The full fog rewrite was 0.33-0.54 ms SLOWER on the Ally | mod README; gpu-fog-rewrite.md |
| GI gather rewrite (RefProbe #121) | Failed on Linux | Dropped on RADV; neutral on PC | ally-replacement-ab.md; gpu-rewrite-round3.md |
| VCLOUD cheaper cloud shader | Small | Ally Lin +1.1 %; -1.2 ms async work but frame neutral on PC2 Win | backlog-small-wins.md; gpu-scheduling-sync.md |
| lodu (MshLod#2 fast path) | Failed (neutral) | Ally 0.0; GP2 0.087 -> 0.084 ms; kept separate, not shipped | backlog-small-wins.md; optimization-retention-and-ally-playtest.md |
| mpc / mshlod culling shaders | Failed | PC2 -0.01 ms | backlog-small-wins.md |
| ESM1 shadow 2048 -> 1024 | Failed | PC2 0.00 | backlog-small-wins.md |
| POSTEFFECT_RESOLUTION STANDARD | Failed | PC2 neutral | backlog-small-wins.md |
| Update-rate knobs (LPVR/CLDR/SKYR/FOGT) | Failed | Neutral; FOGT visible | backlog-small-wins.md |
| VSMA shadow-tile cap | Maybe | 0 tiles to cap in tested scenes | backlog-small-wins.md |
| RippleDecal pass skip | Failed | The claimed 1.92 ms was an artifact; real cost about 0.55 ms | ally-ripple-decal.md |
| FSR3 fp16 rebuild | Not useful on PC | PC uses OptiScaler FSR4, game FSR3 never runs | windows-gpu-plan.md |
| FG-off dead passes | None found | No FG work runs with FG off | gpu-copies-postres.md |

### 4.3 Load times

| Optimization | Verdict | Measured number, device, OS | Source |
|---|---|---|---|
| EP (pooled events) + SR (sync reads) | Worked on Linux | Ally Lin loads about 94 s -> 37-38 s; automatic under Proton. Not needed on Windows | mod README; pc-vs-ally-load.md |
| LC / LW / GL1 / ZI / PSO pre-creation / I/O tuning | Failed on PC | PC Win load window unchanged (25.3-26.4 s vs 25.8): managers finish 12+ s before the window ends | loading-bottleneck.md s1, s2, s4 |
| Cross-world "Fight now" prewarm (CrossWorldPrewarm=2) | Worked if the menu is open long enough | PC2 Win: 27 s of menu dwell -> cover 1.0 s after the press, boss spawns 0.3 s later. Without it the cover lasts about 27 s. AreaObjSkip gave only about 1.8 s; loader gate skip no gain | faster-load-pc2-6801.md |
| CoverReleaseMs / NearArenaOnly | Failed | PC Win: control returns in 0.5 s but screen stays black about 30-40 s; "7 s" was a log number only. Ally Lin 31.1 s vs 30.1-30.2 s | cross-world-load-2026-10-16.md |
| Ally Continue window (118.7 s) | Open | Earlier fixed-timer reading was revised: real CPU work to about +93 s then the press prompt; critical path is manager A request latency, reads about 30x slower per call than PC | loading-ally-fixed.md |

### 4.4 Optional Extra Performance (changes the look)

Source: extra-performance-settings.md. Default off, so it is not part of "pure-perf".

| Option | PC Win | PC2 Win (GPU time) | Ally Lin | Look change |
|---|---|---|---|---|
| Performance preset | +4.3 % +/-0.6 (xp9sky); README says +3.14 % +/-0.08 | -0.52 ms (-2.7 %) | +3.48 % +/-0.07 | none visible |
| Potato preset | +10.5 % +/-0.1 | -1.82 ms (-9.4 %) | +9.57 % +/-0.17 | coarser fog, less AO/contact shadow, 20 % of pixels changed |
| XGI (GI res x0.35) | +3.6 % | -3.1 % GPU | +2.1 % | none visible |
| XFOG fog grid | +1.5 to +2.8 % | -3.5 % GPU | +0.1 % | dimmer glow; fog off changes 23 % of pixels |
| XAO (AO off) | +1.8 % | -1.6 % GPU | +2.0 % | none visible in stills |
| XPOST (blur+DOF off) | +1.8 % | -1.1 % GPU | +2.1 % | none in stills |
| XCLD (cloud steps) | -0.2 % | not given | +2.5 % | none |
| XSHD, XSHF, XPRT, XTER, XLOD | about 0 | not given | XLOD -0.4 % | none |

Measured PC at 1440p GPU-bound with a cheap sky scene; the note expects heavy fog/cloud scenes to gain more but gives no number.

### 4.5 Latency and presentation

| Item | Verdict | Number | Source |
|---|---|---|---|
| VSync OFF + FreeSync (independent flip) | Settings choice | Saved about 16 ms at a 60 cap on PC; composed flip adds 8.6 ms vs 1.9 ms present-to-display | session-2026-10-08-perf.md s2 |
| VrrPresent | Maybe | Built, offline verified, not run when written; Windows only | vrr-present.md |
| IK input kick | Failed on Ally | No effect (GPU-bound); 2 ms wait cap was a cost, now 0 | mod README |
| MaxFrameLatency 2-3 | Maybe | "Throughput lever when the GPU is the limit, at up to one frame more delay"; no number | mod README |
| Proton FG safe toggle | Worked (stability) | Ally FG on worked 3/3; stock FG off crashed 1/1 in that log | fg-proton-fix.md |

### 4.6 Stutter

| Item | Verdict | Number | Source |
|---|---|---|---|
| PJH (PSO journal prewarm and hand-over) | Maybe (promising, default off) | PC Win, 4 launches: prewarm 15,732 PSOs in 2.7 s, runtime creates 81 -> 43 (-46 %), creates of at least 1 ms: 0. No hitch existed on that route so the real benefit is unproven | perf-opportunities-pc-20261015.md #1; stutter-pso.md |
| PL (create PSO outside lock) | Failed | No frame-time gain | perf-opportunities-pc-20261015.md |

## 5. Optimization opportunities still open

None of the gains below are measured unless a number is quoted. Expected gains are the notes' own estimates.

### (a) Pure logic, CPU and scheduling work, no visual change

| Idea | Expected gain | Risk | Windows | Linux | Source |
|---|---|---|---|---|---|
| First-sight PSO hitch fix: ship PJH=1 and seed the journal | Boss-kill hitches of 98-448 ms; test pass criterion is 50 % fewer frames over 100 ms. PC route showed 46 % fewer runtime creates | Low-medium: about 64 MB journal, boot disk writes | Yes (measured on PC) | Plausible; PSO creation cost under vkd3d not measured | perf-opportunities #1; stutter-pso.md |
| Isolate RD, M2S, DQ, AABB as one-at-a-time rows | Each at most about 0.2 ms/frame (RD/M2S); AABB 0.5-0.6 ms per logic tick (estimate) | RD/M2S medium (cross-thread ordering); AABB bit-exact offline | Yes | Ally never completed the controlled pair; CPU savings there are power, not fps | perf-opportunities #2; checkpoints 2026-10-28 |
| Explain 1% low (44-53 fps vs 96-99 average): classify slowest 1 % frames as CPU, GPU or WAIT with existing CSVs | Unknown; measurement first | None for analysis; worker-join patches medium | Yes | Needs Linux CSVs | perf-opportunities #3 |
| Replace the drain spin at 418F40 with a wake-based wait / renderer drain wait handshake | PC2: about 6.1 ms of main-thread spin per frame is the wall, so the ceiling is high but the replay thread work is the real limit (about 1 ms frame per 1 ms cut) | Medium: ordering | Yes | Likely same engine path | cpu-bound-midrange.md; STATUS.md FO verdict |
| Speed up the single D3D12 replay thread (the "GO" route after FO was ruled out) | About 1 ms of frame per 1 ms cut on PC2 | Medium-high | Yes | Ally is GPU-bound, no gain | STATUS.md 2026-10 FO VERDICT |
| Dispatcher PSO cache lookup (about 3.1 ms/frame on PC2) and constant blob memcpy (about 1.8 ms) | PCM measured only +0.5 %, so a deeper redesign is needed | Medium | Yes | Yes if CPU-bound | cpu-bound-midrange.md s2, s3 |
| Wake-storm control: spin-before-block, WorkerCap for handhelds, wake coalescing | Power headroom for the GPU on Ally; WG already partly done | Medium | Little | Highest value (power-shared) | ally-report-2026-10-06.md |
| Loading: split the 37 s first rematch load by phase; menu-time prefetch | Win condition 5 s or more median | Medium (never skip readiness flags) | Yes | Ally loads still slower | perf-opportunities #4 |
| Loading: identify the gate G5/G8 holding 15 s after resource managers drain (LG probe), then LB/LS fixes | Up to the gate share (about 10 s on PC) | Medium | Yes | Ally pending | loading-bottleneck.md s4, s5 |
| Item list from untried-ideas: PP on Ally, KL_Div, RD/M2S/J1 on Ally, LM=1 sweeper, LC/TSY first-load, small-copy memcpy probe, JT critical-path probe | Not quantified | Low each | Some | Several are Ally-only | .handoff/untried-ideas.md |
| Cap logic ticks to 60 Hz more cheaply (AABB, BPO, CP prefetch) | AABB 0.5-0.6 ms, BPO 0.12-0.25 ms, CP at most 0.15 ms per tick (estimates) | Low-medium | Yes | Yes | checkpoints 2026-10-28 11:08 |

### (b) Affects visuals or effects

| Idea | Expected gain | Visual cost | Risk | Windows | Linux | Source |
|---|---|---|---|---|---|---|
| Lower GI from HIGHEST to HIGH (or XGI) | HIGHEST costs about 2.5 ms PC vs 0.71 ms HIGH, +11-14 ms Ally. XGI x0.35 measured +3.6 % PC, +2.1 % Ally | Not visible in still-image comparison | Low | Yes | Yes, biggest lever on Ally | gpu-maxsettings.md; extra-performance-settings.md |
| Potato preset (coarser fog, no AO, harder shadows) | +10.5 % PC, +9.57 % Ally | 20 % of pixels changed | Low (option exists) | Yes | Yes | extra-performance-settings.md |
| Fog volume resolution / fog off (XFOG 3) | +2.8 % PC | Glow gone, mean diff 12.8, "big" | Low | Yes | Ally +0.1 % at level 2 | extra-performance-settings.md |
| AO off, motion blur/DOF off, bloom off | +1.8 %, +1.8 %, up to +6.7 % (noisy) PC | AO visible in some scenes (Potato loses contact darkening) | Low | Yes | Yes (+2.0 %, +2.1 % Ally) | extra-performance-settings.md |
| Two-frame effect lag (frame overlap with lag) | Emulator +11.74 % +/-1.54 SE; earlier estimate +15-19 % on PC2 | Effects one frame late; look risk | High: 1-2 weeks, fo10 hung | Windows only tested | Ally is GPU-bound, no gain | STATUS.md FO verdict; handoff notes |
| Async compute for GPU effects, grass and tree wind | Slot 0 about 0.13 ms (+1-1.5 %); Ally estimate 0.3-0.5 ms minus power effect | None (same shaders) | Medium: unknown why KT disabled it on non-NVIDIA | Yes | Ally sign unknown (power-limited) | gpu-async-wins.md |
| Reduce late-frame copies/resolve (POSTEFFECT_RESOLUTION and resolution audit) | PC2 copy/resolve/clear 1.73 ms (7.5 % of frame) is the pool, but STANDARD tested neutral | Possible softness | Medium | Yes | Yes | gpu-rewrite-round3.md s1; backlog-small-wins.md |
| Meshlet LOD scale (XLOD) and terrain tessellation | XLOD +3.8 % +/-2.9 not significant; XTER +0.6 % | Pop-in/less detail at distance | Low | Yes | Not measured as gain | extra-performance-settings.md |

## 6. Honest limits

- The Ally is limited by a 25 W shared power budget; same-picture work is estimated at about 5-12 ms at MAX, not enough for 60 fps (gpu-maxsettings.md).
- Many CPU fixes are fps-neutral when GPU-bound but save power or help weaker CPUs (session-2026-10-08-perf.md s4).
- A "whole default stack vs stock" result exists only in the final bench above (two chunks per arm); several device pairs never completed (perf-opportunities Unknowns).
- Numbers from different sessions use different scenes, settings and noise levels (about 4 % arm-to-arm on PC). Treat single-pair results as indicative only.
- Linux evidence is thinner than Windows: one Ally device, small samples, wide chunk variance.
