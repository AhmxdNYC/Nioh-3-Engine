# Training stage / arena contract

2026-10-11, Codex. Static inspection only; no build, game launch, save write, Ghidra run or new runtime claim. Worktree `codex-training-stage` started at `1641d3f`. Compared `main` (`cad8a558`) with `rematch-instant` (`b2cfe097`) using `git show`. Below, **RI** means the latter revision of `mod/Nioh3BossRematch`; unqualified note references are tracked project notes. Read `AGENT_BRIEF.md`, `notes/training-mode-inventory.md` and the shared token rules first.

## Decision

The first usable route is **one cleared boss in its native mission/arena through the existing Fight now loader**. It supports a selectable replay destination and faster arena arrival. It does not establish arbitrary boss/arena combinations, an in-place fight reset, reward-free practice, or seamless cross-stage switching.

| Operation | Existing mechanism | Actual boundary |
|---|---|---|
| Move within the current mission | RI `src/rematch.cpp:270` reproduces the native `Player::SetPos` tail, including physics/actor sync and streaming/path focus | Position change in the current stage; readiness is not synchronously established by the call |
| Load another supported mission | `MissionRequestById` 0x2239600 -> queue -> native load sequence including `MissionLoadStep` 0x223AF50 | Normal mission load; destination mission owns geometry, script, placements, gates and rewards |
| Start that mission at its arena | RI `src/rematch.cpp:376,1267`: v4 overrides a real start-point record while loading | Same mission load; no new stage composition or generic spawn API |
| Seamless arbitrary stage switch | No proven mechanism in the inspected paths | Would require separate stage/resource/actor/script/navigation lifetime and transition contracts |

Main has 88 tracked sites and the post-load warp path; RI has 94, adding the six-site SPAWN group (`sites.json:899`). RI's v4 change is not already in main. `src/main.cpp:14` exports only `Nioh3BossRematch_Version`; training cannot assume a public rematch/spawn/reset C ABI. Integrate inside the add-on's request/pump boundary.

## Request, queue and thread contract

1. UI/pipe posts a packed atomic request word (`RI src/rematch.cpp:131-148`). This is a single mailbox, not a FIFO: another `Post` before consumption can replace the previous word. `Tick` consumes it, holds at most one pending request, and refuses a new request while one is pending (`:1334-1339`). A training controller should admit only one operation with an explicit generation/result.
2. Every native call and data mutation runs on the existing game-thread pump. `InstallPump` chains its tick before the previous callee at RVA 0x56529C (`src/hooks.cpp:225-277`); `PumpTick` guards re-entry and catches/logs exceptions (`:171-179`). Catching an exception does not roll back an already executed native write or finish cleanup.
3. New starts require offline, no co-op, readable game/player, not loading or paused, no recently changed mission queue, no scroll, no boss fight and add-on Idle (`RI src/rematch.cpp:184-197`). A pending start waits at most 5 seconds for these gates (`:1364-1375`). Read failures currently default some booleans to false; training must carry read-validity and fail closed on unknown session/build/state rather than equate unknown with offline.
4. `ExecDirect` rechecks clear status on the **current journey**, reads its byte from mission state +0x10, finds a free slot, calls the native, then verifies the queue (`:663-775`). Do not synthesize another journey or bypass clear eligibility for the first prototype.
5. Native free-slot test is `u32 +4 == 0 && u8 +8 == 0`, for three 0x58-byte slots. Consumed slots can remain occupied: `notes/boss-rematch-build.md:28-30` records this observation. Do not require all slots to become empty or clear them manually. The 10-second queue-change gate and an available slot are different conditions.
6. Admission must require queue mission +4, start +0xC and mode +0x14 all match the request. Existing code refuses mismatched mission/mode but **only warns** on a mismatched start (`RI src/rematch.cpp:715-727`); `src/rematch_core.h:221-227` already has the stricter `FightSlotOk` predicate.

### Native evidence where the export is misleading

Read existing inert `D:/nioh3-re/decomp/0x2239600.c`, `0x223953C.c`, `0x22396D8.c`, `0x223AF50.c` and `0x137DCC.c`; native bytes came read-only from `C:/Users/Ahmad Hamza/nioh3-engine/re/Nioh3.dump.exe` (documented RVA/raw-offset windows, v2.02). No exports were regenerated.

- At 0x2239628, `41 8B D8` saves R8D (start); 0x223962B `44 8A FA` saves DL (journey byte); 0x223962E `8B F9` saves ECX (mission). Zero R8D calls the default-start resolver 0x1C17D8C. That resolver reads mission record +0x90 (`0x1C17DAE: 8B 80 90 00 00 00`), otherwise the first point from the mission's point set.
- At 0x223965C, `41 0F 28 06` loads 16 bytes from saved R9 (params); input must be aligned. Source uses `alignas(16)` and `{0x101,0x1000000,0x10100,0,...}` (`RI src/rematch.cpp:236-243`). The native copies 24 parameter bytes.
- Request base is `[rbp-0x60]`: 0x22396AB `89 7D A4` stores mission at +4, 0x22396AE `89 5D AC` stores start at +0xC, 0x223967D `48 C7 45 B4 02 00 00 00` stores mode 2 at +0x14. Params are **+0x40**, not the +0x20 asserted by an older path note: 0x223966F `0F 29 45 E0`; native copy 0x2233E02 `0F 10 42 40`, 0x2233E06 `0F 11 41 40`, 0x2233E0A `F2 0F 10 4A 50`.
- `0x223953C.c:9,28-31` suggests an undefined scalar first argument. Native entry saves RDX (`0x2239546: 48 8B FA`) as the request pointer, scans its own queue, and passes that pointer to the copy at 0x2239589 (`48 8B D7`). RCX from the caller is overwritten. Do not infer a usable `Enqueue(params,request)` API from that decompile; use the established ById wrapper.
- `WarpPointFind` export `0x137DCC.c:11` omits its argument. Native 0x137DE2 `8B F1` saves ECX as point id, 0x137E03 `8B D6` forwards it to set lookup. It scans existing registry entries and returns zero if absent; it does not load a new point set.
- Actual pump call bytes: `0x56529C: E8 43 D7 E4 FF` -> 0x3B29E4. `MissionLoadStep` is a loader step, not the general field/title request pump (historical correction in `notes/boss-rematch-build.md:70-72`).

## State observations and completion

`RI src/rematch.cpp:151-182` reads these without making native calls:

| Signal | Meaning / requirement |
|---|---|
| `[GameMain]+0x588..0x58D` | Six stop bytes; +0x58A loading, +0x58B/+0x58C pause |
| `[[MissionMgr]+8]+0x14` | Retry/start-point id; **not the 16-bit mission id** despite old logs calling it stage |
| same state +0x28 | Running mission id; native 0x22372C3 `44 8B 79 28` versus +0x14 at 0x22372CB `44 8B 71 14` proves distinct fields |
| same state +0x10 | Current journey byte |
| `[[CharMgr]]+0x2C18` | Picture Scroll mission state |
| `[PlayMode]+0x34`, `[NetMgr]+0x1DC` | Existing online modes 1/2 and connected/co-op gate; read-validity must accompany values |
| `[PlayerTab]+0xCD*0x18`, character +0x110 | Player handle and position; existence alone does not establish controller/resource readiness |

**Request accepted** is the matched native queue record, not the pipe's `queued` reply. **Destination loaded** requires a load to have been observed, loading down, expected mission, readable player/state, and a stable observation window. Existing tracker waits 3 seconds after load down with player/state readable (`RI src/rematch.cpp:1073-1107`). It resets the timer when loading resumes, but does not reset it on a non-loading unreadable-player/state frame. A strict training observer should require consecutive valid frames and pause-aware settling.

**Arena ready** additionally needs the allowlisted transform/ground, trigger/gate state, expected actor identity/count, usable collision/navigation, and no pending destination transition. `spawn=arena` is only `!AtStartPoint`: greater than 5 metres from the original point in XZ (`RI src/rematch_core.h:239-241`, `src/rematch.cpp:1294-1300`). A distant/out-of-bounds position can receive that label. Existing rematch PASS is an increased global `BossBattleMode` count or an already active fight (`:833-840,915-921`); it does not prove the selected placement appeared once.

Native load readiness includes path/navmesh initialization (StagePathInitStep 0x3B35B4 reaching state 5), area/request queues, object setup, current area, blockers and character readiness (`notes/loading-bottleneck.md:71-83`). A warp's single StagePathInitStep call is not completion. Do **not** call `LoadIsLoadEnd` merely as an observer: it advances a ready-wait accumulator (`notes/tour-mode.md:16`). Use validated read-only observations or the loader's original result, preserving its call count.

**Return requested**: `MissionRequestReturn(params,1)` with aligned params (`RI src/rematch.cpp:1027-1034`). Native reads saved return fields +0x18/+0x19/+0x1C, remaps their mission, falls back to +0x10/+0x11/+0x14 under specific conditions (`D:/nioh3-re/decomp/0x22396D8.c:25-47`; bytes at 0x2239702 `41 8A 78 19`, 0x2239706 `41 8B 58 1C`, 0x223970D `41 8A 68 18`). Therefore this is the game's return route, not an unconditional warp to a chosen training lobby.

**Return completed**: expected return destination, loading ended, readable player/state, mission +0x28 changed from the practice mission for at least 5 seconds, add-on Idle, and verified record restoration. Existing tracker uses the mission change when +0x28 matched on landing, otherwise falls back to changed retry point (`RI src/rematch.cpp:1087-1090,1116-1145`). That fallback and an Idle reached by timeout are insufficient training completion. Do not schedule the next start from `exit requested` alone.

## What v4 changes, and what it does not

The SPAWN group checks point lookup, position/yaw getters, retry-point reads and camera path (`RI src/rematch.cpp:336-353`; `sites.json:899-950`). A request with a supported arena point arms a patch. On the first observed loading tick, lookup must yield the real chosen start-point record; its initial XYZ must be within 1 metre of the catalogue. The patch snapshots XYZ/yaw at +0x40..+0x4F and flags at +8, writes the arena transform, and clears **mask 0x4** (arrival-camera preset). It rechecks every loading tick and once per second afterward (`:374-432,755-762,1267-1300`). IDs remain unchanged.

Native confirmation: placement 0x1C0F2D3 `8B 49 14` gets retry point; 0x1C0F363 `E8 64 8A 52 FE` calls point lookup; 0x1C0F37B `E8 9C 33 7A FE` / 0x1C0F38E `E8 E5 32 7A FE` call XYZ/yaw getters. XYZ reads +0x40/+0x44/+0x48 at 0x3B2726-0x3B2736; yaw reads +0x4C (`0x3B2685: F3 0F 10 41 4C`). The alternate placement path when actor-manager +0x48 object's word +0x110 is nonzero reads DataMgr +0x188 instead (`0x1C0F2CC: 0F B7 96 10 01 00 00`, `0x1C0F2E6: 48 8B 8B 88 01 00 00`); then the record override need not take.

Streaming-centre path 0x223BA4E `8B 0D D8 09 38 02` reads request start 0x45BC42C -> point lookup at 0x223BA54 -> XYZ getter at 0x223BA73 -> grid-cell lookup at 0x223BB45 (`E8 B6 5B 16 FE`). Thus the override can centre ordinary mission streaming around the arena. It cannot ask that mission to own another mission's resources.

Post-load warp remains fallback when the player is still within 5 metres of the original start. Native SetPos tail synchronizes character/controller, actor-manager position, scene focus and path initialization (`RI src/rematch.cpp:266-301`). Important two-load evidence: 0x23AB685 `48 8B 05 B4 E7 20 02`, then 0x23AB693 `48 8B 08`; omitting the second load caused the old exception. Streaming calls are 0x23AB6DA -> 0x3A083C and 0x23AB6EF -> 0x3B35B4. This is current-stage streaming, not a cross-mission loader.

Restoration occurs in `ToIdle` on normal exit, timeout or pre-entry abort, and on online/co-op detection. It writes back only if lookup still returns the same record and transform/flags are exactly the add-on's bytes (`RI src/rematch.cpp:417-432,620-637,1270-1274`). Changed/unloaded records are left to the game; failed writes are logged. Restoration success therefore needs an explicit observed result, not just `g_sp.st == 0`. Reapply preserves the latest observed original; allocation replacement, partial writes and exception cases still need runtime evidence.

## Gate, asset and navigation limits

- A mission is a bundle: bytecode script at record +0x50, enemy placements +0x54, other placement tables +0x58..+0x68, start points +0x6C. Set construction requires its referenced tables to resolve (`notes/boss-duel-flow.md:46-52`). Boss selection returns a placement from that mission's set; multi-phase/multi-boss missions can have several candidates (`:55-60`). A placement id is neither a character asset key nor an executable RVA.
- Character assets resolve through loader DB -> scene/character setting DB -> action/motion/model/rig/shader resources; these records alone do not provide HP, AI or boss behavior (`notes/boss-char-assets.md:6-9`). No callable training asset preload/despawn contract was established here. A boss absent from the loaded mission's placement set is not made available by changing XYZ or passing its id to an activation hook.
- Arena boxes, doors, EnemyArea and progress are mission-script owned. Normal chain: trigger -> gate closure -> Character Create -> BossBattleMode -> death opens gate, clear gadget and mission flags (`notes/boss-arenas.md:8-14`). Gate entry may require a genuine WaitWarpStart poll. Existing hook captures its 16-byte key and slot, then `RightsRequest` performs the game's door/StartWarp action (`RI src/rematch.cpp:53-78,974-1023`). A transform change cannot substitute for that handshake.
- Different arenas need terrain/collision/streaming readiness and a compatible navmesh/EnemyArea/leash, plus script/gate ownership. These are pair-specific acceptance requirements, not demonstrated universal compatibility. There is no inspected mechanism to transplant or synthesize them safely.
- Redirecting a shipped Emaki map can choose existing scroll content, not arbitrary story enemies. Corrected static result: mission 0xCC96 does not load story missions' +0x54 enemy tables (`notes/boss-rematch-build.md:105-109`), superseding the optimistic arbitrary-record proposal in `notes/boss-rematch-paths.md:9,25`.

## Failure, retry, online and persistence

- Load-start timeout: no load within 120 seconds; absolute request-to-finish limit 20 minutes (`RI src/rematch.cpp:1109-1112`). Timeout resets add-on state/restores, but does not cancel a native queue record already accepted. Training must not reuse Idle as proof that a late load cannot arrive.
- Verification timeout is 60 seconds of non-loading/non-paused time after arena arrival/warp, 180 after gate request; gate wait has a 30-second play window (`:924-936,988-1019`). Source comments saying 8 seconds are stale. At most three trigger re-warps, only if back near original start, outside trigger, no gate intro/fight (`:956-969`). These failures do not unload a mission or prevent gameplay rewards.
- Death/retry is a same-mission reload, not exit; the original retry id remains and the record override is intended to be reapplied. Intro loads can also occur inside the same mission and must not trigger another warp (`:1116-1128`). Force restart after a defeat has no proved in-place reset; the first controller should return, verify cleanup, then reload the same cleared mission. Existing Idle gate prevents starting a second mission while a rematch is active.
- Abort restores add-on data only before its tracker marks entry, and does not cancel the native request (`:1355-1362`). After entry, use the native return path. Exit requires EXIT group, known mission, no online/co-op and no loading (`:1344-1354`); it does not itself verify queue acceptance or completion. Self-test retries exit indefinitely at 120-second intervals (`:1250-1255`); do not inherit that unbounded retry policy.
- Mid-session online/co-op removes the spawn override, blocks the normal warp and clears pending gate rights (`:1270-1274,872-875,984-986`), but does not exit/reset the fight or latch the whole controller off. Redirected scrolls only warn while online (`:1151-1157`). Moreover the duel approach warp in `FnVerify` lacks an online check (`:939-952`). Require a training-wide fail-closed latched disarm before any native action; no automatic exit while connected. Remaining native gameplay/accepted loads need observation and explicit recovery.
- This route preserves native replay behavior. It does **not** suppress loot, amrita, inventory, achievements, clear flags, map discovery, retry-point saves or autosaves. Existing reward hooks are observations (`src/bosslog.cpp:126-159,203`), not prevention. Historical replay evidence includes repeat clear/defeat flags (`notes/boss-rematch-build.md:136-139`). The v4 override changes resident point data, not the persistent point id; that is narrower than proving no persistent side effects.
- Tour's process-lifetime save guards are a separate feature, not present in BossRematch: their native save/autosave paths and layered verification are described in `notes/tour-mode.md:47-68`. Blocking disk writes would still leave in-memory story/reward effects and possible session/achievement effects to prove. Backup/restoration verifies file recovery; it is not proof of a reward-free training session.

## Concrete first prototype / runtime acceptance gate

Start **OFF**, with a read-only observer on the existing pump. Its facts include read-validity, mission/journey/retry ids, request generation/accepted slot, observed load edges, actor placement identity/count/lifetime, player/arena/trigger state, fight and gate epochs, online state and restoration outcome. Observer success precedes mutation enablement.

First allowlist entry: **R1CBC Yamagata Masakage**, mission **0x1CBC**, placement **65092**, start **67064**, trigger box **66599**. RI build note `notes/boss-rematch-build.md:273-288` reports one PC run: override applied before load, first observed arena spawn **337042,10100,206691**, native box/intro -> fight, **zero warps**, request-to-fight **24.5 seconds**; unchanged retry id; restoration logged on leaving. This is prior reported evidence, not a new test here. Its 19.7-second mission load and 6.5-second intro load demonstrate that the route is not seamless.

Bind acceptance to the exact transform and provenance: RI shipped catalogue R1CBC arena is **337691,10244,206714**, different from that run's first-frame position. `ApplyArenas` accepts a matching mission/placement sidecar and adds 50 units to Y (`RI src/rematch_core.h:171-193`, `src/rematch.cpp:1602-1605`). Do not transfer the one-run PASS to every catalogue/captured coordinate. The earlier bad post-intro capture is documented and fixed in v4; the observer must verify the active point belongs to the trigger/ground, not merely trust `captured`.

Prototype operation: confirm current-journey clear -> strict queue admission -> native mission load and allowlisted v4 spawn override -> observe arena plus exactly one expected boss -> native combat/death behavior -> return -> prove load/exit/restoration -> next session. First label it **experimental native-arena replay/practice**; do not advertise arbitrary arena selection or reward suppression. Native-at-fight Hanzo **RC362 / mission0xC362 / placement1102178 / start1107817** is a later distinct-route candidate, not proof of v4 override behavior.

Required before calling the entry supported: initial load, native intro/gate, collision/AI/navigation, expected actor uniqueness, three death/retry cycles, defeat then full reload, abort before/after native admission, late load/timeout, failed/missing SPAWN and EXIT groups, read failure, online transition, title/Continue, restoration then ordinary shrine replay, camera and before/after persistence evidence. The v4 all-row, death, restore, save, camera and Proton tests remain explicitly **QUEUED, not run** (`RI notes/boss-rematch-build.md:290-322`). Adding another enemy/arena pair requires its own evidence; catalogue `verified=1` and native-site resolution alone do not pass this gate.
