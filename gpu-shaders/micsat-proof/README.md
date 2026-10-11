# MIC SAT audit: conditional local pass; robustness and artifact gates open

2026-10-11. Only isolated verifier/receipt/note added. Existing micsat.py,
original shaders, installed files and device state untouched.

## Concrete pass/fail insight

**The rewrite does issue out-of-bounds reads that stock avoids.** For a valid
4x4 one-view SAT, top-left coordinates0, the original corner region reads
only index3. The actual transformed instructions also evaluate the first
discarded-corner index4294967294 (uint32(-2)). A strict-bounds memory model
accepts the original and rejects the candidate. This falsifies an in-bounds
justification, not the candidate's output under a robust read contract.

`check.py` executes the actual original corner CFGs and actual transformed
select bodies from micsat.rewrite, using a capture of its pre-renumbered Fn.
All3 copies,6000 finite comparisons pass with zero OR arbitrary0xdeadbeef
out-of-range values; a deliberately inverted select polarity is rejected.
Discarded reads have no influence on the returned SAT hit boolean in these
cases. Both original and candidate use stable synthetic read-only contents.

Header hoisting has a separate obvious bounds witness: T[0]=0 with only one
element originally exits afterT[0], while the candidate also readsT[2]/T[1].
This witness is source inspection, not included in the6000 corner executions.
The original width>=1 and view-count tests still guard coordinate arithmetic;
micsat.py does not unconditionally move its divisions or float conversions.

## Resource semantics matter

The shader binds t21 as a typed Buffer<uint>, not arbitrary CPU memory. The
candidate's stated contract depends on safe robust out-of-range loads whose
unused values can be discarded. The strict-bounds witness is therefore NOT
evidence of an observed D3D12/Vulkan crash or different picture.

Pinned local vkd3d-proton source `D:/refs/vkd3d-proton/libs/vkd3d/device.c`
lines2639 onward rejects missing robustBufferAccess2 or robustImageAccess2;
it also requires nullDescriptor. Lines2619 onward explain why robustness
cannot simply be disabled for D3D12. This supports a potential sound target
contract but does not identify the installed Ally binary, configuration,
descriptor range, lifetime or current resource binding. Do not merely cite
generic optional Vulkan robustBufferAccess as proof for this device.

No concrete t21 allocation/descriptor-size/lifetime witness was established
here. Full native acceptance remains closed until target robustness/resource
validity is bound, including header and discarded corner reads. A resource
freed concurrently or invalid descriptor is not excused by in-range/OOB rules.

## Stale ISA/proof identity caught

Full indexed original MIC#0 digest: e31c389f6d4a38de7b1b784b87c710d2,
`gpu-shaders/dxil_index.csv`: 0x17ad25bb.file record0, offset588,24976bytes.
Verifier uses retained `meshvs/work/mic/mic0.ll` with SHA256
3acc9a120cacddb06222c096dcea37170a35d80e50eb44840ab4ec7b1de57b4b.
Fresh original-container-to-body binding has not yet been established.

**Current3-copy rewrite body does NOT equal retained mic0_sat.ll.** Therefore
old retained ISA is not a bound codegen proof for this verifier's transform.
Reading the retained gfx1103 artifacts yields:

| Retained metric | orig.gfx1103.isa | sat.gfx1103.isa |
|---|---:|---:|
| instructions |2830|2822|
| full waits |33|30|
| branches |101|98|
| VGPR |72|64|
| VMEM loads |77|77|

These are historical static metrics, not new compile results. Round4's
gfx1102 table reports33->20 waits and101->92 branches, on a different target
and potentially different retained rewrite revision. Do not mix that table
with these gfx1103 artifacts or call either a current native speedup.

## Limits and next concrete task

The6000 checks start at each original corner branch. Header reads, float
coordinate math, full-CS sample oracle, atomics/UAV outputs, concurrent
threads and final resource equivalence are outside this checker. Independent
input states are tested, including states beyond valid geometry; the finite
result does not establish universal behavior. Read traces intentionally differ
because speculative reads are the transformation. Only SAT hit output is
compared under stated memory semantics, and integer remainder uses signed
DXIL behavior with invalid divisor cases rejected.

Next required unit: locate exact original MIC container and bind fresh DXC
body, assemble the CURRENT3-copy transform, then fresh same-target ISA.
Concurrently bind t21 producer/descriptor limits and actual runtime robust
contract. Only then extend proof through all samples/atomics/stores. Until
these gates close, **no native queue or staging**, and no new performance win.

Run: `py -I -B gpu-shaders/micsat-proof/check.py`.
Receipt: ignored `out/receipt.json`. Existing source/shaders are read-only.
