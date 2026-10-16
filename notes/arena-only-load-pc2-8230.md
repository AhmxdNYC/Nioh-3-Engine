# Skip the world load, go straight to the arena (pc2, task #8230)

Status: n=1, test build only (xw5116/b Nioh3Training.asi), NOT in the current build. Needs repeats.

Evidence: captures/rematch/xworldprobe-20261015-064559 (scenario xw5116/s6.txt)
- `oworld 0xB1 374664` (keep 0) requested at 06:48:07 PDT (Yoritomo world 0xB1)
- `arena 16102 ...` warp issued at 06:48:12, arena stage 67253 state -9 -> 4 -> 5 at +4.1 s
- player released to control at +4.3 s after the warp (about 9.4 s after the oworld request)
- Stage was ONLY the arena cell (loaded(9) list, 1 loaded at the warp), not the full world
- screenshot arena_after_oworld2.png: Yoritomo arena, boss HP bar up, player in the arena, no black cover
- actors: 5 in table at +1 s and +44 s (boss present)

Compare: native cross-world world load 13-20 s (prewarm on), 30-33 s (prewarm off).

Caveats:
- n=1. Repeat at least 3 times, both arms, on the current build.
- Not verified: memory use, mission script/doors of the arena (placement filter, see arena-swap-static-pc2-4947.md), leaving the arena back to the old world.
- Game currently busy with another slot's test (pid 23608), so no repeat yet.
