# PSa11: retained-ISA rejection and larger local row prototype

The two-OR index fold is closed without compiling: all seven PSa11 variants in
the retained gfx1201 ISA archive contain zero scalar/vector `or_b32` in their
pixel entry. The similarly named GS section is explicitly excluded. The audit
binds association through the retained RGP `pso_map.csv`, records each complete
ISA file hash, and fails if the seven-variant count or zero-OR observation
changes. These archived files are not evidence of current gameplay execution
or a new measured GPU result. No candidate was signed or queued for that fold.

The next prototype targets 13 same-block four-corner integer address groups
in the same exact PS `a11ff9e5ffd0f3356d74865e29e38228`. Each original address
is `(w*y+x)+base`, using flag-free wrapping i32 operations. It becomes
`row=w*y+base; address=row+x`, sharing each row only across its two corners.
The row is created at its first consuming address, not at block entry. All
four original corner values must have exactly one same-block add consumer;
the four other groups fail that requirement and remain untouched.

Changed blocks: 1049, 1153, 1266, 1370, 1467, 1800, 2223, 2334, 2472,
2583, 2722, 2812, 2922. Retained: 176, 412, 594, 773. This removes 26 DXIL
UMad definitions; it does not claim 26 fewer machine instructions. The retained
ISA contains row-address preparation but exact source-to-ISA attribution and
whether this rewrite improves its lowering remain untested.

The all-bit identity follows associativity in Z/(2^32). No float algebra,
resource stability, equal-looking read substitution, branch/loop assumption,
or speculative read is used. The whole function differs only by the exact
listed integer edits; all resource/float/control instructions stay in order.
Per-lane operands must be defined (no claim about arbitrary LLVM poison).
Original source hash and sole-use/local ordering are enforced. 5,392 modular
boundary/random cases pass; unexpected float, resource and address mutations
are rejected. This is a narrow structural argument, not a whole-DXIL interpreter.

Original container SHA256:
`da83f00e00883f3a4824f75276461de0efd2e452c364be54b1f64265f940839c`.
Proposed normalized body SHA256:
`f303f5e5604394fd73fa57a71b0295701dfc0259fd7f7c3dcba995902ccfcfae`.
Private output: `D:/nioh3-re/pb2pixel-c-row-20261012/local-row.ll` and
`static-gate.json` (individual edit sets and retained ISA hashes).

No GPU work during manual gameplay. Next gate after the root releases the
GPU is signed roundtrip/interface validation and one sequential paired
original/candidate ISA comparison. Reject if instruction/register tradeoffs
do not justify it. Allocated registers, native PSO creation, real execution,
image equivalence and performance are all unpassed. Existing PS7dc/PS05ad live
test packets retain priority; their acceptances do not transfer.

```powershell
python -B gpu-shaders/pb2pixel/material_c_static_gate.py --repo 'C:/Users/Ahmad Hamza/nioh3-engine' --original D:/nioh3-re/pb2pixel-c-20261012/original.dxbc --archive D:/nioh3-re/moved/re_scripts_gpu_out_rgp_top300/Nioh3_2026-10-06_19-39-58_shaders/isa --out D:/nioh3-re/pb2pixel-c-row-20261012
```
