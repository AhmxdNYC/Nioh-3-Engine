# Training completeness checklist (user #22920, 2026-10-22)
Goal: player never feels an option is missing vs SF6-style training.
## Stage / environment (mostly NOT built)
- Pick stage/arena: Boss Matches loads a mission (slow, 3-37 s). Wanted: "Stage" picker in Training (all 18 boss arenas + open world), seamless if arena swap contract allows (notes/training-arena-switch-contract.md), else via rematch load.
- Time of day / weather toggle (if game exposes it) - unproven, research.
- Boss pick (which boss in which arena), boss phase (1/2/3) picker, boss HP preset (100/75/50/25%).
- Reset: fight, player position, save/restore position (checkpoint slot).
- Toggle arena hazards / gates / spectators (unproven).
## Boss behavior
- Hold Ki break (in progress #22918), boss stance (idle/aggro/random), repeat move, sequences, speed, Ki drain, infinite HP/Ki.
- Boss block/guard on/off, boss no-attack-until-hit, boss unblockable-only filter.
## Player
- Infinite HP/Ki/ninjutsu (done), speed (done), damage mult (done), restore consumables/amrita/spirit gauge, arts gauge, Ki pulse, loadout/weapon swap hot key.
## Feedback
- Boss + player frame meter (done), freeze/frame-step (built), damage + combo counter, hitbox display, input display (controller overlay), record/replay.
Priority next: Stage picker, boss phase/HP preset, position save/load, damage/combo counter.
## Toggle -> adjustable (user #22921)
- Hold Ki break: toggle + hold duration slider (1 s .. forever); one-hit Ki break: Ki damage multiplier slider (x1..x10..instant)
- Infinite HP/Ki/ninjutsu: keep toggle + regen-rate slider (0 = off, per-second %); player/boss HP set % slider
- Boss Ki drain per s (exists), boss Ki regen rate, boss aggression/reaction delay, repeat-move gap (exists)
- Speeds (exist), freeze: frame-step count N, slow-motion % slider
- Damage multipliers (exist); boss guard chance %; boss attack-frequency slider; stage time-of-day slider if exposed
## Time of day / Sky (user #22942/#22943) - feasibility lead
- notes/renderer/09-volumetrics.md: time of day enters render via scene+2CC (seconds) and scene+B2EC; sun/moon directions (SkySunMoonDirections 5EBD08), fog, clouds, sky ambient all read it. => a Time-of-day slider = write scene+2CC each frame. UNPROVEN in game.
- Sky: cloud/pattern params in NatureSkyUpdatePatternParams 5F1628, sky ambient change fires on RS+96C. Sky picker (clear/cloudy/night) = needs cloud param discovery.
- Step 1: read scene+2CC live (log), step 2: freeze/write it, step 3: slider 0-24h, step 4: sky/cloud presets.
## Boss availability (user #22984, from notes/rematch-all-bosses.md, static data, 2026-10-22)
- 96 bosses total: 59 route R (replay mission), 8 D (master duels, field), 3 H variants, 29 X (no route: 19 field-only, 7 no boss record, 3 no data); 20 E0 are Hundred Demon wave battles, not named bosses.
- Boss Matches gate = MissionClearedGetSlot(702B64)==1 on the current journey => a 100% save passes the gate for all R/H/D rows without replaying. Only 1 boss (Jakotsu-baba) is log-confirmed; 59 marked verified=1 in the tsv, not all in-game proven.
- 29 X bosses cannot be started by the mod at all, even on a 100% save (field only / no data) unless summon-in-place works for them.
