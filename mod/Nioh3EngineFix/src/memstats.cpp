// memstats.cpp - process / GPU memory line for the 30 s stats thread (see memstats.h). Measurement only.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>

#include <d3d12.h>
#include <dxgi1_4.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>

#include "memstats.h"

namespace memstats {
namespace {

void (*g_log)(const char*, ...) = nullptr;
#define MLOG(...)                      \
    do {                               \
        if (g_log) g_log(__VA_ARGS__); \
    } while (0)

bool g_on = false;
int g_level = 0;  // [Profiler] MemStats: 1 = 30 s line, 2 = + 1 s 'stats vs:' line (VS probe, 1.7.48)
uintptr_t g_base = 0;
bool g_known = false;
void* (*g_getDev)() = nullptr;

// v2.02 engine layout (notes\memory-leaks.md §5): g_RenderGlobals (GfxDevice*) at RVA 45BA588, ID3D12Device* at
// GfxDevice+4268h (RecreateSwapchainDevice 2EEA690 calls CreateCommandAllocator / CreateQueryHeap through it).
constexpr uintptr_t kRvaGfxDevice = 0x45BA588;
constexpr uintptr_t kOffD3D12Device = 0x4268;

IDXGIAdapter3* g_adapter = nullptr;  // kept for the whole session
bool g_adapterByLuid = false;
int g_adapterTries = 0;
bool g_adapterGaveUp = false;

struct Sample {
    double t = 0;  // seconds
    double priv = 0, ws = 0, commit = 0;
    double handles = 0, threads = 0;
    double vl = -1, vn = -1;  // VRAM local / non-local used, MB (-1 = unknown)
};
bool g_haveFirst = false;
Sample g_first;
LARGE_INTEGER g_qpf{};

constexpr double kMB = 1024.0 * 1024.0;

// ---- the game's D3D12 device -> adapter LUID ------------------------------------------------------------------------
// SEH only (no C++ objects here): the engine pointer chain is read while the game may be tearing the device down.
bool ReadEnginePtr(uintptr_t base, void** out) {
    __try {
        const uintptr_t g = *reinterpret_cast<volatile uintptr_t*>(base + kRvaGfxDevice);
        if (!g) return false;
        *out = *reinterpret_cast<void* volatile*>(g + kOffD3D12Device);
        return *out != nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool LuidFromDevice(void* raw, LUID* luid) {
    __try {
        ID3D12Device* d = nullptr;
        if (FAILED(static_cast<IUnknown*>(raw)->QueryInterface(__uuidof(ID3D12Device), reinterpret_cast<void**>(&d))) || !d)
            return false;
        *luid = d->GetAdapterLuid();
        d->Release();
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool GameDeviceLuid(LUID* luid) {
    void* dev = nullptr;
    if (g_getDev) dev = g_getDev();
    if (!dev && g_known && g_base) ReadEnginePtr(g_base, &dev);
    return dev && LuidFromDevice(dev, luid);
}

// CreateDXGIFactory1 from the SYSTEM dxgi.dll (full path): skips proxies in the game folder (OptiScaler's dxgi.dll,
// Streamline's sl.interposer) so no wrapper sees our factory. Under Proton system32\dxgi.dll is DXVK's dxgi.
using CreateFactory1Fn = HRESULT(WINAPI*)(REFIID, void**);
CreateFactory1Fn SystemCreateFactory1() {
    wchar_t path[MAX_PATH];
    const UINT n = GetSystemDirectoryW(path, MAX_PATH);
    if (!n || n + 10 >= MAX_PATH) return nullptr;
    wcscat_s(path, L"\\dxgi.dll");
    HMODULE m = GetModuleHandleW(path);
    if (!m) m = LoadLibraryW(path);  // kept loaded: the adapter lives for the session
    return m ? reinterpret_cast<CreateFactory1Fn>(GetProcAddress(m, "CreateDXGIFactory1")) : nullptr;
}

void LogAdapter(IDXGIAdapter3* a, const char* how) {
    DXGI_ADAPTER_DESC1 d{};
    if (FAILED(a->GetDesc1(&d))) return;
    char name[128];
    WideCharToMultiByte(CP_UTF8, 0, d.Description, -1, name, sizeof(name), nullptr, nullptr);
    MLOG("MEM: VRAM source: %s (%s), LUID %08lX:%08lX, dedicated %.0f MB, shared %.0f MB", name, how,
         (unsigned long)d.AdapterLuid.HighPart, (unsigned long)d.AdapterLuid.LowPart, (double)d.DedicatedVideoMemory / kMB,
         (double)d.SharedSystemMemory / kMB);
}

// Finds the adapter once. By LUID while the game's device is not known yet; after 10 lines (5 min) without a device it
// settles for the first hardware adapter and says so.
void EnsureAdapter() {
    if (g_adapter || g_adapterGaveUp) return;
    ++g_adapterTries;
    LUID luid{};
    const bool haveLuid = GameDeviceLuid(&luid);
    if (!haveLuid && g_adapterTries < 10) return;
    CreateFactory1Fn create = SystemCreateFactory1();
    if (!create) {
        MLOG("MEM: system dxgi.dll / CreateDXGIFactory1 not available - VRAM not logged");
        g_adapterGaveUp = true;
        return;
    }
    if (haveLuid) {
        IDXGIFactory4* f4 = nullptr;
        if (SUCCEEDED(create(__uuidof(IDXGIFactory4), reinterpret_cast<void**>(&f4))) && f4) {
            IDXGIAdapter3* a = nullptr;
            if (SUCCEEDED(f4->EnumAdapterByLuid(luid, __uuidof(IDXGIAdapter3), reinterpret_cast<void**>(&a))) && a) {
                g_adapter = a;
                g_adapterByLuid = true;
            }
            f4->Release();
        }
    }
    if (!g_adapter) {  // no IDXGIFactory4, LUID not enumerable (Proton LUID mismatch) or no device: adapter match / first hw
        IDXGIFactory1* f1 = nullptr;
        if (SUCCEEDED(create(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&f1))) && f1) {
            IDXGIAdapter1* pick = nullptr;
            IDXGIAdapter1* a1 = nullptr;
            for (UINT i = 0; !pick && f1->EnumAdapters1(i, &a1) != DXGI_ERROR_NOT_FOUND; ++i) {
                DXGI_ADAPTER_DESC1 d{};
                const bool ok = SUCCEEDED(a1->GetDesc1(&d)) && !(d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE);
                const bool match = haveLuid && d.AdapterLuid.LowPart == luid.LowPart && d.AdapterLuid.HighPart == luid.HighPart;
                if (ok && (match || !haveLuid)) pick = a1;
                else a1->Release();
            }
            if (!pick && haveLuid) {  // LUID never matched: first hardware adapter
                for (UINT i = 0; !pick && f1->EnumAdapters1(i, &a1) != DXGI_ERROR_NOT_FOUND; ++i) {
                    DXGI_ADAPTER_DESC1 d{};
                    if (SUCCEEDED(a1->GetDesc1(&d)) && !(d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) pick = a1;
                    else a1->Release();
                }
            }
            if (pick) {
                DXGI_ADAPTER_DESC1 d{};
                pick->GetDesc1(&d);
                g_adapterByLuid = haveLuid && d.AdapterLuid.LowPart == luid.LowPart && d.AdapterLuid.HighPart == luid.HighPart;
                IDXGIAdapter3* a3 = nullptr;
                if (SUCCEEDED(pick->QueryInterface(__uuidof(IDXGIAdapter3), reinterpret_cast<void**>(&a3))) && a3) g_adapter = a3;
                pick->Release();
            }
            f1->Release();
        }
    }
    if (g_adapter) {
        LogAdapter(g_adapter, g_adapterByLuid ? "the game's device, by LUID"
                                              : (haveLuid ? "LUID not found - first hardware adapter"
                                                          : "game device not found - first hardware adapter"));
    } else {
        MLOG("MEM: no IDXGIAdapter3 for the game's GPU - VRAM not logged");
        g_adapterGaveUp = true;
    }
}

bool QueryVram(DXGI_MEMORY_SEGMENT_GROUP g, double* used, double* budget) {
    if (!g_adapter) return false;
    DXGI_QUERY_VIDEO_MEMORY_INFO i{};
    if (FAILED(g_adapter->QueryVideoMemoryInfo(0, g, &i))) return false;
    *used = (double)i.CurrentUsage / kMB;
    *budget = (double)i.Budget / kMB;
    return true;
}

int CountThreads() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return -1;
    const DWORD pid = GetCurrentProcessId();
    int n = 0;
    THREADENTRY32 te{};
    te.dwSize = sizeof(te);
    for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te))
        if (te.th32OwnerProcessID == pid) ++n;
    CloseHandle(snap);
    return n;
}

// ---- 1.7.48 VS probe (MemStats = 2): 1 s VRAM / texture page pool / residency line (notes\root-cause-vram-streaming.md) ---
// Reads only. Engine fields (v2.02):
//   pool object P = [[[[base+4759910]+530h]+F8h]+70h] (PagePoolOccupancyCheck 126C64's own path; compared with
//   [base+45BA588]); size in 64 KB pages = (P+40ECh - P+40F0h) x P+40CCh; used = P+6CD0h; reserved = max(+756Ch,+7570h) +
//   +7584h + +757Ch + +7578h; the engine's controller tests (used - reserved) x 100 / size > 85. Tile-request pool:
//   free = +7560h - (reserved + this frame's +7590h), per-frame cap +75A8h. Page-pool stats of the last scanned frame
//   (PagePoolFrameUpdate copies P+61A0h+8F8h -> +A10h): touches +A18h, queue-full +A24h, hits +A30h, misses +A34h.
// D3D12: the engine device's vtable (GfxDevice+4268h) slots 27 CreateCommittedResource, 28 CreateHeap,
//   30 CreateReservedResource, 34 MakeResident, 35 Evict are counted (calls, objects, bytes per memory pool). Installed when
//   the device is first readable, so boot-time allocations made before that are not in the totals.
namespace vs {
constexpr uintptr_t kRvaPoolRoot = 0x4759910;
struct PoolRead {
    uintptr_t p = 0, gdev = 0;
    uint32_t sets = 0, setsLo = 0, ways = 0, used = 0, r756c = 0, r7570 = 0, r7584 = 0, r757c = 0, r7578 = 0;
    uint32_t cap = 0, frameReq = 0, frameCap = 0;
    uint32_t touches = 0, qfull = 0, hits = 0, misses = 0;
};
uint32_t U32(uintptr_t a) { return *reinterpret_cast<volatile uint32_t*>(a); }
uintptr_t Ptr(uintptr_t a) { return *reinterpret_cast<volatile uintptr_t*>(a); }
bool ReadPool(uintptr_t base, PoolRead* r) {
    __try {
        uintptr_t a = Ptr(base + kRvaPoolRoot);
        if (!a || !(a = Ptr(a + 0x530)) || !(a = Ptr(a + 0xF8))) return false;
        const uintptr_t p = Ptr(a + 0x70);
        if (!p) return false;
        r->p = p;
        r->gdev = Ptr(base + kRvaGfxDevice);
        r->sets = U32(p + 0x40EC); r->setsLo = U32(p + 0x40F0); r->ways = U32(p + 0x40CC); r->used = U32(p + 0x6CD0);
        r->r756c = U32(p + 0x756C); r->r7570 = U32(p + 0x7570); r->r7584 = U32(p + 0x7584);
        r->r757c = U32(p + 0x757C); r->r7578 = U32(p + 0x7578);
        r->cap = U32(p + 0x7560); r->frameReq = U32(p + 0x7590); r->frameCap = U32(p + 0x75A8);
        r->touches = U32(p + 0x61A0 + 0xA18); r->qfull = U32(p + 0x61A0 + 0xA24);
        r->hits = U32(p + 0x61A0 + 0xA30); r->misses = U32(p + 0x61A0 + 0xA34);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// counters: hook threads add, the stats thread reads and diffs
volatile LONG64 c_mrCalls = 0, c_mrObjs = 0, c_evCalls = 0, c_evObjs = 0;
volatile LONG64 c_heapN = 0, c_heapLocal = 0, c_heapSys = 0;  // bytes
volatile LONG64 c_comN = 0, c_comLocal = 0, c_comSys = 0;      // bytes
volatile LONG64 c_resvN = 0, c_plN = 0, c_plSysN = 0, c_plSys = 0;  // placed resources (bytes = resource size, inside heaps)
volatile LONG64 c_forced = 0;                                       // VSForceSysUpload conversions
bool g_hooked = false, g_hookTried = false;
int g_forceSys = 0;  // [Profiler] VSForceSysUpload (PC A/B only): UPLOAD heaps -> CUSTOM write-combine L0 (= system memory)

// per-caller system-memory allocation table (heaps, committed, placed-into-sys-heap): who owns the non-local bytes.
// Key = return addresses of the hooked call and of its caller (RVAs in Nioh3.exe, 0xFFFFFFFF = outside the exe).
struct CallerRow { uint32_t ra0, ra1; char kind; LONG64 n, bytes, maxOne; };
constexpr int kRows = 96;
CallerRow g_rows[kRows];
volatile LONG g_nRows = 0;
SRWLOCK g_rowLock = SRWLOCK_INIT;
uint32_t ToRva(void* a) {
    const uintptr_t x = reinterpret_cast<uintptr_t>(a);
    return (g_base && x >= g_base && x < g_base + 0x5000000) ? (uint32_t)(x - g_base) : 0xFFFFFFFFu;
}
__declspec(noinline) void NoteSys(char kind, UINT64 bytes) {
    void* fr[8] = {};
    const USHORT n = RtlCaptureStackBackTrace(1, 8, fr, nullptr);  // our own frames are outside the exe and skipped below
    uint32_t r0 = 0xFFFFFFFFu, r1 = 0xFFFFFFFFu;
    for (USHORT i = 0; i < n; ++i) {  // first frames inside the exe (skips wrappers such as OptiScaler / sl.interposer)
        const uint32_t r = ToRva(fr[i]);
        if (r == 0xFFFFFFFFu) continue;
        if (r0 == 0xFFFFFFFFu) r0 = r;
        else { r1 = r; break; }
    }
    AcquireSRWLockExclusive(&g_rowLock);
    int i = 0;
    for (; i < g_nRows; ++i)
        if (g_rows[i].ra0 == r0 && g_rows[i].ra1 == r1 && g_rows[i].kind == kind) break;
    if (i == g_nRows && g_nRows < kRows) g_rows[g_nRows++] = CallerRow{r0, r1, kind, 0, 0, 0};
    if (i < kRows) {
        ++g_rows[i].n;
        g_rows[i].bytes += (LONG64)bytes;
        if ((LONG64)bytes > g_rows[i].maxOne) g_rows[i].maxOne = (LONG64)bytes;
    }
    ReleaseSRWLockExclusive(&g_rowLock);
}

using ResidencyFn = HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*, UINT, ID3D12Pageable* const*);
using CreateHeapFn = HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*, const D3D12_HEAP_DESC*, REFIID, void**);
using CreateCommittedFn = HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*, const D3D12_HEAP_PROPERTIES*, D3D12_HEAP_FLAGS,
                                                      const D3D12_RESOURCE_DESC*, D3D12_RESOURCE_STATES,
                                                      const D3D12_CLEAR_VALUE*, REFIID, void**);
using CreatePlacedFn = HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*, ID3D12Heap*, UINT64, const D3D12_RESOURCE_DESC*,
                                                   D3D12_RESOURCE_STATES, const D3D12_CLEAR_VALUE*, REFIID, void**);
using CreateReservedFn = HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*, const D3D12_RESOURCE_DESC*, D3D12_RESOURCE_STATES,
                                                     const D3D12_CLEAR_VALUE*, REFIID, void**);
ResidencyFn o_mr = nullptr, o_ev = nullptr;
CreateHeapFn o_heap = nullptr;
CreateCommittedFn o_com = nullptr;
CreatePlacedFn o_pl = nullptr;
CreateReservedFn o_resv = nullptr;

// true = system memory on a discrete GPU: UPLOAD / READBACK, or CUSTOM with MemoryPoolPreference L0
bool SysPool(const D3D12_HEAP_PROPERTIES& h) {
    if (h.Type == D3D12_HEAP_TYPE_UPLOAD || h.Type == D3D12_HEAP_TYPE_READBACK) return true;
    return h.Type == D3D12_HEAP_TYPE_CUSTOM && h.MemoryPoolPreference == D3D12_MEMORY_POOL_L0;
}
// VSForceSysUpload: D3D12 defines UPLOAD as CUSTOM {WRITE_COMBINE, L0} on a discrete (NUMA) adapter, so the conversion keeps
// the API contract; it only takes away a driver's freedom to put UPLOAD heaps in CPU-visible VRAM (ReBAR / SAM). PC A/B.
D3D12_HEAP_PROPERTIES ForceSys(const D3D12_HEAP_PROPERTIES& h) {
    D3D12_HEAP_PROPERTIES c = h;
    c.Type = D3D12_HEAP_TYPE_CUSTOM;
    c.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE;
    c.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
    return c;
}
HRESULT STDMETHODCALLTYPE HkMakeResident(ID3D12Device* d, UINT n, ID3D12Pageable* const* o) {
    InterlockedIncrement64(&c_mrCalls);
    InterlockedAdd64(&c_mrObjs, n);
    return o_mr(d, n, o);
}
HRESULT STDMETHODCALLTYPE HkEvict(ID3D12Device* d, UINT n, ID3D12Pageable* const* o) {
    InterlockedIncrement64(&c_evCalls);
    InterlockedAdd64(&c_evObjs, n);
    return o_ev(d, n, o);
}
HRESULT STDMETHODCALLTYPE HkCreateHeap(ID3D12Device* d, const D3D12_HEAP_DESC* h, REFIID r, void** out) {
    D3D12_HEAP_DESC alt;
    const D3D12_HEAP_DESC* use = h;
    if (g_forceSys && h && h->Properties.Type == D3D12_HEAP_TYPE_UPLOAD) {
        alt = *h;
        alt.Properties = ForceSys(h->Properties);
        use = &alt;
    }
    HRESULT hr = o_heap(d, use, r, out);
    if (FAILED(hr) && use != h) hr = o_heap(d, h, r, out);  // never let the A/B break the game
    else if (use != h && SUCCEEDED(hr)) InterlockedIncrement64(&c_forced);
    if (SUCCEEDED(hr) && h && out) {
        InterlockedIncrement64(&c_heapN);
        const bool sys = SysPool(h->Properties);
        InterlockedAdd64(sys ? &c_heapSys : &c_heapLocal, (LONG64)h->SizeInBytes);
        if (sys) NoteSys(h->Properties.Type == D3D12_HEAP_TYPE_READBACK ? 'h' : 'H', h->SizeInBytes);
    }
    return hr;
}
HRESULT STDMETHODCALLTYPE HkCreateCommitted(ID3D12Device* d, const D3D12_HEAP_PROPERTIES* hp, D3D12_HEAP_FLAGS f,
                                            const D3D12_RESOURCE_DESC* rd, D3D12_RESOURCE_STATES st,
                                            const D3D12_CLEAR_VALUE* cv, REFIID r, void** out) {
    D3D12_HEAP_PROPERTIES alt;
    const D3D12_HEAP_PROPERTIES* use = hp;
    if (g_forceSys && hp && hp->Type == D3D12_HEAP_TYPE_UPLOAD) {
        alt = ForceSys(*hp);
        use = &alt;
    }
    HRESULT hr = o_com(d, use, f, rd, st, cv, r, out);
    if (FAILED(hr) && use != hp) hr = o_com(d, hp, f, rd, st, cv, r, out);
    else if (use != hp && SUCCEEDED(hr)) InterlockedIncrement64(&c_forced);
    if (SUCCEEDED(hr) && hp && rd && out) {
        UINT64 bytes = rd->Width;  // buffers: exact; textures: the driver's allocation size
        if (rd->Dimension != D3D12_RESOURCE_DIMENSION_BUFFER) bytes = d->GetResourceAllocationInfo(0, 1, rd).SizeInBytes;
        InterlockedIncrement64(&c_comN);
        const bool sys = SysPool(*hp);
        InterlockedAdd64(sys ? &c_comSys : &c_comLocal, (LONG64)bytes);
        if (sys) NoteSys(hp->Type == D3D12_HEAP_TYPE_READBACK ? 'c' : 'C', bytes);
    }
    return hr;
}
HRESULT STDMETHODCALLTYPE HkCreatePlaced(ID3D12Device* d, ID3D12Heap* heap, UINT64 off, const D3D12_RESOURCE_DESC* rd,
                                         D3D12_RESOURCE_STATES st, const D3D12_CLEAR_VALUE* cv, REFIID r, void** out) {
    const HRESULT hr = o_pl(d, heap, off, rd, st, cv, r, out);
    if (SUCCEEDED(hr) && heap && rd && out) {
        InterlockedIncrement64(&c_plN);
        const D3D12_HEAP_DESC hd = heap->GetDesc();
        if (SysPool(hd.Properties)) {
            const UINT64 bytes = rd->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER ? rd->Width
                                                                                  : d->GetResourceAllocationInfo(0, 1, rd).SizeInBytes;
            InterlockedIncrement64(&c_plSysN);
            InterlockedAdd64(&c_plSys, (LONG64)bytes);
            NoteSys(hd.Properties.Type == D3D12_HEAP_TYPE_READBACK ? 'p' : 'P', bytes);
        }
    }
    return hr;
}
HRESULT STDMETHODCALLTYPE HkCreateReserved(ID3D12Device* d, const D3D12_RESOURCE_DESC* rd, D3D12_RESOURCE_STATES st,
                                           const D3D12_CLEAR_VALUE* cv, REFIID r, void** out) {
    const HRESULT hr = o_resv(d, rd, st, cv, r, out);
    if (SUCCEEDED(hr)) InterlockedIncrement64(&c_resvN);
    return hr;
}
bool Patch(void** slot, void* hook, void** orig) {
    DWORD old = 0, t = 0;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) return false;
    *orig = *slot;
    *slot = hook;
    VirtualProtect(slot, sizeof(void*), old, &t);
    return true;
}
bool ReadVtable(void* dev, void*** vt) {
    __try {
        *vt = *reinterpret_cast<void** volatile*>(dev);
        return *vt != nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
SRWLOCK g_installLock = SRWLOCK_INIT;
const char* g_installedBy = "engine device read";
void InstallHooks(void* fromCreate = nullptr) {
    AcquireSRWLockExclusive(&g_installLock);
    if (g_hookTried) { ReleaseSRWLockExclusive(&g_installLock); return; }
    void* dev = fromCreate;
    if (!dev && !(g_known && g_base && ReadEnginePtr(g_base, &dev))) {  // device not there yet: retry
        ReleaseSRWLockExclusive(&g_installLock);
        return;
    }
    if (fromCreate) g_installedBy = "D3D12CreateDevice (IAT)";
    g_hookTried = true;
    void** vt = nullptr;
    if (!ReadVtable(dev, &vt)) {
        MLOG("VS: device vtable not readable - D3D12 counters off");
        ReleaseSRWLockExclusive(&g_installLock);
        return;
    }
    const bool ok = Patch(&vt[27], (void*)&HkCreateCommitted, (void**)&o_com) &&
                    Patch(&vt[28], (void*)&HkCreateHeap, (void**)&o_heap) &&
                    Patch(&vt[29], (void*)&HkCreatePlaced, (void**)&o_pl) &&
                    Patch(&vt[30], (void*)&HkCreateReserved, (void**)&o_resv) &&
                    Patch(&vt[34], (void*)&HkMakeResident, (void**)&o_mr) && Patch(&vt[35], (void*)&HkEvict, (void**)&o_ev);
    g_hooked = ok;
    ReleaseSRWLockExclusive(&g_installLock);
    double lu = -1, lb = 0;
    if (!QueryVram(DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &lu, &lb)) lu = -1;
    MLOG("VS: D3D12 device %p vtable %p: residency/allocation counters %s via %s (slots 27/28/29/30/34/35; VRAM local at "
         "install %.0f MB)%s", dev, (void*)vt, ok ? "installed" : "FAILED", g_installedBy, lu,
         g_forceSys ? "; VSForceSysUpload = 1: UPLOAD heaps -> CUSTOM write-combine L0 (system memory) - A/B ONLY" : "");
    // Resizable BAR / SAM hint: D3D12_OPTIONS16 (feature 43) GPUUploadHeapSupported = CPU-visible VRAM for the whole heap.
    // Raw values so an older SDK header still builds; a runtime without the feature returns E_INVALIDARG (logged as n/a).
    struct Opt16 { BOOL dynDepthBias; BOOL gpuUploadHeap; } o16{};
    const HRESULT hr16 = static_cast<ID3D12Device*>(dev)->CheckFeatureSupport((D3D12_FEATURE)43, &o16, sizeof(o16));
    D3D12_FEATURE_DATA_ARCHITECTURE arch{};
    const HRESULT hra = static_cast<ID3D12Device*>(dev)->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE, &arch, sizeof(arch));
    MLOG("VS: GPUUploadHeapSupported (ReBAR/SAM) %s, UMA %s, CacheCoherentUMA %s",
         SUCCEEDED(hr16) ? (o16.gpuUploadHeap ? "YES" : "no") : "n/a", SUCCEEDED(hra) ? (arch.UMA ? "yes" : "no") : "n/a",
         SUCCEEDED(hra) ? (arch.CacheCoherentUMA ? "yes" : "no") : "n/a");
}
// early install: poll every 2 ms from Start() for up to 10 min so boot-time allocations are counted too
// Earliest install: the game's only D3D12CreateDevice call (2EE300C) goes through its IAT slot (v2.02 RVA 38E2E18, import
// from sl.interposer.dll); the wrapper hooks the returned device before the engine creates a single heap.
constexpr uintptr_t kRvaIatCreateDevice = 0x38E2E18;
using CreateDeviceFn = HRESULT(WINAPI*)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
CreateDeviceFn o_createDevice = nullptr;
HRESULT WINAPI HkCreateDevice(IUnknown* a, D3D_FEATURE_LEVEL fl, REFIID r, void** out) {
    const HRESULT hr = o_createDevice(a, fl, r, out);
    if (SUCCEEDED(hr) && out && *out) {
        ID3D12Device* d = nullptr;
        if (SUCCEEDED(static_cast<IUnknown*>(*out)->QueryInterface(__uuidof(ID3D12Device), reinterpret_cast<void**>(&d))) && d) {
            InstallHooks(d);
            d->Release();
        }
    }
    return hr;
}
bool HookCreateDeviceIat() {
    if (!g_known || !g_base) return false;
    void** slot = reinterpret_cast<void**>(g_base + kRvaIatCreateDevice);
    HMODULE m = GetModuleHandleW(L"sl.interposer.dll");
    void* expect = m ? (void*)GetProcAddress(m, "D3D12CreateDevice") : nullptr;
    __try {
        if (!*slot || (expect && *slot != expect)) return false;  // not the import we know (or already wrapped by someone)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    return Patch(slot, (void*)&HkCreateDevice, (void**)&o_createDevice);
}
DWORD WINAPI EarlyThread(void*) {
    for (int i = 0; i < 300000 && !g_hookTried; ++i) {
        InstallHooks();
        if (!g_hookTried) Sleep(2);
    }
    return 0;
}
void LogCallers() {
    CallerRow rows[kRows];
    int n;
    AcquireSRWLockShared(&g_rowLock);
    n = g_nRows;
    memcpy(rows, g_rows, sizeof(CallerRow) * n);
    ReleaseSRWLockShared(&g_rowLock);
    for (int i = 0; i < n; ++i)  // biggest first
        for (int j = i + 1; j < n; ++j)
            if (rows[j].bytes > rows[i].bytes) { CallerRow t = rows[i]; rows[i] = rows[j]; rows[j] = t; }
    char line[1400];
    size_t o = (size_t)snprintf(line, sizeof line, "stats vs callers (system memory; H heap / C committed / P placed-in-sys-heap, "
                                                   "lower case = READBACK; rva0<-rva1: n, MB, max MB):");
    for (int i = 0; i < n && i < 24 && o < sizeof line - 80; ++i)
        o += (size_t)snprintf(line + o, sizeof line - o, " %c %X<-%X: %lld, %.1f, %.2f;", rows[i].kind, rows[i].ra0, rows[i].ra1,
                              rows[i].n, rows[i].bytes / kMB, rows[i].maxOne / kMB);
    MLOG("%s", line);
}
}  // namespace vs

}  // namespace

bool Start(const StartInfo& info) {
    g_log = info.log;
    g_base = info.base;
    g_known = info.knownBuild;
    g_getDev = info.getD3D12Device;
    g_level = info.iniPath ? (int)GetPrivateProfileIntW(L"Profiler", L"MemStats", 0, info.iniPath) : 0;
    g_on = g_level != 0;
    QueryPerformanceFrequency(&g_qpf);
    if (g_level >= 2) {
        vs::g_forceSys = info.iniPath ? (int)GetPrivateProfileIntW(L"Profiler", L"VSForceSysUpload", 0, info.iniPath) : 0;
        void* dev0 = nullptr;
        const bool devAlready = g_known && g_base && ReadEnginePtr(g_base, &dev0);
        const bool iat = !devAlready && vs::HookCreateDeviceIat();
        MLOG("VS: D3D12CreateDevice IAT hook %s", iat ? "set (device hooked at creation)" : devAlready ? "not needed (device exists)" : "not set - polling the engine device");
        if (HANDLE t = CreateThread(nullptr, 0, vs::EarlyThread, nullptr, 0, nullptr)) CloseHandle(t);
    }
    if (g_on)
        MLOG("MEM: MemStats on - one 'stats mem:' line per stats period (process memory, handles, threads, VRAM)%s%s",
             g_getDev || g_known ? "" : "; no D3D12 device source, VRAM from the first hardware adapter",
             g_level >= 2 ? "; MemStats = 2: + one 'stats vs:' line per second (VRAM, texture page pool, D3D12 residency)" : "");
    return g_on;
}

bool Enabled() { return g_on; }

// 1.7.36 (tour.cpp): VRAM of the game's adapter in MB; false until the stats thread found the adapter (MemStats = 1)
bool Vram(double* localUsed, double* localBudget, double* nonLocalUsed, double* nonLocalBudget) {
    if (!g_on || !*reinterpret_cast<IDXGIAdapter3* volatile*>(&g_adapter)) return false;
    return QueryVram(DXGI_MEMORY_SEGMENT_GROUP_LOCAL, localUsed, localBudget) &&
           QueryVram(DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, nonLocalUsed, nonLocalBudget);
}

void LogLine() {
    if (!g_on) return;
    Sample s;
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    s.t = (double)now.QuadPart / (double)g_qpf.QuadPart;

    PROCESS_MEMORY_COUNTERS_EX pm{};
    pm.cb = sizeof(pm);
    double peakWs = 0;
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pm), sizeof(pm))) {
        s.priv = (double)pm.PrivateUsage / kMB;
        s.ws = (double)pm.WorkingSetSize / kMB;
        s.commit = (double)pm.PagefileUsage / kMB;
        peakWs = (double)pm.PeakWorkingSetSize / kMB;
    }
    DWORD hc = 0;
    s.handles = GetProcessHandleCount(GetCurrentProcess(), &hc) ? (double)hc : -1;
    s.threads = CountThreads();

    EnsureAdapter();
    double vlb = 0, vnb = 0;
    if (!QueryVram(DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &s.vl, &vlb)) s.vl = -1;
    if (!QueryVram(DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &s.vn, &vnb)) s.vn = -1;

    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    const bool haveSys = GlobalMemoryStatusEx(&ms) != 0;

    char vram[128];
    if (s.vl >= 0)
        snprintf(vram, sizeof(vram), "VRAM local used/budget %.0f/%.0f MB, non-local %.0f/%.0f MB", s.vl, vlb,
                 s.vn >= 0 ? s.vn : 0.0, vnb);
    else
        snprintf(vram, sizeof(vram), "VRAM n/a");
    char sys[128] = "";
    if (haveSys)
        snprintf(sys, sizeof(sys), " | sys RAM avail %.0f MB, commit %.0f/%.0f MB", (double)ms.ullAvailPhys / kMB,
                 (double)(ms.ullTotalPageFile - ms.ullAvailPageFile) / kMB, (double)ms.ullTotalPageFile / kMB);

    if (!g_haveFirst) {
        g_first = s;
        g_haveFirst = true;
    } else if (g_first.vl < 0 && s.vl >= 0) {  // adapter found after the first line: VRAM deltas count from here
        g_first.vl = s.vl;
        g_first.vn = s.vn;
    }
    const Sample& f = g_first;
    const double mins = (s.t - f.t) / 60.0;
    char delta[200] = "";
    if (mins > 0.1)
        snprintf(delta, sizeof(delta),
                 " | since start (%.0f min): private %+.0f, ws %+.0f, handles %+.0f, threads %+.0f, VRAM local %+.0f, "
                 "non-local %+.0f",
                 mins, s.priv - f.priv, s.ws - f.ws, s.handles - f.handles, s.threads - f.threads,
                 (s.vl >= 0 && f.vl >= 0) ? s.vl - f.vl : 0.0, (s.vn >= 0 && f.vn >= 0) ? s.vn - f.vn : 0.0);

    MLOG("stats mem: private %.0f MB, working set %.0f MB (peak %.0f), commit %.0f MB, handles %.0f, threads %.0f, %s%s%s",
         s.priv, s.ws, peakWs, s.commit, s.handles, s.threads, vram, sys, delta);
}

// 1.7.48 VS probe: one 'stats vs:' line per second when MemStats = 2 (stats thread, every second).
void Tick1s() {
    if (g_level < 2) return;
    if (!vs::g_hookTried) vs::InstallHooks();
    EnsureAdapter();
    double lu = -1, lb = 0, nu = -1, nb = 0;
    if (!QueryVram(DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &lu, &lb)) lu = -1;
    if (!QueryVram(DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &nu, &nb)) nu = -1;
    static double plu = -1;
    const double dlu = (plu >= 0 && lu >= 0) ? lu - plu : 0.0;
    plu = lu;
    char pool[360] = "pool n/a";
    vs::PoolRead r;
    if (g_known && g_base && vs::ReadPool(g_base, &r)) {
        const long long size = (long long)((int)r.sets - (int)r.setsLo) * (long long)r.ways;
        const unsigned resv = (r.r756c > r.r7570 ? r.r756c : r.r7570) + r.r7584 + r.r757c + r.r7578;
        const double occ = size > 0 ? ((double)r.used - (double)resv) * 100.0 / (double)size : -1.0;
        const long long freeReq = (long long)r.cap - ((long long)resv + r.frameReq);
        snprintf(pool, sizeof(pool),
                 "pool %lld pages (%.0f MB) used %u reserved %u occ %.1f%%%s | req-pool %u free %lld frame %u/%u | "
                 "lastframe touches %u hits %u misses %u qfull %u | P%s45BA588",
                 size, (double)size / 16.0, r.used, resv, occ, occ > 85.0 ? " OVER85" : "", r.cap, freeReq, r.frameReq,
                 r.frameCap, r.touches, r.hits, r.misses, r.qfull, r.p == r.gdev ? "==" : "!=");
    }
    static LONG64 p[10] = {};
    const LONG64 c[10] = {vs::c_mrCalls, vs::c_mrObjs, vs::c_evCalls, vs::c_evObjs, vs::c_heapN,
                          vs::c_heapLocal, vs::c_heapSys, vs::c_comN, vs::c_comLocal, vs::c_comSys};
    char d3d[480] = "d3d12 n/a";
    if (vs::g_hooked)
        snprintf(d3d, sizeof(d3d),
                 "d3d12/s MakeResident %lld (%lld obj) Evict %lld (%lld obj) CreateHeap %lld (+%.1f MB vram, +%.1f MB sys) "
                 "Committed %lld (+%.1f MB vram, +%.1f MB sys) | since hook: heaps %.0f/%.0f MB, committed %.0f/%.0f MB "
                 "(vram/sys), reserved res %lld, MakeResident obj %lld, Evict obj %lld, placed %lld (in sys heaps %lld, %.0f MB), "
                 "forced-sys %lld",
                 c[0] - p[0], c[1] - p[1], c[2] - p[2], c[3] - p[3], c[4] - p[4], (c[5] - p[5]) / kMB, (c[6] - p[6]) / kMB,
                 c[7] - p[7], (c[8] - p[8]) / kMB, (c[9] - p[9]) / kMB, c[5] / kMB, c[6] / kMB, c[8] / kMB, c[9] / kMB,
                 (long long)vs::c_resvN, c[1], c[3], (long long)vs::c_plN, (long long)vs::c_plSysN, vs::c_plSys / kMB,
                 (long long)vs::c_forced);
    for (int i = 0; i < 10; ++i) p[i] = c[i];
    MLOG("stats vs: local %.0f/%.0f MB (%+.0f MB/s), nonlocal %.0f/%.0f MB | %s | %s", lu, lb, dlu, nu, nb, pool, d3d);
    static int ticks = 0;
    if (vs::g_hooked && ++ticks % 30 == 0) vs::LogCallers();
}

}  // namespace memstats
