# Live arena swap without a loading screen (bridge 4431)

2026-10-14, Ally (Proton, game v2.02). RVAs are in the unpacked in-memory image (/tmp/re-dump/Nioh3.mem.bin).
Tool: mod/Nioh3Training (branch ally/arena-swap-4431), commands `stage`, `warp`, `arena`, `mworld`, `oworld`.

## Step 1: how the game holds arena geometry (static RE)

- Geometry is not owned by the mission. A **world** (byte) is a grid of **stage cells**; the stage manager
  `[45B9E58]` streams cells around a centre point. Sub-object `stage+0x500`: byte +0 world, u32 +4 main cell id,
  u8 +0x389 init state (0..5, 5 = ready: cells + path/navmesh initialised), loaded-cell set +0x40 (count +0x60,
  entry `[0]>>4` = cell id), streaming centre `stage+0x10B0` (+0x10C0 = dirty).
- World record `3A1924(world)`: origin +4/+8 (int), grid dims +0xCE/+0xCF; cell size 1,000,000 units. Main cell =
  grid cell containing the start point (`3A1700`, called by the mission loader at 223BB45, then `1FC4208` sets it).
- Mission record (`377B38([[45B9E30]], id)`) +0x15C = its world byte; `MissionRequestById` copies it into the
  request (+8). A mission request = {journey, mission, world, start point, mode, 24-byte transition flags}.
- `Player::SetPos` (VM 23AB4DC) tail = `1C27010(player,&pos,yawDeg)` + physics vfunc +0xD0 + actor-manager pos +
  `A429B4` + `1C2A3CC` + stage centre `stage+0x10B0` + `3A083C(stage+0x500)` (re-pick cell) + `3B35B4(stage+0x500,1)`
  (stage init step). So a warp inside the current world re-centres streaming; no mission load.
- Script loaders: `Load::Stage` 23A23A0 -> `1FB46E8` -> `3A0534(stage+0x500, cell, immediate, params, flag)`
  (deferred list +0x390 while state != 5); `Load::StageOtherWorld` 23A2524 same with the cell record's +0xD4 flag;
  `Load::IsStageLoadEnd` A40A30 (cell in stage+0x19D8 list and stage+0x760 == 3). These add cells of the
  current world; they do not change world.
- **World switch without a mission load**: `World::WarpOtherWorld*` (23B9E80 ...) build a request with
  **mission 0** (stay in the running mission), the target world byte and a start point, and send it through
  `22399B8` into the same mission queue (`223953C`). Variants only differ in the 24-byte flags
  (default {01 01 00 00 00 00 00 01 | 00 01 01 00 00 00 00 00}): WarpOtherWorld clears +7 and +0xA, sets +0xE;
  WhiteOut also sets +4 and +0xF; KeepLoadedStage sets +2 and +9. Mission script, placements and progress stay.

## Consequence for "Fight now" without a load screen

| Case | Route | Load screen |
|---|---|---|
| Arena in the world you are in (e.g. Rajomon -> Yoritomo / Ibaraki, both Heian world, catalogue region 0xb1) | warp (SetPos tail) + Load::Character + CreateDynamic | none |
| Arena in another world (training ground, other era) | WarpOtherWorld request (mission 0, target world, start) then warp + spawn | engine world transition (flags pick whiteout vs normal) |
| Native scripted fight (intro, fog gate, boss gauge, clear flag) | MissionRequestById (current Fight now) | yes |

## Step 2: live test (Ally, Proton, v2.02, 1920x1080 fullscreen, focus verified 104/104 checks)

One game session, save restored afterwards (RESTORED True). Evidence /tmp/arena-4431 (screenshots, logs).
Gameplay gate before each arm: HUD visible + stick input moved the player (gate7, pre-on, off-ctl2).

| Arm | From -> to | Command -> control | Screen |
|---|---|---|---|
| OFF (native mission load, = Fight now) | Yoritomo arena -> `mission 0x7B35 1 374664` | Continue prompt at ~40-46 s (field entities +39.6 s), control after A | loading screen + tips |
| ON cold (`arena`, warp + spawn, same world 0xB1) | Rajomon (Ibaraki area) -> Yoritomo arena | **9.9 s** (9.7 s = first Load::Character of Yoritomo 16102) | none: camera re-centred, arena streamed in |
| ON warm (boss resources cached) | Ibaraki boss fight (Rajomon) -> Yoritomo arena | **0.54 s** | none |
| Cross-world (`oworld 0x4B 67064 1` = WarpOtherWorldWhiteOut) | Frozen City (world 0xB1) -> Hitokoto (0x4B) | world switch +1.0 s, field entities +17.2 s, gameplay ~15-20 s | white fade, no tips / Continue |
| then `arena` Yamagata | Hitokoto shrine -> Yamagata arena | **0.53 s** | none |

Observations:
- Arena geometry/collision: player released 0.2 s after spawn landed on the ground (y 15111 -> 15016), walked
  the courtyard; no fall-through. Stage init state stayed 5 throughout; the streamer re-centred by itself.
- Yoritomo spawned with owner 1 (player; no enemy placement near) was live and moving but stayed passive
  (no aggro, no boss bar) in this run. Yamagata spawned with owner = a live Hitokoto placement (60223) got the
  **boss bar, intro line and attacked at once**. Owner choice matters; pick a live enemy placement of the world.
- Environment (time of day / fog / music) follows the running mission, not the arena: Yoritomo's arena showed
  Rajomon's daylight+fog, not Frozen City's night. No fog gate / door / clear flag / rewards from the arena's
  own script (it is not running).
- Walking into Ibaraki's trigger while testing started his native fight; `arena` from inside that fight moved
  the player out to Yoritomo's arena (BossBattleMode of Ibaraki not cleaned up; not examined).

## Verdict and next

- Same-world arena: works with no loading screen, 0.5 s warm / ~10 s cold (cold = boss model streaming;
  preload with `Load::Character` when the menu opens to hide it). vs ~40-46 s native on Ally.
- Different world (training ground 0x32 -> Heian 0xB1 etc.): the game's own WarpOtherWorld request switches
  world without reloading the mission: white fade ~15-20 s instead of the 40-46 s loading screen. Not zero.
  A true zero-load cross-world swap would need a second resident world; nothing found that keeps two worlds.
- Next: owner = nearest live enemy placement (aggro + gauge), boss facing/arena centre table for all bosses
  (catalogue arena_x/boss_pos already has them), preload on menu open, despawn on exit, wire into the
  BossRematch Fight-now menu behind an INI flag with the native load as fallback for other worlds.
