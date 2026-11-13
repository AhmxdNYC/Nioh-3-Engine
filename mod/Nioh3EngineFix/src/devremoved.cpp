// devremoved.cpp - see devremoved.h.
#include "devremoved.h"

#include <windows.h>
#include <d3d12.h>

#include <cstdio>
#include <cstring>

namespace devremoved {
namespace {
ID3D12Device* volatile g_dev = nullptr;
volatile LONG g_dredOn = 0;
}  // namespace

void NoteDevice(ID3D12Device* dev) { g_dev = dev; }
void Forget(ID3D12Device* dev) { if (g_dev == dev) g_dev = nullptr; }

void Start(const wchar_t* iniPath, bool wine) {
    if (!wine)
        if (HMODULE nt = GetModuleHandleW(L"ntdll.dll")) wine = GetProcAddress(nt, "wine_get_version") != nullptr;
    if (wine || !iniPath || GetPrivateProfileIntW(L"Debug", L"Dred", 0, iniPath) != 1) return;
    HMODULE d3d = LoadLibraryW(L"d3d12.dll");
    if (!d3d) return;
    using GetDbg = HRESULT(WINAPI*)(REFIID, void**);
    auto get = reinterpret_cast<GetDbg>(GetProcAddress(d3d, "D3D12GetDebugInterface"));
    if (!get) return;
    ID3D12DeviceRemovedExtendedDataSettings* s = nullptr;
    if (SUCCEEDED(get(IID_PPV_ARGS(&s))) && s) {
        s->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        s->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        InterlockedExchange(&g_dredOn, 1);
        s->Release();
    }
}

const char* ReasonName(long hr) {
    switch ((unsigned long)hr) {
        case 0x00000000: return "S_OK (device not removed)";
        case 0x887A0005: return "DXGI_ERROR_DEVICE_REMOVED";
        case 0x887A0006: return "DXGI_ERROR_DEVICE_HUNG (GPU timeout / TDR)";
        case 0x887A0007: return "DXGI_ERROR_DEVICE_RESET";
        case 0x887A0020: return "DXGI_ERROR_DRIVER_INTERNAL_ERROR";
        case 0x887A0001: return "DXGI_ERROR_INVALID_CALL";
        case 0x887A002B: return "DXGI_ERROR_ACCESS_DENIED (runtime removal: the GPU used memory it must not; no TDR)";
        case 0x887A0022: return "DXGI_ERROR_NOT_CURRENTLY_AVAILABLE";
        case 0x8007000E: return "E_OUTOFMEMORY";
        default: return "?";
    }
}

static void Dred(ID3D12Device* dev, char* out, size_t cap) {
    ID3D12DeviceRemovedExtendedData* d = nullptr;
    if (FAILED(dev->QueryInterface(IID_PPV_ARGS(&d))) || !d) { strncat_s(out, cap, "; DRED data not available\n", _TRUNCATE); return; }
    char b[200];
    D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT bo{};
    const HRESULT bhr = d->GetAutoBreadcrumbsOutput(&bo);
    if (FAILED(bhr)) {
        // [Debug] Dred = 1 was requested, but the game's device existed before Start ran, so DRED is not active.
        snprintf(b, sizeof b, "; DRED requested but not active on this device (breadcrumbs %08lX)\n", (unsigned long)bhr);
        strncat_s(out, cap, b, _TRUNCATE);
    } else {
        int total = 0, incomplete = 0;
        for (const D3D12_AUTO_BREADCRUMB_NODE* n = bo.pHeadAutoBreadcrumbNode; n && total < 4096; n = n->pNext, ++total) {
            const UINT last = n->pLastBreadcrumbValue ? *n->pLastBreadcrumbValue : 0;
            if (last < n->BreadcrumbCount) ++incomplete;
        }
        snprintf(b, sizeof b, "; DRED breadcrumbs: %d command list(s), %d not completed by the GPU\n", total, incomplete);
        strncat_s(out, cap, b, _TRUNCATE);
    }
    D3D12_DRED_PAGE_FAULT_OUTPUT po{};
    if (SUCCEEDED(d->GetPageFaultAllocationOutput(&po)) && po.PageFaultVA) {
        snprintf(b, sizeof b, "; DRED page fault at GPU VA 0x%llX\n", (unsigned long long)po.PageFaultVA);
        strncat_s(out, cap, b, _TRUNCATE);
    }
    d->Release();
}

void DescribeDevice(void* devp, char* out, size_t cap) {
    ID3D12Device* dev = static_cast<ID3D12Device*>(devp);
    if (!out || !cap) return;
    out[0] = 0;
    if (!dev) { snprintf(out, cap, "no game device captured (overlay not up yet or off)\n"); return; }
    __try {
        const HRESULT hr = dev->GetDeviceRemovedReason();
        snprintf(out, cap, "GetDeviceRemovedReason = %08lX %s\n", (unsigned long)hr, ReasonName(hr));
        if (g_dredOn && hr != S_OK) Dred(dev, out, cap);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        snprintf(out, cap, "GetDeviceRemovedReason faulted (%08lX): device already destroyed\n", GetExceptionCode());
    }
}

void Describe(char* out, size_t cap) { DescribeDevice(const_cast<ID3D12Device*>(g_dev), out, cap); }

}  // namespace devremoved
