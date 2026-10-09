# Volumetrics rewrite for mid-range RDNA2: VCLOUD + VFSCAT (2026-10-09)

Scope: the two async volumetric compute passes on pc2 (RX 6650 XT, 1080p HIGH, FSR 65, GP2 file #125/#126, stock
shaders, 23 gameplay windows, mean span 23.2 ms):
- VCLOUD `cs=b76874bb` (DrawSky): 1.60 ms async wall, rank 2;
- VFSCAT-CllLpvSfbVsm `cs=641524ee`: 1.04 ms async wall, rank 3.

Status: **VCLOUD rewrite built, proven, and queued for a pc2 A/B** (bridge #500 + #509, pack #133). Nothing is
installed on any machine. For VFSCAT the existing `fogpar` stays the candidate (no new build; see section 4).

## 1. What the GP2 numbers mean for VCLOUD

- The VCLOUD async time scales with the graphics load of the window:
  - about 0.55-0.70 ms in light 8-10 ms frames;
  - 2.1-4.4 ms in the heavy ~30 ms spawn windows;
  - 0.04 ms where no sky is visible.
- So it is wall time stretched by contention with graphics, not ALU cost. The dispatch is tiny: VCBayer is render/8,
  so at 1248x702 that is 156x88 px = 20x11 groups = 220 wave64. That is about 3.4 waves per SIMD on 32 CUs.
- Each wave runs a long serial ray march: up to 256 steps, two layers, and a 4-step light march at each step that hits
  cloud. With so few waves, the per-wave latency chain sets the wall time.
- Frame cost is lower than 1.60 ms, because the shader is async. It is paid where the graphics queue waits at DrawSky
  and in the CU time the shader takes from graphics.

## 2. VCLOUD: where the time goes (decompile + RGA)

**The record.** It is in `shader_dx12\0x5f17ca9d.file`, group VCLOUD, which has 8 CXDK-wrapped compute permutations.
Record 7 = b76874bb, the biggest: 8786 DXIL lines, 81 samples, 8 textures + 8 samplers, a 1328-byte cbuffer, and
[numthreads(8,8,1)].
- Tools: `vcloud\extract_vcloud.py` extracts it, and `gi\ll2hlsl.py` decompiles it to `extract\vcloud_cs7_dec.hlsl`.

**RDNA2 proxy.** The RDTS 2026-05 offline DX12 driver lists no gfx103x, so gfx1102 (RDNA3, RX 7600) is used as the
proxy. Both have the same 1024-VGPR SIMD, the SGPR limit, and the same amdxc.

| | VGPR / waves (wave64) | ISA instr | VALU | samples | full vm waits | s_buffer_load | full lgkmcnt(0) waits |
|---|---|---|---|---|---|---|---|
| shipped | 108 / 4 | 9096 | 5067 | 81 | 40 (+41 partial) | 661 | **514** |

**Structure of one march step:**
1. mask (2D array, mip from a projected footprint) + base (3D);
2. if there is density: distort (2D), then detail (3D, whose coordinates use the distort result);
3. if lit: a sky-ambient table lookup, then **a 4-step light march** toward the sun, then the same toward the moon. Each
   light step samples mask + base + detail, and the light march ends with a transmittance-table lookup.

**Two kinds of waste on the critical path:**

1. **One texture round trip per light step.**
   - The 4 light steps are nested `if (step k leaves the layer) T = product so far; else { ... }`.
   - The exit test depends only on geometry (the height of position + k*step), never on a sample.
   - Even so, the compiler waits for each step's 3 samples before it starts the next step. With sky ambient and
     transmittance, a lit cloudy step costs about 9 dependent texture round trips.
2. **Scalar constant and descriptor reloads.**
   - A light step needs 3 texture descriptors + 3 samplers (36 SGPRs) and about 54 constant dwords (mW2P, g_mtP2W,
     W2M, mask thresholds, base/detail scale and offset, mip parameters).
   - amdxc re-loads them before nearly every use, each with its own `s_waitcnt lgkmcnt(0)`: 514 such waits in the
     shader against 81 samples, and about 18 per light step. This is a scalar-cache round trip each time, serialised.
   - It is a compiler policy, not only SGPR pressure. A wave32 build (SM 6.6 `[WaveSize(32)]`) uses only 86 SGPRs and
     re-loads even more (558 waits).

**Tried and not shipped:**
- `mkvc.py --vk`: copy the loop constants into VGPRs once before each loop with an exact `x ^ (DTid.x >> 31)`.
  - The `proj` group (48 constants): lgkm waits 506 -> 359, but 128 VGPRs.
  - All 108 constants: 247 waits, but 152 VGPRs = 3 waves.
  - Not used, because of the occupancy risk on a 3.4-waves-per-SIMD dispatch.
- if-conversion alone (`lm4`): amdxc then serialises every sample (74 full waits). It schedules the long straight-line
  block in source order, so the samples also have to be moved up in the source (step 3 below).

## 3. VCLOUD fix (vc7): same arithmetic, fewer serial waits

1. **`vcloud\flatten.py`** if-converts the 4 light-march chains (2 layers x sun/moon). Every step is computed, then
   `T = vLast; T = c4 ? v4 : T; ...; T = c1 ? 1.0 : T`.
   - Every chain body is pure (SampleLevel + arithmetic), so T is bit-identical.
   - The cost is 12 speculative samples, wasted only where a light ray leaves the layer early.
2. **`vcloud\hoist.py --split 7`** works inside each now straight-line light block:
   - It moves the samples and their coordinate statements to the top in 2 batches. Batch 1 = sky ambient + steps
     1-2 + transmittance (8 samples); batch 2 = steps 3-4 (6 samples).
   - This is a pure reorder of SSA statements.
   - One batch of 14 needs 156 VGPRs (3 waves); 2 batches need **124 = 4 waves**, the same occupancy as shipped.
3. **`vcloud\prefetch.py`** (variant pf) software-pipelines both main march loops:
   - Step i+1's mask + base samples are issued at the top of step i. A prologue covers step 0.
   - The position p_{i+1} = mad(dir, step, p_i) is the value step i+1 computes itself. The coordinate statements (93
     per loop) are a renamed copy of the shipped ones, and the results are carried in the loop.
   - The cost is one unused prefetch per ray.

**Variants** (`vcloud\build_vcloud.py`; packed record = the verified DXIL, checked by dumpbin):

| variant | source | VGPR/waves gfx1102 | ISA instr | VALU | full lgkm waits | samples (static) | exactness (interpreter, `gi\verify.py --kind vcloud`) |
|---|---|---|---|---|---|---|---|
| stock | - | 108 / 4 | 9096 | 5067 | 514 | 81 | - |
| **u** | lm4h2 | 124 / 4 | 8955 | 5074 | 447 | 81 | not bit-exact; see below |
| p | lm4h2 + 726 `max(x,x)` pins | 128 / 4 | 10121 | 5945 | 447 | 81 | **bit-exact** (below) |
| **pfu** | pf | 120 / 4 | 9257 | 5216 | 472 | 85 | same deviations as u |
| pfp | pf + pins | 128 / 4 | 10593 | 6153 | 502 | 85 | **bit-exact** (below) |

**What "not bit-exact" means for u and pfu:**
- Synthetic coverage scenes: max relative difference 1.3e-3. Realistic scenes: 2.1e-3, max 3 fp16 ulp. 91 % of
  writes are bit-exact.
- The plain decompile -> dxc round trip (`build\rt0`, no restructure) has **identical** deviation counts. So all of
  it comes from dxc fast-math re-associating the unchanged code, not from the restructure.

**Bit-exact proof for p and pfp:**
- p: 96 pin-training seeds, then 512 hold-out seeds (114,714 writes), 0 ulp, 111/111 blocks; 64 realistic seeds,
  0 ulp.
- pfp: 256 + 512 hold-out seeds, plus 64 realistic seeds, all 0 ulp.

**Interface.** SFI0 / ISG1 / OSG1 / PSV0 are identical, and the dumpbin interface is identical. The CXDK tail is
byte-identical, and the other 7 records are byte-identical.

**Which variant for which platform.** Pins are free under RADV/ACO, which folds `fmax(a,a)` early. The AMD Windows
compiler folds them late, at +17 % VALU, and it re-fuses FMAs anyway; that last point is inferred from
gpu-fog-rewrite.md. So **u / pfu are the Windows candidates and p / pfp the Ally ones**.

**Latency model.** `gi\perfmodel.py --inorder`, `vcloud-real` scenes, 24 seeds: my scenes, with ground eye, sun up,
and 24 steps.

| | round trips / wave | critical cycles / wave | DXIL instr / thread | mem reads / thread |
|---|---|---|---|---|
| stock | 86.9 | 55.8 k | 5872 | 76.9 |
| u | 55.3 (x1.57) | 37.0 k (x1.51) | 6147 | 83.2 |
| pfu | 39.5 (x2.20) | 27.4 k (x2.04) | 6654 | 89.7 |

- The scalar-reload savings (514 -> 447 lgkm waits) are not in the model.
- The model's issue-bound estimate, which assumes 4 resident waves hide all latency, gives only x1.06-1.11. That
  regime does not fit a 220-wave async dispatch, but it is why the A/B decides.

**Expected (E):**
- VCLOUD async wall on pc2: about -30..-50 % (1.60 -> ~0.8-1.1 ms).
- GPU frame: about 0.1-0.5 ms (0.4-2 %). It is larger in the heavy sky windows, where VCLOUD was 2-4 ms wall, and
  depends on how long graphics waits at DrawSky.

## 4. VFSCAT

- `fogpar` (`gpu-shaders\replacement\0xa9910db8.file`, GP2 hash 5473bfb7) is already KEEP for the Ally: bit-exact
  early-exit VSM level search, +0.15/+0.30 fps (release/profiles.json). It is installed in pc2's #441 mod baseline,
  but pc2 never measured it on RDNA2: #125 was stock.
- The #500 A/B keeps GHDM/fogpar the same in every arm and reports the VFSCAT GP2 row. That gives the first RDNA2
  fogpar number (cross-session vs #125 stock 1.04 ms).
- On gfx1102, VFSCAT (96 VGPR, 5 waves) also re-loads constants: 112 lgkm waits against 27 samples and 74 buffer
  loads.
- It is a big throughput-bound dispatch (~14k wave64 at HIGH), and the full rewrite lost to +VGPR
  (gpu-fog-rewrite.md). So no new VFSCAT variant was built. A VGPR-constant variant would cost occupancy for a
  throughput-bound shader.

## 5. A/B queued on pc2 (bridge #500, amended by #509)

**Setup.**
- Pack: #133 `nioh3-vcloud-vc7-pack2.zip`. Unzip onto the repo root; install with `gpu-shaders\apply_vcloud.ps1`
  (`-Variant u|p|pfu|pfp`, `-Revert`, `-Status`, sha-checked, refuses while the game runs).
- Config: `configs/pc2-bench-1080-120-high.xml`, with GHDM/fogpar as in #441 in every arm.

**Arms.** A = stock, B = u, C = pfu, run in the order A B C C B A (or A B C A if time is short).

**Spawn points.** Keys 7356, 52, 7292 and 5631, at least 30 s each. These are the #125 heavy windows, where VCLOUD
was 2.4-3.0 ms.

**Measurements:**
- PresentMon GPUBusy per arm;
- GP2 rows VCLOUD (A b76874bb / B 3b5911ea / C 8f4b202a) and VFSCAT;
- shotdiff at the 7356 spawn: A1 vs B1 and A1 vs C1, with --noise A2.

**Decision.** KEEP if GPUBusy is not worse in both pairs and shotdiff passes. If pfu wins on Windows, the Ally A/B
should use pfp.

## 6. Files

- **`gpu-shaders\vcloud\`:**
  - `extract_vcloud.py`;
  - `flatten.py`, `hoist.py`, `prefetch.py`: the restructure;
  - `mkvc.py`: the VGPR-constant experiment, not shipped;
  - `vcscene.py`: VCLOUD scenes;
  - `build_vcloud.py`;
  - `cc.sh`, `isastat.sh`: compile + RGA + ISA counters;
  - `vc7_pins.txt`;
  - `out\verify_*.txt`, `out\rga_vc7.md`, `out\fp_run*.txt`.
- **`gpu-shaders\apply_vcloud.ps1`.**
- **Shared changes:**
  - `gi\scenes.py`: kind `vcloud` / `vcloud-real`;
  - `gi\mkfast.py`: item `vc7`, whose source is `vcloud\src\vc7_src.hlsl`;
  - `gi\find_precise.py`: vc* -> kind vcloud;
  - `gi\rga_stats.py`: gfx1102 / gfx103x VGPR sizes.
- **`re\scripts_gpu\out\VCLOUD_7.{ll,dxbc}`:** the shipped record.

**sha256 of 0x5f17ca9d.file:**

| variant | sha256 | GP2 hash |
|---|---|---|
| original | d92866c1...843e71 | - |
| u | abf3ab1a...edbcd | 3b5911ea |
| p | fb596ee6...7a110 | 58404453 |
| pfu | 1ec99309...1f650 | 8f4b202a |
| pfp | 732eede2...44ba4 | c1e50cee |

**Rebuild:**
```
cd gpu-shaders\vcloud
py -I extract_vcloud.py
py -I ..\gi\ll2hlsl.py extract\vcloud_cs7.ll extract\vcloud_cs7_dec.hlsl
py -I flatten.py extract\vcloud_cs7_dec.hlsl src\lm4.hlsl
py -I hoist.py src\lm4.hlsl src\lm4h2.hlsl --split 7
py -I prefetch.py src\lm4h2.hlsl src\pf.hlsl
```
Then, for each X in lm4h2 and pf:
1. copy `src\X.hlsl` to `src\vc7_src.hlsl`;
2. run `py -I ..\gi\mkfast.py vc7`;
3. copy `..\gi\out\vc7_fast.hlsl` to `src\X_pinned.hlsl`.

Finally run `py -I build_vcloud.py`.

**Pins:** `py -I ..\gi\find_precise.py vc7 --seeds 0:96`.

## 7. Next options (not built)

- **Speculate the distort sample** into the mask/base batch, to save 1 round trip per cloudy step. The cost is one 2D
  sample per empty step.
- **Hoist the detail mip/coordinate math** above the density test.
- **Partial VGPR constants** for the light march only, if pc2 shows VCLOUD is still latency-bound after vc7.
