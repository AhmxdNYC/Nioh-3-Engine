# Owned cleanup core: offline checkpoint

Task #11017, cx-pc2. `owned_registry.h` is a bounded two-job C lifecycle
core following the #10541 study and #10841 Python contract. It is deliberately
not included by training.c: existing game behavior and ASIs are unchanged.

Run `py mod/Nioh3Training/tests/run_owned_registry_tests.py`.
The actual generic Windows ASI target passes cancellation before issuance,
indeterminate issuance timeout, late materialization during clear, stale
incarnation/token rejection, at-most-once delete attempt including exceptions,
complete-enumeration/owner-release/HUD proof gates, independent observer state,
world loss, and 200 slot-recycling cycles with balanced logical resource pins
and both player-protection baselines. Assertions remain enabled under -O2.
The move and idle suites also pass (202 catalogue entries).

This core is NOT a native adapter, despawn-all implementation, or multi-spawn
feature. It does not implement encounter/BGM/focus, resource acquisition,
hook quiescence, native world cleanup, or baseline player writes. Resource
counters are logical consumer pins, not proof of native release. There is no
live result. In particular, current pointer/snapshot/handle-only WatchTick
cannot supply the independently proven creation incarnation required here.

Next: native RE owner establishes admission/reservation, request provenance,
owner-slot release, and deferred HUD/hook-reader completion. Then wire one
single-boss path through this registry and cancellation barrier in game-thread
dispatch, followed by owned despawn-all and finally serialized append. Keep
existing kill toggles and Return ownership independent. Do not wire a timeout
or address comparison to a proof parameter merely to make a test pass.
