// fcap.h - FC: one-frame D3D12 command-stream capture ([Profiler] FrameCap = 0 = off). Included by gpuprof.cpp
// INSIDE namespace gp (it uses gp's g_log / g_base and gpuopt's command-list hooks); not a stand-alone header.
//
// Purpose (notes\renderer\00-map.md section 4): check the decompiled frame graph against what the GPU really receives, on
// RDNA2 (pc2), RDNA3 (Ally, vkd3d-proton) and RDNA4 (PC). One capture = every command-list call, barrier, ExecuteCommandLists,
// queue Signal / Wait / UpdateTileMappings and Present of FrameCap_Frames (default 2) consecutive presented frames, written to
// Nioh3EngineFix_fcap_<n>[_<tour tag>].csv next to the ini; digest with re\scripts_renderer\fcap_digest.py.
//
// Cost model: nothing while not capturing. FrameCap != 0 only makes gp ask gpuopt for its command-list hooks (pass-through, the
// GPUProf = 1 configuration) and psoswap for its PSO -> shader-set map. In a capture window every hookable list slot goes to
// GpuOptSlow (one 128-byte record per call, no lock), so those 2 frames run slower; the file is written by a background thread.
// Needs: [Profiler] GPUProf_Markers = 1 (dllmain verifies the GP code sites only when GPUProf or GPUProf_Markers is set; without
// it gp gets no command-list hooks and a capture holds queue rows only), [Profiler] GPUProf = 0 (FC owns the hook mask), [GpuOpt] Barriers = 0 (barriers pass through, one row per barrier),
// the OSD on (its ExecuteCommandLists hook feeds the ECL rows).
//
// ini [Profiler]:
//   FrameCap = 0             1 = on
//   FrameCap_Frames = 2      presents per capture window (1..8)
//   FrameCap_AtSec = 60      without a tour: first capture this many seconds after the first presented frame
//   FrameCap_EverySec = 0    without a tour: repeat period (0 = once)
//   FrameCap_TourDelay = 150 with a tour: frames after each dwell start (tour.cpp LogWindow(nullptr)) before the capture
//   FrameCap_Max = 12        captures per launch
//   FrameCap_MaxEvents = 400000   records per capture (128 B each, VirtualAlloc'ed per capture and freed after the write)
//
// Record layout per call: caller = return address of the D3D12 call as a Nioh3.exe RVA (the dispatcher's token handler; ffffffff =
// another module, e.g. sl.interposer / FFX), the three register arguments + up to three stack arguments (5th..7th, read from the
// thunk frame: regs[10..12]), and a per-slot decode (PSO shader set, marker text, RTV/DSV handles, clear colours, root constants,
// IB/VB views, copy locations). Resources met in barriers / copies / clears / ExecuteIndirect get one 'res' row with their desc.

namespace fc {

struct Ev {                 // 128 bytes
    uint64_t qpc, list;
    uint64_t a[6];          // a1..a3 = rdx, r8, r9; a[3..5] = stack args 5..7 (where the method has them)
    uint32_t ra, tid;
    uint16_t slot;
    uint8_t set, kind;      // set: low 4 bits = gpuopt vtable set, high 4 bits = D3D12_COMMAND_LIST_TYPE (queue type for ECL/sync)
    uint32_t x;             // PSO set id / flags / raw float bits
    uint64_t p[6];          // decoded payload (or 48 chars of marker text)
};
static_assert(sizeof(Ev) == 128, "Ev layout");
enum : uint8_t { kCall = 0, kBarrier, kEcl, kSignal, kWait, kPresent, kTileMap, kRes };
const char* const kKindName[] = {"call", "barrier", "ecl", "signal", "wait", "present", "tilemap", "res"};
const char* const kListSlot[86] = {
    "QueryInterface", "AddRef", "Release", "GetPrivateData", "SetPrivateData", "SetPrivateDataInterface", "SetName", "GetDevice",
    "GetType", "Close", "Reset", "ClearState", "DrawInstanced", "DrawIndexedInstanced", "Dispatch", "CopyBufferRegion",
    "CopyTextureRegion", "CopyResource", "CopyTiles", "ResolveSubresource", "IASetPrimitiveTopology", "RSSetViewports",
    "RSSetScissorRects", "OMSetBlendFactor", "OMSetStencilRef", "SetPipelineState", "ResourceBarrier", "ExecuteBundle",
    "SetDescriptorHeaps", "SetComputeRootSignature", "SetGraphicsRootSignature", "SetComputeRootDescriptorTable",
    "SetGraphicsRootDescriptorTable", "SetComputeRoot32BitConstant", "SetGraphicsRoot32BitConstant", "SetComputeRoot32BitConstants",
    "SetGraphicsRoot32BitConstants", "SetComputeRootConstantBufferView", "SetGraphicsRootConstantBufferView",
    "SetComputeRootShaderResourceView", "SetGraphicsRootShaderResourceView", "SetComputeRootUnorderedAccessView",
    "SetGraphicsRootUnorderedAccessView", "IASetIndexBuffer", "IASetVertexBuffers", "SOSetTargets", "OMSetRenderTargets",
    "ClearDepthStencilView", "ClearRenderTargetView", "ClearUnorderedAccessViewUint", "ClearUnorderedAccessViewFloat",
    "DiscardResource", "BeginQuery", "EndQuery", "ResolveQueryData", "SetPredication", "SetMarker", "BeginEvent", "EndEvent",
    "ExecuteIndirect", "AtomicCopyBufferUINT", "AtomicCopyBufferUINT64", "OMSetDepthBounds", "SetSamplePositions",
    "ResolveSubresourceRegion", "SetViewInstanceMask", "WriteBufferImmediate", "SetProtectedResourceSession", "BeginRenderPass",
    "EndRenderPass", "InitializeMetaCommand", "ExecuteMetaCommand", "BuildRaytracingAccelerationStructure",
    "EmitRaytracingAccelerationStructurePostbuildInfo", "CopyRaytracingAccelerationStructure", "SetPipelineState1", "DispatchRays",
    "RSSetShadingRate", "RSSetShadingRateImage", "DispatchMesh", "Barrier", "OMSetFrontAndBackStencilRef", "RSSetDepthBias",
    "IASetIndexBufferStripCutValue", "SetProgram", "DispatchGraph"};
// list slots with 5th+ arguments on the stack (read from the thunk frame)
bool HasStackArgs(int s) {
    switch (s) {
    case 12: case 13: case 15: case 16: case 19: case 36: case 35: case 46: case 47: case 49: case 50: case 54: case 59: case 64: case 67:
        return true;
    default:
        return false;
    }
}

struct Cfg { int on = 0, frames = 2, atSec = 60, everySec = 0, tourDelay = 150, max = 12, maxEvents = 400000; };
Cfg c;
std::wstring s_dir;
Ev* s_ev = nullptr;
volatile LONG s_n = 0, s_dropped = 0;
volatile LONG s_writing = 0;
volatile uint8_t* s_mask = nullptr;
uint8_t s_saved[96];
LONG64 s_frames = 0, s_armAt = -1, s_qpc0 = 0, s_capQpc0 = 0;
int s_framesLeft = 0, s_idx = 0, s_lastDwellIdx = -1;
bool s_tour = false, s_dwellCaptured = false;
uintptr_t s_lo = 0, s_hi = 0;
char s_tag[64] = {};
SRWLOCK s_tagLock = SRWLOCK_INIT;
std::wstring s_lastFile;

// resources seen in this capture (open addressing, CAS insert)
constexpr int kResTab = 8192;
volatile LONG64 s_res[kResTab];

uint32_t Rva(uintptr_t ra) { return (ra >= s_lo && ra < s_hi) ? (uint32_t)(ra - s_lo) : 0xFFFFFFFFu; }

Ev* NewEv(uint8_t kind) {
    const LONG i = InterlockedIncrement(&s_n) - 1;
    if (i >= c.maxEvents) {
        InterlockedIncrement(&s_dropped);
        return nullptr;
    }
    Ev* e = &s_ev[i];
    memset(e, 0, sizeof(Ev));
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    e->qpc = (uint64_t)t.QuadPart;
    e->tid = GetCurrentThreadId();
    e->kind = kind;
    return e;
}

void NoteRes(const void* r) {
    if (!r) return;
    const LONG64 v = (LONG64)(uintptr_t)r;
    uint32_t h = (uint32_t)((((uint64_t)v) >> 4) * 0x9E3779B1u) & (kResTab - 1);
    for (int k = 0; k < 64; ++k, h = (h + 1) & (kResTab - 1)) {
        const LONG64 cur = s_res[h];
        if (cur == v) return;
        if (cur == 0 && InterlockedCompareExchange64(&s_res[h], v, 0) == 0) {
            const D3D12_RESOURCE_DESC d = ((ID3D12Resource*)r)->GetDesc();
            if (Ev* e = NewEv(kRes)) {
                e->a[0] = (uint64_t)v;
                e->a[1] = (uint64_t)d.Dimension;
                e->a[2] = d.Width;
                e->a[3] = d.Height;
                e->a[4] = (uint64_t)d.DepthOrArraySize | ((uint64_t)d.MipLevels << 16) | ((uint64_t)d.SampleDesc.Count << 32);
                e->a[5] = (uint64_t)d.Format | ((uint64_t)d.Flags << 32);
                e->p[0] = ((ID3D12Resource*)r)->GetGPUVirtualAddress();
            }
            return;
        }
        if (cur == v) return;
    }
}

void CopyText(Ev* e, const void* data, uint32_t size) {
    char* o = (char*)e->p;
    if (!data || !size) return;
    const uint8_t* b = (const uint8_t*)data;
    const bool wide = size >= 4 && b[1] == 0;
    size_t k = 0;
    if (wide) {
        const wchar_t* w = (const wchar_t*)data;
        for (uint32_t i = 0; i < size / 2 && k < 47 && w[i]; ++i) o[k++] = (w[i] >= 32 && w[i] < 127 && w[i] != ',') ? (char)w[i] : '_';
    } else {
        for (uint32_t i = 0; i < size && k < 47 && b[i]; ++i) o[k++] = (b[i] >= 32 && b[i] < 127 && b[i] != ',') ? (char)b[i] : '_';
    }
    o[k] = 0;
}

void ListCall(void* list, int set, int slot, uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t* regs) {
    if (slot < 0 || slot >= 86) return;
    Ev* e = NewEv(kCall);
    if (!e) return;
    e->list = (uint64_t)list;
    e->slot = (uint16_t)slot;
    const int lt = (int)((ID3D12CommandList*)list)->GetType();
    e->set = (uint8_t)((set & 15) | ((lt & 15) << 4));
    e->a[0] = a1, e->a[1] = a2, e->a[2] = a3;
    if (regs) {
        e->ra = Rva(regs[5]);   // thunk frame: regs[4] = return into the thunk, regs[5] = the caller's return address
        if (HasStackArgs(slot)) e->a[3] = regs[10], e->a[4] = regs[11], e->a[5] = regs[12];
    }
    switch (slot) {
    case 10: e->x = a2 ? psoswap::PsoSetId((const void*)a2) : 0; break;
    case 25: e->x = psoswap::PsoSetId((const void*)a1); break;
    case 15: NoteRes((void*)a1); NoteRes((void*)a3); break;
    case 16: {
        const auto* d = (const D3D12_TEXTURE_COPY_LOCATION*)a1;
        const auto* s = (const D3D12_TEXTURE_COPY_LOCATION*)(regs ? regs[11] : 0);
        if (d) e->p[0] = (uint64_t)d->pResource, e->p[1] = d->Type == D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX ? d->SubresourceIndex : 0xFFFFFFFFu, NoteRes(d->pResource);
        if (s) e->p[2] = (uint64_t)s->pResource, e->p[3] = s->Type == D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX ? s->SubresourceIndex : 0xFFFFFFFFu, NoteRes(s->pResource);
        break;
    }
    case 17: NoteRes((void*)a1); NoteRes((void*)a2); break;
    case 19: NoteRes((void*)a1); NoteRes((void*)a3); break;
    case 64: NoteRes((void*)a1); break;
    case 21: if (a1 && a2) memcpy(e->p, (const void*)a2, sizeof(D3D12_VIEWPORT)); break;   // first viewport (24 bytes)
    case 22: if (a1 && a2) memcpy(e->p, (const void*)a2, sizeof(D3D12_RECT)); break;
    case 28: if (a2) for (uint32_t i = 0; i < (uint32_t)a1 && i < 2; ++i) e->p[i] = (uint64_t)((void* const*)a2)[i]; break;
    case 35: case 36: if (a3) memcpy(e->p, (const void*)a3, (a2 < 12 ? (size_t)a2 : 12) * 4); break;
    case 43: if (a1) { const auto* v = (const D3D12_INDEX_BUFFER_VIEW*)a1; e->p[0] = v->BufferLocation, e->p[1] = v->SizeInBytes | ((uint64_t)v->Format << 32); } break;
    case 44: if (a3 && a2) { const auto* v = (const D3D12_VERTEX_BUFFER_VIEW*)a3; e->p[0] = v->BufferLocation, e->p[1] = v->SizeInBytes | ((uint64_t)v->StrideInBytes << 32); } break;
    case 46: {
        const auto* h = (const D3D12_CPU_DESCRIPTOR_HANDLE*)a2;
        if (h) for (uint32_t i = 0; i < (uint32_t)a1 && i < 5; ++i) e->p[i] = (a3 & 0xFF) ? h[0].ptr : h[i].ptr;   // single-handle form: base only
        const auto* d = (const D3D12_CPU_DESCRIPTOR_HANDLE*)(regs ? regs[10] : 0);
        e->p[5] = d ? d->ptr : 0;
        break;
    }
    case 47: if (regs) memcpy(&e->x, (const char*)regs - 0x20, 4); break;   // depth = saved xmm3 ([rsp+60h] of the slow frame)
    case 48: if (a2) memcpy(e->p, (const void*)a2, 16); break;
    case 49: case 50: NoteRes((void*)a3); if (regs && regs[10]) memcpy(e->p, (const void*)regs[10], 16); break;
    case 51: NoteRes((void*)a1); break;
    case 56: case 57: CopyText(e, (const void*)a2, (uint32_t)a3); break;
    case 59: NoteRes((void*)a3); if (regs) NoteRes((void*)regs[11]); break;
    case 66: if (a1 && a2) { const auto* w = (const D3D12_WRITEBUFFERIMMEDIATE_PARAMETER*)a2; e->p[0] = w->Dest, e->p[1] = w->Value; } break;
    default: break;
    }
}

void Barrier(void* list, int set, unsigned n, const void* bp, void* ra) {
    const auto* b = (const D3D12_RESOURCE_BARRIER*)bp;
    const int lt = list ? (int)((ID3D12CommandList*)list)->GetType() : 0;
    for (unsigned i = 0; i < n && i < 64 && b; ++i) {
        Ev* e = NewEv(kBarrier);
        if (!e) return;
        e->list = (uint64_t)list;
        e->slot = 26;
        e->set = (uint8_t)((set & 15) | ((lt & 15) << 4));
        e->ra = Rva((uintptr_t)ra);
        e->x = i | (n << 16);
        e->a[0] = (uint64_t)b[i].Type;
        e->a[1] = (uint64_t)b[i].Flags;
        if (b[i].Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION) {
            e->a[2] = (uint64_t)b[i].Transition.pResource;
            e->a[3] = b[i].Transition.Subresource;
            e->a[4] = (uint64_t)b[i].Transition.StateBefore;
            e->a[5] = (uint64_t)b[i].Transition.StateAfter;
            NoteRes(b[i].Transition.pResource);
        } else if (b[i].Type == D3D12_RESOURCE_BARRIER_TYPE_UAV) {
            e->a[2] = (uint64_t)b[i].UAV.pResource;
            NoteRes(b[i].UAV.pResource);
        } else {
            e->a[2] = (uint64_t)b[i].Aliasing.pResourceBefore;
            e->a[3] = (uint64_t)b[i].Aliasing.pResourceAfter;
        }
    }
}

// ---- queue side: ECL (from osd's hook via gp::OnEcl) + Signal / Wait / UpdateTileMappings (patched here) --------------
struct QType { void* q; int t; };
QType s_q[16];
SRWLOCK s_qLock = SRWLOCK_INIT;
int QueueType(void* q) {
    AcquireSRWLockShared(&s_qLock);
    for (const QType& e : s_q)
        if (e.q == q) {
            ReleaseSRWLockShared(&s_qLock);
            return e.t;
        }
    ReleaseSRWLockShared(&s_qLock);
    const int t = (int)((ID3D12CommandQueue*)q)->GetDesc().Type;
    AcquireSRWLockExclusive(&s_qLock);
    for (QType& e : s_q)
        if (!e.q) {
            e.q = q, e.t = t;
            break;
        }
    ReleaseSRWLockExclusive(&s_qLock);
    return t;
}
using SigFn = HRESULT(STDMETHODCALLTYPE*)(ID3D12CommandQueue*, ID3D12Fence*, UINT64);
using UtmFn = void(STDMETHODCALLTYPE*)(ID3D12CommandQueue*, ID3D12Resource*, UINT, const D3D12_TILED_RESOURCE_COORDINATE*,
                                       const D3D12_TILE_REGION_SIZE*, ID3D12Heap*, UINT, const D3D12_TILE_RANGE_FLAGS*, const UINT*,
                                       const UINT*, D3D12_TILE_MAPPING_FLAGS);
SigFn s_origSignal = nullptr, s_origWait = nullptr;
UtmFn s_origUtm = nullptr;
volatile LONG s_capOn = 0;   // mirror of g_fcCap for the queue hooks
void QueueEv(uint8_t kind, ID3D12CommandQueue* q, void* a, uint64_t b, void* ra) {
    __try {
        if (Ev* e = NewEv(kind)) {
            e->set = (uint8_t)(QueueType(q) << 4);
            e->a[0] = (uint64_t)q, e->a[1] = (uint64_t)a, e->a[2] = b;
            e->ra = Rva((uintptr_t)ra);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}
HRESULT STDMETHODCALLTYPE HkSignal(ID3D12CommandQueue* q, ID3D12Fence* f, UINT64 v) {
    if (s_capOn) QueueEv(kSignal, q, f, v, _ReturnAddress());
    return s_origSignal(q, f, v);
}
HRESULT STDMETHODCALLTYPE HkWait(ID3D12CommandQueue* q, ID3D12Fence* f, UINT64 v) {
    if (s_capOn) QueueEv(kWait, q, f, v, _ReturnAddress());
    return s_origWait(q, f, v);
}
void STDMETHODCALLTYPE HkUtm(ID3D12CommandQueue* q, ID3D12Resource* r, UINT nReg, const D3D12_TILED_RESOURCE_COORDINATE* co,
                             const D3D12_TILE_REGION_SIZE* sz, ID3D12Heap* h, UINT nRange, const D3D12_TILE_RANGE_FLAGS* fl,
                             const UINT* st, const UINT* cnt, D3D12_TILE_MAPPING_FLAGS mf) {
    if (s_capOn) QueueEv(kTileMap, q, r, ((uint64_t)nReg) | ((uint64_t)nRange << 32), _ReturnAddress());
    s_origUtm(q, r, nReg, co, sz, h, nRange, fl, st, cnt, mf);
}
bool PatchQ(void** slot, void* hook, void** orig) {
    DWORD old = 0;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_EXECUTE_READWRITE, &old)) return false;
    *orig = *slot;
    InterlockedExchangePointer(slot, hook);
    VirtualProtect(slot, sizeof(void*), old, &old);
    return true;
}
volatile LONG s_qPatched = 0;
void PatchQueue(void* q) {
    if (InterlockedExchange(&s_qPatched, 1)) return;
    void** vt = *(void***)q;
    const bool ok = PatchQ(&vt[14], (void*)&HkSignal, (void**)&s_origSignal) && PatchQ(&vt[15], (void*)&HkWait, (void**)&s_origWait) &&
                    PatchQ(&vt[8], (void*)&HkUtm, (void**)&s_origUtm);
    GLOG("FC: queue vtable %p: Signal / Wait / UpdateTileMappings hooks %s", (void*)vt, ok ? "installed" : "FAILED (no sync rows)");
}

void Ecl(void* q, unsigned n, void* const* lists, void* ra) {
    PatchQueue(q);
    if (!s_capOn) return;
    const int t = QueueType(q);
    for (unsigned i = 0; i < n && lists; ++i)
        if (Ev* e = NewEv(kEcl)) {
            e->set = (uint8_t)(t << 4);
            e->list = (uint64_t)lists[i];
            e->a[0] = (uint64_t)q, e->a[1] = i, e->a[2] = n;
            e->ra = Rva((uintptr_t)ra);
        }
}

// ---- capture window control --------------------------------------------------------------------------------------------
const int kArmSlots[] = {9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36,
                         37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
                         64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85};

DWORD WINAPI WriterThread(void*);

void StartCapture() {
    if (!s_ev) s_ev = (Ev*)VirtualAlloc(nullptr, (size_t)c.maxEvents * sizeof(Ev), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!s_ev) {
        GLOG("FC: could not allocate %d records - capture %d skipped", c.maxEvents, s_idx + 1);
        return;
    }
    memset((void*)s_res, 0, sizeof(s_res));
    InterlockedExchange(&s_n, 0);
    InterlockedExchange(&s_dropped, 0);
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    s_capQpc0 = t.QuadPart;
    ++s_idx;
    s_framesLeft = c.frames;
    if (s_mask)
        for (int v : kArmSlots) s_saved[v] = s_mask[v], s_mask[v] = 1;
    InterlockedExchange(&s_capOn, 1);
    InterlockedExchange(&g_fcCap, 1);
    if (Ev* e = NewEv(kPresent)) e->a[0] = (uint64_t)s_frames;
}

void StopCapture() {
    if (Ev* e = NewEv(kPresent)) e->a[0] = (uint64_t)s_frames;
    InterlockedExchange(&g_fcCap, 0);
    InterlockedExchange(&s_capOn, 0);
    if (s_mask)
        for (int v : kArmSlots) s_mask[v] = s_saved[v];
    InterlockedExchange(&s_writing, 1);
    if (HANDLE h = CreateThread(nullptr, 0, WriterThread, (void*)(intptr_t)s_idx, 0, nullptr)) CloseHandle(h);
    else InterlockedExchange(&s_writing, 0);
}

void OnFrame() {   // osd Present hook (gp::OnFrame), once per presented frame
    if (!c.on) return;
    ++s_frames;
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    if (!s_qpc0) s_qpc0 = t.QuadPart;
    if (g_fcCap) {
        if (--s_framesLeft <= 0) StopCapture();
        else if (Ev* e = NewEv(kPresent)) e->a[0] = (uint64_t)s_frames;
        return;
    }
    if (s_writing || s_idx >= c.max) return;
    if (s_tour) {
        if (s_armAt >= 0 && s_frames >= s_armAt) {
            s_armAt = -1;
            s_lastDwellIdx = s_idx + 1;
            StartCapture();
        }
        return;
    }
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    const double sec = (double)(t.QuadPart - s_qpc0) / (double)f.QuadPart;
    const double due = c.atSec + (double)(s_idx ? c.everySec * s_idx : 0);
    if (sec >= due && (s_idx == 0 || c.everySec > 0)) StartCapture();
}

void TourDwell() {   // tour dwell start
    if (!c.on) return;
    s_tour = true;
    if (!g_fcCap && s_idx < c.max) s_armAt = s_frames + c.tourDelay;
}
void TourTag(const char* tag) {   // tour dwell end: name the capture taken in this dwell
    if (!c.on || !tag) return;
    s_tour = true;
    s_armAt = -1;
    if (s_lastDwellIdx <= 0) return;
    AcquireSRWLockExclusive(&s_tagLock);
    strncpy_s(s_tag, tag, _TRUNCATE);
    for (char* p = s_tag; *p; ++p)
        if (!isalnum((unsigned char)*p) && *p != '-') *p = '_';
    const int idx = s_lastDwellIdx;
    s_lastDwellIdx = -1;
    std::wstring from = s_lastFile;
    ReleaseSRWLockExclusive(&s_tagLock);
    GLOG("FC: capture %d = tour %s", idx, tag);
    // file already written: rename it (otherwise the writer picks the tag up)
    if (!s_writing && !from.empty()) {
        wchar_t to[MAX_PATH];
        swprintf_s(to, L"%sNioh3EngineFix_fcap_%d_%hs.csv", s_dir.c_str(), idx, s_tag);
        MoveFileExW(from.c_str(), to, MOVEFILE_REPLACE_EXISTING);
        AcquireSRWLockExclusive(&s_tagLock);
        s_tag[0] = 0;
        s_lastFile.clear();
        ReleaseSRWLockExclusive(&s_tagLock);
    }
}

DWORD WINAPI WriterThread(void* arg) {
    const int idx = (int)(intptr_t)arg;
    Sleep(200);   // hooks that were mid-record when the window closed finish their row
    const LONG n = s_n < c.maxEvents ? s_n : c.maxEvents;
    wchar_t path[MAX_PATH];
    AcquireSRWLockExclusive(&s_tagLock);
    if (s_tag[0]) swprintf_s(path, L"%sNioh3EngineFix_fcap_%d_%hs.csv", s_dir.c_str(), idx, s_tag), s_tag[0] = 0;
    else swprintf_s(path, L"%sNioh3EngineFix_fcap_%d.csv", s_dir.c_str(), idx);
    ReleaseSRWLockExclusive(&s_tagLock);
    FILE* f = nullptr;
    if (_wfopen_s(&f, path, L"wb") || !f) {
        GLOG("FC: cannot write %ls", path);
        InterlockedExchange(&s_writing, 0);
        return 0;
    }
    std::vector<char> buf(1 << 20);
    setvbuf(f, buf.data(), _IOFBF, buf.size());
    LARGE_INTEGER fq;
    QueryPerformanceFrequency(&fq);
    fprintf(f, "# FC frame capture %d (Nioh3EngineFix): %ld records, %ld dropped, %d presented frames, exe base %p\n", idx, n,
            s_dropped, c.frames, (void*)s_lo);
    fprintf(f, "# kind: call = command-list method (slot), barrier = one D3D12_RESOURCE_BARRIER, ecl = one list of an ExecuteCommandLists, "
               "signal/wait = queue fence op, present = frame boundary, tilemap = UpdateTileMappings, res = resource desc (first sight)\n");
    fprintf(f, "# ltype: D3D12_COMMAND_LIST_TYPE of the list (call/barrier) or queue (ecl/signal/wait/tilemap): 0 DIRECT, 2 COMPUTE, 3 COPY. "
               "ra = caller RVA in Nioh3.exe (ffffffff = other module). a0..a2 = rdx/r8/r9, a3..a5 = stack args 5-7\n");
    fprintf(f, "seq,t_us,tid,kind,ltype,set,list,slot,call,ra,a0,a1,a2,a3,a4,a5,x,info\n");
    char info[256];
    for (LONG i = 0; i < n; ++i) {
        const Ev& e = s_ev[i];
        info[0] = 0;
        if (e.kind == kCall && (e.slot == 25 || e.slot == 10) && e.x) {
            psoswap::DescribeSet(e.x, info, sizeof(info));
        } else if (e.kind == kCall && (e.slot == 56 || e.slot == 57)) {
            memcpy(info, e.p, 47), info[47] = 0;
        } else if (e.kind == kCall && e.slot == 46) {
            snprintf(info, sizeof(info), "rtv %llx %llx %llx %llx %llx dsv %llx", e.p[0], e.p[1], e.p[2], e.p[3], e.p[4], e.p[5]);
        } else if (e.kind == kCall && (e.slot == 48 || e.slot == 50 || e.slot == 21)) {
            const float* v = (const float*)e.p;
            snprintf(info, sizeof(info), "%g %g %g %g %g %g", v[0], v[1], v[2], v[3], v[4], v[5]);
        } else if (e.kind == kCall && e.slot == 47) {
            float d;
            memcpy(&d, &e.x, 4);
            snprintf(info, sizeof(info), "depth %g", d);
        } else if (e.kind == kCall && (e.slot == 35 || e.slot == 36)) {
            const uint32_t* v = (const uint32_t*)e.p;
            snprintf(info, sizeof(info), "%x %x %x %x %x %x %x %x %x %x %x %x", v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[10], v[11]);
        } else if (e.p[0] || e.p[1] || e.p[2] || e.p[3]) {
            snprintf(info, sizeof(info), "%llx %llx %llx %llx %llx %llx", e.p[0], e.p[1], e.p[2], e.p[3], e.p[4], e.p[5]);
        }
        const double us = (double)((LONG64)e.qpc - s_capQpc0) * 1e6 / (double)fq.QuadPart;
        fprintf(f, "%ld,%.1f,%u,%s,%u,%u,%llx,%u,%s,%x,%llx,%llx,%llx,%llx,%llx,%llx,%x,%s\n", i, us, e.tid, e.kind < 8 ? kKindName[e.kind] : "?",
                (unsigned)(e.set >> 4), (unsigned)(e.set & 15), e.list, e.slot, e.kind == kCall || e.kind == kBarrier ? kListSlot[e.slot < 86 ? e.slot : 0] : "-", e.ra,
                e.a[0], e.a[1], e.a[2], e.a[3], e.a[4], e.a[5], e.x, info);
    }
    fclose(f);
    LONG calls = 0, barriers = 0, ecl = 0, draws = 0, disp = 0;
    for (LONG i = 0; i < n; ++i) {
        const Ev& e = s_ev[i];
        if (e.kind == kCall) {
            ++calls;
            if (e.slot == 12 || e.slot == 13) ++draws;
            if (e.slot == 14 || e.slot == 59 || e.slot == 76) ++disp;
        } else if (e.kind == kBarrier) ++barriers;
        else if (e.kind == kEcl) ++ecl;
    }
    VirtualFree(s_ev, 0, MEM_RELEASE);
    s_ev = nullptr;
    AcquireSRWLockExclusive(&s_tagLock);
    s_lastFile = path;
    ReleaseSRWLockExclusive(&s_tagLock);
    GLOG("FC: capture %d written: %ls (%ld records: %ld list calls, %ld draws, %ld dispatch/EI, %ld barriers, %ld ECL lists; %ld dropped)", idx, path,
         n, calls, draws, disp, barriers, ecl, s_dropped);
    InterlockedExchange(&s_writing, 0);
    return 0;
}

void Start(const wchar_t* ini, int prof) {
    auto I = [&](const wchar_t* k, int def, int lo, int hi) {
        const int v = (int)GetPrivateProfileIntW(L"Profiler", k, def, ini);
        return v < lo ? lo : v > hi ? hi : v;
    };
    c.on = I(L"FrameCap", 0, 0, 1);
    if (!c.on) return;
    c.frames = I(L"FrameCap_Frames", 2, 1, 8);
    c.atSec = I(L"FrameCap_AtSec", 60, 1, 36000);
    c.everySec = I(L"FrameCap_EverySec", 0, 0, 36000);
    c.tourDelay = I(L"FrameCap_TourDelay", 150, 1, 100000);
    c.max = I(L"FrameCap_Max", 12, 1, 200);
    c.maxEvents = I(L"FrameCap_MaxEvents", 400000, 10000, 4000000);
    if (prof) {
        GLOG("%s", "FC: [Profiler] FrameCap needs GPUProf = 0 (GP2 sampling owns the hook mask) - FC off");
        c.on = 0;
        return;
    }
    std::wstring d(ini);
    const size_t sl = d.find_last_of(L"\\/");
    s_dir = sl == std::wstring::npos ? std::wstring() : d.substr(0, sl + 1);
    if (g_base) {
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(g_base);
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(g_base + dos->e_lfanew);
        s_lo = (uintptr_t)g_base;
        s_hi = s_lo + nt->OptionalHeader.SizeOfImage;
    }
    InterlockedExchange(&g_fcEcl, 1);
    GLOG("FC: frame capture on - %d presented frame(s) per capture, max %d captures, %d records each; tour dwells: %d frames after the dwell "
         "starts, else at %d s%s. Needs [GpuOpt] Barriers = 0 and the OSD (ECL rows). Files: %lsNioh3EngineFix_fcap_<n>.csv",
         c.frames, c.max, c.maxEvents, c.tourDelay, c.atSec, c.everySec ? " and then periodically" : " (once)", s_dir.c_str());
}

}  // namespace fc
