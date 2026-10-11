# Training entry observer groundwork - 2026-10-11, Codex

Private `codex-training-pump`, parent c297a20; add-on 1.4.2. This bounded unit
implements prerequisite registration and independent default-OFF entry counting,
not the complete observer or training mode. No deployment/public sync.

`Training Observe=0` performs no observer registration or prerequisite lookup.
`Observe=1` requires the exact known-build flag and CORE/FIGHT/PUMP resolution.
The three handlers register after BossLog registration and before every FIGHT
installer, including rematch preparation/debug installation. One Init-owned pump
serves observer and separately enabled rematch. Observer-only startup skips
rematch Prepare and Pump/Tick and does not start UI. Training Enable is reserved
and does not activate mutations. General Enabled remains the outer startup gate.

Create/begin/end handlers ignore all arguments, including pointers, and increment
local atomic entry totals only after successful FIGHT and pump installation.
Pump callbacks during installation remain passive. Counts() returns independently
loaded atomic totals, not a coherent session snapshot. No entry is represented as
a spawned/live actor, death, completed fight or native readiness. No synchronous
logging, native calls, game reads or writes occur in these observer callbacks.
Existing independently enabled rematch and debug handlers retain their behavior;
the observer does not make those handlers read-only.

Validation: actual training.cpp plus extracted production dispatcher/live bodies
compiled under MSVC /W4 /WX: 143 checks PASS. Covers default-off, unknown build,
each missing prerequisite, failed FIGHT/pump/rematch preparation, callback during
installation, observer/rematch combinations, disabled callbacks, invalid pointer
arguments, registration idempotence and INI defaults. Static Init wiring checks
registration/startup precedes debug FIGHT and UI remains rematch-live gated.
Existing add-on harness 60 checks PASS; ALL 94 SITES OK including shifted-image
simulation. Isolated release build without warnings; diff check PASS.

Limits: production Prepare's rematch setup is represented by an injected test
stub, not fully executed. OnEntry has no return status; current registrations
fit its four-handler limit (begin three, end/create two) and 96-site table, but
this unit does not generalize future capacity/install safety. Entry totals can
wrap and convey no epoch, order, attempt identity or event-loss guarantee.
No mission/session snapshot, epoch/event ring, pipe/UI observer output or native
runtime acceptance yet. Later snapshot/event work must replace these interim
totals with validity-aware copied evidence and bounded-loss accounting.

No local game/PresentMon/Ghidra/restoration required. No installed files,
benchmark-pinned inputs, saves/settings/hardware/controller/holds/Claude changes.
Stable EngineFix1.7.61, FO NO-GO and guard PR1 unmerged remain. External device
and player gates remain independent. Next: validity-aware read-only pump scalar
snapshots and mission/load/session epochs, then bounded scalar events; preserve
observer-only isolation and distinguish all unknown reads from false/zero.
