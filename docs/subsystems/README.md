# Subsystem write-ups (goal part 4)
One page per area costing >= 0.3 % of profiled CPU self time. Shares are the mean of the Ally and PC profiles (`py -I tools\progress\perf_coverage.py --self`, areas from `tools\progress\progress_treemap.py`; regenerate numbers with `progress_treemap.py --check`). Coverage = share of the area's self time in functions with a real or `Unknown_` name. RVAs are image-relative.

| area | CPU share (Ally / PC) | threads | page | coverage |
|---|---|---|---|---|
| GFX / D3D12 submission | 22.6 % (33.5 / 11.7) | [App]Game, SynergisticDispatcherTask, Present | [gfx-submission](gfx-submission.md) | 99.7 % |
| Jobs / KIDS scheduler | 19.0 % (17.3 / 20.8) | Scheduler0..6, StageUpdate x8, [App]Game spin | [jobs-kids](jobs-kids.md) | 99.7 % |
| Collision / Physics | 12.1 % (2.3 / 21.9) | KIDSTS[s9:CollisionUpdate] x9 | [collision](collision.md) | 99.7 % |
| Meshlet streaming | 11.7 % (10.2 / 13.3) | TS_MESHLET_REQ x10, [App]Game | [meshlet](meshlet.md) | 99.9 % |
| Streaming / Resources | 9.8 % (11.7 / 7.9) | [App]Game, workers | [streaming](streaming.md) | 99.7 % |
| CRT memcpy/memset/libm (0x8-0xC MB) | 7.9 % (12.9 / 2.9) | [App]Game, workers | [crt-memcpy](crt-memcpy.md) | 100 % |
| Memory (page pool, allocator) | 3.7 % (3.3 / 4.2) | [App]Game, any | [memory](memory.md) | 99.2 % |
| Effects / Post / object update | 3.7 % (1.9 / 5.6) | EffectUpdateThread x10, Scheduler0 | [effects-objects](effects-objects.md) | 98.7 % |
| Main loop / timing | 2.9 % (3.1 / 2.7) | [App]Game, WinMain | [main-loop](main-loop.md) | 98.9 % |
| UI | 2.4 % (3.1 / 1.7) | [App]Game | [ui](ui.md) | 98.8 % |
| Audio | 2.2 % (0.4 / 3.9) | audio mix thread (1A786A8), SoundSystemUpdate | [audio](audio.md) | 97.8 % |
| Animation / bounds math | 1.5 % (0.6 / 2.4) | Scheduler0, workers | [animation](animation.md) | 97.6 % |
| Mission / game logic | 0.6 % (0.1 / 1.1) | MissionThread, Scheduler0 | [mission-logic](mission-logic.md) | 97.5 % |

Below the 0.3 % threshold (no page): Input 0.03 %, Shadow (CPU side) 0.01 % (see notes/shadows-culling.md, input-thread.md), and a few 0.01-0.02 % unclassified address buckets. Third-party (XeSS/FSR/Streamline) CPU time is outside game code and not in the self-time profile.

Caveat: the area is inferred from symbol names, and unnamed functions take the area of the nearest named function only if it is within 64 KB, or within 192 KB when both neighbours agree (explicit name-prefix rules and `area_overrides.tsv` win over neighbours). Some functions are therefore filed under a neighbouring area; the page for the function's real role says so (e.g. Audio lists PlaceableVisibilityUpdateRange, SinCosApprox, AabbCenter). Self-time profile totals only game-attributed samples (Ally 5.1 %, PC 16 % of all samples; the rest is driver, kernel, waits).

## Naming issues
Cleaned up 2026-10-08 (notes/cleanup-decompile-db.md); nothing open.
- B94230 is `MemcpyErms`: a 16-byte `rep movsb` stub (0 direct callers). B94250 `Memcpy` jumps to it (jne at B943C6) for large copies, so the two are separate routines and both descriptions now say so.
- Generic math/container leaves (`Vec4AddInPlace`, `Vec4Scale`, `Vec3Dot`, `TransformedBoxAabb`, `SinCosApprox`, `CrtAtan2f`) now sit in the "Math / CRT / containers" area, and `PlaceableDistanceTest`, `ObjVirtUpdateLocked` in "Object / scene update", by prefix rules and `data/area_overrides.tsv`.
- The six former "?" rows are named: BDF238 `CrtAtan2f`, 371784 `EntityProximityTriggerScan`, AC8D10 `NavOpenListPopExpand`, 7403EC `RbTreeContainsU32`, 259C4C `AnyChildVirtualTrue`, 2833F0 `ShortKeyTableFlagGet` (low/medium confidence except the two CRT/container ones).
