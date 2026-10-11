// gimerge.h - GI gather light-group merge (notes\gi-highest-audit.md s2, remaining-work 3.2) for Nioh3EngineFix (1.7.29).
// [Debug] GIProbe = 0 and [Patches] GIMerge = 0 by default: then nothing is installed at all.
//
// What the game does: ReflectionProbePassRecord (542C08) dispatches the RefProbe gathers #121 (diffuse) and #414
// (reflection) once per stencil light group ([1,2]) over the FULL gather grid. Every lane loads its stencil byte and
// exits unless (u32(gv_mask_param.y) & stencil.g) == u32(gv_mask_param.x). The group-1 dispatch is 96-99 % early-out
// waves (r5 trace: 1.1-1.3 us/wave against 17-21 us for real waves); at GI HIGHEST that is 32,400 waves per gather.
//
// GIProbe (A0, measurement only): logs, per target dispatch, the grid, the groups (= waves at wave64), gv_mask_param,
// the CBV, which root tables changed against the previous target dispatch on that list, which cbuffer rows differ
// against the previous dispatch of the same shader, and whether a single masked test can accept both refs. Counters
// every 30 s ("stats/s giprobe:"); one full frame of per-dispatch lines every GIProbeEvery frames.
//
// GIMerge (A1, ASI only, no shader change): for two back-to-back dispatches of the same gather (same PSO, same grid,
// exactly one root table rebound in between, cbuffers byte-identical except gv_mask_param.x/.y, and a (x', y') whose
// accepted stencil set is exactly the union of both), the first dispatch is not recorded and the second one runs with
// (x', y') written into its own upload-ring cbuffer before the GPU has seen it. Every pixel is then written once, by
// the same shader with the same inputs, as before. Exact by construction; see notes\gi-highest-audit.md s2 and s6.
//   * The first dispatch is HELD, not dropped: anything on that list other than a root-table rebind, a UAV barrier or
//     the matching second dispatch replays it on the spot (same PSO, its own root tables restored, then the current
//     ones; plus a UAV barrier when one was held over), so a pattern break costs nothing but the original order.
//   * GIMerge = 1 first qualifies: it only merges after GIMergeQualify clean pairs in a row (observed, not merged), and
//     drops back to observing on any pair that fails a condition. Any later dispatch that would read the rewritten
//     cbuffer (same CBV table still bound, no new CBV) disables the merge for the session.
//
// Hooks: an import hook on D3D12CreateDevice (armed early, chains psoswap's), then on that device's vtable
// CreateGraphicsPipelineState / CreateComputePipelineState (PSO -> shader digest) and CreateConstantBufferView (which
// CBV a dispatch reads), and ID3D12Resource::Map/Unmap (upload-buffer VA -> CPU pointer). Command-list calls arrive
// through gpuopt's thunks (gpuopt_hook2 slots + gpuopt_hold); all are COM/vtable sites, plain Win64 ABI.
#pragma once
#include <cstddef>
#include <cstdint>

namespace gimerge {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;  // dllmain's Log
    const wchar_t* iniPath = nullptr;             // Nioh3EngineFix.ini ([Debug] GIProbe*, [Patches] GIMerge*)
    const char* wine = nullptr;                   // wine_get_version() or nullptr (only logged)
};

// Reads the keys; both 0 -> returns, nothing happens. Otherwise arms the D3D12CreateDevice import hook. Call EARLY
// (dllmain Worker, right after psoswap::Start, before the engine creates its device).
void Start(const StartInfo& info);
bool WantsHooks();  // gpuopt must install its command-list hooks for us
// gpuopt::InstallNow, after its vtables are patched: our slot mask, the original methods per vtable set, the hold slot
void EnableHooks(volatile uint8_t* mask, int slots, void* const* orig0, void* const* orig1, void* volatile* hold);
// gpuopt slow path (thunk -> GpuOptSlow), BEFORE gpuopt's own batch flush and before the original method.
// Returns true when the call must NOT reach the original method (only ever for Dispatch).
bool OnListCall(void* list, int set, int slot, uintptr_t a1, uintptr_t a2, uintptr_t a3);
// gpuopt HkResourceBarrier, only for the held list, before gpuopt defers / passes the barriers
void OnBarrier(void* list, int set, unsigned n, const void* barriers);
void OnFrame();  // osd Present hook (per-frame rates, probe frame selection)
void LogLine();  // stats thread, every 30 s: counters + buffered probe lines
const char* StatusLine(char* buf, size_t n);

// ---- pure helpers (exposed for tests\gimerge_harness.cpp) ----
// Accepted set of the gather test: S(x, y) = { s : (s & y) == x }. Returns true and (x', y') when S(x1,y1) u S(x2,y2) is
// exactly S(x', y') over the whole u32 domain (so over any stencil format).
bool MaskUnion(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t& xo, uint32_t& yo);

// ---- test seam (harness only; never called by the mod) ----
struct TestApi {
    void (*registerPso)(void* pso, const uint8_t digest[16]);
    void (*createCbv)(uint64_t va, uint32_t size);              // as CreateConstantBufferView on this thread
    void (*mapRange)(uint64_t va, void* cpu, uint64_t size);    // as Map of an upload buffer
    void (*setMode)(int probe, int merge, int qualify);
    uint64_t (*counter)(int which);  // 0 merged, 1 replayed, 2 held, 3 pairs ok, 4 pairs bad, 5 reuse conflicts, 6 armed
};
const TestApi& Test();

}  // namespace gimerge
