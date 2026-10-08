# Runtime-compiled post shaders (Ally dump 172): inventory + bit-exact rewrites for psoswap

2026-10-07. Input: `D:\nioh3-re\ally-dump\shaderdump_172.zip` (psoswap DumpMode=2, Proton, MAX settings; 1812 blobs,
extracted to `D:\nioh3-re\ally-dump\shaderdump_172\dump`, disassembly cache `...\asm`). Game not run.
**Not available:** `captures\ally\nioh3_ally_modlog_172-A_1915.log` is not on this PC (searched C: and D:), so the
inventory uses bytecode, names and the r5 RGP map only, with no GPUProf markers and no per-PSO Ally times.

## 1. Inventory (`gpu-shaders\post\out\runtime_inventory.csv`, `py -I runtime_inventory.py <dump>`)
- Columns: dump file / container digest, DXIL HASH digest, stage, source, role, size, instructions, samples / loads /
  gathers, loops, r5 µs, resources, cbuffer members.
- Where the 1812 blobs come from:
  - 1653 are on disk (`shader_dx12`, `dxil_index.csv`): materials and RT.
  - 104 are in Nioh3.exe (`out\inventory.csv`, see gpu-post-exe.md).
  - **55 exist only at run time:** 45 DXBC (fxc 10.1) and 10 DXIL.
- Known digests:
  - 17149dca bloom downsample (4 taps) and 18a4c2bb bloom blur (9 taps) are DXBC PS (VS c5909527).
  - 6a305be4 is the light composite (no CA) seen in r5 (VS d21b4b07).
  - **df4adc57 is not in the dump.** That PSO was not created in this session / at MAX.
- The runtime post set at MAX (all **DXBC SM5**, so psoswap replacements are `.dxbc`, not `.dxil`):

| pass | shaders | shape |
|---|---|---|
| motion blur | 615daafd velocity decode → **f11afe97 tile max** → 76a3c427 neighbour max → **4e13e236 gather** (VS d8fc9faf) | gather: 2·N iterations × (colour + velocity) samples, N = floor(min(len/2, g_cbMbParams0.w)) ("10 samples" cap); tile max: n×n point samples per tile, serial |
| DOF | CS 3dd31ae3 / e3cb5cbe (bokeh sprites), sprite VS/PS 01da6d84 / 0ddd389c / aaee9c94, CoC 51dd7477 / b5dfef9e, 745a18ff, baff8a76, e7318142, **cf792d56 gather + composite** (VS 523efaab) | cf792d56: (2R+1)² kernel from the g_tSampleInfo table, per tap 1 load → 2 dependent samples, all pixels |
| bloom | 7139b08f bright pass, 17149dca, 18a4c2bb, b5b4638c | straight-line 4-9 taps |
| composite | **04c1052d** (+ dramatic LUTs, depth), **e6efc8ff**, 6a305be4, 278e3bad (radial blur) | 04c1052d / e6efc8ff: chromatic-aberration loop, clamp(len, 3, 16) taps, serial (CA is ON in the Ally config) |
| other | lens flare 796e2cdd / b94af32d, eye adaptation CS 92075474 / a1712355 / 6ee82656, heat haze 9ea0865d / f3596c47, HDR LUT CS 0c73c30e, runtime DXIL CS 8986d40f (edge/outline, 932 instructions) | — |

## 2. Rewritten (5 PS; same interface, same ops in the same order; only *when* fetches are issued changes)
Pixel shaders have no groupshared memory, so the TOONEDGE2 tiling does not apply. The win is load batching: each
loop below waits for its own fetch every iteration. The rewrite issues several iterations' independent fetches
together, then runs their ALU in the original order with the original ops. Only full batches are taken, so there
are **no extra fetches** except in tile max (point samples on a low-resolution pass).

| file (orig digest) | pass | change | fetch rounds / px (interp.) | VGPR (RGA gfx1103, PAL) |
|---|---|---|---|---|
| f11afe97 | MB tile max | 8 rows per batch; past-the-end rows re-sample row 0 (never selected) | 93 → 15 | 12 → 24 (16 waves) |
| 4e13e236 | MB gather | 2 step positions (4 iterations, 8 samples) per pass; the last position alone takes 4 samples | 7.4 → 3.3 | 32 → 56 (16 → 9 waves) |
| cf792d56 | DOF gather + composite | 4 taps per pass (4 table loads, then 8 samples), row-major order kept; last 1-3 taps as shipped | 80 → 23 | 28 → 48 (16 → 10 waves) |
| 04c1052d | composite + CA | 4 CA taps per pass while 4 remain; the rest of the shader is a mechanical re-translation (`dxbc2hlsl.py`) | 16.9 → 11.3 | 36 → 48 (14 → 10 waves) |
| e6efc8ff | composite + CA | same | 14.3 → 8.7 | 36 → 48 |

Not rewritten, and why:
- **Bloom and CoC (17149dca, 18a4c2bb, b5b4638c, 7139b08f, 51dd7477, b5dfef9e, 745a18ff, baff8a76):** straight-line
  fetches. The driver already issues them together. The work is pure filtering bandwidth.
- **6a305be4:** no loop, 5 fetches.
- **615daafd, 76a3c427:** 2-4 fetches.
- **278e3bad radial blur:** a serial loop with a data-dependent early exit. Batching it would need speculative
  samples, and the pass only runs during a gameplay effect. It is the next candidate if GP2 shows it.
- **8986d40f:** a runtime DXIL CS, out of scope for this pass.

## 3. Proof
- **Interpreter.** `dxbc_interp.py` runs SM4/5 bytecode from D3DDisassemble with hex literals (exact constant bits),
  per component as uint32 with IEEE float32 ops. Its models are strict, so op substitutions show:
  - `mad` is an exact single-rounding FMA, so it differs from mul+add.
  - `div`, `sqrt`, `rsq`, `exp`, `log` and `sincos` get an operand-hash bit flip, so algebraic rewrites of them do
    not match.
  - `sample_l` is bilinear with a hash of the exact coordinate bits mixed in, so any change in coordinate bits shows.
  - `ld` out of range returns 0.
  - The bit comparison includes NaN payload and sign.
- **Verification run.** `verify_post.py` covered 400 scenes per shader:
  - Random texture sizes and contents, including quantised values, duplicate magnitudes (first-max ordering),
    NaN / ±Inf / denormals / −0, and 65024 clamps.
  - Random cbuffers: tile sizes 0, 1, 2.5, 7.9, 8.0001, 16, 20, −1; kernel sizes 0-16; radius −1-100;
    every CA count; the dramatic-LUT bit fields.
  - SV_Position and UV come from the full-screen VS, plus off-grid points.
- **Result: all 5 BIT-EXACT, 0 mismatching pixels out of 51,200.** Every shipped instruction was executed
  (coverage 28/28, 92/92, 112/112, 288/288, 159/159). Output: `gpu-shaders\post\out\verify_*.txt`.
- **Mutation checks.** These changes are caught: `<=` for `<`, missing validity gating, an unclamped tile row, a
  changed step, mul+add for mad, swapped accumulation, and a wrong constant in the first DOF reconstruction (fxc wrote 0x40490fdc for
  π; the shipped value is 0x40490fdb). Equivalent edits are correctly not flagged, for example re-comparing a
  duplicate sample.
- **Reconstruction.** The plain reconstruction of f11afe97 recompiles to the shipped body instruction for
  instruction. The fxc reorder of the V2W row (mul/mad order) was found by divergence tracing and fixed with the
  original matrix multiply.
- **D3D12 check.** `pso_check.py` → `ALL PSOS OK`. This PC's D3D12 runtime builds a graphics PSO from the game's
  VS + the replacement PS for all 5. This checks DXBC signing and VS→PS linkage; ISGN/OSGN are byte-identical.
- **Not done: GPU execution.** As in the sweep proof, the interpreter proves the bytecode computes the same function.
  Each output uses the original's op patterns, so ACO / PAL codegen of the arithmetic is the same.

## 4. Expected gain (model only; no Ally per-PSO time exists for these passes yet)
Assumptions: 780M at ~2.0 GHz, 12 CU × 4 bilinear/clk, ~500 clk fetch latency, 1080p.
- **Tile max: 0.05-0.1 ms.** The pass is latency-bound by design: ~8k pixels, so the waves cannot hide latency,
  and the original pays n² serial round trips (n = 16: 256).
- **CA composite: 0-0.05 ms.** It sits at the edge of latency-bound at 14 waves.
- **MB gather and DOF: ~0 to small.** With 16 waves the shipped loops already keep the texture units busy when most
  of the screen blurs, and the higher VGPR count lowers occupancy. They help when the pass is latency-bound
  (partial-screen blur, small kernels) and could cost a little otherwise.
- **Total ≈ 0.05-0.2 ms per frame (≤ 1 %).** Decide per file with GP2: `[Profiler] GPUProf = 2` prints ms per
  `vs=… ps=<digest>`. A/B by moving one file out of the folder between sessions.

## 5. Ally install (psoswap; each file is independent)
1. Copy `gpu-shaders\post\out\*.dxbc` and `SHA256SUMS` (built by `py -I build_post.py`; gitignored) into
   `<Nioh 3 folder>/Nioh3EngineFix_shaders/`, next to the ASI and ini.
2. Check them there: `cd "<Nioh 3 folder>/Nioh3EngineFix_shaders" && sha256sum -c SHA256SUMS`. The expected values:

```
516f636faa5c7cf690bc0c8eef702f6bd946aecef9d16a942c71dcd31448d3c4  f11afe97be03915b7284f59eadab1819.dxbc
62f916521f358b050216bb3d02261a3b8cdea9a75cd28520668d050ee04cd198  4e13e236319c9df7c81203eca95c4df4.dxbc
3c4902b63c9cb03793e744b63b68fea856833da5883883c942ced6112885af6d  cf792d56ae14e49c093cb5f27a66f871.dxbc
38ce95b33f3656187b2cd95507aae21880879868208fbbb58f08a3d7596d9f2e  04c1052dc24e69dc69ae5c52430f97b5.dxbc
afc8bb78541d7da0a7e39a00ff8318f899a251cd5ebb504aa2aa88b3ecf092ff  e6efc8ff49a9b254151466f9835e8fad.dxbc
```
3. Ini settings: `[ShaderSwap] Enabled = 1`, `DumpMode = 0`, `LogAll = 0`. Optionally `[Profiler] GPUProf = 2` for
   per-PSO ms.
4. The log should show `5 replacement file(s) indexed`, then `<digest>: replacement loaded (ps DXBC …)` when each pass
   first runs. If the runtime rejects a PSO, psoswap uses the original and logs `REJECTED`.
5. Undo: `Enabled = 0`, or delete the files.

## 6. Files (`gpu-shaders\post\`)
| file | what |
|---|---|
| `runtime_inventory.py`, `out\runtime_inventory.csv` | dump inventory |
| `dxbc_interp.py` | SM4/5 reference interpreter (strict op models, fetch-round count, coverage, trace) |
| `verify_post.py`, `out\verify_*.txt` | bit-exact proof + scenes (`--src` to test any HLSL) |
| `build_post.py` | fxc compile (D3DCompile ps_5_0, game flags), interface check, `out\<digest>.dxbc` + `SHA256SUMS` |
| `dxbc2hlsl.py`, `mkcompose.py` | mechanical DXBC → HLSL, composite build with the CA loop edited |
| `src\*_fast.hlsl`, `src\*_ref.hlsl`, `src\*.hlsli` | replacements and reconstructions (`src\gen\` is generated, gitignored) |
| `rga_post.py`, `out\rga_post.txt` | RGA gfx1103 VGPR / VMEM comparison |
| `pso_check.py`, `pso_check.cpp` | D3D12 PSO acceptance on this PC |
