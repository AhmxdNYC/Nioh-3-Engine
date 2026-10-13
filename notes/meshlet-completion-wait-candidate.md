# MW meshlet completion wait candidate ? 2026-10-13

Status: implemented default OFF, not installed or game-tested. No CPU/FPS benefit claimed.

## Behavior

`[Patches] MW = 1` opts in at startup only. Reader3FD5C replaces the existing Sleep(0) call after256 stock polling iterations with one WaitOnAddress round on the actual manager+5F8 byte. The original3FD46 comparison remains authoritative after every return, including timeout/spurious wake. The round requests1ms, which is not a guaranteed wall-time bound. No extra spin phase or simulation/quality change.

Writer981211 executes the original byte-zero only after4A2D0 returns, then WakeByAddressAll on that exact computed address. It makes no further manager dereference. The root job and all its child work remain intact. This differs from W6's Present flag; W1/W2 fix write-combined stores, not this wait.

The paired group validates both unique patterns, original bytes, IAT target, job branch, native register/stack ABI and writer ordering. Missing APIs or failed prechecks retain stock. MW-only installation requires complete thread enumeration/open/suspension/context capture and a bounded allocation-free kernel census rejecting new uncaptured threads. Replacement readback and any original-byte rollback occur before threads resume. An unrecoverable rollback/cache-flush failure is reported unresolved, never falsely as stock restored; helper pointers remain valid. Other patch groups retain their existing installation behavior.

Reader cave passes RSI (manager), preserving RSI/RBX through normal x64 ABI. Writer cave passes RAX (manager); next native instruction zeroes EAX, and RBX remains preserved. NOP padding precedes calls so return addresses are native instruction boundaries.

## Validation and qualification

- `tests/meshletwait_harness.cpp` tests actual Windows WaitOnAddress/WakeByAddressAll: completion before waiting; clear during enrollment; disabled/missing API fallback; timeout returns without clearing; wrong-address and spurious correct-address wake cannot satisfy the native outer loop;50 concurrent completion cycles.
- Same harness injects publication corruption, validates both-site rollback, original mismatch refusal and explicit unresolved cache-flush failure. Pure census classification rejects unknown/invalid thread IDs.
- `tests/test_meshletwait_binding.py` checks exact native reader/writer/prologue/branch/epilogue bytes and cave register bindings.
- Full pattern suite: ALL257 SITES OK,140 dependency ranges match.
- Final Release x64 candidate build PASS. ASI SHA256 `3933e76c10f9b7ea00448dfe7de03119ca4c17d739f65bf43b94e3d98dbd0462`,1755648 bytes. Existing C4456 shadowed-local warning retained; no compile errors. Build output alone does not validate gameplay. Logs: `mod/Nioh3EngineFix/build/mw-build.log`, `mw-native-tests.log`, `mw-binding-tests.log`, `mw-generated-check.log`.

During the first live trial, require `MW: ON`, actual positive wait rounds and wakes. Zero rounds means this branch was not exercised. Capture process CPU, frames/pacing and available GPU/memory metrics together. Compare equivalent gameplay with MW disabled/enabled; wait percentages from earlier sampling are not achievable CPU savings. Preserve all other settings and normal save progress.

MW cumulative wait-round, timeout, wake and error counters are emitted by the existing30s stats logger. No hot-path logs or new measurement thread. Startup-only design means separate launches for OFF/ON; no runtime menu toggle.

API semantics: [Microsoft WaitOnAddress documentation](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitonaddress) describes value comparison and spurious returns requiring a recheck; the native outer comparison supplies that recheck.
