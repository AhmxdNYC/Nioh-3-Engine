// mpccheck.h - MpcCheck ([Profiler] MpcCheck = 0 by default; needs GPUProf = 2 and [GpuOpt] Barriers = 0): runtime self-check of the
// MPCS tail skip (notes\meshlet-frontend.md 3). DIAGNOSTIC: it never skips anything; it records the STOCK chain and proves, per chain,
// that what MPCS would skip is a no-op on that frame's data:
//   MPC #28-#31: snapshot every buffer the engine UAV-barriers after MPC #23 (before #28 runs) and again after #31 (before the next
//                dispatch); equal contents = skipping #28-#31 gives the same buffers. Applies when #23 ran as Dispatch(1,1,1), i.e.
//                1 <= cnt <= 1024 - exactly the cave's condition (the CPU cnt of MeshletPreculling 5301FC).
//   MPC #26:     the same buffers before #26 and after it (before #27); applies when #23 ran (>= 1 group).
// Snapshots are buffer copies into READBACK memory (transition UAV -> COPY_SOURCE -> UAV around them) inserted on the engine's own
// command list in stats frames only (the timing frames of the MLP probe stay clean); a WriteBufferImmediate marker after the last
// copy tells the Present thread when the data is complete. With the SHIPPED 0x29ee0902.file #28-#31 do real work, so the check
// must report mismatches there (negative control); with the `mpc` build it must report none.
// Log: "MPCK: ..." lines every 30 s (chains checked / equal / MISMATCH with the first differing dword).
#pragma once
#include <cstdint>

struct D3D12_RESOURCE_BARRIER;

namespace mpcck {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;
    int chains = 0;   // [Profiler] MpcCheck: chains to verify, then the check stops
};

bool Start(const StartInfo& info);   // reads [Profiler] MpcCheckMaxMB; false = off
bool Wanted();                       // still checking (target not reached, not failed)

// gp (gpuprof.cpp), stats frames, recording thread, inside a MeshletPreculling region, BEFORE the original method
void OnRegion(void* list, int set, bool begin);
void OnDispatch(void* list, int set, uint32_t key, uint32_t x, uint32_t y, uint32_t z, bool indirect);
void OnBarrier(void* list, int set, unsigned n, const D3D12_RESOURCE_BARRIER* b);   // AFTER the original ResourceBarrier
void OnClose(void* list, int set);
void OnFrame();   // Present thread: completed snapshots are compared here
void LogLine();   // stats thread, 30 s

// shared with the harness test: MPC kernel of a CS digest prefix (first 32 bits): 23, 24, 26, 27, 28..31, or 0
int KernelOf(uint32_t key, bool* mpcBuild);

}  // namespace mpcck
