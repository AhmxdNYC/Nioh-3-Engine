# Shadow pipeline: provably redundant GPU work (2026-10-08)

Static audit only. Sources: the decompile `<re-scratch>\decomp` plus read-only Ghidra queries, the dumped Nioh3.exe (Steam v2.02), the shipped `shader_dx12` DXIL, and three Ally mod logs (run 175, routes L1-L3: GP marker tables + GP2 per-PSO digests, 89 windows, Ally config `SHADOW_QUAL STANDARD`). The game was not run. Builds on `shadows-culling.md` (§1–3 there); the Ally VSM details are below.

## 0. Bottom line

| # | Candidate | Verdict | Ally GPU ms/frame | Patch |
|---|---|---|---|---|
| **S1** | VSM tiles drawn in "Batch Tiles" beyond the per-frame tile budget (drawn, never marked valid, cleared and redrawn next frame) | **Not provably exact (§1b).** The texels are never sampled, but the skipped draws/culls feed texture-streaming sampler feedback and meshlet cache codes, so later frames can differ. **Not implemented.** | 0.1–0.4 mean over the measured routes; 3–5.7 during movement bursts (5 of 89 windows) | none (VBC dropped) |
| S2 | Cascade phase-2 redraw of direct (dynamic) casters (`shadows-culling.md` H1/A2) | **Written off: does not happen** | 0 | — |
| S3 | Empty VSM pages cleared/drawn | Already skipped for drawing; the clear is needed | 0 (clear < 0.13) | — |
| S4 | Depth-only shadow draws with a material PS (`SM-Sfb` + `SM-B0I2mIptrSfb#8`) | PS is an alpha test; a null-PS twin is exact only for draws with CB0[1].x ≤ 0 | ≤ 0.03 | not worth it |
| S5 | VSM lifespan / content-stamp refreshes of unchanged tiles | Not provably identical (the refresh is how late-streamed meshlets reach the page) | unknown, steady part of 0.18 | — |
| S6 | Per-cascade culling without early reject | CPU early-out exists per list; GPU chain cost is `shadows-culling.md` B1 | (B1) | — |
| S7 | Casters drawn into cascades that are already covered by a finer cascade | Exact only with map-based cascade selection; WDO#14 selection not decoded | ≤ 0.1–0.15 | open |

Measured Ally shadow cost (GP markers, mean over the windows where the row made the top 30): **VirtualShadowmap 1.8–2.0 ms/frame** (0.15–9.3 per window, bursty), of which **"Batch Tiles 8" self 1.2–1.4**; Cascade 0 0.15–0.19, Cascade 1 0.15–0.20 (self 0.05), Cascade 2 ~0.18 (each 2.0/frame = both occlusion phases); GenerateShadowMapHiZ 0.17. GP2 per PSO (L2 mean): SMTREE2 under Batch Tiles 0.200, `SM-BlrMfvMshltvSfb` (VS-only) 0.061, `SM-Sfb`/`SM-B0I2mIptrSfb` 0.050, SMTREE2 in cascades 0.038, `SM-BlrMfvMm00MshltvSfb` 0.013. GP2 windows carry PSO-stamp overhead, so absolute ms are upper bounds.

**Ally VSM path:** on the Ally the VSM runs the **CPU tile path** `VsmClipmapRender 1F8340` (mode 5; markers "VirtualShadowmap", "Clipmap %d", "Batch Tiles %d" exist only there). `VsmTilesPerFrameBudget 1F7960` is **8** at STANDARD..HIGHEST and 2 at LIGHTEST/LIGHT (ApplyShadowQuality), not 16. The VSM tile path draws **only static list A + tree/grass renderers** (`param_26 = 1`, list B never walked); dynamic casters go through the cascades.

## 1. S1: VSM batch draws beyond the tile budget

### Evidence (1F8340, Steam v2.02 RVAs)
- Per call (one clipmap), three loops over the clipmap's tile list (`view+4460h`, 16-byte entries {clip|flags, x, y}):
  1. **Clear loop** (call sites 1F87EB..1F886B): every tile that is dirty (`pm+41720` bit 0) and requested (`pm+40118` bit 2) is cleared. Empty tiles (`VsmTileIsEmpty 1F80C8`: caster stamp `TileList+38h[...] == 0`) are marked valid right there (`149468(..,0)` at 1F886B) and never drawn.
  2. **Batch loop** (1F8A91..1F9183, only when batch mode is on: `env+266C` and `local_857`): for every dirty, requested, non-empty tile, `budget = 1F7960()` (call 1F8AB2) is only tested for **!= 0** (`test edi,edi` before 1F8AF2). The tile is bound (`VsmTileBindViewport 1F96E0`, call 1F8AF2) and appended; every 8 tiles one `ShadowCascadeRender 1F5C2C` draws the batchable items (ctx+FCh = 1, so DrawListWalk 39E20 only takes items with `obj+80h != 0`). **There is no budget cap and no mark here.**
  3. **Per-tile loop** (1F9228..1F9537): same predicate, plus `local_844 < budget`; binds (1F92B8), draws the non-batchable items (ctx+FDh = 1: only `obj+80h == 0`), then **marks** the tile (`149468(..,1)` at 1F9537 -> `VsmPageMarkValid 14951C`: `40118 |= 3`, stamp, age 0). At most `budget` tiles per frame become valid.
- So when N dirty tiles arrive, tiles past the first `budget` bound ones are cleared and batch-drawn every frame until their turn: stock draws N + (N-B) + (N-2B) + ... tiles instead of N.
- **The work is never seen.**
  - `VsmFlagsUpload 912A2C` memcpys `pm+40118` into the per-frame GPU buffer `tVirtualShadowMapFlags`.
  - Every VSM consumer gates **each tap** on bit 0 of the flag word of the tile that tap lands in. For example, WDO-PtSrbSsmVsm#14 (0x1667d3a0.file): `bufferLoad(flags, tileIdx(tap))` -> `and 1` -> select, per `sampleCmpLevelZero`.
  - `vsm_consumers.py` checked **all 356** ps/cs variants that bind `sVirtualShadow`: RefProbe 20, VFSCAT 120, WDO-PtSrbSsmVsm 108, WDO-PtSrbVsm 108. All are gated.
  - Bit 0 is set only by 14951C (single caller chain 149468 <- 1F8340). Invalidation clears it (`VsmTileInvalidate 79150`: `40118 &= ~3` for scroll 100h, lost request 200h and forced 80h).
  - A tile that is invalid when it is batch-drawn and is not marked this frame is therefore never sampled. The next frame's clear loop clears it again before any draw that can make it valid.
- **Draw scissor.** 1F96E0 sets the tile scissor to {1,1,w-2,h-2} when `clip+922h == 0`, so batch draws touch only tile interiors. A bilinear compare tap attributed to a valid neighbour reads at most that neighbour's border texel, which the unchanged clear loop writes identically. (The 86904C = scissor setter mapping is inferred.)

### Why skipping is texel-exact (the cross-frame side effects in §1b break full exactness)
- Skip a tile in the batch loop only when **(a)** `budget` tiles have already been bound in this call, **and (b)** its `40118` bit 0 is 0 (invalid now).
- Order and binding predicate are the same in both loops (1F96E0 is deterministic per tile within a call). So the per-tile loop marks exactly the first `budget` bound tiles, and all of them are still batch-drawn. Batches 1..ceil(B/8) are byte-identical to stock.
- Tiles that are valid but dirty (lifespan 10h or stamp change 20h keep bit 0 set while re-rendering) are left as in stock.
- `local_858`, which ANDs the batch success flags, cannot change. Every batch walks the same union list `531E6C(view, count)`, so a dropped batch has the same readiness as the kept ones.

### Estimated Ally saving (size of the redundant work; not achievable bit-exactly, §1b)
- With budget B and T tiles per VSM frame, the redundant share is s = max(0, 1 - B/T) (aligned burst model, script `vsm_batch_waste.py`).
- At B = 8 (Ally STANDARD) the Batch Tiles self time saved over the 89 windows is **0.12 ms/frame frame-weighted and 0.36 ms/frame as a window mean**.
- In the 5 movement-burst windows (T = 36–62 tiles per VSM frame, VSM 5–9 ms/frame) it is **2.8–5.7 ms/frame**. These are the frames that drop under 60 fps when the camera turns or teleports.
- Marker times include GP2 stamp overhead in some windows, and per-window counts are rounded to 0.1/frame. Treat this as ±50 %.
- The saving also removes the dropped batches' culling dispatches (`23584C` per batch).

### Implementation sketch (VBC; not built, see §1b)
- Flag `[Patches] VBC = 0` (default off). It is v2.02 only and installed at launch.
- Four 5-byte rel32 call redirects to C thunks, using thread_local state:

| site | orig call | thunk |
|---|---|---|
| `VBC1` 538F62 (535188 -> 1F8340) | `VsmClipmapRender` | `g_bound = 0; g_budget = 0;` then orig |
| `VBC2` 1F8AB2 (batch loop) | `VsmTilesPerFrameBudget` | `g_budget = orig(p);` return it |
| `VBC3` 1F8AC2 (batch loop) | `VsmTileIsEmpty(pm=r14, list=r13, tile=rbx)` | `r = orig(); if (!r && g_bound >= g_budget && !(*(u32*)(*(u64*)(pm+40118h) + 4*idx) & 1)) r = 1; return r;` with `idx = ((tile[0]&0xF)*[pm+400E8h] + tile[2])*[pm+400E4h] + tile[1]` (same index as 1F8094) |
| `VBC4` 1F8AF2 (batch loop) | `VsmTileBindViewport(clip=rsi, ctx=r15)` | `r = orig(); if (r) ++g_bound; return r;` |

- Patterns (dump bytes):
  - 1F8AAB: `49 8B 8A 30 3D 00 00 E8 ?? ?? ?? ?? 4C 8B C3 49 8B D5 49 8B CE 8B F8 E8 ?? ?? ?? ?? 45 33 DB 84 C0 0F 85 ?? ?? ?? ?? 8B 53 08 8B 4B 04 89 8E 98 03 00 00 89 96 9C 03 00 00 85 FF 0F 84 ?? ?? ?? ?? 49 8B D7 48 8B CE E8`
  - 538F5B: `48 8D 95 80 50 00 00 E8`
  - `test_patterns.py` should also check each rel32 target (1F7960 / 1F80C8 / 1F96E0 / 1F8340).
- Self-check, debug only: a fifth thunk on the mark call 1F9537 asserts that the tile was bound in the batch loop this frame. Count `VBC skipped tiles/frame` in the stats line.
- Inert in VSM mode 6 (1F8340 returns early) and when batch mode is off (batch loop not entered).
- **Validation.** Do an Ally A/B on a camera-pan / teleport route and compare GP "Batch Tiles 8" /frame and incl against VBC=0. Visual: shadows must match frame-for-frame, and a screenshot diff on a scripted pan should be bit-exact.
- **Risk (texels only):** output to all sampled texels is proven unchanged. The remaining risks are a wrong thread_local assumption (535188 can run per view) and the scissor inference. Both are covered by the self-check and the screenshot diff.

## 1b. S1 exactness check: fails, VBC not built

The check used Ghidra on the v2.02 dump plus an Ally PSO dump (shader asm). The game folder was not touched. Some parts of the §1 argument hold; the cross-frame side effects break it.

**Holds**
- **Index.** 149468 marks `40118` at the raw index `((c)*H + y)*W + x`. It is the same index that 1F8094 and 1F8168 read. 796A8 converts only the copy used for the stamp lookup.
- **Budget.** It comes from 1F7960 at every tile in both loops. Per SHADOW_QUAL row it is 2/2/8/8/8, and the node default is 16. It is never 0, so the "budget tiles always drawn" part holds at every quality level.
- **Bind order.** 1F96E0/1F7A44 are pure functions of the tile coords (`clip+398/39C`), the clip config (`+828/82C/8A0/8A4/1EC/210/198`) and the RT size. They give the same result in the batch loop and the per-tile loop. `clip+220` and `clip+394` are the same in both loops.
- **Duplicates.** The design must count **distinct** bound tiles. The 1st-occurrence argument then holds even if the list has duplicates. The plain counter in §1 does not handle duplicates.
- **SceneRenderView draw count.** The count drops under VBC, but its consumers only test `!= 0` (10F060:1828, 33D0CE4) or ignore it (1CFD1C takes 2 args). The first batch is always kept, so `!= 0` is preserved.
- **Batch readiness.** `local_858` (1F5C2C clears `+1EE358` when `ctx+FEh` is not ready) is the same as long as readiness is monotone within the loop.

**Fails**
1. **Sampler feedback (decisive).**
   - The batch-tile PSOs write texture-streaming feedback.
   - `SMTREE2-BlrMfvMshltvSfb` (3a5c2c88…, the 0.200 ms SMTREE2 under Batch Tiles) and `SMTREE2-I2mSfb` do a `bufferStore` to `uGeneral_FeedbackMap` (u0). So do `SM-BlrMfvMm00MshltvSfb`, `SM-BlrMfvMshltvReSfb`, `SM-SfbUh`, `SM-MdbotSfb` and `SM-BlrMdb5tltotMfvMshltvSfb`. "Sfb" = sampler feedback.
   - That map drives material texture streaming (`meshlet-mainthread.md` §1: 1290B8 tile requests).
   - Skipped draws for waiting tiles therefore remove mip requests. A caster that is only in waiting tiles (often off-screen, so no main-view feedback) streams later.
   - When its tile's turn comes, the VBC tile is rendered and **marked valid** with coarser mips. The alpha test runs through the `sGeneral_MinMipMap` residency clamp, so the leaf silhouettes differ.
   - Nothing re-dirties the tile for a texture-residency change (20h is the caster stamp only; 10h lifespan comes much later). Valid shadow texels can therefore differ for many frames.
2. **Meshlet cache codes.**
   - The culling dispatched per batch (23584C) uses the variants with `tPageFlags`. They write `uCacheCodes` (store 1) and `uExistsNotLoadedModel`.
   - The streaming pass (`tClusters tMeshletResourceOffsets tG1mVisFlags uRequestCmds uCacheCodes uIsMappedCmds`) reads those codes.
   - Dropping the culling of waiting tiles delays meshlet requests (and may affect LRU) for geometry seen only there. This has the same "rendered-valid with less resident data" consequence.
   - Keeping the culling and skipping only the draws would not fix item 1.
3. **Sub-light shadow throttle (mitigable, listed for completeness).**
   - `local_818` (DrawListWalk count via `param_2[0xE]`) includes every batch draw, because `local_834` is snapshotted before the batch loop.
   - At 1F95BF it is compared with node param `37B3FAE9` (5F6C44: default -1 = off; level node data can set it). If it reaches that value, `obj+25Ah` is set, which 542C08 ("SubPointLightShadows") uses to defer sub-light shadow updates.
   - A VBC would have to stay inert whenever that param is ≥ 0.

**Conclusion**
- The work past the budget is not observable as texels, but it is observable through streaming. That breaks the project's bit-exact rule.
- No `VBC` flag, sites or code exist in the ASI.
- A "same look within streaming jitter" variant would need an in-game screenshot A/B and an explicit decision that streaming-timing differences are acceptable. That is not an offline proof.
- **If it is revisited:**
  - Bind hook only (asm shim at 1F8AF2 passes `rbx` tile, `r14` pm, `edi` budget), a distinct-tile set, and the entry reset at 538F62 (1F8340 takes 2 args).
  - Inert when `37B3FAE9 ≥ 0`.
  - Skip only when there are ≥ budget distinct tiles and `40118` bit 0 is 0.
  - Do not override 1F80C8. That would also drop the `clip+398/39C` writes that the final flush runs with.

## 2. Write-offs

- **S2: cascade phase 2 re-draws dynamic casters (H1). False.**
  - The second cascade loop in SceneRenderView 535188 (1F5C2C call at 5389BB, after `GenerateShadowMapHiZ` at 538592) sets `local_7f9c = 1` (dec_535188.c:1461). That is **ctx+FCh** of the draw context `local_8098[252]`.
  - DrawListWalk 39E20 (lines 227–228) then walks only items with `obj+80h != 0`, i.e. GPU-culled meshlet items for the two-phase re-test.
  - Direct draws (lists A/B, characters) are drawn once, in phase 1. The main view does the same (lines 2136/2624).
  - A2 in `shadows-culling.md` is void.
- **S3: empty VSM pages.**
  - Empty tiles (stamp 0) are cleared and marked in the clear loop and never drawn. Non-empty pages draw only meshlets that pass the per-tile `tValidRectCountCumulativeMap` cull.
  - The clear is required: the physical tile is reused toroidally.
  - "Clear Tile" never reaches the GP top 30 (< 0.13 ms). Removing it would need a new "valid+empty" flag semantic in 356 shader variants. Not worth it.
- **S4: depth-only draws with material PS.**
  - `SM-B0I2mIptrSfb#8` (0xf999073d.file, 0x8450) has no outputs. Its logic: if `CB0[1].x > 0`, it samples alpha (with the residency min-LOD clamp) and computes `a = sat(a*CB0[2].y + CB0[2].z)`, else `a = 1`. It discards if `a < CB0[0].w`.
  - It is a no-op only for draws whose material has no alpha texture. A null-PS twin PSO would need the per-draw CB0 contents at replay time.
  - The whole group is 0.05 ms on the Ally. The PS share is ≤ 0.03 ms. Not worth a replay-time cbuffer peek.
  - The VSM static draws are already VS-only (`SM-BlrMfvMshltvSfb`, `SM-BlrMfvMm00MshltvSfb`). SMTREE2's PS is a real leaf alpha test.
- **S5: VSM refresh of "unchanged" tiles.** The page manager `VsmPageMgrUpdate 465210` re-dirties a valid tile in these cases:
  - **20h** — the GPU caster stamp changed (`VsmTileStampCompare 78DE0`), debounced by `pm+81D0C`.
  - **10h** — the age reached `VsmTileLifespan 77924` (env 0x47C1C9CC per clipmap). The age starts after the 4 -> 8 -> 10h settle in `VsmTileAgeAdvance 79074`.
  - **40h/80h** — per-clipmap forced invalidation (`VsmClipmapForceInvalidate 23ADEC`).

  20h is real change. 10h exists to pick up content the stamp does not capture (meshlet residency and LOD arrive later: MeshletCheckRequestLoad). So a skipped refresh is not provably identical. Written off. Measuring it first needs a counter on 79150's 10h branch.
- **S6: per-cascade culling early reject.**
  - 1F5C2C records MIC/MPC only if list A or B has a non-zero atomic count (`+18h`), and the shadow Hi-Z + loop 2 run only if loop 1 recorded culling (`local_cde0 != 0`). So the CPU early-outs exist.
  - The remaining cost is the fixed latency-bound MPC chain (`shadows-culling.md` B1, Ally MeshletPreculling 1.1–1.5 ms/frame over all views).
- **S7: cascade containment (open).**
  - Skipping a caster in cascade k when its light-space XY bounds lie inside cascade k-1 is exact only if the resolve picks the finest cascade whose map contains the receiver (map-based selection), with no blend band.
  - WDO-PtSrbSsmVsm#14 (3852 instructions) was not decoded far enough to tell that apart from view-depth splits.
  - The cap is ≤ 0.1–0.15 ms (the cascade draw part of ~0.5 ms). It also needs the per-cascade list builder on the KIDS threads.
  - Next step: decode the `sShadow` selection in `shaud_wdo14.ll` (cbuffer regs 4–16 = cascade matrices).

## 3. VSM tile state (page manager `pm = base+16C640`, per tile u32 arrays)

| array | bits |
|---|---|
| `+40118` request/valid (uploaded as `tVirtualShadowMapFlags`) | 0,1 valid (set by 14951C, cleared on invalidate); 2 requested (`VsmTileRequestUpdate 14957C`) |
| `+41720` state | 0 dirty; 1 rendered-valid; 2 just rendered; 3 settling; 4 lifespan-tracked (10h); 5 stamp changed (20h); 6/7 forced (40h/80h from pm+40044 per clipmap); 8 outside window (100h, `VsmTileOutOfWindowFlag 78C44`); 9 lost request (200h); 17 (20000h, 465280); 18 in frustum (40000h, 77AB0) |
| `+41718` | caster stamp at render (compare with `TileList+38h`) |
| `+41728` | bits 0–11 age (lifespan), 12–23 frames since stamp change |

Sizes `+400E4` W, `+400E8` H, clipmaps `+400D8`/`+400F8`, first `+40100`; arrays (re)allocated in `VsmPageMgrResize 4669E8`.

## 4. Files (private dev repo)
| File | What |
|---|---|
| `gp_marker_digest.py` | mean per-marker ms from GP marker tables (`<regex> <logs>`) |
| `vsm_batch_waste.py` | per-window VSM/batch counts and the S1 saving model |
| `vsm_consumers.py` | proves every sVirtualShadow consumer gates taps on flag bit 0 |
| `q3_vsm_state.txt` | read-only Ghidra decompiles: 14951C, 79074, 78DE0, 23ADEC, 466804, 4669E8, 466238, 912A2C, 14957C, 32F2730, 32F7588, 32F7CB0 |

The names above are in symbols.tsv (1F80C8 is VsmTileIsEmpty).
