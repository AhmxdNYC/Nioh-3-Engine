# Third PB2 material: local integer C-buffer index folding

Static-only unit during the user's manual game session. No GPU, game process,
input, bridge, ASI or settings access; game shader files were only read.

Untouched PS `a11ff9e5ffd0f3356d74865e29e38228` is paired historically with
VSbe0fa132 in a 0.77ms USER PSO interval. The interval is not isolated pixel
cost. Fresh extraction finds slot3 in `0xabb677e6.file`, full file SHA256
`817056c5d554e20d10d0bfb5a933f11b7d49cceb2f504c80773a7f028214bfdf`;
shader SHA256 `da83f00e00883f3a4824f75276461de0efd2e452c364be54b1f64265f940839c`.
The duplicate shader in another pack is not a reason to replace that pack.

Two exact same-block chains prepare material C-buffer indices:

- block323:326=shl(input325,1);333=or(326,1);334=add(333,50).
- block2014:2032=shl(input2031,1);2033=or(2032,1);2034=add(2033,3).

Because flag-free32-bit `shl(x,1)` has bit0 clear, `(y|1)+c` equals
`y+(c+1)` modulo2^32. The prototype removes333/2033 and changes only the two
adds to51/4. It preserves both shifts, every resource call/index value, every
float instruction and all control flow. Both removed OR results have exactly
one use. The replacement is at the adjacent add, so no preparation is hoisted
over intervening material work; one live temporary is replaced, not augmented.
Overflow/high-bit input values require no extra assumption or nsw/nuw flags.

`material_c_index.py` binds the whole original body, verifies exact source
chains, local ordering and sole consumers, and checks the complete edit set.
69,635 integer fixtures pass plus a wrong-constant negative. The full-bit
argument is the known-zero low bit and modular addition; finite tests alone
are not presented as exhaustive proof. Output body SHA256
`e1cacc397d56a56e556560ea2823f99c63d4866ef429704f3d86cb963285d95b`.
Private proposed text/receipt: `D:/nioh3-re/pb2pixel-c-20261012/fold-index.*`.

No assembly or RGA was run. This is deliberately a small preparation candidate,
not a promised material-pass gain. The driver may already fold both chains;
next gate after manual gameplay ends is signed roundtrip and a single paired
original/candidate ISA comparison. Reject immediately if emitted code is
unchanged or registers worsen. Do not deploy on the strength of fewer DXIL ops.
Ready PS7dc and PS05ad native packets retain priority; no prior rejection changes.

```powershell
python -B gpu-shaders/pb2pixel/material_c_index.py --repo 'C:/Users/Ahmad Hamza/nioh3-engine' --out D:/nioh3-re/pb2pixel-c-20261012
```
