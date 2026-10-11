// mpccheck.cpp - MpcCheck: runtime self-check of the MPCS tail skip (see mpccheck.h, notes\meshlet-frontend.md 3). Diagnostic only.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

#include "gpuopt.h"
#include "mpccheck.h"

extern "C" {
extern void* gpuopt_orig0[96];  // gpuopt_thunks.asm: the original command-list methods per vtable set
extern void* gpuopt_orig1[96];
}

namespace mpcck {
namespace {

void (*g_log)(const char*, ...) = nullptr;
#define CLOG(fmt, ...) do { if (g_log) g_log("MPCK: " fmt, __VA_ARGS__); } while (0)

constexpr int kSlotGetDevice = 7, kSlotCopyBuffer = 15, kSlotResourceBarrier = 26, kSlotWriteImm = 66;
using GetDeviceFn = HRESULT(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, REFIID, void**);
using CopyBufferFn = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, ID3D12Resource*, UINT64, ID3D12Resource*, UINT64, UINT64);
using BarrierFn = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, UINT, const D3D12_RESOURCE_BARRIER*);
using WriteImmFn = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, UINT, const D3D12_WRITEBUFFERIMMEDIATE_PARAMETER*,
                                            const D3D12_WRITEBUFFERIMMEDIATE_MODE*);
void* Orig(int set, int slot) { return (set ? gpuopt_orig1 : gpuopt_orig0)[slot]; }

int g_target = 0;
size_t g_maxBytes = 64ull << 20;
volatile LONG g_on = 0;
volatile LONG g_wbi[2] = {-1, -1};   // per vtable set: list supports WriteBufferImmediate (ID3D12GraphicsCommandList2), -1 = not asked yet

constexpr int kMaxRes = 6;
constexpr uint32_t kTokenBase = 0x4D504300u;   // "MPC" in the high bytes of the marker value

// one checked chain: up to 4 snapshots of the same resource set in one READBACK buffer (part p at p * partBytes), marker at the end
struct Job {
    ID3D12Resource* rb = nullptr;
    uint8_t* map = nullptr;
    size_t cap = 0;          // bytes of rb
    size_t partBytes = 0;
    int nres = 0;
    UINT64 size[kMaxRes] = {};
    bool taken[4] = {};      // S1 before #28, S2 after #31, S3 before #26, S4 after #26
    bool x23one = false, ran23 = false, mpcBuild = false, wbi = false, extra = false;
    uint32_t x23 = 0, token = 0;
    LONG64 frame = 0;
};
std::vector<Job*> g_free, g_pending;
SRWLOCK g_lock = SRWLOCK_INIT;
volatile LONG64 g_frame = 0;
volatile LONG g_tokenSeq = 0;
// counters: recording thread, Present thread and stats thread -> interlocked; window values are exchanged out every 30 s
enum C { cChains, cApplic, cEqual, cMismatch, cSkipNoRes, cSkipBudget, cNotApplic, cApplic26, cEqual26, cMismatch26, cTimeouts, cExtraRes,
         cMpcBuild, cShipped, cNoWbi, cNum };
volatile LONG64 g_win[cNum], g_tot[cNum];
void Inc(C c) {
    InterlockedIncrement64(&g_win[c]);
    InterlockedIncrement64(&g_tot[c]);
}
volatile LONG g_logged = 0;

struct TChk {
    bool active, ran23, x23one, mpc;
    int stage;        // 0 before #23, 1 after #23 (collect resources), 2 inside #28-#31 (S1 taken or not applicable), 3 tail done
    int stage26;      // 0 none, 1 #26 dispatched (S3 taken), 2 done
    uint32_t x23;
    ID3D12Resource* res[kMaxRes];
    int nres;
    Job* job;
};
thread_local TChk t_c;

bool InSet(ID3D12Resource* r) {
    for (int i = 0; i < t_c.nres; ++i)
        if (t_c.res[i] == r) return true;
    return false;
}

Job* NewJob(void* list, int set) {
    UINT64 sum = 0;
    for (int i = 0; i < t_c.nres; ++i) {
        const D3D12_RESOURCE_DESC d = t_c.res[i]->GetDesc();
        if (d.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER) return nullptr;
        sum += (d.Width + 255) & ~255ull;
    }
    const size_t need = (size_t)sum * 4 + 256;
    if (need > g_maxBytes) {
        Inc(cSkipBudget);
        return nullptr;
    }
    Job* j = nullptr;
    AcquireSRWLockExclusive(&g_lock);
    for (size_t i = 0; i < g_free.size(); ++i)
        if (g_free[i]->cap >= need) {
            j = g_free[i];
            g_free.erase(g_free.begin() + (ptrdiff_t)i);
            break;
        }
    ReleaseSRWLockExclusive(&g_lock);
    if (!j) {
        ID3D12Device* dev = nullptr;
        if (FAILED(reinterpret_cast<GetDeviceFn>(Orig(set, kSlotGetDevice))((ID3D12GraphicsCommandList*)list, __uuidof(ID3D12Device), (void**)&dev)) || !dev)
            return nullptr;
        D3D12_HEAP_PROPERTIES hp = {};
        hp.Type = D3D12_HEAP_TYPE_READBACK;
        D3D12_RESOURCE_DESC rd = {};
        rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        rd.Width = need;
        rd.Height = 1;
        rd.DepthOrArraySize = 1;
        rd.MipLevels = 1;
        rd.SampleDesc.Count = 1;
        rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ID3D12Resource* rb = nullptr;
        void* p = nullptr;
        D3D12_RANGE all = {0, need};
        const bool ok = SUCCEEDED(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                               __uuidof(ID3D12Resource), (void**)&rb)) && rb && SUCCEEDED(rb->Map(0, &all, &p)) && p;
        dev->Release();
        if (!ok) {
            if (rb) rb->Release();
            CLOG("%s", "READBACK buffer creation failed - check off");
            InterlockedExchange(&g_on, 0);
            return nullptr;
        }
        j = new Job();
        j->rb = rb;
        j->map = (uint8_t*)p;
        j->cap = need;
    }
    j->partBytes = (size_t)sum;
    j->nres = t_c.nres;
    for (int i = 0; i < t_c.nres; ++i) j->size[i] = t_c.res[i]->GetDesc().Width;
    for (bool& b : j->taken) b = false;
    j->extra = false;
    j->wbi = false;
    memset(j->map + j->cap - 16, 0, 16);   // marker slot
    return j;
}

// one snapshot of the chain's resource set into part p: UAV -> COPY_SOURCE, copy, COPY_SOURCE -> UAV (original methods, no hooks)
void Snap(void* list, int set, int p) {
    Job* j = t_c.job;
    if (!j || j->taken[p]) return;
    auto cl = (ID3D12GraphicsCommandList*)list;
    D3D12_RESOURCE_BARRIER b[kMaxRes];
    for (int i = 0; i < j->nres; ++i) {
        b[i] = {};
        b[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b[i].Transition.pResource = t_c.res[i];
        b[i].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b[i].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        b[i].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    }
    reinterpret_cast<BarrierFn>(Orig(set, kSlotResourceBarrier))(cl, (UINT)j->nres, b);
    UINT64 off = (UINT64)p * j->partBytes;
    for (int i = 0; i < j->nres; ++i) {
        reinterpret_cast<CopyBufferFn>(Orig(set, kSlotCopyBuffer))(cl, j->rb, off, t_c.res[i], 0, j->size[i]);
        off += (j->size[i] + 255) & ~255ull;
    }
    for (int i = 0; i < j->nres; ++i) std::swap(b[i].Transition.StateBefore, b[i].Transition.StateAfter);
    reinterpret_cast<BarrierFn>(Orig(set, kSlotResourceBarrier))(cl, (UINT)j->nres, b);
    j->taken[p] = true;
}

bool HasWbi(void* list, int set) {
    LONG v = g_wbi[set ? 1 : 0];
    if (v < 0) {
        ID3D12GraphicsCommandList2* l2 = nullptr;
        v = SUCCEEDED(((ID3D12GraphicsCommandList*)list)->QueryInterface(__uuidof(ID3D12GraphicsCommandList2), (void**)&l2)) && l2 ? 1 : 0;
        if (l2) l2->Release();
        InterlockedExchange(&g_wbi[set ? 1 : 0], v);
    }
    return v == 1;
}

// region end: marker after the last copy, hand the job to the Present thread
void Finish(void* list, int set) {
    Job* j = t_c.job;
    t_c.job = nullptr;
    if (!j) return;
    if (!j->taken[0] && !j->taken[2]) {   // nothing snapshotted
        AcquireSRWLockExclusive(&g_lock);
        g_free.push_back(j);
        ReleaseSRWLockExclusive(&g_lock);
        return;
    }
    j->token = kTokenBase ^ (uint32_t)InterlockedIncrement(&g_tokenSeq);
    if (!j->token) j->token = 1;
    j->wbi = HasWbi(list, set);
    if (j->wbi) {
        D3D12_WRITEBUFFERIMMEDIATE_PARAMETER pr = {j->rb->GetGPUVirtualAddress() + j->cap - 16, j->token};
        const D3D12_WRITEBUFFERIMMEDIATE_MODE m = D3D12_WRITEBUFFERIMMEDIATE_MODE_MARKER_OUT;
        reinterpret_cast<WriteImmFn>(Orig(set, kSlotWriteImm))((ID3D12GraphicsCommandList*)list, 1, &pr, &m);
    }
    j->x23one = t_c.x23one;
    j->ran23 = t_c.ran23;
    j->x23 = t_c.x23;
    j->mpcBuild = t_c.mpc;
    j->frame = g_frame;
    AcquireSRWLockExclusive(&g_lock);
    g_pending.push_back(j);
    ReleaseSRWLockExclusive(&g_lock);
}

// Present thread: compare part a with part b; returns the number of differing dwords and the first one
uint64_t Diff(const Job* j, int a, int b, int* resOut, size_t* dwOut, uint32_t* va, uint32_t* vb) {
    uint64_t n = 0;
    size_t off = 0;
    for (int i = 0; i < j->nres; ++i) {
        const uint8_t* pa = j->map + (size_t)a * j->partBytes + off;
        const uint8_t* pb = j->map + (size_t)b * j->partBytes + off;
        const size_t sz = (size_t)j->size[i];
        if (memcmp(pa, pb, sz) != 0)
            for (size_t k = 0; k + 4 <= sz; k += 4) {
                uint32_t x, y;
                memcpy(&x, pa + k, 4);
                memcpy(&y, pb + k, 4);
                if (x != y) {
                    if (!n) { *resOut = i; *dwOut = k / 4; *va = x; *vb = y; }
                    ++n;
                }
            }
        off += (sz + 255) & ~(size_t)255;
    }
    return n;
}

}  // namespace

// [Profiler] MpcCheckKeys = "23=<8 hex>,28=<8 hex>,..." (tests\gp_harness.cpp mode 5 only): extra digest -> kernel entries
uint32_t g_extraKey[8] = {};
int g_extraK[8] = {}, g_nExtra = 0;

int KernelOf(uint32_t key, bool* mpcBuild) {
    if (mpcBuild) *mpcBuild = false;
    for (int i = 0; i < g_nExtra; ++i)
        if (g_extraKey[i] == key) return g_extraK[i];
    switch (key) {   // container digests (first 32 bits) of 0x29ee0902.file: shipped / `mpc` build (gpu-culling-rewrite.md)
        case 0x34078a28u: return 23;
        case 0xc7e0a06bu: if (mpcBuild) *mpcBuild = true; return 23;
        case 0x4225b9ffu: case 0x55198d09u: return 28;
        case 0x6b87b43eu: case 0xd17e19a0u: return 29;
        case 0x3ecb247du: case 0x718b35feu: return 30;
        case 0xde4d71b0u: case 0x676577acu: return 31;
        case 0x5f610a3eu: return 26;
        case 0x304bac05u: return 24;
        case 0x1b4b5b75u: return 27;
        default: return 0;
    }
}

bool Start(const StartInfo& info) {
    g_log = info.log;
    g_target = info.chains;
    if (g_target <= 0 || !info.iniPath) return false;
    int mb = (int)GetPrivateProfileIntW(L"Profiler", L"MpcCheckMaxMB", 64, info.iniPath);
    if (mb < 1) mb = 1;
    if (mb > 1024) mb = 1024;
    g_maxBytes = (size_t)mb << 20;
    wchar_t ek[256] = {};
    GetPrivateProfileStringW(L"Profiler", L"MpcCheckKeys", L"", ek, 256, info.iniPath);
    for (const wchar_t* p = ek; *p && g_nExtra < 8;) {
        wchar_t* e = nullptr;
        const long k = wcstol(p, &e, 10);
        if (e == p || *e != L'=') break;
        const unsigned long v = wcstoul(e + 1, &e, 16);
        g_extraK[g_nExtra] = (int)k;
        g_extraKey[g_nExtra++] = (uint32_t)v;
        while (*e == L',' || *e == L' ') ++e;
        p = e;
    }
    if (g_nExtra) CLOG("%d test digests from MpcCheckKeys", g_nExtra);
    InterlockedExchange(&g_on, 1);
    CLOG("MPC tail-skip self-check on: %d chains, stats frames only, snapshots of the buffers UAV-barriered after MPC #23 before #28 / after #31 "
         "and before / after #26 (max %d MB per chain); nothing is skipped", g_target, mb);
    return true;
}

bool Wanted() {
    if (!g_on) return false;
    if ((int)(g_tot[cEqual] + g_tot[cMismatch]) >= g_target) return false;
    if (gpuopt::Batching()) {   // deferred engine barriers would cross our inserted transitions
        if (InterlockedExchange(&g_on, 0)) CLOG("%s", "[GpuOpt] Barriers = 1 (batching) is on - the check needs pass-through barriers; off");
        return false;
    }
    return true;
}

void OnRegion(void* list, int set, bool begin) {
    if (begin) {
        t_c = TChk{};
        t_c.active = true;
        Inc(cChains);
        return;
    }
    if (!t_c.active) return;
    if (t_c.stage == 2 && t_c.job && t_c.job->taken[0]) Snap(list, set, 1);   // the region ended right after #31
    if (t_c.stage26 == 1 && t_c.job && t_c.job->taken[2]) Snap(list, set, 3);
    Finish(list, set);
    t_c.active = false;
}

void OnDispatch(void* list, int set, uint32_t key, uint32_t x, uint32_t, uint32_t, bool indirect) {
    if (!t_c.active || !g_on) return;
    bool mpc = false;
    const int k = KernelOf(key, &mpc);
    if (k == 23 && !indirect) {
        t_c.stage = 1;
        t_c.x23 = x;
        t_c.ran23 = x >= 1 && x <= 32;   // as caveMpcsA: 1 <= cnt <= 32768
        t_c.x23one = x == 1;
        t_c.mpc = mpc;
        t_c.nres = 0;
        Inc(mpc ? cMpcBuild : cShipped);
        return;
    }
    if (k >= 28 && k <= 31) {
        if (t_c.stage == 1) {   // first tail dispatch: S1 = the state #23 left
            t_c.stage = 2;
            if (!t_c.x23one) {
                Inc(cNotApplic);
            } else if (!t_c.nres) {
                Inc(cSkipNoRes);
            } else {
                Inc(cApplic);
                if (!t_c.job) t_c.job = NewJob(list, set);
                if (t_c.job) Snap(list, set, 0);
            }
        }
        return;
    }
    if (t_c.stage == 2) {   // the first dispatch after #31: S2
        t_c.stage = 3;
        if (t_c.job && t_c.job->taken[0]) Snap(list, set, 1);
    }
    if (t_c.stage26 == 1) {   // the dispatch after #26 (#27): S4
        t_c.stage26 = 2;
        if (t_c.job && t_c.job->taken[2]) Snap(list, set, 3);
    }
    if (k == 26 && t_c.ran23 && t_c.nres) {
        t_c.stage26 = 1;
        Inc(cApplic26);
        if (!t_c.job) t_c.job = NewJob(list, set);
        if (t_c.job) Snap(list, set, 2);
    }
}

void OnBarrier(void*, int, unsigned n, const D3D12_RESOURCE_BARRIER* b) {
    if (!t_c.active || !b) return;
    for (unsigned i = 0; i < n; ++i) {
        if (b[i].Type != D3D12_RESOURCE_BARRIER_TYPE_UAV || !b[i].UAV.pResource) continue;
        ID3D12Resource* r = b[i].UAV.pResource;
        if (t_c.stage == 1) {   // between #23 and #28: the buffers #23 wrote
            if (!InSet(r) && t_c.nres < kMaxRes) t_c.res[t_c.nres++] = r;
        } else if ((t_c.stage == 2 || t_c.stage26 == 1) && !InSet(r)) {
            Inc(cExtraRes);   // a buffer the tail writes that the snapshots do not cover
            if (t_c.job) t_c.job->extra = true;
        }
    }
}

void OnClose(void*, int) {}   // snapshots may continue on the next list of the same region; the marker goes on the region's last list

void OnFrame() {
    const LONG64 f = InterlockedIncrement64(&g_frame);
    std::vector<Job*> done;
    AcquireSRWLockExclusive(&g_lock);
    for (size_t i = 0; i < g_pending.size();) {
        Job* j = g_pending[i];
        uint32_t mk = 0;
        memcpy(&mk, j->map + j->cap - 16, 4);
        const bool ready = j->wbi ? mk == j->token : f - j->frame >= 30;
        const bool timeout = j->wbi && !ready && f - j->frame >= 240;
        if (ready || timeout) {
            if (timeout) { Inc(cTimeouts); j->taken[0] = j->taken[2] = false; }
            if (!j->wbi) Inc(cNoWbi);
            done.push_back(j);
            g_pending.erase(g_pending.begin() + (ptrdiff_t)i);
        } else {
            ++i;
        }
    }
    ReleaseSRWLockExclusive(&g_lock);
    for (Job* j : done) {
        int ri = 0;
        size_t dw = 0;
        uint32_t va = 0, vb = 0;
        if (j->taken[0] && j->taken[1]) {
            const uint64_t nd = Diff(j, 0, 1, &ri, &dw, &va, &vb);
            if (nd) {
                Inc(cMismatch);
                if (InterlockedIncrement(&g_logged) <= 20)
                    CLOG("MISMATCH #28-#31 (%s 0x29ee0902.file, #23 Dispatch(%u,1,1)): %llu dwords differ; first: buffer %d (%llu B) dword %zu: after #23 "
                         "%08X, after #31 %08X%s", j->mpcBuild ? "mpc" : "SHIPPED", j->x23, (unsigned long long)nd, ri,
                         (unsigned long long)j->size[ri], dw, va, vb, j->extra ? " (+ unsnapshotted barriered buffers)" : "");
            } else {
                Inc(cEqual);
            }
        }
        if (j->taken[2] && j->taken[3]) {
            const uint64_t nd = Diff(j, 2, 3, &ri, &dw, &va, &vb);
            if (nd) {
                Inc(cMismatch26);
                if (InterlockedIncrement(&g_logged) <= 20)
                    CLOG("MISMATCH #26 (%s 0x29ee0902.file, #23 Dispatch(%u,1,1)): %llu dwords differ; first: buffer %d dword %zu: before %08X after %08X",
                         j->mpcBuild ? "mpc" : "SHIPPED", j->x23, (unsigned long long)nd, ri, dw, va, vb);
            } else {
                Inc(cEqual26);
            }
        }
        AcquireSRWLockExclusive(&g_lock);
        g_free.push_back(j);
        ReleaseSRWLockExclusive(&g_lock);
    }
}

void LogLine() {
    if (!g_target) return;
    LONG64 w[cNum], t[cNum];
    for (int i = 0; i < cNum; ++i) {
        w[i] = InterlockedExchange64(&g_win[i], 0);
        t[i] = g_tot[i];
    }
    CLOG("window: %lld chains (#23 %lld mpc / %lld shipped); #28-#31 skip applicable %lld: equal %lld, MISMATCH %lld | #26: applicable %lld, equal %lld, "
         "MISMATCH %lld | not applicable (cnt 0 or > 1024) %lld, no buffers %lld, over budget %lld, marker timeouts %lld, no WriteBufferImmediate %lld, "
         "tail barriers outside the snapshot set %lld || TOTAL: #28-#31 equal %lld / MISMATCH %lld, #26 equal %lld / MISMATCH %lld, target %d chains%s",
         w[cChains], w[cMpcBuild], w[cShipped], w[cApplic], w[cEqual], w[cMismatch], w[cApplic26], w[cEqual26], w[cMismatch26], w[cNotApplic],
         w[cSkipNoRes], w[cSkipBudget], w[cTimeouts], w[cNoWbi], w[cExtraRes], t[cEqual], t[cMismatch], t[cEqual26], t[cMismatch26], g_target,
         Wanted() ? "" : " - DONE");
}

}  // namespace mpcck
