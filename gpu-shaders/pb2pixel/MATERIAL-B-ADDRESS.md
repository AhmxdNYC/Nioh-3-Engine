# Local integer row preparation in PS05ad1715

One mechanism, independent of the rejected four-material UV sharing. Source is
the same freshly bound original PS05ad17151de4f513fb82c7428874a7f9, shader SHA256
`4173c9dce9ad919b5238426f345de1d1d648b1c47efeaf1ea2c59e01e558dd4b`,
body `a4dcfea86c85c3ec50f02b20da99382d431fc27b807c2aa9dc8c24e41dd9e3c1`.
No failed UV candidate is composed into this prototype.

The actual shader has eleven four-corner integer address groups. Two groups,
original blocks309/463, have three consumers per corner across later material
layers. These stay byte-for-byte structurally unchanged. Nine groups have one
consumer per corner in their own block:225/387/540/709/797/895/981/1141/1346.
Those compute four `UMad(width,y,x)` values and then add the same base. They now
compute `UMad(width,y,base)` twice and add the lane's own x coordinates. Each row
base is defined immediately before its first address use and dies after its
second address use. No cross-block hoist, descriptor broadcast or new resource
read is introduced. All address arithmetic stays wrapping unsigned32, with no
new nsw/nuw flags. Eighteen UMad definitions are removed in the proposed DXIL.

This is NOT a guarantee of lower registers: the width/x/y operands can remain
live later within each block. Unlike the UV experiment, no expression spans
preceding material regions, but actual occupancy still needs the compiler gate.
That gate must reject any VGPR/SGPR/scratch increase, even with fewer instructions.

`material_b_address.py` implements both the local rewrite and an ordered
whole-body comparison. Only flag-free i32 add and UMad normalize as polynomials
over Z/(2^32). Every other instruction, including float arithmetic, branch,
sample, resource read and output, stays at its original ordered site with exact
operands after integer normalization. Each opaque definition has a unique
ordinal, even identical-looking loads: the checker never equates two resource
results just because their call expressions match. Four actual-body mutation
negatives pass (wrong base, changed resource, changed float operator, substituting
a distinct cbuffer read's result). 4096 arbitrary32-bit overflow fixtures also
pass; these supplement the modular identity rather than pretending to exhaust it.

Signed assembly passes, fresh DXC body matches the proposed text exactly, and
ISG1/OSG1/PSV0/SFI0 remain identical. Candidate SHA256
`efb7895ca386d3e0b43f4d6b61aa20195cf0c0d50c79efc4baedab313b56444c`;
body `65c3d39dd41de65ebc1d3ffcee68faa208e840a93daf9c544ccb3c6772dbab7e`.
Private files: `D:/nioh3-re/pb2pixel-b-address-20261012/local-row.*`.

Codegen is deliberately pending while root game PID31708 is active. No RGA,
game, install, pack or device action in this unit. The script's optional `--isa`
runs only original and candidate sequentially against the exact VS70d0ee9f,
with identical synthetic RGA root/PSO inputs, after the game closes. There is no
native, pixel, image or FPS claim. Existing ready PB2 candidate delivery remains
unchanged and higher priority for actual PC2 testing.

```powershell
python -B gpu-shaders/pb2pixel/material_b_address.py --repo 'C:/Users/Ahmad Hamza/nioh3-engine' --original D:/nioh3-re/pb2pixel-b-20261012/original.dxbc --out D:/nioh3-re/pb2pixel-b-address-20261012 --assemble
```
