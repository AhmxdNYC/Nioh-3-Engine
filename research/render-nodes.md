# Render nodes: the engine code behind every GPU pass (2026-10-08)

Names: 1142 rows, merged into symbols.tsv (source `render-nodes`). Method: `perf-coverage.md` "GPU-pass CPU paths", now scripted over **all** 769 types that the registry builder 26F5D84 registers: 70 render nodes, 216 shader objects, 483 data objects.

## How the chain was found (repeatable, no Ghidra needed)
- `strings.tsv` lists each type-name string together with its `<Type>RegisterType` (0x243xxxx..0x245xxxx). That function stores {handler table, name} and calls RegisterAssetHandler 3246E9C(reg, `mov edx` hash, &global).
- The handler table's slot 7 gives the family: 3254D00 = render node, 3240A98 = data, 25ECxxx = shader. Slot 22 is the factory.
- For a node, the factory or its callees (depth <= 2) `lea` exactly one vtable whose +18/+20/+28 slots are AA0140/AA0130/AA0120. That vtable gives +8 Execute, +10 ResourcesReady and +30 Release.
- This reproduces every chain that perf-rename-3 found by hand (StaticNature, VolumetricFogScatter, ReflectionProbePass and the rest).
- 174 functions in 0x248xxxx..0x24Axxxx only return a lazy {0, table, name} descriptor. They are named `<Type>TypeInfo`.

## Attribution notes and shared slots
- **0x295C44 is the RealtimeLocalReflectionPass Execute** (RealtimeLocalReflectionPassExecute): vtable 410A0E0, ctor 3684F7C, factory 25B1D84, hash 0xAC078655. The adjacent PrepareThinSurfaceBSDF vtable is 410A070, Execute 453ACC; do not attribute by vtable adjacency.
- **EffectRenderNodeExecute 3D0AE0 is the Execute of the "ClusterLightCulling" node type** (vtable 4109970). Effects are recorded at that graph slot, which is consistent with effect lights feeding the clustered light lists. The name is kept and the description is extended.
- BeginCheckSkip, EndCheckSkip, Compound, ProfileEvent and RenderGraph each have their own vtable and Execute (153014, 1114B8, 969F80, 3465ACC, 3465B04; renderer/05-frame-graph.md §2.2). `RenderNodeBase_vftable` 40C9C48 with its `_purecall` Execute / Ready slots is only the base class that derived ctors override. 33 nodes use the `ret 0` release AA01C0.
- Nine Executes are 14-49-byte forwarders. The real body is named `<Node>ExecuteImpl`: for example ClusteredDeferredShading2Pass 545CC4 -> **54762C** (2.7 KB), MergeRLR2Pass -> 153E98, CombineHDR -> 1057AC, CopyColorRenderTarget -> 8493B4.

## CPU weight
Node Executes have ~0 self time. Their cost sits in shared leaves that are already named: Memcpy B94250 (Ally 2.9 %), ResourceIsReady 1A6030 (1.8 %), StructCopy32. Coverage with this file: **Ally 99.6 -> 99.7 %**, combined 99.5 % (the tail is about 800 functions of <= 0.01 % each). The gain is on the GPU side: every pass now has a named CPU entry point.

## GPU "other / unattributed" bucket (Ally GP2 175-L2: 2.65 ms of 30.1 ms) -> nodes
| Shader family (ms) | Node / code | Notes |
|---|---|---|
| GFM / HBAO, GFM-D, GFM (~0.65) | **SSAO** node: Execute 15612C (7.4 KB); **ApplySSAOPass** Execute 3688234 | HBAO file 0x3de2e18a. GFM VS 0xec36dc74 is the full-screen VS shared with HBAO. GFM / GFM-D PSs are generic separable Gaussian filters (`vGusPrm`, `vGusWgt[3/4]`, ~4 draws/frame), not AO pre-steps. The AO pre-steps are HBAO #3 (full-res linear depth, 0.13 ms) and #4 (half-res point downsample, 0.08 ms). |
| copy / resolve / clear (0.55) | **CopyColorRenderTarget** 8493B4, **ClearScene** 8236D8 + the exe copy shaders (`gpu-copies-postres.md`) | |
| AMBSL (~0.37-0.43) | **StaticScreenLayout** node: Execute 253C68 (= UiViewRender) -> UiTextFrame 254440 -> UiRenderPathEntry 7D61A8 -> UiRenderPath 7D622C -> UiDrawLayer 1C0FFC -> UiQuadBuild 1CD690 | Screen-layout (2D UI) batched-quad shader, file 0xadb3a8f3, 7-12 draws/frame. See "AMBSL" below. |
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
  - (b) GPU draw merge: **nothing to merge** (see next section).

## HBAO draw merge: nothing to merge
The engine already merges the switch groups into one draw:
- The HBAO main PS ships as permutations with `nSwitchRef[N]`, `vRadiusParam[N]`, `vRayParam[N]` and `vOccParam[N]` for N = 1, 2 or 3 (0x3de2e18a #9..#34). One draw tests `sPrmSwi & nSwitchRef[i]` per pixel, picks parameter set i, and writes 1.0 (no AO) where no group matches. There is no discard. Several full-screen draws would overwrite each other, so the design itself rules out one draw per group.
- On the Ally (GP2, 175-L1/L2, every window), each AO stage runs **exactly 1.0 draw per frame**: lin-depth #3 0.13 ms, half-res downsample #4 0.08 ms, HBAO main #23 (N=1) 0.033 ms, blur H #6, blur V #7 0.042 ms, temporal #8. In 175-L3 the 2-group permutation #26 (or #24) replaces #23 and is still one draw.
- The 7-group loop in 15612C does not issue HBAO draws. It copies a 0x54-byte per-group block (item +128..+178), calls the view setup FD8CEC8/FD33E1C per group, and draws mesh draw-list items with alpha test ("vATest", "s_depthParam"...). These are geometry passes, not full-screen AO.
- **Result:** no ASI flag and no shader. Expected Ally gain: 0 ms.
- The only exact AO-side idea left is to fold #4 into #3, which would save ~0.08 ms at most. A PS cannot write two render-target sizes in one draw, so this needs a CS replacement plus command-list surgery. HBAO samples #4 through a sampler at half-res UVs, so reading #3 directly is not bit-exact. Not pursued.

## AMBSL = StaticScreenLayout UI quads (2026-10-08)
- **Shader (0xadb3a8f3, 37 VS + 39 PS):**
  - VS inputs are POSITION, COLOR, TEXCOORD and BLENDWEIGHT (one float). The cbuffer holds `cDiff`, `cAmb`, `mW2P`, `mL2W[192]` (64 x 3-row transforms) and `Gamma`.
  - PS cbuffer: `SaturationScale`, `Gamma`, `ContrastValuesOfTexture[3]`, `vATest`, one texture `sStage0`. The file also names `sSCLDepth`.
  - BLENDWEIGHT is the per-quad index into `mL2W`.
- **Tie to the node (code):** UiQuadBuild 1CD690 writes 44-byte (0x2C) vertices, 4 per quad (0xB0). It stores `(float)quadIndex` at vertex +0x28, the BLENDWEIGHT slot right after the RGBA8 color that ends at +0x27 (alpha is scaled by the fade factor). This is the AMBSL VS vertex layout.
- **Path:** UiDrawLayer is reached only from UiRenderPath (and UiRasterizePath, the glyph pre-pass), under UiTextFrame. UiTextFrame is called from UiViewRender 253C68, which is the StaticScreenLayout node's Execute (vtable 40EAF20).
- **Marker:** the GP2 marker column is "-" for every AMBSL PSO, which matches the UiTextFrame branch, not the "OffScreenRendering" event branch of the same Execute. "UIOutputForFSR" (3451A94) only begins when S+0x25B1 is set (separate UI target for frame generation).
- **Cost (Ally GP2):** 0.37 ms/frame, 7-12 draws. #33/#43 0.15 ms over ~5 draws/frame. #13/#45 is a single ~0.1 ms draw, probably a full-screen HUD layer.
- **Names:** merged into symbols.tsv (source `render-nodes (ambsl)`).
- **Same-look ideas:** not evaluated yet. Output-res UI work, ~1.2 % of the Ally frame.

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
- AMBSL is mapped (StaticScreenLayout). Its PS cost (#43, #45) has not been looked at.
- 483 data-object types have only RegisterType/TypeInfo names. Their factories, ctors and vtables (for example SkyAmbient data 2586404 -> 35E46EC) are not walked.
