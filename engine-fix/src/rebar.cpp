// rebar.cpp - RB: Resizable BAR / SAM detection + one-time notice (see rebar.h, notes\rebar-fix-v3.md).
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>

#include "osd.h"
#include "rebar.h"

namespace rebar {
namespace {

void (*g_log)(const char*, ...) = nullptr;
#define RLOG(...)                      \
    do {                               \
        if (g_log) g_log(__VA_ARGS__); \
    } while (0)

std::wstring g_ini, g_marker;
uintptr_t g_base = 0;
bool g_known = false;
std::string g_wine;
int g_notice = 1;         // [General] ReBarNotice
bool g_testSession = false;   // [Debug] Tour / VirtualPad: hands-free runs, no toast
int g_forceSys = 0;           // [Profiler] VSForceSysUpload (with MemStats = 2): layout forced for an A/B
volatile LONG g_verdict = -1;

constexpr double kSysMB = 600.0;   // engine UPLOAD ring 700 MB + READBACK 100 MB; ReBAR systems show 160-212 MB non-local
constexpr double kMB = 1024.0 * 1024.0;
constexpr uintptr_t kRvaGfxDevice = 0x45BA588, kOffD3D12Device = 0x4268;   // v2.02 (memstats.cpp)

bool ReadEngineDevice(void** out) {
    __try {
        const uintptr_t g = *reinterpret_cast<volatile uintptr_t*>(g_base + kRvaGfxDevice);
        if (!g) return false;
        *out = *reinterpret_cast<void* volatile*>(g + kOffD3D12Device);
        return *out != nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// AddRef'd ID3D12Device of the engine (v2.02), or nullptr
ID3D12Device* EngineDevice() {
    if (!g_known || !g_base) return nullptr;
    void* raw = nullptr;
    if (!ReadEngineDevice(&raw)) return nullptr;
    ID3D12Device* d = nullptr;
    __try {
        if (FAILED(static_cast<IUnknown*>(raw)->QueryInterface(__uuidof(ID3D12Device), reinterpret_cast<void**>(&d)))) d = nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        d = nullptr;
    }
    return d;
}

using CreateFactory1Fn = HRESULT(WINAPI*)(REFIID, void**);
CreateFactory1Fn SystemCreateFactory1() {
    wchar_t path[MAX_PATH];
    const UINT n = GetSystemDirectoryW(path, MAX_PATH);
    if (!n || n + 10 >= MAX_PATH) return nullptr;
    wcscat_s(path, L"\\dxgi.dll");
    HMODULE m = GetModuleHandleW(path);
    if (!m) m = LoadLibraryW(path);
    return m ? reinterpret_cast<CreateFactory1Fn>(GetProcAddress(m, "CreateDXGIFactory1")) : nullptr;
}

// the game's adapter: by the engine device's LUID, else the hardware adapter with the most dedicated VRAM
IDXGIAdapter3* FindAdapter(ID3D12Device* dev, bool* byLuid) {
    *byLuid = false;
    CreateFactory1Fn create = SystemCreateFactory1();
    if (!create) return nullptr;
    IDXGIFactory4* f = nullptr;
    if (FAILED(create(__uuidof(IDXGIFactory4), reinterpret_cast<void**>(&f))) || !f) return nullptr;
    IDXGIAdapter3* out = nullptr;
    if (dev) {
        const LUID l = dev->GetAdapterLuid();
        if (SUCCEEDED(f->EnumAdapterByLuid(l, __uuidof(IDXGIAdapter3), reinterpret_cast<void**>(&out))) && out) *byLuid = true;
    }
    if (!out) {
        SIZE_T best = 0;
        IDXGIAdapter1* a = nullptr;
        for (UINT i = 0; f->EnumAdapters1(i, &a) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 d{};
            if (SUCCEEDED(a->GetDesc1(&d)) && !(d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) && d.DedicatedVideoMemory > best) {
                IDXGIAdapter3* a3 = nullptr;
                if (SUCCEEDED(a->QueryInterface(__uuidof(IDXGIAdapter3), reinterpret_cast<void**>(&a3))) && a3) {
                    if (out) out->Release();
                    out = a3;
                    best = d.DedicatedVideoMemory;
                }
            }
            a->Release();
        }
    }
    f->Release();
    return out;
}

// a D3D12 device on the adapter when the engine's is not readable (unknown build): D3D12 hands back the existing
// device of that adapter if the game made one
ID3D12Device* DeviceOn(IDXGIAdapter3* a) {
    HMODULE m = GetModuleHandleW(L"d3d12.dll");
    if (!m) return nullptr;
    using Fn = HRESULT(WINAPI*)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
    auto fn = reinterpret_cast<Fn>(GetProcAddress(m, "D3D12CreateDevice"));
    ID3D12Device* d = nullptr;
    if (!fn || FAILED(fn(a, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), reinterpret_cast<void**>(&d)))) return nullptr;
    return d;
}

bool NonLocalMB(IDXGIAdapter3* a, double* mb) {
    DXGI_QUERY_VIDEO_MEMORY_INFO i{};
    if (FAILED(a->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &i))) return false;
    *mb = (double)i.CurrentUsage / kMB;
    return true;
}

DWORD WINAPI Thread(void*) {
    // wait until the game renders (overlay Present hook) or, without the overlay, until the engine device exists + 60 s;
    // then 30 s more so the boot heaps and the first scene are in place
    const DWORD t0 = GetTickCount();
    bool rendered = false;
    for (;;) {
        if (osd::LastPresentQpc() != 0) { rendered = true; break; }
        void* raw = nullptr;
        const bool dev = g_known && ReadEngineDevice(&raw);
        if (GetTickCount() - t0 > (dev ? 60000u : 120000u)) break;
        Sleep(500);
    }
    Sleep(30000);
    ID3D12Device* dev = EngineDevice();
    bool byLuid = false;
    IDXGIAdapter3* a = FindAdapter(dev, &byLuid);
    if (!a) {
        RLOG("RB: no DXGI adapter - ReBAR check skipped");
        if (dev) dev->Release();
        return 0;
    }
    if (!dev) dev = DeviceOn(a);
    DXGI_ADAPTER_DESC1 ad{};
    a->GetDesc1(&ad);
    char name[128];
    WideCharToMultiByte(CP_UTF8, 0, ad.Description, -1, name, sizeof name, nullptr, nullptr);
    int uma = -1, gpuUp = -1;   // -1 n/a
    if (dev) {
        D3D12_FEATURE_DATA_ARCHITECTURE arch{};
        if (SUCCEEDED(dev->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE, &arch, sizeof arch))) uma = arch.UMA ? 1 : 0;
        struct Opt16 { BOOL dynDepthBias; BOOL gpuUploadHeap; } o16{};   // D3D12_OPTIONS16 (feature 43), raw for older SDKs
        if (SUCCEEDED(dev->CheckFeatureSupport((D3D12_FEATURE)43, &o16, sizeof o16))) gpuUp = o16.gpuUploadHeap ? 1 : 0;
        dev->Release();
    }
    double s[10];
    int ns = 0;
    for (int i = 0; i < 10; ++i) {
        double mb = 0;
        if (NonLocalMB(a, &mb)) s[ns++] = mb;
        if (i < 9) Sleep(2000);
    }
    a->Release();
    double med = -1;
    if (ns) {
        std::sort(s, s + ns);
        med = s[ns / 2];
    }
    const bool sys = uma != 1 && gpuUp != 1 && med >= kSysMB;
    InterlockedExchange(&g_verdict, sys ? 1 : 0);
    RLOG("RB: %s (%s), dedicated %.0f MB, UMA %s, GPUUploadHeapSupported %s, DXGI non-local usage %.0f MB (median of %d) -> "
         "UPLOAD heaps in %s%s",
         name, byLuid ? "the game's device" : "largest adapter", (double)ad.DedicatedVideoMemory / kMB,
         uma < 0 ? "n/a" : uma ? "yes" : "no", gpuUp < 0 ? "n/a" : gpuUp ? "YES" : "no", med, ns,
         sys ? "SYSTEM MEMORY (no Resizable BAR / SAM)" : "VRAM (Resizable BAR / SAM on, or an APU)",
         rendered ? "" : " [overlay not hooked]");
    if (!sys) return 0;
    if (g_forceSys) {
        RLOG("RB: [Profiler] VSForceSysUpload = %d forces this layout (A/B run) - no notice", g_forceSys);
        return 0;
    }
    if (!g_notice) return 0;
    RLOG("RB: Resizable BAR / Smart Access Memory is off: enabling it in the BIOS can give up to ~30%% more fps in this game "
         "(see README)");
    if (g_testSession) {
        RLOG("RB: hands-free test session ([Debug] Tour / VirtualPad) - no toast");
        return 0;
    }
    if (GetFileAttributesW(g_marker.c_str()) != INVALID_FILE_ATTRIBUTES) {
        RLOG("RB: notice already shown once (Nioh3EngineFix.rebarnotice exists) - no toast");
        return 0;
    }
    if (osd::LastPresentQpc() == 0) {
        RLOG("RB: overlay not hooked (FPS counter, popup and menu off?) - log line only, toast not shown");
        return 0;
    }
    static const char* const kLines[3] = {
        "Resizable BAR / Smart Access Memory is OFF",
        "Enabling it in the BIOS can give up to ~30% more fps in this game (see README).",
        "Shown once. Set [General] ReBarNotice = 0 to hide it for good."};
    osd::ShowNotice(kLines, 3, 25.0f);
    for (int i = 0; i < 1200 && !osd::NoticeDone(); ++i) Sleep(1000);   // 20 min max (title screen idle, menus, ...)
    if (!osd::NoticeDone()) return 0;
    HANDLE f = CreateFileW(g_marker.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f != INVALID_HANDLE_VALUE) {
        static const char kText[] =
            "Nioh3EngineFix: the Resizable BAR notice was shown. Delete this file to see it again; set [General] ReBarNotice = 0 "
            "in Nioh3EngineFix.ini to skip the check.\r\n";
        DWORD w = 0;
        WriteFile(f, kText, (DWORD)(sizeof kText - 1), &w, nullptr);
        CloseHandle(f);
        RLOG("RB: notice shown - marker written, it will not show again");
    } else {
        RLOG("RB: notice shown, marker not writable (%lu) - it may show again next launch", GetLastError());
    }
    return 0;
}

}  // namespace

void Start(const StartInfo& info) {
    g_log = info.log;
    g_base = info.base;
    g_known = info.knownBuild;
    if (info.wine) g_wine = info.wine;
    if (!info.iniPath) return;
    g_ini = info.iniPath;
    if (info.markerPath) g_marker = info.markerPath;
    g_notice = (int)GetPrivateProfileIntW(L"General", L"ReBarNotice", 1, info.iniPath) != 0 ? 1 : 0;
    const bool mirrorAuto = GetPrivateProfileIntW(L"GpuOpt", L"SceneBufMirror", 0, info.iniPath) == 1;   // SBM auto needs the verdict
    g_testSession = GetPrivateProfileIntW(L"Debug", L"Tour", 0, info.iniPath) != 0 ||
                    GetPrivateProfileIntW(L"Debug", L"VirtualPad", 0, info.iniPath) != 0;
    g_forceSys = GetPrivateProfileIntW(L"Profiler", L"MemStats", 0, info.iniPath) >= 2
                     ? (int)GetPrivateProfileIntW(L"Profiler", L"VSForceSysUpload", 0, info.iniPath)
                     : 0;
    if (!g_notice && !mirrorAuto) {
        RLOG("RB: [General] ReBarNotice = 0 - Resizable BAR check off");
        return;
    }
    if (!g_wine.empty()) {
        InterlockedExchange(&g_verdict, 0);
        RLOG("RB: Wine / Proton %s - Resizable BAR check skipped (vkd3d-proton places UPLOAD heaps itself)", g_wine.c_str());
        return;
    }
    if (HANDLE h = CreateThread(nullptr, 0, Thread, nullptr, 0, nullptr)) {
        SetThreadPriority(h, THREAD_PRIORITY_BELOW_NORMAL);
        CloseHandle(h);
    }
}

int UploadInSystemMemory() { return (int)g_verdict; }

}  // namespace rebar
