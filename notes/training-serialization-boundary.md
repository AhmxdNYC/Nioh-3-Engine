# Training destination serialization: enclosing mission boundary

2026-10-12. Static retained-dump continuation of a248218. No game launch,
native calls, addon changes, installation, controller or bridge action.
Combined Summon c6d9abe remains unrun; independent arena transfer remains
unimplemented and inadmissible.

## New exact evidence

The visible MissionThreadRunFrame mutexes do **not** remain held across the
destination callback. At E63241 and E632C4, `_Mtx_unlock` releases the earlier
object+738 and object+788 mutexes. The final gate at E632D0 locks the mutex
addressed at 4709AB0, optionally waits at E632EF, copies its readiness byte,
then **unlocks at E63311** before the tail jump E6332D -> E64700.
E6474C subsequently calls MissionManagerUpdate 379620. These instructions
rule out citing those mutex scopes as a retained owner lease. They do not
rule out an external task-graph/event barrier; that contract remains to trace.

Within MissionManagerUpdate, E6474C's callee iterates the manager's list at
manager+10. 379803 calls 37A434 with node+10's owner. The loop backedge at
37980B precedes the post-loop work: task-list update 3798A6 -> 89A328,
compaction 3798D9 -> E33840, and erasure 3798E8 -> 8F4A20. Therefore that
specific compaction/erase pass follows the synchronous script slices. Calls
inside those slices and other update callees are not excluded by this result.

Compaction invokes predicate E22D10 at E33870/E338A7. That predicate has a
branch that calls the existing child insertion 94B04 at E22DB2 and records
owner+142 = 1. A cleanup pass is therefore itself a script-child producer;
enumerating only Character::Create or the ordinary Script::Create wrapper
would miss this insertion site. The branch conditions include owner+134,
runner presence and owner+142; its full downstream disposal contract is not
closed here.

Compaction also releases replaced node+18 control blocks at E338D1 -> F262C.
The latter uses atomic strong-count decrement then a virtual disposal call
on the last reference. 8F4A20 has a cold branch at 19227FC; it unlinks a range
and calls 1BE5B6C at 192280C. That node helper calls F25CC on node+10 before
freeing the list node. This is concrete ownership machinery to trace next,
not evidence that the borrowed VM+90 owner acquired a reference.

## Reproduction

From this worktree:

```powershell
py tools/rematch/audit_training_serialization_boundary.py --dump 'C:/Users/Ahmad Hamza/nioh3-engine/re/Nioh3.dump.exe'
py tools/rematch/audit_destination_owner_chain.py --dump 'C:/Users/Ahmad Hamza/nioh3-engine/re/Nioh3.dump.exe'
```

Observed: new audit PASS, 9 exact byte windows / 17 direct edges / 4 in-memory
mutation rejections. Prior owner audit also PASS, 8 windows / 10 calls /
3 mutation rejections. Every admission/retention/producer-exclusion result
remains false. Capstone inspection used the retained flat RVA dump; no new
dependencies installed. PE exception ranges split some functions into hot/
cold and prologue fragments, so window boundaries are explicit rather than
treating the first exception range as the whole function.

## Next executable work

Trace E22D10's continuation and F25CC -> control-block disposal to the actual
owner destructor, and enumerate the event readiness writers plus the
9DCE70/9DCF6C producer callsites against the E63311 release boundary. A mutex
being present elsewhere or callbacks sharing a thread is insufficient.
Keep callback reentrancy and external owner teardown as independent gates.
Do not cache VM+90 or fabricate a VM/factory frame.

This gate does not block testing the already implemented native Yamagata
Summon path through its reviewed current-entry lifecycle. The lead must
verify the latest adapted pilot pins and run its offline tests first;
the old dbd6752 entry-hold runner is explicitly unsuitable for the c6d9abe
paused-menu intent. See c519aa5's combined-training-live-checklist.md and
the newer 1d7267c pilot integration. Native acceptance still requires one
owned Summon, exact actor65092/battle1CBC, visible control, and exact original
mission/stage return. None of those runtime witnesses was produced here.
