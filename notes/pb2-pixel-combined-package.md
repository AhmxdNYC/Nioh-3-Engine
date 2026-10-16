# PB2 UV + row-base composition: private test package

2026-10-12. Retain the **combined candidate** for the next native gates:
its actual gfx1103 result improves on the standalone UV candidate without
additional registers/resources. No game, installation, ASI or main changes.

## Exact composition and correctness domain

`gpu-shaders/pb2pixel/compose.py` starts with pinned original PS
`0205c80d9035cd8de989cd758a3cf987eda459156ed2bdd9f6224c5ead258268`
and signed UV candidate
`bbd47920e47762776bf232ac49b06f4dd344a35e81b9cb80b14c1ca4bd9b2b34`.
It freshly disassembles both, requires UV to equal the original's exact
approved rewrite, then factors the three row-base groups in that signed UV
body. Only those two input module hashes are accepted by the row-base tool.

The independent whole-body certificate preserves each block's ordered resource
calls, float computations that are not the hoisted pure UV operations, branches
and all output stores. Only integer add/UMad normalizes algebraically, over
Z/(2^32); float expressions remain opaque exact operations. The actual signed
combined body matches the source rewrite and original certificate. All four
interface chunks remain identical.

PASS: 10,657 integer cases including overflow boundaries; 144 paired raw-bit
UV substitution certificates covering positive/negative zero, infinities,
subnormals, finite extremes and different quiet/signaling NaN encodings;
four actual-source negatives reject (wrong address base, UV component,
clamp endpoint, resource identity). The raw-bit fixtures prove expression
preservation, **not numerical execution of GPU NaN behavior**. Identical pure
dx.op evaluations on unchanged operands are assumed deterministic/nontrapping.
Existing undefined/poison domains, e.g. invalid float-to-integer conversion,
are not made defined or claimed rendered correctly. No float reassociation,
resource widening/motion, subgroup assumption or new validity requirement.

## Actual combined emission, not added standalone gains

| gfx1103 RGA metric | Original | UV only | Combined |
|---|---:|---:|---:|
| Instructions | 658 | 645 | 638 |
| VALU | 355 | 346 | 340 |
| VGPR / SGPR | 32 / 38 | 32 / 38 | 32 / 38 |
| Buffer loads / samples | 17 / 3 | 17 / 3 | 17 / 3 |
| Branches / full VMEM waits | 12 / 7 | 12 / 7 | 12 / 7 |
| Scratch instructions | 0 | 0 | 0 |

All comparisons bind the same compiler binary and synthetic root/PSO hashes.
This is compiler evidence, not elapsed GPU time or FPS. No standalone savings
are summed. Original and UV retained receipts are checked against actual ISA
files; those already-passed compilations are not rerun by this tool.

Combined PS SHA256
`a062c69ff48f8fcb0bd8822d17f3a5dcf0ab2b66d6375af9ecfcddfa8bcd4b38`;
normalized body
`ca3406225c17d2b6617e12fcb45d93cc809af43c313040d34b65cba089d4f9dc`;
ISA `f4760fb251e1967db9648c42b64e0d936bf9bb53774b35110f3822547eb2a769`.
Private evidence: `D:/nioh3-re/pb2pixel-20261011/combined.json` plus
`combined.dxbc`, `.ll`, `.gfx1103.isa.txt`.

## Package and reversible ownership

`D:/nioh3-re/pb2pixel-20261011/combined-package/` contains `candidate.file`,
`original.file` and `manifest.json`; target is ONLY
`shader_dx12/0xb4751b0f.file`, zero-based record1. Original file SHA256
`c32b5ae870a80f68f8fced65e85ff4306a81ba2ab5dddb6983aff80c652c817a`.
Manifest SHA256 `b102ce7459f171304a34c20941ca9d7df9c2d129c9601f99a1b5b27deba9a95a`.
The manifest carries full candidate-file and PS hashes and every record's
before/after hash. Exactly record1 changes; companion record and auxiliary
tail remain identical; restoring original PS yields the exact original payload.
Wrong-slot and corrupted-candidate pack negatives reject.

Activation requires device idle, lead lease and exact current original-file
hash, fresh current-entry backup, then a final ownership recheck before atomic
publish. Restore only while idle and if the target still equals this candidate
file hash; otherwise retain backup/receipt and refuse to overwrite newer work.
Verify restored original hash. This package implements no installer or game
control, and is private game-derived material, never a public release.

**Unpassed for this combined PS:** captured Windows PSO admission, exact native
draw/image comparison, live duplicate-container selection, Proton/RADV and
short matched A/B. UV-only admission does not transfer. Root/PC2 can consume
the new exact candidate and original for those gates; retained journal roots
and original VS remain available, but the missing draw resources described in
`pb2-pixel-draw-capture-gap.md` cannot be replaced by invented inputs.

Reproduce: `python -B gpu-shaders/pb2pixel/compose.py --repo
'C:/Users/Ahmad Hamza/nioh3-engine' --out D:/nioh3-re/pb2pixel-20261011`.

Separate future lane, not evidence about this shader: Ally G9LoadCharacterReady
was reported82s versus historical total37–47s. Those endpoints differ; memory
pressure is only suspected. No cause or shader impact established here.
