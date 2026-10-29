// w1b_rpa_harness.cpp - offline proof of the W1B and RPA caves (src/drawprep.asm) against the ORIGINAL v2.02 code bytes
// read from re\Nioh3.dump.exe. Not a game test. Built and run by tests/run_w1b_rpa_harness.py.
//
//   w1b_rpa_harness.exe [dump]           all checks, must end ALL PASSED
//   w1b_rpa_harness.exe [dump] --derive  print the byte ranges RenderParamsAssign 247950 copies (input of tools/gen_rpa_copy.py)
//
// W1B  glyph padding clear 61BAC..61BC1 (61B1C). The original 22 bytes (dump), W1's 22 bytes (patches.json W1 new) and the
//      cave (switch on; switch off in W1 mode and in original mode) all run from the same full register state
//      (tests/drawprep_harness.asm RunCave) on random rows: r15d = pad 1..64 (the je at 61B99 skips pad 0), byte [r14] =
//      0..255, rbx = a row buffer with guard bytes. Checks: the buffer bytes are identical everywhere, and every GPR + xmm0-15
//      afterwards: switch on == original exactly (the cave is the original loop with `and [m],0` -> `mov [m],0`), switch off
//      == W1 / == original exactly.
// RPA  RenderParamsAssign 247950 (29.6 KB of field moves + calls 2477D4 / 2478A0 x2). The dump bytes 2477D4..24ECEA are copied
//      into one executable region (rel32 calls stay valid; no RIP-relative operands). --derive classifies every dst byte as
//      copied-from-the-same-src-offset or untouched (6 random trials, any other outcome fails). The test runs the original
//      and the cave (switch on, and off = original through the displaced `mov [rsp+8],rbx` + resume 247955) on 300k random
//      src/dst pairs (src 16-aligned as 2478A0's movaps requires; dst at every alignment 0..15; dst == src self-assign too),
//      both inside guard margins, and compares all bytes, rax (= dst) and the non-volatile GPRs + xmm6-15.
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
void caveW1b();
void caveRpa();
extern volatile uint32_t dp_w1b_enable, dp_w1b_w1, dp_rpa_enable;
extern uint64_t dp_w1b_resume, dp_rpa_resume;
}

enum { RAX, RCX, RDX, RBX, RSP, RBP, RSI, RDI, R8, R9, R10, R11, R12, R13, R14, R15 };
static const char* kGn[16] = {"rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"};
static int g_fail = 0;
static std::mt19937_64 rng(20261008);
static std::vector<uint8_t> g_img;

static void Fail(const char* what, long long c) {
    if (++g_fail <= 20) printf("FAIL case %lld: %s\n", c, what);
}
static void RandomRegs(Regs& r) {
    for (auto& v : r.g) v = rng();
    for (auto& xr : r.x)
        for (auto& b : xr) b = (uint8_t)rng();
}
static bool SameRegs(const Regs& a, const Regs& b, unsigned gmask, unsigned xmask, long long c, const char* tag) {
    bool ok = true;
    for (int i = 0; i < 16; ++i)
        if ((gmask >> i & 1) && a.g[i] != b.g[i]) {
            char m[96];
            snprintf(m, sizeof(m), "%s: %s differs", tag, kGn[i]);
            Fail(m, c);
            ok = false;
        }
    for (int i = 0; i < 16; ++i)
        if ((xmask >> i & 1) && memcmp(a.x[i], b.x[i], 16) != 0) {
            char m[96];
            snprintf(m, sizeof(m), "%s: xmm%d differs", tag, i);
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
static bool LoadDump(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    g_img.resize((size_t)n);
    const bool ok = fread(g_img.data(), 1, (size_t)n, f) == (size_t)n;
    fclose(f);
    return ok;
}
static void Hex(const uint8_t* p, size_t n, char* out) {
    for (size_t i = 0; i < n; ++i) sprintf(out + 3 * i, "%02X ", p[i]);
    if (n) out[3 * n - 1] = 0;
}

// ---- sites (v2.02; patches.json W1B / RPA) ----
constexpr uint32_t kW1bSite = 0x61BAC, kW1bLen = 22;
static const uint8_t kW1bOrig[] = {0x33, 0xC9, 0x41, 0x0F, 0xB6, 0x06, 0x41, 0x03, 0xC7, 0x03, 0xC1,
                                   0xFF, 0xC1, 0x83, 0x24, 0x83, 0x00, 0x41, 0x3B, 0xCF, 0x72, 0xEC};
static const uint8_t kW1New[] = {0x41, 0x0F, 0xB6, 0x06, 0x41, 0x03, 0xC7, 0x48, 0x8D, 0x3C, 0x83,
                                 0x44, 0x89, 0xF9, 0x31, 0xC0, 0xF3, 0xAB, 0x0F, 0x1F, 0x40, 0x00};  // patches.json W1 "new"
constexpr uint32_t kRpaLo = 0x2477D4, kRpaFn = 0x247950, kRpaHi = 0x24ECEA;  // 2477D4 / 2478A0 / 247950..24ECE9 (ret)
constexpr size_t kRpaSize = 0x3D38;  // last write: qword [dst+3D30h]
constexpr size_t kMargin = 0x400;

// Runs a code pointer as a Win64 call (rcx, rdx) from a full register state; the stub gives it 20h shadow space so the
// original's `mov [rsp+8],rbx` lands in the stub frame, then leaves through LandA.
static uint8_t* g_callStub = nullptr;
static void BuildCallStub(uint8_t* p) {
    // sub rsp,20h ; call [rip+x] ; add rsp,20h ; jmp LandA ; x: dq target (patched per call)
    const uint8_t a[] = {0x48, 0x83, 0xEC, 0x20, 0xFF, 0x15, 0x12, 0x00, 0x00, 0x00, 0x48, 0x83, 0xC4, 0x20};
    memcpy(p, a, sizeof(a));
    JmpAbs(p + sizeof(a), (const void*)&LandA);  // 14 bytes -> target qword at p + 28
}
static void CallViaStub(const void* fn) {
    const uint64_t t = (uint64_t)(uintptr_t)fn;
    memcpy(g_callStub + 28, &t, 8);
    RunCave(g_callStub);
}

int main(int argc, char** argv) {
    const char* dumpPath = "C:\\nioh3-engine\\re\\Nioh3.dump.exe";
    bool derive = false;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--derive")) derive = true;
        else dumpPath = argv[i];
    }
    if (!LoadDump(dumpPath) || g_img.size() < kRpaHi + 16) {
        printf("cannot read %s\n", dumpPath);
        return 2;
    }
    // ---- the dump must hold the bytes patches.json expects ----
    if (memcmp(&g_img[kW1bSite], kW1bOrig, kW1bLen) != 0) { printf("dump 61BAC != patches.json W1 orig\n"); return 2; }
    {
        const uint8_t head[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0xFA, 0x48, 0x8B, 0xD9};
        if (memcmp(&g_img[kRpaFn], head, sizeof(head)) != 0) { printf("dump 247950 prologue differs\n"); return 2; }
    }

    uint8_t* code = Exec(0x10000);
    g_callStub = code + 0x800;
    BuildCallStub(g_callStub);
    // RPA original: 2477D4..24ECEA at the same relative layout
    uint8_t* rpa = Exec(kRpaHi - kRpaLo + 0x100);
    memcpy(rpa, &g_img[kRpaLo], kRpaHi - kRpaLo);
    const void* rpaOrig = rpa + (kRpaFn - kRpaLo);
    // W1B: original 22 bytes / W1's 22 bytes, each followed by jmp LandC (= resume 61BC2)
    uint8_t* refOrig = code;
    memcpy(refOrig, &g_img[kW1bSite], kW1bLen);
    // the original's jb (72 EC) targets 61BAE = site+2: relative, so it stays inside the copy
    JmpAbs(refOrig + kW1bLen, (const void*)&LandC);
    uint8_t* refW1 = code + 0x100;
    memcpy(refW1, kW1New, kW1bLen);
    JmpAbs(refW1 + kW1bLen, (const void*)&LandC);
    FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
    dp_w1b_resume = (uint64_t)(uintptr_t)&LandC;
    dp_rpa_resume = (uint64_t)(uintptr_t)(rpa + (kRpaFn + 5 - kRpaLo));

    // ---- RPA buffers ----
    const size_t span = kRpaSize + 2 * kMargin + 64;
    std::vector<uint8_t> srcBuf(span + 64), d0(span + 64), d1(span + 64), init(span + 64);
    auto al16 = [](uint8_t* p) { return (uint8_t*)(((uintptr_t)p + 15) & ~(uintptr_t)15); };

    if (derive) {
        // class per byte: 0 unknown, 1 copied, 2 untouched; trials where src == dst0 at a byte say nothing about it
        std::vector<int> cls(kRpaSize + 2 * kMargin, 0);
        for (int t = 0; t < 6; ++t) {
            uint8_t* s = al16(srcBuf.data()) + kMargin;
            uint8_t* d = al16(d0.data()) + kMargin + (t % 3) * 4;
            for (size_t i = 0; i < span; ++i) { srcBuf[i] = (uint8_t)rng(); d0[i] = (uint8_t)rng(); }
            std::vector<uint8_t> before(d - kMargin, d + kRpaSize + kMargin);
            Regs in;
            RandomRegs(in);
            in.g[RCX] = (uint64_t)(uintptr_t)d;
            in.g[RDX] = (uint64_t)(uintptr_t)s;
            h_in = in;
            CallViaStub(rpaOrig);
            for (size_t i = 0; i < cls.size(); ++i) {
                const long long o = (long long)i - (long long)kMargin;
                const uint8_t a = d[o], b0 = before[i], sv = s[o];
                if (sv == b0) continue;
                const int k = a == sv ? 1 : a == b0 ? 2 : 3;
                if (k == 3 || (cls[i] && cls[i] != k)) { printf("byte %+lld: neither copied nor untouched\n", o); return 1; }
                cls[i] = k;
            }
        }
        int nr = 0;
        for (size_t i = 0; i < cls.size();) {
            if (cls[i] == 0) { printf("byte %+lld: unresolved\n", (long long)i - (long long)kMargin); return 1; }
            if (cls[i] != 1) { ++i; continue; }
            size_t j = i;
            while (j < cls.size() && cls[j] == 1) ++j;
            const long long o = (long long)i - (long long)kMargin;
            if (o < 0 || j - kMargin > kRpaSize) { printf("copy outside 0..%zX\n", kRpaSize); return 1; }
            printf("%llX %zX\n", o, j - i);
            ++nr;
            i = j;
        }
        fprintf(stderr, "%d ranges\n", nr);
        return 0;
    }

    // ---- W1B ----
    {
        static uint8_t row[0x2000];
        static uint8_t hdr[16];
        long long cases = 0;
        for (long long c = 0; c < 200000; ++c) {
            const uint32_t pad = c % 5 == 0 ? 1u + (uint32_t)(rng() % 64) : (c % 5 == 1 ? 1u + (uint32_t)(rng() % 4) : 1u);
            hdr[0] = (uint8_t)rng();
            hdr[1] = (uint8_t)rng();
            Regs in;
            RandomRegs(in);
            uint8_t* base = row + 0x40 + (rng() % 16) * 4;
            in.g[RBX] = (uint64_t)(uintptr_t)base;
            in.g[R14] = (uint64_t)(uintptr_t)hdr;
            in.g[R15] = (in.g[R15] & 0xFFFFFFFF00000000ull) | pad;  // r15d = pad (the upper half is whatever it was)
            for (auto& b : row) b = (uint8_t)rng();
            // original
            std::vector<uint8_t> start(row, row + sizeof(row));
            h_in = in;
            RunCave(refOrig);
            const Regs rOrig = h_out;
            std::vector<uint8_t> mOrig(row, row + sizeof(row));
            if (h_land != 3) Fail("W1B original stub did not reach 61BC2", c);
            // W1 bytes
            memcpy(row, start.data(), sizeof(row));
            h_in = in;
            RunCave(refW1);
            const Regs rW1 = h_out;
            if (memcmp(row, mOrig.data(), sizeof(row)) != 0) Fail("W1 bytes: row differs from the original (pad >= 1)", c);
            // cave: on, off/W1 mode, off/original mode
            for (int mode = 0; mode < 3; ++mode) {
                dp_w1b_enable = mode == 0 ? 1u : 0u;
                dp_w1b_w1 = mode == 1 ? 1u : 0u;
                memcpy(row, start.data(), sizeof(row));
                h_in = in;
                RunCave((const void*)&caveW1b);
                if (h_land != 3) Fail("W1B cave did not resume at 61BC2", c);
                if (memcmp(row, mOrig.data(), sizeof(row)) != 0) Fail(mode == 0 ? "W1B on: row differs" : "W1B off: row differs", c);
                const Regs& want = mode == 1 ? rW1 : rOrig;
                SameRegs(h_out, want, 0xFFFFu & ~(1u << RSP), 0xFFFFu, c, mode == 0 ? "W1B on vs original" : mode == 1 ? "W1B off vs W1" : "W1B off vs original");
            }
            ++cases;
        }
        dp_w1b_enable = dp_w1b_w1 = 0;
        printf("W1B: %lld rows x (original, W1, cave on, cave off/W1, cave off/original): bytes + all GPR/xmm identical\n", cases);
    }

    // ---- RPA ----
    {
        const unsigned nonvol = 1u << RBX | 1u << RBP | 1u << RSI | 1u << RDI | 1u << R12 | 1u << R13 | 1u << R14 | 1u << R15;
        const unsigned xnonvol = 0xFFC0u;  // xmm6-15
        long long cases = 0;
        for (long long c = 0; c < 300000; ++c) {
            uint8_t* s = al16(srcBuf.data()) + kMargin;
            const size_t dal = (size_t)(c % 16);
            const bool self = c % 97 == 5;
            const int fill = (int)(c % 4);  // random / zeros / 0xFF / src-equal dst (exercises "already equal" bytes)
            for (size_t i = 0; i < span; ++i) {
                srcBuf[i] = fill == 1 ? 0 : fill == 2 ? 0xFF : (uint8_t)rng();
                init[i] = (uint8_t)rng();
            }
            uint8_t* dA = self ? s : al16(d0.data()) + kMargin + dal;
            uint8_t* dB = self ? nullptr : al16(d1.data()) + kMargin + dal;
            if (fill == 3 && !self) memcpy(init.data() + kMargin, s, kRpaSize);
            std::vector<uint8_t> srcCopy;
            if (!self) {
                memcpy(dA - kMargin, init.data(), kRpaSize + 2 * kMargin);
                memcpy(dB - kMargin, init.data(), kRpaSize + 2 * kMargin);
            } else {
                srcCopy.assign(s - kMargin, s + kRpaSize + kMargin);
            }
            Regs in;
            RandomRegs(in);
            in.g[RCX] = (uint64_t)(uintptr_t)dA;
            in.g[RDX] = (uint64_t)(uintptr_t)s;
            // original
            h_in = in;
            CallViaStub(rpaOrig);
            const Regs rO = h_out;
            if (rO.g[RAX] != (uint64_t)(uintptr_t)dA) Fail("RPA original: rax != dst", c);
            std::vector<uint8_t> afterO(dA - kMargin, dA + kRpaSize + kMargin);
            for (uint32_t en = 0; en < 2; ++en) {
                uint8_t* dC = self ? s : dB;
                if (self) memcpy(s - kMargin, srcCopy.data(), srcCopy.size());
                else if (en == 1) memcpy(dB - kMargin, init.data(), kRpaSize + 2 * kMargin);
                dp_rpa_enable = en;
                Regs in2 = in;
                in2.g[RCX] = (uint64_t)(uintptr_t)dC;
                h_in = in2;
                CallViaStub((const void*)&caveRpa);
                if (h_out.g[RAX] != (uint64_t)(uintptr_t)dC) Fail(en ? "RPA on: rax != dst" : "RPA off: rax != dst", c);
                if (memcmp(dC - kMargin, afterO.data(), afterO.size()) != 0) {
                    size_t k = 0;
                    while (dC[(long long)k - (long long)kMargin] == afterO[k]) ++k;
                    char m[96];
                    snprintf(m, sizeof(m), "RPA %s: byte %+lld differs", en ? "on" : "off", (long long)k - (long long)kMargin);
                    Fail(m, c);
                }
                SameRegs(h_out, rO, nonvol, xnonvol, c, en ? "RPA on (non-volatiles)" : "RPA off (non-volatiles)");
                SameRegs(h_out, in2, nonvol, xnonvol, c, en ? "RPA on vs input (non-volatiles)" : "RPA off vs input (non-volatiles)");
            }
            ++cases;
        }
        dp_rpa_enable = 0;
        printf("RPA: %lld src/dst pairs (dst alignment 0..15, self-assign, fills) x (original, cave off, cave on): all bytes in\n"
               "     +-%zX guard margins, rax and the non-volatile GPRs + xmm6-15 identical\n",
               cases, kMargin);
    }
    // ---- timing on this PC (information only; the game decides) ----
    {
        LARGE_INTEGER f, t0, t1;
        QueryPerformanceFrequency(&f);
        // W1B: one pad-1 row clear on write-combined memory (the atlas staging is an upload heap), through RunCave
        uint8_t* wc = (uint8_t*)VirtualAlloc(nullptr, 1 << 16, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE | PAGE_WRITECOMBINE);
        static uint8_t hdr2[16] = {7, 1};
        if (wc) {
            const void* v[4] = {refOrig, refW1, (const void*)&caveW1b, (const void*)&caveW1b};
            const char* nm[4] = {"original (RMW)", "W1 (rep stosd)", "W1B off->W1", "W1B on"};
            for (int k = 0; k < 4; ++k) {
                dp_w1b_enable = k == 3 ? 1u : 0u;
                dp_w1b_w1 = 1;
                Regs in;
                RandomRegs(in);
                in.g[R14] = (uint64_t)(uintptr_t)hdr2;
                in.g[R15] = 1;
                const int n = 200000;
                QueryPerformanceCounter(&t0);
                for (int i = 0; i < n; ++i) {
                    in.g[RBX] = (uint64_t)(uintptr_t)(wc + (i & 1023) * 48);
                    h_in = in;
                    RunCave(v[k]);
                }
                QueryPerformanceCounter(&t1);
                printf("  time W1B %-15s %6.1f ns/row (incl. ~same RunCave overhead)\n", nm[k], (t1.QuadPart - t0.QuadPart) * 1e9 / f.QuadPart / n);
            }
            dp_w1b_enable = dp_w1b_w1 = 0;
        }
        using Fn = void* (*)(void*, const void*);
        uint8_t* s = al16(srcBuf.data()) + kMargin;
        uint8_t* d = al16(d0.data()) + kMargin + 4;
        for (int k = 0; k < 2; ++k) {
            dp_rpa_enable = (uint32_t)k;
            const Fn fn = k ? (Fn)(void*)&caveRpa : (Fn)rpaOrig;
            const int n = 100000;
            QueryPerformanceCounter(&t0);
            for (int i = 0; i < n; ++i) fn(d, s);
            QueryPerformanceCounter(&t1);
            printf("  time RPA %-15s %6.1f ns/call (hot loop: 29.6 KB of code stays in L1i/L2 here, unlike in the game)\n",
                   k ? "cave on" : "original", (t1.QuadPart - t0.QuadPart) * 1e9 / f.QuadPart / n);
        }
        dp_rpa_enable = 0;
    }
    if (g_fail) {
        printf("FAILED: %d mismatches\n", g_fail);
        return 1;
    }
    printf("ALL PASSED\n");
    return 0;
}
