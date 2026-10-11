# FO epoch witnesses - 2026-10-11

Experimental parent `3b89c04`; production source unchanged. **FO NO-GO.**
Run from mod/Nioh3EngineFix: `py -I tests/run_frameoverlap_98_epoch_audit.py`.
New audit only: **12 checks, zero failures, zero compiler warnings**.
The prior eleven controls are compiled for shared helpers but never executed.
Local evidence: `build/fo98epoch/audit.log`. No shipping ASI build or game test.

The fixture includes the actual frameoverlap.cpp through the existing audit.
EpochEmit models only the compare/write/stamp slice verified in static decomp
`D:/nioh3-re/decomp/0x2f06ab4.c`: obj+190 versus ctx+6EB8, twelve-byte 0x98
token, then object stamp. Both Fo98 template slots use this same fixture;
this DOES NOT independently verify 7C50B4's internals or its equivalence.
Thread joins make all ordinary epoch-field accesses sequential. This audit
does not test the unresolved concurrent epoch synchronization gap.

| Schedule | Observed witness |
|---|---|
| obj epoch6, ctx7, off-main Fo98 | obj stamped7, queue1, live write unchanged |
| repeat off-main + main call at7 | duplicate suppressed, queue1, no live token |
| ctx8 before first publication | obj stamped8, queue2 holding identical object pointers without epoch metadata |
| main Flush98 | two identical-pointer tokens copied once, queue0; no replay/GPU completion modeled |
| TAIL queues epoch9, actual FlushAll | TAIL done, barrier cleared, record still unpublished |
| modeled native reset effects | write/lastKick reset, generation40->41, queue1 retained |
| TAIL done>=issued, actual Restore | installed flag cleared despite queue1 |
| modeled restored emitter routing at10, then flush | new epoch token precedes retained epoch9 token |

Reset uses only the verified write/lastKick/generation effects from 418C70;
it does not execute native EndFrame or its dispatcher. Restore uses an initialized
installation critical section and empty patch/pointer tables, so no process code
is patched. The final routing witness explicitly calls the original fixture after
Restore; it does not claim the empty tables actually restored emitter slots.
The final identical tokens have known epoch order from the controlled schedule,
not an epoch recoverable from token bytes. No object retirement is simulated.

These are negative controls and behavioral witnesses, not a production repair,
deadlock proof, retirement/lifetime proof, output-equivalence proof or FPS evidence.
No installed ASI/INI/config/saves, default visuals, hardware or settings changed.

Next: trace 12B0D0/12AF40 and 7C50B4's lock/update path, then 0x98 consumer
and PagePool retirement ownership. Establish admission, resource lifetime and
epoch linearization before choosing a combined queue/space/closure repair.
Do not ship count-only or second-flush-only patches as safety fixes.

Committed/pushed experimental evidence: `24494d5`. Main checkpoint parent `82b0235`.
