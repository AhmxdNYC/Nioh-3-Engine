// GH #152: GpuOpt only patches command-list vtables that live in the D3D12 runtime, never a proxy's (ReShade dxgi.dll).
#include <d3d12.h>
#include <stdio.h>
#include "../src/listowner.h"
#pragma comment(lib, "d3d12.lib")

static int g_fail = 0;
static void Check(bool ok, const char* what) {
    printf("%s %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_fail;
}
static void* g_fakeVtable[4] = {};  // a "wrapper" vtable in a non-runtime module (this exe)

int main() {
    Check(!listowner::InD3D12Runtime(nullptr), "null is not the runtime");
    Check(!listowner::InD3D12Runtime(g_fakeVtable), "vtable in a non-runtime image is a wrapper");
    if (HMODULE dx = LoadLibraryW(L"dxgi.dll"))
        Check(!listowner::InD3D12Runtime((void*)GetProcAddress(dx, "CreateDXGIFactory1")),
              "code in dxgi.dll (ReShade's proxy name) is a wrapper");
    if (HMODULE sysd = LoadLibraryW(L"d3d12.dll"))
        Check(listowner::InD3D12Runtime((void*)GetProcAddress(sysd, "D3D12CreateDevice")), "system d3d12.dll is the runtime");
    {   // a d3d12.dll outside the system folder (a proxy mod in the game folder) is a wrapper
        wchar_t sys[MAX_PATH], tmp[MAX_PATH], src[MAX_PATH], dir[MAX_PATH], dst[MAX_PATH];
        GetSystemDirectoryW(sys, MAX_PATH);
        GetTempPathW(MAX_PATH, tmp);
        swprintf_s(src, L"%s\\d3d12.dll", sys);
        swprintf_s(dir, L"%slistowner-%lu", tmp, GetCurrentProcessId());
        CreateDirectoryW(dir, nullptr);
        swprintf_s(dst, L"%s\\d3d12.dll", dir);
        HMODULE copy = CopyFileW(src, dst, FALSE) ? LoadLibraryW(dst) : nullptr;  // full path: a second, real image
        if (copy && copy != GetModuleHandleW(L"d3d12.dll")) {
            Check(!listowner::InD3D12Runtime((void*)GetProcAddress(copy, "D3D12CreateDevice")),
                  "d3d12.dll copy outside the system folder is a wrapper");
        } else {
            printf("SKIP game-folder d3d12.dll copy (copy/load failed or loader reused the system image)\n");
        }
        if (copy) FreeLibrary(copy);
        DeleteFileW(dst);
        RemoveDirectoryW(dir);
    }
    int real = 0;
    ID3D12Device* dev = nullptr;
    if (SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), (void**)&dev)) && dev) {
        ID3D12CommandAllocator* a = nullptr;
        ID3D12GraphicsCommandList* l = nullptr;
        if (SUCCEEDED(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void**)&a)) &&
            SUCCEEDED(dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, a, nullptr, __uuidof(ID3D12GraphicsCommandList),
                                             (void**)&l))) {
            Check(listowner::InD3D12Runtime(*(void**)l), "real D3D12 command-list vtable is the runtime (hooks stay on)");
            real = 1;
            l->Release();
        }
        if (a) a->Release();
        dev->Release();
    }
    if (!real) printf("SKIP real D3D12 list (no D3D12 device on this machine)\n");
    printf(g_fail ? "FAILED %d\n" : "ALL PASS\n", g_fail);
    return g_fail ? 1 : 0;
}
