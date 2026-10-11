// cbvupload.cpp - CBU: CBS streaming CB copy (F3), CBN no-op upload skip (F1), CBT CBV table reuse (F4), CbvStats,
// CbvRecord. See cbvupload.h and notes\renderer\cbv-upload.md.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <emmintrin.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "cbvupload.h"
#include "cbvstage.h"

namespace cbvupload {
namespace {

// ---- v2.02 sites (patches.json group CBU) ----
constexpr uint32_t kTblSite = 0x118E19;   // SynFlushCbvTables: call SynUploadCbvTable 118E28
constexpr uint32_t kUpload = 0x118E28;
constexpr uint32_t kGfxCopy = 0x118F4D;   // SynUploadCbvTable: call Memcpy B94250 (ring <- shadow, declared size)
constexpr uint32_t kCsCopy = 0x495AF2;    // CmdListUploadComputeCbvTable 4959EC: same call
constexpr uint32_t kMemcpy = 0xB94250;
struct Region {
    uint32_t rva, len, fnv;
};
// whole functions whose behaviour the wrapper depends on (call sites included, checked before any write)
constexpr Region kFlushFn = {0x118DA8, 0x80, 0x73769803};    // SynFlushCbvTables
constexpr Region kUploadFn = {0x118E28, 0x297, 0xBAE85B33};  // SynUploadCbvTable
constexpr Region kCsFn = {0x4959EC, 0x26D, 0x7AF941FF};      // CmdListUploadComputeCbvTable
constexpr Region kDescFn = {0x1192B0, 0xB4, 0x2542B674};     // ShaderVisibleDescRingAlloc (CBT derives the table index)

// ---- SynCtx layout (S, notes\renderer\01b-replay-handlers.md 0.1, 06-resources.md 1.3) ----
constexpr size_t kList = 0x580;       // open ID3D12GraphicsCommandList*
constexpr size_t kTag = 0x6D0;        // u64 fence value the open list signals (ring tag)
constexpr size_t kStagInc = 0x938;    // u32 CPU staging descriptor increment
constexpr size_t kStagBase = 0x940;   // u64 CPU staging heap start (CBV slot stage*14+slot)
constexpr size_t kRing = 0x980;       // DescRing: +30h u64 stamps*, +38h base, +3Ch cap, +40h max request, +44h cursor, +4Ch wraps
constexpr size_t kRingInc = 0x998;    // = kRing + 18h
constexpr size_t kRingGpu = 0x9A8;    // = kRing + 28h
constexpr size_t kMask = 0xB70;       // u32[8] CbvDirtyMask
constexpr size_t kTblN = 0xB90;       // u32[8] CBV table size last copied
constexpr size_t kShadow = 0xBB0;     // u8*[8*14] CPU shadow per slot
constexpr size_t kSizes = 0xF30;      // u32[8*14] size of the last upload per slot
constexpr int kStages = 7, kSlots = 14, kIdx = kStages * kSlots;

template <class T>
inline T& At(uint8_t* c, size_t off) {
    return *reinterpret_cast<T*>(c + off);
}

using UploadFn = void (*)(uint8_t* c, int* rootIdx, int stage, uint8_t* layout, uint64_t* fenceCache);

void (*g_log)(const char*, ...) = nullptr;
UploadFn g_orig = nullptr;
volatile uint32_t g_cbs = 0, g_cbn = 0, g_cbt = 0, g_stats = 0;
volatile uint32_t g_cbsFence = 1;  // [Tuning] CBS_Fence (live): 0 = no sfence (the driver's next serialising instruction drains, as for the stock memcpy)
bool g_instCbs = false, g_instTbl = false, g_instCbn = false, g_instCbt = false;
int g_statsSec = 10;

struct Rec {  // last upload of one slot
    uint8_t* bytes = nullptr;
    uint32_t cap = 0, size = 0;
    uint64_t epoch = 0, tag = 0, list = 0, stag = 0;
};
struct Tbl {  // last CBV table copy of one stage
    uint64_t epoch = 0, tag = 0, list = 0, stag = 0, handle = 0;
    uint32_t n = 0;
    int root = 0;
};
struct Counts {
    uint64_t calls = 0, uploads = 0, bytes = 0, noopAny = 0, noopSame = 0, noopSameBytes = 0, skipped = 0, skippedBytes = 0, tblCand = 0, tblHit = 0,
             cuts = 0, cutRedo = 0;
};
struct Ctx {
    uint8_t* c = nullptr;
    uint64_t epoch = 1;  // records / tables from an older epoch are invalid
    bool tracking = false;
    Rec rec[kIdx];
    Tbl tbl[kStages];
    Counts n;
};
constexpr int kMaxCtx = 8;
Ctx* volatile g_ctx[kMaxCtx] = {};
SRWLOCK g_ctxLock = SRWLOCK_INIT;
volatile LONG64 g_copies = 0, g_copyBytes = 0;  // CBS copies (any thread)

Ctx* Find(uint8_t* c) {
    for (int i = 0; i < kMaxCtx; ++i) {
        Ctx* x = g_ctx[i];
        if (!x) break;
        if (x->c == c) return x;
    }
    AcquireSRWLockExclusive(&g_ctxLock);
    Ctx* r = nullptr;
    for (int i = 0; i < kMaxCtx; ++i) {
        Ctx* x = g_ctx[i];
        if (x && x->c == c) { r = x; break; }
        if (!x) {
            x = new (std::nothrow) Ctx;
            if (x) {
                x->c = c;
                MemoryBarrier();
                g_ctx[i] = x;
            }
            r = x;
            break;
        }
    }
    ReleaseSRWLockExclusive(&g_ctxLock);
    return r;
}

// ---- CbvRecord ----
// file: "CBUTRACE" u32 version 1, then per call: u32 'CALL', u32 stage, u32 mask, u32 count, u32 n, u32 rootIdx,
// u64 ctx, u64 tag, u64 list, count x {u32 size, u32 slot}, then for each entry whose slot bit is in mask: size bytes.
int g_recWant = 0, g_recDelay = 60;
volatile LONG g_recState = 0;  // 0 off / waiting, 1 recording, 2 writing / done
ULONGLONG g_recStart = 0;
uint8_t* g_recBuf = nullptr;
size_t g_recCap = 0, g_recLen = 0;
int g_recCalls = 0;
wchar_t g_recPath[MAX_PATH] = {};

DWORD WINAPI RecWriter(LPVOID) {
    FILE* f = nullptr;
    if (_wfopen_s(&f, g_recPath, L"wb") == 0 && f) {
        fwrite(g_recBuf, 1, g_recLen, f);
        fclose(f);
        if (g_log) g_log("CBU: CbvRecord wrote %d calls (%.1f MB) to the trace file", g_recCalls, g_recLen / 1048576.0);
    } else if (g_log) {
        g_log("CBU: CbvRecord could not write the trace file");
    }
    return 0;
}

void RecPut(const void* p, size_t n) {
    if (g_recLen + n > g_recCap) return;
    memcpy(g_recBuf + g_recLen, p, n);
    g_recLen += n;
}

void Record(uint8_t* c, int* rootIdx, int stage, uint8_t* layout) {
    if (g_recState == 0) {
        if (!g_recStart) g_recStart = GetTickCount64();
        if (GetTickCount64() - g_recStart < (ULONGLONG)g_recDelay * 1000) return;
        g_recState = 1;
        if (g_log) g_log("CBU: CbvRecord started (%d calls)", g_recWant);
    }
    if (g_recState != 1) return;
    const uint32_t mask = At<uint32_t>(c, kMask + stage * 4), count = At<uint32_t>(layout, 0x40), n = At<uint32_t>(layout, 0x44);
    const uint32_t* ent = At<uint32_t*>(layout, 0x38);
    size_t need = 9 * 4 + 3 * 8 + count * 8;
    for (uint32_t i = 0; i < count; ++i)
        if (mask >> (ent[2 * i + 1] & 31) & 1) need += ent[2 * i];
    if (g_recLen + need > g_recCap) {
        g_recWant = g_recCalls;  // full: stop here
    } else {
        const uint32_t h[6] = {0x4C4C4143u, (uint32_t)stage, mask, count, n, (uint32_t)*rootIdx};
        RecPut(h, sizeof h);
        const uint64_t q[3] = {(uint64_t)(uintptr_t)c, At<uint64_t>(c, kTag), At<uint64_t>(c, kList)};
        RecPut(q, sizeof q);
        RecPut(ent, count * 8);
        for (uint32_t i = 0; i < count; ++i) {
            const uint32_t slot = ent[2 * i + 1];
            if (mask >> (slot & 31) & 1) RecPut(At<uint8_t*>(c, kShadow + (stage * kSlots + slot) * 8), ent[2 * i]);
        }
        ++g_recCalls;
    }
    if (g_recCalls >= g_recWant) {
        g_recState = 2;
        if (HANDLE t = CreateThread(nullptr, 0, RecWriter, nullptr, 0, nullptr)) CloseHandle(t);
    }
}

bool Store(Rec& r, const uint8_t* src, uint32_t size) {
    if (r.cap < size) {
        const uint32_t cap = (size + 255) & ~255u;
        uint8_t* p = static_cast<uint8_t*>(realloc(r.bytes, cap));
        if (!p) return false;
        r.bytes = p;
        r.cap = cap;
    }
    memcpy(r.bytes, src, size);
    r.size = size;
    return true;
}

}  // namespace
}  // namespace cbvupload

using namespace cbvupload;

namespace {
thread_local bool t_fence = false;  // CbuStreamCopy streamed since the last sfence (graphics site: CbuTableCall fences)

// F3: forward non-temporal copy. dst is a ring block (256-aligned in practice); src is the shadow (any alignment).
// Only [dst, dst + n) is written. No fence here: tests/cbs_bench.cpp on the RX 9070 measured an sfence after every copy at
// ~+400 ns per upload (it waits for the write-combining buffers to reach the BAR); the graphics site fences once per table
// (CbuTableCall, after SynUploadCbvTable returns: before the table can be used by a closed / submitted list), the compute
// site after each copy (CbuStreamCopyCs). A ring-full cut inside SynUploadCbvTable submits through the driver
// (ExecuteCommandLists: serialising), the same ordering the original's regular write-combined memcpy stores rely on.
inline bool StreamBody(void* dst, const void* src, size_t n) {
    if (!g_cbs || ((uintptr_t)dst & 15) != 0) return false;
    uint8_t* d = static_cast<uint8_t*>(dst);
    const uint8_t* s = static_cast<const uint8_t*>(src);
    size_t left = n;
    while (((uintptr_t)d & 63) && left >= 16) {  // up to the first full line
        _mm_stream_si128((__m128i*)d, _mm_loadu_si128((const __m128i*)s));
        d += 16, s += 16, left -= 16;
    }
    while (left >= 64) {  // whole lines, in order: one full write-combining buffer each
        const __m128i a = _mm_loadu_si128((const __m128i*)s), b = _mm_loadu_si128((const __m128i*)(s + 16));
        const __m128i e = _mm_loadu_si128((const __m128i*)(s + 32)), f = _mm_loadu_si128((const __m128i*)(s + 48));
        _mm_stream_si128((__m128i*)d, a);
        _mm_stream_si128((__m128i*)(d + 16), b);
        _mm_stream_si128((__m128i*)(d + 32), e);
        _mm_stream_si128((__m128i*)(d + 48), f);
        d += 64, s += 64, left -= 64;
    }
    while (left >= 16) {
        _mm_stream_si128((__m128i*)d, _mm_loadu_si128((const __m128i*)s));
        d += 16, s += 16, left -= 16;
    }
    for (; left; --left) *d++ = *s++;  // cbuffer sizes are multiples of 16: never in practice
    if (g_stats) {
        InterlockedIncrement64(&g_copies);
        InterlockedAdd64(&g_copyBytes, (LONG64)n);
    }
    return true;
}

inline void CallOrig(uint8_t* c, int* rootIdx, int stage, uint8_t* layout, uint64_t* fenceCache) {
    g_orig(c, rootIdx, stage, layout, fenceCache);
    if (t_fence) {
        if (g_cbsFence) _mm_sfence();
        t_fence = false;
    }
}
}  // namespace

extern "C" void* CbuStreamCopy(void* dst, const void* src, size_t n) {
    if (!StreamBody(dst, src, n)) memcpy(dst, src, n);
    else t_fence = true;
    cbvstage::NoteWrite(dst, n);  // CBF: ranges written into the staging ring (no-op otherwise)
    return dst;
}
extern "C" void* CbuStreamCopyCs(void* dst, const void* src, size_t n) {
    if (!StreamBody(dst, src, n)) memcpy(dst, src, n);
    else if (g_cbsFence) _mm_sfence();
    cbvstage::NoteWrite(dst, n);
    return dst;
}

// F1 / F4 / stats / record: replaces the call SynFlushCbvTables -> SynUploadCbvTable (118E19).
extern "C" void CbuTableCall(uint8_t* c, int* rootIdx, int stage, uint8_t* layout, uint64_t* fenceCache) {
    if (g_recWant > 0 && g_recState != 2) Record(c, rootIdx, stage, layout);
    cbvstage::OnTable(c);  // CBF: learns the list, creates the rings, swaps / restores the ring bases (no-op unless installed)
    const uint32_t cbn = g_cbn, cbt = g_cbt, st = g_stats;
    Ctx* x = (cbn || cbt || st) && stage >= 0 && stage < kStages ? Find(c) : nullptr;
    if (!x) {
        // tracking off: forget everything seen so far (the next tracked call starts a new epoch)
        for (int i = 0; i < kMaxCtx; ++i) {
            Ctx* z = g_ctx[i];
            if (!z) break;
            if (z->c == c) z->tracking = false;
        }
        CallOrig(c, rootIdx, stage, layout, fenceCache);
        return;
    }
    if (!x->tracking) {
        ++x->epoch;
        x->tracking = true;
    }
    Counts& k = x->n;
    ++k.calls;
    const uint64_t tag = At<uint64_t>(c, kTag), list = At<uint64_t>(c, kList), stag = At<uint64_t>(c, kStagBase);
    uint32_t* maskp = &At<uint32_t>(c, kMask + stage * 4);
    const uint32_t mask0 = *maskp, count = At<uint32_t>(layout, 0x40), n = At<uint32_t>(layout, 0x44);
    const uint32_t* ent = At<uint32_t*>(layout, 0x38);
    uint32_t mask = mask0, maskWould = mask0;  // maskWould: as if CBN were on (CBT candidates in the census)
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t size = ent[2 * i], slot = ent[2 * i + 1];
        if (!(mask0 >> (slot & 31) & 1) || slot >= (uint32_t)kSlots) continue;
        const int idx = stage * kSlots + (int)slot;
        Rec& r = x->rec[idx];
        ++k.uploads;
        k.bytes += size;
        const uint8_t* sh = At<uint8_t*>(c, kShadow + idx * 8);
        const bool eq = r.epoch == x->epoch && r.size == size && sh && memcmp(r.bytes, sh, size) == 0;
        if (!eq) continue;
        ++k.noopAny;
        if (r.tag != tag || r.list != list || r.stag != stag || At<uint32_t>(c, kSizes + idx * 4) != size) continue;
        ++k.noopSame;
        k.noopSameBytes += size;
        maskWould &= ~(1u << (slot & 31));
        if (cbn) {
            mask &= ~(1u << (slot & 31));
            ++k.skipped;
            k.skippedBytes += size;
        }
    }
    // F4: nothing to upload any more -> reuse the stage's last table copy (same list, same content)
    Tbl& t = x->tbl[stage];
    const int root = *rootIdx;
    if ((maskWould & 0x7FFFFFFFu) == 0 && n && t.epoch == x->epoch && t.tag == tag && t.list == list && t.stag == stag && t.n == n &&
        t.root == root) {
        ++k.tblCand;
        if (cbt && (mask & 0x7FFFFFFFu) == 0) {
            ++k.tblHit;
            void* l = reinterpret_cast<void*>(list);
            using SetTbl = void(__stdcall*)(void*, uint32_t, uint64_t);
            (*reinterpret_cast<SetTbl**>(l))[0x100 / 8](l, (uint32_t)(root + stage), t.handle);
            At<uint32_t>(c, kTblN + stage * 4) = n;
            *maskp = mask;  // the caller zeroes it next
            return;
        }
    }
    *maskp = mask;
    uint8_t* ring = c + kRing;
    const uint32_t cur0 = At<uint32_t>(ring, 0x44), wraps0 = At<uint32_t>(ring, 0x4C);
    CallOrig(c, rootIdx, stage, layout, fenceCache);
    const uint64_t tag2 = At<uint64_t>(c, kTag), list2 = At<uint64_t>(c, kList);
    if (tag2 != tag || list2 != list) {
        // the list was cut inside (ring full): blocks made before the cut belong to the old list -> forget this stage.
        // A slot CBN skipped still points at its block of the OLD list, which the cut has just retired, while the table
        // bound in the new list reads it (stock would have uploaded it again, after the cut for every slot behind the cut
        // point). Upload exactly the skipped slots once more in the new list and bind the table again (the second bind
        // replaces the first before any draw): afterwards every skipped slot is on a block of the new list.
        ++k.cuts;
        for (int s = 0; s < kSlots; ++s) x->rec[stage * kSlots + s].epoch = 0;
        t.epoch = 0;
        const uint32_t redo = mask0 & ~mask & 0x7FFFFFFFu;
        if (redo) {
            ++k.cutRedo;
            *maskp = redo;
            CallOrig(c, rootIdx, stage, layout, fenceCache);
        }
        return;
    }
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t size = ent[2 * i], slot = ent[2 * i + 1];
        if (!(mask >> (slot & 31) & 1) || slot >= (uint32_t)kSlots) continue;
        const int idx = stage * kSlots + (int)slot;
        Rec& r = x->rec[idx];
        const uint8_t* sh = At<uint8_t*>(c, kShadow + idx * 8);
        if (sh && Store(r, sh, size)) {
            r.epoch = x->epoch;
            r.tag = tag;
            r.list = list;
            r.stag = stag;
        } else {
            r.epoch = 0;
        }
    }
    // the table the original just copied: ShaderVisibleDescRingAlloc returns base + cursor (the early wrap only resets
    // the cursor after the allocation); verify through the cursor and the per-descriptor fence stamps
    const uint32_t cur1 = At<uint32_t>(ring, 0x44), wraps1 = At<uint32_t>(ring, 0x4C);
    const uint64_t* stamps = At<uint64_t*>(ring, 0x30);
    bool ok = n != 0 && (cur1 == cur0 + n || (cur1 == 0 && wraps1 == wraps0 + 1));
    for (uint32_t i = 0; ok && i < n; ++i) ok = stamps[cur0 + i] == tag;
    if (ok) {
        const uint32_t idxT = At<uint32_t>(ring, 0x38) + cur0;
        t.handle = (uint64_t)(uint32_t)(idxT * At<uint32_t>(c, kRingInc)) + At<uint64_t>(c, kRingGpu);
        t.epoch = x->epoch;
        t.tag = tag;
        t.list = list;
        t.stag = stag;
        t.n = n;
        t.root = root;
    } else {
        t.epoch = 0;
    }
}

namespace cbvupload {
namespace {

uint8_t* AllocIn(uintptr_t from, uintptr_t to, size_t size) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    const uintptr_t g = si.dwAllocationGranularity;
    MEMORY_BASIC_INFORMATION mbi;
    for (uintptr_t a = (from + g - 1) & ~(g - 1); a < to;) {
        if (!VirtualQuery((void*)a, &mbi, sizeof(mbi))) break;
        const uintptr_t rb = (uintptr_t)mbi.BaseAddress, re = rb + mbi.RegionSize;
        if (mbi.State == MEM_FREE) {
            const uintptr_t c = (rb + g - 1) & ~(g - 1);
            if (c >= from && c + size <= re && c + size <= to)
                if (void* p = VirtualAlloc((void*)c, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE)) return (uint8_t*)p;
        }
        if (re <= a) break;
        a = re;
    }
    return nullptr;
}
uint8_t* AllocNear(uintptr_t lo, uintptr_t hi, size_t size) {
    const uintptr_t reach = 0x7FF00000ull - (hi - lo);
    if (uint8_t* p = AllocIn(hi, hi + reach, size)) return p;
    return AllocIn(lo > reach + 0x10000 ? lo - reach : 0x10000, lo, size);
}

// 5-byte call rewrite with every other thread suspended and none stopped inside the site (psomemo.cpp FrozenWrite5).
bool FrozenWrite5(uint8_t* at, const uint8_t* bytes) {
    DWORD old = 0;
    if (!VirtualProtect(at, 5, PAGE_EXECUTE_READWRITE, &old)) return false;
    const DWORD self = GetCurrentThreadId(), pid = GetCurrentProcessId();
    constexpr int kMaxT = 1024;
    static DWORD tids[kMaxT];
    static HANDLE hs[kMaxT];
    static bool sus[kMaxT];
    bool written = false;
    for (int attempt = 0; attempt < 200 && !written; ++attempt) {
        int nt = 0;
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snap != INVALID_HANDLE_VALUE) {
            THREADENTRY32 te{};
            te.dwSize = sizeof(te);
            for (BOOL ok = Thread32First(snap, &te); ok && nt < kMaxT; ok = Thread32Next(snap, &te))
                if (te.th32OwnerProcessID == pid && te.th32ThreadID != self) tids[nt++] = te.th32ThreadID;
            CloseHandle(snap);
        }
        int nh = 0;
        for (int i = 0; i < nt; ++i)
            if (HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, tids[i])) hs[nh++] = h;
        // ---- frozen section (no heap / CRT) ----
        for (int i = 0; i < nh; ++i) sus[i] = SuspendThread(hs[i]) != (DWORD)-1;
        bool clash = false;
        for (int i = 0; i < nh && !clash; ++i) {
            if (!sus[i]) continue;
            CONTEXT ctx{};
            ctx.ContextFlags = CONTEXT_CONTROL;
            if (!GetThreadContext(hs[i], &ctx)) continue;
            if (ctx.Rip > (uintptr_t)at && ctx.Rip < (uintptr_t)at + 5) clash = true;
        }
        if (!clash) {
            memcpy(at, bytes, 5);
            FlushInstructionCache(GetCurrentProcess(), at, 5);
            written = true;
        }
        for (int i = 0; i < nh; ++i)
            if (sus[i]) ResumeThread(hs[i]);
        // ---- end frozen section ----
        for (int i = 0; i < nh; ++i) CloseHandle(hs[i]);
        if (!written) Sleep(1 + attempt % 4);
    }
    VirtualProtect(at, 5, old, &old);
    return written && memcmp(at, bytes, 5) == 0;
}

uint32_t Fnv(uintptr_t base, const Region& r) {
    uint32_t h = 0x811C9DC5u;
    const uint8_t* p = (const uint8_t*)(base + r.rva);
    for (uint32_t i = 0; i < r.len; ++i) h = (h ^ p[i]) * 0x01000193u;
    return h;
}

// E8 rel32 at `site` that targets `callee`?
bool CallsTo(uintptr_t base, uint32_t site, uint32_t callee) {
    const uint8_t* p = (const uint8_t*)(base + site);
    int32_t rel;
    memcpy(&rel, p + 1, 4);
    return p[0] == 0xE8 && base + site + 5 + (intptr_t)rel == base + callee;
}

// relay `mov rax, imm64; jmp rax` at `at` (rax: no argument, return value only), then E8 rel32 site -> relay
bool Retarget(uintptr_t base, uint32_t site, uint8_t* at, const void* fn) {
    at[0] = 0x48;
    at[1] = 0xB8;
    const uint64_t f = (uint64_t)(uintptr_t)fn;
    memcpy(at + 2, &f, 8);
    at[10] = 0xFF;
    at[11] = 0xE0;
    FlushInstructionCache(GetCurrentProcess(), at, 16);
    uint8_t* s = (uint8_t*)(base + site);
    const intptr_t d = (intptr_t)at - (intptr_t)(s + 5);
    if (d < INT32_MIN || d > INT32_MAX) return false;
    uint8_t nb[5] = {0xE8, 0, 0, 0, 0};
    const int32_t r = (int32_t)d;
    memcpy(nb + 1, &r, 4);
    return FrozenWrite5(s, nb);
}

Counts Sum() {
    Counts s;
    for (int i = 0; i < kMaxCtx; ++i) {
        Ctx* x = g_ctx[i];
        if (!x) break;
        s.calls += x->n.calls, s.uploads += x->n.uploads, s.bytes += x->n.bytes, s.noopAny += x->n.noopAny;
        s.noopSame += x->n.noopSame, s.noopSameBytes += x->n.noopSameBytes, s.skipped += x->n.skipped, s.skippedBytes += x->n.skippedBytes;
        s.tblCand += x->n.tblCand, s.tblHit += x->n.tblHit, s.cuts += x->n.cuts, s.cutRedo += x->n.cutRedo;
    }
    return s;
}

DWORD WINAPI StatsThread(LPVOID) {
    Counts p = Sum();
    LONG64 pc = g_copies, pb = g_copyBytes;
    LARGE_INTEGER f, t0, t1;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t0);
    for (;;) {
        Sleep((DWORD)g_statsSec * 1000);
        QueryPerformanceCounter(&t1);
        const double s = (double)(t1.QuadPart - t0.QuadPart) / (double)f.QuadPart;
        t0 = t1;
        const Counts c = Sum();
        const LONG64 cc = g_copies, cb = g_copyBytes;
        if (cbvstage::Installed() && g_log) {
            char fb[384];
            cbvstage::StatsLine(fb, sizeof fb, s);
            if (fb[0]) g_log("CBU %s", fb);
        }
        if (!g_stats) {
            p = c, pc = cc, pb = cb;
            continue;
        }
        const double up = (double)(c.uploads - p.uploads), by = (double)(c.bytes - p.bytes);
        const double tb = (double)(c.calls - p.calls);
        auto pct = [](double a, double b) { return b > 0 ? 100.0 * a / b : 0.0; };
        if (g_log)
            g_log("CBU: %.1f s | switches CBS %u CBN %u CBT %u fence %u | tables %.0f/s, uploads %.0f/s (%.2f MB/s, avg %.0f B) | no-op uploads: "
                  "any list %.1f %%, same list %.1f %% (%.2f MB/s) | skipped %.0f/s (%.2f MB/s) | table reuse candidates %.1f %% of "
                  "tables, hits %.0f/s | list cuts in a table %.0f (re-uploaded %.0f) | CBS copies %.0f/s (%.2f MB/s)",
                  s, g_cbs, g_cbn, g_cbt, g_cbsFence, tb / s, up / s, by / s / 1048576.0, up > 0 ? by / up : 0.0, pct((double)(c.noopAny - p.noopAny), up),
                  pct((double)(c.noopSame - p.noopSame), up), (double)(c.noopSameBytes - p.noopSameBytes) / s / 1048576.0,
                  (double)(c.skipped - p.skipped) / s, (double)(c.skippedBytes - p.skippedBytes) / s / 1048576.0,
                  pct((double)(c.tblCand - p.tblCand), tb), (double)(c.tblHit - p.tblHit) / s, (double)(c.cuts - p.cuts), (double)(c.cutRedo - p.cutRedo),
                  (double)(cc - pc) / s, (double)(cb - pb) / s / 1048576.0);
        p = c, pc = cc, pb = cb;
    }
}

int Key(const wchar_t* sec, const wchar_t* k, int def, const wchar_t* ini) { return (int)GetPrivateProfileIntW(sec, k, def, ini); }

}  // namespace

#ifdef CBU_HARNESS
void TestReset() {  // forget every context (each harness run uses fresh mock SynCtx objects)
    for (int i = 0; i < kMaxCtx; ++i) {
        if (Ctx* x = g_ctx[i]) {
            for (Rec& r : x->rec) free(r.bytes);
            delete x;
        }
        g_ctx[i] = nullptr;
    }
}
void TestSetup(UploadFn orig, int cbs, int cbn, int cbt, int stats) {
    g_orig = orig;
    g_cbs = (uint32_t)cbs, g_cbn = (uint32_t)cbn, g_cbt = (uint32_t)cbt, g_stats = (uint32_t)stats;
    g_instCbs = g_instTbl = g_instCbn = g_instCbt = true;
}
TestCounts TestGet() {
    const Counts c = Sum();
    return TestCounts{c.calls, c.uploads, c.bytes, c.noopAny, c.noopSame, c.skipped, c.skippedBytes, c.tblCand, c.tblHit,
                      (uint64_t)g_copies, (uint64_t)g_copyBytes};
}
#endif

bool SetTuning(const char* key, const char* val, char* reply, size_t n) {
    volatile uint32_t* sw = nullptr;
    bool inst = false;
    const char* name = nullptr;
    if (!_stricmp(key, "CBS")) sw = &g_cbs, inst = g_instCbs, name = "CBS";
    else if (!_stricmp(key, "CBN")) sw = &g_cbn, inst = g_instCbn, name = "CBN";
    else if (!_stricmp(key, "CBT")) sw = &g_cbt, inst = g_instCbt, name = "CBT";
    else if (!_stricmp(key, "CBSTATS")) sw = &g_stats, inst = g_instTbl, name = "CBSTATS";
    else if (!_stricmp(key, "CBS_FENCE")) sw = &g_cbsFence, inst = g_instCbs, name = "CBS_FENCE";
    else if (!_stricmp(key, "CBF")) sw = cbvstage::Switch(), inst = sw != nullptr, name = "CBF";
    else if (!_stricmp(key, "CBF_QUEUE")) sw = cbvstage::QueueSwitch(), inst = sw != nullptr, name = "CBF_QUEUE";
    else if (!_stricmp(key, "CBMODE")) {  // one key for one-launch A/B of combinations: 1 CBS, 2 CBN, 4 CBT, 8 CBS without sfence, 16 stats
        const int v = atoi(val);
        if (v < 0 || v > 127) {
            snprintf(reply, n, "CBMODE must be 0..127 (got %s)", val);
            return true;
        }
        InterlockedExchange(reinterpret_cast<volatile LONG*>(&g_cbsFence), (v & 8) ? 0 : 1);
        if (g_instCbs) InterlockedExchange(reinterpret_cast<volatile LONG*>(&g_cbs), (v & 9) ? 1 : 0);
        if (g_instCbn) InterlockedExchange(reinterpret_cast<volatile LONG*>(&g_cbn), (v & 2) ? 1 : 0);
        if (g_instCbt) InterlockedExchange(reinterpret_cast<volatile LONG*>(&g_cbt), (v & 4) ? 1 : 0);
        if (g_instTbl) InterlockedExchange(reinterpret_cast<volatile LONG*>(&g_stats), (v & 16) ? 1 : 0);
        if (volatile uint32_t* f = cbvstage::Switch()) InterlockedExchange(reinterpret_cast<volatile LONG*>(f), (v & 32) ? 1 : 0);
        if (volatile uint32_t* f = cbvstage::QueueSwitch()) InterlockedExchange(reinterpret_cast<volatile LONG*>(f), (v & 64) ? 1 : 0);
        snprintf(reply, n, "CBMODE = %d", v);
        if (g_log)
            g_log("LIVE tuning: %s (CBS %u fence %u, CBN %u, CBT %u, stats %u, CBF %u queue %u)", reply, g_cbs, g_cbsFence, g_cbn, g_cbt, g_stats,
                  cbvstage::Switch() ? *cbvstage::Switch() : 0u, cbvstage::QueueSwitch() ? *cbvstage::QueueSwitch() : 0u);
        return true;
    }
    else if (!_stricmp(key, "CBALL")) {  // every installed CBU switch at once (CBS + CBN + CBT)
        const int v = atoi(val);
        if (v != 0 && v != 1) {
            snprintf(reply, n, "CBALL must be 0 or 1 (got %s)", val);
            return true;
        }
        if (g_instCbs) InterlockedExchange(reinterpret_cast<volatile LONG*>(&g_cbs), v);
        if (g_instCbn) InterlockedExchange(reinterpret_cast<volatile LONG*>(&g_cbn), v);
        if (g_instCbt) InterlockedExchange(reinterpret_cast<volatile LONG*>(&g_cbt), v);
        snprintf(reply, n, "CBALL = %d", v);
        if (g_log) g_log("LIVE tuning: %s (CBS %u, CBN %u, CBT %u)", reply, g_cbs, g_cbn, g_cbt);
        return true;
    }
    else return false;
    const int v = atoi(val);
    if (v != 0 && v != 1) {
        snprintf(reply, n, "%s must be 0 or 1 (got %s)", name, val);
        return true;
    }
    if (!inst || !sw) {
        snprintf(reply, n, "%s = 0 (not installed: [Patches] / [Profiler] key is 0 at boot, or the sites differ)", name);
        if (g_log) g_log("LIVE tuning: %s", reply);
        return true;
    }
    InterlockedExchange(reinterpret_cast<volatile LONG*>(sw), v);
    snprintf(reply, n, "%s = %d", name, v);
    if (g_log) g_log("LIVE tuning: %s", reply);
    return true;
}

void StatusLine(char* buf, unsigned n) {
    if (!n) return;
    buf[0] = 0;
    if (!g_instCbs && !g_instTbl) return;
    snprintf(buf, n, "CBU: CBS %s, CBN %s, CBT %s, stats %s", !g_instCbs ? "-" : g_cbs ? "ON" : "off", !g_instCbn ? "-" : g_cbn ? "ON" : "off",
             !g_instCbt ? "-" : g_cbt ? "ON" : "off", !g_instTbl ? "-" : g_stats ? "ON" : "off");
}

void Start(const StartInfo& info) {
    g_log = info.log;
    if (!g_log || !info.iniPath || !info.base) return;
    const wchar_t* ini = info.iniPath;
    const int cbs = Key(L"Patches", L"CBS", 0, ini), cbn = Key(L"Patches", L"CBN", 0, ini), cbt = Key(L"Patches", L"CBT", 0, ini);
    const int stats = Key(L"Profiler", L"CbvStats", 0, ini), rec = Key(L"Profiler", L"CbvRecord", 0, ini);
    if (!cbs && !cbn && !cbt && !stats && rec <= 0 && !Key(L"Patches", L"CBF", 0, ini)) return;
    if (!info.knownBuild) {
        g_log("CBU: v2.02 only - not installed");
        return;
    }
    const uintptr_t b = info.base;
    bool wantCbs = cbs == 1 || cbs == 2, wantCbn = cbn == 1 || cbn == 2, wantCbt = cbt == 1 || cbt == 2;
    const bool fnOk = Fnv(b, kFlushFn) == kFlushFn.fnv && Fnv(b, kUploadFn) == kUploadFn.fnv && Fnv(b, kDescFn) == kDescFn.fnv &&
                      CallsTo(b, kTblSite, kUpload) && CallsTo(b, kGfxCopy, kMemcpy);
    const bool csOk = Fnv(b, kCsFn) == kCsFn.fnv && CallsTo(b, kCsCopy, kMemcpy);
    if (!fnOk) {
        g_log("CBU: SynFlushCbvTables / SynUploadCbvTable / ShaderVisibleDescRingAlloc differ from v2.02 (another mod?) - not installed");
        return;
    }
    if ((wantCbn || wantCbt) && Key(L"GpuOpt", L"CbvMirror", 0, ini) != 0) {
        g_log("CBU: CBN / CBT not installed: [GpuOpt] CbvMirror != 0 (mirror CBVs have their own lifetime)");
        wantCbn = wantCbt = false;
    }
    int cbf = Key(L"Patches", L"CBF", 0, ini);
    if (cbf != 1 && cbf != 2) cbf = 0;
    if (cbf && !csOk) {
        g_log("CBU: CBF not installed: CmdListUploadComputeCbvTable 4959EC differs (its ring writes could not be noted)");
        cbf = 0;
    }
    if (cbf && Key(L"GpuOpt", L"CbvMirror", 0, ini) != 0) {
        g_log("CBU: CBF not installed: [GpuOpt] CbvMirror != 0");
        cbf = 0;
    }
    const bool wantCbf = cbf != 0;
    const bool wantTbl = wantCbs || wantCbf || wantCbn || wantCbt || stats != 0 || rec > 0;  // CBS: the wrapper issues the sfence
    if (!wantCbs && !wantTbl) return;  // (wantTbl covers CBF)
    const auto* dos = (const IMAGE_DOS_HEADER*)b;
    const auto* nt = (const IMAGE_NT_HEADERS64*)(b + dos->e_lfanew);
    uint8_t* page = AllocNear(b, b + nt->OptionalHeader.SizeOfImage, 4096);
    if (!page) {
        g_log("CBU: no executable page within 2 GB of the image - not installed");
        return;
    }
    g_orig = (UploadFn)(b + kUpload);
    g_statsSec = Key(L"Profiler", L"CbvStatsSec", 10, ini) < 2 ? 2 : Key(L"Profiler", L"CbvStatsSec", 10, ini);
    g_cbsFence = Key(L"Tuning", L"CBS_Fence", 1, ini) ? 1u : 0u;
    if (rec > 0) {
        g_recWant = rec;
        g_recDelay = Key(L"Profiler", L"CbvRecordDelaySec", 60, ini);
        g_recCap = (size_t)256 << 20;
        g_recBuf = static_cast<uint8_t*>(VirtualAlloc(nullptr, g_recCap, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        wcsncpy_s(g_recPath, ini, _TRUNCATE);
        if (wchar_t* sl = wcsrchr(g_recPath, L'\\')) wcscpy_s(sl + 1, MAX_PATH - (sl + 1 - g_recPath), L"Nioh3EngineFix_cbvtrace.bin");
        if (!g_recBuf) g_recWant = 0;
    }
    g_cbs = cbs == 1 ? 1u : 0u;
    g_cbn = cbn == 1 ? 1u : 0u;
    g_cbt = cbt == 1 ? 1u : 0u;
    g_stats = stats ? 1u : 0u;
    if (!wantCbn) g_cbn = 0;
    if (!wantCbt) g_cbt = 0;
    if (wantTbl) {
        if (Retarget(b, kTblSite, page, (const void*)&CbuTableCall)) {
            g_instTbl = true;
            g_instCbn = wantCbn;
            g_instCbt = wantCbt;
        } else {
            g_log("CBU: 118E19 write failed - CBN / CBT / stats not installed");
            g_cbn = g_cbt = g_stats = 0;
            g_recWant = 0;
        }
    }
    bool relays = false;
    if (wantCbs || wantCbf) {
        bool ok = Retarget(b, kGfxCopy, page + 0x20, (const void*)&CbuStreamCopy);
        if (ok && !g_instTbl) ok = false;  // the graphics copies need the wrapper's sfence / CBF its table hook
        const bool okGfx = ok;
        if (ok && csOk) ok = Retarget(b, kCsCopy, page + 0x40, (const void*)&CbuStreamCopyCs);
        relays = ok && csOk;
        g_instCbs = wantCbs && ok;
        if (!g_instCbs) g_cbs = 0;
        if (wantCbs)
            g_log("CBU: CBS streaming copy %s at 118F4D%s (switch %s)", g_instCbs ? "installed" : "NOT installed",
                  csOk ? " + 495AF2" : " (compute 4959EC differs: graphics only)", g_cbs ? "ON" : "off");
        if (wantCbf && !(okGfx && relays)) g_log("CBU: CBF not installed: copy relays not written");
    }
    if (wantCbf && relays) {
        cbvstage::Configure(g_log, cbf, Key(L"Tuning", L"CBF_Queue", 0, ini));
        g_log("CBU: CBF staging ring installed (switch %s, %s); rings are made at the first table call after the game queue is known",
              cbf == 1 ? "ON" : "off", Key(L"Tuning", L"CBF_Queue", 0, ini) ? "COPY queue + wait" : "DIRECT prologue");
    }
    if (g_instTbl)
        g_log("CBU: table wrapper at 118E19: CBN %s, CBT %s, stats %s (every %d s), record %d calls after %d s", !g_instCbn ? "-" : g_cbn ? "ON" : "off",
              !g_instCbt ? "-" : g_cbt ? "ON" : "off", g_stats ? "ON" : "off", g_statsSec, g_recWant, g_recDelay);
    if (g_instTbl || g_instCbs) {
        if (HANDLE t = CreateThread(nullptr, 0, StatsThread, nullptr, 0, nullptr)) CloseHandle(t);
    }
}

}  // namespace cbvupload
