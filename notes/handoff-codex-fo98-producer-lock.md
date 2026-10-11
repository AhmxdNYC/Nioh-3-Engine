# FO98 resource producer lock modes - 2026-10-11

Bounded offline trace, main parent 41a1b74; experimental 24494d5 unchanged.
FO remains NO-GO. No runtime, lifetime, safety or performance verdict.

Native 12B0D0 adds 198 to the resource pointer. Mode 1 tail-calls
AcquireSRWLockShared through IAT 38E2628; mode 2 calls
AcquireSRWLockExclusive through IAT 38E2618; every other mode returns
without acquiring. Native 12AF40 uses the same object+198 address and
mode dispatch: mode 1 releases shared via IAT 38E2630, mode 2 releases
exclusive via IAT 38E2620. Import identities were read from the existing
dump's PE import descriptors; these are CPU SRW locks, not GPU waits.

2F06AB4 preserves its fourth argument (r9d), acquires with that mode,
calls 152D48 with resource+C0/resource+10/tile descriptor, conditionally
writes the 12-byte 1010098 record into context+3C40 and updates
resource+190 from context+6EB8, then tail-calls the matching release.
Thus the selected lock spans both bookkeeping and emission. A shared
mode permits concurrent shared holders; a no-lock mode provides no
exclusion here. No universal producer exclusion or closure follows
without establishing actual caller modes and surrounding ownership.

152D48 has no call instruction in its 432-byte native body. Its decomp
computes descriptor-selected tile/mip bounds and raises bytes in the
bitmap at (resource+C0)+18, returning a changed flag. This helper does
not add a synchronization/wait edge. 2F06AB4 ignores its return value.
The separate 2F06B5C path conditionally calls the same bitmap helper
after 2EEA644, but has no local 98 write or SRW acquire/release. Its
surrounding ownership is unproven; do not infer a race from this alone.

## Verification and checkpoint

py -I re/scripts_renderer/fo98_producer_lock_provenance.py pins the two
43-byte mode helpers and the 432-byte bitmap body. Three new static
provenance checks PASS. Byte identity is not a behavioral safety test.
Native review used Capstone and existing decomp; no Ghidra or raw export.
An optional pefile import was unavailable; PE import names were resolved
with standard-library struct instead, without installing dependencies.
Prior passed suites were not repeated.

No local game/test at entry/checkpoint; no game lock needed. No build,
deployment, installed ASI/INI/config/save/settings/hardware/default visual
change. Prior restoration retained; unrelated captures untouched.
Controller untouched; no Claude call/wake. Phone inbox empty. Remote
1767/1768/1781 remain pending at last known bridge1785; no fresh query.
Serial ownership until exit; commit/push with Codex attribution and
Discord result via existing notifier after push.

Next ready unit: identify actual indirect callers of resource vtable+28
and their r9d modes, beginning with resource construction/live-map paths;
trace context stream ownership and closure across endFrame. Cache+20
identity, GPU completion, ring restart and combined queue/bounds/lifetime
repair remain open.
