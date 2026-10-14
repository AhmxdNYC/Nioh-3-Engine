# Training mode: live boss spawn + live mission switch (bridge 4318)

2026-10-14, Ally (Proton, game v2.02, Nioh3EngineFix + BossRematch 1.4.13 installed). Runtime-tested, not static. Addresses are RVAs in the unpacked in-memory image (dumped from the running process; on-disk exe is packed).

## Result

| Goal | Verdict | Evidence |
|---|---|---|
| A. Summon any boss into the current area | WORKS (experimental) | Yamagata 307259, Kumagai 485999, Yama-Tsuyoshi 656503, Edo boss 412493, Yagyo rider 185713, Oni of Rajomon 894462 created next to the player in Rajomon and Hitokoto; hostile, lock-on, health bars, they hit and killed the player. Resource load before spawn 0.48-1.78 s (5 timed spawns). |
| B. Switch mission live, no title menu | WORKS | `MissionRequestById` from inside a mission: Rajomon -> Hitokoto (0x1CBC, start 67064) and Hitokoto -> Rajomon (0xEA75, default start). Normal loading screen; request to playable field ~28 s on Ally. |

## Native path (what the mission scripts use)

- Script VM binding table (pairs `{char* name, fn}`) at .rdata ~0x397EEB0. `Character::CreateDynamic` (0x238CD5C) only decodes VM args and calls native **0x1C0C5E0**:
  `uint32 CreateDynamic(uint32 charId, const float pos[4] /*x,y,z,1*/, float yawDeg, uint32 a4, uint32 ownerPlacementId, uint32 a6)`.
  It needs the character's resource entry ready (`entry+0x2C8 != 0`, lookup `0x133440([0x45B9E60], 0x134314(0,charId))`), and an owner id that is registered in charMgr (`[[0x45B9E40]]+0x48` sorted `{id,wrapper}` array). Returns generated actor id `(slot<<28)|owner` and enqueues creation via `0x683F74`; nonzero = accepted.
- `Load::Character` (0xA429F4 -> 0xA42A7C) calls **0x3F9E58(charId, 2, 0)** to stream the character's model/motion/AI. Calling it directly from the game thread loads resources for characters that are not in the current mission (0.5-1.8 s).
- charId is the boss "char" id from `notes/boss-map-all.csv` (`boss_char_id`), e.g. 307259 Yamagata, 894462 Oni of Rajomon, 485999 Kumagai.
- Live switch: `MissionRequestById` 0x2239600(missionId, slot byte 1, startPoint or 0, params {0x101,0x1000000,0x10100,0..}) called from the game thread mid-mission.

## Tool: mod/Nioh3Training (Nioh3Training.asi)

Standalone ASI (built on Linux with zig: `mod/Nioh3Training/build.sh`). Chains the per-frame call at 0x56529C (after BossRematch), runs commands on the game thread. Write lines to `<game>\Nioh3Training.cmd`; output in `<game>\Nioh3Training.log`.

- `spawn <charId> [dist] [owner]`: request Load::Character, wait for resources, then CreateDynamic `dist` units along +X from the player (owner auto = first live enemy placement, else player 1)
- `mission <missionId> <slot> <start>`: live mission switch
- `reg` (registry dump with live positions), `pos`, `loaded <charId>`, `lreq`, `dyn`, `act`, `call`, `plug` (RE helpers)
- Trace hooks: CreateDynamic and Load::Character entry logging (log-only)

## Known limits / next

- No despawn/cleanup yet. Spawned actors persist until death/reload; normal death/retry cleared them in tests. Rewards/drops from killing a spawned boss were not tested.
- Spawn direction is world +X (player yaw not decoded). Some bosses spawn without their arena script (no boss intro/gauge/music). A boss gauge did not appear; only a world health bar.
- Owner semantics are partly understood: an owner id is required. Multiple spawns with the same owner return the same id, and whether a new spawn replaces the previous one is unverified.
- Not wired into the mod menu/UI yet; command file only. Online/co-op is unguarded: use offline only.

## Update 2026-10-14 (bridge 4380): despawn, forward spawn, kill reward, mission phases

Ally, Proton, game v2.02. Runtime-tested; evidence /tmp/train-4380 (bridge file with the ASI).

| Item | Verdict | Evidence |
|---|---|---|
| 1. Despawn | WORKS | `despawn [all\|last\|N]`. The spawned actor is found by diffing the actor table (below) around CreateDynamic: new actor whose handle dword[1] == the returned id (appears 15-22 ms later). Delete = native `2BF7E8(actor)` (the tail of `Character::Delete`). Spawn -> kill -> despawn -> spawn again verified (log run5). The id returned by CreateDynamic (`slot<<28 \| ownerPlacement`) is NOT in the charMgr registry, so scripts' `Character::Delete(id)` path cannot find it; the actor pointer is used instead. |
| 2. Spawn in front | WORKS | camera yaw = `356018(20A914(0))` rad; forward on (x,z) = (sin yaw, cos yaw). Calibrated in game: walking forward moved (-164,+363) = (-0.41,0.91) vs predicted (-0.415,0.910). Spawn at `dist` units ahead (default 400) facing the player (yaw+180). Yamagata appeared dead ahead of the camera and attacked. |
| 3. Kill reward | WORKS | `kill [last\|N] [drop]` calls `786C84(comp,0,0,-1,drop,1,0,0,1)` (the ForceKill / ForceKillWithItemDrop native, guarded by `actor+AB84==-1`). Yamagata killed with drop=1: loot light pillars appeared and the HUD Amrita counter went 430,178 -> 581,207 (+151,029). |
| 3b. Boss HP bar | NOT for spawns | `gauge <id>` / autogauge call `UI::ShowBossGauge` native `1D115D4(id,0,0)`. It resolves the id by owner placement: with the Oni of Rajomon placement as owner it showed "Ibaraki Doji" (the Oni's bar, not the spawn's); with an ordinary enemy owner nothing showed. No gauge for the spawned actor itself. Default `autogauge 1` (harmless; use `autogauge 0` to skip). |
| 4. Mission switch phases | LOGGED | `mission 0x1CBC 1 67064` (hex accepted now). Rajomon -> Hitokoto Slope: +0.5 s request, **+1.0 s world data switched** (registry 228 -> 44, player pos jumps to the new map), game thread never stalls (30 ticks/0.5 s, ~22 while streaming), **+16.8 s registry 44 -> 136** (field entities created), load screen shows "Continue" at about +22-24 s (needs an A press), playable right after. Total request -> playable about 28 s, of which most is the loading screen. |

Other fixes: spawning again with the same owner while the previous actor still exists silently produces no actor (slot occupied); the ASI now logs a warning after 3 s. `despawn` the old one first (a dead corpse counts).
Dying: bosses kill the player in a few seconds; use `despawn` or `kill` right away when testing.
