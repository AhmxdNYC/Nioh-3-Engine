# Emitter duplicate analysis: actual trace coverage

2026-10-11, Codex. Offline Python analysis only; no native source edits,
build, game, installation or device action. New tools:
`tools/bench/cbv_emit_opportunities.py` and its focused unittest file.

## Existing trace is insufficient for emitter elimination

Inspected actual writer `mod/Nioh3EngineFix/src/cbvupload.cpp:145-183`, call
site `:259`, and harness `tests/cbvupload_harness.cpp:598-697`. The writer
records pre-wrapper **replay table-flush** state, not emitter calls. It writes
bare `CALL` records; its comment at116 advertising a CBUTRACE/version header
does not match either writer or loader. The analyzer follows actual bytes.

Present: stage/mask/layout-count/table-count/root index; context, list tag and
list pointer; layout size/slot pairs; dirty slots' complete replay-shadow
payloads. Absent: emitter call/site/context identity, stream segment and token
offset, emitter-to-replay mapping, emitter source mutation/lifetime, staging
identity, post-call list cut/redo, explicit loss or coherent-capture evidence.

The existing harness selects the most frequent context and simulates list
changes from tag changes; it constructs synthetic lists/rings and compares
GPU-visible bindings. That validates its modeled replay behavior, but cannot
recover how many emitter copies produced an observed shadow or were overwritten
before a flush. Do not equate repeated flush payloads with removable tokens.

## Measured existing file, not a synthetic performance result

Input: `captures/pc/fastab/20261010-1333-cbu1pc/cbvtrace.bin`.
SHA256 `28a7ffb9a7742bd4901d8d48dfd134fe6b8591126cfc98eb08dc6b62ce48db9b`.

| Structural/observation metric | Result |
| --- | ---: |
| File bytes | 106,158,528 |
| Complete CALL records | 30,000 |
| Observed contexts | 1 |
| Dirty payload records | 34,496 |
| Dirty payload bytes | 104,367,440 |
| Observed tag or list transitions | 135 |
| Equal prior slot payload records, same observed context/list/tag/root/layout | 6,733 |
| Corresponding equal payload bytes | 22,255,584 |
| Emitter-correlated calls | **0 (0%)** |
| Candidate eliminated emitter copies/bytes | **unknown**, emitted as JSON null |

Equality counts are replay observations. They are not an estimate of new
savings, not full CBN eligibility (staging/epoch evidence is absent), and not
a same-picture or FPS result. A syntactically complete file can still omit
events because this format has no loss receipt.

## Analyzer behavior and verification

The validator bounds every header/layout/payload, rejects truncated/trailing
data, unsupported stage/slot domains, duplicate slot layouts, and the stale
documented header. It separates context/stage/slot observations, requires
matching root/layout and bytes, and discards cached observations on an observed
tag/list transition. Pointer reuse or missed equal-endpoint transitions remain
unobservable, explicitly excluded from its claims. Reports expose no captured
pointer values or game payload bytes.

Run:

```
py tools/bench/test_cbv_emit_opportunities.py
py tools/bench/cbv_emit_opportunities.py captures/pc/fastab/20261010-1333-cbu1pc/cbvtrace.bin
```

Tests cover byte changes, context/stage/root/list/tag separation, ABA across
observed transitions, layout-root reuse, untouched slots, every truncation
position in a fixture, malformed fields, and mandatory unknown emitter-savings
outputs. Fixtures are synthetic parser tests, not measured opportunities.

## Smallest next collection/proof requirement

A future private paired trace needs bounded records with explicit sequence,
capture-generation/loss, emitter site/context generation, segment+token offset,
token kind/stage/slot/declared size and copied payload. Replay records must
link those token identities to shadow writes, flushes, command-list/staging
epochs, draws, and list-cut/redo boundaries. Retain enough source mutation
evidence to distinguish equal bytes now from a still-live immutable reference;
never retain raw game pointers as ownership.

First use that trace to run original stream versus a candidate-elision stream
through actual replay bodies and compare every draw's bytes/descriptors,
including tiny rings, mid-flush cuts, overwritten pre-draw writes, mutable
source reuse and shader/layout changes. Existing CBN's cut-redo logic
(`cbvupload.cpp:328-344`) is a concrete reason naive emitter skipping is unsafe.
No native recording hook or optimization is implemented in this task.
