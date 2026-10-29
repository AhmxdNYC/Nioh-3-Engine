// asyncslots.h - AS: the engine's own async-compute slots, switched on at boot and A/B'd live (1.7.44, default off).
// notes\gpu-async-wins.md.
//
// KTGL has 13 async-compute "slots" (g_AsyncSlotTable .data 44C04D0, {u8 enable, f32, u32 id} x 13). The renderer init
// (RenderAsyncSlotsInit 2269EC4) creates one D3D12 COMPUTE queue + context per enabled slot. Each slot user is written twice
// by KT: `ctx = AsyncSlotGetCtx(sched, slot)` (52BE1C); NULL -> the synchronous path in the graphics stream; else the same
// dispatches on the slot's compute context, which first waits on the graphics fence of that point (5F17C -> 3D2724), and
// publishes its own fence (AsyncSlotAddFence 148514). The consumers wait on it right before they read (peek 3C5D0C ->
// GfxWaitAsyncFence 4F8F38 -> pop 4F94B4): ViewVsmList / MaterialTechniqueBind for 11/12, the RefProbe pass and
// EffectViewPrepare for 0. Shipped: slots 3 (VCLOUD), 4 (VFSCAT fog), 6 (ocean) on; slot 0 (effects) on in the table but
// switched off at 226A490 on every non-NVIDIA GPU; 11 (GrassVertexDisplacement) and 12 (Tree2Wind) off everywhere.
//
// AS moves graphics-queue compute that runs at low occupancy onto async queues with the engine's own fences:
//   [Patches] AS = 0 | 1 | 2  (0 = nothing; 1 = boot slots + live switch ON; 2 = boot slots, switch OFF = stock)
//   [Tuning]  AS_Slots = 0   slots to create at boot (Ally: 0 11 12; on the PC 11 / 12 cost more than they hide) (before the renderer init, like AS0). "-N" (N = 3, 4, 6) = keep
//             slot N synchronous while the switch is ON (live A/B of the shipped async passes, e.g. on the power-limited Ally).
// Live switch: the PRODUCER call of each switchable slot (one per slot, all callers audited) goes through caveAsK, which
// ANDs the slot mask: off = the stock slot set at those producers, on = all created slots minus the "-N" ones. The joins /
// KIDS job / init calls of slot 0 stay stock, so a deferred effect batch (RS+13ACh) always finds its context.
// Same output: the same shaders and the engine's own async path with its own fences (KT ships it on NVIDIA for slot 0).
// Risk: slots 11/12 never ran on PC; KT switched slot 0 off for non-NVIDIA on purpose (unknown reason; stable in the
// 1.7.5 / 1.7.16 runs). Intel GPUs: slot 0 is refused (hardware-compatibility.md).
#pragma once
#include <cstdint>

namespace asyncslots {

struct BootCfg {
    int mode = 0;            // [Patches] AS
    uint32_t want = 0;       // AS_Slots positive entries (bit per slot)
    uint32_t sync = 0;       // AS_Slots "-N" entries
    uint32_t vendor = 0;     // primary display adapter PCI vendor (0 = unknown)
    // results of the boot write
    uint32_t added = 0;      // slots the boot write added vs the stock set for this vendor
    bool applied = false;    // boot write done (renderer had not initialised yet)
    bool gateWritten = false;  // 226A497 74 -> EB (the AS0 byte)
};

// Reads the ini ([Patches] AS, [Tuning] AS_Slots) and the adapter vendor. false = AS off (nothing to do at boot).
bool BootWanted(const wchar_t* iniPath, BootCfg& b, void (*log)(const char*, ...));
// One step of dllmain's decrypt-wait loop. true = still pending (code / .data not decrypted yet): call again.
bool BootTry(uintptr_t base, BootCfg& b, void (*log)(const char*, ...));

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    uintptr_t base = 0;       // Nioh3.exe image base
    bool knownBuild = false;  // v2.02 only
    const BootCfg* boot = nullptr;
};
// After decryption: installs the producer-site caves for the switchable slots (added or "-N"). No-op with AS = 0.
void Start(const StartInfo& info);
// Live switch (nullptr when not installed): 0 = stock slot set at the producers, 1 = AS slots.
volatile uint32_t* Switch();
// One status line for the 30 s stats log ("" when not installed).
void StatusLine(char* buf, unsigned n, uintptr_t base);

}  // namespace asyncslots
