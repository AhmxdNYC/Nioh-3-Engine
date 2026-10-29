// scenemirror.h - SBM: meshlet scene-buffer mirror for GPUs WITHOUT Resizable BAR (1.7.57; notes\rebar-fix-v3.md).
// Default OFF ([GpuOpt] SceneBufMirror = 0). Diagnostic census: [Profiler] SceneBufProbe = 1.
//
// What the GPU reads from system memory without ReBAR, besides the per-draw constants (CbvMirror, notes\root-cause-vram-
// streaming.md): the meshlet SCENE BUFFERS. MeshletFrameBegin 5D214 swaps MeshletSys+16A0B0 between two sets of buffer
// objects (MeshletSys+169FB0 / +16A030, 80h each; slots +18h..+78h) and MeshletUpdateSceneBuffers 4BBEC maps every slot
// and writes this frame's data once: +18h instance matrices (48 B each), +28h / +40h per-object offsets, +30h part entries
// (16 B), +50h (12 B), +58h the MRLOD root list (8 B per root = MshLod#1's tDispatcherMRLOD t20), +20h / +38h prefix
// tables, +70h a counter, +78h a READBACK result. Buffer objects whose usage index (+68h, table 3BEA260) is 2 are UPLOAD
// resources the CPU maps directly (vtbl+40h Map, offset +80h); the GPU kernels (MIC, MshLod#1/#2, MPC) and the meshlet
// draws then read them over PCIe. pc2 #1070: MshLod#1 = 53,166 threads x 8 B = 425 KB per call in 602 us = 0.71 GB/s.
//
// The mirror: one hook on the call 5D417 (MeshletFrameBegin -> MeshletUpdateSceneBuffers; patches.json SBM1). After the CPU
// wrote the current set, every UPLOAD slot's written range [+80h, +80h + +A8h) is copied into a DEFAULT-heap twin of its
// resource (same offsets) by ONE command list per frame on our own COPY queue (DMA engine; SceneBufMirrorQueue = 0: on the
// game's DIRECT queue instead), one fence signal. Every game DIRECT / COMPUTE queue Waits on that fence once, at its next
// ExecuteCommandLists (osd.cpp queue hook). Buffer SRVs the engine created on those resources whose byte range lies inside
// a scene buffer are re-created on the twin (once, in the frame their set is copied; new ones at creation) - every other view
// keeps reading the original resource, which still holds the same bytes, so anything not redirected stays correct.
// Synchronisation is inherited from the engine: it rewrites a set only after the GPU finished the frame that read it, so
// the twin regions of that set are free at the same moment. Any failure -> every redirected SRV is put back on the original.
//
// Keys: [GpuOpt] SceneBufMirror 0 off (default, nothing installed) / 1 auto (only when rebar.cpp found the UPLOAD heaps in
// system memory: discrete GPU, no ReBAR; never UMA / Wine) / 2 force (test, e.g. with [Profiler] VSForceSysUpload = 2);
// [GpuOpt] SceneBufMirrorQueue 1 own COPY queue (default) / 0 game DIRECT queue (CP DMA); [Profiler] SceneBufProbe 1 = census
// only (which slots are UPLOAD, bytes per frame, SRVs found) + a late-CPU-write check when the mirror runs. Log: 'SBM:'.
#pragma once
#include <cstdint>

struct ID3D12Device;
struct ID3D12Resource;
struct ID3D12CommandQueue;
struct D3D12_SHADER_RESOURCE_VIEW_DESC;
struct D3D12_CPU_DESCRIPTOR_HANDLE;

namespace scenemirror {

// BootIat (memstats.cpp): reads the keys so Wanted() is valid before the device exists
void Configure(const wchar_t* iniPath, void (*log)(const char*, ...));
bool Wanted();   // SceneBufMirror != 0 or SceneBufProbe != 0: memstats installs the device hooks, osd the queue hook

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;
    uintptr_t base = 0;
    bool knownBuild = false;
};
// after decryption: verifies and retargets the 5D417 call (v2.02 only). Returns true when installed.
bool Start(const StartInfo& info);

// memstats.cpp HkSrv, BEFORE the original CreateShaderResourceView: records buffer SRVs on UPLOAD-like resources;
// *use = the resource to create the view on (the twin when this view is redirected), = res otherwise
using SrvFn = void(__stdcall*)(ID3D12Device*, ID3D12Resource*, const D3D12_SHADER_RESOURCE_VIEW_DESC*, D3D12_CPU_DESCRIPTOR_HANDLE);
void OnSrv(ID3D12Device* dev, ID3D12Resource* res, const D3D12_SHADER_RESOURCE_VIEW_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE h,
           ID3D12Resource** use);
void SetSrvOriginal(SrvFn fn);   // the unhooked CreateShaderResourceView (rewrites must not recurse into OnSrv)
// memstats.cpp: system-memory test for placed resources (VS heap ranges)
void SetSysVaTest(bool (*fn)(uint64_t va));

// osd.cpp ExecuteCommandLists hook, game submissions only, before the original call
void OnEcl(ID3D12CommandQueue* q);

void LogLine();   // stats thread, every 30 s

// harness only (tests\sbm_harness.cpp): set the keys without an ini / call site, and run the post-update step
void TestSetup(void (*log)(const char*, ...), int mode, int queueMode, int probe);
void TestAfterUpdate(uint64_t meshletSys);

}  // namespace scenemirror
