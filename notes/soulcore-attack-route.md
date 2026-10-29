# Soul-core attack routing: one call for the next experiment

Task #52171, cx-ally, 2026-10-29. Offline only: no game launch, install,
process access, or benchmark. PC2's reported live baseline is BossRematch
`a1434149` on `pc/tenshin-body`; Ally installed builds remain EngineFix
`88fd85a0`, BossRematch `acf972c2`, Training `956f7b09`.

## Call to add

On the game thread, once the intended form's action/motion resources have been
installed, issue **one** action request on the rising edge of the attack button:

```cpp
// Microsoft x64 ABI; st is the action-state object, NOT the character object.
using SetAction = void (__fastcall *)(void* st, uint32_t id,
                                     uint32_t flags, float startTime);
reinterpret_cast<SetAction>(base + 0x66C4A8)(st, 0x628B, 0, 0.0f);
// RCX=st, EDX=0x628B, R8D=0, XMM3=0.0f. No fifth argument.
```

This is a concrete **form-6E73 action experiment**, not proof that 628B is a
repeatable damaging yokai attack. The executable explicitly chooses 628B for
6E73 at 0x2E6DC9 and invokes precisely this ABI at 0x2E6D18..21. For form 5455,
the corresponding executable-selected ID is 6248 (0x2E6DE7). These are the
form-entry actions; the motion/hitbox semantics live in resource data, not in
this image. Do not label a successful entry-animation replay an attack fix.
The same call with a verified attack ID is the smallest complete force-action
wrapper; it avoids re-running Begin/RefreshForm and their form replacement.

**Do not latch state+1068 bit 14 while repeating attacks.** It is an input
request consumed by the gate, not a persistent transformed-mode flag. Latching
it can repeatedly select the form-entry action instead of the attack transition.

## Where an attack button goes

Disassembly-proven chain (all addresses are RVAs):

1. In the dispatcher, higher-priority pending bits at state+A8 are handled
   first: bit 4 -> 90248C, bit 6 -> 748C58, bit 30 -> 9E4EAC. At 2E9251 it
   calls 2E6C34(st).
2. 2E6C34 checks bit 14 of state+1068 at 2E6C43. If clear, or 481290(st)
   returns true, it returns false. 481290 is true only with a valid current
   action descriptor whose +18 bit 29 is set AND state+AE0 nonzero.
3. If that request is accepted, it skips Begin/RefreshForm when bit 1 of
   `(*(character+3A0)+178)+10` is already set. Otherwise it calls 843010 and
   74C1DC(st,0), which can replace the forced form before selection.
   **Already active does not make this gate reject the request.**
4. It reads the form through vf+178 (555B00: state+A68), chooses the form-entry
   action, calls 66C4A8, then clears input ID 4E via
   FC1A20(st+1060,0x4E,false) at 2E6D43..51. The dispatcher masks pending bits
   with FFFFFFFFBFFFFF84 and skips its ordinary transition branch for that slot.
5. Without an accepted Tenshin request, normal pending bits reach
   **9021E4(st, uint8_t slot)** at 2E927A / 2E9320. It enumerates the current
   action's transitions via 3103B8, checks timing, and calls vf+120=20CF68
   and vf+128=2B2D88 for input/condition predicates.
6. 20CF68 reads up to three input conditions from transition words
   +10/+12/+14, mode bytes +16/+17/+18, and auxiliary words +1A/+1C/+1E.
   At 20D088 it calls input object's vf+B0; input object is state+18, falling
   back to state+10. Thus the button-to-action relation belongs to the
   selected action's transition data, not a hard-coded Tenshin attack switch.
7. A matched transition goes through state vf+1A8=35CF28 at 902435, selects
   its target with 35EF04, and commits via vf+F8=4F54C4 -> 35BC68.

The “Charge Attack Y+B / End Living Artifact” screenshot establishes the HUD's
mode, not which transition table or attack entry was selected. Changing body
visibility at character+B440 / model+A0 does not itself change this route.

## Verify the ID before interpreting a replay

66C4A8 clears pending/action-transition fields, then calls vf+F8 with
`(st,id,nullptr,-1,0)` and ends with vf+170=66C544(st,1). Calling 35BC68 alone
would omit player bookkeeping in 4F54C4 and wrapper resets.

Before the experiment, inspect `49B688(st+178,id,&resolvedKind,-1)`:
its return is the resolved entry, `entry+20` is the descriptor, and
`descriptor+20` is the signed motion identifier (-1 means transition-only).
Log resolvedKind and the pointer as well as the requested ID. The lookup
searches multiple tables; state+1D8 is kind C, checked after kinds D/E.
A naked numeric ID does not prove the correct form's action was selected.

If lookup misses, **35BC68 falls back to action 0** at 35BEF9..35BF06.
66C4A8 has no useful success return. Confirm the resulting state+E8 entry,
motion and actual damage/hitboxes rather than assuming the call succeeded.
Even a nonnegative motion ID proves an animation entry, not damage.

For an actual repeatable yokai attack ID, capture the target selected by
35EF04 for a matched attack transition in the correctly installed form table,
then pass that ID to the same 66C4A8 call. No fixed follow-up attack ID is proven
by this offline image. In particular, do not infer 628C from 628B+1: 628C is
already the entry action for forms 1302/1909/5095.

## Evidence and next step

Capstone 5.0.7, image-layout dump (file offset = RVA):
`/var/home/bazzite/tmp-archive/re-dump/Nioh3.mem.bin`, SHA-256
`f8a4fb31b691aaf9bb829fca4a32b49a4ad4c1c780f6c11cc516108b75b5d968`.
The requested `re/Nioh3.dump.exe` was not present locally. The vtable at
38FE698 contains the function addresses cited above. This is static evidence;
compatibility with PC2's current executable must be byte-checked before use.

Next action for the game owner: stop reasserting input 4E after entry, verify
which table resolves 628B, invoke the one-shot call above, and record requested
versus resulting action/motion and whether it damages a target. If it only
replays transformation, collect the matched attack-transition target as above.
