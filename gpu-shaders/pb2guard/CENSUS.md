# Larger PB2 descriptor region: concrete follow-up found

2026-10-11. `census.py` completed against freshly dumped original
VS1559714ca496fffe6ddd3c827e1d962e, bound to the pinned original SHA256.
Only this new scanner/note/ignored receipt added; no broader rewrite or compile.

**There is a concrete three-waterfall region worth one bounded prototype.**
Unlike the rejected block250 two-chunk guard, block35 contains seven reads
through exactly one dynamic handle73, created from descriptor index72.
The retained freshly generated original gfx1103 ISA uses the same descriptor
address registerv4 for three distinct waterfall loops:

| Native label | Native buffer loads | Source dependency interpretation |
|---|---:|---|
|01B4|1|initial three-word meshlet record74|
|0214|1|dependent metadata103, including next-index bits|
|02A0|5|remaining dependent reads83/86/91/112/141|

The mapping is supported by shared handle/address and dependency shapes, not
compiler debug-source mapping. All three loops explicitly comparev4 against
its first lane and load a descriptor from heap bases[6:7], using that scalar
address. Their descriptor destination registers differ (second iss[36:39],
otherss[40:43]); destination difference does not imply a different descriptor.
All existing data loads and their dependencies must remain intact.

## Source control-flow boundary

Guard point: **after original SSA72, before createHandle73** in block35.
Exact contiguous suffix is72 instructions, ending before its unconditional
branch to176. Its block has predecessor32 only. No interior branches, stores,
atomics or fences. Calls are createHandle, rawBufferLoad.i32 and a cbuffer
read only. Float operations in this suffix are bitcasts, not reassociation
opportunities. Thirteen values leave the suffix:

`119,121,124,129,131,137,142,143,144,96,97,98,99`.

The isolated prototype could clone this entire suffix under one active-lane
descriptor-equality guard, use a first-lane descriptor only in the equal
branch, retain the original suffix verbatim in the mixed branch, then join
the13 outputs at the original control-flow boundary. Both branches must
execute only for lanes already in block35. Every other live-in remains
per-lane; equal descriptor does NOT imply equal G1M row, vertex/instance,
matrix address, offset or metadata. The alternative original block145 must
not execute either cloned suffix or gain speculative reads.

This boundary permits preserving all read addresses, predicates and order
without lifting loads across original branches. It is a structural feasibility
result, **not proof of a new implementation**. SSA dominance, typed joins and
read-trace preservation must be checked on the actual transformed code.

## Expected opportunity and remaining gates

Three old descriptor waterfall setups versus one equality test is a more
credible scheduling opportunity than guarding an already single-loop region.
But the compiler may clone code, retain descriptor loads, grow live ranges or
lose occupancy; no native saving is established. The13 joined values and72
cloned instructions make register/code-size pressure material. Unknown equal-
descriptor frequency under real RADV VS scheduling remains an important gate.

Next unit: one-record prototype of ONLY this suffix, actual-source local
interpreter supporting every instruction in it, arbitrary active-lane masks
and independent descriptors/offsets, read-address/order and13-output comparison.
Full-width integer wrapping/flags, raw bitcasts, buffer bounds and resource
validity need explicit treatment. Negative controls must broadcast offsets,
skip/invert the guard, alter dependent metadata/descriptor and omit one join.
Preserve input/output signatures, all25 original VS outputs and untouched
blocks; no consecutive-VertexID-wave assumption. Then fresh original/candidate
ISA: compare equal and mixed paths, loads, descriptor setup, code size and
register allocation. Stop if no actual multi-loop advantage survives codegen.
No live queue until that result and the relevant complete proof gates exist.

`out/census.json` records source/ISA hashes, CFG boundary, seven source loads,
transitive dependencies,13 live-outs and the three native loop counts.
Reproduce: `py -I -B gpu-shaders/pb2guard/census.py`.

Pixel-material work remains next if this fails: full-hash PS7dcfe3de and
PS05ad1715 (mapped in `notes/material-pb2-next-opportunities.md`) have distinct
material permutations and texture-feedback requirements. Start with existing
rewrite history and exact paired native costs; do not count prior sampler-
feedback or GHDM work as a new optimization or infer PS-only cost from PSO time.
