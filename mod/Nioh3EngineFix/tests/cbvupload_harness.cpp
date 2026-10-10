// cbvupload_harness.cpp - offline test of src/cbvupload.cpp (CBS / CBN / CBT) against the REAL v2.02 code of
// SynUploadCbvTable 118E28 and ShaderVisibleDescRingAlloc 1192B0, copied from re\Nioh3.dump.exe and relocated into this
// process (rel32 calls -> mocks; the stack-cookie load -> a local). Not a game test. Built and run by
// tests/run_cbvupload_harness.py [trace.bin ...].
//
// Two arms run the same input stream on their own mock SynCtx (ring memory, staging / shader-visible descriptor heaps,
// fence + list model):
//   stock   = SynFlushCbvTables' call to the relocated original (memcpy at 118F4D),
//   patched = CbuTableCall (cbvupload.cpp) -> relocated original whose 118F4D call goes to CbuStreamCopy.
// Every SetGraphicsRootDescriptorTable is a "bind": the harness resolves the bound table (shader-visible ring) -> CBV
// (GPU VA, size) -> ring bytes, for every slot the layout declares, and checks
//   (1) patched bytes == stock bytes at every bind (where the stock view's ring block is still owned),
//   (2) a stock view that is safe (its ring block belongs to the open list or a later one) is safe in the patched arm,
//   (3) at every list close, in both arms, every safe view and every bound table still holds what it held at bind time
//       (no ring block / descriptor-ring entry was reused under an open list),
//   (4) CBS: CbuStreamCopy == memcpy byte for byte on [0, n) for n = 0..4160 x 16 source alignments, and never writes
//       outside [dst, dst + n).
// Inputs: synthetic streams (random layouts, no-op / changed / untouched slot writes, shader changes, list closes, GPU
// completion lag, small rings so the ring-full submit path and wraps run) and recorded game traces (CbvRecord).
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <random>
#include <string>
#include <vector>

#define CBU_HARNESS 1
#include "../src/cbvupload.h"
#include "../src/cbvstage.h"

static int g_fail = 0;
#define CHECK(c, ...)                                   \
    do {                                                \
        if (!(c)) {                                     \
            if (g_fail < 30) {                          \
                printf("FAIL %s:%d: ", __FILE__, __LINE__); \
                printf(__VA_ARGS__);                    \
                printf("\n");                           \
            }                                           \
            ++g_fail;                                   \
        }                                               \
    } while (0)

// ---------------------------------------------------------------- dump + relocation
static std::vector<uint8_t> g_dump;
static uint32_t Fnv(uint32_t rva, uint32_t n) {
    uint32_t h = 0x811C9DC5u;
    for (uint32_t i = 0; i < n; ++i) h = (h ^ g_dump[rva + i]) * 0x01000193u;
    return h;
}

constexpr uint32_t kFn = 0x118E28, kFnLen = 0x297, kDesc = 0x1192B0, kDescLen = 0xB4;
struct Code {
    uint8_t* mem = nullptr;
    uint64_t cookie = 0x2B992DDFA232ull;
};

// mocks (defined below)
extern "C" {
static bool MockRingAlloc(uint8_t* ring, uint64_t size, uint64_t tag, uint64_t completed, uint64_t* out, uint64_t* retry);
static void MockSubmit(uint8_t* listRing, uint8_t* pool, uint64_t fence, uint8_t flag);
static void MockWait(uint8_t* c, uint64_t fence);
static void MockRefresh(uint8_t* c);
static void MockCookie(uint64_t) {}
}

// call sites inside 118E28 (rva) -> target kind
struct Fix {
    uint32_t at;
    int kind;  // 0 ring alloc, 1 submit, 2 wait, 3 refresh, 4 memcpy, 5 desc alloc, 6 cookie check
};
static const Fix kFix[] = {{0x118EE8, 0}, {0x118F06, 1}, {0x118F12, 2}, {0x118F1A, 3}, {0x118F4D, 4}, {0x118FD5, 5},
                           {0x118FF5, 1}, {0x119001, 2}, {0x119009, 3}, {0x11902B, 5}, {0x1190A6, 6}};

static Code Build(void* memcpyTarget) {
    Code k;
    k.mem = (uint8_t*)VirtualAlloc(nullptr, 0x2000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    memcpy(k.mem, &g_dump[kFn], kFnLen);
    uint8_t* desc = k.mem + 0x400;
    memcpy(desc, &g_dump[kDesc], kDescLen);
    uint8_t* stubs = k.mem + 0x800;
    uint64_t* cookie = (uint64_t*)(k.mem + 0xC00);
    *cookie = k.cookie;
    const void* tg[7] = {(void*)&MockRingAlloc, (void*)&MockSubmit, (void*)&MockWait, (void*)&MockRefresh, memcpyTarget, desc,
                         (void*)&MockCookie};
    for (int i = 0; i < 7; ++i) {  // jmp qword [rip+0]; dq target
        uint8_t* s = stubs + i * 16;
        s[0] = 0xFF, s[1] = 0x25;
        memset(s + 2, 0, 4);
        memcpy(s + 6, &tg[i], 8);
    }
    for (const Fix& f : kFix) {
        uint8_t* p = k.mem + (f.at - kFn);
        CHECK(p[0] == 0xE8, "no call at %X", f.at);
        int32_t old;
        memcpy(&old, p + 1, 4);
        const uint32_t tgt = f.at + 5 + old;
        const uint32_t want[7] = {0x1190C0, 0x945664, 0x945610, 0x9C9EF0, 0xB94250, 0x1192B0, 0xB912E0};
        CHECK(tgt == want[f.kind], "call at %X goes to %X", f.at, tgt);
        const int32_t r = (int32_t)((stubs + f.kind * 16) - (p + 5));
        memcpy(p + 1, &r, 4);
    }
    // 118E41 mov rax,[rip+disp32] (__security_cookie) -> the local cookie
    uint8_t* q = k.mem + (0x118E41 - kFn);
    CHECK(q[0] == 0x48 && q[1] == 0x8B && q[2] == 0x05, "cookie load moved");
    const int32_t r = (int32_t)((uint8_t*)cookie - (q + 7));
    memcpy(q + 3, &r, 4);
    FlushInstructionCache(GetCurrentProcess(), k.mem, 0x2000);
    return k;
}

// ---------------------------------------------------------------- mock SynCtx
constexpr uint32_t kDescSz = 32;  // mock descriptor: {u64 va, u32 size, pad}
struct MockObj {
    void** vtbl;
    struct Sim* sim;
};
struct View {
    uint64_t va = 0;
    uint32_t size = 0;
    uint64_t ser = 0;  // allocation serial of the CBV's block
};
struct BindRec {
    uint32_t root = 0;
    uint32_t svIdx = 0, n = 0;
    std::vector<uint8_t> svBytes;  // the n descriptors at bind time
    std::vector<View> views;       // layout slots only
    std::vector<std::vector<uint8_t>> bytes;
    std::vector<int> safe;  // SafeOf at bind time
    std::vector<char> deferred;  // CBF: the view is on the VRAM ring, its bytes are what the copy left there at list close
    bool cut = false;       // this arm cut the list inside this table flush (ring full)
};

struct Sim {
    uint8_t* c = nullptr;
    // upload ring
    uint8_t* ring = nullptr;
    uint8_t* sys = nullptr;   // CBF: staging (CPU writes) and VRAM ring (CBVs), same offsets as the ring
    uint8_t* vram = nullptr;
    bool f8 = false;
    uint64_t ringCap = 0, head = 0;
    struct Blk {
        uint64_t off, size, tag;
    };
    std::vector<Blk> live;  // FIFO
    std::vector<uint64_t> granTag;  // per 256 B: tag of the owning block, 0 = free
    std::vector<uint64_t> granSer;  // per 256 B: allocation serial of the owning block, 0 = free
    uint64_t serial = 0;
    // heaps
    std::vector<uint8_t> staging, sv;
    std::vector<uint64_t> stamps;
    uint32_t svCap = 0;
    uint64_t svGpu = 0x100000000ull;
    MockObj dev{}, list[4]{};
    int listIdx = 0;
    uint64_t completed = 0;
    // binds
    std::vector<BindRec> binds;  // all binds of the run
    std::vector<size_t> openBinds;
    std::vector<uint32_t> curLayoutSizes;  // per bind: declared size per table entry (0 = not in layout)
    const std::vector<std::pair<uint32_t, uint32_t>>* curEnt = nullptr;
    int curStage = 0;
    uint64_t closes = 0, cutsInside = 0, ringFails = 0, redoBinds = 0;
    bool checkClose = true;
    bool tainted = false;  // a ring-full cut opened a list that was not re-dirtied yet (until the next normal close)
    const char* name = "";

    uint64_t& Tag() { return *(uint64_t*)(c + 0x6D0); }
};
static Sim* g_cur = nullptr;  // the arm being run (mocks without a SynCtx argument)

static void FreeDone(Sim& s) {
    while (!s.live.empty() && s.live.front().tag <= s.completed) {
        const auto& b = s.live.front();
        for (uint64_t g = b.off / 256; g < (b.off + b.size) / 256; ++g) s.granTag[g] = 0, s.granSer[g] = 0;
        memset(s.ring + b.off, 0xCD, b.size);  // poison: a freed block's bytes are gone
        if (s.f8) memset(s.sys + b.off, 0xCE, b.size), memset(s.vram + b.off, 0xCF, b.size);
        s.live.erase(s.live.begin());
    }
}

// 1 = the CBV's own ring block is alive and belongs to the open list (or a later one), 0 = alive but older (the engine's
// cross-list reuse of untouched slots), -1 = the block the CBV was made on is gone (freed / reused), -2 = no CBV
// ring offset of a GPU address on the UPLOAD ring or (CBF) on the VRAM ring
static bool OffOf(Sim& s, uint64_t va, uint64_t size, uint64_t& off) {
    if (va >= (uint64_t)s.ring && va + size <= (uint64_t)s.ring + s.ringCap) return off = va - (uint64_t)s.ring, true;
    if (s.vram && va >= (uint64_t)s.vram && va + size <= (uint64_t)s.vram + s.ringCap) return off = va - (uint64_t)s.vram, true;
    return false;
}
static int SafeOf(Sim& s, uint64_t va, uint32_t size, uint64_t ser) {
    uint64_t off;
    if (!OffOf(s, va, size, off)) return -2;
    uint64_t t = s.granTag[off / 256];
    if (!t || s.granSer[off / 256] != ser) return -1;
    return t >= s.Tag() ? 1 : 0;
}

static Sim* g_flushSim = nullptr;
static void F8Copy(uint64_t off, uint64_t len) { memcpy(g_flushSim->vram + off, g_flushSim->sys + off, len); }

static void CloseList(Sim& s) {
    if (s.f8) {  // the submission: CBF copies every noted staging range into the VRAM ring before the lists run
        g_flushSim = &s;
        cbvstage::TestFlush(&F8Copy);
        for (size_t bi : s.openBinds) {
            BindRec& b = s.binds[bi];
            for (size_t j = 0; j < b.views.size(); ++j)
                if (b.deferred[j] && b.safe[j] >= 0) {
                    // an older list's block (safe 0, the engine's cross-list reuse) may be gone by now: unguaranteed in
                    // stock too (stock's bind-time capture hides it), so such a view is not compared
                    if (SafeOf(s, b.views[j].va, b.views[j].size, b.views[j].ser) < 0) {
                        b.safe[j] = -1;
                        continue;
                    }
                    b.bytes[j].assign((uint8_t*)b.views[j].va, (uint8_t*)b.views[j].va + b.views[j].size);
                }
        }
    }
    // the GPU "executes" the list now: every bind made in it must still read what it read at bind time
    if (s.checkClose)
        for (size_t bi : s.openBinds) {
            BindRec& b = s.binds[bi];
            CHECK(memcmp(&s.sv[(size_t)b.svIdx * kDescSz], b.svBytes.data(), b.svBytes.size()) == 0, "%s: bound table %zu changed before list close",
                  s.name, bi);
            for (size_t j = 0; j < b.views.size(); ++j)
                if (b.safe[j] == 1 && !b.deferred[j])
                    CHECK(memcmp((void*)b.views[j].va, b.bytes[j].data(), b.bytes[j].size()) == 0, "%s: bind %zu view %zu overwritten before close",
                          s.name, bi, j);
        }
    s.openBinds.clear();
    ++s.closes;
    s.Tag()++;
    s.listIdx = (s.listIdx + 1) % 4;
    *(MockObj**)(s.c + 0x580) = &s.list[s.listIdx];
}

extern "C" {
static bool MockRingAlloc(uint8_t* ringStruct, uint64_t size, uint64_t tag, uint64_t completed, uint64_t* out, uint64_t* retry) {
    Sim& s = *g_cur;
    if (completed > s.completed) completed = s.completed;
    FreeDone(s);
    uint64_t at = s.head;
    if (at + size > s.ringCap) at = 0;
    // every granule of [at, at+size) must be free (or owned by nothing live)
    for (uint64_t g = at / 256; g < (at + size) / 256; ++g)
        if (s.granTag[g]) {
            ++s.ringFails;
            if (retry) *retry = s.granTag[g];
            return false;
        }
    if (at == 0 && s.head != 0 && s.head + size > s.ringCap) {
        // wrap: the tail end stays unused
    }
    ++s.serial;
    for (uint64_t g = at / 256; g < (at + size) / 256; ++g) s.granTag[g] = tag, s.granSer[g] = s.serial;
    s.live.push_back({at, size, tag});
    s.head = at + size;
    out[0] = *(uint64_t*)(ringStruct + 0x10) + at;  // the ring's CPU / GPU bases (CBF swaps them)
    out[1] = *(uint64_t*)(ringStruct + 0x18) + at;
    return true;
}
static void MockSubmit(uint8_t* listRing, uint8_t*, uint64_t fence, uint8_t) {
    Sim& s = *g_cur;
    CHECK(listRing == s.c + 0x5A0, "submit arg");
    if (fence >= s.Tag()) {
        ++s.cutsInside;
        s.tainted = true;
        CloseList(s);
    }
}
static void MockWait(uint8_t* c, uint64_t fence) {
    Sim& s = *g_cur;
    CHECK(c == s.c, "wait arg");
    if (fence > s.completed) s.completed = fence;
    if (s.completed >= s.Tag()) s.completed = s.Tag() - 1;
}
static void MockRefresh(uint8_t* c) {
    Sim& s = *g_cur;
    *(uint64_t*)(c + 0x2510) = s.completed;
}
static void __stdcall MockCreateCbv(MockObj* self, const uint64_t* desc, uint64_t cpu) {
    Sim& s = *self->sim;
    CHECK(cpu >= (uint64_t)s.staging.data() && cpu + kDescSz <= (uint64_t)s.staging.data() + s.staging.size(), "CBV handle out of the staging heap");
    uint8_t d[kDescSz] = {};
    memcpy(d, &desc[0], 8);
    const uint32_t sz = (uint32_t)desc[1];
    memcpy(d + 8, &sz, 4);
    uint64_t ser = 0;  // harness only: the allocation serial of the block (lifetime check)
    uint64_t off;
    if (OffOf(s, desc[0], 1, off)) ser = s.granSer[off / 256];
    memcpy(d + 16, &ser, 8);
    memcpy((void*)cpu, d, kDescSz);
}
static void __stdcall MockCopyDesc(MockObj* self, uint32_t n, uint64_t dst, uint64_t src, int type) {
    Sim& s = *self->sim;
    CHECK(type == 0, "heap type");
    CHECK(dst >= (uint64_t)s.sv.data() && dst + (uint64_t)n * kDescSz <= (uint64_t)s.sv.data() + s.sv.size(), "copy dst out of the sv heap");
    memcpy((void*)dst, (void*)src, (size_t)n * kDescSz);
}
static void __stdcall MockSetTable(MockObj* self, uint32_t root, uint64_t gpu) {
    Sim& s = *self->sim;
    CHECK(self == *(MockObj**)(s.c + 0x580), "SetGraphicsRootDescriptorTable on a list that is not open");
    BindRec b;
    b.root = root;
    CHECK(gpu >= s.svGpu && (gpu - s.svGpu) % kDescSz == 0, "table handle");
    b.svIdx = (uint32_t)((gpu - s.svGpu) / kDescSz);
    b.n = *(uint32_t*)(s.c + 0xB90 + s.curStage * 4);  // written after the call; fixed up by the caller
    s.binds.push_back(std::move(b));
    s.openBinds.push_back(s.binds.size() - 1);
}
}

static void* g_devVt[64];
static void* g_listVt[64];

static void InitSim(Sim& s, const char* name, uint64_t ringCap, uint32_t svCap) {
    s.name = name;
    s.c = (uint8_t*)calloc(1, 0x2600);
    s.ringCap = ringCap;
    s.ring = (uint8_t*)VirtualAlloc(nullptr, ringCap, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    memset(s.ring, 0xCD, ringCap);
    s.granTag.assign(ringCap / 256, 0);
    s.granSer.assign(ringCap / 256, 0);
    s.staging.assign(533 * kDescSz, 0);
    s.svCap = svCap;
    s.sv.assign((size_t)svCap * kDescSz, 0);
    s.stamps.assign(svCap, 0);
    s.dev = {g_devVt, &s};
    for (auto& l : s.list) l = {g_listVt, &s};
    uint8_t* c = s.c;
    *(MockObj**)(c + 0x580) = &s.list[0];
    *(uint64_t*)(c + 0x6D0) = 1;  // open list tag
    *(MockObj**)(c + 0x8E0) = &s.dev;
    *(MockObj**)(c + 0x8E8) = &s.dev;
    *(uint32_t*)(c + 0x938) = kDescSz;
    *(uint64_t*)(c + 0x940) = (uint64_t)s.staging.data();
    uint64_t* cb = (uint64_t*)(c + 0xB28);  // CB ring struct: [1] size, [2] CPU base, [3] GPU base (UploadRingAllocate reads them)
    cb[1] = ringCap;
    cb[2] = cb[3] = (uint64_t)s.ring;
    uint8_t* r = c + 0x980;
    *(uint64_t**)(r + 0x30) = s.stamps.data();
    *(uint32_t*)(r + 0x38) = 0;       // base
    *(uint32_t*)(r + 0x3C) = svCap;   // capacity
    *(uint32_t*)(r + 0x40) = 14;      // max request
    *(uint32_t*)(r + 0x44) = 0;
    *(uint32_t*)(r + 0x4C) = 0;
    *(uint32_t*)(c + 0x998) = kDescSz;
    *(uint64_t*)(c + 0x9A0) = (uint64_t)s.sv.data();
    *(uint64_t*)(c + 0x9A8) = s.svGpu;
    for (int i = 0; i < 112; ++i) *(uint8_t**)(c + 0xBB0 + i * 8) = (uint8_t*)calloc(1, 65536 + 64);
}

// ---------------------------------------------------------------- one flush (SynFlushCbvTables for one stage)
struct Layout {
    std::vector<uint32_t> ent;  // size, slot pairs
    uint32_t n = 0;
    uint8_t obj[0x60] = {};
    void Fill() {
        *(uint32_t**)(obj + 0x38) = ent.data();
        *(uint32_t*)(obj + 0x40) = (uint32_t)(ent.size() / 2);
        *(uint32_t*)(obj + 0x44) = n;
    }
};

static Code g_stockCode, g_patchCode;

static void Flush(Sim& s, bool patched, int stage, Layout& L, int rootBase) {
    g_cur = &s;
    s.curStage = stage;
    uint32_t& mask = *(uint32_t*)(s.c + 0xB70 + stage * 4);
    if (!mask || !L.n) return;
    int root = rootBase;
    uint64_t fence = *(uint64_t*)(s.c + 0x2510);
    const size_t before = s.binds.size();
    const uint64_t cuts0 = s.cutsInside;
    if (patched)
        CbuTableCall(s.c, &root, stage, L.obj, &fence);
    else
        ((cbvupload::UploadFn)g_stockCode.mem)(s.c, &root, stage, L.obj, &fence);
    mask = 0;
    if (patched && s.binds.size() == before + 2) {
        // CBN after an in-call cut: the skipped slots were uploaded again and the table bound a second time; the first bind
        // is replaced before any draw -> drop it (it is still checked at its list's close only if that list closed between)
        s.binds.erase(s.binds.begin() + (ptrdiff_t)before);
        for (size_t i = 0; i < s.openBinds.size();) {
            if (s.openBinds[i] == before) { s.openBinds.erase(s.openBinds.begin() + (ptrdiff_t)i); continue; }
            if (s.openBinds[i] > before) --s.openBinds[i];
            ++i;
        }
        ++s.redoBinds;
    }
    CHECK(s.binds.size() == before + 1, "%s: %zu binds in one table flush", s.name, s.binds.size() - before);
    if (s.binds.size() != before + 1) return;
    BindRec& b = s.binds.back();
    b.cut = s.tainted || s.cutsInside != cuts0;
    b.n = *(uint32_t*)(s.c + 0xB90 + stage * 4);
    CHECK(b.n == L.n, "%s: B90 = %u, n = %u", s.name, b.n, L.n);
    CHECK(b.root == (uint32_t)(rootBase + stage), "%s: root %u", s.name, b.root);
    b.svBytes.assign(&s.sv[(size_t)b.svIdx * kDescSz], &s.sv[(size_t)b.svIdx * kDescSz] + (size_t)L.n * kDescSz);
    for (size_t i = 0; i < L.ent.size(); i += 2) {
        const uint32_t size = L.ent[i], slot = L.ent[i + 1];
        View v;
        memcpy(&v.va, &b.svBytes[(size_t)slot * kDescSz], 8);
        uint32_t cbvSize;
        memcpy(&cbvSize, &b.svBytes[(size_t)slot * kDescSz + 8], 4);
        uint64_t ser;
        memcpy(&ser, &b.svBytes[(size_t)slot * kDescSz + 16], 8);
        v.size = size;
        v.ser = ser;
        b.views.push_back(v);
        const int sf = v.va ? SafeOf(s, v.va, size, ser) : -2;
        b.deferred.push_back(s.vram && v.va >= (uint64_t)s.vram && v.va < (uint64_t)s.vram + s.ringCap);
        b.safe.push_back(sf);
        if (sf >= 0 && !b.deferred.back()) {
            CHECK(cbvSize >= size, "%s: CBV size %u < declared %u", s.name, cbvSize, size);
            b.bytes.emplace_back((uint8_t*)v.va, (uint8_t*)v.va + size);
        } else {
            b.bytes.emplace_back();
        }
    }
}

// shadow write tokens: bytes into the slot's shadow + dirty bit
static void WriteSlot(Sim& s, int stage, uint32_t slot, const uint8_t* p, uint32_t n, uint32_t off = 0) {
    uint8_t* sh = *(uint8_t**)(s.c + 0xBB0 + (stage * 14 + slot) * 8);
    memcpy(sh + off, p, n);
    *(uint32_t*)(s.c + 0xB70 + stage * 4) |= 1u << slot;
}
// SynSetShaderProgram's re-dirty rules for a new layout on the stage
static void SetLayout(Sim& s, int stage, const Layout& L) {
    uint32_t& mask = *(uint32_t*)(s.c + 0xB70 + stage * 4);
    if (*(uint32_t*)(s.c + 0xB90 + stage * 4) < L.n) mask |= 0x80000000u;
    for (size_t i = 0; i < L.ent.size(); i += 2)
        if (*(uint32_t*)(s.c + 0xF30 + (stage * 14 + L.ent[i + 1]) * 4) < L.ent[i]) mask |= 1u << L.ent[i + 1];
}

// ---------------------------------------------------------------- compare the arms
static void CompareBinds(Sim& a, Sim& b, const char* what) {
    CHECK(a.binds.size() == b.binds.size(), "%s: bind count %zu vs %zu", what, a.binds.size(), b.binds.size());
    const size_t n = a.binds.size() < b.binds.size() ? a.binds.size() : b.binds.size();
    uint64_t views = 0, cmp = 0, cutSkip = 0, oldGone = 0, cutGone = 0;
    for (size_t i = 0; i < n; ++i) {
        const BindRec &x = a.binds[i], &y = b.binds[i];
        CHECK(x.root == y.root && x.n == y.n && x.views.size() == y.views.size(), "%s: bind %zu shape", what, i);
        if (x.views.size() != y.views.size()) continue;
        for (size_t j = 0; j < x.views.size(); ++j) {
            ++views;
            if (x.safe[j] < 0) continue;  // stock: no CBV yet, or its block is gone (the engine's own cut / cross-list hazard)
            if (y.cut || x.cut) {         // a ring-full cut inside this flush in either arm: the arms cut at different
                ++cutSkip;                // points (CBN / CBS allocate less), so pre-cut views legitimately differ in age
                if (y.safe[j] < 0) { ++cutGone; continue; }
            }
            // stock reads a block of an older list (0): the engine's cross-list reuse of untouched slots; how long such a
            // block lives depends on ring consumption, which CBN / CBS change -> compare bytes only while it lives in both
            if (x.safe[j] == 0 && y.safe[j] < 0) { ++oldGone; continue; }
            ++cmp;
            CHECK(y.safe[j] >= 0, "%s: bind %zu view %zu: stock block alive (%d), patched block gone (%d)", what, i, j, x.safe[j], y.safe[j]);
            if (y.safe[j] < 0) continue;
            CHECK(x.bytes[j] == y.bytes[j], "%s: bind %zu slot-view %zu bytes differ (stock safe %d, patched safe %d)", what, i, j, x.safe[j], y.safe[j]);
            if (x.safe[j] == 1 && !x.cut && !y.cut) CHECK(y.safe[j] == 1, "%s: bind %zu view %zu safe in stock, not in patched (%d)", what, i, j, y.safe[j]);
        }
    }
    printf("  %-34s binds %zu, views %llu, compared %llu (after ring-full cuts: %llu, patched view gone there: %llu; older-list block gone in patched only: %llu)\n", what, n,
           (unsigned long long)views, (unsigned long long)cmp, (unsigned long long)cutSkip, (unsigned long long)cutGone, (unsigned long long)oldGone);
}

// ---------------------------------------------------------------- synthetic streams
struct SynCfg {
    const char* name;
    uint64_t ringCap;
    uint32_t svCap;
    int steps;
    bool redirtyOnNewList;  // the engine's safe usage for small rings (all layout slots re-dirtied when a list opens)
    int cbs, cbn, cbt;
    uint64_t seed;
    int f8 = 0;  // 1 = CBF on, 2 = CBF toggled on / off at random during the stream
};

static void FreeSim(Sim& s) {  // each run maps up to 3 rings of 512 MB per arm: give them back
    if (s.ring) VirtualFree(s.ring, 0, MEM_RELEASE);
    if (s.sys) VirtualFree(s.sys, 0, MEM_RELEASE);
    if (s.vram) VirtualFree(s.vram, 0, MEM_RELEASE);
    for (int i = 0; i < 112; ++i) free(*(uint8_t**)(s.c + 0xBB0 + i * 8));
    free(s.c);
    s.ring = s.sys = s.vram = s.c = nullptr;
}

static void SetupF8(Sim& B, int f8) {
    cbvstage::TestReset();
    if (!f8) return;
    B.sys = (uint8_t*)VirtualAlloc(nullptr, B.ringCap, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    B.vram = (uint8_t*)VirtualAlloc(nullptr, B.ringCap, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    memset(B.sys, 0xCE, B.ringCap);
    memset(B.vram, 0xCF, B.ringCap);
    B.f8 = true;
    cbvstage::TestSetup(B.sys, B.vram, B.ringCap);
}

static void RunSynthetic(const SynCfg& cfg) {
    Sim A, B;
    InitSim(A, "stock", cfg.ringCap, cfg.svCap);
    InitSim(B, "patched", cfg.ringCap, cfg.svCap);
    A.checkClose = B.checkClose = true;
    cbvupload::TestReset();
    SetupF8(B, cfg.f8);
    cbvupload::TestSetup((cbvupload::UploadFn)g_patchCode.mem, cfg.cbs, cfg.cbn, cfg.cbt, 1);
    const cbvupload::TestCounts c0 = cbvupload::TestGet();
    std::mt19937_64 rng(cfg.seed);
    auto R = [&](uint64_t n) { return (uint32_t)(rng() % n); };
    // layout pool
    std::vector<Layout> pool(40);
    for (auto& L : pool) {
        const int cnt = 1 + R(5);
        uint32_t used = 0, maxSlot = 0;
        for (int i = 0; i < cnt; ++i) {
            uint32_t slot;
            do slot = R(14); while (used >> slot & 1);
            used |= 1u << slot;
            uint32_t size = 16 * (1 + R(R(8) == 0 ? 4096 : 128));
            if (size > 65536) size = 65536;
            L.ent.push_back(size);
            L.ent.push_back(slot);
            if (slot > maxSlot) maxSlot = slot;
        }
        L.n = maxSlot + 1 + R(3);
        if (L.n > 14) L.n = 14;
        L.Fill();
    }
    int cur[7];
    for (int st = 0; st < 7; ++st) cur[st] = R(40);
    std::vector<uint8_t> buf(65536);
    for (int step = 0; step < cfg.steps; ++step) {
        const int stage = R(7);
        if (R(6) == 0) {  // shader change on this stage
            cur[stage] = R(40);
            SetLayout(A, stage, pool[cur[stage]]);
            SetLayout(B, stage, pool[cur[stage]]);
        }
        Layout& L = pool[cur[stage]];
        for (size_t i = 0; i < L.ent.size(); i += 2) {
            const uint32_t size = L.ent[i], slot = L.ent[i + 1];
            const uint32_t k = R(10);
            uint8_t* shA = *(uint8_t**)(A.c + 0xBB0 + (stage * 14 + slot) * 8);
            if (k < 4) {  // no-op write: same bytes again (whole or a part)
                const uint32_t off = R(2) ? 0 : 16 * R(size / 16), n = size - off;
                memcpy(buf.data(), shA + off, n);
                WriteSlot(A, stage, slot, buf.data(), n, off);
                WriteSlot(B, stage, slot, buf.data(), n, off);
            } else if (k < 7) {  // changed bytes
                const uint32_t off = 16 * R(size / 16), n = 16 * (1 + R((size - off) / 16));
                for (uint32_t j = 0; j < n; ++j) buf[j] = (uint8_t)rng();
                if (R(4) == 0) memcpy(buf.data(), shA + off, n);  // sometimes still identical
                WriteSlot(A, stage, slot, buf.data(), n, off);
                WriteSlot(B, stage, slot, buf.data(), n, off);
            }
        }
        if (R(8) == 0) {  // flush another stage untouched (bit-31 / empty masks)
        }
        Flush(A, false, stage, L, 3);
        if (cfg.f8 == 2 && R(300) == 0) *cbvstage::Switch() ^= 1u;  // live CBF on / off
        Flush(B, true, stage, L, 3);
        if (R(25) == 0) {  // list close; the GPU lags 0..3 lists behind
            const uint64_t lag = R(4);  // one draw for both arms
            for (Sim* s : {&A, &B}) {
                CloseList(*s);
                const uint64_t done = s->Tag() - 1 > lag ? s->Tag() - 1 - lag : 0;
                if (done > s->completed) s->completed = done;
                *(uint64_t*)(s->c + 0x2510) = s->completed;
                s->tainted = false;
                if (cfg.redirtyOnNewList)
                    for (int st = 0; st < 7; ++st) {
                        uint32_t& m = *(uint32_t*)(s->c + 0xB70 + st * 4);
                        for (size_t i = 0; i < pool[cur[st]].ent.size(); i += 2) m |= 1u << pool[cur[st]].ent[i + 1];
                    }
            }
        }
    }
    for (Sim* s : {&A, &B}) CloseList(*s);
    const cbvupload::TestCounts c1 = cbvupload::TestGet();
    CompareBinds(A, B, cfg.name);
    cbvstage::TestReset();
    FreeSim(A);
    FreeSim(B);
    printf("  %-34s uploads %llu, no-op same list %llu, skipped %llu, table hits %llu, CBS copies %llu, cuts inside %llu/%llu, ring fails %llu/%llu\n",
           "", (unsigned long long)(c1.uploads - c0.uploads), (unsigned long long)(c1.noopSame - c0.noopSame),
           (unsigned long long)(c1.skipped - c0.skipped), (unsigned long long)(c1.tblHit - c0.tblHit), (unsigned long long)(c1.copies - c0.copies),
           (unsigned long long)A.cutsInside, (unsigned long long)B.cutsInside, (unsigned long long)A.ringFails, (unsigned long long)B.ringFails);
    if (cfg.cbn) CHECK(c1.skipped > c0.skipped, "%s: CBN never skipped", cfg.name);
    if (cfg.cbt && cfg.cbn) CHECK(c1.tblHit > c0.tblHit, "%s: CBT never hit", cfg.name);
    if (cfg.cbs) CHECK(c1.copies > c0.copies, "%s: CBS never copied", cfg.name);
}

// ---------------------------------------------------------------- recorded trace replay
struct TCall {
    uint32_t stage, mask, n, root;
    uint64_t ctx, tag, list;
    std::vector<uint32_t> ent;
    std::vector<std::vector<uint8_t>> data;  // per entry (empty if not dirty)
};

static bool LoadTrace(const char* path, std::vector<TCall>& out) {
    FILE* f = nullptr;
    if (fopen_s(&f, path, "rb") || !f) return false;
    std::vector<uint8_t> b;
    fseek(f, 0, SEEK_END);
    b.resize((size_t)ftell(f));
    fseek(f, 0, SEEK_SET);
    fread(b.data(), 1, b.size(), f);
    fclose(f);
    size_t p = 0;
    auto U32 = [&](uint32_t& v) { if (p + 4 > b.size()) return false; memcpy(&v, &b[p], 4); p += 4; return true; };
    auto U64 = [&](uint64_t& v) { if (p + 8 > b.size()) return false; memcpy(&v, &b[p], 8); p += 8; return true; };
    while (p < b.size()) {
        TCall t;
        uint32_t magic, count;
        if (!U32(magic) || magic != 0x4C4C4143u) return false;
        if (!U32(t.stage) || !U32(t.mask) || !U32(count) || !U32(t.n) || !U32(t.root) || !U64(t.ctx) || !U64(t.tag) || !U64(t.list)) return false;
        t.ent.resize(count * 2);
        for (auto& e : t.ent) if (!U32(e)) return false;
        t.data.resize(count);
        for (uint32_t i = 0; i < count; ++i)
            if (t.mask >> (t.ent[2 * i + 1] & 31) & 1) {
                const uint32_t sz = t.ent[2 * i];
                if (p + sz > b.size()) return false;
                t.data[i].assign(&b[p], &b[p] + sz);
                p += sz;
            }
        out.push_back(std::move(t));
    }
    return true;
}

static void RunTrace(const char* path, int cbs, int cbn, int cbt, uint64_t ringCap, uint32_t svCap, const char* label, int f8 = 0) {
    std::vector<TCall> calls;
    if (!LoadTrace(path, calls)) {
        CHECK(false, "trace %s unreadable", path);
        return;
    }
    // the context with the most calls (the graphics SynCtx)
    std::map<uint64_t, size_t> per;
    for (auto& t : calls) per[t.ctx]++;
    uint64_t ctx = 0;
    size_t best = 0;
    for (auto& kv : per)
        if (kv.second > best) best = kv.second, ctx = kv.first;
    Sim A, B;
    InitSim(A, "stock", ringCap, svCap);
    InitSim(B, "patched", ringCap, svCap);
    cbvupload::TestReset();
    SetupF8(B, f8);
    cbvupload::TestSetup((cbvupload::UploadFn)g_patchCode.mem, cbs, cbn, cbt, 1);
    const cbvupload::TestCounts c0 = cbvupload::TestGet();
    std::vector<Layout> lays;
    lays.reserve(calls.size());
    uint64_t lastTag = 0, lists = 0;
    size_t used = 0;
    uint64_t dirty = 0, dirtyBytes = 0;
    for (auto& t : calls) {
        if (t.ctx != ctx || t.stage >= 7) continue;
        ++used;
        if (lastTag && t.tag != lastTag)
            for (Sim* s : {&A, &B}) {  // the engine opened a new list between these calls; GPU lags one list
                CloseList(*s);
                s->tainted = false;
                ++lists;
                const uint64_t done = s->Tag() >= 2 ? s->Tag() - 2 : 0;
                if (done > s->completed) s->completed = done;
                *(uint64_t*)(s->c + 0x2510) = s->completed;
            }
        lastTag = t.tag;
        lays.emplace_back();
        Layout& L = lays.back();
        L.ent = t.ent;
        L.n = t.n;
        L.Fill();
        for (Sim* s : {&A, &B}) {
            for (size_t i = 0; i < t.data.size(); ++i)
                if (!t.data[i].empty()) {
                    uint8_t* sh = *(uint8_t**)(s->c + 0xBB0 + (t.stage * 14 + t.ent[2 * i + 1]) * 8);
                    memcpy(sh, t.data[i].data(), t.data[i].size());
                }
            *(uint32_t*)(s->c + 0xB70 + t.stage * 4) = t.mask;
        }
        for (size_t i = 0; i < t.data.size(); ++i)
            if (!t.data[i].empty()) ++dirty, dirtyBytes += t.data[i].size();
        Flush(A, false, (int)t.stage, L, (int)t.root);
        Flush(B, true, (int)t.stage, L, (int)t.root);
    }
    for (Sim* s : {&A, &B}) CloseList(*s);
    const cbvupload::TestCounts c1 = cbvupload::TestGet();
    char what[160];
    snprintf(what, sizeof what, "trace %s", label);
    CompareBinds(A, B, what);
    cbvstage::TestReset();
    FreeSim(A);
    FreeSim(B);
    const double up = (double)(c1.uploads - c0.uploads);
    printf("  %-34s calls %zu (of %zu), lists %llu, dirty slots %llu (%.1f MB), no-op any %.1f %%, same list %.1f %%, skipped %llu, table cand %llu hits %llu\n",
           "", used, calls.size(), (unsigned long long)lists, (unsigned long long)dirty, dirtyBytes / 1048576.0,
           up ? 100.0 * (double)(c1.noopAny - c0.noopAny) / up : 0.0, up ? 100.0 * (double)(c1.noopSame - c0.noopSame) / up : 0.0,
           (unsigned long long)(c1.skipped - c0.skipped), (unsigned long long)(c1.tblCand - c0.tblCand), (unsigned long long)(c1.tblHit - c0.tblHit));
}

// ---------------------------------------------------------------- CBS unit test
static void TestCopy() {
    cbvupload::TestSetup((cbvupload::UploadFn)g_patchCode.mem, 1, 0, 0, 0);
    std::vector<uint8_t> src(8192 + 64), a(8192 + 1024), b(8192 + 1024);
    std::mt19937 rng(7);
    for (auto& x : src) x = (uint8_t)rng();
    int cases = 0;
    for (uint32_t n = 0; n <= 4160; n += (n < 300 ? 1 : 16))
        for (uint32_t so = 0; so < 16; ++so)
            for (uint32_t dofs : {0u, 16u, 48u, 256u, 3u}) {
                memset(a.data(), 0x5A, a.size());
                memset(b.data(), 0x5A, b.size());
                uint8_t* da = (uint8_t*)(((uintptr_t)a.data() + 255) & ~(uintptr_t)255) + 256 + dofs;
                uint8_t* db = (uint8_t*)(((uintptr_t)b.data() + 255) & ~(uintptr_t)255) + 256 + dofs;
                void* r = CbuStreamCopy(db, src.data() + so, n);
                memcpy(da, src.data() + so, n);
                CHECK(r == db, "CBS return value");
                CHECK(memcmp(da - 64, db - 64, n + 192) == 0, "CBS n=%u so=%u dofs=%u differs from memcpy", n, so, dofs);
                ++cases;
            }
    cbvupload::TestSetup((cbvupload::UploadFn)g_patchCode.mem, 0, 0, 0, 0);
    printf("  CBS copy == memcpy (incl. guard bytes): %d cases\n", cases);
}

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    const char* dump = argc > 1 ? argv[1] : nullptr;
    FILE* f = nullptr;
    if (!dump || fopen_s(&f, dump, "rb") || !f) {
        printf("usage: cbvupload_harness <Nioh3.dump.exe> [trace.bin ...]\n");
        return 2;
    }
    fseek(f, 0, SEEK_END);
    g_dump.resize((size_t)ftell(f));
    fseek(f, 0, SEEK_SET);
    fread(g_dump.data(), 1, g_dump.size(), f);
    fclose(f);
    CHECK(Fnv(kFn, kFnLen) == 0xBAE85B33u, "118E28 is not v2.02");
    CHECK(Fnv(kDesc, kDescLen) == 0x2542B674u, "1192B0 is not v2.02");
    if (g_fail) return 1;
    g_devVt[0x88 / 8] = (void*)&MockCreateCbv;
    g_devVt[0xC0 / 8] = (void*)&MockCopyDesc;
    g_listVt[0x100 / 8] = (void*)&MockSetTable;
    g_stockCode = Build((void*)&memcpy);
    g_patchCode = Build((void*)&CbuStreamCopy);
    printf("CBS unit test\n");
    TestCopy();
    printf("synthetic streams (stock relocated 118E28 vs CbuTableCall + CbuStreamCopy)\n");
    const SynCfg cfgs[] = {
        {"big ring, all off", 512ull << 20, 4096, 20000, false, 0, 0, 0, 11},
        {"big ring, CBS", 512ull << 20, 4096, 20000, false, 1, 0, 0, 12},
        {"big ring, CBN", 512ull << 20, 4096, 20000, false, 0, 1, 0, 13},
        {"big ring, CBN+CBT", 512ull << 20, 4096, 20000, false, 0, 1, 1, 14},
        {"big ring, CBS+CBN+CBT", 512ull << 20, 4096, 20000, false, 1, 1, 1, 15},
        {"small rings, CBS+CBN+CBT", 1ull << 20, 160, 20000, true, 1, 1, 1, 16},
        {"tiny rings (cuts), CBS+CBN+CBT", 256ull << 10, 64, 20000, true, 1, 1, 1, 17},
        {"tiny rings (cuts), CBT only", 256ull << 10, 64, 20000, true, 0, 0, 1, 18},
        {"big ring, CBF", 512ull << 20, 4096, 20000, false, 0, 0, 0, 21, 1},
        {"big ring, CBF + CBS+CBN+CBT", 512ull << 20, 4096, 20000, false, 1, 1, 1, 22, 1},
        {"small rings, CBF toggled live", 1ull << 20, 160, 20000, true, 1, 1, 1, 23, 2},
        {"tiny rings (cuts), CBF + all", 256ull << 10, 64, 20000, true, 1, 1, 1, 24, 1},
    };
    for (const auto& c : cfgs) RunSynthetic(c);
    for (int i = 2; i < argc; ++i) {
        printf("recorded trace %s\n", argv[i]);
        RunTrace(argv[i], 1, 1, 1, 1024ull << 20, 8192, "big ring, CBS+CBN+CBT");
        RunTrace(argv[i], 0, 1, 0, 1024ull << 20, 8192, "big ring, CBN");
        RunTrace(argv[i], 1, 1, 1, 64ull << 20, 1024, "small rings, CBS+CBN+CBT");
        RunTrace(argv[i], 0, 1, 1, 1024ull << 20, 8192, "big ring, CBN+CBT");
        RunTrace(argv[i], 0, 0, 0, 1024ull << 20, 8192, "big ring, CBF", 1);
        RunTrace(argv[i], 1, 1, 1, 64ull << 20, 1024, "small rings, CBF + all", 1);
    }
    if (g_fail) {
        printf("FAILED: %d checks\n", g_fail);
        return 1;
    }
    printf("ALL PASSED\n");
    return 0;
}
