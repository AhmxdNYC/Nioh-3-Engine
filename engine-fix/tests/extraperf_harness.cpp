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

int main() {
    using namespace extraperf;
    // ---- configure from a temp ini
    wchar_t ini[MAX_PATH], dir[MAX_PATH];
    GetTempPathW(MAX_PATH, dir);
    GetTempFileNameW(dir, L"xp", 0, ini);
    WritePrivateProfileStringW(L"ExtraPerf", L"Enabled", L"1", ini);
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
    SetLevel(kAO, 1);
    CHECK(MatchPreset() == -1, "a changed row must read 'custom'");
    ApplyPreset(kPresetGame);
    CHECK(AnyAvailable(), "installed");
    char line[1024];
    StatusLine(line, sizeof line);
    CHECK(strstr(line, "EXTRA") != nullptr, "status line: %s", line);

    printf(g_fail ? "%d FAILED\n" : "ALL PASSED\n", g_fail);
    return g_fail ? 1 : 0;
}
