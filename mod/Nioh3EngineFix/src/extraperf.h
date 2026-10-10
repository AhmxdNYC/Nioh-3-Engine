// extraperf.h - "Extra performance (changes the look)" (1.7.59): options the game's own menu does not have, trading picture for
// fps. Every option drives a real engine control (a RenderConfig field the game refills every frame, a quality-table result or
// an engine fraction) - no draw skips. Design, sites, evidence and measurements: notes\extra-performance-settings.md.
//
// Every option is a level (0 = the game's own value) switched live from the mod menu, `vpad menu set <key> <n>` or LiveCtl
// (`XCLD = 2`); [ExtraPerf] Enabled = 1 installs the hooks (all levels 0 = pass-through), 0 installs nothing.
//
//   key    option                       engine control (level 1 / 2 / 3 / 4)
//   XCLD   cloud ray-march steps        RenderConfig S+0x410 VolumetricCloudMaxRaySampleNum / S+0x40C MinRaySampleNum (int, env
//                                       256 / 32): min(game, 128/24 | 64/16 | 32/8). VCLOUD marches ceil(min(mad(|dir.y|, x, y)
//                                       * 1.31 / w, y)) steps from g_vRaySampleNum = these two (vcloud_cs7_dec.hlsl 1042-1054)
//   XFOG   volumetric fog grid          S+0x13DC / S+0x13E0 VolumetricFogResolutionX / Z (int; ApplyVolumetricQuality 39B080 writes
//                                       them every frame: HIGH 160/64 .. LIGHT 80/32): min(game, 64/24 | 48/16), level 3 = the
//                                       game's own VOLUMETRIC LIGHTEST values (0 / 0 and S+0x1398 ScatterScale 0 = fog off)
//   XGI    GI gather resolution         GiScaleLookup 187A50 results (Indirect / Reflection / IndirectOcn / ReflectionOcn render
//                                       scales, read every tick by GiRenderScalesApply 1872C4 at 18745A/464/471/47F) x 0.75 | 0.5 |
//                                       0.35, floor 0.1 (the engine's own sub-resolution gather path; GI LIGHTEST uses 0.2..0.3)
//   XSHD   shadow distance              S+0xAF4 / S+0xAF8 ShadowEraseFarEnd / Start (float, env 50000 / 25000 cm: caster cull +
//                                       fade) x 0.6 | 0.35 | 0.2
//   XSHF   shadow filter taps           S+0xB44 ShadowSampleNum (int, env 4): min(game, 2 | 1)
//   XLOD   geometry LOD (meshlets)      the 8 per-category meshlet LOD scales ApplyMeshletLodScales 2353C0 writes to the
//                                       GameUpdateTick object +0xA108..+0xA124 (call site 233A49; bigger = coarser: MESHLET_QUALITY
//                                       HIGH x1, STANDARD x1.5, LIGHT x2) x 1.5 | 2 | 3, capped at the engine's 10.0
//   XTER   terrain tessellation         S+0x1970 TerrainTessellationMaxDistance / S+0x1978 DistanceAdaptRate (float; LANDSCAPE
//                                       LIGHTEST 1000 / 0.3): x 0.7 | 0.4 | 0.2
//   XPRT   GPU particles                g_ViewGpuEffectSimFraction 4566008 / g_ViewGpuEffectDrawFraction 4566000 (the engine's
//                                       per-view share of GPU particle groups simulated / drawn; stored per view by 4A58DC at
//                                       4A5B76 / 4A5B7E) x 0.75 | 0.5 | 0.25 | 0 (off)
//   XAO    ambient occlusion off        S+0x12F4 SSAORayNum = S+0x12F8 SSAOStepNum = 0, S+0x12D8 SSAOContrast = 0 (the game's
//                                       AMBIENT_OCCLUSION OFF values, ApplyAmbientOcclusion 39B20C)
//   XPOST  camera effects off           1: S+0x188C LensSimulationMotionBlur2Enable = 0 (MOTION_BLUR OFF, 39B4A4) + S+0x1700
//                                       LensSimulationDOFFlag = 0 (DEPTH_OF_FIELD OFF); 2: + S+0x30 BloomWeight = 0 (BLOOM OFF)
//   XPRE   preset                       0 game (all 0), 1 Performance, 2 Potato (sets the rows above; any change -> "custom")
//
// RenderConfig writes happen in samelook's RCO cave (RCO1 39B7CA, right after ApplyGraphicsSettings, before the renderer reads
// S). Per field: when the engine re-filled it since our last write the new engine value is the base; when it did not (env
// blocks that reload only on area change) the remembered base is used, so nothing compounds, and switching a level back to 0
// restores the remembered engine value if the engine has not re-filled it by then.
#pragma once
#include <cstddef>
#include <cstdint>

namespace extraperf {

enum Opt { kClouds, kFog, kGi, kShadowDist, kShadowFilter, kGeometry, kTerrain, kParticles, kAO, kPost, kNumOpts };
enum Preset { kPresetGame, kPresetPerformance, kPresetPotato, kNumPresets };

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;
};
void Configure(const StartInfo& si);  // reads [ExtraPerf]; installs nothing
bool Wanted();                        // [ExtraPerf] Enabled = 1: install the hooks (RCO + GIS + EPL + EPF groups)

// dllmain, after each group is written (nullptr / false = not installed)
void SetRcoInstalled();
void SetGiInstalled(uintptr_t giScaleLookup);           // original callee of the 4 GIS sites (187A50)
void SetLodInstalled(uintptr_t applyMeshletLodScales);  // original callee of EPL1 (2353C0)
void SetFxInstalled(void* simFraction, void* drawFraction);  // the two globals the EPF1 / EPF2 stores write

// ---- menu / remote (any thread; values are plain ints) ----
int Count();                         // kNumOpts
const char* Key(int opt);            // "XCLD" ... (menu tech id, ini key, LiveCtl key)
int Levels(int opt);                 // number of levels incl. 0
const char* LevelName(int opt, int lvl);
bool Available(int opt);             // its hook is installed this session
int Level(int opt);
void SetLevel(int opt, int lvl);     // clamps; logs "EXTRA: <key> -> <level>"
const char* PresetName(int p);
void ApplyPreset(int p);             // sets every option the preset lists (logs)
int MatchPreset();                   // the preset the current levels equal, -1 = custom
bool AnyAvailable();

// RCO cave (samelook.cpp RcoApplyC) - every tick, S = the frame's RenderConfig
void ApplyS(uint8_t* S);
// 30 s stats line ("" when nothing is installed)
void StatusLine(char* buf, unsigned n);

}  // namespace extraperf

// asm caves (extraperf.asm) and their data
extern "C" {
void caveGiScale();   // GIS1-4: call GiScaleLookup, xmm0 = max(xmm0 * ep_giMul, ep_giMin)
void caveMeshLod();   // EPL1: call ApplyMeshletLodScales(rcx, rdx), then ExtraPerfLodC(rcx)
void caveFxSim();     // EPF1: replaces movss [g_ViewGpuEffectSimFraction], xmm1  (stores xmm1 * ep_fxMul)
void caveFxDraw();    // EPF2: replaces movss [g_ViewGpuEffectDrawFraction], xmm2 (stores xmm2 * ep_fxMul)
void ExtraPerfLodC(uint8_t* obj);
extern uintptr_t ep_giOrig, ep_lodOrig, ep_pSimFrac, ep_pDrawFrac;
extern float ep_giMul, ep_giMin, ep_fxMul;
extern float ep_giLastIn, ep_giLastOut;  // caveGiScale read-back (stats line)
}
