# ReBAR / UPLOAD-heap research for CbvMirror (2026-10-09)

Web-search based; no vendor perf guide page was retrievable verbatim, so vendor statements
below are from GPUOpen/Microsoft pages and are flagged where inferred.

## Findings
1. GPUOpen: a shader reading an UPLOAD-heap buffer "requires reaching out to system RAM through PCIe" and can run slower; the standard pattern is UPLOAD staging -> CopyResource/CopyBufferRegion -> DEFAULT (VRAM) buffer. https://gpuopen.com/learn/using-d3d12-heap-type-gpu-upload/
2. Without ReBAR only ~256 MB of VRAM is CPU-visible; ReBAR exposes all of it. D3D12_HEAP_TYPE_GPU_UPLOAD (Agility SDK 1.710/1.711+, needs ReBAR + new OS/driver) is the official API for CPU-writable VRAM. Check `D3D12_FEATURE_DATA_D3D12_OPTIONS16::GPUUploadHeapSupported`. Spec: falls back to system memory if VRAM is short. https://github.com/microsoft/DirectX-Specs/blob/master/d3d/D3D12GPUUploadHeaps.md , https://devblogs.microsoft.com/directx/
3. Measured SAM gains (scene/CPU dependent, shrink at high res): Borderlands 3 +16% 1080p / +14% 1440p / +11% 4K (TechSpot RX 6800) https://www.techspot.com/article/2178-amd-smart-access-memory ; FH5 +13.7% QHD (Hexus) to +22-26% (ComputerBase forum single-user) https://www.computerbase.de/2020-12/amd-smart-access-memory-test-radeon-rx-6800/ ; AC Valhalla ~+13% avg, +9-10% lows. These are whole-frame averages. Our per-pass 9-100x slowdowns are much larger than these, so ReBAR-vs-not is not directly comparable.
4. Tools: vkd3d-proton places UPLOAD heaps in host-visible VRAM (ReBAR) by default and exposes `VKD3D_CONFIG=no_upload_hvv` to disable it (costs GPU perf) - i.e. the reference D3D12 translation layer itself treats "UPLOAD in VRAM" as the fast path. https://github.com/HansKristian-Work/vkd3d-proton . NVIDIA enables rBAR per-game via driver profile; Profile Inspector can force it (needs platform ReBAR anyway, so useless for pc2). No open-source "mirror UPLOAD CBV to VRAM" implementation was found; CbvMirror appears novel. (Special K / DXVK specifics not confirmed.)
5. Nioh 3: no ReBAR on/off benchmark found. Guides say enable ReBAR and describe the game as CPU-bound on low/mid PCs, GI is the heavy GPU setting (DSOGaming, wccftech, pcoptimizedsettings). Our own measurement (pc2 vs 9070) is the only data.

## Pitfalls
- Copy must run after the CPU's last write and before GPU use. Mirroring at ExecuteCommandLists time is right only if the game never rewrites a ring slice that an in-flight list still reads (per-frame ring); if it reuses slices within a frame, mirror content is stale/aliased.
- Copy-queue sync: one copy submission + one fence signal per frame (batch all ranges, coalesce adjacent ones), and make direct/compute queues Wait once. Fence per ECL multiplies cross-queue waits; copy queue to direct wait can add latency/bubbles.
- Copy queue reads UPLOAD over PCIe too (x8 on pc2) - that is the point, but it is a bulk linear read, far better than random per-draw shader reads. Dirty-range tracking beats copying whole heaps.
- 256 B alignment: keep mirror offsets identical to source offsets (or remap GPU VAs), CBV size multiples of 256.
- Root-descriptor CBVs (VA in command list) vs descriptor-table CBVs (view in heap): both need rewriting of VA / descriptor to point at the mirror; root CBVs recorded in command lists need patching or a VA-preserving scheme.
- Multi-queue ordering: compute/copy queues of the game that read the same CBVs also need the wait.
- Budget: ring size counts against VRAM (QueryVideoMemoryInfo); cap and fall back to passthrough.
- ReBAR machines: UPLOAD may already be in VRAM on some drivers; detect and disable mirror (GPUUploadHeapSupported, or measure BAR size via DXGI/adapter, or bail if mirror measured slower). NVIDIA/Intel drivers place UPLOAD differently; ship as AMD-no-ReBAR only by default.

## Recommendations for CbvMirror
1. Single fence per frame (at Present or first ECL of frame), one batched copy list with coalesced dirty ranges.
2. Detect ReBAR: OPTIONS16 GPUUploadHeapSupported plus adapter BAR size/ dedicated-visible check; auto-disable if true.
3. Extend to the ~23% vertex buffers in UPLOAD, preferably as static one-time promotion (copy once, cache by resource + generation) rather than per frame.
4. Prefer promoting whole UPLOAD resources that are only written once (hash-check) to DEFAULT.
5. Cap ring by VRAM budget, with passthrough fallback and a stats counter (bytes/frame, wait time).
6. Validate against a ReBAR-off vs ReBAR-on A/B on pc2 if the BIOS allows, which is the true upper bound.

## Calibrated expected gain
- Generic ReBAR/SAM whole-frame gain: 5-25% typical, up to ~25% outliers.
- Nioh 3 has per-pass 9-100x gaps, so the upper bound is the gap to a ReBAR-on card of equal class; realistic recovery of a mirror (extra copy cost + sync) is roughly 50-80% of the ReBAR benefit: expect ~10-25% frame time on GPU-bound scenes, little in CPU-bound ones. Treat >30% as unlikely.
