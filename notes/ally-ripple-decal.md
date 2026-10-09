# Ally "RippleDecal" lead (bridge #969): root cause and verdict (2026-10-09)

**Verdict: no fix, drop the lead.** The 1.92 ms "saved" was a measurement artifact. Skipping this pass breaks the whole
G-buffer, and the broken G-buffer makes later lighting passes cheaper. The pass really costs about **0.55 ms** on the Ally
(GP2, USER config). That is about 4x pc2 per pixel, which matches the Ally's memory bandwidth. There is no RADV or vkd3d
slow path. No build was made, so there is nothing to A/B.

The SKY_all and PB2_be0f leads from the same run are also not same-look wins (section 4).

## 1. What the pass is
- The group label "PB2RIPPLE / PB2DECAL" comes from GP2 naming the VS file. The VS 563edb9d is a shared fullscreen VS. The
  matched PS **c013d811** is `PB2DECAL-CdlRgbSfb` (0xc5bc597a.file record@460856, dxil hash 199089ea). It is the
  **clustered deferred-decal G-buffer pass**. It is not the rain ripples: the PB2RIPPLE PSes (41c111f2, d2e92a52, f4e280dc,
  f6bfe4c4) were not in the group.
- Per pixel, the PS does this (disassembly: `gpu-shaders/materials/work/0xc5bc597a_16.ll`):
  - Samples depth and picks a log-Z cluster slice. Reads the cluster header (count and offset) from `tCdlDecalIndices`.
  - Samples GBuf0-3. Decodes the octahedral normal into world space.
  - Loops over the cluster's decals. Each decal reads about 52 typed-buffer attribute loads plus up to 10 texture arrays
    (albedo, normal, roughness, occlusion, emissive, each with a "second" set). It also clamps mip levels against the
    sampler-feedback min-mip buffer.
  - Re-encodes the normal and writes **4 MRTs**. A pixel with no decals still reads all 4 G-buffer targets and depth, and
    still writes all 4 targets.
- PSO state (new tool `re/scripts_gpu/pj_psodesc.py` on the PC PSO journal; 8 variants):
  - RTVs are B8G8R8A8_UNORM_SRGB, B8G8R8A8_UNORM, R16G16B16A16_UNORM and R11G11B10_FLOAT. DSV is D32_S8.
  - **Blending is off.** The draw overwrites its targets.
  - Depth test is off. **Stencil is EQUAL with read masks 2f / fb / fd / fe.** Each draw is a fullscreen triangle for one
    stencil material class.
  - The SkipFx ack shows ~4 draws/frame (1170-1208 skipped in 8 s).
- Because blending is off and the pass reads the source G-buffer while writing the destination, it is a G-buffer
  read-modify-write ("ping-pong"). Its targets are the G-buffer that lighting reads.

## 2. Why the 1.92 ms is not the pass's own cost
- Skip shot `captures/ally/cvl_1749/diffs/1_15_BASE_vs_1_19_RippleDecal.png`: the whole ground goes black and the player
  turns white. 73 % of pixels change by more than 8/255 (BASE-vs-BASE: 34 %). Decals are not "missing": the G-buffer the
  lighting reads is stale or garbage. Lighting stale or garbage data is cheaper, so the 1.92 ms includes downstream work.
- The pass's own GPU time on the Ally is 0.547 / 0.562 ms (S1 GP2, USER cfg 960x540, 2 intervals/frame;
  `captures/ally/gpu_budget_1531`). The 1.7.5 MAX-preset digest showed 1.19 ms.
- On pc2 it costs 0.216 ms at 1248x702 HIGH (4.8 draws). Per pixel that is 0.25 ns (pc2) vs 1.06 ns (Ally), about 4.3x.

## 3. Why the pass is relatively expensive on the Ally, and why there is no same-look lever
- **It is bandwidth-bound.** Each pixel reads about 24 B (4 G-buffer targets plus depth) and writes 20 B. At 960x540 that is
  about 23 MB per frame, or about 0.3 ms at the Ally's realistic ~80 GB/s of shared LPDDR5.
  - pc2's G-buffer fits in its 32 MB Infinity Cache. The Ally (780M) has none, so a 4x per-pixel gap is expected.
- **Occupancy is low.** An offline RGA compile for gfx1103 (PAL proxy; the ACO compiler on the Ally may differ) gives:
  - 222 VGPRs at wave64, so about 4 waves per SIMD, set by the decal loop.
  - No scratch.
  - Per decal: 52 `buffer_load_format_x`, 17 image samples and 1 image load.
  - ISA saved as `captures/ally/cvl_1749/diffs/decal_c013d811_rga_gfx1103_isa.txt`.
- **Candidate causes from the brief, each checked:**

  | Candidate | Finding |
  |---|---|
  | Small decal draws with blending | Ruled out: 4 fullscreen stencil draws, blending off. |
  | Overdraw of decal volumes | Ruled out: no volumes are drawn. |
  | DCC format conversion | Ruled out: the formats are native and the targets are fully overwritten. A depth/HTILE decompress, if any, happens at the barrier, which SkipFx does not skip. |
  | vkd3d root-descriptor or typed-UAV path | Ruled out: the pass has no UAVs, and typed SRV buffers are native texel buffers. |
  | Dynamic-index temporaries (36 two-float allocas) | Ruled out: these are 8 B each, below RADV's 256 B scratch threshold, so they become selects, not scratch. |
  | ACO codegen | Cannot be ruled out. No RADV dump is available, but the cost ratio does not need it. |
- **No fix within the rules:**
  - The ~0.3 ms copy floor stays unless the engine renders in place, which needs the CPU side to rebind targets. That is not
    same-look-safe.
  - Shader tuning (a wave-uniform no-decal fast path, a lower-VGPR loop) can save at most ~0.2 ms (<1 % of the frame).
    That is inside the cost-vs-looks noise (±0.2 ms) and not provable without many ABBA runs.
  - Nothing was built. Windows was not affected.

## 4. Other leads from #969
- **SKY_all (0.95 ms): visible, not a same-look win.**
  - The sky is in view (upper right). Skipping it washes the sky out and brightens the scene (`diffs/1_00_BASE_vs_1_05_SKY_all.png`).
  - Pixels changed by more than 32/255: 17.9-18.4 %, against a BASE-vs-BASE floor of 9.5 %. Mean abs diff: 12.5-13.0 vs 7.4-7.6.
  - Cost breakdown on the Ally (S1 GP2): DrawSky = VCLOUD async CS 1.52 ms wall (frame cost 0.53 ms per the VCLOUD skip
    group) + sky draw 0.14 ms.
  - "Lower rate or lower resolution with reprojection" is already what the engine does: VCLOUD runs at render/8 and updates
    1/16 of its pixels per frame with Bayer reprojection (`notes/gpu-volumetrics-rewrite.md`). The VCLOUD hoist rewrite was
    already rejected at ≤0.05 ms (`notes/ally-gpu-budget.md` 3.4). CLDR and SKYR are blend weights or already time-sliced
    (`notes/same-look-package.md`).
  - Nothing is left to amortise at the same look.
- **PB2_be0f (0.67 ms): visible.** It is the **player character's** material family: the skip shot removes the player's
  body (`diffs/2_00_BASE_vs_2_04_PB2_be0f.png`). Whole-frame SSIM hides it because the character is a small part of the
  frame. It is a cost reference, not a removal candidate.
- **Tool issue.** The Ally port `cvl_ally.py` judges looks by whole-frame SSIM only. Particle noise (BASE-vs-BASE SSIM
  0.74-0.83) hides local but obvious changes. It should also report the excess-pixel metric (% of pixels changed by more
  than 32/255 vs the BASE floor, as `cost_vs_looks.py` does). It should mark G-buffer producers as invalid skips (SSIM < floor - 0.2).

## 5. Files
- `captures/ally/cvl_1749/` (bridge file #165, extracted; tgz deleted): `run1/table.txt`, `windows.csv`, `mod.log`, shots, `diffs/`.
- `re/scripts_gpu/pj_psodesc.py`: prints the PSO state for a shader hash from a PJ journal.
