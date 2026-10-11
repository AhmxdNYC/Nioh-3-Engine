# Streaming / Resources (texture streaming, resource readiness, bounds math)
CPU share 9.8 % (Ally 11.7 %, PC 7.9 %); coverage 99.7 %. Threads: [App]Game (render-frame-begin, 4BBEC), Scheduler0/workers (bounds), KTGLGraph.Cache.FileLoad(+Poll) file threads (low).

## Role
Feedback-driven material texture streaming each frame, resource readiness checks (1312 render-node callers), and part bounds (OBB to AABB) math used by update jobs.

## Key data
- Texture streaming: visited array M+888h, slot count M+124h, per-texture stamp +48h (`TextureStreamTouchSlot`), mip-tile pyramid requested +120h vs resident +D8h, tile stamp +188h, <= 16 tile requests per texture per frame (`TextureStreamRequestTiles`); block-allocator slots re-acquired via 1283A8.
- Resource: dependency list r+40h walked by `ResourceDependenciesReady` 1A6820; `ResResolveDependencyArray` 1A009C builds a temp array from d+214h/+27Ch/+298h.
- Bounds: `BoundsFromPartObbs` 71C60 (vfunc +E8h base, per-part SinCos/ObbToAabb); math leaves Vec4AddInPlace 6EFD0, Vec4Scale 6F010, Vec3Dot 6F060, `TransformedBoxAabb` 6FED0 (Arvo).
- Vertex stream pack 70230 (4.3 KB, float3/4 attribute copy).

## Hot functions
| RVA | name | % |
|---|---|---|
| 1E7050 | TextureStreamUpdate | 1.82 |
| 128EF0 | TextureStreamTouchSlot | 1.52 |
| 6EFD0 | Vec4AddInPlace (ObbToAabb callee, 71 callers) | 0.92 |
| 1A6030 | ResourceIsReady (poll-style predicate) | 0.90 |
| 1290B8 | TextureStreamRequestTiles | 0.76 |
| 71C60 | BoundsFromPartObbs | 0.52 |
| 1A188C / 1A6820 | ObjHandleResolve / ResourceDependenciesReady | 0.42 each |

## What drives the cost
Visible material slots x textures per frame (pointer chasing); bounds math scales with updated objects (ObbToAabb 6EA50 = 6.5-8 % of Scheduler0, see the Animation page).
Loading hitches are separate (notes/load-memory.md).

## Deeper notes
notes/meshlet-mainthread.md (1E7050/128EF0/1290B8, P1E7 prefetch, S1E7), mainthread-logic.md (AABB cave), load-memory.md, stutter.md, work-deciders.md (streaming budgets).
