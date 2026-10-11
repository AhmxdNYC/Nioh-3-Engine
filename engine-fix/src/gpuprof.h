// gpuprof.h - GP: in-process GPU pass profiler of Nioh3EngineFix (1.7.0). Replaces RGP where it cannot run (the Ally:
// RADV SQTT froze the device). [Profiler] GPUProf = 0 by default.
//
// 1. Markers: the engine has its own debug markers (pass names: "Cascade %d", "Tile %d %d", "MeshletPreculling", every
//    render-graph node name), switched off at boot by two bytes: record g+2A62h (KtglContext, [Nioh3+45BA588]) and
//    replay g+552h (SynCtx+542h). With both set, the dispatcher thread calls ID3D12GraphicsCommandList::BeginEvent
//    (vtbl+1C8h, UTF-16 name, colour as Metadata) / EndEvent (vtbl+1D0h) on the list it records (SynBeginEvent 113E64,
//    SynTokEndEvent 113FC8, 113F70), unless the engine's optional marker interface [4BD09C0] is set. See
//    notes\shadows-culling.md 4 and patcher\gpu_markers.fsx. GP writes the two bytes (replay first) once g exists.
// 2. Timing (GPUProf = 1): gpuopt (gpuopt.cpp) hooks every command-list vtable slot; GP takes BeginEvent, EndEvent,
//    Close and Reset. Each Begin/End records EndQuery(TIMESTAMP) into a query heap split into 8 frame slots; the
//    queries of a list are resolved into a readback buffer in chunks (when a chunk fills, and at Close), always after
//    they were written on that same list. A frame slot is read on the Present thread 6 frames later (no stall), then
//    cleared. Names are aggregated nesting-aware (inclusive and self time) per 30 s window.
// 3. GPUProf = 2 (1.7.4, GP2): also bills the GPU time between consecutive timestamps of a list to the PSO bound (keyed by its
//    shader hashes via psoswap::PsoSetId) or to "copy / clear", with the enclosing marker. Stamps at the first hooked call, on
//    bucket change (first draw / dispatch after SetPipelineState, copy, clear), at marker begin / end and before Close. Intervals
//    are GPU-pipelined (approximate per-PSO cost). Keys: GPUProf2_MaxStamps / _Top / _AB. See README.md "per-PSO mode".
// GPUProf_Markers = 1 (GPUProf = 0): markers only, no queries (for PIX on the PC).
// Any failure: logged, GP switches itself off (hooks pass through), rendering is never touched beyond the added
// EndQuery / ResolveQueryData commands.
#pragma once
#include <cstddef>
#include <cstdint>

namespace gp {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;
    uint8_t* base = nullptr;   // Nioh3.exe
    bool codeOk = false;       // dllmain verified the marker code (GP check sites + deps + token table) on v2.02
    const char* wine = nullptr;
};

// Reads [Profiler] GPUProf / GPUProf_Markers / GPUProf_MaxMarkers / GPUProf_Top. Starts the marker-enable thread when
// either is on (and codeOk). Returns at once.
void Start(const StartInfo& info);
bool WantsHooks();           // GPUProf = 1 or 2 (and codeOk): gpuopt must install its command-list hooks
// gpuopt, after its vtables are patched: slots to send to OnListCall (Close 9, Reset 10, BeginEvent 57, EndEvent 58)
void EnableHooks(volatile uint8_t* hookMask, int slots);
// gpuopt slow path (asm thunk -> GpuOptSlow) for the hooked slots, BEFORE the original method runs
void OnListCall(void* list, int set, int slot, uintptr_t a1, uintptr_t a2, uintptr_t a3);
void OnFrame();              // osd Present hook, once per frame of the drawing swapchain
void LogLine();              // stats thread, every 30 s (no-op while SetExternalWindows(true))
// 1.7.36 Tour (tour.cpp worker thread): the windows follow the tour's dwells. LogWindow(nullptr) discards what was gathered
// (dwell start), LogWindow("tour p/s/n") logs "GP: window tour p/s/n" + the usual lines for the time since (dwell end).
void SetExternalWindows(bool on);
void LogWindow(const char* tag);
// menu: "GP: <frame ms> | top 5 ..." (thread-safe; empty when GP is off)
const char* StatusLine(char* buf, size_t n);
const char* MarkerName(uint16_t id);   // 1.7.48 VS: name of a marker id from the command-list hooks ("-" = none)
bool Active();

// 1.7.40 GQ ([Profiler] GQ = 1, needs GPUProf = 2): graphics-queue gap census. Per DIRECT list Reset in a sampled frame:
// a timestamp before its first GPU command (head) and one at Close (end); per sampled frame the idle time between
// consecutive lists (sorted by GPU time), the list heads (start -> first GP2 stamp) and the top gaps with the lists'
// markers and ECL call sites. Log lines "GQ: ..." after the GP2 block (notes\gq-jt-probes.md). Cost-free when off: the
// two calls below test one global first.
extern volatile long g_gqHeads;   // != 0: GQ on and this frame is sampled (gpuopt's ResourceBarrier hook tests it)
void OnListHead(void* list, int set);   // gpuopt HkResourceBarrier, before anything is recorded or deferred
// osd HkExecuteCommandLists, for every caller, before the original: ECL order + call site of the GQ lists
void OnEcl(void* queue, unsigned n, void* const* lists, void* retAddr, bool gameCaller);

// 1.7.47 MLP ([Profiler] MeshletProbe = 0, needs GPUProf = 2; notes\meshlet-frontend.md): meshlet front-end probe. Inside the meshlet
// markers (MeshletResolveLOD, MeshletPreculling, MeshletInstanceCulling, MeshletCheckRequestLoad, MeshletRequestLoad, GenerateSceneHiZ,
// GenerateShadowMapHiZ) of a sampled frame: a timestamp before every Dispatch / ExecuteIndirect and before + after every ResourceBarrier
// call -> per region dispatch / barrier / gap time and per-dispatch rows; every other sampled frame CS invocations per dispatch from
// pipeline-statistics queries instead (MeshletProbe_Stats = 1). Regions carry the recorder caller from mlprobe.cpp (the CPU half).
// Log lines "GP: MLP: ..." after the GP2 block; MeshletProbe_Dump = N writes the raw events of N sampled frames to
// Nioh3EngineFix_mlp.csv next to the ini. MpcCheck (mpccheck.cpp) uses the same region tracking.
extern volatile long g_mlpBarriers;   // != 0: MLP or MpcCheck on (gpuopt's pass-through ResourceBarrier path calls OnBarrier)
void OnBarrier(void* list, int set, unsigned n, const void* barriers, bool post, void* ra = nullptr);

// FC ([Profiler] FrameCap = 0; fcap.h, notes\renderer\00-map.md section 4): one-frame D3D12 command-stream capture. Needs GPUProf = 0,
// [GpuOpt] Barriers = 0 and the OSD. g_fcCap != 0 only inside a capture window: gpuopt then sends every armed list slot to FcListCall
// (with the thunk's saved registers, for the caller RVA and stack arguments) and its pass-through barriers to OnBarrier; osd sends
// ExecuteCommandLists to OnEcl while g_fcEcl != 0 (the first call also installs FC's queue Signal / Wait / UpdateTileMappings hooks).
extern volatile long g_fcCap;
extern volatile long g_fcEcl;
void FcListCall(void* list, int set, int slot, uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t* regs);

}  // namespace gp
