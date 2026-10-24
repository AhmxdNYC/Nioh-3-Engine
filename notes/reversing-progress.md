# How far the reversing has gone

2026-10-24. gx-pc #29270. Offline recount, no game, no new decompile. The charts in `docs/progress/` were regenerated with `progress_treemap.py`, `renderer_progress.py`, and `re/scripts_renderer/progress_chart.py`. They already matched this tree, so this note does not change the pictures. The same recount is summarized in `notes/re-status-2026-10-24.md`.

A named function is not a rebuilt system. "Understood" below means the symbol has a real name, not `FUN_` / `sub_` / `Unknown_`.

## The short version

The code that shows up in a CPU or GPU profile is named. Most of the executable is not.

| what | count | meaning |
|---|---|---|
| Functions in the exe | 211,206 (52.7 MB) | every `.pdata` start |
| Functions with a real name | 12,679 (6.00%) | was 6,913 (3.27%) on the October 8 badge |
| Code bytes with a real name | 9.34% | the other 90.7% of bytes have no name |
| Profiled CPU self time named | 100% of 1,311 functions | Ally + PC profiles |
| GPU passes mapped | 59 / 59 | Ally MAX capture, top buckets 100% identified |
| Renderer catalog | 54,295 functions, 8,331 named | 20.71% of those bytes |
| Renderer core | 2,382 / 5,932 (40.2%) | depth under 50; unchanged from the October 11 audit |
| Purpose unknown | 0.36 MB (0.7%) | labelled by the region graph; goal was under 5% |

`symbols.tsv` is a different list from the 211,206 functions. It has 13,827 function rows: 9,053 high, 4,126 medium, 648 low. High means a decompile, a working patch, or a live probe.

## Hooks that exist

`mod/Nioh3EngineFix/patches.json` on this tree has **257 sites in 72 groups**. The README's older "255 sites" is that catalog. The groups are the opt-in switches: waits and spins (W1, W2, W3, W6, L1, R1, FG, GF), draw and submit (DQ, P1E7, MA, PCM, CBU, PP), meshlets (M1, M2, MLP), bounds (AABB, BPO, CP, SPF), and the menu/live rows around them.

A separate safety list, not a second catalog, is in `notes/hook-register-audit.md`: 45 call sites, 17 mid-function caves, 28 compare-and-swap loads. That audit's verdicts were safe or already fixed. It is not a count of every later experiment.

Half-rate post reuse and the dynamic-resolution hook live on their own branches. They are not extra rows in this 257.

## What is mapped

These are the systems we can name, and in several cases patch, because they cost time:

- Draw submission and the replay thread. Draw lists are the best-named renderer package (97.2% of that package's bytes). The frame-graph package is 60.4%, resources 78.2%.
- Meshlet streaming, collision and physics, animation and bounds, effects, and the job scheduler. The thirteen subsystem pages in `docs/subsystems/` cover every area that was at least 0.3% of profiled CPU self time. Those pages are the 2026-10-08 write-up. This recount did not rewrite them. Their coverage of *profiled* time is 97.5% to 100%.
- GPU passes: FSR3, lighting, materials, shadows, fog, trees, clouds, copies, culling, post. Identified, not rewritten.
- Present pacing for frame generation (`FG` / `fgWait` at `0x3777A40`): the clock spin is known and already has a switch.
- Boss fight *data* (action tables, hit counts) is mapped for the bosses those notes name. That is a table, not a decompile of enemy AI.

## What is still unknown

- About 94% of functions, and about 91% of code bytes, have no real name.
- The biggest unnamed blocks are generated rig code (11.52 MB, 99 names), menu and item text (6.38 MB, 39 names), and online/P2P (1.39 MB, 60 names).
- Inside the renderer, meshlet code is only 8.5% named by bytes, materials 9.4%, effects 11.3%, post 11.6%, backend 12.1%. Lighting is 26.4%. Volumetrics is 42.9%.
- Region labels exist for almost all bytes, but most regions are low confidence. A low label means "mostly this, neighbours mixed in" (`docs/code-map.md`).
- Enemy AI is 2,376 functions with 20 names. Camera is 339 functions with 17 names.
- No full renderer has been rebuilt. Frame overlap stays NO-GO. Training mode has no shipped practice loop beyond the rematch path.

## What this pass did not do

No new function was decompiled. No hook was added. The game was not opened.
