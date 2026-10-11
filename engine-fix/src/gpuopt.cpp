// gpuopt.cpp - D3D12 barrier batching for Nioh 3's command lists (see gpuopt.h and notes\gpu-barriers.md).
//
// Correctness argument (why the GPU sees the same dependencies):
//   * D3D12 defines ResourceBarrier(n, b) as n sequential ResourceBarrier(1, b[i]) calls. A barrier only orders and
//     transitions the work recorded BEFORE it against the work recorded AFTER it. Moving a barrier later in the list is
//     therefore result-neutral as long as no command that executes or observes GPU work is crossed. gpuopt crosses
//     only pure state setters (PSO, root signature/arguments, descriptor heaps, IA/RS/OM blend/stencil-ref state); every
//     other method of the list (draw, dispatch, copy, clear, resolve, query, ExecuteIndirect/Bundle, markers,
//     predication, render passes, OMSetRenderTargets, SetSamplePositions, Close/Reset/ClearState, QueryInterface,
//     Release, any slot we do not know) first emits the pending batch.
//   * Within a batch the original order is kept. A second barrier on a resource that is already in the batch emits
//     the batch first, so per-resource sequences never depend on in-call ordering inside a driver.
//   * Drops: (a) a UAV barrier on R (or NULL) when the batch already holds a UAV barrier on R or NULL - no command
//     between them, so the first one already orders everything the second would; (b) optional: A:S1->S2 then
//     A:S2->S1 (same subresource, no split flags) for S1 = RENDER_TARGET, DEPTH_WRITE or read-only states: with no
//     command in between the pair is the identity (no write is made visible or ordered by it).
//   * Only calls made on the dispatcher thread from Nioh3.exe / sl.interposer.dll are deferred. Calls from the D3D12
//     runtime itself, upscalers, overlays and other threads pass through after emitting what is pending on the list.
//   * Any call on a list with pending barriers from ANY thread or module goes through the hooked vtable, so the
//     frame-end barrier the main thread records on the dispatcher's list (3D62E4) and upscaler evaluations
//     (QueryInterface unwrap, then their own recording) see the batch first.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <intrin.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>

#include "gpuopt.h"
// 1.7.59-rpf1: RPF (replayprof.cpp) may chain its timing thunk in front of ours; RpForwards(slot, want) says the slot still
// reaches `want`. Resolves to a 'false' stub when replayprof.cpp is not linked (harnesses).
extern "C" bool RpForwards(const void* slotValue, const void* want);
extern "C" bool RpForwardsNone(const void*, const void*) { return false; }
#pragma comment(linker, "/alternatename:RpForwards=RpForwardsNone")
#include "gimerge.h"
#include "gpuprof.h"
#include "skipfx.h"
#include "psoswap.h"

#pragma intrinsic(_ReturnAddress)

extern "C" {
// gpuopt_thunks.asm
extern void* volatile gpuopt_pend[4];
extern void* gpuopt_orig0[96];
extern void* gpuopt_orig1[96];
extern volatile uint8_t gpuopt_flush[96];
extern volatile uint8_t gpuopt_trace;
extern volatile uint8_t gpuopt_hook[96];  // GP profiler (gpuprof.cpp): slots that always take the slow path
extern volatile uint8_t gpuopt_hook2[96];  // 1.7.29 gimerge.cpp: slots that always take the slow path
extern void* volatile gpuopt_hold;         // 1.7.29 gimerge.cpp: list with a held Dispatch (all its calls: slow path)
extern volatile uint8_t gpuopt_hook3[96];  // skipfx.cpp ([Debug] SkipFx diagnostic): slots that always take the slow path
extern volatile uint8_t gpuopt_hook4[96];  // psoswap ShaderHot (fast-testing): SetPipelineState / Reset / ClearState rebind
extern void* gpuopt_thunks0[96];
extern void* gpuopt_thunks1[96];
// != 0: skip the method. regs = the saved argument registers {r9, r8, rdx, rcx} the method is called with afterwards
int GpuOptSlow(void* self, uint32_t setSlot, uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t* regs);
}

namespace gpuopt {
namespace {

constexpr int kSlots = 96;
constexpr int kSlotResourceBarrier = 26;
constexpr int kPendSlots = 4;
constexpr int kBatchCap = 128;

// ---- vtable map (ID3D12GraphicsCommandList .. List10, single-inheritance order) -----------------------------------------
enum Kind : uint8_t {
    kSkip,     // not hooked: IUnknown::AddRef, private data, names, GetDevice/GetType (no recording)
    kFlush,    // records / observes GPU work, or hands the list to someone else: emit the batch first
    kBind,     // pure state setter: crossing it is result-neutral (flushes only with MergeAcrossBinds = 0)
    kBarrier,  // ResourceBarrier (own hook)
};
struct SlotInfo { const char* name; Kind kind; };
const SlotInfo kSlotInfo[kSlots] = {
    {"QueryInterface", kFlush}, {"AddRef", kSkip}, {"Release", kFlush}, {"GetPrivateData", kSkip},
    {"SetPrivateData", kSkip}, {"SetPrivateDataInterface", kSkip}, {"SetName", kSkip}, {"GetDevice", kSkip},
    {"GetType", kSkip}, {"Close", kFlush}, {"Reset", kFlush}, {"ClearState", kFlush},
    {"DrawInstanced", kFlush}, {"DrawIndexedInstanced", kFlush}, {"Dispatch", kFlush}, {"CopyBufferRegion", kFlush},
    {"CopyTextureRegion", kFlush}, {"CopyResource", kFlush}, {"CopyTiles", kFlush}, {"ResolveSubresource", kFlush},
    {"IASetPrimitiveTopology", kBind}, {"RSSetViewports", kBind}, {"RSSetScissorRects", kBind},
    {"OMSetBlendFactor", kBind}, {"OMSetStencilRef", kBind}, {"SetPipelineState", kBind},
    {"ResourceBarrier", kBarrier}, {"ExecuteBundle", kFlush}, {"SetDescriptorHeaps", kBind},
    {"SetComputeRootSignature", kBind}, {"SetGraphicsRootSignature", kBind},
    {"SetComputeRootDescriptorTable", kBind}, {"SetGraphicsRootDescriptorTable", kBind},
    {"SetComputeRoot32BitConstant", kBind}, {"SetGraphicsRoot32BitConstant", kBind},
    {"SetComputeRoot32BitConstants", kBind}, {"SetGraphicsRoot32BitConstants", kBind},
    {"SetComputeRootConstantBufferView", kBind}, {"SetGraphicsRootConstantBufferView", kBind},
    {"SetComputeRootShaderResourceView", kBind}, {"SetGraphicsRootShaderResourceView", kBind},
    {"SetComputeRootUnorderedAccessView", kBind}, {"SetGraphicsRootUnorderedAccessView", kBind},
    {"IASetIndexBuffer", kBind}, {"IASetVertexBuffers", kBind},
    {"SOSetTargets", kFlush},          // stream-out targets: kept conservative
    {"OMSetRenderTargets", kFlush},    // conservative: some drivers derive DSV layouts at bind time; costs no merging
    {"ClearDepthStencilView", kFlush}, {"ClearRenderTargetView", kFlush},
    {"ClearUnorderedAccessViewUint", kFlush}, {"ClearUnorderedAccessViewFloat", kFlush}, {"DiscardResource", kFlush},
    {"BeginQuery", kFlush}, {"EndQuery", kFlush}, {"ResolveQueryData", kFlush}, {"SetPredication", kFlush},
    {"SetMarker", kFlush}, {"BeginEvent", kFlush}, {"EndEvent", kFlush}, {"ExecuteIndirect", kFlush},
    // List1
    {"AtomicCopyBufferUINT", kFlush}, {"AtomicCopyBufferUINT64", kFlush}, {"OMSetDepthBounds", kBind},
    {"SetSamplePositions", kFlush},    // MSAA depth decompression in barriers uses the current sample positions
    {"ResolveSubresourceRegion", kFlush}, {"SetViewInstanceMask", kBind},
    // List2, List3
    {"WriteBufferImmediate", kFlush}, {"SetProtectedResourceSession", kFlush},
    // List4
    {"BeginRenderPass", kFlush}, {"EndRenderPass", kFlush}, {"InitializeMetaCommand", kFlush},
    {"ExecuteMetaCommand", kFlush}, {"BuildRaytracingAccelerationStructure", kFlush},
    {"EmitRaytracingAccelerationStructurePostbuildInfo", kFlush}, {"CopyRaytracingAccelerationStructure", kFlush},
    {"SetPipelineState1", kBind}, {"DispatchRays", kFlush},
    // List5, List6, List7, List8, List9, List10
    {"RSSetShadingRate", kBind}, {"RSSetShadingRateImage", kBind}, {"DispatchMesh", kFlush}, {"Barrier", kFlush},
    {"OMSetFrontAndBackStencilRef", kBind}, {"RSSetDepthBias", kBind}, {"IASetIndexBufferStripCutValue", kBind},
    {"SetProgram", kBind}, {"DispatchGraph", kFlush},
    // 86..95: not in any known interface; never patched (the vtable is only patched up to its known size)
    {"slot86", kFlush}, {"slot87", kFlush}, {"slot88", kFlush}, {"slot89", kFlush}, {"slot90", kFlush},
    {"slot91", kFlush}, {"slot92", kFlush}, {"slot93", kFlush}, {"slot94", kFlush}, {"slot95", kFlush},
};

// IIDs newer than the 10.0.22621 SDK headers (from the Agility SDK d3d12.h)
const GUID kIID_List8 = {0xee936ef9, 0x599d, 0x4d28, {0x93, 0x8e, 0x23, 0xc4, 0xad, 0x05, 0xce, 0x51}};
const GUID kIID_List9 = {0x34ed2808, 0xffe6, 0x4c2b, {0xb1, 0x1a, 0xca, 0xbd, 0x2b, 0x0c, 0x59, 0xe1}};
const GUID kIID_List10 = {0x7013c015, 0xd161, 0x4b63, {0xa0, 0x8c, 0x23, 0x85, 0x52, 0xdd, 0x8a, 0xcc}};

// ---- configuration / state ---------------------------------------------------------------------------------------------
struct Cfg {
    int barriers = 0;          // 0 off, 1 on, 2 hooks + counters only (batching off; live switchable)
    int mergeAcrossBinds = 1;
    int dropDupUav = 1;
    int dropUndoPairs = 0;
    int anyCaller = 0;
    int maxBatch = 64;
    int r2a = 0, r2b = 0, r1 = 0;  // barrier rewrites (Wine only, dispatcher thread; notes\vkd3d-audit.md Addendum B)
    int traceRecords = 0;
    int traceDelaySec = 30;
    int installDelaySec = 20;
    wchar_t deferThreads[256] = L"SynergisticDispatcher";
};
Cfg g_cfg;
void (*g_log)(const char*, ...) = nullptr;
std::wstring g_iniPath;
std::string g_wine;
volatile LONG g_state = (LONG)State::Off;
volatile LONG g_batching = 0;
volatile LONG g_dropUndo = 0;
volatile LONG g_r2a = 0, g_r2b = 0, g_r1 = 0;  // live switches (Start copies the ini)
bool g_gpuProf = false;  // the GP profiler needs the hooks even with Barriers = 0 (batching then stays off)
bool g_giHooks = false;  // 1.7.29: gimerge (GIProbe / GIMerge) needs them too
bool g_skipFx = false;   // skipfx ([Debug] SkipFx diagnostic) needs them too
bool g_shaderHot = false;  // psoswap ShaderHot (live shader A/B, fast-testing) needs the PSO-bind slots
SRWLOCK g_installLock = SRWLOCK_INIT;

uintptr_t g_exeLo = 0, g_exeHi = 0;
volatile uintptr_t g_slLo = 0, g_slHi = 0;

using RBFn = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, UINT, const D3D12_RESOURCE_BARRIER*);
RBFn g_origRB[2] = {};
void** g_vt[2] = {};
int g_vtSlots[2] = {};

void LOG(const char* fmt, ...) {
    if (!g_log) return;
    char b[768];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf_s(b, sizeof b, _TRUNCATE, fmt, ap);
    va_end(ap);
    g_log("GPUOPT: %s", b);
}

std::string ModuleOf(const void* p) {
    HMODULE m = nullptr;
    if (!p || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                  (LPCWSTR)p, &m))
        return "?";
    wchar_t w[MAX_PATH] = {};
    GetModuleFileNameW(m, w, MAX_PATH);
    const wchar_t* f = wcsrchr(w, L'\\');
    f = f ? f + 1 : w;
    char a[MAX_PATH] = {};
    WideCharToMultiByte(CP_UTF8, 0, f, -1, a, MAX_PATH, nullptr, nullptr);
    char r[MAX_PATH + 32];
    _snprintf_s(r, sizeof r, _TRUNCATE, "%s+0x%llX", a, (unsigned long long)((uintptr_t)p - (uintptr_t)m));
    return r;
}

bool ImageRange(HMODULE m, uintptr_t& lo, uintptr_t& hi) {
    if (!m) return false;
    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(m);
    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>((const uint8_t*)m + dos->e_lfanew);
    lo = (uintptr_t)m;
    hi = lo + nt->OptionalHeader.SizeOfImage;
    return true;
}

void ResolveStreamline() {
    if (g_slLo) return;
    uintptr_t lo, hi;
    if (ImageRange(GetModuleHandleW(L"sl.interposer.dll"), lo, hi)) {
        g_slHi = hi;
        MemoryBarrier();
        g_slLo = lo;
    }
}

// ---- statistics ----------------------------------------------------------------------------------------------------------
struct Counters {
    volatile LONG64 frames, calls, barriers, deferred, passthrough, passCalls, emitCalls, emitBarriers, dupUav, undoSeen, undoDropped,
        conflictSplits, fullSplits, foreignFlushes, r2a, r2b, r1, keptTex, keptUav, keptUnknown, keptForeign;
    volatile LONG maxBatch;
    volatile LONG64 bySlot[kSlots];
};
Counters g_c{};
inline void Add(volatile LONG64& v, LONG64 d = 1) { InterlockedAdd64(&v, d); }

// ---- trace -----------------------------------------------------------------------------------------------------------------
struct TraceRec {
    int64_t qpc;
    void* list;
    uint32_t tid;
    uint16_t slot;      // vtable slot; 26 = ResourceBarrier struct; 200 = gpuopt emitted a batch
    uint8_t action;     // 0 call, 1 deferred, 2 passed through, 3 dropped dup UAV, 4 dropped undo pair, 5 emit
    uint8_t set;
    uint32_t callerRva; // first return address inside Nioh3.exe (0 = none within 16 frames)
    uint32_t caller2Rva; // second return address inside Nioh3.exe (the caller of the first frame's function)
    uint64_t a1, a2, a3;
    uint32_t resDim, resFlags;  // barrier rows: D3D12_RESOURCE_DIMENSION and D3D12_RESOURCE_FLAGS of the resource (0 = unknown)
};
TraceRec* g_trace = nullptr;
volatile LONG g_traceN = 0;
LONG g_traceCap = 0;
volatile LONG g_traceDone = 0;

// First two return addresses inside Nioh3.exe (unwound via .pdata by RtlCaptureStackBackTrace). Only called while tracing.
void CallerRvas(uint32_t& c1, uint32_t& c2) {
    void* fr[16] = {};
    USHORT n = RtlCaptureStackBackTrace(1, 16, fr, nullptr);
    c1 = c2 = 0;
    for (USHORT i = 0; i < n; ++i) {
        const uintptr_t a = (uintptr_t)fr[i];
        if (a < g_exeLo || a >= g_exeHi) continue;
        if (!c1) c1 = (uint32_t)(a - g_exeLo);
        else { c2 = (uint32_t)(a - g_exeLo); break; }
    }
}

// ---- per-resource cache: GetDesc once per pointer (trace) + the last tracked state (rewrites) ---------------------------
// Keyed by the ID3D12Resource pointer. A recycled pointer can carry a stale Dimension/Flags here, so every rewrite
// candidate calls GetDesc again (a memcpy in vkd3d); the cached values are only used for the trace columns.
struct ResEnt {
    ID3D12Resource* p;
    uint32_t dim, flags;
    uint32_t last;      // last whole-resource StateAfter the engine's tracker moved it to (0 = COMMON or unknown)
    uint32_t lastTid;
    uint32_t lastGen;   // list generation (thread-local) of that barrier
};
constexpr int kResBits = 15;
ResEnt g_res[1 << kResBits];
SRWLOCK g_resLock = SRWLOCK_INIT;
thread_local ID3D12GraphicsCommandList* t_lastCl = nullptr;
thread_local uint32_t t_listGen = 1;  // bumped when the thread moves to another list and on Close(): "same list" test

void DescInto(ResEnt& e) {
    const D3D12_RESOURCE_DESC d = e.p->GetDesc();
    e.dim = (uint32_t)d.Dimension;
    e.flags = (uint32_t)d.Flags;
}

// lock held. Never returns null.
ResEnt* ResFind(ID3D12Resource* p) {
    const size_t h = (size_t)(((uint64_t)(uintptr_t)p >> 4) * 0x9E3779B97F4A7C15ull >> (64 - kResBits));
    ResEnt* victim = &g_res[h];
    for (int i = 0; i < 8; ++i) {
        ResEnt& e = g_res[(h + i) & ((1 << kResBits) - 1)];
        if (e.p == p) return &e;
        if (!e.p) {
            victim = &e;
            break;
        }
    }
    memset(victim, 0, sizeof *victim);
    victim->p = p;
    DescInto(*victim);
    return victim;
}

void TraceAdd(void* list, int set, int slot, int action, uint64_t a1, uint64_t a2, uint64_t a3, bool caller,
              ID3D12Resource* res = nullptr) {
    if (!gpuopt_trace || !g_trace) return;
    const LONG i = InterlockedIncrement(&g_traceN) - 1;
    if (i >= g_traceCap) {
        gpuopt_trace = 0;
        return;
    }
    TraceRec& r = g_trace[i];
    LARGE_INTEGER q;
    QueryPerformanceCounter(&q);
    r.qpc = q.QuadPart;
    r.list = list;
    r.tid = GetCurrentThreadId();
    r.slot = (uint16_t)slot;
    r.action = (uint8_t)action;
    r.set = (uint8_t)set;
    r.callerRva = r.caller2Rva = 0;
    if (caller) CallerRvas(r.callerRva, r.caller2Rva);
    r.a1 = a1;
    r.a2 = a2;
    r.a3 = a3;
    r.resDim = r.resFlags = 0;
    if (res) {
        AcquireSRWLockExclusive(&g_resLock);
        const ResEnt* e = ResFind(res);
        r.resDim = e->dim;
        r.resFlags = e->flags;
        ReleaseSRWLockExclusive(&g_resLock);
    }
}

void TraceBarrier(void* list, int set, int action, const D3D12_RESOURCE_BARRIER& b, bool caller) {
    if (!gpuopt_trace) return;
    uint64_t res = 0, states = 0, sub = 0;
    ID3D12Resource* dres = nullptr;
    if (b.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION) {
        res = (uint64_t)b.Transition.pResource;
        dres = b.Transition.pResource;
        states = ((uint64_t)(uint32_t)b.Transition.StateBefore << 32) | (uint32_t)b.Transition.StateAfter;
        sub = b.Transition.Subresource;
    } else if (b.Type == D3D12_RESOURCE_BARRIER_TYPE_UAV) {
        res = (uint64_t)b.UAV.pResource;
        dres = b.UAV.pResource;
    } else {
        res = (uint64_t)b.Aliasing.pResourceBefore;
        dres = b.Aliasing.pResourceBefore;
        states = (uint64_t)b.Aliasing.pResourceAfter;
    }
    TraceAdd(list, set, kSlotResourceBarrier, action, res, states,
             ((uint64_t)b.Type << 56) | ((uint64_t)b.Flags << 48) | (sub & 0xFFFFFFFFull), caller, dres);
}

DWORD WINAPI TraceThread(LPVOID) {
    Sleep((DWORD)g_cfg.traceDelaySec * 1000);
    LOG("trace on (%d records)", g_traceCap);
    gpuopt_trace = 1;
    const DWORD t0 = GetTickCount();
    while (g_traceN < g_traceCap && GetTickCount() - t0 < 120000) Sleep(200);
    gpuopt_trace = 0;
    Sleep(500);  // let in-flight recorders finish their record
    const LONG n = g_traceN < g_traceCap ? g_traceN : g_traceCap;
    std::wstring path = g_iniPath;
    const size_t cut = path.find_last_of(L"\\/");
    path = (cut == std::wstring::npos ? std::wstring() : path.substr(0, cut + 1)) + L"Nioh3EngineFix.gpuopt-trace.csv";
    FILE* f = nullptr;
    if (_wfopen_s(&f, path.c_str(), L"wb") || !f) {
        LOG("trace: cannot write the CSV");
        return 0;
    }
    LARGE_INTEGER fq;
    QueryPerformanceFrequency(&fq);
    const int64_t q0 = n ? g_trace[0].qpc : 0;
    fprintf(f, "seq,us,tid,list,set,slot,method,action,caller_rva,a1,a2,a3,caller2_rva,res_dim,res_flags\n");
    static const char* kAct[] = {"call", "deferred", "pass", "drop_dup_uav", "drop_undo", "emit"};
    for (LONG i = 0; i < n; ++i) {
        const TraceRec& r = g_trace[i];
        const char* m = r.slot < kSlots ? kSlotInfo[r.slot].name : (r.slot == 200 ? "EMIT" : "?");
        fprintf(f, "%ld,%.3f,%u,%p,%u,%u,%s,%s,0x%X,0x%llX,0x%llX,0x%llX,0x%X,%u,0x%X\n", i, (r.qpc - q0) * 1e6 / (double)fq.QuadPart,
                r.tid, r.list, r.set, r.slot, m, r.action < 6 ? kAct[r.action] : "?", r.callerRva,
                (unsigned long long)r.a1, (unsigned long long)r.a2, (unsigned long long)r.a3, r.caller2Rva, r.resDim, r.resFlags);
    }
    fclose(f);
    InterlockedExchange(&g_traceDone, 1);
    LOG("trace: %ld records written to Nioh3EngineFix.gpuopt-trace.csv (analyse: re\\scripts_d3d12\\gpuopt_trace.py)", n);
    return 0;
}

// ---- pending batches (one per deferring thread) ----------------------------------------------------------------------
struct Pend {
    SRWLOCK lock;
    void* list;     // mirrors gpuopt_pend[i] while n > 0
    int set;
    UINT n;
    D3D12_RESOURCE_BARRIER b[kBatchCap];
};
Pend g_pend[kPendSlots];
volatile LONG g_pendOwner[kPendSlots] = {};
thread_local int t_slot = -2;        // -2 unknown, -1 none available
thread_local int t_cls = 0;          // 0 unknown, 1 deferring thread, 2 not (re-checked while the name is empty)
thread_local int t_clsRetry = 0;
thread_local int t_inEmit = 0;       // re-entrancy guard (our own emit calls, third-party detours inside them)
volatile LONG g_loggedFirstDefer = 0, g_loggedCallerReject = 0;

// emits p (lock held)
void EmitLocked(int i, int reasonSlot, bool foreign) {
    Pend& p = g_pend[i];
    if (!p.n) return;
    const UINT n = p.n;
    t_inEmit++;
    g_origRB[p.set]((ID3D12GraphicsCommandList*)p.list, n, p.b);
    t_inEmit--;
    if (gpuopt_trace) TraceAdd(p.list, p.set, 200, 5, n, (uint64_t)reasonSlot, 0, false);
    p.n = 0;
    InterlockedExchangePointer((void* volatile*)&gpuopt_pend[i], nullptr);
    Add(g_c.emitCalls);
    Add(g_c.emitBarriers, n);
    if (reasonSlot >= 0 && reasonSlot < kSlots) Add(g_c.bySlot[reasonSlot]);
    if (foreign) Add(g_c.foreignFlushes);
    LONG m = g_c.maxBatch;
    while ((LONG)n > m && InterlockedCompareExchange(&g_c.maxBatch, (LONG)n, m) != m) m = g_c.maxBatch;
}

// emits whatever is deferred on `list` (any thread's slot except `except`)
void FlushList(void* list, int except, int reasonSlot) {
    for (int i = 0; i < kPendSlots; ++i) {
        if (i == except || gpuopt_pend[i] != list) continue;
        Pend& p = g_pend[i];
        AcquireSRWLockExclusive(&p.lock);
        if (p.list == list && p.n) EmitLocked(i, reasonSlot, i != t_slot);
        ReleaseSRWLockExclusive(&p.lock);
    }
}

bool ThreadDefers() {
    if (t_cls == 1) return true;
    if (t_cls == 2 && t_clsRetry-- > 0) return false;
    using GetDescFn = HRESULT(WINAPI*)(HANDLE, PWSTR*);
    static GetDescFn getDesc = [] {
        HMODULE k = GetModuleHandleW(L"kernel32.dll");
        return k ? reinterpret_cast<GetDescFn>(GetProcAddress(k, "GetThreadDescription")) : nullptr;
    }();
    bool any = !wcscmp(g_cfg.deferThreads, L"*");
    bool match = false, named = false;
    PWSTR d = nullptr;
    if (getDesc && SUCCEEDED(getDesc(GetCurrentThread(), &d)) && d) {
        named = d[0] != 0;
        // ';'-separated substrings
        wchar_t list[256];
        wcsncpy_s(list, g_cfg.deferThreads, _TRUNCATE);
        wchar_t* ctx = nullptr;
        for (wchar_t* tok = wcstok_s(list, L";", &ctx); tok && !match; tok = wcstok_s(nullptr, L";", &ctx))
            if (*tok && wcsstr(d, tok)) match = true;
        LocalFree(d);
    }
    t_cls = (any || match) ? 1 : 2;
    t_clsRetry = named ? 0x7FFFFFFF : 4096;  // unnamed threads are re-checked now and then (names may come later)
    return t_cls == 1;
}

int MySlot() {
    if (t_slot != -2) return t_slot;
    const LONG tid = (LONG)GetCurrentThreadId();
    for (int i = 0; i < kPendSlots; ++i)
        if (InterlockedCompareExchange(&g_pendOwner[i], tid, 0) == 0) {
            t_slot = i;
            return i;
        }
    t_slot = -1;
    LOG("more than %d deferring threads - thread %ld passes through", kPendSlots, tid);
    return -1;
}

bool CallerAllowed(const void* ra) {
    if (g_cfg.anyCaller) return true;
    const uintptr_t a = (uintptr_t)ra;
    if (a >= g_exeLo && a < g_exeHi) return true;
    if (!g_slLo) ResolveStreamline();
    if (g_slLo && a >= g_slLo && a < g_slHi) return true;
    if (!InterlockedExchange(&g_loggedCallerReject, 1))
        LOG("ResourceBarrier on the dispatcher thread from %s is not deferred (only Nioh3.exe / sl.interposer.dll; "
            "[GpuOpt] AnyCaller = 1 to allow)", ModuleOf(ra).c_str());
    return false;
}

constexpr UINT kReadOnlyStates = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER | D3D12_RESOURCE_STATE_INDEX_BUFFER |
                                 D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
                                 D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT |
                                 D3D12_RESOURCE_STATE_COPY_SOURCE | D3D12_RESOURCE_STATE_RESOLVE_SOURCE |
                                 D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE;
bool UndoSafeState(D3D12_RESOURCE_STATES s) {
    if (s == D3D12_RESOURCE_STATE_RENDER_TARGET || s == D3D12_RESOURCE_STATE_DEPTH_WRITE) return true;
    return s != 0 && (s & ~kReadOnlyStates) == 0;
}

ID3D12Resource* ResOf(const D3D12_RESOURCE_BARRIER& b) {
    return b.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION ? b.Transition.pResource
           : b.Type == D3D12_RESOURCE_BARRIER_TYPE_UAV      ? b.UAV.pResource
                                                            : nullptr;
}

// ---- barrier rewrites R2a / R2b / R1 (notes\vkd3d-audit.md Addendum B; all default OFF, Wine only, dispatcher thread) ------
// Input: the structs of one ResourceBarrier call. Output: the structs to record (0..2n). glue[j] = out[j] is the second
// half of a rewritten pair and must stay in the same batch as out[j-1].
//   bookkeeping: per resource the engine tracker's last StateAfter ("P"), the recording thread and the list generation.
//   R2a: COMMON -> X on a BUFFER without ALLOW_UNORDERED_ACCESS, P known (set earlier, same thread, an EARLIER list):
//        StateBefore := P | NPSR | PSR when P is read-only; a write P gives {P -> NPSR|PSR} + {NPSR|PSR -> X}.
//   R2b: the same for UAV buffers (EXPERIMENT): {UAV -> X} (+ {P -> X} when P has other bits); UAV -> UAV becomes a UAV barrier.
//   R1 : drop read -> read where the new state is a superset of the old one and only VCB|IB|NPSR|PSR bits are involved.
// Anything that cannot be checked (texture, no P, other thread, same list, partial subresource, split-barrier flags) is passed
// through unchanged and counted (kept_*).
constexpr UINT kReadMask = 0x2AE3;     // VCB|IB|DEPTH_READ|NPSR|PSR|IA|COPY_SOURCE|RESOLVE_SOURCE
constexpr UINT kShaderRead = 0xC0;     // NPSR|PSR
constexpr UINT kR1Mask = 0xC3;         // VCB|IB|NPSR|PSR
thread_local D3D12_RESOURCE_BARRIER t_rw[2 * kBatchCap];
thread_local bool t_glue[2 * kBatchCap];

UINT RewriteBarriers(ID3D12GraphicsCommandList* cl, UINT n, const D3D12_RESOURCE_BARRIER* b, D3D12_RESOURCE_BARRIER* out,
                     bool* glue) {
    const bool r2a = g_r2a != 0, r2b = g_r2b != 0, r1 = g_r1 != 0;
    const uint32_t tid = GetCurrentThreadId();
    if (cl != t_lastCl) {
        t_lastCl = cl;
        ++t_listGen;
    }
    const uint32_t gen = t_listGen;
    UINT m = 0;
    AcquireSRWLockExclusive(&g_resLock);
    for (UINT k = 0; k < n; ++k) {
        const D3D12_RESOURCE_BARRIER& x = b[k];
        if (x.Type == D3D12_RESOURCE_BARRIER_TYPE_UAV) {
            out[m] = x;
            glue[m++] = false;
            continue;
        }
        if (x.Type != D3D12_RESOURCE_BARRIER_TYPE_TRANSITION) {
            if (x.Type == D3D12_RESOURCE_BARRIER_TYPE_ALIASING) {  // the tracker's picture of both resources is gone
                if (x.Aliasing.pResourceBefore) ResFind(x.Aliasing.pResourceBefore)->last = 0;
                if (x.Aliasing.pResourceAfter) ResFind(x.Aliasing.pResourceAfter)->last = 0;
            }
            out[m] = x;
            glue[m++] = false;
            continue;
        }
        const D3D12_RESOURCE_TRANSITION_BARRIER& t = x.Transition;
        ResEnt* e = ResFind(t.pResource);
        const UINT before = (UINT)t.StateBefore, after = (UINT)t.StateAfter;
        const bool whole = x.Flags == D3D12_RESOURCE_BARRIER_FLAG_NONE &&
                           (t.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES || t.Subresource == 0);
        bool handled = false;
        if ((r2a || r2b) && whole && before == 0 && after != 0) {
            DescInto(*e);
            if (e->dim != D3D12_RESOURCE_DIMENSION_BUFFER) {
                Add(g_c.keptTex);
            } else if (!e->last) {
                Add(g_c.keptUnknown);
            } else if (e->lastTid != tid || e->lastGen == gen) {
                Add(g_c.keptForeign);
            } else {
                const UINT P = e->last;
                if (!(e->flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)) {
                    if (r2a) {
                        out[m] = x;
                        glue[m] = false;
                        if ((P & ~kReadMask) == 0) {
                            out[m++].Transition.StateBefore = (D3D12_RESOURCE_STATES)(P | kShaderRead);
                        } else {  // P is a write state (COPY_DEST, STREAM_OUT, ...): P -> reads -> X
                            out[m].Transition.StateBefore = (D3D12_RESOURCE_STATES)P;
                            out[m++].Transition.StateAfter = (D3D12_RESOURCE_STATES)kShaderRead;
                            out[m] = x;
                            out[m].Transition.StateBefore = (D3D12_RESOURCE_STATES)kShaderRead;
                            glue[m++] = true;
                        }
                        Add(g_c.r2a);
                        handled = true;
                    }
                } else if (r2b) {
                    if (P == D3D12_RESOURCE_STATE_UNORDERED_ACCESS && after == D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
                        D3D12_RESOURCE_BARRIER u = {};
                        u.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                        u.UAV.pResource = t.pResource;
                        out[m] = u;
                        glue[m++] = false;
                    } else {
                        out[m] = x;
                        out[m].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
                        glue[m++] = false;
                        if ((P & ~kShaderRead) && P != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
                            out[m] = x;
                            out[m].Transition.StateBefore = (D3D12_RESOURCE_STATES)P;
                            glue[m++] = true;
                        }
                    }
                    Add(g_c.r2b);
                    handled = true;
                } else {
                    Add(g_c.keptUav);
                }
            }
        } else if (r1 && x.Flags == D3D12_RESOURCE_BARRIER_FLAG_NONE && before && before != after && (before & after) == before &&
                   (after & ~kR1Mask) == 0) {
            Add(g_c.r1);  // dropped: the engine's tracker already holds `after`, later barriers name a covering before-state
            handled = true;
            e->last = after;
            e->lastTid = tid;
            e->lastGen = gen;
            continue;
        }
        if (!handled) {
            out[m] = x;
            glue[m++] = false;
        }
        e->last = whole ? after : 0;
        e->lastTid = tid;
        e->lastGen = gen;
    }
    ReleaseSRWLockExclusive(&g_resLock);
    return m;
}

template <int S>
void STDMETHODCALLTYPE HkResourceBarrier(ID3D12GraphicsCommandList* cl, UINT n, const D3D12_RESOURCE_BARRIER* b) {
    if (t_inEmit) {
        g_origRB[S](cl, n, b);
        return;
    }
    if (gp::g_gqHeads) gp::OnListHead(cl, S);  // 1.7.40 GQ: a barrier is often a list's first GPU command (stamp before it)
    if (g_giHooks) gimerge::OnBarrier(cl, S, n, b);  // 1.7.29: a transition on a list with a held Dispatch replays it first
    void* const ra = _ReturnAddress();
    Add(g_c.calls);
    Add(g_c.barriers, n);
    const bool* gl = nullptr;  // glue flags of a rewritten call (second half of a pair stays in the batch of the first)
    if ((g_r2a | g_r2b | g_r1) && n && b && n <= (UINT)kBatchCap && !g_wine.empty() && ThreadDefers() && CallerAllowed(ra)) {
        n = RewriteBarriers(cl, n, b, t_rw, t_glue);
        b = t_rw;
        gl = t_glue;
        if (!n) return;  // every struct dropped (R1): no vkd3d call at all
    }
    int i = -1;
    if (g_batching && n && b && n <= (UINT)g_cfg.maxBatch && ThreadDefers() && CallerAllowed(ra)) i = MySlot();
    if (i < 0) {
        FlushList(cl, -1, kSlotResourceBarrier);
        if (gpuopt_trace)
            for (UINT k = 0; k < n && b; ++k) TraceBarrier(cl, S, 2, b[k], true);
        const bool mlp = gp::g_mlpBarriers != 0 || gp::g_fcCap != 0;   // 1.7.47 MLP / MpcCheck stamps; FC barrier rows
        if (mlp) gp::OnBarrier(cl, S, n, b, false, ra);
        g_origRB[S](cl, n, b);
        if (mlp) gp::OnBarrier(cl, S, n, b, true, ra);
        Add(g_c.passthrough, n);
        Add(g_c.passCalls);
        return;
    }
    FlushList(cl, i, kSlotResourceBarrier);  // another thread's pending batch on this list (frame-end hand-over)
    Pend& p = g_pend[i];
    AcquireSRWLockExclusive(&p.lock);
    if (p.n && p.list != cl) EmitLocked(i, kSlotResourceBarrier, false);  // this thread moved to another list
    if (!InterlockedExchange(&g_loggedFirstDefer, 1))
        LOG("first deferred barrier: thread %lu, caller %s, list %p (vtable set %d)", GetCurrentThreadId(),
            ModuleOf(ra).c_str(), (void*)cl, S);
    for (UINT k = 0; k < n; ++k) {
        const D3D12_RESOURCE_BARRIER& x = b[k];
        if (x.Type != D3D12_RESOURCE_BARRIER_TYPE_TRANSITION && x.Type != D3D12_RESOURCE_BARRIER_TYPE_UAV) {
            // aliasing (or unknown): keep it exactly where it was
            EmitLocked(i, kSlotResourceBarrier, false);
            if (gpuopt_trace) TraceBarrier(cl, S, 2, x, true);
            t_inEmit++;
            g_origRB[S](cl, 1, &x);
            t_inEmit--;
            Add(g_c.passthrough);
            Add(g_c.passCalls);
            continue;
        }
        if (gl && gl[k]) {  // second half of a rewritten pair: shares the batch with its first half (room was reserved)
            p.b[p.n++] = x;
            Add(g_c.deferred);
            if (gpuopt_trace) TraceBarrier(cl, S, 1, x, true);
            continue;
        }
        const UINT need = (gl && k + 1 < n && gl[k + 1]) ? 2u : 1u;
        ID3D12Resource* r = ResOf(x);
        int same = -1;          // pending entry on the same resource
        bool uavCovered = false;
        for (UINT j = 0; j < p.n; ++j) {
            const D3D12_RESOURCE_BARRIER& e = p.b[j];
            if (e.Type == D3D12_RESOURCE_BARRIER_TYPE_UAV && (!e.UAV.pResource || e.UAV.pResource == r))
                uavCovered = true;
            if (r && ResOf(e) == r) same = (int)j;
        }
        if (x.Type == D3D12_RESOURCE_BARRIER_TYPE_UAV) {
            // a pending UAV barrier on r (or NULL) with nothing recorded since: this one adds nothing
            bool onlyUav = true;
            for (UINT j = 0; j < p.n && onlyUav; ++j)
                if (r && ResOf(p.b[j]) == r && p.b[j].Type != D3D12_RESOURCE_BARRIER_TYPE_UAV) onlyUav = false;
            if (g_cfg.dropDupUav && uavCovered && onlyUav) {
                Add(g_c.dupUav);
                if (gpuopt_trace) TraceBarrier(cl, S, 3, x, true);
                continue;
            }
            if (same >= 0) {  // a transition of r is pending: keep the per-resource order in separate calls
                EmitLocked(i, kSlotResourceBarrier, false);
                Add(g_c.conflictSplits);
            }
        } else if (same >= 0) {
            const D3D12_RESOURCE_BARRIER& e = p.b[same];
            const bool inverse = e.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION &&
                                 e.Flags == D3D12_RESOURCE_BARRIER_FLAG_NONE && x.Flags == D3D12_RESOURCE_BARRIER_FLAG_NONE &&
                                 e.Transition.Subresource == x.Transition.Subresource &&
                                 e.Transition.StateBefore == x.Transition.StateAfter &&
                                 e.Transition.StateAfter == x.Transition.StateBefore;
            if (inverse) {
                Add(g_c.undoSeen);
                if (g_dropUndo && UndoSafeState(e.Transition.StateBefore)) {
                    // remove e, drop x: different resources in a batch commute, so the rest keeps its meaning
                    if (gpuopt_trace) {
                        TraceBarrier(cl, S, 4, e, false);
                        TraceBarrier(cl, S, 4, x, true);
                    }
                    memmove(&p.b[same], &p.b[same + 1], (p.n - same - 1) * sizeof(D3D12_RESOURCE_BARRIER));
                    p.n--;
                    Add(g_c.undoDropped, 2);
                    if (!p.n) InterlockedExchangePointer((void* volatile*)&gpuopt_pend[i], nullptr);
                    continue;
                }
            }
            EmitLocked(i, kSlotResourceBarrier, false);
            Add(g_c.conflictSplits);
        }
        if (p.n + need > (UINT)g_cfg.maxBatch) {
            EmitLocked(i, kSlotResourceBarrier, false);
            Add(g_c.fullSplits);
        }
        p.b[p.n++] = x;
        p.list = cl;
        p.set = S;
        if (gpuopt_pend[i] != cl) InterlockedExchangePointer((void* volatile*)&gpuopt_pend[i], cl);
        Add(g_c.deferred);
        if (gpuopt_trace) TraceBarrier(cl, S, 1, x, true);
    }
    ReleaseSRWLockExclusive(&p.lock);
}

// ---- install -------------------------------------------------------------------------------------------------------------
int SlotCount(IUnknown* list) {
    struct { const GUID* iid; int slots; } tab[] = {
        {&kIID_List10, 86}, {&kIID_List9, 84}, {&kIID_List8, 82},
        {&__uuidof(ID3D12GraphicsCommandList7), 81}, {&__uuidof(ID3D12GraphicsCommandList6), 80},
        {&__uuidof(ID3D12GraphicsCommandList5), 79}, {&__uuidof(ID3D12GraphicsCommandList4), 77},
        {&__uuidof(ID3D12GraphicsCommandList3), 68}, {&__uuidof(ID3D12GraphicsCommandList2), 67},
        {&__uuidof(ID3D12GraphicsCommandList1), 66}, {&__uuidof(ID3D12GraphicsCommandList), 60},
    };
    for (auto& t : tab) {
        IUnknown* q = nullptr;
        if (SUCCEEDED(list->QueryInterface(*t.iid, (void**)&q)) && q) {
            const bool same = *(void***)q == *(void***)list;  // single-inheritance chain: same vtable
            q->Release();
            if (same) return t.slots;
        }
    }
    return 0;
}

void ApplyFlushMask() {
    for (int i = 0; i < kSlots; ++i) {
        const Kind k = kSlotInfo[i].kind;
        gpuopt_flush[i] = (uint8_t)(k == kFlush || (k == kBind && !g_cfg.mergeAcrossBinds));
    }
}

bool PatchVtable(void** vt, int slots, int S) {
    void** orig = S ? gpuopt_orig1 : gpuopt_orig0;
    void** thunks = S ? gpuopt_thunks1 : gpuopt_thunks0;
    DWORD old;
    if (!VirtualProtect(vt, slots * sizeof(void*), PAGE_READWRITE, &old)) {
        LOG("VirtualProtect(vtable %p) failed (%lu)", (void*)vt, GetLastError());
        return false;
    }
    for (int i = 0; i < slots; ++i) orig[i] = vt[i];
    g_origRB[S] = reinterpret_cast<RBFn>(vt[kSlotResourceBarrier]);
    MemoryBarrier();
    int n = 0;
    // every flush / bind slot first, ResourceBarrier last: a barrier is never deferred before all flush points exist
    for (int i = 0; i < slots; ++i) {
        const Kind k = kSlotInfo[i].kind;
        if (k != kFlush && k != kBind) continue;
        InterlockedExchangePointer(&vt[i], thunks[i]);
        ++n;
    }
    void* rb = S ? (void*)&HkResourceBarrier<1> : (void*)&HkResourceBarrier<0>;
    InterlockedExchangePointer(&vt[kSlotResourceBarrier], rb);
    DWORD t;
    VirtualProtect(vt, slots * sizeof(void*), old, &t);
    LOG("vtable set %d at %s: %d slots, %d thunks + ResourceBarrier hooked (original %s)", S, ModuleOf(vt).c_str(), slots,
        n, ModuleOf((void*)g_origRB[S]).c_str());
    return true;
}

bool DoInstall() {
    wchar_t sys[MAX_PATH] = {};
    UINT n = GetSystemDirectoryW(sys, MAX_PATH);
    if (!n || n > MAX_PATH - 16) return false;
    HMODULE d3d12 = LoadLibraryW((std::wstring(sys) + L"\\d3d12.dll").c_str());
    if (!d3d12) {
        LOG("loading the system d3d12.dll failed (%lu)", GetLastError());
        return false;
    }
    auto create = reinterpret_cast<PFN_D3D12_CREATE_DEVICE>(GetProcAddress(d3d12, "D3D12CreateDevice"));
    if (!create) return false;
    ID3D12Device* dev = nullptr;
    HRESULT hr = create(nullptr, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), (void**)&dev);
    if (FAILED(hr) || !dev) {
        LOG("D3D12CreateDevice failed (hr %08lX)", (unsigned long)hr);
        return false;
    }
    void** vts[2] = {};
    int slots[2] = {};
    const D3D12_COMMAND_LIST_TYPE types[2] = {D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_TYPE_COMPUTE};
    for (int t = 0; t < 2; ++t) {
        ID3D12CommandAllocator* a = nullptr;
        ID3D12GraphicsCommandList* l = nullptr;
        if (SUCCEEDED(dev->CreateCommandAllocator(types[t], __uuidof(ID3D12CommandAllocator), (void**)&a)) &&
            SUCCEEDED(dev->CreateCommandList(0, types[t], a, nullptr, __uuidof(ID3D12GraphicsCommandList), (void**)&l))) {
            l->Close();
            vts[t] = *reinterpret_cast<void***>(l);
            slots[t] = SlotCount(l);
        }
        if (l) l->Release();
        if (a) a->Release();
    }
    dev->Release();  // dummies released before patching: none of their calls go through the hooks
    if (!vts[0] || slots[0] < 60) {
        LOG("could not create a dummy DIRECT command list (vtable %p, %d slots)", (void*)vts[0], slots[0]);
        return false;
    }
    ApplyFlushMask();
    g_vt[0] = vts[0];
    g_vtSlots[0] = slots[0];
    if (!PatchVtable(vts[0], slots[0], 0)) return false;
    if (vts[1] && vts[1] != vts[0] && slots[1] >= 60) {
        g_vt[1] = vts[1];
        g_vtSlots[1] = slots[1];
        if (!PatchVtable(vts[1], slots[1], 1)) LOG("COMPUTE list vtable not hooked - compute lists pass through");
    } else {
        LOG("COMPUTE lists share the DIRECT vtable");
    }
    return true;
}

DWORD WINAPI WatchThread(LPVOID) {
    // someone restoring a slot we patched would let a deferred barrier slip past a flush point: stop deferring then
    for (;;) {
        Sleep(2000);
        if (!g_slLo) ResolveStreamline();
        for (int s = 0; s < 2; ++s) {
            if (!g_vt[s]) continue;
            void** thunks = s ? gpuopt_thunks1 : gpuopt_thunks0;
            for (int i = 0; i < g_vtSlots[s]; ++i) {
                const Kind k = kSlotInfo[i].kind;
                const void* want = k == kBarrier ? (s ? (void*)&HkResourceBarrier<1> : (void*)&HkResourceBarrier<0>)
                                   : (k == kFlush || k == kBind) ? thunks[i]
                                                                 : nullptr;
                if (want && g_vt[s][i] != want && !RpForwards(g_vt[s][i], want) && g_batching) {
                    InterlockedExchange(&g_batching, 0);
                    InterlockedExchange(&g_state, (LONG)State::Paused);
                    LOG("vtable set %d slot %d (%s) was replaced by %s - batching switched OFF for safety", s, i,
                        kSlotInfo[i].name, ModuleOf(g_vt[s][i]).c_str());
                }
            }
        }
    }
}

DWORD WINAPI AutoInstallThread(LPVOID) {
    const DWORD t0 = GetTickCount();
    while (GetTickCount() - t0 < 300000 && !(GetModuleHandleW(L"d3d12.dll") && GetModuleHandleW(L"dxgi.dll"))) Sleep(250);
    Sleep((DWORD)g_cfg.installDelaySec * 1000);
    if (g_state == (LONG)State::Waiting) {
        LOG("auto-install %lu ms after start", GetTickCount() - t0);
        InstallNow();
    }
    return 0;
}

int IniInt(const wchar_t* key, int def, int lo, int hi) {
    int v = (int)GetPrivateProfileIntW(L"GpuOpt", key, def, g_iniPath.c_str());
    return v < lo ? lo : v > hi ? hi : v;
}

}  // namespace

// ---- asm slow path ---------------------------------------------------------------------------------------------------------
}  // namespace gpuopt

extern "C" int GpuOptSlow(void* self, uint32_t setSlot, uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t* regs) {
    using namespace gpuopt;
    if (t_inEmit) return 0;
    const int slot = (int)(setSlot & 0xFF), set = (int)(setSlot >> 8);
    if (gpuopt_trace) TraceAdd(self, set, slot, 0, a1, a2, a3, kSlotInfo[slot].kind == kFlush);
    // gimerge first: a held Dispatch is replayed BEFORE the barriers deferred after it are emitted (original order)
    bool skip = (gpuopt_hook2[slot] || self == gpuopt_hold) && gimerge::OnListCall(self, set, slot, a1, a2, a3);
    if (gpuopt_flush[slot]) FlushList(self, -1, slot);
    if (gpuopt_hook[slot]) gp::OnListCall(self, set, slot, a1, a2, a3);  // GP: after the batch flush, before the method
    if (gp::g_fcCap && gpuopt_hook[slot]) gp::FcListCall(self, set, slot, a1, a2, a3, regs);  // FC frame capture
    // SkipFx diagnostic (cost-vs-looks): after the flush, so a skipped draw / dispatch leaves every resource state as recorded
    if (gpuopt_hook3[slot] && skipfx::OnListCall(self, set, slot, a1, a2, a3)) skip = true;
    // ShaderHot (diagnostic): LAST, after every observer above saw the game's own PSO (GP2 / SkipFx attribution stay stock):
    // the method itself is called with the twin when the set's live switch says so. regs: [0] r9, [1] r8, [2] rdx.
    if (gpuopt_hook4[slot] && regs && !skip) {
        if (slot == 25 || slot == 11) {          // SetPipelineState(pso) / ClearState(pso): a1 = rdx
            const uintptr_t n = psoswap::HotMap(a1);
            if (n != a1) regs[2] = n;
        } else if (slot == 10) {                 // Reset(allocator, initial pso): a2 = r8
            const uintptr_t n = psoswap::HotMap(a2);
            if (n != a2) regs[1] = n;
        }
    }
    return skip ? 1 : 0;
}

namespace gpuopt {

void Start(const StartInfo& info) {
    g_log = info.log;
    g_iniPath = info.iniPath ? info.iniPath : L"";
    g_wine = info.wine ? info.wine : "";
    g_cfg.barriers = IniInt(L"Barriers", 0, 0, 2);
    g_cfg.mergeAcrossBinds = IniInt(L"MergeAcrossBinds", 1, 0, 1);
    g_cfg.dropDupUav = IniInt(L"DropDuplicateUav", 1, 0, 1);
    g_cfg.dropUndoPairs = IniInt(L"DropUndoPairs", 0, 0, 1);
    g_cfg.anyCaller = IniInt(L"AnyCaller", 0, 0, 1);
    g_cfg.maxBatch = IniInt(L"MaxBatch", 64, 2, kBatchCap);
    g_cfg.r2a = IniInt(L"R2a", 0, 0, 1);
    g_cfg.r2b = IniInt(L"R2b", 0, 0, 1);
    g_cfg.r1 = IniInt(L"R1", 0, 0, 1);
    g_cfg.traceRecords = IniInt(L"TraceRecords", 0, 0, 4000000);
    g_cfg.traceDelaySec = IniInt(L"TraceDelaySec", 30, 0, 3600);
    g_cfg.installDelaySec = IniInt(L"InstallDelaySec", 20, 0, 600);
    GetPrivateProfileStringW(L"GpuOpt", L"DeferThreads", L"SynergisticDispatcher", g_cfg.deferThreads,
                             (DWORD)(sizeof g_cfg.deferThreads / sizeof(wchar_t)), g_iniPath.c_str());
    g_gpuProf = info.gpuProf;
    g_giHooks = info.giMerge;
    g_skipFx = info.skipFx;
    g_shaderHot = info.shaderHot;
    if (!info.wine) g_cfg.r2a = g_cfg.r2b = g_cfg.r1 = 0;  // the rewrites are designed for vkd3d-proton only
    if (!g_cfg.barriers && !g_cfg.traceRecords && !g_gpuProf && !g_giHooks && !g_skipFx && !g_shaderHot && !g_cfg.r2a && !g_cfg.r2b && !g_cfg.r1) {
        g_state = (LONG)State::Off;
        return;
    }
    for (auto& p : g_pend) InitializeSRWLock(&p.lock);
    uintptr_t lo, hi;
    if (ImageRange(GetModuleHandleW(nullptr), lo, hi)) g_exeLo = lo, g_exeHi = hi;
    g_batching = g_cfg.barriers == 1;
    g_dropUndo = g_cfg.dropUndoPairs;
    g_r2a = g_cfg.r2a;
    g_r2b = g_cfg.r2b;
    g_r1 = g_cfg.r1;
    g_state = (LONG)State::Waiting;
    LOG("Barriers=%d MergeAcrossBinds=%d DropDuplicateUav=%d DropUndoPairs=%d MaxBatch=%d DeferThreads=\"%ls\" "
        "AnyCaller=%d TraceRecords=%d R2a=%d R2b=%d R1=%d%s%s",
        g_cfg.barriers, g_cfg.mergeAcrossBinds, g_cfg.dropDupUav, g_cfg.dropUndoPairs, g_cfg.maxBatch,
        g_cfg.deferThreads, g_cfg.anyCaller, g_cfg.traceRecords, g_cfg.r2a, g_cfg.r2b, g_cfg.r1, g_wine.empty() ? "" : ", wine ", g_wine.c_str());
    if (HANDLE h = CreateThread(nullptr, 0, AutoInstallThread, nullptr, 0, nullptr)) CloseHandle(h);
}

bool InstallNow() {
    AcquireSRWLockExclusive(&g_installLock);
    bool ok = g_state == (LONG)State::Active || g_state == (LONG)State::Paused;
    if (g_state == (LONG)State::Waiting) {
        __try {
            ok = DoInstall();
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            ok = false;
            LOG("exception 0x%08lX during install", GetExceptionCode());
        }
        if (ok) {
            if (g_cfg.traceRecords) {
                g_trace = (TraceRec*)VirtualAlloc(nullptr, (SIZE_T)g_cfg.traceRecords * sizeof(TraceRec),
                                                  MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
                if (g_trace) {
                    g_traceCap = g_cfg.traceRecords;
                    if (HANDLE h = CreateThread(nullptr, 0, TraceThread, nullptr, 0, nullptr)) CloseHandle(h);
                }
            }
            if (HANDLE h = CreateThread(nullptr, 0, WatchThread, nullptr, 0, nullptr)) CloseHandle(h);
            if (g_gpuProf) gp::EnableHooks(gpuopt_hook, kSlots);
            if (g_giHooks) gimerge::EnableHooks(gpuopt_hook2, kSlots, gpuopt_orig0, g_vt[1] ? gpuopt_orig1 : nullptr, &gpuopt_hold);
            if (g_skipFx) skipfx::EnableHooks(gpuopt_hook3, kSlots, gpuopt_orig0, g_vt[1] ? gpuopt_orig1 : nullptr);
            if (g_shaderHot) {   // Reset 10, ClearState 11, SetPipelineState 25
                gpuopt_hook4[10] = gpuopt_hook4[11] = gpuopt_hook4[25] = 1;
                LOG("ShaderHot: PSO-bind slots (SetPipelineState, Reset, ClearState) routed to the twin map");
            }
            g_state = (LONG)(g_batching ? State::Active : State::Paused);
            LOG("installed - batching %s", g_batching ? "ON" : "off (counters only)");
        } else {
            g_state = (LONG)State::Failed;
            LOG("NOT installed - the game's command lists are untouched");
        }
    }
    ReleaseSRWLockExclusive(&g_installLock);
    return ok;
}

State GetState() { return (State)g_state; }
const char* StateName(State s) {
    switch (s) {
        case State::Off: return "off";
        case State::Waiting: return "waiting for the renderer";
        case State::Active: return "on";
        case State::Paused: return "hooked, batching off";
        case State::Failed: return "failed (see log)";
    }
    return "?";
}

void SetBatching(bool on) {
    InterlockedExchange(&g_batching, on ? 1 : 0);
    if (g_state == (LONG)State::Active || g_state == (LONG)State::Paused)
        InterlockedExchange(&g_state, (LONG)(on ? State::Active : State::Paused));
}
bool Batching() { return g_batching != 0; }
int BarriersIni() { return g_cfg.barriers; }
void SetMergeAcrossBinds(bool on) {
    g_cfg.mergeAcrossBinds = on ? 1 : 0;
    ApplyFlushMask();
}
bool MergeAcrossBinds() { return g_cfg.mergeAcrossBinds != 0; }
void SetDropUndoPairs(bool on) { InterlockedExchange(&g_dropUndo, on ? 1 : 0); }
bool DropUndoPairs() { return g_dropUndo != 0; }
void SetRewrite(int which, bool on) {
    volatile LONG* v = which == 0 ? &g_r2a : which == 1 ? &g_r2b : &g_r1;
    if (!g_wine.empty()) InterlockedExchange(v, on ? 1 : 0);  // no effect off Wine
}
bool Rewrite(int which) { return (which == 0 ? g_r2a : which == 1 ? g_r2b : g_r1) != 0; }
bool RewriteIni(int which) { return (which == 0 ? g_cfg.r2a : which == 1 ? g_cfg.r2b : g_cfg.r1) != 0; }

void OnFrame() { Add(g_c.frames); }

void GetStats(Stats& o) {
    o.frames = (uint64_t)g_c.frames;
    o.calls = (uint64_t)g_c.calls;
    o.barriers = (uint64_t)g_c.barriers;
    o.deferred = (uint64_t)g_c.deferred;
    o.passthrough = (uint64_t)g_c.passthrough;
    o.passthroughCalls = (uint64_t)g_c.passCalls;
    o.emitCalls = (uint64_t)g_c.emitCalls;
    o.emitBarriers = (uint64_t)g_c.emitBarriers;
    o.dupUavDropped = (uint64_t)g_c.dupUav;
    o.undoPairsSeen = (uint64_t)g_c.undoSeen;
    o.undoPairsDropped = (uint64_t)g_c.undoDropped;
    o.conflictSplits = (uint64_t)g_c.conflictSplits;
    o.fullSplits = (uint64_t)g_c.fullSplits;
    o.foreignFlushes = (uint64_t)g_c.foreignFlushes;
    o.r2a = (uint64_t)g_c.r2a;
    o.r2b = (uint64_t)g_c.r2b;
    o.r1Dropped = (uint64_t)g_c.r1;
    o.keptTexture = (uint64_t)g_c.keptTex;
    o.keptUav = (uint64_t)g_c.keptUav;
    o.keptUnknown = (uint64_t)g_c.keptUnknown;
    o.keptForeign = (uint64_t)g_c.keptForeign;
    o.maxBatch = (uint32_t)g_c.maxBatch;
    for (int i = 0; i < kSlots; ++i) o.flushBySlot[i] = (uint64_t)g_c.bySlot[i];
}

const char* StatusLine(char* buf, size_t n) {
    static SRWLOCK l = SRWLOCK_INIT;
    static Stats prev;
    static LARGE_INTEGER prevT;
    if (!buf || !n) return buf;
    const State st = GetState();
    if (st != State::Active && st != State::Paused) {
        _snprintf_s(buf, n, _TRUNCATE, "%s", StateName(st));
        return buf;
    }
    Stats s;
    GetStats(s);
    LARGE_INTEGER now, f;
    QueryPerformanceCounter(&now);
    QueryPerformanceFrequency(&f);
    AcquireSRWLockExclusive(&l);
    const double secs = prevT.QuadPart ? (now.QuadPart - prevT.QuadPart) / (double)f.QuadPart : 0;
    const uint64_t fr = s.frames - prev.frames;
    const double div = fr ? (double)fr : (secs > 0 ? secs : 1);
    const char* unit = fr ? "frame" : "s";
    const double calls = (s.calls - prev.calls) / div, bars = (s.barriers - prev.barriers) / div;
    // ResourceBarrier calls that reach the driver: pass-through calls + gpuopt's batch emits
    const double out = ((s.passthroughCalls - prev.passthroughCalls) + (s.emitCalls - prev.emitCalls)) / div;
    const double drop = ((s.dupUavDropped - prev.dupUavDropped) + (s.undoPairsDropped - prev.undoPairsDropped)) / div;
    _snprintf_s(buf, n, _TRUNCATE,
                "%s: %.0f barriers/%s in %.0f calls -> %.0f calls (%+.0f %%), dropped %.1f/%s, undo pairs %.1f/%s, max "
                "batch %u",
                st == State::Active ? "on" : "off", bars, unit, calls, out, calls > 0 ? (out - calls) * 100.0 / calls : 0.0,
                drop, unit, (s.undoPairsSeen - prev.undoPairsSeen) / div, unit, s.maxBatch);
    if (g_r2a | g_r2b | g_r1 | (LONG)(s.r2a | s.r2b | s.r1Dropped | s.keptTexture | s.keptUav | s.keptUnknown | s.keptForeign)) {
        const size_t len = strlen(buf);
        _snprintf_s(buf + len, n - len, _TRUNCATE,
                    "; R2a %s %.1f/%s, R2b %s %.1f/%s, R1 %s %.1f/%s dropped; kept tex %.1f uav %.1f unknown %.1f foreign %.1f",
                    g_r2a ? "ON" : "off", (s.r2a - prev.r2a) / div, unit, g_r2b ? "ON" : "off", (s.r2b - prev.r2b) / div, unit,
                    g_r1 ? "ON" : "off", (s.r1Dropped - prev.r1Dropped) / div, unit, (s.keptTexture - prev.keptTexture) / div,
                    (s.keptUav - prev.keptUav) / div, (s.keptUnknown - prev.keptUnknown) / div,
                    (s.keptForeign - prev.keptForeign) / div);
    }
    prev = s;
    prevT = now;
    ReleaseSRWLockExclusive(&l);
    return buf;
}

}  // namespace gpuopt
