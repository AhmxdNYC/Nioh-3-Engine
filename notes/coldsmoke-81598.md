# First-use smoke implementation (#81598 / human #81535)

Base d73e3ee9 includes pre-use seal menu choices and migration to core IDs. Diagnostic bb265bd7 is included as b457f79ad. This is a test candidate, not a public release.

The first press previously had no learned slot smoke template and returned to the game's later smoke. `CoverStartSmoke` now calls the native actor effect API on a cold slot: `2C7E90(player)`, then `2A10B4(component, &handle, 0x62557, 0, 0)`. Both functions have exact entry-byte guards from pc2's static research a678c375; calls use Grva and run on the existing game-thread path. A null component, guard mismatch, SEH fault, or unchanged/invalid -1 handle leaves game smoke enabled. A completed call with an output handle sets g_coverMade, suppressing later duplicate smoke. No save writes or new worker threads.

The native call sets thread-local replay and cold-observation flags. `HkFxRec` logs its shape before the replay early return, so the player's smoke is not learned as a core's template or muted recursively. World effects log measured matrix-row scale and position; bone-attached effects log the bone and explicitly mark effective scale unavailable (native descriptor default is 1). The ordinary later seal effect still supplies per-core templates as before. Warm slots retain the previous queued-template path.

Offline validation: release build via build.bat; 16 native dispatch/cover checks (warm bypass, cold native, both byte-guard failures, null component/player, invalid handle, both call faults, disabled smoke, TLS restoration); 93 picker/menu checks; sc22 replay passes its available report22 fixture (two optional logs absent).

pc2 live gate: fresh process -> load save -> first LEFT and first DOWN -> repeat each, record video; restart and reverse the direction order. Compare first/later smoke placement, size, throw hiding and fade timing, exactly one form begin/end, and restored input. Repeat with per-core OFF. Inspect `FormCore first-press native-fx`, `native-smoke`, and `SealCover` lines. First-use player-scale appearance is unproven until this gate; offline tests do not validate the engine API or visuals.

This candidate uses the authorized pre-use placeholders from d73e3ee9; it does not claim an equipped-core item-to-spawn mapping. Original audit notes/first-press-81537.md describes the pre-fix investigation; this document supersedes its diagnostic-only status for this branch.

Superseded for #81879: see notes/smoke-fix-81879.md. Zero native handles now fail, and this branch includes equipped-core build 1e4d2ce7. The earlier non--1 success assumption and placeholder-only scope above describe the original candidate, not this combined build.
