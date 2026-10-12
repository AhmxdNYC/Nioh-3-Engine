# PB2 PS7dcfe3de: exact binding and UV hoist probe

Private offline work only. No shader staging, installation, native admission or
FPS claim. Fresh original/candidate gfx1103 codegen completed after the lead
confirmed game closure and restoration.

## Input and concrete candidate

`gpu-shaders/pb2pixel/bind.py` extracts original `0xb4751b0f.file` record 1,
requiring full file SHA256
`c32b5ae870a80f68f8fced65e85ff4306a81ba2ab5dddb6983aff80c652c817a`
and full record SHA256
`28bbce9b2d4585c69be7439fc59c75a21bc339dfdff3fede9d5c693938c46533`.
Exact PS digest `7dcfe3de3f3fcbc199be405eae1596b4`, extracted container SHA256
`0205c80d9035cd8de989cd758a3cf987eda459156ed2bdd9f6224c5ead258268`.
Fresh normalized DXC module SHA256
`f01abd800dbbe594ffe931f3f373db599abb749695e031b0064a9825ceeeeabd`.
Private outputs live at `D:/nioh3-re/pb2pixel-20261011`.

This exact shader has three distinct texture/sampler/clamp sample tuples,
five raw material loads and twelve typed min-mip loads, and twenty scalar
render-target stores. Reusing samples just because their UV is shared is wrong.
Its three four-corner min-mip groups are the already rejected SFB branch
dedupe direction; see `notes/gpu-materials-trim.md`. Do not revive that claim.

The new bounded probe instead hoists only identical pure UV subexpressions:
Frc(u), Frc(v), Max(u,0), Max(v,0), Min(Max(u,0),1), Min(Max(v,0), repeated
across three independently selected wrap/clamp paths. Eighteen static
operations become six, with no reassociation, resource-read motion, sample
reuse, new branch, descriptor broadcast or address-expression change.
Original SSA groups are 88/176/272, 89/177/273, 111/199/295, 112/200/296,
113/201/297 and 114/202/298. `uv_probe.py` requires the exact input body and
checks each group's identical opcode/operands before moving it.

This is not automatically a performance improvement: hoisting lengthens
lifetimes, evaluates both pure wrap/clamp variants, and the native compiler
may already perform equivalent common-subexpression elimination.

## Structural check and explicit limits

`check_uv.py` independently canonicalizes expressions and control-flow block
identities. Every block's ordered resource calls, samples, output stores and
terminators must match after expression substitution. All ten blocks PASS;
three actual-body mutants (wrong UV component, wrong SRV register and changed
clamp constant) fail. Two input mutations also fail the exact module binding.
No resource or memory validity assumption is added by this pure-op rewrite.

Signed assembly, fresh body equality and ISG1/OSG1/PSV0/SFI0 equality PASS.
The certificate also checks the actual signed candidate, not just source IR.
Finite full-output checks: 256 pixels across four shared resource scenes,
all 10 blocks covered, output/discard/UAV logs identical. This is supplementary
finite evidence, not universal float/resource verification.

The unmodified old `verify_sfb.py` initially mismatched because it creates each
scene from that module's debug resource names; reassembly strips those names.
An actual oracle trace isolated the difference to the first sampler's name
(`sBindlessSamplerState@s0` versus `@s0`), with identical remaining operands.
The new check creates both independent scenes from the original binding table.
No shader instruction changed to hide the mismatch; the shared resource binding
is necessary to compare the same program inputs. Existing oracle limits remain.

The structural argument assumes identical deterministic dx.op behavior for
identical operands and unobservable evaluation of unused pure float results.
It does not use an algebraic NaN simplification, float tolerance or finite
random coverage as a universal proof. Native helper-lane behavior and driver
compilation are not simulated.

## Fresh emitted result: small viable offline reduction

| gfx1103 offline metric | Original | UV hoist |
|---|---:|---:|
| Instructions | 658 | 645 |
| VALU | 355 | 346 |
| VGPR / SGPR | 32 / 38 | 32 / 38 |
| Buffer loads / samples | 17 / 3 | 17 / 3 |
| Branches / full VMEM waits | 12 / 7 | 12 / 7 |
| Scratch instructions | 0 | 0 |

The reduction is real emitted work: `v_fract_f32` 6→2, fused
`v_maxmin_f32` 6→2, `v_mov_b32` 7→5, `v_swap_b32` 2→3,
`s_delay_alu` 86→83 and `s_waitcnt_depctr` 8→7. Static counts include both
wrap/clamp branches; do not equate eight fewer static UV instructions with
eight saved instructions on every lane, or 13/658 with a frame-time gain.
No memory traffic reduction is claimed. Original code already narrows the
three material loads with unused fourth dwords to b96, so a DXIL-mask trim
would be a paper optimization on this compiler.

Candidate SHA256
`bbd47920e47762776bf232ac49b06f4dd344a35e81b9cb80b14c1ca4bd9b2b34`;
normalized body
`8cd2f2b9e1ac1a2422d27665ec90454b7cd23845fb5a96e6c879a40c8ee36138`.
Original/candidate ISA SHA256:
`5f9acbc2a7e1e8b845ad1d5010df64f3d5f78f2aa2c736fa9898759eb89f662b` /
`8d05adf8ba61416dfa5da0243b38e499bcb5ae703d475f243f7e7c042d417cbc`.
Both compilations used identical generated root and pipeline hashes, recorded
with compiler hashes in private `binding.json` and `uv-hoist.json`.

Decision: retain for independent review and exact captured-PSO admission,
not native installation. Next concrete gate is the captured graphics root/PSO
for this exact PS and paired VS, then a one-record pack with untouched tails
and all other records identical. Actual Proton codegen and bounded native A/B
remain required; historical whole-PSO timing is not isolated pixel cost.

## Reproduction and admission

Run the scripts from the isolated worktree with `--repo` pointing at the main
private repository and `--out D:/nioh3-re/pb2pixel-20261011`:

- `bind.py`: fresh extraction and DXC binding; `--isa` adds original gfx1103 ISA.
- `check_uv.py`: structural certificate and mutation negatives, no compiler.
- `uv_probe.py`: signed assembly, fresh-body and metadata checks; `--isa` adds
  candidate gfx1103 ISA.

RGA uses the existing synthetic root signature and eight RGBA16F target harness.
It is an offline compiler diagnostic, not a captured native PSO/root signature,
Proton/RADV proof or runtime measurement. Reject the probe if there is no actual
native instruction/resource benefit or if registers/waits meaningfully worsen.
