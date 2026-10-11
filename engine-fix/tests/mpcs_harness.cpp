// mpcs_harness.cpp - MPCS caves (src\mpcskip.asm), the MpcCheck kernel map and the SHA-256 of the mpc file guard, without the game.
//   1. Every (entry A/B, cnt, #26 reached, stack depth, switch) against the model of what the skip may do:
//        #28-#31 skipped  <=>  switch on and 1 <= cnt <= 1024                          (verify_culling.py check B domain)
//        #26 skipped      <=>  switch on and #23 recorded (site A) and 1 <= cnt <= 32768 in THIS invocation
//      plus the registers the stock code needs at each landing (rax = [rsi], edx = 0Eh / 0Bh), rbx / rsi / rdi / rsp untouched,
//      the token pointer +8 on the site A path. Random sequences of invocations at a few stack depths (stale marks: an A-path
//      invocation that never reaches #26, followed by invocations at the same rsp) - the #26 skip must never come from another
//      invocation's mark.
//   2. mpcck::KernelOf against the record digests of the shipped and the mpc build of 0x29ee0902.file.
//   3. SHA-256 (FIPS 180-4 vectors) and, when present, gpu-shaders\replacement\0x29ee0902.file == the guard's hash.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>

#include "../src/mlprobe.h"
#include "../src/mpccheck.h"

extern "C" {
uint32_t MpcsInvoke(uint32_t cnt, uint32_t unused, uint32_t reachC, uint32_t depth);
extern uint32_t mph_bindA;
extern uint64_t mph_tok;
extern uint32_t mpcs_enable;
extern uint64_t mpcs_n28, mpcs_n26, mpcs_seen;
}

static int g_fail = 0;
#define CHECK(c, ...) do { if (!(c)) { if (g_fail++ < 30) { printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } } while (0)

int main() {
    // ---- 1. caves ----
    const uint32_t cnts[] = {0, 1, 2, 63, 64, 1023, 1024, 1025, 2048, 32767, 32768, 32769, 33000, 0x7FFFFFFFu, 0xFFFFFC00u, 0xFFFFFC01u, 0xFFFFFFFFu};
    uint64_t cases = 0;
    for (uint32_t en = 0; en < 2; ++en)
        for (uint32_t a = 0; a < 2; ++a)
            for (uint32_t cnt : cnts)
                for (uint32_t depth = 0; depth < 4; ++depth) {
                    mpcs_enable = en;
                    mph_bindA = a;
                    const uint64_t tok0 = mph_tok;
                    const uint32_t r = MpcsInvoke(cnt, 0, 1, depth);
                    const bool want28 = en && cnt >= 1 && cnt <= 1024;
                    const bool want26 = en && a && cnt >= 1 && cnt <= 32768;
                    CHECK(!(r & 0x100), "register check failed: en %u A %u cnt %u depth %u", en, a, cnt, depth);
                    CHECK(((r & 2) != 0) == want28 && ((r & 1) != 0) == !want28, "#28-#31: en %u A %u cnt %u -> bits %X", en, a, cnt, r);
                    CHECK(((r & 8) != 0) == want26 && ((r & 4) != 0) == !want26, "#26: en %u A %u cnt %u -> bits %X", en, a, cnt, r);
                    CHECK(mph_tok - tok0 == (a ? 8u : 0u), "token pointer: en %u A %u cnt %u: +%llu", en, a, cnt, (unsigned long long)(mph_tok - tok0));
                    ++cases;
                }
    // random sequences incl. A-path invocations that never reach #26 (stale marks) at the same stack depth
    std::mt19937 rng(4711);
    uint64_t seqCases = 0, stale = 0;
    for (int i = 0; i < 200000; ++i) {
        const uint32_t en = rng() % 4 != 0;
        const uint32_t a = rng() % 2;
        const uint32_t reach = (rng() % 5) != 0;
        const uint32_t depth = rng() % 3;
        uint32_t cnt;
        switch (rng() % 4) {
            case 0: cnt = 0; break;
            case 1: cnt = 1 + rng() % 1024; break;
            case 2: cnt = 1025 + rng() % 40000; break;
            default: cnt = cnts[rng() % (sizeof cnts / sizeof cnts[0])]; break;
        }
        mpcs_enable = en;
        mph_bindA = a;
        const uint32_t r = MpcsInvoke(cnt, 0, reach, depth);
        const bool want28 = en && cnt >= 1 && cnt <= 1024;
        const bool want26 = en && a && cnt >= 1 && cnt <= 32768;
        CHECK(!(r & 0x100), "seq %d: register check failed", i);
        CHECK(((r & 2) != 0) == want28, "seq %d: #28-#31 en %u A %u cnt %u reach %u -> %X", i, en, a, cnt, reach, r);
        if (reach) CHECK(((r & 8) != 0) == want26 && ((r & 12) != 0), "seq %d: #26 en %u A %u cnt %u -> %X", i, en, a, cnt, r);
        else CHECK(!(r & 12), "seq %d: #26 landed without being reached", i);
        if (a && !reach) ++stale;
        ++seqCases;
    }
    printf("caves: %llu edge cases + %llu random invocations (%llu left a mark unconsumed) - counters seen %llu, #28 skips %llu, #26 skips %llu\n",
           (unsigned long long)cases, (unsigned long long)seqCases, (unsigned long long)stale, (unsigned long long)mpcs_seen,
           (unsigned long long)mpcs_n28, (unsigned long long)mpcs_n26);

    // ---- 2. kernel map (record digests: gpu-shaders\culling\extract + the decompressed mpc build) ----
    struct K { uint32_t key; int k; bool mpc; };
    const K ks[] = {{0x34078a28, 23, false}, {0xc7e0a06b, 23, true}, {0x4225b9ff, 28, false}, {0x55198d09, 28, false}, {0x6b87b43e, 29, false},
                    {0xd17e19a0, 29, false}, {0x3ecb247d, 30, false}, {0x718b35fe, 30, false}, {0xde4d71b0, 31, false}, {0x676577ac, 31, false},
                    {0x5f610a3e, 26, false}, {0x304bac05, 24, false}, {0x1b4b5b75, 27, false}, {0xa09d4223, 0, false}, {0x18b6fee7, 0, false},
                    {0x9855f293, 0, false}, {0, 0, false}};
    for (const K& k : ks) {
        bool m = false;
        const int got = mpcck::KernelOf(k.key, &m);
        CHECK(got == k.k && m == k.mpc, "KernelOf(%08x) = %d/%d, want %d/%d", k.key, got, m, k.k, k.mpc);
    }
    printf("kernel map: %zu digests\n", sizeof ks / sizeof ks[0]);

    // ---- 3. SHA-256 ----
    auto hex = [](const uint8_t* d, char* o) { for (int i = 0; i < 32; ++i) sprintf_s(o + 2 * i, 3, "%02x", d[i]); };
    uint8_t d[32];
    char h[65];
    mlprobe::Sha256("", 0, d);
    hex(d, h);
    CHECK(!strcmp(h, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"), "sha256('') = %s", h);
    mlprobe::Sha256("abc", 3, d);
    hex(d, h);
    CHECK(!strcmp(h, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"), "sha256('abc') = %s", h);
    const char* m56 = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    mlprobe::Sha256(m56, strlen(m56), d);
    hex(d, h);
    CHECK(!strcmp(h, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"), "sha256(56) = %s", h);
    static uint8_t mil[1000000];
    memset(mil, 'a', sizeof mil);
    mlprobe::Sha256(mil, sizeof mil, d);
    hex(d, h);
    CHECK(!strcmp(h, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"), "sha256(1M a) = %s", h);
    char fh[65];
    if (mlprobe::Sha256File(L"..\\..\\..\\gpu-shaders\\replacement\\0x29ee0902.file", fh) ||
        mlprobe::Sha256File(L"..\\..\\..\\..\\gpu-shaders\\replacement\\0x29ee0902.file", fh)) {
        CHECK(!strcmp(fh, "3947f3d70264920e8cf0514139b715ce6552c6389b0e364ca9fb443cca4f0d73"), "mpc replacement file sha256 %s", fh);
        printf("sha256: FIPS vectors OK, staged mpc file %.16s... == guard\n", fh);
    } else {
        printf("sha256: FIPS vectors OK (staged mpc file not found - guard hash not cross-checked)\n");
    }
    if (g_fail) {
        printf("MPCS HARNESS: %d FAILURES\n", g_fail);
        return 1;
    }
    printf("MPCS HARNESS: ALL OK\n");
    return 0;
}
