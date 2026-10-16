# PB2 one-region descriptor guard: reject native promotion

2026-10-11. Isolated offline prototype. No game launch, installed file writes,
replacement-pack staging or shared-source edits. Original remains untouched.

Target VS container1559714ca496fffe6ddd3c827e1d962e only. In block250, guard
descriptor index256 with WaveActiveAllEqual. Equal active lanes use its
WaveReadLaneFirst value for one uniform handle; mixed lanes retain the original
nonuniform handle. Both branches fetch the same two16-byte vertex chunks at
each lane's own offsets254/255. Eight uint32 phi values rejoin the unchanged
arithmetic/output remainder. No slot/vertex/matrix broadcast occurs.

The two read-only loads are grouped before local arithmetic; they keep their
per-lane order and original addresses. Correctness depends on the original
SRV data being stable for the invocation, as required for the reasoning here.

## Actual offline results

- Fresh DXC dump binds retained original body to original container SHA256
  5a7d10634d334e35ed62a5706fa784c905793f8d8a20a60539be9abd0ea970cc.
- Assembled candidate validates/signs, freshly dumped body equals transformed
  body. Candidate container digestf15a2ac91d56a6411621e01edbfb0e69.
  Original ISG1/OSG1/PSV0 preserved; SFI0 changes only WaveOps.
- Changed-region test:384 waves,6456 active-lane cases,221 equal/163 mixed
  waves, widths4/32/64 and arbitrary nonempty masks. Per-lane descriptor/read
  addresses and all eight joined uint32 values match. Five intentional
  mutations rejected: missing guard, inverted guard, broadcast offset, wrong
  descriptor and dropped output value. Structural assertions retain every
  instruction outside the selected loads/extracts and successor phi labels.
- Fresh original/candidate gfx1103 RGA offline compilation, identical sink PS:

| Static metric | Original | Candidate |
|---|---:|---:|
| instructions |574|593|
| VMEM loads |35|37|
| SMEM loads |24|25|
| readfirstlane |7|9|
| branches |18|20|
| full memory waits |11|12|
| VGPR / SGPR |44 /46|44 /46|
| scratch |0|0|

These are static totals across both branches, not executed instruction counts.
The equal path DOES remove its local waterfall repeat branch (candidate
label06A4); the mixed branch still waterfalls (label0650). But the original
already groups BOTH chunks into ONE waterfall (label0630), so uniform stock
executes one iteration. The candidate adds all-equal detection, branch and
another first-lane read, saves no vertex loads or register allocation, and
leaves the other waterfalls intact. There is no demonstrated net benefit.
**Reject this isolated region for native queue/promotion.** This is a codegen
triage decision, not a measured FPS loss and not a rejection of every broader
descriptor optimization.

## Limits of the check

The interpreter executes actual selected instruction strings from the
rewriter, including actual handle/load/extract arguments. It tests independent
region-entry descriptor and byte-offset values, a broader space than one
consecutive-ID wave model. It does NOT execute SV_VertexID-to-offset computation,
the complete vertex shader, real backend wave scheduling or all final outputs.
Unchanged remainder plus identical joined values is a local compositional
argument, not an executed25-output full-shader proof. Synthesized read values
are uint32 hashes; no float operations are modeled or NaNs canonicalized.
Native descriptor validity, out-of-range resource semantics and malformed
upstream inputs are not established. No universal same-picture claim.

RGA's offline Windows gfx1103 driver/sink PS is not RADV or the actual paired
material PSO. Artifact hashes and detailed receipts are in ignored `out/`.
Original/candidate signatures alone do not establish all platform behavior.

## Reproduce without touching game files

From repository root:

    py -I -B gpu-shaders/pb2guard/prototype.py
    py -I -B gpu-shaders/pb2guard/check.py
    py -I -B gpu-shaders/pb2guard/isa_check.py

All outputs and temporary compilation folders stay under this directory.
Imports reuse local assembler/parser/compiler utilities without staging.

Next valid bounded step: inspect whether a larger contiguous same-descriptor
region can share more than one native waterfall while retaining lane-specific
addresses. First build an exact use/control-flow census; do not simply broaden
this guard or resurrect the failed MSV scalarization. Existing ready `lodu`
native gating remains more actionable than this rejected micro-candidate.
