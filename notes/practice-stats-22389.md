# Practice stats (#22389): levers, evidence, status

Branch `pc/practice-stats-22389` (from `pc/hitmenu-22349`). Offline work only: static RE of `re/Nioh3.dump.exe` (file offset = RVA), no game run.
Nothing here has been tested in game yet.

Code: `mod/Nioh3Training/src/practice_stats.h` (pure decisions, `tests/practice_stats_test.c`), `practice_stats_game.h` (game glue, exports),
`PstDmgScale` / `HpDmgHookInstall` in `training.c`; menu `mod/Nioh3BossRematch/src/practice_stats_menu.h`.
Ini: `Nioh3Training.ini [Practice]` `PlayerInfiniteKi`, `DamageDealtPct`, `DamageTakenPct`, `BossKiDrainPct`. All default OFF / 100 %.
Exports: `Nioh3Training_PracticeStatGet(what)` and `Nioh3Training_PracticeStatSet(what, value, persist)`, with what = 0 inf Ki, 1 dealt, 2 taken, 3 drain.
Everything acts only while a Practice session runs (`g_tr.active`) or a Training boss is alive (`BvActor`).

## Status per feature

| # | Feature | Status | Lever |
|---|---------|--------|-------|
| 1 | Boss speed multiplier | **Not available yet** (no menu row) | No per-character time or animation rate found offline. See leads below. |
| 2a | Infinite boss Ki | **Already existed** (#14320, per boss) | Boss X > Health and Ki > Boss infinite Ki and Set boss Ki % |
| 2b | Boss Ki drain | **Implemented** (all Training bosses, one value) | `KiSetCur 2F88D0(st+0x98, v)` at 20 Hz, floor 0.5 |
| 3a | Infinite player Ki | **Implemented, field GUESSED** | `[[player+0x3A0]+0xAC/+0xB0]`, `KiSetCur 2F88D0` |
| 3b | Infinite ninjutsu / onmyo | **Not available yet** (no menu row) | Leads below |
| 4 | Damage dealt / taken multipliers | **Implemented** | Scale `edx` of HP damage `787268` at hit-apply call site `329890` |

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
