# Effects / Post and per-object update
CPU share 3.7 % (Ally 1.9 %, PC 5.6 %); coverage 98.7 %. Threads: [App]EffectUpdateThread x10 (9 "Disp" + 1 "NoDisp"), KIDSTaskScheduler0 and workers (object update).

## Role
Particle/effect instances plus the generic per-object update that the area heuristic groups here (name contains "Effect"/"Particle"). Post-processing itself is GPU work (see notes/gpu-post-*.md).

## Key data
- `EffectManagerUpdateInstances` 15FC80 walks every live effect instance each frame: displayed ones go to the 9 Disp workers; non-displayed ones (out of range, owner hidden/faded) go to the single NoDisp worker, which runs the full update serially (kicked at end of game update, joined at the start of the next; at most ~7.6 % of a core at the shrine).
- Particle emitter modules: `ParticleEmitterUpdateModules` CE7E0 (vtable slot 3BE57E8, spawn via 2ECE610), Alt C8958 (vtable 3BE46A8).
- Object update chain 182000/204584 -> 408FE0 -> `ObjUpdatePerObject` 1A7BF0 (44.6 KB): part slots o+240h, attachments o+400h, resource attachment list o+170h (`ObjUpdateAttachedEffects` 1A7408), field o+3B0h (`ObjSetField3B0Propagate`), dirty transforms (`ObjNodeFlushDirtyTransform` 4C2DA0, bits u16 +56h).
- Material bind: `MaterialBindS2FAcclParams` 3C4B74 (slot 10, 1/(far-near)), `MaterialParamNameEquals` 3C7750 (hash then strcmp per node).

## Hot functions
| RVA | name | % |
|---|---|---|
| 1A7BF0 | ObjUpdatePerObject | 1.39 |
| 3C4B74 | MaterialBindS2FAcclParams | 0.55 |
| 168E80 | Vec3CopyW0 | 0.38 |
| 1A6FC4 | ObjPartsAnyVirt508 | 0.20 |
| 1A7408 | ObjUpdateAttachedEffects | 0.15 |
| 3C7750 | MaterialParamNameEquals | 0.11 |

## What drives the cost
Work scales with all loaded effect instances, not visible ones (invisible effects simulated every frame). Neither effects nor animation run on the render main thread, so they only matter
if Scheduler0 becomes the limiter (heavy fights; no combat capture exists). MOTION_QUALITY is dead in v2.02 (feeds an unread field).

## Deeper notes
notes/effects-animation.md (pool sizes Disp 9 / NoDisp 1, MOTION_QUALITY), mainthread-logic.md, gpu-post-exe.md and gpu-post-runtime.md (post passes), settings.md.
