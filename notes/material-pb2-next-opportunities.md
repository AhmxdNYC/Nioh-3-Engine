# PB2 material next opportunities — 2026-10-11

Read-only shader/retained-ISA investigation; this note is the only new file.
No shader compilation, source change, deployment or game launch. No new gain
or equivalence claim. Existing MSV remains rejected, not a candidate to reinstall.

## Exact hot-pair mapping

`notes/ally-gpu-budget.md` historical USER rows associate VS1559714c / PS7dcfe3de
with 1.57ms and VS70d0ee9f / PS05ad1715 with 1.10ms. These are old PSO intervals,
not isolated VS measurements; grouping, scene and hardware matter. The recorded
7.54ms G-buffer family total is a prioritization hint, not a savings budget.

Full container hashes from `gpu-shaders/dxil_index.csv`:

| Stage | Full container digest | Original containers, zero-based record |
|---|---|---|
| VS A | 1559714ca496fffe6ddd3c827e1d962e | 0xa79af848.file #10; 0xf00d5c8c.file #10 |
| PS A | 7dcfe3de3f3fcbc199be405eae1596b4 | 0x46f41041.file #1; 0xb4751b0f.file #1; 0xdf8c9879.file #1 |
| VS B | 70d0ee9feee590f9c48c40b808858e27 | 0x0b9df6ef.file #15; 0xabe54628.file #15; 0xfd4f418f.file #15 |
| PS B | 05ad17151de4f513fb82c7428874a7f9 | 0xbc09ad4a.file #1 |

Mapping was independently enumerated in file order; identical shader bytes in
multiple containers do not identify which material permutation the live draw
selected. Do not replace every duplicate without the runtime/permutation gate.

Inspected retained original `gpu-shaders/meshvs/work/1559714c.dxbc` header
matches the full digest; SHA256
`5a7d10634d334e35ed62a5706fa784c905793f8d8a20a60539be9abd0ea970cc`.
`70d0ee9f.dxbc` likewise matches; SHA256
`83d64677954ab9c2368b032ea022659353a701bf6a1daf07c37eeaf93a6d55bf`.
Adjacent original `.ll` files have the corresponding shader hashes
59e15b227755a91e60a1daec1d1e6124 / c67dbeab0c08bc56aa6a724c635cd741.
Fresh DXC-body-to-container binding was not rerun in this read-only task.

## Ranked reductions, with counter-evidence

1. **Guarded descriptor uniformity, leaving every lane's vertex data intact.**
   Both original DXIL bodies use divergent root descriptor handles: SSA202
   at block199 and SSA257 at block250 derive the same root index179+1.
   VS A lines476-477 and544-545 surround index/vertex fetches; VS B
   lines484-485 and552-553 have the corresponding pair. Retained ORIGINAL
   `meshvs/work/isa/0xa79af848_10_gfx1103.isa.txt` has repeated descriptor
   waterfalls at labels01B4,0214,02A0,0490,053C,05B8,0630, each with
   readfirstlane, lane equality, descriptor load and a repeat branch.
   This is concrete repeated scheduling work, not proof all loops share the
   same live domain or can safely be fused.

   Small candidate: in one bounded region, test active-lane equality of the
   FULL descriptor index. Only the equal branch may use a first-lane index
   with NonUniformResourceIndex false; keep original mixed branch unchanged.
   Keep per-lane offsets, matrix/instance IDs, vertex indices and all float
   instructions unchanged. Do not broadcast slot, payload, primitive, G1M
   row or matrix based merely on descriptor equality. Uniform fraction,
   extra branch cost, code size and actual RADV codegen are unknown. This is
   a targeted research proposal, not a ready patch or promised reduction.

2. **Three-index temporary elimination, lower priority.** Both bodies contain
   a local three-i32 array: decoded 10-bit triangle indices216/218/220 are
   stored, then SSA226 reads element VertexID%3 (VS A lines498-506; VS B
   corresponding209 block). A direct integer select is a bounded algebraic
   alternative, with no cross-lane sharing. However retained original ISA
   has no scratch/movrels/movreld matches, so the compiler already avoids
   obvious memory traffic. Only consider a prototype if target ISA shows
   fewer instructions/registers; reject unchanged codegen. No float algebra
   or matrix reassociation is justified by this audit.

3. **Do not implement apparent scalar-load packing as a new optimization.**
   VS A block35 DXIL has ten scalar G1M-row loads at offsets0/4/8/12,
   16/20/24 and36/40/44 (lines318-336); VS B has the analogous sequence.
   Retained original gfx1103 ISA ALREADY combines these into b128 at0,
   b96 at36 and b96 at16 (lines75,76,99). The alternate t26 record loads
   are also combined (lines186-189). Fewer DXIL calls alone would manufacture
   a paper win. Wider loads crossing missing fields are additionally unsafe
   without bounds evidence. This negative finding prevents wasted work.

Retained ISA is a historical offline compiler artifact, not current Proton
ISA or runtime binding proof. Verify artifact provenance and target compiler
when advancing any codegen claim. Redundant createHandle expressions and
repeated cbuffer loads may already be eliminated; don't count them as native
cost without checking generated code.

## Output and proof contract

VS A emits25 scalar outputs: SV_Position xyzw, TEXCOORD0/1 xyzw,
TEXCOORD2/3 xyz, TEXCOORD4 xy and TEXCOORD5 uint. VS B emits31:
SV_Position xyzw, TEXCOORD0-3 xyzw, TEXCOORD4/5 xyz, TEXCOORD6 uint.
Preserve all stores, not just position or a screenshot; previous-position/
motion-related outputs, material IDs, normals and texture coordinates matter.
Keep input/output signatures, interpolation and PSV metadata exact except
explicitly justified WaveOps capability if the candidate needs it.

Before any native test:

- Bind original container, fresh dumped body, transformed body, assembled
  body and packed record by hashes. Change one original VS record only;
  unrelated records and associated pixel shaders remain byte-identical.
- For the descriptor proposal, cover arbitrary vertex-ID ordering and mixed
  slots, duplicate IDs, partial active masks, divergent paths, all-equal
  descriptors with DIFFERENT lane offsets, distinct descriptors with equal
  payloads, both draw modes and both index-width branches. Preserve exact
  per-lane read addresses/values and outputs. No assumption of consecutive
  IDs or a wave containing one384-vertex slot.
- Cover every changed block and holdouts. Guard inversion, omitted guard,
  broadcast vertex offset, incorrect descriptor and dropped-output mutations
  must fail. Preserve raw float bits, NaNs, signed zero and original arithmetic
  order; an interpreter that collapses NaN payloads cannot claim bit equality.
- The previous MSV proof skipped nonterminating random cases and assumed
  consecutive-ID waves. It did not cover the runtime wave composition that
  produced stretched triangles on Ally. See `notes/gpu-rewrite-round3.md:296`.
  Do not reuse its PASS as this proposal's evidence.

First native unit after offline gates: exact paired VS/PS full-hash witness
with MinRun1 in a representative USER scene, then short movement/streaming
and geometry/motion image smoke. Preserve installed controls and restore.
One matched15-20s profiler-off A/B pair can triage; it cannot establish a
small noisy improvement or prove global visual equivalence. No isolated VS
cost can be derived merely by comparing total PSO times.

**Next code task:** a one-record, one-region guarded descriptor candidate
plus adversarial arbitrary-lane proof. First reject it offline if target
ISA does not actually remove waterfall overhead or increases resource cost.
No blanket187-file MSV rewrite, no default picture change.
