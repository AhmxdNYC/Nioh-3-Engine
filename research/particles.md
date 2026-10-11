# Nioh 3: particle / effect pipeline (spawn, update, GPU sim, cull, draw, merge) and same-look waste

Static RE of the unpacked exe dump (Steam v2.02, RVAs) with read-only Ghidra queries, plus the existing CPU profiles,
the Ally GP2 digests (L1, L2) and the PC RGP r5 segments.
Nothing was run, patched or installed. The symbols named here are in symbols/symbols.tsv.
Builds on research/effects-animation.md (CPU update, E1), gpu-barriers.md §2, gpu-scheduling.md (AS0), invisible-gpu.md §4b.

## TL;DR
1. **Effects are cheap on the Ally at every measured spot.** Measured Ally GP2: the G1E effect draws are 0.185 ms (L1)
   and 0.226 ms (L2), and EffectMerge is 0.009 ms. The GPU sim chain does not reach GP2's table. On PC it is 0.32 ms,
   latency-bound: ~700 dispatches of 1-3 groups each, with a global UAV barrier between 10 stages.
   Scaling the PC's G1E 376 µs by 7.5 without the render-res factor (×0.25) would suggest ~3 ms on the Ally; the
   measurement is ~0.2 ms.
   **Combat is unmeasured.** That is the one capture that could change this ranking.
2. **New: on AMD the whole GPU-effect command recording runs synchronously on the render main thread, at 6-7 % of
   [App]Game.** The chain is EffectRenderNodeExecute 3D0AE0 → EffectViewRecordSync 365AA98 → EffectViewRecordGpu
   3D275C → 2EA7D24 → 2EC91E0 → **GpuParticleSimRecord 694B38**. It is 6.1-7.1 % inclusive in r5, r6-A1..B2, r7-A1..B1.
   On NVIDIA, async slot 0 runs the same 3D275C inside the KIDS job `KIDS_ASync_LLCulling_Effect` (3D16AC).
   The **existing AS0 patch** therefore also moves this CPU work off the main thread, not only the GPU dispatches.
   - So ui-mainthread.md's "6-8 % inclusive (365AA98 → …)" chain is mostly this effect recording, not UI.
3. **VISUAL_EFFECT has exactly one functional reader**, `EffectCreate` 4CE969. All references were re-checked: 4CE969
   READ, 607F78 menu, 1FE59F1 save, 12A4183 write. The three getter thunks have no code and no absolute-pointer references.
   - It does not touch GPU budgets, distances or the low-res effect buffer.
   - It only filters spawns that go through EffectCreate (23 callers). Spawns through other paths (stage/placeable
     effects) were not checked against the gate.
4. **Culling exists, but only per particle and only against the frustum.**
   - The CPU particle collectors (particle-type slot 1) test every particle in clip space.
   - They gate on emitter alpha ≥ 1.19e-5 and pick a distance LOD for mesh particles.
   - There is no emitter-level bounds early-out, no occlusion test and **no per-particle alpha-0 test**.
   - The pixel shaders only alpha-test the *texture* alpha against a cbuffer ref. A particle with vertex/colour alpha 0
     still pays raster, the full PS (lighting, fog, soft depth) and blend.
5. **The usual suspects are not waste:**
   - The GPU sim is recorded once per frame (guard `dl+0x2E3`), not once per pass or per view.
   - Objects with no elapsed time (`obj+0x84` < eps) are skipped.
   - Faded emitters are released (C7A7C).
   - Invisible (NoDisp) instances are CPU-updated; E1 already covers that.

## 1. Pipeline map (RVAs)
```
SPAWN    EffectCreate 4CE90C (VISUAL_EFFECT=LIMITED drops def flag 0x800000 at 4CE969) -> emitter spawn 2ECE610
CPU SIM  (Scheduler0) EffectManagerFrameUpdate 39E910 -> EffectManagerUpdateInstances 15FC80 (Disp/NoDisp split)
         -> 9 Disp + 1 NoDisp workers EffectWorkerRunList 3D83BC -> EffectSystemUpdate 3D9F64 -> emitter vfunc +0x40
         -> EffectEmitterUpdate C7A7C: time fade (+0x28..+0x34; release < 1.19e-5), bindings, then per particle group
            ParticleGroupTypeFnTable 0x3BE3C98 [27 types x 0x170 B] slot 0 = update (type 3 C7F4C, 7 C8958, 19 CE7E0,
            0/1 C683C, ...; types 10/11 = GPU groups, slot 0 = stub A954D0). Per-particle module VM CE7E0 (curves via
            table 0x3BE6AE0/0x3BE6690, node graph 6D360/219E80 on the effect threads).
RENDER   (main thread [App]Game on AMD; KIDS async job on NVIDIA)
         EffectRenderNodeExecute 3D0AE0 (per view index, view+0x974 + node+0x5B; 32 DLs at effmgr+0x180)
           async: job 3D16AC -> 3D275C          sync (AMD): EffectViewRecordSync 365AA98
         EffectViewPrepare 3D2DB0: collect the view's DLs (3D490C/3D4848/3D478C/3D46D0, 4 categories x <=7),
           OR flags, set view volume on GPU objects (EffectGpuObjsBindView 3D9B20), effect targets
         EffectViewRecordGpu 3D275C, per DL:
           sim : 1893374 -> 2EA7D24 -> EffectDlRecordSimImpl 2EC91E0  (DL flag 0x100; tokens 0x7C/0x7D around it)
           draw: 18933D4 -> 2EA7FCC -> EffectDlRecordDrawImpl 2EC9580 (DL flag 0x200)
             both call GpuParticleSimRecord 694B38 first (once per frame: dl+0x2E3 -> 0x100)
               per object: EffectGpuObjLatchSubsteps 3D859C (2 Sleep(0) spinlocks, 4-slot dt ring -> +0x70/+0x74)
               10 stages x every object x every group: GpuParticleGroupRecordStage 695264 -> sub-emitter vfunc +0xF0
                 (records Dispatch); stage 0 also 6947C0 -> 694458; after each stage with work: token 0x8A
                 (SynTokUavBarrier, NULL resource = global UAV barrier)
               object gate: flags 0x800000 and accumulated dt obj+0x84 >= 1.19e-5; group gate: +0x28 & 0x20
             draw: per group GpuParticleGroupRecordDraw 2EA23EC -> sub-emitter vfunc +0x118
             per-view fractions g_ViewGpuEffectSimFraction 0x4566008 (type-10 groups) / ...DrawFraction 0x4566000
             (type-11), < 1.0 keeps only that share; set per view by 4A58DC from view+0x2650/+0x2654;
             g_ViewGpuEffectsDisabled 0x4BD172C skips both.
         CPU particles: emitter vfunc +0x70 EffectEmitterCollectVisible 3DBBCC -> type slot 1 per group:
           billboard 7E1894 / 3DBE90, mesh 3DA9A0 (distance-LOD table +0x244/+0x248): clip-space test per particle,
           emitter alpha/visibility checks 3DAFBC/3DAF50, push to sink vtbl+8 with key = view depth + size bias
           (DL caps: EffectDisplaylistMaxPrimNum 7600, DepthResolution 4096; the sort itself was not decompiled)
         GPU particle manager: create 2EC6218 (0x53E0 B, ctor 2EC601C, vtable 0x4099A58), init 2EC6318 ->
           GpuParticleInit 2EC7AD4: 27 update CS (one per particle type) at +0x538, 3x27 at +0x610, more at +0x898;
           owned by the effect system (EffectSystemCreate 2EA8744, +0x3F0)
MERGE    EffectMergeShrinkBufferExecute 7F5838 (registered at 226AD63 into renderer+0x1EF2B0): N passes
         (vtbl+0x1E0 of the merge object from registry 0x4758B10, scale x factor +0x1EDC) composite the low-res effect
         buffer. RenderConfig fields +0x19BC EffectLowResoRenderLevel (ctor default -1, RenderConfigCtor 6C1ED4),
         +0x19C0.. RenderLevelTable*, +0x19D4.. LowResoCustomFlags*, +0x19E4.. LowResoScale (default 0.5),
         +0x19F0.. MixResoThreshold: env-data driven; no quality setting writes them.
```

## 2. Costs (measured)
| part | PC | Ally | source |
|---|---|---|---|
| CPU sim (EffectUpdateThread x10) | ~0.5 core busy, 96 % waiting | 1.9 % of profiled CPU | effects-animation.md §1.4 |
| GPU-effect recording on main thread (365AA98 incl.) | **6.1-7.1 % of [App]Game** (9 captures) | main thread 86.7 % in the Sleep(0) wait (GPU-bound), so fps-neutral | cpuprof r5/r6/r7, Ally L2 SP |
| GPU sim chain (RGP segs 70-80) | 0.32 ms, 0-13 % occupancy (168/190/190/190/74/75 dispatches) + 87.5 µs CP-DMA copy | below GP2 row threshold; est. 0.3-0.7 ms (latency-bound) | gpu-scheduling.md, gpu-barriers.md |
| G1E particle draws | 376 µs | **0.185 / 0.226 ms** (L1 / L2) | gp2 digests |
| EffectMerge | - | 0.009 ms | gp2 L2 #105 |

## 3. Same-look candidates, ranked by Ally impact (none built)
| # | candidate | where | Ally impact | PC impact | look / risk |
|---|---|---|---|---|---|
| 1 | **Measure combat first**: GP2 + SP in a fight, VISUAL_EFFECT NORMAL vs LIMITED | - | decides if anything below matters (idle: effects ≤ ~0.5 ms total) | - | none |
| 2 | **AS0** (exists, default off): enable effect async slot 0 on AMD. Moves the GPU sim chain to async compute **and** the 6-7 % main-thread recording into the KIDS job 3D16AC | 226A497 (patches.json AS0) | GPU: overlap of the latency-bound chain, est. 0.2-0.5 ms; CPU: fps-neutral | est. main thread -6-7 %, GPU -0.25-0.3 ms (not measured) | identical if the driver path works; KT disabled it on non-NVIDIA on purpose (driver bug risk) |
| 3 | **Cull particles whose final alpha is 0** before they reach the DL: the collectors push every in-frustum particle | slot 1 of 0x3BE3C98 (7E1894, 3DBE90, 3DA9A0, ...) before `sink->vtbl+8`; or PS early discard in the G1E shaders | unknown: fade tails only; ≤ 0.05 ms idle, more in fights | small | bit-exact **only** for blend states where src (0,0,0,0) leaves dst unchanged (alpha / additive / premultiplied); wrong for multiply/min blends and if dst alpha is read later (TAA EffectStencil). Needs per-PSO blend check + a live count of alpha-0 pushes first |
| 4 | Identify the **87.5 µs CP-DMA copy** after sim stage 9 (PC) | after RGP seg 79; d3d12 trace `CopyBufferRegion` | if it is a whole particle/count-buffer copy every frame, Ally runs it as a compute copy on 85 GB/s | ≤ 0.09 ms | unknown until identified |
| 5 | GPU sim record loop: 10 stages × every object × every group × sub-emitter vcall, most stages empty for a type; every real dispatch does a full state commit (DrawStateCommit 1DAF6C, ShaderConstWriteStream 1D7340) | 694B38 / 695264 | fps-neutral (CPU) | part of the 6-7 % main thread; per-type stage masks could cut the empty vcalls | low (CPU only) but needs an engine change; AS0 removes it from the main thread anyway |
| 6 | E1 (built, research/effects-animation.md §5): NoDisp update every N-th pass | 160B83 / 7E82B4 | fps-neutral | ≤ 0.04 core | invisible-only clock lag |
| - | Merge the per-emitter 1x1x1 dispatches | sub-emitter vfunc +0xF0 | ≤ 10-20 µs | ≤ 10 µs | needs shader + engine change (gpu-barriers.md §2): no |
| - | EffectMerge 7F5838 | - | 9 µs | - | nothing to gain |

## 4. Checked and not waste
- **GPU sim is not duplicated.** EffectDlRecordSimImpl and EffectDlRecordDrawImpl both call 694B38, but its first line tests and
  clears `dl+0x2E3`, so the sim is recorded once per frame per DL. The UAV barrier is per stage, not per emitter (≤ 10/frame).
- **Paused/no-time objects are skipped.** The first stage loop requires `obj+0xCC & 0x800000` and `obj+0x84 ≥ 1.19e-5`.
  `+0x84`, the substep count `+0x70` and the dt ring are reset after recording.
- **Faded effects stop.** EffectEmitterUpdate releases an emitter when its fade drops below 1.19e-5. The collectors
  require emitter alpha ≥ 1.19e-5 for particles flagged as fading.
- **Off-screen CPU particles are not drawn.** The per-particle clip-space test sits in every collector seen
  (billboards 7E1894/3DBE90, mesh 3DA9A0).
  - They are still *simulated* while inside the display distance (Disp). That is needed for correct state when the camera turns.
  - Instances beyond the display distance or with a hidden owner go to NoDisp (effects-animation.md, E1).
- **Distance LOD.** Instances use their display distance `e+0xC0` and height `e+0x194` (15FC80). Mesh particles pick a
  model LOD from per-group distance tables. Fading is time-based (C7A7C); no distance fade was found.
- **No occlusion culling for effects.** None was found; only the frustum test.
  - `UseEmitterOcc` (string 0x38ED328; users 2EE220/619C94 sit in script-function tables) was not checked. It is
    probably sound emitter occlusion.

## 5. Open questions (for a live session; the game was not run)
1. Combat capture (#1). Also count GPU effect objects per frame. The DL frame list is `dl+0x2F8[frame]`, with its count
   at `dl+0x2F0[frame]`.
2. Do NoDisp instances' GPU groups get dispatched? Find which code fills the DL object lists, or read `obj+0x84` live
   for an off-screen instance. If they do, an invisible-instance GPU skip would be the GPU twin of E1. It would not be
   same-look on reappearance, the same caveat as E1.
3. Alpha-0 share (#3): a counting hook on `sink->vtbl+8` in 7E1894/3DBE90 comparing the particle colour alpha.
   The colour is written by CE7E0 into `p+0x40..0x4C`; that it is the final vertex colour is a hypothesis.
4. Blend states of the G1E PSOs, needed before any discard edit (#3). Shaders: 0x5f066893.file PS 2c4db869 (32 KB,
   lit, fog, `discard if texA - cb[85].x < 0`) and d4ed3fe4 (16 KB, same test).

## 6. Consequences for other notes
- ui-mainthread.md: the "6-8 %" chain root 365AA98 is the effect GPU recording for a view,
  `EffectViewRecordSync`. The real UI share is smaller by ~6 % of the main thread.
- mainthread-logic.md §4.4: `3D859C` "UI/2D chain" is the per-GPU-effect-object substep latch inside 694B38.
- invisible-gpu.md §4b and round-2 #4: the "75 dispatches each with a UAV barrier" are per-emitter dispatches inside one
  of **10 sim stages**. There is one global UAV barrier (token 0x8A) per non-empty stage, which agrees with
  gpu-barriers.md §2.
- SynTokUavBarrier 113A0C is issued once per non-empty GPU-particle sim stage (≤ 10/frame), not once per
  particle-emitter dispatch.
