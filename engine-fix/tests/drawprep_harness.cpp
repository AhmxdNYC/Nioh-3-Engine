// drawprep_harness.cpp - offline test of the real src/drawprep.asm caves (DQ, P1E7) on this PC. Not a game test.
// Built and run by tests/run_drawprep_harness.py. Each case runs the cave AND the original v2.02 instruction bytes (copied
// from patches.json orig, rebuilt with absolute exits) from the same full register state (tests/drawprep_harness.asm) and
// compares where they leave to and every GPR + xmm0-15 afterwards:
//   DQ   switch off: identical to the original in every register and exit; switch on: exit = (quantum > elapsed, ordered),
//        every register unchanged; quantum == xmm6 gives the original's exits exactly; the early-retry counter counts
//        exactly the exits the original would still have spun on. Special floats (NaN, inf, -0, denormals, boundaries).
//   P1E7 switch off/on: r14 / r8 as the original, rcx / r10 the only other registers that may change (on), nothing else;
//        the texture array ends at a PAGE_NOACCESS page, so any read past tex[count-1] (k+1 / k+2 bound) faults.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>

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
void caveDq();
void caveP1e7();
extern volatile uint32_t dp_dq_enable, dp_p1e7_enable, dp_dq_early;
extern float dp_dq_quantum;
extern uint64_t dp_dq_loop, dp_dq_exit, dp_p1e7_resume;
}

enum { RAX, RCX, RDX, RBX, RSP, RBP, RSI, RDI, R8, R9, R10, R11, R12, R13, R14, R15 };
static const char* kGn[16] = {"rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"};
static int g_fail = 0;
static std::mt19937_64 rng(20261008);

static void Fail(const char* what, int c) {
    if (++g_fail <= 20) printf("FAIL case %d: %s\n", c, what);
}
static void RandomRegs(Regs& r) {
    for (auto& v : r.g) v = rng();
    for (auto& xr : r.x)
        for (auto& b : xr) b = (uint8_t)rng();
}
static void SetF(Regs& r, int x, float f) { memcpy(r.x[x], &f, 4); }
// Compare out vs a reference out, ignoring the GPRs in `skip` (bit mask); rsp is the host's and always equal.
static bool SameRegs(const Regs& a, const Regs& b, unsigned skip, int c, const char* tag) {
    bool ok = true;
    for (int i = 0; i < 16; ++i)
        if (!(skip >> i & 1) && a.g[i] != b.g[i]) {
            char m[96];
            snprintf(m, sizeof(m), "%s: %s differs", tag, kGn[i]);
            Fail(m, c);
            ok = false;
        }
    if (memcmp(a.x, b.x, sizeof(a.x)) != 0) {
        char m[96];
        snprintf(m, sizeof(m), "%s: xmm differs", tag);
        Fail(m, c);
        ok = false;
    }
    return ok;
}
static uint8_t* Exec(size_t n) { return (uint8_t*)VirtualAlloc(nullptr, n, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE); }
static uint8_t* JmpAbs(uint8_t* p, const void* to) {
    p[0] = 0xFF; p[1] = 0x25; memset(p + 2, 0, 4);
    const uint64_t t = (uint64_t)(uintptr_t)to;
    memcpy(p + 6, &t, 8);
    return p + 14;
}

int main() {
    // ---- reference stubs from the original v2.02 bytes ----
    uint8_t* code = Exec(4096);
    // DQ original: comiss xmm6,xmm0 (0F 2F F0); ja loop -> rebuilt as ja +14 (over the exit jmp) -> LandA = loop, LandB = exit
    uint8_t* refDq = code;
    {
        uint8_t* p = refDq;
        const uint8_t head[] = {0x0F, 0x2F, 0xF0, 0x77, 0x0E};
        memcpy(p, head, 5); p += 5;
        p = JmpAbs(p, (const void*)&LandB);
        JmpAbs(p, (const void*)&LandA);
    }
    // P1E7 original: mov r14,[r12]; mov r8,[r11+1C8h] (patches.json P1E7 orig), then -> LandC = resume 1E72D1
    uint8_t* refPf = code + 0x100;
    {
        const uint8_t orig[] = {0x4D, 0x8B, 0x34, 0x24, 0x4D, 0x8B, 0x83, 0xC8, 0x01, 0x00, 0x00};
        memcpy(refPf, orig, sizeof(orig));
        JmpAbs(refPf + sizeof(orig), (const void*)&LandC);
    }
    FlushInstructionCache(GetCurrentProcess(), code, 4096);
    dp_dq_loop = (uint64_t)(uintptr_t)&LandA;
    dp_dq_exit = (uint64_t)(uintptr_t)&LandB;
    dp_p1e7_resume = (uint64_t)(uintptr_t)&LandC;

    // ---- DQ ----
    const float specials[] = {0.0f, -0.0f, 1e-45f, -1e-45f, 1e-38f, 0.0002f, std::nextafter(0.0002f, 1.0f), std::nextafter(0.0002f, 0.0f),
                              0.001f, std::nextafter(0.001f, 1.0f), std::nextafter(0.001f, 0.0f), 0.0005f, 0.5f, -1.0f, INFINITY,
                              -INFINITY, NAN, -NAN, 1e30f};
    const int nsp = (int)(sizeof(specials) / sizeof(specials[0]));
    int dqCases = 0, dqEarly = 0;
    for (int c = 0; c < 60000; ++c) {
        Regs in;
        RandomRegs(in);
        float x0, x6, q;
        auto pick = [&](int mode) -> float {
            if (mode == 0) return specials[rng() % nsp];
            if (mode == 1) return std::uniform_real_distribution<float>(0.0f, 0.0015f)(rng);
            uint32_t u = (uint32_t)rng();
            float f;
            memcpy(&f, &u, 4);
            return f;
        };
        x0 = pick((int)(rng() % 3));
        x6 = (rng() % 4) ? 0.001f : pick((int)(rng() % 3));
        q = (rng() % 4) ? 0.0002f : pick((int)(rng() % 2));
        if (c % 7 == 0) q = x6;  // quantum == xmm6: the enabled path must give the original's exits
        SetF(in, 0, x0);
        SetF(in, 6, x6);
        dp_dq_quantum = q;
        // reference (original bytes)
        h_in = in;
        RunCave(refDq);
        const Regs ref = h_out;
        const uint32_t refLand = h_land;
        if (refLand != ((x6 > x0) ? 1u : 2u)) Fail("DQ reference stub disagrees with C (ja model)", c);
        // switch off
        dp_dq_enable = 0;
        uint32_t e0 = dp_dq_early;
        h_in = in;
        RunCave((const void*)&caveDq);
        if (h_land != refLand) Fail("DQ off: exit differs from the original", c);
        SameRegs(h_out, ref, 0, c, "DQ off");
        if (dp_dq_early != e0) Fail("DQ off: early counter moved", c);
        // switch on
        dp_dq_enable = 1;
        e0 = dp_dq_early;
        h_in = in;
        RunCave((const void*)&caveDq);
        const uint32_t want = (q > x0) ? 1u : 2u;  // C '>' is false when unordered, like ja
        if (h_land != want) Fail("DQ on: exit != (quantum > elapsed)", c);
        if (q == x6 && !std::isnan(q) && h_land != refLand) Fail("DQ on, quantum == xmm6: exit differs from the original", c);
        SameRegs(h_out, in, 1u << RSP, c, "DQ on (vs input)");
        const uint32_t early = dp_dq_early - e0;
        // counted only on an ordered exit (unordered exits leave through `jp` uncounted; the game's quantum is never NaN)
        const uint32_t wantEarly = (want == 2u && !std::isnan(q) && !std::isnan(x0) && x6 > x0) ? 1u : 0u;
        if (early != wantEarly) Fail("DQ on: early-retry counter wrong", c);
        dqEarly += (int)early;
        ++dqCases;
    }
    dp_dq_enable = 0;
    printf("DQ: %d cases x (original, off, on); %d early retries counted\n", dqCases, dqEarly);

    // ---- P1E7 ----
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    const size_t pg = si.dwPageSize;
    uint8_t* arena = (uint8_t*)VirtualAlloc(nullptr, pg * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    DWORD old;
    VirtualProtect(arena + pg, pg, PAGE_NOACCESS, &old);
    static uint8_t objs[16][0x200], strm[16][0x200], group[0x40], self[0x200];
    const uint64_t thisVal = 0x1122334455667788ull;
    memcpy(self + 0x1C8, &thisVal, 8);
    int pfCases = 0;
    for (int c = 0; c < 20000; ++c) {
        const int count = 1 + (int)(rng() % 8);
        uint64_t* tex = (uint64_t*)(arena + pg - 8 * count);  // tex[count-1] is the last qword before the guard page
        for (int i = 0; i < count; ++i) {
            const int kind = (int)(rng() % 4);
            uint8_t* o = objs[(c + i) % 16];
            uint64_t s = kind == 2 ? 0 : (uint64_t)(uintptr_t)strm[(c * 3 + i) % 16];
            memcpy(o + 0x68, &s, 8);
            tex[i] = kind == 3 ? 0 : (uint64_t)(uintptr_t)o;
        }
        const int32_t cnt = count;
        memcpy(group + 0x18, &cnt, 4);
        const int k = (int)(rng() % (uint64_t)count);  // the loop head guarantees k < [rdx+18h]
        Regs in;
        RandomRegs(in);
        in.g[R12] = (uint64_t)(uintptr_t)&tex[k];
        in.g[R15] = (in.g[R15] & 0xFFFFFFFF00000000ull) | (uint32_t)k;  // only r15d is k
        in.g[RDX] = (uint64_t)(uintptr_t)group;
        in.g[R11] = (uint64_t)(uintptr_t)self;
        h_in = in;
        RunCave(refPf);
        const Regs ref = h_out;
        if (h_land != 3 || ref.g[R14] != tex[k] || ref.g[R8] != thisVal) Fail("P1E7 reference stub wrong", c);
        for (uint32_t en = 0; en < 2; ++en) {
            dp_p1e7_enable = en;
            h_in = in;
            RunCave((const void*)&caveP1e7);
            if (h_land != 3) Fail(en ? "P1E7 on: did not resume at 1E72D1" : "P1E7 off: did not resume at 1E72D1", c);
            SameRegs(h_out, ref, en ? (1u << RCX | 1u << R10) : 0u, c, en ? "P1E7 on" : "P1E7 off");
        }
        ++pfCases;
    }
    dp_p1e7_enable = 0;
    printf("P1E7: %d cases x (original, off, on), texture array against a guard page\n", pfCases);
    if (g_fail) {
        printf("FAILED: %d mismatches\n", g_fail);
        return 1;
    }
    printf("ALL PASSED\n");
    return 0;
}
