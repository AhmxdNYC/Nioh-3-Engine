# Training bounded entry records - 2026-10-11, Codex

Owned codex-training-pump, parent ce5c7e3, private add-on 1.4.6. Observe stays
OFF by default. No deployment or changes to device-pinned EngineFix inputs.

Create/begin/end handlers count admitted live entries and try the existing local
snapshot lock without waiting or reading arguments. A successful publication
copies kind, local publication sequence/time and the full cached scalar snapshot
(including validity/gaps, sequence/time and mission/load/session generations).
The 64-record ring retains the newest records. Overwriting an oldest record
increments capacity loss; unavailable lock increments contention loss. Totals
are independent atomic samples and can lag an event read. Sequence orders local
publications, not native entry arrival. Counters retain existing modulo limits.

Events copies an oldest-first bounded prefix without draining. It returns zero
when disabled, with null output or zero capacity. A short caller buffer truncates
the returned view, not the stored ring. Repeated reads repeat retained records;
consumer cursor/transport is next. Query copies can contend with entries/pumps.
Records may precede first pump (unknown zero snapshot), contain unknown scalars,
or carry aged cached observations after reactivation. A copied generation does
not prove that an entry belongs to that native session, an actor is alive, or a
fight is ready. No game pointers, native calls, mutations or dynamic allocation.

Actual observer plus production SEH reads: 53 checks PASS under MSVC /W4 /WX,
including invalid argument non-dereference, disabled behavior, event kinds/order,
copied generations/time, explicit contention, ring overwrite, prefix/no-drain
and copied-history stability after another pump. Actual lifecycle143/startup
wiring and warning-free isolated release build PASS. Compatibility results in
lead ledger. No runtime acceptance, performance or visual claim.

Next: cached query consumer with cursor/retention/loss/age semantics, keeping
observer-only startup isolated. Separate restore ownership and player/runtime
gates remain unfinished. No native mutations permitted by this observer unit.
