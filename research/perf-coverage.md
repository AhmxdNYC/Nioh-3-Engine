# Perf coverage: naming every profiled Nioh3.exe function

Goal: the symbol DB (symbols.tsv) should name every function that shows up in a profile.
Tool (private dev repo): `perf_coverage.py [--before] [--self] [--list N] [--incl N] [--gpu] [--tsv out]`.
Pass 1 added 189 rows (34 high, 53 medium, 102 low) and 37 in-place renames of placeholder names. All passes below
(1-4 and the audio / ObjUpdate addendum) are merged into symbols.tsv, renames included.

## Method
- **Sources.** The script reads:
  - the Ally ASI sampler: every `SP` block in 5 Ally logs;
  - the PC sampler `cpuprof`: 12 captures. It uses the `attr` and `self` views for the coverage figure and the `incl` view for callers. An older-format test capture is skipped;
  - every function RVA cited in the `gpu-*` notes, `d3d12-submission.md` and `invisible-gpu.md`.
- **Weighting.** Each log or capture is normalised to its sample total, then averaged. "Combined" is the mean of the Ally and PC shares.
- **Folding to function starts.** Each RVA goes to its function start through chained `.pdata` in the dumped exe.
  - Leaf functions have no `.pdata`. For them the script uses the Ghidra function table.
  - cpuprof rows tagged `(leaf?)` carry a guessed start. The script takes their hottest RIP instead.
  - Example: `121E70(leaf?)` resolves to 121E90 `ParamTableGetU32_D15EC139`. `D8C40(leaf?)` resolves to `CollisionMapFind` D8D90.
- **Two coverage views:**
  - `--self` counts only samples whose RIP is in game code, which is real CPU work.
  - The default view is wall time: system and wait time are attributed to the first game frame, so blocked threads count too.
- **What counts as "named".** A `function` row whose name is not a Ghidra default and not a placeholder.
  - The placeholders are `Sched0Hot_`, `MainHot_`, `RenderSubmitHot_` and `PauseSpin_` (the "hotspot, role not identified" seeds).
  - `Unknown_<area>_<rva>` rows are counted separately. Their role is unclear, but each has a description backed by evidence.

## Coverage before / after
| view | before (pre-merge DB) | after: named | after: + Unknown_ |
|---|---|---|---|
| **self (CPU work), combined** | 56.5 % | 82.0 % | **96.6 %** |
| self, Ally | 64.5 % | 92.6 % | 97.5 % |
| self, PC | 53.7 % | 78.6 % | 96.3 % |
| wall time, combined | 91.1 % | 96.5 % | 99.3 % |
| PC inclusive >= 0.3 % (99 fns) | 49 named | 94 named | |
| fns cited in GPU notes (63) | 44 named | 55 named | |

The "before" figures treat placeholder names as unnamed. If placeholders count as names, the self coverage before was 75.6 %.
Wall time was already high because Ally/PC wall time is dominated by wait loops that were named earlier, such as the frame-sync and FenceWait waits.

## Pass 1 placeholder renames (applied in place)
These 37 rows replaced placeholder names at the same RVA (names below are the current symbols.tsv names):
- `MainHot_*`:
  - 418F40 -> WaitDispatcherDrainSpin
  - 5DF70 -> MeshletPageCountdownTick
  - 1D84A0 -> StageSlotTableUpdate2568
  - 1D8720 -> StageSlotTableUpdate2578
  - 1D8220 -> StageSlotTableUpdate6780
  - 1D7EB0 -> StageSlotTableUpdate5F20
  - 1DAF6C -> DrawStateCommit
  - 1E0B64 -> DrawStateFlushDirty
- `Sched0Hot_*`:
  - Object update: 1A7BF0 ObjUpdatePerObject, 121FC0 ObjUpdateWorkerBody, 16CFF0 ObjUpdateJobDispatch, 425B0 ObjCountValidSlots
  - Bounds: 71C60 BoundsFromPartObbs, 6EA50 ObbToAabb
  - Vector math: 6EFD0 Vec4AddInPlace, 6F010 Vec4Scale, 6F060 Vec3Dot, 174610 Vec4CopyW0, 54360 SinCosApprox
  - Other: 12B440 RollingAvgListUpdate, 66D00 UiNodeUpdateWorldMatrix, 889C0 AnimClipSamplePose, E32D90 DequeLowerBoundU64,
    F33A0 PauseSourcesAnyActive, 16BA70 KdTreeCollectNearestInFrustum, 12FBDD0 ObjPoolReuseIdleForTypeIds,
    284ED4 GameContentIdIsAvailable, 13484C SlotAttachmentListRebuild, F450C ItemBoxScanFlaggedEntries
    (several of these first got `Unknown_*` names and real names in pass 2)
- `RenderSubmitHot_*`:
  - 119820 PsoCacheBTreeFind
  - 1192B0 ShaderVisibleDescRingAlloc
  - 115B9C SynBindTextureSlot
  - 118E28 SynUploadCbvTable
  - 114A20 SynTokSetIndexBuffer
  - 116260 SynTokSetVertexBufferTracked
  - 58C144 PsoComputeCacheBTreeFind
  - 1164C4 SynTokSetPipelineLayout

## What the newly named code is (by subsystem)
- **Object update (Scheduler0 + workers, the largest unnamed block).** The chain is:
  - `ObjUpdateJobRoot` 182000 -> `JobParallelForSplit` 204584 -> `ObjUpdateJobFanout` 408FE0 -> `ObjUpdateJobEntry` 16EF8C -> `ObjUpdateJobDispatch` 16CFF0 -> `ObjUpdatePerObject` 1A7BF0.
  - From `ObjUpdatePerObject`: `BoundsFromPartObbs` / `ObbToAabb` / vector helpers, and `ObjUpdateWorkerCall` 1202B0 -> `ObjUpdateWorkerBody` 121FC0.
  - Inside the worker body: param-table getters `ParamTableFindHashType` 121F80 (994 callers) and `ParamTableGetU32_BD8BEF28`.
  - Part-slot locking: `ObjPartAcquireShared` 484C0 and `ObjMarkDirtyRecursive`.
- **StageUpdate (KIDSTS s2).**
  - `StageUpdateJobEntry` FADEA0 -> `StageVisibilityUpdate` 174C9C.
  - `StageObjLodUpdateRange` 469C0, `PlaceableCollisionUpdate` D8E00, `PlaceableDistanceTest`, `SortedEntries28Find`, `LowerBoundU32Stride28`.
- **Streaming / meshlets.**
  - Page lookup: `SetAssocCacheLookup` 2515B0, `PageCacheFindOwned` 251554, `StreamPageLookupOrInsert` 116A24.
  - Request path: `MeshletChunkIssueLoad` 49D5C, `MeshletRequestSwapBuffers`, `MeshletRequestJobEntry`, `MeshletResGetCount`.
- **Main-thread state commit.** `DrawStateCommit` 1DAF6C and the four `StageSlotTableUpdate*` functions. The 1E0B64 / 1E55D4 / 1E5854 / 1E8058 helpers are named in pass 2 (DrawStateFlushDirty, DrawStateBindRecords, DrawStateBindResourceSlot, TexViewIsCurrent).
- **Dispatcher (D3D12 submission).** `WaitDispatcherDrainSpin`, `PsoCacheBTreeFind`, `ShaderVisibleDescRingAlloc`, `SynUploadCbvTable`, `SynBindTextureSlot`, `SynTokSetIndexBuffer`, `RingWaitIdleSpin` 252730 (a spin without PAUSE on GfxSubmitFrameCmdList paths).
- **MissionThread.**
  - Call chain: `MissionThreadEntry` E5E060 -> `MissionThreadRunFrame` E63140 -> `MissionThreadUpdate` E64700 -> `MissionThreadStep` 20B900.
  - Task helpers got role names in pass 2 (e.g. MissionTaskStateUpdate 2866F0).
- **Threads / CRT.** `CrtThreadStart` BDB740, `AppThreadTrampoline` 5B6670, `_Cnd_wait`, `HandleWaitMs` 8629AC (4 % of PC wall time is blocked here), `WinMain`, `__scrt_common_main_seh`.
- **GPU-setting paths from the notes.**
  - GI = LIGHTEST skip sites: `GiLightestParamsApply`, `GiLightestFlagApply`, `GiLightestSkipTest`.
  - Shadow-tier setters: `ParamTableSetU32_*`.
  - Landscape: `LandscapeLodScaleApply`, `LandscapeLightestFlagsApply`.
  - Other: `RenderFrameSettingsApply` 39B570, `DebugRenderScaleInfoPrint`.
  - The remaining "unnamed" GPU-note hits are not functions: they are hashes or offsets that happen to land in `.pdata`, or D3D12Core offsets.

## Gaps after pass 1 (closed by the passes below)
- **`Unknown_` share of self time after pass 1, by area (combined):**
  - ObjUpdate 3.7 %
  - Thread5892 1.5 %
  - Mission 0.8 %
  - StateCommit 0.8 %
  - Transform 0.5 %
  - SceneDraw 0.4 %
  - Geom, ObjState, ObjParts, SynTok, Anim, BTreeFind, Container about 0.3 % each
- **Unnamed tail after pass 1.** 3.4 % of self time was spread over about 1,060 functions, the largest at 0.05 % (3D8770, 1FDC1C, 4FC330, 1989F0, 42154 ...; all named in later passes).
- **Thread 5892** (12.6 % CPU in cpuprof-r9, entry AudioMixThreadProc 1A786A8): it sets MXCSR FTZ, runs float block DSP (E5840) and a
  decoder (18AB80, negative error codes), and shares `HandleWaitMs` with AudioThread. The addendum below identifies it as the audio mix thread.
- **Not identified.** The object-model "Obj" layout:
  - known fields: flags +4Ch, part slots +240h (count +120h), component +158h, attachments +400h;
  - unknown: the engine class name. Many `Unknown_ObjUpdate_*` rows would become real names once one vtable/RTTI anchor is found.
- **Caveat.** Both samplers measure wall time, so self time on PC includes spin loops. Ally `[App] other` rows are partly stale-stack attributions (see `cpu-hotspots-ally.md` §1). Examples are 37F550 and 4A09F0.

## Addendum 2026-10-07 (audio thread + ObjUpdate naming)
8 new rows and 18 placeholder renames (merged). Combined named coverage 96.5 % -> 97.1 % (PC 96.0 -> 97.2 %), `Unknown_` 2.8 -> 2.2 %; Ally unchanged (its Unknown_ rows are other functions).

### Thread 5892 (AudioMixThreadProc 1A786A8) = the audio mix thread
- Evidence: PC profile shows XAudio2_9.dll frames (0.7 %, incl. X3DAudioCalculate); the loop drives sink objects (vfunc +60h state 7 = ready, +88h get buffer) into a ring of 0x18-byte slots; the exe carries `kids::audio::BeginUpdate/EndUpdate`, `Mixer`/`Mastering`/`DuckingComp` strings and libVorbis 1.3.7. No CRI/ADX2/Wwise/FMOD strings anywhere: it is the in-house KIDS mixer on XAudio2. The thread is unnamed (nobody calls SetThreadDescription on it); it is entered through `ThreadFuncInvoke` 9FC2F4 -> 1A786A8 (no direct xref: a virtual Run).
- **No spin, no polling.** 87.2 % of its samples are `ZwWaitForSingleObject` under `HandleWaitMs` (the 100 ms-timeout wait on its event, re-looped only on timeout). "12.6 % CPU" is the 12.8 % that is real work, i.e. ~0.126 core.
- Where that work goes (share of the thread's samples, inclusive): effect chain 36044C -> E2050 7.6 % (reverb E3FF0 5.4 % -> `ConvReverbProcess` E5394 5.1 % -> `ConvReverbPartitionMac` E5840 5.0 %, FFT E7840/E7D00 ~0.3 %); voice decode `AudioVoiceListDecode` 18AA7C 4.4 % (AudioVoiceDecodeBlock 18AB80 3.1 % self + 69220 1.2 %); XAudio2 0.7 %.
- Decode is demand-driven (voice ring refilled only when empty; paused voices skipped). Decoding is therefore not wasted beyond audible-or-not voices; there is no volume/audibility test (`AudioVoiceListDecode` skips only paused voices), so a voice at zero gain still decodes. Whether that happens often is unmeasured.
- Mix rate / block size: not derivable statically (block frames at mixer +12Ch, channels +11Ch, ring slots +130h); needs the probe below. No evidence of needlessly high-rate mixing.

### Same-sound fix candidate (not applied; needs one in-game probe)
**Silent-input convolution skip** in `ConvReverbProcess` (E5394). The reverb is a uniform partitioned FFT convolution (N partitions = mixer `+38h`, block = `*(u16)(ir+4)`); its cost is independent of the input level. `E5394` already skips only when wet gain (+40h and target +18h) < 0.0001; it never checks for silent input. Design:
1. Keep a per-state counter `quiet` (new side array keyed by the `param_5` state pointer, or an ASI-owned map). At the `iVar7==1` branch (before the per-channel `ConvReverbPartitionMac` loop), test the just-queued input block (`lVar16 + uVar20*uVar18*4 + ch*uVar15*4`, `uVar18` floats per channel) for all-zero bits (AVX `vptest`, or abs<1e-30 if FTZ inputs are denormal-free); increment `quiet` if all channels are zero, else reset it.
2. When `quiet > partitions + 1` the frequency-domain delay line and the overlap tail are all exactly zero, so every `ConvReverbPartitionMac` result is +0.0. Skip the call, write zeros to the output slot (`memset(pfVar1,0,...)` then the same gain ramp and copy-out as today), and still advance `param_5+0x34` / `+0x40` / `+0x28` identically so state stays in lockstep. Output is bit-identical except possibly the sign of zero.
3. Safety: only count a block as quiet when the *input* is exactly zero, never when it is merely small, so a decaying tail is never cut.
- Upper bound: ~5 % of one core (0.05 core, well under 1 W), and only the share of time the reverb input is silent. Gain is real only in quiet scenes/menus; in combat the SFX bus is rarely zero. Not worth a code cave before measuring.
- **Probe first** (ASI, read-only, 30 s): count per second (a) wakeups of 1A786A8's `HandleWaitMs`, (b) E5394 calls and how many have an all-zero input block, (c) voices decoded by 18AB80 and how many have gain 0. If (b) < 30 % silent and (c) ~0, drop the idea. The Ally is GPU-bound at 99 %, so this is power-only; at 25 W shared the audio thread is ~0.1 core = low single-digit % of CPU power.
- Not recommended: lowering mix rate / block count / reverb partitions (changes sound), skipping paused decode (already done).

### ObjUpdate naming: why RTTI did not help
- The exe has 859 RTTI type descriptors but only 204 vtable-linked COLs, all `ktul::`/`ktgl::CResource`/`std::` utility classes. **No game object class has RTTI**, so no RTTI anchor exists for `Obj`. A scan of "mov eax,imm32; ret" type-hash getters (6564 functions, 3125 distinct hashes) vs crc32/fnv/djb of the exe's strings found no name match; the hash scheme is not a plain one. Hash D40B3C8Fh (component type tested by 204128 / 203840 / 61FEA8's family) therefore stays unnamed.
- What we do have: the virtual ObjUpdate functions sit in 165-slot vtables of one family: 38F16B8 / 38F1BE0 / **38F2108** (main, ~250 constructor lea-sites) / 38F25B8 / 38F2A98 / 38F2F78 / 38F3438 / 38F3908 / 38F3DD8. Slots: 205840 at 38F16E8; 204128 at 38F1B10 + 38F2038; 61FEA8 at 38F1B78 + 38F20A0 (13 slots later); 5795B8 at 38F5568 + 38F5A38; 408648 at 404FED0 + 40DADF0. The next step to learn the class name is the constructor of vtable 38F2108 (lea sites 4D2C04, 4D56F6, 614AA6 ...): a string passed to its registration call would name the class. That would rename the `Obj*` prefix in one go.
- The 18 renames are role names from the decompiled behaviour (confidence low; medium only for audio). `Obj` stays the placeholder class name.

## Pass 2 (2026-10-07): self-time coverage toward 99 %
202 new function rows and 78 in-place renames of existing `Unknown_*` / `Sched0Hot_` / `MainHot_` / `RenderSubmitHot_` /
`PauseSpin_` rows (merged).

Coverage, `--self` (CPU work only):
| view | before | after |
|---|---|---|
| **combined, named** | 87.5 % | **99.5 %** |
| Ally, named | 93.0 % | 99.5 % |
| PC, named | 85.7 % | 99.5 % |
| combined, `Unknown_` | 9.2 % | 0.0 % |

Order of work: Ally share first (the target device), then combined share. Names are role names from the decompile (most `low` confidence, data-structure / math / CRT helpers `medium`-`high`); each row's description carries the evidence (size, fields, constants, callers).

### What the newly named code is (by subsystem)
- **Resource readiness (largest Ally block, ~2.6 % of Ally game self time).** `ResourceIsReady` 1A6030 recurses through `ResourceDependenciesReady` 1A6820 (dependency list r+40h, type-E entries expand to child arrays, linked chains via the handle table). Handle lists are checked by `ResHandleListAllReady` 1A19E0 (301 callers) and its array twin `ResResolveDependencyArray` 1A009C: per handle `GetResIdFromRes` -> ready / failed bitmaps at ctx+268h; a missing handle kicks a load (324AE18 on ctx+210h); `StreamingTexContext` (type hash AD57EBBAh) handles must also have their resident bit. Staged loaders `ResObjLoadStageStep` 1A0BFC and `ResSetupStageStep` 1A5820 poll these every frame until their stage counter advances; `ResEntryListAllReady` 904B58 and `ResourceIsReadyTmp` 1A5EC4 (399 callers) are the per-entry wrappers. Hot loop: the per-frame re-poll of not-yet-ready objects (each poll re-walks the whole dependency tree with lock-prefixed loads).
- **Draw-state commit (main thread).** `DrawStateFlushDirty` 1E0B64 commits by dirty bits ctx+12E8h: 40h re-resolves the shader binding set, 10h recomputes the permutation index from a 6-dimension table (shader+8Ch..A0h). `DrawStateBindRecords` 1E55D4 walks the 12-byte binding records (ctx+1100h) and `DrawStateBindResourceSlot` 1E5854 (32 callers) finds the reflection record for slot < 16 and binds the view; `TexViewIsCurrent` 1E8058 is the "already bound" test. `SynTokReadStateBlock128` 189C0C4 is the dispatcher-side decoder of the 128-byte state block.
- **UI.** `UiNodeUpdateWorldMatrix` 66D00 (2.4 % of Scheduler0) and `UiNodeDrawIfVisible` 6642C live in the UI widget vtable block (38E7188.., next to UiWidget / UiIcon slots); `UiTextLayoutGlyphs` 62408 lays text out through a per-font 64K code-point map + 12-byte glyph records. UI nodes rebuild their local matrix only when flag +104h bit 4 is clear, but always recompute world = parent * local when bit 8 is clear.
- **Animation / skinning.** `AnimClipSamplePose` 889C0 (16.16 fixed-point frame time, per-track bone index), `AnimPoseLerpHalfQuat` 21FF9C (half-float quaternion pose blend, job-wrapped), `SkelBuildBoneMatrices` CC940 (from ObjUpdatePerObject: parent-chain TRS composition, inverse bind, palette write), `QuatMul`, `Vec4MulMat44`, `Mat34InverseAffine`.
- **Audio mixer (thread 1A786A8).** `AudioBiquadProcess` E1740, channel conversion / mix kernels `AudioChannelConvert` E34A0, `AudioChannelConvertMix` E33AC, `AudioChannelMixGain` E2610, `AudioChannelMixGainMatrix` E1EF8, reverb bus I/O `AudioReverbMixInputs` E3610 / `AudioReverbMixOutputs` E49B0 around `AudioReverbEffectProcess`.
- **Render graph.** `RenderNodeExecutePerView` 10F060: per-view render-node execute (vtable 4109F88, beside the ReflectionProbePass / PrepareThinSurfaceBsdf node vtables); gates every input on `ResourceIsReady`, bit 2Ah routes to the FSR3 SaveOpaqueColor path.
- **Mission / game logic.** `MissionTaskStateUpdate` 2866F0 (state 0 wait / 1 active / 2 finish, plus a player-distance test).
- **Containers / CRT / math helpers** (many callers each): `ObjHandleRelease` 47DF8 (1150 callers, packed strong/weak counts), `DequeLowerBoundU64`, `GlobalEntityFindById` 1363D4 (220 callers), RB-tree insert / iterate, introsorts, `CrtPow`, `CrtFExp`, `CrtFloatTrig`, `_malloc_base`, `Vec3Cross`, `Vec4Dot`, `Vec4ArrayCopyStrided`.
- **Physics / cloth / soft body.** `ClothSimSolveParticles` 169500, `ClothSimTransformCollidersAndSolve` C94A4, `SoftBodySolveDistanceConstraints` 71130 / `SoftBodySolveTriangleBendConstraints` 6FA30, `CapsuleColliderPushOutPoint`, SAT overlap tests (`AabbTriangleOverlapSat` 78150, `ObbAabbOverlapSat` 178494), `PhysicsWorldStepTimed` 2D4EB4.
- **Secondary animation.** `ObjAnimPoseApply` 82BF0 -> `ObjAnimPosePostUpdate` 82F1C -> `AnimPoseDynamicsStep` 7FE68 (dt sub-stepped against 1/60 s, per-chain LOD gate), `SkeletonLocalToModelPose` CC490, `AnimTrackSetApplyFiltered` 65214.
- **Spatial queries / culling.** `OctreeQueryVolumeCollect` 165088, `OctreeParallelQuery` 202940, `KdTreeCollectNearestInFrustum` 16BA70, `SceneViewsTestBoundsVisible` 16654C, `PlaceableCullSmallByFrustumDistance` DD7AC, `StageObjDistanceFadeUpdate` 174908, `StageGridCellsVisibleInRadius` 3695D4.
- **Work-deciding / budgets.** `PendingRequestsDispatchBudgeted` E26890, `VsmClipmapUpdateLevels` 77AB0 (virtual shadow-map clipmap level update), `ObjPoolReuseIdleForTypeIds` 12FBDD0, `FrameStepSpawnJobsAndSync` 123BD50.
- **Jobs.** `JobNodeExecute` 111184 is the generic Scheduler0 job runner (everything it runs is under it: 3.3 % PC inclusive) and `JobInvokeTimed` 1113B4 its optional per-job QPC statistics (10-sample ring, min/max); `ObjTimedUpdate` 204DB0 / `ObjTimedUpdateJob` 7E35E4 / `ObjRegUpdateJob` 111EA4 ("Reg(%s)" profiling labels); `ResUpdateJobDispatch` 855E48 picks single vs split batches.
- **Script VM.** `ScriptVmRunOpcodes` 13A830, `ScriptVmResolveVarRef`, `ScriptVmCallNative`, `ScriptVmOpCallScriptFunc`, `ScriptCtxIsAtEnd`.
- **Particles / effects.** `ParticleEmitterUpdateModules` CE7E0 (+Alt C8958), `EffectEmitterUpdateParticleLife` C7F4C, `ObjUpdateAttachedEffects` 1A7408 (the "CE1ResourceEffect" path), `ObjLateUpdateAttachments` 211194.
- **Streaming I/O.** `IoRequestRingDispatch` 117090 / `IoRequestRingRetireCompleted` 117330, `ZlibInflateFast` B36BF0, `MeshletMarkRequestedAddCost` 49950.
- **Perf smell found.** `SceneMapObjPartsUpdateSlicedJob` 1196B70 iterates the std::map at g_pSceneDrawCfg+558h by index and re-walks it from begin() for every index (O(n^2) node walk; each job handles a 1/15 slice). Cheap to fix in principle (keep the iterator), but it is 0.03 % of PC self time, so it is not worth a patch.

### Remaining gaps
- **Self time:** 0.5 % of game self time remains unnamed, spread over ~850 functions of <= 0.01 % each (largest D0F48, 27E788, 371784, 55378, 866978).
- **Wall-time view:** Ally still shows ~2.3 % in callers of blocked waits (12F5E0 SubObjGatedVirtualCall, 2665C4 VirtualTestBranchDispatch, 31DA38 ObjWaitListWalk, 7E71B0 GameMainGuardedCall, 37F550 ObjFloatRangeUpdate, 9AFED0 RenderFrameRecordRebuild, 394BC8 OnceInitStateCall, 5AD560 ChildListVirtualUpdate; low confidence): these are not CPU work.
- **Confidence:** about half of the new names are `low` (role from behaviour and callers, no string or RTTI anchor). Game classes still have no RTTI, so class names (e.g. the `Obj` family) remain inferred.
- **Duplicates by design:** where two naming passes named the same RVA, only one name was kept.

### GPU-pass CPU paths (pass 3: 92 rows incl. 9 node vtables, 5 in-place renames; merged)
Ordered by Ally GP2 cost (the L2 top-20 table); FSR3 is covered by `fsr3-audit.md`. Found by following each pass/type name in the exe strings table from the type registry builder 26F5D84 (it holds the `RegisterAssetHandler` call at 26F5E69).



**Common pattern.** Each pass name in 0x26F5D84 is registered by a small `<Type>RegisterType` function. That function stores {handler table, name} in a 0x4Bxxxxx global and tail-calls RegisterAssetHandler 3246E9C(reg, typeHash, &global). The handler tables come in three families:
- **Render node:** handler slot 7 = 3254D00, slot 22 = factory.
- **Data object:** slot 7 = 3240A98.
- **Shader object:** slot 7 = 25ECxxx, slot 14 = A5F500. Its factory loads the program and never creates a node.

The node factory (slot 22) resolves the node's resource/shader hashes and calls the ctor, which sets a **7-slot node vtable**:

| Slot | Role |
|---|---|
| +0 | scalar dtor |
| +8 | **Execute**(node, RenderCtx r, ?, Scene sc) |
| +10 | ResourcesReady |
| +18 / +20 / +28 | defaults AA0140 / AA0130 / AA0120 |
| +30 | ReleaseResources |

Every Execute uses the same view gate: view = (node byte & 7F) + [[r+F8h]+974h]; it must be < 0x20 and sc+180h+8*view must be non-null. `[r+F8h]` is the per-frame render-settings block (RS) that `ApplyGraphicsSettings` 39A39C / RenderParamsAssign 247950 feed.

**UltraMarine vtables.** 0x410A460 and 0x410A498 are the UltraMarineEffectGBufferEdit and UltraMarine node vtables (symbols.tsv:
UltraMarineNode_vftable for 410A498; 410A460 still carries the older name ReflectionProbeCmdA_vftable):
- ctors 368ADB0 / 368ADE8;
- factories 25B6FBC / 25B7064;
- types 0x874A47C3 / 0x1872E683.

Evidence: ctor 368ADE8 is called only by UltraMarineFactory 25B7064 = slot 22 of the UltraMarineRegisterType table 3B38CA0. The
slot functions are UltraMarineEffectGBufferEditNode_ScalarDtor 368AE9C, UltraMarineNode_ScalarDtor 368AEDC and
UltraMarineNode_ResourcesReady 368B0AC. research/invisible-gpu.md §1 "Command objects" uses the same attribution.

#### 1. Fog (VFSCAT #2 1.07 ms; SSFog)
- **Chain:** VolumetricFogScatterPassRegisterType 244F3E8 (hash 3C0512C3) -> Factory 25B9F84 -> Ctor 345F12C -> vtable 40EBA10 -> **Execute 14871C**.
  - **Async compute** (AsyncSlotGetCtx(RS+1130h,4) non-null, the profiled case): RecordAsync 90749C -> SetupDispatchAsync 18FAD4 -> program vf+60h.
  - **Sync:** 38C1AF8 -> 38C15A0 -> 38C1084 -> program vf+68h.
  - The shader object is registered separately: VolumetricFogScatterShaderRegisterType 243A558.
- **What sets the amount of work.** 14871C computes a render-sized input (RS+960/964 are the render width / height, work-deciders.md; cb+14D0/14D4 is not the froxel grid X/Y):
  - X = RS+960h × view scale sc_view+1F00h × dyn-res sc_view+1ECCh (clamped to (0,2]);
  - Y = RS+964h × sc_view+1F04h × the same dyn-res factor;
  - the result is written to cb+14D0h/14D4h.
  - Depth slices come from the target resource (u16 +38h/+3Ah of 147854's result).
  - The quality knob upstream is ApplyVolumetricQuality 39B080 (S+13DC/13E0, already at max on HIGH); froxel X/Z come from VOLUMETRIC only. RS+960/964 are g_RenderResolution, copied by RenderSystemSyncRenderSize 0x12399D0 (work-deciders.md), so there is no S+13DC/13E0 -> RS+960/964 path.
- **Per-frame CPU:** the work is constant (one Execute per view). The extra branches (LPV, VSM keylight, dust wind) only toggle constants.
- **SSFog** is a shader member of the **StaticNature** node: StaticNatureRegisterType 244D1AC -> Ctor 346DE18 -> vtable 40EC6E0 -> Execute 6D7B88 -> 6D7B98 -> StaticNatureRenderView 108590 (S2F scattering params). The SSFog shader type itself is 243A058 (schema field at 0x2697F70).

#### 2. GI (RefProbe #7/#9 ≈ 1.0 ms; SDFOCC/RefProbe #20)
- **ReflectionProbePass:** Register 2448C5C -> Factory 25B2AC0 -> Ctor 368A33C -> vtable 410A428 -> **Execute 54294C** -> **Record 542C08**.
  - Execute skips the pass if the [[r+F8h]+968h] sign bit is set.
  - Record checks the RenderingType flags at node+E8h and the view-mode gate RS+95Ch&3, binds SubPoint/SubSpotLightShadows, and records the #121/#414 gathers.
- **Denoiser:** 2448AAC -> 25B2850 -> 367A494 -> vtable 4109CE8. Its Execute 367BD44 is a generic "vf+40h prepare → vf+38h record" (shared with vtable 4109CA8).
- **What sets the amount of work:** the gather render scale from GiRenderScalesApply 1872C4 / GiScaleLookup 187A50 (already documented). There is no per-frame throttle in Execute/Record; the probe radiance relight is event-driven (invisible-gpu.md).
- **Material side:** MaterialBindReflectionProbeLightingParams 3C3F7C sets probeGlobalDiffuseILScaler/SpecScaler per technique.
- **SkyAmbient pass** (244B1D8 -> 25B48AC -> 346DB38 -> vtable 40EC830 -> Execute 145C60) is **already time-sliced**:
  - 43 steps spread over a period of view+29Ch seconds (mode view+3C8h: 0 every frame, 1 sliced, 2 forced).
  - SkyAmbientDetectChange 1467EC resets the slicing when sky params change.
  - SkyAmbientRecordSteps 145F6C records the steps.
- LocalizedSkylight (2442EAC) and ReflectionProbeField (2448B3C, ctor 339E5C8) are data objects; they hold no per-frame code.

#### 3. Sky (DrawSky/VCLOUD #10 0.39 ms async)
- **Chain:** NatureSkyRegisterType 2445548 -> 25ADE2C -> 346D83C -> vtable 40EC750 -> Execute 293220 -> **NatureSkyRender 293230**.
  - **Mode A** (node+240h bit0): "RenderSkyPatternMap" (2935E4), refreshed only when NatureSkyUpdatePatternParams 5F1628 says the params changed (time delta sc+2CCh - node+24Ch).
  - **Mode B** ("NUpdate"): "DrawSky" = **NatureSkyDrawClouds 5F2A88** (VCLOUD), then "DrawScreenSpaceVelocity" 293784.
- **What sets the amount of work:**
  - VCLOUD renders into a quarter-res target (5F2A88 checks w==W>>2, h==H>>2).
  - The history target ping-pongs (node+210h/110h, node+244h toggle).
  - Cloud shadow/weather inputs: 2F95D80/2F95FF8/2F96228.
  - No time-slicing of DrawSky was seen: it runs every frame.
- **MergeNatureCloudAndPatternMap** (244348C -> 346D7EC -> vtable 40EC788 -> Execute 346ED64) runs only once per sim-time change (sc+2CCh != node+2Ch): check 3667FB4, then record 365BBD4.
- SkyDome 244B268 and SkyAmbient object 244B148 are data types.

#### 4. PB2 materials, trees, UltraMarine (#6 0.65, #12 0.34, #13 0.33 ms)
- **PB2/PB2TREE2** have no dedicated node. They are ordinary G-buffer draws through the draw-list path: SceneRenderView 535188 -> DrawListWalk -> DrawEmit 1E80B0 -> DrawSubmit 1D2F60. Per draw, MaterialTechniqueBind 3C77F4 calls MaterialBindPb2AmbientScaleParams 3C5914 and MaterialResetPb2SpecialParams 3C4598.
- Those two helpers strncmp the technique name ("PB2TREE2"/"PB2FUR"/"PB2EYEC"/"ThinSurfaceBSDF") **on every draw**. That is a small CPU cost; the GPU work is decided by the meshlet/draw lists (MESHLET_QUALITY → ApplyMeshletLodScales 2353C0; GRASS → GrassPlacementCull 31EF94).
- **UltraMarine (water):** UltraMarineRegisterType 244E6AC -> Factory 25B7064 -> Ctor 368ADE8 -> vtable **410A498** -> Execute 852620 -> 852650 -> 850F18 -> **UltraMarineRecord 850FE0** (marker "UltramarineRender" in cold chunk 1A23646). It renders once per active view slot (+188h).

#### 5. Shadows (WDO_SST/VSM draws #14 #17–19)
- **VirtualShadowMapPass:** 244F1C0 -> 25B9DA8 -> 368B3B0 -> vtable 410A5F8 -> VsmPassNodeExecute 465384 -> VsmPassExecute 465414. The VSM clipmap/tile budget is already named (VsmClipmapRender 1F8340, VsmTilesPerFrameBudget 1F7960).
- **Local lights:** LocalLightShadowmapRender 7E92B4 (point/spot/ESM) runs MeshletInstanceCulling + MeshletPreculling + DrawListWalk per light/face (8 sites).
- **What sets the amount of work:**
  - SHADOW_QUAL → ApplyShadowQuality 18A87C;
  - MaxShadows / ShadowmapResolution params;
  - the VSM tile budget.
- Shader types: VirtualShadowMap 243A4B8, WritefShadowPass 2437F38 (unnamed here), ShadowTree 243A0F8 (unnamed here).

#### 6. Culling (MPC #16 0.31 ms)
- MeshletPreculling / MeshletInstanceCulling are shader types (2439018 / 2438F78). Their records (5301FC / 53FD3C, already named) are called from 38 sites:
  - SceneRenderView 535188 (6 times);
  - LocalLightShadowmapRender 7E92B4 (8 times);
  - VsmPassExecute 465414;
  - ShadowCascadeRender 1F5C2C;
  - 1EB85C, 23584C, 237044, 44DE78.
- So **MPC runs once per culled view/shadow face**, not once per frame. Its cost scales with the number of shadow views. GpuCullingRecord 69566C ("GPUCulling") is a separate culling path, reached via 905E10.
- OcclusionCullingManager 2446370 is a data type (OccluderUnitArrayCapacity = 16).

#### Not done / open
- The VCLOUD cloud-step count source inside 5F2A88 was not traced.

## Pass 4 (2026-10-08): 100.00 % named in every view
110 new rows and 14 in-place renames of `Unknown_*` (merged). `perf_coverage.py --precise` gives 2-decimal coverage and lists every remaining unnamed function with 4 decimals.

| view | before (combined / Ally / PC) | after |
|---|---|---|
| attributed (default) | 99.80 / 99.75 / 99.85 %, 110 fns unnamed (0.1994 %) | 100.00 / 100.00 / 100.00 %, 0 unnamed |
| `--self` | 100.0 % rounded, 15 fns (13 PC, 2 Ally) unnamed or `Unknown_` | 100.00 %, 0 unnamed |
| GPU-note fns / PC incl >= 0.3 % | 59/59, 99/99 | unchanged |

124 functions in total (union of both views, every RVA with a nonzero share), all folded to function starts: none was a mid-function RVA. 14 are leaf fragments without .pdata (found through `leaf_ranges.tsv`: stubs like `mov al,1; ret`, getters, type-hash getters). Evidence per row is in its description (callers, callees, decomp). 66 had a decompile in `<re-scratch>\decomp`, the other 58 were read with capstone over the dumped exe (no Ghidra run).

By subsystem: synchronisation/event primitives and spin locks (SpinLockAcquireYield 363 callers, EventSetBool/EventSetStatus, EventPair*), allocators (arena/slab/TLSF wrappers), job-system job functions (JobFn*, Job*SignalDone), resource-handle readiness (ResHandleCheckReadyAndKickLoader, 1315 callers), D3D12 wrapper (Syn* root-slot/state transition, cmd list ring, upload ring), CRT (`_free_base`, `_Mtx_reset_owner`, vsnprintf), input/pad and mouse polling, audio/sound worker, Streamline Reflex, XeSS execute, physics step, UI/draw nodes, character/animation numeric bodies.
Confidence: 21 high/medium-high on names that are CRT/API wrappers; the large numeric bodies (AnimPoseEvalSkinningBatchHuge 100 KB, FrameTimeFloatSimStepLarge, SceneVisibilityGatherLarge, ResourceFileOpenAndLoadTask, ...) are marked `low`: named by dominant callees/shape, role refinement still possible. No `Unknown_` placeholders were used.
