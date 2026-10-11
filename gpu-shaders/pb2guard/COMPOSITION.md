# PB2 whole-VS compositional equivalence gate

New `composition.py` closes a stronger IR-level gate than finite suffix
execution alone. It proves conditional compositional equivalence of the entire
bound VS, including all25 output instructions, using exact structural terms and
unchanged CFG/SSA context. It does **not** execute the whole VS or establish
native compiler, pipeline, visual or performance acceptance.

## Actual input binding

- Original SHA256 `5a7d10634d334e35ed62a5706fa784c905793f8d8a20a60539be9abd0ea970cc`, digest `1559714ca496fffe6ddd3c827e1d962e`.
- Candidate SHA256 `0d44ddcd220d87d17d2e38bfff2533550fb2e72595dee71f5037d061466985be`, digest `11b0311eb9ff27c2db64bd7647d2de5e`.

Both actual containers are rehashed and freshly DXC-dumped, with their executable
bodies checked against retained LL. All container parts except DXIL/HASH/SFI0
are byte-identical; SFI0 adds only WaveOps4000h. Input/type/entry declarations
are identical. Outside the function body, only the two wave declarations and
the expected entry WaveOps flag change are accepted. Resource/output metadata,
function attributes and the original resource range remain identical.

The checker does not invoke `multi.rewrite`. It independently locates the
specified original72-instruction suffix and derives its escaping-value census
from actual uses outside that suffix. It reuses the independently hardened
`multi_check.inspect_cfg` to bind every actual clone definition, operand,
guard edge and typed join, and every outside-region instruction under a
bijective SSA/block renaming. Reusing that checker is explicit: this is not
a second independent implementation of its CFG parser.

## Conditional universal argument

The domain is any execution where the original shader's inputs are defined,
descriptor handles/resources are valid and read-only contents remain stable,
and the backend conforms to DXIL active-lane wave semantics. This does not
assume consecutive vertex/instance IDs, full waves, descriptor equality across
the whole draw or a fixed wave width.

1. Every instruction leading to the region is unchanged under SSA renaming.
   Thus corresponding participating lanes reach the guard with identical
   per-lane live-ins and active masks. The original has no wave operation whose
   downstream participation semantics could be affected by this insertion.
2. If active descriptor indices are mixed, the mixed branch is exactly the
   original suffix, including its NonUniform flag. If all are equal, the
   first active lane's **integer descriptor index** equals every participating
   lane's own index. All lanes active at the guard take the same guard outcome;
   no operation changes participation before the first-lane read.
3. Only that index is replaced in the uniform branch; lane-specific vertex
   indices, offsets, arithmetic and resource-read operands remain untouched.
   Removing NonUniform there is valid under the established uniform-index
   domain. No raw resource value, floating-point value or vertex address is
   broadcast.
4. The checker interns exact typed operation tuples, including flags, constants,
   masks and symbolic live-ins. It uses the equal-descriptor lemma only for the
   first-lane index and the justified handle annotation. Both branches then
   produce exactly the same13 escaping terms and14 ordered read terms as the
   original. Equality uses tuple interning, **not hash equality or sampled
   numeric values**. All integer overflow flags and bitcasts remain unchanged.
5. Every typed phi supplies the corresponding escaping value to the same
   continuation, with only the necessary predecessor-label change. Every other
   block/instruction—including all25 output writes—is identical. Substituting
   equal live-outs therefore preserves the remaining original computation and
   output requests on its defined domain. Paths that bypass the region are
   unchanged. No new memory address or read mask is introduced.

This proves equality of the relevant IR computation under those assumptions;
it does not assert that separately optimized native programs have bit-identical
floating-point results under all backend fast-math choices. Original undefined
inputs/poison are excluded. Raw NaN payloads survive the changed region exactly,
but a later original fast-FP instruction may itself exclude NaN inputs from its
defined domain. The proof does not redefine that domain or certify malformed
native draws.

## Adversarial sensitivity and raw-bit cases

31 negative cases are rejected: the previous24 actual emitted CFG/phi/output
mutants, changed read mask, changed raw bitcast, removed overflow flag, changed
original/candidate container bytes, resource metadata and entry feature flags.

An additional finite actual-suffix check covers272 waves /5616 active-lane
cases at widths1/4/32/64/128, sparse masks including only the last lane, equal
descriptors with different lane operands, and mixed descriptors. Explicit raw
read patterns include +0, -0, +Inf, -Inf, quiet/signaling NaNs and arbitrary
FFFFFFFF. All13 live-outs and ordered reads match without converting the raw
float bits through host floating point. These tests check implementation
sensitivity; the structural argument above supplies the universal scope.

    py -I -B gpu-shaders/pb2guard/composition.py

Receipt: ignored `out/composition.json`, including actual container and checker
hashes. Local run approximately2.2 seconds; no compilation, installation or
game operation. New candidate bytes were not generated.

## Remaining native gate

The next gate is now actual target translation/pipeline admission, as specified
in `ADMISSION.md`: exact VS paired with the real material PS/root/input/topology
state; correct container permutation; active VS subgroup support and correct
uniform/mixed lowering on the actual backend. The capability snapshot alone
does not prove this translation. After lead admission, use a short controlled
visual/motion smoke and matched A/B. No default install or native performance
claim follows from this certificate.
