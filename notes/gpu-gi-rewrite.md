# GI gather rewrite (RefProbe #121 + #414), staged, not installed

Context: `notes\gpu-maxsettings.md` §3/§6. The GI HIGHEST cost is the per-pixel probe-field gathers in
`shader_dx12\0xd070067d.file` ("RefProbe-CllPtSfbSplsSslsSsmVsm"): #121 diffuse (12.4k ISA instructions) and #414 reflection (24.7k).
Both are latency-bound: they read the probes one corner at a time.

## What shipped (item `gi`, one record: #121)
- **Change.** The 8-corner trilinear probe loop (DXIL loop 725) and the two 8-step probe-search loops (325, 413) are
  fully unrolled and if-converted. Every corner's reads are issued without waiting behind a branch. Speculated reads use clamped indices,
  so no read can leave its resource. Stores stay guarded.
- **Reads per wave.** Dependent round trips per wave drop ×2.2 (87 → 40). Branches drop from 287 to 177, and full `s_wait_loadcnt` from 203 to 160 (RGA gfx1103).
- **Interface.** It is identical: DXBC parts, SFI0, ISG1, OSG1, PSV0, cbuffer layout, bindings and numthreads all match.
  The only text difference is the reflection comment's layout type name (`hostlayout.` vs `dx.alignment.legacy.`, which comes from the dxc version).
  The other 553 records are byte-identical.
- **Pipeline.** It is mechanical and reproducible. `gi\mkfast.py rp121` does three things:
  - decompiles the shipped DXIL (`ll2hlsl.py`);
  - collapses the three loops;
  - pins 103 locals. Most pins are `max(a op b, -FLT_MAX)` (`mkfast.pin`); a few dead names get `precise`, which has no effect.

  `gi\build_gi.py` then compiles, checks the interface, splices and repacks.
- **Why pins.** dxc's fast-math reassociated about 100 products and sums differently from the shipped binary. The plain round trip
  was up to 231 ulp off, though still within 1 fp16 ulp. `gi\find_precise.py` found the root ops by trace-diffing every float SSA value of both
  programs. Each pin keeps one shipped evaluation order.
  - `precise` costs more. It disables FMA contraction over whole expression trees: +1.6k ISA.
  - `mad(a,1,b)` pins do not work: dxc folds them back.
  - Pruning the pin list on 32 seeds overfits. The whole SH-coefficient families are needed.

## Proof (gi\out\verify_rp121.txt)
- The test is `verify.py` (reference DXIL interpreter, IEEE float32 per op) on the packaged DXIL against the shipped DXIL:
  - **768 coverage seeds: 141,395 writes, 100 % bit-exact, 0 ulp, 109/109 blocks covered;**
  - **256 realistic (dense probe field) seeds: 48,324 writes, 100 % bit-exact.**
  - Write sets are identical.

## Cost / expected gain (gi\out\perf_rp121.txt; model + offline ISA, NOT measured)
| | VGPR | waves/SIMD | ISA instr | model est. |
|---|---|---|---|---|
| shipped, gfx1103 (Ally) | 120 | 4 | 11,627 | 1.00 |
| rewrite, gfx1103 | 144 | 3 | 13,662 | **×1.4–1.5** (latency-bound → issue-bound) |
| shipped, gfx1201 (RX 9070) | 116 | 6 | 12,164 | 1.00 |
| rewrite, gfx1201 | 140 | 5 | 13,998 | **×0.95–1.0** (already issue-bound) |

- **Ally at MAX (GI HIGHEST, 1080p).** #121 is about half of the gather cost: roughly 6–9 ms on the Ally. Expected saving is **~2–2.5 ms if the model holds**. A realistic range is 0–2.5 ms, because the model assumes 500-cycle loads and perfect overlap.
- **Ally at its current STANDARD GI / 540p.** About 0.1 ms.
- **PC.** Neutral to slightly slower (≤ +0.1 ms at HIGHEST). **Don't install it on the PC unless RGP shows otherwise. It is meant for the Ally.**
- **Must be measured.** Use RGP for the #121 dispatch time, or a MangoHud A/B at GI HIGHEST at a fixed spot (`ally_shaders.sh apply gi` / `revert gi`).

## Not shipped
- **#414 (reflection).** First attempt (full collapse of all 12 loops) was weak; now shipped, see the addendum below.
- **VFSCAT#235 (volumetric fog).** Its light loop has a data-dependent trip count, so the collapse doesn't apply. It needs a hand
  software-pipelined loop plus a `scenes.vfscat_scene` generator (verify.py already dispatches `--kind vfscat`).
  - **Done so far.** The decompile round trip exists (`out\vf235_dec.*`). No LDS barriers, so the interpreter can run it.
  - **Expected gain.** 0.4–0.9 ms on the Ally. Next candidate.

## Files
- `gpu-shaders\gi\`: `mkfast.py`, `find_precise.py`, `build_gi.py`, `ll2hlsl.py`, `llir.py`, `dxil_interp.py`, `verify.py`,
  `scenes.py`, `perfmodel.py`, `rga_stats.py`, `s2g.py`, `out\rp121_fast.hlsl` (the replacement source)
- `gpu-shaders\apply_gi.ps1` (Windows install / revert / status)
- `gpu-shaders\ally_shaders.sh` entry `gi`
- **sha256 of `0xd070067d.file`:**
  - original v2.02: `d3cbb484ee3380d337641c8c893aa152f2c8d053afb37f49091609b1d87dc372`
  - replacement (#121 + #414): `59897999a85a03398f060abcc1b6a50ed69036994c3e2a2e4d0e1e06e7f2de4f`
  - superseded (#121 only): `c952f39a76bc09668953b1bc483e1434f1cbe121bfe9e907d6f49fc601e0fb97` (installers treat it as SUPERSEDED: apply upgrades, revert restores)

## Addendum (2026-10-07): #414 (reflection probe trace) shipped in the same file
The replacement now holds BOTH rewrites: #121 is the unchanged shipped build (a #121-only rebuild still gives
`c952f39a…`), #414 is new, and the other 552 records are byte-identical.

**What #414 does.** It finds a probe level, gathers the 8 probe corners, and reads the octahedral depth, normal
and radiance of each probe along the reflection ray.

**Why the first attempt was weak, and what changed.**
1. **Loop 1789 is not collapsed.** This is the 8-probe visibility and sky-occlusion gather. Unrolled, it needs
   156–256 VGPR (3 or 2 waves, and scratch spills). The other 11 constant-trip loops are collapsed:
   475, 1662, 2856, 3266, 3501, 3881, 4291, 4524, 4753, 5062 and 5291.
2. **Speculated reads were being serialised again.** Two causes:
   - The collapse OR-ed the in-edge predicates, giving `(p&&c)||(p&&!c)`, which dxc does not fold.
   - HLSL 2021 short-circuits `&&`, `||` and `?:`.

   As a result, dxc sank the speculated reads back into branches. The collapsed 475 cost 16 dependent round
   trips per pass instead of 3. Two fixes:
   - New `ll2hlsl --cdpred`: a block that is control-equivalent to an earlier block reuses that block's predicate.
   - Compile with `-HV 2018`.
3. **If-conversion of the Chebyshev test in loop 1789** (`mk414.ifconv`, arithmetic only). The second depth
   sample and the 9 `sky_occ` reads no longer wait behind a branch on the first sample: 4 → 2 round trips per probe.
4. **Selective prefetch: implemented but off** (`mk414.py --prefetch`; `MK414_PF` picks which reads).
   - Next probe's info one iteration ahead: 1 round trip per probe, but 128 VGPR (more than the shipped 124).
   - Partial prefetch stays at 124 VGPR but gains nothing.
   - The model is already issue-bound without it, so the plain version ships.
- **Merged search loops: not needed.** The level search (475) and the corner lookups (1662) collapse cleanly.

**Pins: 140 locals as `max(x, x)`, in `gi\rp414_pins.txt`.** In collapsed loops a pin covers every iteration's
copy (`mkfast` `pin_coll`).
- **New failure mode.** Once a loop is unrolled, the trilinear corner bit is a constant. Fast-math then folds the
  shipped `(x-(1-x))+(1-x)` to `x`. Fix: pin the `x-(1-x)` and `1-x` families (18 + 18 locals).
- **find_precise.py additions:** `--kind refgeo`, and an operand-depth fallback for roots whose operands
  coincide with the same expression computed elsewhere.

**Scenes: `gi\geoscene.py` (`--kind refgeo`).** Everything is ray-cast from one analytic scene in field
coordinates:
- room plus boxes and spheres, sometimes open to the sky;
- probe octahedral depth, normal, radiance and mask;
- a perspective camera with depth, octahedral view normal, stencil and vEye.

Probes inside obstacles are invalid. The field spans tens of units, because the shader's parallax point sits
10 units off the surface. `realistic=True` gives game-like parameters for the model.

**Proof** (`gi\out\verify_rp414.txt`; packaged DXIL == `out\rp414_fast.dxil` vs shipped DXIL): 0 ulp everywhere, with identical write sets.

| scenes | seeds | writes | blocks |
|---|---|---|---|
| refgeo | 0–767 | 242,445 | 268/270 |
| refgeo hold-out | 768–2303 | 481,523 | 268/270 |
| refgeo realistic | 256 | 190,600 | 261/270 |
| refprobe (old random) | 0–767 | 141,853 | 265/270 |
| refprobe hold-out | 768–2303 | 290,419 | 265/270 |
| refprobe realistic | 256 | 48,119 | 251/270 |

- The two blocks no scene reaches are 2508 and 2616. They are ray-parallel-to-texel guards, `sin < 1e-4`.
- A mutation run widens that constant to 0.1 in BOTH programs: 270/270 blocks, 81,736 writes, 0 ulp.

**Cost** (`gi\out\perf_rp414.txt`; RGA plus model, NOT measured)

| | VGPR | waves | ISA instr | branches | full mem waits |
|---|---|---|---|---|---|
| shipped gfx1103 / gfx1201 | 124 / 108 | 4 / 6 | 23,213 / 25,120 | 363 | 306 |
| packaged gfx1103 / gfx1201 (AMD folds pins late: +10 % VALU) | 120 / 108 | 4 / 6 | 25,618 / 26,433 | 161 | 266 |
| pins folded (= what RADV/ACO compiles) gfx1103 | 120 | 4 | 23,474 | 172 | 273 |

- **Model, realistic geometry scenes, per wave:**
  - dependent round trips: 132 → 24 (dataflow), 159 → 41 (in-order);
  - the rewrite is now issue-bound;
  - Ally estimate **×2.2 (dataflow) – ×2.6 (in-order)**;
  - old random realistic scenes: ×2.2–2.7.
- **PC: ×1.45–1.8.** At 6 waves the PC is also issue-bound.
- **Ally at MAX.** #414 is roughly the other half of the 6–9 ms gather at GI HIGHEST.
  - **Expected saving: ~1.5–2.5 ms if the model holds.**
  - Realistic range: 0–2.5 ms. The model assumes 500-cycle loads and perfect latency hiding.
- **Must be measured.** RGP #414 dispatch time, or MangoHud A/B at GI HIGHEST (`ally_shaders.sh apply gi` /
  `revert gi`).

**Rebuild:** `py -I gi\mk414.py; py -I gi\mkfast.py rp414; py -I gi\build_gi.py`. The rp121, vf235 and rp414
builds are checked byte-identical after the shared tool changes.

**New files:**
- `gi\mk414.py`, `gi\rp414_pins.txt`, `gi\geoscene.py`, `gi\out\rp414_fast.hlsl`;
- `gi\rtprof.py` (per-block round trips, any kind, `--blocks`);
- `gi\critpath.py` (read arrival times inside one block);
- `gi\fpdiag.py` (first diverging values for one thread).
