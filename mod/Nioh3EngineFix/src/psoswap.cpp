// psoswap.cpp - shader bytecode substitution at PSO creation (see psoswap.h and notes\gpu-post-exe.md).
//
// Safety argument:
//   * A PSO is only changed when a replacement file exists for one of its shaders. Everything else is passed to the
//     original function with the caller's own arguments (the pointers are not even copied).
//   * Game memory (descs, streams, shader blobs) is only read inside __try blocks (SafeCopy / SafeReadBlob); a fault
//     makes that PSO pass through unchanged. No lock is held while game memory is read or files are written.
//   * A swapped PSO is created from a private copy of the desc / stream. If the runtime refuses it, the original desc is
//     used (the caller sees exactly the stock result) and the replacement key is disabled for the session.
//   * A graphics PSO is only swapped when all of its stages end up as the same container kind (all DXBC or all DXIL),
//     and every replacement must declare the same shader stage as the blob it replaces. A cached PSO blob in the desc
//     belongs to the original bytecode and is dropped from the swapped copy (the driver compiles it fresh).
//   * Nested hooks (a Streamline proxy device forwarding to the hooked real device, our own library-load fallback) pass
//     straight through: t_depth is raised around every call into the runtime.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "psoswap.h"

namespace psoswap {
namespace {

// ---- vtable slots (single-inheritance COM order; d3d12.h 10.0.22621) ------------------------------------------------
// ID3D12Object 0..6 (IUnknown 0..2), ID3D12Device 7..43, ID3D12Device1 44..46, ID3D12Device2 47.
constexpr int kSlotCreateGraphics = 10;          // ID3D12Device::CreateGraphicsPipelineState
constexpr int kSlotCreateCompute = 11;           // ID3D12Device::CreateComputePipelineState
constexpr int kSlotCreatePipelineLibrary = 44;   // ID3D12Device1::CreatePipelineLibrary
constexpr int kSlotCreatePipelineState = 47;     // ID3D12Device2::CreatePipelineState
// ID3D12PipelineLibrary: DeviceChild::GetDevice 7, StorePipeline 8, LoadGraphicsPipeline 9, LoadComputePipeline 10,
// GetSerializedSize 11, Serialize 12; ID3D12PipelineLibrary1::LoadPipeline 13.
constexpr int kSlotLoadGraphics = 9;
constexpr int kSlotLoadCompute = 10;
constexpr int kSlotLoadPipeline = 13;
constexpr int kMaxSets = 4;

const GUID kIID_Device = {0x189819f1, 0x1db6, 0x4b57, {0xbe, 0x54, 0x18, 0x21, 0x33, 0x9b, 0x85, 0xf7}};
const GUID kIID_Device1 = {0x77acce80, 0x638e, 0x4e65, {0x88, 0x95, 0xc1, 0xf2, 0x33, 0x86, 0x86, 0x3e}};
const GUID kIID_Device2 = {0x30baa41e, 0xb15b, 0x475c, {0xa0, 0xbb, 0x1a, 0xf5, 0xc5, 0xb6, 0x43, 0x28}};
const GUID kIID_Library = {0xc64226a8, 0x9201, 0x46af, {0xb4, 0xcc, 0x53, 0xfb, 0x9f, 0xf7, 0x41, 0x4f}};
const GUID kIID_Library1 = {0x80eabf42, 0x2568, 0x4e5e, {0xbd, 0x82, 0xc3, 0x7f, 0x86, 0x96, 0x1d, 0xc3}};
const GUID kIID_Pso = {0x765a30f3, 0xf624, 0x4c6f, {0xa8, 0x28, 0xac, 0xe9, 0x48, 0x62, 0x24, 0x45}};  // ID3D12PipelineState

using CreateGfxFn = HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*, const D3D12_GRAPHICS_PIPELINE_STATE_DESC*, REFIID, void**);
using CreateCsFn = HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*, const D3D12_COMPUTE_PIPELINE_STATE_DESC*, REFIID, void**);
using CreateStreamFn = HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*, const D3D12_PIPELINE_STATE_STREAM_DESC*, REFIID, void**);
using CreateLibFn = HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*, const void*, SIZE_T, REFIID, void**);
using LoadGfxFn = HRESULT(STDMETHODCALLTYPE*)(ID3D12PipelineLibrary*, LPCWSTR, const D3D12_GRAPHICS_PIPELINE_STATE_DESC*,
                                              REFIID, void**);
using LoadCsFn = HRESULT(STDMETHODCALLTYPE*)(ID3D12PipelineLibrary*, LPCWSTR, const D3D12_COMPUTE_PIPELINE_STATE_DESC*,
                                             REFIID, void**);
using LoadStreamFn = HRESULT(STDMETHODCALLTYPE*)(ID3D12PipelineLibrary*, LPCWSTR, const D3D12_PIPELINE_STATE_STREAM_DESC*,
                                                 REFIID, void**);
using CreateDeviceFn = HRESULT(WINAPI*)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);

// ---- configuration / state -----------------------------------------------------------------------------------------
struct Cfg {
    int enabled = 0;
    int dumpMode = 0;
    int logAll = 0;
    int track = 0;  // [Profiler] GPUProf >= 2: remember which shaders each PSO was built from (GP2, gpuprof.cpp)
    int hot = 0;    // [ShaderHot] Enabled: twin PSOs for the live shader A/B (diagnostic, notes\fast-testing.md)
    std::wstring folder, hotFolder;
};
Cfg g_cfg;
void (*g_log)(const char*, ...) = nullptr;
std::string g_wine;
volatile LONG g_state = (LONG)State::Off;
volatile LONG g_late = 0;
SRWLOCK g_lock = SRWLOCK_INIT;        // g_repl / g_seen; never held while game memory is read or files are written
SRWLOCK g_installLock = SRWLOCK_INIT;  // vtable patching
uintptr_t g_exeLo = 0, g_exeHi = 0;
thread_local int t_depth = 0;

struct Counters {
    volatile LONG64 graphics, compute, stream, libLoads, shaders, exeUnique, swappedPsos, swappedBlobs, rejected, skipped,
        dumped;
};
Counters g_c = {};
void Add(volatile LONG64& v, LONG64 n = 1) { InterlockedAdd64(&v, n); }

#define LOG(...)                                       \
    do {                                               \
        if (g_log) {                                   \
            char _b[600];                              \
            snprintf(_b, sizeof _b, __VA_ARGS__);      \
            g_log("PSOSWAP: %s", _b);                  \
        }                                              \
    } while (0)

// ---- shader keys ---------------------------------------------------------------------------------------------------------
struct Key {
    uint64_t a = 0, b = 0;  // container digest bytes 0..7 / 8..15 (little-endian loads), or FNV-1a-64 in a with fnv = 1
    uint32_t fnv = 0;
    bool operator==(const Key& o) const { return a == o.a && b == o.b && fnv == o.fnv; }
};
struct KeyHash {
    size_t operator()(const Key& k) const { return (size_t)(k.a ^ (k.b * 0x9E3779B97F4A7C15ull) ^ k.fnv); }
};

struct BlobInfo {
    Key key;
    int stage = -1;     // D3D12_SHADER_VERSION_TYPE: 0 ps, 1 vs, 2 gs, 3 hs, 4 ds, 5 cs, 13 ms, 14 as
    bool dxil = false;  // the program part is DXIL (SM6) rather than SHEX/SHDR (SM4/5)
    uint32_t size = 0;  // container size
    bool hasHash = false;  // DXIL: a HASH part exists (its digest is what RGP and dxc -dumpbin call the shader hash)
    Key hkey;              // HASH-part digest
};

void KeyName(const Key& k, char out[40]) {
    if (k.fnv) {
        snprintf(out, 40, "f%016llx", (unsigned long long)k.a);
        return;
    }
    uint8_t d[16];
    memcpy(d, &k.a, 8);
    memcpy(d + 8, &k.b, 8);
    for (int i = 0; i < 16; ++i) snprintf(out + 2 * i, 3, "%02x", d[i]);
}

bool ParseKeyName(const wchar_t* s, size_t n, Key& k) {
    auto hex = [](wchar_t c) -> int {
        if (c >= L'0' && c <= L'9') return c - L'0';
        if (c >= L'a' && c <= L'f') return c - L'a' + 10;
        if (c >= L'A' && c <= L'F') return c - L'A' + 10;
        return -1;
    };
    if (n == 32) {
        uint8_t d[16];
        for (int i = 0; i < 16; ++i) {
            const int h = hex(s[2 * i]), l = hex(s[2 * i + 1]);
            if (h < 0 || l < 0) return false;
            d[i] = (uint8_t)(h * 16 + l);
        }
        memcpy(&k.a, d, 8);
        memcpy(&k.b, d + 8, 8);
        k.fnv = 0;
        return true;
    }
    if (n == 17 && (s[0] == L'f' || s[0] == L'F')) {
        uint64_t v = 0;
        for (int i = 1; i < 17; ++i) {
            const int h = hex(s[i]);
            if (h < 0) return false;
            v = v * 16 + (uint64_t)h;
        }
        k.a = v;
        k.b = 0;
        k.fnv = 1;
        return true;
    }
    return false;
}

const char* StageName(int s) {
    switch (s) {
        case 0: return "ps";
        case 1: return "vs";
        case 2: return "gs";
        case 3: return "hs";
        case 4: return "ds";
        case 5: return "cs";
        case 13: return "ms";
        case 14: return "as";
        default: return "??";
    }
}

// Parses a DXBC container (plain memory access; callers wrap it in SafeReadBlob when the memory is the game's).
bool ParseBlob(const uint8_t* p, size_t len, BlobInfo& out) {
    if (!p || len < 32 || memcmp(p, "DXBC", 4) != 0) return false;
    uint32_t size, parts;
    memcpy(&size, p + 24, 4);
    memcpy(&parts, p + 28, 4);
    if (size < 32 || size > len || size > (64u << 20) || parts == 0 || parts > 64 || 32 + 4ull * parts > size) return false;
    int stage = -1;
    bool dxil = false;
    for (uint32_t i = 0; i < parts; ++i) {
        uint32_t off;
        memcpy(&off, p + 32 + 4 * i, 4);
        if (off > size - 12) return false;
        const uint8_t* part = p + off;
        if (!memcmp(part, "SHEX", 4) || !memcmp(part, "SHDR", 4) || !memcmp(part, "DXIL", 4)) {
            uint32_t ver;
            memcpy(&ver, part + 8, 4);
            stage = (int)(ver >> 16);
            dxil = !memcmp(part, "DXIL", 4);
        } else if (!memcmp(part, "HASH", 4) && off <= size - 28) {
            memcpy(&out.hkey.a, part + 12, 8);
            memcpy(&out.hkey.b, part + 20, 8);
            out.hkey.fnv = 0;
            out.hasHash = out.hkey.a || out.hkey.b;
        }
    }
    if (stage < 0) return false;
    if (!out.hasHash) out.hkey = Key{};
    out.stage = stage;
    out.dxil = dxil;
    out.size = size;
    memcpy(&out.key.a, p + 4, 8);
    memcpy(&out.key.b, p + 12, 8);
    out.key.fnv = 0;
    if (!out.key.a && !out.key.b) {  // unsigned container: hash the whole thing
        uint64_t h = 0xcbf29ce484222325ull;
        for (uint32_t i = 0; i < size; ++i) h = (h ^ p[i]) * 0x100000001b3ull;
        out.key.a = h;
        out.key.fnv = 1;
    }
    return true;
}

bool SafeReadBlob(const void* p, SIZE_T len, BlobInfo* out) {
    __try {
        return ParseBlob(static_cast<const uint8_t*>(p), (size_t)len, *out);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool SafeCopy(void* dst, const void* src, size_t n) {
    __try {
        memcpy(dst, src, n);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// ---- replacements ----------------------------------------------------------------------------------------------------------
struct Repl {
    Key key;  // the file's name: container digest, HASH-part digest, or f+FNV
    std::wstring path;
    std::vector<uint8_t> data;
    BlobInfo info;
    int state = 0;  // 0 not loaded, 1 ready, -1 unusable file, -2 rejected by the runtime this session
    int hotSet = -1;  // ShaderHot: set index (g_hotRepl entries only)
    int hotPol = 1;   // ShaderHot: 1 = on_ file (twin bound while the set is ON), 0 = off_ file (twin bound while OFF)
};
std::unordered_map<Key, Repl, KeyHash> g_repl;     // filled in Start, only state/data change afterwards (under g_lock)
std::unordered_map<Key, Repl, KeyHash> g_hotRepl;  // ShaderHot files (same rules as g_repl)
std::unordered_set<Key, KeyHash> g_seen;        // shaders already logged / dumped

bool ReadWholeFile(const std::wstring& path, std::vector<uint8_t>& out, size_t maxBytes) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz{};
    bool ok = GetFileSizeEx(h, &sz) && sz.QuadPart > 0 && (uint64_t)sz.QuadPart <= maxBytes;
    if (ok) {
        out.resize((size_t)sz.QuadPart);
        DWORD got = 0;
        ok = ReadFile(h, out.data(), (DWORD)out.size(), &got, nullptr) && got == out.size();
    }
    CloseHandle(h);
    return ok;
}

// Returns the ready replacement for this shader, or nullptr. Loads the file on first use (outside the lock).
const Repl* GetReplIn(std::unordered_map<Key, Repl, KeyHash>& map, const BlobInfo& orig) {
    AcquireSRWLockShared(&g_lock);
    auto it = map.find(orig.key);
    if (it == map.end() && orig.hasHash) it = map.find(orig.hkey);
    if (it == map.end()) {
        ReleaseSRWLockShared(&g_lock);
        return nullptr;
    }
    Repl* r = &it->second;  // node addresses are stable (no erase, no insert after Start)
    const int st = r->state;
    const std::wstring path = st == 0 ? r->path : std::wstring();
    ReleaseSRWLockShared(&g_lock);
    if (st == 1) return r;
    if (st < 0) {
        Add(g_c.skipped);
        return nullptr;
    }

    std::vector<uint8_t> data;
    BlobInfo bi;
    char kn[40];
    KeyName(r->key, kn);
    int ns = -1;
    if (!ReadWholeFile(path, data, 16u << 20)) {
        LOG("%s: cannot read %ls - not used", kn, path.c_str());
    } else if (!ParseBlob(data.data(), data.size(), bi) || bi.size != data.size()) {
        LOG("%s: %ls is not a valid DXBC/DXIL container (or its size field != file size) - not used", kn, path.c_str());
    } else if (bi.stage != orig.stage) {
        LOG("%s: replacement is a %s shader, the original a %s shader - not used", kn, StageName(bi.stage), StageName(orig.stage));
    } else {
        ns = 1;
    }
    AcquireSRWLockExclusive(&g_lock);
    if (r->state == 0) {
        r->state = ns;
        if (ns == 1) {
            r->data.swap(data);
            r->info = bi;
        }
    }
    const bool ready = r->state == 1;
    ReleaseSRWLockExclusive(&g_lock);
    if (ready && ns == 1) {
        char rk[40];
        KeyName(bi.key, rk);
        LOG("%s%s: replacement loaded (%s %s, %u bytes, new digest %s)", r->hotSet >= 0 ? "hot " : "", kn, StageName(bi.stage),
            bi.dxil ? "DXIL" : "DXBC", bi.size, rk);
    }
    if (!ready) Add(g_c.skipped);
    return ready ? r : nullptr;
}
const Repl* GetRepl(const BlobInfo& orig) { return GetReplIn(g_repl, orig); }

void DisableKeyIn(std::unordered_map<Key, Repl, KeyHash>& map, const Key& k, HRESULT hr) {
    AcquireSRWLockExclusive(&g_lock);
    auto it = map.find(k);
    const bool first = it != map.end() && it->second.state == 1;
    if (first) it->second.state = -2;
    ReleaseSRWLockExclusive(&g_lock);
    if (first) {
        char kn[40];
        KeyName(k, kn);
        LOG("%s%s: the runtime REJECTED the PSO with this replacement (hr %08lX) - %s; key disabled for this session",
            &map == &g_hotRepl ? "hot " : "", kn, (unsigned long)hr,
            &map == &g_hotRepl ? "no twin, the game's PSO stays bound in both states" : "created from the original desc");
    }
}
void DisableKey(const Key& k, HRESULT hr) { DisableKeyIn(g_repl, k, hr); }

struct Use {  // a replacement used in a plan (per-PSO plans below)
    Key key;
    int n = 0;
};

// ---- ShaderHot: sets, live switches, game PSO -> twin map --------------------------------------------------------------
constexpr int kMaxHotSets = 16;
constexpr uint32_t kHotTab = 1u << 13;
struct HotSetInfo {
    char name[32] = {};
    volatile LONG on = 0;
    int files = 0, onFiles = 0, offFiles = 0;
    volatile LONG twins = 0, fails = 0, logged = 0;
    volatile LONG64 altBinds = 0, baseBinds = 0;
};
HotSetInfo g_hotSets[kMaxHotSets];
int g_hotN = 0;
struct HotEnt {
    volatile uintptr_t p;    // the game's PSO (we hold one reference on it while it is mapped)
    volatile uintptr_t alt;  // its twin (our reference)
    volatile LONG set;
    volatile LONG pol;
};
HotEnt* g_hotTab = nullptr;  // [kHotTab] open addressing; alt/set/pol are written before p
SRWLOCK g_hotLock = SRWLOCK_INIT;  // writers only
volatile LONG g_hotUsed = 0;

struct HotPlanState {   // thread-local: a twin plan in progress (Consider looks up g_hotRepl, nothing is observed / tracked)
    bool on = false;
    int set = -1, conflicts = 0, pol = 1;
    Key key;
};
thread_local HotPlanState t_hp;
struct HotScope {
    HotScope() { t_hp = HotPlanState(); t_hp.on = true; }
    ~HotScope() { t_hp.on = false; }
};

const Repl* GetHot(const BlobInfo& bi) {
    const Repl* r = GetReplIn(g_hotRepl, bi);
    if (!r) return nullptr;
    if (t_hp.set < 0) {
        t_hp.set = r->hotSet;
        t_hp.pol = r->hotPol;
        t_hp.key = r->key;
    } else if (r->hotSet != t_hp.set || r->hotPol != t_hp.pol) {
        ++t_hp.conflicts;  // one twin per PSO: a second set (or the other polarity) in the same PSO is left stock
        return nullptr;
    }
    return r;
}

uint32_t HotPos(uintptr_t p) { return (uint32_t)(((p >> 4) * 0x9E3779B97F4A7C15ull) >> 40) & (kHotTab - 1); }

// base / alt: one reference each, both handed over (released here when they cannot be filed)
void HotRegister(IUnknown* base, IUnknown* alt, int set, int pol) {
    bool filed = false, dup = false;
    const uintptr_t p = (uintptr_t)base;
    AcquireSRWLockExclusive(&g_hotLock);
    uint32_t pos = HotPos(p);
    for (int probe = 0; probe < 64 && g_hotUsed < (LONG)(kHotTab / 2); ++probe, pos = (pos + 1) & (kHotTab - 1)) {
        HotEnt& e = g_hotTab[pos];
        if (e.p == p) {
            dup = true;
            break;
        }
        if (!e.p) {
            e.alt = (uintptr_t)alt;
            e.set = set;
            e.pol = pol;
            MemoryBarrier();
            e.p = p;
            InterlockedIncrement(&g_hotUsed);
            filed = true;
            break;
        }
    }
    ReleaseSRWLockExclusive(&g_hotLock);
    HotSetInfo& s = g_hotSets[set];
    if (!filed) {
        alt->Release();
        base->Release();
        if (!dup) {
            InterlockedIncrement(&s.fails);
            LOG("HOT: set '%s': twin map full - PSO %p stays stock in both states", s.name, (void*)base);
        }
        return;
    }
    const LONG n = InterlockedIncrement(&s.twins);
    if (InterlockedIncrement(&s.logged) <= 8) {
        char kn[40];
        KeyName(t_hp.key, kn);
        LOG("HOT: set '%s': twin %ld ready - game PSO %p (shader %s) <-> twin %p, bound while the set is %s%s", s.name, (long)n,
            (void*)base, kn, (void*)alt, pol ? "ON" : "OFF", t_hp.conflicts ? " (another hot set in this PSO left stock)" : "");
    }
}

// After the game's PSO was created (hr OK): twin from the hot plan. create(desc-ish) is done by the caller-specific lambda.
template <class CreateTwin>
void HotFinish(void** pp, CreateTwin create, const Use* uses, int nUses) {
    if (t_hp.set < 0) return;
    HotSetInfo& s = g_hotSets[t_hp.set];
    void* basePtr = nullptr;
    if (!pp || !SafeCopy(&basePtr, pp, sizeof basePtr) || !basePtr) return;
    void* alt = nullptr;
    HRESULT hr;
    {
        ++t_depth;
        hr = create(&alt);
        --t_depth;
    }
    if (FAILED(hr) || !alt) {
        InterlockedIncrement(&s.fails);
        for (int i = 0; i < nUses; ++i)
            if (uses[i].n) DisableKeyIn(g_hotRepl, uses[i].key, hr);
        return;
    }
    void* base = nullptr;   // the pointer the engine binds (ID3D12PipelineState), with our own reference
    if (FAILED(static_cast<IUnknown*>(basePtr)->QueryInterface(kIID_Pso, &base)) || !base) {
        static_cast<IUnknown*>(alt)->Release();
        InterlockedIncrement(&s.fails);
        return;
    }
    HotRegister(static_cast<IUnknown*>(base), static_cast<IUnknown*>(alt), t_hp.set, t_hp.pol);
}

// ---- observation: log exe-embedded shaders once, dump on request -----------------------------------------------------------
std::string ModuleOfAddr(const void* p) {
    HMODULE m = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            static_cast<LPCWSTR>(p), &m) ||
        !m)
        return "heap";
    char path[MAX_PATH] = {};
    GetModuleFileNameA(m, path, MAX_PATH);
    const char* s = strrchr(path, '\\');
    return s ? s + 1 : path;
}

void Dump(const BlobInfo& bi, const void* p) {
    std::vector<uint8_t> copy(bi.size);
    if (!SafeCopy(copy.data(), p, bi.size)) return;
    const std::wstring dir = g_cfg.folder + L"\\dump";
    CreateDirectoryW(g_cfg.folder.c_str(), nullptr);
    CreateDirectoryW(dir.c_str(), nullptr);
    char kn[40];
    KeyName(bi.key, kn);
    wchar_t name[64];
    swprintf(name, 64, L"\\%hs.%ls", kn, bi.dxil ? L"dxil" : L"dxbc");
    HANDLE h = CreateFileW((dir + name).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;  // already there (earlier session) or no access
    DWORD put = 0;
    const bool ok = WriteFile(h, copy.data(), (DWORD)copy.size(), &put, nullptr) && put == copy.size();
    CloseHandle(h);
    if (ok) Add(g_c.dumped);
}

void Observe(const BlobInfo& bi, const void* p) {
    Add(g_c.shaders);
    const uintptr_t a = (uintptr_t)p;
    const bool inExe = a >= g_exeLo && a < g_exeHi;
    if (!inExe && !g_cfg.logAll && g_cfg.dumpMode < 2) return;
    AcquireSRWLockShared(&g_lock);
    bool seen = g_seen.count(bi.key) != 0;
    ReleaseSRWLockShared(&g_lock);
    if (seen) return;
    AcquireSRWLockExclusive(&g_lock);
    seen = !g_seen.insert(bi.key).second;
    ReleaseSRWLockExclusive(&g_lock);
    if (seen) return;
    char kn[40];
    KeyName(bi.key, kn);
    char hn[48] = "";
    if (bi.hasHash) {
        memcpy(hn, " hash=", 6);
        KeyName(bi.hkey, hn + 6);
    }
    if (inExe) {
        Add(g_c.exeUnique);
        LOG("exe %s %s %u B rva=0x%llx %s%s", StageName(bi.stage), kn, bi.size, (unsigned long long)(a - g_exeLo),
            bi.dxil ? "DXIL" : "DXBC", hn);
    } else if (g_cfg.logAll) {
        LOG("ext %s %s %u B %s %s%s", StageName(bi.stage), kn, bi.size, ModuleOfAddr(p).c_str(), bi.dxil ? "DXIL" : "DXBC", hn);
    }
    if (g_cfg.dumpMode == 2 || (g_cfg.dumpMode == 1 && inExe)) Dump(bi, p);
}

// ---- GP2 PSO tracking: PSO pointer -> shader set (lock-free reads on the command-list hot path) -----------------------
constexpr int kTrkStages = 6;
constexpr uint32_t kMaxTrkSets = 32768, kPsoTab = 1u << 17;
struct SetRec {
    uint8_t n = 0;
    uint8_t st[kTrkStages] = {};
    Key k[kTrkStages];
};
SetRec* g_sets = nullptr;                 // [kMaxTrkSets], VirtualAlloc'ed on Start when tracking; records are immutable once published
volatile LONG g_setN = 1;                 // 0 = unknown PSO
std::unordered_map<uint64_t, uint32_t> g_setIdx;
SRWLOCK g_trkLock = SRWLOCK_INIT;
struct PsoEnt {
    volatile uintptr_t p;
    volatile uint32_t id;
};
PsoEnt* g_psoTab = nullptr;               // [kPsoTab] open addressing; id is written before p
struct Trk {
    int n = 0;
    uint8_t st[kTrkStages];
    Key k[kTrkStages];
};
thread_local Trk t_trk;

void TrkAdd(int stage, const Key& k) {
    Trk& t = t_trk;
    if (t.n < kTrkStages) {
        t.st[t.n] = (uint8_t)stage;
        t.k[t.n] = k;
        ++t.n;
    }
}
void TrkBegin() {
    if (g_cfg.track && !t_depth) t_trk.n = 0;
}
// after a PSO creation call returned (t_depth back to 0): file the PSO under its shader set
void TrkCommit(HRESULT hr, void** pp) {
    if (!g_cfg.track || t_depth) return;
    Trk& t = t_trk;
    const int n = t.n;
    t.n = 0;
    if (FAILED(hr) || !pp || !n || !g_sets || !g_psoTab) return;
    void* pso = nullptr;
    if (!SafeCopy(&pso, pp, sizeof pso) || !pso) return;
    uint64_t h = 0xCBF29CE484222325ull;
    auto mix = [&](uint64_t v) { h = (h ^ v) * 0x100000001B3ull; };
    for (int i = 0; i < n; ++i) {
        mix(t.st[i]);
        mix(t.k[i].a);
        mix(t.k[i].b);
        mix(t.k[i].fnv);
    }
    uint32_t id = 0;
    AcquireSRWLockExclusive(&g_trkLock);
    auto it = g_setIdx.find(h);
    if (it != g_setIdx.end()) {
        id = it->second;
    } else if ((uint32_t)g_setN < kMaxTrkSets) {
        id = (uint32_t)g_setN;
        SetRec& r = g_sets[id];
        r.n = (uint8_t)n;
        for (int i = 0; i < n; ++i) r.st[i] = t.st[i], r.k[i] = t.k[i];
        g_setIdx.emplace(h, id);
        InterlockedIncrement(&g_setN);
    }
    ReleaseSRWLockExclusive(&g_trkLock);
    if (!id) return;
    const uintptr_t p = (uintptr_t)pso;
    uint32_t pos = (uint32_t)(((p >> 4) * 0x9E3779B97F4A7C15ull) >> 40) & (kPsoTab - 1);
    for (int probe = 0; probe < 64; ++probe, pos = (pos + 1) & (kPsoTab - 1)) {
        PsoEnt& e = g_psoTab[pos];
        if (e.p == p || e.p == 0) {
            e.id = id;
            e.p = p;  // x64: stores are not reordered, so a reader that sees p sees this id
            return;
        }
    }
}

// ---- per-PSO plans -------------------------------------------------------------------------------------------------------
constexpr int kMaxStages = 8;
struct Kinds {
    int dxbc = 0, dxil = 0;
};

// Inspect one bytecode slot; returns the replacement to use (or nullptr) and records the resulting container kind.
const Repl* Consider(const D3D12_SHADER_BYTECODE& bc, Kinds& kinds, Use& use) {
    if (!bc.pShaderBytecode || bc.BytecodeLength < 32) return nullptr;
    BlobInfo bi;
    if (!SafeReadBlob(bc.pShaderBytecode, bc.BytecodeLength, &bi)) return nullptr;
    const Repl* r = nullptr;
    if (t_hp.on) {  // ShaderHot twin plan: hot files only, nothing observed / tracked a second time
        r = GetHot(bi);
    } else {
        if (g_cfg.enabled) Observe(bi, bc.pShaderBytecode);
        if (g_cfg.track) TrkAdd(bi.stage, bi.key);
        r = g_cfg.enabled ? GetRepl(bi) : nullptr;
    }
    const bool dx = r ? r->info.dxil : bi.dxil;
    (dx ? kinds.dxil : kinds.dxbc)++;
    if (r) {
        use.key = r->key;
        use.n = 1;
    }
    return r;
}

bool MixedKinds(const Kinds& k, const Use* uses, int n) {
    if (!(k.dxbc && k.dxil)) return false;
    for (int i = 0; i < n; ++i) {
        if (!uses[i].n) continue;
        char kn[40];
        KeyName(uses[i].key, kn);
        LOG("%s: not used in a PSO whose other stages are a different container kind (DXBC and DXIL cannot be mixed)", kn);
    }
    Add(g_c.skipped);
    return true;
}

struct GfxPlan {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc;
    Use uses[5];
    int swaps = 0;
};
bool PlanGraphics(const D3D12_GRAPHICS_PIPELINE_STATE_DESC* d, GfxPlan& pl) {
    if (!SafeCopy(&pl.desc, d, sizeof pl.desc)) return false;
    D3D12_SHADER_BYTECODE* slots[5] = {&pl.desc.VS, &pl.desc.PS, &pl.desc.DS, &pl.desc.HS, &pl.desc.GS};
    Kinds kinds;
    const Repl* r[5] = {};
    for (int i = 0; i < 5; ++i) {
        r[i] = Consider(*slots[i], kinds, pl.uses[i]);
        if (r[i]) ++pl.swaps;
    }
    if (!pl.swaps || MixedKinds(kinds, pl.uses, 5)) return false;
    for (int i = 0; i < 5; ++i)
        if (r[i]) *slots[i] = {r[i]->data.data(), r[i]->data.size()};
    pl.desc.CachedPSO = {};
    return true;
}

struct CsPlan {
    D3D12_COMPUTE_PIPELINE_STATE_DESC desc;
    Use use;
};
bool PlanCompute(const D3D12_COMPUTE_PIPELINE_STATE_DESC* d, CsPlan& pl) {
    if (!SafeCopy(&pl.desc, d, sizeof pl.desc)) return false;
    Kinds kinds;
    const Repl* r = Consider(pl.desc.CS, kinds, pl.use);
    if (!r) return false;
    pl.desc.CS = {r->data.data(), r->data.size()};
    pl.desc.CachedPSO = {};
    return true;
}

// Stream descriptors: subobjects are pointer-aligned { type; inner } records (CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT).
bool InnerLayout(uint32_t type, size_t& size, size_t& align) {
    switch (type) {
        case 0: size = sizeof(void*); align = alignof(void*); return true;                      // ROOT_SIGNATURE
        case 1: case 2: case 3: case 4: case 5: case 6: case 24: case 25:                       // VS PS DS HS GS CS AS MS
            size = sizeof(D3D12_SHADER_BYTECODE); align = alignof(D3D12_SHADER_BYTECODE); return true;
        case 7: size = sizeof(D3D12_STREAM_OUTPUT_DESC); align = alignof(D3D12_STREAM_OUTPUT_DESC); return true;
        case 8: size = sizeof(D3D12_BLEND_DESC); align = alignof(D3D12_BLEND_DESC); return true;
        case 9: case 13: case 14: case 16: case 18: case 20: size = 4; align = 4; return true;  // UINT / enums / format
        case 10: size = sizeof(D3D12_RASTERIZER_DESC); align = alignof(D3D12_RASTERIZER_DESC); return true;
        case 11: size = sizeof(D3D12_DEPTH_STENCIL_DESC); align = alignof(D3D12_DEPTH_STENCIL_DESC); return true;
        case 12: size = sizeof(D3D12_INPUT_LAYOUT_DESC); align = alignof(D3D12_INPUT_LAYOUT_DESC); return true;
        case 15: size = sizeof(D3D12_RT_FORMAT_ARRAY); align = alignof(D3D12_RT_FORMAT_ARRAY); return true;
        case 17: size = sizeof(DXGI_SAMPLE_DESC); align = alignof(DXGI_SAMPLE_DESC); return true;
        case 19: size = sizeof(D3D12_CACHED_PIPELINE_STATE); align = alignof(D3D12_CACHED_PIPELINE_STATE); return true;
        case 21: size = sizeof(D3D12_DEPTH_STENCIL_DESC1); align = alignof(D3D12_DEPTH_STENCIL_DESC1); return true;
        case 22: size = sizeof(D3D12_VIEW_INSTANCING_DESC); align = alignof(D3D12_VIEW_INSTANCING_DESC); return true;
        // Agility SDK types newer than these headers (sizes from the 1.613 d3d12.h)
        case 26: size = 56; align = 4; return true;                                             // DEPTH_STENCIL2
        case 27: size = 44; align = 4; return true;                                             // RASTERIZER1
        case 28: size = 40; align = 4; return true;                                             // RASTERIZER2
        case 29: size = 2 * sizeof(void*); align = alignof(void*); return true;                 // SERIALIZED_ROOT_SIGNATURE
        default: return false;
    }
}

struct StreamPlan {
    std::vector<uint64_t> buf;  // 8-byte aligned private copy of the stream
    D3D12_PIPELINE_STATE_STREAM_DESC desc{};
    Use uses[kMaxStages];
    int swaps = 0;
};
bool PlanStream(const D3D12_PIPELINE_STATE_STREAM_DESC* d, StreamPlan& pl) {
    D3D12_PIPELINE_STATE_STREAM_DESC sd{};
    if (!SafeCopy(&sd, d, sizeof sd) || !sd.pPipelineStateSubobjectStream || sd.SizeInBytes < 8 || sd.SizeInBytes > (1u << 20))
        return false;
    pl.buf.assign((sd.SizeInBytes + 7) / 8, 0);
    uint8_t* b = reinterpret_cast<uint8_t*>(pl.buf.data());
    if (!SafeCopy(b, sd.pPipelineStateSubobjectStream, sd.SizeInBytes)) return false;
    struct Hit {
        size_t off;
        const Repl* r;
    } hits[kMaxStages];
    int nh = 0, nUse = 0;
    Kinds kinds;
    size_t cachedOff = SIZE_MAX;
    size_t pos = 0;
    while (pos + 4 <= sd.SizeInBytes) {
        uint32_t type;
        memcpy(&type, b + pos, 4);
        size_t isz, ial;
        if (!InnerLayout(type, isz, ial)) return false;  // unknown subobject: leave this PSO alone
        const size_t inner = (pos + 4 + ial - 1) & ~(ial - 1);
        const size_t next = (inner + isz + sizeof(void*) - 1) & ~(sizeof(void*) - 1);
        if (inner + isz > sd.SizeInBytes) return false;
        if (type == 19) cachedOff = inner;
        const bool shader = (type >= 1 && type <= 6) || type == 24 || type == 25;
        if (shader && nUse < kMaxStages) {
            D3D12_SHADER_BYTECODE bc;
            memcpy(&bc, b + inner, sizeof bc);
            const Repl* r = Consider(bc, kinds, pl.uses[nUse]);
            ++nUse;
            if (r && nh < kMaxStages) hits[nh++] = {inner, r};
        }
        pos = next;
    }
    if (!nh || MixedKinds(kinds, pl.uses, nUse)) return false;
    for (int i = 0; i < nh; ++i) {
        const D3D12_SHADER_BYTECODE bc = {hits[i].r->data.data(), hits[i].r->data.size()};
        memcpy(b + hits[i].off, &bc, sizeof bc);
    }
    if (cachedOff != SIZE_MAX) memset(b + cachedOff, 0, sizeof(D3D12_CACHED_PIPELINE_STATE));
    pl.swaps = nh;
    pl.desc.SizeInBytes = sd.SizeInBytes;
    pl.desc.pPipelineStateSubobjectStream = b;
    return true;
}

template <class PlanT>
void Rejected(const PlanT& pl, int n, HRESULT hr) {
    Add(g_c.rejected);
    for (int i = 0; i < n; ++i)
        if (pl.uses[i].n) DisableKey(pl.uses[i].key, hr);
}
void Accepted(int blobs) {
    Add(g_c.swappedPsos);
    Add(g_c.swappedBlobs, blobs);
}

// ---- device hooks --------------------------------------------------------------------------------------------------------
struct DevSet {
    void** vt = nullptr;
    CreateGfxFn gfx = nullptr;
    CreateCsFn cs = nullptr;
    CreateLibFn lib = nullptr;
    CreateStreamFn stream = nullptr;
};
DevSet g_dev[kMaxSets];
volatile LONG g_devN = 0;

struct LibSet {
    void** vt = nullptr;
    LoadGfxFn gfx = nullptr;
    LoadCsFn cs = nullptr;
    LoadStreamFn stream = nullptr;
};
LibSet g_lib[kMaxSets];
volatile LONG g_libN = 0;

struct Depth {  // raised around every call into the runtime: nested hooks pass through
    Depth() { ++t_depth; }
    ~Depth() { --t_depth; }
};

HRESULT DoGfx0(CreateGfxFn orig, ID3D12Device* self, const D3D12_GRAPHICS_PIPELINE_STATE_DESC* d, REFIID riid, void** pp) {
    if (t_depth || !d) return orig(self, d, riid, pp);
    Add(g_c.graphics);
    GfxPlan pl;
    if (!PlanGraphics(d, pl)) {
        Depth g;
        return orig(self, d, riid, pp);
    }
    Depth g;
    HRESULT hr = orig(self, &pl.desc, riid, pp);
    if (SUCCEEDED(hr)) {
        Accepted(pl.swaps);
        return hr;
    }
    const HRESULT o = orig(self, d, riid, pp);
    if (SUCCEEDED(o)) Rejected(pl, 5, hr);
    return o;
}
HRESULT DoCs0(CreateCsFn orig, ID3D12Device* self, const D3D12_COMPUTE_PIPELINE_STATE_DESC* d, REFIID riid, void** pp) {
    if (t_depth || !d) return orig(self, d, riid, pp);
    Add(g_c.compute);
    CsPlan pl;
    if (!PlanCompute(d, pl)) {
        Depth g;
        return orig(self, d, riid, pp);
    }
    Depth g;
    HRESULT hr = orig(self, &pl.desc, riid, pp);
    if (SUCCEEDED(hr)) {
        Accepted(1);
        return hr;
    }
    const HRESULT o = orig(self, d, riid, pp);
    if (SUCCEEDED(o)) {
        Add(g_c.rejected);
        DisableKey(pl.use.key, hr);
    }
    return o;
}
HRESULT DoStream0(CreateStreamFn orig, ID3D12Device* self, const D3D12_PIPELINE_STATE_STREAM_DESC* d, REFIID riid, void** pp) {
    if (t_depth || !d) return orig(self, d, riid, pp);
    Add(g_c.stream);
    StreamPlan pl;
    bool planned = false;
    try {
        planned = PlanStream(d, pl);
    } catch (...) {
        planned = false;
    }
    Depth g;
    if (!planned) return orig(self, d, riid, pp);
    HRESULT hr = orig(self, &pl.desc, riid, pp);
    if (SUCCEEDED(hr)) {
        Accepted(pl.swaps);
        return hr;
    }
    const HRESULT o = orig(self, d, riid, pp);
    if (SUCCEEDED(o)) Rejected(pl, kMaxStages, hr);
    return o;
}

// ---- ShaderHot twins: built from the game's own desc (hot bytecode, cached blob dropped) straight on the runtime's
// original method, so no other hook (PJ journal, Streamline) ever sees a twin -------------------------------------------
void HotTwinGfx(CreateGfxFn create, ID3D12Device* dev, const D3D12_GRAPHICS_PIPELINE_STATE_DESC* d, void** pp) {
    HotScope hs;
    GfxPlan pl;
    if (!create || !PlanGraphics(d, pl)) return;
    HotFinish(pp, [&](void** alt) { return create(dev, &pl.desc, kIID_Pso, alt); }, pl.uses, 5);
}
void HotTwinCs(CreateCsFn create, ID3D12Device* dev, const D3D12_COMPUTE_PIPELINE_STATE_DESC* d, void** pp) {
    HotScope hs;
    CsPlan pl;
    if (!create || !PlanCompute(d, pl)) return;
    HotFinish(pp, [&](void** alt) { return create(dev, &pl.desc, kIID_Pso, alt); }, &pl.use, 1);
}
void HotTwinStream(CreateStreamFn create, ID3D12Device* dev, const D3D12_PIPELINE_STATE_STREAM_DESC* d, void** pp) {
    HotScope hs;
    StreamPlan pl;
    bool planned = false;
    try {
        planned = create && PlanStream(d, pl);
    } catch (...) {
        planned = false;
    }
    if (!planned) return;
    HotFinish(pp, [&](void** alt) { return create(dev, &pl.desc, kIID_Pso, alt); }, pl.uses, kMaxStages);
}

HRESULT DoGfx(CreateGfxFn orig, ID3D12Device* self, const D3D12_GRAPHICS_PIPELINE_STATE_DESC* d, REFIID riid, void** pp) {
    const bool top = !t_depth;
    const HRESULT hr = DoGfx0(orig, self, d, riid, pp);
    if (top && g_hotN && d && SUCCEEDED(hr)) HotTwinGfx(orig, self, d, pp);
    return hr;
}
HRESULT DoCs(CreateCsFn orig, ID3D12Device* self, const D3D12_COMPUTE_PIPELINE_STATE_DESC* d, REFIID riid, void** pp) {
    const bool top = !t_depth;
    const HRESULT hr = DoCs0(orig, self, d, riid, pp);
    if (top && g_hotN && d && SUCCEEDED(hr)) HotTwinCs(orig, self, d, pp);
    return hr;
}
HRESULT DoStream(CreateStreamFn orig, ID3D12Device* self, const D3D12_PIPELINE_STATE_STREAM_DESC* d, REFIID riid, void** pp) {
    const bool top = !t_depth;
    const HRESULT hr = DoStream0(orig, self, d, riid, pp);
    if (top && g_hotN && d && SUCCEEDED(hr)) HotTwinStream(orig, self, d, pp);
    return hr;
}

// the runtime's original create method for this device object (the hooked vtable's saved slot), nullptr if unknown
template <class Fn>
Fn DevOrig(void* dev, Fn DevSet::*m) {
    if (!dev) return nullptr;
    void** vt = *static_cast<void***>(dev);
    for (LONG i = 0; i < g_devN; ++i)
        if (g_dev[i].vt == vt) return g_dev[i].*m;
    return nullptr;
}
// pipeline-library loads: the twin is created on the library's device
template <class DescT, class Twin>
void HotTwinLib(ID3D12PipelineLibrary* lib, const DescT* d, void** pp, const GUID& devIid, Twin twin) {
    void* dev = nullptr;
    if (FAILED(lib->GetDevice(devIid, &dev)) || !dev) return;
    twin(dev, d, pp);
    static_cast<IUnknown*>(dev)->Release();
}

void HookLibraryObject(IUnknown* lib);

template <int S>
HRESULT STDMETHODCALLTYPE HkGfx(ID3D12Device* self, const D3D12_GRAPHICS_PIPELINE_STATE_DESC* d, REFIID riid, void** pp) {
    TrkBegin();
    const HRESULT hr = DoGfx(g_dev[S].gfx, self, d, riid, pp);
    TrkCommit(hr, pp);
    return hr;
}
template <int S>
HRESULT STDMETHODCALLTYPE HkCs(ID3D12Device* self, const D3D12_COMPUTE_PIPELINE_STATE_DESC* d, REFIID riid, void** pp) {
    TrkBegin();
    const HRESULT hr = DoCs(g_dev[S].cs, self, d, riid, pp);
    TrkCommit(hr, pp);
    return hr;
}
template <int S>
HRESULT STDMETHODCALLTYPE HkStream(ID3D12Device* self, const D3D12_PIPELINE_STATE_STREAM_DESC* d, REFIID riid, void** pp) {
    TrkBegin();
    const HRESULT hr = DoStream(g_dev[S].stream, self, d, riid, pp);
    TrkCommit(hr, pp);
    return hr;
}
template <int S>
HRESULT STDMETHODCALLTYPE HkLib(ID3D12Device* self, const void* blob, SIZE_T len, REFIID riid, void** pp) {
    HRESULT hr;
    {
        Depth g;
        hr = g_dev[S].lib(self, blob, len, riid, pp);
    }
    if (SUCCEEDED(hr) && pp && *pp && !t_depth) HookLibraryObject(static_cast<IUnknown*>(*pp));
    return hr;
}

// ---- pipeline-library hooks: a swapped load is created directly on the library's device ---------------------------------
template <class PlanT, class DescT, class CreateCall>
HRESULT LibLoad(PlanT& pl, bool planned, ID3D12PipelineLibrary* self, CreateCall create, int nUses, void** pp, bool& done) {
    done = false;
    if (!planned || !pp) return E_FAIL;
    ID3D12Device* dev = nullptr;
    if (FAILED(self->GetDevice(kIID_Device, reinterpret_cast<void**>(&dev))) || !dev) return E_FAIL;
    HRESULT hr;
    {
        Depth g;
        hr = create(dev);
    }
    dev->Release();
    if (SUCCEEDED(hr)) {
        done = true;
        Accepted(pl.swaps);
        return hr;
    }
    Rejected(pl, nUses, hr);  // the library path below is the stock behaviour
    return hr;
}

template <int S>
HRESULT HkLoadGfxI(ID3D12PipelineLibrary* self, LPCWSTR name, const D3D12_GRAPHICS_PIPELINE_STATE_DESC* d,
                                    REFIID riid, void** pp) {
    if (t_depth || !d) return g_lib[S].gfx(self, name, d, riid, pp);
    Add(g_c.libLoads);
    GfxPlan pl;
    const bool planned = PlanGraphics(d, pl);
    bool done;
    HRESULT hr = LibLoad<GfxPlan, D3D12_GRAPHICS_PIPELINE_STATE_DESC>(
        pl, planned, self, [&](ID3D12Device* dev) { return dev->CreateGraphicsPipelineState(&pl.desc, riid, pp); }, 5, pp, done);
    if (done) return hr;
    Depth g;
    return g_lib[S].gfx(self, name, d, riid, pp);
}
template <int S>
HRESULT HkLoadCsI(ID3D12PipelineLibrary* self, LPCWSTR name, const D3D12_COMPUTE_PIPELINE_STATE_DESC* d,
                                   REFIID riid, void** pp) {
    if (t_depth || !d) return g_lib[S].cs(self, name, d, riid, pp);
    Add(g_c.libLoads);
    struct P {
        CsPlan c;
        Use uses[1];
        int swaps = 1;
    } pl;
    const bool planned = PlanCompute(d, pl.c);
    pl.uses[0] = pl.c.use;
    bool done;
    HRESULT hr = LibLoad<P, D3D12_COMPUTE_PIPELINE_STATE_DESC>(
        pl, planned, self, [&](ID3D12Device* dev) { return dev->CreateComputePipelineState(&pl.c.desc, riid, pp); }, 1, pp, done);
    if (done) return hr;
    Depth g;
    return g_lib[S].cs(self, name, d, riid, pp);
}
template <int S>
HRESULT HkLoadStreamI(ID3D12PipelineLibrary* self, LPCWSTR name, const D3D12_PIPELINE_STATE_STREAM_DESC* d,
                                       REFIID riid, void** pp) {
    if (t_depth || !d) return g_lib[S].stream(self, name, d, riid, pp);
    Add(g_c.libLoads);
    StreamPlan pl;
    bool planned = false;
    try {
        planned = PlanStream(d, pl);
    } catch (...) {
        planned = false;
    }
    bool done = false;
    HRESULT hr = E_FAIL;
    if (planned) {
        ID3D12Device* dev = nullptr;
        ID3D12Device2* dev2 = nullptr;
        if (SUCCEEDED(self->GetDevice(kIID_Device, reinterpret_cast<void**>(&dev))) && dev) {
            if (SUCCEEDED(dev->QueryInterface(kIID_Device2, reinterpret_cast<void**>(&dev2))) && dev2) {
                {
                    Depth g;
                    hr = dev2->CreatePipelineState(&pl.desc, riid, pp);
                }
                dev2->Release();
                if (SUCCEEDED(hr)) {
                    done = true;
                    Accepted(pl.swaps);
                } else {
                    Rejected(pl, kMaxStages, hr);
                }
            }
            dev->Release();
        }
    }
    if (done) return hr;
    Depth g;
    return g_lib[S].stream(self, name, d, riid, pp);
}

// GP2: the library hooks above are plain functions; these wrappers file the resulting PSO under its shader set
// (+ ShaderHot: a twin for every loaded PSO built from a hot shader, created on the library's device)
void HotLibGfx(void* dev, const D3D12_GRAPHICS_PIPELINE_STATE_DESC* d, void** pp) {
    HotTwinGfx(DevOrig(dev, &DevSet::gfx), static_cast<ID3D12Device*>(dev), d, pp);
}
void HotLibCs(void* dev, const D3D12_COMPUTE_PIPELINE_STATE_DESC* d, void** pp) {
    HotTwinCs(DevOrig(dev, &DevSet::cs), static_cast<ID3D12Device*>(dev), d, pp);
}
void HotLibStream(void* dev, const D3D12_PIPELINE_STATE_STREAM_DESC* d, void** pp) {
    HotTwinStream(DevOrig(dev, &DevSet::stream), static_cast<ID3D12Device*>(dev), d, pp);
}
#define PSOSWAP_LIB_WRAP(NAME, DESC, HOT, DEVIID)                                                                      \
    template <int S>                                                                                                   \
    HRESULT STDMETHODCALLTYPE NAME(ID3D12PipelineLibrary* self, LPCWSTR name, const DESC* d, REFIID riid, void** pp) { \
        const bool top = !t_depth;                                                                                     \
        TrkBegin();                                                                                                    \
        const HRESULT hr = NAME##I<S>(self, name, d, riid, pp);                                                        \
        TrkCommit(hr, pp);                                                                                             \
        if (top && g_hotN && d && SUCCEEDED(hr)) HotTwinLib(self, d, pp, DEVIID, HOT);                                 \
        return hr;                                                                                                     \
    }
PSOSWAP_LIB_WRAP(HkLoadGfx, D3D12_GRAPHICS_PIPELINE_STATE_DESC, HotLibGfx, kIID_Device)
PSOSWAP_LIB_WRAP(HkLoadCs, D3D12_COMPUTE_PIPELINE_STATE_DESC, HotLibCs, kIID_Device)
PSOSWAP_LIB_WRAP(HkLoadStream, D3D12_PIPELINE_STATE_STREAM_DESC, HotLibStream, kIID_Device2)

// ---- vtable patching -------------------------------------------------------------------------------------------------------
std::string ModuleOfVt(void* p) { return ModuleOfAddr(p); }

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
void PatchDevSet(void** vt, bool has1, bool has2) {
    DevSet& s = g_dev[S];
    s.vt = vt;
    Patch(&vt[kSlotCreateGraphics], (void*)&HkGfx<S>, (void**)&s.gfx);
    Patch(&vt[kSlotCreateCompute], (void*)&HkCs<S>, (void**)&s.cs);
    if (has1) Patch(&vt[kSlotCreatePipelineLibrary], (void*)&HkLib<S>, (void**)&s.lib);
    if (has2) Patch(&vt[kSlotCreatePipelineState], (void*)&HkStream<S>, (void**)&s.stream);
}
template <int S>
void PatchLibSet(void** vt, bool has1) {
    LibSet& s = g_lib[S];
    s.vt = vt;
    Patch(&vt[kSlotLoadGraphics], (void*)&HkLoadGfx<S>, (void**)&s.gfx);
    Patch(&vt[kSlotLoadCompute], (void*)&HkLoadCs<S>, (void**)&s.cs);
    if (has1) Patch(&vt[kSlotLoadPipeline], (void*)&HkLoadStream<S>, (void**)&s.stream);
}

bool SameVt(IUnknown* o, const GUID& iid, void** vt, bool& sameOut) {
    void* q = nullptr;
    sameOut = false;
    if (FAILED(o->QueryInterface(iid, &q)) || !q) return false;
    sameOut = *static_cast<void***>(q) == vt;
    static_cast<IUnknown*>(q)->Release();
    return true;
}

// One vtable per device set. A device class that answers Device2 through a different vtable (not seen in practice) gets a
// second set for that vtable.
void HookDeviceVt(IUnknown* obj, const char* via) {
    void** vt = *reinterpret_cast<void***>(obj);
    AcquireSRWLockExclusive(&g_installLock);
    bool known = false;
    for (LONG i = 0; i < g_devN; ++i) known |= g_dev[i].vt == vt;
    if (!known && g_devN < kMaxSets) {
        bool same1 = false, same2 = false;
        const bool has1 = SameVt(obj, kIID_Device1, vt, same1) && same1;
        const bool has2 = SameVt(obj, kIID_Device2, vt, same2) && same2;
        switch (g_devN) {
            case 0: PatchDevSet<0>(vt, has1, has2); break;
            case 1: PatchDevSet<1>(vt, has1, has2); break;
            case 2: PatchDevSet<2>(vt, has1, has2); break;
            default: PatchDevSet<3>(vt, has1, has2); break;
        }
        InterlockedIncrement(&g_devN);
        InterlockedExchange(&g_state, (LONG)State::Active);
        LOG("hooked device vtable %p (%s) via %s: CreateGraphics/ComputePipelineState%s%s", (void*)vt, ModuleOfVt(vt).c_str(),
            via, has1 ? ", CreatePipelineLibrary" : "", has2 ? ", CreatePipelineState (stream)" : "");
    }
    ReleaseSRWLockExclusive(&g_installLock);
}

void HookDeviceObject(IUnknown* obj, const char* via) {
    void* dev = nullptr;
    if (FAILED(obj->QueryInterface(kIID_Device, &dev)) || !dev) return;
    IUnknown* d = static_cast<IUnknown*>(dev);
    HookDeviceVt(d, via);
    void* d2 = nullptr;
    if (SUCCEEDED(d->QueryInterface(kIID_Device2, &d2)) && d2) {
        if (*static_cast<void***>(d2) != *reinterpret_cast<void***>(d)) HookDeviceVt(static_cast<IUnknown*>(d2), via);
        static_cast<IUnknown*>(d2)->Release();
    }
    d->Release();
}

bool HookDeviceSafe(IUnknown* obj, const char* via) {
    __try {
        HookDeviceObject(obj, via);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        LOG("exception 0x%08lX while hooking a device - this device is not hooked", GetExceptionCode());
        return false;
    }
}

void HookLibraryImpl(IUnknown* lib) {
    void** vt = *reinterpret_cast<void***>(lib);
    AcquireSRWLockExclusive(&g_installLock);
    bool known = false;
    for (LONG i = 0; i < g_libN; ++i) known |= g_lib[i].vt == vt;
    if (!known && g_libN < kMaxSets) {
        bool same1 = false;
        const bool has1 = SameVt(lib, kIID_Library1, vt, same1) && same1;
        switch (g_libN) {
            case 0: PatchLibSet<0>(vt, has1); break;
            case 1: PatchLibSet<1>(vt, has1); break;
            case 2: PatchLibSet<2>(vt, has1); break;
            default: PatchLibSet<3>(vt, has1); break;
        }
        InterlockedIncrement(&g_libN);
        LOG("hooked pipeline-library vtable %p (%s): LoadGraphicsPipeline, LoadComputePipeline%s", (void*)vt,
            ModuleOfVt(vt).c_str(), has1 ? ", LoadPipeline" : "");
    }
    ReleaseSRWLockExclusive(&g_installLock);
}
void HookLibraryObject(IUnknown* lib) {
    __try {
        HookLibraryImpl(lib);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        LOG("exception 0x%08lX while hooking a pipeline library - library loads pass through", GetExceptionCode());
    }
}

// ---- D3D12CreateDevice import hook ----------------------------------------------------------------------------------------
CreateDeviceFn g_origCreate[2] = {};
void** g_iatSlot[2] = {};
const char* g_iatDll[2] = {};

void (*g_onDevice)(void*) = nullptr;  // PJ observer (psojournal.cpp); it guards itself with __try
bool g_observeOnly = false;

template <int I>
HRESULT WINAPI HkCreateDevice(IUnknown* adapter, D3D_FEATURE_LEVEL fl, REFIID riid, void** pp) {
    const HRESULT hr = g_origCreate[I](adapter, fl, riid, pp);
    if (SUCCEEDED(hr) && pp && *pp) {
        if (!g_observeOnly) HookDeviceSafe(static_cast<IUnknown*>(*pp), g_iatDll[I]);
        if (g_onDevice) g_onDevice(*pp);
    }
    return hr;
}

bool ImageRange(HMODULE m, uintptr_t& lo, uintptr_t& hi) {
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(m);
    if (!m || dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(reinterpret_cast<uint8_t*>(m) + dos->e_lfanew);
    lo = reinterpret_cast<uintptr_t>(m);
    hi = lo + nt->OptionalHeader.SizeOfImage;
    return true;
}

int ArmImportHooks() {
    auto* base = reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    const IMAGE_DATA_DIRECTORY& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress) return 0;
    int n = 0;
    for (auto* imp = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); imp->Name && n < 2; ++imp) {
        const char* dll = reinterpret_cast<const char*>(base + imp->Name);
        const bool sl = !_stricmp(dll, "sl.interposer.dll"), d12 = !_stricmp(dll, "d3d12.dll");
        if (!sl && !d12) continue;
        auto* names = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + (imp->OriginalFirstThunk ? imp->OriginalFirstThunk : imp->FirstThunk));
        auto* slots = reinterpret_cast<void**>(base + imp->FirstThunk);
        // by name, or by ordinal (d3d12.lib links D3D12CreateDevice as ordinal 101): match the bound slot value then
        HMODULE dm = GetModuleHandleA(dll);
        void* bound = dm ? (void*)GetProcAddress(dm, "D3D12CreateDevice") : nullptr;
        for (int i = 0; names[i].u1.AddressOfData; ++i) {
            if (IMAGE_SNAP_BY_ORDINAL64(names[i].u1.Ordinal)) {
                if (!bound || slots[i] != bound) continue;
            } else {
                auto* ibn = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + names[i].u1.AddressOfData);
                if (strcmp(reinterpret_cast<const char*>(ibn->Name), "D3D12CreateDevice") != 0) continue;
            }
            if (!slots[i]) break;
            g_iatDll[n] = sl ? "sl.interposer.dll!D3D12CreateDevice" : "d3d12.dll!D3D12CreateDevice";
            g_iatSlot[n] = &slots[i];
            const bool ok = n == 0 ? Patch(&slots[i], (void*)&HkCreateDevice<0>, (void**)&g_origCreate[0])
                                   : Patch(&slots[i], (void*)&HkCreateDevice<1>, (void**)&g_origCreate[1]);
            if (ok) {
                LOG("IAT hook on %s armed (slot RVA 0x%llx, target %s)", g_iatDll[n],
                    (unsigned long long)(reinterpret_cast<uint8_t*>(&slots[i]) - base), ModuleOfAddr((void*)g_origCreate[n]).c_str());
                ++n;
            }
            break;
        }
    }
    return n;
}
int ArmImportHooksSafe() {
    __try {
        return ArmImportHooks();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

int IndexFolder() {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((g_cfg.folder + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    int n = 0;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        const wchar_t* dot = wcsrchr(fd.cFileName, L'.');
        if (!dot) continue;
        if (_wcsicmp(dot, L".dxbc") && _wcsicmp(dot, L".dxil") && _wcsicmp(dot, L".cso") && _wcsicmp(dot, L".bin")) continue;
        Key k;
        if (!ParseKeyName(fd.cFileName, (size_t)(dot - fd.cFileName), k)) continue;
        Repl r;
        r.key = k;
        r.path = g_cfg.folder + L"\\" + fd.cFileName;
        auto ins = g_repl.emplace(k, std::move(r));
        if (!ins.second) {
            LOG("%ls: a replacement for the same key is already indexed (%ls) - ignored", fd.cFileName,
                ins.first->second.path.c_str());
            continue;
        }
        ++n;
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return n;
}

int IniInt(const wchar_t* key, int def, int lo, int hi, const std::wstring& ini) {
    const int v = (int)GetPrivateProfileIntW(L"ShaderSwap", key, def, ini.c_str());
    return v < lo ? lo : v > hi ? hi : v;
}

std::wstring ResolveFolder(const std::wstring& ini, const wchar_t* sec, const wchar_t* key, const wchar_t* def) {
    wchar_t folder[MAX_PATH] = {};
    GetPrivateProfileStringW(sec, key, def, folder, MAX_PATH, ini.c_str());
    std::wstring f = folder;
    const bool absolute = f.size() > 1 && (f[1] == L':' || (f[0] == L'\\' && f[1] == L'\\'));
    if (!absolute) {
        const size_t sep = ini.find_last_of(L"\\/");
        f = (sep == std::wstring::npos ? std::wstring(L".") : ini.substr(0, sep)) + L"\\" + f;
    }
    while (!f.empty() && (f.back() == L'\\' || f.back() == L'/')) f.pop_back();
    return f;
}

bool ShaderExt(const wchar_t* dot) {
    return dot && (!_wcsicmp(dot, L".dxbc") || !_wcsicmp(dot, L".dxil") || !_wcsicmp(dot, L".cso") || !_wcsicmp(dot, L".bin"));
}

// ShaderHot: one sub-folder per set; files on_<key>.<ext> / off_<key>.<ext> / <key>.<ext>
int IndexHot() {
    WIN32_FIND_DATAW sd;
    HANDLE hs = FindFirstFileW((g_cfg.hotFolder + L"\\*").c_str(), &sd);
    if (hs == INVALID_HANDLE_VALUE) return 0;
    do {
        if (!(sd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || sd.cFileName[0] == L'.') continue;
        if (g_hotN >= kMaxHotSets) {
            LOG("HOT: more than %d sets in %ls - '%ls' ignored", kMaxHotSets, g_cfg.hotFolder.c_str(), sd.cFileName);
            continue;
        }
        HotSetInfo& s = g_hotSets[g_hotN];
        int k = 0;
        for (const wchar_t* c = sd.cFileName; *c && k < 31; ++c) s.name[k++] = (char)((*c >= L'A' && *c <= L'Z') ? *c - L'A' + L'a' : (*c < 127 ? *c : L'?'));
        s.name[k] = 0;
        const std::wstring dir = g_cfg.hotFolder + L"\\" + sd.cFileName;
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                const wchar_t* dot = wcsrchr(fd.cFileName, L'.');
                if (!ShaderExt(dot)) continue;
                const wchar_t* nm = fd.cFileName;
                int pol = 1;
                if (!_wcsnicmp(nm, L"on_", 3)) nm += 3;
                else if (!_wcsnicmp(nm, L"off_", 4)) nm += 4, pol = 0;
                Key key;
                if (!ParseKeyName(nm, (size_t)(dot - nm), key)) continue;
                Repl r;
                r.key = key;
                r.path = dir + L"\\" + fd.cFileName;
                r.hotSet = g_hotN;
                r.hotPol = pol;
                auto ins = g_hotRepl.emplace(key, std::move(r));
                if (!ins.second) {
                    LOG("HOT: %ls: the same shader key is already in set '%s' (%ls) - ignored", fd.cFileName,
                        g_hotSets[ins.first->second.hotSet].name, ins.first->second.path.c_str());
                    continue;
                }
                ++s.files;
                ++(pol ? s.onFiles : s.offFiles);
            } while (FindNextFileW(h, &fd));
            FindClose(h);
        }
        if (!s.files) {
            LOG("HOT: '%ls' holds no on_/off_ shader files - not a set", sd.cFileName);
            memset(&s, 0, sizeof s);
            continue;
        }
        s.on = (s.offFiles && !s.onFiles) ? 1 : 0;   // the state of what is installed: binding unchanged until HotSet
        LOG("HOT: set '%s': %d file(s) (%d on_, %d off_), starts %s%s", s.name, s.files, s.onFiles, s.offFiles, s.on ? "ON" : "OFF",
            (s.onFiles && s.offFiles) ? " - WARNING: mixed on_/off_ files, each twin follows its own file's polarity" : "");
        ++g_hotN;
    } while (FindNextFileW(hs, &sd));
    FindClose(hs);
    return g_hotN;
}

int FindHotSet(const char* name) {
    if (!name) return -1;
    char b[32];
    int k = 0;
    for (; name[k] && k < 31; ++k) b[k] = (char)((name[k] >= 'A' && name[k] <= 'Z') ? name[k] - 'A' + 'a' : name[k]);
    b[k] = 0;
    for (int i = 0; i < g_hotN; ++i)
        if (!strcmp(g_hotSets[i].name, b)) return i;
    return -1;
}

}  // namespace

void Start(const StartInfo& info) {
    g_log = info.log;
    g_wine = info.wine ? info.wine : "";
    const std::wstring ini = info.iniPath ? info.iniPath : L"";
    g_cfg.enabled = IniInt(L"Enabled", 0, 0, 1, ini);
    g_cfg.dumpMode = IniInt(L"DumpMode", 0, 0, 2, ini);
    g_cfg.logAll = IniInt(L"LogAll", 0, 0, 1, ini);
    g_cfg.track = (GetPrivateProfileIntW(L"Profiler", L"GPUProf", 0, ini.c_str()) >= 2 ||
                   GetPrivateProfileIntW(L"Debug", L"SkipFx", 0, ini.c_str()) == 1 ||
                   GetPrivateProfileIntW(L"Profiler", L"FrameCap", 0, ini.c_str()) != 0) ? 1 : 0;   // GP2 / SkipFx / FC need the PSO -> shader map
    g_onDevice = info.onDevice;
    g_cfg.hot = GetPrivateProfileIntW(L"ShaderHot", L"Enabled", 0, ini.c_str()) == 1 ? 1 : 0;
    if (g_cfg.hot) {
        g_cfg.hotFolder = ResolveFolder(ini, L"ShaderHot", L"Folder", L"Nioh3EngineFix_hot");
        g_hotTab = static_cast<HotEnt*>(VirtualAlloc(nullptr, sizeof(HotEnt) * kHotTab, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        if (!g_hotTab || !IndexHot()) {
            LOG("HOT: [ShaderHot] Enabled = 1 but %ls holds no set (sub-folder with on_/off_ shader files) - ShaderHot off",
                g_cfg.hotFolder.c_str());
            g_cfg.hot = 0;
            g_hotN = 0;
        } else {
            LOG("HOT: DIAGNOSTIC ON - %d shader set(s) from %ls get twin PSOs; the live switch (LiveCtl 'Hot.<set> = 0/1') picks "
                "which one SetPipelineState binds. A/B testing only: never ship or play with ShaderHot on.", g_hotN, g_cfg.hotFolder.c_str());
        }
    }
    if (!g_cfg.enabled && !g_cfg.track && !g_cfg.hot) {
        g_state = (LONG)State::Off;
        if (info.observeOnly && info.onDevice) {  // PJ only: arm the device hook, hook nothing here, stay Off
            g_observeOnly = true;
            LOG("Enabled=0: D3D12CreateDevice hook armed only to report the device to PJ (no swap, no dump, no tracking)");
            if (!ArmImportHooksSafe()) LOG("no D3D12CreateDevice import found in the main module - PJ gets no device");
        }
        return;
    }
    if (g_cfg.track) {
        g_sets = static_cast<SetRec*>(VirtualAlloc(nullptr, sizeof(SetRec) * kMaxTrkSets, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        g_psoTab = static_cast<PsoEnt*>(VirtualAlloc(nullptr, sizeof(PsoEnt) * kPsoTab, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        if (!g_sets || !g_psoTab) g_cfg.track = 0;
    }
    g_cfg.folder = ResolveFolder(ini, L"ShaderSwap", L"Folder", L"Nioh3EngineFix_shaders");
    uintptr_t lo, hi;
    if (ImageRange(GetModuleHandleW(nullptr), lo, hi)) g_exeLo = lo, g_exeHi = hi;
    const int files = g_cfg.enabled ? IndexFolder() : 0;
    g_state = (LONG)State::Waiting;
    if (!g_cfg.enabled)
        LOG("Enabled=0 but %s: PSO creation is hooked only for that (no swap, no dump)",
            g_cfg.track && g_cfg.hot ? "GP2/SkipFx tracking and ShaderHot" : g_cfg.track ? "[Profiler] GPUProf >= 2 / SkipFx (PSO -> shader map)" : "ShaderHot (twin PSOs)");
    LOG("Enabled=%d track=%d DumpMode=%d LogAll=%d Folder=\"%ls\": %d replacement file(s) indexed%s%s", g_cfg.enabled, g_cfg.track, g_cfg.dumpMode, g_cfg.logAll,
        g_cfg.folder.c_str(), files, g_wine.empty() ? "" : ", wine ", g_wine.c_str());
    if (!ArmImportHooksSafe())
        LOG("no D3D12CreateDevice import found in the main module - waiting for InstallNow (late install: PSOs created "
            "before it keep their original bytecode)");
}

bool InstallNow() {
    if (g_state == (LONG)State::Off) return false;
    if (g_devN > 0) return true;
    // the IAT hook did not fire (Start ran after the device was created, or the unpacker rewrote the slot)
    for (int i = 0; i < 2; ++i)
        if (g_iatSlot[i] && *g_iatSlot[i] != (i == 0 ? (void*)&HkCreateDevice<0> : (void*)&HkCreateDevice<1>))
            LOG("the IAT slot of %s no longer points to the hook (rewritten after Start)", g_iatDll[i]);
    wchar_t sys[MAX_PATH] = {};
    const UINT n = GetSystemDirectoryW(sys, MAX_PATH);
    if (!n || n > MAX_PATH - 16) return false;
    HMODULE d3d12 = LoadLibraryW((std::wstring(sys) + L"\\d3d12.dll").c_str());
    auto create = d3d12 ? reinterpret_cast<CreateDeviceFn>(GetProcAddress(d3d12, "D3D12CreateDevice")) : nullptr;
    ID3D12Device* dev = nullptr;
    const HRESULT hr = create ? create(nullptr, D3D_FEATURE_LEVEL_11_0, kIID_Device, reinterpret_cast<void**>(&dev)) : E_FAIL;
    if (FAILED(hr) || !dev) {
        LOG("late install: D3D12CreateDevice failed (hr %08lX) - not hooked", (unsigned long)hr);
        if (!g_devN) g_state = (LONG)State::Failed;
        return false;
    }
    InterlockedExchange(&g_late, 1);
    HookDeviceSafe(dev, "late install (default adapter)");
    if (g_onDevice) g_onDevice(dev);  // PJ: root signatures created before this point are unknown (their PSOs are skipped)
    dev->Release();
    if (g_devN) LOG("late install: PSOs the engine created before this point keep their original bytecode");
    return g_devN > 0;
}

bool Tracking() { return g_cfg.track != 0 && g_sets && g_psoTab; }
uint32_t PsoSetId(const void* pso) {
    if (!g_psoTab || !pso) return 0;
    const uintptr_t p = (uintptr_t)pso;
    uint32_t pos = (uint32_t)(((p >> 4) * 0x9E3779B97F4A7C15ull) >> 40) & (kPsoTab - 1);
    for (int probe = 0; probe < 64; ++probe, pos = (pos + 1) & (kPsoTab - 1)) {
        const PsoEnt& e = g_psoTab[pos];
        if (e.p == p) return e.id;
        if (e.p == 0) return 0;
    }
    return 0;
}
int DescribeSet(uint32_t id, char* buf, size_t n) {
    if (!buf || !n) return 0;
    buf[0] = 0;
    if (!id || id >= (uint32_t)g_setN || !g_sets) return 0;
    const SetRec& r = g_sets[id];
    size_t o = 0;
    for (int i = 0; i < r.n && o + 48 < n; ++i) {
        char kn[40];
        KeyName(r.k[i], kn);
        o += (size_t)snprintf(buf + o, n - o, "%s%s=%s", i ? " " : "", StageName(r.st[i]), kn);
    }
    return (int)o;
}
uint32_t SetCount() { return (uint32_t)g_setN - 1; }

// ---- ShaderHot public API ---------------------------------------------------------------------------------------------------
bool HotEnabled() { return g_cfg.hot != 0 && g_hotN > 0; }
int HotSetCount() { return g_hotN; }
bool HotSet(const char* name, int on, char* reply, size_t n) {
    const int i = FindHotSet(name);
    if (i < 0) {
        if (reply && n) snprintf(reply, n, "no hot set '%s'%s", name ? name : "", HotEnabled() ? "" : " ([ShaderHot] off)");
        return false;
    }
    HotSetInfo& s = g_hotSets[i];
    const LONG was = InterlockedExchange(&s.on, on ? 1 : 0);
    if (was != (on ? 1 : 0))
        LOG("HOT: set '%s' -> %s (%ld twin(s), %ld failed; alt binds so far %lld)", s.name, on ? "ON" : "OFF", (long)s.twins, (long)s.fails,
            (long long)s.altBinds);
    if (reply && n)
        snprintf(reply, n, "Hot.%s = %s (value %d) twins %ld fails %ld", s.name, on ? "ON" : "OFF", on ? 1 : 0, (long)s.twins, (long)s.fails);
    return true;
}
int HotGet(const char* name) {
    const int i = FindHotSet(name);
    return i < 0 ? -1 : (int)g_hotSets[i].on;
}
uintptr_t HotMap(uintptr_t p) {
    const HotEnt* t = g_hotTab;
    if (!t || !p) return p;
    uint32_t pos = HotPos(p);
    for (int probe = 0; probe < 64; ++probe, pos = (pos + 1) & (kHotTab - 1)) {
        const uintptr_t q = t[pos].p;
        if (q == p) {
            HotSetInfo& s = g_hotSets[t[pos].set];
            const bool alt = t[pos].pol ? s.on != 0 : s.on == 0;
            InterlockedIncrement64(alt ? &s.altBinds : &s.baseBinds);
            return alt ? t[pos].alt : p;
        }
        if (!q) return p;
    }
    return p;
}
const char* HotStatus(char* buf, size_t n) {
    if (!buf || !n) return "";
    buf[0] = 0;
    size_t o = 0;
    for (int i = 0; i < g_hotN && o + 96 < n; ++i) {
        const HotSetInfo& s = g_hotSets[i];
        const int w = snprintf(buf + o, n - o, "%s%s=%s (%ld twin, %ld failed, %d file, binds alt %lld / game %lld)", i ? "; " : "", s.name,
                               s.on ? "ON" : "OFF", (long)s.twins, (long)s.fails, s.files, (long long)s.altBinds, (long long)s.baseBinds);
        if (w < 0 || (o += (size_t)w) >= n) break;
    }
    return buf;
}

State GetState() { return (State)g_state; }
const char* StateName(State s) {
    switch (s) {
        case State::Off: return "off";
        case State::Waiting: return "waiting for the device";
        case State::Active: return "on";
        case State::Failed: return "failed (see log)";
    }
    return "?";
}

void GetStats(Stats& o) {
    o.graphics = (uint64_t)g_c.graphics;
    o.compute = (uint64_t)g_c.compute;
    o.stream = (uint64_t)g_c.stream;
    o.libLoads = (uint64_t)g_c.libLoads;
    o.shaders = (uint64_t)g_c.shaders;
    o.exeUnique = (uint64_t)g_c.exeUnique;
    o.swappedPsos = (uint64_t)g_c.swappedPsos;
    o.swappedBlobs = (uint64_t)g_c.swappedBlobs;
    o.rejected = (uint64_t)g_c.rejected;
    o.skipped = (uint64_t)g_c.skipped;
    o.dumped = (uint64_t)g_c.dumped;
    o.replacements = (uint32_t)g_repl.size();
    o.vtables = (uint32_t)g_devN;
    o.late = g_late != 0;
}

const char* StatusLine(char* buf, size_t n) {
    if (!buf || !n) return "";
    Stats s;
    GetStats(s);
    snprintf(buf, n, "%s: %llu PSOs (gfx %llu, cs %llu, stream %llu, lib %llu), %llu swapped (%llu blobs), %llu rejected, "
             "%llu skipped, %llu exe shaders%s%s",
             StateName(GetState()), (unsigned long long)(s.graphics + s.compute + s.stream + s.libLoads),
             (unsigned long long)s.graphics, (unsigned long long)s.compute, (unsigned long long)s.stream,
             (unsigned long long)s.libLoads, (unsigned long long)s.swappedPsos, (unsigned long long)s.swappedBlobs,
             (unsigned long long)s.rejected, (unsigned long long)s.skipped, (unsigned long long)s.exeUnique,
             s.late ? ", late install" : "", s.dumped ? ", dumped" : "");
    return buf;
}

}  // namespace psoswap
