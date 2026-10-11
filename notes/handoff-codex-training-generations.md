# Training observed generations - 2026-10-11, Codex

Owned private codex-training-pump, parent dfc134c, add-on 1.4.5. Default-OFF
observer prerequisites and single dispatcher unchanged; no deployment.

loadGeneration starts at one on the first valid loading scalar, then advances
once per observed raw change or readable recovery after unknown/skipped samples.
Unknown samples retain the generation and mark loadingGap. Both rising/falling
and unfamiliar raw changes count; this is not a native attempt counter.

sessionGeneration starts on the first separately readable mission/play-mode/
connection tuple, then advances once for a tuple change or gap recovery, even
when recovered values equal the last tuple. sessionValid requires all three;
sessionGap combines their gaps. Partial unknown samples retain the generation.
Simultaneous tuple changes advance once. Loading and retry-only changes do not
advance this session tuple generation. No native session identity, atomicity,
readiness or complete transition history follows. A skipped interval with equal
endpoints still advances both valid generations to invalidate prior observations.
No event/readiness consumer is implemented yet.

Changed actual source and production SEH Rd: MSVC /W4 /WX, 44 checks PASS,
including disjoint scalar storage, same-mission load changes, simultaneous raw
session changes, repeated unknowns, equal-value recovery, contention recovery,
mission change and retry-only control. Actual lifecycle143/startup wiring PASS;
isolated release build warning-free. Final compatibility checks recorded in the
lead checkpoint. No game/native runtime, performance or visual acceptance claim.
Reactivation can expose cached old snapshots; sequence/time are age evidence only.
64-bit counters have the existing modulo arithmetic and no lifetime uniqueness.

Next: bounded scalar entry events tagged with copied observation generations,
explicit contention/capacity loss and a query consumer. Unknown/skipped native
history and asynchronous entry ordering must remain explicit. v4 restore mutation
ownership and runtime gates remain separate. Pinned device inputs unchanged.
