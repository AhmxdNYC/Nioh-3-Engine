// psoswap_harness.cpp - psoswap on a real D3D12 device of this PC, no game (run: py -I tests/run_psoswap_harness.py).
// The harness exe imports D3D12CreateDevice from d3d12.dll, so psoswap::Start arms the same IAT path the game uses
// (the game imports it from sl.interposer.dll). Compute shaders are compiled at run time (FXC, SM5 DXBC), the
// replacement is written as <Folder>\<digest>.dxbc, and every PSO is dispatched and read back.
// Checks: IAT hook fires and hooks the device; CreateComputePipelineState swap (output = replacement); stream
// CreatePipelineState swap; pipeline-library LoadComputePipeline swap; no replacement -> original; stage mismatch ->
// original + skipped; garbage file -> original; replacement the runtime rejects (u1 not in root signature) -> original
// output, rejected counted, key disabled; DumpMode 2 writes blobs; InstallNow idempotent; StatusLine.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstdarg>
#include <string>
#include <vector>

#include "../src/psoswap.h"

static std::string g_logText;
static void Log(const char* fmt, ...) {
    char b[1024];
    va_list a;
    va_start(a, fmt);
    vsnprintf(b, sizeof b, fmt, a);
    va_end(a);
    printf("  log| %s\n", b);
    g_logText += b;
    g_logText += "\n";
}

static int g_fail = 0;
#define CHECK(c, ...)                         \
    do {                                      \
        if (c) {                              \
            printf("OK   ");                  \
        } else {                              \
            printf("FAIL ");                  \
            ++g_fail;                         \
        }                                     \
        printf(__VA_ARGS__);                  \
        printf("\n");                         \
    } while (0)

static std::vector<uint8_t> Compile(const char* src, const char* target) {
    ID3DBlob *code = nullptr, *err = nullptr;
    HRESULT hr = D3DCompile(src, strlen(src), "h", nullptr, nullptr, "main", target, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &err);
    if (FAILED(hr)) {
        printf("compile failed: %s\n", err ? (const char*)err->GetBufferPointer() : "?");
        exit(2);
    }
    std::vector<uint8_t> v((uint8_t*)code->GetBufferPointer(), (uint8_t*)code->GetBufferPointer() + code->GetBufferSize());
    code->Release();
    if (err) err->Release();
    return v;
}

static std::string Digest(const std::vector<uint8_t>& b) {
    char s[40];
    for (int i = 0; i < 16; ++i) snprintf(s + 2 * i, 3, "%02x", b[4 + i]);
    return s;
}

// HASH-part digest of a DXIL container (what RGP / dxc -dumpbin show), or "" when there is none.
static std::string HashDigest(const std::vector<uint8_t>& b) {
    uint32_t n;
    memcpy(&n, &b[28], 4);
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t off;
        memcpy(&off, &b[32 + 4 * i], 4);
        if (!memcmp(&b[off], "HASH", 4)) {
            char s[40];
            for (int k = 0; k < 16; ++k) snprintf(s + 2 * k, 3, "%02x", b[off + 12 + k]);
            return s;
        }
    }
    return "";
}

static std::vector<uint8_t> ReadBytes(const char* p) {
    std::vector<uint8_t> v;
    FILE* f = nullptr;
    if (fopen_s(&f, p, "rb") || !f) return v;
    fseek(f, 0, SEEK_END);
    v.resize((size_t)ftell(f));
    fseek(f, 0, SEEK_SET);
    if (fread(v.data(), 1, v.size(), f) != v.size()) v.clear();
    fclose(f);
    return v;
}

static void WriteFileBytes(const std::wstring& p, const void* d, size_t n) {
    FILE* f = nullptr;
    if (_wfopen_s(&f, p.c_str(), L"wb") || !f) return;
    fwrite(d, 1, n, f);
    fclose(f);
}

static std::string CsSrc(int mul, int add, bool useU1 = false) {
    char s[512];
    snprintf(s, sizeof s,
             "RWStructuredBuffer<uint> u : register(u0);\n%s"
             "[numthreads(64,1,1)] void main(uint3 id : SV_DispatchThreadID) { u[id.x] = id.x * %du + %du; %s}\n",
             useU1 ? "RWStructuredBuffer<uint> v : register(u1);\n" : "", mul, add, useU1 ? "v[id.x] = 1u; " : "");
    return s;
}

struct Gpu {
    ID3D12Device* dev = nullptr;
    ID3D12CommandQueue* q = nullptr;
    ID3D12CommandAllocator* ca = nullptr;
    ID3D12GraphicsCommandList* cl = nullptr;
    ID3D12Fence* fence = nullptr;
    UINT64 fv = 0;
    ID3D12RootSignature* rs = nullptr;
    ID3D12Resource* buf = nullptr;
    ID3D12Resource* rb = nullptr;
};
constexpr UINT kN = 256;

static void Init(Gpu& g) {
    D3D12_COMMAND_QUEUE_DESC qd = {D3D12_COMMAND_LIST_TYPE_DIRECT};
    g.dev->CreateCommandQueue(&qd, IID_PPV_ARGS(&g.q));
    g.dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g.ca));
    g.dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g.ca, nullptr, IID_PPV_ARGS(&g.cl));
    g.cl->Close();
    g.dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g.fence));
    D3D12_ROOT_PARAMETER p = {};
    p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
    p.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    D3D12_ROOT_SIGNATURE_DESC rd = {1, &p, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE};
    ID3DBlob *b = nullptr, *e = nullptr;
    D3D12SerializeRootSignature(&rd, D3D_ROOT_SIGNATURE_VERSION_1, &b, &e);
    g.dev->CreateRootSignature(0, b->GetBufferPointer(), b->GetBufferSize(), IID_PPV_ARGS(&g.rs));
    b->Release();
    D3D12_HEAP_PROPERTIES hp = {D3D12_HEAP_TYPE_DEFAULT};
    D3D12_RESOURCE_DESC r = {};
    r.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    r.Width = kN * 4;
    r.Height = r.DepthOrArraySize = r.MipLevels = 1;
    r.SampleDesc.Count = 1;
    r.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    r.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    g.dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &r, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&g.buf));
    hp.Type = D3D12_HEAP_TYPE_READBACK;
    r.Flags = D3D12_RESOURCE_FLAG_NONE;
    g.dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &r, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&g.rb));
}

// Dispatches pso over kN threads; returns true when u[i] == i*mul+add for all i.
static bool Run(Gpu& g, ID3D12PipelineState* pso, int mul, int add) {
    g.ca->Reset();
    g.cl->Reset(g.ca, pso);
    g.cl->SetComputeRootSignature(g.rs);
    g.cl->SetComputeRootUnorderedAccessView(0, g.buf->GetGPUVirtualAddress());
    g.cl->Dispatch(kN / 64, 1, 1);
    D3D12_RESOURCE_BARRIER b = {};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = g.buf;
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    g.cl->ResourceBarrier(1, &b);
    g.cl->CopyResource(g.rb, g.buf);
    std::swap(b.Transition.StateBefore, b.Transition.StateAfter);
    g.cl->ResourceBarrier(1, &b);
    g.cl->Close();
    ID3D12CommandList* l = g.cl;
    g.q->ExecuteCommandLists(1, &l);
    g.q->Signal(g.fence, ++g.fv);
    while (g.fence->GetCompletedValue() < g.fv) Sleep(1);
    uint32_t* m = nullptr;
    D3D12_RANGE rr = {0, kN * 4};
    g.rb->Map(0, &rr, (void**)&m);
    bool ok = true;
    for (UINT i = 0; i < kN; ++i) ok &= m[i] == i * (UINT)mul + (UINT)add;
    printf("     u[0..2] = %u %u %u (want %d %d %d)\n", m[0], m[1], m[2], add, mul + add, 2 * mul + add);
    D3D12_RANGE none = {0, 0};
    g.rb->Unmap(0, &none);
    return ok;
}

static ID3D12PipelineState* MakeCs(Gpu& g, const std::vector<uint8_t>& cs) {
    D3D12_COMPUTE_PIPELINE_STATE_DESC d = {};
    d.pRootSignature = g.rs;
    d.CS = {cs.data(), cs.size()};
    ID3D12PipelineState* p = nullptr;
    HRESULT hr = g.dev->CreateComputePipelineState(&d, IID_PPV_ARGS(&p));
    if (FAILED(hr)) printf("     CreateComputePipelineState hr %08lX\n", (unsigned long)hr);
    return p;
}

int main(int argc, char** argv) {
    wchar_t tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    const std::wstring root = std::wstring(tmp) + L"psoswap_harness_" + std::to_wstring(GetCurrentProcessId());
    const std::wstring folder = root + L"\\repl", ini = root + L"\\Nioh3EngineFix.ini";
    CreateDirectoryW(root.c_str(), nullptr);
    CreateDirectoryW(folder.c_str(), nullptr);

    // shaders: A swapped (plain), B stage mismatch, C garbage, D rejected, E stream, F library, G no replacement
    auto A = Compile(CsSrc(1, 1).c_str(), "cs_5_0"), A2 = Compile(CsSrc(3, 7).c_str(), "cs_5_0");
    auto B = Compile(CsSrc(1, 100).c_str(), "cs_5_0");
    auto C = Compile(CsSrc(1, 200).c_str(), "cs_5_0");
    auto D = Compile(CsSrc(1, 300).c_str(), "cs_5_0"), D2 = Compile(CsSrc(5, 5, true).c_str(), "cs_5_0");
    auto E = Compile(CsSrc(1, 400).c_str(), "cs_5_0"), E2 = Compile(CsSrc(7, 11).c_str(), "cs_5_0");
    auto F = Compile(CsSrc(1, 500).c_str(), "cs_5_0"), F2 = Compile(CsSrc(9, 13).c_str(), "cs_5_0");
    auto G = Compile(CsSrc(1, 600).c_str(), "cs_5_0");
    auto VS = Compile("float4 main(uint i : SV_VertexID) : SV_Position { return float4(i, 0, 0, 1); }", "vs_5_0");
    auto W = [&](const std::vector<uint8_t>& orig, const std::vector<uint8_t>& repl, const wchar_t* ext) {
        const std::string d = Digest(orig);
        WriteFileBytes(folder + L"\\" + std::wstring(d.begin(), d.end()) + ext, repl.data(), repl.size());
    };
    W(A, A2, L".dxbc");
    W(B, VS, L".dxbc");
    const char junk[] = "DXBC this is not a container";
    {
        const std::string d = Digest(C);
        WriteFileBytes(folder + L"\\" + std::wstring(d.begin(), d.end()) + L".bin", junk, sizeof junk);
    }
    W(D, D2, L".cso");
    W(E, E2, L".dxbc");
    W(F, F2, L".dxbc");
    // H: DXIL (dxc cs_6_0, built by the runner: argv[1] = original u = id+800, argv[2] = replacement u = id*11+17);
    // the replacement is named by the HASH-part digest, not the container digest
    std::vector<uint8_t> H, H2;
    if (argc >= 3) {
        H = ReadBytes(argv[1]);
        H2 = ReadBytes(argv[2]);
    }
    const std::string hh = H.size() > 64 ? HashDigest(H) : std::string();
    if (!hh.empty()) WriteFileBytes(folder + L"\\" + std::wstring(hh.begin(), hh.end()) + L".dxil", H2.data(), H2.size());
    WritePrivateProfileStringW(L"ShaderSwap", L"Enabled", L"1", ini.c_str());
    WritePrivateProfileStringW(L"ShaderSwap", L"Folder", L"repl", ini.c_str());
    WritePrivateProfileStringW(L"ShaderSwap", L"DumpMode", L"2", ini.c_str());
    WritePrivateProfileStringW(L"ShaderSwap", L"LogAll", L"1", ini.c_str());
    WritePrivateProfileStringW(L"Profiler", L"GPUProf", L"2", ini.c_str());  // GP2: PSO -> shader-set map

    psoswap::StartInfo si;
    si.log = &Log;
    si.iniPath = ini.c_str();
    psoswap::Start(si);
    CHECK(psoswap::GetState() == psoswap::State::Waiting, "Start: state %s (want waiting)", psoswap::StateName(psoswap::GetState()));
    CHECK(g_logText.find("IAT hook on d3d12.dll!D3D12CreateDevice armed") != std::string::npos, "IAT hook armed");

    Gpu g;
    HRESULT hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g.dev));  // through the IAT slot
    if (FAILED(hr)) {
        printf("no D3D12 device (hr %08lX)\n", (unsigned long)hr);
        return 2;
    }
    CHECK(psoswap::GetState() == psoswap::State::Active, "device created through the IAT hook: state %s (want active)",
          psoswap::StateName(psoswap::GetState()));
    Init(g);

    ID3D12PipelineState* p = MakeCs(g, A);
    CHECK(p && Run(g, p, 3, 7), "A: CreateComputePipelineState swapped -> replacement output");
    {   // GP2: the swapped PSO is filed under the ORIGINAL shader's digest; the map answers by PSO pointer
        char d[400] = {};
        const uint32_t id = psoswap::PsoSetId(p);
        const int n = psoswap::DescribeSet(id, d, sizeof d);
        CHECK(psoswap::Tracking() && id != 0 && n > 3 && !strncmp(d, "cs=", 3), "GP2: PSO -> set %u \"%s\"", id, d);
        CHECK(psoswap::PsoSetId((const void*)0x1234560) == 0, "GP2: unknown pointer -> 0");
    }
    if (p) p->Release();
    p = MakeCs(g, G);
    CHECK(p && Run(g, p, 1, 600), "G: no replacement -> original output");
    if (p) p->Release();
    p = MakeCs(g, B);
    CHECK(p && Run(g, p, 1, 100), "B: replacement is a vs -> original output");
    if (p) p->Release();
    CHECK(g_logText.find("replacement is a vs shader, the original a cs shader") != std::string::npos, "B: stage mismatch logged");
    p = MakeCs(g, C);
    CHECK(p && Run(g, p, 1, 200), "C: garbage replacement -> original output");
    if (p) p->Release();
    p = MakeCs(g, D);
    CHECK(p && Run(g, p, 1, 300), "D: runtime-rejected replacement (u1 not in root sig) -> original output");
    if (p) p->Release();
    CHECK(g_logText.find("REJECTED") != std::string::npos, "D: rejection logged");
    p = MakeCs(g, D);  // key disabled: passes straight through now
    CHECK(p && Run(g, p, 1, 300), "D again: disabled key -> original output");
    if (p) p->Release();

    if (!hh.empty()) {
        p = MakeCs(g, H);
        CHECK(p && Run(g, p, 11, 17), "H: DXIL shader, replacement keyed by its HASH digest %s -> replacement output", hh.c_str());
        if (p) p->Release();
    } else {
        printf("SKIP H: no DXIL test blobs (runner did not find dxc)\n");
    }

    // stream descriptor (ID3D12Device2::CreatePipelineState)
    ID3D12Device2* d2 = nullptr;
    if (SUCCEEDED(g.dev->QueryInterface(IID_PPV_ARGS(&d2)))) {
        struct alignas(void*) Stream {
            struct alignas(void*) {
                D3D12_PIPELINE_STATE_SUBOBJECT_TYPE t = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE;
                ID3D12RootSignature* rs;
            } rs;
            struct alignas(void*) {
                D3D12_PIPELINE_STATE_SUBOBJECT_TYPE t = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS;
                D3D12_SHADER_BYTECODE cs;
            } cs;
        } st;
        st.rs.rs = g.rs;
        st.cs.cs = {E.data(), E.size()};
        D3D12_PIPELINE_STATE_STREAM_DESC sd = {sizeof st, &st};
        hr = d2->CreatePipelineState(&sd, IID_PPV_ARGS(&p));
        CHECK(SUCCEEDED(hr) && Run(g, p, 7, 11), "E: stream CreatePipelineState swapped -> replacement output");
        CHECK(st.cs.cs.pShaderBytecode == E.data(), "E: caller's stream untouched");
        if (SUCCEEDED(hr)) p->Release();
        d2->Release();
    } else {
        printf("SKIP E: no ID3D12Device2\n");
    }

    // pipeline library (ID3D12Device1::CreatePipelineLibrary -> LoadComputePipeline)
    ID3D12Device1* d1 = nullptr;
    if (SUCCEEDED(g.dev->QueryInterface(IID_PPV_ARGS(&d1)))) {
        ID3D12PipelineLibrary* lib = nullptr;
        hr = d1->CreatePipelineLibrary(nullptr, 0, IID_PPV_ARGS(&lib));
        if (SUCCEEDED(hr)) {
            D3D12_COMPUTE_PIPELINE_STATE_DESC d = {};
            d.pRootSignature = g.rs;
            d.CS = {F.data(), F.size()};
            p = nullptr;
            hr = lib->LoadComputePipeline(L"F", &d, IID_PPV_ARGS(&p));
            CHECK(SUCCEEDED(hr) && p && Run(g, p, 9, 13), "F: LoadComputePipeline swapped -> replacement output");
            if (p) p->Release();
            d.CS = {G.data(), G.size()};
            p = nullptr;
            hr = lib->LoadComputePipeline(L"G", &d, IID_PPV_ARGS(&p));
            CHECK(FAILED(hr) && !p, "G: library load without replacement -> stock miss (hr %08lX)", (unsigned long)hr);
            lib->Release();
        } else {
            printf("SKIP F: CreatePipelineLibrary hr %08lX\n", (unsigned long)hr);
        }
        d1->Release();
    }

    CHECK(psoswap::InstallNow(), "InstallNow idempotent (already hooked)");
    psoswap::Stats s;
    psoswap::GetStats(s);
    char line[300];
    printf("     %s\n", psoswap::StatusLine(line, sizeof line));
    const uint32_t wantRepl = hh.empty() ? 6u : 7u;
    CHECK(s.replacements == wantRepl, "%u replacement files indexed (%u)", wantRepl, s.replacements);
    CHECK(s.swappedPsos >= 2 && s.rejected == 1 && s.skipped == 3, "stats: swapped %llu, rejected %llu, skipped %llu",
          (unsigned long long)s.swappedPsos, (unsigned long long)s.rejected, (unsigned long long)s.skipped);
    WIN32_FIND_DATAW fd;
    int dumped = 0;
    for (const wchar_t* pat : {L"\\dump\\*.dxbc", L"\\dump\\*.dxil"}) {
        HANDLE h = FindFirstFileW((folder + pat).c_str(), &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do ++dumped;
            while (FindNextFileW(h, &fd));
            FindClose(h);
        }
    }
    CHECK(dumped >= 7 && (uint64_t)dumped == s.dumped, "DumpMode 2: %d blobs in dump\\ (stats %llu)", dumped, (unsigned long long)s.dumped);

    g.rb->Release();
    g.buf->Release();
    g.rs->Release();
    g.fence->Release();
    g.cl->Release();
    g.ca->Release();
    g.q->Release();
    g.dev->Release();
    printf(g_fail ? "%d CHECK(S) FAILED\n" : "ALL PSOSWAP CHECKS OK\n", g_fail);
    return g_fail ? 1 : 0;
}
