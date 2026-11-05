# Selective VRS v3 ? task69697, thread69562

2026-11-05, main PC offline. Base ce702b92 / EF a3c9bf7f, v2. Requested branch cx-pc/vrs-selective-v3.

## Why v2 rejected everything
pc2 log5174 final: 0/13,297,664 calls; exact8RT 91,11,87,87,26,26,87,34 had 1,274,599 calls, rejectOR48. 0x40=NonGBuffer; count/format already match, leaving DSV not D32S8, depth disabled, cull NONE, or geometry shader. Existing logs cannot distinguish which; do not assert a specific culprit. 0x08=Shader was initialized by Describe(false) and never cleared because VrsObserve skipped disassembly after structural rejection. It is not evidence that the shader was inspected and found unsafe. Opus read-only review confirmed this control flow.

## Requested relaxed policy
Captured opaque surface signatures6RT [91,11,87,26,87,34],7RT [91,11,87,26,87,26,34],8RT [91,11,87,87,26,26,87,34] qualify independent of depth format/enable, culling, geometry-shader presence and shader disassembly. These exact signatures all appear without Blend/Samples/Binding/Unknown rejection in5174. Any blend/logic operation still excludes the draw; independent per-target blend is checked. Other emissive/particle/post/UI signatures remain fine; the prior normalized7RT verified-shader fallback is retained. Sample/alpha-to-coverage, missing pixel shader and unknown/mismatched bindings still fail fine for API correctness.

This deliberately supersedes v2 shader safety gating for those captured surface layouts. Their cutouts, shader side effects, or emissive/velocity values inside the same MRT are not separately excluded. Formats/blending cannot prove semantic pass identity. No guaranteed no-glitter claim. Do not add unobserved depth-write/function requirements that could again make coverage zero.

## Diagnostics
First200 intercepted draw calls in a single Present-to-Present recording interval, once each10s report interval. Render hooks copy scalar records only; watcher prints them after capture completes. This is a recording interval, not a GPU execution-frame assertion. Fields: frame/index, actual reject mask, RT count/formats, known binding/count, indirect flag, DSV, depth enable, cull, GS, blend mask, shaderPolicy. shaderPolicy0=unverified,1=verified,2=captured-layout policy. Metadata version2 prevents accepting old-layout private data.
LegacyShape bits retain old failed predicates: layout1 / DSV2 / depthOff4 / cullNone8 / GS16. Per-signature legacyShapeOR accumulates every draw so the culprit remains visible even if the8RT pass occurs after the first200 sampled calls. FirstDSV/Depth/Cull/GS are representative fields from that signature first observed, not exhaustive. Actual reject masks retain v2 mapping: unknown1, depth/noPS2, blend4, shader8, format16, motion32, shape64, samples128, binding256.

## Captured histogram replay
Fixture tests/vrs_capture_5174.tsv is the final27-row cumulative signature histogram from real pc2 log5174, not fabricated individual PSOs. Replay uses actual Describe/RejectDraw; checks36 combinations of uncaptured DSV/depth/cull/GS state per row rather than claiming measured values. Recorded aggregate Blend/Samples/Unknown/Binding rejections are retained conservatively.
Result: 5,406,166/13,297,664 calls admitted (40% integer), including1,274,599 eight-target calls;6RT2,808,226 and7RT1,323,341. This proves nonzero offline policy coverage on captured metadata; it is NOT a live tagged count, pixel coverage, GPU-time share, or predicted40% FPS gain. Captured data lacks individual PSO/descriptors/shaders. Request a new capture if v3 live tagging remains zero.

## Validation and lead retest
build.bat runs HUD patterns, real DXBC/DXIL helper fixtures, policy/state/capability tests, new200-sample cap/deferred-report tests, and captured histogram replay. Live settings remain restart per arm: OFF VrsCeiling0; ON selective VrsCeiling3 / VrsGBufferMotion1x1=0. Guard1 vetoes entire motion-writing MRT draws. Require nonzero live coverage, report OFF/ON whole FPS and GPU times, inspect both-arm images for glitter. Lead owns game/benchmarks/screenshots; this worker neither launches nor installs.
Prior pc2 RX6650XT,1080p FSR65 Optimized,stage7356/P2: OFF50fps20ms / selective-v2 ON49fps20ms, zero tagging. These are v2 results, not v3. Main-PC supplied installation snapshot EF a3c9bf7f / TR0e30328a / BR4c219306 / Cam7482c03d unchanged by this worker.

Final artifact: source3260c91f; EF SHA256 0ae82ceb7dc83b81266da4a5dcfcac3ce9979d83c1017b285d438ea61fdd862c, bridge5184; final build log5185. Final frozen-source build.bat exit0 and all4 executables (HUD/policy/state/capture) PASS. Binary verified to contain final legacyShapeOR and surface-layout diagnostic strings.
