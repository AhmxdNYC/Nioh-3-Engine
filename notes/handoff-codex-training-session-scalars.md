# Training loading/session scalars - 2026-10-11, Codex

Owned private codex-training-pump, parent2674878, add-on1.4.4. Default-OFF
Observe and known-build/CORE/FIGHT/PUMP prerequisites unchanged. No deployment.

Pump independently resolves/reads CORE GameMain+58A (loading byte),
PlayMode+34 (signed mode), NetMgr+1DC (connection byte), using the existing
rematch read offsets without calling rematch. Each global/object hop uses actual
SEH Rd, low/null and addition-overflow checks. Output holds scalars only; no
retained game pointer, native call, write, argument traversal, UI or logging loop.
Each scalar has its own validity and gap. Unknown publishes zero plus invalid;
readable zero and unfamiliar raw values remain valid without classification.
These offsets inherit existing read provenance, not new native runtime acceptance.

Unknown sets that field's gap, including the first readable recovery sample;
the next readable sample clears it. The cached sample also includes a sampled
publicationLosses total. A changed total marks all new fields and missionGap
on the next publication, including after a contention skip. A later concurrent
loss may be reflected only in a subsequent sample. Total-loss query remains an
independent atomic read. Sequential native reads and skipped/unknown intervals
cannot prove a coherent native session or complete transition history. Mission
observed-identity epochs retain their prior limits; session/loading generations
are not implemented here. Re-enabling can expose the existing cached sample;
sequence/time allow consumers to assess age but are not a freshness guarantee.

Validation of changed actual source/production declarations: MSVC /W4 /WX,
31 snapshot checks PASS with actual SEH Rd and allocated/PAGE_NOACCESS memory.
Includes valid zero/raw values, unknown/recovery, a mode read straddling an
unreadable page while loading/connection remain readable, null/low/overflow
objects, skipped publication and gap clear, plus existing mission witnesses.
One fixture initially overlapped mission and telemetry storage; corrected to
separate allocations before final PASS. Actual lifecycle143/startup wiring,
existing add-on60, ALL94 SITES OK (shifted-image simulation), warning-free
isolated release build and diff check PASS. No FPS/visual/native safety claim.

Next narrow unit: define observed loading/session generations from valid raw
changes with explicit unknown/skipped recovery semantics, then bounded scalar
events/loss and consumer transport. Runtime acceptance and v4 restore mutation
gate remain separate. Installed/pinned inputs and device owners untouched.
