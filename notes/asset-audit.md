# Asset audit: are Nioh 3's meshes/textures/collision built inefficiently? (2026-10-07)

**Verdict: no meaningful lossless asset win.** The assets are clean (0.5 % duplicate vertices, index buffers already at
Forsyth/meshoptimizer quality, all textures BCn with full mip chains, 0 duplicate textures, collision 1 % of render
triangles). The one real inefficiency, half-empty meshlets, is worth about **0.04 ms/frame on the Ally (~0.1 %)** if
fixed losslessly. Fixing it means rebuilding cluster-LOD tables in 39 GB of streamed data, so it is not worth doing.
Analysis only: nothing was installed and the game was not run.

## Sample
- Futamata: `S01_SEN_02_FUTAMATA` placements (`28d5c108`, 3,725 REFs) resolved through REF → `Placeable::Model::WorldPQ`
  (`9d683d24`) → `Displayset::Model` (`b157c8a0`) → ModelData KTID. That gives 397 models and 3,724 instances. The
  `S01_SEN_02_BASE` stage DBs and its ProceduralPlacement `eda0ea78` (the foliage and trees behind PB2TREE2) add 96
  models. Total: 493 models (~300 MB), 66 collision files, and 417 of the 2,290 Futamata textures (3.26 GB).
- Copies are in `D:\nioh3-re\assets\` (`m\`, `col\`, `tex\g\`). The results are in `audit.tsv` and `audit_proc.tsv`.

## Format (RE'd here; G1M `_M1G` v9300, G1MG v0045)
- G1MG subsections 0x10001–0x10016. VB 0x10004, layout 0x10005, IB 0x10007, submesh 0x10008 (14 u32, flags 0x4000 =
  meshlet). The meshlet sections:
  - **0x1000c**: meshlets {triOff, triCnt, vtxOff, vtxCnt}.
  - **0x1000b**: triangles, one u32 per triangle = 3 × 10-bit meshlet-local indices.
  - **0x10007**: for meshlet models (flags 0x80000000), the meshlet vertex list (u32 global VB index).
  - **0x1000f**: cluster-LOD level starts (3–8 levels).
  - **0x1000e**: per-meshlet class, parent/child refs, bounding sphere and error.
  - 0x10010–0x10016: groups.
  - Decoding is verified: 0 out-of-range local indices over 31 M triangles.
- **How the meshlets are drawn** (DXIL of PB2 VS `70d0ee9f…` and PB2TREE2 VS `9429db01…`): the draws are non-indexed.
  `VertexID % 384 / 3` is the triangle slot, so **every meshlet costs 384 VS lanes (128 triangle slots × 3 corners)**.
  Slots past `triCnt` early-out after the header loads. On the PB2 VS the early-out path is ~45 % of the full VS's loads;
  on the tree VS it is ~10–20 %.
  - Consequence: in the meshlet path, vertex-cache order (ACMR) and duplicate vertices cost nothing.
  - Triangle order (overdraw) is also moot, because the meshlet output order comes from culling atomics.
  - **Meshlet fill is the only geometric lever.**
- G1T v0066 (`GT1G`): StreamingTexContext keeps the small mips zlib-compressed in the file, and the top mip(s) are
  streamed separately. G1CO: `HVB_` BVH with {nNodes, nodeOff, nTris, triOff, nVerts, vertOff}.

## Findings
| # | What | Evidence | Ally impact | Lossless fix? |
|---|---|---|---|---|
| 1a | Duplicate vertices | 0.53 % of VB vertices are byte-identical (166 k of 31.4 M, instance-weighted) | ≈0. In the meshlet path they cost only memory | Yes, but pointless |
| 1b | Index order (non-meshlet ModelData) | Instance-weighted ACMR (FIFO-16) **1.043** vs **1.066** after Forsyth. Big meshes: 0.66/0.87/0.90 vs 0.69/0.89/0.90. Already optimised | 0 | Nothing to gain |
| 1c | **Meshlet fill** | Avg **56 %** of the 128 slots (LOD0 47 %). Only **63.6 %** of VS waves contain any triangle. Foliage/trees: 46 % fill, LOD0 40 %, 54 % of waves used. Cause: the builder caps meshlets at **128 vertices** (`max_vtx` = 128 on 172/223 models), but meshes average 1.26 meshlet-verts/tri (foliage 1.47, cards with split UVs/normals). So the vertex cap binds long before 128 triangles, even though the VS path has no vertex budget at all (local indices are 10-bit) | Merging only meshlets that are always selected together (same LOD level, same class, same parent/child refs) removes **21.7 %** of meshlets (foliage **26.7 %**); the unconstrained bound is 43 % / 53 %. That saves ~12 % of PB2 meshlet VS work and ~5 % of tree VS work. Estimated **~0.04 ms/frame** (VS assumed ~40 % of the 0.645 + 0.337 ms PB2/PB2TREE2 draw time in GP2), plus some degenerate-primitive culling (not measured) | Possible in principle: append-and-repoint per StreamingMeshletModelData entry (n3inject) with exact revert. But it means rebuilding 0x1000c/0x1000b/0x10007/0x1000e/0x10010–16 consistently, plus bounds and cones, for 11,856 entries (39 GB). The LOD-group semantics are only partly decoded. **Not worth it.** |
| 2 | Textures | 417 sampled: BC1 352, BC7 38, BC4 9, BC6H 2; uncompressed only for 16 1×1 constants and 3 tiny R32 textures. **0** incomplete mip chains (each chain ends at 4 px on the short side). **0** exact duplicates. Sizes: 174 × 2048², 12 × 4096², 135 × 1024² (report only) | Streaming keeps only the needed mips resident, so the sizes cost VRAM/IO, not GPU time | Nothing lossless to fix |
| 3 | Tiny draws | Futamata places **2,402 instances of 266 distinct one-quad ModelData** (2 tris, 4 verts: decals/cards). Each is its own model, so they are drawn directly and not occlusion-culled. These match the 262 tiny alpha-tested direct draws/frame (shadows-culling.md B4) | CPU ~0.3–0.45 ms main thread (B4 estimate); GPU tiny | **No data-only fix.** Different models/materials per quad. Batching or instancing is engine-side (B4). Merging quads into one model would change per-object culling/fade, so it is not lossless at the data level |
| 4 | Collision | 66 G1CO files used by Futamata displaysets: **46 k collision tris vs 4.65 M render tris (1.0 %)** instance-weighted, max 871 tris per file. Many are 12-tri boxes. Nothing over-detailed. The collision CPU (worker-jobs-opt.md) is per-collider part bookkeeping (cold cache lines), not triangle tests | — | Not needed (and changing collision would change gameplay) |

**PoC reorder: not built.** No reorder has a payoff in this format:
- The meshlet path is non-indexed, and its draw order is atomic-driven.
- The indexed meshes already match Forsyth.
- The only lever is a meshlet merge. That is a DAG rebuild, not a reorder.

## Scripts (`re\scripts_assets\`, all `py -I`, stdlib only)
`stage_models.py` (placements → ModelData), `g1m_chunks.py`, `g1mg_sections.py`, `g1m_audit.py` (fill, waves, merge
bounds, dup verts, ACMR vs Forsyth), `audit_summary.py`, `g1co_stats.py`, `stage_textures.py` (displaysets → G1T),
`g1t_stats.py`.
