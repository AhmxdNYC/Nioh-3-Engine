// skipfx.h - SkipFx: per-PSO / per-marker draw & dispatch SKIP list for the cost-vs-looks audit (notes\cost-vs-looks.md).
// DIAGNOSTIC ONLY. [Debug] SkipFx = 0 by default; never in a release profile; with 0 nothing is installed.
//
// What it does (SkipFx = 1):
//   * rides on gpuopt's command-list thunks (gpuopt_thunks.asm, own mask gpuopt_hook3, so GP2 sampling never switches it off):
//     SetPipelineState / Reset (bound PSO per list), BeginEvent / EndEvent (engine marker stack per recording thread, as GP),
//     OMSetRenderTargets (bound RTVs, for ClearMode), DrawInstanced, DrawIndexedInstanced, Dispatch, ExecuteIndirect, DispatchMesh.
//   * a draw / dispatch is NOT recorded (the thunk returns to the caller; the gpuopt barrier batch is emitted first, so resource
//     states stay exactly as without the skip) when
//       - the bound PSO's shader set (psoswap::DescribeSet: "vs=<hash> ps=<hash>" / "cs=<hash>") contains one of the PSO tokens
//         (hex fragments of >= 6 chars, optionally stage-qualified "ps=c4656a8d"), or
//       - an enclosing engine marker matches one of the Marker names (case-insensitive, exact or "Prefix*"; needs engine markers:
//         [Profiler] GPUProf_Markers = 1 or GPUProf >= 1).
//   * compute: a skipped dispatch writes nothing (its UAVs keep their previous contents - stale, not neutral; see the note).
//     Draws: ClearMode = 1 clears the bound render targets to Clear=r,g,b,a once per OMSetRenderTargets binding, but only when the
//     skipped draw is the first draw on that binding (an intermediate target the pass owns); 0 = plain skip (default).
//   * live: the control file [Debug] SkipFxFile (default Nioh3EngineFix_skip.ini next to the ini) is polled every 100 ms; its
//     [SkipFx] Tag / PSO / Marker / ClearMode / Clear replace the active list (write it to a temp name and rename). Initial list =
//     [Debug] SkipPSO / SkipMarker / SkipClearMode / SkipClear. Every apply logs "SkipFx: apply ..." and every 500 ms the ack file
//     <control>.ack gets "gen tag t_apply_ms draws dispatches ei mesh clears matched_sets" (counts since the apply) so a harness
//     can verify the skip hit something. On the next apply the previous condition's totals + matched shader sets are logged.
//   * PSO identity needs the psoswap PSO-creation map: SkipFx = 1 turns psoswap tracking on by itself (as GPUProf = 2 does).
//   * any exception in the hook: SkipFx switches itself off (nothing is skipped any more), logged.
#pragma once
#include <cstddef>
#include <cstdint>

namespace skipfx {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;
};

void Start(const StartInfo& info);  // reads [Debug] SkipFx...; starts the control-file thread when on
bool WantsHooks();                  // SkipFx = 1: gpuopt must install its command-list thunks
// gpuopt, after its vtables are patched: the slots to route to OnListCall, and the original methods (ClearRenderTargetView,
// GetDevice). orig1 may be null (one vtable).
void EnableHooks(volatile uint8_t* mask, int slots, void* const* orig0, void* const* orig1);
// gpuopt slow path, after the barrier flush and the GP hook, before the method. true = do not call the method.
bool OnListCall(void* list, int set, int slot, uintptr_t a1, uintptr_t a2, uintptr_t a3);
// "SkipFx <tag>: 120 draws/s 4 dispatches/s skipped (3 sets)" or "" when off (menu / OSD status row, optional)
const char* StatusLine(char* buf, size_t n);

}  // namespace skipfx
