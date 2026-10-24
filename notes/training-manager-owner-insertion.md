# Actual mission-manager owner insertion and scheduling boundary

2026-10-12; static continuation after91ee70d. No native mutation/game/bridge.

## Closed manager-to-typed-owner link

702CC0 loads the same global manager wrapper45B9E40 that MissionThreadUpdate
loads atE64745 before379620. If its predicate permits and the requested id is
nonzero, cold1A01226 passes that wrapper through1C07FAC;1C07FB8 dereferences
it and calls1C07F0C with the manager object.

1C07F0C checks for an existing entry at3741AC, allocates throughE307B8 and
initializes the new owner at1C2608C. It then explicitly takes **manager+10**
at1C07F82, loads that list's sentinel, and callsE2AAA0 at1C07F8E with the
unique owner pointer on its stack. E2AAA0 ->1BE1998 ->1BE1BF0 installs the
previously traced typed owner/control pair at node+10/node+18. The constructor
sets strong and weak counts to one, binds vtable3A892F0 and clears the incoming
unique pointer. E2AAA0 publishes the node's forward/back links and increments
the list count; the caller then disposes the now-empty unique pointer.

Thus the manager+10 list traversed by379620 can contain the exact typed owner
whose final-reference disposal reaches1BF9B10. This closes the generic-pair
ambiguity from the prior checkpoint. It does not establish a retained lease
for VM+90, producer exclusion, or actual TrainingGround entry acceptance.

The initialization1C2608C destroys/reset portions including runner+8 and script
asset before configuration. The only discovered direct call is the fresh-owner
path above; no in-place live-owner reset or race is claimed from this discovery.

## Gate signal is not scene-task completion

Retained vtable slot4035810 points to2E33A4, named SceneViewBuildTask_Run in
the retained decompile. It calls KtglEventSet for4709AB0 at2E33BB **before**
tail-calling the actual task body2E33D8 at2E33D0. Together with the earlier
manual-reset and unlock-before-mission-update proofs, this rules out treating
this signal as proof that the scene task body has completed. It remains
possible that another dependency excludes relevant mutation; not proven here.

## Bounded caller census and remaining scope

A raw relative-call/jump search found1BF9B2B as the only direct1C10570 caller;
that is the already-known destructor. No absolute pointer or simple RIP-relative
LEA reference to1C10570 was found. This is discovery evidence, **not an exhaustive
indirect/alias-call proof**, and the audit does not report all cleanup paths closed.

Known creation-list writers remain distinct:683F74 gets global4745348 and
enqueues into its+BC0 list via9DCE70; wrapper1A81B28 independently accepts an
object and enqueues at object+BC0. Wrapper9DCF54 calls list insert9DCF6C
directly. The latter wrappers lacked discovered direct callers, which neither
proves unreachability nor admits global exclusion.12 retained-decompile callers
of683F74 also need contextual scheduling closure. A mission-thread label or
strong owner reference alone is insufficient to serialize that creation list.

## Executed audit

`py tools/rematch/audit_training_manager_insertion.py --dump
'C:/Users/Ahmad Hamza/nioh3-engine/re/Nioh3.dump.exe'` passed11 exact windows,
13 direct edges, same-global manager identities, actual+10 list offset,
scene-task vtable identity, and6 in-memory mutation rejections.

Remaining gates explicitly stay false: callback owner lease, exhaustive cleanup
paths, all-producer exclusion and native mutation admission. No factory or VM
frame was invented. Next work is contextual writer/indirect-task ordering;
the existing native Summon pilot can be tested independently of this unfinished
arbitrary destination-owner construction adapter.
