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

## PC test of 511a2719 (log #2331), analysed offline
- IFRAMEOB hit its 2500-line cap, but it still covers 37 dodge starts (10015-10042). Every tick changes the transform floats:
  h0/h1 +0x30..+0x3C, +0x80..+0x98, and o0/o1 +0x90..+0xAC, +0xC0, +0xF0..+0xF8.
- Not a float: **obj1 +0x10/+0x14 is a 64-bit pointer** (high dword 0x260). It is re-set about 20 ms after every dodge start. Freshly
  allocated values (0x260CFAxxxxx, rising) usually change again 450-650 ms in. Recurring values (0x260C37xxxxx, 0x260F9F8xxxx, 0x260C911xxxx)
  come back across dodges. This looks like a reference to the current state / def object of the hurtbox. It is not a flag yet.
  It needs a deref to see whether the referenced object has a type that means "no hit".
- obj0 +0x18 / +0xE8 and the shape +0x70 / +0xB0.. change only when the hurtbox is re-created or at the end. No flag-like value
  turns on a few ticks after the start and off before the end.

## Build 5fc59ebd (this commit): IFRAMEOB v2
- Skips the known transform offsets and any change that looks like a small float step (both values normal floats, |delta| < 25 % + 1).
- Adds `pJ=<ptr>:w0,w1,..` = the object obj+0x10 points to (+0..+0x40), logged when the pointer or its content changes.
- Max 150 lines per dodge start, 2500 in total, so all dodges are covered. `[J new]` = the hurtbox was re-created.
- What to check: IFrameProbe 1, 5 dodges. In the `p1=` lines, look for a word that differs between the fresh pointer set at +20 ms
  and the later one at about 500 ms (a type / flag). Also look for any `oJ+` / `hJ+` integer that turns on after the start and off before the end.

## PC test of 5fc59ebd (log #2335), analysed offline
- The `pJ=` entries are there. 155 lines cover about 20 dodges. obj+0x10 points into a linked list of nodes, all with the same vtable
  (0x1F96CE56030). Each node: +8 next, +0x10 back-link, then data.
  - Around +20 ms after the dodge start, obj1 gets a fresh node: `+0x18 = 1, +0x1C = 0x1000001, +0x20` a rising counter,
    `+0x30 = 100000.0f`, `+0x3C = 0x6197884D`.
  - About 450-700 ms in, the next one has `+0x18 = 0xC` and hash `0x0AE72C5F`.
  - After that come recurring nodes full of floats (world positions, which look like contact points).
  - 10015 never gets the first kind.
  - 0x6197884D is one of the hashes accepted by 0x95EC18 (a set test used near the action start, 0x4F3214). So these nodes are keyed
    by an action / object hash. They are not a flag.
- **The data is not flat, but there is still no proven on/off flag.** Doing the fallback: a log-only hook on the contact handler.

## Contact-handler probe (this commit)
- `hookcheck.fsx entry 0x117A6D0 all`: 4 direct callers (117A49D, 117A4E6, 117A574, 117A57F), **must preserve []**.
  The entry bytes `mov r11,rsp; push rbp; push rdi; push r14; push r15; lea rbp,[r11-1E8h]` (16 B) have no rip-relative operand.
- The detour (only with `[Debug] IFrameProbe = 1`) calls the original first. When the damage side belongs to the player (obj+8 == player actor)
  and the attack side does not, it counts the contact and compares the hit-queue count `[[47452E0]+0x5910]` before and after.
  So it tells "contact queued a hit" from "contact rejected inside 117A6D0".
- Log `IFRAMECT t .. act .. contacts N queued Q rejected R atk <owner> <atk obj>` on every tick with player contacts.
  Read during a dodge through an attack:
  no contacts = the hurtbox is not touched (geometry / broadphase);
  contacts but rejected = an early-out in 117A6D0, which can then be bisected;
  queued = the hit is rejected later, in the damage queue.
- Build **1bfef492** (this commit). 117A6D0 takes 2 arguments: a linear scan shows r8/r9/xmm0-3 are written before any read
  (xorps xmm2 / movss xmm1 / movd xmm0 / mov r8). So the C detour forwards (rcx, rdx) only. The hook is installed only when IFrameProbe = 1,
  on the pump thread (first FmIFrameProbe call). The original runs first and nothing is written to game memory.
- What to check: IFrameProbe 1. (a) Let a boss hit you once while standing: that should give `IFRAMECT .. queued >= 1`. (b) Dodge through 5 attacks:
  read IFRAMECT during act 10015-10042 (no line = no contact at all). Send the log. The stripe rule is unchanged.

## PC test of 1bfef492: contacts happen, every dodge contact is rejected
- Standing: e.g. act 2000 `contacts 4 queued 1 rejected 3` (several shapes per tick, one hit gets queued). In dodges 10000-10199: 636 lines, all
  `queued 0` (rejected 1-6). Some non-dodge lines are q0 too (act 4088).
- Reject exits of 117A6D0, in order (from the decompile, `def` = dmgObj+8, `H` = atkObj+0xB0, `row` = [H]):
  1. def+0xABC0 != 0
  2. ctrl+0xEB1 (NoHitInvincible)
  3. team block (attacker is a character): def+0xADB1; same side + 0xA5F20; both side 2; row+0x92 type 6/7 with another side;
     attacker side 0 && def+0xAB84 != -1; both side 0 && !IsNoDamage && 0x402D88 != 5
  4. dmgObj+0xB0 def type 0x17 / 0x19
  5. row+0x48 bit 9; bit 6 (only its own target); bit 12 && !0x8AB924(def)
  6. row+0x48 bit 24 (with def+0xAB84 / side)
  7. row+0x4C bit 0: grab / owner chains (ctrl+0xB68, ctrl+0x668)
  8. row+0x48 bits 7/8/14/10/13/15/28/31/30 and row+0x44 bit 24 (state conditions on def)
  9. 0x9B4B2C (row+0x92 >= 0, attack-type special cases)
  10. hit cap: (int8)row+0x21 vs H+0xE4
  11. **hit history**: 0x2B44C4(defAI, atkObj) = the attack (owner, hit group) is already in defAI+0x78
  12. row+0x44 bit 9; dmgObj+0xD4 & 0x6000 (front / back only)
  13. otherwise the hit goes to the queue push 0x7897B0 -> 0x789844. The push merges a contact into an existing entry with the same
      attacker / defender / hit group in the same tick (returns 0 = no new entry). That explains standing `4 contacts, 1 queued`.

## Build 055b0816 (this commit): per-contact inputs
- For every player contact (max 1500) the detour reads the inputs of the exits above BEFORE the original runs, and logs one line:
  `IFRAMECX t .. act .. queued 0/1 atk <owner> obj <atk obj> abc0 eb1 eb0 adb1 side d/a dtype d4 row r48 r44 r4c r21 r92 hcnt hist`.
  `hist` = 0x2B44C4(defAI, atkObj) (1 = this attack's hit group is already in the player's hit history). `hcnt` = H+0xE4 vs `r21`.
- Read-only: pure loads plus the list walk 2B44C4 (no writes, checked in the disassembly). It runs on the physics thread, like the game's own call.
- What to check: IFrameProbe 1. Get hit once while standing (queued 1), then dodge through 5 attacks. Compare the IFRAMECX inputs of the
  rejected dodge contacts with the queued standing contact. The field that differs is the i-frame rule. Look first at hist, hcnt/r21, dtype,
  d4 & 0x6000, side, eb1 / abc0.

## PC test of 055b0816 (IFRAMECX): the hit history rejects later dodge contacts
- All exit inputs are the same in dodge and standing. Only `hist` / `hcnt` differ. In dodges the contacts are rejected with hist = 1
  (the attack's hit group is already in the player's hit history), and the first contact of each dodge (6 seen) was queued with hist = 0.
- Who writes the hit history (0x789134 = add the damage object to defAI+0x60 -> list +0x18 = AI+0x78)? Only the damage-object commit paths:
  0x78A548 (78A458), 0x748EFA, 0x74901E, 0x1FF4444, and the contact handler's online/special branch 0x117C3BE (attacker [+0xE8]+0x80 in {2,5,6}).
  The 0x78A458 path records it **also when the damage object ended with no damage (+0x2A0 = 1)**.
  => In a dodge, the first contact is queued, the damage pipeline turns it into "no damage", and it is still recorded. Every later
  contact of that attack is then rejected by the hit history. **The i-frame decision is in the damage pipeline, not in the contact handler.**
- Candidates inside the pipeline (0x78A7F0 -> 0x78AC18 / 0x326C94), all read from the defender:
  - 0x2A3264 "cannot be damaged": the current action record dword +0x18 bit 29 together with ctrl+0xAE0 (and ctrl+0xA8 bit 20); ctrl+0xEA0,
    ctrl+0xEA1 (unless hit type 4), status+0x2DC8 bit 31, ctrl+0xEAE for hit type 9, [ctrl+0x15D8]+2.
  - 0x326C94 when not guarding: **the defender's current action record qword +0 bit 35** gives no damage (cVar5 = 0). In the player
    file only actions 10081-10096, 1476 and 1480 have it, not 10015-10042. Also status+0x48 == 1 together with row bit 1.
  - The guard path (+0x1E0 == 2, defence state 0x2A34F8).
- Next: a log-only hook on 0x78A7F0 that logs the damage object's result fields for player hits.
- The 207 non-dodge rejects with hist = 0 are most likely the queue merge in 0x789844. Contacts of the same attacker / defender / hit group
  in the same tick fold into one queue entry (0x789844 returns 0). The hit history is written only when the queue is processed later
  that frame. This matches "standing: 4 contacts, 1 queued, 3 rejected".

## Build 23dc9fb4 (this commit): IFRAMEDM damage-pipeline probe
- Hook on 0x78A7F0 (entry `push rbx / sub rsp,20h / mov rbx,rcx / call 78AC18`). The trampoline rebuilds those 3 instructions and calls 78AC18
  through rax (it is a rel32 call, so it cannot be copied), then jumps to 78A7FE. hookcheck entry 78A7F0: 1 direct caller (78A4B5), must preserve [].
  Installed only with IFrameProbe 1. The original runs, and nothing is written to game memory.
- One line per hit on the player (max 1500): `IFRAMEDM t act state-id nodmg(+0x2A0) f14c 1e0 1e4 1ec 1fc guard282 286 28e 29e 2ae atk row0 |
  rec0 (bit35) rec18 stat48 s2dc8 ea0 ea1 eae ae0 a8`. The defender inputs are read before the original runs.
- What to check: IFrameProbe 1. One normal hit while standing (nodmg 0), then 5 dodges through attacks (the first contact of each dodge
  gives an IFRAMEDM line with nodmg 1). The field that is set only on the dodge lines is the i-frame rule. Strong candidates: `f14c`
  (0 = 78A7F0 skips the damage calc), `1e0` / `1e4`, `stat48`, `rec0 bit35`, `rec18` bit 29 with `ae0`.
