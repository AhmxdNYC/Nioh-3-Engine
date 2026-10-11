# renderer/10-effects: effect instances, CPU / GPU particles, decals, ripples, effect lights (package 10, 2026-10-10)

Static pass over `<re-scratch>\decomp` plus `particles.md`, `effects-animation.md`, `ally-ripple-decal.md`, `cost-vs-looks.md` and `05-frame-graph.md`. Game not run.
Tags: **M** measured (cited note), **S** read in the C export, **?** inferred. This note is the renderer-side contract; `particles.md` carries the full RVA walk and the same-look
candidate list, so it is summarised here and not repeated.

## 1. Entry points

| role | function | notes |
|---|---|---|
| spawn | EffectCreate 4CE90C (3.7 KB, 23 callers) | the only functional reader of VISUAL_EFFECT (4CE969); creates the emitter set (spawn 2ECE610) |
| system | EffectSystemCreate 2EA8744 (from PostEffectLibraryInit 32FAE8C) | owns the GPU particle manager at +3F0 (GpuParticleMgrCreate 2EC6218, vtable 4099A58, 0x53E0 B) |
| per-frame CPU | EffectManagerFrameUpdate 39E910 (338 B, from GameModeUpdateMain) | see §2 |
| CPU emitters | EffectManagerUpdateInstances 15FC80 (5.1 KB), EffectManagerUpdateEmitters 15E658 (2.4 KB) | Disp / NoDisp split, attach-point matrices (MatrixFromScaleQuatTrans) |
| render node "ClusterLightCulling" (slot 161; 05 table) | EffectRenderNodeExecute 3D0AE0 (1 KB) | **records the effect GPU work at this graph slot**; `05-frame-graph.md` and `render-nodes.md` tie it to the clustered-light-list node |
| view record | EffectViewRecordSync 365AA98 (1.2 KB) -> EffectViewPrepare 3D2DB0 -> EffectViewRecordGpu 3D275C | sync on the main thread (AMD) or in KIDS job 3D16AC `KIDS_ASync_LLCulling_Effect` (NVIDIA, async slot 0) |
| render node "OldEffect" (16 instances: 98, 195, 196, 306, 321 ... 426) | OldEffectNodeExecute 14FAD8 (4.1 KB) | the **G1E** forward effect draw. It opens an `"EffReduction"` event when `RS+1FC & 20000000` (the low-res effect reduction), resolves two RT handles and calls the draw/merge sub-recorders. L21 in `05` |
| merge | EffectMergeShrinkBufferExecute 7F5838 | composites the low-res effect buffer (RenderConfig+19BC.. `EffectLowResoRenderLevel`, scale 0.5 default) |
| decal cull | ClusterDecalCullingNodeExecute 52BAFC (shell, 5 callees; graph 0x44B44454, nodes 6, 7, 31..199 on every view) | builds the clustered decal lists `tCdlDecalIndices` |
| decal draw | GBufferEditMiasmaDecalNodeExecute 937D68 -> GBufferEditMiasmaDecalNodeExecuteImpl 937D7C (11.9 KB, 83 callees); node 143 | G-buffer edit with decal copy, plus the `PB2DECAL` fullscreen passes at nodes 122, 131, 140, 215, 258, 259 |
| ripples | RippleNodeExecute 1031E4 (node 145; shader types Ripple 0x2C136E26 / Ripple2 0xC18A1958) | surface ripple draws; **not** the `PB2DECAL` pass (ally-ripple-decal.md §1) |

## 2. CPU side: per-frame update **S**

`EffectManagerFrameUpdate` 39E910 (inside GameModeUpdateMain):
1. mode flag `mgr+140` (1 or 2) triggers `FUN_A5B75C` with parity; failure resets the mode to 0.
2. `EffectThreadListsReset` 7E82B4 empties the Disp (+3500) and NoDisp (+6D00) work-entry vectors (std::vector of 0x30-byte `{sys, obj, dt, attach pos}`, `EffectWorkEntryPush` 73A784).
3. `EffectManagerUpdateInstances` 15FC80: per instance, display distance `e+C0`, height `e+194`, layer groups (`EffectLayerGroupsUpdateChildren` 79EE1C), pushes Disp or NoDisp entries.
4. `EffectManagerUpdateEmitters` 15E658: attach matrices, fades.
5. `EffectDispKick` 3D6F14 sets the start events of 9 Disp workers (`EffectWorkerRunList` 3D83BC -> `EffectSystemUpdate` 3D9F64, sub-stepped half-dt twice). NoDisp (1 worker) is kicked at `GameModePostUpdate` 565F64 and joined at the next update. Join: `EffectDispJoin` 3D6F60 on KIDSTaskScheduler0, never on [App]Game.
6. Cost: ~0.5 core, 96 % waiting; Ally 1.9 % of profiled CPU **M**.

Emitters: `EffectEmitterUpdate` C7A7C (time fade, release below 1.19e-5), per particle group via the type table `ParticleGroupTypeFnTable` 0x3BE3C98 (27 types x 0x170 B; slot 0 update, slot 1 collect; types 10 / 11 are the GPU groups with stub update). Per-particle module VM at CE7E0.

## 3. Render side

**View scope.** Per view `index = (node+5B & 7F) + (R+974 & 7FFFFFFF)`, DL table at `effmgr+180[32]`. The effect manager has up to 4 categories x 7 display lists (collectors 3D490C / 3D4848 / 3D478C / 3D46D0).

**CPU particles** (billboard 7E1894 / 3DBE90, mesh 3DA9A0): collector per type slot 1 does a clip-space test per particle and pushes to the sink with `key = view depth + size bias`. DL caps: `EffectDisplaylistMaxPrimNum` 7600, `DepthResolution` 4096. They draw through the OldEffect node (G1E, forward, lit, fogged, soft depth). No occlusion, no per-particle alpha-0 test **M** (`particles.md` §0.4).

**GPU particles.**
- Manager: 27 update CS (one per type) at mgr+538, 3 x 27 at +610, more at +898, render targets at +52B0/+52B8, viewport scale at +53A0.
- Sim: `EffectViewRecordGpu` 3D275C per DL calls 1893374 -> 2EA7D24 -> `EffectDlRecordSim` 2EC91E0, which calls **`GpuParticleSimRecord` 694B38 once per frame** (guard `dl+2E3`): latch substeps (`EffectGpuObjLatchSubsteps` 3D859C), then **10 stages x every object x every group** -> `GpuParticleGroupRecordStage` 695264 -> sub-emitter vfunc +F0 (one dispatch each), and **one global UAV barrier (token 0x8A) after each non-empty stage** (<= 10 per frame).
- Draw: `EffectDlRecordDraw` 2EC9580 per group (vfunc +118), per-view fractions `g_ViewGpuEffectSimFraction` 4566008 and `...DrawFraction` 4566000 (set by 4A58DC from view+2650 / +2654), `g_ViewGpuEffectsDisabled` 4BD172C.
- The G-buffer-lit part: none. Particles are forward-shaded in OldEffect (L21) after the opaque lighting.

**Effect lights.** Effect lights are point / spot entries submitted into the same CPU light array as map lights and consumed by the clustered list builder (`tCll*`); EffectRenderNodeExecute running at the ClusterLightCulling slot is consistent with this (`render-nodes.md`, `lighting.md` §2). The effect-side producer (which emitter field becomes a light, the light-count cap) was not isolated: **question for package 03 / game-side**.

**Decals.**
- Map decals are data objects `Decal`, `DecalRoad`, `MiasmaDecal`, with shader types `PhysicallyBased(2)Deferred(Miasma)Decal`.
- `ClusterDecalCulling` (a compute node at 6 / 7 / 31 / 32 ... 199) writes `tCdlDecalIndices` per view.
- `PB2DECAL-CdlRgbSfb` (PS c013d811, `0xc5bc597a.file`) is the deferred-decal pass: a **fullscreen triangle per stencil material class** (~4 draws per frame) that reads GBuf0-3 + depth + the cluster list and writes the 4 MRTs back (blending off, depth off, stencil EQUAL 2f / fb / fd / fe). It is a read-modify-write of the G-buffer ("ping-pong" through `GBufferEdit*`, 7 edit nodes 122 / 131 / 140 / 143 / 215 / 258 / 259).
- Miasma decals (the curse field) have their own 11.9 KB record (937D7C): view params, quaternion/axis transforms, `CopyRectCommand` for the hazard copy.

**Ripples.** `Ripple` node 145 draws rain / water-contact ripples (shader types Ripple, Ripple2); RainDrop (async slot 8, `09-volumetrics.md`) feeds it. Not isolated in GP2.

## 4. Costs (existing data)

| part | PC (RX 9070) | pc2 | Ally |
|---|---|---|---|
| GPU sim chain (~700 dispatches of 1-3 groups, 10 barriers) | 0.32 ms, 0-13 % occupancy | below the GP2 threshold | est. 0.3-0.7 ms |
| G1E draws | 376 us | | 0.185-0.226 ms (L1 / L2) **M** |
| EffectMerge | | | 0.009 ms |
| PB2DECAL | | 0.216 ms (1248x702) | 0.55 ms (960x540) **M** |
| CPU: GPU-effect recording (365AA98 chain) | 6.1-7.1 % of [App]Game on AMD | | fps-neutral (GPU bound) |
| CPU: simulation | ~0.5 core | | 1.9 % |

Combat frames are unmeasured (`particles.md` §5). A "RippleDecal 1.92 ms" reading on the Ally is a measurement artefact (skipping the pass corrupts the G-buffer); the pass costs ~0.55 ms and has no same-look lever (`ally-ripple-decal.md`).

## 5. New renderer: keep / replace

- **Keep (as inputs):** the CPU effect update (game-side, threaded, cheap), the per-type update CS and their parameter blocks, the `ClusterDecalCulling` list and the decal material records, the effect-light submissions, the low-res reduction + merge scheme. A new backend should consume the DL (display list) objects per view and re-record them.
- **Replace / restructure (the real wins):**
  1. **GPU sim recording.** 700 tiny dispatches and 10 barriers are latency, not throughput. Batch one dispatch per particle type (27 CS) per stage using indirect args, or a single persistent-thread sim, and drop the per-stage global UAV barrier for per-buffer barriers. On RDNA this removes the ~0.3 ms critical path without touching visuals. Needs the stage / group descriptor layout (sub-emitter vfunc +F0): follow-up RE.
  2. **Record off the main thread everywhere.** `EffectViewRecordGpu` already works as a job (async slot 0 on NVIDIA). Allow it on AMD (patch AS0, exists, default off) or run the record in the new submission layer's worker. Expected gain (E, not measured): up to the 6-7 % of the main thread that the recording costs on AMD.
  3. **PB2DECAL.** Per-material-class fullscreen triangles read-modify-write the full 4-target G-buffer. A new renderer should apply decals to the G-buffer during the G-buffer fill (or tile-classified so empty tiles skip), not as a separate pass over all pixels. Estimated gain (E) on the Ally: up to ~0.4 ms; on PC small. This is a renderer-design change, not an easy patch.
  4. **Alpha-0 particle early-out** (collectors): small, unmeasured.
- **Cheap wins without a new renderer:** AS0 on AMD (CPU), half-res particle buffer for G1E (est. 0.17 ms on the Ally per `cost-vs-looks.md`, medium risk).
- **Questions:** the effect-light producer and cap (03 lighting); `EffectDlRecordSim` internals and the sub-emitter dispatch descriptors (backend + GPU particle RE).
