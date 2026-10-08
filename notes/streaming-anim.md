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

## Ranked candidates (MQ + PT1 built and proved, RR1 skipped: see §4)

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

## 4. Addendum 2026-10-08: MQ, PT1, RR1 built and proved offline (nothing applied, game not run)

| id | script | offline result | proof level | expected PC gain | Ally |
|---|---|---|---|---|---|
| **MQ** | `patcher\mq_live.fsx` | ALL PASSED (`re\scripts_cpu\mq_offline.txt`) | **bit-exact, tested**: original bytes (copied from the dump) vs cave, 2,755,130 inputs, 0 bitwise diffs, 0 MXCSR-flag diffs | body 2.86 -> 1.35 ns/call (-53 %, 256-bone native loop) => ~0.8-0.9 % of each KIDSTaskScheduler thread (88780 is ~1.7 %) | 0 fps |
| **PT1** | `patcher\pt1_live.fsx` | ALL PASSED (`re\scripts_cpu\pt1_offline.txt`) | **same state, argued + tested**: refinement proof below, machine-checked control flow, native test on a real SRWLOCK + 2-thread stress | 6.0 -> 3.3 ns per touch with an idle lock, 26.4 -> 6.7 ns with the lock line contended; <= ~0.3 % of the main thread (measure with `status`: lock pairs saved/s x ~20 ns) | 0 fps |
| **RR1** | none | - | **skipped: not provable** (4.3) | ~0 (not sampled on PC) | 0 |

All scripts: `offline` (default) | `dry` | `status` | `apply` | `on`/`off` | `revert`; writes need `FT_ALLOW_WRITE=1` and use
`safeWrite` (all threads frozen, none inside a patched range); memory only, default off (nothing happens unless `apply` runs).

### 4.1 MQ: MatrixFromScaleQuatTrans 88780 (ASI-ready)
- Site: `88780: 4C 8B DC 48 81 EC 88 00 00 00` (mov r11,rsp; sub rsp,88h) -> `jmp cave` + 5 nops. Fallback/off path:
  `mov r11,rsp; sub rsp,88h; jmp 8878A`. .pdata 88780..88938, no branch into 88781..88789, 299 direct call/jmp sites,
  the body has no branches/calls/FMA; its only rip-relative operand is 3BD41E0 = 1.0f.
- Signature: `float* f(float out[16] rcx, const float s[3] rdx, const float q[4] r8, const float t[4] r9)`; returns out.
- Exactness rules: every stock scalar op is done by the same packed op with the **same dest/src order** (x86 keeps the
  first operand's NaN payload when both are NaN; the negative control - one addps with swapped operands - gives 2,075
  diffs per 100k cases, so the harness catches it); lane 3 always repeats lane 0's operands, so no extra MXCSR flags;
  the scale is read as exactly 12 bytes (movq + insertps); all inputs are read before the first store; out overlapping
  s -> stock code (it re-reads s between stores). Tested in 5 MXCSR modes (RN, RN+FTZ+DAZ, RD, RU, RZ) with random bit
  patterns, NaN payloads/sNaN, +-inf, denormals, -0, overflow-range values and unit quaternions; 3,001 aliasing cases.
- Cave code (SSE4.1, volatile xmm0-5 only, leaf, no stack use):
```
test byte [en],1 ; jz fallback ; (inc counter if en&2)
lea rax,[rcx+40h] ; cmp rdx,rax ; jae ok ; lea rax,[rdx+0Ch] ; cmp rcx,rax ; jb fallback      ; out overlaps s?
ok: movups xmm0,[r8] ; movaps xmm5,xmm0 ; shufps xmm5,xmm5,FFh       ; wb = (w,w,w,w)
shufps xmm0,xmm0,24h                                                 ; q4 = (x,y,z,x)
movaps xmm1,xmm0 ; addps xmm1,xmm0                                   ; q2 = (x2,y2,z2,x2)
movaps xmm2,xmm1 ; mulps xmm2,xmm0                                   ; D = (x2x,y2y,z2z,.)
movaps xmm3,xmm0 ; shufps xmm3,xmm3,49h ; movaps xmm4,xmm1 ; mulps xmm4,xmm3   ; A = (x2y,y2z,z2x,.)
mulps xmm1,xmm5 ; shufps xmm1,xmm1,92h                               ; Ws = (z2w,x2w,y2w,.)
movaps xmm5,xmm4 ; addps xmm5,xmm1 ; subps xmm4,xmm1                 ; P = (m1,m6,m8), M = (m4,m9,m2) unscaled
movaps xmm0,[ones] ; movaps xmm3,xmm2 ; shufps xmm3,xmm3,49h ; subps xmm0,xmm3
shufps xmm2,xmm2,92h ; subps xmm0,xmm2                               ; G = (m0,m5,m10) unscaled
movq xmm1,[rdx] ; insertps xmm1,[rdx+8],20h ; shufps xmm1,xmm1,24h   ; S = (s0,s1,s2,s0)
mulps xmm0,xmm1 ; mulps xmm5,xmm1 ; shufps xmm1,xmm1,49h ; mulps xmm4,xmm1
movaps xmm2,xmm0 ; insertps xmm2,xmm5,18h ; insertps xmm2,xmm4,A8h   ; row0 = m0 m1 m2 0
movaps xmm3,xmm4 ; insertps xmm3,xmm0,58h ; insertps xmm3,xmm5,68h   ; row1 = m4 m5 m6 0
insertps xmm0,xmm5,88h ; insertps xmm0,xmm4,58h                      ; row2 = m8 m9 m10 0
movups xmm1,[r9] ; insertps xmm1,[ones],30h                          ; row3 = t.x t.y t.z 1
movups [rcx],xmm2 ; movups [rcx+10h],xmm3 ; movups [rcx+20h],xmm0 ; movups [rcx+30h],xmm1 ; mov rax,rcx ; ret
```
- ASI note: entry hook with a stock fallback; the cave must be within rel32 reach (or use a 14-byte absolute jmp and
  move the fallback accordingly). Keep the overlap test.

### 4.2 PT1: one pool-lock hold per texture in TextureStreamRequestTiles phase 2 (ASI-ready)
Sites (bytes checked offline; all five written at once with every thread frozen):

| site | stock | patched |
|---|---|---|
| S_ACQ 114CA8 | `FF 15 7A D9 7C 03` call [AcquireSRWLockShared] | `call capAcq` + nop |
| S_REL 114CEE | `FF 15 3C D9 7C 03` call [ReleaseSRWLockShared] | `call capRel` + nop |
| S_B 129651 | `45 01 96 90 75 00 00` add [r14+7590h],r10d (phase 2 begins) | jmp: if switch on, `lock cmpxchg owner, 0 -> tid`; displaced; jmp 129658 |
| S_C 12982E | `48 8B 06 4C 8D 45 60` (touch failed, before call [rax+28h]) | jmp: if owner == tid and held: held = 0, ReleaseShared; displaced; jmp 129835 |
| S_D 1298B3 | `80 7D 0C 00 0F 84 86 00 00 00` (phase 2 ends, single join) | jmp: save volatile GPRs; if owner == tid: release held, held = 0, owner = 0; displaced cmp; je 129943 / jmp 1298BD |

capAcq(rcx = lock): gate = `owner == gs:[48h] && [rsp+70h] == B+1297FE` (114C38 called from phase 2; 114C38's frame
depth at both IAT calls is 68h, checked offline). Gate open: held == lock ? skip : (held = lock; AcquireShared).
Gate closed: AcquireShared. capRel: gate open and held == lock ? skip : ReleaseShared. The return-address gate keeps the
second 114C38 caller (7C4BCC in 7C463C) and every other thread stock. Function pointers are copied from the live IAT
at apply time. Full asm: `buildCave` in `patcher\pt1_live.fsx` (347 code bytes).

Proof that no other thread can observe a different state:
1. **Closure (machine-checked offline).** Phase 2 = [129651, 1298B3) is entered only at 129651, every branch stays
   inside, and it exits only through 1298B3; its only calls are `call 114C38` (1297F9) and `call [rax+28h]` (129841,
   right after S_C). So while the lock is held across touches this thread runs only the loop bookkeeping (reads of the
   texture's maps and its own frame-local dedup bitmap) and 114C38's pre-lock part: two non-atomic stats adds
   (+8F8h/+900h; the same instructions as stock), a read of +D10h, and 117B44 = `ID3D12Resource::GetDesc` (vtable +50h
   of the texture's [rsi+10h]; it returns 1-4 = D3D12_RESOURCE_DIMENSION and tests Flags & 3 = RT|DS).
2. **Refinement.** The lock is the only synchronisation between this thread and the other users of pool+D08h. Census
   (offline): shared in 49D5C/49F4C/4A134 (TS_MESHLET_REQ workers), 116890, 128844, 128B0C, 129B70, 48FF40, 4903E0,
   490874, 491874, 6BDF78, 75821C, 7A56B4, 7F49B4, 7F6CC4, 83176C, 1896C34..189705C, 2EEF7F4; exclusive in 250BF8,
   252510 (PagePoolFrameUpdate), 4906E8, 492310, 2EEF9A8, 2EEFAC8, 2EEFC30. Holding a *shared* lock longer only removes
   interleavings: every PT1 execution is a stock execution in which each exclusive acquirer arrived after the texture's
   last touch (shared holders never exclude each other, so their interleavings are unchanged). No new state can appear;
   exclusive takers may wait up to one texture's phase 2 (a bounded loop without waits, microseconds).
3. **No deadlock.** No recursive acquisition: the paths that can take the same lock again (call [rax+28h] can reach
   491874; 75821C runs after S_D) are preceded by a release (S_C, S_D); an inner 1290B8 on the same thread neither steals
   nor double-holds (tested). The only wait added while holding is GetDesc, a lock-free accessor in d3d12 /
   vkd3d-proton - the one assumption outside the game's code. The thread never takes the lock exclusive inside the
   window (that would deadlock stock as well).
4. **Tested natively** on a real SRWLOCK: 19 functional checks (claim, skip, release paths, other caller, other owner,
   switch off, recursion, displaced instructions, both je/jne exits) + 200,000 textures x 1-40 touches against an
   exclusive-taker thread and a foreign shared thread: no deadlock, no exclusive section overlapped a held window, the
   lock was free at the end.
- `revert` writes switch = 0, waits until owner == 0 (no texture mid-loop), then restores the 5 sites. Never restore the
  bytes by hand while a texture holds the lock (it would leak the shared hold and block every exclusive taker).

### 4.3 RR1: skipped (cannot be proven same-result)
- `ResourceIsReady` 1A6030 is not a pure query: for a not-fully-ready resource it runs the type's stage step (vfunc
  +D0h) and lock-cmpxchg's the stage/busy bits of r+18h; through `ResourceDependenciesReady` 1A6820 -> 3823A0 it recurses
  into every type-E dependency (driving *their* loading) and walks the r+8 chain through the handle table at ctx+158h
  (`ResHandleSlotAcquire` 3252100 / `Release` 32525C0, slot reference counts).
- A per-resource "fully ready" memo would have to be invalidated on every change of r+18h of the resource and of every
  dependency, of the r+40h dependency arrays, of the r+8 chain and of the handle slots. These are written by loader
  code on loader threads with plain lock-cmpxchg loops, not through one choke point, so no generation counter can be
  shown complete without hooking every writer. A dependency unloaded (or its memory reused) between two calls in the
  same frame would make the memo answer "ready" where the stock code answers "not ready" or re-drives the load: a
  different result and a possible use-after-free. The frame-scoped variant in the table above ("a dep unloaded
  mid-frame is seen next frame") is therefore *not* same-result.
- Value: not sampled in any PC capture; 0.03-0.17 % of Ally kidsts samples (worker threads). Skipped.
