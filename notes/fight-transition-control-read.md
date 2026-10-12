# Fight transition: current native control restriction read

Read-only native audit, 2026-10-11. No native calls, hook installation or game actions. Source: inert `re/Nioh3.dump.exe`; PE `.text` and `.pdata` RVA/raw offsets checked equal. Function boundaries below obtained from PE exception directory. This is a current restriction predicate, not proof of physical input response or boss actor readiness.

## Native chain and validation anchors

`Player::SetControlOff` wrapper [0x23AB148,0x23AB1CE): at 0x23AB184 bytes `48 8B 0D CD 0B 21 02` load manager pointer from RVA0x45BBD58; at0x23AB18B `B2 48` selects reason0x48. Boolean true calls0x9DC6D0 (`E8 3A 15 63 FE` at0x23AB191), false calls0x563EDC (`E8 3F 8D 1B FE` at0x23AB198). These are ordinary native callees after VM argument decoding; wrapper OnEntry is not completion evidence.

Set [0x9DC6D0,0x9DC724) and clear [0x563EDC,0x563F30) find a sorted two-byte `{uint8 reason,uint8 active}` record in manager+0x18 pointer, manager+0x20 qword COUNT. Set writes `C6 40 01 01` at0x9DC710, clear writes `C6 40 01 00` at0x563F1C. Both invoke recompute0x705B40. Missing reason does not allocate a new entry.

Native reason predicate [0x563F48,0x563F9E) does binary lookup in this primary table, but primary false alone is insufficient: after finding the reason, it scans secondary manager+0 begin / +8 end, stride16, reason at entry+8. Cold block0x19D37F0 `38 50 08` compares that reason; match returns true (`B0 01 C3` at0x19D37FE). Only absence from this secondary vector permits returning primary byte+1 (0x563F96 `41 8A 40 01`).

Recompute [0x705B40,0x705BE5) clears manager+0x34 (`89 41 34` at0x705B5D), ORs data-defined masks for active primary reasons, all secondary reasons, and nonzero manager+0x30 reason. Cold contribution block [0x1A016C0,0x1A01712) covers the secondary and +0x30 paths. Helper [0x1090590,0x10907AE) ORs fourteen bits from reason definition+4 into manager+0x34. Existing getters read individual bits (e.g.0x27A8C4 reads bit0;0x353428 reads bit1;0x563ADC reads bit2). This audit does not assign gameplay meanings to all fourteen bits. Requiring aggregate zero is a conservative veto covering all represented restrictions, potentially stricter than necessary.

## Minimal reader contract

Run on the existing game thread; never call these native setters/getters. Validate the build/site anchors before enabling this reader. Use safe Rd/Copy; unknown is not allowed.

1. Read nonzero manager from module+0x45BBD58, primary ptr/count, secondary begin/end, reason+0x30 and aggregate+0x34.
2. Primary count must be1..256 with readable nonnull ptr, overflow-safe count*2, strictly increasing unique reason bytes, active bytes only0/1. Require exactly one reason0x48 with active0. The256 cap follows the uint8 key space; rejecting larger/corrupt arrays is intentional.
3. Secondary end>=begin, size divisible16, readable and overflow-safe, at most256 records (engineering fail-closed cap, not a proven native maximum). Require no record+8==0x48. A nonempty secondary array may still carry other active restriction reasons; aggregate zero is required separately.
4. Require aggregate==0. Record current+0x30 for consistency; it is included in native recomputation, but its numeric value itself is a reason ID, not a boolean control flag.
5. Re-read manager identity/descriptors/+0x30/+0x34 and re-copy both buffers; require equality. Any failure/change resets readiness settling. Sampling consistency does not establish cross-thread locking; do not read from render/UI worker or retain native pointers across ticks.
6. Combine this CURRENT predicate with valid offline/native attempt identity, exact requested mission, loading0, readable player/state, pause0, expected boss identity and root's separately proven actor readiness. A post-request SetControlOff(true) edge is useful evidence but cannot be mandatory for native at-fight routes with no intro. Never treat a stale false event as current permission.

Keep unknown/failure distinct from blocked. Zero aggregate plus reason48false proves absence of recorded restrictions in this manager snapshot, not player alive, rendering complete, boss constructed, or first controllable frame. Current bosslog create events are entry requests; FnVerify PASS precedes loading/pause checks and cannot replace those proofs.

## Actual failure-path checks for implementation

Use the actual reader with mapped fixture memory: null/unreadable global; zero/oversized count; missing/duplicate/unsorted48; invalid active byte; primary48true; primary48false but secondary48present; malformed/reversed/oversized secondary span; nonzero aggregate from another reason; manager/descriptors/bytes changing between copies; all-valid currentclear. Every negative must keep cover readiness false without native mutation. Separately test attempt replacement, wrong mission, stale boss witness, intro load, pause and stopped pump.

Presentation timeout/cancel/stale-host must release owned input/cover without asserting native request cancellation, clearing quarantine, or discarding spawn/map restoration ownership. Cover draw ACK must be attempt/session-bound before native admission; it is presentation evidence, not native readiness.

## Current expected boss: closed AlgoActive read, bounded life-state edge

The `GetAlgoActiveFlag` VM binding [0x238E51C,0x238E5C1) reads global0x45B9E40 at0x238E558 (`48 8B 0D E1 B8 22 02`), resolves its integer placement argument through0x135B78, requires wrapper and wrapper[0] nonnull, then calls0x486BE0. This supplies exact placement lookup semantics, unlike the recent Create ring.

Lookup0x135B78 is a leaf without a .pdata entry; decode through its returns [0x135B78,0x135BE2). p=Rd(base+0x45B9E40); context=Rd(p); map=Rd(context+0x48); table=Rd(map), count=Rd(map+8). Table stride16, uint32 placement key at+0 and wrapper pointer at+8. Native lower-bound search requires exact key, then returns wrapper. The second indirection before context+0x48 is essential. Read-only implementation can bounded-binary-search this sorted table with safe reads, validating exact key at the selected entry; do not equate wrapper with live actor.

Getter leaf [0x486BE0,0x486C03) bytes:
`48 8B 81 B0 03 00 00 48 85 C0 74 14 48 8B 40 50 48 85 C0 74 0B 83 B8 74 15 00 00 01 0F 94 C0 C3 32 C0 C3`.
Thus live=Rd(wrapper); component=Rd(live+0x3B0); algo=Rd(component+0x50); current AlgoActive iff all nonnull and Rd<int32>(algo+0x1574)==1. Preserve unreadable/null distinctions even though native getter collapses null to false.

Setter binding [0xA3F1C8,0xA3F328) builds and queues a command through0x66BBF4; its entry is NOT an activation-completion event. Builder [0xA3F328,0xA3F389) uses command vtable RVA0x3910D38, with execution slot+0x10 pointing0x1C0A274. The execution reads command+8 boolean and dispatches true0x66982C /false0x668D74. True writes `C7 81 74 15 00 00 01 00 00 00` at0x669845; false writes `89 81 74 15 00 00` at0x668D91. Both use the same live+3B0 ->+50 chain. A current-state read therefore avoids the asynchronous setter-request gap.

Read the placement map, selected key/wrapper, wrapper[0], component/algo and active state twice within the same game-thread tick, together with expected attempt sequence, current mission and loading state. Require equality, current active1, requested placement nonzero, and unchanged attempt/mission. Suggested map count cap1,048,576 is an engineering fail-closed bound, not a proved native maximum; bounded binary search avoids scanning large arrays. Never cache actor pointers between ticks. A double read detects changes but is not a lifetime lock or ABA-proof epoch: map descriptor and pointer equality cannot detect a destroy/recreate at the same addresses. The admission attempt sequence is a rematch epoch, not an actor allocation generation. Do not invent an actor generation from it.

Native life/death is a separate predicate.0x7E11F0(wrapper) uses wrapper+0xE3 ONLY when wrapper[0] is null; with live actor it tail-jumps0x4892A8. Therefore reading wrapper+E3 while live exists is wrong. [0x4892A8,0x48937F) checks live+0x3A0->dword+0x48==1; virtual state flags0x35,0xB0,0x2C through component+8/vtable+0xC0; signed live+0x8D4>=1; and a special actor-kind path (masked live+0 tag0x930FE0, character id0x4E52 and predicate0x280720 argument0x7AB). The virtual calls and special path are not reduced to passive fields by this audit. Current AlgoActive1 plus materialized actor is NOT a closed native alive predicate. A production readiness reader must retain that distinction or separately validate the exact native readonly life predicate/virtual implementations; do not replace it with HP, Create-ring age, wrapper+E3 or an event latch.

Useful actual-reader fixtures: wrong/missing placement; correct wrapper but null live; missing component/algo; active0/2 versus1; changing selected key or wrapper/live/algo on second read; same placement from prior mission; replaced attempt; correct current active with dead-state evidence must not be promoted to alive. Failures keep readiness false; presentation timeout still releases input without native cancellation claims.
