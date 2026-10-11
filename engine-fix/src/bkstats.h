// bkstats.h - BKS: D3D12 back-end queue / wait census + KL kick-limit knob (1.7.60, default off).
// notes\renderer\backend.md (structs, thread diagram, which wait is which).
//
// MEASUREMENT (BKS, [Profiler] Backend = 0 | 1): no hooks, no code bytes. A thread reads the engine's OWN wait counters
// (KTGL keeps {total, max, count} QPC accumulators around every blocking point of the back end) and samples the queue
// state every BackendSampleUs:
//   main thread   : kicks / draws / stream bytes per frame, dispatcher-ring-full spin and end-of-frame drain spin
//                   (GfxDevice+3D00h, the per-frame snapshot CmdStreamEndFrame 418B94 writes), ECL idle spin at submit
//                   (CmdListRing+C8h, GfxSubmitFrameCmdList 252758)
//   dispatcher    : ECL-ring-full stall (CmdListRing+B0h, EclRingPush 86F760), list recycle fence wait (+F8h, 4920E0),
//                   tok 0x95 cache upload work (KtglCache+9F8h, 7F6904), list count / ECL capacity / high-water
//   sampler       : main in drain spin (g_DispatcherDrainBusy 45CE8A0) vs recording; dispatcher STARVED while the main
//                   thread records (ring empty, nothing in flight) = the time a smaller kick limit could fill;
//                   backlog; dispatcher / ECL ring full; KtglCache SRW lock +CF8h held / contended.
//   One "BKS:" line every BackendSec (default [Profiler] ReportSec).
// KNOB (KL, [Tuning] KL_Div = 0, live 'Tuning.KL_Div = n'): KtglContext::CmdFlushLimit (GfxDevice+3C70h, the draw count
//   after which the main thread hands a stream segment to SynergisticDispatcherTask, KtglCmdStreamFlush 254110) =
//   max(16, stock / n). 0 = the engine's own value. Pure data: every reader compares 'limit <= count' at use time, the
//   hand-off and the replay are the engine's own code, so the same tokens reach the same command lists in the same order;
//   only the hand-off granularity changes. Re-asserted when the engine rewrites the field (device re-create).
#pragma once
#include <cstddef>
#include <cstdint>

namespace bkstats {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;
    uintptr_t base = 0;       // Nioh3.exe image base
    bool knownBuild = false;  // v2.02 only
};
void Start(const StartInfo& info);
// LiveCtl 'Tuning.KL_Div = n' (dllmain LiveTuning): true = key handled (reply = new value text or the error)
bool SetTuning(const char* key, const char* val, char* reply, size_t n);

}  // namespace bkstats
