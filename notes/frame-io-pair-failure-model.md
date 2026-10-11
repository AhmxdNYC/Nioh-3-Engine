# Paired meshlet readback: offline failure model

2026-10-11. New standalone model `tools/bench/frame_io_pair_model.py`; tests
`tools/bench/test_frame_io_pair_model.py`. No native calls or installation.

The model uses the decoded five-uint64, 40-byte CopyBufferRegion record:
destination, destination offset, source, source offset, byte count. Mode0
dependencies remain zero as bound in `frame-io-writer-closure.md`. Buffer content
is represented by immutable source identity/range, not fabricated capture bytes.
This is an operational contract model, not a replay of actual game memory.

**Result: the tempting eager pair is still NO-GO.** Success reduces modeled
submissions2→1 while preserving ordered A/B maps, consumption and closes, but
that is not a performance measurement. B creation failure skips the stock A
copy/map; combined A-map/B-create failure changes which operation fails first.
A/B independent submit failures also do not automatically map onto one paired
submission. These cases are not rejected using advance knowledge of injected
failures: otherwise admission would cheat by predicting runtime failures.

The model records a deliberately strong lifecycle trace (map, close, failure
stage), not just final consumer output. A difference in that trace is evidence
that stock failure semantics have not been preserved; it does not prove that
the game displays a different picture or reports a distinct public error code.
The underlying native error path may collapse several failures into the same
manager flag. Proving that collapse harmless requires further native analysis.

Cases exercised:

- Wrapper alias A==B; overlapping source or staging ranges; destination/source
  aliasing across either copy. Guarded execution falls back before preparation.
- Stock rereads B pointer and count after A maps. An unguarded captured pair
  diverges if either changes. The model's stable-epoch/lifetime flags represent
  requirements, **not native guarantees already achieved**. Known future
  reread values are fixture inputs; a runtime hook cannot predict them.
- First/second staging creation failure; first/second Map failure; independent
  A/B submission failure and combined submission failure.
- First Map failure still permits an unnecessary B copy in the eager pair,
  although its modeled lifecycle output agrees. This extra source read is
  explicitly retained in the record trace and requires a stable-resource domain.
- Pending fence and unknown fence status retain all prepared wrapper/source/
  staging references. Attempts to release either source or staging before
  completion throw. After explicit completion, reverse cleanup releases each
  reference exactly once. Unknown fence status is **not** cancellation.
- Required reference trace: all pins precede the first GPU copy; every release
  follows the last fence completion. No CPU Map is permitted while work remains
  pending. Successfully mapped wrappers return to state4 during cleanup.

The model currently treats submit failure as known rejection before queue
ownership, solely to expose the failure-order problem. Native partial-submit
or device-loss semantics are not established; they need an unknown-ownership
state with retained resources until the backend's retirement contract is known.
It must never assume a failed API result means the GPU owns nothing.

Next valid step: design a preparation transaction that keeps B preparation
failure pending while preserving A's stock work and failure precedence, then
prove allocator side effects and submit ownership with actual helpers. Until
that exists, retain the original sequential maps; a compare-only observer is
still the first possible native measurement. No speed claim or native patch
follows from these model results.

Verification:

    py -I -B tools/bench/test_frame_io_pair_model.py

15 bounded tests, including adversarial counterexamples and reference ordering.
No game, executable, GPU compilation or persistent process was touched.
