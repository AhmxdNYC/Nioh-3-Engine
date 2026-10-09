// psojournal.h - PJ: PSO journal + title-screen prewarm ([Patches] PJ = 0 by default; notes\stutter.md 3.1 / 6.1).
//
// Why: the engine creates a PSO on its first draw (PsoGetOrCreateGfx 119670 -> CreateGfxPso 9290D8) on the
// SynergisticDispatcher thread while it holds the PSO-cache critical section; the main thread spins at frame end
// (WaitDispatcherDrainSpin 418F40) until the dispatcher drains, so a slow create is a frame hitch. Its PSO lists
// (PsoListLoad 3483E04) only cover what they record. A create is slow when the driver's caches miss: vkd3d-proton
// (DXIL -> SPIR-V) + RADV/ACO on the Ally, the AMD shader cache on Windows - after a driver / Proton update, a cache
// eviction, or the first visit of content outside the lists.
//
// What it does (D3D12 level, nothing in Nioh3.exe is patched):
//   * hooks ID3D12Device::CreateGraphicsPipelineState / CreateComputePipelineState / CreateRootSignature and
//     ID3D12Device2::CreatePipelineState (stream) on the vtable of the device the engine creates. The device comes from
//     psoswap's D3D12CreateDevice IAT hook (psoswap arms it when PJ != 0; it then only reports the device).
//   * every PSO the engine creates successfully is serialized (all desc fields, shader stages as content keys, the root
//     signature as its serialized blob's key, the input layout with its semantic names; a cached-PSO blob is dropped)
//     and appended to <ini folder>\Nioh3EngineFix_pj\pj_<os>_<gpu>_sm<NN>.bin - one file per device class - with each
//     shader / root-signature blob stored once. Padding is zeroed, so a desc gets the same key in every session.
//   * the next launch reads the journal and, once the title screen is idle (G+58Ah clear, no engine PSO create for
//     PJ_QuietMs), creates on ONE lowest-priority background thread every journaled PSO the engine has not created in
//     this session yet, through the same device method the engine used (and the hooks below PJ, e.g. psoswap), then
//     releases it. The driver's in-memory and disk caches are warm when the engine asks for the same PSO; the game's
//     own ComputePsoWarmOrCache 2EC8BF4 does exactly this for 16 compute PSOs at boot.
//
// Same look: the engine's calls are passed through with its own arguments and its own result; the journal only reads.
// Prewarm objects are separate PSOs that are released at once and never bound. Game memory is read inside __try only.
// Bounded: journal file <= PJ_MaxMB (default 64), <= 16384 PSO records, writer queue <= 32 MB (beyond: dropped, counted);
// prewarm reads blobs on demand from the file and frees everything when done. Register-safe: COM vtable hooks (plain
// Win64 ABI, notes\hook-register-audit.md).
//
// 2026-10-09 (journal v2, notes\stutter-pso.md): only RUNTIME creates are journaled. The boot PSO list (PreloadPSOMode,
// ~17,000 creates in the first ~15 s, made again in every launch) filled the v1 journal (64 MB / 14,399 PSOs) before the
// title, so the runtime first-use PSOs that hitch were never journaled ("dropped"). Boot = until the engine has created
// no PSO for PJ_QuietMs and no level load runs (the prewarm gate); creates before it are only marked, not written.
//
// PJH (hand-over, [Patches] PJH = 0 off | 1 on | 2 installed + off; live menu row "PSO hand-over (PJH)"): the prewarm
// KEEPS the PSOs it creates (<= PJ_KeepMax, built on the engine's own root-signature objects) instead of releasing them,
// and when the engine later creates the same desc (serialized before its call, same key) it gets that object (a fresh
// reference of the requested interface) at once: no driver compile on the dispatcher, whatever state the driver's shader
// cache is in. Same PSO: same device method, same desc bytes, same shaders, same root signature. Misses and the switch
// off run the engine's own call unchanged. PJH != 0 implies PJ = 1.
//
// PL (1.7.46, [Patches] PL = 0 off | 1 on | 2 installed + off = boot-list timing line only; notes\boot-shader-compile.md):
// a persistent ID3D12PipelineLibrary for the BOOT PSO list only (creates before the boot gate). Lookup by the PJ desc key
// with the engine's own desc (the runtime refuses a differing desc), miss -> the engine's call + StorePipeline; at the
// gate the library is serialized when it grew and released. Invalidation: exe build, device class, adapter ids, UMD
// driver version, size, Hash128 of the blob, and D3D12's own check (DRIVER_VERSION_MISMATCH / ADAPTER_NOT_FOUND). Off on
// Wine and when ShaderSwap / GPUProf 2 / GIProbe / GIMerge hook PSO creation. Works with or without PJ.
// ini ([Patches]):  PJ = 0 off | 1 journal + prewarm | 2 journal only;  PJ_MaxMB = 64;  PJ_QuietMs = 3000;
//                   PJH = 0 | 1 | 2;  PJ_KeepMax = 4096;  PL = 0 | 1 | 2;  PL_MaxMB = 1024
#pragma once
#include <cstddef>
#include <cstdint>

namespace pj {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;  // dllmain's Log
    const wchar_t* iniPath = nullptr;             // Nioh3EngineFix.ini ([Patches] PJ, PJ_MaxMB, PJ_QuietMs)
    uintptr_t base = 0;                           // Nioh3.exe image base (title gate reads [base+4759910]+58Ah)
    bool knownBuild = false;                      // v2.02: title gate on; otherwise the quiet-time gate only
    bool wine = false;                            // part of the journal's device-class name
    // tests only (tests\pj_harness.cpp): override the folder / quiet time, start the prewarm without the quiet gate
    const wchar_t* folder = nullptr;
    int quietMs = -1;
};

// [Patches] PJ != 0 (dllmain passes this to psoswap so its D3D12CreateDevice hook is armed and reports the device).
bool Wanted(const wchar_t* iniPath);
// Reads the ini; PJ = 0 -> nothing at all. Otherwise starts the writer / prewarm threads (they wait for the device).
void Start(const StartInfo& info);
// Every device the engine creates (called from psoswap's D3D12CreateDevice hook, any thread). Hooks its vtable once.
void OnDevice(void* device);

struct Stats {
    int mode = 0;                // PJ value (0 off)
    bool hooked = false;         // a device vtable is hooked
    uint32_t fileRecs = 0;       // PSO records in the journal at start
    uint32_t newRecs = 0;        // records appended this session
    uint32_t shaders = 0;        // shader blobs in the journal (start + new)
    uint32_t rootSigs = 0;       // root-signature blobs in the journal (start + new)
    uint64_t fileBytes = 0;      // journal size now
    uint64_t seen = 0;           // engine PSO creates seen (graphics + compute + stream)
    uint64_t skipped = 0;        // not journaled: unknown root signature, unsupported field, unreadable desc
    uint64_t dropped = 0;        // not journaled: queue full / journal full
    // prewarm
    int prewarmState = 0;        // 0 off / waiting, 1 running, 2 done
    uint32_t pwCandidates = 0;   // journal records at prewarm start
    uint32_t pwAlready = 0;      // skipped: the engine had already created this PSO in this session
    uint32_t pwOk = 0;           // created + released
    uint32_t pwFailed = 0;       // the device refused it (desc journaled on another device class / driver)
    uint32_t pwMissing = 0;      // a shader / root-signature blob missing or corrupt in the journal
    double pwMs = 0, pwMaxMs = 0;  // total / slowest create
    // journal v2 / PJH
    bool bootDone = false;       // the boot PSO list is over (runtime creates are journaled)
    uint64_t bootCreates = 0;    // engine creates during the boot list (not journaled)
    int handover = 0;            // PJH value (0 off)
    uint64_t handed = 0;         // engine creates answered with a prewarmed PSO
    uint64_t handMiss = 0;       // engine creates after boot with the switch on and no prewarmed PSO (compiled as usual)
    uint64_t kept = 0;           // prewarmed PSOs waiting for the engine
    uint64_t keepRefused = 0;    // prewarmed but released: PJ_KeepMax reached
    // PL (boot pipeline library) + boot-list timing
    int pl = 0;                  // PL value (0 off; 2 also when PL = 1 was refused: Wine / ShaderSwap / GPUProf 2 / GI hooks)
    int plPhase = 0;             // 0 opening, 1 open, 2 closed, -1 off
    bool plFromFile = false;     // the library came from the file (boot creates are loaded)
    bool plRebuilt = false;      // a file existed but was refused (stale / corrupt / D3D12 said no)
    uint32_t plFileEntries = 0;  // pipelines in the file at start
    uint64_t plLoaded = 0, plMiss = 0, plStored = 0, plEarly = 0, plErrors = 0;
    bool plDone = false;         // closed at the boot gate (and written when it grew)
    uint64_t bootN = 0;          // engine creates before the boot gate
    uint32_t bootThreads = 0;    // distinct threads that created them
};
void GetStats(Stats& out);
const char* StatusLine(char* buf, size_t n);  // "journal 1834 PSOs (+12) 41.2 MB | prewarm done 220 ok ..." ("" when off)
void LogLine();                               // "stats/s PJ: ..." (stats thread, every 30 s); no-op when off

// PJH live switch (menu row; 1 = hand prewarmed PSOs over); nullptr when PJH = 0 or PJ is off
volatile uint32_t* HandoverSwitch();

// tests: wait until the writer queue is drained / the prewarm has finished / the boot gate opened (true) or the timeout
// expired (false)
bool WaitIdleForTest(unsigned long ms);
bool WaitBootForTest(unsigned long ms);
bool WaitPrewarmForTest(unsigned long ms);
bool WaitPlReadyForTest(unsigned long ms);  // the PL library is open (or PL decided it is off)
bool WaitPlDoneForTest(unsigned long ms);   // PL closed at the boot gate (library written when it grew)

}  // namespace pj
