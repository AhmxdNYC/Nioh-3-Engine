// loadmem.cpp - LM load-memory sweeper (see loadmem.h, notes\load-memory.md).
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "gamerva.h"
#include <psapi.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <unordered_map>

#include "loadmem.h"

namespace loadmem {
namespace {

void (*g_log)(const char*, ...) = nullptr;
#define LLOG(...)                      \
    do {                               \
        if (g_log) g_log(__VA_ARGS__); \
    } while (0)

// v2.02 (notes\load-memory.md section 1; lock layout checked against the dump: HeapFree 130D0C does lea rcx,[heap+60h]
// -> call B614A8 (returns 0 on success), and ends in jmp B61524)
constexpr uintptr_t kRvaMtxLock = 0xB614A8, kRvaMtxUnlock = 0xB61524;
constexpr uintptr_t kRvaArenaPtr = 0x4B50348;      // holds the address of the static arena object (IMG+475A440)
constexpr uintptr_t kRvaVtHeap = 0x408F940, kRvaVtHeap2 = 0x408FB78;
constexpr size_t kArenaScan = 4u << 20;
constexpr uintptr_t kOffHdr = 0x18, kOffBase = 0x20, kOffFirst = 0x28, kOffEnd = 0x30, kOffBins = 0x38, kOffBinCount = 0x44,
                    kOffMutex = 0x60;
constexpr uint64_t kChunkHdr = 0x20;   // prev, size (neg = allocated), bin prev, bin next
constexpr uint64_t kPage = 0x1000;
constexpr uint64_t kMaxNodesPerBin = 1u << 20;

bool g_on = false;
uintptr_t g_base = 0;
std::atomic<int> g_state{0};   // 0 waiting for the arena, 1 running, 2 self-test failed (off), 3 off (reset failing)
std::atomic<int> g_selfPass{-1};  // -1 not run / skipped, 0 failed, 1 passed
bool g_force = false;
testing::Cfg g_cfg;
DWORD g_periodMs = 1000;
testing::Stats g_st;               // written by the sweeper thread only, read racily for the log line
std::atomic<uint64_t> g_sweeps{0}, g_heapCount{0};
LARGE_INTEGER g_qpf{};
testing::Ops g_ops;

inline uint64_t NowUs() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (uint64_t)(t.QuadPart * 1000000ull / (uint64_t)g_qpf.QuadPart);
}

// ---- guarded reads (no C++ objects in these frames) --------------------------------------------------------------------
bool SafeRead64(uintptr_t a, uint64_t* out) {
    __try {
        *out = *reinterpret_cast<volatile uint64_t*>(a);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
bool SafeRead32(uintptr_t a, uint32_t* out) {
    __try {
        *out = *reinterpret_cast<volatile uint32_t*>(a);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
bool SafeCopy(void* dst, uintptr_t src, size_t n) {
    __try {
        memcpy(dst, reinterpret_cast<const void*>(src), n);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

int DefLock(void* mtx) {
    using Fn = int (*)(void*);
    return reinterpret_cast<Fn>(g_base + Grva(kRvaMtxLock))(mtx);
}
int DefUnlock(void* mtx) {
    using Fn = int (*)(void*);
    return reinterpret_cast<Fn>(g_base + Grva(kRvaMtxUnlock))(mtx);
}
bool DefReset(uintptr_t a, size_t n) { return VirtualAlloc(reinterpret_cast<void*>(a), n, MEM_RESET, PAGE_READWRITE) != nullptr; }

struct Key {
    uint64_t c; int64_t s;
    bool operator==(const Key& o) const { return c == o.c && s == o.s; }
};
struct KeyHash {
    size_t operator()(const Key& k) const { return (size_t)(((k.c >> 4) * 0x9E3779B97F4A7C15ull) ^ ((uint64_t)k.s * 0xC2B2AE3D27D4EB4Full)); }
};
struct Rec {
    uint32_t last = 0;      // sweep number this (chunk, size) was last seen free
    uint64_t done = 0;      // bytes of the interior already reset (resumes after a budget cut)
};

inline bool Aligned16(uint64_t v) { return (v & 15) == 0; }

}  // namespace

namespace testing {
struct HeapState {
    uintptr_t h = 0;
    uint32_t sweep = 0;
    std::unordered_map<Key, Rec, KeyHash> seen;
};

void SetOps(const Ops& o) {
    g_ops = o;
    if (!g_ops.lock) g_ops.lock = &DefLock;
    if (!g_ops.unlock) g_ops.unlock = &DefUnlock;
    if (!g_ops.reset) g_ops.reset = &DefReset;
    if (!g_qpf.QuadPart) QueryPerformanceFrequency(&g_qpf);
}
HeapState* NewState(uintptr_t heap) {
    auto* s = new HeapState();
    s->h = heap;
    return s;
}
void DeleteState(HeapState* s) { delete s; }

bool ValidHeap(uintptr_t h) {
    uint64_t hdr, base, first, end, bins, t;
    uint32_t nb;
    if (!SafeRead64(h + kOffHdr, &hdr) || hdr != kChunkHdr) return false;
    if (!SafeRead64(h + kOffBase, &base) || !SafeRead64(h + kOffFirst, &first) || !SafeRead64(h + kOffEnd, &end) ||
        !SafeRead64(h + kOffBins, &bins) || !SafeRead32(h + kOffBinCount, &nb))
        return false;
    if (!base || !Aligned16(base) || first < base || end <= first || !Aligned16(first) || !Aligned16(end)) return false;
    if (bins < base || bins >= first || nb < 1 || nb > 64) return false;
    return SafeRead64(first, &t) && SafeRead64(first + 8, &t) && SafeRead64(h + kOffMutex, &t);
}

void Sweep(HeapState* hs, const Cfg& cfg, Stats& st) {
    const uintptr_t h = hs->h;
    uint64_t base, first, end, bins;
    uint32_t nb;
    if (!SafeRead64(h + kOffBase, &base) || !SafeRead64(h + kOffFirst, &first) || !SafeRead64(h + kOffEnd, &end) ||
        !SafeRead64(h + kOffBins, &bins) || !SafeRead32(h + kOffBinCount, &nb) || nb < 1 || nb > 64)
        return;
    const uint32_t sw = ++hs->sweep;
    void* mtx = reinterpret_cast<void*>(h + kOffMutex);
    for (uint32_t b = 0; b < nb; ++b) {
        const uint64_t w0 = NowUs();
        if (g_ops.lock(mtx) != 0) continue;
        const uint64_t t0 = NowUs();
        if (t0 - w0 > 1000) ++st.lockWaits;
        if (t0 - w0 > st.maxLockWaitUs) st.maxLockWaitUs = t0 - w0;
        uint64_t c = 0;
        // everything below is read under the heap lock; one lock per bin, never across bins
        if (SafeRead64(h + kOffBins, &bins) && bins >= base && SafeRead64(bins + 8ull * b, &c)) {
            uint64_t nodes = 0;
            while (c && nodes++ < kMaxNodesPerBin) {
                if (c < first || c + kChunkHdr > end || !Aligned16(c)) { ++st.badNodes; break; }
                uint64_t S64, next;
                if (!SafeRead64(c + 8, &S64) || !SafeRead64(c + 0x18, &next)) { ++st.badNodes; break; }
                const int64_t S = (int64_t)S64;
                if (S <= 0 || !Aligned16((uint64_t)S) || c + kChunkHdr + (uint64_t)S > end) { ++st.badNodes; break; }
                ++st.nodes;
                if ((size_t)S >= cfg.minBytes) {
                    const Key k{c, S};
                    auto it = hs->seen.find(k);
                    if (it == hs->seen.end()) {
                        hs->seen.emplace(k, Rec{sw, 0});
                    } else if (it->second.last == sw) {
                        // already visited this sweep: nothing to do
                    } else if (it->second.last + 1 == sw) {
                        // free and unchanged since the previous sweep: discard the interior pages (never the header)
                        Rec& r = it->second;
                        r.last = sw;
                        const uint64_t a = (c + kChunkHdr + kPage - 1) & ~(kPage - 1);
                        const uint64_t e = (c + kChunkHdr + (uint64_t)S) & ~(kPage - 1);
                        if (e > a) {
                            const uint64_t total = e - a;
                            while (r.done < total) {
                                const uint64_t n = total - r.done < cfg.sliceBytes ? total - r.done : cfg.sliceBytes;
                                if (g_ops.reset((uintptr_t)(a + r.done), (size_t)n)) {
                                    r.done += n;
                                    st.bytes += n;
                                    if (r.done >= total) ++st.chunks;
                                } else {
                                    ++st.resetFails;
                                    r.done = total;  // do not retry this chunk
                                    break;
                                }
                                if (NowUs() - t0 > cfg.budgetUs) break;
                            }
                        }
                    } else {
                        it->second = Rec{sw, 0};  // seen again after a gap: treat as new
                    }
                }
                if (NowUs() - t0 > cfg.budgetUs) { ++st.capped; break; }
                c = next;
            }
        }
        g_ops.unlock(mtx);
        const uint64_t hold = NowUs() - t0;
        if (hold > st.maxHoldUs) st.maxHoldUs = hold;
    }
    // forget everything not seen free in this sweep (allocated, merged, resized): it must earn 2 sweeps again
    for (auto it = hs->seen.begin(); it != hs->seen.end();) {
        if (it->second.last != sw) it = hs->seen.erase(it);
        else ++it;
    }
}

}  // namespace testing

namespace {

// ---- discovery ----------------------------------------------------------------------------------------------------------
size_t FindHeaps(uintptr_t arena, uintptr_t* out, size_t maxOut) {
    size_t n = 0;
    static uint8_t buf[4096];
    const uint64_t v1 = g_base + Grva(kRvaVtHeap), v2 = g_base + Grva(kRvaVtHeap2);
    for (size_t off = 0; off < kArenaScan && n < maxOut; off += sizeof buf) {
        if (!SafeCopy(buf, arena + off, sizeof buf)) continue;
        for (size_t i = 0; i + 8 <= sizeof buf && n < maxOut; i += 8) {
            uint64_t v;
            memcpy(&v, buf + i, 8);
            if ((v == v1 || v == v2) && testing::ValidHeap(arena + off + i)) out[n++] = arena + off + i;
        }
    }
    return n;
}

// ---- self-test: reset must shrink the working set -----------------------------------------------------------------------
SIZE_T WorkingSet() {
    PROCESS_MEMORY_COUNTERS pmc{};
    pmc.cb = sizeof pmc;
    return GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof pmc) ? pmc.WorkingSetSize : 0;
}
bool SelfTest(char* msg, size_t msgN) {
    constexpr SIZE_T N = 64u << 20;
    uint8_t* p = (uint8_t*)VirtualAlloc(nullptr, N, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!p) { snprintf(msg, msgN, "scratch alloc failed"); return false; }
    for (SIZE_T i = 0; i < N; i += kPage) p[i] = 1;
    const SIZE_T w0 = WorkingSet();
    const bool ok = VirtualAlloc(p, N, MEM_RESET, PAGE_READWRITE) != nullptr;
    SIZE_T w1 = w0;
    if (ok) {
        for (int i = 0; i < 20; ++i) {   // up to 1 s for a lazy reset to show
            Sleep(50);
            w1 = WorkingSet();
            if (w0 > w1 && w0 - w1 >= N / 2) break;
        }
    }
    VirtualFree(p, 0, MEM_RELEASE);
    const bool fell = ok && w0 > w1 && w0 - w1 >= N / 2;
    snprintf(msg, msgN, "working set %zu -> %zu MB after MEM_RESET of 64 MB scratch (%s)", (size_t)(w0 >> 20), (size_t)(w1 >> 20),
             !ok ? "MEM_RESET failed" : fell ? "fell" : "did NOT fall");
    return fell;
}

bool CodeLooksRight() {
    static const uint8_t lockHead[] = {0x33, 0xD2, 0xE9};                                   // xor edx,edx ; jmp
    static const uint8_t unlockHead[] = {0x48, 0x83, 0xEC, 0x28, 0x83, 0x69, 0x4C, 0x01};   // sub rsp,28h ; sub dword [rcx+4Ch],1
    return !memcmp(reinterpret_cast<const void*>(g_base + Grva(kRvaMtxLock)), lockHead, sizeof lockHead) &&
           !memcmp(reinterpret_cast<const void*>(g_base + Grva(kRvaMtxUnlock)), unlockHead, sizeof unlockHead);
}

DWORD WINAPI SweeperMain(LPVOID) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    if (!g_force) {
        char msg[200];
        const bool pass = SelfTest(msg, sizeof msg);
        g_selfPass = pass ? 1 : 0;
        LLOG("LM: self-test %s: %s", pass ? "PASSED" : "FAILED - LM stays off (LM_Force = 1 skips the test)", msg);
        if (!pass) { g_state = 2; return 0; }
    } else {
        LLOG("LM: self-test skipped (LM_Force = 1)");
    }

    uintptr_t heaps[96];
    testing::HeapState* states[96] = {};
    size_t nh = 0;
    uint64_t lastScan = 0;
    uintptr_t arena = 0;
    for (;;) {
        Sleep(g_periodMs);
        if (g_state == 3) return 0;
        if (!arena) {
            uint64_t v = 0;
            if (SafeRead64(g_base + Grva(kRvaArenaPtr), &v) && v >= g_base && v < g_base + 0x8000000) arena = (uintptr_t)v;
            else continue;
        }
        // look for heaps until found, then once a minute for late ones
        const uint64_t now = GetTickCount64();
        if (nh == 0 || now - lastScan > 60000) {
            lastScan = now;
            uintptr_t found[96];
            const size_t nf = FindHeaps(arena, found, 96);
            for (size_t i = 0; i < nf; ++i) {
                bool have = false;
                for (size_t j = 0; j < nh; ++j) have |= heaps[j] == found[i];
                if (!have && nh < 96) { heaps[nh] = found[i]; states[nh] = testing::NewState(found[i]); ++nh; }
            }
            g_heapCount = nh;
            if (nh && g_state == 0) {
                g_state = 1;
                LLOG("LM: %zu engine heaps found, sweeping every %lu ms (min chunk %zu KB)", nh, g_periodMs, g_cfg.minBytes >> 10);
            }
        }
        if (!nh) continue;
        for (size_t i = 0; i < nh; ++i) testing::Sweep(states[i], g_cfg, g_st);
        ++g_sweeps;
        if (g_st.resetFails >= 8) { LLOG("LM: MEM_RESET keeps failing - sweeper stopped"); g_state = 3; return 0; }
    }
}

}  // namespace

bool Start(const StartInfo& info) {
    g_log = info.log;
    if (!info.iniPath) return false;
    const int lm = (int)GetPrivateProfileIntW(L"Memory", L"LM", 0, info.iniPath);
    if (!lm) return false;
    g_base = info.base;
    QueryPerformanceFrequency(&g_qpf);
    if (!CodeLooksRight()) { LLOG("LM: _Mtx_lock/_Mtx_unlock bytes differ from v2.02 - not started"); return false; }
    g_force = GetPrivateProfileIntW(L"Memory", L"LM_Force", 0, info.iniPath) != 0;
    g_periodMs = (DWORD)GetPrivateProfileIntW(L"Memory", L"LM_PeriodMs", 1000, info.iniPath);
    if (g_periodMs < 250) g_periodMs = 250;
    int kb = (int)GetPrivateProfileIntW(L"Memory", L"LM_MinKB", 256, info.iniPath);
    if (kb < 64) kb = 64;
    g_cfg.minBytes = (size_t)kb << 10;
    const int bud = (int)GetPrivateProfileIntW(L"Memory", L"LM_BudgetUs", 1000, info.iniPath);
    g_cfg.budgetUs = (uint32_t)(bud < 100 ? 100 : bud);
    const int sl = (int)GetPrivateProfileIntW(L"Memory", L"LM_SliceMB", 128, info.iniPath);
    g_cfg.sliceBytes = (size_t)(sl < 4 ? 4 : sl) << 20;
    testing::SetOps(testing::Ops{});
    HANDLE t = CreateThread(nullptr, 0, &SweeperMain, nullptr, 0, nullptr);
    if (!t) { LLOG("LM: CreateThread failed"); return false; }
    CloseHandle(t);
    g_on = true;
    LLOG("LM: on (period %lu ms, min chunk %d KB, budget %d us/lock, slice %d MB, force %d)", g_periodMs, kb, (int)g_cfg.budgetUs, sl, g_force ? 1 : 0);
    return true;
}

bool Enabled() { return g_on; }

void LogLine() {
    if (!g_on || !g_log) return;
    static uint64_t pBytes = 0, pChunks = 0, pSweeps = 0;
    const uint64_t b = g_st.bytes, c = g_st.chunks, s = g_sweeps.load();
    const int st = g_state.load();
    const char* sn = st == 0 ? "waiting for arena/heaps" : st == 1 ? "running" : st == 2 ? "OFF (self-test failed)" : "OFF (reset failing)";
    const int sp = g_selfPass.load();
    g_log("stats lm: %s, heaps %llu, sweeps %llu (+%llu), reset %llu chunks / %llu MB (+%llu / +%llu MB), lock waits >1ms %llu (max wait %llu us), max hold %llu us, budget cuts %llu, bad nodes %llu, reset fails %llu, self-test %s",
          sn, (unsigned long long)g_heapCount.load(), (unsigned long long)s, (unsigned long long)(s - pSweeps), (unsigned long long)c,
          (unsigned long long)(b >> 20), (unsigned long long)(c - pChunks), (unsigned long long)((b - pBytes) >> 20),
          (unsigned long long)g_st.lockWaits, (unsigned long long)g_st.maxLockWaitUs, (unsigned long long)g_st.maxHoldUs,
          (unsigned long long)g_st.capped, (unsigned long long)g_st.badNodes, (unsigned long long)g_st.resetFails,
          sp < 0 ? "skipped" : sp ? "passed" : "FAILED");
    pBytes = b; pChunks = c; pSweeps = s;
}

}  // namespace loadmem
