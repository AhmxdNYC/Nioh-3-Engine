// bkstats_harness.cpp - offline test of src/bkstats.cpp (BKS census + KL) against a fake v2.02 image and GfxDevice.
// Built and run by tests/run_bkstats_harness.py. Not a game test.
#define NOMINMAX
#include <windows.h>

#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "../src/bkstats.h"

static std::mutex g_mx;
static std::vector<std::string> g_lines;
static void Log(const char* fmt, ...) {
    char b[2048];
    va_list a;
    va_start(a, fmt);
    vsnprintf(b, sizeof b, fmt, a);
    va_end(a);
    std::lock_guard<std::mutex> l(g_mx);
    g_lines.push_back(b);
    printf("  log: %s\n", b);
}
static int fails = 0;
static void Check(bool c, const char* m) {
    if (!c) {
        ++fails;
        printf("FAIL: %s\n", m);
    }
}
template <class T> static volatile T& F(uintptr_t a) { return *reinterpret_cast<volatile T*>(a); }

int main() {
    // fake image: reserve the v2.02 range, commit the pages the module reads
    const size_t kImg = 0x4600000;
    uint8_t* img = (uint8_t*)VirtualAlloc(nullptr, kImg, MEM_RESERVE, PAGE_NOACCESS);
    const uintptr_t base = (uintptr_t)img;
    for (uintptr_t rva : {0x418000u, 0x86F000u, 0x45BA000u, 0x45CE000u}) VirtualAlloc(img + rva, 0x1000, MEM_COMMIT, PAGE_READWRITE);
    const uint8_t kEcl[] = {0x4C, 0x8D, 0x81, 0xB0, 0x00, 0x00, 0x00};
    const uint8_t kDrain[] = {0x48, 0x8D, 0x42, 0x28};
    const uint8_t kBusy[] = {0x8B, 0x41, 0x28, 0x39, 0x41, 0x24, 0x74, 0x03, 0xB0, 0x01, 0xC3, 0x83, 0x79, 0x30, 0x00};
    memcpy(img + 0x86F775, kEcl, sizeof kEcl);
    memcpy(img + 0x418F46, kDrain, sizeof kDrain);
    memcpy(img + 0x418F90, kBusy, sizeof kBusy);
    // engine clock (QPC) and GfxDevice
    static uint8_t clk[0x40] = {};
    LARGE_INTEGER qf;
    QueryPerformanceFrequency(&qf);
    clk[0x12] = 1;
    memcpy(clk + 0x18, &qf.QuadPart, 8);
    F<uintptr_t>(base + 0x45BA780) = (uintptr_t)clk;
    std::vector<uint8_t> gmem(0x7000, 0);
    const uintptr_t g = (uintptr_t)gmem.data();
    F<uintptr_t>(base + 0x45BA588) = g;
    F<uint32_t>(g + 0x3C00) = 64;          // ring cap
    F<uint32_t>(g + 0x3C28) = 8u << 20;    // stream buffer
    F<uint32_t>(g + 0x3C70) = 256;         // CmdFlushLimit (engine value)
    F<uint32_t>(g + 0x3C18) = 3;           // sync-point tables
    F<uint32_t>(g + 0x4240) = 3;           // frame slots
    F<uint32_t>(g + 0x5B0 + 0x18) = 40;    // lists
    F<uint32_t>(g + 0x5B0 + 0x98) = 40;    // ECL cap
    F<uint32_t>(g + 0x5B0 + 0xA8) = 9;     // ECL high-water
    const double tps = (double)qf.QuadPart / 1000.0;  // ticks per ms

    wchar_t ini[MAX_PATH];
    GetTempPathW(MAX_PATH, ini);
    wcscat_s(ini, L"bkstats_harness.ini");
    FILE* fi = nullptr;
    _wfopen_s(&fi, ini, L"w");
    fputs("[Profiler]\nBackend = 1\nBackendSampleUs = 500\nBackendSec = 5\nBackendDelaySec = 0\n[Debug]\nLiveCtl = 1\n", fi);
    fclose(fi);

    // fake engine: 25 ms frames = 19 ms recording (dispatcher starved the first half) + 6 ms drain (flag 1, backlog 3);
    // per frame: 2000 draws, 8 kicks, 800 KB, drain 6 ms (1 count), ECL stall 1 ms, recycle 0.5 ms, tok95 0.25 ms, 28 submits;
    // the cache SRW lock is held exclusive during the first 5 ms of every frame
    std::atomic<bool> stop{false};
    std::thread eng([&] {
        uint64_t ecl = 0, rec = 0, tok = 0;
        uint32_t gen = 0, sub = 0, ec = 0;
        LARGE_INTEGER t0, t;
        QueryPerformanceCounter(&t0);
        uint32_t w = 0, r = 0;
        for (int fr = 0; !stop; ++fr) {
            const double fs = fr * 25.0;
            auto now = [&] {
                QueryPerformanceCounter(&t);
                return (double)(t.QuadPart - t0.QuadPart) * 1000.0 / (double)qf.QuadPart;
            };
            while (now() < fs + 19.0 && !stop) {
                const double ph = now() - fs;
                F<uint64_t>(g + 0x61A0 + 0xCF8) = ph < 5.0 ? 1 : 0;
                if (ph < 9.5) { r = w; F<uint32_t>(g + 0x3C10) = 0; }        // starved
                else { w = r + 1; F<uint32_t>(g + 0x3C10) = 1; }              // one segment in flight
                F<uint32_t>(g + 0x3C04) = w;
                F<uint32_t>(g + 0x3C08) = r;
                Sleep(0);
            }
            F<uint8_t>(base + 0x45CE8A0) = 1;
            w = r + 3;
            F<uint32_t>(g + 0x3C04) = w;
            F<uint32_t>(g + 0x3C10) = 3;
            while (now() < fs + 25.0 && !stop) Sleep(0);
            r = w;
            F<uint32_t>(g + 0x3C08) = r;
            F<uint32_t>(g + 0x3C10) = 0;
            F<uint8_t>(base + 0x45CE8A0) = 0;
            ecl += (uint64_t)(1.0 * tps); ++ec; rec += (uint64_t)(0.5 * tps); tok += (uint64_t)(0.25 * tps); sub += 28;
            if (fr % 150 == 149) tok = 0;  // the engine resets some of its own timers: deltas must not underflow
            F<uint64_t>(g + 0x5B0 + 0xB0) = ecl; F<uint32_t>(g + 0x5B0 + 0xC0) = ec;
            F<uint64_t>(g + 0x5B0 + 0xF8) = rec; F<uint32_t>(g + 0x5B0 + 0x108) = ec;
            F<uint64_t>(g + 0x61A0 + 0x9F8) = tok;
            F<uint32_t>(g + 0x5B0 + 0x11C) = sub;
            F<uint32_t>(g + 0x3C60) = ++gen;  // final kick: generation +1, then the snapshot copy
            F<uint32_t>(g + 0x3D00) = 2000 + (fr & 1);
            F<uint32_t>(g + 0x3D04) = 8;
            F<uint32_t>(g + 0x3D0C) = 800 * 1024;
            F<uint64_t>(g + 0x3D28) = (uint64_t)(6.0 * tps) + fr % 3;
            F<uint64_t>(g + 0x3D30) = (uint64_t)(6.0 * tps);
            F<uint32_t>(g + 0x3D38) = 1;
            // FOV: handle wait 0.4 ms x2, sync-point GPU wait 1.5 ms x1 on every other frame, stat3CB8 0.2 ms x1
            F<uint64_t>(g + 0x3D40) = (uint64_t)(0.2 * tps); F<uint32_t>(g + 0x3D50) = 1;
            F<uint64_t>(g + 0x3D58) = (uint64_t)(0.8 * tps); F<uint64_t>(g + 0x3D60) = (uint64_t)(0.4 * tps);
            F<uint32_t>(g + 0x3D68) = 2;
            F<uint64_t>(g + 0x3D70) = (fr & 1) ? (uint64_t)(1.5 * tps) : 0;
            F<uint64_t>(g + 0x3D78) = (fr & 1) ? (uint64_t)(1.5 * tps) : 0;
            F<uint32_t>(g + 0x3D80) = (fr & 1) ? 1 : 0;
        }
    });

    bkstats::StartInfo si;
    si.log = &Log;
    si.iniPath = ini;
    si.base = base;
    si.knownBuild = true;
    bkstats::Start(si);
    char rep[256];
    Sleep(300);
    // KL: stock 256 -> /4 = 64; engine rewrite 300 -> 75 re-asserted; /64 -> clamp 16; 0 -> engine value back
    Check(bkstats::SetTuning("KL_Div", "4", rep, sizeof rep) && F<uint32_t>(g + 0x3C70) == 64, "KL_Div 4 -> 64");
    F<uint32_t>(g + 0x3C70) = 300;
    Sleep(400);
    Check(F<uint32_t>(g + 0x3C70) == 75, "engine rewrite 300 -> KL re-asserts 75");
    Check(bkstats::SetTuning("KL_Div", "64", rep, sizeof rep) && F<uint32_t>(g + 0x3C70) == 16, "KL_Div 64 -> clamp 16");
    Check(bkstats::SetTuning("KL_Div", "0", rep, sizeof rep) && F<uint32_t>(g + 0x3C70) == 300, "KL_Div 0 -> engine value 300");
    Check(bkstats::SetTuning("KL_Div", "99", rep, sizeof rep) && strstr(rep, "must be"), "KL_Div 99 refused");
    Check(F<uint32_t>(g + 0x3C70) == 300, "refused value leaves the field alone");
    Check(!bkstats::SetTuning("PP_Period", "4", rep, sizeof rep), "other keys not claimed");
    Sleep(11500);  // two report windows
    stop = true;
    eng.join();

    std::string a, b, fv;
    {
        std::lock_guard<std::mutex> l(g_mx);
        for (auto& s : g_lines) {
            if (s.rfind("BKS: ", 0) == 0 && s.find("| main:") != std::string::npos) a = s;
            if (s.rfind("BKS: disp:", 0) == 0) b = s;
            if (s.rfind("BKS: fov:", 0) == 0) fv = s;
        }
    }
    Check(!a.empty() && !b.empty(), "two report lines");
    auto num = [](const std::string& s, const char* key) {
        const size_t p = s.find(key);
        return p == std::string::npos ? -1.0 : atof(s.c_str() + p + strlen(key));
    };
    const double fps = num(a, "frames ") > 0 ? num(a, "(") : -1, kicks = num(a, "kicks/f "), draws = num(a, "draws/f "),
                 kb = num(a, "streamKB/f "), drain = num(a, "drain "), lim = num(a, "limit ");
    const double st = num(b, "starved-while-recording "), dr = num(b, "main in drain "), ecl = num(b, "ECL stall "),
                 rcw = num(b, "recycle wait "), t95 = num(b, "tok95 cache upload "), sub = num(b, "submits/f "),
                 held = num(b, "held "), excl = num(b, "excl "), lists = num(b, "lists ");
    printf("parsed: fps %.1f kicks %.1f draws %.0f KB %.0f drain %.2f limit %.0f | starved %.1f drain%% %.1f ecl %.2f rec %.2f tok95 %.2f "
           "sub %.1f held %.1f excl %.1f lists %.0f\n", fps, kicks, draws, kb, drain, lim, st, dr, ecl, rcw, t95, sub, held, excl, lists);
    auto approx = [](double v, double want, double tol) { return std::fabs(v - want) <= tol; };
    Check(approx(fps, 40.0, 3.0), "frames/s ~40");
    Check(approx(kicks, 8.0, 0.01) && approx(draws, 2000.0, 1.0) && approx(kb, 800.0, 1.0), "per-frame snapshot sums");
    Check(approx(drain, 6.0, 0.05), "drain ms/frame ~6.0 (engine timer)");
    Check(approx(lim, 300.0, 0.1), "limit line = engine value after KL 0");
    Check(approx(st, 50.0, 8.0), "starved-while-recording ~50 %");
    Check(approx(dr, 24.0, 4.0), "main in drain ~24 % of samples");
    Check(approx(ecl, 1.0, 0.1) && approx(rcw, 0.5, 0.05) && t95 > 0.0 && t95 <= 0.26, "ECL stall / recycle / tok95 ms per frame");
    Check(approx(sub, 28.0, 1.5), "submits per frame ~28");
    Check(approx(held, 20.0, 4.0) && approx(excl, 20.0, 4.0), "cache lock held / exclusive ~20 %");
    Check(approx(lists, 40.0, 0.1), "lists");
    const double hw = num(fv, "handle wait "), sw = num(fv, "sync-point GPU wait "), swin = num(fv, "in "),
                 x3 = num(fv, "stat3CB8 "), stab = num(fv, "sync tables "), slots = num(fv, "frame slots ");
    printf("parsed fov: hw %.2f sw %.2f in %.0f%% x3 %.2f tables %.0f slots %.0f\n", hw, sw, swin, x3, stab, slots);
    Check(!fv.empty(), "fov line");
    Check(approx(hw, 0.8, 0.02) && approx(sw, 0.75, 0.05) && approx(swin, 50.0, 5.0) && approx(x3, 0.2, 0.02),
          "fov waits per frame");
    Check(approx(stab, 3.0, 0.1) && approx(slots, 3.0, 0.1), "fov sizes");
    printf(fails ? "FAILED (%d)\n" : "ALL PASSED\n", fails);
    return fails ? 1 : 0;
}
