#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include "../src/vrs_selective.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
void* gpuopt_orig0[96]{}, *gpuopt_orig1[96]{}, *gpuopt_thunks0[96]{}, *gpuopt_thunks1[96]{};
int g_vtSlots[2]{79,79};
void LOG(const char*, ...) {}
#include "../src/vrs_ceiling.inl"
struct Fake { void** vt; };
void* dv[14]{}, *lv[79]{};
Fake device{dv}, list{lv};
D3D12_FEATURE_DATA_D3D12_OPTIONS6 caps{};
HRESULT queryResult=S_OK, resetResult=S_OK;
int writes=0, resets=0;
vrsselect::Meta testMeta;
HRESULT STDMETHODCALLTYPE Private(void*, REFGUID, UINT* n, void* out) { if (*n!=sizeof testMeta) return E_FAIL; memcpy(out,&testMeta,*n); return S_OK; }
void* pv[4]{}; Fake pso{pv};
D3D12_SHADING_RATE lastRate{};
ULONG STDMETHODCALLTYPE Release(void*) { return 1; }
HRESULT STDMETHODCALLTYPE Feature(void*, D3D12_FEATURE, void* out, UINT) { *(D3D12_FEATURE_DATA_D3D12_OPTIONS6*)out=caps; return queryResult; }
HRESULT STDMETHODCALLTYPE Device(void*, REFIID, void** out) { *out=&device; return S_OK; }
D3D12_COMMAND_LIST_TYPE STDMETHODCALLTYPE Type(void*) { return D3D12_COMMAND_LIST_TYPE_DIRECT; }
void STDMETHODCALLTYPE Rate(void*, D3D12_SHADING_RATE r, const D3D12_SHADING_RATE_COMBINER* c) {
    if(c[0]!=D3D12_SHADING_RATE_COMBINER_PASSTHROUGH || c[1]!=D3D12_SHADING_RATE_COMBINER_PASSTHROUGH) abort();
    ++writes; lastRate=r;
}
HRESULT STDMETHODCALLTYPE Reset(void*, void*, void*) { ++resets; return resetResult; }
#define CHECK(x) do { if(!(x)) { printf("FAIL line %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main() {
    dv[2]=(void*)&Release; dv[13]=(void*)&Feature; lv[7]=(void*)&Device; lv[8]=(void*)&Type;
    gpuopt_orig0[77]=(void*)&Rate; gpuopt_thunks0[10]=(void*)&Reset;
    auto* cl=(ID3D12GraphicsCommandList*)&list;
    g_vrsMode=0; VrsApply(cl,0,true); CHECK(writes==0);
    g_vrsMode=1; caps.VariableShadingRateTier=D3D12_VARIABLE_SHADING_RATE_TIER_NOT_SUPPORTED;
    VrsApply(cl,0,true); CHECK(writes==0);
    caps.VariableShadingRateTier=D3D12_VARIABLE_SHADING_RATE_TIER_1;
    CHECK(VrsReset<0>(cl,nullptr,nullptr)==S_OK); CHECK(writes==1 && resets==1 && g_vrsLists==1 && lastRate==D3D12_SHADING_RATE_2X2);
    g_vrsMode=2; VrsApply(cl,0,true); CHECK(lastRate==D3D12_SHADING_RATE_2X2);
    caps.VariableShadingRateTier=D3D12_VARIABLE_SHADING_RATE_TIER_2;
    VrsApply(cl,0,true); CHECK(lastRate==D3D12_SHADING_RATE_2X2);
    caps.AdditionalShadingRatesSupported=TRUE; VrsApply(cl,0,true); CHECK(lastRate==D3D12_SHADING_RATE_4X4);
    int before=writes; resetResult=E_FAIL; CHECK(VrsReset<0>(cl,nullptr,nullptr)==E_FAIL); CHECK(writes==before && resets==2);
    queryResult=E_FAIL; VrsApply(cl,0,true); CHECK(writes==before);
    queryResult=S_OK; g_vtSlots[0]=60; VrsApply(cl,0,true); CHECK(writes==before);
    g_vtSlots[0]=79; VrsApply(cl,0,true); VrsApply(cl,0,false); VrsApply(cl,0,false,true);
    CHECK(g_vrsDraws==1 && g_vrsIndirect==1);
    pv[3]=(void*)&Private;
    g_vrsMode=3; g_vrsMotionGuard=true; queryResult=S_OK;
    testMeta={}; testMeta.reject=0; testMeta.count=7;
    for (auto& f:testMeta.formats) f=DXGI_FORMAT_R8G8B8A8_UNORM;
    resetResult=S_OK; VrsReset<0>(cl,nullptr,(ID3D12PipelineState*)&pso);
    CHECK(lastRate==D3D12_SHADING_RATE_1X1); // Reset is never coarse in selective mode
    VrsApply(cl,0,false); CHECK(lastRate==D3D12_SHADING_RATE_1X1); // missing OM bind
    VrsTrack(cl,0,46,7); VrsApply(cl,0,false); CHECK(lastRate==D3D12_SHADING_RATE_2X2);
    testMeta.reject=vrsselect::Blend; VrsTrack(cl,0,25,(uintptr_t)&pso);
    VrsApply(cl,0,false); CHECK(lastRate==D3D12_SHADING_RATE_1X1); // coarse cannot leak to particles
    testMeta.reject=0; testMeta.motion=true; VrsTrack(cl,0,25,(uintptr_t)&pso);
    VrsApply(cl,0,false); CHECK(lastRate==D3D12_SHADING_RATE_1X1);
    g_vrsMotionGuard=false; VrsApply(cl,0,false); CHECK(lastRate==D3D12_SHADING_RATE_2X2);
    VrsTrack(cl,0,27,0); VrsApply(cl,0,false,true); CHECK(lastRate==D3D12_SHADING_RATE_1X1); // bundles invalidate
    VrsTrack(cl,0,25,(uintptr_t)&pso); VrsTrack(cl,0,46,7);
    VrsApply(cl,0,false); CHECK(lastRate==D3D12_SHADING_RATE_2X2);
    VrsTrack(cl,0,11,0); VrsApply(cl,0,false); CHECK(lastRate==D3D12_SHADING_RATE_1X1);
    VrsTrack(cl,0,9,0); VrsApply(cl,0,false); CHECK(lastRate==D3D12_SHADING_RATE_1X1);
    puts("VRS selective state: coarse -> excluded fine, OM unknown, motion guard, bundles, ClearState, Close PASS");
    puts("VRS ceiling: capability fallback, off, failed reset HRESULT, interface gate, counters PASS");
}
