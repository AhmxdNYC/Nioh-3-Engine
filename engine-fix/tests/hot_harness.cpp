// hot_harness.cpp - ShaderHot (psoswap twin PSOs + gpuopt PSO-bind rebind) and LiveCtl on a real D3D12 device of this PC.
// No game. Run: py -I tests/run_hot_harness.py (must end "ALL HOT CHECKS OK").
// The harness exe imports D3D12CreateDevice from d3d12.dll, so psoswap::Start arms the same IAT path the game uses, and
// gpuopt hooks the runtime's command-list vtable exactly as in the game: every SetPipelineState / Reset below goes through
// the real asm thunks (gpuopt_thunks.asm) and GpuOptSlow's register rewrite.
// Checks:
//   t1 (on_ file, stock installed): PSO from A runs A while OFF, A2 after HotSet ON (SetPipelineState and Reset(initial PSO)
//      paths), A again after OFF; a PSO from an unrelated shader G is never touched;
//   t2 (off_ file, replacement installed): starts ON, PSO from B2 runs B2, after OFF runs B;
//   pipeline library: StorePipeline + LoadComputePipeline of A gets its own twin (toggles too);
//   stream CreatePipelineState of A gets a twin;
//   the game's PSO keeps its own bytecode for GetCachedBlob users (the twin is never handed out);
//   LiveCtl: control file [Live] with Gen / Tag / Hot.t1 / Tuning.X / a menu key / two bad keys -> ack gen + ok/fail
//   counts + read-backs; a pre-existing control file is ignored until it changes.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <string>
#include <vector>

#include "../src/psoswap.h"
#include "../src/gpuopt.h"
#include "../src/livectl.h"

namespace memstats {  // gpuprof.cpp's VS bind census (not part of this test)
bool BindsWanted() { return false; }
void OnBind(int, uintptr_t, uintptr_t, uintptr_t, uint16_t, uint32_t) {}
}  // namespace memstats

static std::string g_logText;
static CRITICAL_SECTION g_logCs;
static void Log(const char* fmt, ...) {
    char b[1200];
    va_list a;
    va_start(a, fmt);
    vsnprintf(b, sizeof b, fmt, a);
    va_end(a);
    EnterCriticalSection(&g_logCs);
    printf("  log| %s\n", b);
    g_logText += b;
    g_logText += "\n";
    LeaveCriticalSection(&g_logCs);
}
static bool LogHas(const char* s) {
    EnterCriticalSection(&g_logCs);
    const bool r = g_logText.find(s) != std::string::npos;
    LeaveCriticalSection(&g_logCs);
    return r;
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

static std::vector<uint8_t> Compile(int mul, int add) {
    char s[400];
    snprintf(s, sizeof s,
             "RWStructuredBuffer<uint> u : register(u0);\n"
             "[numthreads(64,1,1)] void main(uint3 id : SV_DispatchThreadID) { u[id.x] = id.x * %du + %du; }\n", mul, add);
    ID3DBlob *code = nullptr, *err = nullptr;
    if (FAILED(D3DCompile(s, strlen(s), "h", nullptr, nullptr, "main", "cs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &err))) {
        printf("compile failed: %s\n", err ? (const char*)err->GetBufferPointer() : "?");
        exit(2);
    }
    std::vector<uint8_t> v((uint8_t*)code->GetBufferPointer(), (uint8_t*)code->GetBufferPointer() + code->GetBufferSize());
    code->Release();
    return v;
}
static std::wstring Digest(const std::vector<uint8_t>& b) {
    wchar_t s[40];
    for (int i = 0; i < 16; ++i) swprintf(s + 2 * i, 3, L"%02x", b[4 + i]);
    return s;
}
static void WriteFileBytes(const std::wstring& p, const void* d, size_t n) {
    FILE* f = nullptr;
    if (_wfopen_s(&f, p.c_str(), L"wb") || !f) return;
    fwrite(d, 1, n, f);
    fclose(f);
}
static std::string ReadText(const std::wstring& p) {
    FILE* f = nullptr;
    if (_wfopen_s(&f, p.c_str(), L"rb") || !f) return "";
    char b[2048] = {};
    const size_t n = fread(b, 1, sizeof b - 1, f);
    fclose(f);
    return std::string(b, n);
}

struct Gpu {
    ID3D12Device* dev = nullptr;
    ID3D12CommandQueue* q = nullptr;
    ID3D12CommandAllocator* ca = nullptr;
    ID3D12GraphicsCommandList* cl = nullptr;
    ID3D12Fence* fence = nullptr;
    UINT64 fv = 0;
    ID3D12RootSignature* rs = nullptr;
    ID3D12Resource *buf = nullptr, *rb = nullptr;
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

// viaReset: Reset(allocator, pso) (initial-PSO path, slot 10); else Reset(allocator, null) + SetPipelineState (slot 25)
static bool Run(Gpu& g, ID3D12PipelineState* pso, int mul, int add, bool viaReset, const char* what) {
    g.ca->Reset();
    g.cl->Reset(g.ca, viaReset ? pso : nullptr);
    if (!viaReset) g.cl->SetPipelineState(pso);
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
    printf("     %-34s u[0..2] = %u %u %u (want %d %d %d) via %s\n", what, m[0], m[1], m[2], add, mul + add, 2 * mul + add,
           viaReset ? "Reset(initial PSO)" : "SetPipelineState");
    D3D12_RANGE none = {0, 0};
    g.rb->Unmap(0, &none);
    return ok;
}
static bool Both(Gpu& g, ID3D12PipelineState* p, int mul, int add, const char* what) {
    const bool a = Run(g, p, mul, add, false, what);
    const bool b = Run(g, p, mul, add, true, what);
    return a && b;
}

static ID3D12PipelineState* MakeCs(Gpu& g, const std::vector<uint8_t>& cs) {
    D3D12_COMPUTE_PIPELINE_STATE_DESC d = {};
    d.pRootSignature = g.rs;
    d.CS = {cs.data(), cs.size()};
    ID3D12PipelineState* p = nullptr;
    if (FAILED(g.dev->CreateComputePipelineState(&d, IID_PPV_ARGS(&p)))) return nullptr;
    return p;
}

// LiveCtl test doubles (the game passes menu::Remote / LiveTuning)
static int FakeRemote(const char* key, int set, int value, char* reply, size_t n, unsigned) {
    static int pp = 0;
    if (_stricmp(key, "PP")) {
        snprintf(reply, n, "no menu item '%s'", key);
        return -1;
    }
    if (set) pp = value ? 1 : 0;
    snprintf(reply, n, "Page-pool sweep (PP) = %s (value %d)", pp ? "ON" : "off", pp);
    return 0;
}
static bool FakeTuning(const char* key, const char* val, char* reply, size_t n) {
    if (_stricmp(key, "PP_Period")) return false;
    snprintf(reply, n, "PP_Period = %d", atoi(val));
    return true;
}
static std::string AckField(const std::string& ack, const char* k) {
    const std::string pat = std::string(" ") + k + "=";
    std::string a = " " + ack;
    const size_t p = a.find(pat);
    if (p == std::string::npos) return "";
    const size_t s = p + pat.size();
    size_t e = a.find_first_of(" \r\n", s);
    return a.substr(s, e == std::string::npos ? std::string::npos : e - s);
}
static std::string WaitAck(const std::wstring& ack, const char* gen, int ms) {
    for (int t = 0; t < ms; t += 50) {
        const std::string a = ReadText(ack);
        if (AckField(a, "gen") == gen) return a;
        Sleep(50);
    }
    return ReadText(ack);
}

int main() {
    InitializeCriticalSection(&g_logCs);
    wchar_t tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    const std::wstring root = std::wstring(tmp) + L"hot_harness_" + std::to_wstring(GetCurrentProcessId());
    const std::wstring hot = root + L"\\hot", ini = root + L"\\Nioh3EngineFix.ini", ctl = root + L"\\live.ini", ack = ctl + L".ack";
    CreateDirectoryW(root.c_str(), nullptr);
    CreateDirectoryW(hot.c_str(), nullptr);
    CreateDirectoryW((hot + L"\\t1").c_str(), nullptr);
    CreateDirectoryW((hot + L"\\T2").c_str(), nullptr);
    CreateDirectoryW((hot + L"\\empty").c_str(), nullptr);

    const auto A = Compile(1, 1), A2 = Compile(3, 7);    // t1: stock A installed, replacement A2
    const auto B = Compile(1, 50), B2 = Compile(5, 9);   // t2: replacement B2 installed, stock B
    const auto G = Compile(1, 600);                        // unrelated
    WriteFileBytes(hot + L"\\t1\\on_" + Digest(A) + L".dxbc", A2.data(), A2.size());
    WriteFileBytes(hot + L"\\T2\\off_" + Digest(B2) + L".dxbc", B.data(), B.size());
    WriteFileBytes(hot + L"\\empty\\readme.txt", "x", 1);
    WritePrivateProfileStringW(L"ShaderHot", L"Enabled", L"1", ini.c_str());
    WritePrivateProfileStringW(L"ShaderHot", L"Folder", L"hot", ini.c_str());
    WritePrivateProfileStringW(L"Debug", L"LiveCtl", L"1", ini.c_str());
    WritePrivateProfileStringW(L"Debug", L"LiveCtlFile", L"live.ini", ini.c_str());
    // a stale control file from an "earlier run": must not be applied at start
    WritePrivateProfileStringW(L"Live", L"Gen", L"99", ctl.c_str());
    WritePrivateProfileStringW(L"Live", L"Hot.t1", L"1", ctl.c_str());

    psoswap::StartInfo si;
    si.log = &Log;
    si.iniPath = ini.c_str();
    psoswap::Start(si);
    CHECK(psoswap::HotEnabled() && psoswap::HotSetCount() == 2, "ShaderHot on, 2 sets indexed (the empty folder is not a set): %d",
          psoswap::HotSetCount());
    CHECK(psoswap::HotGet("t1") == 0 && psoswap::HotGet("t2") == 1, "initial states: t1 OFF (on_ file), t2 ON (off_ file, folder 'T2')");
    gpuopt::StartInfo gi;
    gi.log = &Log;
    gi.iniPath = ini.c_str();
    gi.shaderHot = psoswap::HotEnabled();
    gpuopt::Start(gi);
    CHECK(gpuopt::InstallNow(), "gpuopt command-list hooks installed (Barriers = 0, ShaderHot)");
    CHECK(LogHas("ShaderHot: PSO-bind slots"), "PSO-bind slots routed");
    livectl::StartInfo li;
    li.log = &Log;
    li.iniPath = ini.c_str();
    li.remote = &FakeRemote;
    li.tuning = &FakeTuning;
    li.hotSet = &psoswap::HotSet;
    li.hotGet = &psoswap::HotGet;
    livectl::Start(li);
    CHECK(livectl::Enabled(), "LiveCtl on");

    Gpu g;
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g.dev)))) {
        printf("no D3D12 device\n");
        return 2;
    }
    CHECK(psoswap::GetState() == psoswap::State::Active, "device hooked through the IAT: %s", psoswap::StateName(psoswap::GetState()));
    Init(g);

    ID3D12PipelineState* pA = MakeCs(g, A);
    ID3D12PipelineState* pB = MakeCs(g, B2);
    ID3D12PipelineState* pG = MakeCs(g, G);
    CHECK(pA && pB && pG, "PSOs created");
    CHECK(LogHas("HOT: set 't1': twin 1 ready") && LogHas("HOT: set 't2': twin 1 ready"), "one twin per hot PSO logged");
    Sleep(400);
    CHECK(psoswap::HotGet("t1") == 0, "stale control file (Gen 99) ignored at start");

    CHECK(Both(g, pA, 1, 1, "t1 OFF: game PSO A"), "t1 OFF -> stock A output");
    CHECK(Both(g, pB, 5, 9, "t2 ON: game PSO B2"), "t2 ON -> installed replacement B2 output");
    char r[200];
    CHECK(psoswap::HotSet("T1", 1, r, sizeof r), "HotSet T1 ON (case-insensitive): %s", r);
    CHECK(Both(g, pA, 3, 7, "t1 ON: twin A2"), "t1 ON -> replacement A2 output (twin bound)");
    CHECK(Both(g, pB, 5, 9, "t2 still ON"), "t2 untouched by t1");
    CHECK(Both(g, pG, 1, 600, "unrelated G"), "unrelated PSO never rebound");
    CHECK(psoswap::HotSet("t2", 0, r, sizeof r), "HotSet t2 OFF: %s", r);
    CHECK(Both(g, pB, 1, 50, "t2 OFF: twin B"), "t2 OFF -> stock B output (twin bound)");
    CHECK(!psoswap::HotSet("nope", 1, r, sizeof r), "unknown set refused: %s", r);

    // the game's PSO is untouched: its cached blob is the stock pipeline's (non-empty, and not the twin's)
    ID3DBlob* cb = nullptr;
    const HRESULT chr = pA->GetCachedBlob(&cb);
    CHECK(SUCCEEDED(chr) && cb && cb->GetBufferSize() > 0, "game PSO GetCachedBlob works (%zu bytes) - the twin is never handed out",
          cb ? cb->GetBufferSize() : (size_t)0);
    if (cb) cb->Release();

    // stream descriptor + pipeline library get their own twins
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
        st.cs.cs = {A.data(), A.size()};
        D3D12_PIPELINE_STATE_STREAM_DESC sd = {sizeof st, &st};
        ID3D12PipelineState* ps = nullptr;
        CHECK(SUCCEEDED(d2->CreatePipelineState(&sd, IID_PPV_ARGS(&ps))) && ps && Both(g, ps, 3, 7, "stream PSO, t1 ON"),
              "stream CreatePipelineState: twin bound while t1 ON");
        if (ps) ps->Release();
        d2->Release();
    }
    ID3D12Device1* d1 = nullptr;
    if (SUCCEEDED(g.dev->QueryInterface(IID_PPV_ARGS(&d1)))) {
        ID3D12PipelineLibrary* lib = nullptr;
        if (SUCCEEDED(d1->CreatePipelineLibrary(nullptr, 0, IID_PPV_ARGS(&lib)))) {
            CHECK(SUCCEEDED(lib->StorePipeline(L"A", pA)), "library: StorePipeline(game PSO A) accepted (the game's own PSO)");
            D3D12_COMPUTE_PIPELINE_STATE_DESC d = {};
            d.pRootSignature = g.rs;
            d.CS = {A.data(), A.size()};
            ID3D12PipelineState* pl = nullptr;
            const HRESULT lh = lib->LoadComputePipeline(L"A", &d, IID_PPV_ARGS(&pl));
            CHECK(SUCCEEDED(lh) && pl && Both(g, pl, 3, 7, "library-loaded A, t1 ON"), "library load: own twin, bound while t1 ON");
            psoswap::HotSet("t1", 0, r, sizeof r);
            CHECK(pl && Both(g, pl, 1, 1, "library-loaded A, t1 OFF"), "library load: stock while t1 OFF");
            if (pl) pl->Release();
            lib->Release();
        } else {
            printf("SKIP library: CreatePipelineLibrary failed\n");
        }
        d1->Release();
    }

    // LiveCtl frame clock: one frame, a 400 ms gap (one hitch > 50 ms), then 60 frames ~5 ms apart
    {
        livectl::OnFrame();
        Sleep(400);
        const std::string a0 = ReadText(ack);
        for (int i = 0; i < 60; ++i) {
            livectl::OnFrame();
            Sleep(5);
        }
        Sleep(600);
        const std::string a1 = ReadText(ack);
        auto d = [&](const char* k) { return atoll(AckField(a1, k).c_str()) - atoll(AckField(a0, k).c_str()); };
        printf("     clock: %lld frames in %lld us, h20 %lld h33 %lld h50 %lld\n", d("frames"), d("qpc_us"), d("h20"), d("h33"), d("h50"));
        // Sleep(5) may take a whole 15.6 ms timer tick, so only the 400 ms gap is a sure hitch
        CHECK(d("frames") == 60 && d("qpc_us") > 250000 && d("qpc_us") < 5000000 && d("h50") >= 1 && d("h33") >= d("h50") &&
                  d("h20") >= d("h33") && d("h20") < 60,
              "ack frame clock: frames / qpc_us / hitch counters");
    }
    // LiveCtl: one apply with good and bad keys
    {
        const char* txt = "[Live]\r\nGen = 7\r\nTag = t1 on q1\r\nHot.t1 = 1\r\nTuning.PP_Period = 8\r\nPP = 1\r\nHot.nope = 1\r\nTuning.Bogus = 3\r\n";
        WriteFileBytes(ctl, txt, strlen(txt));
        const std::string a = WaitAck(ack, "7", 4000);
        printf("     ack: %s", a.c_str());
        CHECK(AckField(a, "gen") == "7" && AckField(a, "tag") == "t1_on_q1", "ack echoes gen 7 and the tag");
        CHECK(AckField(a, "ok") == "3" && AckField(a, "fail") == "2", "ack ok=3 fail=2 (unknown set / tuning key fail)");
        CHECK(AckField(a, "Hot.t1") == "1" && AckField(a, "PP") == "1" && AckField(a, "Tuning.PP_Period") == "8" &&
                  AckField(a, "Hot.nope") == "err",
              "ack read-backs Hot.t1=1 PP=1 Tuning.PP_Period=8 Hot.nope=err");
        CHECK(!AckField(a, "t_apply_unix_ms").empty(), "ack carries the apply time");
        CHECK(Both(g, pA, 3, 7, "after LiveCtl Hot.t1=1"), "LiveCtl switched the twin in");
        const char* off = "[Live]\r\nGen = 8\r\nTag = off\r\nHot.t1 = 0\r\n";
        WriteFileBytes(ctl, off, strlen(off));
        const std::string b = WaitAck(ack, "8", 4000);
        CHECK(AckField(b, "gen") == "8" && AckField(b, "ok") == "1" && AckField(b, "Hot.t1") == "0", "second apply: gen 8, Hot.t1=0");
        CHECK(Both(g, pA, 1, 1, "after LiveCtl Hot.t1=0"), "LiveCtl switched back to stock");
    }
    char hs[600];
    printf("     %s\n", psoswap::HotStatus(hs, sizeof hs));
    // t1: direct + stream twins; the library load adds one more unless the runtime hands back the stored object itself
    CHECK((strstr(hs, "t1=OFF (2 twin") || strstr(hs, "t1=OFF (3 twin")) && strstr(hs, "t2=OFF (1 twin") && !strstr(hs, "binds alt 0 "),
          "HotStatus: t1 2-3 twins, t2 1 twin, alt binds counted");

    pA->Release();
    pB->Release();
    pG->Release();
    printf(g_fail ? "%d CHECK(S) FAILED\n" : "ALL HOT CHECKS OK\n", g_fail);
    fflush(stdout);
    ExitProcess(g_fail ? 1 : 0);  // the LiveCtl / gpuopt threads never end
}
