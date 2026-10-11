# Mission queue: four physical records, three admissible positions

2026-10-11, Codex. Existing exports and bounded Capstone inspection of inert
`re/Nioh3.dump.exe`; no game/Ghidra/source/device action. Supplements
`training-queue-consumer-proof.md`; it does not establish cancellation.

## Findings

The native object really contains **four 0x58-byte records** at
`45BC420..45BC580` (end exclusive). Normal inspected producers only admit
indices0..2. The fourth record, at `45BC528`, is an empty spare at the normal
boundaries of the inspected initialization/reset/enqueue/retire paths.
That is a bounded invariant of these paths, not a complete-writer claim.

An earlier shorthand "scans three slots" is imprecise: `223953C` scans
through the physical four-record range, then rejects selected index>=3.
Native `2239553..2239573` examines mission+4/byte+8, advances by0x58, compares
the next record against end `45BC580`; `2239578..223957C` enforces index<3.
The UI producer `F1FD80` independently does the same scan/reject
(`F1FE63..F1FE92`). Emaki's exported scan has the same physical end, and its
`index > 2` check precedes record mutation (`0x10dbed0.c:125-145`).

| Path | Concrete evidence | Fourth-record effect |
| --- | --- | --- |
| Initialization `AE117C` | zeros0x160 bytes; passes stride0x58, count4, constructorA74400 to167D70; calls223552C | Four initialized records, followed by explicit empty fill. |
| Reset `223552C` | memset local0x58, A74400 default constructor, R8=temporary, call10D47A0 | All four receive the same default-empty semantic record. |
| Fill `10D47A0` | Straight-line leaf loads the same R8 fields and writes each of four global records | Reachable reset writer; no index or source-pointer advance. |
| Checked accessor `10D93D0` | rejects RDX>=4, computes base+index*0x58 | Confirms physical capacity4; does not by itself restrict mutation to3. |
| ById/Core/Region/Quest enqueue `223953C` | checked selected index<3; only confirmed direct E8 calls to accessor are2239581/2239594 | No normal accepted enqueue writes fourth. |
| Replay-menu direct producer `F1FD80` | independently index<3; writes mission/start/mode2 with computed base+index*58 | No normal accepted menu enqueue writes fourth. |
| Emaki `10DBED0` | separately index<3, writes selected semantic fields | No normal accepted Emaki enqueue writes fourth. |
| Retirement `223568C` | overwrite head with empty; left-rotate four semantic records via three reversals | Normal end state `[B,C,D,empty]`; with old D empty, it remains an empty spare. |

Reset callers in the existing `223552C` export are initializationAE117C,
MissionStartFromSave2237244 and cleanup2235788. Direct native calls at
AE11B0 and2237270 were located; the cleanup export calls reset at its tail.
Continue can therefore reset the queue and immediately enqueue another
native request. Observing title/Continue or reset alone is not a complete
"no future load" witness.

## Two important limits for a future completion receipt

**Fourth empty is a boundary invariant, not an every-instruction invariant.**
The three-reversal retirement algorithm temporarily moves live requests
through that record. Starting `[empty,B,C,empty]`, reversing the tail yields
`[empty,empty,C,B]`; the final reversal yields `[B,C,empty,empty]`.
A snapshot during mutation can see a nonempty fourth without any illegal
producer. Pre/post original-call receipts must account for interleaving;
independent atomic/guarded reads do not make a coherent queue snapshot.

**A second fill leaf exists.** `10D95D0..10D9859` repeats the same record
pointed by RDX into all four positions. It does not copy four distinct source
records. An initial reading suggesting four input records was corrected by
checking the unchanged RDX and repeated offsets0..57. No confirmed direct
E8/E9 caller or absolute code-pointer reference was found for that leaf in
this dump. This absence is not proof that computed/indirect entry is
impossible; if called with a nonempty request it would populate fourth too.

There are also pointer-return/iterator leaves exposing base/end or computed
base+index*58 (`10D5F80`, `10D5FA0`, `10D8C50`, `10D8C60`, `10D9400`,
`10D95C0`, `10D9860`, `10D9870`, `2235864`). No confirmed direct E8/E9
callers were found in this bounded address census. `223586C` returns3,
consistent with logical capacity, but its call provenance is likewise not
closed. Do not infer a complete writer inventory from unreferenced-looking
helper bodies.

## Search and evidence limits

To find exact native references efficiently, executable-section 32-bit
displacements were filtered for target addresses in the queue range using
NumPy, then Capstone checked candidate instruction boundaries from enclosing
unwind entries. Raw hits in unwind gaps were only hints; they were separately
decoded from exact leaf starts before being used. The initial unaligned
candidate output included false instructions and is **not** proof evidence.
End address45BC580 is also referenced by unrelated synchronization code: it
lies outside the queue, so those writes do not establish fourth-record writes.

This discovers direct RIP-relative bases/field accesses, not arbitrary
register-derived aliases, absolute/immediate addresses, dynamically generated
code, indirect calls or every cold-fragment path. Exact direct E8/E9 references
to the identified pointer/fill helpers were checked against native instruction
boundaries. No completeness claim follows for indirect references.

Executable section range checks and flat RVA/raw identity were inherited
from the preceding native inspection. Straight-line leaves have no covering
`.pdata` entry; their entry and ret boundaries were verified within the gap
before the next unwind function. Nonleaves use `.pdata` ranges.

| Half-open byte range | SHA256 |
| --- | --- |
| AE117C..AE11C1 | `5c7bb51fa9640a378f24d0c8c9dd4c135b99e711be50a31cf2604097be7ff8de` |
| 223552C..223555D | `1cccda689a64a25978dcb232205adcb99b67b55a528c59868c5286d25c80d199` |
| 10D47A0..10D4A6C | `1345c40d791e10f03a3b539e487a547bda177a12ab1de640be093abf2f22104d` |
| 10D93D0..10D93F3 | `4994fcc3ca9b3e2571bc7a644564332c919dba9c9862defb87bb36d1080377e3` |
| 10D95D0..10D9859 | `2a38fb9d94f67e7fc2030add74db43f4a926b753d3405624a941dc935444038a` |

## Consequence for implementation

Keep existing new-request admission limited to the three accepted positions.
A future native retirement receipt must copy **all four physical records**,
use semantic fields (padding is not copied), and establish pre/post call
boundaries without concurrent mutation. Require a known predecessor snapshot
where the fourth is empty, exact rotation relation, and loss-free attribution
of the admitted request to the head. Unexpected fourth content at a stable
boundary should invalidate attribution, not be erased by the add-on.

The inspected paths explain the capacity mismatch and justify the empty-spare
model under stated boundaries. Complete writer/alias and concurrency closure
remain unproved. No arbitrary quarantine release or queue clearing is enabled
by this note.
