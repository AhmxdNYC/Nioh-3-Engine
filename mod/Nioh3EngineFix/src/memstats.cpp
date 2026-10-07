// memstats.cpp - process / GPU memory line for the 30 s stats thread (see memstats.h). Measurement only.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>

#include <d3d12.h>
#include <dxgi1_4.h>

#include <cstdint>
#include <cstdio>
#include <cwchar>

#include "memstats.h"

namespace memstats {
namespace {

void (*g_log)(const char*, ...) = nullptr;
#define MLOG(...)                      \
    do {                               \
        if (g_log) g_log(__VA_ARGS__); \
    } while (0)

bool g_on = false;
uintptr_t g_base = 0;
bool g_known = false;
void* (*g_getDev)() = nullptr;

// v2.02 engine layout (notes\memory-leaks.md §5): g_RenderGlobals (GfxDevice*) at RVA 45BA588, ID3D12Device* at
// GfxDevice+4268h (RecreateSwapchainDevice 2EEA690 calls CreateCommandAllocator / CreateQueryHeap through it).
constexpr uintptr_t kRvaGfxDevice = 0x45BA588;
constexpr uintptr_t kOffD3D12Device = 0x4268;

IDXGIAdapter3* g_adapter = nullptr;  // kept for the whole session
bool g_adapterByLuid = false;
int g_adapterTries = 0;
bool g_adapterGaveUp = false;

struct Sample {
    double t = 0;  // seconds
    double priv = 0, ws = 0, commit = 0;
    double handles = 0, threads = 0;
    double vl = -1, vn = -1;  // VRAM local / non-local used, MB (-1 = unknown)
};
bool g_haveFirst = false;
Sample g_first;
LARGE_INTEGER g_qpf{};

constexpr double kMB = 1024.0 * 1024.0;

// ---- the game's D3D12 device -> adapter LUID ------------------------------------------------------------------------
// SEH only (no C++ objects here): the engine pointer chain is read while the game may be tearing the device down.
bool ReadEnginePtr(uintptr_t base, void** out) {
    __try {
        const uintptr_t g = *reinterpret_cast<volatile uintptr_t*>(base + kRvaGfxDevice);
        if (!g) return false;
        *out = *reinterpret_cast<void* volatile*>(g + kOffD3D12Device);
        return *out != nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool LuidFromDevice(void* raw, LUID* luid) {
    __try {
        ID3D12Device* d = nullptr;
        if (FAILED(static_cast<IUnknown*>(raw)->QueryInterface(__uuidof(ID3D12Device), reinterpret_cast<void**>(&d))) || !d)
            return false;
        *luid = d->GetAdapterLuid();
        d->Release();
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool GameDeviceLuid(LUID* luid) {
    void* dev = nullptr;
    if (g_getDev) dev = g_getDev();
    if (!dev && g_known && g_base) ReadEnginePtr(g_base, &dev);
    return dev && LuidFromDevice(dev, luid);
}

// CreateDXGIFactory1 from the SYSTEM dxgi.dll (full path): skips proxies in the game folder (OptiScaler's dxgi.dll,
// Streamline's sl.interposer) so no wrapper sees our factory. Under Proton system32\dxgi.dll is DXVK's dxgi.
using CreateFactory1Fn = HRESULT(WINAPI*)(REFIID, void**);
CreateFactory1Fn SystemCreateFactory1() {
    wchar_t path[MAX_PATH];
    const UINT n = GetSystemDirectoryW(path, MAX_PATH);
    if (!n || n + 10 >= MAX_PATH) return nullptr;
    wcscat_s(path, L"\\dxgi.dll");
    HMODULE m = GetModuleHandleW(path);
    if (!m) m = LoadLibraryW(path);  // kept loaded: the adapter lives for the session
    return m ? reinterpret_cast<CreateFactory1Fn>(GetProcAddress(m, "CreateDXGIFactory1")) : nullptr;
}

void LogAdapter(IDXGIAdapter3* a, const char* how) {
    DXGI_ADAPTER_DESC1 d{};
    if (FAILED(a->GetDesc1(&d))) return;
    char name[128];
    WideCharToMultiByte(CP_UTF8, 0, d.Description, -1, name, sizeof(name), nullptr, nullptr);
    MLOG("MEM: VRAM source: %s (%s), LUID %08lX:%08lX, dedicated %.0f MB, shared %.0f MB", name, how,
         (unsigned long)d.AdapterLuid.HighPart, (unsigned long)d.AdapterLuid.LowPart, (double)d.DedicatedVideoMemory / kMB,
         (double)d.SharedSystemMemory / kMB);
}

// Finds the adapter once. By LUID while the game's device is not known yet; after 10 lines (5 min) without a device it
// settles for the first hardware adapter and says so.
void EnsureAdapter() {
    if (g_adapter || g_adapterGaveUp) return;
    ++g_adapterTries;
    LUID luid{};
    const bool haveLuid = GameDeviceLuid(&luid);
    if (!haveLuid && g_adapterTries < 10) return;
    CreateFactory1Fn create = SystemCreateFactory1();
    if (!create) {
        MLOG("MEM: system dxgi.dll / CreateDXGIFactory1 not available - VRAM not logged");
        g_adapterGaveUp = true;
        return;
    }
    if (haveLuid) {
        IDXGIFactory4* f4 = nullptr;
        if (SUCCEEDED(create(__uuidof(IDXGIFactory4), reinterpret_cast<void**>(&f4))) && f4) {
            IDXGIAdapter3* a = nullptr;
            if (SUCCEEDED(f4->EnumAdapterByLuid(luid, __uuidof(IDXGIAdapter3), reinterpret_cast<void**>(&a))) && a) {
                g_adapter = a;
                g_adapterByLuid = true;
            }
            f4->Release();
        }
    }
    if (!g_adapter) {  // no IDXGIFactory4, LUID not enumerable (Proton LUID mismatch) or no device: adapter match / first hw
        IDXGIFactory1* f1 = nullptr;
        if (SUCCEEDED(create(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&f1))) && f1) {
            IDXGIAdapter1* pick = nullptr;
            IDXGIAdapter1* a1 = nullptr;
            for (UINT i = 0; !pick && f1->EnumAdapters1(i, &a1) != DXGI_ERROR_NOT_FOUND; ++i) {
                DXGI_ADAPTER_DESC1 d{};
                const bool ok = SUCCEEDED(a1->GetDesc1(&d)) && !(d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE);
                const bool match = haveLuid && d.AdapterLuid.LowPart == luid.LowPart && d.AdapterLuid.HighPart == luid.HighPart;
                if (ok && (match || !haveLuid)) pick = a1;
                else a1->Release();
            }
            if (!pick && haveLuid) {  // LUID never matched: first hardware adapter
                for (UINT i = 0; !pick && f1->EnumAdapters1(i, &a1) != DXGI_ERROR_NOT_FOUND; ++i) {
                    DXGI_ADAPTER_DESC1 d{};
                    if (SUCCEEDED(a1->GetDesc1(&d)) && !(d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) pick = a1;
                    else a1->Release();
                }
            }
            if (pick) {
                DXGI_ADAPTER_DESC1 d{};
                pick->GetDesc1(&d);
                g_adapterByLuid = haveLuid && d.AdapterLuid.LowPart == luid.LowPart && d.AdapterLuid.HighPart == luid.HighPart;
                IDXGIAdapter3* a3 = nullptr;
                if (SUCCEEDED(pick->QueryInterface(__uuidof(IDXGIAdapter3), reinterpret_cast<void**>(&a3))) && a3) g_adapter = a3;
                pick->Release();
            }
            f1->Release();
        }
    }
    if (g_adapter) {
        LogAdapter(g_adapter, g_adapterByLuid ? "the game's device, by LUID"
                                              : (haveLuid ? "LUID not found - first hardware adapter"
                                                          : "game device not found - first hardware adapter"));
    } else {
        MLOG("MEM: no IDXGIAdapter3 for the game's GPU - VRAM not logged");
        g_adapterGaveUp = true;
    }
}

bool QueryVram(DXGI_MEMORY_SEGMENT_GROUP g, double* used, double* budget) {
    if (!g_adapter) return false;
    DXGI_QUERY_VIDEO_MEMORY_INFO i{};
    if (FAILED(g_adapter->QueryVideoMemoryInfo(0, g, &i))) return false;
    *used = (double)i.CurrentUsage / kMB;
    *budget = (double)i.Budget / kMB;
    return true;
}

int CountThreads() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return -1;
    const DWORD pid = GetCurrentProcessId();
    int n = 0;
    THREADENTRY32 te{};
    te.dwSize = sizeof(te);
    for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te))
        if (te.th32OwnerProcessID == pid) ++n;
    CloseHandle(snap);
    return n;
}

}  // namespace

bool Start(const StartInfo& info) {
    g_log = info.log;
    g_base = info.base;
    g_known = info.knownBuild;
    g_getDev = info.getD3D12Device;
    g_on = info.iniPath && GetPrivateProfileIntW(L"Profiler", L"MemStats", 0, info.iniPath) != 0;
    QueryPerformanceFrequency(&g_qpf);
    if (g_on)
        MLOG("MEM: MemStats on - one 'stats mem:' line per stats period (process memory, handles, threads, VRAM)%s",
             g_getDev || g_known ? "" : "; no D3D12 device source, VRAM from the first hardware adapter");
    return g_on;
}

bool Enabled() { return g_on; }

void LogLine() {
    if (!g_on) return;
    Sample s;
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    s.t = (double)now.QuadPart / (double)g_qpf.QuadPart;

    PROCESS_MEMORY_COUNTERS_EX pm{};
    pm.cb = sizeof(pm);
    double peakWs = 0;
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pm), sizeof(pm))) {
        s.priv = (double)pm.PrivateUsage / kMB;
        s.ws = (double)pm.WorkingSetSize / kMB;
        s.commit = (double)pm.PagefileUsage / kMB;
        peakWs = (double)pm.PeakWorkingSetSize / kMB;
    }
    DWORD hc = 0;
    s.handles = GetProcessHandleCount(GetCurrentProcess(), &hc) ? (double)hc : -1;
    s.threads = CountThreads();

    EnsureAdapter();
    double vlb = 0, vnb = 0;
    if (!QueryVram(DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &s.vl, &vlb)) s.vl = -1;
    if (!QueryVram(DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &s.vn, &vnb)) s.vn = -1;

    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    const bool haveSys = GlobalMemoryStatusEx(&ms) != 0;

    char vram[128];
    if (s.vl >= 0)
        snprintf(vram, sizeof(vram), "VRAM local used/budget %.0f/%.0f MB, non-local %.0f/%.0f MB", s.vl, vlb,
                 s.vn >= 0 ? s.vn : 0.0, vnb);
    else
        snprintf(vram, sizeof(vram), "VRAM n/a");
    char sys[128] = "";
    if (haveSys)
        snprintf(sys, sizeof(sys), " | sys RAM avail %.0f MB, commit %.0f/%.0f MB", (double)ms.ullAvailPhys / kMB,
                 (double)(ms.ullTotalPageFile - ms.ullAvailPageFile) / kMB, (double)ms.ullTotalPageFile / kMB);

    if (!g_haveFirst) {
        g_first = s;
        g_haveFirst = true;
    } else if (g_first.vl < 0 && s.vl >= 0) {  // adapter found after the first line: VRAM deltas count from here
        g_first.vl = s.vl;
        g_first.vn = s.vn;
    }
    const Sample& f = g_first;
    const double mins = (s.t - f.t) / 60.0;
    char delta[200] = "";
    if (mins > 0.1)
        snprintf(delta, sizeof(delta),
                 " | since start (%.0f min): private %+.0f, ws %+.0f, handles %+.0f, threads %+.0f, VRAM local %+.0f, "
                 "non-local %+.0f",
                 mins, s.priv - f.priv, s.ws - f.ws, s.handles - f.handles, s.threads - f.threads,
                 (s.vl >= 0 && f.vl >= 0) ? s.vl - f.vl : 0.0, (s.vn >= 0 && f.vn >= 0) ? s.vn - f.vn : 0.0);

    MLOG("stats mem: private %.0f MB, working set %.0f MB (peak %.0f), commit %.0f MB, handles %.0f, threads %.0f, %s%s%s",
         s.priv, s.ws, peakWs, s.commit, s.handles, s.threads, vram, sys, delta);
}

}  // namespace memstats
