// GH #152: is a D3D12 command-list vtable owned by the D3D12 runtime itself (D3D12Core.dll / d3d12.dll, which also
// covers vkd3d-proton under Wine)? Any other owner is a wrapper - ReShade's dxgi.dll or another proxy in the game folder -
// and GpuOpt must not patch it: deferring barriers through ReShade's wrapper crashed inside it 12 ms after install.
#pragma once
#include <windows.h>
#include <wchar.h>

namespace listowner {
inline bool InD3D12Runtime(const void* p) {
    HMODULE m = nullptr;
    if (!p || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                  (LPCWSTR)p, &m))
        return false;
    wchar_t w[MAX_PATH] = {};
    if (!GetModuleFileNameW(m, w, MAX_PATH)) return false;
    const wchar_t* f = wcsrchr(w, L'\\');
    f = f ? f + 1 : w;
    if (_wcsicmp(f, L"D3D12Core.dll") == 0) return true;  // system or the game's Agility SDK folder
    if (_wcsicmp(f, L"d3d12.dll") != 0) return false;
    // d3d12.dll counts only from the system folder (Windows, or vkd3d-proton in Wine's system32): a d3d12.dll in the
    // game folder is a proxy mod
    wchar_t sys[MAX_PATH] = {};
    const UINT n = GetSystemDirectoryW(sys, MAX_PATH);
    return n && n < MAX_PATH && (size_t)(f - w) == (size_t)n + 1 && _wcsnicmp(w, sys, n) == 0;
}
}  // namespace listowner
