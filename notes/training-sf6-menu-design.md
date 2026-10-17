# Training menu design: SF6-style items (pc2, 2026-10-17)

Design only, answering pc NOTE #12524 (user #12523). No source, build, game or save change. Each item gets a status from the existing notes, so the menu never shows a control whose behaviour is not proven:

- **Ready**: reuses a proven path, safe to build now.
- **Gated**: buildable as a UI item, but the action stays disabled until its named proof exists.
- **Unsupported**: no proven mechanism; show nothing or a greyed "not available" line.

Evidence: [integration design](training-mode-design.md), [spawn contract](training-spawn-contract.md), [progress](training-mode-progress.md), [boss movesets](boss-movesets.md).

## Menu items

| SF6 feature | Menu item | Status | Basis / what is missing |
|---|---|---|---|
| Idle stance (dummy does nothing) | `Dummy: Idle / Act` toggle, default Idle | Gated | HP-0 tests froze boss AI with the F9 time-scale freeze, which also freezes the player and the world. Idle needs a boss-AI hold without time scale. Prove on one boss first. |
| CPU stance | `CPU stance: Stand / Guard / Attack` | Gated | Same AI hold as Idle. Stance selection is not proven. |
| Pick and toggle boss moves | Per-boss move list, each move `On / Off` | Gated | Move names and ids come from `notes/boss-movesets.md` (all bosses, from live logs). Blocking an action id before it starts is unproven. Ship the list as read-only first; toggles come after the block is proven on one move. |
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
