// samelook_harness.cpp - offline test of src/samelook.cpp (VSMA decisions + the render-config knobs). Not a game test.
// Simulates VsmClipmapRender's batch loop the way 1F8340 calls the hooked functions (budget call, then the bind call for a
// non-empty tile) and the per-tile loop's marking rule (first `budget` distinct bound tiles get bit 0), over bursts of new
// dirty tiles, and checks:
//   1. every tile the per-tile loop marks was batch-drawn on that visit (VsmaBindC called the original bind for it);
//   2. valid tiles (bit 0) are always drawn;
//   3. the next VSMA_Front x budget waiting tiles are always drawn;
//   4. no waiting tile is skipped on VSMA_K consecutive visits of its clipmap;
//   5. VSMA_K = 1, switch off and a throttle param >= 0 all give the stock draw sequence;
//   6. RCO writes / restores the S fields.
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "../src/samelook.h"

static int g_fail = 0;
#define CHECK(c, ...)                         \
    do {                                      \
        if (!(c)) {                           \
            ++g_fail;                         \
            printf("FAIL %s:%d ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);              \
            printf("\n");                     \
        }                                     \
    } while (0)

static void Log(const char* fmt, ...) {
    va_list a;
    va_start(a, fmt);
    vprintf(fmt, a);
    va_end(a);
    printf("\n");
}

// ---- fake engine ---------------------------------------------------------------------------------------------------------
static const uint32_t W = 16, H = 16;
static std::vector<uint8_t> g_pm(0x40200, 0);
static std::vector<uint32_t> g_flags(W * H * 8, 0);
static int32_t g_throttle = -1;
static uint32_t g_budget = 8;
static std::vector<uint32_t> g_boundThisVisit;  // tile indices the original bind was called for (batch loop)
static std::vector<uint32_t> g_list;            // current visit's dirty + requested non-empty tiles (engine order), as indices
static uint32_t g_clip = 3;

static uint32_t TileIdx(const uint32_t* t) { return ((t[0] & 0xF) * H + t[2]) * W + t[1]; }

static uint8_t __fastcall FakeBind(void*, void*) { return 1; }
static uint32_t __fastcall FakeBudget(void*) { return g_budget; }
static int32_t __fastcall FakeThrottle(void*, uint64_t, int32_t def) { return g_throttle >= 0 ? g_throttle : def; }

static uint32_t g_curTile[4];
static uint8_t __fastcall RecordingBind(void* clip, void* ctx) {
    g_boundThisVisit.push_back(TileIdx(g_curTile));
    return FakeBind(clip, ctx);
}

// one VsmClipmapRender call: batch loop over g_list, then the per-tile loop marks the first `budget` distinct tiles
static std::vector<uint32_t> g_marked;
static void __fastcall FakeRender(void*, void*) {
    g_boundThisVisit.clear();
    for (uint32_t idx : g_list) {
        uint32_t rest = idx;
        const uint32_t x = rest % W;
        rest /= W;
        const uint32_t y = rest % H;
        const uint32_t cl = rest / H;
        g_curTile[0] = cl | 0x10;
        g_curTile[1] = x;
        g_curTile[2] = y;
        const uint32_t b = caveVsmaBudget(nullptr);
        VsmaBindC(nullptr, nullptr, g_curTile, g_pm.data(), b);
    }
    // per-tile loop: first budget distinct tiles in order (bind always succeeds here) -> valid
    g_marked.clear();
    std::set<uint32_t> seen;
    for (uint32_t idx : g_list) {
        if (g_marked.size() >= g_budget) break;
        if (seen.count(idx)) continue;  // second occurrence: no longer dirty after the first mark
        seen.insert(idx);
        g_marked.push_back(idx);
    }
}

static void WriteIni(const wchar_t* path, const char* body) {
    FILE* f = _wfopen(path, L"wb");
    fputs(body, f);
    fclose(f);
}

struct SimResult {
    uint64_t drawn = 0, stock = 0;
    uint32_t maxRun = 0;
    uint32_t maxPerVisit = 0, stockMaxPerVisit = 0;
};

// bursts of new waiting tiles; `valid` tiles in the list are refreshes; `dups` repeats the first 3 indices once
static SimResult Simulate(uint32_t K, uint32_t front, bool on, int bursts, uint32_t perBurst, bool dups, bool expectStock) {
    // reset engine state
    std::fill(g_flags.begin(), g_flags.end(), 0u);
    std::map<uint32_t, uint32_t> run;  // waiting tile -> consecutive skipped visits
    SimResult r;
    std::vector<uint32_t> waiting;
    uint32_t next = 0;
    std::vector<uint32_t> validRefresh;
    for (int visit = 0; visit < bursts * 12; ++visit) {
        if (visit % 12 == 0) {
            for (uint32_t i = 0; i < perBurst; ++i) waiting.push_back((g_clip * H * W) + ((next++) % (H * W)));
            if (next % 7 == 0 && !validRefresh.empty()) {}
        }
        // a couple of valid tiles being refreshed (lifespan) in the middle of the list
        g_list = waiting;
        std::vector<uint32_t> valids;
        for (uint32_t i = 0; i < 2 && visit > 3; ++i) {
            const uint32_t v = g_clip * H * W + (H * W - 1 - i);
            g_flags[v] |= 1;
            valids.push_back(v);
        }
        if (!valids.empty() && g_list.size() > 20) g_list.insert(g_list.begin() + 20, valids.begin(), valids.end());
        else valids.clear();
        if (dups && g_list.size() > 30) g_list.insert(g_list.begin() + 30, g_list.begin(), g_list.begin() + 3);
        for (uint32_t i : g_list) g_flags[i] |= 4;
        caveVsmaEntry(nullptr, nullptr);
        // checks
        std::set<uint32_t> bound(g_boundThisVisit.begin(), g_boundThisVisit.end());
        for (uint32_t m : g_marked) CHECK(bound.count(m), "visit %d: marked tile %u was not batch-drawn", visit, m);
        for (uint32_t v : valids) CHECK(bound.count(v), "visit %d: valid tile %u skipped", visit, v);
        // front: the first budget distinct + front*budget waiting candidates after them must be drawn
        {
            std::set<uint32_t> d;
            uint32_t pastCount = 0;
            for (uint32_t idx : g_list) {
                if (d.size() < g_budget) { d.insert(idx); continue; }
                if (d.count(idx) || (g_flags[idx] & 1)) continue;
                if (pastCount < front * g_budget) CHECK(bound.count(idx), "visit %d: front tile %u skipped", visit, idx);
                ++pastCount;
            }
        }
        if (expectStock) CHECK(g_boundThisVisit.size() == g_list.size(), "visit %d: stock expected (%zu of %zu)", visit, g_boundThisVisit.size(), g_list.size());
        if (getenv("SLH_VERBOSE")) printf("  visit %2d: list %3zu drawn %3zu\n", visit, g_list.size(), g_boundThisVisit.size());
        r.drawn += g_boundThisVisit.size();
        r.stock += g_list.size();
        if (g_boundThisVisit.size() > r.maxPerVisit) r.maxPerVisit = (uint32_t)g_boundThisVisit.size();
        if (g_list.size() > r.stockMaxPerVisit) r.stockMaxPerVisit = (uint32_t)g_list.size();
        for (uint32_t idx : waiting) {
            if (std::find(g_marked.begin(), g_marked.end(), idx) != g_marked.end()) { run.erase(idx); continue; }
            if (bound.count(idx)) run[idx] = 0;
            else {
                const uint32_t n = ++run[idx];
                if (n > r.maxRun) r.maxRun = n;
            }
        }
        // marked tiles leave the waiting list; refreshed valid tiles are re-marked or stay valid
        std::vector<uint32_t> rest;
        for (uint32_t idx : waiting)
            if (std::find(g_marked.begin(), g_marked.end(), idx) == g_marked.end()) rest.push_back(idx);
            else g_flags[idx] = 1;
        waiting.swap(rest);
        (void)on;
        (void)K;
    }
    return r;
}

int main() {
    wchar_t tmp[MAX_PATH], ini[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    swprintf(ini, MAX_PATH, L"%sslh_%lu.ini", tmp, GetCurrentProcessId());
    *reinterpret_cast<uint32_t*>(g_pm.data() + 0x400E4) = W;
    *reinterpret_cast<uint32_t*>(g_pm.data() + 0x400E8) = H;
    *reinterpret_cast<uint32_t**>(g_pm.data() + 0x40118) = g_flags.data();

    struct Case { const char* ini; uint32_t K, front; bool on; int32_t thr; bool expectStock; };
    const Case cases[] = {
        {"[Patches]\nVSMA=1\n[Tuning]\nVSMA_K=2\nVSMA_Front=2\n", 2, 2, true, -1, false},
        {"[Patches]\nVSMA=1\n[Tuning]\nVSMA_K=3\nVSMA_Front=1\n", 3, 1, true, -1, false},
        {"[Patches]\nVSMA=1\n[Tuning]\nVSMA_K=2\nVSMA_Front=2\nVSMA_First=1\n", 2, 2, true, -1, false},
        {"[Patches]\nVSMA=1\n[Tuning]\nVSMA_K=1\n", 1, 2, true, -1, true},
        {"[Patches]\nVSMA=2\n[Tuning]\nVSMA_K=2\n", 2, 2, false, -1, true},
        {"[Patches]\nVSMA=1\n[Tuning]\nVSMA_K=2\n", 2, 2, true, 5, true},
    };
    for (const Case& c : cases) {
        WriteIni(ini, c.ini);
        samelook::StartInfo si;
        si.log = &Log;
        si.iniPath = ini;
        samelook::Configure(si);
        samelook::SetVsmaTargets((uintptr_t)&FakeRender, (uintptr_t)&FakeBudget, (uintptr_t)&RecordingBind, (uintptr_t)&FakeThrottle);
        g_throttle = c.thr;
        for (int dups = 0; dups < 2; ++dups) {
            SimResult r = Simulate(c.K, c.front, c.on, 3, 62, dups != 0, c.expectStock);
            printf("K %u front %u %s throttle %d dups %d: batch tiles drawn %llu of stock %llu (%.1f %% saved), peak visit %u vs %u, "
                   "max skipped run %u\n", c.K, c.front, c.on ? "on " : "off", c.thr, dups, (unsigned long long)r.drawn,
                   (unsigned long long)r.stock, 100.0 * (1.0 - (double)r.drawn / (double)r.stock), r.maxPerVisit, r.stockMaxPerVisit, r.maxRun);
            if (!c.expectStock) {
                CHECK(r.maxRun <= c.K - 1, "a waiting tile was skipped %u visits in a row (K %u)", r.maxRun, c.K);
                CHECK(r.drawn < r.stock, "nothing saved");
            }
        }
    }
    // ---- RCO ----
    WriteIni(ini, "[Patches]\nLPVR=1\nFOGT=2\n[Tuning]\nLPVR_N=3\nFOGT_V=0.25\n");
    samelook::StartInfo si;
    si.log = &Log;
    si.iniPath = ini;
    samelook::Configure(si);
    samelook::SetRcoInstalled();
    CHECK(samelook::KnobSwitch(samelook::kSky) == nullptr, "SKYR switch exists although SKYR = 0");
    CHECK(samelook::KnobSwitch(samelook::kLpv) && *samelook::KnobSwitch(samelook::kLpv) == 1, "LPVR not on");
    CHECK(samelook::KnobSwitch(samelook::kFogT) && *samelook::KnobSwitch(samelook::kFogT) == 0, "FOGT not installed-off");
    std::vector<uint8_t> S(0x3000, 0);
    *reinterpret_cast<uint32_t*>(&S[0x1394]) = 1;
    *reinterpret_cast<float*>(&S[0x13C0]) = 0.0f;
    RcoApplyC(S.data());
    CHECK(*reinterpret_cast<uint32_t*>(&S[0x1394]) == 3, "LPV not written");
    CHECK(*reinterpret_cast<float*>(&S[0x13C0]) == 0.0f, "FOGT written while off");
    *samelook::KnobSwitch(samelook::kFogT) = 1;
    RcoApplyC(S.data());
    CHECK(*reinterpret_cast<float*>(&S[0x13C0]) == 0.25f, "FOGT not written after switching on");
    *samelook::KnobSwitch(samelook::kLpv) = 0;
    RcoApplyC(S.data());
    CHECK(*reinterpret_cast<uint32_t*>(&S[0x1394]) == 1, "LPV not restored after switching off");
    char line[1024];
    samelook::StatusLine(line, sizeof line, 30);
    printf("%s\n", line);
    DeleteFileW(ini);
    printf(g_fail ? "FAILED (%d)\n" : "ALL PASSED\n", g_fail);
    return g_fail ? 1 : 0;
}
