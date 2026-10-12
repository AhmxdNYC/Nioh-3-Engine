# Untouched second PB2 material: four-material UV preparation

Fresh read-only extraction binds `0xbc09ad4a.file` record 1 to full PS digest
`05ad17151de4f513fb82c7428874a7f9`; full shader SHA256
`4173c9dce9ad919b5238426f345de1d1d648b1c47efeaf1ea2c59e01e558dd4b`.
Its historical paired VS is `70d0ee9feee590f9c48c40b808858e27`.
The historical 1.10ms paired interval is a prioritization hint, not isolated
pixel cost or an available savings budget.

`material_b_uv.py` implements one input-bound candidate on this untouched shader.
Four material samples use the same input XY `%98/%99`. Each independently
calculates Frc(X/Y) or min(max(X/Y,0),1), under its own wrap-mode branch.
Exact original definitions are grouped as (666,754,852,938),
(667,755,853,939), (689,777,875,961), (690,778,876,962),
(691,779,877,963), (692,780,878,964). The prototype hoists six pure operations
after input99 and removes eighteen duplicate calls. Each group's operand/opcode
identity is checked after the exact substitutions; the original body hash is
required. No broad impure-value numbering is used. No resource read, sample,
handle, derivative or subsequent float arithmetic changes position or order.
Special float inputs retain identical operands and opcode; there is no numerical
host-float approximation or reassociation. Executing unused pure expressions
adds no memory accesses. Unknown/poison domains gain no new definedness claim.

Sample-count reduction was rejected in this slice: the four samples share XY
but use distinct texture/sampler pairs 71/73,75/77,79/81,83/85 and independent
LOD clamps. The repeated 87/89 pair elsewhere reads distinct array layers and/or
transformed coordinates; it is not a redundant sample. Reusing those values
would need texture/content assumptions absent from the shader contract.

Run generated the exact proposed body privately at
`D:/nioh3-re/pb2pixel-b-20261012/four-material-uv.ll`, body SHA256
`887090cb9e5910a81731351bff2d18bcfe25de3cd57d1c15ac2bf75d17992baf`.
No compiler/RGA was run while root's live pilot was active. Next bounded gate:
assemble (`--assemble`), fresh body/interface validation, then sequential original
and candidate gfx1103 codegen using the actual paired VS. Reject if eighteen
fewer DXIL operations fail to reduce emitted cost or create a worse register/
scratch tradeoff. This is a prototype, not a deployable pack, and has no FPS,
image or native acceptance claim.

```powershell
python -B gpu-shaders/pb2pixel/material_b_uv.py --repo 'C:/Users/Ahmad Hamza/nioh3-engine' --out D:/nioh3-re/pb2pixel-b-20261012
```
