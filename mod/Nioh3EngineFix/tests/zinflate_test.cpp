// zinflate_test.cpp - offline test of [Patches] Inflate (src/zinflate.cpp, #108787; #included to reach its internals).
// Not a game test. Input: a fixture written by tests/run_zinflate_test.py (Python zlib, 16 KiB chunks, windowBits 15 = the
// game's inflateInit_ format): records {u32 kind, u32 rawLen, u32 cLen, raw[rawLen], comp[cLen]}; kind 0 = valid zlib
// stream of raw, 1 = invalid for zlib (corrupt / raw deflate / gzip / truncated / wrong adler: Python zlib refuses it).
// Checks:
//   1. Decode: every valid chunk -> 1, *outLen == rawLen, identical bytes; every invalid one -> 0 (game fallback).
//   2. cap smaller than the output -> 0; overlap of dst and src (src inside dst) -> same bytes via scratch.
//   3. the replacements end to end against fake game functions placed at base+420654 / base+A09A40 / base+A347D8 in an
//      RWX block: valid -> no game call; invalid -> exactly one game call with the original arguments, game result
//      returned; A09A40: 8-byte sum header, sum mismatch -> game; verify mode: 2000 chunks compared, then libdeflate only.
//   4. 4 threads decoding at once (one decompressor per thread).
// Known leniency (counted, not a failure): libdeflate maps an incomplete 1-codeword code to a complete one and decodes
// litlen 286/287 as length 258, where zlib refuses. A stream like that never comes from an encoder (the game's zlib 1.1.4
// refuses it too, so no chunk the game reads is affected); a corrupt one is only accepted when its adler32 still matches.
// Such a chunk passes only if libdeflate's output equals the original raw bytes (full length).
//   zinflate_test.exe FIXTURE  ->  "ZINFLATE TEST: ALL PASSED"
#define _CRT_SECURE_NO_WARNINGS
#include "../src/zinflate.cpp"

#include <cstdarg>
#include <cstdlib>
#include <vector>

using namespace zinfl;

static int g_fail = 0;
#define CHECK(c, ...)                     \
    do {                                  \
        if (!(c)) {                       \
            printf("FAIL: " __VA_ARGS__); \
            printf("\n");                 \
            ++g_fail;                     \
        }                                 \
    } while (0)

static int g_logLines = 0;
static void Log(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    printf("  log: ");
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
    ++g_logLines;
}

struct Rec {
    uint32_t kind;
    std::vector<uint8_t> raw, comp;
};

// ---- fake game: "original" wrappers = a plain libdeflate decode on a private decompressor (the zlib reference was
// Python's; here only the call contract matters), counting calls and recording arguments ----
static libdeflate_decompressor* g_ref = nullptr;
static std::atomic<int> g_gameCalls{0};
static const void* g_gameSrc = nullptr;
static uint32_t g_gameCsize = 0;
static uint64_t FakeGameA(void* dst, uint32_t cap, uint64_t* outLen, const void* src, uint32_t csize) {
    g_gameCalls.fetch_add(1);
    g_gameSrc = src;
    g_gameCsize = csize;
    uint8_t buf[0x4000];
    size_t n = 0;
    if (libdeflate_zlib_decompress(g_ref, src, csize, buf, cap < 0x4000 ? cap : 0x4000, &n) != LIBDEFLATE_SUCCESS)
        return 0xABCD00;  // al = 0 with junk above it, like the game
    *outLen = n;
    memcpy(dst, buf, n);
    return 0xABCD01;
}
static uint64_t FakeSum(const void* p, uint32_t n) {  // stand-in for A347D8
    uint64_t h = 0x1234;
    const uint8_t* b = (const uint8_t*)p;
    for (uint32_t i = 0; i < n; ++i) h = h * 31 + b[i];
    return h;
}
static uint64_t FakeGameC(void* dst, uint32_t cap, uint64_t* outLen, const void* src, uint32_t csize) {
    g_gameCalls.fetch_add(1);
    g_gameSrc = src;
    g_gameCsize = csize;
    uint64_t sum;
    memcpy(&sum, src, 8);
    if (FakeSum((const uint8_t*)src + 8, csize - 8) != sum) return 0;
    uint8_t buf[0x4000];
    size_t n = 0;
    if (libdeflate_zlib_decompress(g_ref, (const uint8_t*)src + 8, csize - 8, buf, cap < 0x4000 ? cap : 0x4000, &n) !=
        LIBDEFLATE_SUCCESS)
        return 0;
    *outLen = n;
    memcpy(dst, buf, n);
    return 1;
}
static void PutJmp(uint8_t* at, const void* target) {  // mov rax, imm64; jmp rax
    at[0] = 0x48;
    at[1] = 0xB8;
    memcpy(at + 2, &target, 8);
    at[10] = 0xFF;
    at[11] = 0xE0;
}

static std::vector<Rec> g_recs;
// invalid chunk accepted by libdeflate: tolerated only with the original bytes, full length (see the leniency note above)
static bool LenientSame(const Rec& r, const uint8_t* out, uint64_t n) {
    return n == r.raw.size() && memcmp(out, r.raw.data(), r.raw.size()) == 0;
}
static std::atomic<int> g_lenient{0};
static DWORD WINAPI Worker(void* arg) {
    const int id = (int)(intptr_t)arg;
    std::vector<uint8_t> out(0x4000);
    int bad = 0;
    for (int pass = 0; pass < 3; ++pass)
        for (size_t i = id; i < g_recs.size(); i += 4) {
            const Rec& r = g_recs[i];
            uint64_t n = ~0ull;
            const int ok = Decode(out.data(), 0x4000, &n, r.comp.data(), (uint32_t)r.comp.size());
            if (r.kind == 0 ? !(ok && n == r.raw.size() && memcmp(out.data(), r.raw.data(), n) == 0)
                            : ok != 0 && !LenientSame(r, out.data(), n))
                ++bad;
        }
    return (DWORD)bad;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("usage: zinflate_test FIXTURE\n");
        return 2;
    }
    FILE* f = fopen(argv[1], "rb");
    if (!f) {
        printf("cannot open %s\n", argv[1]);
        return 2;
    }
    for (;;) {
        uint32_t h[3];
        if (fread(h, 4, 3, f) != 3) break;
        Rec r;
        r.kind = h[0];
        r.raw.resize(h[1]);
        r.comp.resize(h[2]);
        if ((h[1] && fread(r.raw.data(), 1, h[1], f) != h[1]) || (h[2] && fread(r.comp.data(), 1, h[2], f) != h[2])) break;
        g_recs.push_back(std::move(r));
    }
    fclose(f);
    int nValid = 0, nInvalid = 0;
    uint64_t rawBytes = 0, compBytes = 0;
    for (const Rec& r : g_recs) {
        (r.kind == 0 ? nValid : nInvalid)++;
        if (r.kind == 0) rawBytes += r.raw.size(), compBytes += r.comp.size();
    }
    printf("fixture: %zu chunks (%d valid, %d invalid), %.1f MB -> %.1f MB\n", g_recs.size(), nValid, nInvalid,
           compBytes / 1048576.0, rawBytes / 1048576.0);
    CHECK(nValid >= 1000 && nInvalid >= 50, "fixture too small");

    // 1. Decode on every chunk
    std::vector<uint8_t> out(0x4000 + 64);
    int okSame = 0, okRefused = 0;
    LARGE_INTEGER q0, q1, fq;
    QueryPerformanceFrequency(&fq);
    QueryPerformanceCounter(&q0);
    for (size_t i = 0; i < g_recs.size(); ++i) {
        const Rec& r = g_recs[i];
        memset(out.data(), 0xCC, out.size());
        uint64_t n = ~0ull;
        const int ok = Decode(out.data(), 0x4000, &n, r.comp.data(), (uint32_t)r.comp.size());
        if (r.kind == 0) {
            const bool same = ok && n == r.raw.size() && memcmp(out.data(), r.raw.data(), r.raw.size()) == 0 && out[n] == 0xCC;
            okSame += same;
            CHECK(same, "valid chunk %zu (raw %zu, comp %zu): ok %d n %llu", i, r.raw.size(), r.comp.size(), ok, (unsigned long long)n);
        } else {
            okRefused += ok == 0;
            if (ok) g_lenient.fetch_add(1);
            CHECK(ok == 0 ? n == ~0ull : LenientSame(r, out.data(), n), "invalid chunk %zu: ok %d n %llu (accepted with other bytes)",
                  i, ok, (unsigned long long)n);
        }
    }
    QueryPerformanceCounter(&q1);
    printf("1. Decode: %d/%d valid chunks identical, %d/%d invalid refused, %d accepted with the original bytes (leniency) "
           "(%.1f ms)\n", okSame, nValid, okRefused, nInvalid, g_lenient.load(), (q1.QuadPart - q0.QuadPart) * 1000.0 / fq.QuadPart);

    // 2. cap below the output size; overlap
    int capOk = 0, capN = 0, ovOk = 0, ovN = 0;
    for (const Rec& r : g_recs) {
        if (r.kind != 0 || r.raw.size() < 2) continue;
        uint64_t n = ~0ull;
        if (capN < 200) {
            ++capN;
            capOk += Decode(out.data(), (uint32_t)r.raw.size() - 1, &n, r.comp.data(), (uint32_t)r.comp.size()) == 0 && n == ~0ull;
            n = 0;
            const int exact = Decode(out.data(), (uint32_t)r.raw.size(), &n, r.comp.data(), (uint32_t)r.comp.size());
            CHECK(exact == 1 && n == r.raw.size(), "cap == size must succeed");
        }
        if (ovN < 200 && r.comp.size() <= r.raw.size()) {  // src placed at the end of dst (in-place style)
            ++ovN;
            std::vector<uint8_t> buf(0x4000 + r.comp.size());
            uint8_t* src = buf.data() + 0x4000 - r.comp.size() / 2;
            memmove(src, r.comp.data(), r.comp.size());
            const int ok = Decode(buf.data(), 0x4000, &n, src, (uint32_t)r.comp.size());
            ovOk += ok && n == r.raw.size() && memcmp(buf.data(), r.raw.data(), n) == 0;
        }
    }
    CHECK(capOk == capN && capN > 0, "cap < output: %d/%d refused", capOk, capN);
    CHECK(ovOk == ovN && ovN > 0, "overlap: %d/%d identical", ovOk, ovN);
    printf("2. cap < output refused %d/%d, overlapping dst/src identical %d/%d\n", capOk, capN, ovOk, ovN);

    // 3. replacements end to end with fake game functions at their RVAs
    libdeflate_options o;
    memset(&o, 0, sizeof(o));
    o.sizeof_options = sizeof(o);
    o.malloc_func = &HeapMalloc;
    o.free_func = &NoFree;
    g_ref = libdeflate_alloc_decompressor_ex(&o);
    const size_t span = 0xA34800;
    uint8_t* img = (uint8_t*)VirtualAlloc(nullptr, span, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    CHECK(img != nullptr, "VirtualAlloc");
    if (!img) return 1;
    PutJmp(img + 0x420654, (const void*)&FakeGameA);
    PutJmp(img + 0xA09A40, (const void*)&FakeGameC);
    PutJmp(img + 0xA347D8, (const void*)&FakeSum);
    // Start() refuses here (the fake bodies are not the v2.02 bytes): that is the bytes check working
    {
        StartInfo si;
        si.log = &Log;
        si.base = (uintptr_t)img;
        si.knownBuild = true;
        si.iniPath = L"__no_such_ini__";
        CHECK(!Start(si) && !Replacement(0), "Start with Inflate missing from the ini must stay off");
    }
    g_log = &Log;
    g_base = (uintptr_t)img;
    g_mode = 1;
    g_repl[0] = g_repl[2] = true;
    CHECK(Replacement(1) == nullptr, "324D064 must never be replaced");
    const InflFn ra = Replacement(0), rc = Replacement(2);
    CHECK(ra && rc, "replacements");
    int aSame = 0, aGameOnBad = 0, cSame = 0, cGameOnBad = 0, cSumBad = 0, n3 = 0;
    for (const Rec& r : g_recs) {
        ++n3;
        uint64_t n = ~0ull;
        g_gameCalls = 0;
        memset(out.data(), 0xCC, out.size());
        uint64_t res = ra(out.data(), 0x4000, &n, r.comp.data(), (uint32_t)r.comp.size());
        if (r.kind == 0)
            aSame += (res & 0xFF) == 1 && g_gameCalls == 0 && n == r.raw.size() && memcmp(out.data(), r.raw.data(), n) == 0;
        else
            aGameOnBad += (res == 0xABCD00 && g_gameCalls == 1 && g_gameSrc == r.comp.data() && g_gameCsize == r.comp.size()) ||
                          ((res & 0xFF) == 1 && g_gameCalls == 0 && LenientSame(r, out.data(), n));
        // _C: {sum, payload}
        std::vector<uint8_t> cs(8 + r.comp.size());
        const uint64_t sum = FakeSum(r.comp.data(), (uint32_t)r.comp.size());
        memcpy(cs.data(), &sum, 8);
        if (!r.comp.empty()) memcpy(cs.data() + 8, r.comp.data(), r.comp.size());
        g_gameCalls = 0;
        n = ~0ull;
        res = rc(out.data(), 0x4000, &n, cs.data(), (uint32_t)cs.size());
        if (r.kind == 0)
            cSame += (res & 0xFF) == 1 && g_gameCalls == 0 && n == r.raw.size() && memcmp(out.data(), r.raw.data(), n) == 0;
        else
            cGameOnBad += ((res & 0xFF) == 0 && g_gameCalls == 1 && g_gameSrc == cs.data()) ||
                          ((res & 0xFF) == 1 && g_gameCalls == 0 && LenientSame(r, out.data(), n));
        if (r.kind == 0 && n3 % 10 == 0) {  // wrong sum -> the game decides (and refuses)
            cs[0] ^= 1;
            g_gameCalls = 0;
            n = ~0ull;
            res = rc(out.data(), 0x4000, &n, cs.data(), (uint32_t)cs.size());
            cSumBad += (res & 0xFF) == 0 && g_gameCalls == 1 && n == ~0ull;
        }
    }
    CHECK(aSame == nValid, "420654 valid: %d/%d via libdeflate", aSame, nValid);
    CHECK(aGameOnBad == nInvalid, "420654 invalid: %d/%d fell back to the game with the same args", aGameOnBad, nInvalid);
    CHECK(cSame == nValid, "A09A40 valid: %d/%d", cSame, nValid);
    CHECK(cGameOnBad == nInvalid, "A09A40 invalid: %d/%d", cGameOnBad, nInvalid);
    CHECK(cSumBad == nValid / 10, "A09A40 bad sum: %d/%d to the game", cSumBad, nValid / 10);
    printf("3. 420654: %d valid identical w/o game call, %d invalid -> game; A09A40: %d / %d, bad sum -> game %d; fallbacks logged %llu\n",
           aSame, aGameOnBad, cSame, cGameOnBad, cSumBad, (unsigned long long)g_fallbacks.load());

    // verify mode: first 2000 calls run both and report; afterwards libdeflate only
    g_mode = 2;
    int vCalls = 0, after = 0, afterGame = 0;
    for (int k = 0; k < 2600; ++k) {
        const Rec& r = g_recs[(size_t)k * 7 % g_recs.size()];
        uint64_t n = ~0ull;
        g_gameCalls = 0;
        ra(out.data(), 0x4000, &n, r.comp.data(), (uint32_t)r.comp.size());
        if (k < 2000) vCalls += g_gameCalls == 1;
        else if (r.kind == 0) ++after, afterGame += g_gameCalls != 0;
    }
    CHECK(vCalls == 2000, "verify: game ran on %d/2000", vCalls);
    CHECK(g_verifyDone == 2000 && g_vMismatch == 0 && g_vSame + g_vBothFail + g_vLdRefused == 2000, "verify counters same %d both-fail %d mismatch %d",
          g_vSame.load(), g_vBothFail.load(), g_vMismatch.load());
    CHECK(after > 0 && afterGame == 0, "after verify: %d valid chunks, %d game calls", after, afterGame);
    printf("3v. verify: %d identical, %d both failed, %d mismatch; after 2000: %d valid chunks, %d game calls\n", g_vSame.load(),
           g_vBothFail.load(), g_vMismatch.load(), after, afterGame);
    g_mode = 1;

    // 4. threads
    HANDLE th[4];
    for (int i = 0; i < 4; ++i) th[i] = CreateThread(nullptr, 0, &Worker, (void*)(intptr_t)i, 0, nullptr);
    WaitForMultipleObjects(4, th, TRUE, INFINITE);
    DWORD badT = 0;
    for (HANDLE h : th) {
        DWORD c = 0;
        GetExitCodeThread(h, &c);
        badT += c;
        CloseHandle(h);
    }
    CHECK(badT == 0, "threads: %lu wrong results", badT);
    printf("4. 4 threads x 3 passes: %lu wrong\n", badT);

    printf(g_fail ? "ZINFLATE TEST: %d FAILED\n" : "ZINFLATE TEST: ALL PASSED\n", g_fail);
    return g_fail ? 1 : 0;
}
