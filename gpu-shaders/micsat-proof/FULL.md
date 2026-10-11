# MIC full actual-body finite execution gate

`full.py` executes the freshly bound original and current signed three-copy
rewrite's DXC bodies, not a hand-transcribed SAT formula. This supersedes the
earlier notes' statement that no full-body check exists. It does **not** establish
universal equivalence or native runtime resource validity.

Bound container SHA256:

- Original: `3566a0efaaeac7724d0354c6a79bd0007ff15db33629f23b0555a324a80aeffc`.
- Current: `bc5acd4195f7fe2be492aeb3048b13c8957c2a4d33c0fa01981aee17e4e2bc86`.

The harness rehashes both actual containers, checks their embedded digests,
freshly dumps both through DXC and verifies body identity, and checks that the
current hashed rewriter reproduces all three changes. Its receipt records
harness, interpreter and sample-oracle hashes. A peer found that an earlier
version trusted recorded container hashes and accepted zero scenes; both gaps
are fixed. Zero/negative counts now fail, and partial block coverage has a
separate status. Full coverage remains finite model evidence.

## Result

Three bounded suites passed, each covering every original block (103/103) and
every transformed block (85/85):

| Suite | Seeds | Scenes / threads | UAV reads / stores | Atomic OR | Samples |
|---|---|---|---|---|---|
| Arbitrary stable integer SAT entries | 0–63 | 64 / 512 | 512 / 512 | 449 | 1536 |
| Valid cumulative SAT holdout; width/height 1, 2, 4, 8 | 1024–1087 | 64 / 512 | 512 / 512 | 411 | 1536 |
| Cumulative SAT, shuffled serialized threads, aliased instance IDs | 2048–2303 | 256 / 2048 | 2048 / 2048 | 1692 | 3424 |

For each scene, all eight modeled threads execute against shared UAV state.
The original and candidate agree on final UAV bytes and ordered observable
events: UAV reads, stores including masks, atomic-OR address/value/old result,
and every texture sample argument including resource/sampler, coordinates,
offsets and LOD. The sample result is a deterministic oracle of the arguments;
this catches changed sampling requests but is not a texture-filter emulator.

Each suite separately catches six actual-body mutants: inverted selected SAT
corner in each of the three rewritten blocks, removed store, changed sample
offset, and zeroed atomic operand. Holdout first detections are seeds 1024,
1051, 1037, 1024, 1041 and 1026 respectively. This demonstrates sensitivity to
each rewritten copy and the principal observable output classes.
The aliased suite initially ran only 64 scenes and missed the third SAT mutant;
extending it to 256 caught that mutant at seed 2149. No failing candidate output
was observed. The longer suite is the retained aliased-suite result.

Independent `review_check.py` rejected nine admission faults: nonpositive scene
counts, false hashes/digests, corrupted containers, modified LL with a coherently
modified body hash, and a stale rewriter hash. Four additional actual-source
mutants were caught (texture identity, sampler identity, LOD, dropped atomic).
Another 64 normal and 256 sparse SAT model scenes passed, including empty SAT,
one-word zero width, zero views and header-only storage. Sparse scenes exercise
the interpreter's robust-access contract; they are not observed native bindings.
The review receipt is `out/review-check.json`; original artifacts were unchanged.

## Deliberate limits and remaining native gate

- Finite synthetic scenes are not arbitrary cbuffer/geometry/resource proofs.
  The all-block coverage is useful but does not imply all paths or values.
- SRVs are stable and valid for the modeled dispatch. The native t21 producer,
  bound allocation and actual runtime signature selection remain a separate
  gate. Additional speculative SRV loads are intentionally not required to
  match the original read trace; they must independently satisfy the resource
  contract described in `ROOT-SIGNATURE.md` and `FRESH.md`.
- Typed OOB reads return zero in the interpreter. The separate `check.py`
  exercises arbitrary returned values for discarded corners, but neither
  harness makes an invalid descriptor/lifetime safe.
- Float operations use the existing reference interpreter, including its
  conversion/clamping assumptions and ignored fast flags. The test is not
  hardware floating-point/poison/NaN-payload equivalence certification.
- Threads execute sequentially, ascending or deterministically shuffled by
  scene. Dispatch fixtures include distinct and aliased instance IDs. This is
  not an interleaving test and does not prove arbitrary aliasing/races equivalent.
  Scheduling by raw basic block would not establish equal schedules because
  the rewrite changes control flow; such a misleading check was not added.
- The signed candidate has current offline gfx1103 metrics from the recovered
  default compute layout: 2813→2755 static instructions, 33→20 full waits,
  101→92 branches, 72→64 VGPR, unchanged 62 SGPR and 77 VMEM, zero scratch.
  These are codegen observations, not a measured game performance gain.

No shader was staged, installed or submitted to the native queue by this work.

Reproduce (each local run took approximately one second):

    py -I -B gpu-shaders/micsat-proof/full.py 64 --mutants
    py -I -B gpu-shaders/micsat-proof/full.py 64 --mutants --valid-sat --start 1024
    py -I -B gpu-shaders/micsat-proof/full.py 256 --mutants --valid-sat --start 2048 --alias-threads --shuffle-threads

Receipts are ignored local `out/full-arbitrary-0.json` and
`out/full-cumulative-1024.json`, and `out/full-cumulative-aliased-shuffled-2048.json`.
The earlier `out/full-progress.json` is stale.
