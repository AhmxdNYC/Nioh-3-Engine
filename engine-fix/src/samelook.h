// samelook.h - the "same-look" GPU package (1.7.49, all default off): notes\same-look-package.md.
//
// VSMA (VSM waiting-tile amortisation, [Patches] VSMA = 0 | 1 | 2; live menu row; notes\shadows-redundancy.md S1/1b):
//   VsmClipmapRender 1F8340 (the CPU tile path of the virtual shadow map, one clipmap per call) clears every dirty+requested
//   tile, then batch-draws ALL of them ("Batch Tiles 8"), then draws the non-batchable items of the first `budget` bound
//   tiles (VsmTilesPerFrameBudget 1F7960: 8 at SHADOW_QUAL STANDARD..HIGHEST, 2 at LIGHTEST/LIGHT) and marks only those
//   valid (40118 bit 0). Every VSM consumer (all 356 shader variants) gates each tap on bit 0, and the next visit clears the
//   tile again before it can be drawn, so the batch draws of the tiles past the budget are never seen as texels. They only
//   feed texture-streaming sampler feedback and the meshlet cache codes (requests / residency) for the casters in them,
//   which is why S1 was not bit-exact and VBC was never built (shadows-redundancy 1b).
//   VSMA keeps those requests and cuts the redraws:
//     - the first `budget` distinct bound tiles of a visit (the ones the engine marks) are always drawn: stock;
//     - a tile that is valid (bit 0 set, a lifespan / stamp refresh) is always drawn: stock;
//     - the next VSMA_Front x budget waiting tiles in the engine's order (the ones marked in the next visits) are always
//       drawn: their warm-up before marking is stock;
//     - every other waiting tile is drawn on one visit of its clipmap out of VSMA_K (phase = tile index, so the redraws
//       spread evenly over the visits). It is never skipped more than VSMA_K - 1 visits in a row, and on its first
//       visit after it became a waiting tile it is drawn whatever its phase when VSMA_First = 1 (default 0: the phase
//       decides; it is far down the list, so its request still leads its marking by >= VSMA_Front visits).
//   Inert (stock for the whole call) when the node param 37B3FAE9 (sub-light shadow throttle, 8F041C; default -1) is
//   >= 0, because the engine compares that against a draw count that includes the batch draws.
//   Sites: VSMA1 538F62 (call 1F8340: per-call reset), VSMA2 1F8AB2 (call 1F7960 in the batch loop: throttle lookup on
//   the same param table), VSMA3 1F8AF2 (call VsmTileBindViewport 1F96E0 in the batch loop -> caveVsmaBind, which also
//   passes rbx = tile entry, r14 = page manager, edi = budget). A skipped tile simply is not bound: the engine's own
//   "bind failed" path (no append, no batch), exactly as an empty tile.
//
// Engine update-rate knobs (render-config overrides, written into the frame's RenderConfig S right after
// ApplyGraphicsSettings and before the renderer consumes it: site RCO1 39B7CA, the renderer vfunc +4B8h call):
//   LPVR  [Patches] LPVR = 0|1|2, [Tuning] LPVR_N   -> S+1394 LPVUpdateFrequency (u32, env 1 in all presets)
//   SKYR  [Patches] SKYR = 0|1|2, [Tuning] SKYR_V   -> S+0144 SkylightAmbientUpdateFreq (f32, env 1)
//   FOGT  [Patches] FOGT = 0|1|2, [Tuning] FOGT_V   -> S+13C0 VolumetricFogTemporalFactor (f32, env 0)
//   CLDR  [Patches] CLDR = 0|1|2, [Tuning] CLDR_V   -> S+07C0 VolumetricCloudReprojectionBlendWeightCurrentFrame (f32, env 0)
//   One live menu row each. The first frame logs the values the engine had in S (and S+13DC VolumetricFogResolutionX as an
//   offset check) and, while a knob is on, whether the engine re-filled the field between two frames.
// 0 = nothing installed, 1 = installed + on, 2 = installed + off (switch with the menu row or `vpad menu set <key> 1`).
#pragma once
#include <cstddef>
#include <cstdint>

namespace samelook {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;
};
// Reads the ini keys (call before the patch groups are installed). Nothing is installed here.
void Configure(const StartInfo& si);
int VsmaIni();  // [Patches] VSMA (0 / 1 / 2)
int RcoIni();   // non-zero when any of LPVR / SKYR / FOGT / CLDR is non-zero
// dllmain's expGroup prep: original call targets (verified against the expected RVAs there)
void SetVsmaTargets(uintptr_t vsmRender, uintptr_t budget, uintptr_t bind, uintptr_t throttleGetter);
void SetRcoInstalled();
// live switches (nullptr = not installed this session)
volatile uint32_t* VsmaSwitch();
enum Knob { kLpv, kSky, kFogT, kCloud, kNumKnobs };
volatile uint32_t* KnobSwitch(int knob);
// fast-testing LiveCtl: set a [Tuning] value live (VSMA_K, VSMA_Front, LPVR_N, SKYR_V, FOGT_V, CLDR_V); false = unknown key
bool SetTuning(const char* key, const char* val, char* reply, size_t n);
// 30 s stats line ("" when nothing is installed)
void StatusLine(char* buf, unsigned n, unsigned secs);
void OnPresent();  // osd Present hook: frames for the per-frame rates

}  // namespace samelook

// C entry points reached through the patch trampolines (caves) - not for direct use
extern "C" {
void caveVsmaEntry(void* a, void* b);
uint32_t caveVsmaBudget(void* table);
void caveVsmaBind();  // samelook.asm
void caveRco();       // samelook.asm
uint8_t VsmaBindC(void* clip, void* ctx, const uint32_t* tile, uint8_t* pm, uint32_t budget);
void RcoApplyC(uint8_t* S);
}
