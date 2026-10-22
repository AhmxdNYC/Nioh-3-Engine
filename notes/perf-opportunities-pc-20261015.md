# PC (RX 9070) remaining optimization opportunities, 2026-10-15

Read-only desk study. Game not run, no source changed. Rules: no graphics-quality reductions, no OS/driver tuning (everything
below is ASI/engine-side, same picture). Inputs: .handoff/open.md, ibaraki-offon-result-2026-10-14.md, combined-cpu-trial-2026-10-13.md,
cpu-stacks-current-2026-10-13.md, cpu-next-candidates-2026-10-13.md, renderer-drain-wait-candidate-2026-10-13.md, pc-cpu-next.md,
pc-submit-overhead.md, remaining-work.md, whats-left.md, stutter.md, stutter-pso.md, boot-shader-compile.md,
rematch-fast-restart-2026-10-13.md, captures/pc/samelook/20261009-1324-pcsl1/report_console.txt, mod README/ini.

## Where the PC stands (evidence)

- Latest measured result (Ibaraki, 4 arms OFF,ON,ON,OFF, ibaraki-offon-result-2026-10-14.md): OFF 96.3 fps / 1% low 44.6 ms-equivalent fps /
  CPU 7.06 cores; ON 99.5 fps / 1% low 53.4 / CPU 6.34 cores. Arm-to-arm noise ~4%. The CPU fixes (RD/M2S/J1/WG/LK/MW) buy ~3% fps, ~10% CPU.
- The gap between avg (99.5) and 1% low (53.4) is the big remaining target: it is about frame pacing/hitches, not throughput.
- Thread budget (cpu-stacks-current): scheduler workers 2.2-2.3 cores, stage workers 1.0-1.15, collision 0.8, main 0.74-0.82,
  meshlet-request workers 0.73-0.80, dispatcher 0.49-0.53 (total ~7.75 cores). The main thread is not saturated; the spread is in workers that poll.
- Synthetic same-look scene (samelook pcsl1): ft p50 6.3 ms, p99 8.7, max 11-14 ms at ~200 fps. No hitch there. Hitches are in real play (combat, boss kill, new effects).
- Gameplay hitches seen on PC are first-use PSO compiles (stutter-pso.md s.1: boss kill 282/102/448 ms mod, similar stock; 25 creates >= 1 ms per session; 106 CPU spikes of 20-50 ms, ~2.3/min uncapped).
- Level/mission load is the largest wall-clock cost: boss rematch first load 37 s (open.md), retries 3-6 s.

## Ranked opportunities

### 1. First-sight PSO compile hitches: make PJH (PSO hand-over) the default and extend the journal across sessions
- Evidence: stutter-pso.md s.1 (4 gameplay hitches > 100 ms in a 7-minute mission, all PSO creates; worst 206 ms inside lock on dispatcher thread, main spins in 418F40 until done); stutter.md class E. PJH 4-launch on PC (stutter-pso.md l.131-133): prewarm 15,732 PSOs in 2.7 s, runtime creates 81 -> 43 (-46%), creates >= 1 ms: 0 in all four launches. That route had no hitch to fix, so the DROP verdict says "no evidence on that route", not "does not work". PJ/PJH are default 0 (ini lines 386-403).
- Change: ship PJH=1 (journal + title idle prewarm + hand-over) in the PC profile; seed the journal from a key-only list so first-ever sights are covered; keep the single lowest-priority thread.
- Risk: low-medium. Memory ~64 MB of journal plus kept PSOs (PJ_KeepMax 4096), disk writes at boot, driver-cache dependence. Same picture by construction (identical desc/shaders/root sig). Worst case is a wasted thread at the title.
- Smallest test: a hitch-prone route (boss kill + first-time effects, e.g. the same boss fight twice from a fresh journal). Arms: PJH=2 (installed, off) vs PJH=1 via live menu row, 2 launches each, tools/pj_hitch.py. Metric: frames > 50/100 ms, 0.1% low, SPR creates >= 1 ms in the boss window. Pass: >= 50% fewer > 100 ms frames, no worse p99.

### 2. Renderer drain wait (RD) and meshlet seed-read (M2S): decide with a controlled pair, then consider a blocking dispatcher handshake
- Evidence: cpu-stacks-current: 418F40 drain is 7.0-7.1% of main-thread self samples (exact RIP 418F75), an active poll; meshlet worker 48E90 is 15.6% self in the 49150..4915A capacity-sum loop. RD/M2S built default off, merged in the combined bundle; the Ibaraki ON arm includes them but is not an isolated RD/M2S result. pc-cpu-next also shows [App]Game waits 0.6-1.05 ms/frame on the logic packet (DQ retry spin).
- Change: no new code. Isolate RD, M2S, DQ (200 us quantum) and AABB (ObbToAabb, 0.6-0.8 ms of Scheduler0, never A/B'd on PC) as one-at-a-time live rows. If RD wins, extend to a wake-based wait instead of PAUSE polling.
- Risk: RD/M2S medium (cross-thread ordering; races reviewed offline only). AABB bit-exact offline. M1/full M2 stay excluded.
- Smallest test: same-binary ABBA with ONE switch (e.g. RD) at a fixed location, 30 s settle + 60 s capture, MaxFps 240 so the cap does not compress (run5/run6 were cap-compressed). Metrics: process CPU cores and ft p50/p99 aligned. Keep if CPU >= 3% lower or fps >= +1% with p99 not worse by more than 2%. Expect small (<= 0.2 ms/frame), so noise (~4%) forces 2+ quads.

### 3. Frame pacing in combat: explain the 1% low (44-53 fps vs 96-99 fps average)
- Evidence: Ibaraki table above. The 1% low improved 20% with the combined CPU set, which suggests main/worker contention rather than GPU. cpu-stacks shows 7.75 cores busy of 12 logical, with 2.2+ cores of scheduler workers and 1.1 cores of stage workers polling/executing shared jobs; wake storms (stutter.md class D) and the logic join wait (1.44 ms on Scheduler0, KIDSTaskScheduler1-6 only ~24% busy while 0 waits). No frame-class attribution of the worst 1% exists (CPU vs GPU vs WAIT per frame; pc_spikes.py exists for this).
- Change: first measurement only. Run re/scripts_stutter/pc_spikes.py over the existing Ibaraki/Yoritomo PresentMon CSVs (captures/pc2 and ally/pc rematch captures), classify the slowest 1% as CPU/GPU/WAIT, and bucket by time against SPR creates, stage loads and streaming. Then target the dominant class (candidates: CP/SPF/DL worker-join patches, already built default off).
- Risk: none for the analysis; patch risk depends on target (worker-join patches medium, behavior-adjacent).
- Smallest test: no game run needed first (existing CSVs). Follow-up in game: one 60 s combat window with SPR + CpuStats on, compare frames > 25 ms count between CP/SPF off and on.

### 4. Cut the 37 s first load of the boss rematch (loading/streaming, not frame rate)
- Evidence: open.md (37 s first load vs 3-6 s retry); rematch-fast-restart notes: mission/script setup and post-first-pass character-set readiness are the native cost; streaming center already placed at the arena; PC load fixes already in tree (ReadFile prefetch PF/SR, boot PSO list parallelised in the title). Phase split of the 37 s is the open next step.
- Change: split the 37 s by phase with the existing LoadStats and fightnow_test retry mode (menu to request, native mission load, request to control), compare warm vs cold, then apply the cheapest cut: prefetch the arena's resources (Preload on menu draw is already in via ff2ccfd) and move one-shot first-use PSO/shader work (PJH prewarm) ahead of the load. The in-place arena swap (Nioh3Training_FightNow) already avoids the mission load on retries.
- Risk: medium; skipping readiness checks is unsafe, so only warm caches/prefetch, never skip flags.
- Smallest test: tools/rematch/fightnow_test.ps1, first load cold vs after a menu-time Preload, 3 runs each, time from request to control. Win: >= 5 s median.

### 5. End-of-load GPU burst (loading-screen tail) and load-start CPU stall
- Evidence: stutter-pso.md s.1: 56 of the mod's 94 frames > 100 ms are the GPU-bound end-of-load burst (3-6 s of 100-130 ms at 100% GPU); the mod run had about twice stock's GPU work per presented frame there; cause unidentified. 24 more are a CPU stall at load start.
- Change: GPUProf=2 across the load end, mod vs stock, to find the pass that owns it; if one pass is a one-shot, move it earlier (output identical) or split it over frames. Lowest priority: the player is on the loading screen, it only shortens loads and the 0.1% low of a session.
- Risk: low for the profile, medium for any pass change.
- Smallest test: two loads with GPUProf=2 (stock vs mod ini), compare per-pass times in t = end-10 s .. end.

## What not to pursue (already decided or no gain)

PL (create PSO outside lock): no frame-time gain. B (draw-list search): DROP. Barriers on PC: keep 0 (measured -7% regression). M1/full M2: correctness gaps. Skipping drain or arbitrary Sleep in 418F40. Blanket skipping of draw/collision/stage jobs. Anything that lowers resolution, shadows or LOD.

## Unknowns that block a firm ranking

- No isolated RD/M2S/J1 effect on PC (only the bundle in Ibaraki). The PC2 and Ally controlled pairs never completed (Runtime4 update, missing frames.csv).
- No PC traversal/new-effect combat hitch census (stutter.md class not measured on PC, remaining-work 2.7); #3 fills this gap.
