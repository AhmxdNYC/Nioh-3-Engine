# Selective VRS v1 (#69321)

Base: ceiling source 2a0418b2 / checkpoint f5525748, ASI 8139fcc5. Lead supplied pc2 RX 6650 XT, 1080p FSR65, stage7356/P2: OFF 50 -> ceiling2x2 59 FPS (+19%), severe glitter (bridge file5129). These are the lead's measurements, not selective results.

Restart between arms. Existing modes 0/1/2 retain behavior unless the new guard is explicitly enabled.
```ini
[Perf]
VrsCeiling=3
VrsGBufferMotion1x1=1
```
Mode3: 2x2 on admitted opaque MRT draws, all others explicitly 1x1. Guard defaults to1 in mode3 and0 in modes1/2. Set guard0 to test the same selective predicates while allowing the recognized RG motion-candidate target. For a ceiling isolation arm use VrsCeiling=1 plus VrsGBufferMotion1x1=1 (normal ceiling=1/guard0). This guard applies to the entire draw, not a single MRT: D3D12 shading rate cannot differ per output target.

PSO metadata: existing early psoswap create hooks and graphics pipeline-library loads attach immutable metadata via SetPrivateData. Reads occur on PSO bind, no shader parsing or object ownership retention per draw. Pointer reuse cannot inherit stale private data. Unsupported stream PSOs/DXIL, failed metadata, or ShaderSwap/ShaderHot are unknown and remain fine. Dynamic shader disassembly is cached at PSO creation by container digest using system D3DCompiler47; absent DLL / failed disassembly is fine.

Admission is deliberately narrow: observed 7-RT G-buffer shape with D32S8 depth, depth enabled, culling, no GS, single sample, no alpha-to-coverage, blending or logic ops. Only normalized material formats are accepted (RGBA8, RGB10A2, RGBA16 UNORM/SNORM, R8/RG8 UNORM). FLOAT HDR, integer masks and unknown formats reject the entire draw. RG16 FLOAT/SNORM/UNORM and RG32 FLOAT are motion candidates: guard1 rejects them in an MRT draw. Pixel disassembly must be PS5.0/5.1 without discard, UAV declarations, depth/coverage/stencil outputs or sample-frequency inputs. Single-target post/history/UI, depth-only, particles/blended and GBufferEdit-style depth-disabled passes stay fine. Format/layout eligibility is a conservative candidate classification, not a proof of semantic resource identity or visual equivalence.

The notes confirm 7 RTs but do not contain their exact PSO format tuple. Therefore eligible coverage may be small or ZERO, especially with motion guard1; do not describe a zero-tag run as successful VRS. This v1 must first be measured by the lead. The shader/pixel packing cause of the glitter remains unproven. Both guard values expose exactly how much coverage is sacrificed; no +19% retention claim.

State: successful Reset initializes 1x1 and the initial PSO; ClearState, Close/Release, bundles, unknown bind paths and render-pass end invalidate eligibility. OMSetRenderTargets / BeginRenderPass supply RT count; unknown/mismatched counts remain1x1. Before every direct/indirect/mesh work call select rate again, so coarse cannot leak into an excluded following pass. State caches are thread-local; misses/migration fail fine. No compute shader work is reduced.

Every10s: VRS signature n=... rt=<8 DXGI enum numbers> tagged2x2=... fine1x1=... rejectOR=... . Counts are draw API calls, not expanded indirect draws. Reasons: 0x1 unknown, 0x2 depth/no pixel shader, 0x4 blend/logic, 0x8 shader unverified/cutout/side effects, 0x10 HDR/unsupported format, 0x20 motion guard, 0x40 not opaque7RT G-buffer, 0x80 MSAA/coverage, 0x100 unknown/mismatched binding. Up to256 signatures, no eviction; excess signatures remain rate-classified but omitted from per-signature counts.

Validation: real D3DCompile/D3DDisassemble opaque vs clip vs depth-output fixtures; PSO policy for blends/HDR/motion/unknown; mock native calls cover 2x2->1x1, missing OM state, guard toggle, bundle, ClearState, Close, successful/failed Reset, capability fallback. build.bat runs these plus existing HUD tests. No game launched, installed or benchmarked by this seat; lead owns pc2/main-PC visual/performance A/B and Discord pictures.

Primary API references: https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_graphics_pipeline_state_desc and https://learn.microsoft.com/en-us/windows/win32/api/d3dcompiler/nf-d3dcompiler-d3ddisassemble . Local sources: notes/renderer/12-capture-check.md and notes/render-architecture.md (7RT/depth and velocity observations).
