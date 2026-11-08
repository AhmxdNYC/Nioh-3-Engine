# Combined first-use fixes (#81879)

Base is the equipped-core toggle build 1e4d2ce7 (bridge #6329, BR 7f244882), with native-smoke commits b457f79ad/7487a94df replayed on top. No older picker/menu implementation was substituted.

Evidence: pc2 RESULT #81875, log attachment #6342. Native smoke output was zero on every call. DOWN first spawned at t=207.109 after the press hide began at 207.069, but with follow disabled the spawn-only adoption path never ran; the form ended at 209.175 with no bound seal. LEFT first's orb was t=220.839, its old timeout 222.651, and its real capture 222.810: the accepted core arrived after the 1.8-second wait, causing an end and another begin.

Changes:
- Reject zero and -1 native effect handles, and faults. A new failed press clears the previous cover-success flag; the game's smoke is not moved below the floor by duplicate-cover suppression.
- Bind a fresh spawn-hit seal even in pin/follow-OFF mode. Existing born-after-press, freshness, first-capture priority and 300 ms readiness checks remain.
- Once an orb confirms a real use, wait through the existing accepted-use window (default 2.2 seconds after the latest orb/action, or configured SealOrbSpawnMs if longer). The separate 2.5-second watchdog respects that deadline. No-orb and interrupted-throw fast release are unchanged; there is no unbounded wait.

Validation: release build succeeds; 23 native-smoke/cover checks (including actual duplicate-smoke suppression branch), 18 extracted production binding/timeout branch checks using the failing pc2 timings and negative cases, 103 picker/menu checks, 36 equipment checks with 117 extracted mappings, and 357 rematch harness checks pass (537 total). sc22 replay also passes; two historical optional fixtures are unavailable. Offline tests prove branch behavior, not in-game timing/appearance.

Candidate BR SHA256: bb7995a8f7a2df9209b12dc079a82ea72b3574db773f88ef6a82107d914ddddf. No game was launched, installed, stopped, read, or controlled by cx-pc.

Next (pc owns live acceptance): fresh LEFT-first and DOWN-first, repeats, follow OFF and ON; verify handle=0 leaves game smoke visible and no 'moved away' log for that failed native call. Confirm exactly one begin/end per accepted use, binding before despawn, restored movement, equipped rows before use, and per-core OFF remains the game's summon. This candidate does not claim native first-press smoke now succeeds: it preserves game smoke when native creation fails.
