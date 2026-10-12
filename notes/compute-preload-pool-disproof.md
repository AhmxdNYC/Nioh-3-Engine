# Preload pool identity: default-pool pause is not a preload join

2026-10-11. Static continuation of `compute-preload-reset-owner.md`. No hook,
observer, game, build, new scheduler policy or measured performance claim.

## Concrete identity result

The native boot preload request uses vtable**3AAD6C8**: slot+8 is1101B90
(actual load), slot+10 is11030B0 (polls request+98/state), and slot+28 is
**11030A0, `return true`**. The distinction matters: ResourceStepJob1343D00
invokes+10 and therefore only steps this request; it does not execute boot
PSO compilation. The earlier generic TrySync reference was not a binding of
the preload body to that pool.

The true+28 selects **manager-B1FF320's asynchronous load branch**:

1. Read GameMain+6B4 at1FF942. Existing pool catalog identifies it as
   RequestLoad (normally7); the exact native fact is the dynamic selector.
2. Allocate job58 bytes; its vtable is**3B84658**, whose run slot is
   **1343CB0**. Its request pointer is+38.
3. Allocate owned call context; at1FFABA write the job itself into context[0],
   and at1FFAD0 attach the context to job+48. This is the supplied group.
4. Selector zero uses scheduler+E8 at1FFAEB. A nonzero selector instead walks
   scheduler+3A0 at1FFAF4, matching pool+34 at1FFB0F. Dispatch31A63C occurs
   at1FFB2B.
5. Run1343CB0 checks request terminal/busy virtuals+50/+58, then invokes
   request+8 at1343CE0 with job+48 as argument. For this vtable the target is
   **1101B90**, which passes context[0] into PsoListLoad's group argument.
6. PsoListLoad uses that group's owner worker and pool worker count, pushes
   compute children to the same owner and joins before returning.

Thus the normal RequestLoad selector reaches a separate named pool, whereas
reset3242CB8 acknowledges scheduler+E8 only. **Equating those two owners is
disproven by actual dispatch selection.** A special runtime selector of zero
would choose+E8; it must not be assumed for the normal configured game.

One can have default-pool acknowledgment with RequestLoad jobs outstanding
without violating either pool's local counter rules. This is a counterexample
to the proposed *proof*, not evidence that native reset actually races preload.
Native safety may rely on the preceding resource-manager drain and its
producer ownership. That separate exclusion still needs proof.

## What still supplies protection

Manager-B's busy flag+248 remains set while it processes its active request
vector; it removes completed requests after their terminal predicates, while
the boot request's done byte is only set after the joined PsoListLoad call.
This supports waiting for an already admitted boot request to finish. The
manager's next empty-pending pass clears busy; it is not a permanent gate on
new admission. DisplayReset waits those busy bytes **before** pausing the
default scheduler. The remaining necessary fact is which producer can append
a new load request in that interval or from another pool while reset is held.
Do not replace that question with another busy-flag sample or treat the two
waits as one atomic barrier.

## Independent destruction boundary

The ordinary graphics destructor2EE0D58 ->2EE0694 ->2EEC4C8 is not the only
destruction route. **2EE0D90** calls**2EE09FC**, which calls2EFB020 on outer+10;
that reaches2F00BC4 directly. 2EE0D90 can then free a3D90-byte allocation
through1A98E60 when its flags request deletion. This smaller base-context
destructor has no display-reset pause or preload join in the bounded body.
The previous common primitive retirement requirements cover it; an adapter
installed only around RecreateSwapchainDevice would miss it.

These are virtual destructor entries with no retained direct callers. Their
upstream owner release/worker-shutdown ordering is still unclosed; no claim
that the game performs destruction concurrently is justified. Cache admission
must not continue past any common teardown entry, even if root/count survive.

## Decision

No serving-cache hook is admitted. The tempting shortcut “the reset pause
already joins preload because both use the same worker pool” is definitively
rejected. The next useful evidence is the **manager-B pending-queue producer
serialization** around reset, plus the owner release that reaches2EE0D90;
further shadow-cache arithmetic tests cannot establish those facts.

Expanded `audit_compute_preload_reset.py` verifies19 pinned code/data windows,
rejects19 changed-byte fixtures and checks25 native instructions, including
the dynamic pool selector, supplied-group assignment and actual load virtual.
Pinned vtable contents remain in the private dump; no game bytes are committed.
