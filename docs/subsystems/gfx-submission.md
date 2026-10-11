# GFX / D3D12 submission (draw prep + KTGL token stream)
CPU share 22.6 % (Ally 33.5 %, PC 11.7 %); coverage 99.7 %. Threads: [App]Game (main, draw prep), SynergisticDispatcherTask (D3D12 replay), Present thread.

## Role
The main thread does not call D3D12 per draw. It walks the scene draw lists, filters state against shadow copies and records tokens into KTGL's own deferred
command stream (`KtglContext+0x3C40`); full segments are handed to a ring (`KtglCmdStreamFlush` 254110) and the dispatcher thread replays them into D3D12.
The main thread then spins at frame end until the dispatcher drains (418F40, counted on the Jobs page).

## Key data
- `KtglContext`: token stream +3C40, `CmdFlushLimit` +3C70 (copied from device-param +804), shadow state ctx+25xx, dirty bits ctx+12E8 (40h = re-resolve binding set).
- Draw list: items grouped by equal `object+0x18`; list type 3 uses a branchless binary search for the group end.
- Slot tables: ctx+2578h / ctx+2568h paths (`StageSlotTableUpdate2578/2568`, slots at +90h).
- Render/post parameter block: 29.6 KB copied field by field (`RenderParamsAssign` 247950, 29 callers).
- Object entry table o+340h, per-object flags o+4Ch (`ObjEntryTableMatch`, `ObjCountValidSlots`).

## Hot functions (combined self share)
| RVA | name | % |
|---|---|---|
| 1E80B0 | DrawEmit (per draw per sub-pass, 17.5 % incl) | 5.18 |
| 61B1C | WcUploadZeroFill (UI glyph atlas padding clear, write-combined) | 3.69 |
| 39E20 / 3BC0C | DrawListWalk / DrawListGroupLength | 1.80 / 1.19 |
| 1DAF6C | DrawStateCommit (160 callers) | 1.12 |
| 1D8720 / 1D84A0 | StageSlotTableUpdate2578 / 2568 | 1.06 / 0.80 |
| 1D7340 | ShaderConstWriteStream (tokens 2B/2C/37/38 + memcpy) | 1.01 |
| 247950 | RenderParamsAssign | 0.94 |
| 425B0 | ObjCountValidSlots | 0.83 |
| 113204 / 535188 | FrameRenderViews / per-view scene renderer (53 KB stack frame) | 0.46 |

## What drives the cost
Number of draws and objects (cache-missing pointer chases), not API calls: state is already shadow-filtered. Main thread splits ~35 % draw walker 535188,
13-19 % render-frame-begin 4A529C, 6-8 % UI (corrected 2026-10-08, see notes/particles.md section 6: 365AA98 is EffectViewRecordSync, mostly effects), rest other render-graph nodes. The atomic-load idiom `lock cmpxchg [m],0` in per-draw paths serializes the chases.
GPU-bound on the Ally, so CPU savings there are fps-neutral. The dispatcher is ~45 % idle (amdxc64 26 %, D3D12Core 6 %).

## Deeper notes
notes/mainthread-drawprep.md (ranked A..G), parallel-drawprep.md, d3d12-submission.md (P1 Present wait, B1 barriers, K1 flush limit), gpu-barriers.md,
mainthread-logic.md (what [App]Game really is), ui-mainthread.md (glyph atlas, 61B1C), worker-threads.md.
