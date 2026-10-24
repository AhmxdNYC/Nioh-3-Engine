# Training / Practice features (2026-10-21)
## Working (tested in game)
- Practice menu: Train here (open world), Training Bosses, Summon a boss here, Boss Matches; Return object opens our menu only
- Summon any number of bosses (A, B, C...), each idle by default, own Boss X page (stance, aggro, reset position, HP/Ki, moves, sequences)
- Pick a move to repeat with gap between repeats; Sequences (full chains, Yoritomo); moves grouped (attacks / twitches / unsorted / Ki-break)
- CPU/idle timer slider for all bosses; boss aggro (walks up, also on Stand still)
- Player can't die (infinite HP) toggle; death = instant respawn in arena with settings kept
- Infinite player Ki; infinite player ninjutsu (slot 1 verified, slots 2-3 not seen); boss Ki drain per second; infinite boss HP/Ki
- Damage you deal (x0.1..x10) and Damage boss deals (x0..x10) (verified x2 and x0)
- Boss speed and Player speed 10-300% (applied; feel not timed)
## Built / data found, being finished
- Frame data (startup / hits / recovery, any boss, log-only probe so far): 0x4E6E startup 95, hits 94-106 + 141-157, recovery 177; fps-independence still to verify at 30/120 fps
- Moves grouped by hit count (1-hit, 2-hit...) and Unblockable (flag bit 0x2000 at record +4, 19 Yoritomo moves)
## Queued (you said go ahead)
- Instant reset: boss HP/Ki/phase + your position
- Freeze + frame-advance with frame data display
- Later: Street-Fighter-style frame meter overlay
## Not covered yet
- Summoned spirits / Ki damage / damage-over-time not scaled by damage multipliers
- Ideas from your list not started: Ki pulse recovery, spirit force, arts gauge, amrita, living artifact, consumables
