# Practice stats (#22389): levers, evidence, status

Branch `pc/practice-stats-22389` (from `pc/hitmenu-22349`). Offline work only: static RE of `re/Nioh3.dump.exe` (file offset = RVA), no game run.
Nothing here has been tested in game yet.

Code: `mod/Nioh3Training/src/practice_stats.h` (pure decisions, `tests/practice_stats_test.c`), `practice_stats_game.h` (game glue, exports),
`PstDmgScale` / `HpDmgHookInstall` in `training.c`; menu `mod/Nioh3BossRematch/src/practice_stats_menu.h`.
Ini: `Nioh3Training.ini [Practice]` `PlayerInfiniteKi`, `DamageDealtPct`, `DamageTakenPct`, `BossKiDrainPerSec`, `PlayerInfiniteNinjutsu`, `BossSpeedPct`, `PlayerSpeedPct`. All default OFF / 100 %.
Exports: `Nioh3Training_PracticeStatGet(what)` and `Nioh3Training_PracticeStatSet(what, value, persist)`, with what = 0 inf Ki, 1 dealt, 2 taken, 3 drain.
Everything acts only while a Practice session runs (`g_tr.active`) or a Training boss is alive (`BvActor`).

## Status per feature

| # | Feature | Status | Lever |
|---|---------|--------|-------|
| 1 | Boss / player speed multiplier | **Implemented** (#22389 / #22390) | Entry hook `ComputeMotionSpeed 0x134E68`, `notes/boss-speed-lever-22389.md` |
| 2a | Infinite boss Ki | **Already existed** (#14320, per boss) | Boss X > Health and Ki > Boss infinite Ki and Set boss Ki % (same page as 2b; harness checks the row order) |
| 2b | Boss Ki drain | **Implemented** (Boss X health and Ki page, all Training bosses, one value) | Ki lost per second (`BossKiDrainPerSec`, 0 = off, steps 5 .. 1000), `KiSetCur 2F88D0(st+0x98, v)` at 20 Hz, floor 0.5 |
| 3a | Infinite player Ki | **Implemented, field PROVEN** | `st+0xAC = st+0xB0`, the body of `Player::RecoverStamina` (binding `23AACA4` -> `5FB5C8(st+0x98)`) |
| 3b | Infinite ninjutsu | **Implemented** | `st+0x36AC+k*0x30 = +0x36B0`, `Player::FullNinjutsuGauge 23A8354` |
| 4 | Damage I deal (x0.1 .. x10, Player page) / Damage boss deals (x0 .. x10, Boss page) | **Implemented, by attacker** | Scale `edx` of HP damage `787268` at `329890` in hit apply `327DEC`; attacker = `325F40(hit)` (hit = r13, passed in r9 by the stub) |

## Evidence

| Item | PROVEN (code or live log) | GUESSED / unverified |
|------|---------------------------|----------------------|
| Status struct `st = [actor+0x3A0]`; HP `+0x58`, max `+0x50` | Player: PlayerRefill / PlayerHp. Bosses: BossVitals logs. | |
| Ki `st+0xAC` cur, `+0xB0` max (floats); setter `2F88D0(st+0x98, v)`; exhausted test `844FE4(st+0x98)` | Bosses: BvWriteKi, NfTick and KiBreakBareFix write it live, and the KIBREAK logs read it. `2F88D0` reads its object `+0x14` (= st+0xAC) and `+0x18` (= st+0xB0) (disasm). | Player: assumed to be the same status class, because the HP fields are shared. No player Ki read is logged yet. The code checks the read (0 < max <= 100000) and logs `PRACTICE infinite Ki: player Ki read c / m` once, so the first live run proves or disproves it. After 3 failed write-backs it switches itself off. |
| Hit HP damage `787268(hp = st+0x38, int dmg, keepAlive, 0)` called at `329890` inside hit apply `327DEC` (`r8b = [r13+0x28D]`, `edx = cvttss2si xmm15`) | NOFIN hook on the same site. The live log `NOFIN HP damage boss slot` is the player's hits on bosses. Byte check `45 8A 85 8D 02 00 00` at site-22. | Hits on the player go through the same site: guessed. 327DEC is the generic hit apply, but no player-victim log exists yet. The first 30 scaled hits are logged as `PRACTICE damage xN: player deals / takes a -> b`. |
| Damage direction | | Victim == player status means "taken". Any other victim means "dealt". The attacker is not checked: an ally's or an arena NPC's hits on a boss count as "dealt". |
| Coverage of the multiplier | | The other 7 callers of `787268` are not scaled. They are 0x1FF44FF (builder), 0xAFF1D / 0xAFFCE / 0x21FAECB (clear HP), 0xE43B9A (HP minus a scaled stat, possibly DoT or status), 0x21F6540 (adds HP), and 0x786ED8 (#21570). So status, elemental or DoT damage and some projectile or ninjutsu paths may stay x1. Ki damage is never scaled (it is the KiDamage `844604` path). |
| Boss Ki drain vs native regen | | The boss's native Ki regen fights the drain. Low % values may look like "no effect" while the boss regenerates. Needs a live look. |

## Leads: boss speed (feature 1)

- **Follow-up: `notes/boss-speed-lever-22389.md`.** The per-character play rate comes from `ComputeMotionSpeed 0x134E68(ctrl)`. An entry hook that does `*= mult` when `[ctrl+0xE0]` is the boss gives a boss-only speed lever. The same note has the ninjutsu gauge: `st+0x36AC+k*0x30` cur / `+0x36B0` max, k = 0..2.

- `G+0x624` is the global game time scale (`DbgTimeScale`, `[0x4759910]+0x624`). Getters: `0x42DDE4` (raw), `0x434DF4` (scaled), and `0x46484` (dt = scale * k / fps, 65 callers). Changing it is global: the player slows too. Not used.
- `Position::AnimeSpeed` / `Position::GetLocalAnimeSpeed` / `Rotation::AnimeSpeed` binding names (`0x3BEFA50` ..., table `0x39794F0`, used by `0x94F4C` / `0x34DB38`). These are root-motion read-outs, not a play-rate setter. GUESSED, not followed.
- `MissionScriptFunction` has no Character time-scale or speed setter (full list: `Character_*` in strings.tsv).
- Next step (live or deeper static): find the per-actor motion update that consumes `0x46484` dt, then look for a per-character multiplier, for example a hit-stop or slow field read beside it (actor `+0x1800` at `0x5FAEEC` is a countdown timer, not a rate). A Sloth or slow-type debuff, if Nioh 3 has one, would expose it.
- The per-move `sched` 'r' / play-rate % in `notes/yoritomo-moves-unblockable-22305.md` is file data. A per-move play rate could be patched in the loaded action table, but only for catalogued moves. GUESSED.

## Leads: ninjutsu / onmyo (feature 3b)

- Strings: `Online::PlayerNinjutsu` (`0x3A72C80`, via `0x3C481A8` -> table `0x3C47B70`, functions `0x282CB8C`, `0x28301F4`, `0x28306A4`, `0xADC888`), `MENU_ID_NINJUTSU_HUD`, `Player_EquipNinjutsu`, `BehaviorNinjutsuUseNode`, `MENU_ID_ONMYOUJUTSU_*`. The use-count or points field was not identified. Not followed.
- `Player::RecoverFull` (VM `23AAB04` -> `54AFBC` + `204E038`) refills everything at a shrine. The test-only `player_protection.h` already calls it every tick. It probably refills ninjutsu and onmyo uses too, but it also resets other state (`6028A4` rewrites dozens of actor fields), so it was not used per tick. Live check: count the ninjutsu uses before and after one RecoverFull.

## Live checks to run (game free)

1. Practice session, Player page: turn Infinite Ki ON. The log should show `player Ki read` with a plausible max (a few hundred) and `infinite Ki applied`. Sprint or dodge: the Ki bar stays full.
2. Set Damage you deal to x2. Hit the boss: `PRACTICE damage x2.00: player deals a -> 2a`, and the boss HP bar drops twice as fast. Set Damage you take to x0: `player takes a -> 0`.
3. Boss page: set Boss Ki drain to 10 %/s. Log `PRACTICE Ki drain Boss A`. The Ki bar sinks to about 0 and stays one hit from a break. Turn infinite Ki ON on the same boss: the drain stops.

## Update: attacker side, player Ki proof, Ki drain units (offline static RE, not run in game)

- Player Ki: `Player::RecoverStamina` (string `0x3BF9BD8`, slot `0x397EA60` -> `0x23AACA4`): `GetPlayer 20A914(0)`, `st = [p+0x3A0]`, `5FB5C8(st+0x98)`:
  `if (+0x14 < +0x18) +0x14 = +0x18`. So the player Ki is the same `st+0xAC` cur / `st+0xB0` max as the bosses. Infinite Ki now does this raw write (no KiSetCur).
- Hit apply `327DEC(hit)`: `r13 = hit` from `327E87` to the end. Victim = `[hit+0x1C8]` (`327DD4`); the HP object is `[[hit+0x1C8]+0x3A0]+0x38`
  (`328399..328422` -> `rbp-0x10` -> `r12` -> `rcx` at `329889`). `3283D8..3283FB` compares the victim with GetPlayer `[[0x4745348]+0x1338]`.
  Attacker = `325F40(hit)`: `[hit+0x198]` resolved to a character (type 0 itself; type 4 -> `+0x260` owner, type 3 -> `+0x1E0`; else `588700` = `[[x+0x240]+0x690]`), pure reads.
  327DEC calls it 36 times, the first right after the HP damage (`3298EF`). `edx` at `329884` (`cvttss2si edx, xmm15`) is the final HP damage.
- Hook: the existing `329890` call stub now starts with `mov r9, r13` (the site sets `r9d = 0` and `787268` gets `0` from the detour). Byte checks: 18 bytes at `329890-22`, 12 bytes at `325F40`.
  Attacker == player -> DamageDealtPct; attacker == a Training boss (`BvActor`) -> DamageTakenPct; anything else (allies, NPCs, the player's own self-damage) x1.
- GUESSED / unproven in game: the player's projectiles / ninjutsu resolve to the player through `325F40`; a summoned spirit's hits are not the player's
  (its owner link `ctrl+0xFF0` / `684A34` is not followed); the 7 other `787268` callers (DoT, status, scripts) stay x1.

## Freeze + frame advance (#22589, live on pc 2026-10-21)
Code: `mod/Nioh3Training/src/practice_freeze.h` (pure, `tests/practice_freeze_test.c` simulates the counter + edge latch), `practice_freeze_game.h`; menu rows
'Freeze fight' / 'Step 1 frame' on the Practice hub (`practice_stats_menu.h`). Keys `[Practice] FreezeKey` (default 120 = F9; in Practice it drives this freeze
instead of the debug freeze) and `StepKey` (default 117 = F6, hold = repeat after 350 ms, every 80 ms). Default OFF, never saved.
- Hold = the F9 mechanism (stop counter #3 G+0x59C, own CAS flag). The latch 89B6EC is edge-triggered (flag set on counter 0 -> non-0, cleared on non-0 -> 0;
  disasm), so a step clears G+0x58B right after the latch at its FrameDtSetup call 5AD64A (chained E8) and restores it to `counter != 0` after the step.
  That frame runs at the normal time scale; it also steps through the menu pause (same counter). Never steps through the native pause G+0x58C.
- One step = until the frame meter clock reaches the next cell (1/60 s of game time). Live (VRR, fps-for-dt 86-102): every press +1 cell, 1 or 2 engine ticks,
  10.3-24 ms of game time each (the engine tick is variable). Below 60 fps one tick covers 2 cells and the step reports +2.
- Live results: 23 / 60 / 3 steps logged `+1` each (`PRACTICE STEP #n` lines), world static between presses; after resume Yoritomo 0x4E6E finished with
  startup 94, active 93-105 / 140-156, recovery 176, total 332 (= unfrozen VRR numbers). Freeze + menu open + menu steps + menu close: still frozen.
  `kill last` while frozen -> released at once ("boss died"); `trainstop` while frozen -> released ("session ended"). Also released on load / transition
  (MenuPauseBusy), boss actor change, player change, 20 min, pump stalled 2 s (watchdog thread), Return.
- Not tested live: release on a real mission change (covered by the load / busy check), [Debug] Hotkeys = 0.

## Reset fight boss Ki 'WRITE DID NOT STICK' (fixed, live 2026-10-21)
KiSetCur 2F88D0 and the native regen stop at `max - [st+0x120]` (a Ki ceiling loss that the player's hits add to; 0 at spawn). BvWriteKi now clears it
when the target is above that ceiling; live read back 1664 / 1664 after Reset fight. Details: notes/frame-meter-second-boss-22356.md.

## Ki damage I deal + One-hit Ki break (#22588, offline static RE, not run in game)
- `[Practice] KiDamageDealtPct` (0..1000, default 100; Player page row 'Ki damage I deal', after 'Damage I deal') and `OneHitKiBreak` (0/1, default 0; Boss X page row after 'Boss Ki drain').
- Lever: KiDamage `844604(rcx = K = st+0x98, xmm1 = amount)` = `KiSetCur 2F88D0(K, [K+0x14] - amount)` unless KiDamageImmune `2F8884`. Its only call in the hit apply `327DEC` is **`329E9D`** (capstone scan of `re/Nioh3.dump.exe`; the other callers `9C585C` / `1FF4454` are not hooked).
  Around it: `329E31` bl = `2A3758(K)` (Ki <= 0) before the hit; `329F25..329F46` if !bl and Ki <= 0 after -> the native Ki-break transition.
- Hook: E8 at `329E9D` -> stub `mov r8, r13 (hit); jmp PstKiDamage(K, amount, hit)`; byte check 20 bytes before the call + the `325F40` attacker getter. Installed on first use (value != default).
- Scaling: attacker = `325F40(hit)` == player and victim != player -> amount x pct / 100 (positive amounts only).
- One-hit: victim is a Training boss (BvActor), not dead / exhausted (`844FE4`) / state 0x23 / HP-0 window / its infinite Ki ON, Ki > 0: amount = cur + max(scaled, 1), so the same hit crosses 0 = native break. A hit that already empties the bar is untouched.
- Unproven in game: that a forced crossing gives the full native stagger every time (the NOFIN prime-0.5 path relies on the same crossing, live #9319), guard / parry hits (they may use another Ki path), ninjutsu / projectiles' attacker resolution, the Ki-immune windows (super armor) where KiDamage no-ops. Log: `hook Ki damage 329E9D: on`, `PRACTICE Ki damage: ... ONE-HIT Ki BREAK` (first 30).
