# Three-waterfall PB2 prototype: offline benefit; native gate closed

2026-10-11. New `multi.py` and `multi_check.py` only; all artifacts in ignored
`out/`. No original shader change, replacement pack, game or deployment.

The exact block35 suffix from CENSUS.md is cloned under a guard on FULL
descriptor index72. Only the equal-active-lane branch replaces that descriptor
with its first-lane value and clears NonUniformResourceIndex. Every other
operand remains per-lane. Both branches preserve the72 original instructions
and their order;13 correctly typed live-outs join before original block176.
Original block145 and every other block are structurally unchanged except
necessary successor phi predecessor labels. The shader still has25 outputs.

Fresh original DXC dump is bound to original SHA256
5a7d10634d334e35ed62a5706fa784c905793f8d8a20a60539be9abd0ea970cc.
Assembled and validated candidate digest11b0311eb9ff27c2db64bd7647d2de5e,
SHA2560d44ddcd220d87d17d2e38bfff2533550fb2e72595dee71f5037d061466985be.
Fresh candidate dump matches transformed body; signatures/PSV remain original,
only WaveOps feature addition permitted in SFI0.

## Concrete generated-code improvement

Fresh same-tool gfx1103 original/candidate comparison:

| Metric | Original | Multi candidate |
|---|---:|---:|
| Whole-module static instructions |574|632|
| Whole-module static VMEM loads |35|46|
| Whole-module static SMEM loads |24|28|
| Whole-module static readfirstlane |7|9|
| Whole-module static branches |18|18|
| VGPR / SGPR |44 /46|44 /38|
| Scratch |0|0|
| Selected region equal-path descriptor loads |3|1|
| Selected region equal-path waterfall repeat loops |3|0|

Candidate equal branch label038C..04A4 keeps one descriptor in s[32:35] for
all seven original dynamic-root loads. Mixed branch labels0200/0250/02B8
retains the three waterfalls. Stock corresponding labels01B4/0214/02A0
load the descriptor separately. Candidate guard adds comparison/branch plus
first-lane work; increased static totals count both cloned paths and are not
runtime instruction counts. The unchanged VGPR count and reduced SGPR count
do not establish an occupancy increase. Read order is preserved in the DXIL;
compiler scheduling differs, as it already does for ordinary read-only loads.

This **passes the narrow codegen opportunity gate**, unlike the rejected
single-waterfall prototype. It does not pass a net performance or native
correctness gate: guard frequency, mixed-case overhead, I-cache effects and
RADV backend behavior remain unknown. Do not deploy or queue as a proven win.

## Actual-source adversarial local proof

`multi_check.py` interprets all72 actual instructions in the original and
both rewritten suffixes, not a hand-transcribed output formula. It supports
raw descriptor loads, extraction, integer operations, comparisons/selects,
cbuffer reads and raw-bit-preserving float bitcasts. Integer overflow flags
are checked, not silently ignored. Fixed-seed synthetic inputs keep the
preserved nsw cbuffer/index add within its defined range.

192 waves /3154 active-lane cases: widths4/32/64, partial/arbitrary masks,
single last-active lane, equal descriptors with differing dependent addresses,
mixed descriptors and independent lane live-ins. 112 equal /80 mixed waves.
All13 live-out values and ordered descriptor/address/read-mask traces match.
Six original interpreter sensitivity faults remain: omitted/inverted guard,
broadcast address, wrong descriptor, changed dependent metadata and a flipped
joined result. These are not tests of actual emitted CFG mutations.

Independent review reproduced false acceptance by the original checker after
inverting the emitted branch or swapping actual phi inputs: both still passed
192 waves/3154 lanes when generated and retained text agreed. The checker now
parses the emitted module, derives clone SSA identities from its definitions,
and validates the actual equality operand, first-lane operand, branch edges,
NonUniform flags, all13 typed phi inputs/predecessors and successor. It derives
the escaping-value census from original uses and verifies every outside-region
instruction under the emitted SSA/block mapping, including downstream outputs.
The interpreted suffixes now come from that parsed emitted source. Guard and
join semantics are structurally validated, not executed by a whole-CFG VM.

24 actual-source mutation negatives are rejected: inverted/omitted guard,
wrong guard/first-lane operands, wrong NonUniform flag, bypassed join, wrong
successor, each of13 swapped phi pairs, missing phi, wrong phi type/predecessor,
and changed downstream output. These mutations alter emitted instructions in
memory; they do not change candidate bytes or write replacement shaders.
Transform assertions additionally prove branch clones are the original region
under SSA renaming, changing only the equal-branch descriptor operand/flag.

Limits: the emitted CFG/SSA structural check supports a compositional argument;
the192-wave interpreter check remains finite and is not execution of the
entire shader or all25 final outputs. Upstream vertex-ID/instance generation
is represented by independent region-entry values; no assumption of adjacent
IDs is used. Synthetic memory provides stable read-only words, with raw float
bits preserved, but does not prove actual resource bounds, validity, concurrent
mutation or every DXIL poison/input combination. Actual backend wave behavior
and malformed native inputs remain untested. One operand domain is deliberately
bounded to avoid undefined signed overflow; that is documented rather than
skipped silently. Native visual/motion/streaming correctness is not established.

The generated ISA uses an offline Windows driver and sink PS, not actual
RADV/material pairing. RGA output alone cannot prove same picture or FPS.

## Next gate

Retain as **offline candidate, native gate closed**. Required before live queue:
independent review of transformed SSA/resource validity and compositional
proof; exact original-record/packed candidate identity and complete applicable
output/mixed-wave proof; actual target compiler/codegen admission. Then the
lead can choose a short exact-execution visual/motion smoke and matched A/B.
No broad MSV rewrite or default-setting change is justified.

Reproduce from root:

    py -I -B gpu-shaders/pb2guard/multi.py
    py -I -B gpu-shaders/pb2guard/multi_check.py

Receipts: `out/multi-binding.json`, `multi-proof.json`, `multi-isa.json` and
`multi.gfx1103.isa.txt`; prior original ISA is reproduced by `isa_check.py`.
