// gpuopt.h - GPU submission optimisation for Nioh3EngineFix: D3D12 barrier batching on the game's command lists.
// Design, measurements and proofs: notes\gpu-barriers.md. Default OFF ([GpuOpt] Barriers = 0).
//
// What it does (B1 in notes\d3d12-submission.md):
//   KTGL's dispatcher thread ("SynergisticDispatcherTask") records every resource transition / UAV barrier as its own
//   ResourceBarrier(1, ...) call (sites 115E97, 118443, 118519, 118597, 1139FE, 113A52, 1146D0, 114AF4, 7F5365,
//   7F9D27/64/9F/D6). On AMD's Windows driver every call is its own CP sync + cache flush (RGP r5: 1360 barriers, 478
//   back-to-back chains per frame). gpuopt defers those barriers per command list and emits them as ONE
//   ResourceBarrier(n, ...) right before the next command that can observe them (draw, dispatch, copy, clear, resolve,
//   query, ExecuteIndirect, Close, OMSetRenderTargets, ... - see kSlotInfo in gpuopt.cpp). Order is preserved; the
//   D3D12 spec defines an n-element call as n sequential calls, so the GPU sees exactly the same dependencies.
//   Pure state setters (PSO, root signature/arguments, IA/RS state, descriptor heaps) do not flush, so the texture-bind
//   transitions (115B9C) and the render-target transitions (1180D0) of one draw merge into one call.
//   Optional, provably result-neutral drops inside a batch: a duplicate UAV barrier (same resource, or covered by a
//   NULL UAV barrier) with no command between them (DropDuplicateUav, default 1); an immediately undone transition
//   pair A:S1->S2, A:S2->S1 for read-only / RT / depth-write S1 (DropUndoPairs, default 0, counted either way).
//
// How: the vtable of the D3D12 runtime's command-list class (D3D12Core, or vkd3d-proton under Proton) is hooked, so
// every recorder (the engine, Streamline's proxy, OptiScaler/FSR/XeSS evaluation on the same list) goes through it.
// Deferral only happens for calls made on the dispatcher thread (thread name) from Nioh3.exe or sl.interposer.dll;
// everything else passes through and first flushes whatever is deferred on that list. Asm thunks
// (gpuopt_thunks.asm) cover every vtable slot, including unknown ones, with a 7-instruction fast path.
//
// Integration (another agent owns dllmain/osd/menu; nothing there was edited):
//   1. build.bat: ml64 /nologo /c /W3 /Fobuild\gpuopt_thunks.obj src\gpuopt_thunks.asm
//                 cl ... /Fobuild\gpuopt.obj src\gpuopt.cpp            (same flags as osd.cpp)
//      link: add build\gpuopt.obj build\gpuopt_thunks.obj  (CMakeLists: add both sources)
//   2. dllmain (after menu::Configure, next to osd::Start): fill gpuopt::StartInfo and call gpuopt::Start(si).
//      Start() reads [GpuOpt] and returns at once; with Barriers = 0 and TraceRecords = 0 it does nothing at all.
//   3. osd InstallThread, right after InstallHooks() succeeded (the engine is rendering then):
//      gpuopt::InstallNow();   (if this call is never added, Start() installs by itself InstallDelaySec after the
//      d3d12 device exists - see Start)
//   4. osd Present hook (once per presented frame, any thread): gpuopt::OnFrame();
//   5. menu (live): "GPU barrier batching" -> gpuopt::SetBatching(on); status row -> gpuopt::StatusLine(buf, n);
//      optional rows MergeAcrossBinds / DropUndoPairs -> SetMergeAcrossBinds / SetDropUndoPairs. "Save to ini"
//      writes [GpuOpt] Barriers = 1/2.
//   6. stats thread (every 30 s): gpuopt::StatusLine -> Log("stats/s gpuopt: ...").
#pragma once
#include <cstddef>
#include <cstdint>

namespace gpuopt {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;  // dllmain's Log
    const wchar_t* iniPath = nullptr;             // Nioh3EngineFix.ini ([GpuOpt]); also where the trace CSV goes
    const char* wine = nullptr;                   // wine_get_version() or nullptr (only logged)
    bool gpuProf = false;                         // 1.7.0: the GP profiler needs the hooks (installs them with Barriers = 0)
    bool giMerge = false;                         // 1.7.29: gimerge (GIProbe / GIMerge) needs them too
    bool skipFx = false;                          // skipfx ([Debug] SkipFx diagnostic, cost-vs-looks) needs them too
    bool shaderHot = false;                       // psoswap ShaderHot (fast-testing live shader A/B) rebinds PSOs through them
};

enum class State : uint8_t {
    Off = 0,       // [GpuOpt] Barriers = 0 and no trace: nothing installed
    Waiting,       // Start() ran, the hooks are not installed yet
    Active,        // hooks installed, batching on
    Paused,        // hooks installed, batching off (Barriers = 2 or switched off live): counts only
    Failed,        // install failed (see log); the game is untouched
};

struct Stats {
    uint64_t frames = 0;            // OnFrame() calls
    uint64_t calls = 0;             // ResourceBarrier calls seen on hooked lists (all threads)
    uint64_t barriers = 0;          // barrier structs in those calls
    uint64_t deferred = 0;          // structs taken into a batch
    uint64_t passthrough = 0;       // structs passed through unchanged (other threads/callers, batching off, aliasing)
    uint64_t passthroughCalls = 0;  // ResourceBarrier calls passed through
    uint64_t emitCalls = 0;         // ResourceBarrier calls gpuopt made for its batches
    uint64_t emitBarriers = 0;      // structs in those calls
    uint64_t dupUavDropped = 0;     // duplicate UAV barriers removed from a batch
    uint64_t undoPairsSeen = 0;     // A:S1->S2 followed by A:S2->S1 inside one batch window
    uint64_t undoPairsDropped = 0;  // of those, removed (both halves; DropUndoPairs = 1)
    uint64_t conflictSplits = 0;    // batch emitted early because the same resource came again (kept sequential)
    uint64_t fullSplits = 0;        // batch emitted because it reached MaxBatch
    uint64_t foreignFlushes = 0;    // a batch was emitted by another thread's call on the list (e.g. frame-end barrier)
    uint64_t r2a = 0;               // 1.7.10 R2a: COMMON -> X rewritten on buffers without ALLOW_UNORDERED_ACCESS
    uint64_t r2b = 0;               // R2b (experiment): the same for UAV buffers
    uint64_t r1Dropped = 0;         // R1 (experiment): read -> read superset transitions dropped
    uint64_t keptTexture = 0;       // COMMON -> X candidates passed through: texture
    uint64_t keptUav = 0;           //   ... UAV buffer while R2b is off
    uint64_t keptUnknown = 0;       //   ... no tracked previous state
    uint64_t keptForeign = 0;       //   ... previous state from another thread / the same list
    uint32_t maxBatch = 0;          // largest batch emitted
    uint64_t flushBySlot[96] = {};  // which list method emitted the batches (index = vtable slot)
};

// Reads [GpuOpt] from the ini. Barriers = 0 and TraceRecords = 0: returns, nothing else happens. Otherwise starts a
// background thread that installs the hooks once d3d12.dll is loaded and InstallDelaySec (default 20) have passed,
// unless InstallNow() did it first. Copies everything, never throws.
void Start(const StartInfo& info);
// Installs the hooks now (idempotent; call from a thread that may create a throw-away D3D12 device, e.g. the osd's
// install thread once the engine renders). Returns true when the hooks are in place.
bool InstallNow();

State GetState();
const char* StateName(State s);
void SetBatching(bool on);          // live; off = pass-through (pending batches still get emitted at their next flush)
bool Batching();
int BarriersIni();                  // [GpuOpt] Barriers at launch (menu Save keeps 0 when only GP installed the hooks)
void SetMergeAcrossBinds(bool on);  // live; off = every hooked list method flushes (strict adjacency)
bool MergeAcrossBinds();
void SetDropUndoPairs(bool on);     // live
bool DropUndoPairs();
// 1.7.10 barrier rewrites (Wine only, dispatcher thread, default OFF; notes\vkd3d-audit.md Addendum B). which: 0 = R2a, 1 = R2b
// (UAV buffers, EXPERIMENT), 2 = R1 (read->read supersets, EXPERIMENT). Live. SetRewrite does nothing off Wine.
void SetRewrite(int which, bool on);
bool Rewrite(int which);
bool RewriteIni(int which);        // value at launch ([GpuOpt] R2a / R2b / R1)

void OnFrame();                     // once per presented frame (optional; enables per-frame figures)
void GetStats(Stats& out);          // cumulative
// One line with per-frame (or per-second, when OnFrame is never called) rates since the previous StatusLine call,
// e.g. "on: 1220 barriers/frame in 1220 calls -> 431 calls (-65 %), dropped 3 dup UAV, 0 undo pairs". Thread-safe.
const char* StatusLine(char* buf, size_t n);

}  // namespace gpuopt
