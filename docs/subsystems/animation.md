# Animation, skeleton and bounds math
CPU share 1.5 % (Ally 0.6 %, PC 2.4 %); coverage 97.6 %. Threads: KIDSTaskScheduler0 and job workers (character update inside the game update, GameModeUpdateMain 39E4E4); not on [App]Game.

## Role
Pose sampling, blending and skeleton build for characters, plus the AABB/OBB math the update jobs use for bounds. The animation system is cheap in profiles; ObbToAabb dominates.

## Key data
- `ObbToAabb` 6EA50: 54 non-inlined vec calls (Vec3Dot 6F060 / Vec4Scale / Vec4AddInPlace), 86 % of calls from `BoundsFromPartObbs` 71C60, 38 callers; 6.5-8.1 % of Scheduler0 (bit-exact SSE cave "AABB" built, ~87 % cheaper).
- `MatrixFromScaleQuatTrans` 88780 (238 callers); `AabbToObbMatrix` 6BB80; `AabbTriangleOverlapSat` 78150 (Akenine-Moller SAT).
- `AnimClipSamplePose` 889C0 (time x c+18h in 16.16 fixed, clamped to c+1Ch frames); `PackedCubicVec3Eval` 8A420 (20-bit mantissas + 4-bit shared exponent); `AnimPoseLerpHalfQuat` 21FF9C (half-float quaternion xyz, w rebuilt).
- Blend graph: `AnimBlendNodeEvaluate` 6D360, `AnimBlendAccumulateWeightedInputs` 219E80 (node inputs at dword offsets +0x14, vfunc +8 accumulator).
- Skeleton: `SkelBuildBoneMatrices` CC940 (from ObjUpdatePerObject via 697764), `SkeletonLocalToModelPose` CC490 (48-byte bones, TRS), `PoseCopyTransforms48` 37D7E8.

## Hot functions
| RVA | name | % |
|---|---|---|
| 6EA50 | ObbToAabb | 0.60 |
| 88780 | MatrixFromScaleQuatTrans | 0.25 |
| 889C0 | AnimClipSamplePose | 0.19 |
| CC940 | SkelBuildBoneMatrices | 0.11 |
| 78150 | AabbTriangleOverlapSat | 0.06 |

## What drives the cost
Number of animated characters and parts with changed bounds. Combat-heavy scenes would raise this; no combat capture exists yet. Neither joins the render main thread.

## Deeper notes
notes/effects-animation.md, mainthread-logic.md (AABB patch, 300 k randomized tests), worker-threads.md.
