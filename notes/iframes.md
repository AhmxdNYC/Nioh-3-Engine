# I-frames on the frame meter (#35176)

Offline only (2026-10-25). The game was not launched or touched. Exe: `re/Nioh3.dump.exe` (file offset = RVA).
Decompiles came from the read-only Ghidra copy (`D:\nioh3-re\decomp_ro.cmd`).

## Short answer
**An i-frame is not recovery.** On the meter, recovery is an ATTACKER fact: the frames after the move's last hit window
until its action ends (`frame_meter.h`, `FM_RECOVERY`). An i-frame is a DEFENDER fact: a frame when hits on you do not
connect or do no damage. These are different things. A dodge has no hit windows, so the meter shows it as "No hit" or
"Pending". It never shows the dodge as recovery, and it never shows its i-frames.

## How the game marks "no hit" and "no damage" (proven in code)
In this note `ctrl = [[actor+0x3B0]+8]`. That is the same ctrl the Training mod already reads (`FpPlayerTick`).

| flag | written by | read by (gate) | effect |
|---|---|---|---|
| `ctrl+0xEB1` byte | script `Character::SetNoHitInvincible` (native 0xA3FF5C -> command vtbl 0x3910DC0, execute 0x1C0AB10) | contact handler 0x117A6D0 (`if ctrl+0xEB1: return`), 0x678xxx, 0xA131B2 | the attack never connects (no hit record) |
| `ctrl+0xEB0` byte | script `Character::SetNoDamageInvincible` (native 0xA30824 -> vtbl 0x390FEF8, execute 0x1C0AAE0) | `IsNoDamage(ctrl)` 0x2A343C | the hit connects, but there is no damage / no reaction |
| `actor+0xABC0` byte | actor commands (0x1C2563C etc.), cleared in the actor init 0x2C19C4 | contact handler 0x117A79E (the first check: `return`) | no hit |
| `actor+0xADB1` byte | actor command 0x1C0AB51 / 0x1C26BC8 (Player/Mission `NoDamageMode` family) | `IsNoDamage` 0x2A344C, contact handler, 0x2A39F4, 0x911DA1 | no damage |
| skill/buff `0x2AE` | vfunc `ctrl+0x138(0x2AE,0,0xFF)` when `[[actor+0x3B0]+0x50]+0x1940 & 4` (the player) | `IsNoDamage` | no damage |

`IsNoDamage(ctrl)` 0x2A343C: `actor=ctrl+0xE0; if actor+0xADB1 -> 1; if player && vcall+0x138(0x2AE) -> 1; else ctrl+0xEB0`.
Its callers are the damage / reaction paths: 0x326C94 (damage calc, `+0x29E` cleared), 0x327DEC, 0x786C84, 0x78CE8C, 0x911D54 and others.

Hit pipeline (proven): physics contact 0x117A3E0. Shape filter `+0x10`: attack `0x440003F`, damage (hurtbox) `0x3FFFC0`.
The pool id is at `+0x58`, the pool is `[45C6F38]`. Then the attack-vs-damage-collision handler 0x117A6D0, then the hit queue
(manager `[47452E0]`, 50 x 0x1C0, count `+0x5910`, push 0x7897D0), then the processor 0x789B5C, then the damage object
0x78A458 / 0x78A194, then the validity check 0x78AC18 and the damage calc 0x326C94.

## What a dodge uses (strong inference, NOT proven)
- Every other gate on that path was checked. All are for the team / faction, hit history, airborne (condition 2), downed
  (condition 0x23), dead (0x2A3264: ctrl+0xEA0/EA1, status+0x2DC8 bit 31) or attack-type exemptions. These are rare row
  bits: over 21,219 timed hitbox rows in 308 archive_01 files, row `+0x48` bit 13 = 3.5 %, bit 28 = 6 rows.
  **No gate tests a per-frame "avoid" or "invincible" state.**
- The script flags above are written only by script and actor commands. No action-event code writes them.
- So a dodge's no-damage frames are most likely made by switching the hurtbox (Collision::Damage shapes) off or to another
  set for that window. The action event slots `+0x40` point to sub-table 5 events. Events 41-44 and 47 are known to switch
  the body collision set (`notes/yoritomo-attack-values.md`). With no damage shapes there is no contact, so no hit is
  ever queued. This is not proven yet: the field that says "hurtbox off" was not found.

## Training change (this branch, built and offline-tested only)
- New cell overlay **"No damage" (i-frames)** on the PLAYER bar: a gold stripe across the cell while any proven game flag
  above is set (`ctrl+0xEB1`, `ctrl+0xEB0`, `actor+0xABC0`, `actor+0xADB1`). Read only.
- A log-only probe (`[Debug] IFrameProbe 1`): it logs the player action id, `ctrl+0xA8`, `ctrl+0x1060` and the
  `ctrl+0xEA0..0xEB2` bytes every time they change. One dodge session shows which bit (if any) follows the dodge window.
- What to check in game: Practice > Player frame meter ON, then dodge. If the gold stripe stays off during dodges, dodge
  i-frames are hurtbox-based (as inferred). In that case read the IFRAME probe lines to pick the right signal.

## Exact next step if the stripe is off during dodges
Find the code that applies the sub5 collision-set events, starting from the per-tick ctrl update 0x35CF28 and the
action start 0x4F54C4. Also look at the Collision manager functions at 0x237CC54..0x237D8E8: 0x237D954 reads
`actor+0xABC0`, and they are the main users of the pool `[45C6F38]`. Then find the hurtbox shapes' enable or filter
word, and count the player's enabled damage shapes per tick.

## Code
- `frame_meter.h`: `nodmg[]` per cell plus a frozen copy, `FmMarkNoDmg`, `FmViewNoDmg`, and `FmLiveRun`. A run with i-frame cells
  but no hit (a dodge) is shown live and then frozen, the same as an attack. Before this change a dodge was never on screen
  while an older attack was frozen.
- `frame_meter_game.h`: `FmPlayerNoDmg` (four byte reads under `__try`), the gold stripe on the PLAYER row only, and the
  legend entry "No damage (i-frame)". `FmIFrameProbe` writes the `IFRAME ...` log lines when `[Debug] IFrameProbe = 1`.
- `tests/frame_meter_test.c` `NoDmgTests`: i-frame cells are never recovery; a dodge is shown and then frozen with its stripe;
  a hitch fills the skipped cells once; a ring slot that gets reused does not leak an old stamp.
- Build: `mod/Nioh3Training/build.sh` (zig clang). The source uses `__attribute__`, so plain MSVC `cl` does not build it.

## PC test of 89cd937d (coordinator, 2026-10-25)
- Dodges are drawn as their own runs. The gold stripe never lit: `nodmg 0` on all 202 IFRAME lines, and ea0/ea8 were 0 every time.
- a8/x1060 only flicker (bits 0x4000, 0x02, 0x01) on dodge actions 2060/2051/10024/10018, and the same bits flicker in idle.
- Result: the script flags do not cover dodges. Dodge i-frames are hurtbox-based (the inference above). Next: find the hurtbox enable state.

## Dodge data (offline, 2026-10-25, after the PC test)
- Player dodge actions in the PC log: 10015/10018/10021/10024 (decimal). They are in archive_01 `88/2229E56A...` (787 actions).
  2000/2001/2051/2060 are in `E5/95C355A8...`; 0x80C = 2060 loops (next = self), so it is a movement state and not a dodge.
- Dodge records 10014-10030 (motions 30001-30018): flags00 0x0004/0x0005, flags10 0x0800. Their events are all instant (no length):
  common 10072@0, local 40@0, 41@24, 42@0. Sub5 rows 40/41/42 differ only in a few u16 values (`+0x00 = 0x96`, `+0x04 = 0xC8` on 41,
  `+0x30` 0x00FF / 0x7DFF, and the `+0x88..` 0x4B/0x64 bytes). They look like speed / motion-blend events, not a hurtbox window.
  **The dodge record has no event with a length, so the i-frame window is not an event window in the action record.**
- The PC log: on the dodge start `x1060` gets 0x4001 for one tick, then `a8` bit 48 (0x0001000000000000) is set for the whole dodge.
  Bit 48 is also set in 2000/2001 (move), so it is not the i-frame.
- New candidate: contacts whose shape mask is `0x3800000` (bits 23-25, neither attack 0x440003F nor damage 0x3FFFC0) go to
  0x117C7F0 instead of 0x117A6D0 (dispatch 0x117A3E0). Nioh 3 has a JUST_DODGE event (UI/tutorial enum index 25 at 0x3C35888),
  so the dodge may swap the damage shapes for a "dodge sensor" category. Next step: check 0x117C7F0 and who sets shape `+0x10`.

## Hurtbox side (offline, continued)
- Collision shape layout (proven): `+0x10` = own category mask (set by 0xB9F14), `+0x18` = collides-with mask (0xB9B38, under a spin
  flag at `+0x40`). A layer id maps to the masks through 0x5884D4 and 0x5884B4. Collides-with is almost all bits
  (0x3BFFFFFF / 0xDFFFFFF), so attack and damage are paired in the contact callback 0x117A3E0, not by the physics filter.
- Damage-collision layer: 0x2B9140(side, obj+0xD4 flags, owner type). The result is 6..0x15, from the side
  (`[actor+0xB0]+0x10`) and obj+0xD4 bits 0 / 2 / 15. It is re-applied only when the side changes (0x669738, 0x237D470/0x237D480,
  which walk the pool and match obj+8 == owner). **Not a dodge switch.**
- `ctrl+0xEB4` (u32 mask, by part index obj+0xD8) is the weak-point / part mask: 0x787D74 is used by 0x789A78 to pick the
  better damage collision of a pair. Not the i-frame.
- The attack-vs-sensor contacts (mask 0x3800000) go to 0x117C7F0. That only feeds the AI's "attack incoming" sensing (AI+0x1C0,
  condition 0xF1). Not the i-frame.
- Still open in 0x117A6D0: **a damage collision whose def `[obj+0xB0]+0x14` is 0x17 or 0x19 never gets hit**. Also, a collision
  object that leaves the pool cannot be hit. Both are per-object, so the probe now logs the player's damage-collision objects.

## Build 2f8b6086 (this commit): hurtbox probe and candidate
- `FmHurtScan` (read only) walks the pool and keeps the player's damage collisions (`[obj+8] == actor`, `[[obj]+0x10] & 0x3FFFC0`).
  For each one it records slot, obj+0xD4, obj+0xD8, shape+0x10 and def type `[[obj+0xB0]+0x14]`.
- Gold stripe = the proven flags OR the hurtbox candidate: the player normally has damage collisions (the largest count
  seen without no-hit types), and now has none, or every one has def type 0x17/0x19 (the no-hit types in 117A6D0).
- `IFrameProbe = 1` now also logs `IFRAMEHB t .. act .. n .. off .. d4or .. d4and .. types ..: slot:d4/d8/mask/t<type> ...` every
  time that set changes. The `IFRAME` line also has `eb0` (ctrl+0xEB0..EB7, with the EB4 part mask).
- What to log in game: IFrameProbe 1, stand idle for 2 s, then do 5 dodges / steps with a gap between them. Send the `IFRAMEHB` lines.
  If `n`, `off`, a `d4` bit or a type changes inside the dodge, that field is the i-frame. Compare it with the stripe.
  If nothing in IFRAMEHB changes during a dodge, the window is in the motion data or in a timer. The next fields to log are then
  the player status floats near `[actor+0x3A0]` and `ctrl+0x7F0..0x820`.

## PC test of 2f8b6086 (log #2325), analysed offline
- IFRAMEHB is flat across 5 dodges (10015-10042): n = 2, d4or 0x121, type -1, the same as idle. Only the pool slots change, because the hurtboxes are re-allocated per action. They are not removed.
- ctrl+0xA8 in a dodge, tick by tick: start 0 (x1060 = 0x4001 for one tick, then 0) -> bit 48 (0x0001000000000000) set 9-53 ms
  after the start, until the end -> bit 0 on the last tick (end / cancel).
  Bit 48 is also set in 2000/2001 (move/idle) about 100 ms in, and in 4059. Bit 1 = next action queued, bit 42 = locomotion.
  **No bit turns on and then off inside a dodge, so ctrl+0xA8 / x1060 do not mark the i-frame window.**
- The damage path also has a "defence state" 0x2A34F8 (guard types from ctrl+0xEA2..0xEA8, Ki > 0). That is guard / perfect
  guard, not dodge (ea0/ea8 were 0 in the log).
- Next data needed: shape+0x18 (collides-with mask, set per bit by 0xA11340) of the player's hurtboxes was not logged.
  The status / ctrl blocks are not decoded yet, so they are logged as diffs during dodges.

## Build 308d35bf (this commit)
- IFRAMEHB now also has `m18or` and, per object, shape+0x18 (`slot:d4/d8/cat/collides/t<type>`).
- Stripe candidate (added): the player's hurtboxes stop colliding with any attack category (`(OR of shape+0x18) & 0x440003F == 0`),
  but only after they were seen colliding with one for this actor. The earlier rules stay: no hurtboxes left, all no-hit types, the 4 proven flags.
- New backup line `IFRAMEST t .. act ..` during actions 10000-10199. It lists every u32 that changed since the previous tick in status
  `[actor+0x3A0]` +0x80..+0x400 and +0x2DC0..+0x2E00 (`sOFF=hex(float)`) and in ctrl +0x7F0..+0x820 (`cOFF=...`). Max 2500 lines.
- What to check: IFrameProbe 1, idle 2 s, 5 dodges. Send the IFRAMEHB + IFRAMEST lines and say whether the stripe lit.
  Look for a field that changes a few ticks after the dodge start and changes back well before the end: that is the i-frame window.

## PC test of 308d35bf (log #2328), analysed offline
- IFRAMEHB is flat (m18or 0x3BFFFC0 in dodge and idle). The stripe did not light.
- IFRAMEST: 526 lines over 36 dodge starts (10015/10017/10018/10019/10021/10024). The only fields that change are Ki
  (status +0xAC current, +0xB8 previous, +0xCC regen step, +0x140 time-since-use ramp, +0xD4 = 1.0 once). ctrl +0x7F0..+0x820 never
  changes. **No status / ctrl field marks the i-frame window.**
- Dodge action data, again: event 10072 is common (165 player actions). Dodges have `+0xE8/+0xEA` = 36/36 (0x24) or 52/52 (0x34);
  2051 has 9/9, idle/move have FFFF. That is about the frame where the end / cancel bit 0 appears, not an i-frame window.
  Event 41 @24 is the same row class as 40/42 (speed / blend values). **No frame range in the action record can be read as i-frames.**
- Also checked: the hit history at AI+0x78 (0x2B44C4) is a per-attack hit-group list, and the damage validity check 0x78AC18 tests condition
  0x115 on the ATTACKER. Neither is a dodge state.

## Build 511a2719 (this commit)
- New `IFRAMEOB t .. act .. n ..` lines during dodges: every u32 that changed since the previous tick in the player's first 4
  hurtbox collision objects (`oJ+OFF`, obj +0..+0x100) and their physics shapes (`hJ+OFF`, shape +0..+0x100). `[J new]` = the object was re-created.
- If IFRAMEOB is also flat, the window is not stored in any runtime field we can reach cheaply. Then it is in the motion
  file (motions 30001-30018) or computed from the equipment weight. Next step: decode the motion files' event tracks (root.rdb G1A /
  motion, keyed by `+0xF0`), or put a log-only hook on the contact handler 0x117A6D0 (needs hookcheck) to see whether a contact
  happens at all during a dodge.
