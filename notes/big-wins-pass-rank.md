# Big-win rank: other-engine tricks against this game's passes

2026-10-23, offline. The game was not launched. Sources are the measured Ally USER budget in `notes/ally-gpu-budget.md` (28.7 ms GPU, 34.9 fps, 960x540 internal, FSR 3.1 to 1080p, volumetrics STANDARD, frame generation off) and the async measurements in `notes/ally-gpu-gap.md` and `notes/gpu-async-wins.md`. The triage in `notes/handoff-codex-next-perf-triage.md` still stands: do not treat an async row or a MinRun4 stamp as frame time saved.

Two different bars, both off 28.7 ms (34.8 fps). **+15% fps needs 3.74 ms saved** (new frame 24.96 ms). **4.3 ms is 15% less frame time** (new frame 24.4 ms, about +18% fps). cxl-pc #27724. None of the four families below is estimated at 3.74 ms. Tags: **M** measured, **E** estimate from a measured pass. An async wall-clock number is not frame time.

## Rank

| rank | trick other engines use | passes it would touch | frame ms, Ally USER | clears 3.74 ms (+15% fps)? |
|---|---|---|---|---|
| 1 | Draw distance, foliage LOD, shadow distance (Skyrim / Witcher / UE scalability). Picture changes. | G-buffer 7.54, trees 0.49, terrain 0.77, shadow maps 1.38, deferred resolve 2.04 | **1.5-2.7 E** if distant draws drop about a quarter and fog is also turned off | no |
| 2 | Shader cuts: half-rate probes, fp16, meshlet scalarize (Frostbite / AMD). Half-rate changes the picture. The rest can match. | RefProbe 1.77; meshlet VS inside the 7.54; FSR accumulate 1.97; fogpar on the 1.78 async fog | **0.6-1.7 E** same picture. **+0.6-0.9 E** more if probes update every other frame | no |
| 3 | GPU cull reuse (last-frame visibility, Nanite-style). One-frame pops if it rejects more. | culling family 2.88 (MPC 1.02 + 0.35, MCLRC 0.65, the rest smaller). Extra G-buffer only if more meshlets die | **0.3-1.5 E**. Skipping the second LOD resolve was already rejected: it is the shadow clipmap, not a duplicate | no |
| 4 | More async compute (id Tech / Frostbite overlap) | fog and clouds are already async. Slot 0 effects 0.55. GI gathers 1.26 | **0.2-0.8 E**. Fog moved between queues saved **0.00 ms M** (span within 0.06). PC slot 0 is **0.13 ms M** on an RX 9070 | no |

## Why each cap is that low

**Draw distance.** The G-buffer is the largest family, 7.54 of 28.7 ms, and about three quarters of the top material row is the meshlet vertex shader. Cutting objects the camera never reaches can only save the draws that are actually far. A quarter of the G-buffer is 1.9 ms. Trees are 0.49 and terrain 0.77, so a foliage-only cut is under 1 ms. Shadow maps are 1.38, so a shorter shadow distance is a few tenths, not the whole shadow resolve (2.04), because the resolve still runs. Turning volumetrics off removes real GPU work of about 0.7 ms, not the 1.78 async stamp: on the graphics queue the same fog needed 0.71 ms (`ally-gpu-gap.md`). Added up, an aggressive picture cut is about 2 to 3 ms, around 8 to 10 percent.

The only measured knob that already cleared 15% is internal resolution, and it is already on the fast side. REN_RAT 50 is 28.7 ms. REN_RAT 100 is 54.6 ms. There is no measurement below 50, so this note does not invent one.

**Shader cuts.** The same-picture list in the budget note is fogpar 0.05-0.2 ms of frame time, meshlet load scalarize 0.3-1.0, FSR fp16 0.2-0.5, wave32 about 0. That stack is the 0.6-1.7 band. Half-rate GI is what other engines do with probes. USER RefProbe is only 1.77 ms, so half of it is under 1 ms. At the MAX preset the same gathers are about 7.9 ms, which is where a half-rate probe would matter. MAX is 64 ms. +15% fps there needs 8.3 ms saved, and 15% less frame time is 9.6 ms, so half-rate GI still does not clear either bar alone.

**Cull reuse.** The cull family is 2.88 ms. Reusing last frame's result skips dispatches only for what did not move, so the saving is a fraction of 2.88, written here as 0.3-0.6. Rejecting extra meshlets would also cut the G-buffer, up to about 1 ms more if 15% of those draws die. The two LOD resolves are not reusable copies of each other (`notes/meshlet-redundant-lod.md`).

**Async.** The Ally is power-limited. Overlapping fog with graphics did not shorten the frame. New async work on a full GPU is starved the same way, so the old "1-3 ms if GI goes async" line is the wall-clock stamp, not frame time. The honest add is a few tenths: slot 0 at 0.2-0.4 on the Ally, and 0.13 measured on this PC.

## What this does not claim

pc2 is often CPU-bound. A GPU-pass estimate does not move a CPU-bound frame. The printed pc2 +15.6% on one hook was later capped by its own mechanism at about 0.5% (`notes/pc-keep-candidates.md`). It is not a 15% result.

No branch in this note builds a patch. The next measured step is still the triage list: fogpar on the Ally, then the AABB control, then a MinRun1 cost probe. Those are tests, not estimated wins.
