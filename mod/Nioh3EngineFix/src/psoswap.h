// psoswap.h - shader bytecode substitution at pipeline-state creation for Nioh3EngineFix.
// Design, inventory and proofs: notes\gpu-post-exe.md. Default OFF ([ShaderSwap] Enabled = 0).
//
// Why: about a third of Nioh 3's post/copy shaders are not in shader_dx12\*.file but embedded in Nioh3.exe (271 DXBC
// containers in .rdata: the PrepareThinSurfaceBSDF temporal filter, CustomDepthUpscale, the full-screen copies, the
// GPU particle / hair / ocean / fluid kernels, FidelityFX) and some PSOs are built from bytecode that exists nowhere on
// disk (bloom / compose in the r5 trace; probably built at run time - LogAll shows their module as "heap").
// A file swap cannot reach either; a PSO-creation hook can.
//
// What it does:
//   * hooks ID3D12Device::CreateGraphicsPipelineState / CreateComputePipelineState, ID3D12Device2::CreatePipelineState
//     (stream descriptors) and ID3D12Device1::CreatePipelineLibrary -> ID3D12PipelineLibrary(1)::LoadGraphicsPipeline /
//     LoadComputePipeline / LoadPipeline on the vtable of the device the game gets back from D3D12CreateDevice (the
//     game imports it from sl.interposer.dll; psoswap swaps that IAT slot before the engine creates its device).
//   * keys every VS/PS/GS/HS/DS/CS/AS/MS blob by its container digest (the 16 bytes after "DXBC"; for FXC shaders this
//     is what RGP shows), and a DXIL blob also by its HASH-part digest (what RGP and dxc -dumpbin show for DXIL); a
//     blob with an all-zero digest is keyed "f" + FNV-1a-64 of the whole container. A file may be named by either.
//   * when <Folder>\<key>.dxbc|.dxil|.cso|.bin exists (same shader stage; all stages of the PSO end up one container kind), the PSO is created
//     with that bytecode instead (a cached PSO blob in the desc is dropped). If the runtime rejects it, the PSO is
//     created again from the game's own desc, the key is disabled for the session and a log line says so.
//   * logs every shader whose bytecode lives inside Nioh3.exe once ("exe <stage> <key> <size> rva=..."); DumpMode = 1
//     writes those blobs to <Folder>\dump\, DumpMode = 2 writes every blob seen (any source, with its module in the log).
//   * DXIL and DXBC go through the same API under vkd3d-proton; nothing here is Windows-runtime specific.
//   * never changes a PSO it has no replacement for; every failure path calls the original with the original arguments.
//
// Integration (another agent owns dllmain/osd/menu; nothing there was edited):
//   1. build.bat: cl ... /Fobuild\psoswap.obj src\psoswap.cpp   (same flags as osd.cpp); link: add build\psoswap.obj.
//      CMakeLists: add src/psoswap.cpp.
//   2. dllmain Worker, EARLY: right after the Proton FG guard block and BEFORE the SteamStub decrypt wait (the import
//      table is the loader's, so the IAT slot is valid then, and the engine has not created its device yet):
//          { psoswap::StartInfo pi; pi.log = &Log; pi.iniPath = iniPath.c_str(); pi.wine = <wine or nullptr>;
//            psoswap::Start(pi); }      // returns at once; Enabled = 0 -> does nothing at all
//      (iniPath = SelfPathWithExt(L".ini") as for gpuopt; Start copies every string.)
//   3. osd InstallThread, next to InstallGpuOptSafe():  if (psoswap::GetState() != psoswap::State::Off) psoswap::InstallNow();
//      Only does something when the IAT path did not hook a device (Start called too late, unknown import layout): it
//      then hooks the game's device through a D3D12CreateDevice on the default adapter (= the game's singleton). PSOs
//      created before that point keep their original bytecode (logged as "late install").
//   4. stats thread (every 30 s, optional): psoswap::StatusLine(buf, n) -> Log("stats/s psoswap: %s", buf).
//   5. menu (optional, read-only row): StateName(GetState()) + StatusLine. There is no live switch: PSOs are built at boot.
//   6. Nioh3EngineFix.ini:
//          [ShaderSwap]
//          ; 1 = hook PSO creation, substitute <Folder>\<digest>.dxbc for matching shaders, log exe-embedded shaders
//          Enabled = 0
//          ; relative to the ini's folder
//          Folder = Nioh3EngineFix_shaders
//          ; 0 = off, 1 = write exe-embedded originals to <Folder>\dump, 2 = write every shader blob seen
//          DumpMode = 0
//          ; 1 = also log non-exe shaders once (module + key + stage); very long at boot
//          LogAll = 0
#pragma once
#include <cstddef>
#include <cstdint>

namespace psoswap {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;  // dllmain's Log
    const wchar_t* iniPath = nullptr;             // Nioh3EngineFix.ini ([ShaderSwap]); Folder is relative to its folder
    const char* wine = nullptr;                   // wine_get_version() or nullptr (only logged)
};

enum class State : uint8_t {
    Off = 0,   // [ShaderSwap] Enabled = 0: nothing installed
    Waiting,   // Start() ran, no device hooked yet (IAT hook armed, or waiting for InstallNow)
    Active,    // a device vtable is hooked
    Failed,    // neither the IAT hook nor InstallNow could hook a device (see log); the game is untouched
};

struct Stats {
    uint64_t graphics = 0;      // CreateGraphicsPipelineState calls seen
    uint64_t compute = 0;       // CreateComputePipelineState calls seen
    uint64_t stream = 0;        // CreatePipelineState (stream desc) calls seen
    uint64_t libLoads = 0;      // ID3D12PipelineLibrary Load* calls seen
    uint64_t shaders = 0;       // shader blobs inspected
    uint64_t exeUnique = 0;     // distinct shaders whose bytecode lives in Nioh3.exe
    uint64_t swappedPsos = 0;   // PSOs created with at least one replacement
    uint64_t swappedBlobs = 0;  // replacement blobs used
    uint64_t rejected = 0;      // replacement PSOs the runtime refused (re-created from the original desc)
    uint64_t skipped = 0;       // replacements not used (stage / container-kind mismatch, unreadable file, disabled key)
    uint64_t dumped = 0;        // blobs written by DumpMode
    uint32_t replacements = 0;  // files indexed in Folder at Start
    uint32_t vtables = 0;       // device vtables hooked
    bool late = false;          // hooked by InstallNow after the engine started rendering
};

// Reads [ShaderSwap]. Enabled = 0: returns, nothing else happens. Otherwise indexes <Folder> and swaps the main module's
// IAT slot(s) for D3D12CreateDevice (sl.interposer.dll / d3d12.dll) so the device the engine creates is hooked the moment
// it exists. Copies everything, never throws, never blocks.
void Start(const StartInfo& info);
// Late path (idempotent): when no device vtable is hooked yet, hooks the default adapter's D3D12 device (the game's
// singleton once it exists). Call from a thread that may create a D3D12 device, e.g. the osd install thread.
bool InstallNow();

State GetState();
const char* StateName(State s);
void GetStats(Stats& out);
// e.g. "on: 1834 PSOs (gfx 1612, cs 222, stream 0, lib 0), 1 swapped (1 blob), 0 rejected, 104 exe shaders"
const char* StatusLine(char* buf, size_t n);

}  // namespace psoswap
