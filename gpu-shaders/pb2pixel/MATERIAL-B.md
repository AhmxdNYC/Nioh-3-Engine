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

## Signed assembly and gfx1103 gate: REJECT

After the game closed, signed assembly succeeded and fresh DXC output matched
the proposed body exactly. ISG1/OSG1/PSV0/SFI0 remained byte-identical. Candidate
SHA256 is `073db04d741267fe35bbd1eb188b3da57fa613606822283e192bf3ad5f2b5700`.
Sequential original/candidate RGA runs used the bound VS70d0ee9f and identical
generated root/PSO inputs. Results:

| Metric | Original | Candidate |
|---|---:|---:|
| Instructions | 2102 | 2080 |
| VALU | 1174 | 1158 |
| VGPR | 56 | 64 |
| Estimated waves/SIMD | 9 | 8 |
| SGPR | 54 | 54 |
| Buffer loads / samples | 71 / 15 | 71 / 15 |
| Branches / full waits / partial waits | 54 / 27 / 54 | 54 / 27 / 54 |
| Scratch | 0 | 0 |

The native queue rejects this variant: twenty-two fewer static instructions
do not justify increased register pressure and lower estimated occupancy.
This is an offline rejection under the candidate's conservative resource gate,
not a measured native regression. There is no pack or installation artifact.
Likely explanation (inference): hoisting to entry extends the UV temporaries'
lifetimes across substantial unrelated material work. Any later retry should
move the common expressions only to the nearest shared dominating region,
not preserve this entry-hoist variant or broaden it to more values.

Private receipt `four-material-uv.json` retains compiler/helper/root/PSO hashes
and both complete metric sets. Original ISA SHA256
`8ad6895cd73746c0674ed1d5d074e973fda70fd03e07babda9afc0fe5e03dcbe`;
candidate `3b183023c1f2fbb48310ca3cb800c31ee366fd7853d97283948b4183f4b01151`.
Reproduce with the previous command plus `--isa`; this assembles and runs the
two compiler jobs sequentially. Generated RGA root/eight RGBA16F target state
is diagnostic, not captured native graphics state.
