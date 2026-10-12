# PB2 PS7dcfe3de: exact binding and UV hoist probe

Private offline work only. No shader staging, installation, native admission or
FPS claim. Codegen comparison waits for the lead's local-game-closed notice.

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

The structural argument assumes identical deterministic dx.op behavior for
identical operands and unobservable evaluation of unused pure float results.
It does not use an algebraic NaN simplification, float tolerance or finite
random coverage as a universal proof. Native helper-lane behavior and driver
compilation are not simulated. Signed assembly/body/metadata checks and native
codegen comparison must pass before considering a next gate.

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
