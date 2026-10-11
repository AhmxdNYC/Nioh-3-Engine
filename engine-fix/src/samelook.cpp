// samelook.cpp - VSMA (VSM waiting-tile amortisation) + engine update-rate knobs LPVR / SKYR / FOGT / CLDR (1.7.49).
// Design, proof and measurements: notes\same-look-package.md; header comment in samelook.h.
#define WIN32_LEAN_AND_MEAN
#include "samelook.h"

#include <windows.h>

#include "extraperf.h"  // 1.7.59: the "Extra performance" render-config options ride on the same RCO cave

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <vector>

namespace samelook {
namespace {

void (*g_log)(const char*, ...) = nullptr;
#define SLOG(...) \
    do {                              \
        if (g_log) g_log(__VA_ARGS__); \
    } while (0)

// ---- ini --------------------------------------------------------------------------------------------------------------------
int g_vsmaIni = 0;          // [Patches] VSMA
uint32_t g_vsmaK = 2;       // [Tuning] VSMA_K: a waiting tile past the front is drawn on 1 visit of its clipmap out of K
uint32_t g_vsmaFront = 2;   // [Tuning] VSMA_Front: x budget waiting tiles after the marked ones are always drawn
uint32_t g_vsmaFirst = 0;   // [Tuning] VSMA_First: 1 = a tile's first waiting visit is always drawn
int g_knobIni[kNumKnobs] = {};
uint32_t g_lpvN = 2;
float g_skyV = 4.0f, g_fogV = 0.5f, g_cloudV = 0.5f;

float IniFloat(const wchar_t* sec, const wchar_t* key, float def, const wchar_t* ini) {
    wchar_t b[64];
    GetPrivateProfileStringW(sec, key, L"", b, 64, ini);
    if (!b[0]) return def;
    wchar_t* e = nullptr;
    const double v = wcstod(b, &e);
    return e == b ? def : (float)v;
}

// ---- VSMA state (one VsmClipmapRender call at a time; the owner thread is the one inside the call) --------------------------
uintptr_t g_origRender = 0, g_origBudget = 0, g_origBind = 0, g_throttleGetter = 0;
volatile uint32_t g_vsmaOn = 0;
bool g_vsmaInstalled = false;
volatile long g_owner = 0;  // thread id inside a hooked VsmClipmapRender call, 0 = none

struct Call {
    bool active = false;      // VSMA applies to this call (switch on, owner thread, throttle off)
    bool haveThrottle = false;
    bool sawTile = false;     // first tile of the batch loop seen (clipmap visit counted)
    uint32_t clip = 0;
    uint32_t visit = 0;       // visit number of this clipmap
    uint32_t nBound = 0;      // distinct tiles bound while nBound < budget (= the tiles the engine marks this call)
    uint32_t bound[32] = {};
    uint32_t past = 0;        // waiting candidates seen past the marked ones (engine order)
} g_call;
uint32_t g_visit[16] = {};               // visits per clipmap level
std::vector<uint32_t> g_lastWait;        // per tile index: visit + 1 of the clipmap's last visit it was a waiting candidate

// counters (64-bit, render thread only; the stats thread reads them racily - fine for a rate)
volatile uint64_t c_calls = 0, c_inert = 0, c_throttle = 0, c_other = 0, c_visits = 0;
volatile uint64_t c_marked = 0, c_valid = 0, c_front = 0, c_phase = 0, c_first = 0, c_skip = 0, c_presents = 0;

using RenderFn = void(__fastcall*)(void*, void*);
using BudgetFn = uint32_t(__fastcall*)(void*);
using BindFn = uint8_t(__fastcall*)(void*, void*);
using ThrottleFn = int32_t(__fastcall*)(void*, uint64_t, int32_t);

// ---- knobs --------------------------------------------------------------------------------------------------------------
struct KnobDef {
    const char* key;
    uint32_t off;
    bool isFloat;
};
const KnobDef kKnob[kNumKnobs] = {
    {"LPVR", 0x1394, false},  // LPVUpdateFrequency
    {"SKYR", 0x0144, true},   // SkylightAmbientUpdateFreq
    {"FOGT", 0x13C0, true},   // VolumetricFogTemporalFactor
    {"CLDR", 0x07C0, true},   // VolumetricCloudReprojectionBlendWeightCurrentFrame
};
volatile uint32_t g_knobOn[kNumKnobs] = {};
bool g_rcoInstalled = false;
uint32_t g_knobVal[kNumKnobs] = {};      // bit pattern written
uint32_t g_knobSeen[kNumKnobs] = {};     // engine value the last time the knob was off (or the first frame)
volatile uint64_t c_rcoFrames = 0, c_refill[kNumKnobs] = {}, c_written[kNumKnobs] = {};
bool g_rcoLogged = false;
uint32_t g_lastWritten[kNumKnobs] = {};
bool g_wroteLast[kNumKnobs] = {};

}  // namespace

void Configure(const StartInfo& si) {
    g_log = si.log;
    const wchar_t* ini = si.iniPath;
    g_vsmaIni = (int)GetPrivateProfileIntW(L"Patches", L"VSMA", 0, ini);
    if (g_vsmaIni < 0 || g_vsmaIni > 2) g_vsmaIni = 0;
    g_vsmaK = (uint32_t)GetPrivateProfileIntW(L"Tuning", L"VSMA_K", 2, ini);
    if (g_vsmaK < 1) g_vsmaK = 1;
    if (g_vsmaK > 8) g_vsmaK = 8;
    g_vsmaFront = (uint32_t)GetPrivateProfileIntW(L"Tuning", L"VSMA_Front", 2, ini);
    if (g_vsmaFront > 16) g_vsmaFront = 16;
    g_vsmaFirst = GetPrivateProfileIntW(L"Tuning", L"VSMA_First", 0, ini) ? 1u : 0u;
    static const wchar_t* const kKey[kNumKnobs] = {L"LPVR", L"SKYR", L"FOGT", L"CLDR"};
    for (int k = 0; k < kNumKnobs; ++k) {
        int v = (int)GetPrivateProfileIntW(L"Patches", kKey[k], 0, ini);
        g_knobIni[k] = (v < 0 || v > 2) ? 0 : v;
    }
    g_lpvN = (uint32_t)GetPrivateProfileIntW(L"Tuning", L"LPVR_N", 2, ini);
    if (g_lpvN < 1) g_lpvN = 1;
    if (g_lpvN > 64) g_lpvN = 64;
    g_skyV = IniFloat(L"Tuning", L"SKYR_V", 4.0f, ini);
    g_fogV = IniFloat(L"Tuning", L"FOGT_V", 0.5f, ini);
    g_cloudV = IniFloat(L"Tuning", L"CLDR_V", 0.5f, ini);
    g_knobVal[kLpv] = g_lpvN;
    memcpy(&g_knobVal[kSky], &g_skyV, 4);
    memcpy(&g_knobVal[kFogT], &g_fogV, 4);
    memcpy(&g_knobVal[kCloud], &g_cloudV, 4);
}

int VsmaIni() { return g_vsmaIni; }
int RcoIni() {
    int r = 0;
    for (int k = 0; k < kNumKnobs; ++k) r |= g_knobIni[k];
    return r;
}

void SetVsmaTargets(uintptr_t vsmRender, uintptr_t budget, uintptr_t bind, uintptr_t throttleGetter) {
    g_origRender = vsmRender;
    g_origBudget = budget;
    g_origBind = bind;
    g_throttleGetter = throttleGetter;
    g_vsmaInstalled = true;
    g_vsmaOn = g_vsmaIni == 1 ? 1u : 0u;
    SLOG("VSMA: VSM waiting-tile amortisation installed, switch %s (K %u, front %u x budget, first-visit draw %s)",
         g_vsmaOn ? "ON" : "off", g_vsmaK, g_vsmaFront, g_vsmaFirst ? "on" : "off");
}

void SetRcoInstalled() {
    g_rcoInstalled = true;
    for (int k = 0; k < kNumKnobs; ++k) g_knobOn[k] = g_knobIni[k] == 1 ? 1u : 0u;
    SLOG("RCO: update-rate knobs installed: LPVR %s (LPVUpdateFrequency = %u), SKYR %s (SkylightAmbientUpdateFreq = %.3f), "
         "FOGT %s (VolumetricFogTemporalFactor = %.3f), CLDR %s (VolumetricCloudReprojectionBlendWeightCurrentFrame = %.3f)",
         !g_knobIni[kLpv] ? "-" : g_knobOn[kLpv] ? "ON" : "off", g_lpvN, !g_knobIni[kSky] ? "-" : g_knobOn[kSky] ? "ON" : "off",
         (double)g_skyV, !g_knobIni[kFogT] ? "-" : g_knobOn[kFogT] ? "ON" : "off", (double)g_fogV,
         !g_knobIni[kCloud] ? "-" : g_knobOn[kCloud] ? "ON" : "off", (double)g_cloudV);
}

// fast-testing (LiveCtl 'Tuning.<key> = v'): the [Tuning] values are read at use time (VsmaBindC / RcoApplyC), so a
// changed value applies from the next clipmap visit / frame. One aligned 32-bit store each.
bool SetTuning(const char* key, const char* val, char* reply, size_t n) {
    if (!key || !val) return false;
    const double d = atof(val);
    const long v = strtol(val, nullptr, 10);
    if (!_stricmp(key, "VSMA_K")) {
        g_vsmaK = (uint32_t)(v < 1 ? 1 : v > 8 ? 8 : v);
        snprintf(reply, n, "VSMA_K = %u", g_vsmaK);
    } else if (!_stricmp(key, "VSMA_Front")) {
        g_vsmaFront = (uint32_t)(v < 0 ? 0 : v > 16 ? 16 : v);
        snprintf(reply, n, "VSMA_Front = %u", g_vsmaFront);
    } else if (!_stricmp(key, "LPVR_N")) {
        g_lpvN = (uint32_t)(v < 1 ? 1 : v > 64 ? 64 : v);
        InterlockedExchange(reinterpret_cast<volatile LONG*>(&g_knobVal[kLpv]), (LONG)g_lpvN);
        snprintf(reply, n, "LPVR_N = %u", g_lpvN);
    } else if (!_stricmp(key, "SKYR_V") || !_stricmp(key, "FOGT_V") || !_stricmp(key, "CLDR_V")) {
        const int k = !_stricmp(key, "SKYR_V") ? kSky : !_stricmp(key, "FOGT_V") ? kFogT : kCloud;
        float f = (float)d;
        (k == kSky ? g_skyV : k == kFogT ? g_fogV : g_cloudV) = f;
        LONG bits;
        memcpy(&bits, &f, 4);
        InterlockedExchange(reinterpret_cast<volatile LONG*>(&g_knobVal[k]), bits);
        snprintf(reply, n, "%s = %.3f", key, (double)f);
    } else {
        return false;
    }
    SLOG("LIVE tuning: %s", reply);
    return true;
}

volatile uint32_t* VsmaSwitch() { return g_vsmaInstalled ? &g_vsmaOn : nullptr; }
volatile uint32_t* KnobSwitch(int k) {
    return (g_rcoInstalled && k >= 0 && k < kNumKnobs && g_knobIni[k]) ? &g_knobOn[k] : nullptr;
}

void OnPresent() { c_presents = c_presents + 1; }

void StatusLine(char* buf, unsigned n, unsigned secs) {
    buf[0] = 0;
    if (!n) return;
    static uint64_t p[16] = {};
    const double s = secs ? (double)secs : 1.0;
    unsigned w = 0;
    if (g_vsmaInstalled) {
        const uint64_t v[11] = {c_calls, c_inert, c_throttle, c_other, c_visits, c_marked, c_valid, c_front, c_phase, c_first, c_skip};
        uint64_t d[11];
        for (int i = 0; i < 11; ++i) {
            d[i] = v[i] - p[i];
            p[i] = v[i];
        }
        const uint64_t pres = c_presents - p[11];
        p[11] = c_presents;
        const uint64_t drawn = d[5] + d[6] + d[7] + d[8] + d[9];
        int k = snprintf(buf + w, n - w,
                         "stats/s VSMA (switch %s): VsmClipmapRender %.2f/frame (%.1f/s; stock: off %.1f, throttle param %.1f, other "
                         "thread %.1f), tile visits %.1f/s; batch tiles drawn %.1f/s (marked %.1f, valid %.1f, front %.1f, phase "
                         "%.1f, first %.1f) skipped %.1f/s = %.1f %% of the batch tiles",
                         g_vsmaOn ? "ON" : "off", pres ? (double)d[0] / (double)pres : 0.0, d[0] / s, d[1] / s, d[2] / s, d[3] / s,
                         d[4] / s, drawn / s, d[5] / s, d[6] / s, d[7] / s, d[8] / s, d[9] / s, d[10] / s,
                         drawn + d[10] ? 100.0 * (double)d[10] / (double)(drawn + d[10]) : 0.0);
        if (k > 0) w += (unsigned)k < n - w ? (unsigned)k : n - w - 1;
    }
    if (g_rcoInstalled && w + 2 < n) {
        int k = snprintf(buf + w, n - w, "%sstats/s RCO: frames %.1f/s", w ? " | " : "", (double)(c_rcoFrames - p[12]) / s);
        p[12] = c_rcoFrames;
        if (k > 0) w += (unsigned)k < n - w ? (unsigned)k : n - w - 1;
        for (int i = 0; i < kNumKnobs && w + 2 < n; ++i) {
            if (!g_knobIni[i]) continue;
            k = snprintf(buf + w, n - w, ", %s %s written %llu refilled %llu", kKnob[i].key, g_knobOn[i] ? "ON" : "off",
                         (unsigned long long)c_written[i], (unsigned long long)c_refill[i]);
            if (k > 0) w += (unsigned)k < n - w ? (unsigned)k : n - w - 1;
        }
    }
}

}  // namespace samelook

using namespace samelook;

extern "C" void caveVsmaEntry(void* a, void* b) {
    c_calls = c_calls + 1;
    const long me = (long)GetCurrentThreadId();
    const bool mine = g_vsmaOn && InterlockedCompareExchange(&g_owner, me, 0) == 0;
    if (!mine) {
        if (g_vsmaOn) c_other = c_other + 1;
        else c_inert = c_inert + 1;
        reinterpret_cast<RenderFn>(g_origRender)(a, b);
        return;
    }
    g_call = Call{};
    g_call.active = true;
    reinterpret_cast<RenderFn>(g_origRender)(a, b);
    g_call.active = false;
    InterlockedExchange(&g_owner, 0);
}

// batch loop, per dirty + requested tile of this clipmap: VsmTilesPerFrameBudget(param table)
extern "C" uint32_t caveVsmaBudget(void* table) {
    const uint32_t r = reinterpret_cast<BudgetFn>(g_origBudget)(table);
    if (g_call.active && !g_call.haveThrottle && g_owner == (long)GetCurrentThreadId()) {
        g_call.haveThrottle = true;
        // 8F041C(table, -, default): node param 37B3FAE9 (sub-light shadow throttle), the call at 1F95BF passes -1
        const int32_t t = reinterpret_cast<ThrottleFn>(g_throttleGetter)(table, 0, -1);
        if (t >= 0) {
            g_call.active = false;
            c_throttle = c_throttle + 1;
        }
    }
    return r;
}

// batch loop: VsmTileBindViewport(clip, ctx) for a non-empty tile; tile = {clip|flags, x, y, ...}, pm = page manager
extern "C" uint8_t VsmaBindC(void* clip, void* ctx, const uint32_t* tile, uint8_t* pm, uint32_t budget) {
    auto bind = reinterpret_cast<BindFn>(g_origBind);
    Call& c = g_call;
    if (!c.active || g_owner != (long)GetCurrentThreadId() || budget == 0 || budget > 32) return bind(clip, ctx);
    const uint32_t cl = tile[0] & 0xF;
    if (!c.sawTile) {
        c.sawTile = true;
        c.clip = cl;
        c.visit = ++g_visit[cl];
        c_visits = c_visits + 1;
    }
    // the same tile index the engine uses (1F8094 / 149468): ((clip * H) + y) * W + x
    const uint32_t H = *reinterpret_cast<const uint32_t*>(pm + 0x400E8);
    const uint32_t W = *reinterpret_cast<const uint32_t*>(pm + 0x400E4);
    const uint32_t idx = (cl * H + tile[2]) * W + tile[1];
    for (uint32_t i = 0; i < c.nBound; ++i)
        if (c.bound[i] == idx) {
            c_marked = c_marked + 1;
            return bind(clip, ctx);  // a tile the engine marks this call (a duplicate entry of it): stock
        }
    if (c.nBound < budget) {  // still inside the budget: these are exactly the tiles the per-tile loop marks
        const uint8_t r = bind(clip, ctx);
        if (r) c.bound[c.nBound++] = idx;
        c_marked = c_marked + 1;
        return r;
    }
    const uint32_t* flags = *reinterpret_cast<uint32_t* const*>(pm + 0x40118);
    if (!flags || (flags[idx] & 1)) {  // valid tile being refreshed (lifespan / stamp): sampled now -> stock
        c_valid = c_valid + 1;
        return bind(clip, ctx);
    }
    const uint32_t pos = c.past++;
    bool first = false;
    if (g_vsmaFirst) {
        if (idx >= g_lastWait.size() && idx < (1u << 22)) g_lastWait.resize((size_t)idx + 1024, 0);
        if (idx < g_lastWait.size()) {
            first = g_lastWait[idx] + 1 != c.visit;  // not a waiting candidate on this clipmap's previous visit
            g_lastWait[idx] = c.visit;
        } else {
            first = true;
        }
    }
    if (pos < g_vsmaFront * budget) {  // marked within the next VSMA_Front visits: warm-up as stock
        c_front = c_front + 1;
        return bind(clip, ctx);
    }
    if ((c.visit + idx) % g_vsmaK == 0) {
        c_phase = c_phase + 1;
        return bind(clip, ctx);
    }
    if (first) {
        c_first = c_first + 1;
        return bind(clip, ctx);
    }
    c_skip = c_skip + 1;
    return 0;  // not bound: the engine's own bind-failed path (nothing appended, no batch for it)
}

// RCO1: after ApplyGraphicsSettings, before the renderer's vfunc +4B8h reads the frame's RenderConfig S
extern "C" void RcoApplyC(uint8_t* S) {
    if (!S) return;
    c_rcoFrames = c_rcoFrames + 1;
    if (!g_rcoLogged) {
        g_rcoLogged = true;
        uint32_t v[kNumKnobs];
        for (int k = 0; k < kNumKnobs; ++k) memcpy(&v[k], S + kKnob[k].off, 4);
        float f[kNumKnobs];
        memcpy(f, v, sizeof f);
        SLOG("RCO: engine values in S at the first frame: LPVUpdateFrequency %u, SkylightAmbientUpdateFreq %.3f, "
             "VolumetricFogTemporalFactor %.3f, VolumetricCloudReprojectionBlendWeightCurrentFrame %.3f; offset check: "
             "VolumetricFogResolutionX (S+13DC) %u, VolumetricFogResolutionZ (S+13E0) %u, SSAORayNum (S+12F4) %u",
             v[kLpv], (double)f[kSky], (double)f[kFogT], (double)f[kCloud], *reinterpret_cast<uint32_t*>(S + 0x13DC),
             *reinterpret_cast<uint32_t*>(S + 0x13E0), *reinterpret_cast<uint32_t*>(S + 0x12F4));
    }
    for (int k = 0; k < kNumKnobs; ++k) {
        if (!g_knobIni[k]) continue;
        uint32_t* p = reinterpret_cast<uint32_t*>(S + kKnob[k].off);
        const uint32_t cur = *p;
        if (g_wroteLast[k] && cur != g_lastWritten[k]) c_refill[k] = c_refill[k] + 1;  // the engine re-filled it since
        if (g_knobOn[k]) {
            if (!g_wroteLast[k]) g_knobSeen[k] = cur;
            *p = g_knobVal[k];
            g_lastWritten[k] = g_knobVal[k];
            g_wroteLast[k] = true;
            c_written[k] = c_written[k] + 1;
        } else {
            if (g_wroteLast[k] && cur == g_lastWritten[k]) *p = g_knobSeen[k];  // switched off and nobody re-filled: put it back
            g_wroteLast[k] = false;
            g_knobSeen[k] = *p;
        }
    }
    extraperf::ApplyS(S);  // 1.7.59 Extra performance options (no-op unless installed and a level is set)
}
