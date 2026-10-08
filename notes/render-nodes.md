# Render nodes: the engine code behind every GPU pass (2026-10-08)

Names: `re\symbols\incoming\render-nodes-renames.tsv` (1142 rows, pending merge). Method: `perf-coverage.md` "GPU-pass CPU paths", now scripted over **all** 769 types that the registry builder 26F5D84 registers: 70 render nodes, 216 shader objects, 483 data objects.

## How the chain was found (repeatable, no Ghidra needed)
- `strings.tsv` lists each type-name string together with its `<Type>RegisterType` (0x243xxxx..0x245xxxx). That function stores {handler table, name} and calls RegisterAssetHandler 3246E9C(reg, `mov edx` hash, &global).
- The handler table's slot 7 gives the family: 3254D00 = render node, 3240A98 = data, 25ECxxx = shader. Slot 22 is the factory.
- For a node, the factory or its callees (depth <= 2) `lea` exactly one vtable whose +18/+20/+28 slots are AA0140/AA0130/AA0120. That vtable gives +8 Execute, +10 ResourcesReady and +30 Release.
- This reproduces every chain that perf-rename-3 found by hand (StaticNature, VolumetricFogScatter, ReflectionProbePass and the rest).
- 174 functions in 0x248xxxx..0x24Axxxx only return a lazy {0, table, name} descriptor. They are named `<Type>TypeInfo`.

## Corrections and shared slots
- **0x295C44 was misattributed.** It is the **RealtimeLocalReflectionPass** Execute: vtable 410A0E0, ctor 3684F7C, factory 25B1D84, hash 0xAC078655. PrepareThinSurfaceBSDF is vtable 410A070, Execute 453ACC. The earlier name came from vtable adjacency.
- **EffectRenderNodeExecute 3D0AE0 is the Execute of the "ClusterLightCulling" node type** (vtable 4109970). Effects are recorded at that graph slot, which is consistent with effect lights feeding the clustered light lists. The name is kept and the description is extended.
- BeginCheckSkip, EndCheckSkip, Compound, ProfileEvent and RenderGraph share `RenderNodeBase_vftable` 40C9C48. Its Execute and Ready slots are `_purecall`, so the graph walker handles these structural nodes itself. 33 nodes use the `ret 0` release AA01C0.
- Nine Executes are 14-49-byte forwarders. The real body is named `<Node>ExecuteImpl`: for example ClusteredDeferredShading2Pass 545CC4 -> **54762C** (2.7 KB), MergeRLR2Pass -> 153E98, CombineHDR -> 1057AC, CopyColorRenderTarget -> 8493B4.

## CPU weight
Node Executes have ~0 self time. Their cost sits in shared leaves that are already named: Memcpy B94250 (Ally 2.9 %), ResourceIsReady 1A6030 (1.8 %), StructCopy32. Coverage with this file: **Ally 99.6 -> 99.7 %**, combined 99.5 % (the tail is about 800 functions of <= 0.01 % each). The gain is on the GPU side: every pass now has a named CPU entry point.

## GPU "other / unattributed" bucket (Ally GP2 175-L2: 2.65 ms of 30.1 ms) -> nodes
| Shader family (ms) | Node / code | Notes |
|---|---|---|
| GFM / HBAO, GFM-D, GFM (~0.65) | **SSAO** node: Execute 15612C (7.4 KB); **ApplySSAOPass** Execute 3688234 | HBAO PS 0x3de2e18a. GFM VS 0xec36dc74 is the full-screen VS shared with HBAO. GFM-only/GFM-D draws are likely the AO depth pre-steps (inferred, not traced). |
| copy / resolve / clear (0.55) | **CopyColorRenderTarget** 8493B4, **ClearScene** 8236D8 + the exe copy shaders (`gpu-copies-postres.md`) | |
| AMBSL (~0.43) | **not mapped** | `frame_account.py` places it at output time (UI). The node is still open. |
| PBRLR (~0.28) | **RealtimeLocalReflectionPass** 295C44, MergeRLRPass 3685BD0 -> 3686040, MergeRLR2Pass 9A6444 -> 153E98 | SSR trace, temporal filter, mip chain |
| G1E (~0.23) | EffectRenderNodeExecute 3D0AE0 (ClusterLightCulling slot), OldEffect node 14FAD8 ("EffReduction") | see `particles.md` |
| tonemap/compose + post VS (~0.2) | **CombineHDR** 1057AC, **HdrOutput** 6D8928 -> 6D89D4, Apply 945350 -> PostEffectApply, Fade 294C04, SSFadeSaturation 927DF8 | |
| SSFOG (0.15) | StaticNature (already named) | |
| SM-/Batch Tiles (0.11) | VirtualShadowMapPass (already named) | |

## SSAO node (GPU ~0.65 ms = 2.2 % of the Ally frame)
- **Role:** HBAO plus the AO apply, per view.
- **Data structures:**
  - The view gate uses the node byte at +14B (the effect node uses +5B). The per-frame render-settings block is `[r+F8]`: +968 is the valid flag, +974 the view base, +70 the view params.
  - Node +28/+30/+38/+40 hold the resource/shader slots. Bit 0 of +148 is a flag.
  - The draw-list items checked per group carry: +108 = group id, top nibble of qword 0 = 1 (kind), material +44 bit 20h = skip, params at +130..+140.
- **Hot loop:** for each of up to **7 switch groups** (bitmask), it walks the view's draw list for that group. It sets per-group params by **string-name lookup per draw**: `ParamFindByNameUnlessExcluded` 52DC20 with "s_depthParam", "s_viewParam" and "s_depthFade", each an inline strcmp against an exclusion list, then `ParamFindByName` 52DC84.
- **Same-look ideas:**
  - (a) CPU: resolve the three Param* once per node (bit-exact). This is fps-neutral on the GPU-bound Ally.
  - (b) GPU: the groups are separate full-screen HBAO draws rejected per pixel by `sPrmSwi & nSwitchRef`. Merging them would cut the redundant VS/raster and early-reject work, but it must win an Ally A/B first (RADV rule, STATUS 21:50). The r5 PC spot measured ~0, while the Ally shows 0.65 ms, so the cost depends on the scene.

## RealtimeLocalReflection (GPU ~0.28 ms PBRLR + merges)
- **Role:** screen-space reflection.
- **Flow:** RealtimeLocalReflectionPass (295C44: temporal filter PS 449cf0ae, clears through "PBP_TextureClear") -> MergeRLRPass / MergeRLR2Pass (forwarders into 3686040 / 153E98).
- **Settings:** SSR quality scales the ray steps at S+0x107C (`gpu-maxsettings.md`).
- **Same-look idea:** none new (`gpu-shader-sweep.md` #13 was already ruled out).

## Clustered/tiled deferred shading (PB2 lighting, part of the 2.19 ms materials bucket)
- **Role:** deferred lighting. ClusteredDeferredShadingPass 3677264 (6.7 KB) binds SSSTransmit, Sub{Point,Spot}LightShadows, ShadowMask, AmbientOcc, HatchingShadow and Illustration (59 virtual calls). ClusteredDeferredShading2Pass Impl is 54762C, TiledDeferredShadingPass 3678CB4.
- **Light culling:** TileLightCulling 3672030, ClusterDecalCulling 52BAFC, ClusterLasmapCulling 52A9EC.
- **CPU:** ~0.

## Open
- AMBSL is not yet tied to a node.
- The GFM-only draws are inferred as AO pre-steps.
- 483 data-object types have only RegisterType/TypeInfo names. Their factories, ctors and vtables (for example SkyAmbient data 2586404 -> 35E46EC) are not walked.
