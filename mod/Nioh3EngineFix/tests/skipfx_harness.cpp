// skipfx_harness.cpp - offline logic test of src/skipfx.cpp (the [Debug] SkipFx diagnostic): PSO / stage-qualified / marker
// matching, live control-file reload, ack file, ClearMode. Fake psoswap map and fake command-list methods. Not a game test.
// Built and run by tests/run_skipfx_harness.py.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <cstdio>
#include <cstring>
#include <string>

#include "../src/osd.h"
#include "../src/psoswap.h"
#include "../src/skipfx.h"

// ---- fake psoswap ---------------------------------------------------------------------------------------------------------
namespace psoswap {
bool Tracking() { return true; }
uint32_t PsoSetId(const void* p) {
    switch ((uintptr_t)p) {
        case 0x1000: return 1;
        case 0x2000: return 2;
        case 0x3000: return 3;
        case 0x5000: return 4;
        default: return 0;
    }
}
int DescribeSet(uint32_t id, char* b, size_t n) {
    const char* s = id == 1   ? "cs=b76874bb17bdfc10a0739b74191c68a9"
                    : id == 2 ? "vs=e5190e82a274aaa5bfc24b14ca77567a ps=c4656a8d23602c5bacff59efa7950ab0"
                    : id == 3 ? "vs=c4656a8d00000000000000000000aaaa ps=1111111111111111111111111111bbbb"
                    : id == 4 ? "cs=4b4c3cbe0000000000000000000000cc"
                              : "";
    strncpy_s(b, n, s, _TRUNCATE);
    return (int)strlen(b);
}
uint32_t SetCount() { return 4; }
}  // namespace psoswap

// ---- fake osd frame counter (#66661 half-rate) ----
static int64_t g_frames = 0;
namespace osd {
int64_t GameFrames() { return g_frames; }
}  // namespace osd

static int g_fail = 0, g_clears = 0;
static void Check(bool ok, const char* what) {
    printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) ++g_fail;
}
static void Log(const char* fmt, ...) {
    va_list a;
    va_start(a, fmt);
    printf("  log: ");
    vprintf(fmt, a);
    printf("\n");
    va_end(a);
}
static void STDMETHODCALLTYPE FakeClear(void*, D3D12_CPU_DESCRIPTOR_HANDLE, const FLOAT*, UINT, const D3D12_RECT*) { ++g_clears; }

static void* g_orig[96];
static uint8_t g_mask[96];
static void* const L1 = (void*)0x7000;

static bool Call(int slot, uintptr_t a1 = 0, uintptr_t a2 = 0, uintptr_t a3 = 0) { return skipfx::OnListCall(L1, 0, slot, a1, a2, a3); }
static bool Begin(const wchar_t* name) { return Call(57, 0, (uintptr_t)name, (uintptr_t)((wcslen(name) + 1) * 2)); }

static void WriteCtl(const std::wstring& p, const char* body) {
    const std::wstring tmp = p + L".w";
    FILE* f = nullptr;
    _wfopen_s(&f, tmp.c_str(), L"wb");
    fputs(body, f);
    fclose(f);
    MoveFileExW(tmp.c_str(), p.c_str(), MOVEFILE_REPLACE_EXISTING);
    Sleep(400);
}
static std::string ReadAll(const std::wstring& p) {
    FILE* f = nullptr;
    std::string s;
    if (_wfopen_s(&f, p.c_str(), L"rb") || !f) return s;
    char b[1024];
    size_t n;
    while ((n = fread(b, 1, sizeof b, f)) > 0) s.append(b, n);
    fclose(f);
    return s;
}

int main() {
    wchar_t tmpd[MAX_PATH];
    GetTempPathW(MAX_PATH, tmpd);
    const std::wstring dir = std::wstring(tmpd) + L"skipfx_harness";
    CreateDirectoryW(dir.c_str(), nullptr);
    const std::wstring ini = dir + L"\\Nioh3EngineFix.ini", ctl = dir + L"\\Nioh3EngineFix_skip.ini", ack = ctl + L".ack";
    DeleteFileW(ctl.c_str());
    WritePrivateProfileStringW(L"Debug", L"SkipFx", L"1", ini.c_str());
    WritePrivateProfileStringW(L"Debug", L"SkipPSO", L"b76874bb", ini.c_str());
    WritePrivateProfileStringW(L"Debug", L"SkipMarker", L"", ini.c_str());

    skipfx::StartInfo si;
    si.log = &Log;
    si.iniPath = ini.c_str();
    skipfx::Start(si);
    Check(skipfx::WantsHooks(), "SkipFx = 1 wants the hooks");
    g_orig[48] = (void*)&FakeClear;
    skipfx::EnableHooks(g_mask, 96, g_orig, nullptr);
    Check(g_mask[25] && g_mask[12] && g_mask[14] && g_mask[57] && g_mask[59] && g_mask[79] && !g_mask[26], "hook mask: PSO/draws/markers, not barriers");

    // 1. ini list: cs b76874bb
    Call(10, 0, 0);  // Reset
    Call(25, 0x1000);
    Check(Call(14, 1, 1, 1), "ini PSO b76874bb: its Dispatch is skipped");
    Check(Call(59), "... and its ExecuteIndirect");
    Call(25, 0x2000);
    Check(!Call(12, 3, 1), "other PSO: draw recorded");
    Call(25, 0x4000);
    Check(!Call(12, 3, 1), "untracked PSO (id 0): never skipped");

    // 2. live control file: stage-qualified ps=c4656a8d (must not hit set 3, whose VS has that fragment)
    WriteCtl(ctl, "[SkipFx]\r\nTag=SMsfb\r\nPSO=ps=c4656a8d\r\n");
    Call(25, 0x2000);
    Check(Call(12, 3, 1), "reload: ps=c4656a8d draw skipped");
    Call(25, 0x3000);
    Check(!Call(12, 3, 1), "ps=c4656a8d does not match a VS hash");
    Call(25, 0x1000);
    Check(!Call(14, 1, 1, 1), "previous list (b76874bb) no longer skipped");
    Sleep(700);   // the ack is rewritten every 500 ms
    std::string a = ReadAll(ack);
    Check(a.find("tag=SMsfb") != std::string::npos && a.find("draws=1") != std::string::npos, "ack file: tag + 1 skipped draw");

    // 3. markers (UTF-16, exact + prefix), nesting
    WriteCtl(ctl, "[SkipFx]\r\nTag=mk\r\nMarker=DrawSky, Casc*\r\n");
    Call(25, 0x3000);
    Check(!Call(12), "outside markers: recorded");
    Begin(L"Frame");
    Begin(L"DrawSky");
    Check(Call(12), "inside DrawSky: skipped");
    Begin(L"Inner");
    Check(Call(14), "nested inside DrawSky: skipped");
    Call(58);
    Call(58);
    Check(!Call(12), "after EndEvent of DrawSky: recorded");
    Begin(L"Cascade 0");
    Check(Call(13), "prefix Casc* matches 'Cascade 0'");
    Call(58);
    Begin(L"DrawSkyX");
    Check(!Call(12), "exact name: DrawSkyX not matched");
    Call(58);
    Call(58);
    Call(58);  // unbalanced extra End: clamped
    Check(!Call(12), "unbalanced EndEvent tolerated");

    // 4. ClearMode
    WriteCtl(ctl, "[SkipFx]\r\nTag=clr\r\nPSO=c4656a8d23\r\nClearMode=1\r\nClear=1,1,1,1\r\n");
    D3D12_CPU_DESCRIPTOR_HANDLE h[2] = {{0x100}, {0x200}};
    Call(46, 2, (uintptr_t)h, 0);
    Call(25, 0x2000);
    g_clears = 0;
    Check(Call(12) && g_clears == 2, "first skipped draw on a fresh binding: both RTVs cleared");
    Check(Call(12) && g_clears == 2, "second skipped draw: no second clear");
    Call(46, 1, (uintptr_t)h, 0);
    Call(25, 0x1000);
    Check(!Call(12), "binding 2: unrelated draw recorded first");
    Call(25, 0x2000);
    Check(Call(12) && g_clears == 2, "binding already drawn to: skipped without a clear");

    // 5. baseline (empty list)
    WriteCtl(ctl, "[SkipFx]\r\nTag=base\r\nPSO=\r\nMarker=\r\n");
    Call(25, 0x2000);
    Check(!Call(12), "baseline: nothing skipped");
    Begin(L"DrawSky");
    Check(!Call(12), "baseline: markers not skipped");
    Call(58);
    a = ReadAll(ack);
    Check(a.find("tag=base") != std::string::npos, "ack follows the baseline tag");
    // 6. #66661 Period = 2 from the control file: matched work runs on even frames, skipped on odd ones
    WriteCtl(ctl, "[SkipFx]\r\nTag=half\r\nPSO=cs=b76874bb\r\nPeriod=2\r\n");
    Call(25, 0x1000);
    g_frames = 10;
    Check(!Call(14, 1, 1, 1), "Period 2, even frame: dispatch runs");
    g_frames = 11;
    Check(Call(14, 1, 1, 1), "Period 2, odd frame: dispatch skipped (last result reused)");
    Call(25, 0x2000);
    Check(!Call(12, 3, 1), "Period 2: unmatched draw always recorded");

    char st[256];
    printf("status: %s\n", skipfx::StatusLine(st, sizeof st));

    // 7. #66661 XHALF via the Risky preset (no [Debug] SkipFx): built-in compute list, lean hook mask, Period 2
    const std::wstring ini2 = dir + L"\\half.ini";
    DeleteFileW(ini2.c_str());
    Check(!skipfx::IniWants(ini2.c_str()), "empty ini: no SkipFx / half-rate");
    WritePrivateProfileStringW(L"ExtraPerf", L"Preset", L"2", ini2.c_str());
    Check(!skipfx::IniWants(ini2.c_str()), "Preset 2: no half-rate");
    WritePrivateProfileStringW(L"ExtraPerf", L"Preset", L"3", ini2.c_str());
    Check(skipfx::IniWants(ini2.c_str()), "Preset 3 without XHALF: half-rate wanted");
    WritePrivateProfileStringW(L"ExtraPerf", L"XHALF", L"0", ini2.c_str());
    Check(!skipfx::IniWants(ini2.c_str()), "Preset 3 + XHALF = 0: off (explicit 0 wins)");
    WritePrivateProfileStringW(L"ExtraPerf", L"XHALF", nullptr, ini2.c_str());
    WritePrivateProfileStringW(L"ExtraPerf", L"Preset", L"4", ini2.c_str());
    Check(skipfx::IniWants(ini2.c_str()), "#67206 Preset 4 (Extreme) without XHALF: half-rate wanted");
    WritePrivateProfileStringW(L"ExtraPerf", L"XHALF_Mask", L"0", ini2.c_str());
    Check(!skipfx::IniWants(ini2.c_str()), "#67206 XHALF_Mask = 0: nothing to skip, off");
    WritePrivateProfileStringW(L"ExtraPerf", L"XHALF_Mask", L"12", ini2.c_str());
    Check(!skipfx::IniWants(ini2.c_str()), "#67915 cloud + fog only mask cannot enable half-rate");
    WritePrivateProfileStringW(L"ExtraPerf", L"XHALF_Mask", nullptr, ini2.c_str());
    WritePrivateProfileStringW(L"ExtraPerf", L"Preset", L"3", ini2.c_str());
    Check(skipfx::HalfList(15) == "cs=4b4c3cbe,cs=6ae59bd0,cs=b76874bb,cs=641524ee" && skipfx::HalfList(8) == "cs=641524ee" &&
              skipfx::HalfList(5) == "cs=4b4c3cbe,cs=b76874bb" && skipfx::HalfList(0).empty(),
          "#67206 XHALF_Mask -> per-pass PSO list");
    {
        int pd = 0, rn = 0;
        skipfx::HalfRate(ini2.c_str(), &pd, &rn);
        Check(pd == 4 && rn == 2, "#67206 default rate: Period 4, Run 2 (both frame parities written each cycle)");
        WritePrivateProfileStringW(L"ExtraPerf", L"XHALF_Period", L"2", ini2.c_str());
        skipfx::HalfRate(ini2.c_str(), &pd, &rn);
        Check(pd == 2 && rn == 1, "Period 2 without Run: Run 1 (the old 1-in-2)");
        WritePrivateProfileStringW(L"ExtraPerf", L"XHALF_Period", L"3", ini2.c_str());
        WritePrivateProfileStringW(L"ExtraPerf", L"XHALF_Run", L"5", ini2.c_str());
        skipfx::HalfRate(ini2.c_str(), &pd, &rn);
        Check(pd == 3 && rn == 2, "Run >= Period is clamped to Period - 1");
        WritePrivateProfileStringW(L"ExtraPerf", L"XHALF_Period", nullptr, ini2.c_str());
        WritePrivateProfileStringW(L"ExtraPerf", L"XHALF_Run", nullptr, ini2.c_str());
    }
    WritePrivateProfileStringW(L"ExtraPerf", L"XHALF", nullptr, ini2.c_str());
    WritePrivateProfileStringW(L"ExtraPerf", L"Enabled", L"0", ini2.c_str());
    Check(!skipfx::IniWants(ini2.c_str()), "Preset 3 but [ExtraPerf] Enabled = 0: off");
    WritePrivateProfileStringW(L"ExtraPerf", L"Enabled", nullptr, ini2.c_str());
    si.iniPath = ini2.c_str();
    skipfx::Start(si);
    Check(skipfx::WantsHooks(), "XHALF: wants the hooks");
    uint8_t m2[96] = {};
    skipfx::EnableHooks(m2, 96, g_orig, nullptr);
    Check(m2[10] && m2[11] && m2[25] && m2[14] && m2[59] && !m2[12] && !m2[13] && !m2[57] && !m2[58] && !m2[46] && !m2[79],
          "XHALF lean mask: PSO binds + Dispatch / ExecuteIndirect only (draws, markers, RT binds untouched)");
    void* const L2 = (void*)0x7100;
    skipfx::OnListCall(L2, 0, 10, 0, 0x5000, 0);   // Reset with the GI gather PSO as the initial state
    g_frames = 20;
    Check(!skipfx::OnListCall(L2, 0, 14, 8, 8, 1), "XHALF GI gather 4b4c3cbe, frame 0 of 4: runs");
    g_frames = 21;
    Check(!skipfx::OnListCall(L2, 0, 14, 8, 8, 1), "#67206 frame 1 of 4: runs too (the other ping-pong parity)");
    g_frames = 22;
    Check(skipfx::OnListCall(L2, 0, 14, 8, 8, 1), "frame 2 of 4: skipped");
    g_frames = 23;
    Check(skipfx::OnListCall(L2, 0, 14, 8, 8, 1), "XHALF GI gather, frame 3 of 4: skipped");
    skipfx::OnListCall(L2, 0, 25, 0x1000, 0, 0);
    Check(!skipfx::OnListCall(L2, 0, 14, 1, 1, 1), "#67915 XHALF 2: volumetric clouds always run (brown-sky guard)");
    skipfx::OnListCall(L2, 0, 25, 0x3000, 0, 0);
    Check(!skipfx::OnListCall(L2, 0, 14, 1, 1, 1), "XHALF: any other compute PSO always runs");
    g_frames = 0;
    skipfx::OnListCall(L2, 0, 25, 0x5000, 0, 0);
    Check(!skipfx::OnListCall(L2, 0, 14, 8, 8, 1), "no presents counted yet (frame 0): nothing skipped");
    printf("status: %s\n", skipfx::StatusLine(st, sizeof st));
    printf(g_fail ? "SKIPFX HARNESS: %d FAILED\n" : "SKIPFX HARNESS: ALL OK\n", g_fail);
    return g_fail ? 1 : 0;
}
