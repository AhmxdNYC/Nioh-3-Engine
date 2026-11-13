// Offline check (#96825): a fake removed device yields the reason line; no device / healthy device / DRED-off paths.
#define CINTERFACE
#define COBJMACROS
#include <windows.h>
#include <d3d12.h>
#include <cstdio>
#include <cstring>
#include <cstdint>
#undef CINTERFACE
namespace devremoved { void DescribeDevice(void* dev, char* out, size_t cap); const char* ReasonName(long hr); }
static HRESULT g_hr = 0;
static HRESULT STDMETHODCALLTYPE FakeReason(ID3D12Device*) { return g_hr; }
int main() {
    static ID3D12DeviceVtbl vt;  // all other slots null: only GetDeviceRemovedReason is ever called
    vt.GetDeviceRemovedReason = FakeReason;
    struct { const ID3D12DeviceVtbl* v; } fake{&vt};
    int fail = 0;
    auto check = [&](const char* name, bool ok, const char* got) { std::printf("%s %s: %s", ok ? "PASS" : "FAIL", name, got); if (!ok) ++fail; };
    char o[256];
    g_hr = (HRESULT)0x887A0005;
    devremoved::DescribeDevice(&fake, o, sizeof o);
    check("removed", strstr(o, "887A0005") && strstr(o, "DXGI_ERROR_DEVICE_REMOVED"), o);
    g_hr = (HRESULT)0x887A002B;
    devremoved::DescribeDevice(&fake, o, sizeof o);
    check("access denied", strstr(o, "887A002B") && strstr(o, "ACCESS_DENIED"), o);
    g_hr = (HRESULT)0x887A0006;
    devremoved::DescribeDevice(&fake, o, sizeof o);
    check("hung/tdr", strstr(o, "TDR") != nullptr, o);
    g_hr = 0;
    devremoved::DescribeDevice(&fake, o, sizeof o);
    check("healthy", strstr(o, "not removed") != nullptr, o);
    devremoved::DescribeDevice(nullptr, o, sizeof o);
    check("no device", strstr(o, "no game device") != nullptr, o);
    devremoved::DescribeDevice((void*)(uintptr_t)8, o, sizeof o);  // bad pointer: SEH guard
    check("dead device", strstr(o, "faulted") != nullptr, o);
    std::printf(fail ? "devremoved test FAIL\n" : "devremoved test PASS\n");
    return fail ? 1 : 0;
}
