// padmap_harness.cpp - PadListGuard map-miss cave (GitHub #96) offline test. Not a game test.
//   padmap_harness.exe <Nioh3.dump.exe>   -> ALL OK, exit 0
// 1. Dump checks: at() miss path bytes at kAtMiss (lea rcx,[msg] ; call thrower), msg == "invalid unordered_map<K, T> key",
//    every kAtCalls entry is `call kAtFn`, and kAtFn is reached from the miss path's function.
// 2. Runs the cave built by BuildMapMiss: a caller in the table gets a zeroed scratch value (dirty scratch is re-zeroed),
//    callee-saved registers and rsp intact, counter +1; a caller NOT in the table reaches the original thrower with rcx = msg.
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "../src/padguard_cave.h"

using namespace padguard_cave;

namespace {
int g_fail = 0;
#define CHECK(c, ...)                                  \
    do {                                               \
        if (!(c)) {                                    \
            ++g_fail;                                  \
            printf("FAIL %s:%d ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                       \
            printf("\n");                              \
        }                                              \
    } while (0)

CONTEXT g_ctx;
volatile int g_phase = 0;
volatile uintptr_t g_msgSeen = 0;
extern "C" void Thrower(uintptr_t msg) {  // stands in for 0xB5FB84 (throws std::out_of_range in the game)
    g_msgSeen = msg;
    g_phase = 2;
    RtlRestoreContext(&g_ctx, nullptr);
}
}  // namespace

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    const char* dump = argc > 1 ? argv[1] : "C:\\Users\\Ahmad Hamza\\nioh3-engine\\re\\Nioh3.dump.exe";
    FILE* f = fopen(dump, "rb");
    if (!f) {
        printf("FAIL cannot open %s\n", dump);
        return 2;
    }
    std::vector<uint8_t> d;
    _fseeki64(f, 0, SEEK_END);
    d.resize((size_t)_ftelli64(f));
    _fseeki64(f, 0, SEEK_SET);
    fread(d.data(), 1, d.size(), f);
    fclose(f);
    const uintptr_t B = (uintptr_t)d.data();

    // 1. dump
    CHECK(memcmp(d.data() + kAtMiss, kAtMissOrig, kAtMissLen) == 0, "kAtMiss bytes differ from the dump");
    int32_t disp;
    memcpy(&disp, d.data() + kAtMiss + 3, 4);
    const uintptr_t msgRva = kAtMiss + 7 + disp;
    CHECK(strcmp((const char*)d.data() + msgRva, "invalid unordered_map<K, T> key") == 0, "msg string at %zX", (size_t)msgRva);
    CHECK(CallTarget(B + kAtMiss + 7) - B == 0xB5FB84, "thrower %zX", (size_t)(CallTarget(B + kAtMiss + 7) - B));
    for (int i = 0; i < kNumAtCalls; ++i) {
        CHECK(d[kAtCalls[i]] == 0xE8 && CallTarget(B + kAtCalls[i]) - B == kAtFn, "call %X is not call at()", kAtCalls[i]);
    }
    // every jump into the miss path comes from inside at() or its cold block (je 4316F9): count `je/jmp rel32` + `je rel8`
    int into = 0;
    for (size_t a = 0x1000; a + 6 < d.size() && a < 0x2C00000; ++a) {
        if (d[a] == 0x0F && d[a + 1] == 0x84) {
            int32_t r;
            memcpy(&r, &d[a + 2], 4);
            if (a + 6 + (intptr_t)r == kAtMiss) ++into;
        } else if (d[a] == 0x74 && a + 2 + (int8_t)d[a + 1] == kAtMiss) {
            ++into;
        }
    }
    printf("jumps into the at() miss path: %d\n", into);
    CHECK(into >= 2, "expected the two je of at() into its miss path");

    // 2. run the cave
    uint8_t* page = (uint8_t*)VirtualAlloc(nullptr, 0x2000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    memset(page, 0xCC, 0x2000);
    memset(page + kMapScratch, 0, kMapScratchLen);
    memset(page + kMapCounter, 0, 4);
    uint8_t* stubs = page + 0x1000;
    // atStub (0x1000): sub rsp,28h ; mov rax, cave ; jmp rax   (at()'s frame, then its miss path)
    uint8_t* at = stubs;
    size_t n = 0;
    at[n++] = 0x48, at[n++] = 0x83, at[n++] = 0xEC, at[n++] = 0x28;
    at[n++] = 0x48, at[n++] = 0xB8;
    const uintptr_t cave = (uintptr_t)page + kMapCave;
    memcpy(at + n, &cave, 8), n += 8;
    at[n++] = 0xFF, at[n++] = 0xE0;
    // callers (0x1100 / 0x1200): push rbx,rbp,rsi,rdi,r12-r15 sentinel check is done by MSVC around the call; plain: call at ; ret
    auto caller = [&](uint8_t* c) {
        size_t k = 0;
        c[k++] = 0x48, c[k++] = 0x83, c[k++] = 0xEC, c[k++] = 0x28;  // sub rsp,28h (align + shadow)
        c[k++] = 0xE8;
        const int32_t r = (int32_t)((intptr_t)at - (intptr_t)(c + k + 4));
        memcpy(c + k, &r, 4), k += 4;
        const size_t retOff = k;
        c[k++] = 0x48, c[k++] = 0x83, c[k++] = 0xC4, c[k++] = 0x28;  // add rsp,28h
        c[k++] = 0xC3;
        return (uintptr_t)c + retOff;
    };
    const uintptr_t retA = caller(stubs + 0x100), retB = caller(stubs + 0x200);
    uintptr_t rets[kNumAtCalls];
    for (int i = 0; i < kNumAtCalls; ++i) rets[i] = 0x1111 + i;  // unrelated return addresses
    rets[kNumAtCalls - 1] = retA;                                // A is the last entry (the loop walks the whole table)
    const uintptr_t msg = 0xABCDEF12345678ull;
    const size_t len = BuildMapMiss(page, rets, kNumAtCalls, msg, (uintptr_t)&Thrower);
    printf("map-miss cave: %zu bytes\n", len);
    CHECK(len > 0 && len < kMapRets - kMapCave, "cave size %zu", len);
    FlushInstructionCache(GetCurrentProcess(), page, 0x2000);

    using Fn = uint8_t* (*)();
    const Fn callA = (Fn)(stubs + 0x100), callB = (Fn)(stubs + 0x200);
    memset(page + kMapScratch, 0x5A, kMapScratchLen);  // dirty: a previous caller wrote into the scratch value
    uint8_t* v = callA();
    CHECK(v == page + kMapScratch, "A: value %p, want scratch %p", (void*)v, (void*)(page + kMapScratch));
    bool zero = true;
    for (size_t i = 0; i < kMapScratchLen; ++i) zero = zero && page[kMapScratch + i] == 0;
    CHECK(zero, "A: scratch not zeroed");
    CHECK(*(volatile long*)(page + kMapCounter) == 1, "A: counter %ld", *(volatile long*)(page + kMapCounter));
    for (int i = 0; i < 1000; ++i) callA();
    CHECK(*(volatile long*)(page + kMapCounter) == 1001, "A x1000: counter %ld", *(volatile long*)(page + kMapCounter));

    RtlCaptureContext(&g_ctx);
    if (g_phase == 0) {
        g_phase = 1;
        callB();
        g_phase = 3;  // returned: wrong (the throw path never returns)
    }
    CHECK(g_phase == 2, "B: phase %d, want 2 (original thrower reached)", g_phase);
    CHECK(g_msgSeen == msg, "B: thrower rcx %zX, want msg", (size_t)g_msgSeen);
    CHECK(*(volatile long*)(page + kMapCounter) == 1001, "B counted as a hit");
    (void)retB;

    // 3. second map (#115): dump checks of at() 431E6C, its miss path and its 7 callers
    CHECK(memcmp(d.data() + kAt2Miss, kAt2MissOrig, kAtMissLen) == 0, "kAt2Miss bytes differ from the dump");
    static const uint8_t kHead[4] = {0x48, 0x83, 0xEC, 0x28};
    CHECK(memcmp(d.data() + kAt2Fn, kHead, 4) == 0, "at2 does not open with sub rsp,28h");
    memcpy(&disp, d.data() + kAt2Miss + 3, 4);
    const uintptr_t msg2Rva = kAt2Miss + 7 + disp;
    CHECK(strcmp((const char*)d.data() + msg2Rva, "invalid unordered_map<K, T> key") == 0, "msg2 string at %zX", (size_t)msg2Rva);
    CHECK(CallTarget(B + kAt2Miss + 7) - B == 0xB5FB84, "thrower2 %zX", (size_t)(CallTarget(B + kAt2Miss + 7) - B));
    for (int i = 0; i < kNumAt2Calls; ++i) {
        CHECK(d[kAt2Calls[i]] == 0xE8 && CallTarget(B + kAt2Calls[i]) - B == kAt2Fn, "call %X is not call at2()", kAt2Calls[i]);
    }
    // every call of at2 in the image is in the table (a caller left out would still get the original throw: safe, but log it)
    int calls2 = 0;
    for (size_t a = 0x1000; a + 5 < d.size() && a < 0x2C00000; ++a) {
        if (d[a] != 0xE8) continue;
        int32_t r;
        memcpy(&r, &d[a + 1], 4);
        if (a + 5 + (intptr_t)r == kAt2Fn) ++calls2;
    }
    printf("calls of at2 in the image: %d (table %d)\n", calls2, kNumAt2Calls);
    CHECK(calls2 == kNumAt2Calls, "at2 has %d calls, table %d", calls2, kNumAt2Calls);
    int into2 = 0;
    for (size_t a = 0x1000; a + 6 < d.size() && a < 0x2C00000; ++a) {
        if (d[a] == 0x0F && d[a + 1] == 0x84) {
            int32_t r;
            memcpy(&r, &d[a + 2], 4);
            if (a + 6 + (intptr_t)r == kAt2Miss) ++into2;
        } else if (d[a] == 0x74 && a + 2 + (int8_t)d[a + 1] == kAt2Miss) {
            ++into2;
        }
    }
    printf("jumps into the at2 miss path: %d\n", into2);
    CHECK(into2 >= 3, "expected the two je of at2 + its cold chunk into its miss path");

    // 4. run the second cave in its own block; the first block's scratch / counter stay untouched
    memset(page + kMap2.scratch, 0, kMapScratchLen);
    memset(page + kMap2.counter, 0, 4);
    const uintptr_t cave2 = (uintptr_t)page + kMap2.cave;
    memcpy(at + 6, &cave2, 8);  // atStub now jumps to the second cave
    uintptr_t rets2[kNumAt2Calls];
    for (int i = 0; i < kNumAt2Calls; ++i) rets2[i] = 0x2222 + i;
    rets2[0] = retA;  // A first this time
    const size_t len2 = BuildMapMiss(page, rets2, kNumAt2Calls, msg, (uintptr_t)&Thrower, kMap2);
    printf("map-miss cave 2: %zu bytes\n", len2);
    CHECK(len2 > 0 && len2 < kMap2.rets - kMap2.cave, "cave2 size %zu", len2);
    FlushInstructionCache(GetCurrentProcess(), page, 0x2000);
    memset(page + kMapScratch, 0x11, kMapScratchLen);   // first block: must not be touched
    memset(page + kMap2.scratch, 0x5A, kMapScratchLen);  // second block: dirty, must be re-zeroed
    v = callA();
    CHECK(v == page + kMap2.scratch, "A2: value %p, want scratch2 %p", (void*)v, (void*)(page + kMap2.scratch));
    zero = true;
    for (size_t i = 0; i < kMapScratchLen; ++i) zero = zero && page[kMap2.scratch + i] == 0;
    CHECK(zero, "A2: scratch2 not zeroed");
    bool firstKept = true;
    for (size_t i = 0; i < kMapScratchLen; ++i) firstKept = firstKept && page[kMapScratch + i] == 0x11;
    CHECK(firstKept, "A2: first block scratch touched");
    CHECK(*(volatile long*)(page + kMap2.counter) == 1, "A2: counter2 %ld", *(volatile long*)(page + kMap2.counter));
    CHECK(*(volatile long*)(page + kMapCounter) == 1001, "A2: first counter changed");
    g_phase = 0;
    g_msgSeen = 0;
    RtlCaptureContext(&g_ctx);
    if (g_phase == 0) {
        g_phase = 1;
        callB();
        g_phase = 3;
    }
    CHECK(g_phase == 2, "B2: phase %d, want 2 (original thrower reached)", g_phase);
    CHECK(g_msgSeen == msg, "B2: thrower rcx %zX, want msg", (size_t)g_msgSeen);
    CHECK(*(volatile long*)(page + kMap2.counter) == 1, "B2 counted as a hit");

    if (g_fail) {
        printf("FAILED %d\n", g_fail);
        return 1;
    }
    printf("ALL OK\n");
    return 0;
}
