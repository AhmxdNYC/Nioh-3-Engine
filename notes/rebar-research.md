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

## Real implementations (deep search)
Searched 2026-10-09; source files were fetched and grepped, not just summarised. Corrections to the earlier section are marked.

### 1. vkd3d-proton (https://github.com/HansKristian-Work/vkd3d-proton)
- Config: README documents `VKD3D_CONFIG=no_upload_hvv` ("Blocks any attempt to use host-visible VRAM (large/resizable BAR) for the UPLOAD heap"; frees VRAM at the cost of GPU perf). `upload_hvv` does NOT exist (correction: only the opt-out flag exists). Also `force_host_cached`.
- `libs/vkd3d/memory.c`, `vkd3d_select_memory_flags()` (~L732): `D3D12_HEAP_TYPE_UPLOAD` uses `device->memory_info.upload_heap_memory_properties` (field in `libs/vkd3d/vkd3d_private.h`, ~L4662), i.e. by default UPLOAD is automatically promoted to DEVICE_LOCAL|HOST_VISIBLE|HOST_COHERENT (ReBAR) memory. If the game ever allocates `D3D12_HEAP_TYPE_GPU_UPLOAD`, `has_used_gpu_upload_heap` is set and UPLOAD stops using ReBAR (log: "disabling automatic UPLOAD to ReBar promotion").
- `D3D12_HEAP_TYPE_GPU_UPLOAD` maps to DEVICE_LOCAL|HOST_VISIBLE|HOST_COHERENT; `GPUUploadHeapSupported` = `memory_info.has_gpu_upload_heap` (device.c ~L9533).
- Budgeting: `rebar_budget_mask`, `rebar_current`, `rebar_budget` in `vkd3d_allocate_device_memory()` (memory.c ~L905-1060); allocations over the fixed ReBAR budget fall back to sysmem. Same shape as our ring cap + passthrough fallback.
- There is NO "copy to device-local on submit" path. vkd3d-proton relies on host-visible VRAM; without a BAR, UPLOAD simply stays in sysmem. Nothing to copy from there.

### 2. DXVK (https://github.com/doitsujin/dxvk)
- `src/d3d11/d3d11_buffer.cpp`, `D3D11Buffer::GetMemoryFlags()` (~L318): DEFAULT + BIND_CONSTANT_BUFFER or CPU access => DEVICE_LOCAL|HOST_VISIBLE; DYNAMIC => HOST_VISIBLE|COHERENT plus DEVICE_LOCAL if any bind flags (so CBs are in VRAM when a BAR exists); STAGING => cached host. `d3d11.cachedDynamicResources` (flags v/i/c/r/a) strips DEVICE_LOCAL for apps that read back.
- `src/dxvk/dxvk_memory.cpp`, `allocateMemory()` (~L816): for DEVICE_LOCAL requests, only memory types from the first heap are considered "so we avoid falling back to HVV on systems without resizable BAR by accident". `pickChunkSize()` (~L1980) shrinks chunks for small BARs. `DxvkMemoryPool::MaxChunkSize` is the chunk cap.
- `dxvk.conf`: `d3d9.deviceLocalConstantBuffers` (Auto = discrete GPUs only) streams D3D9 constants into VRAM. This is the closest to our idea: CBs deliberately in VRAM.
- Wiki developer guidelines (https://github.com/doitsujin/dxvk/wiki/Developer-guidelines) say DXVK puts such resources in host-visible VRAM "especially with Resizable BAR". No upload-then-copy path for CBs; D3D11 Map(DISCARD) renames buffer slices in mapped memory. No ReBAR detection beyond memory-type flags.

### 3. Special K (https://github.com/SpecialKO/SpecialK)
- Fetched `src/render/d3d12/d3d12.cpp` and grepped for UPLOAD / GPU_UPLOAD / rebar / resizable: no hits. No ReBAR, UPLOAD-heap, or heap-placement feature found. It does not do this.

### 4. D3D12 interception / samples
- GPU upload heaps spec: https://github.com/microsoft/DirectX-Specs/blob/master/d3d/D3D12GPUUploadHeaps.md . Falls back to sysmem when over budget (WDDM requirement); budget via `QueryVideoMemoryInfo(LOCAL)`; CPU reads are very slow (WC pages).
- NVIDIA post (https://developer.nvidia.com/blog/optimizing-dx12-resource-uploads-to-the-gpu-using-cpu-visible-vram): Watch Dogs Legion on RTX 3080, one VB draw 0.2 ms -> <0.01 ms, L2 sectors 97.5% from VRAM, but whole frame only -0.9%. Diagnoses with Nsight GPU Trace "L2 misses to system memory" / BAR1 reads. No code; no sample repo.
- Ring-buffer references (not mirrors): Microsoft fence-based resource management (https://learn.microsoft.com/en-us/windows/win32/direct3d12/fence-based-resource-management); DirectXTK12 `GraphicsMemory` (https://github.com/Microsoft/DirectXTK12/wiki/GraphicsMemory), per-frame UPLOAD alloc with fence-tracked lifetime; GPUOpen Cauldron `DynamicBufferRing` (CBs/VB/IB in UPLOAD ring) and `UploadHeap` (texture staging then CopyBufferRegion to DEFAULT); GPUOpen HelloD3D12 (https://github.com/GPUOpen-LibrariesAndSDKs/HelloD3D12) uses UPLOAD -> DEFAULT, comments that it should run on the copy queue.
- No open-source D3D12 hook/ReShade add-on that mirrors UPLOAD CBVs into a VRAM ring on a copy queue was found. CbvMirror looks novel (confirms earlier note). GFXReconstruct PR #3227 (reusable UPLOAD heap for trim-load staging) is unrelated to placement.

### 5. Community fixes for "slow without ReBAR"
- Only firmware-level: ReBarUEFI (https://github.com/MrAnonymous7122/ReBarUEFI), a UEFI DXE driver that enables ReBAR on boards lacking it (needs flashing a modified BIOS; unrelated to our ASI but the only real fix users have). No game-side wrapper/ASI found.

### Techniques worth copying
1. vkd3d-proton ReBAR budget (`rebar_budget` / `rebar_current`): hard cap on promoted bytes, fallback to sysmem when exceeded. Matches our cap + passthrough.
2. vkd3d "has used GPU_UPLOAD" flag: if the game itself allocates GPU_UPLOAD heaps (or OPTIONS16 says supported), auto-disable the mirror.
3. DXVK `d3d9.deviceLocalConstantBuffers = Auto`: enable only on discrete GPUs; same default policy as ours (AMD discrete, no ReBAR).
4. NVIDIA: choose targets by Nsight "L2 misses to sysmem" per draw; do not mirror everything (whole-frame gain on their case was <1%).
