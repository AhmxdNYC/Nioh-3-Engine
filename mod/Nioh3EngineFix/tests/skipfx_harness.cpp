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
        default: return 0;
    }
}
int DescribeSet(uint32_t id, char* b, size_t n) {
    const char* s = id == 1   ? "cs=b76874bb17bdfc10a0739b74191c68a9"
                    : id == 2 ? "vs=e5190e82a274aaa5bfc24b14ca77567a ps=c4656a8d23602c5bacff59efa7950ab0"
                    : id == 3 ? "vs=c4656a8d00000000000000000000aaaa ps=1111111111111111111111111111bbbb"
                              : "";
    strncpy_s(b, n, s, _TRUNCATE);
    return (int)strlen(b);
}
uint32_t SetCount() { return 3; }
}  // namespace psoswap

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
    char st[256];
    printf("status: %s\n", skipfx::StatusLine(st, sizeof st));
    printf(g_fail ? "SKIPFX HARNESS: %d FAILED\n" : "SKIPFX HARNESS: ALL OK\n", g_fail);
    return g_fail ? 1 : 0;
}
