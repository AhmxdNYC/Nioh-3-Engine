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
//
// PL (1.7.46, notes\boot-shader-compile.md): a persistent ID3D12PipelineLibrary for the BOOT PSO list (the ~17,000 PSOs
// the game creates before the title in every launch). Every engine create before the boot gate is first looked up in the
// library (LoadGraphics/Compute/Pipeline with the engine's own desc; the D3D12 runtime refuses a desc that differs from the
// stored one); a miss runs the engine's own call and stores the result. At the gate the library is serialized (only when
// something new was stored) and released. File: <ini folder>\Nioh3EngineFix_pj\pl_<os>_<gpu>_sm<NN>.bin = 128-byte header
// (exe build, device class, adapter ids, UMD driver version, blob size + Hash128) + the D3D12 blob. The blob is mapped
// read-only for the whole session (file-backed pages, no commit charge); a rewrite while it is mapped goes to .new and is
// promoted at the next launch. Any mismatch (exe, driver, adapter, size, checksum) or a D3D12 refusal starts a new library;
// any load error falls back to the engine's own call. After the gate nothing changes: runtime creates belong to PJ / PJH.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
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
constexpr uint32_t kVersion = 2;        // 2 (2026-10-09): runtime PSOs only (after the boot PSO list); v1 files start over
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
    int handover = 0;           // PJH: 0 off, 1 on, 2 installed + off (live switch)
    uint32_t keepMax = 4096;    // PJ_KeepMax: prewarmed PSOs held for the hand-over
    int pl = 0;                 // PL: 0 off, 1 boot pipeline library, 2 installed + off (boot timing line only)
    uint64_t plMaxBytes = 1024ull << 20;  // PL_MaxMB: largest library written
} g_cfg;
volatile uint32_t g_pjhSwitch = 0;  // PJH live switch (menu row): 1 = hand prewarmed PSOs to the engine's creates
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
constexpr uint8_t kKnown = 1, kCreated = 2, kEngine = 4;  // kCreated: by the engine or the prewarm; kEngine: by the engine
SRWLOCK g_lock = SRWLOCK_INIT;
std::unordered_set<Key, KeyHash> g_shKnown, g_rsKnown;
std::unordered_map<const void*, Key> g_rsPtr;
std::unordered_map<Key, uint8_t, KeyHash> g_rec;
std::unordered_map<Key, IUnknown*, KeyHash> g_kept;  // PJH: prewarmed PSOs waiting for the engine's create (one reference each)
std::unordered_map<Key, IUnknown*, KeyHash> g_rsObj;  // PJH: the engine's root signatures by blob key (one reference held each)
constexpr size_t kMaxRsObj = 256;
std::atomic<bool> g_bootDone{false};  // the boot PSO list is done (title idle + quiet): from now on creates are journaled
void* volatile g_gameDev2 = nullptr;  // ID3D12Device2 interface of g_gameDev (the stream creates arrive on it)

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
std::atomic<uint64_t> g_seen{0}, g_skipped{0}, g_dropped{0}, g_bootCreates{0};
std::atomic<uint64_t> g_handed{0}, g_handMiss{0}, g_keptN{0}, g_keepRefused{0};
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

// rec / id = Encode(s) and its key (computed by the hook before the engine's create). Before the boot PSO list is done the
// desc is only marked as created by the engine (the game creates its boot list in every launch: nothing to prewarm);
// after it, every new desc is journaled (the runtime first-use creates that hitch).
void RecordPso(const Ser& s, std::vector<uint8_t>&& rec, const Key& id) {
    const bool boot = !g_bootDone.load(std::memory_order_acquire);
    bool fresh;
    AcquireSRWLockExclusive(&g_lock);
    auto it = g_rec.find(id);
    if (it == g_rec.end() && g_rec.size() >= kMaxRecKeys) {
        fresh = false;  // bookkeeping full: neither journaled nor marked
    } else {
        uint8_t& f = g_rec[id];
        fresh = !boot && !(f & kKnown);
        f |= kCreated | kEngine | (boot ? 0 : kKnown);
    }
    ReleaseSRWLockExclusive(&g_lock);
    if (boot) g_bootCreates.fetch_add(1, std::memory_order_relaxed);
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

// Serializes an engine desc before its create: false = not journaled (reason counted)
bool Prepare(int kind, const void* desc, Ser& s, std::vector<uint8_t>& rec, Key& id) {
    try {
        bool ok = false;
        if (kind == 1) ok = SerGfx(static_cast<const D3D12_GRAPHICS_PIPELINE_STATE_DESC*>(desc), s);
        if (kind == 2) ok = SerCs(static_cast<const D3D12_COMPUTE_PIPELINE_STATE_DESC*>(desc), s);
        if (kind == 3) ok = SerStream(static_cast<const D3D12_PIPELINE_STATE_STREAM_DESC*>(desc), s);
        if (ok) {
            rec = Encode(s);
            if (rec.size() > kMaxRecord) {
                s.why = 2;
                return false;
            }
            id = Hash128(rec.data(), rec.size());
        }
        return ok;
    } catch (...) {
        s.why = 3;
        return false;
    }
}
void CountSkip(const Ser& s) {
    g_skipped.fetch_add(1);
    InterlockedIncrement(reinterpret_cast<volatile LONG*>(&g_skipWhy[s.why & 3]));
}

// PJH: the prewarmed PSO for this desc, if one is waiting (it leaves the map; the caller owns its reference). A miss
// marks the desc as created by the engine before the engine's create runs, so a prewarm racing with it does not keep a
// second copy.
IUnknown* TakeKept(const Key& id) {
    IUnknown* p = nullptr;
    AcquireSRWLockExclusive(&g_lock);
    auto it = g_kept.find(id);
    if (it != g_kept.end()) {
        p = it->second;
        g_kept.erase(it);
    }
    if (g_rec.size() < kMaxRecKeys || g_rec.count(id)) g_rec[id] |= kCreated | kEngine;
    ReleaseSRWLockExclusive(&g_lock);
    if (p) g_keptN.fetch_sub(1, std::memory_order_relaxed);
    return p;
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
        // the pointer value is what the PSO descs carry; with PJH one reference to the first object per blob is kept so
        // the prewarm builds its PSOs on the engine's own root signature (a hand-over then carries the exact pointer
        // the engine would have passed)
        bool hold = false;
        AcquireSRWLockExclusive(&g_lock);
        if (g_rsPtr.size() < kMaxRsPtrs || g_rsPtr.count(rs)) g_rsPtr[rs] = k;
        const bool need = g_rsKnown.insert(k).second;
        if (g_cfg.handover && !g_rsObj.count(k) && g_rsObj.size() < kMaxRsObj) {
            g_rsObj[k] = static_cast<IUnknown*>(rs);
            hold = true;
        }
        ReleaseSRWLockExclusive(&g_lock);
        if (!hold) static_cast<IUnknown*>(rs)->Release();
        if (need && !Enqueue(kTagRootSig, k, std::move(data))) {
            AcquireSRWLockExclusive(&g_lock);
            g_rsKnown.erase(k);
            ReleaseSRWLockExclusive(&g_lock);
        }
    } catch (...) {
    }
}

// ---- boot PSO list timing (every engine create before the boot gate; PJ or PL on) + PL hooks (defined below) -------------
std::atomic<uint64_t> g_bN{0}, g_bTicks{0}, g_bMax{0};
std::atomic<int64_t> g_bFirst{0}, g_bLast{0};
std::atomic<uint32_t> g_bThreads{0};
thread_local bool t_bootSeen = false;
int64_t g_qpcStart = 0;                // QPC at process creation (log times are "+s after process start")
std::atomic<int64_t> g_devQpc{0};      // QPC when the first device was reported
void BootNote(int64_t t0, int64_t t1) {
    const uint64_t dt = (uint64_t)(t1 - t0);
    g_bN.fetch_add(1, std::memory_order_relaxed);
    g_bTicks.fetch_add(dt, std::memory_order_relaxed);
    for (uint64_t m = g_bMax.load(std::memory_order_relaxed); dt > m && !g_bMax.compare_exchange_weak(m, dt);) {
    }
    int64_t z = 0;
    g_bFirst.compare_exchange_strong(z, t0);
    for (int64_t m = g_bLast.load(std::memory_order_relaxed); t1 > m && !g_bLast.compare_exchange_weak(m, t1);) {
    }
    if (!t_bootSeen) {
        t_bootSeen = true;
        g_bThreads.fetch_add(1, std::memory_order_relaxed);
    }
}
std::atomic<int> g_plPhase{0};  // 0 not ready, 1 open (boot creates are loaded / stored), 2 closed, -1 off
bool PlTryLoad(int kind, ID3D12Device* self, const void* d, const Key& id, REFIID riid, void** pp);
void PlStore(int kind, const Key& id, IUnknown* obj);
std::atomic<uint64_t> g_plEarly{0};  // boot creates before the library was ready (not looked up, not stored)

// ---- device hooks --------------------------------------------------------------------------------------------------------
// One engine PSO create (graphics 1 / compute 2 / stream 3). The desc is serialized BEFORE the call: with PJH on, a
// prewarmed PSO for the same desc (same device method, same shaders, the engine's root signature) is handed back instead
// of a second compile, as a fresh reference of the requested interface; otherwise the engine's own call runs unchanged.
// Before the boot gate (the boot PSO list) every create is timed, and with PL = 1 it is first loaded from the pipeline
// library (the engine's own desc; the runtime refuses a desc that differs from the stored one) or stored after the call.
template <class Call>
HRESULT EngineCreate(int kind, ID3D12Device* self, const void* d, REFIID riid, void** pp, Call call) {
    Ser s;
    std::vector<uint8_t> rec;
    Key id;
    const bool boot = !g_bootDone.load(std::memory_order_acquire);
    const bool plOn = boot && g_cfg.pl == 1;
    const bool ok = d && (g_cfg.mode || plOn) && Prepare(kind, d, s, rec, id);
    if (boot) {
        const int64_t t0 = Qpc();
        bool loaded = false;
        HRESULT hr = E_FAIL;
        if (plOn && ok && pp) {
            if (g_plPhase.load(std::memory_order_acquire) == 1) loaded = PlTryLoad(kind, self, d, id, riid, pp);
            else if (g_plPhase.load(std::memory_order_relaxed) == 0) g_plEarly.fetch_add(1, std::memory_order_relaxed);
        }
        if (loaded) hr = S_OK;
        else hr = call();
        BootNote(t0, Qpc());
        if (hr == S_OK && d && pp && *pp) {
            NoteCreate(self);
            if (plOn && ok && !loaded) PlStore(kind, id, static_cast<IUnknown*>(*pp));
            if (g_cfg.mode) {
                if (ok) RecordPso(s, std::move(rec), id);
                else CountSkip(s);
            }
        }
        return hr;
    }
    if (!g_cfg.mode) return call();  // PL only: after the gate every create is the engine's own, untouched
    if (ok && pp && g_pjhSwitch && (self == g_gameDev || self == g_gameDev2)) {
        if (IUnknown* k = TakeKept(id)) {
            void* out = nullptr;
            const HRESULT q = k->QueryInterface(riid, &out);
            k->Release();
            if (SUCCEEDED(q) && out) {
                *pp = out;
                NoteCreate(self);
                g_handed.fetch_add(1, std::memory_order_relaxed);
                return S_OK;
            }
        } else {
            g_handMiss.fetch_add(1, std::memory_order_relaxed);
        }
    }
    const HRESULT hr = call();
    if (hr == S_OK && d && pp && *pp) {
        NoteCreate(self);
        if (ok) RecordPso(s, std::move(rec), id);
        else CountSkip(s);
    }
    return hr;
}
template <int S>
HRESULT STDMETHODCALLTYPE HkGfx(ID3D12Device* self, const D3D12_GRAPHICS_PIPELINE_STATE_DESC* d, REFIID riid, void** pp) {
    if (t_depth) return g_set[S].gfx(self, d, riid, pp);
    Depth g;
    return EngineCreate(1, self, d, riid, pp, [&]() { return g_set[S].gfx(self, d, riid, pp); });
}
template <int S>
HRESULT STDMETHODCALLTYPE HkCs(ID3D12Device* self, const D3D12_COMPUTE_PIPELINE_STATE_DESC* d, REFIID riid, void** pp) {
    if (t_depth) return g_set[S].cs(self, d, riid, pp);
    Depth g;
    return EngineCreate(2, self, d, riid, pp, [&]() { return g_set[S].cs(self, d, riid, pp); });
}
template <int S>
HRESULT STDMETHODCALLTYPE HkStream(ID3D12Device* self, const D3D12_PIPELINE_STATE_STREAM_DESC* d, REFIID riid, void** pp) {
    if (t_depth) return g_set[S].stream(self, d, riid, pp);
    Depth g;
    return EngineCreate(3, self, d, riid, pp, [&]() { return g_set[S].stream(self, d, riid, pp); });
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

// ---- PL: boot pipeline library ---------------------------------------------------------------------------------------------
constexpr uint32_t kPlMagic = 0x4C50334E;  // "N3PL"
constexpr uint32_t kPlVersion = 1;
struct PlHeader {
    uint32_t magic, version, exeStamp, exeSize;
    char devClass[32];
    uint32_t vendor, device, subsys, revision;  // DXGI adapter ids
    uint64_t umd;                               // user-mode driver version (IDXGIAdapter::CheckInterfaceSupport)
    uint64_t blobSize;                          // D3D12 blob bytes after the header
    uint64_t sumA, sumB;                        // Hash128 of the blob
    uint32_t entries, flags;
    uint64_t reserved[3];
};
static_assert(sizeof(PlHeader) == 128, "PL header");
constexpr size_t kPlIdent = offsetof(PlHeader, blobSize);  // the part that must equal this session's

struct PlState {
    ID3D12PipelineLibrary* lib = nullptr;
    ID3D12PipelineLibrary1* lib1 = nullptr;  // stream (mesh-shader) PSOs
    void* dev = nullptr;                     // the device the library belongs to (no reference) and its Device2 pointer
    void* dev2 = nullptr;
    bool fromFile = false;  // holds a previous session's pipelines: boot creates are looked up first
    bool rebuilt = false;   // a file existed but was refused: the new library is written at the gate
    uint32_t fileEntries = 0;
    HANDLE file = INVALID_HANDLE_VALUE, map = nullptr;  // kept for the whole session (the blob must outlive the library)
    const uint8_t* view = nullptr;
    std::wstring path;
    PlHeader want{};
} g_pl;
SRWLOCK g_plLock = SRWLOCK_INIT;  // shared: one load / store; exclusive: close
SRWLOCK g_plStripe[64] = {};      // per name: loads of the same PSO must not run concurrently (D3D12 docs)
std::atomic<uint64_t> g_plLoaded{0}, g_plLoadTicks{0}, g_plMiss{0}, g_plLoadErr{0}, g_plStored{0}, g_plStoreErr{0};
std::atomic<bool> g_plInit{false}, g_plBroken{false};
std::atomic<int> g_plDone{0};  // 1 = closed (and saved when needed)
char g_plCache[160] = "";      // driver shader-cache support line

void PlName(const Key& k, wchar_t* n, size_t cap) { swprintf(n, cap, L"%016llx%016llx", (unsigned long long)k.a, (unsigned long long)k.b); }

HRESULT PlLoadRaw(ID3D12PipelineLibrary* lib, ID3D12PipelineLibrary1* lib1, int kind, const void* d, const wchar_t* name,
                  REFIID riid, void** out) {
    __try {
        if (kind == 1) return lib->LoadGraphicsPipeline(name, static_cast<const D3D12_GRAPHICS_PIPELINE_STATE_DESC*>(d), riid, out);
        if (kind == 2) return lib->LoadComputePipeline(name, static_cast<const D3D12_COMPUTE_PIPELINE_STATE_DESC*>(d), riid, out);
        if (kind == 3 && lib1) return lib1->LoadPipeline(name, static_cast<const D3D12_PIPELINE_STATE_STREAM_DESC*>(d), riid, out);
        return E_NOTIMPL;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return E_UNEXPECTED;
    }
}
HRESULT PlStoreRaw(ID3D12PipelineLibrary* lib, const wchar_t* name, ID3D12PipelineState* p) {
    __try {
        return lib->StorePipeline(name, p);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return E_UNEXPECTED;
    }
}

bool PlTryLoad(int kind, ID3D12Device* self, const void* d, const Key& id, REFIID riid, void** pp) {
    if (!g_pl.fromFile || g_plBroken.load(std::memory_order_relaxed) || (self != g_pl.dev && self != g_pl.dev2)) return false;
    wchar_t name[40];
    PlName(id, name, 40);
    bool got = false;
    AcquireSRWLockShared(&g_plLock);
    if (g_plPhase.load(std::memory_order_acquire) == 1) {
        SRWLOCK* st = &g_plStripe[id.a & 63];
        void* out = nullptr;
        AcquireSRWLockExclusive(st);
        const int64_t t0 = Qpc();
        const HRESULT h = PlLoadRaw(g_pl.lib, g_pl.lib1, kind, d, name, riid, &out);
        const int64_t t1 = Qpc();
        ReleaseSRWLockExclusive(st);
        if (SUCCEEDED(h) && out) {
            *pp = out;
            got = true;
            g_plLoaded.fetch_add(1, std::memory_order_relaxed);
            g_plLoadTicks.fetch_add((uint64_t)(t1 - t0), std::memory_order_relaxed);
        } else {
            if (out) static_cast<IUnknown*>(out)->Release();
            if (h == E_INVALIDARG) {
                g_plMiss.fetch_add(1, std::memory_order_relaxed);  // not in the library (or stored with another desc)
            } else {
                if (g_plLoadErr.fetch_add(1) < 4) LOG("PL: a library load failed (hr %08lX) - the game's own create runs", (unsigned long)h);
                if (h == E_UNEXPECTED) g_plBroken = true;  // an exception inside the runtime: no more library calls
            }
        }
    }
    ReleaseSRWLockShared(&g_plLock);
    return got;
}

void PlStore(int kind, const Key& id, IUnknown* obj) {
    if (g_plPhase.load(std::memory_order_acquire) != 1 || g_plBroken.load(std::memory_order_relaxed) || (kind == 3 && !g_pl.lib1))
        return;
    ID3D12PipelineState* ps = nullptr;
    if (FAILED(obj->QueryInterface(kIID_Pso, reinterpret_cast<void**>(&ps))) || !ps) return;
    ID3D12Device* owner = nullptr;  // only PSOs of the library's device
    const bool mine = SUCCEEDED(ps->GetDevice(kIID_Device, reinterpret_cast<void**>(&owner))) && owner && owner == g_pl.dev;
    if (owner) owner->Release();
    if (mine) {
        wchar_t name[40];
        PlName(id, name, 40);
        AcquireSRWLockShared(&g_plLock);
        if (g_plPhase.load(std::memory_order_acquire) == 1) {
            SRWLOCK* st = &g_plStripe[id.a & 63];
            AcquireSRWLockExclusive(st);
            const HRESULT h = PlStoreRaw(g_pl.lib, name, ps);
            ReleaseSRWLockExclusive(st);
            if (SUCCEEDED(h)) g_plStored.fetch_add(1, std::memory_order_relaxed);
            else if (g_plStoreErr.fetch_add(1) < 4) LOG("PL: storing a boot PSO failed (hr %08lX)", (unsigned long)h);
            if (h == E_UNEXPECTED) g_plBroken = true;
        }
        ReleaseSRWLockShared(&g_plLock);
    }
    ps->Release();
}

// adapter ids + UMD driver version through the SYSTEM dxgi.dll (not a proxy in the game folder); zeros when unavailable
// (the D3D12 runtime still refuses a library blob from another driver: D3D12_ERROR_DRIVER_VERSION_MISMATCH)
void PlAdapter(LUID luid, PlHeader& h) {
    __try {
        wchar_t sys[MAX_PATH];
        const UINT n = GetSystemDirectoryW(sys, MAX_PATH);
        if (!n || n > MAX_PATH - 16) return;
        wcscat_s(sys, L"\\dxgi.dll");
        HMODULE m = LoadLibraryW(sys);
        auto cf = m ? reinterpret_cast<HRESULT(WINAPI*)(REFIID, void**)>(GetProcAddress(m, "CreateDXGIFactory1")) : nullptr;
        IDXGIFactory4* f = nullptr;
        if (!cf || FAILED(cf(__uuidof(IDXGIFactory4), reinterpret_cast<void**>(&f))) || !f) return;
        IDXGIAdapter1* a = nullptr;
        if (SUCCEEDED(f->EnumAdapterByLuid(luid, __uuidof(IDXGIAdapter1), reinterpret_cast<void**>(&a))) && a) {
            DXGI_ADAPTER_DESC1 ds{};
            if (SUCCEEDED(a->GetDesc1(&ds))) {
                h.vendor = ds.VendorId;
                h.device = ds.DeviceId;
                h.subsys = ds.SubSysId;
                h.revision = ds.Revision;
            }
            LARGE_INTEGER umd{};
            if (SUCCEEDED(a->CheckInterfaceSupport(__uuidof(IDXGIDevice), &umd))) h.umd = (uint64_t)umd.QuadPart;
            a->Release();
        }
        f->Release();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

// verifies the mapped blob; false on a mismatch or an in-page error
bool PlVerify(const uint8_t* blob, uint64_t n, const PlHeader& h) {
    __try {
        const Key k = Hash128(blob, (size_t)n);
        return k.a == h.sumA && k.b == h.sumB;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
HRESULT PlCreate(ID3D12Device1* d1, const void* blob, SIZE_T n, ID3D12PipelineLibrary** out) {
    __try {
        return d1->CreatePipelineLibrary(blob, n, __uuidof(ID3D12PipelineLibrary), reinterpret_cast<void**>(out));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return E_UNEXPECTED;
    }
}
void PlUnmap() {
    if (g_pl.view) UnmapViewOfFile(g_pl.view);
    if (g_pl.map) CloseHandle(g_pl.map);
    if (g_pl.file != INVALID_HANDLE_VALUE) CloseHandle(g_pl.file);
    g_pl.view = nullptr;
    g_pl.map = nullptr;
    g_pl.file = INVALID_HANDLE_VALUE;
}
const char* HrName(HRESULT h) {
    switch ((unsigned long)h) {
        case 0x887E0001: return "D3D12_ERROR_ADAPTER_NOT_FOUND";
        case 0x887E0002: return "D3D12_ERROR_DRIVER_VERSION_MISMATCH";
        case 0x80070057: return "E_INVALIDARG";
        case 0x887A0004: return "DXGI_ERROR_UNSUPPORTED";
        case 0x8000FFFF: return "E_UNEXPECTED";
        default: return "error";
    }
}

void DevClass(ID3D12Device* d, char* out, size_t n);
Header MakeHeader(const char* devClass);

// Opens (validates, maps) or creates the library. PL init thread, once, for the first device.
void PlOpen(ID3D12Device* dev) {
    const int64_t t0 = Qpc();
    D3D12_FEATURE_DATA_SHADER_CACHE sc{};
    const bool scOk = SUCCEEDED(dev->CheckFeatureSupport(D3D12_FEATURE_SHADER_CACHE, &sc, sizeof sc));
    const UINT f = scOk ? (UINT)sc.SupportFlags : 0;
    PlHeader& w = g_pl.want;
    PlAdapter(dev->GetAdapterLuid(), w);
    snprintf(g_plCache, sizeof g_plCache, "driver shader cache flags %X (%s%s%s%s%s), adapter %04X:%04X rev %X, UMD %u.%u.%u.%u", f,
             f & D3D12_SHADER_CACHE_SUPPORT_LIBRARY ? "library " : "", f & D3D12_SHADER_CACHE_SUPPORT_AUTOMATIC_INPROC_CACHE ? "in-proc " : "",
             f & D3D12_SHADER_CACHE_SUPPORT_AUTOMATIC_DISK_CACHE ? "disk " : "", f & D3D12_SHADER_CACHE_SUPPORT_DRIVER_MANAGED_CACHE ? "driver-managed " : "",
             f ? "" : "none", w.vendor, w.device, w.revision, (unsigned)(w.umd >> 48), (unsigned)(w.umd >> 32) & 0xFFFF,
             (unsigned)(w.umd >> 16) & 0xFFFF, (unsigned)w.umd & 0xFFFF);
    LOG("PL: %s", g_plCache);
    if (g_cfg.pl != 1) {
        g_plPhase = -1;
        return;
    }
    if (!(f & D3D12_SHADER_CACHE_SUPPORT_LIBRARY)) {
        LOG("PL: the driver has no pipeline-library support - PL off for this session");
        g_plPhase = -1;
        return;
    }
    ID3D12Device1* d1 = nullptr;
    if (FAILED(dev->QueryInterface(__uuidof(ID3D12Device1), reinterpret_cast<void**>(&d1))) || !d1) {
        LOG("PL: no ID3D12Device1 - PL off for this session");
        g_plPhase = -1;
        return;
    }
    char cls[32];
    DevClass(dev, cls, sizeof cls);
    const Header jh = MakeHeader(cls);
    w.magic = kPlMagic;
    w.version = kPlVersion;
    w.exeStamp = jh.exeStamp;
    w.exeSize = jh.exeSize;
    memcpy(w.devClass, jh.devClass, sizeof w.devClass);
    CreateDirectoryW(g_cfg.folder.c_str(), nullptr);
    wchar_t wcls[32];
    MultiByteToWideChar(CP_UTF8, 0, cls + 3, -1, wcls, 32);  // "pj_win_dgpu_sm68" -> pl_win_dgpu_sm68.bin
    g_pl.path = g_cfg.folder + L"\\pl_" + wcls + L".bin";
    const std::wstring fresh = g_pl.path + L".new";
    // a library written while the old one was mapped (previous session): promote it when its header is whole
    if (GetFileAttributesW(fresh.c_str()) != INVALID_FILE_ATTRIBUTES) {
        HANDLE h = CreateFileW(fresh.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        PlHeader nh{};
        LARGE_INTEGER sz{};
        const bool good = h != INVALID_HANDLE_VALUE && GetFileSizeEx(h, &sz) && ReadAt(h, 0, &nh, sizeof nh) && nh.magic == kPlMagic &&
                          (uint64_t)sz.QuadPart == sizeof nh + nh.blobSize;
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
        if (!good || !MoveFileExW(fresh.c_str(), g_pl.path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            DeleteFileW(fresh.c_str());
    }
    ID3D12PipelineLibrary* lib = nullptr;
    const char* why = "no library file yet";
    uint64_t blobBytes = 0;
    HANDLE fh = CreateFileW(g_pl.path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (fh != INVALID_HANDLE_VALUE) {
        g_pl.file = fh;
        PlHeader have{};
        LARGE_INTEGER sz{};
        if (!GetFileSizeEx(fh, &sz) || (uint64_t)sz.QuadPart < sizeof have || !ReadAt(fh, 0, &have, sizeof have)) {
            why = "file too short";
        } else if (have.magic != kPlMagic || have.version != kPlVersion) {
            why = "not a PL file / other PL version";
        } else if (have.exeStamp != w.exeStamp || have.exeSize != w.exeSize) {
            why = "other game build";
        } else if (memcmp(&have, &w, kPlIdent) != 0) {
            why = "other GPU / driver version / device class";
        } else if ((uint64_t)sz.QuadPart != sizeof have + have.blobSize || !have.blobSize || have.blobSize > g_cfg.plMaxBytes) {
            why = "size mismatch (cut or over PL_MaxMB)";
        } else {
            g_pl.map = CreateFileMappingW(fh, nullptr, PAGE_READONLY, 0, 0, nullptr);
            g_pl.view = g_pl.map ? static_cast<const uint8_t*>(MapViewOfFile(g_pl.map, FILE_MAP_READ, 0, 0, 0)) : nullptr;
            if (!g_pl.view) {
                why = "cannot map the file";
            } else if (!PlVerify(g_pl.view + sizeof have, have.blobSize, have)) {
                why = "checksum mismatch (corrupt file)";
            } else {
                const HRESULT h = PlCreate(d1, g_pl.view + sizeof have, (SIZE_T)have.blobSize, &lib);
                if (SUCCEEDED(h) && lib) {
                    g_pl.fromFile = true;
                    g_pl.fileEntries = have.entries;
                    blobBytes = have.blobSize;
                } else {
                    lib = nullptr;
                    static char b[96];
                    snprintf(b, sizeof b, "D3D12 refused the blob: %s (hr %08lX)", HrName(h), (unsigned long)h);
                    why = b;
                }
            }
        }
        if (!g_pl.fromFile) {
            g_pl.rebuilt = true;
            PlUnmap();
        }
    }
    if (!lib) {
        const HRESULT h = PlCreate(d1, nullptr, 0, &lib);
        if (FAILED(h) || !lib) {
            LOG("PL: CreatePipelineLibrary failed (%s, hr %08lX) - PL off for this session", HrName(h), (unsigned long)h);
            d1->Release();
            PlUnmap();
            g_plPhase = -1;
            return;
        }
    }
    d1->Release();
    ID3D12PipelineLibrary1* lib1 = nullptr;
    if (FAILED(lib->QueryInterface(__uuidof(ID3D12PipelineLibrary1), reinterpret_cast<void**>(&lib1)))) lib1 = nullptr;
    void* d2 = nullptr;
    if (SUCCEEDED(dev->QueryInterface(kIID_Device2, &d2)) && d2) static_cast<IUnknown*>(d2)->Release();  // pointer only
    g_pl.lib = lib;
    g_pl.lib1 = lib1;
    g_pl.dev = dev;
    g_pl.dev2 = d2;
    int expect = 0;
    if (!g_plPhase.compare_exchange_strong(expect, 1)) {  // the boot gate passed while opening: nothing to do any more
        g_pl.lib = nullptr;
        g_pl.lib1 = nullptr;
        if (lib1) lib1->Release();
        lib->Release();
        LOG("PL: the boot list ended before the library was ready - unused this session");
        return;
    }
    const double ms = (double)(Qpc() - t0) * g_qpcMs;
    if (g_pl.fromFile)
        LOG("PL: library %ls: %u boot pipelines, %.1f MB, mapped + verified in %.0f ms (+%.1f s after process start)", g_pl.path.c_str(),
            g_pl.fileEntries, (double)blobBytes / 1048576.0, ms, (double)(Qpc() - g_qpcStart) * g_qpcMs / 1000.0);
    else
        LOG("PL: new library (%s); boot pipelines are stored and written at the title (+%.1f s after process start)", why,
            (double)(Qpc() - g_qpcStart) * g_qpcMs / 1000.0);
}

DWORD WINAPI PlInitMain(LPVOID p) {
    ID3D12Device* dev = static_cast<ID3D12Device*>(p);
    try {
        PlOpen(dev);
    } catch (...) {
        LOG("PL: out of memory while opening the library - PL off for this session");
        int e = 0;
        g_plPhase.compare_exchange_strong(e, -1);
    }
    dev->Release();
    return 0;
}

HRESULT PlSerializeRaw(ID3D12PipelineLibrary* lib, void* buf, SIZE_T n) {
    __try {
        return lib->Serialize(buf, n);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return E_UNEXPECTED;
    }
}
SIZE_T PlSizeRaw(ID3D12PipelineLibrary* lib) {
    __try {
        return lib->GetSerializedSize();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

// writes the library (header + blob) to <path>.tmp, then renames it to <path> (or <path>.new while <path> is mapped)
void PlSave() {
    const uint64_t stored = g_plStored.load();
    if (!stored) return;
    const int64_t t0 = Qpc();
    const SIZE_T sz = PlSizeRaw(g_pl.lib);
    if (!sz || sz > g_cfg.plMaxBytes) {
        LOG("PL: library not written: %.1f MB (PL_MaxMB = %llu)", (double)sz / 1048576.0, (unsigned long long)(g_cfg.plMaxBytes >> 20));
        return;
    }
    ULARGE_INTEGER freeB{};
    if (GetDiskFreeSpaceExW(g_cfg.folder.c_str(), &freeB, nullptr, nullptr) && freeB.QuadPart < (uint64_t)sz + (256ull << 20)) {
        LOG("PL: library not written: %.1f MB needed, the disk has %.1f MB free", (double)sz / 1048576.0, (double)freeB.QuadPart / 1048576.0);
        return;
    }
    uint8_t* buf = static_cast<uint8_t*>(VirtualAlloc(nullptr, sz, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!buf) {
        LOG("PL: library not written: no memory for %.1f MB", (double)sz / 1048576.0);
        return;
    }
    const HRESULT h = PlSerializeRaw(g_pl.lib, buf, sz);
    bool ok = SUCCEEDED(h);
    if (!ok) LOG("PL: Serialize failed (%s, hr %08lX) - library not written", HrName(h), (unsigned long)h);
    const std::wstring tmp = g_pl.path + L".tmp", target = g_pl.view ? g_pl.path + L".new" : g_pl.path;
    if (ok) {
        PlHeader hd = g_pl.want;
        hd.blobSize = sz;
        const Key k = Hash128(buf, sz);
        hd.sumA = k.a;
        hd.sumB = k.b;
        hd.entries = (uint32_t)std::min<uint64_t>(0xFFFFFFFFull, (g_pl.fromFile ? g_pl.fileEntries : 0) + stored);
        HANDLE fh = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        ok = fh != INVALID_HANDLE_VALUE && WriteAll(fh, 0, &hd, sizeof hd);
        for (uint64_t off = 0; ok && off < sz; off += 16u << 20) {
            const size_t n = (size_t)std::min<uint64_t>(16u << 20, sz - off);
            ok = WriteAll(fh, sizeof hd + off, buf + off, n);
        }
        if (ok) ok = FlushFileBuffers(fh) != 0;
        if (fh != INVALID_HANDLE_VALUE) CloseHandle(fh);
        if (ok) ok = MoveFileExW(tmp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
        if (!ok) {
            LOG("PL: writing %ls failed (error %lu) - library not written", target.c_str(), GetLastError());
            DeleteFileW(tmp.c_str());
        } else {
            LOG("PL: library written: %u boot pipelines, %.1f MB in %.0f ms -> %ls%s", hd.entries, (double)sz / 1048576.0,
                (double)(Qpc() - t0) * g_qpcMs, target.c_str(), g_pl.view ? " (replaces the current file at the next launch)" : "");
        }
    }
    VirtualFree(buf, 0, MEM_RELEASE);
}

void LogBoot() {
    const double s = g_qpcMs / 1000.0;
    const uint64_t n = g_bN.load(), loaded = g_plLoaded.load();
    const int64_t first = g_bFirst.load(), last = g_bLast.load(), dev = g_devQpc.load();
    LOG("boot PSO list: %llu engine creates on %u thread(s), first +%.2f s, last +%.2f s after process start (device +%.2f s, gate "
        "+%.2f s) = %.2f s wall; create calls %.2f thread-s (avg %.2f ms, max %.1f ms)",
        (unsigned long long)n, g_bThreads.load(), first ? (double)(first - g_qpcStart) * s : 0.0,
        last ? (double)(last - g_qpcStart) * s : 0.0, dev ? (double)(dev - g_qpcStart) * s : 0.0, (double)(Qpc() - g_qpcStart) * s,
        first && last ? (double)(last - first) * s : 0.0, (double)g_bTicks.load() * s, n ? (double)g_bTicks.load() * g_qpcMs / (double)n : 0.0,
        (double)g_bMax.load() * g_qpcMs);
    if (g_cfg.pl)
        LOG("PL = %d: %llu loaded from the library (avg %.3f ms), %llu not in it, %llu stored, %llu before the library was ready, "
            "%llu load / %llu store errors%s",
            g_cfg.pl, (unsigned long long)loaded, loaded ? (double)g_plLoadTicks.load() * g_qpcMs / (double)loaded : 0.0,
            (unsigned long long)g_plMiss.load(), (unsigned long long)g_plStored.load(), (unsigned long long)g_plEarly.load(),
            (unsigned long long)g_plLoadErr.load(), (unsigned long long)g_plStoreErr.load(),
            g_cfg.pl == 2 ? " (installed + off: timing only)" : g_plBroken ? " - stopped after an exception" : "");
}

// the boot gate passed: close the library (no more loads / stores), log the boot list, write the library when it grew
DWORD WINAPI PlCloseMain(LPVOID) {
    SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);
    AcquireSRWLockExclusive(&g_plLock);
    const int was = g_plPhase.exchange(2);
    ReleaseSRWLockExclusive(&g_plLock);
    LogBoot();
    if (was == 1) {
        try {
            if (!g_plBroken) PlSave();
        } catch (...) {
            LOG("PL: out of memory while writing the library");
        }
        if (g_pl.lib1) g_pl.lib1->Release();
        if (g_pl.lib) g_pl.lib->Release();  // the mapped blob stays (PSOs loaded from it may still refer to it)
        g_pl.lib1 = nullptr;
        g_pl.lib = nullptr;
    }
    g_plDone = 1;
    return 0;
}
void OnBootDone() {
    if (!g_cfg.pl) return;
    HANDLE t = CreateThread(nullptr, 0, PlCloseMain, nullptr, 0, nullptr);
    if (t) CloseHandle(t);
}

// ---- prewarm ---------------------------------------------------------------------------------------------------------------
struct Prewarm {
    HANDLE file = INVALID_HANDLE_VALUE;
    ID3D12Device* dev = nullptr;
    const Set* set = nullptr;
    const Set* set2 = nullptr;  // the set holding CreatePipelineState (Device2 vtable)
    ID3D12Device* dev2 = nullptr;
    std::unordered_map<Key, std::vector<uint8_t>, KeyHash> sh;  // shader blobs read so far (payload incl. key)
    std::unordered_map<Key, void*, KeyHash> rs;                // root signatures created for the prewarm (own reference)
    bool ownRs = false;                                        // the record being built uses a root signature of ours

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
        AcquireSRWLockShared(&g_lock);  // the engine's own object for this blob (PJH holds a reference)
        auto eo = g_rsObj.find(k);
        void* eng = eo != g_rsObj.end() ? eo->second : nullptr;
        ReleaseSRWLockShared(&g_lock);
        if (eng) return eng;
        ownRs = true;
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
        ownRs = false;
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
    // keep: PJH holds the new PSO for the engine's create of the same desc (id) instead of releasing it at once
    HRESULT Create(uint8_t kind, std::vector<uint64_t>& fixed, const Key& id, bool keep) {
        void* pso = nullptr;
        HRESULT hr = E_FAIL;
        if (kind == 1) hr = set->gfx(dev, reinterpret_cast<const D3D12_GRAPHICS_PIPELINE_STATE_DESC*>(fixed.data()), kIID_Pso, &pso);
        if (kind == 2) hr = set->cs(dev, reinterpret_cast<const D3D12_COMPUTE_PIPELINE_STATE_DESC*>(fixed.data()), kIID_Pso, &pso);
        if (kind == 3) {
            if (!set2 || !set2->stream || !dev2) return E_NOINTERFACE;
            const D3D12_PIPELINE_STATE_STREAM_DESC sd = {fixed.size() * 8, fixed.data()};
            hr = set2->stream(dev2, &sd, kIID_Pso, &pso);
        }
        if (SUCCEEDED(hr) && pso) {
            IUnknown* o = static_cast<IUnknown*>(pso);
            bool kept = false;
            if (keep && !ownRs) {
                AcquireSRWLockExclusive(&g_lock);
                auto f = g_rec.find(id);
                const bool engineHas = f != g_rec.end() && (f->second & kEngine);
                if (!engineHas && g_kept.size() < g_cfg.keepMax && g_kept.emplace(id, o).second) kept = true;
                ReleaseSRWLockExclusive(&g_lock);
                if (kept) g_keptN.fetch_add(1, std::memory_order_relaxed);
                else if (!engineHas) g_keepRefused.fetch_add(1, std::memory_order_relaxed);
            }
            if (!kept) o->Release();
        }
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
        InterlockedExchangePointer(&g_gameDev2, d2);  // no reference: the device lives as long as the game renders
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
            const HRESULT hr = pw.Create(kind, fixed, id, g_cfg.handover != 0);
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

// boot gate (every mode): the engine has created PSOs (its boot list), none for quietMs, and no level load is running.
// Creates before it are the boot list (made in every launch: not journaled; PL loads / stores them); creates after it are
// runtime creates (journaled; PJH).
void BootGate() {
    for (;;) {
        const int64_t last = g_lastCreate;
        if (last && (double)(Qpc() - last) * g_qpcMs >= g_cfg.quietMs && TitleIdle()) break;
        Sleep(250);
    }
    g_bootDone.store(true, std::memory_order_release);
    if (g_cfg.pl) OnBootDone();  // logs the boot list + closes / writes the library on its own thread
    else LogBoot();
}
// PL without PJ: only the gate
DWORD WINAPI GateMain(LPVOID) {
    while (!g_gameDev) Sleep(50);
    BootGate();
    return 0;
}

DWORD WINAPI PrewarmMain(LPVOID) {
    WaitForSingleObject(g_loadedEvent, INFINITE);
    BootGate();
    LOG("boot PSO list done: %llu engine creates so far (not journaled); runtime creates are journaled from now on",
        (unsigned long long)g_bootCreates.load());
    if (g_cfg.mode != 1 || g_startRecs.empty() || g_file == INVALID_HANDLE_VALUE) {
        std::vector<std::vector<uint8_t>>().swap(g_startRecs);
        g_pwState = 2;
        return 0;
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
bool Wanted(const wchar_t* iniPath) {
    return iniPath && (GetPrivateProfileIntW(L"Patches", L"PJ", 0, iniPath) != 0 || GetPrivateProfileIntW(L"Patches", L"PJH", 0, iniPath) != 0 ||
                       GetPrivateProfileIntW(L"Patches", L"PL", 0, iniPath) != 0);
}

void Start(const StartInfo& info) {
    g_log = info.log;
    const wchar_t* ini = info.iniPath ? info.iniPath : L"";
    int mode = (int)GetPrivateProfileIntW(L"Patches", L"PJ", 0, ini);
    if (mode < 0 || mode > 2) mode = 0;
    int pjh = (int)GetPrivateProfileIntW(L"Patches", L"PJH", 0, ini);
    if (pjh < 0 || pjh > 2) pjh = 0;
    if (pjh && mode != 1) mode = 1;  // the hand-over needs the journal + prewarm
    g_cfg.mode = mode;
    g_cfg.handover = pjh;
    g_pjhSwitch = pjh == 1 ? 1u : 0u;
    int pl = (int)GetPrivateProfileIntW(L"Patches", L"PL", 0, ini);
    if (pl < 0 || pl > 2) pl = 0;
    const char* plWhy = nullptr;  // PL = 1 refused: another hook below would see different creates, or not Windows
    if (pl == 1) {
        if (info.wine) plWhy = "Proton/Wine (vkd3d-proton keeps its own pipeline cache)";
        else if (GetPrivateProfileIntW(L"ShaderSwap", L"Enabled", 0, ini) || GetPrivateProfileIntW(L"Profiler", L"GPUProf", 0, ini) >= 2)
            plWhy = "[ShaderSwap] Enabled or [Profiler] GPUProf >= 2 hooks PSO creation (library loads would bypass it)";
        else if (GetPrivateProfileIntW(L"Debug", L"GIProbe", 0, ini) || GetPrivateProfileIntW(L"Patches", L"GIMerge", 0, ini))
            plWhy = "GIProbe / GIMerge hook PSO creation";
        if (plWhy) pl = 2;
    }
    g_cfg.pl = pl;
    int plMb = (int)GetPrivateProfileIntW(L"Patches", L"PL_MaxMB", 1024, ini);
    plMb = plMb < 16 ? 16 : plMb > 4000 ? 4000 : plMb;
    g_cfg.plMaxBytes = (uint64_t)plMb << 20;
    if (!mode && !pl) return;
    int keep = (int)GetPrivateProfileIntW(L"Patches", L"PJ_KeepMax", 4096, ini);
    g_cfg.keepMax = (uint32_t)(keep < 0 ? 0 : keep > 16384 ? 16384 : keep);
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
    {  // process start on the QPC scale (the boot line reports seconds after process start)
        FILETIME c{}, e{}, k{}, u{}, now{};
        GetSystemTimePreciseAsFileTime(&now);
        const int64_t qNow = Qpc();
        int64_t ago = 0;
        if (GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u)) {
            const int64_t n100 = ((int64_t)now.dwHighDateTime << 32 | now.dwLowDateTime) - ((int64_t)c.dwHighDateTime << 32 | c.dwLowDateTime);
            if (n100 > 0 && n100 < 36000000000ll) ago = (int64_t)((double)n100 / 10000.0 / g_qpcMs);
        }
        g_qpcStart = qNow - ago;
    }
    if (mode) {
        g_qEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        g_loadedEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        HANDLE w = CreateThread(nullptr, 0, WriterMain, nullptr, 0, nullptr);
        HANDLE p = CreateThread(nullptr, 0, PrewarmMain, nullptr, 0, nullptr);
        if (w) CloseHandle(w);
        if (p) CloseHandle(p);
        LOG("PJ = %d (%s), journal folder %ls, max %d MB, quiet %d ms, title gate %s; PJH = %d (%s, keep <= %u)", mode,
            mode == 1 ? "journal + title-screen prewarm" : "journal only", g_cfg.folder.c_str(), mb, g_cfg.quietMs,
            g_cfg.known && g_cfg.base ? "G+58Ah" : "off (quiet time only)", pjh,
            pjh == 1 ? "hand-over ON" : pjh == 2 ? "installed, switch OFF" : "off", g_cfg.keepMax);
    } else {
        HANDLE g = CreateThread(nullptr, 0, GateMain, nullptr, 0, nullptr);
        if (g) CloseHandle(g);
    }
    if (pl)
        LOG("PL = %d (%s), folder %ls, max %d MB, boot gate = quiet %d ms + %s", pl,
            pl == 1 ? "boot pipeline library" : plWhy ? plWhy : "installed + off: boot-list timing only", g_cfg.folder.c_str(), plMb,
            g_cfg.quietMs, g_cfg.known && g_cfg.base ? "G+58Ah clear" : "quiet time only");
}

void OnDevice(void* device) {
    if ((!g_cfg.mode && !g_cfg.pl) || !device) return;
    int64_t z = 0;
    g_devQpc.compare_exchange_strong(z, Qpc());
    __try {
        OnDeviceImpl(device);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        LOG("exception 0x%08lX while hooking a device - this device is not journaled", GetExceptionCode());
        return;
    }
    // PL: the first device reported gets the library (opened on its own thread: the engine's thread is not held)
    if (g_cfg.pl && !g_plInit.exchange(true)) {
        void* dev = nullptr;
        if (FAILED(static_cast<IUnknown*>(device)->QueryInterface(kIID_Device, &dev)) || !dev) return;
        HANDLE t = CreateThread(nullptr, 0, PlInitMain, dev, 0, nullptr);
        if (t) CloseHandle(t);
        else static_cast<IUnknown*>(dev)->Release();
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
    o.bootDone = g_bootDone;
    o.bootCreates = g_bootCreates;
    o.handover = g_cfg.handover;
    o.handed = g_handed;
    o.handMiss = g_handMiss;
    o.kept = g_keptN;
    o.keepRefused = g_keepRefused;
    o.pl = g_cfg.pl;
    o.plPhase = g_plPhase;
    o.plFromFile = g_pl.fromFile;
    o.plRebuilt = g_pl.rebuilt;
    o.plFileEntries = g_pl.fileEntries;
    o.plLoaded = g_plLoaded;
    o.plMiss = g_plMiss;
    o.plStored = g_plStored;
    o.plEarly = g_plEarly;
    o.plErrors = g_plLoadErr + g_plStoreErr;
    o.plDone = g_plDone != 0;
    o.bootN = g_bN;
    o.bootThreads = g_bThreads;
}

bool WaitPlDoneForTest(unsigned long ms) {
    const ULONGLONG until = GetTickCount64() + ms;
    while (GetTickCount64() < until) {
        if (g_plDone) return true;
        Sleep(10);
    }
    return false;
}
bool WaitPlReadyForTest(unsigned long ms) {
    const ULONGLONG until = GetTickCount64() + ms;
    while (GetTickCount64() < until) {
        if (g_plPhase != 0) return true;
        Sleep(5);
    }
    return false;
}

volatile uint32_t* HandoverSwitch() { return g_cfg.mode && g_cfg.handover ? &g_pjhSwitch : nullptr; }

bool WaitBootForTest(unsigned long ms) {
    const ULONGLONG until = GetTickCount64() + ms;
    while (GetTickCount64() < until) {
        if (g_bootDone) return true;
        Sleep(10);
    }
    return false;
}

const char* StatusLine(char* buf, size_t n) {
    if (!buf || !n) return "";
    buf[0] = 0;
    Stats s;
    GetStats(s);
    if (!s.mode) {
        if (s.pl)
            snprintf(buf, n, "PL %d: boot creates %llu on %u thread(s), library %s, loaded %llu, not in it %llu, stored %llu, early %llu, errors %llu",
                     s.pl, (unsigned long long)s.bootN, s.bootThreads,
                     s.plPhase == 1 ? "open" : s.plPhase == 2 ? "closed" : s.plPhase < 0 ? "off" : "opening", (unsigned long long)s.plLoaded,
                     (unsigned long long)s.plMiss, (unsigned long long)s.plStored, (unsigned long long)s.plEarly,
                     (unsigned long long)s.plErrors);
        return buf;
    }
    const char* st = s.mode == 2 ? "off (PJ = 2)" : s.prewarmState == 0 ? "waiting for the title" : s.prewarmState == 1 ? "running" : "done";
    snprintf(buf, n,
             "journal %u PSOs (+%u new), %u shaders, %u root sigs, %.1f MB; engine creates %llu (boot list %llu%s), not journaled "
             "%llu (rs %u, unsupported %u, fault %u) + dropped %llu | prewarm %s: %u/%u created, %u already, %u refused, %u missing, "
             "%.0f ms (max %.1f)",
             s.fileRecs, s.newRecs, s.shaders, s.rootSigs, (double)s.fileBytes / 1048576.0, (unsigned long long)s.seen,
             (unsigned long long)s.bootCreates, s.bootDone ? "" : ", still running", (unsigned long long)s.skipped, g_skipWhy[1],
             g_skipWhy[2], g_skipWhy[0] + g_skipWhy[3], (unsigned long long)s.dropped, st, s.pwOk, s.pwCandidates, s.pwAlready,
             s.pwFailed, s.pwMissing, s.pwMs, s.pwMaxMs);
    if (s.handover) {
        const size_t u = strlen(buf);
        if (u + 1 < n)
            snprintf(buf + u, n - u, " | PJH %s: handed %llu, runtime creates without one %llu, waiting %llu, not kept %llu",
                     g_pjhSwitch ? "on" : "off", (unsigned long long)s.handed, (unsigned long long)s.handMiss,
                     (unsigned long long)s.kept, (unsigned long long)s.keepRefused);
    }
    return buf;
}

void LogLine() {
    if (!g_cfg.mode || !g_log) return;  // PL logs its own boot lines (one at the gate)
    char b[900];
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
