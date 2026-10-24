# Map load and draw radius: ranked against the Ally budget

2026-10-23, offline. The game was not launched. Ally USER budget is `notes/ally-gpu-budget.md`: 28.7 ms GPU, 34.9 fps, REN_RAT 50 (960x540 to FSR 3.1 to 1080p), volumetrics STANDARD, frame generation off. +15% fps needs 3.74 ms saved. 4.3 ms is 15% less frame time, about +18% fps (cxl-pc #27724). Tags: **M** measured on a named device, **E** estimate from a stamped pass. The two measured rows are a different Ally capture (~41 fps, GPU 98%, bridge #1723), not the 28.7 ms budget, so their percents are not converted into 28.7 ms.

None of these clears 3.74 ms. Ranks 1 and 2 overlap, so they are not added.

## Rank

| rank | what to cut | engine point | Ally frame ms | hitch change | picture risk |
|---|---|---|---|---|---|
| 1 | Foliage and prop scatter distance | Data, not a hook. ProceduralPlacement on 27 maps: ViewDistance 600 m on 21 maps (also 300-500 m), ModelViewDistanceArray up to 250 m, LODDistanceArray 3-200 m, InstanceCullingDistance 50 m, grass FarClip 150 m (`notes/game-data.md` §4.6) | **0.2-0.8 E**. The labeled tree pass is 0.49 ms. Props are inside the 7.54 ms G-buffer and are not a separate stamp | none measured | Pop-in past about 360-450 m if scaled to 0.6-0.75. Grass thins if density drops |
| 2 | Entity and placeable draw distance | CullEntitiesByDrawDistance `0x1772B4`, GetDrawDistanceLimit `0x177274` (`g_pDrawDistanceCfg+0x80`), GetPlaceableDrawDistance `0x209EB0`, small-object cull `0xDD7AC` (AABB under 10 m) | **0.3-1.0 E**, same draws as rank 1. Not additive | none measured | Same pop-in. Nioh3Fix hooks this family only when its multiplier is above 1, which draws farther, not less |
| 3 | Meshlet LOD, already built | ApplyMeshletLodScales `0x2353C0`, cave EPL1. Menu key XLOD, levels x1.5 / x2 / x3 | **about 0 M**. Ally XLOD2 was -0.4% at ~41 fps (called none). PC RX 9070, 1440p REN_RAT 75: GPU -0.03 ms, fps +3.8% ± 2.9 (not significant) | none | Coarser distant meshes. The Ally line did not include a look shot for this single |
| 4 | Shadow distance, already built | RenderConfig S+0xAF4 ShadowEraseFarEnd, env 500 m. Menu key XSHD x0.6 / 0.35 / 0.2. Character far-shadow flag `0x876EB8` is already 20/30/70 m | **about 0 M**. Ally XSHD3 +0.1%. Shadow-map stamps are 1.38 ms, but the 2.04 ms resolve still runs | none | Distant shadows fade sooner |
| 5 | Terrain LOD / tessellation reach | S+0x1970 TerrainTessellationMaxDistance. Menu key XTER. Landscape LODDistanceScale is 1.3 or 1.49 in data | **0.05-0.2 E**. PC XTER2 (x0.4) saved 0.06 ms GPU. Terrain stamp on the USER budget is 0.77 ms | none | Slightly simpler ground. At 540p internal this is hard to see |
| 6 | How many characters stay drawn | CharacterDrawBudgetCull `0x2814F0`, cap 30/45/60. MODEL_QUAL distances are written and then unread in v2.02 | **0-0.2 E**. The cap is rarely reached | none | A crowd can pop. Not a map-radius win |
| 7 | How much of the map is loaded while moving | Scene focus `0x3A083C`, StagePathInitStep `0x3B35B4`. Placement UpdateDistance 1000 m, FreeDistance 1200 m | **about 0** frame time. This is I/O and bookkeeping, not a GPU pass | **no measured cut**. Ally steady play: 0 spikes in 5,000+ frames. The only big hitches are the first level load (streaming ramp max 80-130 ms, end-of-load max 265-305 ms) | High if shrunk: a warp or a run can leave loaded ground. Do not cut this for fps |

## What not to add

A quarter of the whole G-buffer would be 1.9 ms, and that only if distant meshlets really are a quarter of the 7.54 ms. Nothing in these captures splits near from far, so this note does not use 1.9 as the estimate. Fog-off is a different switch and is not part of a streaming-radius cut.

pc2 is often CPU-bound. A GPU-pass estimate does not move that frame. No patch was built here. The next measured step for this family, if one is run, is one open-field arm that scales ViewDistance and ModelViewDistance to 0.7 and records tree-pass ms, not another LOD-scale tweak: XLOD and XSHD already came back at about 0.
