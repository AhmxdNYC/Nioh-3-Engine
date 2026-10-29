// readprobe.cpp - RP read probe ([Profiler] ReadProbe, measurement only) + EP event pool ([Patches] EP). See readprobe.h and
// notes\loading-ally-fixed.md 8. Every hook calls the original import with the original arguments and returns its result,
// except EP's two event hooks (pool hit / pool return), which are exact by construction (readprobe.h).
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>

#include "loadstats.h"
#include "prefetch.h"
#include "readfix.h"
#include "readprobe.h"

namespace readprobe {
namespace {

// ---- v2.02 addresses (RVAs). Sources: notes\loading-ally-fixed.md 8.1, re\symbols\incoming\read-renames.tsv ----
constexpr uint32_t kGameMainPtr = 0x4759910, kGm58A = 0x58A, kGmFrames = 0x610;
constexpr uint32_t kIatReadFile = 0x38E23B8, kIatReadFileEx = 0x38E26C8, kIatCreateEventA = 0x38E25C8;
constexpr uint32_t kIatCloseHandle = 0x38E2390, kIatWfmo = 0x38E25D0;
constexpr uint32_t kEvCreate = 0x311EE1;  // FileReqAlloc 311E50: CreateEventA(NULL, TRUE, FALSE, NULL) per OVERLAPPED
constexpr uint32_t kEvClose = 0x311F89;   // FileReqFree 311F58: CloseHandle(event[i]) after FileReqWaitAll
constexpr uint32_t kWfmo = 0x3127A1;      // FileReqWaitAll 312754: WaitForMultipleObjects(n, events, TRUE, INFINITE)
constexpr uint32_t kCacheCall = 0x198357C, kCacheFn = 0x2F77A6C;  // E8 -> FsReadCacheLookup (5 args, eax > 0 = hit)
// EP preconditions: the CreateEventA argument setup (r9d = 0 name, r8d = 0 not signalled, ecx = 0 no SA, edx = 1 manual)
// and FileReqFree's call of FileReqWaitAll before its CloseHandle loop.
constexpr uint32_t kEvArgs = 0x311ED5;
constexpr uint8_t kEvArgBytes[12] = {0x45, 0x33, 0xC9, 0x45, 0x33, 0xC0, 0x33, 0xC9, 0x41, 0x8D, 0x51, 0x01};
constexpr uint32_t kFreeWaitCall = 0x311F6F, kWaitAll = 0x312754;

constexpr int kNSites = 14;  // 13 ReadFile + ReadFileEx (last)
struct RSite {
    uint32_t rva;
    int8_t ls;  // LoadStats read class (0 archive layer, 1 other) at its 7 sites, else -1
};
constexpr RSite kRS[kNSites] = {{0x311D17, 0},  {0x312DD2, 0},  {0x31305A, 0},  {0x3130D6, 0},  {0x740392, 1},
                                {0x1FD9CA3, 1}, {0x32562BE, 1}, {0xAF5656, -1}, {0xBDCC46, -1}, {0xBF9485, -1},
                                {0xBF9647, -1}, {0xBF98E5, -1}, {0xBF9FC7, -1}, {0x631FD5, -1}};

// ---- counters: monotonic since start; a window = the difference of two snapshots ----
constexpr int kCls = 24, kSz = 4, kDur = 5, kLoc = 5;
enum Loc { kReread, kSeq, kNear, kInblk, kFar };
enum SF { fCalls, fBytes, fTicks, fOvl, fPend, fErr, fFp, fSyncH, fSz0, fDur0 = fSz0 + kSz, fLoc0 = fDur0 + kDur, fNoLoc = fLoc0 + kLoc,
          fInflG, fInflH, fCls0, fN = fCls0 + kCls };
enum GF { gCr, gCrT, gCl, gClT, gWf, gWfT, gEpHit, gEpPut, gEpClose, gUntracked, gClsRd0, gClsRdT0 = gClsRd0 + kCls,
          gClsWf0 = gClsRdT0 + kCls, gClsWfT0 = gClsWf0 + kCls, gN = gClsWfT0 + kCls };
std::atomic<uint64_t> g_sv[kNSites][fN];
std::atomic<uint64_t> g_gv[gN];
std::atomic<uint32_t> g_inflMax[kNSites];
std::atomic<uint32_t> g_inflight{0};
volatile uint64_t* g_cacheCalls = nullptr;  // in the R3 thunk (asm lock inc)
volatile uint64_t* g_cacheHits = nullptr;

inline void Add(std::atomic<uint64_t>& a, uint64_t v = 1) { a.fetch_add(v, std::memory_order_relaxed); }

void (*g_log)(const char*, ...) = nullptr;
uintptr_t g_base = 0;
int g_mode = 0;          // ReadProbe 0/1/2
bool g_rp = false;       // probe hooks live
bool g_ep = false;       // EP live
bool g_lsFeed = false;   // feed LoadStats' read counters
bool g_fix = false;      // readfix.cpp (RC / AQ) serves the archive read sites
bool g_pf = false;       // 1.7.26 prefetch.cpp (PF) observes 31305A / 3130D6 / 631FD5 before the real call
double g_usPerTick = 0;  // microseconds per QPC tick
uint64_t g_qpcFreq = 0;
wchar_t g_iniDir[MAX_PATH] = {0};

uint64_t Qpc() {
    LARGE_INTEGER q;
    QueryPerformanceCounter(&q);
    return (uint64_t)q.QuadPart;
}

// ---- thread pool of the calling thread (GetThreadDescription "KIDSTS[s%d:<pool>]"), cached in a TLS slot ----
using GetThrDescFn = HRESULT(WINAPI*)(HANDLE, PWSTR*);
GetThrDescFn g_getDesc = nullptr;
DWORD g_tls = TLS_OUT_OF_INDEXES;
SRWLOCK g_clLock = SRWLOCK_INIT;
char g_clName[kCls][24] = {"other"};
std::atomic<int> g_nCl{1};
std::atomic<uint32_t> g_clThreads[kCls];

int ThreadClass() {  // may change the last-error value: callers restore it
    if (g_tls == TLS_OUT_OF_INDEXES) return 0;
    if (void* v = TlsGetValue(g_tls)) return (int)(uintptr_t)v - 1;
    char nm[24] = "unnamed";
    PWSTR d = nullptr;
    if (g_getDesc && SUCCEEDED(g_getDesc(GetCurrentThread(), &d)) && d) {
        const wchar_t* s = d;
        const wchar_t* b = wcschr(d, L'[');
        const wchar_t* c = b ? wcschr(b, L':') : nullptr;
        if (c) s = c + 1;
        size_t j = 0;
        for (; s[j] && s[j] != L']' && j < sizeof(nm) - 1; ++j) nm[j] = s[j] < 128 && s[j] > 32 ? (char)s[j] : '_';
        nm[j] = 0;
        if (!j) strcpy_s(nm, "unnamed");
        LocalFree(d);
    }
    int idx = 0;
    AcquireSRWLockExclusive(&g_clLock);
    const int n = g_nCl.load(std::memory_order_relaxed);
    for (int i = 1; i < n; ++i)
        if (!strcmp(g_clName[i], nm)) { idx = i; break; }
    if (!idx && n < kCls) {
        strcpy_s(g_clName[n], nm);
        idx = n;
        g_nCl.store(n + 1, std::memory_order_release);
    }
    ReleaseSRWLockExclusive(&g_clLock);
    g_clThreads[idx].fetch_add(1, std::memory_order_relaxed);
    TlsSetValue(g_tls, (void*)(uintptr_t)(idx + 1));
    return idx;
}

// ---- per-handle table (4096 slots, open addressing, never shrinks; path re-read on first use in every window) ----
constexpr int kHBits = 12, kHSlots = 1 << kHBits;
struct HEnt {
    std::atomic<uintptr_t> h;
    std::atomic<uint32_t> win;       // window id the path/size/mode were read for
    std::atomic<uint32_t> inflight;  // threads inside a read on this handle
    std::atomic<uint32_t> siteMask;  // sites that read it in the current window
    std::atomic<long> lk;            // guards the fields below
    char name[56];
    uint64_t size;
    uint8_t sync;  // 1 synchronous handle, 0 overlapped, 2 unknown
    uint64_t lastEnd;
    uint64_t rrOff[8];
    uint32_t rrN[8];
    uint32_t blk[8];
    uint8_t bpos, rpos;
    std::atomic<uint64_t> wCalls, wBytes, wTicks;
};
HEnt g_h[kHSlots];
std::atomic<uint32_t> g_winId{1};

using NtQifFn = LONG(NTAPI*)(HANDLE, void*, void*, ULONG, int);
using NtResetFn = LONG(NTAPI*)(HANDLE, LONG*);
NtQifFn g_ntQif = nullptr;
NtResetFn g_ntReset = nullptr;

SRWLOCK g_bigLock = SRWLOCK_INIT;
wchar_t g_bigPath[520] = {0};  // biggest file read so far (calibration target)
uint64_t g_bigSize = 0;

void Lock(HEnt& e) {
    while (e.lk.exchange(1, std::memory_order_acquire)) _mm_pause();
}
void Unlock(HEnt& e) { e.lk.store(0, std::memory_order_release); }

void QueryHandle(HEnt& e, HANDLE h) {
    wchar_t p[520];
    char nm[56] = "?";
    const DWORD n = GetFinalPathNameByHandleW(h, p, 520, 0);
    if (n && n < 520) {
        const wchar_t* t = p + n;
        for (int sl = 0; t > p; --t)
            if (t[-1] == L'\\' && ++sl == 2) break;  // "<dir>\<file>"
        WideCharToMultiByte(CP_UTF8, 0, t, -1, nm, (int)sizeof(nm), nullptr, nullptr);
        nm[sizeof(nm) - 1] = 0;
    }
    FILE_STANDARD_INFO fsi{};
    const uint64_t size = GetFileInformationByHandleEx(h, FileStandardInfo, &fsi, sizeof(fsi)) ? (uint64_t)fsi.EndOfFile.QuadPart : 0;
    uint8_t sync = 2;
    ULONG mode = 0;
    struct { void* st; ULONG_PTR info; } iosb{};
    if (g_ntQif && g_ntQif(h, &iosb, &mode, sizeof(mode), 16 /* FileModeInformation */) >= 0)
        sync = (mode & 0x30) ? 1 : 0;  // FILE_SYNCHRONOUS_IO_ALERT | _NONALERT
    Lock(e);
    memcpy(e.name, nm, sizeof(nm));
    e.size = size;
    e.sync = sync;
    e.lastEnd = ~0ull;
    for (int i = 0; i < 8; ++i) { e.rrOff[i] = ~0ull; e.rrN[i] = 0; e.blk[i] = ~0u; }
    e.bpos = e.rpos = 0;
    Unlock(e);
    if (n && n < 520 && size > g_bigSize) {
        AcquireSRWLockExclusive(&g_bigLock);
        if (size > g_bigSize) {
            g_bigSize = size;
            wcscpy_s(g_bigPath, p);
        }
        ReleaseSRWLockExclusive(&g_bigLock);
    }
}

int HandleSlot(HANDLE h) {
    const uintptr_t k = (uintptr_t)h;
    if (!k || h == INVALID_HANDLE_VALUE) return -1;
    uint32_t i = (uint32_t)(((uint64_t)(k >> 2) * 0x9E3779B97F4A7C15ull) >> (64 - kHBits));
    for (int probe = 0; probe < 64; ++probe, i = (i + 1) & (kHSlots - 1)) {
        HEnt& e = g_h[i];
        uintptr_t cur = e.h.load(std::memory_order_acquire);
        if (!cur && e.h.compare_exchange_strong(cur, k)) cur = k;
        if (cur != k) continue;
        const uint32_t w = g_winId.load(std::memory_order_relaxed);
        uint32_t ew = e.win.load(std::memory_order_relaxed);
        if (ew != w && e.win.compare_exchange_strong(ew, w)) QueryHandle(e, h);
        return (int)i;
    }
    Add(g_gv[gUntracked]);
    return -1;
}

// reread (same offset + size among the last 8) > seq (offset = last end) > near (< 64 KiB away) > inblk (its 64 KiB block is
// one of the last 8) > far.
int Classify(HEnt& e, uint64_t off, uint32_t n) {
    Lock(e);
    int c = kFar;
    bool rr = false;
    for (int i = 0; i < 8; ++i) rr |= e.rrOff[i] == off && e.rrN[i] == n;
    const uint32_t b = (uint32_t)(off >> 16);
    if (rr) c = kReread;
    else if (off == e.lastEnd) c = kSeq;
    else if (e.lastEnd != ~0ull && (off > e.lastEnd ? off - e.lastEnd : e.lastEnd - off) < 0x10000) c = kNear;
    else
        for (int i = 0; i < 8; ++i)
            if (e.blk[i] == b) { c = kInblk; break; }
    e.lastEnd = off + n;
    const uint32_t b1 = (uint32_t)((off + (n ? n - 1 : 0)) >> 16);
    for (uint32_t x = b; x <= b1 && x - b < 8; ++x) {
        if (e.blk[(e.bpos + 7) & 7] == x) continue;
        e.blk[e.bpos] = x;
        e.bpos = (uint8_t)((e.bpos + 1) & 7);
    }
    e.rrOff[e.rpos] = off;
    e.rrN[e.rpos] = n;
    e.rpos = (uint8_t)((e.rpos + 1) & 7);
    Unlock(e);
    return c;
}

// ---- RP=2 trace: 32-byte records in a 32 MB ring ----
#pragma pack(push, 1)
struct Rec {
    uint64_t t0, off;
    uint32_t n, us;
    uint16_t slot;
    uint8_t site, flags, cls, inflG, inflH, loc;
};
#pragma pack(pop)
static_assert(sizeof(Rec) == 32, "trace record");
constexpr uint64_t kTrN = 1u << 20;
Rec* g_tr = nullptr;
std::atomic<uint64_t> g_trIdx{0};

// ---- read hooks ----
struct Pre {
    int slot, cls;
    uint64_t off;
    uint32_t inflG, inflH;
    bool fp;
};

void Begin(HANDLE h, LPOVERLAPPED ov, Pre& p) {
    p.cls = ThreadClass();
    p.slot = HandleSlot(h);
    p.fp = !ov;
    if (ov) {
        p.off = (uint64_t)ov->Offset | (uint64_t)ov->OffsetHigh << 32;
    } else {  // synchronous read at the file pointer (save data, small readers)
        LARGE_INTEGER z{}, c{};
        p.off = SetFilePointerEx(h, z, &c, FILE_CURRENT) ? (uint64_t)c.QuadPart : ~0ull;
    }
    p.inflG = g_inflight.fetch_add(1, std::memory_order_relaxed);
    p.inflH = p.slot >= 0 ? g_h[p.slot].inflight.fetch_add(1, std::memory_order_relaxed) : 0;
}

void End(int s, const Pre& p, DWORD n, bool ov, BOOL r, DWORD le, uint64_t t0, uint64_t t1) {
    std::atomic<uint64_t>* v = g_sv[s];
    const uint64_t dt = t1 - t0;
    const double us = (double)dt * g_usPerTick;
    Add(v[fCalls]);
    Add(v[fBytes], n);
    Add(v[fTicks], dt);
    uint8_t fl = 0;
    if (ov) { Add(v[fOvl]); fl |= 1; }
    if (p.fp) { Add(v[fFp]); fl |= 8; }
    if (!r) {
        const bool pend = le == ERROR_IO_PENDING;
        Add(v[pend ? fPend : fErr]);
        fl |= pend ? 2 : 4;
    }
    Add(v[fSz0 + (n <= 1024 ? 0 : n <= 4096 ? 1 : n <= 65536 ? 2 : 3)]);
    Add(v[fDur0 + (us < 8 ? 0 : us < 32 ? 1 : us < 128 ? 2 : us < 512 ? 3 : 4)]);
    int loc = 255;
    if (p.slot >= 0) {
        HEnt& e = g_h[p.slot];
        if (e.sync == 1) { Add(v[fSyncH]); fl |= 16; }
        if (p.off != ~0ull) loc = Classify(e, p.off, n);
        Add(e.wCalls);
        Add(e.wBytes, n);
        Add(e.wTicks, dt);
        e.siteMask.fetch_or(1u << s, std::memory_order_relaxed);
        e.inflight.fetch_sub(1, std::memory_order_relaxed);
    }
    Add(v[loc == 255 ? fNoLoc : fLoc0 + loc]);
    Add(v[fInflG], p.inflG);
    Add(v[fInflH], p.inflH);
    const uint32_t ig = p.inflG + 1;
    uint32_t m = g_inflMax[s].load(std::memory_order_relaxed);
    while (ig > m && !g_inflMax[s].compare_exchange_weak(m, ig, std::memory_order_relaxed)) {}
    Add(v[fCls0 + p.cls], dt);
    Add(g_gv[gClsRd0 + p.cls]);
    Add(g_gv[gClsRdT0 + p.cls], dt);
    g_inflight.fetch_sub(1, std::memory_order_relaxed);
    if (g_lsFeed && kRS[s].ls >= 0) loadstats::NoteRead(kRS[s].ls, n, ov, dt);
    if (g_tr) {
        Rec& rc = g_tr[g_trIdx.fetch_add(1, std::memory_order_relaxed) & (kTrN - 1)];
        rc.t0 = t0;
        rc.off = p.off;
        rc.n = n;
        rc.us = us > 4e9 ? 0xFFFFFFFFu : (uint32_t)us;
        rc.slot = (uint16_t)(p.slot < 0 ? 0xFFFF : p.slot);
        rc.site = (uint8_t)s;
        rc.flags = fl;
        rc.cls = (uint8_t)p.cls;
        rc.inflG = (uint8_t)(p.inflG > 255 ? 255 : p.inflG);
        rc.inflH = (uint8_t)(p.inflH > 255 ? 255 : p.inflH);
        rc.loc = (uint8_t)loc;
    }
}

using ReadFn = BOOL(WINAPI*)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
using ReadExFn = BOOL(WINAPI*)(HANDLE, LPVOID, DWORD, LPOVERLAPPED, LPOVERLAPPED_COMPLETION_ROUTINE);

// 1.7.24: the R1 hooks also go live with ReadProbe = 0 when readfix (RC / AQ) is on; then only LoadStats is fed.
template <int S>
BOOL WINAPI HkRead(HANDLE h, LPVOID buf, DWORD n, LPDWORD got, LPOVERLAPPED ov) {
    if constexpr (S == 2 || S == 3) {
        if (g_pf) prefetch::Note(S, h, ov, n);  // observation only, keeps the last error
    }
    const DWORD le0 = GetLastError();
    Pre p{};
    if (g_rp) Begin(h, ov, p);
    SetLastError(le0);
    const uint64_t t0 = Qpc();
    const BOOL r = g_fix ? (BOOL)readfix::Read(S, h, buf, n, got, ov) : (*reinterpret_cast<ReadFn*>(g_base + kIatReadFile))(h, buf, n, got, ov);
    const uint64_t t1 = Qpc();
    const DWORD le = GetLastError();
    if (g_rp) End(S, p, n, ov != nullptr, r, le, t0, t1);
    else if (g_lsFeed && kRS[S].ls >= 0) loadstats::NoteRead(kRS[S].ls, n, ov != nullptr, t1 - t0);
    SetLastError(le);
    return r;
}

BOOL WINAPI HkReadEx(HANDLE h, LPVOID buf, DWORD n, LPOVERLAPPED ov, LPOVERLAPPED_COMPLETION_ROUTINE cr) {
    if (g_pf) prefetch::Note(kNSites - 1, h, ov, n);  // observation only, keeps the last error
    if (!g_rp) return (*reinterpret_cast<ReadExFn*>(g_base + kIatReadFileEx))(h, buf, n, ov, cr);
    const DWORD le0 = GetLastError();
    Pre p;
    Begin(h, ov, p);
    SetLastError(le0);
    const ReadExFn real = *reinterpret_cast<ReadExFn*>(g_base + kIatReadFileEx);
    const uint64_t t0 = Qpc();
    const BOOL r = real(h, buf, n, ov, cr);
    const uint64_t t1 = Qpc();
    const DWORD le = GetLastError();
    End(kNSites - 1, p, n, true, r, r ? 0 : le, t0, t1);  // ReadFileEx: TRUE = queued (completion by APC)
    SetLastError(le);
    return r;
}

const void* const kReadHooks[kNSites] = {
    (const void*)&HkRead<0>,  (const void*)&HkRead<1>,  (const void*)&HkRead<2>,  (const void*)&HkRead<3>,
    (const void*)&HkRead<4>,  (const void*)&HkRead<5>,  (const void*)&HkRead<6>,  (const void*)&HkRead<7>,
    (const void*)&HkRead<8>,  (const void*)&HkRead<9>,  (const void*)&HkRead<10>, (const void*)&HkRead<11>,
    (const void*)&HkRead<12>, (const void*)&HkReadEx};

// ---- EP event pool + R2 event hooks ----
constexpr int kEpCap = 4096;
HANDLE g_epPool[kEpCap];
int g_epN = 0;
SRWLOCK g_epLock = SRWLOCK_INIT;

HANDLE EpGet() {
    HANDLE h = nullptr;
    AcquireSRWLockExclusive(&g_epLock);
    if (g_epN) h = g_epPool[--g_epN];
    ReleaseSRWLockExclusive(&g_epLock);
    return h;
}

// Takes the event into the pool only if it was signalled (=> no read pending on it) and is now reset; else false (the
// caller closes it for real, as stock). NtResetEvent leaves the Win32 last-error value alone.
bool EpPut(HANDLE h) {
    if (!h || h == INVALID_HANDLE_VALUE || !g_ntReset) return false;
    LONG prev = 0;
    if (g_ntReset(h, &prev) < 0 || prev != 1) return false;
    AcquireSRWLockExclusive(&g_epLock);
    const bool ok = g_epN < kEpCap;
    if (ok) g_epPool[g_epN++] = h;
    ReleaseSRWLockExclusive(&g_epLock);
    return ok;
}

using CreateEvAFn = HANDLE(WINAPI*)(LPSECURITY_ATTRIBUTES, BOOL, BOOL, LPCSTR);
using CloseFn = BOOL(WINAPI*)(HANDLE);
using WfmoFn = DWORD(WINAPI*)(DWORD, const HANDLE*, BOOL, DWORD);

HANDLE WINAPI HkCreateEventA(LPSECURITY_ATTRIBUTES sa, BOOL manual, BOOL initial, LPCSTR name) {
    const uint64_t t0 = g_rp ? Qpc() : 0;
    HANDLE h = g_ep ? EpGet() : nullptr;
    if (h) {
        Add(g_gv[gEpHit]);
        SetLastError(ERROR_SUCCESS);
    } else {
        h = (*reinterpret_cast<CreateEvAFn*>(g_base + kIatCreateEventA))(sa, manual, initial, name);
    }
    if (g_rp) {
        const uint64_t t1 = Qpc();
        Add(g_gv[gCr]);
        Add(g_gv[gCrT], t1 - t0);
    }
    return h;
}

BOOL WINAPI HkCloseEvent(HANDLE h) {
    const uint64_t t0 = g_rp ? Qpc() : 0;
    BOOL r;
    if (g_ep && EpPut(h)) {
        r = TRUE;
        Add(g_gv[gEpPut]);
    } else {
        r = (*reinterpret_cast<CloseFn*>(g_base + kIatCloseHandle))(h);
        if (g_ep) Add(g_gv[gEpClose]);
    }
    if (g_rp) {
        const uint64_t t1 = Qpc();
        Add(g_gv[gCl]);
        Add(g_gv[gClT], t1 - t0);
    }
    return r;
}

DWORD WINAPI HkWfmo(DWORD n, const HANDLE* hs, BOOL all, DWORD ms) {
    const uint64_t t0 = Qpc();
    const DWORD r = (*reinterpret_cast<WfmoFn*>(g_base + kIatWfmo))(n, hs, all, ms);
    const uint64_t dt = Qpc() - t0;
    const DWORD le = GetLastError();
    const int c = ThreadClass();
    Add(g_gv[gWf]);
    Add(g_gv[gWfT], dt);
    Add(g_gv[gClsWf0 + c]);
    Add(g_gv[gClsWfT0 + c], dt);
    SetLastError(le);
    return r;
}

// ---- patching (same scheme as loadstats.cpp: one interlocked write, or the EB FE two-step for a straddling site) ----
uint8_t* g_thunks = nullptr;
size_t g_thunkUsed = 0;
constexpr size_t kThunkPage = 4096;

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

// push rcx, rdx, r8, r9, r10, r11; sub rsp,38h; [rsp+20h] = the caller's 5th argument; call target; restore; ret.
// Entry rsp = 8 mod 16, 6 pushes + 38h -> 16-aligned at the call. rax = the target's result.
uint8_t* NewSaveThunk(const void* target) {
    static const uint8_t code[48] = {
        0x51, 0x52, 0x41, 0x50, 0x41, 0x51, 0x41, 0x52, 0x41, 0x53,  // push rcx rdx r8 r9 r10 r11
        0x48, 0x83, 0xEC, 0x38,                                      // sub rsp,38h
        0x48, 0x8B, 0x84, 0x24, 0x90, 0x00, 0x00, 0x00,              // mov rax,[rsp+90h]   (entry rsp + 28h)
        0x48, 0x89, 0x44, 0x24, 0x20,                                // mov [rsp+20h],rax
        0xFF, 0x15, 0x0F, 0x00, 0x00, 0x00,                          // call [rip+0Fh] -> qword at +48
        0x48, 0x83, 0xC4, 0x38,                                      // add rsp,38h
        0x41, 0x5B, 0x41, 0x5A, 0x41, 0x59, 0x41, 0x58, 0x5A, 0x59,  // pop r11 r10 r9 r8 rdx rcx
        0xC3};
    if (g_thunkUsed + 64 > kThunkPage) return nullptr;
    uint8_t* t = g_thunks + g_thunkUsed;
    memcpy(t, code, sizeof(code));
    memcpy(t + 48, &target, 8);
    g_thunkUsed += 64;
    return t;
}

// Same as NewSaveThunk for callees with 7 arguments (CreateFileW): sub rsp,48h, forwards the 3 stack arguments from
// entry rsp + 28h/30h/38h (= rsp + A0h/A8h/B0h after 6 pushes + 48h) to [rsp+20h/28h/30h]; 16-aligned at the call.
uint8_t* NewSaveThunk7(const void* target) {
    static const uint8_t code[74] = {
        0x51, 0x52, 0x41, 0x50, 0x41, 0x51, 0x41, 0x52, 0x41, 0x53,  // push rcx rdx r8 r9 r10 r11
        0x48, 0x83, 0xEC, 0x48,                                      // sub rsp,48h
        0x48, 0x8B, 0x84, 0x24, 0xA0, 0x00, 0x00, 0x00,              // mov rax,[rsp+A0h]
        0x48, 0x89, 0x44, 0x24, 0x20,                                // mov [rsp+20h],rax
        0x48, 0x8B, 0x84, 0x24, 0xA8, 0x00, 0x00, 0x00,              // mov rax,[rsp+A8h]
        0x48, 0x89, 0x44, 0x24, 0x28,                                // mov [rsp+28h],rax
        0x48, 0x8B, 0x84, 0x24, 0xB0, 0x00, 0x00, 0x00,              // mov rax,[rsp+B0h]
        0x48, 0x89, 0x44, 0x24, 0x30,                                // mov [rsp+30h],rax
        0xFF, 0x15, 0x15, 0x00, 0x00, 0x00,                          // call [rip+15h] -> qword at +80
        0x48, 0x83, 0xC4, 0x48,                                      // add rsp,48h
        0x41, 0x5B, 0x41, 0x5A, 0x41, 0x59, 0x41, 0x58, 0x5A, 0x59,  // pop r11 r10 r9 r8 rdx rcx
        0xC3};
    if (g_thunkUsed + 96 > kThunkPage) return nullptr;
    uint8_t* t = g_thunks + g_thunkUsed;
    memset(t, 0xCC, 96);
    memcpy(t, code, sizeof(code));
    memcpy(t + 80, &target, 8);
    g_thunkUsed += 96;
    return t;
}

// R3: register-neutral counting thunk around FsReadCacheLookup (only rax and flags change, as across the original call).
uint8_t* NewCacheThunk(const void* target) {
    static const uint8_t code[45] = {
        0xF0, 0x48, 0xFF, 0x05, 0x30, 0x00, 0x00, 0x00,  // lock inc qword [rip+30h] -> calls at +56
        0x48, 0x83, 0xEC, 0x38,                          // sub rsp,38h (entry 8 mod 16 -> aligned)
        0x48, 0x8B, 0x44, 0x24, 0x60,                    // mov rax,[rsp+60h]  (5th argument, entry rsp + 28h)
        0x48, 0x89, 0x44, 0x24, 0x20,                    // mov [rsp+20h],rax
        0xFF, 0x15, 0x14, 0x00, 0x00, 0x00,              // call [rip+14h] -> target at +48
        0x48, 0x83, 0xC4, 0x38,                          // add rsp,38h
        0x85, 0xC0,                                      // test eax,eax
        0x7E, 0x08,                                      // jle +8
        0xF0, 0x48, 0xFF, 0x05, 0x14, 0x00, 0x00, 0x00,  // lock inc qword [rip+14h] -> hits at +64
        0xC3};
    if (g_thunkUsed + 80 > kThunkPage) return nullptr;
    uint8_t* t = g_thunks + g_thunkUsed;
    memset(t, 0xCC, 80);
    memcpy(t, code, sizeof(code));
    memcpy(t + 48, &target, 8);
    memset(t + 56, 0, 16);
    g_cacheCalls = reinterpret_cast<volatile uint64_t*>(t + 56);
    g_cacheHits = reinterpret_cast<volatile uint64_t*>(t + 64);
    g_thunkUsed += 80;
    return t;
}

bool PatchAtomic(uint8_t* p, const uint8_t* bytes, size_t len) {
    const uintptr_t a = (uintptr_t)p, w8 = a & ~uintptr_t(7), w16 = a & ~uintptr_t(15);
    const bool fit8 = a + len <= w8 + 8, fit16 = a + len <= w16 + 16;
    if (!fit8 && !fit16) return false;
    void* win = fit8 ? (void*)w8 : (void*)w16;
    const size_t wlen = fit8 ? 8 : 16;
    DWORD old = 0;
    if (!VirtualProtect(win, wlen, PAGE_EXECUTE_READWRITE, &old)) return false;
    bool ok = false;
    if (fit8) {
        volatile LONG64* q = (volatile LONG64*)w8;
        const LONG64 cur = *q;
        LONG64 nw = cur;
        memcpy((uint8_t*)&nw + (a - w8), bytes, len);
        ok = InterlockedCompareExchange64(q, nw, cur) == cur;
    } else {
        volatile LONG64* q = (volatile LONG64*)w16;
        alignas(16) LONG64 cur[2] = {q[0], q[1]};
        LONG64 nw[2] = {cur[0], cur[1]};
        memcpy((uint8_t*)nw + (a - w16), bytes, len);
        ok = _InterlockedCompareExchange128(q, nw[1], nw[0], cur) != 0;
    }
    VirtualProtect(win, wlen, old, &old);
    FlushInstructionCache(GetCurrentProcess(), win, wlen);
    return ok;
}

bool PatchSpin(uint8_t* p, const uint8_t* bytes, size_t len) {
    static const uint8_t spin[2] = {0xEB, 0xFE};
    uint8_t orig[2] = {p[0], p[1]};
    if (len < 3 || !PatchAtomic(p, spin, 2)) return false;
    DWORD old = 0;
    if (!VirtualProtect(p + 2, len - 2, PAGE_EXECUTE_READWRITE, &old)) {
        PatchAtomic(p, orig, 2);
        return false;
    }
    memcpy(p + 2, bytes + 2, len - 2);
    VirtualProtect(p + 2, len - 2, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, len);
    return PatchAtomic(p, bytes, 2);
}

bool Patch(uint8_t* p, const uint8_t* bytes, size_t len) { return PatchAtomic(p, bytes, len) || PatchSpin(p, bytes, len); }

// FF 15 [iat] -> E8 save-thunk + NOP. 0 ok, 1 bytes differ (other build / another hook owns it), 2 no room / not atomic.
int HookIatCall(uint32_t rva, uint32_t iat, const void* hook, int stackArgs = 1) {
    uint8_t* p = (uint8_t*)(g_base + rva);
    if (p[0] != 0xFF || p[1] != 0x15) return 1;
    int32_t rel;
    memcpy(&rel, p + 2, 4);
    if ((uintptr_t)(p + 6 + rel) != g_base + iat) return 1;
    uint8_t* t = stackArgs > 1 ? NewSaveThunk7(hook) : NewSaveThunk(hook);
    if (!t) return 2;
    const intptr_t d = (intptr_t)t - (intptr_t)(p + 5);
    if (d < INT32_MIN || d > INT32_MAX) return 2;
    uint8_t nb[6] = {0xE8, 0, 0, 0, 0, 0x90};
    const int32_t nrel = (int32_t)d;
    memcpy(nb + 1, &nrel, 4);
    return Patch(p, nb, 6) ? 0 : 2;
}

int HookCacheCall() {
    uint8_t* p = (uint8_t*)(g_base + kCacheCall);
    if (p[0] != 0xE8) return 1;
    int32_t rel;
    memcpy(&rel, p + 1, 4);
    if ((uintptr_t)(p + 5 + rel) != g_base + kCacheFn) return 1;
    uint8_t* t = NewCacheThunk((const void*)(g_base + kCacheFn));
    if (!t) return 2;
    const intptr_t d = (intptr_t)t - (intptr_t)(p + 5);
    if (d < INT32_MIN || d > INT32_MAX) return 2;
    uint8_t nb[5] = {0xE8, 0, 0, 0, 0};
    const int32_t nrel = (int32_t)d;
    memcpy(nb + 1, &nrel, 4);
    return Patch(p, nb, 5) ? 0 : 2;
}

// ---- sampler: 58A windows, calibration, logs ----
bool SafeCopy(void* dst, const void* src, size_t n) {
    __try {
        memcpy(dst, src, n);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
template <class T>
bool Rd(uintptr_t a, T& v) {
    return a > 0x10000 && SafeCopy(&v, (const void*)a, sizeof(T));
}

struct Snap {
    uint64_t s[kNSites][fN];
    uint64_t g[gN];
    uint64_t cacheC, cacheH, qpc;
};
void TakeSnap(Snap& o) {
    for (int i = 0; i < kNSites; ++i)
        for (int f = 0; f < fN; ++f) o.s[i][f] = g_sv[i][f].load(std::memory_order_relaxed);
    for (int f = 0; f < gN; ++f) o.g[f] = g_gv[f].load(std::memory_order_relaxed);
    o.cacheC = g_cacheCalls ? *g_cacheCalls : 0;
    o.cacheH = g_cacheHits ? *g_cacheHits : 0;
    o.qpc = Qpc();
}

struct Mini {  // 1 s totals for the work-end split
    uint64_t qpc, calls, bytes, ticks, wfT, cr;
};
constexpr int kMini = 1800;
Mini g_mini[kMini];
int g_nMini = 0;
Mini TakeMini() {
    Mini m{Qpc(), 0, 0, 0, g_gv[gWfT].load(), g_gv[gCr].load()};
    for (int i = 0; i < kNSites; ++i) {
        m.calls += g_sv[i][fCalls].load();
        m.bytes += g_sv[i][fBytes].load();
        m.ticks += g_sv[i][fTicks].load();
    }
    return m;
}

Snap g_s0, g_s1;
bool g_open = false;
int g_winN = 0;
uint64_t g_trOpen = 0;
constexpr double kMB = 1024.0 * 1024.0;

double Pct(uint64_t a, uint64_t b) { return b ? 100.0 * (double)a / (double)b : 0.0; }

void LogMiniSpan(const char* label, const Mini& a, const Mini& b) {
    const double sec = (double)(b.qpc - a.qpc) / (double)g_qpcFreq;
    g_log("ReadProbe #%d %s %.1f s: %llu calls, %.0f MB, in-call %.2f thread-s, WFMO %.2f thread-s, CreateEventA %llu", g_winN, label,
          sec, (unsigned long long)(b.calls - a.calls), (double)(b.bytes - a.bytes) / kMB,
          (double)(b.ticks - a.ticks) * g_usPerTick / 1e6, (double)(b.wfT - a.wfT) * g_usPerTick / 1e6,
          (unsigned long long)(b.cr - a.cr));
}

void OpenWindow() {
    g_open = true;
    ++g_winN;
    g_winId.fetch_add(1);
    for (HEnt& e : g_h) {
        if (!e.h.load(std::memory_order_relaxed)) continue;
        e.wCalls = 0;
        e.wBytes = 0;
        e.wTicks = 0;
        e.siteMask = 0;
    }
    for (auto& m : g_inflMax) m = 0;
    TakeSnap(g_s0);
    g_trOpen = g_trIdx.load();
    g_nMini = 0;
    g_mini[g_nMini++] = TakeMini();
}

void FlushTrace(uint64_t trEnd) {
    uint64_t from = g_trOpen;
    const uint64_t dropped = trEnd - from > kTrN ? trEnd - from - kTrN : 0;
    from += dropped;
    wchar_t path[MAX_PATH + 64];
    swprintf_s(path, L"%sNioh3EngineFix_reads_%d.bin", g_iniDir, g_winN);
    HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) {
        g_log("ReadProbe #%d trace: cannot write %ls (error %lu)", g_winN, path, GetLastError());
        return;
    }
    struct {
        char magic[8];
        uint64_t qpcFreq, qpcOpen, count, dropped;
        uint32_t recSize, window;
    } hdr = {{'R', 'P', 'T', 'R', 'A', 'C', 'E', '1'}, g_qpcFreq, g_s0.qpc, trEnd - from, dropped, (uint32_t)sizeof(Rec), (uint32_t)g_winN};
    DWORD w = 0;
    WriteFile(f, &hdr, sizeof(hdr), &w, nullptr);
    for (uint64_t i = from; i < trEnd;) {  // contiguous runs of the ring
        const uint64_t at = i & (kTrN - 1), run = (trEnd - i) < (kTrN - at) ? (trEnd - i) : (kTrN - at);
        const uint64_t chunk = run > 65536 ? 65536 : run;
        WriteFile(f, &g_tr[at], (DWORD)(chunk * sizeof(Rec)), &w, nullptr);
        i += chunk;
    }
    CloseHandle(f);
    swprintf_s(path, L"%sNioh3EngineFix_reads_%d.txt", g_iniDir, g_winN);
    FILE* t = nullptr;
    if (_wfopen_s(&t, path, L"w") == 0 && t) {
        fprintf(t, "# ReadProbe trace #%d: header {char[8] 'RPTRACE1', u64 qpcFreq, u64 qpcOpen, u64 count, u64 dropped, u32 recSize, u32 "
                   "window} then count x 32-byte records {u64 qpc, u64 offset (~0 unknown), u32 size, u32 us, u16 handle slot (FFFF none), "
                   "u8 site, u8 flags (1 OVERLAPPED arg, 2 pending, 4 error, 8 file-pointer offset, 16 synchronous handle), u8 pool, "
                   "u8 in-flight global, u8 in-flight same handle, u8 locality (0 reread 1 seq 2 near 3 inblk 4 far 255 none)}\n", g_winN);
        for (int i = 0; i < kNSites; ++i) fprintf(t, "site\t%d\t%X%s\n", i, kRS[i].rva, i == kNSites - 1 ? "\tReadFileEx" : "");
        for (int i = 0; i < g_nCl.load(); ++i) fprintf(t, "pool\t%d\t%s\n", i, g_clName[i]);
        for (int i = 0; i < kHSlots; ++i)
            if (g_h[i].h.load()) fprintf(t, "handle\t%d\t%d\t%llu\t%s\n", i, g_h[i].sync, (unsigned long long)g_h[i].size, g_h[i].name);
        fclose(t);
    }
    g_log("ReadProbe #%d trace: %llu records (%llu dropped) -> %ls", g_winN, (unsigned long long)(trEnd - from),
          (unsigned long long)dropped, path);
}

void CloseWindow() {
    g_open = false;
    TakeSnap(g_s1);
    const uint64_t trEnd = g_trIdx.load();
    const Snap& a = g_s0;
    const Snap& b = g_s1;
    const double sec = (double)(b.qpc - a.qpc) / (double)g_qpcFreq;
    const int nCl = g_nCl.load();
    // sites, by in-call time
    int order[kNSites];
    for (int i = 0; i < kNSites; ++i) order[i] = i;
    auto dS = [&](int s, int f) { return b.s[s][f] - a.s[s][f]; };
    for (int i = 0; i < kNSites; ++i)
        for (int j = i + 1; j < kNSites; ++j)
            if (dS(order[j], fTicks) > dS(order[i], fTicks)) { const int x = order[i]; order[i] = order[j]; order[j] = x; }
    uint64_t totC = 0, totB = 0, totT = 0;
    for (int k = 0; k < kNSites; ++k) {
        const int s = order[k];
        const uint64_t c = dS(s, fCalls);
        totC += c;
        totB += dS(s, fBytes);
        totT += dS(s, fTicks);
        if (!c) continue;
        char thr[160] = {0};
        size_t tl = 0;
        bool used[kCls] = {};
        for (int r = 0; r < 3; ++r) {  // top 3 pools by in-call time
            int best = -1;
            for (int ci = 0; ci < nCl; ++ci)
                if (!used[ci] && dS(s, fCls0 + ci) && (best < 0 || dS(s, fCls0 + ci) > dS(s, fCls0 + best))) best = ci;
            if (best < 0) break;
            used[best] = true;
            tl += (size_t)snprintf(thr + tl, sizeof(thr) - tl, "%s%s %.0f %%", tl ? " " : "", g_clName[best],
                                   Pct(dS(s, fCls0 + best), dS(s, fTicks)));
        }
        int handles = 0;
        for (const HEnt& e : g_h)
            if (e.siteMask.load(std::memory_order_relaxed) & (1u << s)) ++handles;
        const uint64_t locN = c - dS(s, fNoLoc);
        g_log("ReadProbe #%d site %X%s: %llu calls, %.0f MB, in-call %.2f s (avg %.0f us) | thr %s | handles %d, sync-handle %.0f %% | "
              "ovl-api %.0f %% pending %.0f %% err %llu fp-offset %.0f %% | size <=1K %.0f %% 1-4K %.0f %% 4-64K %.0f %% >64K %.0f %% | "
              "us <8 %.0f %% 8-32 %.0f %% 32-128 %.0f %% 128-512 %.0f %% >512 %.0f %% | seq %.0f %% near %.0f %% inblk %.0f %% "
              "reread %.0f %% far %.0f %% | inflight global avg %.1f max %u, same-handle avg %.1f",
              g_winN, kRS[s].rva, s == kNSites - 1 ? " (ReadFileEx)" : "", (unsigned long long)c, (double)dS(s, fBytes) / kMB,
              (double)dS(s, fTicks) * g_usPerTick / 1e6, (double)dS(s, fTicks) * g_usPerTick / (double)c, thr[0] ? thr : "-", handles,
              Pct(dS(s, fSyncH), c), Pct(dS(s, fOvl), c), Pct(dS(s, fPend), c), (unsigned long long)dS(s, fErr), Pct(dS(s, fFp), c),
              Pct(dS(s, fSz0), c), Pct(dS(s, fSz0 + 1), c), Pct(dS(s, fSz0 + 2), c), Pct(dS(s, fSz0 + 3), c), Pct(dS(s, fDur0), c),
              Pct(dS(s, fDur0 + 1), c), Pct(dS(s, fDur0 + 2), c), Pct(dS(s, fDur0 + 3), c), Pct(dS(s, fDur0 + 4), c),
              Pct(dS(s, fLoc0 + kSeq), locN), Pct(dS(s, fLoc0 + kNear), locN), Pct(dS(s, fLoc0 + kInblk), locN),
              Pct(dS(s, fLoc0 + kReread), locN), Pct(dS(s, fLoc0 + kFar), locN), (double)dS(s, fInflG) / (double)c,
              g_inflMax[s].load(), (double)dS(s, fInflH) / (double)c);
    }
    // top 8 files by in-call time
    int top[8];
    int nTop = 0;
    for (int i = 0; i < kHSlots; ++i) {
        const uint64_t t = g_h[i].wTicks.load(std::memory_order_relaxed);
        if (!t) continue;
        int at = nTop < 8 ? nTop++ : 8;
        while (at > 0 && g_h[top[at - 1]].wTicks.load(std::memory_order_relaxed) < t) {
            if (at < 8) top[at] = top[at - 1];
            --at;
        }
        if (at < 8) top[at] = i;
    }
    char files[1024] = {0};
    size_t fl = 0;
    for (int k = 0; k < nTop && fl < sizeof(files) - 128; ++k) {
        const HEnt& e = g_h[top[k]];
        fl += (size_t)snprintf(files + fl, sizeof(files) - fl, "%s%s %s %.2f s %llu calls %.0f MB", k ? " | " : "", e.name,
                               e.sync == 1 ? "sync" : e.sync == 0 ? "ovl" : "?", (double)e.wTicks.load() * g_usPerTick / 1e6,
                               (unsigned long long)e.wCalls.load(), (double)e.wBytes.load() / kMB);
    }
    if (nTop) g_log("ReadProbe #%d files (top %d by in-call s): %s", g_winN, nTop, files);
    auto dG = [&](int f) { return b.g[f] - a.g[f]; };
    auto avgUs = [&](int t, int n) { return dG(n) ? (double)dG(t) * g_usPerTick / (double)dG(n) : 0.0; };
    g_log("ReadProbe #%d events: CreateEventA %llu (avg %.1f us), CloseHandle %llu (avg %.1f us), WFMO %llu (avg %.0f us, %.2f "
          "thread-s) | EP %s: pool hits %llu, returned %llu, really closed %llu | game cache calls %llu hits %llu",
          g_winN, (unsigned long long)dG(gCr), avgUs(gCrT, gCr), (unsigned long long)dG(gCl), avgUs(gClT, gCl),
          (unsigned long long)dG(gWf), avgUs(gWfT, gWf), (double)dG(gWfT) * g_usPerTick / 1e6, g_ep ? "on" : "off",
          (unsigned long long)dG(gEpHit), (unsigned long long)dG(gEpPut), (unsigned long long)dG(gEpClose),
          (unsigned long long)(b.cacheC - a.cacheC), (unsigned long long)(b.cacheH - a.cacheH));
    char thr[1024] = {0};
    size_t tl = 0;
    for (int ci = 0; ci < nCl && tl < sizeof(thr) - 96; ++ci) {
        const uint64_t rt = dG(gClsRdT0 + ci), wt = dG(gClsWfT0 + ci);
        if (!rt && !wt) continue;
        const uint32_t nt = g_clThreads[ci].load();
        const double denom = (double)(nt ? nt : 1) * sec * 1e6;
        tl += (size_t)snprintf(thr + tl, sizeof(thr) - tl, "%s%s x%u ReadFile %.0f %% (%llu) + WFMO %.0f %% (%llu)", tl ? " | " : "",
                               g_clName[ci], nt, 100.0 * (double)rt * g_usPerTick / denom, (unsigned long long)dG(gClsRd0 + ci),
                               100.0 * (double)wt * g_usPerTick / denom, (unsigned long long)dG(gClsWf0 + ci));
    }
    if (tl) g_log("ReadProbe #%d threads (share of each pool's thread-time in the window): %s", g_winN, thr);
    g_log("ReadProbe #%d total %.1f s: %llu calls, %.0f MB, in-call %.2f thread-s, untracked handles %llu", g_winN, sec,
          (unsigned long long)totC, (double)totB / kMB, (double)totT * g_usPerTick / 1e6, (unsigned long long)dG(gUntracked));
    // split at the work end (LoadGates' last failing evaluation), 1 s resolution
    const uint64_t we = loadstats::LastFailEvalQpc();
    if (we > a.qpc && we < b.qpc && g_nMini > 0) {
        int k = 0;
        while (k + 1 < g_nMini && g_mini[k + 1].qpc <= we) ++k;
        Mini end = TakeMini();
        end.qpc = b.qpc;
        char lab[64];
        snprintf(lab, sizeof(lab), "split [set->work end %+.1f s]", (double)(we - a.qpc) / (double)g_qpcFreq);
        LogMiniSpan(lab, g_mini[0], g_mini[k]);
        LogMiniSpan("split [work end->clear]", g_mini[k], end);
    }
    if (g_tr) FlushTrace(trEnd);
}

// Calibration (once, at the title, 58A clear): cached-read cost per handle type, first-touch random reads, event pair.
void Calibrate(uintptr_t gm) {
    wchar_t path[520] = {0};
    AcquireSRWLockShared(&g_bigLock);
    wcscpy_s(path, g_bigPath);
    ReleaseSRWLockShared(&g_bigLock);
    if (!path[0]) {
        GetModuleFileNameW(nullptr, path, 400);
        if (wchar_t* sl = wcsrchr(path, L'\\')) wcscpy_s(sl + 1, 520 - (size_t)(sl + 1 - path), L"package\\root.rdb");
    }
    HANDLE hs = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    HANDLE ho = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
    HANDLE ev = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    uint8_t* buf = (uint8_t*)VirtualAlloc(nullptr, 1 << 18, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    auto stop = [&]() {
        uint8_t f = 0;
        return Rd(gm + kGm58A, f) && f;
    };
    if (hs == INVALID_HANDLE_VALUE || ho == INVALID_HANDLE_VALUE || !ev || !buf) {
        g_log("ReadProbe calib: cannot open %ls (error %lu) - skipped", path, GetLastError());
    } else {
        LARGE_INTEGER fsz{};
        GetFileSizeEx(hs, &fsz);
        const uint64_t size = (uint64_t)fsz.QuadPart;
        auto rdS = [&](uint64_t off, DWORD n) {
            OVERLAPPED o{};
            o.Offset = (DWORD)off;
            o.OffsetHigh = (DWORD)(off >> 32);
            DWORD got = 0;
            const uint64_t t0 = Qpc();
            ReadFile(hs, buf, n, &got, &o);
            return (double)(Qpc() - t0) * g_usPerTick;
        };
        auto rdO = [&](uint64_t off, DWORD n) {
            OVERLAPPED o{};
            o.Offset = (DWORD)off;
            o.OffsetHigh = (DWORD)(off >> 32);
            o.hEvent = ev;
            DWORD got = 0;
            const uint64_t t0 = Qpc();
            if (!ReadFile(ho, buf, n, &got, &o) && GetLastError() == ERROR_IO_PENDING) GetOverlappedResult(ho, &o, &got, TRUE);
            return (double)(Qpc() - t0) * g_usPerTick;
        };
        auto avg = [&](auto fn, DWORD n) {
            double s = 0;
            for (int i = 0; i < 1000; ++i) s += fn(0, n);
            return s / 1000.0;
        };
        rdS(0, 4096);
        rdO(0, 4096);
        const double c4s = avg(rdS, 4096), c4o = avg(rdO, 4096), c1s = avg(rdS, 1), c1o = avg(rdO, 1);
        double unc[3] = {0, 0, 0};
        const DWORD usz[3] = {4096, 65536, 262144};
        bool uncDone = false;
        if (!stop() && size >= (64ull << 20)) {  // 32 first-touch offsets per size, 1 MiB-aligned, spread over the file
            uint64_t x = Qpc() | 1;
            for (int k = 0; k < 3; ++k) {
                double v[32];
                for (int i = 0; i < 32; ++i) {
                    x ^= x << 13; x ^= x >> 7; x ^= x << 17;
                    const uint64_t off = ((x % (size - (1ull << 20))) & ~0xFFFFFull) + 4096ull * (uint64_t)k;
                    v[i] = rdS(off, usz[k]);
                }
                for (int i = 0; i < 32; ++i)  // median
                    for (int j = i + 1; j < 32; ++j)
                        if (v[j] < v[i]) { const double t = v[i]; v[i] = v[j]; v[j] = t; }
                unc[k] = (v[15] + v[16]) / 2;
            }
            uncDone = true;
        }
        double evp = 0;
        for (int i = 0; i < 200; ++i) {
            const uint64_t t0 = Qpc();
            HANDLE e = CreateEventA(nullptr, TRUE, FALSE, nullptr);
            if (e) CloseHandle(e);
            evp += (double)(Qpc() - t0) * g_usPerTick;
        }
        char pn[96] = {0};
        const wchar_t* t = wcsrchr(path, L'\\');
        WideCharToMultiByte(CP_UTF8, 0, t ? t + 1 : path, -1, pn, (int)sizeof(pn), nullptr, nullptr);
        if (uncDone)
            g_log("ReadProbe calib (%s, %.0f MB): cached 4K sync %.1f us ovl %.1f us, 1B sync %.1f us ovl %.1f us | first-touch median "
                  "4K %.0f us 64K %.0f us 256K %.0f us | CreateEventA+CloseHandle %.1f us",
                  pn, (double)size / kMB, c4s, c4o, c1s, c1o, unc[0], unc[1], unc[2], evp / 200.0);
        else
            g_log("ReadProbe calib (%s, %.0f MB): cached 4K sync %.1f us ovl %.1f us, 1B sync %.1f us ovl %.1f us | first-touch skipped "
                  "(%s) | CreateEventA+CloseHandle %.1f us",
                  pn, (double)size / kMB, c4s, c4o, c1s, c1o, size < (64ull << 20) ? "file < 64 MB" : "load started", evp / 200.0);
    }
    if (buf) VirtualFree(buf, 0, MEM_RELEASE);
    if (ev) CloseHandle(ev);
    if (hs != INVALID_HANDLE_VALUE) CloseHandle(hs);
    if (ho != INVALID_HANDLE_VALUE) CloseHandle(ho);
}

DWORD WINAPI Sampler(void*) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    const uint64_t q0 = Qpc();
    int frame0 = -1;
    bool calDone = false;
    for (unsigned tick = 0;; ++tick) {
        Sleep(10);
        uintptr_t gm = 0;
        if (!Rd(g_base + kGameMainPtr, gm) || !gm) continue;
        uint8_t f = 0;
        if (!Rd(gm + kGm58A, f)) continue;
        if (!g_open) {
            if (f) {
                OpenWindow();
                continue;
            }
            int fr = 0;
            if (!calDone && Rd(gm + kGmFrames, fr)) {
                if (frame0 < 0) frame0 = fr;
                if (fr - frame0 >= 120 && (double)(Qpc() - q0) / (double)g_qpcFreq > 2.0) {
                    calDone = true;
                    Calibrate(gm);
                }
            }
            continue;
        }
        if (!f) {
            CloseWindow();
            continue;
        }
        if (tick % 100 == 0 && g_nMini < kMini) g_mini[g_nMini++] = TakeMini();
    }
}

}  // namespace

bool WantsReads(const wchar_t* iniPath) {
    return iniPath && (int)GetPrivateProfileIntW(L"Profiler", L"LoadStats", 0, iniPath) > 0 &&
           ((int)GetPrivateProfileIntW(L"Profiler", L"ReadProbe", 0, iniPath) > 0 || readfix::Wants(iniPath) || prefetch::Wants(iniPath));
}

namespace {
int HookCallForFix(uint32_t rva, uint32_t iat, const void* hook, int stackArgs) {
    return HookIatCall(rva, iat, hook, stackArgs);
}
}  // namespace

bool Start(const StartInfo& info) {
    g_log = info.log;
    g_base = info.base;
    if (!g_log || !g_base || !info.iniPath) return false;
    g_mode = (int)GetPrivateProfileIntW(L"Profiler", L"ReadProbe", 0, info.iniPath);
    // 1.7.24: EP = -1 auto (default): on under Wine/Proton, off on native Windows; 0 off; 1 on
    const int epIni = (int)GetPrivateProfileIntW(L"Patches", L"EP", -1, info.iniPath);
    bool wineHost = false;
    if (HMODULE nt0 = GetModuleHandleW(L"ntdll.dll")) wineHost = GetProcAddress(nt0, "wine_get_version") != nullptr;
    bool ep = epIni < 0 ? wineHost : epIni != 0;
    g_log("EP: ini %d (%s) on %s -> %s", epIni, epIni < 0 ? "auto" : epIni ? "forced on" : "forced off", wineHost ? "Wine/Proton" : "native Windows", ep ? "ON" : "OFF");
    {  // 1.7.28: SR = -1 auto (default): 1 = load windows under Wine/Proton (Ally R2: 47.25 -> 38.45 s), off on native Windows
        int srIni = 0;
        bool srWine = false;
        const int sr = readfix::SrMode(info.iniPath, &srIni, &srWine);
        g_log("SR: ini %d (%s) on %s -> %s", srIni, srIni < 0 ? "auto" : srIni ? "forced on" : "forced off", srWine ? "Wine/Proton" : "native Windows",
              sr == 0 ? "OFF" : srWine ? (sr == 1 ? "ON (load windows)" : "ON (always)") : "refused (Wine/Proton only)");
    }
    bool rp = g_mode > 0;
    if (rp && !info.loadStatsOn) {
        g_log("ReadProbe: needs [Profiler] LoadStats >= 1 - off");
        rp = false;
    }
    const bool fixWanted = readfix::Wants(info.iniPath);
    const bool pfWanted = prefetch::Wants(info.iniPath);
    if (!rp && !ep && !fixWanted && !pfWanted) return false;
    if (!info.knownBuild) {
        g_log("ReadProbe/EP/ReadFix: refused (unknown build; addresses are v2.02)");
        return false;
    }
    LARGE_INTEGER fq;
    QueryPerformanceFrequency(&fq);
    g_qpcFreq = (uint64_t)fq.QuadPart;
    g_usPerTick = 1e6 / (double)fq.QuadPart;
    wcsncpy_s(g_iniDir, info.iniPath, _TRUNCATE);
    if (wchar_t* sl = wcsrchr(g_iniDir, L'\\')) sl[1] = 0;
    else g_iniDir[0] = 0;
    if (HMODULE nt = GetModuleHandleW(L"ntdll.dll")) {
        g_ntQif = reinterpret_cast<NtQifFn>(GetProcAddress(nt, "NtQueryInformationFile"));
        g_ntReset = reinterpret_cast<NtResetFn>(GetProcAddress(nt, "NtResetEvent"));
    }
    HMODULE kb = GetModuleHandleW(L"kernelbase.dll");
    if (!kb) kb = GetModuleHandleW(L"kernel32.dll");
    if (kb) g_getDesc = reinterpret_cast<GetThrDescFn>(GetProcAddress(kb, "GetThreadDescription"));
    if (ep) {
        const uint8_t* a = (const uint8_t*)(g_base + kEvArgs);
        const uint8_t* w = (const uint8_t*)(g_base + kFreeWaitCall);
        int32_t rel = 0;
        memcpy(&rel, w + 1, 4);
        const char* why = nullptr;
        if (memcmp(a, kEvArgBytes, sizeof(kEvArgBytes)) != 0) why = "CreateEventA arguments differ";
        else if (w[0] != 0xE8 || (uintptr_t)(w + 5 + rel) != g_base + kWaitAll) why = "FileReqFree does not call FileReqWaitAll";
        else if (!g_ntReset) why = "no NtResetEvent";
        if (why) {
            g_log("EP: off (%s)", why);
            ep = false;
        }
    }
    if (!rp && !ep && !fixWanted && !pfWanted) return false;
    const auto* dos = (const IMAGE_DOS_HEADER*)g_base;
    const auto* nth = (const IMAGE_NT_HEADERS64*)(g_base + dos->e_lfanew);
    g_thunks = AllocNear(g_base, g_base + nth->OptionalHeader.SizeOfImage);
    if (!g_thunks) {
        g_log("ReadProbe/EP: no executable page within 2 GB of the image - off");
        return false;
    }
    if (rp) {
        g_tls = TlsAlloc();
        if (g_mode >= 2) {
            g_tr = (Rec*)VirtualAlloc(nullptr, kTrN * sizeof(Rec), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            if (!g_tr) g_log("ReadProbe: no 32 MB for the trace ring - mode 1");
        }
        g_lsFeed = true;
        g_rp = true;  // set before the first hook goes live
    }
    g_ep = ep;
    if (fixWanted) {  // readfix needs the thunk page (open/close hooks) before the read sites go live
        readfix::StartInfo fi;
        fi.log = g_log;
        fi.iniPath = info.iniPath;
        fi.base = g_base;
        fi.knownBuild = info.knownBuild;
        fi.hookCall = &HookCallForFix;
        g_fix = readfix::Start(fi);
        if (!rp) g_lsFeed = info.loadStatsOn;  // LoadStats left its read sites to us (WantsReads): keep feeding it
    }
    if (pfWanted) {  // 1.7.26 PF: observes the reads, so it must be live before the read sites are hooked
        prefetch::StartInfo pi;
        pi.log = g_log;
        pi.iniPath = info.iniPath;
        pi.base = g_base;
        pi.knownBuild = info.knownBuild;
        g_pf = prefetch::Start(pi);
        if (!rp) g_lsFeed = info.loadStatsOn;
    }
    char bad[400] = {0};
    size_t bl = 0;
    auto note = [&](const char* what, uint32_t rva, int r) {
        if (r && bl < sizeof(bad) - 40) bl += (size_t)snprintf(bad + bl, sizeof(bad) - bl, " %s %X(%s)", what, rva, r == 1 ? "bytes" : "atomic");
        return r == 0;
    };
    int nRead = 0, nLs = 0, nEv = 0;
    bool cacheOk = false;
    if (rp || fixWanted || g_pf) {
        for (int s = 0; s < kNSites; ++s) {
            const bool ok = note("read", kRS[s].rva, HookIatCall(kRS[s].rva, s == kNSites - 1 ? kIatReadFileEx : kIatReadFile, kReadHooks[s]));
            nRead += ok;
            nLs += ok && kRS[s].ls >= 0;
        }
        if (!rp) g_log("ReadFix: %d/%d read sites hooked (%d/7 LoadStats sites; fixes %s, prefetch %s)%s%s", nRead, kNSites, nLs,
                       g_fix ? "on" : "OFF", g_pf ? "on" : "OFF", bl ? " - skipped:" : "", bad);
    }
    if (rp) {
        nEv += note("WFMO", kWfmo, HookIatCall(kWfmo, kIatWfmo, (const void*)&HkWfmo));
        cacheOk = note("cache", kCacheCall, HookCacheCall());
    }
    bool crOk = false, clOk = false;
    if (rp || g_ep) {
        crOk = note("CreateEventA", kEvCreate, HookIatCall(kEvCreate, kIatCreateEventA, (const void*)&HkCreateEventA));
        clOk = note("CloseHandle", kEvClose, HookIatCall(kEvClose, kIatCloseHandle, (const void*)&HkCloseEvent));
    }
    nEv += crOk + clOk;
    if (g_ep && !(crOk && clOk)) {
        // one half alone is still exact (pool empty -> real create; nothing returned -> real close), but useless: say so
        g_log("EP: event hooks incomplete (create %d, close %d) - pool inactive", (int)crOk, (int)clOk);
    }
    if (g_ep) g_log("EP: event pool on (FileReqAlloc CreateEventA 311EE1 / FileReqFree CloseHandle 311F89, cap %d, signalled events only)", kEpCap);
    if (rp) {
        HANDLE th = CreateThread(nullptr, 0, &Sampler, nullptr, 0, nullptr);
        if (th) CloseHandle(th);
        g_log("ReadProbe: mode %d, %d/%d read sites (%d/7 LoadStats sites%s), events %d/3, game cache call %s%s%s", g_mode, nRead, kNSites,
              nLs, nLs < 7 ? " - LoadStats read counters incomplete" : "", nEv, cacheOk ? "hooked" : "not hooked",
              bl ? " - skipped:" : "", bad);
    } else if (bl && g_ep) {
        g_log("EP: skipped:%s", bad);
    }
    return true;
}

}  // namespace readprobe
