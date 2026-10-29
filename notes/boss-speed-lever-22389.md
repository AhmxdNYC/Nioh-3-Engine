# Boss-only speed lever and ninjutsu gauge (#22389), offline static RE

Source: `re/Nioh3.dump.exe` (file offset = RVA). The game was not run and nothing was installed. All addresses are RVAs.
Names: `a` = actor (the mod's `BvActor` / player pointer). `st = [a+0x3A0]`. `ai = [a+0x3B0]`. `ctrl = [ai+8]` (the mod's existing convention, training.c 3717/3855).
Verdict: **a per-character motion play-rate exists, and the game computes it in one function, `ComputeMotionSpeed 0x134E68`. Hooking it gives a boss-only speed multiplier.** The code paths are proven by disassembly. How it looks in game is not tested.

## 1. How the game scales one character's animation

### Motion object and its speed fields (PROVEN, disasm)
- Motion object `mo = [a+0x40]`. The per-entity update `0x2086E8(entity, timeInfo)` calls `MotionUpdate 0x4831F0(mo, dt = [timeInfo+4])` at `0x209288`/`0x209290`. Every entity gets the same `dt` (`0x1322F90` loops over all objects with one shared `timeInfo`), so `dt` is not per character.
- `0x4831F0` calls `0x93B66C` for the main layer: `mo+0x290 (frame) += dt * mo+0x324 (speed)`. End frame is `+0x320`, loop flag `+0x339`.
- There are 4 play-rate floats on `mo`, one for each motion player inside it: `+0x324` (main body, player at +0x288), `+0xBA8` (player at +0xB38), `+0x10C4` (+0x1058), `+0x1170` (+0x10D8). Sub-layers `n>0`: `mo + 0xDE8 + (n-1)*0x90`.
  The constructor `0x9F664` sets them to 1.0 (`9F74D`, `9FA64`).
- `GetMotionSpeed 0x484488(mo, layer dl)` and `SetMotionSpeed 0x48470C(mo, ?, layer r8b, xmm1 speed)`. Layer 0 writes all 4 fields.
- Script binding `Motion::SetSpeed` (`0x3BF07B8`, slot `0x397A090`) -> `0x23E40E4` writes the same 4 fields. This is the "per-move play rate" the action scripts use.

### Where the speed value comes from: `ComputeMotionSpeed 0x134E68(ctrl, dl layer, r8b flag) -> xmm0` (PROVEN, disasm)
It is called by 18 sites, for example the motion start/play paths `0x66C814`, `0x66D6EC`, `0x66DC54`, and the layer refresh `0x20DAD4` (ctrl vtable 0x38FE698, called per layer from `0x2E8638` (vtable slot 6) and from `0x4812FC`).
The callers store the result in the per-layer cache `ctrl+0xDC4+layer*4` and write it to `[[ctrl+0xE0]+0x40]+0x324/...` (for example `66CB7C..66CBAC`, `20DF44..20DF66` -> `48470C`). So `[ctrl+0xE0]` is the actor and `[actor+0x40]` is its motion object.
Its result is the product of:
1. 1.0 (`0x3BD41E0`).
2. **Timed modifier slots** `ctrl+0x6B8 + i*8`, i = 0..18. Each points to a small object: `+8` time left (float, active while > 0), `+0xC` factor (float), `+0x10` flag. Vtables are at `0x38FEEF8`, `0x38FEF30`, `0x38FEF68` and `0x38FEFA0`: `+0x18` isActive = `0x13553C`, `+0x20` factor = `0x133AD0` etc., `+0x30` tick.
   Slots 0..5 always apply. Slots 6..11 and 13..18 apply only to the layer `i-6` / `i-13` when `r8b == 0`. Slot 12 applies only when `r8b != 0`. The objects are laid out in `0x74DC40`, ticked by `0x20FF70` (vtable slot 62), and reset by `0x4F1B48`.
   These are the engine's own slow/haste style multipliers. Which slot is which effect is NOT identified.
3. `0x13555C(ctrl)`: uses the status float **`st+0x3018`** (a per-character speed scale, default 1.0) when it is != 1.0 and `0x481290(ctrl)` is false. There is also a partner/link term through `[[a+0x3B0]+0x50]` (`0x136B4C` / `0x2017534`, which can overwrite `st+0x3018` at `1355D5`). Another writer is at `0x9DC1B1`.
4. `* [ [a+0x3B0] + 0x684 ]` while the current action has flag bit `0x0400000000000000`.
5. Layer 0 of a rider-type character (`0x866490`): copies the parent's `[[ [ctrl+0xAE0]+0x3B0 ]+8]+0xDC4`.
6. **Hit-stop: the result is forced to 0** when `ctrl+0x488/0x48C` (`0x1354F4`) or `ctrl+0x490/0x494` (`0x135518`) is active. GUESSED: these are the attacker and victim hit-stop timers.

So per-character slow, haste and hit-stop already run through this one function. The global `G+0x624` time scale / `0x46484` dt is a separate, global factor.

## 2. How a mod can make only the boss faster or slower

**Recommended: an entry hook on `0x134E68`** (prologue `48 8B C4 48 89 58 08 48 89 68 10`: `mov rax,rsp; mov [rax+8],rbx; mov [rax+0x10],rbp`).
- Stub: save `rcx`, then call the original (trampoline = the first 7 bytes `48 8B C4 48 89 58 08`, then `jmp 0x134E6F`). These 7 bytes have no RIP-relative operands, so they can be copied as is.
  After it returns, if `[rcx_saved+0xE0]` is a Training boss actor (`BvActor(s)`), do `mulss xmm0, mult`. Then `ret`.
  In C: `float H(u64 ctrl, u8 layer, u8 f){ float r = orig(ctrl, layer, f); if (IsBossActor(*(u64*)(ctrl+0xE0))) r *= g_bossSpeed; return r; }`.
- Effects: every motion the boss starts, and every layer refresh, gets `native * mult`. That covers attacks, movement (root motion), recoveries and grabs. Hit-stop stays 0 (0*mult). The player and other actors are untouched because of the actor filter.
  This is the same path the game's own slow/haste effects use, so it should also scale what keys off the cached `ctrl+0xDC4` speed. That part is GUESSED (see below).
- Latency: a change takes effect from the boss's next motion start or layer refresh, not mid-swing. For an instant effect, also call `SetMotionSpeed 0x48470C([a+0x40], 0, 0, GetMotionSpeed*mult)` once. That one call is untested.

**Fallback A (simplest per-frame): an entry hook on `MotionUpdate 0x4831F0(mo, xmm1 dt)`** (prologue `48 8B C4 48 89 58 08`).
If `rcx == [bossActor+0x40]`, then `xmm1 *= mult`. This scales every motion layer of the boss every frame, including the `+0x11A0`/`+0x1418` motion timers, and native speed resets cannot undo it.
Risk: action logic that counts its own time with `[timeInfo+4]` in `0x2086E8` (AI or action timers that are not motion frames) stays at x1. That could desync script events that are not keyed to motion frames. GUESSED.

**No-hook field write (weaker):** write `st+0x3018 = mult` for the boss each tick. `0x13555C` multiplies it in on the next `0x134E68` call.
Caveats: it is ignored while `0x481290(ctrl)` (action flag bit 29 plus a link at `ctrl+0xAE0`). The game itself writes it at `1355D5` / `9DC1B1` in partner/link cases, and a value of 1.0 is skipped. It acts at motion start only. It cannot be unit-tested offline, and the mod would have to restore 1.0 when the feature is turned off.

## 3. Proven vs guessed (boss speed)

| Claim | Status |
|---|---|
| `mo=[a+0x40]`, `frame += dt*speed(+0x324)` in `0x93B66C`, 4 speed fields, get/set `0x484488` / `0x48470C` | PROVEN (disasm) |
| `0x134E68` is the only source the play/refresh paths use for the speed they write (18 callers) | PROVEN for the callers seen. Other direct writers exist: `0x9B30C0` (1 caller), `0x28E3BFC` (0 direct callers), and the script binding `0x23E40E4`. A per-move `Motion::SetSpeed` from a script would bypass the hook for that move. |
| ctrl -> actor `[ctrl+0xE0]`, actor -> ctrl `[[a+0x3B0]+8]` | PROVEN by usage (134FA7, 13502E, 5FADA6) plus the mod's own convention |
| Scaling the motion speed also scales the attack active frames and hitboxes | GUESSED (Katana action events are frame-keyed; no live test) |
| `ctrl+0x488..0x494` are hit-stop timers; modifier slot meanings | GUESSED |
| AI decision timers do not scale | GUESSED (they look time-based) |

**Confidence: medium-high** that the hook on `0x134E68` (with the actor filter) speeds up or slows down only the boss's animations. Medium that attack timing follows exactly. Live check: Practice, set the boss to x0.5 and x1.5, and log every `0x134E68` return for the boss (layer, native, scaled) once per motion. Watch a combo and a parry window.

## 4. Ninjutsu gauge (goal 2), PROVEN by disasm of the game's own script binding

- `Player::FullNinjutsuGauge` (`0x3BF9FA8`, slot `0x397EE60`) -> `0x23A8354`: `p = GetPlayer 0x20A914(0)` (`[[0x4745348] + (0xCD+i)*0x18]`), `st = [p+0x3A0]`.
  Then for `k = 0..2`: `g = 0x5FE49C(st+0x3688, k) = st + 0x3698 + k*0x30`, and `g+0x14 = g+0x18`.
- So the **current value is the float `st+0x36AC + k*0x30` and the max is `st+0x36B0 + k*0x30`**, k = 0..2. `IsNinjutsuGaugeFull 0x23AA238` checks `cur >= max` (comiss) for one k given by the script arg.
  `EmptyNinjutsuGauge 0x23A7F14` zeroes both and then re-reads a param (`0x608508`, id `0xBCCC`).
- Infinite ninjutsu: each tick, for k = 0..2, if `0 < max` then set `cur = max`. This is exactly what the game's `FullNinjutsuGauge` does, with no other side effects (unlike `RecoverFull 54AFBC`).
- GUESSED: what the 3 entries are (three ninjutsu slots/charges, or ninjutsu plus onmyo). Onmyo may be one of them or a separate gauge, which was not found. Live check: log the 3 cur/max pairs, then cast one ninjutsu and one onmyo spell.
- Other bindings in the same table (`0x397E770`): `Player::FillAmritaGauge`, `FullSamuraiReinforceGauge`, `SetNiohTenshinGaugeReduceSpeed`.
