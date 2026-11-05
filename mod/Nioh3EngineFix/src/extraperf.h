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
//   XRES   render size (#67206)         RenderDimsGet 5AC428 results at its 4 callers (XRS1-4: RenderResScaleApply 5AB9CC, XeSS
//                                       config, AO, 28A7B50) x 0.9 | 0.8 | 0.7 | 0.6: the engine's own dynamic-resolution viewport
//                                       path renders a smaller viewport of the fixed-size targets and the upscaler fills the output.
//                                       Ini key / preset only (no menu row).
//   XFOGS  fog grid scale (#67772)      S+0x13DC / S+0x13E0 x 0.75 | 0.5 (proportional, never raised, floor 16 x 8; ignored while
//                                       XFOG is set): the same froxel volume at fewer cells, sampled by the engine's own trilinear
//                                       froxel lookup (a smooth upsample of a low-frequency term)
//   XSSR   SSR at half size (#67772)    the RLR parameter object (lookup 2386FC([[4759910]+530], 0xEF3D849D)) +0xB8 = 1, the flag
//                                       ApplySsrQuality 39B0E8 sets for SSR LIGHTEST..STANDARD (the engine's own half-size trace);
//                                       2: + S+0x107C ray steps x 0.5. Written after ApplyGraphicsSettings every tick (RCO cave)
//   XPRE   preset                       0 game (all 0), 1 Performance, 2 Potato, 3 Risky, 4 Extreme (= Risky + XRES 3),
//                                       5 Extreme HQ (#67772: native render size; only the low-frequency passes cut: GI x0.35,
//                                       clouds 64 steps, 2 shadow taps, fog grid x0.75, SSR half size)
//   XSHARP FSR sharpness (#67772)       NOT a preset level. S+0x1F58, the float sharpness FsrConfigApply 2998B4C passes to the
//                                       FSR 3.1 upscale dispatch (RCAS strength; enableSharpening S+0x1F54 is always 1 there):
//                                       0 game (FSR_SHARPNESS), 1..5 = 0.0 / 0.25 / 0.5 / 0.75 / 1.0. Only the game's FSR3 path
//                                       reads it (XeSS / DLSS / OptiScaler FSR4 ignore it). Usable with XRES.
//   RenderDistance (#67213)            NOT a preset level. 0.50..1.00 (default 1.00 = stock), step 0.05, live. Scales the
//                                       return of GetDrawDistanceLimit 177274 and GetPlaceableDrawDistance 209EB0, the
//                                       character LOD limit in UpdateCharacterLodDistance 133D18 (xmm8 before comiss xmm6),
//                                       and foliage ViewDistance / ModelViewDistanceArray / LODDistanceArray /
//                                       ShadowAbsolutelyRenderDistanceH when the property table copies them. Presets do
//                                       not change it. Hooks install with ExtraPerf even at 1.00 (multiply by 1).
//
// RenderConfig writes happen in samelook's RCO cave (RCO1 39B7CA, right after ApplyGraphicsSettings, before the renderer reads
// S). Per field: when the engine re-filled it since our last write the new engine value is the base; when it did not (env
// blocks that reload only on area change) the remembered base is used, so nothing compounds, and switching a level back to 0
// restores the remembered engine value if the engine has not re-filled it by then.
#pragma once
#include <cstddef>
#include <cstdint>

namespace extraperf {

// #67772: kFogScale (XFOGS) and kSsr (XSSR) appended so the ini keys / menu rows of the first ten keep their order
enum Opt { kClouds, kFog, kGi, kShadowDist, kShadowFilter, kGeometry, kTerrain, kParticles, kAO, kPost, kFogScale, kSsr, kNumOpts };
enum Preset { kPresetGame, kPresetPerformance, kPresetPotato, kPresetRisky, kPresetExtreme, kPresetExtremeHQ, kNumPresets };
constexpr int kResLevels = 5;   // #67206 XRES: 0 game, 1 = x0.9, 2 = x0.8, 3 = x0.7, 4 = x0.6 of the render size

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
void SetResInstalled(uintptr_t renderDimsGet);           // #67206 original callee of the 4 XRS sites (5AC428)
int ResLevel();
bool ResAvailable();
void SetResLevel(int lvl);           // live; logs

// #67225 Render distance. Percent 50..100 (100 = stock). Presets do not read or write it.
int RdPct();
float RdMul();
void SetRdPct(int pct);              // clamps and snaps to 5 (0.05). 50 = half distance
void StepRd(int dir);                // -1 / +1 step, 0 = back to 100
bool RdAvailable();
void SetRdDraw(uintptr_t getDrawDistanceLimit);
void SetRdPlace(uintptr_t getPlaceableDrawDistance);
void SetRdChar(uintptr_t fallThrough, uintptr_t taken);
void SetRdProp(uintptr_t propMemcpy);

// #67772 XSSR: the RLR parameter-object lookup ApplySsrQuality uses (2386FC(table, key)) and the global whose +0x530 is the
// table (4759910). Both null = XSSR level 1/2 only scales the ray steps (level 2) and the half-size flag is not written.
void SetSsrInstalled(void* const* mgrGlobal, uintptr_t lookup);
bool SsrFlagAvailable();
// #67772 XSHARP: FSR sharpness override. Not a preset field (read after the preset; ApplyPreset never touches it).
constexpr int kSharpLevels = 6;      // 0 game, 1..5 = 0.0 / 0.25 / 0.5 / 0.75 / 1.0
int SharpLevel();
void SetSharpLevel(int lvl);         // clamps; logs
const char* SharpName(int lvl);
bool SharpAvailable();               // = the RCO cave is installed

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
void caveRenderDims();  // XRS1-4: call RenderDimsGet(rcx = out), then ExtraPerfResC(out): {w, h} x ep_resMul
void ExtraPerfResC(uint32_t* dims);
extern uintptr_t ep_resOrig;
extern float ep_resMul;
void caveDrawLimit();   // RDD1-5: xmm0 = GetDrawDistanceLimit() * ep_rdMul
void cavePlaceDist();   // RDP1-10: xmm0 = GetPlaceableDrawDistance() * ep_rdMul
void caveCharLod();     // RDC1: jmp cave, scales xmm8 then repeats comiss/ja
void cavePropDist();    // RDF1: property memcpy, then scale the distance keys in the destination
void* ExtraPerfPropC(void* dst, const void* src, size_t n, uint32_t key);
extern uintptr_t ep_drawOrig, ep_placeOrig, ep_propOrig, ep_charFall, ep_charTake;
extern float ep_rdMul;
void ExtraPerfLodC(uint8_t* obj);
extern uintptr_t ep_giOrig, ep_lodOrig, ep_pSimFrac, ep_pDrawFrac;
extern float ep_giMul, ep_giMin, ep_fxMul;
extern float ep_giLastIn, ep_giLastOut;  // caveGiScale read-back (stats line)
}
