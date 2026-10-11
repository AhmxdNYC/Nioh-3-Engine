# Research notes

Reverse-engineering notes on `Nioh3.exe` (Steam v2.02), published so that others can continue the decompilation.
They are working notes: static analysis (dump + Ghidra), sampling profiles and GPU traces, written while building
the engine fix. Addresses are RVAs (Ghidra image base 0x7FF74F410000) and the names match
[`symbols/symbols.tsv`](../symbols/symbols.tsv).

How to read them:

- **Performance numbers:** the only checked results are in [docs/BENCHMARKS.md](../docs/BENCHMARKS.md). A number in a
  note that is not on that page is an estimate, a single capture, or a measurement of one component (ms per frame,
  share of a thread). It is not an fps claim.
- **Paths** such as `re\scripts_*\`, `captures\...`, `patcher\*.fsx`, `<re-scratch>\` and `<worktrees>\` point into
  the private development repository or local scratch folders. They are not part of this repository.
- **Other note names** (for example `cpu-hotspots-ally.md`) that are not listed below are unpublished development notes.
- **Machines:** "PC" (Ryzen 5 7600X + RX 9070), "pc2" (Ryzen 5 5600X + RX 6650 XT on a slow ~0.8 GB/s chipset slot,
  a worst case, not a typical PC), "Ally" (ROG Ally X under Proton). Details: [docs/BENCHMARKS.md](../docs/BENCHMARKS.md).
- **Symbol sources:** the `source` column of `symbols.tsv` cites development paths (`notes/x.md`).
  [SOURCE_MAP.tsv](SOURCE_MAP.tsv) maps each one to its published note here.

## Renderer decompilation (`renderer/`)

The plan and package map are in [renderer/00-map.md](renderer/00-map.md).

- [renderer/00-map.md](renderer/00-map.md): module map, coverage, the parallel package plan, runtime frame capture, new-renderer feasibility.
- [renderer/01b-replay-handlers.md](renderer/01b-replay-handlers.md): the back end's replay side, token by token down to D3D12, with per-call waste.
- [renderer/05-frame-graph.md](renderer/05-frame-graph.md): how the render graph is built and run each frame, checked against captures.
- [renderer/06-resources.md](renderer/06-resources.md): GPU memory, upload/readback rings, heaps, transient render targets, staging, residency.
- [renderer/07-drawlists.md](renderer/07-drawlists.md): from scene objects to KTGL draw tokens (draw lists, sorting, the draw-list walker).
- [renderer/08-meshlet.md](renderer/08-meshlet.md): the GPU-driven meshlet pipeline (LOD, culling, Hi-Z, streaming) and a replacement design.
- [renderer/09-volumetrics.md](renderer/09-volumetrics.md): volumetric fog, sky clouds, sky ambient, ocean, rain and wind passes.
- [renderer/10-effects.md](renderer/10-effects.md): effect instances, CPU/GPU particles, decals, ripples, effect lights.
- [renderer/11-ui.md](renderer/11-ui.md): the UI quad path, glyph atlas, vertex upload, and the frame-generation HUD-less path.
- [renderer/12-capture-check.md](renderer/12-capture-check.md): the decompiled renderer model checked against frame captures on three GPUs.
- [renderer/13-main-submit.md](renderer/13-main-submit.md): the main-thread render-submission path, its costs, and what to cut next.
- [renderer/backend.md](renderer/backend.md): the D3D12 back end (token stream, replay thread, rings, fences, submission) and a new-backend assessment.
- [renderer/cbv-upload.md](renderer/cbv-upload.md): cheaper per-draw constant uploads on the replay thread (the CBN/CBT fix and its variants).
- [renderer/frame-overlap.md](renderer/frame-overlap.md): overlapping one frame's replay with the next frame's recording (experimental, off by default).
- [renderer/lighting.md](renderer/lighting.md): lighting model, shadows, GI and probes, AO, volumetric inputs.
- [renderer/materials.md](renderer/materials.md): the PSO / shader / material layer, permutations and bindings.
- [renderer/post.md](renderer/post.md): upscalers, the post chain, frame generation, present and pacing.

## Engine systems

- [render-nodes.md](render-nodes.md): the engine code behind every GPU pass; how the 769 registered render/shader types were named.
- [d3d12-sites.md](d3d12-sites.md): census of all D3D12/DXGI vtable call sites (604 sites in 272 functions).
- [d3d12-submission.md](d3d12-submission.md): how KTGL submits to D3D12 and where submission wastes CPU or GPU time.
- [vkd3d-audit.md](vkd3d-audit.md): which of the game's D3D12 patterns cost more under vkd3d-proton (Ally), with game-side fixes.
- [gpu-copies-postres.md](gpu-copies-postres.md): late-frame copies and filters, and what POSTEFFECT_RESOLUTION really changes.
- [invisible-gpu.md](invisible-gpu.md): GPU work with little or no visible effect (probe gathers, blurs, Hi-Z builds).
- [gpu-tiles.md](gpu-tiles.md): UpdateTileMappings call sites and whether they explain the device-wide GPU stalls.
- [shadows-culling.md](shadows-culling.md): shadow-map caching and visibility culling: what the engine already does and what is left.
- [shadows-redundancy.md](shadows-redundancy.md): an audit of provably redundant GPU work in the shadow pipeline.
- [frame-timing.md](frame-timing.md): the frame manager, fps tiers, game-speed scaling and the slow-motion-below-cap fix.
- [settings.md](settings.md): the config.xml / graphics settings map and what each option drives in the engine.
- [work-deciders.md](work-deciders.md): how settings turn into pass parameters, LOD, streaming, culling and scheduling.
- [cpu-profile.md](cpu-profile.md): whole-process CPU profile: threads, spin loops and where the cores go.
- [mainthread-drawprep.md](mainthread-drawprep.md): the main thread's draw-preparation hot spots and which parts are waste.
- [mainthread-logic.md](mainthread-logic.md): the main thread's non-render work and the game-logic thread.
- [meshlet-mainthread.md](meshlet-mainthread.md): meshlet and texture-streaming bookkeeping under render-frame-begin.
- [ui-mainthread.md](ui-mainthread.md): per-frame UI/HUD work on the main thread, ranked candidates.
- [job-system.md](job-system.md): the KIDS job system (push, wake, wait) and what it costs.
- [worker-jobs-opt.md](worker-jobs-opt.md): same-result speedups for worker-thread jobs (collision, stage, object update).
- [input-thread.md](input-thread.md): what [App]InputUpdateWorker does each frame and what can be saved.
- [effects-animation.md](effects-animation.md): effect and character/animation update: what runs, where, and what is waste.
- [particles.md](particles.md): the particle/effect pipeline from spawn through GPU simulation, culling, drawing and merge.
- [stutter.md](stutter.md): hitches and frame-time spikes, their causes, and what stands between the game and a steady frame rate.

## Loading

- [loading.md](loading.md): level-load phase timings, the load pipeline, and same-result speedups.
- [loading-bottleneck.md](loading-bottleneck.md): what bounds level-load wall time, from PC load statistics and static analysis.
- [loading-ally-fixed.md](loading-ally-fixed.md): the Ally's long title-to-Continue load: candidates, fixes and Ally tests.

## Gameplay and bosses

- [gamelogic.md](gamelogic.md): MissionThread, GameUpdateTick and per-tick lookups.
- [boss-exe-strings.md](boss-exe-strings.md): boss/duel identifiers and the script native-binding table (1072 entries) in Nioh3.exe.

## Tools and method

- [perf-coverage.md](perf-coverage.md): naming every profiled function in the symbol database: method, scripts and rename batches.
- [SOURCE_MAP.tsv](SOURCE_MAP.tsv): development note path to published path, for the `source` column of `symbols.tsv`.
