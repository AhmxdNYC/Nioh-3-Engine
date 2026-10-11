// fnprobe.cpp - FnProbe ([Debug] FnProbe = 0 by default). See fnprobe.h. Read-only: the hook counts and samples, then the
// original prologue runs with every register as it was at the function entry.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#include <tlhelp32.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>

#include "fnprobe.h"
#include "fnprobe_core.h"

#pragma warning(disable : 4324)  // Slot is padded to its cache-line alignment on purpose

namespace fnprobe {
namespace {

void (*g_log)(const char*, ...) = nullptr;
uintptr_t g_base = 0;
uintptr_t g_imageEnd = 0;

constexpr int kMaxFn = 32;
constexpr int kCallerSlots = 64;
constexpr int kMaxArgRec = 32;

// The 14 functions of remaining-work 10.1 with their verified v2.02 entry bytes (patches.json group FP, tests\test_patterns.py)
struct Known {
    uint32_t rva;
    uint8_t n;
    uint8_t orig[16];
};
const Known kDefault[] = {
    {0x4F9750, 5, {0x48, 0x89, 0x5C, 0x24, 0x18}},
    {0x556118, 5, {0x48, 0x89, 0x5C, 0x24, 0x20}},
    {0x59F498, 7, {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x08}},
    {0x383800, 5, {0x48, 0x89, 0x5C, 0x24, 0x10}},
    {0x471E68, 5, {0x40, 0x55, 0x53, 0x56, 0x57}},
    {0x7D48D4, 6, {0x40, 0x53, 0x48, 0x83, 0xEC, 0x40}},
    {0x167C50, 5, {0x48, 0x89, 0x5C, 0x24, 0x08}},
    {0x57DA04, 5, {0x48, 0x89, 0x5C, 0x24, 0x18}},
    {0x398A54, 7, {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x18}},
    {0x93AEC4, 7, {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x18}},
    {0x23BF1C, 5, {0x40, 0x55, 0x53, 0x56, 0x57}},
    {0x2002D4, 5, {0x48, 0x89, 0x5C, 0x24, 0x20}},
    {0xF68D8, 5, {0x48, 0x8B, 0xC4, 0x55, 0x53}},
    {0x2C5AA6C, 7, {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x20}},
};

struct ArgRec {
    std::atomic<uint32_t> ready{0};
    uint32_t tid;
    uint64_t tsc;
    uint64_t ret;
    uint64_t a[4];
    uint32_t mem[4][4];
    uint8_t memOk;  // bit k: mem[k] valid
};

struct alignas(64) Slot {
    volatile uint64_t count = 0;  // the thunk's lock xadd target (own cache line)
    uint8_t pad0[56];
    uint32_t rva = 0;
    bool on = false;
    std::atomic<uint64_t> firstTsc{0};
    std::atomic<uint64_t> lastTsc{0};
    std::atomic<uint32_t> firstTid{0};
    std::atomic<uint64_t> samples{0};
    std::atomic<uint64_t> overflow{0};
    std::atomic<uint64_t> callerKey[kCallerSlots];
    std::atomic<uint64_t> callerCnt[kCallerSlots];
    ArgRec rec[kMaxArgRec];
    // report thread only
    uint64_t lastCount = 0, lastSecCount = 0, peakSec = 0, recLogged = 0;
};
Slot g_slots[kMaxFn];
int g_nSlots = 0;
uint32_t g_firstN = 8;

uint64_t g_tsc0 = 0;
LARGE_INTEGER g_qpc0{}, g_qpcFreq{};
SYSTEMTIME g_wall0{};
int g_reportSec = 30;

bool LooksPtr(uint64_t v) { return v >= 0x10000 && v < 0x00007FFFFFFF0000ull && (v & 3) == 0; }

bool Readable(uint64_t a, size_t len) {
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery((const void*)a, &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT) return false;
    const DWORD p = mbi.Protect;
    if (p & (PAGE_GUARD | PAGE_NOACCESS)) return false;
    if (!(p & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
        return false;
    return a + len <= (uint64_t)mbi.BaseAddress + mbi.RegionSize;
}

bool ReadMem(uint64_t a, uint32_t* out) {
    if (!LooksPtr(a) || !Readable(a, 16)) return false;
    __try {
        memcpy(out, (const void*)a, 16);
        return true;
    } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        return false;
    }
}

// C++ part of the thunk (first FnProbeArgs hits, then 1 in FnProbeSample). a0..a3 = the function's own rcx, rdx, r8, r9.
void Common(int idx, uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, const uint64_t* sp, uint64_t old) {
    Slot& s = g_slots[idx];
    const uint64_t t = __rdtsc();
    s.lastTsc.store(t, std::memory_order_relaxed);
    uint64_t z = 0;
    if (s.firstTsc.load(std::memory_order_relaxed) == 0 && s.firstTsc.compare_exchange_strong(z, t))
        s.firstTid.store(GetCurrentThreadId(), std::memory_order_relaxed);
    const uint64_t ret = sp[0];
    s.samples.fetch_add(1, std::memory_order_relaxed);
    uint32_t h = (uint32_t)((ret * 0x9E3779B97F4A7C15ull) >> 58);  // 6 bits
    bool placed = false;
    for (int k = 0; k < kCallerSlots; ++k, h = (h + 1) & (kCallerSlots - 1)) {
        uint64_t key = s.callerKey[h].load(std::memory_order_relaxed);
        if (key == 0) {
            uint64_t e = 0;
            if (s.callerKey[h].compare_exchange_strong(e, ret)) key = ret;
            else key = e;
        }
        if (key == ret) {
            s.callerCnt[h].fetch_add(1, std::memory_order_relaxed);
            placed = true;
            break;
        }
    }
    if (!placed) s.overflow.fetch_add(1, std::memory_order_relaxed);
    if (old < g_firstN && old < kMaxArgRec) {
        ArgRec& r = s.rec[old];
        r.tid = GetCurrentThreadId();
        r.tsc = t;
        r.ret = ret;
        r.a[0] = a0;
        r.a[1] = a1;
        r.a[2] = a2;
        r.a[3] = a3;
        r.memOk = 0;
        for (int k = 0; k < 4; ++k)
            if (ReadMem(r.a[k], r.mem[k])) r.memOk |= (uint8_t)(1u << k);
        r.ready.store(1, std::memory_order_release);
    }
}

template <int I>
void Hk(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, const uint64_t* sp, uint64_t old) {
    Common(I, a0, a1, a2, a3, sp, old);
}
using HookFn = void (*)(uint64_t, uint64_t, uint64_t, uint64_t, const uint64_t*, uint64_t);
template <int... Is>
struct HookTable {
    static constexpr HookFn fns[sizeof...(Is)] = {&Hk<Is>...};
};
template <int N, int... Is>
struct MakeTable : MakeTable<N - 1, N - 1, Is...> {};
template <int... Is>
struct MakeTable<0, Is...> : HookTable<Is...> {};
const HookFn* const kHooks = MakeTable<kMaxFn>::fns;

// ---- patching (scheme of bosslog.cpp / loadstats.cpp) ----
uint8_t* g_thunks = nullptr;
size_t g_thunkUsed = 0;
constexpr size_t kThunkPage = 16384;  // kMaxFn x kThunkMax = 8 KB

uint8_t* AllocIn(uintptr_t from, uintptr_t to) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    const uintptr_t g = si.dwAllocationGranularity;
    MEMORY_BASIC_INFORMATION mbi;
    for (uintptr_t a = (from + g - 1) & ~(g - 1); a < to;) {
        if (!VirtualQuery((void*)a, &mbi, sizeof(mbi))) break;
        const uintptr_t rb = (uintptr_t)mbi.BaseAddress, re = rb + mbi.RegionSize;
        if (mbi.State == MEM_FREE) {
            const uintptr_t c = (rb + g - 1) & ~(g - 1);
            if (c >= from && c + kThunkPage <= re && c + kThunkPage <= to)
                if (void* p = VirtualAlloc((void*)c, kThunkPage, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE)) return (uint8_t*)p;
        }
        if (re <= a) break;
        a = re;
    }
    return nullptr;
}
uint8_t* AllocNear(uintptr_t lo, uintptr_t hi) {
    const uintptr_t reach = 0x7FF00000ull - (hi - lo);
    if (uint8_t* p = AllocIn(hi, hi + reach)) return p;
    return AllocIn(lo > reach + 0x10000 ? lo - reach : 0x10000, lo);
}

bool InExecSection(uint32_t rva, uint32_t len) {
    const auto* dos = (const IMAGE_DOS_HEADER*)g_base;
    const auto* nt = (const IMAGE_NT_HEADERS64*)(g_base + dos->e_lfanew);
    const IMAGE_SECTION_HEADER* sh = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i)
        if ((sh[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) && rva >= sh[i].VirtualAddress &&
            rva + len <= sh[i].VirtualAddress + sh[i].Misc.VirtualSize)
            return true;
    return false;
}

struct Pending {
    uint8_t* entry;
    int n;
    uint8_t bytes[16];
    int slot;
};

// Suspends every other thread, retries while one sits inside a prologue that is about to change, then writes all entries.
int WriteAll(Pending* pend, int np) {
    const DWORD self = GetCurrentThreadId(), pid = GetCurrentProcessId();
    for (int attempt = 0; attempt < 50; ++attempt) {
        HANDLE th[1024];
        int nth = 0;
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snap == INVALID_HANDLE_VALUE) return -1;
        THREADENTRY32 te;
        te.dwSize = sizeof(te);
        for (BOOL ok = Thread32First(snap, &te); ok && nth < 1024; ok = Thread32Next(snap, &te)) {
            if (te.th32OwnerProcessID != pid || te.th32ThreadID == self) continue;
            HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, te.th32ThreadID);
            if (!h) continue;
            if (SuspendThread(h) == (DWORD)-1) {
                CloseHandle(h);
                continue;
            }
            th[nth++] = h;
        }
        CloseHandle(snap);
        bool busy = false;
        for (int i = 0; i < nth && !busy; ++i) {
            CONTEXT c;
            c.ContextFlags = CONTEXT_CONTROL;
            if (!GetThreadContext(th[i], &c)) continue;
            for (int k = 0; k < np; ++k)
                if (c.Rip > (DWORD64)pend[k].entry && c.Rip < (DWORD64)(pend[k].entry + pend[k].n)) busy = true;
        }
        int written = 0;
        if (!busy) {
            for (int k = 0; k < np; ++k) {
                DWORD old = 0;
                if (!VirtualProtect(pend[k].entry, (SIZE_T)pend[k].n, PAGE_EXECUTE_READWRITE, &old)) continue;
                memcpy(pend[k].entry, pend[k].bytes, (size_t)pend[k].n);
                VirtualProtect(pend[k].entry, (SIZE_T)pend[k].n, old, &old);
                FlushInstructionCache(GetCurrentProcess(), pend[k].entry, (SIZE_T)pend[k].n);
                g_slots[pend[k].slot].on = true;
                ++written;
            }
        }
        for (int i = 0; i < nth; ++i) {
            ResumeThread(th[i]);
            CloseHandle(th[i]);
        }
        if (!busy) return written;
        Sleep(2);
    }
    return -2;
}

// ---- report ----
double TscHz() {
    LARGE_INTEGER q;
    QueryPerformanceCounter(&q);
    const double sec = (double)(q.QuadPart - g_qpc0.QuadPart) / (double)g_qpcFreq.QuadPart;
    const uint64_t dt = __rdtsc() - g_tsc0;
    return sec > 0.5 ? (double)dt / sec : 0.0;
}

void FmtWall(uint64_t tsc, double hz, char* out, size_t cap) {
    if (!tsc || hz <= 0) {
        snprintf(out, cap, "-");
        return;
    }
    const double sec = (double)(tsc - g_tsc0) / hz;
    FILETIME ft;
    SystemTimeToFileTime(&g_wall0, &ft);
    ULARGE_INTEGER u;
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    u.QuadPart += (uint64_t)(sec * 1e7);
    ft.dwLowDateTime = u.LowPart;
    ft.dwHighDateTime = u.HighPart;
    SYSTEMTIME st;
    FileTimeToSystemTime(&ft, &st);
    snprintf(out, cap, "%02u:%02u:%02u.%03u", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}

void FmtAddr(uint64_t a, char* out, size_t cap) {
    if (a >= g_base && a < g_imageEnd) {
        snprintf(out, cap, "%llX", (unsigned long long)(a - g_base));
        return;
    }
    HMODULE m = nullptr;
    char name[MAX_PATH] = {0};
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)a, &m) && m &&
        GetModuleFileNameA(m, name, sizeof(name))) {
        const char* b = strrchr(name, '\\');
        snprintf(out, cap, "%s+%llX", b ? b + 1 : name, (unsigned long long)(a - (uint64_t)m));
    } else {
        snprintf(out, cap, "abs:%llX", (unsigned long long)a);
    }
}

void Report(int windowSec) {
    const double hz = TscHz();
    int hit = 0;
    uint64_t tot = 0;
    for (int i = 0; i < g_nSlots; ++i) {
        tot += g_slots[i].count;
        hit += g_slots[i].count != 0;
    }
    g_log("FnProbe: report (window %d s): %d of %d probed functions hit, %llu hits in total", windowSec, hit, g_nSlots, (unsigned long long)tot);
    char never[400] = {0};
    size_t nl = 0;
    for (int i = 0; i < g_nSlots; ++i) {
        Slot& s = g_slots[i];
        if (!s.on) continue;
        const uint64_t c = s.count;
        if (c == 0) {
            if (nl + 12 < sizeof(never)) nl += (size_t)snprintf(never + nl, sizeof(never) - nl, " %X", s.rva);
            continue;
        }
        const uint64_t d = c - s.lastCount;
        s.lastCount = c;
        char f[16], l[16];
        FmtWall(s.firstTsc.load(), hz, f, sizeof(f));
        FmtWall(s.lastTsc.load(), hz, l, sizeof(l));
        // top 4 callers among the sampled hits
        int top[4] = {-1, -1, -1, -1};
        int distinct = 0;
        for (int k = 0; k < kCallerSlots; ++k) {
            if (!s.callerKey[k].load()) continue;
            ++distinct;
            const uint64_t ck = s.callerCnt[k].load();
            for (int j = 0; j < 4; ++j) {
                if (top[j] < 0 || ck > s.callerCnt[top[j]].load()) {
                    for (int m = 3; m > j; --m) top[m] = top[m - 1];
                    top[j] = k;
                    break;
                }
            }
        }
        const uint64_t smp = s.samples.load();
        char cl[200] = {0};
        size_t cn = 0;
        for (int j = 0; j < 4 && top[j] >= 0; ++j) {
            char a[64];
            FmtAddr(s.callerKey[top[j]].load(), a, sizeof(a));
            cn += (size_t)snprintf(cl + cn, sizeof(cl) - cn, " %s %.0f%%", a, smp ? 100.0 * (double)s.callerCnt[top[j]].load() / (double)smp : 0.0);
        }
        g_log("FnProbe %X hits %llu (+%llu in %d s, %.1f/s, peak %llu/s) first %s last %s tid %u | callers (%d distinct%s, %llu sampled):%s",
              s.rva, (unsigned long long)c, (unsigned long long)d, windowSec, windowSec ? (double)d / windowSec : 0.0,
              (unsigned long long)s.peakSec, f, l, s.firstTid.load(), distinct, s.overflow.load() ? "+" : "", (unsigned long long)smp, cl);
        s.peakSec = 0;
        // argument records of the first hits, once each
        for (; s.recLogged < g_firstN && s.recLogged < (uint64_t)kMaxArgRec; ++s.recLogged) {
            ArgRec& r = s.rec[s.recLogged];
            if (!r.ready.load(std::memory_order_acquire)) break;
            char t[16], ra[64], mem[260] = {0}, hs[160] = {0};
            FmtWall(r.tsc, hz, t, sizeof(t));
            FmtAddr(r.ret, ra, sizeof(ra));
            static const char* const kReg[4] = {"rcx", "rdx", "r8", "r9"};
            size_t ml = 0, hl = 0;
            for (int k = 0; k < 4; ++k) {
                if ((r.a[k] >> 32) == 0 && IsHashish((uint32_t)r.a[k]) && hl + 20 < sizeof(hs))
                    hl += (size_t)snprintf(hs + hl, sizeof(hs) - hl, " %s=%08X", kReg[k], (uint32_t)r.a[k]);
                if (!(r.memOk & (1u << k))) continue;
                ml += (size_t)snprintf(mem + ml, sizeof(mem) - ml, " [%s] %08X %08X %08X %08X", kReg[k], r.mem[k][0], r.mem[k][1], r.mem[k][2], r.mem[k][3]);
                for (int w = 0; w < 4; ++w)
                    if (IsHashish(r.mem[k][w]) && hl + 24 < sizeof(hs))
                        hl += (size_t)snprintf(hs + hl, sizeof(hs) - hl, " [%s+%d]=%08X", kReg[k], w * 4, r.mem[k][w]);
            }
            g_log("FnProbe %X hit#%llu %s tid %u ret %s: rcx=%llX rdx=%llX r8=%llX r9=%llX |%s | hash?%s", s.rva, (unsigned long long)(s.recLogged + 1), t,
                  r.tid, ra, (unsigned long long)r.a[0], (unsigned long long)r.a[1], (unsigned long long)r.a[2], (unsigned long long)r.a[3],
                  ml ? mem : " (no readable pointer args)", hl ? hs : " none");
        }
    }
    if (nl) g_log("FnProbe: never hit yet:%s", never);
}

DWORD WINAPI ReportThread(LPVOID) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    int sec = 0;
    for (;;) {
        Sleep(1000);
        for (int i = 0; i < g_nSlots; ++i) {
            Slot& s = g_slots[i];
            const uint64_t c = s.count;
            const uint64_t d = c - s.lastSecCount;
            s.lastSecCount = c;
            if (d > s.peakSec) s.peakSec = d;
        }
        if (++sec >= g_reportSec) {
            Report(sec);
            sec = 0;
        }
    }
}

int ParseList(const wchar_t* s, uint32_t* out, int cap) {
    int n = 0;
    const wchar_t* p = s;
    while (*p && n < cap) {
        while (*p == L' ' || *p == L',' || *p == L'\t' || *p == L';') ++p;
        if (!*p) break;
        wchar_t* e = nullptr;
        const unsigned long long v = wcstoull(p, &e, 16);  // "0x4f9750" and "4F9750" both parse
        if (e == p) break;
        if (v && v < 0x10000000ull) out[n++] = (uint32_t)v;
        p = e;
    }
    return n;
}

}  // namespace

bool Start(const StartInfo& info) {
    g_log = info.log;
    g_base = info.base;
    if (!g_log || !g_base || !info.iniPath) return false;
    if (GetPrivateProfileIntW(L"Debug", L"FnProbe", 0, info.iniPath) == 0) return false;
    if (!info.knownBuild) {
        g_log("FnProbe: refused (unknown build; RVAs are v2.02)");
        return false;
    }
    const int profRs = (int)GetPrivateProfileIntW(L"Profiler", L"ReportSec", 30, info.iniPath);
    g_reportSec = (int)GetPrivateProfileIntW(L"Debug", L"FnProbeReportSec", (UINT)(profRs > 0 ? profRs : 30), info.iniPath);
    if (g_reportSec <= 0) g_reportSec = profRs > 0 ? profRs : 30;
    if (g_reportSec < 5) g_reportSec = 5;
    if (g_reportSec > 600) g_reportSec = 600;
    int firstN = (int)GetPrivateProfileIntW(L"Debug", L"FnProbeArgs", 8, info.iniPath);
    if (firstN < 0) firstN = 0;
    if (firstN > kMaxArgRec) firstN = kMaxArgRec;
    g_firstN = (uint32_t)firstN;
    int sample = (int)GetPrivateProfileIntW(L"Debug", L"FnProbeSample", 16, info.iniPath);
    uint32_t mask = 0;
    while ((int)(mask + 1) * 2 <= sample && mask < 0xFFFF) mask = mask * 2 + 1;  // largest power of two <= sample, minus 1
    wchar_t list[1024] = {0};
    GetPrivateProfileStringW(L"Debug", L"FnProbeList", L"", list, 1024, info.iniPath);
    uint32_t rvas[kMaxFn];
    int nr = ParseList(list, rvas, kMaxFn);
    const bool deflt = nr == 0;
    if (deflt)
        for (const Known& k : kDefault) rvas[nr++] = k.rva;

    const auto* dos = (const IMAGE_DOS_HEADER*)g_base;
    const auto* nt = (const IMAGE_NT_HEADERS64*)(g_base + dos->e_lfanew);
    g_imageEnd = g_base + nt->OptionalHeader.SizeOfImage;
    g_thunks = AllocNear(g_base, g_imageEnd);
    if (!g_thunks) {
        g_log("FnProbe: no executable page within 2 GB of the image - off");
        return false;
    }
    QueryPerformanceFrequency(&g_qpcFreq);
    QueryPerformanceCounter(&g_qpc0);
    g_tsc0 = __rdtsc();
    GetLocalTime(&g_wall0);

    Pending pend[kMaxFn];
    int np = 0;
    char bad[400] = {0};
    size_t bl = 0;
    for (int i = 0; i < nr; ++i) {
        const uint32_t rva = rvas[i];
        uint8_t* p = (uint8_t*)(g_base + rva);
        const char* why = nullptr;
        int n = -1;
        bool dup = false;
        for (int k = 0; k < np; ++k) dup |= pend[k].entry == p;
        if (dup) why = "duplicate";
        else if (!InExecSection(rva, 16)) why = "not in code";
        else {
            const Known* kn = nullptr;
            for (const Known& k : kDefault)
                if (k.rva == rva) kn = &k;
            if (kn) {
                if (memcmp(p, kn->orig, kn->n) == 0) n = kn->n;
                else why = "bytes";
            } else {
                n = PrologueLen(p);
                if (n < 0) why = "prologue";
            }
        }
        if (why) {
            if (bl + 32 < sizeof(bad)) bl += (size_t)snprintf(bad + bl, sizeof(bad) - bl, " %X(%s)", rva, why);
            continue;
        }
        if (g_thunkUsed + kThunkMax > kThunkPage) break;
        const int slot = g_nSlots++;
        Slot& s = g_slots[slot];
        s.rva = rva;
        uint8_t* t = g_thunks + g_thunkUsed;
        const size_t sz = BuildThunk(t, p, n, &s.count, g_firstN, mask, (const void*)kHooks[slot]);
        g_thunkUsed += (sz + 15) & ~size_t(15);
        const intptr_t d = (intptr_t)t - (intptr_t)(p + 5);
        if (d < INT32_MIN || d > INT32_MAX) {
            if (bl + 32 < sizeof(bad)) bl += (size_t)snprintf(bad + bl, sizeof(bad) - bl, " %X(reach)", rva);
            continue;
        }
        Pending& pe = pend[np++];
        pe.entry = p;
        pe.n = n;
        pe.slot = slot;
        memset(pe.bytes, 0x90, sizeof(pe.bytes));
        pe.bytes[0] = 0xE9;
        const int32_t rel = (int32_t)d;
        memcpy(pe.bytes + 1, &rel, 4);
    }
    FlushInstructionCache(GetCurrentProcess(), g_thunks, g_thunkUsed);
    const int w = np ? WriteAll(pend, np) : 0;
    if (w < 0) {
        g_log("FnProbe: could not patch (%s) - off", w == -1 ? "thread snapshot failed" : "a thread stayed inside a prologue for 50 tries");
        return false;
    }
    g_log("FnProbe: %d/%d functions hooked (%s list; measurement only; report every %d s, args of the first %u hits, callers 1 in %u hits)%s%s", w, nr,
          deflt ? "default 10.1" : "FnProbeList", g_reportSec, g_firstN, mask + 1, bl ? " - skipped:" : "", bad);
    if (w == 0) return false;
    if (HANDLE h = CreateThread(nullptr, 0, &ReportThread, nullptr, 0, nullptr)) CloseHandle(h);
    return true;
}

}  // namespace fnprobe
