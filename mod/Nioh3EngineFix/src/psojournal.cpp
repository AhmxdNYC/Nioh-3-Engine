// psojournal.cpp - PJ: PSO journal + title-screen prewarm. Design and safety argument: psojournal.h, notes\stutter.md 3.1.
//
// File format (little-endian; one file per device class, <ini folder>\Nioh3EngineFix_pj\pj_<os>_<gpu>_sm<NN>.bin):
//   header 48 B: 'N3PJ', version, Nioh3.exe TimeDateStamp, SizeOfImage, device-class string (32 B). A different exe build
//                or device class starts a new journal.
//   chunks:      {u32 tag, u32 len, u64 sum = Hash128(payload).a} + payload, appended in first-seen order.
//     'SHDR'  key(16) + shader container (one per distinct shader)
//     'RSIG'  key(16) + node mask(4) + pad(4) + serialized root signature (one per distinct blob)
//     'PSO1'  record (below); its key = Hash128(record), one per distinct desc
//   A chunk whose header or record checksum fails ends the journal there (the tail is cut: a crash mid-append costs only
//   the last chunk). A shader / root-signature chunk is verified when the prewarm reads it.
//   record: u8 kind (1 graphics desc, 2 compute desc, 3 stream), u8 version 1, u8 shader count, u8 has input layout,
//           u32 fixed length, u32 root-signature offset (~0 = none), u32 0, key(16) root signature,
//           fixed bytes (the desc / stream with every pointer zeroed and every padding byte zero),
//           shader count x {u32 offset, u32 length, key(16)},
//           [input layout: u32 offset, u32 n, n x {6 x u32 element fields, u8 name length, name}]
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <atomic>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "psojournal.h"

namespace pj {
namespace {

// ---- constants -----------------------------------------------------------------------------------------------------------
constexpr int kSlotCreateGraphics = 10;        // ID3D12Device::CreateGraphicsPipelineState
constexpr int kSlotCreateCompute = 11;         // ID3D12Device::CreateComputePipelineState
constexpr int kSlotCreateRootSignature = 16;   // ID3D12Device::CreateRootSignature
constexpr int kSlotCreatePipelineState = 47;   // ID3D12Device2::CreatePipelineState
constexpr int kMaxSets = 4;
constexpr uint32_t kMagic = 0x4A50334E;        // "N3PJ"
constexpr uint32_t kVersion = 1;
constexpr uint32_t kTagShader = 0x52444853;    // "SHDR"
constexpr uint32_t kTagRootSig = 0x47495352;   // "RSIG"
constexpr uint32_t kTagPso = 0x314F5350;       // "PSO1"
constexpr size_t kHeaderSize = 48;
constexpr size_t kMaxBlob = 16u << 20;         // a shader / root signature larger than this is not journaled
constexpr size_t kMaxRecord = 64u << 10;
constexpr size_t kMaxStream = 64u << 10;
constexpr size_t kMaxQueueBytes = 32u << 20;
constexpr uint32_t kMaxRecords = 16384;
constexpr size_t kMaxRsPtrs = 65536;
constexpr size_t kMaxRecKeys = 200000;
constexpr uint32_t kGameMainPtr = 0x4759910, kGm58A = 0x58A;  // v2.02: GameMain*, level-load window byte

const GUID kIID_Device = {0x189819f1, 0x1db6, 0x4b57, {0xbe, 0x54, 0x18, 0x21, 0x33, 0x9b, 0x85, 0xf7}};
const GUID kIID_Device2 = {0x30baa41e, 0xb15b, 0x475c, {0xa0, 0xbb, 0x1a, 0xf5, 0xc5, 0xb6, 0x43, 0x28}};
const GUID kIID_RootSig = {0xc54a6b66, 0x72df, 0x4ee8, {0x8b, 0xe5, 0xa9, 0x46, 0xa1, 0x42, 0x92, 0x14}};
const GUID kIID_Pso = {0x765a30f3, 0xf624, 0x4c6f, {0xa8, 0x28, 0xac, 0xe9, 0x48, 0x62, 0x24, 0x45}};

using CreateGfxFn = HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*, const D3D12_GRAPHICS_PIPELINE_STATE_DESC*, REFIID, void**);
using CreateCsFn = HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*, const D3D12_COMPUTE_PIPELINE_STATE_DESC*, REFIID, void**);
using CreateRsFn = HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*, UINT, const void*, SIZE_T, REFIID, void**);
using CreateStreamFn = HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*, const D3D12_PIPELINE_STATE_STREAM_DESC*, REFIID, void**);

// ---- state ---------------------------------------------------------------------------------------------------------------
struct Cfg {
    int mode = 0;
    uint64_t maxBytes = 64ull << 20;
    int quietMs = 3000;
    uintptr_t base = 0;
    bool known = false, wine = false;
    std::wstring folder;
} g_cfg;
void (*g_log)(const char*, ...) = nullptr;

#define LOG(...)                                  \
    do {                                          \
        if (g_log) {                              \
            char _b[600];                         \
            snprintf(_b, sizeof _b, __VA_ARGS__); \
            g_log("PJ: %s", _b);                  \
        }                                         \
    } while (0)

struct Set {
    void** vt = nullptr;
    CreateGfxFn gfx = nullptr;
    CreateCsFn cs = nullptr;
    CreateRsFn rs = nullptr;
    CreateStreamFn stream = nullptr;
};
Set g_set[kMaxSets];
volatile LONG g_setN = 0;
SRWLOCK g_installLock = SRWLOCK_INIT;
thread_local int t_depth = 0;  // raised around calls into the runtime (nested proxy hooks) and on the prewarm thread
struct Depth {
    Depth() { ++t_depth; }
    ~Depth() { --t_depth; }
};

void* volatile g_gameDev = nullptr;  // the device the engine creates its PSOs on (first one seen; no reference held)
volatile LONG64 g_lastCreate = 0;    // QPC of the engine's last PSO create
double g_qpcMs = 0;

struct Key {
    uint64_t a = 0, b = 0;
    bool operator==(const Key& o) const { return a == o.a && b == o.b; }
    bool zero() const { return !a && !b; }
};
struct KeyHash {
    size_t operator()(const Key& k) const { return (size_t)(k.a ^ (k.b * 0x9E3779B97F4A7C15ull)); }
};

// hook-side knowledge (g_lock): what is in the journal or queued, what the engine created in this session
constexpr uint8_t kKnown = 1, kCreated = 2;
SRWLOCK g_lock = SRWLOCK_INIT;
std::unordered_set<Key, KeyHash> g_shKnown, g_rsKnown;
std::unordered_map<const void*, Key> g_rsPtr;
std::unordered_map<Key, uint8_t, KeyHash> g_rec;

// writer queue (g_qLock)
struct Item {
    uint32_t tag;
    Key key;
    std::vector<uint8_t> data;
};
SRWLOCK g_qLock = SRWLOCK_INIT;
std::vector<Item> g_q;
size_t g_qBytes = 0;
HANDLE g_qEvent = nullptr, g_loadedEvent = nullptr;
std::atomic<uint64_t> g_enq{0}, g_done{0};
std::atomic<bool> g_full{false}, g_loaded{false};

// counters
std::atomic<uint64_t> g_seen{0}, g_skipped{0}, g_dropped{0};
std::atomic<uint32_t> g_fileRecs{0}, g_newRecs{0}, g_shaders{0}, g_rootSigs{0};
std::atomic<uint64_t> g_fileBytes{0};
std::atomic<int> g_pwState{0};
std::atomic<uint32_t> g_pwCand{0}, g_pwAlready{0}, g_pwOk{0}, g_pwFailed{0}, g_pwMissing{0};
double g_pwMs = 0, g_pwMaxMs = 0;
uint32_t g_skipWhy[4] = {};  // 0 read fault, 1 unknown root signature, 2 unsupported field, 3 alloc

// writer-owned journal index (only the writer thread writes it after load; the prewarm reads it after g_loadedEvent)
struct Loc {
    uint64_t off = 0;  // payload offset in the file
    uint32_t len = 0;  // payload length
    uint64_t sum = 0;
};
std::unordered_map<Key, Loc, KeyHash> g_fileSh, g_fileRs;
std::unordered_set<Key, KeyHash> g_fileRecIds;
std::vector<std::vector<uint8_t>> g_startRecs;  // records in the journal at start (prewarm list), freed after the prewarm
SRWLOCK g_idxLock = SRWLOCK_INIT;                // g_fileSh / g_fileRs (writer inserts, prewarm looks up)
std::wstring g_path;
HANDLE g_file = INVALID_HANDLE_VALUE;
uint64_t g_end = 0;

// ---- helpers -------------------------------------------------------------------------------------------------------------
bool SafeCopy(void* dst, const void* src, size_t n) {
    __try {
        memcpy(dst, src, n);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
// copies a NUL-terminated string of at most cap-1 chars; -1 on fault or when longer
int SafeStr(char* dst, const char* src, int cap) {
    __try {
        int i = 0;
        for (; i < cap - 1 && src[i]; ++i) dst[i] = src[i];
        dst[i] = 0;
        return src[i] ? -1 : i;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return -1;
    }
}

uint64_t Rotl(uint64_t x, int r) { return (x << r) | (x >> (64 - r)); }
uint64_t Fmix(uint64_t k) {
    k ^= k >> 33;
    k *= 0xFF51AFD7ED558CCDull;
    k ^= k >> 33;
    k *= 0xC4CEB9FE1A85EC53ull;
    k ^= k >> 33;
    return k;
}
Key Hash128(const void* p, size_t n) {
    const uint8_t* b = static_cast<const uint8_t*>(p);
    uint64_t h1 = 0x9E3779B97F4A7C15ull ^ n, h2 = 0xC2B2AE3D27D4EB4Full + n;
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        uint64_t w;
        memcpy(&w, b + i, 8);
        h1 ^= Rotl(w * 0x87C37B91114253D5ull, 31) * 0x4CF5AD432745937Full;
        h1 = Rotl(h1, 27) * 5 + 0x52DCE729;
        h2 += w ^ h1;
        h2 = Rotl(h2, 33) * 0x9E3779B97F4A7C15ull + 0x38495AB5;
    }
    uint64_t t = 0;
    memcpy(&t, b + i, n - i);
    h1 ^= Rotl(t * 0x87C37B91114253D5ull, 31) * 0x4CF5AD432745937Full;
    h2 += t ^ h1;
    h1 = Fmix(h1 + h2);
    h2 = Fmix(h2 + h1);
    return {h1, h2 ^ h1};
}

int64_t Qpc() {
    LARGE_INTEGER q;
    QueryPerformanceCounter(&q);
    return q.QuadPart;
}
size_t Round8(size_t n) { return (n + 7) & ~size_t(7); }

template <class T>
void Put(std::vector<uint8_t>& v, const T& x) {
    const size_t o = v.size();
    v.resize(o + sizeof(T));
    memcpy(v.data() + o, &x, sizeof(T));
}
template <class T>
bool Get(const uint8_t* p, size_t n, size_t& pos, T& x) {
    if (pos + sizeof(T) > n) return false;
    memcpy(&x, p + pos, sizeof(T));
    pos += sizeof(T);
    return true;
}

// ---- serialization (engine thread, inside the hook after the create succeeded) ---------------------------------------------
struct ShRef {
    uint32_t off, len;
    Key key;
    const void* ptr;
};
struct IlElem {
    D3D12_INPUT_ELEMENT_DESC e;
    char name[64];
};
struct Ser {
    uint8_t kind = 0;
    std::vector<uint8_t> fixed;
    Key rs;
    uint32_t rsOff = ~0u;
    ShRef sh[8];
    int nSh = 0;
    uint32_t ilOff = ~0u;
    std::vector<IlElem> il;
    int why = 0;  // failure reason (g_skipWhy index)
};

bool ShaderKey(const void* p, size_t len, Key& k) {
    uint8_t h[20];
    if (len >= 20 && SafeCopy(h, p, 20) && !memcmp(h, "DXBC", 4)) {
        memcpy(&k.a, h + 4, 8);
        memcpy(&k.b, h + 12, 8);
        if (!k.zero()) return true;
    }
    std::vector<uint8_t> tmp(len);
    if (!SafeCopy(tmp.data(), p, len)) return false;
    k = Hash128(tmp.data(), len);
    k.b ^= 0x5A5A5A5A5A5A5A5Aull;  // never equal to a container digest by construction of the inputs
    return true;
}
bool SerShader(Ser& s, size_t off, const D3D12_SHADER_BYTECODE& bc) {
    if (!bc.pShaderBytecode && !bc.BytecodeLength) return true;  // stage absent
    if (!bc.pShaderBytecode || bc.BytecodeLength < 4 || bc.BytecodeLength > kMaxBlob || s.nSh >= 8) return s.why = 2, false;
    ShRef& r = s.sh[s.nSh];
    if (!ShaderKey(bc.pShaderBytecode, bc.BytecodeLength, r.key)) return s.why = 0, false;
    r.off = (uint32_t)off;
    r.len = (uint32_t)bc.BytecodeLength;
    r.ptr = bc.pShaderBytecode;
    ++s.nSh;
    return true;
}
bool SerRs(Ser& s, size_t off, const void* rs) {
    if (!rs) return true;  // root signature from the shader
    AcquireSRWLockShared(&g_lock);
    auto it = g_rsPtr.find(rs);
    const bool found = it != g_rsPtr.end();
    if (found) s.rs = it->second;
    ReleaseSRWLockShared(&g_lock);
    if (!found) return s.why = 1, false;
    s.rsOff = (uint32_t)off;
    return true;
}
bool SerIl(Ser& s, size_t off, const D3D12_INPUT_LAYOUT_DESC& il) {
    memcpy(s.fixed.data() + off + offsetof(D3D12_INPUT_LAYOUT_DESC, NumElements), &il.NumElements, 4);
    if (!il.NumElements) return true;
    if (!il.pInputElementDescs || il.NumElements > 64) return s.why = 2, false;
    std::vector<D3D12_INPUT_ELEMENT_DESC> e(il.NumElements);
    if (!SafeCopy(e.data(), il.pInputElementDescs, sizeof(D3D12_INPUT_ELEMENT_DESC) * il.NumElements)) return s.why = 0, false;
    s.il.resize(il.NumElements);
    for (UINT i = 0; i < il.NumElements; ++i) {
        s.il[i].e = e[i];
        if (!e[i].SemanticName || SafeStr(s.il[i].name, e[i].SemanticName, 64) < 0) return s.why = 0, false;
    }
    s.ilOff = (uint32_t)off;
    return true;
}
// field-wise copies into a zeroed destination: padding stays zero, so equal descs serialize to equal bytes
void CanonBlend(D3D12_BLEND_DESC* c, const D3D12_BLEND_DESC& s) {
    c->AlphaToCoverageEnable = s.AlphaToCoverageEnable;
    c->IndependentBlendEnable = s.IndependentBlendEnable;
    for (int i = 0; i < 8; ++i) {
        D3D12_RENDER_TARGET_BLEND_DESC& d = c->RenderTarget[i];
        const D3D12_RENDER_TARGET_BLEND_DESC& r = s.RenderTarget[i];
        d.BlendEnable = r.BlendEnable;
        d.LogicOpEnable = r.LogicOpEnable;
        d.SrcBlend = r.SrcBlend;
        d.DestBlend = r.DestBlend;
        d.BlendOp = r.BlendOp;
        d.SrcBlendAlpha = r.SrcBlendAlpha;
        d.DestBlendAlpha = r.DestBlendAlpha;
        d.BlendOpAlpha = r.BlendOpAlpha;
        d.LogicOp = r.LogicOp;
        d.RenderTargetWriteMask = r.RenderTargetWriteMask;
    }
}
template <class DS>
void CanonDsCommon(DS* c, const DS& s) {
    c->DepthEnable = s.DepthEnable;
    c->DepthWriteMask = s.DepthWriteMask;
    c->DepthFunc = s.DepthFunc;
    c->StencilEnable = s.StencilEnable;
    c->StencilReadMask = s.StencilReadMask;
    c->StencilWriteMask = s.StencilWriteMask;
    c->FrontFace = s.FrontFace;  // 4 x enum, no padding
    c->BackFace = s.BackFace;
}
void CanonDs(D3D12_DEPTH_STENCIL_DESC* c, const D3D12_DEPTH_STENCIL_DESC& s) { CanonDsCommon(c, s); }
void CanonDs1(D3D12_DEPTH_STENCIL_DESC1* c, const D3D12_DEPTH_STENCIL_DESC1& s) {
    CanonDsCommon(c, s);
    c->DepthBoundsTestEnable = s.DepthBoundsTestEnable;
}
static_assert(sizeof(D3D12_RASTERIZER_DESC) == 44 && sizeof(D3D12_RT_FORMAT_ARRAY) == 36 && sizeof(DXGI_SAMPLE_DESC) == 8,
              "no padding in the structs copied whole");

bool SerGfx(const D3D12_GRAPHICS_PIPELINE_STATE_DESC* p, Ser& s) {
    using D = D3D12_GRAPHICS_PIPELINE_STATE_DESC;
    D d;
    if (!SafeCopy(&d, p, sizeof d)) return s.why = 0, false;
    s.kind = 1;
    s.fixed.assign(Round8(sizeof(D)), 0);
    D* c = reinterpret_cast<D*>(s.fixed.data());
    if (d.StreamOutput.pSODeclaration || d.StreamOutput.NumEntries || d.StreamOutput.pBufferStrides || d.StreamOutput.NumStrides)
        return s.why = 2, false;
    if (!SerRs(s, offsetof(D, pRootSignature), d.pRootSignature)) return false;
    if (!SerShader(s, offsetof(D, VS), d.VS) || !SerShader(s, offsetof(D, PS), d.PS) || !SerShader(s, offsetof(D, DS), d.DS) ||
        !SerShader(s, offsetof(D, HS), d.HS) || !SerShader(s, offsetof(D, GS), d.GS))
        return false;
    c->StreamOutput.RasterizedStream = d.StreamOutput.RasterizedStream;
    CanonBlend(&c->BlendState, d.BlendState);
    c->SampleMask = d.SampleMask;
    c->RasterizerState = d.RasterizerState;
    CanonDs(&c->DepthStencilState, d.DepthStencilState);
    if (!SerIl(s, offsetof(D, InputLayout), d.InputLayout)) return false;
    c->IBStripCutValue = d.IBStripCutValue;
    c->PrimitiveTopologyType = d.PrimitiveTopologyType;
    c->NumRenderTargets = d.NumRenderTargets;
    for (int i = 0; i < 8; ++i) c->RTVFormats[i] = d.RTVFormats[i];
    c->DSVFormat = d.DSVFormat;
    c->SampleDesc = d.SampleDesc;
    c->NodeMask = d.NodeMask;
    c->Flags = d.Flags;  // CachedPSO stays empty: the prewarm compiles (or hits the driver cache) like a first create
    return true;
}
bool SerCs(const D3D12_COMPUTE_PIPELINE_STATE_DESC* p, Ser& s) {
    using D = D3D12_COMPUTE_PIPELINE_STATE_DESC;
    D d;
    if (!SafeCopy(&d, p, sizeof d)) return s.why = 0, false;
    s.kind = 2;
    s.fixed.assign(Round8(sizeof(D)), 0);
    D* c = reinterpret_cast<D*>(s.fixed.data());
    if (!SerRs(s, offsetof(D, pRootSignature), d.pRootSignature) || !SerShader(s, offsetof(D, CS), d.CS)) return false;
    if (!d.CS.pShaderBytecode) return s.why = 2, false;
    c->NodeMask = d.NodeMask;
    c->Flags = d.Flags;
    return true;
}
// inner size / alignment of a stream subobject (same table as psoswap.cpp InnerLayout; types the engine uses: 0, 2,
// 8-11, 14-18, 24, 25 - CreateGfxPso 9290D8 mesh path)
bool InnerLayout(uint32_t type, size_t& size, size_t& align) {
    switch (type) {
        case 0: size = sizeof(void*); align = alignof(void*); return true;
        case 1: case 2: case 3: case 4: case 5: case 6: case 24: case 25:
            size = sizeof(D3D12_SHADER_BYTECODE); align = alignof(D3D12_SHADER_BYTECODE); return true;
        case 7: size = sizeof(D3D12_STREAM_OUTPUT_DESC); align = alignof(D3D12_STREAM_OUTPUT_DESC); return true;
        case 8: size = sizeof(D3D12_BLEND_DESC); align = alignof(D3D12_BLEND_DESC); return true;
        case 9: case 13: case 14: case 16: case 18: case 20: size = 4; align = 4; return true;
        case 10: size = sizeof(D3D12_RASTERIZER_DESC); align = alignof(D3D12_RASTERIZER_DESC); return true;
        case 11: size = sizeof(D3D12_DEPTH_STENCIL_DESC); align = alignof(D3D12_DEPTH_STENCIL_DESC); return true;
        case 12: size = sizeof(D3D12_INPUT_LAYOUT_DESC); align = alignof(D3D12_INPUT_LAYOUT_DESC); return true;
        case 15: size = sizeof(D3D12_RT_FORMAT_ARRAY); align = alignof(D3D12_RT_FORMAT_ARRAY); return true;
        case 17: size = sizeof(DXGI_SAMPLE_DESC); align = alignof(DXGI_SAMPLE_DESC); return true;
        case 19: size = sizeof(D3D12_CACHED_PIPELINE_STATE); align = alignof(D3D12_CACHED_PIPELINE_STATE); return true;
        case 21: size = sizeof(D3D12_DEPTH_STENCIL_DESC1); align = alignof(D3D12_DEPTH_STENCIL_DESC1); return true;
        case 22: size = sizeof(D3D12_VIEW_INSTANCING_DESC); align = alignof(D3D12_VIEW_INSTANCING_DESC); return true;
        default: return false;  // newer Agility types (26-29) and unknown: not journaled
    }
}
bool SerStream(const D3D12_PIPELINE_STATE_STREAM_DESC* p, Ser& s) {
    D3D12_PIPELINE_STATE_STREAM_DESC sd;
    if (!SafeCopy(&sd, p, sizeof sd)) return s.why = 0, false;
    if (!sd.pPipelineStateSubobjectStream || sd.SizeInBytes < 8 || sd.SizeInBytes > kMaxStream) return s.why = 2, false;
    std::vector<uint64_t> srcBuf(Round8(sd.SizeInBytes) / 8);
    const uint8_t* src = reinterpret_cast<const uint8_t*>(srcBuf.data());
    if (!SafeCopy(srcBuf.data(), sd.pPipelineStateSubobjectStream, sd.SizeInBytes)) return s.why = 0, false;
    s.kind = 3;
    s.fixed.assign(Round8(sd.SizeInBytes), 0);
    uint8_t* out = s.fixed.data();
    const size_t n = sd.SizeInBytes;
    size_t pos = 0;
    while (pos + 4 <= n) {
        uint32_t type;
        memcpy(&type, src + pos, 4);
        size_t isz, ial;
        if (!InnerLayout(type, isz, ial)) return s.why = 2, false;
        const size_t inner = (pos + 4 + ial - 1) & ~(ial - 1);
        const size_t next = (inner + isz + sizeof(void*) - 1) & ~(sizeof(void*) - 1);
        if (inner + isz > n) return s.why = 2, false;
        memcpy(out + pos, &type, 4);
        const void* in = src + inner;
        void* o = out + inner;
        switch (type) {
            case 0: {
                void* rs;
                memcpy(&rs, in, sizeof rs);
                if (!SerRs(s, inner, rs)) return false;
                break;
            }
            case 1: case 2: case 3: case 4: case 5: case 6: case 24: case 25: {
                D3D12_SHADER_BYTECODE bc;
                memcpy(&bc, in, sizeof bc);
                if (!SerShader(s, inner, bc)) return false;
                break;
            }
            case 7: {
                D3D12_STREAM_OUTPUT_DESC so;
                memcpy(&so, in, sizeof so);
                if (so.pSODeclaration || so.NumEntries || so.pBufferStrides || so.NumStrides) return s.why = 2, false;
                static_cast<D3D12_STREAM_OUTPUT_DESC*>(o)->RasterizedStream = so.RasterizedStream;
                break;
            }
            case 8: {
                D3D12_BLEND_DESC b;
                memcpy(&b, in, sizeof b);
                CanonBlend(static_cast<D3D12_BLEND_DESC*>(o), b);
                break;
            }
            case 9: case 13: case 14: case 16: case 18: case 20: memcpy(o, in, 4); break;
            case 10: memcpy(o, in, sizeof(D3D12_RASTERIZER_DESC)); break;
            case 11: {
                D3D12_DEPTH_STENCIL_DESC d;
                memcpy(&d, in, sizeof d);
                CanonDs(static_cast<D3D12_DEPTH_STENCIL_DESC*>(o), d);
                break;
            }
            case 12: {
                D3D12_INPUT_LAYOUT_DESC il;
                memcpy(&il, in, sizeof il);
                if (s.ilOff != ~0u) return s.why = 2, false;
                if (!SerIl(s, inner, il)) return false;
                break;
            }
            case 15: memcpy(o, in, sizeof(D3D12_RT_FORMAT_ARRAY)); break;
            case 17: memcpy(o, in, sizeof(DXGI_SAMPLE_DESC)); break;
            case 19: break;  // cached PSO: dropped
            case 21: {
                D3D12_DEPTH_STENCIL_DESC1 d;
                memcpy(&d, in, sizeof d);
                CanonDs1(static_cast<D3D12_DEPTH_STENCIL_DESC1*>(o), d);
                break;
            }
            case 22: {
                D3D12_VIEW_INSTANCING_DESC v;
                memcpy(&v, in, sizeof v);
                if (v.ViewInstanceCount || v.pViewInstanceLocations) return s.why = 2, false;
                static_cast<D3D12_VIEW_INSTANCING_DESC*>(o)->Flags = v.Flags;
                break;
            }
        }
        pos = next;
    }
    return true;
}

std::vector<uint8_t> Encode(const Ser& s) {
    std::vector<uint8_t> v;
    v.reserve(48 + s.fixed.size() + s.nSh * 24 + s.il.size() * 40);
    Put(v, s.kind);
    Put(v, (uint8_t)1);
    Put(v, (uint8_t)s.nSh);
    Put(v, (uint8_t)(s.ilOff != ~0u));
    Put(v, (uint32_t)s.fixed.size());
    Put(v, s.rsOff);
    Put(v, (uint32_t)0);
    Put(v, s.rs);
    v.insert(v.end(), s.fixed.begin(), s.fixed.end());
    for (int i = 0; i < s.nSh; ++i) {
        Put(v, s.sh[i].off);
        Put(v, s.sh[i].len);
        Put(v, s.sh[i].key);
    }
    if (s.ilOff != ~0u) {
        Put(v, s.ilOff);
        Put(v, (uint32_t)s.il.size());
        for (const IlElem& e : s.il) {
            const uint32_t f[6] = {e.e.SemanticIndex, (uint32_t)e.e.Format, e.e.InputSlot, e.e.AlignedByteOffset,
                                   (uint32_t)e.e.InputSlotClass, e.e.InstanceDataStepRate};
            v.insert(v.end(), reinterpret_cast<const uint8_t*>(f), reinterpret_cast<const uint8_t*>(f) + sizeof f);
            const uint8_t ln = (uint8_t)strlen(e.name);
            Put(v, ln);
            v.insert(v.end(), e.name, e.name + ln);
        }
    }
    return v;
}

// ---- writer queue --------------------------------------------------------------------------------------------------------
bool Enqueue(uint32_t tag, const Key& key, std::vector<uint8_t>&& data) {
    const size_t sz = data.size() + 16;
    AcquireSRWLockExclusive(&g_qLock);
    const bool fits = g_qBytes + sz <= kMaxQueueBytes;
    if (fits) {
        g_qBytes += sz;
        g_q.push_back(Item{tag, key, std::move(data)});
        g_enq.fetch_add(1);
    }
    ReleaseSRWLockExclusive(&g_qLock);
    if (!fits) {
        g_dropped.fetch_add(1);
        return false;
    }
    SetEvent(g_qEvent);
    return true;
}

void RecordPso(const Ser& s) {
    std::vector<uint8_t> rec = Encode(s);
    if (rec.size() > kMaxRecord) {
        g_skipped.fetch_add(1);
        return;
    }
    const Key id = Hash128(rec.data(), rec.size());
    bool fresh;
    AcquireSRWLockExclusive(&g_lock);
    auto it = g_rec.find(id);
    if (it == g_rec.end() && g_rec.size() >= kMaxRecKeys) {
        fresh = false;  // bookkeeping full: neither journaled nor marked
    } else {
        uint8_t& f = g_rec[id];
        fresh = !(f & kKnown);
        f |= kKnown | kCreated;
    }
    ReleaseSRWLockExclusive(&g_lock);
    if (!fresh) return;
    if (g_full.load(std::memory_order_relaxed)) {
        g_dropped.fetch_add(1);
        return;
    }
    for (int i = 0; i < s.nSh; ++i) {
        const ShRef& r = s.sh[i];
        AcquireSRWLockExclusive(&g_lock);
        const bool need = g_shKnown.insert(r.key).second;
        ReleaseSRWLockExclusive(&g_lock);
        if (!need) continue;
        std::vector<uint8_t> blob(16 + r.len);
        memcpy(blob.data(), &r.key, 16);
        if (!SafeCopy(blob.data() + 16, r.ptr, r.len) || !Enqueue(kTagShader, r.key, std::move(blob))) {
            AcquireSRWLockExclusive(&g_lock);
            g_shKnown.erase(r.key);  // try again with the next PSO that uses it
            ReleaseSRWLockExclusive(&g_lock);
        }
    }
    Enqueue(kTagPso, id, std::move(rec));
}

void NoteCreate(ID3D12Device* self) {
    InterlockedCompareExchangePointer(&g_gameDev, self, nullptr);
    InterlockedExchange64(&g_lastCreate, Qpc());
    g_seen.fetch_add(1, std::memory_order_relaxed);
}

void OnPso(int kind, const void* desc) {
    try {
        Ser s;
        bool ok = false;
        if (kind == 1) ok = SerGfx(static_cast<const D3D12_GRAPHICS_PIPELINE_STATE_DESC*>(desc), s);
        if (kind == 2) ok = SerCs(static_cast<const D3D12_COMPUTE_PIPELINE_STATE_DESC*>(desc), s);
        if (kind == 3) ok = SerStream(static_cast<const D3D12_PIPELINE_STATE_STREAM_DESC*>(desc), s);
        if (!ok) {
            g_skipped.fetch_add(1);
            InterlockedIncrement(reinterpret_cast<volatile LONG*>(&g_skipWhy[s.why & 3]));
            return;
        }
        RecordPso(s);
    } catch (...) {
        g_skipped.fetch_add(1);
        InterlockedIncrement(reinterpret_cast<volatile LONG*>(&g_skipWhy[3]));
    }
}

void OnRootSig(const void* blob, size_t len, UINT node, void* obj) {
    try {
        if (!blob || !len || len > kMaxBlob) return;
        std::vector<uint8_t> data(24 + len);
        if (!SafeCopy(data.data() + 24, blob, len)) return;
        const Key k = Hash128(data.data() + 24, len);
        memcpy(data.data(), &k, 16);
        memcpy(data.data() + 16, &node, 4);
        void* rs = nullptr;
        if (FAILED(static_cast<IUnknown*>(obj)->QueryInterface(kIID_RootSig, &rs)) || !rs) return;
        static_cast<IUnknown*>(rs)->Release();  // only the pointer value is kept (it is what the PSO descs carry)
        AcquireSRWLockExclusive(&g_lock);
        if (g_rsPtr.size() < kMaxRsPtrs || g_rsPtr.count(rs)) g_rsPtr[rs] = k;
        const bool need = g_rsKnown.insert(k).second;
        ReleaseSRWLockExclusive(&g_lock);
        if (need && !Enqueue(kTagRootSig, k, std::move(data))) {
            AcquireSRWLockExclusive(&g_lock);
            g_rsKnown.erase(k);
            ReleaseSRWLockExclusive(&g_lock);
        }
    } catch (...) {
    }
}

// ---- device hooks --------------------------------------------------------------------------------------------------------
template <int S>
HRESULT STDMETHODCALLTYPE HkGfx(ID3D12Device* self, const D3D12_GRAPHICS_PIPELINE_STATE_DESC* d, REFIID riid, void** pp) {
    if (t_depth) return g_set[S].gfx(self, d, riid, pp);
    Depth g;
    const HRESULT hr = g_set[S].gfx(self, d, riid, pp);
    if (hr == S_OK && d && pp && *pp) {
        NoteCreate(self);
        OnPso(1, d);
    }
    return hr;
}
template <int S>
HRESULT STDMETHODCALLTYPE HkCs(ID3D12Device* self, const D3D12_COMPUTE_PIPELINE_STATE_DESC* d, REFIID riid, void** pp) {
    if (t_depth) return g_set[S].cs(self, d, riid, pp);
    Depth g;
    const HRESULT hr = g_set[S].cs(self, d, riid, pp);
    if (hr == S_OK && d && pp && *pp) {
        NoteCreate(self);
        OnPso(2, d);
    }
    return hr;
}
template <int S>
HRESULT STDMETHODCALLTYPE HkStream(ID3D12Device* self, const D3D12_PIPELINE_STATE_STREAM_DESC* d, REFIID riid, void** pp) {
    if (t_depth) return g_set[S].stream(self, d, riid, pp);
    Depth g;
    const HRESULT hr = g_set[S].stream(self, d, riid, pp);
    if (hr == S_OK && d && pp && *pp) {
        NoteCreate(self);
        OnPso(3, d);
    }
    return hr;
}
template <int S>
HRESULT STDMETHODCALLTYPE HkRs(ID3D12Device* self, UINT node, const void* blob, SIZE_T len, REFIID riid, void** pp) {
    if (t_depth) return g_set[S].rs(self, node, blob, len, riid, pp);
    Depth g;
    const HRESULT hr = g_set[S].rs(self, node, blob, len, riid, pp);
    if (SUCCEEDED(hr) && pp && *pp) OnRootSig(blob, len, node, *pp);
    return hr;
}

bool Patch(void** slot, void* hook, void** orig) {
    DWORD old;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) return false;
    *orig = *slot;  // published before the slot so the hook never sees a null original
    MemoryBarrier();
    InterlockedExchangePointer(slot, hook);
    DWORD t;
    VirtualProtect(slot, sizeof(void*), old, &t);
    return true;
}
template <int S>
void PatchSet(void** vt, bool has2) {
    Set& s = g_set[S];
    s.vt = vt;
    Patch(&vt[kSlotCreateGraphics], (void*)&HkGfx<S>, (void**)&s.gfx);
    Patch(&vt[kSlotCreateCompute], (void*)&HkCs<S>, (void**)&s.cs);
    Patch(&vt[kSlotCreateRootSignature], (void*)&HkRs<S>, (void**)&s.rs);
    if (has2) Patch(&vt[kSlotCreatePipelineState], (void*)&HkStream<S>, (void**)&s.stream);
}
void HookVt(IUnknown* obj) {
    void** vt = *reinterpret_cast<void***>(obj);
    AcquireSRWLockExclusive(&g_installLock);
    bool known = false;
    for (LONG i = 0; i < g_setN; ++i) known |= g_set[i].vt == vt;
    if (!known && g_setN < kMaxSets) {
        bool has2 = false;
        void* q = nullptr;
        if (SUCCEEDED(obj->QueryInterface(kIID_Device2, &q)) && q) {
            has2 = *static_cast<void***>(q) == vt;
            static_cast<IUnknown*>(q)->Release();
        }
        switch (g_setN) {
            case 0: PatchSet<0>(vt, has2); break;
            case 1: PatchSet<1>(vt, has2); break;
            case 2: PatchSet<2>(vt, has2); break;
            default: PatchSet<3>(vt, has2); break;
        }
        InterlockedIncrement(&g_setN);
        LOG("hooked device vtable %p: CreateGraphics/ComputePipelineState, CreateRootSignature%s", (void*)vt,
            has2 ? ", CreatePipelineState (stream)" : "");
    }
    ReleaseSRWLockExclusive(&g_installLock);
}
void OnDeviceImpl(void* obj) {
    void* dev = nullptr;
    if (FAILED(static_cast<IUnknown*>(obj)->QueryInterface(kIID_Device, &dev)) || !dev) return;
    IUnknown* d = static_cast<IUnknown*>(dev);
    HookVt(d);
    void* d2 = nullptr;
    if (SUCCEEDED(d->QueryInterface(kIID_Device2, &d2)) && d2) {
        if (*static_cast<void***>(d2) != *reinterpret_cast<void***>(d)) HookVt(static_cast<IUnknown*>(d2));
        static_cast<IUnknown*>(d2)->Release();
    }
    d->Release();
}
const Set* SetOf(void* obj) {
    void** vt = *static_cast<void***>(obj);
    for (LONG i = 0; i < g_setN; ++i)
        if (g_set[i].vt == vt) return &g_set[i];
    return nullptr;
}

// ---- journal file --------------------------------------------------------------------------------------------------------
bool ReadAt(HANDLE h, uint64_t off, void* buf, uint32_t n) {
    OVERLAPPED ov{};
    ov.Offset = (DWORD)off;
    ov.OffsetHigh = (DWORD)(off >> 32);
    DWORD got = 0;
    return ReadFile(h, buf, n, &got, &ov) && got == n;
}
bool WriteAll(HANDLE h, uint64_t off, const void* buf, size_t n) {
    OVERLAPPED ov{};
    ov.Offset = (DWORD)off;
    ov.OffsetHigh = (DWORD)(off >> 32);
    DWORD put = 0;
    return WriteFile(h, buf, (DWORD)n, &put, &ov) && put == n;
}
struct Header {
    uint32_t magic, version, exeStamp, exeSize;
    char devClass[32];
};
static_assert(sizeof(Header) == kHeaderSize, "header");

Header MakeHeader(const char* devClass) {
    Header h{};
    h.magic = kMagic;
    h.version = kVersion;
    auto* base = reinterpret_cast<const uint8_t*>(GetModuleHandleW(nullptr));
    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    h.exeStamp = nt->FileHeader.TimeDateStamp;
    h.exeSize = nt->OptionalHeader.SizeOfImage;
    strncpy_s(h.devClass, devClass, _TRUNCATE);
    return h;
}

// device class: OS layer, UMA (APU) or not, highest shader model. One journal per class: a journal copied between
// different machines only costs failed / useless prewarm creates, never a wrong PSO.
void DevClass(ID3D12Device* d, char* out, size_t n) {
    D3D12_FEATURE_DATA_ARCHITECTURE a{};
    const bool uma = SUCCEEDED(d->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE, &a, sizeof a)) && a.UMA;
    const D3D_SHADER_MODEL try_[] = {(D3D_SHADER_MODEL)0x69, (D3D_SHADER_MODEL)0x68, D3D_SHADER_MODEL_6_7, D3D_SHADER_MODEL_6_6,
                                     D3D_SHADER_MODEL_6_5, D3D_SHADER_MODEL_6_4, D3D_SHADER_MODEL_6_3, D3D_SHADER_MODEL_6_2,
                                     D3D_SHADER_MODEL_6_1, D3D_SHADER_MODEL_6_0, D3D_SHADER_MODEL_5_1};
    unsigned sm = 0;
    for (D3D_SHADER_MODEL t : try_) {
        D3D12_FEATURE_DATA_SHADER_MODEL q{t};
        if (SUCCEEDED(d->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &q, sizeof q))) {
            sm = (unsigned)q.HighestShaderModel;
            break;
        }
    }
    snprintf(out, n, "pj_%s_%s_sm%02x", g_cfg.wine ? "proton" : "win", uma ? "apu" : "dgpu", sm);
}

// Opens / validates the journal and indexes it. Writer thread, once.
void OpenJournal(ID3D12Device* dev) {
    char cls[32];
    DevClass(dev, cls, sizeof cls);
    CreateDirectoryW(g_cfg.folder.c_str(), nullptr);
    wchar_t wcls[32];
    MultiByteToWideChar(CP_UTF8, 0, cls, -1, wcls, 32);
    g_path = g_cfg.folder + L"\\" + wcls + L".bin";
    g_file = CreateFileW(g_path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
    if (g_file == INVALID_HANDLE_VALUE) {
        LOG("cannot open %ls (error %lu) - journal off for this session", g_path.c_str(), GetLastError());
        g_full = true;
        return;
    }
    const int64_t t0 = Qpc();
    const Header want = MakeHeader(cls);
    LARGE_INTEGER sz{};
    GetFileSizeEx(g_file, &sz);
    Header have{};
    bool fresh = (uint64_t)sz.QuadPart < kHeaderSize || !ReadAt(g_file, 0, &have, sizeof have) ||
                 memcmp(&have, &want, sizeof have) != 0;
    if (fresh && sz.QuadPart)
        LOG("%ls belongs to another game build or device class (exe %08X/%X, class %.32s) - starting a new journal",
            g_path.c_str(), have.exeStamp, have.exeSize, have.devClass);
    uint64_t off = kHeaderSize, end = (uint64_t)sz.QuadPart;
    uint32_t recs = 0, nSh = 0, nRs = 0, bad = 0;
    std::vector<uint8_t> buf;
    while (!fresh && off + 16 <= end) {
        uint32_t hd[4];
        if (!ReadAt(g_file, off, hd, 16)) break;
        const uint32_t tag = hd[0], len = hd[1];
        uint64_t sum;
        memcpy(&sum, hd + 2, 8);
        if (off + 16 + len > end || len < 16) break;  // truncated tail
        if (tag == kTagPso) {
            if (len > kMaxRecord) break;
            buf.resize(len);
            if (!ReadAt(g_file, off + 16, buf.data(), len) || Hash128(buf.data(), len).a != sum) {
                ++bad;
                break;
            }
            const Key id = Hash128(buf.data(), len);
            if (g_fileRecIds.insert(id).second) {
                g_startRecs.push_back(buf);
                ++recs;
            }
        } else if (tag == kTagShader || tag == kTagRootSig) {
            if (len > kMaxBlob + 24) break;
            Key k;
            if (!ReadAt(g_file, off + 16, &k, 16)) break;
            Loc l{off + 16, len, sum};
            if (tag == kTagShader ? g_fileSh.emplace(k, l).second : g_fileRs.emplace(k, l).second) ++(tag == kTagShader ? nSh : nRs);
        } else {
            ++bad;
            break;
        }
        off += 16 + len;
    }
    if (fresh) {
        off = 0;
        SetFilePointer(g_file, 0, nullptr, FILE_BEGIN);
        SetEndOfFile(g_file);
        WriteAll(g_file, 0, &want, sizeof want);
        off = kHeaderSize;
    } else if (off < end) {
        LOG("journal tail cut at %llu of %llu bytes (%s)", (unsigned long long)off, (unsigned long long)end,
            bad ? "bad chunk" : "incomplete chunk");
        LARGE_INTEGER p;
        p.QuadPart = (LONGLONG)off;
        SetFilePointerEx(g_file, p, nullptr, FILE_BEGIN);
        SetEndOfFile(g_file);
    }
    g_end = off;
    g_fileBytes = off;
    g_fileRecs = recs;
    g_shaders = nSh;
    g_rootSigs = nRs;
    // what is in the file is known: the hooks stop copying it from now on
    AcquireSRWLockExclusive(&g_lock);
    for (auto& e : g_fileSh) g_shKnown.insert(e.first);
    for (auto& e : g_fileRs) g_rsKnown.insert(e.first);
    for (const Key& id : g_fileRecIds)
        if (g_rec.size() < kMaxRecKeys || g_rec.count(id)) g_rec[id] |= kKnown;
    ReleaseSRWLockExclusive(&g_lock);
    if (recs >= kMaxRecords || g_end >= g_cfg.maxBytes) g_full = true;
    LOG("journal %ls: %u PSOs, %u shaders, %u root signatures, %.1f MB, indexed in %.0f ms%s", g_path.c_str(), recs, nSh, nRs,
        (double)g_end / 1048576.0, (double)(Qpc() - t0) * g_qpcMs, g_full ? " - FULL, nothing new is journaled" : "");
}

void WriteItem(const Item& it, std::vector<uint8_t>& wbuf) {
    if (g_file == INVALID_HANDLE_VALUE) return;
    AcquireSRWLockExclusive(&g_idxLock);
    bool dup = false;
    if (it.tag == kTagShader) dup = g_fileSh.count(it.key) != 0;
    if (it.tag == kTagRootSig) dup = g_fileRs.count(it.key) != 0;
    if (it.tag == kTagPso) dup = g_fileRecIds.count(it.key) != 0;
    ReleaseSRWLockExclusive(&g_idxLock);
    if (dup) return;
    const uint64_t chunk = 16 + it.data.size();
    if ((it.tag == kTagPso && g_fileRecIds.size() >= kMaxRecords) || g_end + wbuf.size() + chunk > g_cfg.maxBytes) {
        if (!g_full.exchange(true)) LOG("journal full (%.1f MB, %zu PSOs) - nothing new is journaled", (double)g_end / 1048576.0,
                                        g_fileRecIds.size());
        g_dropped.fetch_add(1);
        return;
    }
    const uint64_t at = g_end + wbuf.size();
    uint32_t hd[4] = {it.tag, (uint32_t)it.data.size(), 0, 0};
    const uint64_t sum = Hash128(it.data.data(), it.data.size()).a;
    memcpy(hd + 2, &sum, 8);
    wbuf.insert(wbuf.end(), reinterpret_cast<uint8_t*>(hd), reinterpret_cast<uint8_t*>(hd) + 16);
    wbuf.insert(wbuf.end(), it.data.begin(), it.data.end());
    AcquireSRWLockExclusive(&g_idxLock);
    if (it.tag == kTagShader) g_fileSh.emplace(it.key, Loc{at + 16, (uint32_t)it.data.size(), sum}), ++g_shaders;
    if (it.tag == kTagRootSig) g_fileRs.emplace(it.key, Loc{at + 16, (uint32_t)it.data.size(), sum}), ++g_rootSigs;
    if (it.tag == kTagPso) g_fileRecIds.insert(it.key), ++g_newRecs;
    ReleaseSRWLockExclusive(&g_idxLock);
}
void Flush(std::vector<uint8_t>& wbuf) {
    if (wbuf.empty() || g_file == INVALID_HANDLE_VALUE) return;
    if (!WriteAll(g_file, g_end, wbuf.data(), wbuf.size())) {
        LOG("write failed (error %lu) - journal off for this session", GetLastError());
        g_full = true;
        CloseHandle(g_file);
        g_file = INVALID_HANDLE_VALUE;
    } else {
        g_end += wbuf.size();
        g_fileBytes = g_end;
    }
    wbuf.clear();
}

DWORD WINAPI WriterMain(LPVOID) {
    SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);  // low CPU and I/O priority
    while (!g_gameDev) Sleep(50);
    ID3D12Device* dev = static_cast<ID3D12Device*>(g_gameDev);
    dev->AddRef();
    try {
        OpenJournal(dev);
    } catch (...) {
        LOG("out of memory while indexing the journal - journal off for this session");
        g_full = true;
    }
    dev->Release();
    g_loaded = true;
    SetEvent(g_loadedEvent);
    std::vector<uint8_t> wbuf;
    std::vector<Item> batch;
    for (;;) {
        WaitForSingleObject(g_qEvent, 500);
        AcquireSRWLockExclusive(&g_qLock);
        batch.swap(g_q);
        g_qBytes = 0;
        ReleaseSRWLockExclusive(&g_qLock);
        for (const Item& it : batch) {
            try {
                WriteItem(it, wbuf);
            } catch (...) {
                g_dropped.fetch_add(1);
            }
            if (wbuf.size() >= (1u << 20)) Flush(wbuf);
        }
        Flush(wbuf);
        g_done.fetch_add(batch.size());
        batch.clear();
        batch.shrink_to_fit();
    }
}

// ---- prewarm ---------------------------------------------------------------------------------------------------------------
struct Prewarm {
    HANDLE file = INVALID_HANDLE_VALUE;
    ID3D12Device* dev = nullptr;
    const Set* set = nullptr;
    const Set* set2 = nullptr;  // the set holding CreatePipelineState (Device2 vtable)
    ID3D12Device* dev2 = nullptr;
    std::unordered_map<Key, std::vector<uint8_t>, KeyHash> sh;  // shader blobs read so far (payload incl. key)
    std::unordered_map<Key, void*, KeyHash> rs;                // root signatures created for the prewarm

    bool Payload(const std::unordered_map<Key, Loc, KeyHash>& idx, const Key& k, std::vector<uint8_t>& out) {
        Loc l;
        AcquireSRWLockShared(&g_idxLock);
        auto it = idx.find(k);
        const bool f = it != idx.end();
        if (f) l = it->second;
        ReleaseSRWLockShared(&g_idxLock);
        if (!f) return false;
        out.resize(l.len);
        if (!ReadAt(file, l.off, out.data(), l.len) || Hash128(out.data(), l.len).a != l.sum) return false;
        Key got;
        memcpy(&got, out.data(), 16);
        return got == k;
    }
    const std::vector<uint8_t>* Shader(const Key& k) {
        auto it = sh.find(k);
        if (it != sh.end()) return it->second.empty() ? nullptr : &it->second;
        std::vector<uint8_t>& v = sh[k];
        if (!Payload(g_fileSh, k, v)) v.clear();
        return v.empty() ? nullptr : &v;
    }
    void* RootSig(const Key& k) {
        auto it = rs.find(k);
        if (it != rs.end()) return it->second;
        void* r = nullptr;
        std::vector<uint8_t> v;
        if (Payload(g_fileRs, k, v) && v.size() > 24) {
            UINT node;
            memcpy(&node, v.data() + 16, 4);
            if (FAILED(set->rs(dev, node, v.data() + 24, v.size() - 24, kIID_RootSig, &r))) r = nullptr;
        }
        rs[k] = r;
        return r;
    }
    // 0 ok, 1 missing blob, 2 bad record
    int Build(const std::vector<uint8_t>& rec, std::vector<uint64_t>& fixed, std::vector<D3D12_INPUT_ELEMENT_DESC>& il,
              std::vector<std::string>& names, uint8_t& kind) {
        const uint8_t* p = rec.data();
        const size_t n = rec.size();
        size_t pos = 0;
        uint8_t ver, nSh, hasIl;
        uint32_t flen, rsOff, zero;
        Key rsKey;
        if (!Get(p, n, pos, kind) || !Get(p, n, pos, ver) || !Get(p, n, pos, nSh) || !Get(p, n, pos, hasIl) ||
            !Get(p, n, pos, flen) || !Get(p, n, pos, rsOff) || !Get(p, n, pos, zero) || !Get(p, n, pos, rsKey))
            return 2;
        if (ver != 1 || kind < 1 || kind > 3 || flen % 8 || pos + flen > n || nSh > 8) return 2;
        if (kind == 1 && flen != Round8(sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC))) return 2;
        if (kind == 2 && flen != Round8(sizeof(D3D12_COMPUTE_PIPELINE_STATE_DESC))) return 2;
        fixed.assign(flen / 8, 0);
        uint8_t* f = reinterpret_cast<uint8_t*>(fixed.data());
        memcpy(f, p + pos, flen);
        pos += flen;
        auto ptrAt = [&](uint32_t off, size_t need) { return off % 8 == 0 && off + need <= flen; };
        if (rsOff != ~0u) {
            if (!ptrAt(rsOff, 8)) return 2;
            void* r = RootSig(rsKey);
            if (!r) return 1;
            memcpy(f + rsOff, &r, 8);
        }
        for (int i = 0; i < nSh; ++i) {
            uint32_t off, len;
            Key k;
            if (!Get(p, n, pos, off) || !Get(p, n, pos, len) || !Get(p, n, pos, k) || !ptrAt(off, 16)) return 2;
            const std::vector<uint8_t>* b = Shader(k);
            if (!b || b->size() != 16 + (size_t)len) return 1;
            const D3D12_SHADER_BYTECODE bc = {b->data() + 16, len};
            memcpy(f + off, &bc, sizeof bc);
        }
        if (hasIl) {
            uint32_t off, cnt;
            if (!Get(p, n, pos, off) || !Get(p, n, pos, cnt) || !ptrAt(off, 16) || cnt > 64) return 2;
            il.resize(cnt);
            names.resize(cnt);
            for (uint32_t i = 0; i < cnt; ++i) {
                uint32_t e[6];
                uint8_t ln;
                if (pos + sizeof e > n) return 2;
                memcpy(e, p + pos, sizeof e);
                pos += sizeof e;
                if (!Get(p, n, pos, ln) || pos + ln > n) return 2;
                names[i].assign(reinterpret_cast<const char*>(p + pos), ln);
                pos += ln;
                il[i] = {nullptr, e[0], (DXGI_FORMAT)e[1], e[2], e[3], (D3D12_INPUT_CLASSIFICATION)e[4], e[5]};
            }
            for (uint32_t i = 0; i < cnt; ++i) il[i].SemanticName = names[i].c_str();
            const D3D12_INPUT_LAYOUT_DESC d = {il.data(), cnt};
            memcpy(f + off, &d, sizeof d);
        }
        return pos == n ? 0 : 2;
    }
    HRESULT Create(uint8_t kind, std::vector<uint64_t>& fixed) {
        void* pso = nullptr;
        HRESULT hr = E_FAIL;
        if (kind == 1) hr = set->gfx(dev, reinterpret_cast<const D3D12_GRAPHICS_PIPELINE_STATE_DESC*>(fixed.data()), kIID_Pso, &pso);
        if (kind == 2) hr = set->cs(dev, reinterpret_cast<const D3D12_COMPUTE_PIPELINE_STATE_DESC*>(fixed.data()), kIID_Pso, &pso);
        if (kind == 3) {
            if (!set2 || !set2->stream || !dev2) return E_NOINTERFACE;
            const D3D12_PIPELINE_STATE_STREAM_DESC sd = {fixed.size() * 8, fixed.data()};
            hr = set2->stream(dev2, &sd, kIID_Pso, &pso);
        }
        if (SUCCEEDED(hr) && pso) static_cast<IUnknown*>(pso)->Release();
        return hr;
    }
};

bool TitleIdle() {
    if (!g_cfg.known || !g_cfg.base) return true;
    uintptr_t gm = 0;
    uint8_t f = 1;
    return SafeCopy(&gm, reinterpret_cast<const void*>(g_cfg.base + kGameMainPtr), sizeof gm) && gm &&
           SafeCopy(&f, reinterpret_cast<const void*>(gm + kGm58A), 1) && f == 0;
}

void RunPrewarm() {
    Prewarm pw;
    pw.dev = static_cast<ID3D12Device*>(g_gameDev);
    pw.dev->AddRef();
    pw.set = SetOf(pw.dev);
    void* d2 = nullptr;
    if (SUCCEEDED(pw.dev->QueryInterface(kIID_Device2, &d2)) && d2) {
        pw.dev2 = static_cast<ID3D12Device*>(d2);
        pw.set2 = SetOf(d2);
    }
    pw.file = CreateFileW(g_path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                          FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!pw.set || pw.file == INVALID_HANDLE_VALUE) {
        LOG("prewarm skipped (%s)", !pw.set ? "device vtable not hooked" : "journal not readable");
    } else {
        g_pwState = 1;
        g_pwCand = (uint32_t)g_startRecs.size();
        LOG("prewarm start: %zu journaled PSOs (title idle, quiet %d ms)", g_startRecs.size(), g_cfg.quietMs);
        const int64_t t0 = Qpc();
        std::vector<uint64_t> fixed;
        std::vector<D3D12_INPUT_ELEMENT_DESC> il;
        std::vector<std::string> names;
        for (const std::vector<uint8_t>& rec : g_startRecs) {
            while (!TitleIdle()) Sleep(100);  // a level load started: its own PSO lists come first, resume after it
            const Key id = Hash128(rec.data(), rec.size());
            bool already;
            AcquireSRWLockExclusive(&g_lock);
            uint8_t& fl = g_rec[id];
            already = (fl & kCreated) != 0;
            fl |= kCreated | kKnown;  // created by us now: also not again by a second pass
            ReleaseSRWLockExclusive(&g_lock);
            if (already) {
                ++g_pwAlready;
                continue;
            }
            uint8_t kind = 0;
            il.clear();
            names.clear();
            const int b = pw.Build(rec, fixed, il, names, kind);
            if (b) {
                ++g_pwMissing;
                continue;
            }
            const int64_t c0 = Qpc();
            const HRESULT hr = pw.Create(kind, fixed);
            const double ms = (double)(Qpc() - c0) * g_qpcMs;
            if (SUCCEEDED(hr)) {
                ++g_pwOk;
                g_pwMs += ms;
                if (ms > g_pwMaxMs) g_pwMaxMs = ms;
            } else {
                if (g_pwFailed++ < 8) LOG("prewarm: a journaled %s PSO was refused (hr %08lX)",
                                          kind == 1 ? "graphics" : kind == 2 ? "compute" : "stream", (unsigned long)hr);
            }
        }
        LOG("prewarm done in %.1f s: %u created (%.0f ms total, max %.1f ms), %u already created by the game, %u refused, "
            "%u with a missing/corrupt blob",
            (double)(Qpc() - t0) * g_qpcMs / 1000.0, g_pwOk.load(), g_pwMs, g_pwMaxMs, g_pwAlready.load(), g_pwFailed.load(),
            g_pwMissing.load());
    }
    for (auto& e : pw.rs)
        if (e.second) static_cast<IUnknown*>(e.second)->Release();
    if (pw.file != INVALID_HANDLE_VALUE) CloseHandle(pw.file);
    if (pw.dev2) pw.dev2->Release();
    pw.dev->Release();
    std::vector<std::vector<uint8_t>>().swap(g_startRecs);
}

DWORD WINAPI PrewarmMain(LPVOID) {
    WaitForSingleObject(g_loadedEvent, INFINITE);
    if (g_cfg.mode != 1 || g_startRecs.empty() || g_file == INVALID_HANDLE_VALUE) {
        std::vector<std::vector<uint8_t>>().swap(g_startRecs);
        g_pwState = 2;
        return 0;
    }
    // title gate: the engine has created PSOs (boot list done), none for quietMs, and no level load is running
    for (;;) {
        const int64_t last = g_lastCreate;
        if (last && (double)(Qpc() - last) * g_qpcMs >= g_cfg.quietMs && TitleIdle()) break;
        Sleep(250);
    }
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
    t_depth = 1;  // our own creates are not journaled (and pass through every PJ hook)
    try {
        RunPrewarm();
    } catch (...) {
        LOG("prewarm stopped: out of memory");
    }
    g_pwState = 2;
    return 0;
}

}  // namespace

// ---- API ---------------------------------------------------------------------------------------------------------------------
bool Wanted(const wchar_t* iniPath) { return iniPath && GetPrivateProfileIntW(L"Patches", L"PJ", 0, iniPath) != 0; }

void Start(const StartInfo& info) {
    g_log = info.log;
    const wchar_t* ini = info.iniPath ? info.iniPath : L"";
    int mode = (int)GetPrivateProfileIntW(L"Patches", L"PJ", 0, ini);
    if (mode < 0 || mode > 2) mode = 0;
    g_cfg.mode = mode;
    if (!mode) return;
    int mb = (int)GetPrivateProfileIntW(L"Patches", L"PJ_MaxMB", 64, ini);
    mb = mb < 4 ? 4 : mb > 512 ? 512 : mb;
    g_cfg.maxBytes = (uint64_t)mb << 20;
    int q = (int)GetPrivateProfileIntW(L"Patches", L"PJ_QuietMs", 3000, ini);
    if (info.quietMs >= 0) q = info.quietMs;
    g_cfg.quietMs = q < 0 ? 0 : q > 60000 ? 60000 : q;
    g_cfg.base = info.base;
    g_cfg.known = info.knownBuild;
    g_cfg.wine = info.wine;
    if (info.folder) {
        g_cfg.folder = info.folder;
    } else {
        std::wstring f = ini;
        const size_t sep = f.find_last_of(L"\\/");
        g_cfg.folder = (sep == std::wstring::npos ? std::wstring(L".") : f.substr(0, sep)) + L"\\Nioh3EngineFix_pj";
    }
    LARGE_INTEGER fq;
    QueryPerformanceFrequency(&fq);
    g_qpcMs = 1000.0 / (double)fq.QuadPart;
    g_qEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    g_loadedEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    HANDLE w = CreateThread(nullptr, 0, WriterMain, nullptr, 0, nullptr);
    HANDLE p = CreateThread(nullptr, 0, PrewarmMain, nullptr, 0, nullptr);
    if (w) CloseHandle(w);
    if (p) CloseHandle(p);
    LOG("PJ = %d (%s), journal folder %ls, max %d MB, quiet %d ms, title gate %s", mode,
        mode == 1 ? "journal + title-screen prewarm" : "journal only", g_cfg.folder.c_str(), mb, g_cfg.quietMs,
        g_cfg.known && g_cfg.base ? "G+58Ah" : "off (quiet time only)");
}

void OnDevice(void* device) {
    if (!g_cfg.mode || !device) return;
    __try {
        OnDeviceImpl(device);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        LOG("exception 0x%08lX while hooking a device - this device is not journaled", GetExceptionCode());
    }
}

void GetStats(Stats& o) {
    o = Stats{};
    o.mode = g_cfg.mode;
    o.hooked = g_setN > 0;
    o.fileRecs = g_fileRecs;
    o.newRecs = g_newRecs;
    o.shaders = g_shaders;
    o.rootSigs = g_rootSigs;
    o.fileBytes = g_fileBytes;
    o.seen = g_seen;
    o.skipped = g_skipped;
    o.dropped = g_dropped;
    o.prewarmState = g_pwState;
    o.pwCandidates = g_pwCand;
    o.pwAlready = g_pwAlready;
    o.pwOk = g_pwOk;
    o.pwFailed = g_pwFailed;
    o.pwMissing = g_pwMissing;
    o.pwMs = g_pwMs;
    o.pwMaxMs = g_pwMaxMs;
}

const char* StatusLine(char* buf, size_t n) {
    if (!buf || !n) return "";
    buf[0] = 0;
    Stats s;
    GetStats(s);
    if (!s.mode) return buf;
    const char* st = s.mode == 2 ? "off (PJ = 2)" : s.prewarmState == 0 ? "waiting for the title" : s.prewarmState == 1 ? "running" : "done";
    snprintf(buf, n,
             "journal %u PSOs (+%u new), %u shaders, %u root sigs, %.1f MB; engine creates %llu, not journaled %llu (rs %u, "
             "unsupported %u, fault %u) + dropped %llu | prewarm %s: %u/%u created, %u already, %u refused, %u missing, "
             "%.0f ms (max %.1f)",
             s.fileRecs, s.newRecs, s.shaders, s.rootSigs, (double)s.fileBytes / 1048576.0, (unsigned long long)s.seen,
             (unsigned long long)s.skipped, g_skipWhy[1], g_skipWhy[2], g_skipWhy[0] + g_skipWhy[3],
             (unsigned long long)s.dropped, st, s.pwOk, s.pwCandidates, s.pwAlready, s.pwFailed, s.pwMissing, s.pwMs, s.pwMaxMs);
    return buf;
}

void LogLine() {
    if (!g_cfg.mode || !g_log) return;
    char b[512];
    g_log("stats/s PJ: %s", StatusLine(b, sizeof b));
}

bool WaitIdleForTest(unsigned long ms) {
    const ULONGLONG until = GetTickCount64() + ms;
    while (GetTickCount64() < until) {
        if (g_loaded && g_enq.load() == g_done.load()) return true;
        Sleep(10);
    }
    return false;
}
bool WaitPrewarmForTest(unsigned long ms) {
    const ULONGLONG until = GetTickCount64() + ms;
    while (GetTickCount64() < until) {
        if (g_pwState == 2) return true;
        Sleep(10);
    }
    return false;
}

}  // namespace pj
