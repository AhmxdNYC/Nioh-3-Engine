# Yamagata: native idle and single-action recovery contract

2026-10-11. First-boss scope only: current Practice R1CBC, mission1CBC,
start67064, placement65092. Static retained-dump research; no ASI change, game,
AI command, action dispatch, memory freeze or invulnerability. Companion move
enumeration/dispatch is owned separately in training-yamagata-move-selection.md.

## Concrete native suppression path

The script binding AlgoActiveFlagAllCoop238B604 constructs a native boolean
command through A3F328 and dispatches it through wrapper66BBF4. The command
object vtable3910D38 has execute slot+10=1C0A274. Its byte+8 is the requested
boolean; execute receives the live Character in RDX:

* True tail-calls66982C(live). This finds live+3B0 component, then component+50
  algorithm, sets algorithm+1574 to1 and calls27B040.
* False calls668D74(live), then1FF1FC0(component).668D74 sets algorithm+1574,
  +1AA8 and+1AB8 to0, sets byte+1AC0 to1, calls27B040 and668DC0(component).
  The subsequent1FF1FC0 reaches5A035C through algorithm+10, also used by the
  AlgoAwareFlagOff binding. This is more than a reversible active-bit write.

The all-coop script loop itself is not an appropriate Yamagata selector. The
shared command/live-character mechanics are the reusable seam to investigate;
this does not prove that blindly calling it on Yamagata is admitted.66BBF4
executes immediately through command virtual+10 when the wrapper has a live
pointer, otherwise retains the command in its pending list. An absent live
actor is therefore not a harmless no-op for this dispatcher. Do not leave
training commands waiting on a wrapper that can later acquire another actor.

Native GetAlgoActiveFlag486BE0 returns whether this same+1574 equals1.
It describes algorithm activation, **not idle motion or completed attack**.

## What activation actually gates

86CC50(component,dt) checks algorithm+1574==1 at86CC65 before calling the
large algorithm tick27CC04 at86CC8A. Disabling therefore suppresses a real
decision/update path. Another native action-eligibility path289290 branches
away at28940D when the same flag is not1; manually selected actions may need
a path that does not depend on that eligibility test.

The broader component update359430 still runs. At359792 it tests the active
flag, and when inactive **and component+28 is null**, calls the default
controller at component+18 virtual+18(dt), at3597AC. This is concrete evidence
against interpreting activation-off as freezing the entire Character. It is
not yet proof that every Yamagata animation, root motion, damage reaction and
recovery continues correctly, particularly when an override controller exists.

The dispatch researcher independently traced SetActionID to1C0A41C, which
loads live+3B0/component+8 and calls66C4A8(state,actionID,0,0.0) without an
activation test in that thunk. That makes AI-off plus manual-action dispatch
a plausible bounded candidate, **not established execution or completion**.
The downstream state machine and actual actor type must still be matched.
Follow-up from that researcher rejects treating66C4A8 as a benign queue:
it invokes state virtual+100, clears+118/+260/+290/+258, sets+340/+2C0/+33C,
then invokes virtual+F8 and+170. This forced transition can disrupt recovery;
its unresolved virtual targets are a concrete no-go for a training button.
The separate RequestActionTrans pending field+2E4 needs its native consumer
traced instead; a field write alone still does not prove acceptance.

## A concrete false-positive completion predicate

IsInitScenarioAction2391364 resolves the actor and calls1FFF3EC(component,
scenarioAction). It maps the requested value through2367454(state,request):
lookup49B054(state+178), then word at record+2, or **FFFF on lookup miss**.
It compares that with358528(state): dword at current-action state+E8, or
**FFFF when that pointer is null**. Thus missing mapping plus no current action
compares equal. This predicate cannot certify entry, idle or attack completion
without separately proving both non-sentinel identities.

358528 is a concrete current-action identity getter. An admissible single-move
oracle needs actual selected-action entry and its exit into an independently
identified Yamagata idle/recovery state in the same actor epoch. A timeout,
request return, action change alone, GetAlgoActiveFlag=false, or FFFF equality
does not provide that evidence. IsNowStateExt2391764 calls486C70/486C98's broad
state-enum switch; no specific idle enum was established in this unit.

## Interruption and recovery requirements

This is a prospective contract, not executable safety proof:

1. Admit only one live Yamagata identity in the accepted solo mission, alive,
   control-ready, outside intro/loading/return. Bind wrapper,live,component,
   algorithm and action-state identities to that mission/actor epoch.
2. Acquire one AI-control obligation using a proved native execution phase.
   Existing inactive state is owned by someone else unless provenance proves
   otherwise; do not unconditionally enable it on exit. Use the real native
   command/API if admitted, not repeated writes to+1574 or motion memory.
3. Request a validated action once while the actor is naturally idle. Wait for
   that action's actual start; let its native animation/recovery continue.
   Never request an idle action every frame or replay a pending action to hide
   failed acceptance. The selected action may be a native combo/graph sequence;
   one request does not imply one hit.
4. Damage, stagger, grab, phase change, player/boss death or actor replacement
   interrupts the training transaction. Do not overwrite the native reaction,
   force health, suppress invulnerability rules, or force idle mid-attack.
   Whether native AI-off preserves reactions remains a test/proof dependency.
5. Return to training Idle only after verified selected-action exit and a known
   non-sentinel idle/recovery state. On cancel, stop future requests; wait for
   a proved natural safe boundary before releasing AI control. Restoring1 alone
   cannot restore the fields cleared by668D74, so enable/disable symmetry and
   native reinitialization must be established. Never restore saved actor fields
   through a stale pointer or into a newly loaded mission.

Next concrete closure: follow668DC0/27B040 reset side effects and66C4A8 action
entry/completion while+1574 is0, then bind Yamagata's actual idle/recovery IDs
from its loaded action set. No arbitrary all-boss picker or live backend is
admitted by this note. The user-facing target remains a selected move from an
idle Yamagata, followed by natural recovery back to idle.

## Retained native byte witnesses

These bounded windows were decoded afresh from flat re/Nioh3.dump.exe;
they are not a universal writer/caller inventory.

| RVA/bytes | SHA256 |
|---|---|
| A3F328/61 | b9b84b848343375cb5da9c6ba66218e2a7b5a9a38d03d97c1a100ed1c7fab83a |
| 1C0A274/32 | c6c94291e69a8fbea7a23af9bbb5894de88a611d7c86963913842d1aa2e1215d |
| 668D74/49 | e12225ddb85ee5f4de7c867b85e9e3287a45b1b51734bf586305937600e9322f |
| 66982C/2D | 214ef6bf9483a077c5e667fdcb5c89fd832f8a11451b7dd928b74bd206ea5326 |
| 86CC50/4A | 78f4971a747ff59bd608069c5bec7c75fcc97aa89b1109c179246c367f35f7f1 |
| 35978E/21 | 6b5b72aea30a752b5bfe55257d66d93a03769755ba8de9a2382aa6c6dfabc5c9 |
| 1FFF3EC/27 | 205e200ebb75ebb053484744c7eb071192cc6c9024c52b7e26d57be34e05088f |
| 2367454/25 | d47cde2b74c79a897d5861dedd73734c60fd162646ab3bec1ef0866549b70668 |
| 358528/15 | 6b3a48a53fac933a27c8c138a83ab7b28961f7dad3bc041cb14c82e1ea546d70 |
