# RD: paired renderer drain wait candidate ? 2026-10-13

Isolated from hosted `592a888` in `D:/nioh3-wt/drain-wait-20261013`. Default OFF: `[Patches] RD=0`; startup-only. Existing menus, addon and MW are retained. No installed files, game, bridge, settings or saves touched. Candidate Engine version string remains 1.7.73; distinguish by commit/ASI hash, not version alone.

## Why this candidate

Prior research read: `AGENT_BRIEF.md`, `notes/cpu-stacks-current-2026-10-13.md`, `notes/renderer/backend.md`, `notes/handoff-codex-fo98-dispatcher-completion.md`, `notes/handoff-codex-fo98-fence-identity.md`, existing S1 bytes, and hosted MW paired installer/header. Current repeated self-RIP samples in 418F60..77 establish active polling. The roughly 7% main-thread snapshot share is NOT measured CPU execution cost or a promised gain. Existing S1 adds PAUSE only. RD does not skip the drain or overlap frames.

## Verified native contract

- 418F90 returns busy iff `ring.write(+24) != ring.read(+28) || ring.inFlight(+30) != 0`. 418F40 repeatedly calls it; timing/global busy publication and outer loop remain native.
- Producer 25426C: reserve write with LOCK XADD at 2542B9; copy complete 32-byte item at 2542D7/DB; LOCK INC count at 2542E0; release semaphore at 2542EF. Inline 254110 does the same (reservation2541B3, increment2541E1, release2541F6).
- Consumer1A7C3E0 waits on the semaphore, copies item, advances read at1A7C433, replays tokens and optional submit. Only afterward does1A7C4AF LOCK DEC count, then1A7C4B3 jumps to semaphore wait. Read==write alone can be true while replay is still executing.
- Constructor2EE0172/17D clears the entire 0x48-byte ring starting at device+3BE0, including count. Reset2EEC4C8 calls2EEDDEC: full busy predicate must drain before context cleanup. Then2EEDEF8?2EEDE4C releases a sentinel semaphore credit, joins dispatcher, closes handles and clears ring fields. Empty-after-semaphore at1A7C408 exits consumer. Normal reset therefore cannot clear the ring while the added completion wake is still running. Reset retains count0 from drain; no extra reset hook needed.

These are native ordering facts, not proof of GPU completion. RD preserves exactly the native CPU drain condition and introduces no new GPU completion claim.

## Implementation and race closure

RDR changes only CALL418F63 (5 bytes) to RDWait. RDWait polls the full predicate up to64 times with PAUSE. If still busy, it snapshots positive in-flight count, rechecks the predicate, and calls WaitOnAddress on that exact aligned 4-byte count with a1ms bound. It always returns the full predicate afterward. A zero count with reserved write!=read stays polling: publication has not yet produced a completion wake. Timeout/spurious wake never reports success. Non-timeout API failure latches ordinary predicate polling for the process.

RDW changes LOCK DEC+jump at1A7C4AF (9 bytes) to the paired completion helper, then resumes1A7C3F5. It performs the original atomic decrement exactly once, then unconditionally wakes that count address. No waiter-registration flag can lose a wake. Completion before enrollment is covered by WaitOnAddress expected-value comparison; ABA/new work is covered by the bounded wait and full predicate recheck. Another producer can publish after idle just as in stock; RD does not claim to close unrelated producers or create a frame-overlap boundary.

No cached ring pointer or per-ring generation survives a call. Multiple ring addresses remain distinct. Startup uses the same frozen thread census, exact-byte pair verification, write verification and rollback as MW. Missing APIs skip both patches; uncertain publication retains both helper states. S1 does not overlap either site; its original outer-loop behavior remains valid.

## ABI and unwind

RDWait is called with RCX=ring; native continuation consumes AL, updates global busy, and uses preserved RBX for its next iteration. It does not require volatile registers that the original short leaf happened to retain.

RDWake is JMP-entered with native RSP aligned, native RBP=entryRSP-8 and a0x50-byte allocation. All old token/item locals and volatile registers/flags are dead at this edge. It reuses the existing shadow area (no sub/add wrapper), calls completion, then loads/resumes the original loop head, which sets RCX/EDX before its next wait. Its unwind description includes the inherited RBP push,0x50 allocation and RBX/RDI saves at currentRSP+60/+68; it does not invent a wrapper return address. Register tail jump avoids misidentifying a bare memory jump as a standalone epilogue.

`RtlVirtualUnwind` on the extracted exact production cave passes at entry/call/post-call/tail-jump PCs0/3/8/15, restoring original caller RIP/RSP/RBP/RBX/RDI. This validates those PCs on Windows; it is not a Proton runtime test.

## Verification and next actual test

- Native production-header tests PASS: reservation gap, popped-but-running work, timeout, spurious wake, completion during enrollment, new publication after old completion, error fallback, distinct rings,100 completion/drain/reset cycles and failed paired publication rollback.
- Native inherited-frame unwind test PASS, as above. Reproduce from an x64 MSVC prompt with `py -I mod/Nioh3EngineFix/tests/test_drainwait_native.py`.
- Exact patterns/header checks: ALL259 SITES OK, including new9-byte JMP +4-byte NOP padding and dependencies. No native instruction windows are edited outside the two sites.
- Release build completes using existing toolchain; existing unrelated nt-shadow warning remains. No native gameplay test performed here.

After independent source review: one same-binary RD OFF/ON/OFF comparison in a matched scene with MW/current menu settings fixed. Capture process CPU + frame timing + available GPU/RAM telemetry, verify RD installed and counters/errors, exercise loading/menu/normal close. Keep progress/saves per device workflow. A gain, equal pacing and stable transitions remain unproven. No10?20% reduction claim follows from this implementation.
