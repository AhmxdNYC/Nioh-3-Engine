# Yoritomo hit-count correction, task 40838

Offline analysis, 2026-10-27. No game launch or installed-file change.

## Corrected decision rule

A timed collision window is a static estimate, not proof of one player contact.
An observation tied to exact archive bytes and action takes priority. Human
#40826 reports two hits for the first and third one-hit moves; pc #40838 maps
these to chr16102 actions 0x4E4E and 0x4E55. Both now return 2, while
`static_hits=1` and `hit_source=player observation #40826` preserve the distinction.
Overrides require SHA256 89836dd44b6b741ef3e8ba3c54d7d55961c32324ede7e3bacce778b27dfba568.
This is a scoped correction, NOT a newly decoded general rehit algorithm.

## Static comparisons

| Action | Hitbox row indexes | Windows | +0x0E | +0x10 physical | +0x12 element | +0x14 |
|---|---|---|---|---|---|---|
| 0x4E4E | 2 | 52-59 | 0 | 40 | 0 | 12 |
| 0x4E55 | 35 | 72-80 | 0 | 50 | 0 | 12 |
| 0x4E52 | 9,10 | 54-59,73-81 | 0,0 | 30,42 | 0,0 | 12,12 |

All have group0. The candidate interval fields do not distinguish the targets
from known two-window moves. Neither target has a 20000+ spawned-object event.
Paired actions 0x4E4F/0x4E56 share motions1000/1010 but have no own hitbox rows;
counting same-motion siblings would invent contacts. No such propagation is used.

## What the existing logs can and cannot prove

Snapshot: D:/uo-analysis-40838/training-snapshot.log (read-only copy).
4E4E repeat begins 5:59:33 pm Eastern;4E55 begins6:00:27 pm Eastern.
`KILOG hit Boss A` records Ki damage RECEIVED BY the boss, including one-hit Ki
break and zero-damage callbacks. Those lines are not boss-to-player contacts.
At5:59:40.253 pm the preceding PRACTICE line explicitly says player hit on a
Training boss. Therefore counting those calls per boss swing is invalid.
Player damage logging in training.c NfHpDamage is capped by process-global
`InterlockedIncrement(&ncut) <= 20`. The available player-hit lines include
842 damage at5:59:37.748 pm and1006 at6:00:30.519 pm; they are incomplete samples,
not evidence of exactly one hit. HP-refill messages are not contact counters either.
Exact actual contacts per swing cannot be recovered from this log.

Next engine-rule investigation needs uncapped bounded capture of incoming-player
hit callbacks with attacker identity, action, swing instance, collision group and
motion frame, then comparison against accepted damage/contact rather than all
Ki callbacks. This task does not change or launch the running game.

## Generated result

Only2 of1884 generated records change. Yoritomo attack records: one-hit13 ->11,
two-hit18 ->20, three-hit2 unchanged, unknown55 unchanged. Other bosses and
unblockable flags unchanged. These are facts-table counts; the runtime catalogue
may filter entries. 0x4E52 remains2; paired zero-row moves stay unknown.
Set NIOH3_DATA_ROOT to the checkout containing data/extract if generating elsewhere.
