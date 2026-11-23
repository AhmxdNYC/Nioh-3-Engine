// zinflate.cpp - Inflate ([Patches] Inflate = 0 by default, #108787): libdeflate for the game's 16 KiB zlib chunks, same
// bytes out, the game's own zlib on any libdeflate error. Contract and proof: zinflate.h. Call sites: loadstats.cpp.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "gamerva.h"
#include "zinflate.h"
#include "../third_party/libdeflate/libdeflate.h"

namespace zinfl {
namespace {

constexpr uint32_t kChunk = 0x4000;  // the wrappers' stack output buffer (rbp-60h .. rbp+3FA0h)
constexpr int kVerifyChunks = 2000;

// v2.02 wrapper / helper bodies, FNV-1a 64 over the whole function (no absolute addresses inside: rip-relative and rel32
// only), from re\Nioh3.dump.exe. A different body (other build, another patch inside) -> that wrapper stays the game's.
struct Body { uint32_t rva, len; uint64_t fnv; };
constexpr Body kWrapA = {0x420654, 0x11C, 0xb6b87fffdc9cf8f0ull};  // ZlibInflateChunk16K
constexpr Body kWrapC = {0xA09A40, 0x137, 0x6509dcbbe8ba8732ull};  // ZlibInflateChunk16K_C
constexpr Body kSumC = {0xA347D8, 0x81, 0xd4139a90f6ddeec9ull};    // payload checksum used by _C (leaf)
constexpr uint32_t kWrapRva[3] = {0x420654, 0x324D064, 0xA09A40};

void (*g_log)(const char*, ...) = nullptr;
uintptr_t g_base = 0;
int g_mode = 0;
bool g_repl[3] = {false, false, false};

std::atomic<int> g_verifyTaken{0}, g_verifyDone{0};
std::atomic<int> g_vMismatch{0}, g_vLdRefused{0}, g_vBothFail{0}, g_vSame{0};
std::atomic<uint64_t> g_fallbacks{0};

// One per thread, from the process heap, never freed (no free at DLL unload or thread exit: the game's loader threads
// live as long as the process, and a decompressor is ~11 KB).
struct Tls {
    libdeflate_decompressor* d;
    int lastResult;
    uint8_t scratch[kChunk];
};
thread_local Tls* t_tls = nullptr;

void* HeapMalloc(size_t n) { return HeapAlloc(GetProcessHeap(), 0, n); }
void NoFree(void*) {}

Tls* GetTls() {
    if (Tls* t = t_tls) return t;
    Tls* t = static_cast<Tls*>(HeapAlloc(GetProcessHeap(), 0, sizeof(Tls)));
    if (!t) return nullptr;
    libdeflate_options o;
    memset(&o, 0, sizeof(o));
    o.sizeof_options = sizeof(o);
    o.malloc_func = &HeapMalloc;
    o.free_func = &NoFree;
    t->d = libdeflate_alloc_decompressor_ex(&o);
    t->lastResult = 0;
    if (!t->d) {
        HeapFree(GetProcessHeap(), 0, t);
        return nullptr;
    }
    t_tls = t;
    return t;
}

// 1 ok: out[0..*n) written. 0: libdeflate refused (t->lastResult).
int DecodeTo(Tls* t, void* out, uint32_t lim, size_t* n, const void* src, uint32_t csize) {
    const libdeflate_result r = libdeflate_zlib_decompress(t->d, src, csize, out, lim, n);
    t->lastResult = (int)r;
    return r == LIBDEFLATE_SUCCESS;
}

int DecodeT(Tls* t, void* dst, uint32_t cap, uint64_t* outLen, const void* src, uint32_t csize) {
    const uint32_t lim = cap < kChunk ? cap : kChunk;
    // Always decode into the per-thread scratch, then copy, like the game (stack buffer, memcpy on success): a refused
    // chunk leaves dst and *outLen untouched, so a chunk the game also rejects looks exactly as before (#109526).
    size_t n = 0;
    if (!DecodeTo(t, t->scratch, lim, &n, src, csize)) return 0;
    memcpy(dst, t->scratch, n);
    *outLen = n;
    return 1;
}

uint64_t Fnv(const uint8_t* p, uint32_t n) {
    uint64_t h = 0xcbf29ce484222325ull;
    for (uint32_t i = 0; i < n; ++i) h = (h ^ p[i]) * 0x100000001b3ull;
    return h;
}
bool BodyOk(const Body& b) { return Fnv((const uint8_t*)(g_base + Grva(b.rva)), b.len) == b.fnv; }

InflFn Game(int w) { return reinterpret_cast<InflFn>(g_base + Grva(kWrapRva[w])); }
using SumFn = uint64_t (*)(const void*, uint32_t);

void VerifyEnd() {
    if (g_verifyDone.fetch_add(1) + 1 != kVerifyChunks) return;
    g_log("Inflate verify: %d chunks, %d identical, %d MISMATCH, %d libdeflate refused (game result used), %d both failed; "
          "libdeflate only from now on",
          kVerifyChunks, g_vSame.load(), g_vMismatch.load(), g_vLdRefused.load(), g_vBothFail.load());
}

// W: 0 = 420654, 2 = A09A40. Same arguments in, same result out as the game's wrapper.
template <int W>
uint64_t Repl(void* dst, uint32_t cap, uint64_t* outLen, const void* src, uint32_t csize) {
    const void* pay = src;
    uint32_t plen = csize;
    bool pre = true;  // the wrapper would reach inflate with (pay, plen)
    if (W == 2) {
        // A09A40: u64 sum, payload; return 0 unless A347D8(payload, csize - 8) == sum. csize < 14 cannot hold a zlib
        // stream after the header (and < 8 underflows in the game): leave those to the game.
        if (csize < 8 + 6) {
            pre = false;
        } else {
            uint64_t sum;
            memcpy(&sum, src, 8);
            pay = (const uint8_t*)src + 8;
            plen = csize - 8;
            pre = reinterpret_cast<SumFn>(g_base + Grva(kSumC.rva))(pay, plen) == sum;
        }
    }
    Tls* t = pre ? GetTls() : nullptr;
    if (g_mode == 2 && g_verifyTaken.load(std::memory_order_relaxed) < kVerifyChunks &&
        g_verifyTaken.fetch_add(1) < kVerifyChunks) {
        // libdeflate first into scratch (never overlaps), then the game's into dst: dst gets the game's result
        size_t n = 0;
        const int ld = t ? DecodeTo(t, t->scratch, cap < kChunk ? cap : kChunk, &n, pay, plen) : 0;
        const uint64_t r = Game(W)(dst, cap, outLen, src, csize);
        const bool gameOk = (r & 0xFF) != 0;
        if (gameOk && ld) {
            if (*outLen == n && memcmp(dst, t->scratch, n) == 0) {
                g_vSame.fetch_add(1);
            } else if (g_vMismatch.fetch_add(1) < 5) {
                g_log("Inflate verify: MISMATCH wrapper %X csize %u: game %llu bytes, libdeflate %zu", kWrapRva[W], csize,
                      (unsigned long long)*outLen, n);
            }
        } else if (gameOk) {
            g_vLdRefused.fetch_add(1);
        } else if (ld) {
            if (g_vMismatch.fetch_add(1) < 5)
                g_log("Inflate verify: MISMATCH wrapper %X csize %u: game failed, libdeflate %zu bytes", kWrapRva[W], csize, n);
        } else {
            g_vBothFail.fetch_add(1);
        }
        VerifyEnd();
        return r;
    }
    if (t && DecodeT(t, dst, cap, outLen, pay, plen)) return 1;
    const uint64_t k = g_fallbacks.fetch_add(1);
    if (k < 3 || k == 100 || k == 10000)
        g_log("Inflate: fallback #%llu to the game's zlib (wrapper %X, csize %u, cap %u, %s %d)", (unsigned long long)k + 1,
              kWrapRva[W], csize, cap, pre ? "libdeflate result" : "precheck", t ? t->lastResult : -1);
    return Game(W)(dst, cap, outLen, src, csize);
}

}  // namespace

int Decode(void* dst, uint32_t cap, uint64_t* outLen, const void* src, uint32_t csize) {
    Tls* t = GetTls();
    return t ? DecodeT(t, dst, cap, outLen, src, csize) : 0;
}

bool Start(const StartInfo& info) {
    g_log = info.log;
    g_base = info.base;
    g_mode = (int)GetPrivateProfileIntW(L"Patches", L"Inflate", 0, info.iniPath);
    if (g_mode != 1 && g_mode != 2) g_mode = 0;
    if (!g_mode || !g_log || !g_base) return false;
    if (!info.knownBuild) {
        g_log("Inflate: refused (unknown build; the wrapper contract is proven on v2.02 only)");
        g_mode = 0;
        return false;
    }
    const bool a = BodyOk(kWrapA), c = BodyOk(kWrapC) && BodyOk(kSumC);
    g_repl[0] = a;
    g_repl[2] = c;
    if (!a || !c)
        g_log("Inflate: wrapper bytes differ, left to the game:%s%s", a ? "" : " 420654", c ? "" : " A09A40 (or checksum A347D8)");
    if (!a && !c) {
        g_mode = 0;
        return false;
    }
    return true;
}

int Mode() { return g_mode; }

InflFn Replacement(int w) {
    if (!g_mode || w < 0 || w > 2 || !g_repl[w]) return nullptr;
    return w == 0 ? &Repl<0> : w == 2 ? &Repl<2> : nullptr;
}

void NoteSites(int patched, int wanted, const char* bad) {
    if (!g_mode || !g_log) return;
    if (patched > 0)
        g_log("Inflate: ON (libdeflate) %d sites%s (of %d; 324D064 stays the game's)%s%s", patched,
              g_mode == 2 ? ", verify mode: first 2000 chunks compared" : "", wanted, bad && *bad ? " - not patched:" : "",
              bad ? bad : "");
    else
        g_log("Inflate: OFF - no call site could be patched (of %d)%s%s", wanted, bad && *bad ? ":" : "", bad ? bad : "");
}

}  // namespace zinfl
