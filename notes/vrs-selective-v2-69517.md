# Selective VRS v2 ? task #69520 / result thread #69517

Device: main PC, offline development only. Base cx-pc/vrs-selective 9388b10b (v1 EF1ea659f0). No game launched or installed.

## Evidence
pc2 #260: RX6650XT, 1080p FSR65 Optimized, stage7356/P2. OFF50fps / all-draw2x2 59fps with glitter / selective49fps with zero tagged of148M calls. These are lead-provided v1 measurements, not v2 performance. File5156 tuple: 91,11,87,87,26,26,87,34, count8, rejectOR78. Reasons were count, missing BGRA/HDR formats, motion guard and blanket DXIL rejection. Opus CLI review independently confirmed these four gates.

## Changes
- Exact ordered 8RT tuple admitted alongside the old normalized7RT shape. Format exceptions are scoped to that exact tuple; arbitrary HDR, post and single-target shaders still fail. Depth D32S8/enabled, culling, no GS, no blending/logic, single sample and shader safety gates remain.
- DXIL now disassembled at eligible PSO creation using optional dxcompiler.dll beside the game EXE, with checks cached by shader digest. No shader replacement. Fails fine if DLL/COM/disassembly unavailable. Excluded shapes are not disassembled. DXBC continues using D3DCompiler47.
- DXC output must identify pixel SM6 and non-sample-frequency execution. Discard, UAV, atomic/store, depth/coverage/stencil outputs, sample-index/coverage inputs and evaluate-at-sample operations reject. The helper uses the documented IDxcCompiler3/IDxcOperationResult ABI, not a bitcode substring search. Reference: https://raw.githubusercontent.com/microsoft/DirectXShaderCompiler/main/include/dxc/dxcapi.h
- Motion guard defaults0. Explicit VrsGBufferMotion1x1=1 still rejects the ENTIRE motion-writing draw. It cannot preserve only RG16F while shading other outputs coarsely. Set it explicitly to0 for arm C; an existing v1 INI with1 otherwise still yields zero on the target pass.
- Every10s aggregate tagged_calls/total_calls and each signature tagged2x2/total/fine1x1 are cumulative API invocation counts. Aggregate survives signature-table overflow. Resets are excluded; indirect calls are not expanded into actual draws.

## Retest settings
Restart per arm:
A: VrsCeiling=0
B: VrsCeiling=1, VrsGBufferMotion1x1=0 (prior glitter ceiling)
C: VrsCeiling=3, VrsGBufferMotion1x1=0
Optional diagnostic D: mode3/guard1 should suppress the full captured8RT pass.
Confirm the exact8RT signature has nonzero tagged2x2 before judging speed. Lead owns pc/pc2/ally benchmarks and images.

## Limits
No v2 live speed/visual claim. Pass-format classification cannot prove all semantic resource roles: emissive data written inside the same opaque MRT draw also shares its rate, whereas separate blended/emissive/post/UI shapes remain1x1. Motion/TAA shimmer may persist; actual no-glitter and near-ceiling gain need live gates. Missing game DXC on a device produces safe zero eligibility, not successful optimization. DXIL disassembly is cached once per eligible shader but adds creation-time work.

## Offline validation
Actual D3DCompile opaque/clip/depth fixtures; actual game-folder DXC compile+disassemble SM6 opaque accepted, clip/depth/sample/UAV rejected, malformed bitcode rejected. Exact captured8RT accepted with guard0 and rejected with guard1; every single format mutation, every blended output and unknown shader excluded. Actual command-list hooks exercise captured8RT2x2 -> blend1x1, missing binding, motion guard, bundles, Close/ClearState, failed Reset and capability fallback; tagged/total counts checked. Full build includes both VRS executables and existing HUD tests.
