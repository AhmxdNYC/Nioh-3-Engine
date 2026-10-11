// psomemo_harness.cpp - PM (src/psomemo.cpp) against a model of PsoCacheFindGfx 11976C. Not a game test.
// Model: a cache object whose entry count is at +28h, entries {u32 hash, u32 pad, key[0x80], PSO*}; Find copies the found
// 0x90-byte entry over the caller's key and returns 1, else returns 0 and leaves the key alone (as 11976C does).
// Workload: 2M lookups from a skewed key population (Zipf-like, plus hash collisions: different keys with equal hash),
// inserts of new keys (count grows), and twice a full clear + refill with DIFFERENT PSO values (count drops) - the memo
// must never return a PSO the cache does not hold right now. Every call: return value and all 0x90 key bytes == the model
// called on a copy. Prints ALL PASSED.
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <random>
#include <string>
#include <vector>

#include "../src/psomemo.h"

struct Cache {
    void* root;  // +0 (the model's root: replaced on clear)
    uint8_t pad[0x20];
    uint32_t count;  // +28h
    uint32_t pad2[2];
    uint32_t cap;    // +34h
    std::map<std::string, std::vector<uint8_t>> m;  // key = hash(4) + key(0x80)
};
static std::string K(const uint8_t* key) { return std::string((const char*)key, 4) + std::string((const char*)key + 8, 0x80); }
static int64_t g_origCalls = 0;
static bool ModelFind(void* c, uint8_t* key) {
    ++g_origCalls;
    Cache* cc = static_cast<Cache*>(c);
    auto it = cc->m.find(K(key));
    if (it == cc->m.end()) return false;
    memcpy(key, it->second.data(), 0x90);
    return true;
}
static void Insert(Cache& c, const uint8_t* key, uint64_t pso, uint32_t padv) {
    std::vector<uint8_t> e(0x90);
    memcpy(e.data(), key, 0x88);
    memcpy(e.data() + 4, &padv, 4);
    memcpy(e.data() + 0x88, &pso, 8);
    if (c.m.emplace(K(key), e).second) ++c.count;
}

int main() {
    Cache c{};
    psomemo::TestInit(&ModelFind);
    std::mt19937_64 rng(12345);
    const int nKeys = 6000;
    std::vector<std::vector<uint8_t>> keys(nKeys, std::vector<uint8_t>(0x90));
    for (int i = 0; i < nKeys; ++i) {
        for (int b = 8; b < 0x88; ++b) keys[i][b] = (uint8_t)rng();
        uint32_t h = (uint32_t)rng();
        if (i % 50 == 1) memcpy(&h, keys[i - 1].data(), 4);  // same hash, different key
        memcpy(keys[i].data(), &h, 4);
        memset(keys[i].data() + 4, 0xEE, 4);                   // caller's pad differs from the entry's pad
        memset(keys[i].data() + 0x88, 0, 8);
    }
    uint64_t gen = 1;
    for (int i = 0; i < nKeys / 2; ++i) Insert(c, keys[i].data(), (gen << 32) | i, 0x1111);
    int64_t mism = 0, n = 0, hits = 0;
    int refill = nKeys / 2;
    std::uniform_real_distribution<double> u(0, 1);
    for (int it = 0; it < 2000000; ++it) {
        if (it == 700000 || it == 1400000) {  // clear, then a gradual refill with NEW PSOs while lookups go on (level change)
            c.m.clear();
            c.count = 0;
            c.root = (void*)(uintptr_t)(0x1000 * it);
            ++gen;
            refill = 0;
        }
        if (refill < nKeys / 2) {
            Insert(c, keys[refill].data(), (gen << 32) | refill, 0x2222);
            ++refill;
        }
        if (it == 1700000) {  // clear + refill to the SAME count inside one gap, new root (a grow / reset between two frames)
            c.m.clear();
            c.count = 0;
            c.root = (void*)(uintptr_t)0xBEEF000;
            ++gen;
            for (int i = 0; i < nKeys / 2; ++i) Insert(c, keys[i].data(), (gen << 32) | i, 0x4444);
            refill = nKeys / 2;
        }
        int idx = (int)(nKeys * u(rng) * u(rng));  // skewed toward low indices
        if (it % 997 == 0) Insert(c, keys[(it / 997) % nKeys].data(), (gen << 32) | 0xABC, 0x3333);  // runtime create
        uint8_t a[0x90], b[0x90];
        memcpy(a, keys[idx].data(), 0x90);
        memcpy(b, a, 0x90);
        const int64_t before = g_origCalls;
        const bool ra = psomemo::TestFind(&c, a);
        if (g_origCalls == before) ++hits;
        const bool rb = ModelFind(&c, b);
        --g_origCalls;
        ++n;
        if (ra != rb || memcmp(a, b, 0x90) != 0) {
            if (mism < 5) printf("mismatch at %d key %d: ret %d vs %d\n", it, idx, ra, rb);
            ++mism;
        }
    }
    char line[512];
    psomemo::StatusLine(line, sizeof line, 1);
    printf("%lld lookups, %lld served from the memo (%.1f %%), %lld mismatches\n%s\n", (long long)n, (long long)hits, 100.0 * hits / n,
           (long long)mism, line);
    // cost: memo hit vs model (the model is a std::map, not the real B-tree; this only shows the hit path is cheap)
    LARGE_INTEGER f, t0, t1;
    QueryPerformanceFrequency(&f);
    uint8_t a[0x90];
    QueryPerformanceCounter(&t0);
    for (int i = 0; i < 1000000; ++i) {
        memcpy(a, keys[i & 63].data(), 0x90);
        psomemo::TestFind(&c, a);
    }
    QueryPerformanceCounter(&t1);
    printf("memo hit path: %.1f ns per lookup (incl. the 0x90-byte copies)\n", (double)(t1.QuadPart - t0.QuadPart) * 1e9 / f.QuadPart / 1e6);
    const bool ok = mism == 0 && hits > n / 2;
    printf(ok ? "ALL PASSED\n" : "FAILED\n");
    return ok ? 0 : 1;
}
