// asyncslots_harness.cpp - offline proof of the AS caves (src/asyncslots.asm) against the ORIGINAL v2.02 AsyncSlotGetCtx
// 52BE1C..52BE3D read from re\Nioh3.dump.exe. Not a game test. Built and run by tests/run_asyncslots_harness.py.
//
// The 34 original bytes are copied into an executable region (short branches only, no RIP-relative operand: position
// independent). A producer is emulated by a stub `call <target>; jmp LandA` run from a full random register state
// (tests/drawprep_harness.asm RunCave): target = the original function, or the near trampoline -> caveAsK -> resume at +6.
// Claim proven for every case: running the cave path on a slot-scheduler image with mask M and switch state (enable, and_on,
// and_off) leaves EXACTLY the same 16 GPRs + xmm0-15 as the original function on the same image with mask M & and_X
// (X = on when enabled, else off): the cave only narrows the slot mask, nothing else. Plus: rax equals the model
// (bit set && idx >= 0 ? ctx[idx] : NULL), the scheduler image is untouched, and the per-site counter counts each call once.
// Cases: 1.2M random (mask, 17C indices incl. negative, ctx pointers, slot 0..12, switch values incl. all-ones = stock),
// every cave entry caveAs0..5.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

struct Regs {
    uint64_t g[16];  // rax rcx rdx rbx rsp rbp rsi rdi r8..r15
    uint8_t x[16][16];
};
extern "C" {
extern Regs h_in, h_out;
extern volatile uint32_t h_land;
void RunCave(const void* target);
void LandA();
void LandB();
void LandC();
void caveAs0();
void caveAs1();
void caveAs2();
void caveAs3();
void caveAs4();
void caveAs5();
extern volatile uint32_t as_enable, as_and_on, as_and_off;
extern uint64_t as_resume;
extern volatile uint32_t as_calls[8];
}

enum { RAX, RCX, RDX, RBX, RSP, RBP, RSI, RDI, R8, R9, R10, R11, R12, R13, R14, R15 };
static const char* kGn[16] = {"rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"};
static int g_fail = 0;
static std::mt19937_64 rng(20261009);
static std::vector<uint8_t> g_img;

static void Fail(const char* what, long long c) {
    if (++g_fail <= 20) printf("FAIL case %lld: %s\n", c, what);
}
static bool LoadDump(const char* path) {
    FILE* f = nullptr;
    if (fopen_s(&f, path, "rb") || !f) return false;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    g_img.resize((size_t)n);
    const bool ok = fread(g_img.data(), 1, (size_t)n, f) == (size_t)n;
    fclose(f);
    return ok;
}
static uint8_t* JmpAbs(uint8_t* p, const void* to) {
    p[0] = 0xFF; p[1] = 0x25; memset(p + 2, 0, 4);
    const uint64_t t = (uint64_t)(uintptr_t)to;
    memcpy(p + 6, &t, 8);
    return p + 14;
}
// E8 rel32 at p to `to`, then jmp LandA (what the producer does after the call does not matter here)
static void CallStub(uint8_t* p, const uint8_t* to) {
    p[0] = 0xE8;
    const int32_t r = (int32_t)((intptr_t)to - (intptr_t)(p + 5));
    memcpy(p + 1, &r, 4);
    JmpAbs(p + 5, (const void*)&LandA);
}

constexpr uint32_t kGetCtx = 0x52BE1C;
static const uint8_t kGetCtxBytes[] = {0x8B, 0x81, 0xCC, 0x01, 0x00, 0x00, 0x0F, 0xA3, 0xD0, 0x72, 0x03, 0x33, 0xC0, 0xC3, 0x8B, 0xC2, 0x48,
                                       0x63, 0x94, 0x81, 0x7C, 0x01, 0x00, 0x00, 0x85, 0xD2, 0x78, 0xEF, 0x48, 0x8B, 0x44, 0xD1, 0x10, 0xC3};

int main(int argc, char** argv) {
    const char* dumpPath = argc > 1 ? argv[1] : "re\\Nioh3.dump.exe";
    if (!LoadDump(dumpPath) || g_img.size() < kGetCtx + 0x40) {
        printf("cannot read dump %s\n", dumpPath);
        return 2;
    }
    if (memcmp(&g_img[kGetCtx], kGetCtxBytes, sizeof(kGetCtxBytes)) != 0) {
        printf("dump 52BE1C != asyncslots.cpp kGetCtxBytes\n");
        return 2;
    }
    uint8_t* x = (uint8_t*)VirtualAlloc(nullptr, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!x) return 3;
    memset(x, 0xCC, 0x1000);
    uint8_t* fn = x;  // the original bytes
    memcpy(fn, &g_img[kGetCtx], sizeof(kGetCtxBytes));
    uint8_t* stubOrig = x + 0x100;
    CallStub(stubOrig, fn);
    void (*const caves[6])() = {&caveAs0, &caveAs1, &caveAs2, &caveAs3, &caveAs4, &caveAs5};
    uint8_t* tramp[6];
    uint8_t* stubCave[6];
    for (int k = 0; k < 6; ++k) {
        tramp[k] = x + 0x200 + 0x10 * k;
        JmpAbs(tramp[k], (const void*)caves[k]);
        stubCave[k] = x + 0x300 + 0x20 * k;
        CallStub(stubCave[k], tramp[k]);
    }
    FlushInstructionCache(GetCurrentProcess(), x, 0x1000);
    as_resume = (uint64_t)(uintptr_t)(fn + 6);

    // two scheduler images (A: the cave's input, B: the original's input with the narrowed mask), 0x200 bytes + guards
    alignas(16) static uint8_t schedA[0x400], schedB[0x400], snapA[0x400];
    const long long kCases = 1200000;
    long long ctxHits = 0, nulls = 0, masked = 0;
    for (long long c = 0; c < kCases; ++c) {
        for (auto& b : schedA) b = (uint8_t)rng();
        uint8_t* sa = schedA + 0x100;  // sched pointer (rcx)
        uint32_t mask = (uint32_t)rng();
        if (c % 7 == 0) mask = 0xFFFFFFFFu;
        if (c % 11 == 0) mask = 0;
        memcpy(sa + 0x1CC, &mask, 4);
        for (int i = 0; i < 13; ++i) {
            int32_t idx = (int32_t)(rng() % 15) - 2;  // -2..12 (negative = no context)
            memcpy(sa + 0x17C + 4 * i, &idx, 4);
        }
        const unsigned slot = (unsigned)(rng() % 13);
        uint32_t en = (uint32_t)(rng() & 1), aon = (uint32_t)rng(), aoff = (uint32_t)rng();
        if (c % 5 == 0) aon = 0xFFFFFFFFu;
        if (c % 6 == 0) aoff = 0xFFFFFFFFu;
        if (c % 13 == 0) aoff = ~(1u << slot);
        const int k = (int)(c % 6);
        memcpy(schedB, schedA, sizeof schedA);
        const uint32_t narrowed = mask & (en ? aon : aoff);
        memcpy(schedB + 0x100 + 0x1CC, &narrowed, 4);
        memcpy(snapA, schedA, sizeof schedA);

        Regs in;
        for (auto& v : in.g) v = rng();
        for (auto& xr : in.x)
            for (auto& b : xr) b = (uint8_t)rng();
        in.g[RDX] = (in.g[RDX] & ~0xFFFFFFFFull) | slot;  // edx = slot (the code reads edx only); upper half random
        // cave path on A
        as_enable = en;
        as_and_on = aon;
        as_and_off = aoff;
        const uint32_t before = as_calls[k];
        in.g[RCX] = (uint64_t)(uintptr_t)sa;
        h_in = in;
        RunCave(stubCave[k]);
        if (h_land != 1) { Fail("cave path did not return to the stub", c); continue; }
        const Regs outCave = h_out;
        if (as_calls[k] != before + 1) Fail("site counter not +1", c);
        if (memcmp(schedA, snapA, sizeof schedA) != 0) Fail("scheduler image written", c);
        // original on B
        in.g[RCX] = (uint64_t)(uintptr_t)(schedB + 0x100);
        h_in = in;
        RunCave(stubOrig);
        if (h_land != 1) { Fail("original did not return to the stub", c); continue; }
        Regs outOrig = h_out;
        // rcx differs only because the two images live at different addresses: compare it against its own input
        if (outCave.g[RCX] != (uint64_t)(uintptr_t)sa || outOrig.g[RCX] != (uint64_t)(uintptr_t)(schedB + 0x100)) Fail("rcx changed", c);
        outOrig.g[RCX] = outCave.g[RCX];
        for (int i = 0; i < 16; ++i)
            if (outCave.g[i] != outOrig.g[i]) {
                char m[64];
                snprintf(m, sizeof m, "%s: cave != original(mask & and)", kGn[i]);
                Fail(m, c);
            }
        for (int i = 0; i < 16; ++i)
            if (memcmp(outCave.x[i], outOrig.x[i], 16) != 0) {
                char m[64];
                snprintf(m, sizeof m, "xmm%d differs", i);
                Fail(m, c);
            }
        // model
        int32_t idx;
        memcpy(&idx, sa + 0x17C + 4 * slot, 4);
        uint64_t want = 0;
        if ((narrowed >> slot) & 1u) {
            if (idx >= 0) memcpy(&want, sa + 0x10 + 8 * (int64_t)idx, 8);
        }
        if (outCave.g[RAX] != want) Fail("rax != model", c);
        if (want) ++ctxHits; else ++nulls;
        if (((mask >> slot) & 1u) && !((narrowed >> slot) & 1u)) ++masked;
        // with nothing masked the cave path must equal the original on the unchanged image (stock behaviour)
        if (c % 64 == 0) {
            as_and_on = as_and_off = 0xFFFFFFFFu;
            in.g[RCX] = (uint64_t)(uintptr_t)sa;
            h_in = in;
            RunCave(stubCave[k]);
            const Regs o1 = h_out;
            memcpy(schedB + 0x100 + 0x1CC, &mask, 4);
            in.g[RCX] = (uint64_t)(uintptr_t)(schedB + 0x100);
            h_in = in;
            RunCave(stubOrig);
            Regs o2 = h_out;
            o2.g[RCX] = o1.g[RCX];
            if (memcmp(o1.g, o2.g, sizeof o1.g) != 0 || memcmp(o1.x, o2.x, sizeof o1.x) != 0) Fail("all-ones mask: cave != original", c);
        }
    }
    printf("AS caves: %lld cases (context returned %lld, NULL %lld, NULL because the switch masked the slot %lld), 6 entries\n",
           kCases, ctxHits, nulls, masked);
    if (g_fail) {
        printf("FAILED (%d)\n", g_fail);
        return 1;
    }
    printf("ALL PASSED\n");
    return 0;
}
