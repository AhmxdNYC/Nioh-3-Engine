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
