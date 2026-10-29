// GPU check of the controller-display sprite path (osd.cpp + osd.hlsl): the real DXBC (osd_shaders.h), the same root signature
// (root CBV b0 font, 4 constants b1, SRV table t0 + static linear sampler s0), input layout (40-byte instances with uv), blend,
// texture upload with the full mip chain, SRV heap with null descriptors - drawn offscreen on the default adapter (WARP when
// there is none) into an R8G8B8A8 target, read back and written as TGA. Debug layer on when installed (errors fail the test).
// Usage: padviz_gpu_test.exe <outdir>   -> <outdir>\padviz_gpu_<skin>.tga
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_4.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "../src/padviz.h"
#include "../src/inputhist_dpad.h"
#include "../src/inputhist_draw.h"
#include "../src/padviz_atlas_data.h"
#include "../src/padviz_png.h"
#include "../src/osd_font.h"
#include "../../shared/osd_stroke_data.h"
#include "../src/osd_shaders.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

struct Inst { float x, y, w, h; uint32_t glyph, color; float u0, v0, u1, v1; };
static std::vector<Inst> g_inst;
namespace osd {
void DrawRect(float x, float y, float w, float h, uint32_t c) { g_inst.push_back({x, y, w, h, 0xFFFF, c, 0, 0, 0, 0}); }
float DrawText(float x, float, const char*, uint32_t, float) { return x; }
void DrawSprite(float x, float y, float w, float h, float u0, float v0, float u1, float v1, uint32_t c) {
    g_inst.push_back({x, y, w, h, 0xFFFE, c, u0, v0, u1, v1});
}
}  // namespace osd

#define HR(e) do { HRESULT h_ = (e); if (FAILED(h_)) { printf("FAIL %s: hr %08lX (line %d)\n", #e, (unsigned long)h_, __LINE__); return 1; } } while (0)

static ID3D12InfoQueue* g_iq = nullptr;
static int DebugMessages() {
    if (!g_iq) return 0;
    int bad = 0;
    for (UINT64 i = 0, n = g_iq->GetNumStoredMessages(); i < n; ++i) {
        SIZE_T len = 0;
        g_iq->GetMessage(i, nullptr, &len);
        std::vector<char> buf(len);
        auto* m = reinterpret_cast<D3D12_MESSAGE*>(buf.data());
        g_iq->GetMessage(i, m, &len);
        if (m->Severity <= D3D12_MESSAGE_SEVERITY_WARNING) { printf("D3D12 %s: %s\n", m->Severity <= D3D12_MESSAGE_SEVERITY_ERROR ? "ERROR" : "warning", m->pDescription); ++bad; }
    }
    g_iq->ClearStoredMessages();
    return bad;
}

int main(int argc, char** argv) {
    const std::string out = argc > 1 ? argv[1] : ".";
    const bool history = argc > 2 && !strcmp(argv[2], "--history");
    ID3D12Debug* dbg = nullptr;
    const bool debug = SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dbg)));
    if (debug) dbg->EnableDebugLayer();
    ID3D12Device* dev = nullptr;
    const char* adapter = "default";
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dev)))) {
        IDXGIFactory4* f = nullptr;
        HR(CreateDXGIFactory1(IID_PPV_ARGS(&f)));
        IDXGIAdapter* warp = nullptr;
        HR(f->EnumWarpAdapter(IID_PPV_ARGS(&warp)));
        HR(D3D12CreateDevice(warp, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dev)));
        adapter = "WARP";
    }
    if (debug) dev->QueryInterface(IID_PPV_ARGS(&g_iq));
    printf("device: %s, debug layer %s\n", adapter, g_iq ? "on" : "not installed");

    D3D12_COMMAND_QUEUE_DESC qd{};
    ID3D12CommandQueue* q = nullptr;
    HR(dev->CreateCommandQueue(&qd, IID_PPV_ARGS(&q)));
    ID3D12CommandAllocator* al = nullptr;
    HR(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&al)));
    ID3D12GraphicsCommandList* cl = nullptr;
    HR(dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, al, nullptr, IID_PPV_ARGS(&cl)));
    HR(cl->Close());
    ID3D12Fence* fence = nullptr;
    HR(dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
    UINT64 fv = 0;
    auto wait = [&]() { q->Signal(fence, ++fv); HANDLE e = CreateEventW(nullptr, FALSE, FALSE, nullptr); fence->SetEventOnCompletion(fv, e); WaitForSingleObject(e, 10000); CloseHandle(e); };

    // root signature exactly as osd.cpp BuildCtx
    D3D12_ROOT_PARAMETER rp[3]{};
    rp[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rp[0].Descriptor.ShaderRegister = 0;
    rp[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rp[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rp[1].Constants.ShaderRegister = 1;
    rp[1].Constants.Num32BitValues = 4;
    rp[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    D3D12_DESCRIPTOR_RANGE range{};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.NumDescriptors = 1;
    rp[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rp[2].DescriptorTable.NumDescriptorRanges = 1;
    rp[2].DescriptorTable.pDescriptorRanges = &range;
    rp[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC ss{};
    ss.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    ss.AddressU = ss.AddressV = ss.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    ss.MaxLOD = D3D12_FLOAT32_MAX;
    ss.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC rsd{3, rp, 1, &ss, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};
    ID3DBlob *blob = nullptr, *err = nullptr;
    HR(D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err));
    ID3D12RootSignature* rs = nullptr;
    HR(dev->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rs)));

    static const D3D12_INPUT_ELEMENT_DESC il[4] = {
        {"RECT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"GLYPH", 0, DXGI_FORMAT_R32_UINT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"COLOR", 0, DXGI_FORMAT_R32_UINT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"UV", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
    };
    D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};
    d.pRootSignature = rs;
    d.VS = {kOsdVS, sizeof(kOsdVS)};
    d.PS = {kOsdPS, sizeof(kOsdPS)};
    auto& b = d.BlendState.RenderTarget[0];
    b.BlendEnable = TRUE;
    b.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    b.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    b.BlendOp = D3D12_BLEND_OP_ADD;
    b.SrcBlendAlpha = D3D12_BLEND_ZERO;
    b.DestBlendAlpha = D3D12_BLEND_ONE;
    b.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    b.LogicOp = D3D12_LOGIC_OP_NOOP;
    b.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    d.SampleMask = UINT_MAX;
    d.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    d.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    d.RasterizerState.DepthClipEnable = TRUE;
    d.InputLayout = {il, 4};
    d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    d.NumRenderTargets = 1;
    d.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    d.SampleDesc.Count = 1;
    ID3D12PipelineState* pso = nullptr;
    HR(dev->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&pso)));

    int failures = DebugMessages();
    for (int style = 0; style < 2; ++style) {
        const padviz::Skin& k = padviz::SkinFor(style);
        padviz_png::Atlas A;
        if (!padviz_png::DecodeAtlas(style ? padviz::kAtlasPng_dualsense : padviz::kAtlasPng_xbox,
                                     style ? sizeof(padviz::kAtlasPng_dualsense) : sizeof(padviz::kAtlasPng_xbox), A)) { printf("FAIL decode\n"); return 1; }
        // frame: 3 panels like the CPU preview, plus a text line and a solid rect (the other two quad kinds still work)
        const float s = 420.0f / k.canvasW, pw = k.canvasW * s, ph = k.canvasH * s, gap = 24;
        const UINT W = history ? 1000u : (UINT)(3 * pw + 4 * gap), H = history ? 760u : (UINT)(ph + 2 * gap + 110);
        g_inst.clear();
        padviz::State idle, a, c;
        a.buttons = padviz::kA | padviz::kRB | padviz::kUp | padviz::kStart | padviz::kR3; a.lt = 255; a.rt = 128; a.lx = 23000; a.ly = 23000;
        c.buttons = padviz::kY | padviz::kB | padviz::kLB | padviz::kDown | padviz::kLeft | padviz::kBack | padviz::kTouch | padviz::kGuide;
        c.rt = 255; c.rx = -32768; c.ry = -8000;
        const padviz::State* st3[3] = {&idle, &a, &c};
        if (history) {
            inputhist::g_n = inputhist::g_next = 0; inputhist::g_rows = 12;
            uint64_t now = 1000;
            const uint32_t buttonsToShow[] = {padviz::kUp, padviz::kRight | padviz::kA, padviz::kDown | padviz::kX, padviz::kLeft, 0u, padviz::kA, padviz::kB, padviz::kX, padviz::kY,
                 padviz::kLB, padviz::kRB, 0u, padviz::kRB, padviz::kX | padviz::kRB,
                 padviz::kA | padviz::kB | padviz::kX | padviz::kY | padviz::kLB | padviz::kRB};
            for (uint32_t buttons : buttonsToShow) {
                inputhist::Push(buttons, 0, 0, 255, 255, now); now += 100;
            }
            inputhist_draw::Draw(2560, 1440, 24, 24, false, now, style);
        } else {
            for (int i = 0; i < 3; ++i) padviz::DrawState(*st3[i], gap + i * (pw + gap), gap, s, 0.9f, style);
        }
        const size_t nSprites = g_inst.size();
        osd::DrawRect(4, 4, 40, 8, 0xFF3C8CFF);
        for(int arm=0;arm<4;++arm) {
            const unsigned bits[]={1,8,2,4};
            osd::DrawRect(20.f+arm*59.f,(float)H-100,59,44,0xff382e29);
            inputhist_dpad::Draw(style,bits[arm],32.f+arm*59.f,(float)H-96,35);
        }
        const char* label="Startup 14 Active 15-20 Recovery 18 | Perf Ki | Mod menu";
        for(int n=0;label[n];++n) if(label[n]!=' ')
            g_inst.push_back({19.f+n*8.f,(float)H-25.f,10.f,18.f,(uint32_t)(label[n]-32),0xffffffff,0,0,0,0});

        D3D12_HEAP_PROPERTIES def{D3D12_HEAP_TYPE_DEFAULT}, up{D3D12_HEAP_TYPE_UPLOAD}, rb{D3D12_HEAP_TYPE_READBACK};
        D3D12_RESOURCE_DESC td{};
        td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        td.Width = (UINT64)A.w;
        td.Height = (UINT)A.h;
        td.DepthOrArraySize = 1;
        td.MipLevels = (UINT16)A.levels;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        ID3D12Resource* tex = nullptr;
        HR(dev->CreateCommittedResource(&def, D3D12_HEAP_FLAG_NONE, &td, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&tex)));
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp[12]{};
        UINT rows[12]{};
        UINT64 rowBytes[12]{}, total = 0;
        dev->GetCopyableFootprints(&td, 0, (UINT)A.levels, 0, fp, rows, rowBytes, &total);
        auto buffer = [&](D3D12_HEAP_PROPERTIES& hp, UINT64 size, D3D12_RESOURCE_STATES st, ID3D12Resource** r) {
            D3D12_RESOURCE_DESC bd{};
            bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            bd.Width = size; bd.Height = 1; bd.DepthOrArraySize = 1; bd.MipLevels = 1; bd.SampleDesc.Count = 1;
            bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            return dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, st, nullptr, IID_PPV_ARGS(r));
        };
        ID3D12Resource *stage = nullptr, *ub = nullptr, *rbuf = nullptr, *rt = nullptr;
        HR(buffer(up, total, D3D12_RESOURCE_STATE_GENERIC_READ, &stage));
        const UINT64 fontCb = 9216, instBytes = g_inst.size() * sizeof(Inst);
        HR(buffer(up, fontCb + instBytes, D3D12_RESOURCE_STATE_GENERIC_READ, &ub));
        uint8_t* m = nullptr;
        HR(stage->Map(0, nullptr, (void**)&m));
        int lw = A.w, lh = A.h;
        for (int l = 0; l < A.levels; ++l) {
            for (UINT y = 0; y < rows[l] && y < (UINT)lh; ++y)
                memcpy(m + fp[l].Offset + (size_t)y * fp[l].Footprint.RowPitch, &A.px[A.off[l] + (size_t)y * lw * 4], (size_t)lw * 4);
            lw = std::max(1, lw / 2); lh = std::max(1, lh / 2);
        }
        stage->Unmap(0, nullptr);
        HR(ub->Map(0, nullptr, (void**)&m));
        memset(m, 0, fontCb);
        memcpy(m, n3_osd_strokes, sizeof(n3_osd_strokes));
        memcpy(m + fontCb, g_inst.data(), instBytes);
        ub->Unmap(0, nullptr);
        D3D12_RESOURCE_DESC rd = td;
        rd.Width = W; rd.Height = H; rd.MipLevels = 1;
        rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        D3D12_CLEAR_VALUE cv{DXGI_FORMAT_R8G8B8A8_UNORM, {0.11f, 0.12f, 0.15f, 1.0f}};
        HR(dev->CreateCommittedResource(&def, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_RENDER_TARGET, &cv, IID_PPV_ARGS(&rt)));
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT rfp{};
        UINT64 rtotal = 0;
        dev->GetCopyableFootprints(&rd, 0, 1, 0, &rfp, nullptr, nullptr, &rtotal);
        HR(buffer(rb, rtotal, D3D12_RESOURCE_STATE_COPY_DEST, &rbuf));

        ID3D12DescriptorHeap *srv = nullptr, *rtvh = nullptr;
        D3D12_DESCRIPTOR_HEAP_DESC sh{D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 3, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE};
        HR(dev->CreateDescriptorHeap(&sh, IID_PPV_ARGS(&srv)));
        D3D12_DESCRIPTOR_HEAP_DESC rh{D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1};
        HR(dev->CreateDescriptorHeap(&rh, IID_PPV_ARGS(&rtvh)));
        const UINT inc = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        D3D12_SHADER_RESOURCE_VIEW_DESC nd{};
        nd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        nd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        nd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        nd.Texture2D.MipLevels = 1;
        D3D12_CPU_DESCRIPTOR_HANDLE h = srv->GetCPUDescriptorHandleForHeapStart();
        for (int i = 0; i < 3; ++i) { D3D12_CPU_DESCRIPTOR_HANDLE x{h.ptr + i * inc}; dev->CreateShaderResourceView(nullptr, &nd, x); }
        nd.Texture2D.MipLevels = (UINT)A.levels;
        D3D12_CPU_DESCRIPTOR_HANDLE slot{h.ptr + (1 + style) * inc};
        dev->CreateShaderResourceView(tex, &nd, slot);
        D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvh->GetCPUDescriptorHandleForHeapStart();
        dev->CreateRenderTargetView(rt, nullptr, rtv);

        HR(al->Reset());
        HR(cl->Reset(al, pso));
        for (int l = 0; l < A.levels; ++l) {
            D3D12_TEXTURE_COPY_LOCATION dst{tex, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX}, src{stage, D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT};
            dst.SubresourceIndex = (UINT)l;
            src.PlacedFootprint = fp[l];
            cl->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
        }
        D3D12_RESOURCE_BARRIER br{};
        br.Transition = {tex, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE};
        cl->ResourceBarrier(1, &br);
        cl->ClearRenderTargetView(rtv, cv.Color, 0, nullptr);
        cl->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        D3D12_VIEWPORT vp{0, 0, (float)W, (float)H, 0, 1};
        D3D12_RECT sr{0, 0, (LONG)W, (LONG)H};
        cl->RSSetViewports(1, &vp);
        cl->RSSetScissorRects(1, &sr);
        cl->SetGraphicsRootSignature(rs);
        cl->SetGraphicsRootConstantBufferView(0, ub->GetGPUVirtualAddress());
        cl->SetDescriptorHeaps(1, &srv);
        D3D12_GPU_DESCRIPTOR_HANDLE gh = srv->GetGPUDescriptorHandleForHeapStart();
        gh.ptr += (UINT64)(1 + style) * inc;
        cl->SetGraphicsRootDescriptorTable(2, gh);
        float consts[4] = {1.0f / W, 1.0f / H, 1.0f, 1.0f};
        cl->SetGraphicsRoot32BitConstants(1, 4, consts, 0);
        cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        D3D12_VERTEX_BUFFER_VIEW vbv{ub->GetGPUVirtualAddress() + fontCb, (UINT)instBytes, (UINT)sizeof(Inst)};
        cl->IASetVertexBuffers(0, 1, &vbv);
        cl->DrawInstanced(4, (UINT)g_inst.size(), 0, 0);
        br.Transition = {rt, 0, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE};
        cl->ResourceBarrier(1, &br);
        D3D12_TEXTURE_COPY_LOCATION rdst{rbuf, D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT}, rsrc{rt, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX};
        rdst.PlacedFootprint = rfp;
        cl->CopyTextureRegion(&rdst, 0, 0, 0, &rsrc, nullptr);
        HR(cl->Close());
        ID3D12CommandList* lists[1] = {cl};
        q->ExecuteCommandLists(1, lists);
        wait();
        HR(dev->GetDeviceRemovedReason());
        failures += DebugMessages();

        HR(rbuf->Map(0, nullptr, (void**)&m));
        const std::string path = out + "\\" + (history ? "history_" : "") + (style ? "padviz_gpu_dualsense.tga" : "padviz_gpu_xbox.tga");
        FILE* f = fopen(path.c_str(), "wb");
        if (!f) { printf("FAIL opening preview output: %s\n", path.c_str()); return 1; }
        uint8_t hd[18] = {0, 0, 2};
        hd[12] = (uint8_t)(W & 255); hd[13] = (uint8_t)(W >> 8); hd[14] = (uint8_t)(H & 255); hd[15] = (uint8_t)(H >> 8);
        hd[16] = 32; hd[17] = 0x28;
        fwrite(hd, 1, 18, f);
        long lit = 0, textLit=0, textAA=0;
        for (UINT y = 0; y < H; ++y)
            for (UINT x = 0; x < W; ++x) {
                const uint8_t* p = m + rfp.Offset + (size_t)y * rfp.Footprint.RowPitch + x * 4;
                const uint8_t bgra[4] = {p[2], p[1], p[0], 255};
                fwrite(bgra, 1, 4, f);
                if(y>=H-25 && y<H-7 && x>=19 && x<19+8*strlen(label)) {
                    if(p[0]>180 && p[1]>180 && p[2]>180)++textLit;
                    if(p[0]>50 && p[0]<180 && abs((int)p[0]-(int)p[1])<3)++textAA;
                }
                if (p[0] > 150 && p[1] > 150 && p[2] > 150) ++lit;
            }
        fclose(f);
        printf("Analytic text pixels: solid=%ld AA=%ld\n",textLit,textAA);
        if(textLit<30 || textAA<30) { printf("FAIL: analytic text missing or not antialiased\n");++failures; }
        rbuf->Unmap(0, nullptr);
        // the Xbox body is dark (human #23313) but its lit parts are bright: some share of the frame must be bright (sprites sampled), not just backdrop
        const double frac = (double)lit / ((double)W * H);
        printf("%s: %zu sprites, %ux%u, bright %.1f %% -> %s\n", style ? "DualSense" : "Xbox", nSprites, W, H, frac * 100, path.c_str());
        if (frac < 0.01) { printf("FAIL: sprites did not render\n"); ++failures; }
        for (ID3D12Resource* r : {tex, stage, ub, rbuf, rt}) r->Release();
        srv->Release();
        rtvh->Release();
    }
    printf("%s (%d failures)\n", failures ? "FAILED" : "ALL PASSED", failures);
    return failures ? 1 : 0;
}
