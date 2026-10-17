# Arena swap: minimum arena+boss asset set for other-world bosses (task 4947, pc2, static, 2026-10-15)
No game run, no new decompiles. Built from boss-arenas.md, boss-char-assets.md, boss-only-load-patchnotes-pc2-4492.md, and the Fight-now result (R1CBC 13.8 s load, R7B35 19.9 s load; fightnow-20261015-050906.csv).

## What defines "the arena + the boss" (verified in notes)
- Arena = stage (area byte, `region_byte` in the catalogue; game byte 45BC428) streamed as cells around a centre pos (3A1700(area,&pos) -> cell id). Arena trigger = PlacementBox id and gate gadget hash named as constants in the mission script (boss-arenas.md s1). There is NO separate arena record: cells + placement DB of that stage.
- Boss = placement id (enemy placement DB of the mission, catalogue `placement_id`) -> char id -> loader DB -> setting DB -> WorldPQ/model (boss-char-assets.md chain). Phase-2 bosses are extra placement ids of the same mission.

## Minimum set (design, not yet measured)
1. Cells of the arena only: centre = arena point (catalogue arena_x/y/z), schedule via the cell variants in 0x223B8F0 (0x1FCECE4 / 0x1FCEE1C / 0x1FC4208). UNKNOWN whether they load 1 cell or a ring.
2. Boss placements only: filter 0x7D38FC (create placement; RCX id, EDX mission id) to boss id(s) + arena gate/box rows. Pass-2 rows (0x7D3BDC) hold gate/door/wave rows: keep them.
3. Boss char resources come implicitly from creating the placement (loader DB chain), so no separate boss-asset preload is needed; the cost is the stage's own cells and shared textures.

## Hook points for ally (RVA = VA - 0x7FF74F410000)
- 0x223B8F0 mission load scheduler (state 5 write at end); cell variants 0x1FCECE4, 0x1FCEE1C, 0x1FC4208; centre fn 0x3A1700; byte 45BC462/463/465 select variant (465!=0 skips cells: do not use).
- 0x1C1E470 placement builder (single caller 0xA31A50): pass1 -> 0x7D38FC, pass2 -> 0x7D3BDC.
- NOT 0x223AF50 (post-load setup; nothing to skip).
- Switching world in place (no load screen) would need the stage/area byte change + cell unload of the old stage; the unload side is NOT analysed (no decompile). That is the real gap: ask pc for decomps of 0x1FCECE4 / 0x1FCEE1C and the cell-unload caller.

## Order of work I suggest
(a) log-only hooks on the 3 cell variants + 0x7D38FC during a Fight-now (gives cells count and placement list, 1 run per world; Yamagata=Sengoku, Yoritomo=Heian already give two worlds); (b) flip 45BC462 and compare cell count/load time; (c) filter placements on an easy single-boss mission; (d) only then attempt in-place area swap.
Expected gain unproven: existing notes put 14-16 s in resource-manager drain after scheduling.
