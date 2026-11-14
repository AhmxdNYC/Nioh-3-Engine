// padguard.cpp - PadListGuard ([General] PadListGuard = 1 by default; GitHub #79 / #55). See padguard.h / padguard_cave.h.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "gamerva.h"
#include <tlhelp32.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "padguard.h"
#include "padguard_cave.h"

namespace padguard {
namespace {

using padguard_cave::kNumSites;
using padguard_cave::kWalks;

void (*g_log)(const char*, ...) = nullptr;
uint8_t* g_page = nullptr;
uintptr_t g_base = 0;
uint32_t g_installed = 0;  // bit k = kWalks[k] guarded
uintptr_t g_siteAt[kNumSites] = {};

uint8_t* AllocIn(uintptr_t from, uintptr_t to, size_t size) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    const uintptr_t g = si.dwAllocationGranularity;
    MEMORY_BASIC_INFORMATION mbi;
    for (uintptr_t a = (from + g - 1) & ~(g - 1); a < to;) {
        if (!VirtualQuery((void*)a, &mbi, sizeof(mbi))) break;
        const uintptr_t rb = (uintptr_t)mbi.BaseAddress, re = rb + mbi.RegionSize;
        if (mbi.State == MEM_FREE) {
            const uintptr_t c = (rb + g - 1) & ~(g - 1);
            if (c >= from && c + size <= re && c + size <= to)
                if (void* p = VirtualAlloc((void*)c, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE)) return (uint8_t*)p;
        }
        if (re <= a) break;
        a = re;
    }
    return nullptr;
}
uint8_t* AllocNear(uintptr_t lo, uintptr_t hi, size_t size) {
    const uintptr_t reach = 0x7FF00000ull - (hi - lo);
    if (uint8_t* p = AllocIn(hi, hi + reach, size)) return p;
    return AllocIn(lo > reach + 0x10000 ? lo - reach : 0x10000, lo, size);
}

struct Write {
    uint8_t* at;
    uint8_t bytes[16];
    unsigned len;
};
constexpr int kMaxWrites = kNumSites;

// Same scheme as asyncslots.cpp / drawprep.cpp FrozenWrite: all other threads suspended, none stopped strictly inside a site
// (a thread at a site's first byte is at an instruction boundary in both encodings; the replaced cmp / je contain no call, so
// no return address points inside one). No heap / CRT use while threads are suspended.
bool FrozenWrite(Write* w, int n) {
    if (n < 0 || n > kMaxWrites) return false;
    DWORD oldProt[kMaxWrites] = {};
    for (int i = 0; i < n; ++i)
        if (!VirtualProtect(w[i].at, w[i].len, PAGE_EXECUTE_READWRITE, &oldProt[i])) {
            for (int j = 0; j < i; ++j) { DWORD t; VirtualProtect(w[j].at, w[j].len, oldProt[j], &t); }
            return false;
        }
    const DWORD self = GetCurrentThreadId(), pid = GetCurrentProcessId();
    constexpr int kMaxT = 1024;
    static DWORD tids[kMaxT];
    static HANDLE hs[kMaxT];
    static bool sus[kMaxT];
    bool written = false;
    for (int attempt = 0; attempt < 200 && !written; ++attempt) {
        int nt = 0;
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snap != INVALID_HANDLE_VALUE) {
            THREADENTRY32 te{};
            te.dwSize = sizeof(te);
            for (BOOL ok = Thread32First(snap, &te); ok && nt < kMaxT; ok = Thread32Next(snap, &te))
                if (te.th32OwnerProcessID == pid && te.th32ThreadID != self) tids[nt++] = te.th32ThreadID;
            CloseHandle(snap);
        }
        int nh = 0;
        for (int i = 0; i < nt; ++i)
            if (HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, tids[i])) hs[nh++] = h;
        // ---- frozen section ----
        for (int i = 0; i < nh; ++i) sus[i] = SuspendThread(hs[i]) != (DWORD)-1;
        bool clash = false;
        for (int i = 0; i < nh && !clash; ++i) {
            if (!sus[i]) continue;
            CONTEXT ctx{};
            ctx.ContextFlags = CONTEXT_CONTROL;
            if (!GetThreadContext(hs[i], &ctx)) continue;
            for (int k = 0; k < n; ++k)
                if (ctx.Rip > (uintptr_t)w[k].at && ctx.Rip < (uintptr_t)w[k].at + w[k].len) { clash = true; break; }
        }
        if (!clash) {
            for (int k = 0; k < n; ++k) memcpy(w[k].at, w[k].bytes, w[k].len);
            FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
            written = true;
        }
        for (int i = 0; i < nh; ++i)
            if (sus[i]) ResumeThread(hs[i]);
        // ---- end frozen section ----
        for (int i = 0; i < nh; ++i) CloseHandle(hs[i]);
        if (!written) Sleep(1 + attempt % 4);
    }
    for (int i = 0; i < n; ++i) { DWORD t; VirtualProtect(w[i].at, w[i].len, oldProt[i], &t); }
    if (!written) return false;
    for (int i = 0; i < n; ++i)
        if (memcmp(w[i].at, w[i].bytes, w[i].len) != 0) return false;
    return true;
}

volatile long* Counter(int k) { return (volatile long*)(g_page + padguard_cave::kCounters + 4 * k); }

// One line per hit: every hit of a site is logged up to its 20th, then at most one line per site every 10 s (with the total).
DWORD WINAPI Watch(void*) {
    long seen[kNumSites] = {}, lines[kNumSites] = {};
    ULONGLONG lastLine[kNumSites] = {};
    for (;;) {
        Sleep(100);
        const ULONGLONG now = GetTickCount64();
        for (int k = 0; k < kNumSites; ++k) {
            if (!(g_installed & (1u << k))) continue;
            const long c = *Counter(k);
            if (c == seen[k]) continue;
            if (lines[k] >= 20 && now - lastLine[k] < 10000) continue;
            seen[k] = c;
            lastLine[k] = now;
            ++lines[k];
            g_log("PADGUARD: null list link at +%X (hit %ld)%s", (unsigned)(g_siteAt[k] - g_base), c,
                  lines[k] == 20 ? " - further hits of this walk: one line per 10 s" : "");
        }
    }
}

}  // namespace

long Hits() {
    if (!g_page) return 0;
    long n = 0;
    for (int k = 0; k < kNumSites; ++k)
        if (g_installed & (1u << k)) n += *Counter(k);
    return n;
}

void Start(const StartInfo& info) {
    g_log = info.log;
    if (!g_log || !info.base || !info.iniPath) return;
    g_base = info.base;
    const int want = (int)GetPrivateProfileIntW(L"General", L"PadListGuard", 1, info.iniPath);
    if (want == 0) {
        g_log("PADGUARD: off ([General] PadListGuard = 0) - the pad list walks are stock");
        return;
    }
    // verify every site: exact original bytes and the first instruction at its exit; each installs on its own
    uintptr_t site[kNumSites] = {}, exitAt[kNumSites] = {};
    uint32_t okMask = 0;
    char skipped[256] = "";
    size_t so = 0;
    for (int k = 0; k < kNumSites; ++k) {
        const padguard_cave::Site& s = kWalks[k];
        site[k] = info.base + Grva(kWalks[k].rva);  // 0 on another build when the pattern is missing / not unique
        bool ok = site[k] != info.base && padguard_cave::Shaped(s);
        if (ok) ok = memcmp((const void*)site[k], s.orig, s.len) == 0;
        if (ok) {
            exitAt[k] = padguard_cave::ExitOf(s, site[k]);
            ok = memcmp((const void*)exitAt[k], s.exitBytes, s.exitLen) == 0;
        }
        if (ok) okMask |= 1u << k;
        else if (so + 12 < sizeof skipped) so += (size_t)snprintf(skipped + so, sizeof skipped - so, " %X", kWalks[k].rva);
    }
    if (!okMask) {
        g_log("PADGUARD: not installed (pad manager code bytes differ at every site - another patch or build)");
        return;
    }
    const auto* dos = (const IMAGE_DOS_HEADER*)info.base;
    const auto* nt = (const IMAGE_NT_HEADERS64*)(info.base + dos->e_lfanew);
    g_page = AllocNear(info.base, info.base + nt->OptionalHeader.SizeOfImage, 4096);
    if (!g_page) {
        g_log("PADGUARD: not installed (no executable page within 2 GB of the image)");
        return;
    }
    uint8_t* const p = g_page;
    memset(p, 0xCC, 4096);
    memset(p + padguard_cave::kCounters, 0, 4 * kNumSites);
    // caves + chained unwind info (host function's own RUNTIME_FUNCTION): a stack walk stopped inside a cave unwinds like the
    // host's loop. The cave's jumps carry a REX 40 prefix so RtlVirtualUnwind never mistakes them for an epilogue tail jump.
    const uintptr_t tableBase = (uintptr_t)p < info.base ? (uintptr_t)p : info.base;
    auto* rf = (RUNTIME_FUNCTION*)(p + padguard_cave::kTable);
    int nrf = 0;
    Write w[kMaxWrites];
    int nw = 0;
    for (int k = 0; k < kNumSites; ++k) {
        if (!(okMask & (1u << k))) continue;
        const padguard_cave::Site& s = kWalks[k];
        uint8_t* cave = p + padguard_cave::kBlock * k;
        const size_t len = padguard_cave::Build(cave, s, site[k] + s.len, exitAt[k], (uintptr_t)Counter(k));
        if (!len || !padguard_cave::SiteBytes(w[nw].bytes, s, site[k], (uintptr_t)cave)) {
            okMask &= ~(1u << k);
            continue;
        }
        DWORD64 hostBase = 0;
        if (const PRUNTIME_FUNCTION host = RtlLookupFunctionEntry(site[k], &hostBase, nullptr)) {
            uint8_t* uw = p + padguard_cave::kUnwind + 16 * k;
            padguard_cave::WriteChained(uw, tableBase, hostBase + host->BeginAddress, hostBase + host->EndAddress,
                                        hostBase + (host->UnwindData & ~1u), host->UnwindData & 1u);
            rf[nrf].BeginAddress = (DWORD)((uintptr_t)cave - tableBase);
            rf[nrf].EndAddress = (DWORD)((uintptr_t)cave + len - tableBase);
            rf[nrf].UnwindData = (DWORD)((uintptr_t)uw - tableBase);
            ++nrf;
        }
        w[nw].at = (uint8_t*)site[k];
        w[nw].len = s.len;
        ++nw;
    }
    FlushInstructionCache(GetCurrentProcess(), p, 4096);
    const bool unwound = nrf > 0 && RtlAddFunctionTable(rf, (DWORD)nrf, tableBase);
    if (!nw || !FrozenWrite(w, nw)) {
        if (unwound) RtlDeleteFunctionTable(rf);
        g_log("PADGUARD: not installed (site write failed)");
        return;
    }
    for (int k = 0; k < kNumSites; ++k) g_siteAt[k] = site[k];
    g_installed = okMask;
    if (HANDLE h = CreateThread(nullptr, 0, Watch, nullptr, 0, nullptr)) CloseHandle(h);
    char list[256];
    size_t lo = 0;
    list[0] = 0;
    for (int k = 0; k < kNumSites; ++k)
        if ((okMask & (1u << k)) && lo + 12 < sizeof list) lo += (size_t)snprintf(list + lo, sizeof list - lo, " %X", (unsigned)(site[k] - info.base));
    g_log("PADGUARD: installed at %d of %d pad list walks (%s) -%s; unwind %s - [General] PadListGuard = %d (#79/#55: a null link "
          "ends that walk for the frame instead of crashing)",
          nw, kNumSites, info.knownBuild ? "v2.02" : "found by pattern", list, unwound ? "registered" : "NOT registered", want);
    if (skipped[0]) g_log("PADGUARD: walks NOT guarded (bytes differ - another patch or build):%s", skipped);
}

}  // namespace padguard
