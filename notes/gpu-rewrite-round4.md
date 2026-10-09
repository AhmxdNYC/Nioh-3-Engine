# GPU rewrite round 4: meshlet culling family on RDNA2 (pc2, RX 6650 XT) - 2026-10-09

Follows gpu-rewrite-round3.md §3 (design) and §3.1 (meshlet pipeline investigation, written in this round). Static/offline work
on the PC; the game was not launched here. Round 4 was cut short by a direction change (investigation over new rewrites):
`lodu` is finished and queued; the MIC SAT rewrite is built but unproven; MPC was not started.

## 1. Status

| item | state | files |
|---|---|---|
| **lodu** - MshLod#2 (63c07c21) wave-uniform fast path | **built, proven, staged, A/B queued on pc2** | `gpu-shaders/meshcull/{lodfast,verify_lod,build_cull4}.py`, `results/`, `cull4_manifest.tsv`, `gpu-shaders/apply_cull4.ps1`, `ally_shaders.sh` row `lodu`, pack `meshcull/out/cull4_records.zip` (bridge) |
| MIC#0 SAT if-conversion | rewriter built (all 3 copies), ISA measured, **not proven, not staged** | `gpu-shaders/meshcull/micsat.py` |
| MPC#20/#33 same row/waterfall pattern | identified (round3 §3.1 Q1), not built | - |

## 2. lodu: MshLod#2 guarded uniform fast path

**Kernel.** MshLod#2 is the per-cluster LOD kernel (one thread per cluster payload, numthreads 32, ExecuteIndirect per LOD
level). Per thread: g1m = (p0 >> 6) & 0x3ffff; the G1M offset row `tG1mOffsets[g1m]` (t16, 48 B) and `tMeshletResourceOffsets[g1m]`
(t19, 8 B) are loaded in 22 places (16 + 6; the driver CSEs the dominated ones, 8 VMEM remain); every bindless
`tMeshletG1mBuffer[rootBufferIdx]` access is a readfirstlane waterfall (7 loops). It writes UAVs (cull codes, cache codes, payloads,
MRLOD payloads), does 3 atomics and uses WaveActiveSum / WavePrefixSum / WaveIsFirstLane / WaveReadLaneFirst.

**Rewrite (`meshcull/lodfast.py`, DXIL text level, no float op touched).**
```
b26:  ... g = (p0 >> 6) & 0x3ffff                          (shipped, up to g)
      e = WaveActiveAllEqual(g); k = WaveReadLaneFirst(g); br e, wf_u, wf_r
wf_r: the rest of the kernel, SHIPPED, untouched           (divergent waves)
wf_u: rows t16[k].{0,4,16,28,36,44}, t19[k].{0,4}           (uniform index -> scalar loads)
      + a clone of everything after g, with every row load replaced by the row value, g by k, and the
        createHandle(t0[row0]) NonUniformResourceIndex flag cleared (uniform)
```
The branch is wave-uniform, so all lanes of a wave run one copy; each copy has the shipped CFG and every UAV load/store, atomic
and wave op with the same operands in the same order. The kernel already declares WaveOps (SFI0 / ISG1 / OSG1 / PSV0
byte-identical). The no-clone variant (`--hoist`: one diamond + phis) was measured too and is worse (waterfalls stay).

**Proof (`meshcull/verify_lod.py`, DXIL reference interpreter of sweep/vendor_gi via materials/psim).** Per-lane model with an
ordered event log per thread: UAV loads (address + value), UAV stores, atomics (applied, old value returned), and the shipped
wave ops with their operands; a wave op's result is an oracle of (scene, thread, ordinal, operands), so equal logs mean equal
behaviour under any lane mix of a real wave. The new guard ops are scripted over every outcome a wave can produce: fast (all
equal, k = own g), slow (k foreign), slow-own (k = own g, not all equal). Scenes: random but laid out like the real G1M data
(56-byte cluster-LOD records, -1 sentinels where the kernel tests them, small trip counts). Results (`meshcull/results/`):
- `verify_lod2.txt`: **EQUIVALENT, 400 scenes x 64 threads x 3 guard modes**, event logs + final UAV state identical, shipped
  blocks covered **67/67**; build-time run 200 scenes (also 67/67).
- `verify_lod2_selftest.txt`: guard inverted, wrong row offset, store dropped in the fast copy: **all caught**.
- Build checks (`build_cull4.py`): record digest = v2.02 MshLod#2; dumpbin of the assembled container == rewritten text;
  side-effect census bufferStore / atomicBinOp / waveActiveOp / wavePrefixOp / waveIsFirstLane exactly x2, +1 WaveActiveAllEqual,
  +1 WaveReadLaneFirst; splice from the pack re-creates the file byte-identically.

**ISA (RGA offline DX12 driver; gfx1102 = RDNA3 mid-range proxy, no RDNA2 target; gfx1103 identical; wave32).**

| | instr | VMEM | SMEM | full vmcnt waits | waterfall loops (readfirstlane) | branches | VGPR | SGPR | waves/SIMD |
|---|---|---|---|---|---|---|---|---|---|
| shipped | 906 | 37 | 31 | 30 | 7 (10) | 60 | 48 | 62 | 16 |
| **fast path** (analysis build, e = true) | 843 | **29** | 33 | **22** | **0 (4)** | 53 | **40** | 62 | 16 |
| whole lodu kernel (both copies) | 1633 | 61 | 57 | 52 | 7 (14) | 113 | 48 | 70 | 16 |
| no-clone `--hoist` variant | 928 | 35 | 39 | 25 | 7 (11) | 62 | 40 | 54 | 16 |

Divergent waves run the shipped code + 2 wave ops and a branch. Occupancy unchanged (VGPR 48 max). Code size 906 -> 1633
instructions (~10 KB).

**Expected (E).** MshLod#2 dispatches on pc2 ~0.9 ms/frame (GP2 level buckets "0".."3" 0.45 + 0.20 + 0.12 + 0.07, plus small
levels; MinRun 4 attribution). For uniform waves the dependent round trips drop 30 -> 22 and the 7 descriptor-load waterfalls
disappear (~25-35 % of the wave's latency). Fraction of uniform waves unknown (children of one parent are written contiguously,
so levels >= 1 are likely mostly uniform): **~0.1-0.25 ms on pc2 (0.3-0.8 %)**, ~0 on light spawns. Ally: CS, so the round-3
NGG-culling caveat (VS only) does not apply; RADV/ACO lowers WaveActiveAllEqual / readfirstlane natively - a separate Ally item
is only useful after a pc2 KEEP.

**Install.** Windows: `gpu-shaders\apply_cull4.ps1 [-Revert|-Status]` (sha-checked, backups `gpu-shaders\backup\cull4\`;
tested on a scratch copy: install, re-install no-op, revert, re-revert no-op). Other PC: `py -I gpu-shaders\meshvs\splice_msv.py
<cull4_records.zip> --flat` -> `gpu-shaders\replacement\0x8d9282e6.file` (sha 5f7f3b12...). Harness/Ally item `lodu`.
**Conflict:** same file as `mshlod` (MshLod#3, sha 214ac371...). Never in one pc_gpu_session item list (Set-ItemState would
overwrite one with the other's original); the flat splice overwrites the mshlod replacement on that machine (re-copy it from
bridge #138 when needed). `lodu` is not part of `ally_shaders.sh all`.

| file | sha256 original v2.02 | sha256 lodu |
|---|---|---|
| 0x8d9282e6.file | ec0f5283660caa64565e17cfb8f2dc4d06ee498278fe1a832db16d27b916ebeb | 5f7f3b120b909c74c0238d7af3f4f3cd43299840b21a5a49e195545ea54150f1 |

New record digest d5a1a069857feee3a6589f1c788aca52 (GP2 cs row of the lodu arm).

## 3. MIC#0 SAT if-conversion (built, not proven)

`meshcull/micsat.py` rewrites all three copies of the valid-rect SAT lookup (round3 §3): T[2] and T[1] loaded next to T[0]
(constant indices; the divide-by-w arithmetic stays behind the w >= 1 test), and the three corner diamonds folded into their head
block (loads unconditional, `phi [T[i], L], [0, other]` -> `select`, polarity kept). Assembles and validates.

| MIC#0 e31c389f (gfx1102) | instr | VMEM | full vmcnt waits | partial | branches | VGPR | waves/SIMD |
|---|---|---|---|---|---|---|---|
| shipped | 2830 | 77 | 33 | 25 | 101 | 72 | 14 |
| micsat | 2772 | 77 | **20** | 30 | 92 | **64** | **16** |

(gfx1201: VGPR 72 -> 72, waits 24 -> 20.) Expected 0.05-0.15 ms on pc2 (MIC 0.53 ms/frame). **Missing:** a CS proof (the per-lane
event-log verifier of §2 fits: MIC#0 has 2 atomics, 2 stores and 9 SampleLevel, the samples need the psim oracle), then a build
script + item. Not continued per the direction change.

## 4. A/B queued (pc2)

Bridge pack `cull4_records.zip` + `[QUEUE] pc2` (see MODLOG / bridge). Runs AFTER render-architecture Q2 (#564, items
mpc,mshlod), because `lodu` and `mshlod` share 0x8d9282e6.file. Combined with the queued MSV A/B (#505):
`tools\testmatrix\pc2_pc_gpu_session.ps1 -Phases B -Items msv,lodu -Reps 2 -GP2MinRun 1 -Tag msv_lodu -UserConfig <#441 config>`.
Metrics: PresentMon GPUBusy (primary), GP2 MinRun 1 rows 63c07c21 -> d5a1a069 (lodu) and the meshlet VS rows (msv), shotdiff
within BASE-vs-BASE noise. Verdict KEEP / DROP / NEUTRAL per item.
