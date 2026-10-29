// memstats.h - process / GPU memory line for the 30 s stats thread ([Profiler] MemStats = 1). Measurement only.
//
// Why: notes\memory-leaks.md. MangoHud on the Ally shows system RAM, swap and the APU's GPU memory, but process_rss is
// always 0 under Proton, and nothing logs handles or threads. LogLine() writes one line:
//   stats mem: private X MB, working set Y MB (peak P), commit Z MB, handles H, threads T, VRAM local used/budget A/B MB,
//              non-local C/D MB | sys RAM avail R MB, commit S/L MB | since start (m min): private +x, ws +y, handles +h,
//              threads +t, VRAM local +a, non-local +c
//   - private      = PROCESS_MEMORY_COUNTERS_EX::PrivateUsage (the process commit; Nioh 3 commits a 6 GiB arena at boot,
//                    see memory-leaks.md, so this is ~6.2 GB from the start and only moves when something outside the
//                    arena allocates)
//   - working set  = resident pages (Linux RSS under Wine). The arena's pages count once touched and are never returned
//   - commit       = PagefileUsage (Windows: equals private; Wine: its own bookkeeping)
//   - handles      = GetProcessHandleCount; threads = Toolhelp thread snapshot, this process only
//   - VRAM         = IDXGIAdapter3::QueryVideoMemoryInfo for the game's adapter (found by the D3D12 device's LUID through
//                    CreateDXGIFactory1 from the SYSTEM dxgi.dll, loaded at run time). Under Proton this is DXVK's dxgi;
//                    'used' may then count only allocations made through DXVK's own Vulkan device (budget is still
//                    right). Cross-check with MangoHud gpu_vram_used.
// Nothing here touches the game's state; the adapter is opened once and kept.
#pragma once
#include <cstdint>

namespace memstats {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;  // [Profiler] MemStats (0 off / 1 on, default 0)
    uintptr_t base = 0;                // Nioh3.exe image base
    bool knownBuild = false;           // v2.02: the D3D12 device may be read from the engine (g_RenderGlobals+4268h)
    // Optional: returns the game's ID3D12Device* (AddRef not required, may return null until the device exists).
    // When set it wins over the engine read (the OSD already captures the device from the game's queue).
    void* (*getD3D12Device)() = nullptr;
};

// Reads [Profiler] MemStats. Returns false (and does nothing more) when it is 0.
bool Start(const StartInfo& info);

// True when Start() enabled the module.
bool Enabled();

// Logs one "stats mem:" line. Call from the 30 s stats thread (any thread except one holding the loader lock).
// Cheap: ~1 ms (one Toolhelp thread snapshot + two QueryVideoMemoryInfo calls). No-op when disabled.
void LogLine();
// 1.7.36 (Tour): VRAM local / non-local used and budget in MB (any thread); false until the adapter is known (MemStats = 1).
bool Vram(double* localUsed, double* localBudget, double* nonLocalUsed, double* nonLocalBudget);
// 1.7.48 VS probe ([Profiler] MemStats = 2): one 'stats vs:' line per second - VRAM local / non-local, the engine's texture
// page pool (size, used, occupancy vs its 85 % controller threshold, last-frame hits / misses / queue-full) and D3D12
// MakeResident / Evict / CreateHeap / CreateCommittedResource counts and bytes per memory pool. Call every second.
void Tick1s();
// 1.7.48 VS bind census (from gpuprof's command-list hook, GPUProf = 2): root CBV/SRV/UAV (slots 37-42), IASetIndexBuffer (43),
// IASetVertexBuffers (44) whose GPU address lies in a system-memory heap / buffer, counted per engine marker.
bool BindsWanted();
void OnBind(int slot, uintptr_t a1, uintptr_t a2, uintptr_t a3, uint16_t marker, uint32_t psoSet);   // psoSet 0 = unknown
// 1.7.48 VS boot half: from the decrypt-wait loop; hooks D3D12CreateDevice in the IAT. true = done / not wanted.
bool BootIat(uintptr_t base, const wchar_t* iniPath, void (*log)(const char*, ...));

}  // namespace memstats
