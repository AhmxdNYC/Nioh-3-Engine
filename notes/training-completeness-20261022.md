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
