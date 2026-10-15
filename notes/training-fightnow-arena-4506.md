# Fight now without a loading screen: owner pick, preload, arena table, API (bridge 4506)

2026-10-14, Ally (Proton, game v2.02, 1920x1080 fullscreen, focus verified 30/30 checks, saves restored). Branch ally/arena-swap-4431.
Tool: mod/Nioh3Training (ASI + `Nioh3Training_arenas.tsv` + `Nioh3Training.ini`), table from `tools/gen_arena_table.py`.

## What was built

1. **Auto owner**: `PickOwner` = first registered placement with a live actor that is not the player; enemy records first, then any live registered
   actor. Finding: the owner must be a *live* registered actor. A dead/unspawned placement (even one with an enemy record) and the boss's own
   placement are refused (`CreateDynamic` -> 0); owner = player works but the boss is friendly/passive. A live non-enemy actor (id 9528 in Rajomon)
   gives a hostile boss. Log `ARENA WARNING` when only the player is left.
2. **Arena table for all bosses**: `Nioh3Training_arenas.tsv`, 43 single-boss rows (mission, start, world, char id, placement, player point, boss point,
   kind arena|start). Join of the catalogue (placement_id, start, region, arena/warp point, boss_pos) with `notes/boss-map-all.csv`
   (boss_placement_id -> boss_char_id). Rows with several bosses, no boss position or no world are left out (native load). Worlds: 0xB1 7, 0x96 13,
   0x35 7, 0xE0 7, 0xDF 6, 0x4B/0x8E/0xD8 1 each.
3. **Boss distance**: an idle boss only notices the player within about 10 m (spawned 16 m away it walked off; at 9 m it attacked). The boss is placed
   at most `BossDistance` (default 900 units) from the player along the catalogue line.
4. **Preload**: `Nioh3Training_Preload()` requests `Load::Character` for the `PreloadMax` (3) nearest bosses of the current world. The Boss Rematch
   overlay does **not** pause the game (pause flags stay 0), so a pause trigger cannot see it: Boss Rematch has to call the export when its menu opens.
   `PreloadAmbient=1` preloads once per world 4 s after landing as a fallback.
5. **Fight now wiring** = C API, because Boss Rematch must own its state: `int Nioh3Training_FightNow(uint32_t missionId)` returns 1 when the boss
   was swapped in place (do not call MissionRequestById), 0 when not handled (other world, row missing, Enable=0, a load running). Game thread only.
   Flag: `[ArenaSwap] Enable = 1` (default ON).

## Why not hook MissionRequestById (tested, left as opt-in HookFightNow=0)

Hooking the request for add-on callers works (Rajomon -> Yoritomo arena with no load screen) but Boss Rematch 1.4.13 verifies the queue slot after
the call, finds none and answers `native request uncertain; restart game before another replay` (second replay refused until a restart). The swap
has to be reported to Boss Rematch, so the call belongs in its Fight-now action. The local nioh3-engine source is older than the installed 1.4.13/1.4.15
(no FightNowSpawn, no cover), so Ally cannot edit it. Needed in the real source (pc):

```cpp
// ExecDirect, route "Fight now (in the arena)", after the clear check, before CallRequestById:
using FightNowFn = int (*)(uint32_t);
static FightNowFn fn = [] { HMODULE m = GetModuleHandleW(L"Nioh3Training.asi"); return m ? (FightNowFn)GetProcAddress(m, "Nioh3Training_FightNow") : nullptr; }();
if (fn && fn(r.missionId)) { /* swapped in place: no queue slot, no cover, arm nothing; Result("fight now: in place") */ return; }
// menu open (Boss Rematch page shown): static auto pre = (void(*)())GetProcAddress(m, "Nioh3Training_Preload"); if (pre) pre();
```

## Runtime results (Ally, Rajomon save, world 0xB1, two sessions; screenshots in the bridge file)

| Case | Result |
|---|---|
| Preload (3 nearest) then Fight now Ibaraki Doji | **0.51 s** command -> control, boss bar + intro line + attacks (player 2658 -> 1738), no load screen |
| Fight now Minamoto no Yoritomo, model not loaded | **10.0 s** (9.98 s = first model load; 16.7 s once when 3 preloads ran at the same time) |
| Yoritomo at 16 m | alive, wandered off, no attack in 25 s; at 9 m attacked (368/2658 left, died while unattended) |
| Other world (`fnow 0x1CBC`, Yamagata 0x4B from 0xB1) | not handled -> native request (not exercised to completion: the player was dead and loading) |
| Overlay path with the hook (opt-in) | swap works, Boss Rematch then locks replays until restart (above) |

## Known limits

- Lighting/fog/music stay those of the running mission; the arena's own script does not run (no fog gate, no clear flag, no rewards from the clear).
- Death/retry reloads the *current* mission (e.g. Rajomon), not the arena. Swapped-in boss is despawned when the next swap starts; leaving the mission cleans up.
- Cross-world bosses (the training ground 0x32 -> Heian 0xB1 etc.) stay on the native load; `oworld` WarpOtherWorld (15-20 s white fade) is not wired.
- Table rows of kind `start` use the Fight-now start point as the player position (boss pulled to 9 m); only 9 rows have a trigger-box point (`arena`).
- Final binary differs from the tested one by comments only (header text); sha256 in the RESULT.
