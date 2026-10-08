# Steady-state streaming during play + animation (v2.02): what runs, what repeats, ranked candidates

RE only (decomp + `decomp_ro`, PC cpuprof captures, Ally SP logs); nothing installed, game not run. Loading screens are
covered separately (notes/loading.md). Builds on meshlet-mainthread.md (P1E7/P4AA/C419/S1E7), work-deciders.md §3,
memory.md / docs/subsystems, effects-animation.md §3. Symbols: `re\symbols\incoming\streaming-anim.tsv` (22 new) and
`streaming-anim-renames.tsv` (9 in-place fixes). Not merged.

## TL;DR
- **Ally: nothing here moves fps.** All streaming bookkeeping and animation is CPU (main thread or workers) and the Ally
  is ~99 % GPU-bound. Streaming bookkeeping on the main thread = **3.2 % of Ally main-thread samples** (v7.1 MAX SP:
  1E7050 .67, 4AA34 .67, 128EF0 .58, 2523FC .57, 1290B8 .29, 419A8 .26, 4DB6C .15) and **5.6-6.9 % of the PC main
  thread** (r9: 2523FC 3.03, 1E7050 1.06, 1290B8 .83, 128EF0 .64; 11-capture mean 6.9 %). It matters on the PC when
  CPU-bound and as insurance; on the Ally only the streaming *quality/latency* findings (R1, R2) are relevant.
- **MODEL_QUAL is a no-op in v2.02** (new; corrects work-deciders.md §1 "consumer found"). Its only game reader 396E18
  feeds ScaleByModelQuality / GetModelQualityLodDistance, called only by MotionLodParamsBuild, whose block fields
  +0x94..+0xAC are never read: CharacterJobsDispatchA/B copy bytes 0..0x1F only (verified in both). Together with
  MOTION_QUALITY (effects-animation §3.1) **neither "motion" setting drives animation LOD**. Keep both at HIGH; free.
- **Skinning is on the GPU** (VS palette `mL2W[384]` + `bSkin[3]` in 81 VS / 20 PS variants, `MatrixPaletteTexture`,
  `TempMeshletMatrixPaletteBufferSize`, `ComputeSkinning` / `ComputeVertexSkinning` / `ModelComputeSkinningContext`
  contexts). The CPU builds bone palettes only (SkelBuildBoneMatrices CC940).
- **Same-result waste found:** (a) `ResourceIsReady` re-walks the full dependency tree on *every* call, also for fully
  ready resources (no cached subtree-ready bit); (b) the page-pool occupancy scan (2523FC, the single largest
  streaming item on PC) exists only to feed one >85 % boolean sampled into a time window; (c) every resident tile x mip
  of every visible texture takes and releases an SRW lock per touch; (d) phase-1 tile requests that hit an in-flight
  page still consume the 16/128 request budget (likely; R1). Off-screen characters are animated at full rate by design
  (distance-based reductions only); that is gameplay-relevant (hitboxes), not a same-look candidate.
- `AnimBlendAccumulateWeightedInputs` 219E80 / `AnimBlendNodeEvaluate` 6D360 are sampled **only on
  [App]EffectUpdateThread** in every PC capture -> an effect node graph, renamed `NodeGraph*` (low confidence).

## Ranked candidates (no patches built)

| rank | id | what | where | Ally impact | PC impact (est.) | same result? | risk |
|---|---|---|---|---|---|---|---|
| 1 | **R1** (measure) | Read the pool stats to see whether in-flight re-requests eat the tile budget; if yes, skip the budget charge for cache hits | 1290B8 phase 1 -> PagePoolRequestTile 491874; stats block pool+8F8h, this frame: +918h hits, +91Ch misses, +90Ch queue full (block copied to +A10h at each scan: +A30h/+A34h/+A24h) | possible faster mip streaming-in on slow storage (less blur after camera cuts); 0 fps | 0 | output differs only in *when* tiles arrive (sooner) | low (probe is read-only); fix medium |
| 2 | **PP** (exists, A/B pending) | Page-pool occupancy scan every 4th frame | PagePoolScanAll 2522C4 / PagePoolCountUsed 2523FC | 0 fps (0.2-0.6 % main) | **2-3 % of main thread** | near: feeds only the >85 % window (§1.2); a 3-frame-stale boolean in a 120-sample time window | low |
| 3 | **PT1** | Take the pool SRW shared lock once per texture in 1290B8 phase 2 instead of per tile x mip touch | 114C38 (from 1290B8 @1297FE) | 0 | small (2 locked RMW per touch; the hot cost is cache misses, see meshlet-mainthread §1.2) | **yes** (shared lock, exclusive taker PagePoolFrameUpdate runs on the same main thread, no other state) | low-medium (cave rewrites a loop) |
| 4 | **RR1** | Frame-scoped memo of "fully ready incl. deps" per resource pointer in ResourceIsReady | 1A6030 entry / 1A6820 | 0 (workers) | ~0 in PC captures (not sampled); Ally kidsts 0.04-0.17 % of samples | yes within a frame; a dep unloaded mid-frame is seen next frame | medium (446 callers; needs a TLS cache + generation check) |
| 5 | **MQ** | Bit-exact SSE `MatrixFromScaleQuatTrans` (scalar, 238 callers; same lane-wise op order, no FMA) like the AABB cave | 88780 | 0 | ~1.7 % of each Scheduler thread (18.6 % summed over 12 captures) | **yes** (per-lane op order kept) | low |
| - | rejected | frame-skipping 4AA34/419A8/4BBEC (meshlet-mainthread §3); skipping tile touches (changes LRU victim order); reducing off-screen character update rate (bone positions drive hitboxes); MODEL_QUAL/MOTION_QUALITY changes (dead) | | | | no | |

Honest bottom line: for the Ally goal none of these is an fps lever; R1 is the only one with a possible visible benefit
(streaming latency) and starts with a read-only measurement. PP + MQ + PT1 are the PC CPU set.

## 1. Streaming during play

### 1.1 Texture tiles: request, keep-alive, budget (main thread, render-frame-begin)
Per visible material texture once per frame (stamp +188h vs frame `[45BA588]+6EB0h`), `TextureStreamRequestTiles` 1290B8:
- **Phase 1 (new requests).** Budget n = min(16, pool free = +7560h - (max(+756C,+7570) + +7590 + +7584 + +757C + +7578),
  per-frame cap +75A8h (128) - +7590h). For each tile whose feedback level (map +120h) is finer than the resident level
  (map +D8h, byte 3) it calls `PagePoolRequestTile` 491874 -> `PageCacheLookupOrQueueLoad` 4919A8: hit -> stamp + ready
  flag (state 2 -> vfunc +20h maps it), miss -> `PageLoadQueuePush` 490B80. **Each call counts against n and +7590h
  whether it hit or missed.** While a tile is in flight the resident map is unchanged, so the same tile is requested
  again every frame and, if many are in flight, can starve new requests (R1; the hit/miss counters above settle it).
  Duplicates are deduped by the page cache, so there is no duplicate I/O.
- **Phase 2 (keep-alive).** Walks the whole tile grid again and, for every tile with feedback or below the min level,
  touches every mip from wanted to max: `PagePoolTouchTile` 114C38 -> `PageCacheTouchRange` 1195E8 -> `PageCacheTouch`
  251528 (stamp = max(stamp, frame + keep)). keep = `TextureStreamKeepFrames` 1277EC = max(+7534,+7538) x
  (1 + 3 x free fraction) + +75AC. Each touch: 2 stats adds, SRW shared acquire/release (+D08h), a modulo hash and a way
  scan. Needed for residency; only the per-touch lock is removable (PT1).
- Side lists: `PagePoolPushTextureUpdate` 828898 / `...B` 8291AC when `TextureStreamGroupIsCurrent` 129B20.
- Existing items stay as in meshlet-mainthread.md: 1E7050 / 128EF0 cache misses (P1E7), S1E7 time slicing (off).

### 1.2 PagePool (device+61A0h): frame update, occupancy, residency
- `PagePoolFrameUpdate` 252510 (from WaitSubmitThread 7D9B6C, main): if enabled (+D20h) take/release the exclusive SRW
  (+D08h, a barrier for shared touchers); if a scan is pending (+D21h): copy the 0x118-byte stats block +8F8h -> +A10h,
  clear it, `PagePoolScanAll` 2522C4, frame++ (+D10h), `TextureBlockFrameUpdate` 127334 on g+74E0h; always emit
  render token 0x95 and count +D18h.
- `PagePoolCountUsed` 2523FC = full scan of every set x way x pool type (1 or 3) counting non-expired ways (stamp >=
  frame) and fully used sets. Its result goes to +B2Ch (previous in +B30h = device+6CD0h). **Only reader found:
  `PagePoolOccupancyCheck` 126C64**: pct = (used - reserved) x 100 / size; pushes (pct > 85) into
  `OccupancyWindowPush` 12722C (120-entry time window); every 0.3 x window it steps a streaming bias t+3C0h by 0.1
  (cap 10 - t+3BCh) while over, and re-arms after a camera move > 100 units or > 30 degrees. So 2-3 % of the PC main
  thread buys one boolean per frame for a slow controller; PP (every 4th frame) is the right-sized fix.
  The direction of the +3C0h bias (finer or coarser mips) is not proven; if it coarsens textures under pressure, a
  full Ally pool would show as blur, which is worth knowing (open).
- Residency/eviction: the cache is set-associative with keep-until stamps (bounded LRU, memory-leaks.md); no
  separate eviction pass runs per frame - a load takes an expired way. The victim choice in the load-complete path
  was not decoded (open).

### 1.3 Resource readiness (workers, effects thread, render nodes)
- `ResourceIsReady` 1A6030: state r+18h (bits 20-27 stage-done, 19 loaded, 29 busy, 28 validate). Runs the type's
  stage step (vfunc +D0h; e.g. `ResObjLoadStageStep` 1A0BFC, `ResSetupStageStep` 1A5820) for the first undone stage,
  recurses to r+30h, and **then always calls `ResourceDependenciesReady` 1A6820, even when the resource is fully
  ready**. 1A6820 recurses into type-E dependency arrays (`ResDepArrayResolve` 3823A0) and the linked chain in the
  handle table (`ResHandleSlotAcquire` 3252100 / `Release` 32525C0, with lock-cmpxchg loads per step). Result:
  every query of a ready asset costs O(dependency tree) locked loads; render nodes (RenderNodeExecutePerView 10F060)
  and effects query per frame.
- Handle lists (`ResHandleListAllReady` 1A19E0, `ResResolveDependencyArray` 1A009C): per handle, `GetResIdFromRes`
  4A7A30 (hash lookup) then the pending/failed bitmaps at ctx+268h; a handle without a resource calls
  `ResLoaderWakeIfPending` 324AE18 (SetEvent only if the loader has work and is asleep, so no per-poll syscall storm);
  failure raises ctx+2D8h via `AtomicFlagsSetMasked` 496F0. Streaming textures (type AD57EBBAh) also need the
  resident bit.
- Cost: not sampled in the PC captures at all; Ally SP: 0.03-0.17 % of kidsts samples (largest in the L1 log, which
  includes area transitions). The perf-coverage "largest Ally block" label is relative to a small game-code share on
  worker threads; it is not on the main thread. RR1 is therefore low value.

### 1.4 Background I/O and decompression
- I/O: `SrwQueueWorkerLoop` 116F80 -> `IoRequestRingDispatch` 117090; `WorkQueueConsumerLoop` 116D30 ->
  `IoRequestRingRetireCompleted` 117330; file threads "KTGLGraph.Cache.FileLoad(.Poll)" (thread entry near 2269478).
- Decompression: `ZlibInflate` B36460 -> `ZlibInflateFast` B36BF0, run as CInflateTask / FileDecompressTask jobs.
  Steady state: 0.00 % of Ally samples; meshlet page loads go through `MeshletChunkIssueLoad` 49D5C. No polling waste
  found here (condition variables, not spins).

## 2. Animation

### 2.1 Pipeline (Scheduler0 + KIDSTS[s3:CharacterUpdate] / [s2:StageUpdate] workers; never [App]Game)
GameModeUpdateMain 39E4E4 -> CharacterUpdateDispatch 3972C4 -> MotionLodParamsBuild 396F54 -> CharacterUpdateBalanceAndRun
399C1C (cost-balanced buckets; jobs A/B, join on Scheduler0) -> per character 407098 -> `AnimLayerSampleClip` 87CF8 ->
`AnimClipSamplePose` 889C0 (+ PackedCubicVec3Eval 8A420); blends `AnimPoseLerpHalfQuat` 21FF9C (StageUpdate threads);
ObjUpdatePerObject 1A7BF0 -> `SkelBuildFromObjUpdate` 697764 (copies 48-B local pose when flag bit 4) ->
`SkelBuildBoneMatrices` CC940 (per bone MatrixFromScaleQuatTrans 88780, half-float decode path) -> GPU palette;
ObjAnimPoseApply 82BF0 -> SkeletonLocalToModelPose CC490 -> AnimPoseDynamicsStep 7FE68 (secondary chains).
IK: string `kids::math::Control2BonesJointMatrix` (two-bone IK) exists; no IK hot spot in any capture.

PC cost, summed self % over 12 captures (per-capture mean in brackets): MatrixFromScaleQuatTrans 18.6 (1.7) Scheduler,
AnimClipSamplePose 14.9 (1.4) Scheduler + 4.5 CharacterUpdate, SkelBuildBoneMatrices 7.3 (0.8), SkeletonLocalToModelPose
6.9 (0.7) CharacterUpdate, AnimPoseLerpHalfQuat 4.6 StageUpdate. Ally v7.1: AnimClipSamplePose 0.19 %, CC940 0.03 % of
kidsTS samples. Captures are idle/shrine; combat is unmeasured.

### 2.2 LOD and update rates
- Settings: MOTION_QUALITY and MODEL_QUAL are both dead (above). MODEL_QUAL global 0x45BAF90 readers: 396E18
  (dead path), menu 60627C, save 1FE56A8; writer 12A2368.
- What does reduce work: distance only - CharacterDistanceLodState 42E90C (18/20 m), CharacterActivationUpdate 42F3AC
  (inactive -> reduced path), CharacterAttachmentUpdate 5BD8E0 (> 18 m), CharacterFarCullFlag 876EB8, draw budget
  2814F0. Secondary dynamics chains have authored rates: each chain accumulates dt at +2D4h and updates only past its
  interval +2D0h (7FE68); dt >= 1/60 is flagged for sub-stepping.
- No camera-visibility gate was found on the skeleton path (697764 call sites only test object flags); a character
  behind the camera within ~18 m is fully animated. That is required for melee hit detection, so not a same-look target.
- Unchanged poses: SkelBuildBoneMatrices has per-bone flag tests (0x10/0x20/1) but no whole-skeleton "pose unchanged"
  early-out was found; idle characters still rebuild. Proving bit-identical skips would need the pose-source
  generation, which was not located (open; low value at these costs).

## 3. Open
- R1 measurement: read device+61A0h + A30h (hits) / A34h (misses) / A24h (queue full), the previous-frame copy, per frame on the Ally (assumes 4919A8 gets the same stats block as 1195E8, pool+8F0h).
- Direction and visual effect of the occupancy bias t+3C0h; Ally pool size (+7560h) vs occupancy.
- Victim selection on page load; TextureBlockFrameUpdate 127334 internals.
- Whether VS skinning repeated per pass (depth, shadow cascades, GBuffer, velocity) is a measurable GPU cost on the
  Ally; a pre-skin compute pass would be a renderer change, not an ASI patch.
