// GitHub #38 offline check: vtdisk::OriginalSlotValue recovers the real DXGI functions, and a two-hook Present loop
// (another overlay hooked before and again after us, each calling the other) ends at the depth limit instead of
// overflowing the stack. Build: cl /nologo /EHsc /std:c++17 /W4 tests\vtdisk_test.cpp d3d11.lib dxgi.lib user32.lib
#include "../src/vtdisk.h"
#include <d3d11.h>
#include <dxgi1_2.h>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, ...) do { if (!(c)) { ++g_fail; std::printf("FAIL: " __VA_ARGS__); std::printf("\n"); } } while (0)

using PresentFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
static PresentFn g_real = nullptr, g_otherOrig = nullptr, g_ourOrig = nullptr;
static thread_local int t_depth = 0;
static int g_maxDepth = 0, g_realCalls = 0;
static void** g_slot = nullptr;

static HRESULT STDMETHODCALLTYPE Counted(IDXGISwapChain* sc, UINT s, UINT f) { ++g_realCalls; return g_real(sc, s, f); }
// the other overlay: calls whatever it saved as the original (our hook, after its re-hook)
static HRESULT STDMETHODCALLTYPE OtherHook(IDXGISwapChain* sc, UINT s, UINT f) { return g_otherOrig(sc, s, f); }
// our hook, same shape as osd.cpp HkPresent's loop breaker
static HRESULT STDMETHODCALLTYPE OurHook(IDXGISwapChain* sc, UINT s, UINT f) {
    ++t_depth;
    if (t_depth > g_maxDepth) g_maxDepth = t_depth;
    HRESULT hr;
    if (t_depth >= 8) hr = Counted(sc, s, f);
    else hr = g_ourOrig(sc, s, f);
    --t_depth;
    return hr;
}

int main() {
    // 1) every slot of a fresh (unhooked) factory and swapchain resolves to the value in memory
    IDXGIFactory1* fac = nullptr;
    CHECK(SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&fac)) && fac, "CreateDXGIFactory1");
    if (!fac) return 1;
    void** fvt = *reinterpret_cast<void***>(fac);
    int same = 0;
    for (int i = 0; i < 14; ++i) {
        void* r = vtdisk::OriginalSlotValue(&fvt[i]);
        CHECK(r == fvt[i], "factory slot %d: disk %p memory %p", i, r, fvt[i]);
        same += r == fvt[i];
    }
    std::printf("factory: %d/14 slots match\n", same);

    WNDCLASSW wc{}; wc.lpfnWndProc = DefWindowProcW; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"vtdisk_test";
    RegisterClassW(&wc);
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"t", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr);
    DXGI_SWAP_CHAIN_DESC sd{}; sd.BufferCount = 2; sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; sd.BufferDesc.Width = 64; sd.BufferDesc.Height = 64;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.OutputWindow = hwnd; sd.SampleDesc.Count = 1; sd.Windowed = TRUE; sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    IDXGISwapChain* sc = nullptr; ID3D11Device* dev = nullptr; ID3D11DeviceContext* ctx = nullptr;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd, &sc, &dev, nullptr, &ctx);
    CHECK(SUCCEEDED(hr) && sc, "swapchain hr %08lX", (unsigned long)hr);
    if (!sc) return 1;
    void** vt = *reinterpret_cast<void***>(sc);
    g_real = reinterpret_cast<PresentFn>(vtdisk::OriginalSlotValue(&vt[8]));
    void* real1 = vtdisk::OriginalSlotValue(&vt[22]);
    CHECK((void*)g_real == vt[8], "Present: disk %p memory %p", (void*)g_real, vt[8]);
    CHECK(real1 == vt[22], "Present1: disk %p memory %p", real1, vt[22]);
    std::printf("swapchain: Present %s, Present1 %s\n", (void*)g_real == vt[8] ? "match" : "MISMATCH", real1 == vt[22] ? "match" : "MISMATCH");

    // 2) the #38 loop: other hooks first (orig = real), we hook (orig = other), other re-hooks (orig = ours)
    g_slot = &vt[8];
    DWORD old; VirtualProtect(g_slot, sizeof(void*), PAGE_READWRITE, &old);
    g_otherOrig = (PresentFn)*g_slot; *g_slot = (void*)&OtherHook;
    g_ourOrig = (PresentFn)*g_slot; *g_slot = (void*)&OurHook;
    g_otherOrig = (PresentFn)*g_slot; *g_slot = (void*)&OtherHook;
    hr = sc->Present(0, 0);
    std::printf("loop: Present hr %08lX, max depth %d, real calls %d\n", (unsigned long)hr, g_maxDepth, g_realCalls);
    CHECK(SUCCEEDED(hr), "looped Present failed %08lX", (unsigned long)hr);
    CHECK(g_maxDepth == 8 && g_realCalls == 1, "loop not broken at depth 8 exactly once");
    *g_slot = (void*)g_real; VirtualProtect(g_slot, sizeof(void*), old, &old);

    ctx->Release(); dev->Release(); sc->Release(); fac->Release(); DestroyWindow(hwnd);
    std::printf(g_fail ? "vtdisk_test: %d FAILED\n" : "vtdisk_test: all passed\n", g_fail);
    return g_fail ? 1 : 0;
}
