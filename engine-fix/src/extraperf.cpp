// extraperf.cpp - "Extra performance (changes the look)" options (1.7.59). See extraperf.h and
// notes\extra-performance-settings.md.
#define WIN32_LEAN_AND_MEAN
#include "extraperf.h"

#include <windows.h>

#include <cstdio>
#include <cstring>

// asm cave data (extraperf.asm reads these RIP-relatively)
extern "C" {
uintptr_t ep_giOrig = 0, ep_lodOrig = 0, ep_pSimFrac = 0, ep_pDrawFrac = 0;
float ep_giMul = 1.0f, ep_giMin = 0.0f, ep_fxMul = 1.0f;
float ep_giLastIn = 0.0f, ep_giLastOut = 0.0f;  // caveGiScale: the last GiScaleLookup result and what the cave returned (stats read-back)
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
    {"XGI", 4, {"game", "75 %", "50 %", "35 %"}},
    {"XSHD", 4, {"game", "60 %", "35 %", "20 %"}},
    {"XSHF", 3, {"game", "2 taps", "1 tap"}},
    {"XLOD", 4, {"game", "x1.5 coarser", "x2 coarser", "x3 coarser"}},
    {"XTER", 4, {"game", "70 %", "40 %", "20 %"}},
    {"XPRT", 5, {"game", "75 %", "50 %", "25 %", "off"}},
    {"XAO", 2, {"game", "off"}},
    {"XPOST", 3, {"game", "blur + DOF off", "+ bloom off"}},
};
// presets: level per option (kClouds .. kPost)
const int kPresetLv[kNumPresets][kNumOpts] = {
    // XCLD XFOG XGI XSHD XSHF XLOD XTER XPRT XAO XPOST - tuned on the PC measurements (notes\extra-performance-settings.md 3):
    // XTER (visible flatter ground, no gain), XPRT (no measurable effect) and XPOST (no gain) are left to the player.
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  // game
    {2, 0, 2, 1, 0, 1, 0, 0, 0, 0},  // Performance: clouds, GI, shadow distance, LOD only (all inside the A-vs-A screenshot noise); any fog cut visibly dims the glow
    {3, 2, 3, 3, 2, 2, 0, 0, 1, 1},  // Potato: + coarser fog grid (glow kept: fog off was a big look change for +1.3 %), AO off, 1-tap shadows, blur / DOF off
};
const char* const kPresetName[kNumPresets] = {"game (all off)", "Performance", "Potato"};

bool g_wanted = false;
volatile LONG g_level[kNumOpts] = {};
bool g_rco = false, g_gi = false, g_lod = false, g_fx = false;

// level -> value tables
const int kCloudMax[4] = {0, 128, 64, 32}, kCloudMin[4] = {0, 24, 16, 8};
const int kFogX[4] = {0, 64, 48, 0}, kFogZ[4] = {0, 24, 16, 0};
const float kGiMul[4] = {1.0f, 0.75f, 0.5f, 0.35f};
const float kShdMul[4] = {1.0f, 0.6f, 0.35f, 0.2f};
const int kShf[3] = {0, 2, 1};
const float kLodMul[4] = {1.0f, 1.5f, 2.0f, 3.0f};
const float kTerMul[4] = {1.0f, 0.7f, 0.4f, 0.2f};
const float kFxMul[5] = {1.0f, 0.75f, 0.5f, 0.25f, 0.0f};

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
               fAoRays, fAoSteps, fAoContrast, fMb, fDof, fBloom, kNumFields };
Field g_f[kNumFields] = {{0x410, "VolumetricCloudMaxRaySampleNum", false}, {0x40C, "VolumetricCloudMinRaySampleNum", false},
                         {0x13DC, "VolumetricFogResolutionX", false}, {0x13E0, "VolumetricFogResolutionZ", false},
                         {0x1398, "VolumetricFogScatterScale", true}, {0xAF4, "ShadowEraseFarEnd", true},
                         {0xAF8, "ShadowEraseFarStart", true}, {0xB44, "ShadowSampleNum", false},
                         {0x1970, "TerrainTessellationMaxDistance", true}, {0x1978, "TerrainTessellationDistanceAdaptRate", true},
                         {0x12F4, "SSAORayNum", false}, {0x12F8, "SSAOStepNum", false}, {0x12D8, "SSAOContrast", true},
                         {0x188C, "LensSimulationMotionBlur2Enable", false}, {0x1700, "LensSimulationDOFFlag", false},
                         {0x30, "BloomWeight", true}};

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
}

}  // namespace

void Configure(const StartInfo& si) {
    g_log = si.log;
    const wchar_t* ini = si.iniPath;
    g_wanted = GetPrivateProfileIntW(L"ExtraPerf", L"Enabled", 1, ini) == 1;
    const int preset = (int)GetPrivateProfileIntW(L"ExtraPerf", L"Preset", 0, ini);
    static const wchar_t* const kIniKey[kNumOpts] = {L"XCLD", L"XFOG", L"XGI", L"XSHD", L"XSHF",
                                                     L"XLOD", L"XTER", L"XPRT", L"XAO", L"XPOST"};  // = kOpt[].key
    for (int o = 0; o < kNumOpts; ++o) {
        int v = (int)GetPrivateProfileIntW(L"ExtraPerf", kIniKey[o], 0, ini);
        if (v < 0 || v >= kOpt[o].levels) v = 0;
        g_level[o] = v;
    }
    if (preset > 0 && preset < kNumPresets)  // a preset in the ini wins over the per-option keys
        for (int o = 0; o < kNumOpts; ++o) g_level[o] = kPresetLv[preset][o];
    UpdateCaveData();
}

bool Wanted() { return g_wanted; }

void SetRcoInstalled() {
    g_rco = true;
    XLOG("EXTRA: render-config options installed (XCLD %d, XFOG %d, XSHD %d, XSHF %d, XTER %d, XAO %d, XPOST %d)",
         (int)g_level[kClouds], (int)g_level[kFog], (int)g_level[kShadowDist], (int)g_level[kShadowFilter], (int)g_level[kTerrain],
         (int)g_level[kAO], (int)g_level[kPost]);
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
bool AnyAvailable() { return g_rco || g_gi || g_lod || g_fx; }
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
    UpdateCaveData();
    XLOG("EXTRA: preset %s", kPresetName[p]);
}
int MatchPreset() {
    for (int p = 0; p < kNumPresets; ++p) {
        bool eq = true;
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
}

void StatusLine(char* buf, unsigned n) {
    buf[0] = 0;
    if (!n || !AnyAvailable()) return;
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
    if (k > 0 && (unsigned)k < n && g_lod && g_lodState.wrote)
        k += snprintf(buf + k, n - k, "; meshlet LOD scale[0] engine %.3f -> %.3f", (double)g_lodState.base[0], (double)g_lodState.last[0]);
    pr = r;
    pl = l;
}

}  // namespace extraperf

using namespace extraperf;

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
