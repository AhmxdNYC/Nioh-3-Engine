# Volumetric fog rewrite (VFSCAT #235), staged, not installed

**2026-10-08:** the full rewrite measured +0.33..0.54 ms SLOWER on the Ally (throughput-bound shader; unpinned RADV build 112-116 VGPR -> 4 waves; +13-18 % VALU; speculative loads). Superseded by **fogpar** = item 1 only (`mkfog.py --no-cl --no-tail`, `build_fogpar.py`; bit-exact; RGA 96 VGPR / 5 waves, loads 101 -> 79). See `ally-gpu-budget.md` s6-s7.

Context: `notes\gpu-maxsettings.md` §6 item 8, `notes\gpu-gi-rewrite.md` (same method). VFSCAT-CllLpvSfbVsm #235 in
`shader_dx12\0xa9910db8.file` scatters light into the 160×90×64 froxel volume (8×8×4 groups, async queue). It is
~1.8–2.3 ms of work on the Ally at VOLUMETRIC HIGH (= max), fixed size.

## Where the time went (in-order latency model, realistic scene)
84 dependent memory round trips per wave:
- **VSM clip-level search: 44 (52 %).** Each of the 3 sun samples walks every level start..count-1, never stops early.
  Each level costs 2 round trips: a per-lane `mW2S_VirtualSM[level]` cbuffer row fetch (VMEM, the index is divergent),
  then the page-flag fetch.
- **Clustered point/spot lights: about 4 per light.** The chain is index → position → attributes → shadow/IES sample.
- **Rest:** cascade and VSM samples one at a time (6), and the tail (LPV → noise → reprojection, 4).

## What shipped (item `fog`, one record: #235)
1. **Merged early-exit VSM search.** One loop walks the levels once for all 3 samples and stops when every sample has
   its level.
   - **Why stopping early is exact.** The shipped loop keeps `min(level)` over the valid levels, i.e. the first valid one.
   - **The found level's x/y/z.** They are the loop's own values; the shipped post-loop code recomputes the identical
     expressions.
   - **The 3 cascade-map samples and the 3 final VSM samples** are all issued before any of them is consumed.
   - **Fallback behaviour is kept.** A sample whose search is skipped or finds nothing re-uses the previous
     sample's or light's coordinates and level, exactly as before.
2. **Software-pipelined light list.**
   - Light k+1's index is fetched one iteration ahead.
   - Spot attribute rows and spot info are fetched at the top of the light's iteration (speculative, before the cone test).
   - Point attributes stay where they were: hoisting them cost a wave (RGA 108 VGPR).
   - Out-of-list prefetches read the next cell or past the buffer end (returns 0) and are never used.
3. **Tail.** The reprojection sample and the 6 LPV samples are issued before the noise lookups and consumed after them.

All arithmetic statements are the shipped ones (same SSA names/operands/order); only load placement and loop trip
counts change. `fog\mkfog.py` generates the source from the decompile and asserts every renamed template equals the
shipped text.

**Pins: 346 × `max(x, x)` (`gi\mkfast.py` option `pin_lo="self"`).**
- **What a pin does.** It is an exact identity for every x, NaN included. dxc keeps the FMax, so fast-math cannot
  reassociate through it.
- **Why not `-FLT_MAX` like GI.** The GI shader's `max(x,-FLT_MAX)` pins changed results for NaN and −inf, and they
  survive into the ISA.
- **Why this matters for FMA fusion.** ACO fuses `v_mul_f32` → `v_add_f32` into `v_fma_f32` (aco_optimizer.cpp
  `create_fma_cb`). 42 of the pins sit between a product and its add.
- **Why `max(x, x)` keeps the shipped fusion.** Mesa NIR folds `fmax(a,a)` → `fcanonicalize(a)` → `a`
  (nir_opt_algebraic.py l.992/167). So RADV/ACO compiles the shipped expression DAG and fuses exactly like the shipped
  shader. The AMD Windows compiler (RGA) folds them too, but only late: +220 VALU there.
- **How the list was found.** `gi\find_precise.py` (now also takes `--seeds a,b,c`): 301 pins for the plain decompile,
  then 45 more from the hold-out failures. List in `fog\vf235_pins.txt`.

## Proof (fog\out\verify_*.txt; `gi\verify.py --kind vfscat`, packaged DXIL vs shipped DXIL)
- **768 coverage seeds:** 196,608 writes, 100 % bit-exact, 0 ulp, 137/137 blocks.
- **256 realistic seeds:** 131,072 writes, 0 ulp (130/137 blocks; the realistic scene never takes the rare paths).
- **128 heavy-light realistic seeds:** 65,536 writes, 0 ulp. Write sets are identical in all runs.
- **Hold-out seeds 768–2303** (not used for the pin search): 393,216 writes, 0 ulp, 137/137 blocks.
- **Generator** (`fog\vfscene.py`, via `gi\scenes.vfscat_scene`). Geometry is consistent: froxel → world. The cluster lists
  come from real sphere-vs-cell overlap (30×17 tiles × 16 log slices). Variants cover:
  - parallel lights: 0–3, shadow none/exp/compare, single map or 4 cascades;
  - VSM: 0–16 levels, page residency, clip mask, depth exactly 0 or 1;
  - point/spot types: poly/exp/IES, shadowed or not, disabled lights, more than 16 applied lights, slices ≥ 16, a spot
    aimed exactly down a froxel ray;
  - LPV box or fade, noise on/off, reprojection in/out.
- **Interpreter additions** (`gi\dxil_interp.py`):
  - cube-array sampling, robust to inf coordinates, ±inf constants;
  - in-order timing option: a block waits for the loads earlier blocks consumed;
  - dynamic-index cbuffer reads count as VMEM.

## Cost / expected gain (fog\out\perf_vf235.txt; model + offline ISA, NOT measured)
| | VGPR | waves/SIMD | ISA instr | VMEM loads | full mem waits |
|---|---|---|---|---|---|
| shipped gfx1103 / gfx1201 | 96 / 96 | 5 / 8 | 3614 / 3775 | 101 | 56 |
| rewrite, pins folded (= what RADV compiles) gfx1103 / gfx1201 | 96 / 96 | 5 / 8 | 3508 / 3650 | 81 | 42 |
| rewrite as packaged, RGA gfx1103 | 96 | 5 | 3758 | 81 | 43 |

**Model per wave** (realistic scene: 1 sun, 4 cascades, 8 VSM levels at 85 % residency; moderate = 10 point and 3
spot lights; loads 500 cycles):
| light load | in-order model gfx1103 / gfx1201 | dataflow model (optimistic baseline) |
|---|---|---|
| moderate | **×2.7** / ×2.3 (round trips 84 → 28) | ×1.35 |
| none | ×4.5 | ×1.7 |
| heavy (24 point / 8 spot) | ×2.3 | ×1.35 |

- **Ally at MAX.** About 2 ms of VFSCAT, so **~0.5–1.3 ms saved if the model holds**. The real number depends on VSM
  page residency and on local-light density in fog, neither of which is known.
- **PC.** VFSCAT is async (293 µs, overlapped), so expect ~0. RGA puts the PC at the same occupancy, but the late pin
  folding costs +12 % VALU there. Ally-first, like `gi`.
- **Must be measured.** Use RGP for the #235 dispatch time, or a MangoHud A/B at a fixed foggy spot
  (`ally_shaders.sh apply fog` / `revert fog`).
- **Caveat.** The proof is DXIL-level. Pins are invisible to RADV, so the Ally's ACO output for the shared expressions
  is the shipped one.

## Not done / options
- **Point-attribute prefetch** (one more round trip per point light) needs ~12 more VGPRs: 4 waves on the Ally. Rejected.
- **VSM search** prefetches no next level, so a non-resident start level still costs 2 round trips per level for all
  3 samples together.

## Files
- **`gpu-shaders\fog\`:**
  - `mkfog.py`: the restructure;
  - `vf235_src.hlsl`: generated;
  - `vf235_fast.hlsl`: the packaged source with pins, a copy of `gi\out\vf235_fast.hlsl`;
  - `vf235_pins.txt`;
  - `vfscene.py`: scenes;
  - `build_fog.py`: compile, interface check, splice; tolerates only reflection spellings `float4 m[4]` vs
    `row_major float4x4 m` and the struct name prefix, with the same offsets and stride;
  - `rtprofile.py`: per-block round-trip profile;
  - `pin_audit.py`, `ophist.py`, `build_variant.sh`, `np_variant.sh`.
- **`gpu-shaders\gi\`, shared:**
  - `mkfast.py`: `src`, `pin_lo`, `pin_div` options; dedup fix;
  - `perfmodel.py`: `--inorder`;
  - `find_precise.py`: seed list;
  - `scenes.py`: `vfscat_scene` shim.
  - The rp121 build is unchanged.
- **`gpu-shaders\apply_fog.ps1`, and `gpu-shaders\ally_shaders.sh` entry `fog`.**
- **sha256 of `0xa9910db8.file`:**
  - original v2.02: `4784aabe40fdb10646babde1df734123ce713dc56089a55b4efa0db43bf4f993`
  - replacement: `ae04ee6ac2920c62cac9d694428f8105e42c45ef9c34b13426dead6d97190e6d`
- **Rebuild:** `py -I fog\mkfog.py; py -I gi\mkfast.py vf235; py -I fog\build_fog.py`
