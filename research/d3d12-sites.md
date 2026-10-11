# D3D12/DXGI call-site census (goal part 3, everything except barrier callers)

Static only (dump v2.02). Barriers stay in `research/vkd3d-audit.md` Addendum B / `re\scripts_d3d12\barrier_sites.tsv`
(the 24 ResourceBarrier sites appear in the census for completeness, owned there).

## Result

**604 vtable call sites in 272 functions**: 192 newly named, 80 were already named (they get a D3D12 note).
Census: `re\scripts_d3d12\d3d12_sites.tsv` (site, API, slot, function + name, frequency, subsystem, evidence, this-key).
Symbols: `re\symbols\incoming\d3d12-sites.tsv` (272 rows, not merged); `d3d12-sites-renames.tsv` is empty: the 80 named
enclosing functions agree with their calls.

By interface: Device 195, CL 135, Resource 81, SwapChain 46, Queue 44, Fence 42, IUnknown/Object helpers 34,
DescriptorHeap 13, Factory 7, Adapter 4, Heap/Output/RootSig 1 each.

| API group | sites | frequency |
|---|---|---|
| resources / heaps / memory (Create*Resource, CreateHeap, GetResourceAllocationInfo, GetCopyableFootprints, GetResourceTiling, Map/Unmap, GetGPUVA, MakeResident/Evict, tile mappings) | 125 | streaming 119, per-frame 6 |
| COM lifetime (QI/AddRef/Release/GetDevice/GetParent/names) | 92 | lifetime |
| fences / queue sync (Signal, Wait, GetCompletedValue, SetEventOnCompletion, CreateFence) | 81 | per-frame 73, init 8 |
| draw / dispatch / bind (CL) | 73 | per-draw 68 |
| descriptors / views (CopyDescriptorsSimple, Create*View, CreateSampler, heaps) | 69 | init 30, streaming 25, per-draw 14 |
| swapchain / DXGI (Present, back buffers, desc, factory/adapter/output) | 47 | per-frame 27, init 20 |
| copy / clear / resolve / ExecuteIndirect (CL) | 25 | per-frame 18, per-draw 7 |
| command queue / allocator / list creation + ExecuteCommandLists | 22 | init 20, per-frame 2 (ECL thread, FI presenter) |
| PSO / root signature / command signature | 14 | init 8, on-demand 4 (PSO cache miss) |
| queries / markers / predication | 12 | per-frame 7 |
| device misc (CheckFeatureSupport, GetAdapterLuid, GetDeviceRemovedReason, increment sizes) | 20 | init |

| subsystem | sites | funcs | what |
|---|---|---|---|
| GpuMemory | 192 | 86 | resource/heap allocation, tiled sizing, upload footprints + map/unmap, fence-gated reuse (62A-631, 2EE1-2EE7, 2EF9-2F0A, 2F10-2F14, 1895) |
| SynRecord | 117 | 73 | dispatcher-thread token handlers: all draws/dispatches/binds/copies/clears on SynCtx+580 |
| FfxFrameInterp | 105 | 28 | FSR3 frame-generation swapchain proxy (377xxxx): own queues +B48/+B58, 5 fences, presenter Signal/Wait/Present, composition pass 3780A9C |
| DeviceInit | 71 | 26 | device, queues/lists, SynCtx signatures + query heap, descriptor heaps, adapter memory, swapchain (re)create |
| CmdListRing | 38 | 14 | list begin (heaps, root sigs, compute tables 4951C8-495D9C), queries |
| Fsr3Swapchain / FfxDx12Backend | 38 | 13 | FSR3 integration DXGI side (342-346xxxx), FFX dx12 backend helpers (370F-3718) |
| Present / FenceSync / EclSubmit | 22 | 18 | Present thread, back-buffer index, fence polling, ECL replay (Signal/ECL/Wait tail calls 4F91C0/4F9214/4F93C8) |
| Views / Pso / misc | 21 | 15 | DSV/SRV/UAV/CBV/sampler thunks, PSO create (9290D8, 58C2B0, 2EC8BF4), device-lost checks |

**Per-frame vs init:** 261 per-frame/per-draw sites (CL 131, fences 42, queues 41, swapchain 27, device descriptor ops 15),
144 streaming (resource/upload work driven by streaming), 103 init, 4 on-demand PSO, 92 lifetime.
Per-frame cost on the dispatcher stays where vkd3d-audit put it (SynFlush*/CBV/CopyDescriptorsSimple); the new finds are
the FI presenter's per-present queue work (3777098/37773E4: GetBuffer + 3 Signal + Present + Wait per present) and the
fence-gated upload/alloc paths (GetCompletedValue in 33 sites, mostly GpuMemory).

## Method (re\scripts_d3d12)

1. `d3d12_vsites.fsx` (Iced, whole .pdata): every `call/jmp [reg+slot]` whose reg was loaded as `[this]`, with a symbolic
   backward slice of `this` and of rcx..r9/[rsp+20..40] (`G:X`, `K>D` field loads, `&S:D` locals, `arg:rcx`, `ret:F`),
   plus direct calls, `&local` struct arguments and register stores. 93.6 k virtual calls.
2. `d3d12_iids.py` finds the 89 D3D12/DXGI IIDs in .rdata (`iids.tsv`). `d3d12_sites.py` types call sites by IID-in-riid
   (slot + argument position + IID), seeds (GfxDevice +4268 device/+4270 queue/+4278,+7730 swapchain; SynCtx +580 CL,
   +8E0/+8E8 device), out-param holders followed through callers' structs, typed stores, caller-typed parameters, then
   method name from the slot.
3. Guards against field-offset reuse (found the hard way: suffix typing through stores made 5 000 false CL sites):
   stores only type within the storing function; learned field holders apply within 0x40000 of their origin unless
   >= 0x1000; only g_RenderGlobals chains take field suffixes (G:45C84A8 has its own +580/+188); holder hits on generic
   slots (< 0x40) are dropped; parameter typing needs >= half the callers typed. A slot-pattern "profile" classifier
   was tried and dropped (generated material functions call [this+128] slots 60/130/140 = an engine interface, not CL).
4. `REVIEWED`/`SCOPED` in d3d12_sites.py: 96 + 22 sites typed by hand from slot + argument shape (view-creation thunks
   2EE7C30/2F03FA8/2F03FD0 with a `{device, resource, desc}` struct, GpuBufferUnmap +10/+30, texture upload family
   2EF9C40..2F0A010 `[this+10]` resource, ECL token handlers, fence pollers, FI queues +B40/+B50).
5. `d3d12_census.py`: subsystem by address range, frequency by method/subsystem, names = subsystem prefix + role
   (`OVERRIDE` for 11 hand-picked ones), unique against HEAD symbols.

Cross-check: all 51 CL sites of the earlier token-handler scan (`<re-scratch>\d3d12\vcalls.tsv`) are in the census.

## Gaps (what "every" does not yet cover)

- `this` built from the token stream or untracked registers (`?` keys): only the ECL handlers and SynBeginList were
  reviewed; 13.4 k `?`-keyed virtual calls remain unclassified binary-wide (nearly all engine C++, not D3D12).
- Resource methods on small wrapper fields (`[obj+10]`, `[obj+30]`) outside the reviewed functions: Map/Unmap/
  GetGPUVirtualAddress coverage is partial (8 Map, 14 Unmap, 6 GetGPUVA found).
- ~480 untyped virtual calls remain inside the 272 D3D12 functions; spot check: engine allocators (+3D90/+3D98, slots
  28/30/38/58) and Release on locals, not D3D12.
- Calls made inside sl.interposer / amd_fidelityfx / OptiScaler DLLs are out of scope (not in Nioh3.exe).
- Frequency is by method + subsystem, not measured; the Ally SP/GP2 logs can confirm per-frame counts per site.

Rebuild: `dotnet fsi d3d12_vsites.fsx > vsites.tsv; py -I d3d12_sites.py vsites.tsv > c.tsv; py -I d3d12_census.py c.tsv`.
