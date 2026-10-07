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
