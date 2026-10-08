# Perf coverage: naming every profiled Nioh3.exe function

Goal: the symbol DB (`re\symbols\symbols.tsv`) should name every function that shows up in a profile.
Tool: `py -I re\scripts_re\perf_coverage.py [--before] [--self] [--list N] [--incl N] [--gpu] [--tsv out]`.
New rows: 189 (34 high, 53 medium, 102 low).
- **Already merged.** The main session merged them into `symbols.tsv` on 2026-10-07 at 22:41. The source file is `incoming\merged61007-224150_perf-coverage.tsv`.
- **Still pending.** The 37 placeholder **renames** are in `re\symbols\incoming\perf-coverage-2.tsv`. The merge kept the old placeholder names, so these must be applied **in place**: same RVA, replace the name. Until then, `symbols.tsv` alone gives 77.5 % self / 97.1 % wall (`--before`).

## Method
- **Sources.** The script reads:
  - the Ally ASI sampler: every `SP` block in `captures\ally\*.log`, 5 logs;
  - the PC sampler `cpuprof`: 12 captures `captures\cpuprof-*.txt.tsv`. It uses the `attr` and `self` views for the coverage figure and the `incl` view for callers. `cpuprof-test` is skipped because it uses an older format;
  - every function RVA cited in `notes\gpu-*.md`, `d3d12-submission.md` and `invisible-gpu.md`.
- **Weighting.** Each log or capture is normalised to its sample total, then averaged. "Combined" is the mean of the Ally and PC shares.
- **Folding to function starts.** Each RVA goes to its function start through chained `.pdata` in `re\Nioh3.dump.exe`.
  - Leaf functions have no `.pdata`. For them the script uses the Ghidra table `re\out\functions.tsv`.
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

## Renames (pending: `incoming\perf-coverage-2.tsv`, to be applied in place)
`merge_incoming.py` keeps an existing name and only appends "also: ...". These 37 rows are meant as **renames**. Each one's description ends in `was <Placeholder>_<rva>`:
- `MainHot_*`:
  - 418F40 -> WaitDispatcherDrainSpin
  - 5DF70 -> MeshletPageCountdownTick
  - 1D84A0 -> StageSlotTableUpdate2568
  - 1D8720 -> StageSlotTableUpdate2578
  - 1D8220 -> StageSlotTableUpdate6780
  - 1D7EB0 -> StageSlotTableUpdate5F20
  - 1DAF6C -> DrawStateCommit
  - 1E0B64 -> Unknown_StateCommit_1E0B64
- `Sched0Hot_*`:
  - Object update: 1A7BF0 ObjUpdatePerObject, 121FC0 ObjUpdateWorkerBody, 16CFF0 ObjUpdateJobDispatch, 425B0 Unknown_ObjUpdate_425B0
  - Bounds: 71C60 BoundsFromPartObbs, 6EA50 ObbToAabb
  - Vector math: 6EFD0 Vec4AddInPlace, 6F010 Vec4Scale, 6F060 Vec3Dot, 174610 Vec4CopyW0, 54360 SinCosApprox
  - Other: 12B440 RollingAvgListUpdate, 66D00, 889C0, E32D90, F33A0, 16BA70, 12FBDD0, 284ED4, 13484C, F450C (the last seven are `Unknown_*`)
- `RenderSubmitHot_*`:
  - 119820 PsoCacheBTreeFind
  - 1192B0 ShaderVisibleDescRingAlloc
  - 115B9C SynBindTextureSlot
  - 118E28 SynUploadCbvTable
  - 114A20 SynTokSetIndexBuffer
  - 116260 SynTokResourceBarrier
  - 58C144 Unknown_BTreeFind_58C144
  - 1164C4 Unknown_SynTok_1164C4

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
- **Main-thread state commit.** `DrawStateCommit` 1DAF6C and the four `StageSlotTableUpdate*` functions. The 1E0B64 / 1E55D4 / 1E5854 / 1E8058 helpers remain `Unknown_StateCommit_*`.
- **Dispatcher (D3D12 submission).** `WaitDispatcherDrainSpin`, `PsoCacheBTreeFind`, `ShaderVisibleDescRingAlloc`, `SynUploadCbvTable`, `SynBindTextureSlot`, `SynTokSetIndexBuffer`, `RingWaitIdleSpin` 252730 (a spin without PAUSE on GfxSubmitFrameCmdList paths).
- **MissionThread.**
  - Call chain: `MissionThreadEntry` E5E060 -> `MissionThreadRunFrame` E63140 -> `MissionThreadUpdate` E64700 -> `MissionThreadStep` 20B900.
  - Task helpers are still `Unknown_Mission_*`.
- **Threads / CRT.** `CrtThreadStart` BDB740, `AppThreadTrampoline` 5B6670, `_Cnd_wait`, `HandleWaitMs` 8629AC (4 % of PC wall time is blocked here), `WinMain`, `__scrt_common_main_seh`.
- **GPU-setting paths from the notes.**
  - GI = LIGHTEST skip sites: `GiLightestParamsApply`, `GiLightestFlagApply`, `GiLightestSkipTest`.
  - Shadow-tier setters: `ParamTableSetU32_*`.
  - Landscape: `LandscapeLodScaleApply`, `LandscapeLightestFlagsApply`.
  - Other: `RenderFrameSettingsApply` 39B570, `DebugRenderScaleInfoPrint`.
  - The remaining "unnamed" GPU-note hits are not functions: they are hashes or offsets that happen to land in `.pdata`, or D3D12Core offsets.

## Remaining gaps
- **`Unknown_` share of self time, by area (combined):**
  - ObjUpdate 3.7 %
  - Thread5892 1.5 %
  - Mission 0.8 %
  - StateCommit 0.8 %
  - Transform 0.5 %
  - SceneDraw 0.4 %
  - Geom, ObjState, ObjParts, SynTok, Anim, BTreeFind, Container about 0.3 % each
- **Unnamed tail.** 3.4 % of self time is spread over about 1,060 functions. The largest is 0.05 %: 3D8770, 1FDC1C (`Sched0Hot_`), 4FC330, 1989F0, 42154 ...
- **Unnamed thread 5892** (12.6 % CPU in cpuprof-r9): entry 1A786A8. Evidence:
  - it sets MXCSR FTZ;
  - it runs float block DSP (E5840) and a decoder-like routine (18AB80, negative error codes);
  - it shares `HandleWaitMs` with AudioThread.
  It is probably audio decode/mix, but that is unproven. Ghidra has no strings for it. A live thread-name or stack probe would settle it.
- **Not identified.** The object-model "Obj" layout:
  - known fields: flags +4Ch, part slots +240h (count +120h), component +158h, attachments +400h;
  - unknown: the engine class name. Many `Unknown_ObjUpdate_*` rows would become real names once one vtable/RTTI anchor is found.
- **Caveat.** Both samplers measure wall time, so self time on PC includes spin loops. Ally `[App] other` rows are partly stale-stack attributions (see `cpu-hotspots-ally.md` §1). Examples are 37F550 and 4A09F0.
