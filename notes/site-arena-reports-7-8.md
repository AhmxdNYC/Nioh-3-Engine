# Site reports 7 and 8: arena safety

Task: bridge #65065, base `gxl-pc/release-185`. Offline candidate only; pc/3 owns game verification. No install, game control, or publish performed by cx-pc.

## Report 7: inventory disappears

The generated RC1D9 row sent Tokugawa Kunimatsu to world DF, the tutorial. Its actual mission script imports `Item::ReserveAllItemAll`, `Item::ReserveAllItemEdo`, and `Item::ClearEquipment`. The generator previously accepted every shipped mission with a world and enemy records, without checking inventory-changing scripts. Nine dojo rows use that same world and also import inventory-reservation functionality.

Python generation and the startup C scanner now classify inventory sandbox scripts by those API capabilities, without boss IDs, mission IDs, or localized name lists. All missions in such a world are unsafe destinations. Their primary boss rows become summon-only: mission and character identity stay intact, while world, start, placement and coordinates are zeroed. The boss can be summoned where the player stands; the mod does not enter that tutorial/dojo arena.

Runtime also sanitizes older cached TSV rows, even with AutoArena disabled. Fight now consumes unsafe requests before fade/config/native fallback handling. A failed summon cannot fall through into a native mission load. Explicit cached summon-only mission rows retain that protection if scanning fails.

Exactly these 10 committed rows changed:

| Row | Boss |
|---|---|
| RC1D9 | Tokugawa Kunimatsu |
| RF5E5 | Yagyu Munenori (Novice) |
| R6E65 | Yagyu Munenori (Adept) |
| R1A5E | Yagyu Munenori (Veteran) |
| RD759 | Hattori Hanzo (Novice) |
| RE18C | Hattori Hanzo (Adept) |
| R9BE4 | Hattori Hanzo (Veteran) |
| R26F3 | Saito Fuku (Novice) |
| RA0A1 | Saito Fuku (Adept) |
| R2826 | Saito Fuku (Veteran) |

The Light of Hope and Ghosts at Hirakawa Gate Kunimatsu arena rows remain intact. The existing TSV retains its other rows and tested cell whitelists. Full regeneration also adds unrelated roster placements that predate this change; they are outside this patch.

## Report 8: two Ibaraki Doji

The TSV has exactly one Ibaraki row, REA75 (character 894462, native placement 364374). The mission script's arena-entry trigger references that native placement. `ArenaTick` previously unconditionally issued `CreateDynamic` after entering the arena and `StartArena` cleaned only mod-owned actors. That permits a native boss and a mod boss together; this is the identified code path, pending reproduction/verification by pc/3.

The new guard checks the selected arena's exact native placement wrapper, its placement/character record, current world, and live actor membership. It deletes that native duplicate before creating the mod boss, waits for deferred deletion, and checks for later native spawns through the arena session. It never deletes a player or an actor already registered as mod-owned. The selected native placement is excluded from dynamic owner selection/owner activation. Intentional Boss A/B summons are preserved. Stopping or starting a summon-here session clears the arena placement guard. Summon roster loading also deduplicates identical mission/character entries.

No Ibaraki data row changed and no boss-specific runtime list was introduced.

## Offline verification

- `py mod/Nioh3Training/tests/arena_safety_test.py`: 4 tests passed; actual installed game archives used; 131/131 fresh C/Python scanner rows agree. Confirms all 10 sandbox rows, both safe Kunimatsu variants, and exactly one Ibaraki row.
- `py mod/Nioh3Training/tests/arena_safety_runtime_test.py`: actual C guard bodies compiled and passed against mocked game state. Covers roster dedupe, scan failure/cache fallback, summon failure, existing/deferred/late native duplicate deletion, explicit slots, wrong world/character, player, and raw summon guards.
- Windows Training shared-library build uses the existing Zig flags; artifact is local and not installed. No performance comparison was run.

## pc/3 verification

1. Back up saves and check RC1D9 Fight now from a normal world: summon here, no tutorial travel or equipment change. Repeat with AutoArena disabled and the old TSV, and with arena swapping disabled.
2. Check a dojo row follows the same rule and both safe Kunimatsu variants still enter their own arenas.
3. Enter Ibaraki's arena from another world and from the same world: exactly one fight boss after streaming settles. Check `ARENA dedupe` logs, normal aggression, no premature mission completion/rewards, retry after loss, and Return.
4. Intentionally summon the same character into Boss A/B: both deliberate summons must remain. Check a different boss arena as the generic-policy control.
