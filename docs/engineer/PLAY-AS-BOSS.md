# Play as a boss: starter brief

**What this is.** A collection of everything this project already knows that bears on a "play as a boss" mod for Nioh 3 (PC, Steam v2.02): how bosses are spawned and removed, what an actor looks like in memory, how the player is driven, how actions are forced, how the AI is held or turned on, what we know about the game's own transformation mechanic, and the camera.
**What it is not.** Nothing here has been built. No mod in this repository lets the player control a boss. Section 9 proposes approaches; sections 2 to 8 say what is proven and what is guessed.
**Scope of the facts.** All RVAs are for `Nioh3.exe` v2.02 (PE timestamp `0x6AA7C106`). Re-derive them after a game update (see [symbols/README.md](../../symbols/README.md)).

How to read the evidence tags used below:

| Tag | Meaning |
|---|---|
| **live** | A mod in this repo exercised it in a running game and the notes record the result |
| **static** | Proven by disassembly only, never run |
| **guess** | Inference or an idea, not checked |

Source paths are repo-relative. The mod source in this public copy is an older snapshot (2026-11-02) than the current release (v1.8.39). Where a file or feature exists in the current source but not here it is marked *(source not yet in the public copy)*. Most `notes/` files cited are development notes that are not published; the published research is under [research/](../../research/README.md). Published ones used here: `research/input-thread.md`, `research/boss-exe-strings.md`, `research/gamelogic.md`.

Related reading already in this repo: [README.md](../../README.md), [CONTRIBUTING.md](../../CONTRIBUTING.md), [PUBLIC_EXCLUSIONS.md](../../PUBLIC_EXCLUSIONS.md) (what must never be committed: no game bytes, no decompiler dumps, no game text), [docs/code-map.md](../code-map.md), [docs/subsystems/animation.md](../subsystems/animation.md), [docs/subsystems/mission-logic.md](../subsystems/mission-logic.md), [research/input-thread.md](../../research/input-thread.md), [symbols/symbols.tsv](../../symbols/symbols.tsv).

---

## 1. The four mods and which one owns what

| Mod | Source | Relevance to this task |
|---|---|---|
| `Nioh3Training` (C, `training.c`) | `mod/Nioh3Training/src/` | Spawns, tracks, despawns and controls bosses next to the player. The closest thing to a boss-control toolbox. |
| `Nioh3BossRematch` (C++) | `mod/Nioh3BossRematch/src/` | Boss replay menu; hosts the mod menu pages for Training; implements the soul-core "form" feature (player hidden, yokai puppeted); `rm` pipe commands for probes. |
| `Nioh3EngineFix` | `mod/Nioh3EngineFix/src/` | Performance fixes plus the virtual pad (`vpad.cpp`), the overlay menu host, and the AI-chain crash guard. |
| `Nioh3CamKiProbe` | `mod/Nioh3CamKiProbe/src/probe.cpp` | Camera hook (distance, lens, offsets, soul-form vertical follow). |

Training exports a C API (`Nioh3Training_FightNow`, `_FightNowChr`, `_CanSummon`, `_SetMoveAllowed`, `_SetMoveRepeat`, `_DespawnSlot`, `_SlotChr`, ...) that Boss Rematch calls. Boss Rematch reads Training's generated move data through `Nioh3Training_AutoMoves`. New work should follow that split: native game code lives in a mod with its hooks and sites in `rvas.json`, UI lives in Boss Rematch's menu pages (`mod/Nioh3BossRematch/src/*_menu.h`, `mod/settings/catalog.json`).

All three mods are ASI plugins. Hooks are inline 12-byte `movabs rax; jmp rax` patches with a trampoline published before the live patch (see the comment at the top of `training.c`), or IAT slot patches. Every site is byte-checked against a pattern first and refuses to install on mismatch.

---

## 2. Spawning and despawning characters

### 2.1 Facts about the object model (static unless noted)

- **Actor table.** Every live actor is linked through `actor+0xE0` into 16 hash buckets at `[[0x4745348]+0xB98+8]`; each bucket is `{head, count}`. `Actors()` in `training.c` walks it. This is the ground truth for "is this pointer still alive" (**live**).
- **Player lookup.** `[[0x4745348] + 0xCD*0x18]` is player 0 (`PlayerChar()` in `training.c`; also `kRvaPlayerTab` in `mod/Nioh3EngineFix/src/vpad.cpp`). The stride suggests more player slots follow (co-op). **guess**: slots 1..3 are the other players.
- **Character manager.** `[0x45B9E40]`. `ActorFindById` (`0x135B78`) searches a sorted `{id, ..., pointer}` array and returns a **wrapper**, not necessarily a live Character. `wrapper[0]` is the live Character or null. Placement-created enemies have wrappers; dynamically created ones do not (**live**: `FindWrapper` fails on a `CreateDynamic` handle).
- **Handle.** An actor's handle is the dword at `actor+4`. `CreateDynamic` returns `(slot<<28)|ownerPlacementId`; the actor with that handle appears in the table 15 to 22 ms later (**live**).
- **Resource entries.** A character's resources are an entry in the object manager (`[0x45B9E60]`) keyed by `0x3_00000000 | charId`. `entry+0x2C8 != 0` means loaded (**live**: this is what Training polls). Linked characters (arms, phase models, summons) come along through a dependency closure; companions are keyed `0x4_00000000 | id`. Ready-check of only the main id can race a late companion (**static**, `notes/noload-boss-spawn-*.md`).
- **Character ids** are `kidshash(name)` of the setting name (`notes/boss-char-assets.md`). There is no HP, boss flag or AI id in the per-character setting objects; those are in the game's table layer and placement data.

### 2.2 The spawn paths this project uses

| Path | Used by | How | Status |
|---|---|---|---|
| **Native mission replay** | Boss Rematch "Fight now" when no in-place swap applies | `MissionRequestById` `0x2239600(missionId, slot, startPoint, params*)` from the game thread; the game's own scripts then create the boss. | **live**. Full load screen. |
| **Dynamic create** | Training summons, arena swap, Boss Match | `Load::Character` native `0xA42A7C`, or directly `0x3F9E58(chr, 2, 0)`, then wait for `entry+0x2C8`, then `CreateDynamic` native `0x1C0C5E0(chr, float pos[4], yawDeg, 0, ownerPlacementId, 0)`. | **live** for about 8 bosses from different worlds. 0.16 to 1.8 s to load, no map load. |
| **Placement activation** | Observer only (not used to spawn) | `0x1180740(placementId, unused, flag, value)` is what script `Character::Create` calls. It only flips activation fields on an existing wrapper. | **static**. Returns "accepted", not "ready". |
| **Soul core seal spawn** | Boss Rematch soul-core forms | The game's own item use ends in `0x65A8A8` (the queue-drain that builds an actor from a request, enqueued by `0x683F74`). The mod hooks it (`FormKeySealNote`, `SpiritSpawnNote`). | **live** (shipped feature). |

`CreateDynamic` details that matter (`notes/training-live-spawn-*.md`, `notes/boss-swap-re-*.md`, `notes/training-boss-spawn-native-contract.md`):

- It needs an **owner placement id** that is registered in the character manager and has a live actor. The new actor **inherits the owner's side/team** (`req+0x60`), which is why an owner of "player" gives a boss on the player's side (see 4.3).
- Spawning twice with the same owner while the first actor exists produced no second actor in the original tests (**live**, early notes; later code logs a 3 s warning in `WatchTick`). Despawn first; a corpse counts.
- Reservation hazard (**static**): the generated id can collide when owner slots are exhausted, a failed enqueue does not release the reserved slot, and a timed-out request is not a rejected one. The note concludes an unrestricted `CreateDynamic` wrapper is unsound without a transactional adapter. Training works around this with a small tracked set (`MAXSP` 32 actors, `N3T_SLOTS` boss slots A, B, C, ...) and `owned_registry.h` (*offline-tested lifecycle core, not wired to native calls*).
- The spawn lands in front of the camera when the forward vector is computed as `(sin yaw, cos yaw)` on (x, z) with yaw from `0x356018(0x20A914(0))`. The first attempts spawned behind the camera until this was calibrated (**live**).

### 2.3 "Summonable" versus "not"

There is no engine flag. In the mods, summonable means "the mods hold a character id for it":

- `Nioh3Training_CanSummon(mission)` is true when `SummonTok(mission)` finds an arena row or a summon-only row.
- Rows come from `mod/Nioh3Training/Nioh3Training_arenas.tsv` (kind `arena`, `start`, `summon`) and from `arena_scan.h`, a C port of `tools/auto_arena.py` that derives rows from the player's own game data at startup (`[ArenaSwap] AutoArena = 1`). New characters join the summon roster as `Boss <chr>` until named.
- Boss names, ids and arena geometry: `mod/Nioh3BossRematch/Nioh3BossRematch_bosses.tsv` and the arena tsv. These are small factual tables the mods need at run time (see PUBLIC_EXCLUSIONS).
- Caveats seen live: some bosses spawn without their arena script (no intro, gauge, music, fog gate); a boss from another world works because only character resources are loaded, but the arena props and navmesh come from the current map, so spawn on open flat ground (`notes/noload-boss-spawn-*.md`).

### 2.4 Despawn, kill, fade

| Operation | Call | Status |
|---|---|---|
| Delete an actor | `0x2BF7E8(actor)`, the tail of `Character::Delete` | **live**. Cleanly removed all 6 bosses tested. Script `Character::Delete(id)` cannot find dynamic actors; use the pointer. |
| Fade out | `0x2BC5DC(renderObj+0x240, 0.0f, frames)` via `actor+0x20` | **live** (`FadeOutActor`). Does not free the actor. |
| Force kill | `0x786C84(comp = actor+0x3B0, 0, 0, -1, drop, 1, 0, 0, 1)` | **live**. With `drop=1` gives loot and Amrita. Guarded by `actor+0xAB84 == -1`. |
| Death sensor | `0x4892A8(actor)` returns 1 when HP state is dead or the actor is in finisher states | **live** |
| Unload resources | `0x1C1DF80(chr, 0)`, once per `0x3F9E58` | **static**. Training keeps a small pool (`g_cfg_pool`, default 3) and evicts with it. |
| Group delete | `0x1C101A4(group)` (script `Character::DeleteGroup`) | **static**; group-wide, not a single-actor delete. Do not copy its leaf flag writes as a despawn. |

Lifecycle warning from the placement-activation note: a wrapper, table or live pointer sampled in one epoch is only a diagnostic identity. Invalidate on loading, mission or retry change. Every mod here re-validates an actor against `Actors()` before touching it; keep that rule.

---

## 3. Character and actor structure: what is known

Offsets are from `training.c`, `boss_vitals_game.h`, `aggro.h` and the notes. All are v2.02 and mostly **live** (read or written by shipped code) unless tagged.

### 3.1 Actor (the Character object)

| Offset | Meaning | Tag |
|---|---|---|
| `+0x00`, `+0x04` | handle dwords; `+4` is the handle used by `CreateDynamic` and the table lookup | live |
| `+0x18` | serial (with the pointer key forms a perception reference) | static |
| `+0x20` | render/fade component; fade controller at `[+0x20]+0x240` (start, target, current, elapsed, duration as 5 floats) | live |
| `+0x50` | model container `M`; active index at `[M+0xA0]`; slot pointer `M+8+idx*8` | live (written by an experiment, see 7.3) |
| `+0xB0` | side object; **team id at `[+0xB0]+0x10`** (player 0, enemy 1) | live |
| `+0xDA`, `+0xE3` | wrapper-level activation/death words (wrapper, not actor) | static |
| `+0xE0` | actor-table node | live |
| `+0x110` | position, 4 floats, in centimetres (x, y, z, w) | live |
| `+0x120..+0x15F` | rotation rows. Writing a yaw matrix (`0x5ECE50`) here turns the actor | live |
| `+0x3A0` | extension object ("ext") | live |
| `+0x3B0` | component; `[comp+8]` is the **action state** `st`; `[comp+0x50]` is the AI object | live |
| `+0x893` | "boss" byte set by the creator for placement bosses, not dynamic ones | static, role unknown |
| `+0x8A4` | `2` marks a placeholder actor (AI off, no perception) | static |
| `+0xAB84` | `-1` normally; guard for force-kill | live |
| `+0xB440` | model-hide byte (written by `SetHideModel` `0xA550E8`); see 6.4 | live |

### 3.2 Extension (`[actor+0x3A0]`)

| Offset | Meaning | Tag |
|---|---|---|
| `+0x48` | `1` means dead | live |
| `+0x50` / `+0x58` | HP max / HP current (same fields for player and boss) | live |
| `+0xAC` / `+0xB0` | Ki current / max, floats | live |
| `+0x178` | the Living Artifact ("Tenshin") controller (see 6) | live |
| `+0x188` | flags; bit 1 means Living Artifact active | live |
| `+0x590` | equipped-gear slot words (0x11 of them) | static |

HP writes: native setter `0x66C188(hp+0x38, v)`, falling back to a direct field write that the player's infinite-health feature also uses. Ki writes: `0x2F88D0(st+0x98, v)`. Ki broken: `StateTest 0x23` on the component, or `0x844FE4(st+0x98)`. See `mod/Nioh3Training/src/boss_vitals_game.h`, `player_hp.h`.

### 3.3 Action state `st = [[actor+0x3B0]+8]`

| Offset | Meaning | Tag |
|---|---|---|
| `+0xE8` | pointer to the current action entry; `[entry]` is the u32 action id (`ActIdOf`, `0x358528`); `entry+0x20` is the 336-byte action row | live |
| `+0x10`, `+0x18` | input object (`+0x18`, falling back to `+0x10`); transitions read it through virtual `+0xB0` | static |
| `+0x178` | per-character action tables (kinds C, D, E looked up by `0x49B688`) | static |
| `+0x98` | Ki sub-object | live |
| `+0xA68` | form id used by the Tenshin Begin path | static |
| `+0x1060` | pending-request bits; `0xFC1A20(st+0x1060, 0x4E, 1)` sets the Tenshin request | live (probe) |
| `+0x1068` | input-request flags; bit 14 is a one-shot Tenshin request, not a persistent mode | static |
| `+0x15C4/+0x15C8/+0x15CC` | deferred forced action: id, 0, 0.0f, applied on the next neutral frame when `+0x15C4 == 0xFFFF` and `+0x788 == -1` | live |
| `+0x2E4` | a mode value 1/2, **not** an action id | static |

### 3.4 Team, side and perception

- Teams: player 0, enemy 1 (`DEFAULT_ENEMY_TEAM`). Setters: `0x669510(actor)` SetSideEnemy, `0x65C14C(actor)` SetSidePlayer, and the full runtime change `0x1C272F0(actor, side)` which also rebuilds AI parameters and drops lock-ons held on the old side. The game's own runtime caller `0x2227EE8` calls it with side 0 (enemy to player side). Script binding `Character::SetSide` exists at `0x23981C4` (**static**).
- Perception is a list of 8 entries in the AI's perception object (`[[comp+0x50]+0x10]`). An entry is `{u64 key = *(u64*)actor, u32 serial = actor+0x18}`; empty key is `0xF`. Resolve with `0x20A6F0(perc, i)`. The per-tick fill (`0x288910` / `0x289CB0`) skips same-team actors, and rejects candidates with `+0x8A4 == 2`. This is why a boss on the player's team never targets the player (`notes/training-perception-*.md`, **static** plus live logs).
- Hate: `0x1FF23C8(aiBoss, aiPlayer, amount)` adds hate (`SetInitAccumulatedHate`); it does not make a passive boss approach (**live**).

### 3.5 The player's per-character fields the mods touch

Position `+0x110`, facing rows `+0x120..`, HP/Ki as above, side object `+0xB0`, `NoDeath` replica on the wrapper, `Player::RecoverFull` replica. Equipment, soul core slots and shortcut data live in the inventory object `[0x4751530]` (not the actor): see 6.5.

---

## 4. How the player is driven

### 4.1 Input path (static, from `research/input-thread.md` and `notes/virtual-input.md`)

- A dedicated thread, `[App]InputUpdateWorker`, runs once per game frame (kicked by the frame task, joined later in the same frame). Body: `PadMgrUpdate` `0x43671C` (XInput on up to 4 slots, DirectInput keyboard, Sony HID pads) then `MouseKbUpdate` `0x435698`. The main thread waits up to 1 ms for the pad stage.
- No RawInput, no window-message input. `WM_ACTIVATEAPP` sets one flag read by `IsAppActive` `0x436CF8`; while inactive the pads, keyboard and mouse are ignored.
- The character reads button state through the action state's input object. `0x20CF68` evaluates up to three input conditions per transition (words `+10/+12/+14`, mode bytes `+16..+18`) by calling the input object's virtual `+0xB0`. So the relation "button X triggers action Y" lives in the **action's transition data**, not in a Tenshin-specific switch (`notes/soulcore-attack-route.md`, **static**).
- **The enemy AI uses the same mechanism.** The AI tick (`0x27CC04`, run by `0x86CC50` only while the algorithm flag `+0x1574 == 1`) presses virtual pad buttons (`0x27D1C1 -> 0x73FFCC`); it never picks an action id. Each frame `0x9021E4` walks the current action's transition list, and on a match calls `0x35EF04(state, transition)`, which picks a target id (weighted random over up to 6 target words), then `0x35BC68(st, id, table, layer, slot)`, the **only writer of the current action** (`notes/boss-move-toggle-mechanism.md`, **static** with live hook checks). Script natives that drive this from outside exist: `Character::SetAlgoInputButton`, `SetExtOrderForceInputType`, `SetHoldInputTypeLong`, `SetActionID`, `RequestActionTrans` (all in [symbols/symbols.tsv](../../symbols/symbols.tsv) as `ScriptBind_Character_*`; their bodies are **not yet read**).
- When the algorithm is inactive and `component+0x28` is null, the component update calls a default controller at `component+0x18`, virtual `+0x18` (`notes/training-yamagata-idle-contract.md`, **static**). So AI-off does not freeze the whole Character.

### 4.2 What the mods do to the player's input

| Mechanism | Where | Status |
|---|---|---|
| **Virtual pad** over a named pipe | `mod/Nioh3EngineFix/src/vpad.cpp`, `[Debug] VirtualPad = 1` (exclusive) or `2` (merge). Pipe `\\.\pipe\nioh3vpad`, one text line per command: `press`, `hold`, `release`, `stick L\|R x y [ms]`, `trigger`, `key`, `menu get\|set`, `state`, `ping`. It hooks the game's `XInputGetState` IAT slot, so the game cannot tell it from a real pad. | **live**. Walked the player 367 units unfocused. |
| `pad <btn hex> <lt> <rt> <lx> <ly> <rx> <ry>` sets a whole state in one step; used by the input recorder/replayer `tools/testmatrix/padrec.py` | `vpad_core.h` | *(source not yet in the public copy)* |
| **Keep running unfocused** | `[Debug] KeepRunningUnfocused = 1`; retargets the three `IsAppActive` pad/pause callers (`0x436758`, `0x436A20`, `0x89B2FD`) to a thunk | **live** |
| **Button masking while a form plays** | `FormBlockPad` (default `0xFF4F`), `FormBlockRT`, `FormBlockLS` in `bosslog.cpp`: an IAT hook on `XInputGetState` clears buttons and triggers so the player cannot act while hidden. The IAT slot is taken on the first form start so it sits outside EngineFix's pad hooks. Keyboard is not masked. | **live** (shipped) |
| **Queued d-pad pulse** | `mod/Nioh3BossRematch/src/press_queue.h` replays a soul-core press as a 120 ms d-pad pulse through the XInput hook | *(source not yet in the public copy)* |
| **Pad/trigger observation** | EngineFix `padlat.cpp`, `inputhist.h`, `padviz.h` | live |

Take-away: **injecting a full controller state at the XInput layer is solved and live-tested.** Mapping those buttons onto something other than the player's own character is not.

### 4.3 Making the player perform actions

The mods already put the player (and bosses) into specific actions:

- `0x66C4A8(st, id, 0, 0.0f)` is the game's own forced transition (what `SetActionID` calls). It clears pending fields, calls virtual `+0xF8` with `(st, id, null, -1, 0)` and ends with `0x66C544(st, 1)`. Calling `0x35BC68` alone skips player bookkeeping. If the id does not resolve, `0x35BC68` silently falls back to action 0. **live** on bosses (`MV_FORCE`, `DrForceNative`), **probe only** on the player (`rm tenshin move <hex>`).
- The deferred form (write `+0x15C4`) is gentler and **live** on a boss: took effect within about 15 ms at idle.
- Player action ids seen: `0x286x` class for the shared player seal animations (`28FD`, `28FB`, `28B6`, running throw `28B2`), right talisman `0x2718` (`notes/soulcore-item-29.md`). `notes/player-action-events.tsv` has the player's action table events. The player and boss share one id space and one state class.

---

## 5. Triggering moves and the boss move catalogue

### 5.1 Data

| File | What it holds | Generator |
|---|---|---|
| `mod/boss_moves.h` | `N3Move {chr, id, boss, label, kind, ms}`; kind `A` attack, `S` wind-up, `L` combo link, `T` twitch, `M` movement/system, `I` idle, `0` unclassified; `ms` typical run time. Labels are observations, "unverified". | `tools/gen_boss_moves.py` *(not in the public copy)* |
| `mod/boss_sequences.h` | `N3Next {chr, id, next, loop}`: the action's "next on finish" (`+0xE0` of the row, **proven** on Yoritomo against logs; `0x2` is idle, a self id loops) | `tools/gen_boss_sequences.py` |
| `mod/boss_move_facts.h` | per-move hit windows, unblockable flag, from the boss's own action file | `tools/gen_boss_move_facts.py` |
| `mod/move_catalog.h` | run-time catalogue merged from the static rows and the live action file | n/a |
| `mod/Nioh3Training/Nioh3Training_action_records.tsv`, `_actionsets.tsv` | archive-derived hit counts and damage per action; action-object hashes per character | `tools/gen_actionsets.py` |

### 5.2 Reading a boss's moves at run time

`mod/Nioh3Training/src/auto_moves.h` plus `auto_moves_game.h` read the boss's loaded action file from memory when it spawns, so no hand table is needed. Layout of an action file (see `notes/boss-action-record.md`):

- `u32 n = 16`, then 16 `(offset, size)` sub-tables.
- Sub-table 1: **336-byte (`0x150`) action rows**. `+0x0C` u16 id (proven), `+0x20` u16 motion number, `+0x04` bit `0x2000` great attack (unblockable; proven on Yoritomo), `+0xE0` u16 next action, `+0xF0` u32 action-object hash, `+0x13C/+0x142` attack-block words.
- Sub-table 2: **192-byte hitbox rows**. Damage is hitbox-row word `+0x10`; hit count is the number of hit windows, there is no hit-count field. Several actions can share a motion; each action row owns its own list.
- No duration field on the row; length is in the motion clip.

This is the full list of what a boss can do, with transitions. Idle is id `0x2`. Locomotion ids are below `0x4E20`; attacks start at `0x4E21` on the bosses studied (Yoritomo 173 rows, Shingen 136, Otoroshi 141).

### 5.3 Selecting and vetoing moves (live)

`mod/Nioh3Training/src/move_toggle.h` hooks `0x35EF04` (target pick) and the `0x35BC68` backstop. It can veto ids (re-roll up to 8 times, otherwise return idle `0x2`; returning `0xFFFF` makes the boss retry every frame and never act), force an id, repeat a move, repeat the last 2 or 3 moves, and follow `next` chains. `drill.h` forces a chosen move every gap while the AI is off. Both are **live**. They show that **a boss will play any action id handed to `0x66C4A8` regardless of AI state**, with the caveat that a forced action started with the AI off was not proven for every boss, hence the fallback in `drill.h` that turns the AI on for the drill.

---

## 6. Soul Transformation and the Living Artifact ("Tenshin")

Two different things are in play. Keep them apart.

### 6.1 What the mod's "soul core transformation" actually is (shipped, live)

`[Spirit] SoulCoreForms = 1` in `Nioh3BossRematch.ini` (menu: Player settings, "Soul core transformations"). The game's native soul core is a **summon**: using a core on the d-pad throws a Summoning Seal, which spawns the yokai through `0x65A8A8`. The mod does not make the player a yokai. It makes the summon look like a transformation:

1. Hook the seal spawn (`FormKeySealNote`, `SpiritSpawnNote`); learn `desc[0]`, the character id, from the spawn.
2. **Hide the player** (`[Spirit] FormHide = 4`: zero the native character fade controller at `[[actor+0x20]]+0x240`, because no combination of `B440/B444/85860` hides every part; see `notes/formhide4-native-fade-*.md`). `FormHide 1..3` are earlier modes (native `SetHideModel`, direct byte).
3. **Puppet the yokai on the player's position** (`FormPin`), or for travelling cores **warp the hidden player onto the yokai every frame** (`FormFollowSeal`, using the non-player warp `0x669AB8` plus rotation rows). The camera keeps following the player, so it follows the yokai.
4. Mask all pad input (`FormBlockPad`), end the form when the attack ends (`SealEndOnHit`, `SealEndOnIdle`, `FormHideUntilEnd` watches the Living Artifact flag and the actor table), unhide.
5. Camera: `mod/Nioh3CamKiProbe` follows the yokai's Y (vertical follow) using `mod/shared/soul_camera_vertical.h` *(source not yet in the public copy)*, 20 ms exponential follow and a smoothstep entry/exit.

Also in this feature: `FormCrouchCancel`, per-core `FormCoresOff` (which cores transform, keyed by character id), `[Spirit] FormHideDelayMs`, the `Player > Soul core tuning` page (`soul_core_tuning_menu.h`), and `FastThrow` (motion-rate hook, confirmed only for seal animation ids). `spirit_upgrade.h` migrates old inis.

Status: this is a puppet and presentation layer. The yokai plays its own seal motion under its own AI; the player does not steer it. Do not assume it already gives control of a non-player body.

### 6.2 Core item to yokai character (live, read-only)

The mapping is read from the game's own tables, with no hardcoded core ids (`mod/Nioh3BossRematch/src/form_core_equipment.h` *(source not yet in the public copy)*; `notes/soulcore-core-to-seal-*.md`):

1. Core item row in the item master (`GameData+0x68`, stride `0x1A0`, ~3362 rows): `+0x152` u16 item id, `+0x64` category `900000000` for soul cores (119 rows), `+0x180` u16 core-parameter key.
2. Core-parameter row (`GameData+0x118`, stride `0x54`, 84 rows): `+0x0C` key, `+0x18` Yang item (the Summoning Seal), `+0x1C` Yin item (talisman).
3. Seal item row: category `700000000`, `+0x54` u32 is the **summoned character id**, the same value as the spawn's `desc[0]`.

`GameData = [0x45B9E30]`. 117 of 119 cores map. Verified live on three cores. Equipped cores are at `inv+0x23B6B8 + slot*4` (inventory index, `0xFFFFFFFF` empty; slots 0 and 1 are Yang), item records at `inv+0x10 + i*0xF0`, count at `inv+0x927D0` (`inv = [0x4751530]`).

**Why this matters here:** it is a ready-made table from "item the player owns" to "character id the game can already spawn as a friendly summon". Those yokai characters are the cheapest set of non-boss bodies to experiment on.

### 6.3 Equipping cores and shortcuts

The Training mod can equip a core from its menu (`mod/Nioh3Training/src/soul_core_swap.h`, `soul_core_swap_game.h`, *source not yet in the public copy*). It replays the shrine handler's valid path in order: `0x2285C48` (the game's own "can this core go in this slot" check, which can refuse), `0x216B9D0` (old shortcut), `0x21607A4` and `0x2183744` and `0x215F85C` (write slot, rebuild list), `0x2182968` (move the shortcut), then the HUD refresh. All steps on the game thread. **live**.

Item shortcut script natives are `Item::SetShortCut` `0x23A210C`, `Item::FindShortCut` `0x23A191C`, `Player::GetActiveItemShortcut` `0x23A8474`, `Player::IsItemSetActiveShortCutSlotDirection` `0x23AA098`. Guardian-spirit natives (script level, mission-driven): `Player::AddGuardianSpirit` (`0x23A7714`), `EquipGuardianSpirit` (`0x23A7FF4`), `DelGuardianSpirit`, `HasGuardianSpirit`, `GuardianSpiritForceLost/Return`. The seal test showed the seal yokai is **not** a guardian-spirit spawn (`0x2916C5C` is mission-script only and never ran).

### 6.4 The game's own player transformation ("Nioh Tenshin", Living Artifact)

Script natives `Player::IsNiohTenshin` (`0x23AA2F4`), `ForceEndNiohTenshin` (`0x23A8218`), `SetNiohTenshinGaugeReduceSpeed` (`0x23AB3E4`) and the HUD text "End Living Artifact" show this is the game's own player form mechanic. What we know:

| Item | Detail | Tag |
|---|---|---|
| Controller | `T = [actor+0x3A0] + 0x178`. `T+0x10` flags (bit 0 ready), `T+0x14` type, `T+0x20/+0x24` gauge current/max. Active flag is `ext+0x188` bit 1. | live (read and written by the `rm tenshin` probe) |
| Begin / End | Begin `0x843010` (sets controller bit 1); End `0x1AB4C5C` (clears it and calls the native unhide). Begin has a prologue guard before the probe runs it. | static, probe live |
| Request path | The button path sets request bit `0x4E` via `0xFC1A20(st+0x1060, 0x4E, 1)`. The state dispatcher `0x2E9251` calls `0x2E6C34(st)`, which checks bit 14 of `st+0x1068`, and (if not already active) runs `0x843010` and `0x74C1DC` (RefreshForm). | static |
| Form selection | RefreshForm resolves the equipped core's item definition with `0xF4DBC` (call site `0x74C27F`) and takes the form id from `[def+0x58]`; it writes `st+0xA68`. `SetForm` is `0x74C2FC` plus `0x74C128`. There are 17 forms. | static, hooked in the probe |
| Form entry actions | Executable-chosen: form `6E73` uses action `628B` (`0x2E6DC9`), form `5455` uses `6248` (`0x2E6DE7`); forms `1302/1909/5095` enter with `628C`. Called as `0x66C4A8(st, id, 0, 0.0f)` at `0x2E6D18`. | static |
| Attack routing | Normal pending bits reach `0x9021E4(st, slot)`, so attacks come from the **active form's action transition data**, selected by `0x35EF04`. Do not latch bit 14; it is a one-shot request. | static |
| Body packs | Each form has a body pack loaded through `0x3F9C10` / `0x1A8B990`; the probe can preload and report `entry+0x2C8` for all 17. | live (probe) |
| Hide | `SetHideModel` `0xA550E8` writes `[chr+0xB440]`; Tenshin End unhides through the same byte. | live |

**What is not known (be honest):** (a) whether these 17 forms are "play as a yokai" bodies at all; the notes say the HUD proves only the mode, not which transition table was selected. (b) The outcome of the form-`6E73` one-shot attack experiment is not recorded in the notes. (c) The form definitions (action tables, body packs) are game data, which this repo cannot hold (PUBLIC_EXCLUSIONS). (d) The `rm tenshin ...` commands are dev probes (compiled with `BR_SOULCORE`); the shipped feature in 6.1 does not use them. The probe's design note `notes/player-transform-nioh-tenshin.md` is referenced by source comments but is not in the tree.

---

## 7. Enemy AI: aggro, holding, the chain guard

### 7.1 Turning the AI on and off (live)

- `0x668D74(actor)` AI off; `0x66982C(actor)` AI on. Both take the Character pointer. Training's "Stand still" stance is exactly this (`idle_hold.h`): AI off, re-applied every 100 ms when the game turns it back on, boss kept in idle `0x2`. Never writes action state, position, player input or animation time.
- `0x1FF1FA0(ai, 1, 0)` `SetAwareFlg`. `AwareFix` in `Nioh3Training.ini` controls the extra aware/lock-on calls (`0x68210C`), default 0.
- Aggro helper (`aggro.h`, `[Training] BossAggro`, `AggroRangeCm`, `AggroFace`): every 250 ms set aware, make the player the current perception target (`0x2BE2E8(perc, idx)`), add hate, and when the boss is farther than the range turn the AI on so it walks up, then resume the drill. **live**, with a cosmetic log flood.
- Reset position/facing: the game's own non-player warp `0x669AB8(ch, pos, 0)` plus rotation rows `+0x120..` (yaw matrix `0x5ECE50`) and `0x66C330(ctrl, 0)`. Do not use `CharacterSetPositionYaw` `0x1C27010`; it forces action 0 and writes the player-focus position.

### 7.2 Behaviour that matters for control

- An idle boss notices the player only within about 10 m. `BossDistance` default 900 cm in Training is the working number (**live**).
- Spawn behaviour: the boss backs off from 3 to 5 m in the first 3 s, then closes in (**live**).
- A "passive spawned boss" appeared when the owner was the player (boss created on side 0). Root cause found statically; fix is the runtime side change (see 3.4). `[ArenaSwap] SideFix` and `TeamFix` exist as code defaults of 1. A later run could not reproduce passivity with a live owner. Whether the side fix cures the no-live-owner case is **not recorded**.
- HP-phase logic for story bosses lives in mission scripts (`GetHpRate` thresholds, `SetAIEventFlagForUnit`), not in character data (`notes/boss-ai.md`). A boss spawned outside its mission will not run its scripted phase changes. The AI itself is an engine "AIGraph" system; no per-boss AI script files exist in the archives.

### 7.3 Protecting the process

`mod/Nioh3EngineFix/src/aichainguard.h` *(source not yet in the public copy)* is a vectored-exception guard for the boss-AI chain walker (`0x3103B8`) copy loop at `0x310698`. When an action's runtime slice claims more rows than are mapped, the guard resumes at the loop exit instead of crashing. Forcing odd actions on bosses is the kind of change that exposes bad slices. Read it before you rely on forcing ids on bosses you have not tested.

---

## 8. Camera

- `Nioh3CamKiProbe` hooks `0x2B0E4C` (the third-person camera's orbit entry). Live fields (from the camera update `0x2AEAA8`): `cam+0xB0` boom length, `cam+0x40` lens (radians), eye is `[cam+0x310]` after collision. Distance and lens are written at the blend `0x9565E4`, so the game's own scripted blend still runs on top. Camera manager at `[0x4751490]`. Offsets ease back to zero while any game screen is open.
- The camera follows the **player's** position. Nobody has found or swapped the camera target pointer ("no native target-pointer swap" is an explicit design choice in the soul-camera note). The proven workaround is the one in 6.1: move the hidden player to the controlled body every frame so the camera follows it.
- Y-only follow of the yokai is implemented as a spring on top of the CamKi output (`notes/yokai-camera-*.md`, `notes/camera-blend-*.md`). Live visual smoothness was recorded as unverified in the notes.
- Lock-on is not covered: nothing here documents how to make enemies lock onto, or the player lock onto, a non-player actor beyond `0x68210C` and `SetAlgoLockOn` script natives (not read).

Animation system notes are in [docs/subsystems/animation.md](../subsystems/animation.md): pose sampling `AnimClipSamplePose` `0x889C0`, blend graph `AnimBlendNodeEvaluate` `0x6D360`, skeleton build `SkelBuildBoneMatrices` `0xCC940`. Those are cost-profile notes; they do not describe the retargeting or rig compatibility questions you will need (see 9.3).

---

## 9. Suggested approach

### 9.1 What you are choosing between

| Option | Idea | Reuses | Main risk |
|---|---|---|---|
| **A. Possess a spawned boss** | Spawn the boss as today, switch its AI off, and feed it commands from the real pad. | spawn/despawn, AI off, forced actions, move catalogue, XInput layer, hide-player-and-follow trick | Locomotion, hit/damage plumbing, lock-on and camera for a non-player actor are unmapped |
| **B. Swap the player's model or moveset** | Replace the player's model slots and action table with the boss's. | model-slot write experiments | Rig and skeleton mismatch, player-only systems (equipment, style, animation graph) |
| **C. Extend Tenshin / soul forms** | Add or repurpose a form so the game itself does the transformation. | Tenshin Begin/End, form entry action | Form definitions are game data; new bodies cannot be authored without extracted assets, which this repo cannot hold |
| **D. Ally boss (stepping stone)** | Boss on the player's side with AI **on**. | `0x1C272F0(actor, 0)`, spawn | Not control, but proves side, perception and damage rules with no input work |

**Recommendation: A, staged, with D as the first milestone.** B and C need data and engine knowledge this project does not have. A is the only option where every primitive is already live-proven somewhere in this repo; what is missing is joining them and the input-to-action mapping.

### 9.2 Option A in stages

**Stage 0. Scaffolding (no input yet).**
- Add a Training-hosted "possession session" next to the Boss Match session: spawn one boss through `SummonStart`/`CreateDynamic`, track it in a slot, keep `SessionNoDeath` style protection on the *player body* (it will be hidden).
- Decide how `PlayerChar()` is used. Many mod subsystems treat "actor != PlayerChar()" as "a boss" (`AgBosses` in `aggro.h`, frame meter, boss vitals, the move toggles). A possessed boss is both. Introduce an explicit "controlled actor" notion rather than re-pointing `PlayerChar()`.

**Stage 1. Ally boss (option D).**
- Spawn with owner = player, then call `0x1C272F0(actor, 0)` (side 0). Turn AI on. Observe: do enemies and traps target it? Does it fight on its own? Does it take damage from enemies? This is **guess**: the side setter is proven for enemies, not shown on bosses going the other way. Expect trouble with boss-only AI (it may wait for script flags).
- Useful for answering the hit/damage and perception questions early.

**Stage 2. Move-per-button control.**
- AI off (`0x668D74`). Read the move list with `auto_moves.h`. Map pad buttons to move classes (light attack, heavy, special) using kinds `A`/`S`/`L` and the `boss_sequences.h` chains so a button advances through a combo via `next`.
- Trigger with the deferred write (`+0x15C4`) when the boss is at idle `0x2`, or `0x66C4A8` for immediate. Gate on the same readiness checks `drill.h` uses (idle reached, gap elapsed).
- Read the pad from your own `XInputGetState` hook or from the existing vpad pipe for testing. Mask the real buttons from the player body (`FormBlockPad`-style) so the hidden player does not also act.
- Expect to veto the boss's own random picks while AI is off; `move_toggle.h` shows how.

**Stage 3. Movement and camera.**
- Locomotion ids (below `0x4E20`, kind `M`) are what the AI chooses to walk. Either force them per frame from the stick (guess: the ids are step/run loops that respond to a heading) or drive position directly with `0x669AB8` and set facing rows (both live on bosses, but only as repositioning, not animated locomotion). A hybrid (force a walk loop, nudge position) is the likely practical result.
- Camera: warp the hidden player onto the boss each frame (the `FormFollowSeal` method), keep CamKi's vertical follow.

**Stage 4. Combat plumbing.**
- Hits that the boss deals use its own hitbox rows and the target's side. The player body is hidden and on side 0; make sure enemies, if any, target the boss (side 0) and that the boss does not damage the player body (invulnerable via `NoDeath` and `PlayerCantDie`-style refill).
- HUD: HP/Ki bars read the player's fields. The boss's HP and Ki are at the same offsets; mirror them into the player's fields or hook the readers. **guess**.
- Death: handle the boss dying (`0x4892A8`) and end the session; `owned_registry.h` and the `DespawnDeadDefer` logic show the care needed.

**Stage 5. Release hygiene.** Offline-only guard (see 10), a menu page, a `catalog.json` entry, tests under `mod/Nioh3Training/tests`, and a no-hardcoding audit like the rest of the repo.

### 9.3 Why B and C are hard (what is missing)

- **Model/skeleton.** Each character has its own rig and animation set; the player rig is a human. An experiment wrote the yokai's model pointer into the player's model container (`SpiritBody`, `[Spirit] Body`) and swapped the player's kind-0 slot ids (`FormSlot0`, then a part rebuild `0xF3478(chr+0x9F0, 0)`); the final shipped design hides and puppets instead. The visual outcome of the swap is not recorded in the notes. Whatever animates the swapped model would still be the player's action table. Treat B as unproven.
- **Player-only systems.** Equipment (`ext+0x590`), the style system, the Ki pulse, camera lock-on and the HUD assume a player.
- **Tenshin forms** use game-owned body packs and action tables; no authoring path.

### 9.4 RE still missing for option A

1. The exact ABI of the AI's button injection (`0x27D1C1 -> 0x73FFCC`) and of the input object that `0x20CF68` queries. Driving buttons instead of action ids would let the boss's own transition data decide combos, timings and locomotion. **Highest value.**
2. Who owns `component+0x28` and the default controller at `component+0x18` (virtual `+0x18`), and what a "player-style" controller on a non-player actor would require.
3. How lock-on targeting selects actors (the perception insert `0x2BE2E8` is known for the enemy side; the player's lock-on code is not mapped).
4. Whether forced actions with AI off work for every boss and every state (hit reactions, Ki break, stagger). Only Yoritomo, Shingen and a few others were tried.
5. Damage plumbing when the attacker is on the player's side: hit filtering by side, friendly-fire flags, finisher handling.
6. Boss scripted phases: which parts of a multi-phase boss are in mission scripts (not present when you spawn it dynamically) and which are in character data.
7. HUD readers for HP, Ki and gauges (the boss gauge has an id-resolution quirk: `UI::ShowBossGauge` resolves by owner placement, so a fake id was needed; see `FakeGauge*` in `training.c`).
8. Body-pack and form definitions for Tenshin (6.4).

---

## 10. Risks and rules

- **Offline only.** The Training and Boss Match code is not guarded against online or co-op; use the signals in `notes/online-session-detect.md` (net manager connected byte at `[0x45C84F8]+0x1DC`, play-mode word at `[0x4709B50]+0x34`) and refuse to run when they say online. A mod that changes actors, sides and input must never run in a multiplayer session.
- **Crash surface.** Every game call in the mods is wrapped in SEH and gated by a byte-pattern check. Keep to that. Several crashes in this project came from stale actor pointers (for example the boss gauge reading a despawned actor), and from publishing a trampoline after the live patch.
- **Threads.** Game-state calls run on the game thread (the Training pump); UI threads hand off through atomics. Do not call natives from the menu thread.
- **Save safety.** Boss kills with drop, mission flags and clear flags touch the save and rewards. The Training session blocks the native death/game-over flow with `NoDeath` and keeps a heal-in-place option. Anything that lets a boss body take the player's place must not write progress.
- **Version lock.** Everything is v2.02. After a game update, the pattern checks will refuse to install; that is by design.
- **Public rules.** Obey [PUBLIC_EXCLUSIONS.md](../../PUBLIC_EXCLUSIONS.md). Share RVAs, names and short explanations. Do not commit decompiled bodies, extracted game text or any game bytes. The `tools/` scripts that build local tables work on your own install and write to a git-ignored directory.

---

## 11. Open problems / known gaps

Collected from source comments and the notes cited above. Nothing in this list has a recorded fix.

**Spawning and lifecycle**
- No admitted arbitrary-character factory. `CreateDynamic` has the reservation hazards in 2.2; a transactional adapter with an offline double is the recommended design and is not built.
- `owned_registry.h` is "offline-tested, not wired to native creation/deletion".
- Resource release is by an eviction pool; a ready-check that walks the full dependency closure (including type-4 companions) is a recommendation, not shipped.
- Arena scripts (fog gate, intro, clear flags, BGM) do not run for dynamic bosses. Boss BGM from another world being resident when called is unverified.
- Memory cost per resident boss is unmeasured.

**Control and input**
- The player is only ever driven through the XInput layer. The game's AI input-injection path and the input object are unmapped (9.4 item 1).
- Locomotion control of a boss is untested. The fact that forced actions play with the AI off is proven on several bosses, not all; `drill.h` carries a fallback for the unproven case.
- `FormBlockPad` does not mask the keyboard.

**Side, perception, aggro**
- Boss-on-player-side (ally) behaviour is untried. The runtime side change is proven only enemy-to-player for the game's own caller and player-to-enemy for the mods' spawn fix.
- Whether `SideFix` fully cures passive spawns with no live owner is not recorded. Hate alone does not cause approach.
- `0x893` ("boss" byte) role unknown.

**Transformation**
- The 17 Tenshin forms: what they are, whether any is a yokai body, and the form-`6E73` attack experiment result are all unrecorded.
- The soul-core "form" feature is a puppet. It has no steering, and live visual smoothness of the camera follow was listed as unverified.
- The "use" path of a soul core item (dispatcher, stack decrement, transformation-history row written by `SaveWriteSoulCoreHistory` `0xA55B94`) was not fully traced; the notes give a log-only hook plan.
- Keyboard-only core shortcut presses are not detected by the pad-based learning.

**Camera and UI**
- No camera-target swap. Lock-on for a non-player actor is unmapped.
- Boss gauge resolution by owner placement means a dynamic boss shows no gauge of its own without the fake-id path.

**Data and docs**
- Several referenced notes are unpublished (`notes/player-transform-nioh-tenshin.md` is referenced by source comments and absent from the tree).
- There is no `TECH_DEBT.md` in the public tree or in this brief's source snapshot; the open items above come from source comments and notes instead.
- Boss labels in `boss_moves.h` are observations, not verified names. Many rows are still kind `0` (unclassified).
