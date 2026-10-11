# Mission / game logic (script VM, save tables)
CPU share 0.6 % (Ally 0.1 %, PC 1.1 %); coverage 97.5 %. Threads: [App]GameModeDrawThread (MissionThreadStep, GameModePostUpdateEntities) and Scheduler0 (GameModeUpdateMain). A separate [App]MissionThread exists (~14-18 % CPU on Linux, mostly futex wait).

## Role
Per-frame mission update and a bytecode script VM. The bulk of the game logic (frame step 5ACF80 -> 5AC9EC -> 5651D0 -> 39E4E4) is object update, which is on the Jobs and Effects pages.

## Key data
- `MissionThreadStep` 20B900 (only caller E64700): locks 4759950/4759940, refreshes the player transform, per-object follow `MissionObjFollowParentTransform` 20BD44; `GameModePostUpdateEntities` 20B878. Despite its name, this runs on [App]GameModeDrawThread, not the MissionThread.
- Script VM: `ScriptVmRunOpcodes` 13A830 reads u16 opcodes from byte stream +70h (pos +80h, size +7Ch) until halt bit +9Ch&2; `ScriptVmResolveVarRef` 13B590 (27 callers), `ScriptVmCallNative` 13B18C (builtin table [c[19h]+28h], index < 15), `ScriptVmOpCallScriptFunc` 13C574, `ScriptThreadSetTick` 282320, `ScriptCtxIsAtEnd` 282574.
- Game state g_4751848: `GameContentIdIsAvailable` 284ED4 (paired ids). Save tables: 1500 records x E8h at [o]+927D8h (`SaveItemTableGetCountById` 2FA4C4), 2500 x F0h at g_4751530 (`ItemBoxScanFlaggedEntries` F450C).
- `ObjFarFromPlayerOrPending` 4AB6E4 (distance >= 10000 to player).

## Hot functions
| RVA | name | % |
|---|---|---|
| 20B900 | MissionThreadStep | 0.25 |
| 284ED4 | GameContentIdIsAvailable | 0.07 |
| 4AB6E4 | ObjFarFromPlayerOrPending | 0.06 |
| 13B590 / 13A830 | ScriptVmResolveVarRef / RunOpcodes | 0.04 each |

## What drives the cost
Active scripts and mission objects; nothing here shows up on the render main thread. Over-ticking (44.6 logic ticks/s at 16 fps on the Ally) multiplies it, see the Jobs page.

## Deeper notes
notes/mainthread-logic.md, cpu-hotspots-ally.md (TG tick governor), game-data.md.
