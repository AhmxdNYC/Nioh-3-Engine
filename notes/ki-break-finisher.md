# Boss Ki-break finisher (grapple) detection (user #7290, 2026-10-15)

Branch `pc/ki-break-detect` (from `pc/kifade-fix` 87ec587). Static RE on `re/Nioh3.dump.exe` v2.02 (RVA == raw offset) via `re/decomp.cmd`; passive detection added to Nioh3Training. Game NOT launched, nothing installed (main PC was busy with another job's boss fights).
Builds on `notes/boss-ki-bar-re-2026-10-15.md` (branch pc/boss-ki-re-colors: Ki struct, KiDamage, Ki-break state machine) and `notes/summon-death-toggles-2026-10-15.md` (death blow, sensor 4892A8, cond 0x27).
Confidence: H = read in decompiled code, M = strong inference, L = guess.

## 1. What the finisher is (H for the names, M for the mapping)
- The game has one damage class "Final Blow/Grapple" (gear effects 0x608E/0x7DA6/... "Final Blow/Grapple Damage", "Life Drain (Final Blow/Grapple)", "Archspirit (Final Blow/Grapple)", "Finishing Flair: ... when performing a Grapple or Final Blow, or a melee attack against an enemy who is out of Ki"; `re/symbols/amrita/gear_effects.tsv`). Out-of-Ki enemies are "Winded" ("Melee Damage vs. Winded Enemy").
- Ki-break finisher = **Grapple**: boss Ki runs out -> Ki broken (winded) stun -> player presses attack next to it -> grapple animation that holds the boss and lands one big hit. The death blow at the HP floor is the same grapple/final-blow mechanic at HP 1 (M).

## 2. States and RVAs
`ai = [actor+0x3B0]`, `ctrl = [ai+8]`; `StateTest 0x4893B8(ai, id)` -> ctrl vtable +0xC0; the default implementation **0x2E9948** is a big switch (not a plain bitset). Runtime state bits live at **ctrl+0x1060** (set/clear by `BitSet 0xFC1A20(ctrl+0x1060, bit, on)`; bits 0..0x76).

| id | meaning | how it is computed (0x2E9948) | conf |
|---|---|---|---|
| 0x23 | Ki broken / winded stun (boss gauge "broken" flash, IsKiBroken 0xA6A34) | `ctrl+0xA8` bit 41 **or** `ctrl+0x1060` bit **0x3A** | H (bits), M (meaning) |
| 0x35 | finisher/down action on the victim | current action data `[[ctrl+0xE8]+0x20]` dword +0x18 bit 8 | H (bits), M |
| 0xB0 | same flag on the linked action `[[ctrl+0x118]+0x20]` | dword +0x18 bit 8 | H (bits), M |
| 0x2C | runtime bit **0x48** (`ctrl+0x1068` bit 8) | | H (bits), M |
| 0x27 | "needs death blow" (HP floor revive cond; see summon-death-toggles) | `2A3AC0(ctrl)` or `ctrl+0xA8` bit 45 | H |

- Death sensor **0x4892A8**(actor) = `hp+0x48==1` or StateTest 0x35 / 0xB0 / 0x2C or `actor+0x8D4 > 0` (H). Live log 12:30: sensor 1 for ~2.3 s during the HP-1 finisher, so these three are the finisher (grapple) states on the victim.
- Hit apply **0x327DEC** (37 KB, caller 0x2A32E4): near its end (decomp: `6C5EA4(hit+0x190, 0x8000000000000000)`) an attack whose param flags (hit+0x190) have **bit 63** sets the target's runtime bit **0x3A** (= StateTest 0x23) and `ctrl+0x1014 = 0` (H). KiDamage 0x844604 is applied in the same function before the Ki-break transition check (`Ki exhausted` 0x6B3758 before/after) (H).
- When the hit is lethal and cond 0x27 is true: sets `0x400000000000` (bit 46) via 0x9050F4 on the ctrl and checks StateTest 0x2C (already in the finisher) (H) - the death-blow path.
- Ki exhausted (KiIsExhausted 0x844FE4): `K+0x5C>0 || K+0x74>0 || K+0x7C>0 || K+0x83`, K = `[actor+0x3A0]+0x98`; Ki cur/max = status +0xAC / +0xB0 (H, from boss-ki-bar-re).
- Ki-break state machine 0x844674 (per character, from 0x4F54C4): sets runtime bits 0x18/0x19/0x1A (break start) / 0x28 (break cleared) / 0x39; on break start for the player-side (`actor+0xAB84 == 0`) plays sounds 0x797D / 0x2659 (H for code, M for meaning).
- Script natives (name -> function): `Character::ForceKneelDown` 0x238E1BC (ai ctrl +0x994 = action 0x27, actor+0x91B = 1), `Character::IsKneelDownFlag` 0x2391458 (`[w+0x18]+0x9C` bit 13), `Character::IsDeadDownLoop` 0x238FEDC (StateTest 0x127), `Groupthink::IsTargetDownState` 0x2E2A94. Native table entry = {name ptr, fn ptr} (fn at name-entry + 8).
- Not found: no script/native name "Grapple"; `Collision::Damage::Hit::IsAttackSekiBreak` 0x23C7C1C returns constant 0 (gadget side). The attack-param flag that marks a grapple attack itself was not identified (candidates: hit+0x190 flags tested in 327DEC with masks 1, 2, 0x10000000000, bit 63).

## 3. Detection added (Nioh3Training, passive, log only)
`KiBreakTick()` in `mod/Nioh3Training/src/training.c`, called every pump frame before BossTick; ini `[Summon] KiBreakLog = 1` (default on, 0 = off). Never writes boss Ki, HP or state; it only reads memory and calls the read-only StateTest 0x4893B8 (the same call path 4892A8 already uses). Max 400 log lines per run.
- Watched actors: every boss with an HP/Ki bar (gauge rows `[[[B+0x45C84A8]+0xA8]+0x3C0..0x3C8]`, 0x60 B, slot < 3, id at +4 -> FindWrapper -> actor; covers native mission bosses, Boss Rematch and Training summons via the fake-id hook) plus the boss-mode actor.
- Log lines:
  - `KIBREAK watch boss ...` first sighting.
  - `KIBREAK start #n boss ...: Ki cur/max HP ... (state23 x exhausted y [5C 74 7C 83]) bits1060 / 1068 / A8` - Ki ran out.
  - `KIFINISHER state on boss ...: 35= B0= 2C=, HP, Ki, <classification>` with classification `HP floor (death blow)`, `Ki broken -> GRAPPLE (Ki-break finisher)` or `not Ki broken (down/other finisher)`; `KIFINISHER state off ... after N ms`.
  - `KIFINISHER hit boss ...: <kind> dmg N (x % max) HP a -> b` for every HP drop during a break / finisher state (+1 s grace); kind = `KI-BREAK FINISHER (grapple)`, `hit on Ki-broken boss`, `finisher-state hit`, `FINAL BLOW (HP floor)`.
  - `KIBREAK end #n ... after N ms: hits, dmg during the break, grapple yes/no`.
- Build: `mod/Nioh3Training/build.sh` (zig cc) compiles clean; test ASI only at C:\tmp\kib\Nioh3Training.asi (not installed).

## 4. Needs an in-game test
1. Any boss fight (native mission, Rematch or Training summon): break the boss's Ki and grapple it. Expect `KIBREAK start`, then `KIFINISHER state on ... GRAPPLE`, one `KIFINISHER hit ... KI-BREAK FINISHER (grapple)` with a large dmg, `state off`, `KIBREAK end ... grapple yes`.
2. Confirm which of 35 / B0 / 2C is on during the grapple (vs the HP-1 death blow) and whether StateTest 0x23 or only the exhausted fields flip at the break (the raw bit words are logged to pin this down).
3. Break the Ki and do NOT grapple: expect `hit on Ki-broken boss` lines only and `grapple no`.
4. Check no gameplay change (Ki/HP behaviour identical with KiBreakLog 0 vs 1) and no log flood (cap 400).
5. If the finisher states never turn on at HP > 1, the grapple is identified only by the hit timing inside the break; next step would be a read-only hook on 327DEC to log the hit's attack flags (hit+0x190).
