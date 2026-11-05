// extraperf_harness.cpp - offline tests of src/extraperf.cpp / .asm (1.7.59 Extra performance). Built and run by
// tests/run_extraperf_harness.py; prints ALL PASSED. No game: a fake RenderConfig S, a fake LOD object, fake engine callees.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "../src/extraperf.h"

extern "C" float fxSimProbe(float v);   // extraperf_harness.asm
extern "C" float fxDrawProbe(float v);
extern "C" int charLodProbe(float limit, float cam);  // xmm0 limit, xmm1 camera; returns 1 visible, 2 culled
extern "C" float charLodScaled;
extern "C" void* propProbe(void* dst, const void* src, size_t n, uint32_t key);

static int g_fail = 0;
#define CHECK(c, ...)                                  \
    do {                                               \
        if (!(c)) {                                    \
            ++g_fail;                                  \
            printf("FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                       \
            printf("\n");                              \
        }                                              \
    } while (0)

static std::vector<uint8_t> S(0x2200, 0xCD);
static void setI(uint32_t off, int v) { memcpy(&S[off], &v, 4); }
static void setF(uint32_t off, float v) { memcpy(&S[off], &v, 4); }
static int getI(uint32_t off) { int v; memcpy(&v, &S[off], 4); return v; }
static float getF(uint32_t off) { float v; memcpy(&v, &S[off], 4); return v; }
static bool approx(float a, float b) { return std::fabs(a - b) <= 1e-4f * (1.0f + std::fabs(b)); }

// the engine's per-tick refill (env + ApplyGraphicsSettings at HIGH-ish settings)
static void refill() {
    setI(0x410, 256); setI(0x40C, 32);
    setI(0x13DC, 160); setI(0x13E0, 64); setF(0x1398, 1.0f);
    setF(0xAF4, 50000.0f); setF(0xAF8, 25000.0f); setI(0xB44, 4);
    setF(0x1970, 2000.0f); setF(0x1978, 0.5f);
    setI(0x12F4, 7); setI(0x12F8, 2); setF(0x12D8, 1.75f);
    setI(0x188C, 1); setI(0x1700, 1); setF(0x30, 1.0f);
}
static const uint32_t kFields[] = {0x410, 0x40C, 0x13DC, 0x13E0, 0x1398, 0xAF4, 0xAF8, 0xB44, 0x1970, 0x1978,
                                   0x12F4, 0x12F8, 0x12D8, 0x188C, 0x1700, 0x30};
static bool isField(uint32_t o) {
    for (uint32_t f : kFields)
        if (o >= f && o < f + 4) return true;
    return false;
}

// fake engine callees
static const float kGiTable[4] = {0.6f, 0.5f, 0.35f, 0.6f};
static float g_giOverride = -1.0f;
extern "C" float fakeGiLookup(int which) { return g_giOverride >= 0 ? g_giOverride : kGiTable[which & 3]; }
static void* g_lodCtx = nullptr;
static bool g_lodWrite = true;
extern "C" void fakeApplyLod(uint8_t* obj, void* ctx) {
    g_lodCtx = ctx;
    if (!g_lodWrite) return;  // camera node not found: the engine leaves the 8 floats as they were
    const float v[8] = {2, 2, 2, 8, 0, 2, 2, 2};
    memcpy(obj + 0xA108, v, sizeof v);
}

// #67206 fake RenderDimsGet 5AC428: writes {w, h}, returns the out pointer (as the original)
static uint32_t g_dimW = 960, g_dimH = 540;
extern "C" uint64_t* fakeRenderDims(uint64_t* out) {
    *out = ((uint64_t)g_dimH << 32) | g_dimW;
    return out;
}
extern "C" float fakeRdDist() { return 200.0f; }
extern "C" void* fakePropCopy(void* dst, const void* src, size_t n) {
    memcpy(dst, src, n);
    return dst;
}

int main() {
    using namespace extraperf;
    // ---- configure from a temp ini
    wchar_t ini[MAX_PATH], dir[MAX_PATH];
    GetTempPathW(MAX_PATH, dir);
    GetTempFileNameW(dir, L"xp", 0, ini);
    WritePrivateProfileStringW(L"ExtraPerf", L"Enabled", L"1", ini);
    WritePrivateProfileStringW(L"ExtraPerf", L"RenderDistance", L"0.70", ini);
    WritePrivateProfileStringW(L"ExtraPerf", L"XCLD", L"9", ini);  // out of range -> 0
    WritePrivateProfileStringW(L"ExtraPerf", L"XGI", L"2", ini);
    WritePrivateProfileStringW(nullptr, nullptr, nullptr, ini);
    StartInfo si;
    si.iniPath = ini;
    si.log = [](const char*, ...) {};
    Configure(si);
    CHECK(Wanted(), "Enabled = 1 not read");
    CHECK(Level(kClouds) == 0 && Level(kGi) == 2, "ini levels: XCLD %d XGI %d", Level(kClouds), Level(kGi));
    WritePrivateProfileStringW(L"ExtraPerf", L"Preset", L"2", ini);
    WritePrivateProfileStringW(nullptr, nullptr, nullptr, ini);
    Configure(si);
    CHECK(MatchPreset() == kPresetPotato, "ini Preset = 2 did not load Potato (%d)", MatchPreset());
    CHECK(RdPct() == 70, "Preset = 2 clobbered RenderDistance (%d)", RdPct());
    DeleteFileW(ini);
    for (int o = 0; o < Count(); ++o) {
        CHECK(Key(o)[0] == 'X', "key %d", o);
        for (int l = 0; l < Levels(o); ++l) CHECK(LevelName(o, l)[0] != '?', "%s level %d has no name", Key(o), l);
    }
    ApplyPreset(kPresetGame);
    CHECK(MatchPreset() == kPresetGame, "game preset");
    CHECK(!AnyAvailable(), "nothing installed yet");

    // ---- render-config fields (RCO)
    SetRcoInstalled();
    refill();
    std::vector<uint8_t> before = S;
    ApplyS(S.data());
    CHECK(S == before, "all levels 0 changed S");

    SetLevel(kClouds, 2);
    ApplyS(S.data());
    CHECK(getI(0x410) == 64 && getI(0x40C) == 16, "XCLD 2: %d / %d", getI(0x410), getI(0x40C));
    ApplyS(S.data());  // no refill: must not compound / change
    CHECK(getI(0x410) == 64 && getI(0x40C) == 16, "XCLD 2 again: %d / %d", getI(0x410), getI(0x40C));
    SetLevel(kClouds, 0);
    ApplyS(S.data());
    CHECK(getI(0x410) == 256 && getI(0x40C) == 32, "XCLD off without refill must restore: %d / %d", getI(0x410), getI(0x40C));

    SetLevel(kShadowDist, 1);
    refill();
    ApplyS(S.data());
    CHECK(approx(getF(0xAF4), 30000) && approx(getF(0xAF8), 15000), "XSHD 1: %f / %f", getF(0xAF4), getF(0xAF8));
    ApplyS(S.data());
    ApplyS(S.data());
    CHECK(approx(getF(0xAF4), 30000), "XSHD compounded without refill: %f", getF(0xAF4));
    refill();
    ApplyS(S.data());
    CHECK(approx(getF(0xAF4), 30000), "XSHD after refill: %f", getF(0xAF4));
    refill();  // engine refilled, then switch off: stays the engine's value
    SetLevel(kShadowDist, 0);
    ApplyS(S.data());
    CHECK(approx(getF(0xAF4), 50000), "XSHD off after refill: %f", getF(0xAF4));

    SetLevel(kFog, 3);
    refill();
    ApplyS(S.data());
    CHECK(getI(0x13DC) == 0 && getI(0x13E0) == 0 && getF(0x1398) == 0.0f, "XFOG 3 (off)");
    SetLevel(kFog, 1);
    ApplyS(S.data());  // no refill: base stays the engine's 160 / 64, scatter restored
    CHECK(getI(0x13DC) == 64 && getI(0x13E0) == 24 && getF(0x1398) == 1.0f, "XFOG 3 -> 1: %d %d %f", getI(0x13DC), getI(0x13E0),
          getF(0x1398));
    setI(0x13DC, 80); setI(0x13E0, 32);  // the game at VOLUMETRIC LIGHT
    ApplyS(S.data());
    CHECK(getI(0x13DC) == 64 && getI(0x13E0) == 24, "XFOG 1 at LIGHT");
    setI(0x13DC, 0); setI(0x13E0, 0);  // the game at LIGHTEST: never raised
    ApplyS(S.data());
    CHECK(getI(0x13DC) == 0 && getI(0x13E0) == 0, "XFOG must never raise the game's value");
    SetLevel(kFog, 0);

    SetLevel(kShadowFilter, 2);
    SetLevel(kTerrain, 3);
    SetLevel(kAO, 1);
    SetLevel(kPost, 2);
    refill();
    ApplyS(S.data());
    CHECK(getI(0xB44) == 1, "XSHF 2");
    CHECK(approx(getF(0x1970), 400) && approx(getF(0x1978), 0.1f), "XTER 3: %f %f", getF(0x1970), getF(0x1978));
    CHECK(getI(0x12F4) == 0 && getI(0x12F8) == 0 && getF(0x12D8) == 0.0f, "XAO");
    CHECK(getI(0x188C) == 0 && getI(0x1700) == 0 && getF(0x30) == 0.0f, "XPOST 2");
    SetLevel(kPost, 1);
    refill();
    ApplyS(S.data());
    CHECK(getI(0x188C) == 0 && getI(0x1700) == 0 && getF(0x30) == 1.0f, "XPOST 1 keeps bloom");
    for (uint32_t o = 0; o < S.size(); ++o)
        if (!isField(o) && S[o] != 0xCD) {
            CHECK(false, "byte S+%X outside the fields was written", o);
            break;
        }
    ApplyPreset(kPresetGame);
    ApplyS(S.data());  // no refill: everything back to the engine values
    std::vector<uint8_t> want = S;
    refill();
    CHECK(S == want, "game preset without refill did not restore every field");

    // ---- GI cave
    ep_giOrig = (uintptr_t)&fakeGiLookup;
    SetGiInstalled(ep_giOrig);
    auto gi = reinterpret_cast<float (*)(int)>(&caveGiScale);
    for (int w = 0; w < 4; ++w) CHECK(gi(w) == kGiTable[w], "XGI 0 changed scale %d", w);
    SetLevel(kGi, 2);
    for (int w = 0; w < 4; ++w) CHECK(approx(gi(w), kGiTable[w] * 0.5f), "XGI 2 scale %d = %f", w, gi(w));
    SetLevel(kGi, 3);
    g_giOverride = 0.2f;  // GI LIGHTEST IndirectOcn: 0.2 x 0.35 = 0.07 -> floor 0.1
    CHECK(approx(gi(2), 0.1f), "XGI floor: %f", gi(2));
    g_giOverride = -1.0f;
    SetLevel(kGi, 0);
    CHECK(gi(1) == kGiTable[1], "XGI back to 0");

    // ---- #67206 XRES cave (render size)
    ep_resOrig = (uintptr_t)&fakeRenderDims;
    SetResInstalled(ep_resOrig);
    auto rd = reinterpret_cast<uint64_t* (*)(uint64_t*)>(&caveRenderDims);
    uint64_t dims = 0;
    SetResLevel(0);
    CHECK(rd(&dims) == &dims && dims == ((540ull << 32) | 960), "XRES 0 changed the dims or the return value");
    SetResLevel(3);
    CHECK(rd(&dims) == &dims, "caveRenderDims must return the out pointer");
    CHECK((uint32_t)dims == 672 && (uint32_t)(dims >> 32) == 378, "XRES 3 (x0.7) 960x540 -> %ux%u", (uint32_t)dims, (uint32_t)(dims >> 32));
    g_dimW = 1281;
    g_dimH = 721;
    SetResLevel(4);
    rd(&dims);
    CHECK((uint32_t)dims == 768 && (uint32_t)(dims >> 32) == 432, "XRES 4 (x0.6) odd size -> even %ux%u", (uint32_t)dims, (uint32_t)(dims >> 32));
    g_dimW = 100;   // tiny targets are left alone
    g_dimH = 100;
    rd(&dims);
    CHECK((uint32_t)dims == 100, "XRES must not shrink a < 128 px size");
    g_dimW = 960;
    g_dimH = 540;
    ApplyPreset(kPresetExtreme);
    CHECK(ResLevel() == 3 && MatchPreset() == kPresetExtreme, "Extreme preset = XRES 3 (%d / match %d)", ResLevel(), MatchPreset());
    CHECK(RdPct() == 70, "Extreme preset changed RenderDistance to %d", RdPct());
    ApplyPreset(kPresetRisky);
    CHECK(ResLevel() == 0 && MatchPreset() == kPresetRisky, "Risky preset = XRES 0 (%d / match %d)", ResLevel(), MatchPreset());
    ApplyPreset(kPresetGame);
    rd(&dims);
    CHECK(dims == ((540ull << 32) | 960), "game preset: dims untouched");

    // ---- meshlet LOD cave
    std::vector<uint8_t> obj(0xA200, 0);
    ep_lodOrig = (uintptr_t)&fakeApplyLod;
    SetLodInstalled(ep_lodOrig);
    auto lod = reinterpret_cast<void (*)(uint8_t*, void*)>(&caveMeshLod);
    auto F = [&](int i) { float v; memcpy(&v, &obj[0xA108 + 4 * i], 4); return v; };
    int ctxTag = 0;
    lod(obj.data(), &ctxTag);
    CHECK(g_lodCtx == &ctxTag, "caveMeshLod did not pass rdx through");
    CHECK(F(0) == 2 && F(3) == 8 && F(4) == 0, "XLOD 0 changed the scales");
    SetLevel(kGeometry, 2);
    lod(obj.data(), &ctxTag);
    CHECK(F(0) == 4 && F(3) == 10 && F(4) == 0 && F(7) == 4, "XLOD 2: %f %f %f %f", F(0), F(3), F(4), F(7));
    g_lodWrite = false;  // engine did not write this tick: no compounding
    lod(obj.data(), &ctxTag);
    CHECK(F(0) == 4 && F(3) == 10, "XLOD compounded: %f %f", F(0), F(3));
    SetLevel(kGeometry, 0);
    lod(obj.data(), &ctxTag);
    CHECK(F(0) == 2 && F(3) == 8, "XLOD off without engine write must restore: %f %f", F(0), F(3));
    g_lodWrite = true;
    float big = 12.0f;
    SetLevel(kGeometry, 1);
    lod(obj.data(), &ctxTag);
    memcpy(&obj[0xA108], &big, 4);  // an engine value above the cap is never lowered
    ExtraPerfLodC(obj.data());
    CHECK(F(0) == 12.0f, "XLOD lowered a value above the cap: %f", F(0));
    SetLevel(kGeometry, 0);

    // ---- GPU particle caves
    float sim = -5, draw = -5;
    SetFxInstalled(&sim, &draw);
    CHECK(fxSimProbe(0.8f) == 0.8f && sim == 0.8f, "XPRT 0 sim: ret / global %f", sim);
    CHECK(fxDrawProbe(1.0f) == 1.0f && draw == 1.0f, "XPRT 0 draw %f", draw);
    SetLevel(kParticles, 2);
    CHECK(fxSimProbe(0.8f) == 0.8f && approx(sim, 0.4f), "XPRT 2 sim: register kept, global %f", sim);
    CHECK(fxDrawProbe(1.0f) == 1.0f && approx(draw, 0.5f), "XPRT 2 draw: global %f", draw);
    SetLevel(kParticles, 4);
    CHECK(fxDrawProbe(1.0f) == 1.0f && draw == 0.0f, "XPRT off: global %f", draw);
    SetLevel(kParticles, 0);

    // ---- presets
    ApplyPreset(kPresetPerformance);
    CHECK(MatchPreset() == kPresetPerformance, "Performance preset");
    CHECK(strcmp(PresetName(kPresetPerformance), "Optimized with effect visuals") == 0, "preset 1 name");
    CHECK(strcmp(PresetName(kPresetPotato), "Optimized") == 0, "preset 2 name");
    CHECK(strcmp(PresetName(-1), "custom") == 0, "custom name");
    SetLevel(kAO, 1);
    CHECK(MatchPreset() == -1, "a changed row must read 'custom'");
    ApplyPreset(kPresetGame);
    CHECK(AnyAvailable(), "installed");
    char line[1024];
    StatusLine(line, sizeof line);
    CHECK(strstr(line, "EXTRA") != nullptr, "status line: %s", line);

    // ---- #67225 Render distance caves (presets do not own this slider)
    ep_drawOrig = (uintptr_t)&fakeRdDist;
    ep_placeOrig = (uintptr_t)&fakeRdDist;
    SetRdDraw(ep_drawOrig);
    SetRdPlace(ep_placeOrig);
    auto rdDrawFn = reinterpret_cast<float (*)()>(&caveDrawLimit);
    auto rdPlaceFn = reinterpret_cast<float (*)()>(&cavePlaceDist);
    SetRdPct(100);
    CHECK(rdDrawFn() == 200.0f && rdPlaceFn() == 200.0f, "distance x1.00 changed the return");
    SetRdPct(50);
    CHECK(approx(rdDrawFn(), 100.0f) && approx(rdPlaceFn(), 100.0f), "distance x0.50: draw %f place %f", rdDrawFn(), rdPlaceFn());
    SetRdPct(100);
    CHECK(charLodProbe(100.0f, 40.0f) == 1 && approx(charLodScaled, 100.0f), "char visible x1 scaled %f", charLodScaled);
    CHECK(charLodProbe(100.0f, 150.0f) == 2, "char culled x1");
    CHECK(charLodProbe(100.0f, 100.0f) == 1, "cam == limit is still visible");
    SetRdPct(50);
    CHECK(charLodProbe(100.0f, 60.0f) == 2 && approx(charLodScaled, 50.0f), "char culled x0.50 scaled %f", charLodScaled);
    CHECK(charLodProbe(100.0f, 40.0f) == 1 && approx(charLodScaled, 50.0f), "char visible x0.50 scaled %f", charLodScaled);
    ep_propOrig = (uintptr_t)&fakePropCopy;
    SetRdProp(ep_propOrig);
    auto prop = [&](uint32_t key, const void* src, size_t n, int pct, void* out) {
        std::vector<unsigned char> keep((const unsigned char*)src, (const unsigned char*)src + n);
        SetRdPct(pct);
        void* r = propProbe(out, const_cast<void*>(src), n, key);
        CHECK(r == out, "prop cave did not return dest");
        CHECK(memcmp(src, keep.data(), n) == 0, "prop cave wrote the source");
    };
    uint32_t cm = 60000;
    uint32_t got = 0;
    prop(0xA3DAE7DAu, &cm, 4, 50, &got);
    CHECK(got == 30000, "ViewDistance cm 60000 x0.50 -> %u", got);
    uint32_t bits = 0x476A6000u;  // 60000.0f, above 500000 as a u32 so it takes the float path
    uint32_t fbits = 0;
    prop(0xA3DAE7DAu, &bits, 4, 50, &fbits);
    float fv;
    memcpy(&fv, &fbits, 4);
    CHECK(approx(fv, 30000.0f), "ViewDistance float x0.50 -> %f", fv);
    float arr[4] = {100.0f, -1.0f, 0.0f, 5000.0f};
    float arrOut[4] = {};
    prop(0x1A767A36u, arr, sizeof arr, 50, arrOut);
    CHECK(approx(arrOut[0], 50.0f) && arrOut[1] == -1.0f && arrOut[2] == 0.0f && approx(arrOut[3], 2500.0f),
          "ModelViewDistanceArray x0.50: %f %f %f %f", arrOut[0], arrOut[1], arrOut[2], arrOut[3]);
    uint32_t other = 60000;
    uint32_t otherOut = 0;
    prop(0x11111111u, &other, 4, 50, &otherOut);
    CHECK(otherOut == 60000, "unrelated hash was scaled to %u", otherOut);
    std::vector<unsigned char> oversized(1028, 1), bigOut(1028, 0);
    prop(0xA3DAE7DAu, oversized.data(), oversized.size(), 50, bigOut.data());
    CHECK(bigOut[0] == 1 && bigOut[1027] == 1, "n > 1024 was scaled");
    prop(0xA3DAE7DAu, &cm, 4, 100, &got);
    CHECK(got == 60000, "x1.00 scaled ViewDistance to %u", got);
    const int pct = RdPct();
    ApplyPreset(kPresetExtreme);
    CHECK(RdPct() == pct && ResLevel() == 3, "ApplyPreset Extreme moved RenderDistance to %d", RdPct());
    ApplyPreset(kPresetGame);
    SetRdPct(100);

    printf(g_fail ? "%d FAILED\n" : "ALL PASSED\n", g_fail);
    return g_fail ? 1 : 0;
}
