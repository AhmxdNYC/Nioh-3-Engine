# MIC guarded-header / safe-index candidate

This separate candidate retains the original guards on T[2] and T[1]. For each
of the nine conditionally read SAT corners, it selects the original index when
the original load was taken, or index 0 otherwise. The original value selection
remains after the load. The fourth, originally unconditional corner is unchanged.
No existing shader source or installed/default container was edited.

## Bound output and codegen

`safe_bind.py` verifies the existing original binding, runs `safe.py`, assembles
and signs a separate container, freshly dumps it and checks actual-body equality.
It compiles against the recovered default compute root layout, explicitly
serialized as version 1.0. Local output is `out/safe/candidate.dxbc`.

- Original SHA256: `3566a0efaaeac7724d0354c6a79bd0007ff15db33629f23b0555a324a80aeffc`.
- Safe SHA256: `9a64454f4f89d6571c01cbaf38229aca08ec66e2f0a206fc969a77a9e4409c66`.
- Safe digest: `ed9a06c39e23b641d422411cc4f76144`.

| gfx1103 offline metric | Original | Safe candidate |
|---|---:|---:|
| Static instructions | 2813 | 2767 |
| Full memory waits | 33 | 24 |
| Branches | 101 | 92 |
| VGPR | 72 | 64 |
| SGPR | 62 | 62 |
| VMEM | 77 | 77 |
| Scratch | 0 | 0 |

This retains a concrete codegen improvement despite being less aggressive than
the earlier speculative candidate. It is not an FPS measurement or a native
installation recommendation.

## Why the fallback reduces the resource-contract gap

On each path reaching a rewritten corner, the original already read T[0].
If its original corner predicate is true, the candidate reads the exact original
corner address. Otherwise it reads T[0] again and discards that returned value.
Thus the extra load needs no address absent from the original path; it does not
require a new guarantee that speculative negative/overflowed corner addresses
are safe. Header reads are returned to their original guarded positions, so
an originally skipped header is not made unconditional either.

This argument assumes a valid, stable read-only resource over the dispatch and
the original program's input/definedness domain. It does not prove descriptor
lifetime or race freedom, nor does it make an originally invalid read valid.
Arithmetic needed for the skipped corner is still evaluated; the concrete
integer operations and selections need independent structural review.

## Actual-body finite gate

`safe_check.py` freshly validates both actual container/body identities and the
current rewriter relation. It executes the actual entire original and candidate
CS bodies with the same `full.py` model. Three suites passed:

- 64 arbitrary stable SAT scenes, seeds 0–63.
- 64 valid cumulative SAT holdout scenes, seeds 1024–1087, dimensions 1/2/4/8.
- 256 cumulative SAT scenes, seeds 2048–2303, shuffled serialized threads and
  aliased instance IDs.

Each suite covers all 103 original and 85 candidate blocks and catches six
mutants (each of three corner value selections, removed store, changed sample
offset and zeroed atomic operand). Combined: 384 scenes / 3072 threads.

In addition to matching final UAV bytes and ordered UAV/sample/atomic events,
the check requires each thread's candidate t21 address set to be a subset of
its original address set, and exactly matches all other resource-load traces.
Both new checks pass in every scene. Address-set containment is finite evidence;
it deliberately permits repeated T[0] reads and does not establish timing or
arbitrary concurrent resource mutation equivalence.

The original mutation helper assumed four loads in each folded block; restoring
T[1] produces five, so the separate helper locates six-select blocks. A trailing
DXC comment initially prevented one atomic mutation; it now requires exactly
one replacement and all mutation suites pass. No shader output mismatch was
observed in these development failures.

Full model limitations remain as described in `FULL.md`: synthetic scenes,
reference floating-point semantics, deterministic sample oracle, serialized
thread schedules, and no universal input/race proof. Independent safe-candidate
review remains pending. Nothing was staged or submitted for native testing.

Reproduce:

    py -I -B gpu-shaders/micsat-proof/safe_bind.py
    py -I -B gpu-shaders/micsat-proof/safe_check.py 64 --mutants
    py -I -B gpu-shaders/micsat-proof/safe_check.py 64 --mutants --valid-sat --start 1024
    py -I -B gpu-shaders/micsat-proof/safe_check.py 256 --mutants --valid-sat --start 2048 --alias-threads --shuffle-threads

Receipts: `out/safe/binding.json`, `out/safe/proof-0.json`,
`out/safe/proof-1024.json`, `out/safe/proof-2048.json`.
