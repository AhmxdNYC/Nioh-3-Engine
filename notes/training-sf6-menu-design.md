# Training menu design: SF6-style items (pc2, 2026-10-17)

Design only, answering pc NOTE #12524 (user #12523). No source, build, game or save change. Each item gets a status from the existing notes, so the menu never shows a control whose behaviour is not proven:

- **Ready**: reuses a proven path, safe to build now.
- **Gated**: buildable as a UI item, but the action stays disabled until its named proof exists.
- **Unsupported**: no proven mechanism; show nothing or a greyed "not available" line.

Evidence: [integration design](training-mode-design.md), [spawn contract](training-spawn-contract.md), [progress](training-mode-progress.md), [boss movesets](boss-movesets.md).

## Menu items

| SF6 feature | Menu item | Status | Basis / what is missing |
|---|---|---|---|
| Idle stance (dummy does nothing) | `Dummy: Idle / Act` toggle, default Idle | **Ready** (proven on pc and pc2) | pc: 0 attacks in 31 s vs 17 (Training ASI #970). pc2 (Yoritomo R7B35, 2026-10-17): held 0 actions in 42 s and 23 s; AI on gave 25-36 actions per 60 s. Branch pc/arena-teleport-menu. |
| CPU stance | `CPU stance: Idle hold / Normal AI` | Ready (two states only) | Hold on/off proven. Guard/Attack stances are not proven. |
| Pick and toggle boss moves | Per-boss move list, each move `On / Off` | **Ready** (proven on pc and pc2) | pc: Shingen off=0 uses/60 s, 12 other moves continue, on=5. pc2 (Yoritomo): 4 moves off (0x4E30/31/32/24) = 0 uses in 75 s with 17 other actions, vs 4 and 9 uses when on (60 s each); 199 select vetoes. Table covers 11 bosses (`mod/boss_moves.h`); other bosses need live logging. |
| Spawn several bosses at once | `Add boss` (up to N) | Unsupported | No proven summon: an activation can be accepted before the actor exists, and asset readiness is unknown. Keep it hidden until the spawn contract passes. |
| Despawn all bosses | `Clear bosses` | Unsupported | No callable despawn. Mission reload is the only cleanup. Do not free actors or clear flags by hand. |
| Reset to start | `Reset` | Ready (slow) | Native mission reload using the existing retry path. Measured: first load about 33 s, retry 3-6 s (open.md). Show the load time on screen. |
| Rewind game time | `Rewind` | Unsupported | No state snapshot. Do not show it. |
| Repeatable drills | `Record / Play drill` (boss action ids + timing) | Gated | Recording is possible: the F9 log already records boss action ids and state. Replaying inputs or boss actions on command is unproven. |
| Clear on-screen feedback | Toggle state line, hit/damage counter, drill timer, reset load timer | Ready | Plain HUD text from the mod overlay. Low risk, no game writes. |
| Arena teleport (user #12439/#12444) | `Go to arena` (normal load, with the learned fast-load steps) | Ready (load path) | Native mission request path is proven: R7B35 fight start in 45.9 s, load 32.6 s (pc, 2026-10-14). Fast-load steps (short black screen, early cover release, in-place swap) are the separate load work. Live arena switch is out of scope for now. |

## Build order

1. Ready items first: arena teleport through the native path, `Reset`, HUD feedback.
2. Read-only move list for all bosses in the menu (data from `boss-movesets.md`).
3. Gated items only after their proof: AI hold (Idle/CPU stance), then one move block, then drills.
4. Multi-boss add/clear last, after the spawn contract passes.

## Not in this note

- Live arena switching: deferred (#12439).
- Reward-free practice: no proof, not offered (see `training-mode-design.md`, persistence section).

## pc2 live result (2026-10-17, device pc2, Ryzen 5 5600X + RX 6650 XT, Yoritomo R7B35, god mode on)

- Build: pc/arena-teleport-menu 8442bfc plus pc2 test command (branch pc2/move-toggle-test-pc2).
- Idle hold: held at action 0x2 from the moment the fight started; 0 actions in 42 s (run 1) and 23 s (run 3). With hold off the boss did 25 and 36 actions per 60 s.
- Move toggle (per-boss pref, same path as the menu): 4 ids off = 0 uses in 75 s, 17 other actions continued; on = 4 and 9 uses per 60 s. Vetoes fall back to idle 0x2 as designed.
- Caution: the legacy pipe command `moveblock` is overridden by explicit per-boss ON prefs, so it blocks nothing when the move list was staged all-ON. Test with `moveset <id> <0|1>` (new) or ini `[Moves.<chr>]`.
- Not proven on pc2: the VirtualPad menu navigation (the runner's fixed row indices are stale after the menu grew, no MOVEMENU ack). The toggle path itself is proven.
- Side effect seen: `nodeath` holds HP at 1 but Training still counts the player as beaten and resets the round every 20-60 s; windows above span resets.
