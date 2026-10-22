# Boss swap RE (pc, task 4963): passive swapped boss + cross-world arena-only load

2026-10-15. Static only: Ghidra read-only decompiles (`D:/nioh3-re/decomp_ro.cmd`) of `re/Nioh3.dump.exe` v2.02 (base 0x7FF74F410000; addresses
are RVAs). Read alongside the Nioh3Training source in bridge file 486 (build 4801 = file 485, the build PC tested in #4851/#4855) and the bridge thread #4502-#4979.
No game launch, no mod or ASI changes.

## 1. Why the swapped boss is passive

**Swap path today** (`training.c` ArenaTick): `PickOwner()` looks for a registered placement whose wrapper has a live actor. In the field of world 0xE0, the
only live registered actor is the player (PC dump, file 476: 1 actor in the table). So the owner is 1 (the player), and it calls
`CreateDynamic 1C0C5E0(chr, pos, yaw, 0, owner=1, 0)`.

**What 1C0C5E0 copies from the owner** (decompiled):
- `req+0x60 = *(u32*)(*(owner_live+0xB0) + 0x10)`. This is the owner's **side/team**.
- `req+0x04 = 1C1CC10(owner)`, which is `(1C1CAB8(ownerWrapper) << 28) | ownerId`. For the player this is **0x50000001**. Slot 5 is the
  "exhausted, not reserved" loop limit described in training-boss-spawn-native-contract.md. `FindWrapper 135B78(0x50000001)` returns null
  (the PC log shows "no registered wrapper").

**Consumer `65A8A8`** (the queue drain 5BCBAC that builds the actor) then does the following:
- `669738(*(live+0xB0), req[+0x60])`, which writes **side = live+0xB0 -> +0x10**. So the boss copies the player's side, 0.
- If side == 0 and `req+0x64 != -1`, it calls `6630E0` (player-controller index at +0xAB84). It is skipped here (+0x64 = -1).
- `66954C(live)`: size/param scaling depends on the side (side 0 takes the "player-side" branch).
- `FindWrapper(req+4)` gives `669A10` attach + `669990` notify **only when a child wrapper exists**. With an enemy owner it exists
  (handle 0x103Dxxxx = slot 1 of the owner). With the player as owner there is none, so there is no attach and no notify.

**Side values are proven by their native setters:**
- `669510(live)` = SetSideEnemy: `669738(comp, 1)` + `66954C`.
- `65C14C(live)` = SetSidePlayer: `669738(comp, 0)` + `66954C`.
- The native placement enemy constructor `668E48` (reached from `669110`, the path that ActivatePlacement starts) calls **669510** right after
  `2BFA04`, unless record+0x1E0->+8 has bit 0x80 (in that case `90D78`, the neutral/NPC variant). `CreateDirect 238C9BC` also calls 669510.
  `CreateDynamic` is the only creation path that **inherits** the side from its owner instead of setting it to enemy.

**Root cause:** with owner = player, the boss is created on side 0 (the player's team). Its AI never treats the player as a target. Hate (1FF23C8)
does not override the side, which matches ally's #4630 result. Ally's #4979 hypothesis ("boss copies owner's team") is correct, and this
note gives the field and the setter for it.

**Fix (hook/call), in order of preference:**
1. **Runtime side change, full version:** `1C272F0(live, 1)` (`__fastcall(Character*, u32 side)`) on the game thread as soon as the actor
   appears in the actor table. WatchTick and GaugeTick already find it by handle within about 20 ms. 1C272F0 does the following:
   - `669738(comp, side)` and `9F1DA0(comp, flag)`.
   - If the side changed: rebuilds the AI params (`376C6C` on live+0x3A0) and walks all actors to drop anyone locked on the old side (`E12334`).
   - `2049534(live)`.

   The game's own runtime caller is `2227EE8`, which calls `1C272F0(actor, 0)` (enemy to player side). Use the same call with side 1.
2. **Minimal version:** `669510(live)` (`void(Character*)`). It is exactly what 668E48/CreateDirect do at construction. Call it before the
   actor's first AI tick (same tick it appears). Risk: it skips the AI-target reset that 1C272F0 does. That is fine for a fresh actor.
3. **Pre-spawn, no post-fix:** replicate 1C0C5E0 with the native builders: `683FE4(&req)`, then fill +0/+10/+20(5ECE50 matrix)/+70/+76=1/+DC as
   1C0C5E0 does, but use **`req+0x60 = 1`** and **`req+0x04 = 0`**. With +4 = 0, 65A8A8 generates its own ID via `65B2C0(chr)`, which avoids the
   fake 0x50000001 slot. Then call `683F74(&req)`, which keeps the enqueue token (1C0C5E0 drops it). This is cleaner, but it is a new adapter; do it after 1 or 2.
- Optional parity: set `live+0x893 = 1`. 65A8A8 sets this "boss" byte only for records with +0x9C bit 4 or for certain wrappers; dynamic
  bosses never get it. Unverified what reads it, so treat it as a test toggle, not a default.
- Keep `BossDistance=900`. An idle boss only notices the player within about 10 m (#4630). Hostile side + 9 m should give an attack within 10 s.

## 2. Boss in another world: smallest native load without a mission load

**Stage/world loading is separate from mission loading.** The mission sequencer's world step is `223B8F0`. Unless byte `45BC465` is set, it does:
```
cell = WorldGridFindCellContainingPoint 3A1700(worldByte=[45BC428], &startPos)
45BC463 ? 1FC4208(stageMgr, world, cell)        // only if stage+0x504 == -1 (nothing loaded)
:45BC462 ? 1FCEE1C(stageMgr, world, cell, 1)    // KeepLoadedStage: 1FCE21C(+0x15E8) + 1FCE6F4(+0x500,world,cell,1)
:          9181D8 + 1FCECE4(stageMgr, world, cell) // full: 1FCE47C(+0x500,world,cell), clears lists, 1C31E2C
[[47514D0]+8]+0x11 = world
```
These bytes are queue-slot params `+0x42/+0x43/+0x45` (slot 0x45BC420 + 0x40).

**The native entry that drives only this step is `MissionRequestRegion 22399B8(world, journey, startPoint, mode, params*)` with "mission" 0.**
This is what `World::WarpOtherWorld*` (23B9E80 .. 23BA4EC) call; the mission and its script stay loaded.
- Params come from `1BF7D68(prm)`, then `prm[0xE]=1` (WarpOtherWorld).
- `prm[2]=1` selects **KeepLoadedStage**, as in `ScriptBind_World_WarpOtherWorldKeepLoadedStage 23B9FA8`.
- `prm[4]=prm[0xF]=1` selects the WhiteOut variant.
- Pre-call: `3B28FC(2,0,0)`.

Ally's `oworld` command already does this. Measured: world switch +2 s, field entities +12 s (Heian to Hitokoto), and 15-20 s with WhiteOut. That
is cheaper than a mission load (40-46 s Ally, 16-37 s PC) but is not "arena only": it streams the cells around the start point **plus that
world's field placement sets**.

**Stage-only natives** (script bindings; all take a stage/area id and need a VM context, so call the native below them):
- `Load::StageOtherWorld 23A2524`: `rec = DD570([[45B9E30]+0xE0], id)`, then `FB0530(stageMgr+0x500, id, rec+0xD4 /*world byte*/, 0)` -> `3A0534(streamer, id, 0, opts, world)`.
- `Load::StageOtherWorldNoStreaming 23A27E8`: same call with an opts struct `{0xFF, -1, 0, 0, 1@+6, ...}`.
- `Load::Stage 23A23A0`: `1FB46E8(stageMgr, id, 0)`, which is `1FC4208` if nothing is loaded, else `1A79B7C(+0x500)`.
- `Load::IsStageLoadEnd A40A30`: id is in list `stageMgr+0x19D8..0x19E0` and `stageMgr+0x760 == 3`.
- `Load::StageStreamingStart/End 23A2930/23A28C4`: `stageMgr+0x11F1 = 1/0`.
- `Load::UnloadStage 23A2B80`: `FB5F98(stageMgr, id)` when +0x11F1 == 0.

These are what boss scripts use to pull an extra stage (an arena from another world) into the current scene. They are the candidate for
**"load just the arena"**: `FB0530(stageMgr+0x500, arenaStageId, worldByte, 0)` -> poll the A40A30 predicate -> `Warp(full)` (stream centre + 3A083C/3B35B4)
-> existing swap. Not proven: whether actors/collision from a second world's stage are valid while `+0x500` world byte is still the old world,
and which stage id (table `[[45B9E30]+0xE0]`, 0xD8-byte records, +0xD4 world) holds each arena. 3A0534/1FCE6F4 internals were not decompiled in this pass.

**Boss assets:** `3F9E58(chr, 2, 0)` + poll resource `+0x2C8` (already in StartArena/Preload) is sufficient for CreateDynamic. The boss's mission
script, placement set, gate gadgets and intro are **not** loaded by any of these. Those come only with the mission (`PlacementSetsBuildFromMissionDb 1C1D148`).
So the "full native fight" for a cross-world boss still needs the mission (NativeEnter route) unless the fight is driven by the mod replicas (BBM/BGM/gauge).

## 3. Ranked plan

| # | Step | Risk | First in-game test |
|---|---|---|---|
| 1 | Passive fix: after the swap/summon actor appears, `1C272F0(actor, 1)` (INI toggle; fallback `669510(actor)`). Log side before/after (`*(*(actor+0xB0)+0x10)`). | Low (native setter, same-tick, offline) | PC: world 0xE0 save, `fnow 0x401E` (Nuribotoke R401E, owner=1). Pass = log side 0->1, boss attacks within 10 s, bar shows, kill ends boss mode. Repeat with Yoritomo R7B35 on Ally with `aowner 1` forced. |
| 2 | Same, plus `live+0x893=1` A/B | Low-med (unknown readers) | Same boss twice, 893 off/on: compare stagger/HUD/drops. |
| 3 | Cross-world via the existing native: `oworld` with **KeepLoadedStage** (`prm[2]=1`), then swap | Medium (only WarpOtherWorld/WhiteOut were measured) | From 0xB1, `oworld 0xE0 <start next to Nuribotoke> 0` with prm[2]=1 (add a flag to the command), then `fnow 0x401E`. Measure request -> control, compare with plain oworld (15-20 s) and the native mission load. |
| 4 | Arena-stage-only: call `FB0530(stageMgr+0x500, arenaStageId, world, 0)`, poll `A40A30` logic, warp, swap | High (no proof that a second world's stage is walkable/collidable in the current world) | First a log-only run: hook `23A2524/23A27E8/FB0530` during one native boss fight that uses `Load::StageOtherWorld` to record real stage ids. Then call it once from the field and check ground/collision with `warp` + `pos` before spawning. |
| 5 | Cleaner spawn adapter (683FE4/683F74, req+0x60=1, req+4=0) | Medium | `summon` of one boss with the adapter: token logged, actor appears, side=1, despawn via 2BF7E8 works, then a second summon. |
| 6 | Full fight parity for cross-world bosses (script/gate/intro) | High | Stays the NativeEnter route (mission load + auto-warp) until a boss script can run without its mission. |

Key RVAs:
- 1C0C5E0 CreateDynamic, 683FE4/683F74 request ctor/enqueue, 65A8A8 builder, 1C1CC10 child id.
- 669738 SetSideRaw, 669510 SetSideEnemy, 65C14C SetSidePlayer, 1C272F0 ChangeSide, 668E48 placement enemy ctor, 23981C4 VM Character::SetSide (message to a registered wrapper only).
- 22399B8 MissionRequestRegion, 223B8F0 world step, 1FCECE4/1FCEE1C/1FC4208 stage (re)init, 3A1700 cell lookup.
- 23A2524/FB0530/3A0534 Load::StageOtherWorld, A40A30 IsStageLoadEnd.
