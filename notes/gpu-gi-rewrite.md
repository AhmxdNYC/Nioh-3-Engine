# GI gather rewrite (RefProbe #121), staged, not installed

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
- **#414 (reflection).** It collapses the same way (12 loops).
  - **Model.** Only ×1.22 on the Ally and ×1.36 on the PC.
  - **Cost.** 156 VGPRs (3 waves), +11 % ISA and +47 % memory reads (speculation).
  - **Proof.** Only 234/270 blocks are covered by the scenes, and the round trip is 5 ulp off (pins not searched).
  - Weak case: left out. Next step would be to collapse only its hottest gather loop.
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
  - replacement: `c952f39a76bc09668953b1bc483e1434f1cbe121bfe9e907d6f49fc601e0fb97`
