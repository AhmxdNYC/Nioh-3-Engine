// Isolated compiler admission only: no queue, window, draw, game, or installation.
// Run from extracted out/pipeline; caller verifies binding.json first.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>
#include <stdexcept>
static std::vector<char> Read(const char* name) {
    std::ifstream f(name,std::ios::binary|std::ios::ate);
    if(!f) throw std::runtime_error(name);
    auto n=f.tellg(); if(n<=0 || n>16*1024*1024) throw std::runtime_error("bad size");
    std::vector<char> b(static_cast<size_t>(n)); f.seekg(0); f.read(b.data(),n);
    if(!f) throw std::runtime_error("short read"); return b;
}
int main() { try {
    static_assert(sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC)==656,"journal ABI");
    auto vs=Read("original.dxbc"), cand=Read("candidate.dxbc"), ps=Read("material.dxbc");
    IDXGIFactory4* factory=nullptr;
    HRESULT h=CreateDXGIFactory1(IID_PPV_ARGS(&factory)); if(FAILED(h)) return 2;
    IDXGIAdapter1* adapter=nullptr; ID3D12Device* device=nullptr;
    for(UINT i=0;factory->EnumAdapters1(i,&adapter)!=DXGI_ERROR_NOT_FOUND;++i) {
        DXGI_ADAPTER_DESC1 info{}; adapter->GetDesc1(&info);
        if(!(info.Flags&DXGI_ADAPTER_FLAG_SOFTWARE) && SUCCEEDED(D3D12CreateDevice(adapter,D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device)))) break;
        adapter->Release(); adapter=nullptr;
    }
    if(!device) return 3;
    DXGI_ADAPTER_DESC1 info{}; adapter->GetDesc1(&info); LARGE_INTEGER ver{};
    HRESULT driver=adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice),&ver);
    std::printf("adapter=%ls vendor=%04x device=%04x driver_hr=%08lx driver=%lld\n",info.Description,info.VendorId,info.DeviceId,driver,ver.QuadPart);
    D3D12_FEATURE_DATA_D3D12_OPTIONS1 wave{}; h=device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1,&wave,sizeof wave);
    std::printf("WaveOps hr=%08lx enabled=%d lanes=%u..%u\n",h,wave.WaveOps,wave.WaveLaneCountMin,wave.WaveLaneCountMax);
    if(FAILED(h)||!wave.WaveOps) return 4;
    D3D12_FEATURE_DATA_SHADER_MODEL sm{D3D_SHADER_MODEL_6_5};h=device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL,&sm,sizeof sm);
    std::printf("shader_model hr=%08lx supported=%x\n",h,sm.HighestShaderModel);if(FAILED(h)||sm.HighestShaderModel<D3D_SHADER_MODEL_6_5)return 5;
    unsigned failures=0;
    for(int i=0;i<8;++i) {
        char name[32]; std::snprintf(name,sizeof name,"desc%d.bin",i);auto desc=Read(name);
        std::snprintf(name,sizeof name,"root%d.bin",i);auto root=Read(name);
        if(desc.size()!=656)throw std::runtime_error("desc size");
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};std::memcpy(&d,desc.data(),sizeof d);
        if(d.pRootSignature||d.VS.pShaderBytecode||d.PS.pShaderBytecode||d.DS.pShaderBytecode||d.HS.pShaderBytecode||d.GS.pShaderBytecode||
           d.VS.BytecodeLength||d.PS.BytecodeLength||d.DS.BytecodeLength||d.HS.BytecodeLength||d.GS.BytecodeLength||
           d.StreamOutput.pSODeclaration||d.StreamOutput.NumEntries||d.StreamOutput.pBufferStrides||d.StreamOutput.NumStrides||
           d.InputLayout.pInputElementDescs||d.InputLayout.NumElements||d.CachedPSO.pCachedBlob||d.CachedPSO.CachedBlobSizeInBytes)
            throw std::runtime_error("unexpected recorded pointer/length");
        ID3D12RootSignature* rs=nullptr;h=device->CreateRootSignature(0,root.data(),root.size(),IID_PPV_ARGS(&rs));
        std::printf("record=%d root_hr=%08lx\n",i,h);if(FAILED(h)){++failures;continue;}
        d.pRootSignature=rs;d.PS={ps.data(),ps.size()};
        for(int which=0;which<2;++which){auto& blob=which?cand:vs;d.VS={blob.data(),blob.size()};ID3D12PipelineState* pso=nullptr;
            h=device->CreateGraphicsPipelineState(&d,IID_PPV_ARGS(&pso));std::printf("record=%d %s hr=%08lx\n",i,which?"candidate":"original",h);
            if(FAILED(h))++failures; if(pso)pso->Release();}
        rs->Release();
    }
    device->Release();adapter->Release();factory->Release();std::printf("admission failures=%u; no draws executed\n",failures);return failures?1:0;
}catch(const std::exception& e){std::fprintf(stderr,"refused: %s\n",e.what());return 9;} }
