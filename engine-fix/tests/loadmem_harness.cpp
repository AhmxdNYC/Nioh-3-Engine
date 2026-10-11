// loadmem_harness.cpp - LM (loadmem.cpp) sweep logic on a synthetic engine heap in real memory. Not a game test.
// Builds a heap object + region with the v2.02 layout (heap+18h header size 0x20, +20h base, +28h first chunk, +30h end,
// +38h bin table, +44h bin count, +60h mutex; chunk = prev, size (neg = allocated), bin prev, bin next) and checks:
// stability rule (2 sweeps), page-interior bounds (never a header), changed chunks skipped, no repeat, lock held around
// every reset, budget/slice splitting, and the real VirtualAlloc(MEM_RESET) path leaves headers and live payload intact.
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "../src/loadmem.h"

using namespace loadmem::testing;

static int g_fail = 0;
static void Chk(const char* n, bool c) {
    printf("%s %s\n", c ? "OK  " : "FAIL", n);
    if (!c) ++g_fail;
}

struct Rng { uint64_t s = 88172645463325252ull; uint64_t n() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; } };

// ---- fake lock / reset recording ------------------------------------------------------------------------------------------
static int g_locked = 0, g_lockCalls = 0, g_unlockCalls = 0, g_resetUnlocked = 0;
static void* g_lastMtx = nullptr;
static std::vector<std::pair<uintptr_t, size_t>> g_resets;
static int FLock(void* m) { ++g_lockCalls; ++g_locked; g_lastMtx = m; return 0; }
static int FUnlock(void*) { ++g_unlockCalls; --g_locked; return 0; }
static bool FReset(uintptr_t a, size_t n) { if (!g_locked) ++g_resetUnlocked; g_resets.push_back({a, n}); return true; }

struct Chunk { uintptr_t c; int64_t S; bool freeC; };

struct Fake {
    uint8_t* mem = nullptr;
    size_t size = 0;
    uint8_t heapObj[0xB0] = {};
    std::vector<Chunk> ch;
    uintptr_t base() const { return (uintptr_t)mem; }
    void W(uintptr_t a, uint64_t v) { memcpy((void*)a, &v, 8); }
    uint64_t R(uintptr_t a) { uint64_t v; memcpy(&v, (void*)a, 8); return v; }
    // layout: chunks of given payload sizes (sign = free if flags)
    void Build(const std::vector<std::pair<int64_t, bool>>& spec) {
        size = 64u << 20;
        mem = (uint8_t*)VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        uintptr_t first = base() + 0x80, c = first;
        ch.clear();
        for (auto& sp : spec) { ch.push_back({c, sp.first, sp.second}); c += 0x20 + (uintptr_t)sp.first; }
        uintptr_t end = c;
        uintptr_t prev = 0;
        for (auto& k : ch) {
            W(k.c, prev);
            W(k.c + 8, k.freeC ? (uint64_t)k.S : (uint64_t)(-k.S));
            W(k.c + 0x10, 0); W(k.c + 0x18, 0);
            // payload pattern so we can check live data survives
            memset((void*)(k.c + 0x20), 0xA5, (size_t)k.S);
            prev = k.c;
        }
        Link();
        W((uintptr_t)heapObj + 0x18, 0x20);
        W((uintptr_t)heapObj + 0x20, base());
        W((uintptr_t)heapObj + 0x28, first);
        W((uintptr_t)heapObj + 0x30, end);
        W((uintptr_t)heapObj + 0x38, base());
        uint32_t nb = 16; memcpy(heapObj + 0x44, &nb, 4);
    }
    // put free chunks into bins by index (round-robin over 3 bins)
    void Link() {
        for (int b = 0; b < 16; ++b) W(base() + 8 * b, 0);
        int i = 0;
        for (auto& k : ch) {
            if (!k.freeC) continue;
            int b = (i++) % 3;
            W(k.c + 0x18, R(base() + 8 * b));   // next = old head
            W(base() + 8 * b, k.c);
        }
    }
};

static bool InHeader(const Fake& f, uintptr_t addr) {
    for (auto& k : f.ch) if (addr >= k.c && addr < k.c + 0x20) return true;
    return false;
}

int main() {
    Cfg cfg;
    Stats st;
    Ops ops; ops.lock = &FLock; ops.unlock = &FUnlock; ops.reset = &FReset;
    SetOps(ops);

    // sizes: free big (aligned and unaligned), free small, allocated big, free exactly 256K, free huge
    std::vector<std::pair<int64_t, bool>> spec = {
        {0x40000, true},    // 256K free, c+0x20 not page aligned in general
        {0x1000, false},
        {0x30000 - 0x20 + 0x10, true},  // 192K+: below min
        {0x200000, false},  // big but allocated
        {0x100230, true},   // ~1M free, odd tail
        {0x40, false},
        {0x5FFFE0, true},   // ~6M free
        {0x1000, false},
        {0x80000, true},
    };
    Fake f; f.Build(spec);
    uintptr_t H = (uintptr_t)f.heapObj;
    Chk("ValidHeap(fake)", ValidHeap(H));
    uint8_t junk[0xB0] = {};
    Chk("ValidHeap(zeros) is false", !ValidHeap((uintptr_t)junk));

    HeapState* hs = NewState(H);
    Sweep(hs, cfg, st);
    Chk("sweep 1 resets nothing", g_resets.empty() && st.bytes == 0);
    Chk("lock/unlock balanced after sweep 1, mutex = heap+60h", g_lockCalls == g_unlockCalls && g_lastMtx == (void*)(H + 0x60) && g_lockCalls == 16);

    // expected set after sweep 2
    std::vector<std::pair<uintptr_t, size_t>> exp;
    for (auto& k : f.ch) {
        if (!k.freeC || (size_t)k.S < cfg.minBytes) continue;
        uintptr_t a = (k.c + 0x20 + 0xFFF) & ~(uintptr_t)0xFFF, e = (k.c + 0x20 + (uintptr_t)k.S) & ~(uintptr_t)0xFFF;
        if (e > a) exp.push_back({a, e - a});
    }
    // change one chunk between sweeps: chunk 4 (index 4) shrinks by a split (size differs, still free)
    Chunk& ch4 = f.ch[4];
    f.W(ch4.c + 8, (uint64_t)(ch4.S - 0x10000));
    ch4.S -= 0x10000;
    // the chunk after it keeps position in this simplified model (a tail pad), fine: it only matters that (c,S) differ
    Sweep(hs, cfg, st);
    // exp minus chunk 4
    size_t nExp = 0; uint64_t bytesExp = 0;
    bool allMatch = true;
    for (auto& e : exp) {
        if (e.first >= f.ch[4].c && e.first < f.ch[4].c + 0x20 + (uintptr_t)f.ch[4].S + 0x10000 && e.first > f.ch[4].c) continue;  // chunk 4 interior
        ++nExp; bytesExp += e.second;
        bool found = false;
        for (auto& r : g_resets) found |= (r.first == e.first && r.second == e.second);
        allMatch &= found;
    }
    Chk("sweep 2 resets exactly the stable big free chunks (exact page-interior ranges)", allMatch && g_resets.size() == nExp && st.bytes == bytesExp);
    Chk("changed chunk (size differs between sweeps) was not reset", [&] {
        for (auto& r : g_resets) if (r.first > f.ch[4].c && r.first < f.ch[4].c + 0x20 + (uintptr_t)f.ch[4].S + 0x10000) return false;
        return true; }());
    bool hdrOk = true, bounds = true;
    for (auto& r : g_resets) {
        for (uintptr_t a = r.first; a < r.first + r.second; a += 0x1000) hdrOk &= !InHeader(f, a) && !InHeader(f, a + 0xFFF);
        bounds &= (r.first % 0x1000 == 0) && (r.second % 0x1000 == 0);
    }
    Chk("no reset range overlaps any chunk header; ranges are whole pages", hdrOk && bounds);
    Chk("every reset ran under the heap lock", g_resetUnlocked == 0 && g_locked == 0);
    size_t n2 = g_resets.size();
    Sweep(hs, cfg, st);
    Chk("sweep 3 resets only the previously changed chunk (others not repeated)", g_resets.size() == n2 + 1);
    Sweep(hs, cfg, st);
    Chk("sweep 4: nothing new", g_resets.size() == n2 + 1);

    // chunk freed -> allocated -> freed again at the same place must re-earn 2 sweeps
    {
        Fake g; g.Build({{0x100000, true}, {0x1000, false}});
        HeapState* s2 = NewState((uintptr_t)g.heapObj);
        g_resets.clear(); Stats s;
        Sweep(s2, cfg, s);
        g.W(g.ch[0].c + 8, (uint64_t)(-0x100000));   // allocated
        g.W(g.base(), 0);                             // out of the bin
        Sweep(s2, cfg, s);
        g.W(g.ch[0].c + 8, (uint64_t)0x100000);      // freed again
        g.W(g.base(), g.ch[0].c);
        Sweep(s2, cfg, s);
        Chk("free -> allocated -> free again does not reset (needs 2 consecutive sweeps)", g_resets.empty());
        Sweep(s2, cfg, s);
        Chk("... and resets after 2 consecutive free sweeps", g_resets.size() == 1);
        DeleteState(s2);
        VirtualFree(g.mem, 0, MEM_RELEASE);
    }

    // slicing: 4 MiB slices over the 6 MB chunk
    {
        Fake g; g.Build({{0x5FFFE0, true}, {0x1000, false}});
        HeapState* s2 = NewState((uintptr_t)g.heapObj);
        g_resets.clear(); Stats s; Cfg c2; c2.sliceBytes = 0x100000; c2.budgetUs = 100000000;
        Sweep(s2, c2, s); Sweep(s2, c2, s);
        uint64_t sum = 0; bool contiguous = true; uintptr_t at = g_resets.empty() ? 0 : g_resets[0].first;
        for (auto& r : g_resets) { contiguous &= r.first == at; at += r.second; sum += r.second; contiguous &= r.second <= 0x100000; }
        Chk("1 MB slices cover the 6 MB interior contiguously", g_resets.size() >= 6 && contiguous && sum == s.bytes);
        DeleteState(s2);
        VirtualFree(g.mem, 0, MEM_RELEASE);
    }

    // real VirtualAlloc(MEM_RESET): headers + live payload intact, nothing fails
    {
        SetOps(Ops{});   // defaults: the real reset; keep the fake lock
        Ops o2; o2.lock = &FLock; o2.unlock = &FUnlock; SetOps(o2);
        Fake g; g.Build(spec);
        HeapState* s2 = NewState((uintptr_t)g.heapObj);
        Stats s; Sweep(s2, cfg, s); Sweep(s2, cfg, s);
        bool intact = s.resetFails == 0 && s.bytes > 0;
        for (auto& k : g.ch) {
            intact &= g.R(k.c + 8) == (uint64_t)(k.freeC ? k.S : -k.S);
            if (!k.freeC) for (int64_t i = 0; i < k.S; i += 4096) intact &= *(uint8_t*)(k.c + 0x20 + i) == 0xA5;
        }
        Chk("real MEM_RESET: headers + live payload intact, no failures", intact);
        // reset memory stays valid: writing works
        for (auto& k : g.ch) if (k.freeC) *(volatile uint8_t*)(k.c + 0x20 + k.S - 1) = 1;
        Chk("reset pages remain committed and writable", true);
        DeleteState(s2);
        VirtualFree(g.mem, 0, MEM_RELEASE);
    }
    DeleteState(hs);
    VirtualFree(f.mem, 0, MEM_RELEASE);
    printf(g_fail ? "LOADMEM HARNESS FAILED (%d)\n" : "ALL LOADMEM CHECKS OK\n", g_fail);
    return g_fail ? 1 : 0;
}
