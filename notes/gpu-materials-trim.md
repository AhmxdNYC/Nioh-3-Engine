# Material / long-tail shader trim: strictly-redundant work only (2026-10-07)

Rule (after notes\ally-replacement-ab.md: gi/fog/toonedge2 lost on RADV/ACO): only remove work that is provably
redundant, with the same or fewer instructions, loads and registers, no unrolling/reordering. RGA (AMD Windows
compiler) is a sanity check only.

Tools (gpu-shaders\materials\): `extract_ps.py file:idx` (record -> work\*.dxbc/.ll), `redund.py X.ll` (dominated /
sibling duplicates, dead values, redundant clamps, repeated memory ops), `dedupe_loads.py` + `build_dedupe.py`
(LOAD-DEDUPE builder + proof + staging), `rga_ps.py` (now generates a root signature from the binding tables, uses
`--gpso`, `--stage vert`), `mkvs.py` (pass-through VS for a PS), `get_rec.py` (record out of a staged file).
Interpreter fixes in `psim.py`/`verify_sfb.py` (they made every material PS fail before): bindless `t0,space3unbounded`
ranges parsed, sigIds from the module's own signature table (it can omit container elements), out-of-range local /
constant-array index modelled identically for both programs, byte-addressed register-info buffer, array bindings
(`t12 x16`), LegacyF16ToF32.

## Per shader (GP2 Ally ms/frame from D:\nioh3-re\gp2_L2_top20.md)

| shader | cost | strictly-redundant work? | result |
|---|---|---|---|
| PB2 opaque: PS 0xdf46710f#1 `PB2-BlrGbiMm00MshltPkSfbVl` / VS 0xfd4f418f#15 | 0.645 | PS: none static (only 2 re-read cbuffer rows, scalar, CSE'd downstream). Dynamic only: SFB MinMip clamp loads the same tile 4x when the 4 corners coincide (see SFB below). VS file 0xfd4f418f: 14 sibling records (#18-#33) re-load 5 dwords right after `storeOutput` | VS: **pbvs** staged. PS: no qualifying change |
| PB2TREE2: PS 0xd864e310#1 / VS 0x52c39db0#3 | 0.337 | PS: none static; SFB as above. VS #0-#3: instance-matrix rows (3 x float4) re-loaded after `storeOutput` | VS: **trvs** staged. PS: no qualifying change |
| PB2UMAH (water) PS/VS 0xef2fa7c0#1/#0 | 0.326 | none (12 dominated cbuffer re-reads + 1 `1-x` duplicate: folded by any compiler) | nothing to remove |
| PB2MRLR PS 0xa60921c6#108 (+WDO_SST VS) | 0.324 | none (all 13 texture fetches distinct; 4 cbuffer re-reads) | nothing to remove |
| PB2DS PS 0x04df8ee9#1 (deferred, 44 SampleCmp) | 0.285 | none static. The mip-3 2x2 textureLoads use x and x+1 (always different texels); loop-invariant VALU <= 10/loop | nothing to remove |
| VCLOUD CS 0x5f17ca9d#7 (async) | 0.386 | 329 dominated cbuffer re-reads (scalar, CSE'd downstream). Real invariant work: each of the 2 march loops recomputes 110 VALU ops (109 cbuffer-only) per step - LLVM could not hoist them past the loop's early exit. Hoisting = 81 new loop-carried values (SGPR/VGPR) | **fails the register rule**: not done |
| SDFOCC PS 0xd070067d#9 (+VS 0xfd20cb1a) | 0.283 | march loop recomputes 8 per-thread invariant fsubs per step; hoisting = +8 VGPRs. File is also owned by the gi item | not done |

Counted with `redund.py` on every record above; no textually identical texture sample, no dead value, no provably
redundant clamp, no identical sample/load operands in any of the PSs/CS.

## SFB-DEDUPE (killed agent's work, finished and measured): does NOT qualify

Every streamed material clamps each sample's LOD with `max(MinMip[i00], MinMip[i01], MinMip[i10], MinMip[i11])`
(4 `Buffer<uint>` loads per texture; 0xdf46710f#1: 14 groups = 56 loads). Away from a tile border all 4 indices are equal.
`sfb_rewrite.py` keeps i00 and moves the other 3 loads + uitofp + max tree behind `if (i01!=i00 | i10!=i00 | i11!=i00)`.
Proven bit-exact with the fixed interpreter (both paths taken: e.g. 86/94 pixels per group), but static cost goes UP:

| record | DXIL instr | blocks | RGA gfx1103 inst / branches / full waits / VGPR |
|---|---|---|---|
| 0xdf46710f#1 orig -> sfb | 1479 -> 1591 | 60 -> 88 | 1967 -> 2189 / 50 -> 68 / 25 -> 44 / 56 -> 56 |
| 0xdf46710f#1 'loads' mode | | | 2222 / 66 / 40 / 56 |
| 0xd864e310#1 orig -> sfb | 802 -> 842 | 21 -> 31 | 1118 -> 1177 / 27 -> 32 / 13 -> 17 / 36 -> 40 |

(gfx1201: 56 -> 60 VGPR for 0xdf46710f.) The loads only move; the branch adds compares, exec-mask work and memory
waits on every wave that straddles a tile border. Not staged; the staged SFB files were deleted from replacement\.
An Ally A/B could still settle it, but it is outside today's rule.

## LOAD-DEDUPE (qualifies): trvs, pbvs

DXC cannot CSE a read-only `rawBufferLoad` across `storeOutput` (side-effecting call), so these VSs load the same
SRV dwords twice a few instructions apart. `dedupe_loads.py`: a load on an SRV (createHandle class 0; module has no
UAV/stores at all) whose operands are identical to a load in the same block <= 64 instructions earlier is deleted
and its uses take the first value. Far repeats (e.g. 0x52c39db0#3 line ~1280 vs ~385) are left alone: they would
stretch live ranges if a driver did not merge them already.

Proof per record (`build_dedupe.py`): exact by construction (same read-only resource, same index/offset/mask, nothing
writes memory); module assembled == rewritten text; ISG1/OSG1/PSV0/SFI0 unchanged; dx.op counts equal except
exactly the removed loads; psim interpreter 400 invocations x 6 random scenes bit-identical (the rewritten blocks ran
in all 2400 invocations, except 1 of 3 blocks in 0xfd4f418f#19 that the random scenes never reach).

| item | file | records | per record | DXIL (example) | RGA gfx1103/gfx1201 |
|---|---|---|---|---|---|
| trvs | 0x52c39db0.file (PB2TREE2 VS) | #0-#3 | -3 loads | #3: 2226 -> 2223 instr, 60 -> 57 rawBufferLoad | identical ISA (2780 inst, 92 VGPR) |
| pbvs | 0xfd4f418f.file (PB2 meshlet VS) | #18-#25, #28-#33 (14) | -5 loads | #19: 1597 -> 1592 instr, 127 -> 122 rawBufferLoad | identical ISA (2131 inst, 100 VGPR) |

RGA identical = AMD's compiler already merges them; expect ~0 on the Ally too if NIR merges the loads (reorderable
read-only SSBO / texel-buffer loads). It cannot add work on any compiler; it only matters if the driver did not merge.
Low priority A/B.

Files: sha256 original -> replacement (Steam v2.02), `gpu-shaders\materials\dedupe_manifest.tsv`:
- trvs `0x52c39db0.file` 18f82522...cb91 -> 64679acf...2d2b
- pbvs `0xfd4f418f.file` 806b2d7f...da40 -> a323fe72...8ab2

Install / revert: Ally `./ally_shaders.sh apply|revert|status trvs` (or `pbvs`), Windows
`gpu-shaders\apply_materials.ps1 -Item trvs|pbvs [-Revert|-Status]`. Rebuild: `py -I gpu-shaders\materials\build_dedupe.py
--files 0x52c39db0.file,0xfd4f418f.file`. Note `ally_shaders.sh apply all` now includes both.

## Not done / next
- Bigger same-look wins in these shaders are dynamic (SFB corners, VCLOUD loop invariants); both need an Ally A/B
  with extra branches/registers, which this rule excludes.
- LOAD-DEDUPE could be scanned over all VS records (`build_dedupe.py --scan --files ...`) if trvs/pbvs ever measure positive.
