// upmirror.h - CM: per-draw constant buffers mirrored from the system-memory UPLOAD ring into VRAM ([GpuOpt] CbvMirror,
// default 0). notes\root-cause-vram-streaming.md section 7.
//
// Why: the engine creates every per-draw CBV (recorder site 118F89, ~2,400 per frame) on its 700 MB UPLOAD ring heap. With
// Resizable BAR / SAM the driver backs that heap with VRAM; without it (pc2, most mid-range PCs) it is system memory and every
// draw / dispatch fetches its constants over PCIe - latency per wave, which is what makes small draws and the tiny meshlet
// front-end kernels 10-100x slower on pc2 than on the RX 9070.
// What: the CBV hook (memstats.cpp, device vtable slot 17) hands each recorder-site CBV to RedirectCbv: the bytes get a slot
// in a DEFAULT-heap mirror ring (VRAM), the descriptor is created on the mirror address, and the copy is queued. Before the
// game's next ExecuteCommandLists on a DIRECT / COMPUTE queue (osd.cpp hook) one command list on our own COPY queue copies
// every queued range UPLOAD -> mirror (CopyBufferRegion, merged ranges) and signals a fence; the game queue Waits on it
// before its lists (DMA queue: the copy never queues behind graphics work). Mirror space is retired 80 ms after a later
// copy completed. The mirror is a COMMON-state buffer: copies promote it to COPY_DEST, it decays at the end of our
// ExecuteCommandLists, the game's lists promote it to a constant-buffer read (buffer implicit promotion rules).
// Assumption (A1): a recorder CBV's bytes are final when its descriptor is created (draw prep writes the constants on the
// main thread before the dispatcher records). The picture check (shotdiff) is the proof; CbvMirror = 2 runs the redirect
// on any PC (also with ReBAR, where it gains nothing) so the check can be done anywhere.
// Modes: 0 off (default, nothing installed); 1 auto = on only when the UPLOAD heaps really live in system memory (DXGI
// non-local usage >= 75 % of the UPLOAD heap bytes), the adapter is not UMA and the process is not under Wine/vkd3d;
// 2 force (test). Any failure -> the CBV is created unchanged (fallback), never a broken descriptor.
#pragma once
#include <cstdint>

struct ID3D12Device;
struct ID3D12Heap;
struct ID3D12CommandQueue;
struct ID3D12CommandList;
struct D3D12_HEAP_DESC;
struct D3D12_CONSTANT_BUFFER_VIEW_DESC;

namespace upmirror {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;  // [GpuOpt] CbvMirror (0/1/2), CbvMirrorMB (8..512, default 128), CbvMirrorAllSites (0/1)
    uintptr_t base = 0;                // Nioh3.exe image base (caller-site filter)
};
bool Start(const StartInfo& info);
bool Wanted();             // CbvMirror != 0: memstats installs the device hooks, osd the ExecuteCommandLists hook
bool WantsPresentHook();   // = Wanted() (osd installs its queue/present hooks)

// memstats.cpp device hooks
void OnHeap(ID3D12Device* dev, ID3D12Heap* heap, const D3D12_HEAP_DESC& desc);
void OnHeapType(int type);   // every heap / committed resource: GPU_UPLOAD (5) seen -> CM off
// true: *out holds the redirected desc (create the CBV with it); false: create the original
bool RedirectCbv(ID3D12Device* dev, const D3D12_CONSTANT_BUFFER_VIEW_DESC* in, D3D12_CONSTANT_BUFFER_VIEW_DESC* out, void* retAddr);
// memstats Tick1s (every second): auto-mode decision inputs
void SetAuto(double nonLocalUsedMB, bool uma, bool gpuUploadHeap);   // gpuUploadHeap = OPTIONS16 ReBAR flag

// osd.cpp HkExecuteCommandLists, game callers, before the original
using EclFn = void(__stdcall*)(ID3D12CommandQueue*, unsigned, ID3D12CommandList* const*);
bool OnEcl(ID3D12CommandQueue* q, unsigned n, ID3D12CommandList* const* lists, EclFn orig);   // true = it submitted the game's lists

void LogLine();   // stats thread, every 30 s
}  // namespace upmirror
