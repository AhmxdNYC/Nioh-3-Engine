// pso_check.cpp -- does the D3D12 runtime of this PC accept a replacement pixel shader in a graphics PSO with the
// game's own vertex shader (DXBC signing, VS->PS signature linkage)? Builds the PSO once with the shipped PS and once
// with the replacement. Not a game test. Built and run by pso_check.py.
// usage: pso_check.exe <vs.dxbc> <shipped_ps.dxbc> <replacement_ps.dxbc>
#include <d3d12.h>
#include <cstdio>
#include <vector>

static std::vector<char> ReadAll(const char* p) {
    std::vector<char> v;
    FILE* f = nullptr;
    if (fopen_s(&f, p, "rb") || !f) return v;
    fseek(f, 0, SEEK_END);
    v.resize((size_t)ftell(f));
    fseek(f, 0, SEEK_SET);
    if (fread(v.data(), 1, v.size(), f) != v.size()) v.clear();
    fclose(f);
    return v;
}

int main(int argc, char** argv) {
    if (argc < 4) { printf("usage: pso_check vs ps_orig ps_new\n"); return 2; }
    ID3D12Device* dev = nullptr;
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dev)))) { printf("no device\n"); return 2; }
    D3D12_DESCRIPTOR_RANGE r[3] = {};
    r[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV; r[0].NumDescriptors = 14;
    r[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; r[1].NumDescriptors = 32;
    r[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER; r[2].NumDescriptors = 16;
    D3D12_ROOT_PARAMETER rp[3] = {};
    for (int i = 0; i < 3; i++) {
        rp[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rp[i].DescriptorTable.NumDescriptorRanges = 1;
        rp[i].DescriptorTable.pDescriptorRanges = &r[i];
        rp[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    }
    D3D12_ROOT_SIGNATURE_DESC rsd = {3, rp, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE};
    ID3DBlob* rsb = nullptr;
    ID3DBlob* err = nullptr;
    if (FAILED(D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, &rsb, &err))) { printf("rs serialize\n"); return 2; }
    ID3D12RootSignature* rs = nullptr;
    if (FAILED(dev->CreateRootSignature(0, rsb->GetBufferPointer(), rsb->GetBufferSize(), IID_PPV_ARGS(&rs)))) { printf("rs\n"); return 2; }
    std::vector<char> vs = ReadAll(argv[1]);
    int fails = 0;
    for (int k = 2; k < 4; k++) {
        std::vector<char> ps = ReadAll(argv[k]);
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d = {};
        d.pRootSignature = rs;
        d.VS = {vs.data(), vs.size()};
        d.PS = {ps.data(), ps.size()};
        d.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        d.SampleMask = 0xFFFFFFFFu;
        d.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        d.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        d.RasterizerState.DepthClipEnable = TRUE;
        d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        d.NumRenderTargets = 1;
        d.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
        d.SampleDesc.Count = 1;
        ID3D12PipelineState* pso = nullptr;
        HRESULT hr = dev->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&pso));
        printf("%s PS %s: CreateGraphicsPipelineState hr=0x%08lx %s\n", k == 2 ? "shipped    " : "replacement", argv[k],
               (unsigned long)hr, SUCCEEDED(hr) ? "OK" : "FAILED");
        if (FAILED(hr)) fails++;
        if (pso) pso->Release();
    }
    rs->Release();
    dev->Release();
    return fails;
}
