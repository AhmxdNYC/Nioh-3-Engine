// extraperf.cpp - "Extra performance (changes the look)" options (1.7.59). See extraperf.h and
// notes\extra-performance-settings.md.
#define WIN32_LEAN_AND_MEAN
#include "extraperf.h"

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

// asm cave data (extraperf.asm reads these RIP-relatively)
extern "C" {
uintptr_t ep_giOrig = 0, ep_lodOrig = 0, ep_pSimFrac = 0, ep_pDrawFrac = 0;
float ep_giMul = 1.0f, ep_giMin = 0.0f, ep_fxMul = 1.0f;
float ep_giLastIn = 0.0f, ep_giLastOut = 0.0f;  // caveGiScale: the last GiScaleLookup result and what the cave returned (stats read-back)
// #67206 XRES (caveRenderDims): RenderDimsGet 5AC428 original, the render-size multiplier, last dims in / out (stats read-back)
uintptr_t ep_resOrig = 0;
float ep_resMul = 1.0f;
volatile uint32_t ep_resInW = 0, ep_resInH = 0, ep_resOutW = 0, ep_resOutH = 0;
volatile uint64_t ep_resCalls = 0;
// #67225 RenderDistance (caves in extraperf.asm). 1.0 = stock. Presets never write this.
float ep_rdMul = 1.0f;
uintptr_t ep_drawOrig = 0, ep_placeOrig = 0, ep_propOrig = 0, ep_charFall = 0, ep_charTake = 0;
volatile uint64_t ep_propCalls = 0;
}

namespace extraperf {
namespace {

void (*g_log)(const char*, ...) = nullptr;
#define XLOG(...) \
    do {                              \
        if (g_log) g_log(__VA_ARGS__); \
    } while (0)

struct OptDef {
    const char* key;
    int levels;
    const char* names[5];
};
const OptDef kOpt[kNumOpts] = {
    {"XCLD", 4, {"game", "128 steps", "64 steps", "32 steps"}},
    {"XFOG", 4, {"game", "64x24 cells", "48x16 cells", "off"}},
    {"XGI", 5, {"game", "75 %", "50 %", "35 %", "25 %"}},
    {"XSHD", 4, {"game", "60 %", "35 %", "20 %"}},
    {"XSHF", 3, {"game", "2 taps", "1 tap"}},
    {"XLOD", 4, {"game", "x1.5 coarser", "x2 coarser", "x3 coarser"}},
    {"XTER", 4, {"game", "70 %", "40 %", "20 %"}},
    {"XPRT", 5, {"game", "75 %", "50 %", "25 %", "off"}},
    {"XAO", 2, {"game", "off"}},
    {"XPOST", 3, {"game", "blur + DOF off", "+ bloom off"}},
    {"XFOGS", 3, {"game", "75 % grid", "50 % grid"}},              // #67772
    {"XSSR", 3, {"game", "half size", "half size + 50 % steps"}},  // #67772
};
// presets: level per option (kClouds .. kPost)
const int kPresetLv[kNumPresets][kNumOpts] = {
    // XCLD XFOG XGI XSHD XSHF XLOD XTER XPRT XAO XPOST XFOGS XSSR - tuned on the PC measurements (notes\extra-performance-settings.md 3):
    // XTER (visible flatter ground, no gain), XPRT (no measurable effect) and XPOST (no gain) are left to the player.
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  // game
    {2, 0, 2, 1, 0, 1, 0, 0, 0, 0, 0, 0},  // Optimized with effect visuals: clouds, GI, shadow distance, LOD only; fog, particles, AO and camera effects stay
    {3, 2, 3, 3, 2, 2, 0, 0, 1, 1, 0, 0},  // Optimized: + coarser fog grid (glow kept: fog off was a big look change for +1.3 %), AO off, 1-tap shadows, blur / DOF off
    // #66661 Risky (max speed, the picture may change): every option at its strongest level - fog off, GI x0.25, LOD x3, terrain
    // x0.2, particles x0.25, AO off, blur / DOF / bloom off. With Preset = 3 in the ini, [ExtraPerf] XHALF defaults to 2 (skipfx.cpp:
    // GI gathers, volumetric clouds and fog scatter computed every 2nd frame; needs a restart, it installs at start).
    // #67915: XFOG 2 (48x16 cells) instead of 3 (fog off): fog off zeroes the froxel scatter the sky / atmosphere is lit through,
    // which drew the sky brown (human #67829 / #67854). The sky-light step is never skipped in any preset.
    {3, 2, 4, 3, 2, 3, 3, 3, 1, 2, 0, 0},
    // #67206 Extreme (human #67200: >= +20 %): Risky + XRES 3 = the internal render size x0.7 through the engine's own
    // dynamic-resolution viewport path (Ally REN_RAT 50 -> 35 %; model ~ -4.4 ms of 28.7, notesig-wins-gx-27597.md).
    {3, 2, 4, 3, 2, 3, 3, 3, 1, 2, 0, 0},  // #67915: XFOG 2, see Risky
    // #67772 Extreme HQ (human #67772: Extreme's gain at a near-1:1 picture): native render size, geometry / textures / UI / post
    // untouched; only the low-frequency passes run cheaper, each through the engine's own reduced-resolution path and upsample:
    // GI + reflection probe gathers x0.35 (sub-resolution gather + the engine's bilateral GI composite; PC look = noise, +3.6 %),
    // volumetric clouds 64 steps (already render/8 + Bayer-reprojected), sun-shadow filter 2 taps, fog froxel grid x0.75 per axis
    // (trilinear froxel lookup), SSR half size (the engine's SSR STANDARD trace). Kept: fog / AO / blur / bloom / particles / LOD /
    // terrain / shadow distance. No XHALF (strobe risk, #216) and no XRES. notes\extreme-hq.md.
    {2, 0, 3, 0, 1, 0, 0, 0, 0, 0, 1, 1},
};
// #67206 XRES level per preset (not a menu row; [ExtraPerf] XRES in the ini, a preset wins)
const int kPresetRes[kNumPresets] = {0, 0, 0, 0, 3, 0};
const float kResMul[kResLevels] = {1.0f, 0.9f, 0.8f, 0.7f, 0.6f};
volatile LONG g_res = 0;
bool g_resInst = false;
bool g_rdChars = false;  // GH #142: [ExtraPerf] RenderDistanceCharacters (default 0)
volatile LONG g_rdPct = 100;  // #67225: 50..100, step 5. 100 = the game's distance. Not part of any preset.
bool g_rdDraw = false, g_rdPlace = false, g_rdChar = false, g_rdProp = false;
// #43269: the Optimizations preset row. Custom is not a level row; PresetName returns it when the XCLD-style levels match none of these.
const char* const kPresetName[kNumPresets] = {"game (all off)", "Optimized with effect visuals", "Optimized", "Risky (max speed)",
                                              "Extreme (lower picture quality, more fps)", "Extreme HQ (native res)"};

bool g_wanted = false;
volatile LONG g_level[kNumOpts] = {};
bool g_rco = false, g_gi = false, g_lod = false, g_fx = false;

// level -> value tables
const int kCloudMax[4] = {0, 128, 64, 32}, kCloudMin[4] = {0, 24, 16, 8};
const int kFogX[4] = {0, 64, 48, 0}, kFogZ[4] = {0, 24, 16, 0};
const float kGiMul[5] = {1.0f, 0.75f, 0.5f, 0.35f, 0.25f};
const float kShdMul[4] = {1.0f, 0.6f, 0.35f, 0.2f};
const int kShf[3] = {0, 2, 1};
const float kLodMul[4] = {1.0f, 1.5f, 2.0f, 3.0f};
const float kTerMul[4] = {1.0f, 0.7f, 0.4f, 0.2f};
const float kFxMul[5] = {1.0f, 0.75f, 0.5f, 0.25f, 0.0f};
const float kFogScaleMul[3] = {1.0f, 0.75f, 0.5f};  // #67772 XFOGS
// #67772 XSHARP (non-preset): FSR sharpness written to S+0x1F58; level 0 = the game's FSR_SHARPNESS
const float kSharpVal[kSharpLevels] = {0.0f, 0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
const char* const kSharpName[kSharpLevels] = {"game", "0.00", "0.25", "0.50", "0.75", "1.00 (game max)"};
volatile LONG g_sharp = 0;
// #67772 XSSR half-size flag: RLR parameter object via the engine's own lookup (RCO thread only)
using SsrLookupFn = void* (*)(void* table, uint32_t key);
void* const* g_ssrGlobal = nullptr;
SsrLookupFn g_ssrLookup = nullptr;
volatile uint64_t c_ssrFlagWrites = 0, c_ssrNoObj = 0;
int g_ssrFlagLogged = 0;

// ---- one RenderConfig field override (RCO thread only) ----
// base = the engine's value: taken fresh whenever the field no longer holds what we wrote (the engine re-filled it);
// kept otherwise, so a scaled value never compounds. Off: put the base back if nobody re-filled the field.
struct Field {
    uint32_t off;
    const char* name;
    bool flt;           // float field (else int) - for the log line only
    bool wrote = false;
    int logs = 0;       // value-change log lines since the override started (max 4)
    uint32_t last = 0;  // bits we wrote
    uint32_t base = 0;  // engine bits
    uint64_t writes = 0, refills = 0;
};
enum FieldId { fCloudMax, fCloudMin, fFogX, fFogZ, fFogScatter, fEraseEnd, fEraseStart, fShadowTaps, fTessDist, fTessRate,
               fAoRays, fAoSteps, fAoContrast, fMb, fDof, fBloom, fSsrSteps, fSharp, kNumFields };
Field g_f[kNumFields] = {{0x410, "VolumetricCloudMaxRaySampleNum", false}, {0x40C, "VolumetricCloudMinRaySampleNum", false},
                         {0x13DC, "VolumetricFogResolutionX", false}, {0x13E0, "VolumetricFogResolutionZ", false},
                         {0x1398, "VolumetricFogScatterScale", true}, {0xAF4, "ShadowEraseFarEnd", true},
                         {0xAF8, "ShadowEraseFarStart", true}, {0xB44, "ShadowSampleNum", false},
                         {0x1970, "TerrainTessellationMaxDistance", true}, {0x1978, "TerrainTessellationDistanceAdaptRate", true},
                         {0x12F4, "SSAORayNum", false}, {0x12F8, "SSAOStepNum", false}, {0x12D8, "SSAOContrast", true},
                         {0x188C, "LensSimulationMotionBlur2Enable", false}, {0x1700, "LensSimulationDOFFlag", false},
                         {0x30, "BloomWeight", true}, {0x107C, "RLRRaySteps", false}, {0x1F58, "FsrSharpness", true}};

// "S+410 VolumetricCloudMaxRaySampleNum 256" (value as the field's type)
void FieldText(const Field& f, uint32_t bits, char* buf, size_t n) {
    float x;
    memcpy(&x, &bits, 4);
    if (f.flt) snprintf(buf, n, "%g", (double)x);
    else snprintf(buf, n, "%d", (int)bits);
}

inline uint32_t Bits(float f) {
    uint32_t u;
    memcpy(&u, &f, 4);
    return u;
}
inline float Flt(uint32_t u) {
    float f;
    memcpy(&f, &u, 4);
    return f;
}

// returns the engine base for this tick (to compute the new value from); call Put() or Off() after
uint32_t Base(uint8_t* S, Field& f) {
    const uint32_t cur = *reinterpret_cast<uint32_t*>(S + f.off);
    if (!f.wrote || cur != f.last) {
        if (f.wrote) ++f.refills;
        f.base = cur;
    }
    return f.base;
}
void Put(uint8_t* S, Field& f, uint32_t v) {
    if ((!f.wrote || v != f.last) && f.logs < 4) {  // override starts / its value changes: engine value -> written value (max 4 lines)
        ++f.logs;
        char a[24], b[24];
        FieldText(f, f.base, a, sizeof a);
        FieldText(f, v, b, sizeof b);
        XLOG("EXTRA: S+%X %s: engine %s -> written %s", f.off, f.name, a, b);
    }
    *reinterpret_cast<uint32_t*>(S + f.off) = v;
    f.last = v;
    f.wrote = true;
    ++f.writes;
}
void Off(uint8_t* S, Field& f) {
    if (!f.wrote) return;
    uint32_t* p = reinterpret_cast<uint32_t*>(S + f.off);
    if (*p == f.last) *p = f.base;  // not re-filled since our last write: give the engine its value back
    f.wrote = false;
    f.logs = 0;
    char a[24];
    FieldText(f, *p, a, sizeof a);
    XLOG("EXTRA: S+%X %s: override off (engine value %s)", f.off, f.name, a);
}
void PutMinInt(uint8_t* S, Field& f, int v) {
    const int b = (int)Base(S, f);
    Put(S, f, (uint32_t)(b < v ? b : v));
}
void PutMulFloat(uint8_t* S, Field& f, float m) { Put(S, f, Bits(Flt(Base(S, f)) * m)); }
// #67772 XFOGS / XSSR: int x m, rounded, never raised, at least `lo` when the engine value is >= lo (0 stays 0)
void PutMulInt(uint8_t* S, Field& f, float m, int lo) {
    const int b = (int)Base(S, f);
    int v = (int)((float)b * m + 0.5f);
    if (v < lo) v = b < lo ? b : lo;
    if (v > b) v = b;
    Put(S, f, (uint32_t)v);
}
void PutConst(uint8_t* S, Field& f, uint32_t v) {
    Base(S, f);
    Put(S, f, v);
}

// meshlet LOD floats (game-tick thread only)
struct LodState {
    bool wrote = false;
    float last[8] = {}, base[8] = {};
    uint64_t ticks = 0;
} g_lodState;

volatile uint64_t c_rcoTicks = 0, c_lodTicks = 0;

void UpdateCaveData() {
    const int gi = g_level[kGi], fx = g_level[kParticles];
    ep_giMul = kGiMul[gi];
    ep_giMin = gi ? 0.1f : 0.0f;
    ep_fxMul = kFxMul[fx];
    const int r = g_res;
    ep_resMul = kResMul[r >= 0 && r < kResLevels ? r : 0];
    int pct = (int)g_rdPct;
    if (pct < 50) pct = 50;
    if (pct > 100) pct = 100;
    ep_rdMul = (float)pct / 100.0f;
}

int SnapRd(int pct) {
    if (pct < 50) pct = 50;
    if (pct > 100) pct = 100;
    pct = ((pct + 2) / 5) * 5;
    if (pct < 50) pct = 50;
    if (pct > 100) pct = 100;
    return pct;
}

}  // namespace

void Configure(const StartInfo& si) {
    g_log = si.log;
    const wchar_t* ini = si.iniPath;
    g_wanted = GetPrivateProfileIntW(L"ExtraPerf", L"Enabled", 1, ini) == 1;
    const int preset = (int)GetPrivateProfileIntW(L"ExtraPerf", L"Preset", 0, ini);
    static const wchar_t* const kIniKey[kNumOpts] = {L"XCLD", L"XFOG", L"XGI", L"XSHD", L"XSHF", L"XLOD",
                                                     L"XTER", L"XPRT", L"XAO", L"XPOST", L"XFOGS", L"XSSR"};  // = kOpt[].key
    for (int o = 0; o < kNumOpts; ++o) {
        int v = (int)GetPrivateProfileIntW(L"ExtraPerf", kIniKey[o], 0, ini);
        if (v < 0 || v >= kOpt[o].levels) v = 0;
        g_level[o] = v;
    }
    int res = (int)GetPrivateProfileIntW(L"ExtraPerf", L"XRES", 0, ini);
    g_res = res < 0 || res >= kResLevels ? 0 : res;
    if (preset > 0 && preset < kNumPresets) {  // a preset in the ini wins over the per-option keys
        for (int o = 0; o < kNumOpts; ++o) g_level[o] = kPresetLv[preset][o];
        g_res = kPresetRes[preset];
    }
    // #67772 XSHARP is not a preset field either (so Preset = 4 + XSHARP = n is the 'Extreme + sharpen' arm)
    const int sh = (int)GetPrivateProfileIntW(L"ExtraPerf", L"XSHARP", 0, ini);
    g_sharp = sh < 0 || sh >= kSharpLevels ? 0 : sh;
    // RenderDistance is not a preset field. Read it after the preset so Extreme cannot overwrite the slider.
    wchar_t rdBuf[32];
    GetPrivateProfileStringW(L"ExtraPerf", L"RenderDistance", L"1.00", rdBuf, 32, ini);
    wchar_t* rdEnd = nullptr;
    const double rd = wcstod(rdBuf, &rdEnd);
    int pct = 100;
    if (rdEnd != rdBuf && rd == rd) pct = (int)(rd * 100.0 + (rd >= 0 ? 0.5 : -0.5));
    g_rdPct = SnapRd(pct);
    // GH #142: the character limit at UpdateCharacterLodDistance is the actor ACTIVATION distance (+0xAE14), not only a
    // draw distance. Scaled to 0.50, a far camera (battle-scroll / cutscene camera) switched the player's own character off
    // and it stayed invisible. Characters keep the game's distance unless the ini asks for the old behaviour.
    g_rdChars = GetPrivateProfileIntW(L"ExtraPerf", L"RenderDistanceCharacters", 0, ini) != 0;
    UpdateCaveData();
}

bool Wanted() { return g_wanted; }
bool RdChars() { return g_rdChars; }

void SetRcoInstalled() {
    g_rco = true;
    XLOG("EXTRA: render-config options installed (XCLD %d, XFOG %d, XSHD %d, XSHF %d, XTER %d, XAO %d, XPOST %d, XFOGS %d, XSSR %d, "
         "XSHARP %d = %s)",
         (int)g_level[kClouds], (int)g_level[kFog], (int)g_level[kShadowDist], (int)g_level[kShadowFilter], (int)g_level[kTerrain],
         (int)g_level[kAO], (int)g_level[kPost], (int)g_level[kFogScale], (int)g_level[kSsr], (int)g_sharp, kSharpName[g_sharp]);
}
void SetGiInstalled(uintptr_t orig) {
    ep_giOrig = orig;
    g_gi = orig != 0;
    XLOG("EXTRA: GI gather scale installed (XGI %d = x%.2f)", (int)g_level[kGi], (double)ep_giMul);
}
void SetLodInstalled(uintptr_t orig) {
    ep_lodOrig = orig;
    g_lod = orig != 0;
    XLOG("EXTRA: meshlet LOD scale installed (XLOD %d = x%.1f)", (int)g_level[kGeometry], (double)kLodMul[g_level[kGeometry]]);
}
void SetResInstalled(uintptr_t orig) {
    ep_resOrig = orig;
    g_resInst = orig != 0;
    XLOG("EXTRA: render size scale installed (XRES %d = x%.2f; RenderDimsGet %p, the engine's dynamic-resolution viewport path)",
         (int)g_res, (double)ep_resMul, (void*)orig);
}
void SetSsrInstalled(void* const* mgrGlobal, uintptr_t lookup) {
    g_ssrGlobal = mgrGlobal;
    g_ssrLookup = reinterpret_cast<SsrLookupFn>(lookup);
    XLOG("EXTRA: SSR half-size flag %s (XSSR %d; RLR object lookup %p, table global %p)", SsrFlagAvailable() ? "installed" : "NOT available",
         (int)g_level[kSsr], (void*)lookup, (const void*)mgrGlobal);
}
bool SsrFlagAvailable() { return g_ssrGlobal && g_ssrLookup; }
int SharpLevel() { return (int)g_sharp; }
const char* SharpName(int l) { return l >= 0 && l < kSharpLevels ? kSharpName[l] : "?"; }
bool SharpAvailable() { return g_rco; }
void SetSharpLevel(int l) {
    if (l < 0) l = 0;
    if (l >= kSharpLevels) l = kSharpLevels - 1;
    const LONG old = InterlockedExchange(&g_sharp, l);
    if (old != l) XLOG("EXTRA: XSHARP -> %d (FSR sharpness %s)%s", l, kSharpName[l], g_rco ? "" : " - not installed");
}
int ResLevel() { return (int)g_res; }
bool ResAvailable() { return g_resInst; }
void SetResLevel(int l) {
    if (l < 0) l = 0;
    if (l >= kResLevels) l = kResLevels - 1;
    const LONG old = InterlockedExchange(&g_res, l);
    UpdateCaveData();
    if (old != l) XLOG("EXTRA: XRES -> %d (render size x%.2f)%s", l, (double)kResMul[l], g_resInst ? "" : " - not installed");
}
int RdPct() { return (int)g_rdPct; }
float RdMul() { return ep_rdMul; }
void SetRdPct(int pct) {
    pct = SnapRd(pct);
    const LONG old = InterlockedExchange(&g_rdPct, pct);
    UpdateCaveData();
    if (old != pct) XLOG("EXTRA: RenderDistance -> %.2f%s", (double)ep_rdMul, RdAvailable() ? "" : " - not installed");
}
void StepRd(int dir) {
    if (dir == 0) SetRdPct(100);
    else SetRdPct((int)g_rdPct + (dir > 0 ? 5 : -5));
}
bool RdAvailable() { return g_rdDraw || g_rdPlace || g_rdChar || g_rdProp; }
void SetRdDraw(uintptr_t orig) {
    ep_drawOrig = orig;
    g_rdDraw = orig != 0;
    XLOG("EXTRA: entity draw distance installed (x%.2f, GetDrawDistanceLimit %p)", (double)ep_rdMul, (void*)orig);
}
void SetRdPlace(uintptr_t orig) {
    ep_placeOrig = orig;
    g_rdPlace = orig != 0;
    XLOG("EXTRA: placeable draw distance installed (x%.2f, GetPlaceableDrawDistance %p)", (double)ep_rdMul, (void*)orig);
}
void SetRdChar(uintptr_t fallThrough, uintptr_t taken) {
    ep_charFall = fallThrough;
    ep_charTake = taken;
    g_rdChar = fallThrough != 0 && taken != 0;
    XLOG("EXTRA: character LOD distance installed (x%.2f, fall %p take %p)", (double)ep_rdMul, (void*)fallThrough, (void*)taken);
}
void SetRdProp(uintptr_t orig) {
    ep_propOrig = orig;
    g_rdProp = orig != 0;
    XLOG("EXTRA: foliage view distance installed (x%.2f, property copy %p)", (double)ep_rdMul, (void*)orig);
}
void SetFxInstalled(void* sim, void* draw) {
    ep_pSimFrac = (uintptr_t)sim;
    ep_pDrawFrac = (uintptr_t)draw;
    g_fx = sim && draw;
    XLOG("EXTRA: GPU particle share installed (XPRT %d = x%.2f; globals %p / %p)", (int)g_level[kParticles], (double)ep_fxMul,
         sim, draw);
}

int Count() { return kNumOpts; }
const char* Key(int o) { return o >= 0 && o < kNumOpts ? kOpt[o].key : ""; }
int Levels(int o) { return o >= 0 && o < kNumOpts ? kOpt[o].levels : 1; }
const char* LevelName(int o, int l) {
    if (o < 0 || o >= kNumOpts || l < 0 || l >= kOpt[o].levels) return "?";
    return kOpt[o].names[l];
}
bool Available(int o) {
    switch (o) {
        case kGi: return g_gi;
        case kGeometry: return g_lod;
        case kParticles: return g_fx;
        default: return g_rco && o >= 0 && o < kNumOpts;
    }
}
bool AnyAvailable() { return g_rco || g_gi || g_lod || g_fx || g_resInst; }
int Level(int o) { return o >= 0 && o < kNumOpts ? (int)g_level[o] : 0; }
void SetLevel(int o, int l) {
    if (o < 0 || o >= kNumOpts) return;
    if (l < 0) l = 0;
    if (l >= kOpt[o].levels) l = kOpt[o].levels - 1;
    const LONG old = InterlockedExchange(&g_level[o], l);
    UpdateCaveData();
    if (old != l) XLOG("EXTRA: %s -> %d (%s)%s", kOpt[o].key, l, kOpt[o].names[l], Available(o) ? "" : " - not installed");
}
const char* PresetName(int p) { return p >= 0 && p < kNumPresets ? kPresetName[p] : "custom"; }
void ApplyPreset(int p) {
    if (p < 0 || p >= kNumPresets) return;
    for (int o = 0; o < kNumOpts; ++o) InterlockedExchange(&g_level[o], kPresetLv[p][o]);
    InterlockedExchange(&g_res, kPresetRes[p]);
    UpdateCaveData();
    XLOG("EXTRA: preset %s", kPresetName[p]);
}
int MatchPreset() {
    for (int p = 0; p < kNumPresets; ++p) {
        bool eq = g_res == kPresetRes[p];
        for (int o = 0; o < kNumOpts && eq; ++o) eq = g_level[o] == kPresetLv[p][o];
        if (eq) return p;
    }
    return -1;
}

void ApplyS(uint8_t* S) {
    if (!S || !g_rco) return;
    c_rcoTicks = c_rcoTicks + 1;
    // XCLD
    if (const int l = g_level[kClouds]) {
        PutMinInt(S, g_f[fCloudMax], kCloudMax[l]);
        PutMinInt(S, g_f[fCloudMin], kCloudMin[l]);
    } else {
        Off(S, g_f[fCloudMax]);
        Off(S, g_f[fCloudMin]);
    }
    // XFOG (3 = the game's VOLUMETRIC LIGHTEST: no froxels, ScatterScale 0)
    if (const int l = g_level[kFog]) {
        if (l == 3) {
            PutConst(S, g_f[fFogX], 0);
            PutConst(S, g_f[fFogZ], 0);
            PutConst(S, g_f[fFogScatter], 0);
        } else {
            PutMinInt(S, g_f[fFogX], kFogX[l]);
            PutMinInt(S, g_f[fFogZ], kFogZ[l]);
            Off(S, g_f[fFogScatter]);
        }
    } else if (const int fs = g_level[kFogScale]) {  // #67772 XFOGS: proportional grid (XFOG wins when both are set)
        PutMulInt(S, g_f[fFogX], kFogScaleMul[fs], 16);
        PutMulInt(S, g_f[fFogZ], kFogScaleMul[fs], 8);
        Off(S, g_f[fFogScatter]);
    } else {
        Off(S, g_f[fFogX]);
        Off(S, g_f[fFogZ]);
        Off(S, g_f[fFogScatter]);
    }
    // XSHD
    if (const int l = g_level[kShadowDist]) {
        PutMulFloat(S, g_f[fEraseEnd], kShdMul[l]);
        PutMulFloat(S, g_f[fEraseStart], kShdMul[l]);
    } else {
        Off(S, g_f[fEraseEnd]);
        Off(S, g_f[fEraseStart]);
    }
    // XSHF
    if (const int l = g_level[kShadowFilter]) PutMinInt(S, g_f[fShadowTaps], kShf[l]);
    else Off(S, g_f[fShadowTaps]);
    // XTER
    if (const int l = g_level[kTerrain]) {
        PutMulFloat(S, g_f[fTessDist], kTerMul[l]);
        PutMulFloat(S, g_f[fTessRate], kTerMul[l]);
    } else {
        Off(S, g_f[fTessDist]);
        Off(S, g_f[fTessRate]);
    }
    // XAO
    if (g_level[kAO]) {
        PutConst(S, g_f[fAoRays], 0);
        PutConst(S, g_f[fAoSteps], 0);
        PutConst(S, g_f[fAoContrast], 0);
    } else {
        Off(S, g_f[fAoRays]);
        Off(S, g_f[fAoSteps]);
        Off(S, g_f[fAoContrast]);
    }
    // XPOST
    const int post = g_level[kPost];
    if (post >= 1) {
        PutConst(S, g_f[fMb], 0);
        PutConst(S, g_f[fDof], 0);
    } else {
        Off(S, g_f[fMb]);
        Off(S, g_f[fDof]);
    }
    if (post >= 2) PutConst(S, g_f[fBloom], 0);
    else Off(S, g_f[fBloom]);
    // #67772 XSSR: half-size SSR trace (the flag ApplySsrQuality just wrote for this frame; it rewrites it every tick, so
    // switching off needs no restore) + level 2: ray steps x0.5 (S+0x107C, refilled and scaled by the engine every tick)
    if (const int l = g_level[kSsr]) {
        if (g_ssrGlobal && g_ssrLookup) {
            void* mgr = *g_ssrGlobal;
            void* table = mgr ? *reinterpret_cast<void* const*>(static_cast<uint8_t*>(mgr) + 0x530) : nullptr;
            uint8_t* obj = table ? static_cast<uint8_t*>(g_ssrLookup(table, 0xEF3D849Du)) : nullptr;
            if (obj) {
                uint32_t* flag = reinterpret_cast<uint32_t*>(obj + 0xB8);
                if (*flag != 1u && g_ssrFlagLogged < 2) {
                    ++g_ssrFlagLogged;
                    XLOG("EXTRA: SSR half-size flag (RLR object %p +B8): engine %u -> written 1", (void*)obj, *flag);
                }
                *flag = 1u;
                c_ssrFlagWrites = c_ssrFlagWrites + 1;
            } else {
                c_ssrNoObj = c_ssrNoObj + 1;
            }
        }
        if (l >= 2) PutMulInt(S, g_f[fSsrSteps], 0.5f, 1);
        else Off(S, g_f[fSsrSteps]);
    } else {
        g_ssrFlagLogged = 0;
        Off(S, g_f[fSsrSteps]);
    }
    // #67772 XSHARP: FSR 3.1 sharpness (FsrConfigApply writes S+0x1F58 only when the upscaler settings change; Base() keeps
    // the game's value as the base and Off() gives it back)
    if (const int sh = g_sharp) PutConst(S, g_f[fSharp], Bits(kSharpVal[sh]));
    else Off(S, g_f[fSharp]);
}

void StatusLine(char* buf, unsigned n) {
    buf[0] = 0;
    if (!n || (!AnyAvailable() && !RdAvailable())) return;
    static uint64_t pr = 0, pl = 0;
    const uint64_t r = c_rcoTicks, l = c_lodTicks;
    int k = snprintf(buf, n, "stats/30s EXTRA: levels");
    for (int o = 0; o < kNumOpts && k > 0 && (unsigned)k < n; ++o)
        k += snprintf(buf + k, n - k, " %s=%d%s", kOpt[o].key, (int)g_level[o], Available(o) ? "" : "(n/a)");
    if (k > 0 && (unsigned)k < n) {
        const int m = MatchPreset();
        k += snprintf(buf + k, n - k, " preset %s; RCO ticks %llu, LOD ticks %llu; fields written/refilled:", PresetName(m),
                      (unsigned long long)(r - pr), (unsigned long long)(l - pl));
    }
    for (int i = 0; i < kNumFields && k > 0 && (unsigned)k < n; ++i)
        if (g_f[i].writes)
            k += snprintf(buf + k, n - k, " %X %llu/%llu", g_f[i].off, (unsigned long long)g_f[i].writes,
                          (unsigned long long)g_f[i].refills);
    // read-backs of the non-render-config hooks: what the engine actually got
    if (k > 0 && (unsigned)k < n && g_gi)
        k += snprintf(buf + k, n - k, "; GI scale engine %.3f -> used %.3f", (double)ep_giLastIn, (double)ep_giLastOut);
    if (k > 0 && (unsigned)k < n && g_fx && ep_pSimFrac && ep_pDrawFrac)
        k += snprintf(buf + k, n - k, "; GPU particle fractions sim %.3f draw %.3f", (double)*reinterpret_cast<float*>(ep_pSimFrac),
                      (double)*reinterpret_cast<float*>(ep_pDrawFrac));
    if (k > 0 && (unsigned)k < n && (g_sharp || g_level[kSsr]))
        k += snprintf(buf + k, n - k, "; XSHARP %d (%s); SSR half-size flag writes %llu (no object %llu)", (int)g_sharp,
                      kSharpName[g_sharp], (unsigned long long)c_ssrFlagWrites, (unsigned long long)c_ssrNoObj);
    if (k > 0 && (unsigned)k < n && g_resInst)
        k += snprintf(buf + k, n - k, "; XRES %d x%.2f render size %ux%u -> %ux%u (%llu calls)", (int)g_res, (double)ep_resMul,
                      (unsigned)ep_resInW, (unsigned)ep_resInH, (unsigned)ep_resOutW, (unsigned)ep_resOutH,
                      (unsigned long long)ep_resCalls);
    if (k > 0 && (unsigned)k < n && RdAvailable())
        k += snprintf(buf + k, n - k, "; render distance x%.2f (draw %d place %d char %d foliage %d, %llu foliage copies)",
                      (double)ep_rdMul, g_rdDraw ? 1 : 0, g_rdPlace ? 1 : 0, g_rdChar ? 1 : 0, g_rdProp ? 1 : 0,
                      (unsigned long long)ep_propCalls);
    if (k > 0 && (unsigned)k < n && g_lod && g_lodState.wrote)
        k += snprintf(buf + k, n - k, "; meshlet LOD scale[0] engine %.3f -> %.3f", (double)g_lodState.base[0], (double)g_lodState.last[0]);
    pr = r;
    pl = l;
}

}  // namespace extraperf

using namespace extraperf;

// XRS1-4 (caveRenderDims): after RenderDimsGet 5AC428 wrote {w, h} (render thread / settings apply). x ep_resMul, even, >= 64.
extern "C" void ExtraPerfResC(uint32_t* d) {
    if (!d) return;
    ep_resCalls = ep_resCalls + 1;
    const float m = ep_resMul;
    const uint32_t w = d[0], h = d[1];
    ep_resInW = w;
    ep_resInH = h;
    if (m < 1.0f && m > 0.2f && w >= 128 && h >= 128 && w <= 16384 && h <= 16384) {
        uint32_t nw = (uint32_t)((float)w * m) & ~1u, nh = (uint32_t)((float)h * m) & ~1u;
        if (nw < 64) nw = 64;
        if (nh < 64) nh = 64;
        d[0] = nw;
        d[1] = nh;
    }
    ep_resOutW = d[0];
    ep_resOutH = d[1];
}

// EPL1: after ApplyMeshletLodScales wrote the 8 category scales at obj+0xA108 (game-tick thread)
extern "C" void ExtraPerfLodC(uint8_t* obj) {
    if (!obj) return;
    c_lodTicks = c_lodTicks + 1;
    float* v = reinterpret_cast<float*>(obj + 0xA108);
    LodState& s = g_lodState;
    const int l = g_level[kGeometry];
    if (!l) {
        if (s.wrote)
            for (int i = 0; i < 8; ++i)
                if (v[i] == s.last[i]) v[i] = s.base[i];  // not re-written by the engine since: restore
        s.wrote = false;
        return;
    }
    const float m = kLodMul[l];
    for (int i = 0; i < 8; ++i) {
        if (!s.wrote || v[i] != s.last[i]) s.base[i] = v[i];  // the engine wrote a fresh value this tick
        float x = s.base[i] * m;
        if (x > 10.0f) x = 10.0f;  // the engine's own cap (categories 0-3 and 6)
        if (s.base[i] > 10.0f) x = s.base[i];  // never lower a value the engine set above its cap
        v[i] = x;
        s.last[i] = x;
    }
    s.wrote = true;
    ++s.ticks;
}

// RDF1: memcpy the property, then scale the destination only (the source stays the authored value, so it does not
// compound the next time this record is copied). ViewDistance and ShadowAbsolutelyRenderDistanceH are 4 bytes, stored
// either as an integer centimetre count or as a float. The two distance arrays are floats; values <= 1 (0 and the
// unlimited -1) are left alone.
extern "C" void* ExtraPerfPropC(void* dst, const void* src, size_t n, uint32_t key) {
    using Fn = void* (*)(void*, const void*, size_t);
    void* r = ep_propOrig ? reinterpret_cast<Fn>(ep_propOrig)(dst, src, n) : dst;
    const float m = ep_rdMul;
    if (m == 1.0f || !dst || n == 0 || n > 1024) return r;
    auto scaleFloat = [&](void* p) {
        float v;
        memcpy(&v, p, 4);
        if (v > 1.0f && v < 1.0e7f) {
            v *= m;
            memcpy(p, &v, 4);
        }
    };
    if (key == 0xA3DAE7DAu || key == 0x8FBEBC27u) {  // ViewDistance, ShadowAbsolutelyRenderDistanceH
        if (n != 4) return r;
        ep_propCalls = ep_propCalls + 1;
        uint32_t u;
        memcpy(&u, dst, 4);
        if (u >= 1000 && u <= 500000) {
            uint32_t s = (uint32_t)((double)u * (double)m);
            if (s < 1) s = 1;
            memcpy(dst, &s, 4);
            return r;
        }
        scaleFloat(dst);
        return r;
    }
    if (key == 0x1A767A36u || key == 0x0FD0F003u) {  // ModelViewDistanceArray, LODDistanceArray
        if (n % 4) return r;
        ep_propCalls = ep_propCalls + 1;
        auto* p = static_cast<unsigned char*>(dst);
        for (size_t i = 0; i < n; i += 4) scaleFloat(p + i);
    }
    return r;
}
