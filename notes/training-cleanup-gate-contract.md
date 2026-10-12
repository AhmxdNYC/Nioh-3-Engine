# Training cleanup ownership and mission gate semantics

2026-10-12; continuation of 198d026 in the same isolated worktree. Retained
dump inspection only; no native call, game action or source integration.

## Typed disposal path closed

The owner-pair release and the known owner destructor now have an exact typed
bridge. List-node helper 1BE5B6C passes node+10 to F25CC; F25CC reads pair+8
(the control block) and calls F262C. F262C atomically decrements strong count
at control+8 and calls vtable slot0 only when the previous count was one.

Constructor 1BE1BF0 allocates 18h bytes, initializes strong+8 and weak+Ch to
one, installs vtable3A892F0, writes the owner at control+10, and publishes
{owner,control} to the output pair. It clears the incoming unique pointer.
Vtable3A892F0 slot0 points to 1C01DC4: load control+10 then E312A4, which gets
the allocator and jumps to 1BEDD1C. The latter calls owner destructor1BF9B10
before allocator virtual+58. This supplies the missing typed virtual-disposal
bridge; it is not evidence that every manager node uses this control block,
that VM+90 retains it, or that holding a strong reference would prevent an
explicit cleanup call before destruction.

The inspected E22D10 cleanup continuation increments control+Ch, not +8,
while manipulating another manager collection. Those observed increments
are weak-count activity and cannot be cited as a strong owner lease.

## Mission gate is a manual-reset scheduling latch

Static initializer27F20 supplies edx=1/r8b=1 to 5B9E88 with RCX=4709AB0.
The constructor stores those flags at event+98 (manual reset) and event+99
(initial readiness). MissionThreadRunFrame's wait consumes readiness by
copying +98 to +99; for this initialized object that leaves it signaled.
Therefore multiple waiters may pass until an explicit reset. This rejects
the hypothesis that the mission gate is an automatically consumed,
single-owner entry token.

Concrete exact-object writer paths:

- 39E104 calls 2F6CE8 at39E15B with 4709AB0. The helper locks through48B44,
  writes event+99=0 at2F6CF9, and unlocks. The caller updates frame-related
  globals before this reset; the complete scheduling dependency is unproved.
- 2E33A4 calls 2F6680 at2E33BB with the same event. The helper locks, writes
  event+99=1 at2F6696, notifies the condition variable and unlocks.
- Additional static wrapper1A76B74 loads this event and jumps to the same
  set helper; wrapper1C2ACF8 waits on it. Their invocation ownership remains
  to resolve. Discovery was a bounded RIP-reference scan, not an exhaustive
  alias/indirect-call proof or a global writer census.

These semantics plus the already proved E63311 unlock-before-update boundary
provide no owner/producer exclusion. This is a counterexample to using the
event alone as that proof, not a reproduced in-game race.

## Verification and remaining next step

`py tools/rematch/audit_training_cleanup_gate.py --dump
'C:/Users/Ahmad Hamza/nioh3-engine/re/Nioh3.dump.exe'` passes 13 exact windows,
12 direct edges, exact vtable/three event identities and five in-memory
mutation rejections. Full owner constructor is pinned; gate reset/set flags
and typed disposal are checked. Admission, owner lease and all-producer
exclusion remain false. The source vtable value is from this retained dump,
never a live pointer.

Next trace the pair's actual insertion into destination manager+10 and all
explicit1C10570 callers separately from final-reference destruction. Resolve
39E104 reset / 2E33A4 set scheduling and the indirect1A76B74 producer wrapper
against the 9DCE70/9DCF6C creation producers. Until both owner epoch and
reentrancy exclusion close, do not cache VM+90, manufacture factory context,
or add a mutation hook on the basis of shared-thread or gate names.
