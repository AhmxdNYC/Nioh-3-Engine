// gimerge_harness.cpp - offline checks of src\gimerge.cpp (no device, no game). Run: py -I tests/run_gimerge_harness.py
// 1. MaskUnion: closed form vs brute force over a 5-bit stencil domain (all 2^20 (x1,y1,x2,y2)).
// 2. Hold / merge / replay on a fake command list: the recorded call stream must be the original one until the pair
//    qualifies, then lack the first dispatch with the second one's mask rewritten to the union; a pattern break must
//    replay the held dispatch with its own root table, then restore the current one (+ UAV barrier if one was held over).
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../src/gimerge.h"

static int g_fail = 0;
#define CHECK(c, ...)                         \
    do {                                      \
        if (!(c)) {                           \
            ++g_fail;                         \
            printf("FAIL %s:%d ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);              \
            printf("\n");                     \
        }                                     \
    } while (0)

// ---- 1 ----
static uint32_t Set5(uint32_t x, uint32_t y) {
    uint32_t m = 0;
    for (uint32_t s = 0; s < 32; ++s)
        if ((s & y) == x) m |= 1u << s;
    return m;
}
static void TestUnion() {
    static uint32_t cube[32][32];
    std::vector<uint32_t> all;
    for (uint32_t x = 0; x < 32; ++x)
        for (uint32_t y = 0; y < 32; ++y) cube[x][y] = Set5(x, y);
    long cases = 0, expressible = 0;
    for (uint32_t x1 = 0; x1 < 32; ++x1)
        for (uint32_t y1 = 0; y1 < 32; ++y1)
            for (uint32_t x2 = 0; x2 < 32; ++x2)
                for (uint32_t y2 = 0; y2 < 32; ++y2) {
                    const uint32_t u = cube[x1][y1] | cube[x2][y2];
                    if (!u) continue;
                    bool exists = false;
                    for (uint32_t x = 0; x < 32 && !exists; ++x)
                        for (uint32_t y = 0; y < 32 && !exists; ++y) exists = cube[x][y] == u;
                    uint32_t xo = 0, yo = 0;
                    const bool got = gimerge::MaskUnion(x1, y1, x2, y2, xo, yo);
                    ++cases;
                    expressible += exists;
                    CHECK(got == exists, "MaskUnion(%u,%u,%u,%u) = %d, brute force %d", x1, y1, x2, y2, got, exists);
                    if (got) CHECK(xo < 32 && yo < 32 && cube[xo][yo] == u, "MaskUnion(%u,%u,%u,%u) -> (%u,%u) wrong set", x1, y1, x2, y2, xo, yo);
                }
    uint32_t xo, yo;
    CHECK(!gimerge::MaskUnion(1, 3, 2, 3, xo, yo), "refs 1,2 under mask 3 must not merge");
    CHECK(gimerge::MaskUnion(2, 3, 3, 3, xo, yo) && xo == 2 && yo == 2, "refs 2,3 under mask 3 -> (2,2)");
    printf("MaskUnion: %ld cases, %ld expressible\n", cases, expressible);
}

// ---- 2: fake command list ----
static std::vector<std::string> g_rec;
static void STDMETHODCALLTYPE FDispatch(void*, UINT x, UINT, UINT) { g_rec.push_back("D" + std::to_string(x)); }
static void STDMETHODCALLTYPE FSetTab(void*, UINT i, uint64_t h) { g_rec.push_back("T" + std::to_string(i) + "=" + std::to_string(h)); }
static void STDMETHODCALLTYPE FSetCbv(void*, UINT i, uint64_t h) { g_rec.push_back("C" + std::to_string(i) + "=" + std::to_string(h)); }
static void STDMETHODCALLTYPE FBarrier(void*, UINT, const D3D12_RESOURCE_BARRIER* b) {
    g_rec.push_back(b->Type == D3D12_RESOURCE_BARRIER_TYPE_UAV ? "U" : "B");
}
static void* g_orig[96];
static volatile uint8_t g_mask[96];
static void* volatile g_hold = nullptr;
static char g_list[16];
static void* L = g_list;

// what the gpuopt thunk does
static void Call(int slot, uintptr_t a1, uintptr_t a2 = 0, uintptr_t a3 = 0, const char* name = nullptr) {
    bool skip = false;
    if (g_mask[slot] || g_hold == L) skip = gimerge::OnListCall(L, 0, slot, a1, a2, a3);
    if (skip) return;
    switch (slot) {
        case 14: FDispatch(L, (UINT)a1, (UINT)a2, (UINT)a3); break;
        case 31: FSetTab(L, (UINT)a1, a2); break;
        case 37: FSetCbv(L, (UINT)a1, a2); break;
        default: g_rec.push_back(name ? name : ("S" + std::to_string(slot)).c_str()); break;
    }
}
static void Barrier(bool uav) {
    D3D12_RESOURCE_BARRIER b = {};
    b.Type = uav ? D3D12_RESOURCE_BARRIER_TYPE_UAV : D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    gimerge::OnBarrier(L, 0, 1, &b);
    FBarrier(L, 1, &b);
}

static uint8_t g_ring[1 << 16];
static const uint64_t kVA = 0x100000;
static int g_ringPos = 0;
// engine-like: write $Globals (512 B, mask at 464) into a fresh ring slot, create its CBV, bind table 2 to a new handle
static uint64_t g_handle = 1000;
static uint64_t Bind(float mx, float my, float extra) {
    const int off = (g_ringPos++ % 64) * 512;
    float* f = reinterpret_cast<float*>(g_ring + off);
    for (int i = 0; i < 128; ++i) f[i] = (float)i;
    f[3] = extra;
    f[464 / 4] = mx, f[464 / 4 + 1] = my, f[464 / 4 + 2] = 7.f, f[464 / 4 + 3] = 9.f;
    gimerge::Test().createCbv(kVA + off, 512);
    Call(31, 2, ++g_handle);
    return (uint64_t)off;
}
static float MaskAt(uint64_t off, int k) { return reinterpret_cast<float*>(g_ring + off)[464 / 4 + k]; }

static std::string Rec() {
    std::string s;
    for (auto& r : g_rec) s += r + " ";
    g_rec.clear();
    return s;
}

static void* const P121 = reinterpret_cast<void*>(0x121000);
static void* const POTHER = reinterpret_cast<void*>(0x999000);

// one frame of the pattern: PSO 121, table 0/1, g1 (2,3), UAV barrier, g2 (3,3), then another shader
static std::string Frame(uint64_t& o1, uint64_t& o2, float g2extra = 3.f) {
    Call(10, 0, (uintptr_t)POTHER);  // Reset
    Call(29, 0);                      // root signature
    Call(31, 0, 11);
    Call(31, 1, 12);
    Call(25, (uintptr_t)P121);
    o1 = Bind(2.f, 3.f, 3.f);
    Call(14, 144, 81, 1);
    Barrier(true);
    o2 = Bind(3.f, 3.f, g2extra);
    Call(14, 144, 81, 1);
    Call(25, (uintptr_t)POTHER);
    Bind(0.f, 0.f, 5.f);
    Call(14, 7, 1, 1);
    Call(9, 0, 0, 0, "Close");
    return Rec();
}

static void TestMerge() {
    for (int i = 0; i < 96; ++i) g_orig[i] = nullptr;
    g_orig[14] = (void*)&FDispatch;
    g_orig[31] = (void*)&FSetTab;
    g_orig[37] = (void*)&FSetCbv;
    g_orig[26] = (void*)&FBarrier;
    const gimerge::TestApi& T = gimerge::Test();
    T.setMode(0, 1, 3);
    uint8_t dig[16] = {0x4b, 0x4c, 0x3c, 0xbe, 0x67, 0x24, 0xe0, 0x68, 0x41, 0xb7, 0xed, 0x69, 0xd7, 0x47, 0x2a, 0x46};
    T.registerPso(P121, dig);
    T.mapRange(kVA, g_ring, sizeof g_ring);
    gimerge::EnableHooks(g_mask, 96, g_orig, nullptr, &g_hold);

    uint64_t o1, o2;
    // qualification: 3 frames unchanged
    for (int f = 0; f < 3; ++f) {
        const std::string s = Frame(o1, o2);
        CHECK(s.find("D144 U T2=") != std::string::npos && s.find("D144 S25") != std::string::npos, "frame %d not original: %s", f, s.c_str());
    }
    CHECK(T.counter(6) == 1, "rp121 should be armed after 3 clean pairs (armed=%llu)", (unsigned long long)T.counter(6));
    // merged frame: one D144, second mask rewritten to (2,2), z/w untouched
    std::string s = Frame(o1, o2);
    size_t n = 0;
    for (size_t p = s.find("D144"); p != std::string::npos; p = s.find("D144", p + 1)) ++n;
    CHECK(n == 1, "merged frame must have one D144: %s", s.c_str());
    CHECK(MaskAt(o2, 0) == 2.f && MaskAt(o2, 1) == 2.f && MaskAt(o2, 2) == 7.f && MaskAt(o2, 3) == 9.f, "mask not rewritten to (2,2)");
    CHECK(MaskAt(o1, 0) == 2.f && MaskAt(o1, 1) == 3.f, "first cbuffer must be untouched");
    CHECK(T.counter(0) == 1, "merged counter %llu", (unsigned long long)T.counter(0));
    printf("merged frame: %s\n", s.c_str());

    // pattern break: the second cbuffer differs elsewhere -> replay g1 with table 2 = its own handle, then restore
    s = Frame(o1, o2, 4.f);
    printf("mismatch frame: %s\n", s.c_str());
    n = 0;
    for (size_t p = s.find("D144"); p != std::string::npos; p = s.find("D144", p + 1)) ++n;
    CHECK(n == 2, "mismatch frame must run both dispatches: %s", s.c_str());
    CHECK(MaskAt(o2, 0) == 3.f && MaskAt(o2, 1) == 3.f, "mismatch: mask must stay original");
    // replay sequence: T2=<g1> D144 T2=<g2> U D144
    const uint64_t h2 = g_handle - 1, h1 = g_handle - 2;  // handles of this frame: g1, g2, other
    const std::string want = "T2=" + std::to_string(h1) + " D144 T2=" + std::to_string(h2) + " U D144";
    CHECK(s.find(want) != std::string::npos, "replay sequence '%s' not in '%s'", want.c_str(), s.c_str());
    CHECK(T.counter(6) == 0, "disarmed after a bad pair");

    // re-qualify (3 frames), then a transition barrier between g1 and g2 replays g1 before it
    for (int f = 0; f < 3; ++f) Frame(o1, o2);
    CHECK(T.counter(6) == 1, "re-armed");
    Call(10, 0, (uintptr_t)POTHER);
    Call(29, 0);
    Call(31, 0, 11);
    Call(31, 1, 12);
    Call(25, (uintptr_t)P121);
    Bind(2.f, 3.f, 3.f);
    Call(14, 144, 81, 1);
    Barrier(false);
    Call(9, 0, 0, 0, "Close");
    s = Rec();
    printf("transition frame: %s\n", s.c_str());
    CHECK(s.find("D144 B Close") != std::string::npos, "held dispatch must be replayed before the transition: %s", s.c_str());

    // held then Close: replayed before Close
    Call(10, 0, (uintptr_t)POTHER);
    Call(29, 0);
    Call(31, 0, 11);
    Call(31, 1, 12);
    Call(25, (uintptr_t)P121);
    Bind(2.f, 3.f, 3.f);
    Call(14, 144, 81, 1);
    Call(9, 0, 0, 0, "Close");
    s = Rec();
    CHECK(s.find("D144 Close") != std::string::npos, "held dispatch must be replayed before Close: %s", s.c_str());
    CHECK(g_hold == nullptr, "hold must be clear");

    // reuse conflict: after a merged g2 the next dispatch keeps table 2 and creates no CBV -> merge disabled
    Frame(o1, o2);  // normal merged frame first (still armed)
    Call(10, 0, (uintptr_t)POTHER);
    Call(29, 0);
    Call(31, 0, 11);
    Call(31, 1, 12);
    Call(25, (uintptr_t)P121);
    Bind(2.f, 3.f, 3.f);
    Call(14, 144, 81, 1);
    Bind(3.f, 3.f, 3.f);
    Call(14, 144, 81, 1);
    Call(25, (uintptr_t)POTHER);
    Call(14, 7, 1, 1);  // no Bind: reads the rewritten cbuffer
    Call(9, 0, 0, 0, "Close");
    Rec();
    CHECK(T.counter(5) >= 1 && T.counter(7) == 1, "reuse conflict must disable (conflicts %llu disabled %llu)",
          (unsigned long long)T.counter(5), (unsigned long long)T.counter(7));
    s = Frame(o1, o2);
    n = 0;
    for (size_t p = s.find("D144"); p != std::string::npos; p = s.find("D144", p + 1)) ++n;
    CHECK(n == 2, "disabled: both dispatches must run: %s", s.c_str());

    // refs 1 / 2 under mask 3: never arms
    T.setMode(0, 1, 3);
    for (int f = 0; f < 6; ++f) {
        Call(10, 0, (uintptr_t)POTHER);
        Call(29, 0);
        Call(31, 0, 11);
        Call(25, (uintptr_t)P121);
        Bind(1.f, 3.f, 3.f);
        Call(14, 144, 81, 1);
        Bind(2.f, 3.f, 3.f);
        Call(14, 144, 81, 1);
        Call(9, 0, 0, 0, "Close");
        Rec();
    }
    CHECK(T.counter(6) == 0, "refs 1,2 must never arm");
}

int main() {
    TestUnion();
    TestMerge();
    printf(g_fail ? "GIMERGE HARNESS: %d FAILED\n" : "GIMERGE HARNESS: ALL OK\n", g_fail);
    return g_fail ? 1 : 0;
}
