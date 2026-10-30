# Summoning Seal orb: flight / landing / spawn (static RE, #56515, 2026-10-30)
Source: decrypted dump tmp-archive/re-dump/Nioh3.mem.bin (v2.02, RVA == file offset). Nothing was run live.
Key: [P] = proven by disassembly, [T] = proven by the ally runtime trace, [I] = inferred, needs a live check.

## Objects
- `ctrl` = rcx passed to 322468 (the "obj AB0/B80" values in the trace). `O = [ctrl+8]` is the item world object.
  3203EC is O's per-frame update and calls `322468(ctrl=[O+0x250], xmm1=substep dt)` at 3209EB, once per
  substep [P]. r8/r9 are not arguments of 322468 (it never reads them), so the logged r8/r9 values are leftovers from the caller [P].
- Param getters read fields that were copied into O [P]: int `3233E8(O,i)`: 0xF->O+0x630, 0x17->O+0x644, 0x26->O+0x674,
  0x2A->O+0x684, 0x2B->O+0x688, 0x2F->O+0x698, 0x3E->u16 O+0x718, 0x3F->i16 O+0x734, 0x41->u16 O+0x738.
  float `2A093C(O,i)`: 1->O+0x604, 2/3/4->O+0x6D0/D4/D8, 5->O+0x608, 8/9/A->O+0x614/18/1C, 0xD->O+0x628,
  0x10-12->O+0x634-63C, 0x13-15->O+0x6F8-700, 0x16->O+0x640, 0x30-0x38->O+0x69C..0x6BC.
- **AB0 = the orb** [T+P]: when it enters state 2 it creates effect param 0x17 (O+0x644 = 0x6254D) at 322958.
  It skips the burst because O+0x630 == 0.
- **B80 = the landing/burst object** [T+P]: when it enters state 2, 2A06B4 creates effect param 0xF (O+0x630 = 0x62557).
  The code path is 2A06B4 -> `lea edx,[rax-7]` (=0xF) -> 29EF38 at 2A07EA.
  B80 is a new object created where the orb landed [I, strong]: 322EBB `8D8190(ctrl, param 0x2B = O+0x688)`
  -> 8D81F8 -> 8D8234 queues `{id, pos=O+0xF0, owner=O+0x260 if chr, type=4}` into `[0x4745348] (global mgr)+0xC90+0xA0`.
  The B80 ctrl slot existing before the press, with its r8/r9 changing at +0.69, looks like ctrl-slot reuse [I].
- Yokai chr spawn: no direct-call path from 322468, 2A06B4, 9F0064, 5C9BC4 or 2B87C0 to 683F74/682C00 within depth 4-5 [P].
  The spawn is not issued inside 322468 or 2A06B4. It follows B80's activation by about one frame, most likely from B80's
  own event/timeline (66FC28 -> 682C00 -> 683F74, see 53994) [I].

## State machine, ctrl+0x30 (322468) [P]
- O+0x380 is a release/cancel request word: 1 = release, 2 = cancel/stop, -1 = consumed.
  3A8290(chr, O) sets it to 1 (`mov [rdx+0x380],1` at 3A82B7) and also aims the object (O+0x3DC flag, O+0x3E0 target).
  66FC28 calls 3A8290 right after creating the object with 660B98, at 670B55, 671024, 671403, 6720B1, 672788 and 673003.
  Event 0x1A (670F29) and event 0xEA (672D94) write r12d (=2, set at 670254) to O+0x380 for owned type-4 objects.
- **0 init** (3225F2): if p2F!=0, sets up a sub-body `[ctrl+0x38]=93BA4(O+0x90,1)`: offset p36-38, rotation p30-32 (deg*0.0174533),
  and p33-35 (deg) via 5B4D18. Then it branches on O+0x380: 0 -> state 1, 2 -> state 4, otherwise -> **state 2** (sil=1). O+0x380 is then reset to -1.
- **1 held** (32276C): O+0x380==1 -> state 2, ==2 -> state 4.
- **2 flying** (32279F). One-shot entry while byte ctrl+0x18 != 0 (3227A8..322B8F, then ctrl+0x18 is cleared):
  - copies start position O+0xF0 to ctrl+0xA0 and picks a target;
  - **322834 `call 2A06B4`** (skipped if O+0x750&0x40; then it is called at 322B69 instead). This is the burst effect param 0xF plus param 0x25 (2A0F60);
  - **322958 `call 29EF38`** creates effect param 0x17 (the orb), with the handle stored at ctrl+0x78;
  - sets the launch velocity (see below), then calls `8D8190(param 0x2A)` at 322B81 (spawns an object on launch);
  - 0x1B3..0x1B5 = 1 is set on entry and cleared on landing.
  Per-substep (falls through on the same tick as the entry block):
  - O+0x380==2 -> zero velocities, state 4 (322BE0);
  - timer `ctrl+0x20 += 9C6E30(ctrl)*dt` (322CEE); at p3F (O+0x734) it fades the model via 2BC5DC;
  - **landing test 322DD9: `6A3BB0(O, 0x532|0x53C, word O+0x5F0, !(O+0x750>>12 &1))`**. This is a ground/water-height
    test using the current position O+0xF0 and the previous position O+0x100.
    If true: O+0x750&0x800 -> state 3 directly, otherwise `5B4098(ctrl)` (impact) -> state 3.
    5B4098 zeroes velocities, plays hit effect O+0x384 (28EA460), and runs the area effect 474E58 if p44/p43 > 0.
    5B4098 exits early without landing if O+0x7E0==0 && O+0x750&4 (bounce), or O+0x7E0!=0 && O+0x750&2.
  - distance `ctrl+0x1C += |bodyA.vel|`. If it reaches p0D (O+0x628, when >0), or the timer reaches p3E (O+0x718),
    `5B4620` runs: state 4 with no landing spawn.
- **3 landed** (322EA5): **322EBB `8D8190(ctrl, p2B)`** spawns the landing object (B80) at O+0xF0. Then state 4.
- **4 ending** (322EEE): runs once while byte ctrl+0x19 is set. It stops the param 0x17/0xF effects with 29F598 (by effect id) and detaches.
  After that it waits on the effects (662F0C) and on the linger time `ctrl+0x2C > p41` (O+0x738), then destroys the object (161830).
- Answer to Q1: the flight is state 2. It moves to the landing branch only when 6A3BB0 reports ground contact at 322DD9.
  Timeout or distance gives state 4 with no spawn. 322834/2A06B4 is not the landing code: it is the state-2 entry of the
  object spawned on landing (B80).

## Position / velocity [P, field naming partly I]
- O+0xF0 is the position (vec4), O+0x100 the previous position, O+0x110 the 4x4 orientation (used by 3203EC and 6A3BB0).
- Physics bodies: bodyA=`32002C(O+0x90)`=[O+0x148+0x18], bodyB=`5B482C`=[O+0x148+0x20]. Setters are
  5B4D18 (body+0x60) and 5B4960 (body+0x80). Landing (5B4098/5B4620) zeroes both fields on both bodies.
- The throw is set at state-2 entry:
  - bodyA+0x60 = O+0x110 * (0,0,p1) at 3229D3-322A08: p1 = O+0x604, forward speed [I: linear velocity];
  - bodyA+0x80 = O+0x110 * (p2,p3,p4), with y += -|p5|, at 322A0D-322A91: p5 = O+0x608 [I: acceleration/gravity];
  - bodyB+0x60 = (p8,p9,pA)*deg2rad at 322A96-322AED [I: spin].
  - p30-p38 (state 0) configure the sub-body. They are not the throw velocity.

## Recommended patch (call-site retargets in 322468 only)
1) Land on the first update. Retarget the call at **0x322DD9** (orig `E8 D2 0D 38 00` -> 6A3BB0).
   Guard pattern at 0x322DD3: `81 C2 32 05 00 00 E8 D2 0D 38 00 84 C0 74 21` (unique).
   Hook `u8 h(O, edx, r8d, r9b)`: if O is the seal orb (`*(u32*)(O+0x644)==0x6254D`, optionally also `*(u32*)(O+0x688)!=0`), return 1.
   Otherwise return the original 6A3BB0. The entry block falls through to this test on the same substep, so:
   tick 1 = state 3 (via 5B4098 or 0x800), tick 2 = 8D8190 queues B80 at O+0xF0.
   O+0xF0 at that point is the orb's creation point next to the player (hand/attach offset from the player motion event) [I].
   Risk [I]: if the seal item has O+0x750&4 with O+0x7E0==0, 5B4098 returns without landing.
   Check live that ctrl+0x30 becomes 3. Fallback: OR 0x800 into O+0x750 inside the hook before returning 1. That flag is read only at 322DE6 in this function [P].
2) Optional: spawn at the player's feet instead of the hand. Retarget **0x322EBB** (orig `E8 D0 52 5B 00` -> 8D8190).
   Guard at 0x322EB6: `8B D0 48 8B CF E8 D0 52 5B 00 44 89 6F 30`.
   For the seal orb: copy the 8D8190 body (it calls `8D81F8(id=edx, &pos, owner, 4, 0)`) but pass the owner's position.
   The owner is `[O+0x260]` when `(*(u8*)owner & 0xF)==0`. The chr position offset was not proven statically: read `[owner]+0xF0` live first [I].
   Alternative: before calling the original, write the owner position into O+0xF0 (8D8190 reads O+0xF0).
3) Suppress the orb visual. Retarget **0x322958** (orig `E8 DB C5 F7 FF` -> 29EF38).
   Guard at 0x322955: `48 8B CF E8 DB C5 F7 FF 8B 08 89 4F 78` (unique).
   Hook: if `r8d==0x6254D` (the orb effect; you can also check `*(u32*)([rcx+8]+0x644)`), do `*(u32*)rdx=0; return rdx`.
   This is the same thing 29EF38 itself does when [ctrl+8]==0 (29F0E7 `and dword [rdx],0` -> returns rsi=rdx) [P].
   The caller stores the result in ctrl+0x78. Cleanup (29F598) stops effects by id, so a 0 handle is harmless [P].
   The burst (0x62557, from 2A07EA in B80) and the spawn are not affected.
   Do not touch 322834: that is B80's burst. For the orb, 2A06B4 creates nothing because O+0x630==0.
- Data-poke alternative (instant test with /proc/pid/mem): `*(u32*)(O_orb+0x644)=0` before release removes the orb effect
  (the 322849 test fails). Lowering p3E/p0D does NOT help: those paths go to state 4 without spawning B80.

## Live checks still needed
- O_AB0+0x688 matches B80's item id (B80 comes from the 8D8234 queue).
- 6A3BB0 forced true leads to state 3, then state 4, with B80 spawned within 2 ticks, and the yokai still spawns.
- The value at [O+0x260]+0xF0 is the player's position.
