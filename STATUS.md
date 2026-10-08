# STATUS (handoff) - 2026-10-07 ~14:30
Shipped to Ally: ASI 1.5.0 (v9: W6, GF, SP profiler, MemStats, WL, ...), shader pack v2 (ghdm, esm, wdo28, mpc23, toonedge2).
Measured: W6/GF kill main-thread spins (GPU MHz up), fps flat (GPU-bound). GHDM +8.7 % at MAX. MAX native = 16.3-16.8 fps; lightest-50 = 32 fps.
Bugs: live FG ON->OFF under Proton crashes ~2 s later (log captures\ally\nioh3_ally_modlog_v9-FGoff-crash_*.log); fix = capture only the game's own queue (ECL return addr in Nioh3.exe/sl.interposer) + Proton ON->OFF = next launch. NOT DONE.
Killed at the session limit (resume with SMALL fresh agents, Sonnet unless noted):
1. v10 ASI (partial edits in mod\Nioh3EngineFix\src - check `git status`/build first): FG-off crash fix (priority), WG, TG (opt-in), LK/CP/SPF (notes\worker-jobs-opt.md), then GP GPU-pass profiler (timestamp queries at the engine debug markers; patcher\gpu_markers.fsx).
2. Culling kernel rewrite (Opus) - notes\shadows-culling.md; coordinate with gpu-shaders\sweep\ (MPC#23 already in 0x29ee0902).
3. Tiled-resource stall hunt (Opus) - notes\gpu-scheduling.md (1.35 ms device-wide stalls); just started.
4. GI/fog rewrite (Opus) - was packaging; check gpu-shaders\ for partial work.
5. Stutter/p99 (Sonnet) - partial; notes\stutter.md may be missing.
6. Exe post-effects via PSO hook (Opus) - notes\gpu-post-exe.md; was implementing psoswap.
Ally pending: ESM/WDO28 at MAX, shader v2 A/B, fast-travel MemStats test. Never use RGP on the Ally (froze it).

## 6PM PLAN (run as-is after the reset; the hourly heartbeat at :07 triggers it)
Budget: max 3 agents at once; pass `model`; briefs = goal + inputs + deliverable + "read AGENT_BRIEF.md + token-efficiency.md".
Step 0 (main session, cheap): `git -C nioh3-engine status` - see what the killed agents left half-done in mod\Nioh3EngineFix\src and gpu-shaders\. Relaunch the watcher as a background `watch_inbox.py --once`.
Wave 1 (start together):
  A. [sonnet] ASI v10 = FG-off crash fix ONLY first (queue capture limited to the game's ECL callers; Proton ON->OFF = next launch), build + pattern test + zip; then add LK/CP/SPF (notes\worker-jobs-opt.md) + WG; TG opt-in. Finish half-done src edits or revert them (git) if broken. Report -> main posts [BUILD] to the Ally.
  B. [opus] GI/fog shader rewrite: resume from gpu-shaders\ partial files + notes; the biggest same-look GPU lever at MAX (GI = 15-20 ms on the Ally).
  C. [opus] Exe post-effects: psoswap hook (new files) + motion blur/TAA rewrites (8-10 ms on the Ally). Check partial files first.
Wave 2 (as slots free up):
  D. [sonnet] GP GPU-pass profiler in the ASI (timestamp queries at the engine debug markers; patcher\gpu_markers.fsx) - replaces RGP on the Ally.
  E. [opus] Culling-kernel rewrite (merge with sweep MPC#23 in 0x29ee0902).
  F. [opus] Tiled-resource stall hunt (tilestats module + census).
  G. [sonnet] Stutter/p99 census (finish notes\stutter.md from the Ally CSVs).
Ally side: ESM/WDO28 at MAX, shader v2 A/B, then v10 tests (FG off fix, CP/SPF/WG A/Bs), fast-travel MemStats. Digest logs with re\scripts_cpu\sp_aggregate.py before posting.
Done when each item ships to the Ally or is written off in its notes file; update this file after each.

## 18:45 update
Shipped to Ally: ASI 1.7.2 (FG-off fix, GPUProf, ShaderSwap dump, TileStats/SPR probes, WG/CP/SPF installed-off), shader pack v3. MAX shader A/B: GPU W 23.1->20.5 at same fps (VSync 8.33 ms slot quantization) -> metric = GPU ms. HOLD: next shader pack v4 adds GI #121 (0xd070067d, ~2 ms Ally at GI HIGHEST, bit-exact) - send only after the Ally's 1.7.2 digest (one build per session). Running: VFSCAT fog rewrite (opus). Next after digest: rewrite top GPUProf passes + dumped motion blur/DOF/bloom via psoswap.

## 19:35 update
Ally 1.7.2 digest: FG-off fixed; GP labelled = 14.5 of 59 ms (FSR3 native AA 6.9 ms = user's setting, out of scope); tiles ruled out; CPU patches neutral; dump at D:\nioh3-re\ally-dump\shaderdump_172.zip. Sent: ASI 1.7.3 + shader pack v4 (gi). Running: VFSCAT fog (opus), GPUProf v2 per-PSO timing -> ASI 1.7.4 (sonnet, hold until the Ally's 1.7.3 digest), runtime post shaders from the dump (opus).

## 20:00 backlog (user: optimize everything possible)
Running: runtime post shaders (opus), GI reflection #414 (opus), ASI 1.7.5 = CS culling skip + AS0 async slot-0 experiment (sonnet). Held for the Ally: ASI 1.7.4 GPUProf=2 per-PSO timing (sent after the 1.7.3 digest) -> then target the top unlabelled GPU costs (material/G-buffer shaders likely). Next queue: A2 shadow double-draw check via GP markers; load hitches (DiscardVirtualMemory at stage teardown, needs arena RVAs); FSR3 shader micro-opts (AMD code, low odds); particles; PC tiny-draw instancing (hard).

## 21:50 - session restart + REGRESSION
Ally 1.7.5 GP2: all replacements ON = 0.4-0.6 fps SLOWER than all off. Pairs: sync CS 4.82 vs 3.35 ms (likely gi #121/#414), async CS 3.99 vs 3.40 (likely fog). Cause: RGA (Windows AMD compiler) models don't predict vkd3d->dxil-spirv->RADV/ACO. RULE: no shader ships to the default set without an Ally A/B win. AS0 (async slot 0): stable, no gain. GP2 stamps cost ~14 ms (only for attribution). Running: hash mapping + keep/drop table (sonnet). Ally next: 1.7.7 + v6, per-shader A/B w/o GP2. Killed by the restart (resume later): materials/75% (opus, partial gpu-shaders\materials\), vkd3d audit (opus, nothing written).
