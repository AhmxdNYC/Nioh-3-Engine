# Native-arena practice: next implementation gate

2026-10-11, Codex. Read-only source/native-export review of private candidate
`61764b8`; this note is the only new file. No game, build, installation or
device action. References below are candidate `mod/Nioh3BossRematch/src`
unless qualified. Earlier stage/spawn contracts remain relevant; their line
numbers and statements about unfixed spawn restoration predate this candidate.

## First implementable task

Implement **a game-thread admission/recovery transaction for the existing
native replay route**, then use it for experimental R1CBC practice. This is
needed before another menu or observer feature makes training usable.
Selected entry remains mission `0x1CBC`, placement `65092`, start `67064`,
native box `66599`. Native mission reload is the reset mechanism; no arbitrary
spawn, in-place revive, reward suppression or different arena is established.

The `61764b8` spawn repair addresses pending restoration ownership. It does
not cover unknown session reads, an already accepted request after timeout,
or target-actor readiness. Those are separate admission conditions.

| Current evidence | Consequence for the next change |
| --- | --- |
| `rematch.cpp:156-186`: failed PlayMode/NetMgr reads leave online/guest false; no per-field validity | Do not admit unknown as offline. Carry validity plus raw values into an explicit session policy. An unfamiliar readable PlayMode must stay unsupported until its offline meaning is established. |
| `Gates:189-202`, exit at `1381`, `SpawnTick:1304`, `DoWarp:905`, `GateTick:1017` use those booleans | Apply one mutation eligibility check at every native-action boundary, including return. Restore ownership requires its separate narrow policy; do not discard recovery just because session is unknown. |
| `FnVerify:974-987` can call the duel approach warp without its own online/guest gate | A disarm must suppress this path as well as ordinary warp/gate/spawn paths. Restoring the point record alone does not disarm subsequent calls. |
| `ExecDirect:741-748` returns to refusal after native call exception or unreadable/mismatched post-call queue | The native request may already exist. Mark possible admission before calling, retain request identity, and quarantine new starts on ambiguous outcome. |
| `ExecDirect:757-761` warns on wrong start, then arms fight/warp/spawn at `762-794` | Exact mission/start/mode match must precede arming. Wrong start after native acceptance is recovery/observation, not ordinary pre-call refusal or an arena request. |
| `ToIdle:650-667`, abort `1396`, load timeout `1145` clear replay tracking | Idle is not cancellation. Preserve a separate unresolved native-attempt record after abort/timeout; prevent new native mission requests until resolution or process restart. |
| Landing `1107-1110` does not reset settle time on unreadable state/player or pause | Require consecutive valid, unpaused observations of the expected destination; do not count a gap as settled time. |

Transaction minimum: monotonically changing local attempt identifier; requested
mission/start/journey; pre-call and matched queue slot witness; whether the
native call was attempted; admission state `none / possible / matched`;
observed load edges; expected destination/return observations; recovery reason.
This local identifier is not a native cancellation token. Do not clear it
merely because the queue slot disappeared: the loader may have consumed it.
No proof reviewed here establishes a cancellation API or a finite timeout
after which a possibly admitted request cannot load. Keep that ambiguity
explicit instead of silently retrying.

Mutation disarm should latch on session-read loss, online transition,
unsupported session mode or identity disagreement during an active attempt.
No automatic return while connected. Continue bounded observation/recovery,
but neither a later readable zero nor the training observer's generation
automatically authorizes resuming a partially executed transaction.

Use extracted **actual production bodies** for focused failure injection:
each failed session read; unknown raw mode; online transition before every
warp/gate/return/write; queue copy failure after successful native enqueue;
exception after a simulated enqueue side effect; wrong-start match; timeout
then delayed load; abort then delayed load; pending spawn recovery; invalid
frame and paused frame within settling. Assert zero subsequent native actions
and preserved attempt/recovery ownership, not just a displayed refusal.

## Actor/session lifetime boundary

The existing training observer is insufficient to confirm the selected boss:
`training.cpp:58-60` ignores all Create arguments and records only event kind
plus the cached scalar snapshot. `bosslog.cpp:97-105` does retain the placement
entry scalar, but its 60-second ring printed by `HkBegin:66-81` is neither
session-scoped nor evidence of a completed/live actor. The hook named
`Character::Create` in `sites.json:38-44` resolves native RVA `0x1180740`, not
the VM wrapper, so ECX placement provenance from the spawn contract applies.

Independently re-read `D:/nioh3-re/decomp/0x135b78.c`: ActorFindById binary
searches 16-byte registry entries via manager->[0]+0x48 and returns the pointer
at entry+8. It acquires no reference and contains no synchronization or live
Character test. A later pointer dereference is not justified by this lookup.
The detailed wrapper[0]/construction/cleanup provenance in
`training-spawn-contract.md` remains necessary; no stable handle or disposal
contract was added by the current observer. Do not call the native lookup
from a background consumer and cache its result.

For the first native pilot, correlate exact placement-entry evidence with
recorded visible boss identity, one visible target, collision, AI and gate
behavior. Call that pilot evidence, not proof of general actor uniqueness.
Programmatic actor lifetime validation requires a separate guarded,
game-thread observation contract and native ownership proof.

`training.cpp:142-149` sessionGeneration is derived from separately sampled
scalars. As the existing generation handoff states, it does not establish a
native atomic session identity or event ownership. It can invalidate consumer
observations; it cannot grant authority to mutate an actor.

Read-only return verification also reconfirmed in
`D:/nioh3-re/decomp/0x22396D8.c:25-47`: the native reads saved return fields
and may fall back to current fields. Therefore a successful CallReturn is a
request, not proof of return to a chosen training lobby or completed cleanup.

After transaction tests pass, keep the existing native acceptance matrix:
one initial load/intro/target; three death/retry cycles; defeat/full reload;
abort/late-load/read-failure paths; title/Continue; restoration followed by
ordinary shrine replay. Training remains experimental native replay, with
native reward/progression behavior, until those separate claims are proved.

## Admission transaction specification for implementation after read-gates

Further read-only review at the same candidate found the same ambiguous-call
pattern in `ExecEmaki:833-849`: native exception or no observed slot restores
the map and returns without retaining an attempt. A quarantine created by the
direct/fight route must therefore gate **all new start routes** (`ExecDirect`,
`ExecEmaki`, `ExecOpen`), not just the Fight now menu item. The replay-menu
path itself is not an admission witness: `TrackReplay:1081-1087` uses changed
queue hash/loading/retry id, which cannot identify the user's selected request.
Keep it outside the first strict training route.

### Separate facts from phase

Keep one game-thread-owned `NativeAttempt`, independent of `g_state` and
`g_sp`. Suggested data, all copied scalars/bytes:

- Local sequence, kind, requested mission/start/journey, row and request time.
- Source mission/retry/journey with validity. Never retain their game pointers.
- Before-queue copy plus selected free index; after-queue copy and validity.
- Admission evidence `None`, `Possible`, `Matched`; call attempted/completed.
- Phase `None`, `AwaitLoad`, `Active`, `Returning`, `Quarantined`.
- Quarantine reason and original evidence; never erase the original reason
  when another observation fails. A bounded additional reason mask is enough.
- Observed valid load-up/load-down, destination settle start, return attempt
  status, and monotonically changing publication/result sequence.

The evidence enum answers what was seen; the phase controls operations.
`Matched` plus `Quarantined` is valid: abort/timeout can abandon a request
whose exact queue record was seen. It is not equivalent to `Possible`.
`Active` means the destination route was observed, **not** boss-ready,
actor-owned, progression-free, or safe for arbitrary native actor calls.

| Event | Required transition/action |
| --- | --- |
| Pre-call gate/clear/queue failure | Stay None; report refusal; no native action happened. |
| Immediately before native request call | Store attempt and `Possible/AwaitLoad`; this must occur before the stub/native can throw after an enqueue side effect. |
| Call returns and exact new selected record matches mission, explicit start and mode | `Matched/AwaitLoad`; only now arm Fight-specific effects. Retain copied queue evidence. |
| Call throws, after-copy fails, mission/mode/start differs, or outcome is otherwise unclassified | `Possible/Quarantined` (or preserve stronger evidence already established); disarm new effects; cleanup owned patches without clearing attempt. Never say "not queued". |
| Matched await-load sees valid expected mission/retry after a valid load edge and consecutive unpaused settling | `Matched/Active`; route observation only. An unknown/loading/paused/mismatched frame restarts settling. |
| Abort before any call | Clear only the still-local pending UI request; no quarantine. |
| Abort or timeout after call and before verified active entry | Preserve evidence, set Quarantined, disarm effects and restore owned patches. Native request is not cancelled. |
| Session-invalid/online/identity mismatch during an attempt | Quarantined and disarmed; keep observation, no automatic return/reissue. Ownership cleanup remains separate. |
| Valid explicit exit from normal Active | Permit one native return request and mark Returning before calling; exception is an ambiguous return admission, not retry permission. |
| Repeated exit while Returning | Report pending; issue no second return. Existing self-test's 120-second re-post must not bypass this. |
| New start while any non-None phase or recovery obligation exists | Refuse before any native request/menu call, regardless of UI Idle. |

For ordinary direct replay with requested start zero, native resolves the
default start itself. Do not apply `start == 0` equality to the queue and
misclassify every normal replay. The first training route uses explicit
nonzero `67064`; generic direct replay needs its existing native/default-start
contract represented separately. Copy requested and observed start distinctly.

### What may clear quarantine

**For the first implementation, only a new game-process lifetime.** Do not add
an "acknowledge and forget" command. A new UI/plugin initialization in the same
process is not sufficient; keep the latch for the add-on's process lifetime.
Do not have this feature kill or restart the game automatically.

The inspected ById/Enqueue exports contain one enqueue path which selects a
free slot and calls copy/notification (`0x2239600.c`, `0x223953C.c`). They do
not establish consumer completion/cancellation, nor close exceptional or
concurrent request paths. The following observations **do not clear** an
uncertain attempt: queue hash unchanged; all slots empty; matched slot no
longer visible; current mission unchanged; elapsed 120 seconds; UI Idle;
title/Continue; successful point restoration; or an expected mission visible
after an observation gap. Loading that mission can be useful recovery evidence
without proving that no request remains queued elsewhere.

This restriction is specifically for **abnormal/uncertain quarantine**, not
the ordinary Active replay state. Keep the normal exact-matched replay on its
normal observed exit/restoration path; do not label every active replay as
uncertain. Existing normal rematch exit remains an operational tracker, not
new proof of native queue consumption or training session completion.

Before training automatically starts the next repetition after a normal
return, the stricter destination/return contract must be implemented and
validated. A mission change alone (current `TrackReplay`) is not enough.
Capture and validate the intended return destination using the native saved
return fields/remap semantics, observe the return load and consecutive valid
destination settling, and verify restoration. A native-consumer witness is
needed to support automatic recovery from **uncertain** admission without
restart. Those are follow-on evidence tasks; do not silently treat normal
tracking as that proof.

### Actual-source fault tests

Extend an actual-source extraction harness (the existing spawn-restore
harness shows the compilation approach) with the production attempt struct,
admission helpers, `ExecDirect`, `ToIdle`, request dispatch and relevant tracker
logic. Stub only external calls/reads/UI and native memory; do not replace
the transaction under test with a duplicate model. Compile `/W4 /WX`.

Use a three-slot byte queue and scripted `CallRequestById` outcomes. The stub
can write a native-looking request **then** return exception to reproduce
the dangerous side effect. Track independent counters for ById, Emaki,
menu-open, return, warp, gate, point writes and restore requests. Sequence
snapshots rather than merely checking a predicate in isolation.

Required cases and assertions:

1. Pre-call failure: no attempt, no native call. Normal explicit-start match:
   one call, Matched/AwaitLoad, exact witness, fight effects armed once.
2. Enqueue then exception; successful call then failed queue read; wrong
   start; wrong mission/mode; unchanged queue: Quarantined, no fight/spawn
   arming, preserved requested identity and call evidence.
3. Abort/timeout after match, then delayed queue consumption/load: original
   evidence remains, no new start or automatic return; observing the late
   destination does not silently clear quarantine.
4. Call each start entry while quarantined, including menu and scroll paths:
   every native-call count remains zero. UI state forced Idle must not bypass.
5. Call ToIdle and successful SpawnRestore during quarantine: attempt stays
   latched. Failed SpawnRestore preserves both obligations independently.
6. Empty queue, equal mission, huge elapsed time, title then Continue,
   observation gap then expected mission: none clears quarantine.
7. Valid matched landing with a bad/unpaused-gap frame: settle timer resets;
   no false load-down from failed GameMain read and no early Active state.
8. Normal Active exit: one return only, second/repeated exit refused as
   pending; return exception preserves ambiguous return outcome. Never mark
   completion solely from native return call success.
9. Existing matched normal route can still reach its operational exit path;
   test does not falsely claim training completion. Requested default start
   zero follows its separate rule and does not acquire a false start mismatch.
10. Only test-fixture fresh-process initialization clears uncertain latch.
    Ordinary Prepare/Init-like callbacks and abort/result acknowledgement do
    not; do not add a production reset seam just to make this test convenient.

Retain existing read-gates and spawn recovery suites. Passing these tests
proves wrapper behavior under injected failures, not native loader completion
or the live R1CBC acceptance matrix.
