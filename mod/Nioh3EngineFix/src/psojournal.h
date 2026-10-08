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
// ini ([Patches]):  PJ = 0 off | 1 journal + prewarm | 2 journal only;  PJ_MaxMB = 64;  PJ_QuietMs = 3000
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
};
void GetStats(Stats& out);
const char* StatusLine(char* buf, size_t n);  // "journal 1834 PSOs (+12) 41.2 MB | prewarm done 220 ok ..." ("" when off)
void LogLine();                               // "stats/s PJ: ..." (stats thread, every 30 s); no-op when off

// tests: wait until the writer queue is drained / the prewarm has finished (true) or the timeout expired (false)
bool WaitIdleForTest(unsigned long ms);
bool WaitPrewarmForTest(unsigned long ms);

}  // namespace pj
