# Terrain normal normalization removal: reject exact-output gate

Untouched PB2LS terrain PS `7d506745cbc46263c10ca650410188c0`, paired historically
with VS55954792, is a 0.77ms historical USER PSO lead. That is combined PSO cost,
not an isolated pixel budget. Higher-ranked fog/GI/cloud/meshlet mechanisms
already have active gates or prior rejections; this is a small direct material
lead rather than another broad rescan.

Fresh source binding: `shader_dx12/0xdc52fd1c.file`, record12. Full file SHA256
`e1e2937569f67c7d477909ffb219f44dd05a1a5abd0bdbb880c2f8a6a1b81c09`;
shader `d14932706805fa9e3736bf32c40ac0a2fc42637e2c3688dd26714c3386ac1110`;
fresh DXC normalized body
`2bbda09d89e10bfa44e5374c0af0bf554c651d5f966a179ab7de19559949b931`.
Private source/receipt: `D:/nioh3-re/terrain-normal-20261012`.

Mechanism examined: remove sampled-normal normalization at SSA121–125
(dot3, rsqrt, three multiplies) because the final octahedral encoding at
SSA197–204 divides by the absolute-component sum, apparently cancelling any
common positive scale. This would shorten a normalization dependency without
extending temporaries. However real-number scale cancellation is insufficient.

`terrain_normal_gate.py` evaluates the exact bound output dependency slice
ending at actual output SSA220/221. Only the candidate aliases123/124/125 to
118/119/120; other slice arithmetic retains its order. With ordinary finite
sample RGB `(0.5233004093,0.6356523633,0.5860449076)`, an orthonormal basis,
identity view and handedness+1, binary32 per-operation evaluation produces:

| | Encoded Y | little-endian binary32 bytes |
|---|---:|---|
| Original | 0.45244771242141724 | 3aa7e73e |
| Normalization removed | 0.45244768261909485 | 39a7e73e |

Reject from the exact-output lane before assembling/compiling. This is a
counterexample in an explicitly bounded floating-point model, **not** measured
AMD fast-math codegen or proof of a visible/quantized render-target difference.
No claim that one ULP is visible. Native fusion, reciprocal approximation and
target conversion are not emulated; the result defeats a blanket exact-bit
algebraic argument and provides no basis to promise unchanged output.
Do not relabel this as a same-picture optimization without a stronger actual
codegen/output contract. No shader/package/install was produced.

```powershell
python -B gpu-shaders/pb2pixel/terrain_normal_gate.py --repo 'C:/Users/Ahmad Hamza/nioh3-engine' --out D:/nioh3-re/terrain-normal-20261012
```
