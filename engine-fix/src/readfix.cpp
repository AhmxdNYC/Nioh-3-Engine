// readfix.cpp - RC read cache, AQ async archive reads, TSY TrySync threads, SH synchronous archive handles. All default
// off; contract and safety argument in readfix.h, design in notes\loading-ally-fixed.md 8.3 / 8.7.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <new>

#include "readfix.h"

namespace readfix {
namespace {

// ---- v2.02 addresses (RVAs) ----
constexpr uint32_t kGameMainPtr = 0x4759910, kGm58A = 0x58A;
constexpr uint32_t kIatReadFile = 0x38E23B8, kIatCreateFileW = 0x38E2378, kIatCloseHandle = 0x38E2390;
constexpr uint32_t kOpenSite = 0x314199;   // FileOpenWin32 314144: CreateFileW (7 args)
constexpr uint32_t kCloseSite = 0x313AE6;  // file close: CloseHandle(old [f+18h]) after `or [f+18h],-1`
constexpr uint32_t kPoolCount = 0x4B723B4, kPoolWeight = 0x4B723B8;  // pool 9 (TrySync) entry of PoolTable 4B722D0
constexpr uint32_t kPoolBase = 0x4B72A10;                            // PoolTable+740h: base written by SizeWorkerPools
constexpr uint32_t kShSite = 0x314181;                               // FileOpenWin32: and eax,200h
constexpr uint8_t kShOrig[5] = {0x25, 0x00, 0x02, 0x00, 0x00};

// readprobe's site index -> what readfix may do there. RC: the 6 archive sites (not 1FD9CA3 save data, not the other
// subsystems). AQ: the 3 overlapped submit sites. Lazy classification: the layer sites whose handles FileOpenWin32 made.
constexpr uint32_t kRcSites = (1u << 0) | (1u << 1) | (1u << 2) | (1u << 3) | (1u << 4) | (1u << 6);
constexpr uint32_t kAqSites = (1u << 2) | (1u << 3) | (1u << 4);
constexpr uint32_t kLazySites = (1u << 0) | (1u << 1) | (1u << 2) | (1u << 3) | (1u << 4);

using ReadFn = BOOL(WINAPI*)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
using CreateFileWFn = HANDLE(WINAPI*)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
using CloseFn = BOOL(WINAPI*)(HANDLE);
struct IoSb {
    union {
        LONG Status;
        PVOID Pointer;
    };
    ULONG_PTR Information;
};
using NtQifFn = LONG(WINAPI*)(HANDLE, IoSb*, PVOID, ULONG, ULONG);

void (*g_log)(const char*, ...) = nullptr;
uintptr_t g_base = 0;
NtQifFn g_ntQif = nullptr;
wchar_t g_pkg[1024] = {0};  // final path of <game>\package\ (with the trailing backslash)
size_t g_pkgLen = 0;

int g_rcMode = 0, g_aqMode = 0, g_aqThreads = 4, g_srMode = 0;
bool g_priv = false;  // open a private synchronous handle per eligible overlapped game handle (SR, RC block loads, AQ)
uint32_t g_blkShift = 16, g_maxRead = 64 * 1024;
uint32_t g_nBlk = 0;

std::atomic<bool> g_live{false};      // Read() routes through the fixes
std::atomic<bool> g_rcActive{false};  // RC serves (mode 2: always; mode 1: inside a load window)
std::atomic<bool> g_aqActive{false};  // AQ queues (same rule)
std::atomic<bool> g_srActive{false};  // SR serves (same rule)

inline ReadFn RealRead() { return *reinterpret_cast<ReadFn*>(g_base + kIatReadFile); }

uint64_t Qpc() {
    LARGE_INTEGER q;
    QueryPerformanceCounter(&q);
    return (uint64_t)q.QuadPart;
}
double g_sPerTick = 0;

bool SafeCopy(void* dst, const void* src, size_t n) {
    __try {
        memcpy(dst, src, n);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// ---- counters ----
enum C { cSr, cSrB, cSrT, cSrPass, cPriv, cPrivFail, cElig, cHit, cHitB, cMiss, cBlk, cBlkB, cBlkT, cBlkFail, cPassBig, cPassEof, cPassInel, cPassBusy, cAqQ, cAqT, cAqFull,
         cAqDirect, cClsOk, cClsNo, cN };
std::atomic<uint64_t> g_c[cN];
std::atomic<uint32_t> g_aqMaxDepth{0};
inline void Add(C c, uint64_t v = 1) { g_c[c].fetch_add(v, std::memory_order_relaxed); }

// ---- per-thread event for our own reads (manual-reset, made once per thread) ----
DWORD g_tls = TLS_OUT_OF_INDEXES;
HANDLE ThreadEvent() {
    if (g_tls == TLS_OUT_OF_INDEXES) return nullptr;
    HANDLE e = TlsGetValue(g_tls);
    if (!e) {
        e = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        TlsSetValue(g_tls, e);
    }
    return e;
}

// A positioned read with our own OVERLAPPED; waits for completion. Returns the NTSTATUS-style result in *st.
bool OwnRead(HANDLE h, uint64_t off, void* dst, DWORD n, DWORD* got, LONG* st) {
    OVERLAPPED o{};
    o.Offset = (DWORD)off;
    o.OffsetHigh = (DWORD)(off >> 32);
    o.hEvent = ThreadEvent();
    o.Internal = (ULONG_PTR)(LONG)0xC0000001;  // STATUS_UNSUCCESSFUL unless the read writes its status
    DWORD g = 0;
    BOOL r = RealRead()(h, dst, n, nullptr, &o);
    if (!r && GetLastError() == ERROR_IO_PENDING) r = GetOverlappedResult(h, &o, &g, TRUE);
    else if (r) g = (DWORD)o.InternalHigh;
    *got = g;
    *st = r ? (LONG)o.Internal : ((LONG)o.Internal ? (LONG)o.Internal : (LONG)0xC0000001);
    if (r && *st == 0x103) *st = 0;  // completed: never report STATUS_PENDING back
    return r != FALSE;
}

// A positioned read on the private synchronous handle: returns true only when all n bytes arrived.
bool PrivRead(HANDLE p, uint64_t off, void* dst, DWORD n) {
    OVERLAPPED o{};
    o.Offset = (DWORD)off;
    o.OffsetHigh = (DWORD)(off >> 32);
    DWORD g = 0;
    return ReadFile(p, dst, n, &g, &o) && g == n;
}

// ---- handle table: 8192 slots, open addressing, never shrinks; an entry is reset by the open/close hooks ----
enum HState : uint32_t { hUnknown = 0, hElig = 2, hInel = 3 };
struct HEnt {
    std::atomic<uintptr_t> h;
    std::atomic<uint32_t> state;
    std::atomic<uint32_t> lk;
    uint32_t fid;
    uint8_t sync, fromOpen, roOpen;
    uint64_t size;
    HANDLE priv;  // private synchronous handle to the same file (or nullptr), closed when the entry is reset
};
constexpr int kHBits = 13, kHSlots = 1 << kHBits;
HEnt g_h[kHSlots];
std::atomic<uint32_t> g_fid{0};
std::atomic<int> g_logged{0};

HEnt* FindH(HANDLE h, bool insert) {
    const uintptr_t v = (uintptr_t)h;
    uint32_t i = (uint32_t)(((v >> 2) * 0x9E3779B97F4A7C15ull) >> (64 - kHBits));
    for (int k = 0; k < 64; ++k, i = (i + 1) & (kHSlots - 1)) {
        uintptr_t cur = g_h[i].h.load(std::memory_order_acquire);
        if (cur == v) return &g_h[i];
        if (!cur) {
            if (!insert) return nullptr;
            if (g_h[i].h.compare_exchange_strong(cur, v, std::memory_order_acq_rel) || cur == v) return &g_h[i];
        }
    }
    return nullptr;
}
bool TryLock(HEnt& e) {
    uint32_t z = 0;
    return e.lk.compare_exchange_strong(z, 1, std::memory_order_acquire);
}
void SpinLock(HEnt& e) {
    while (!TryLock(e)) YieldProcessor();
}
void Unlock(HEnt& e) { e.lk.store(0, std::memory_order_release); }

void ResetHandle(HANDLE h, bool open, bool roOpen) {
    HEnt* e = FindH(h, open);
    if (!e) return;
    SpinLock(*e);
    e->state.store(hUnknown, std::memory_order_release);
    e->fromOpen = open;
    e->roOpen = open && roOpen;
    HANDLE p = e->priv;
    e->priv = nullptr;
    Unlock(*e);
    if (p) CloseHandle(p);
}

// Called with the entry locked and state == hUnknown.
void Classify(HEnt& e, HANDLE h, bool lazy) {
    const char* why = nullptr;
    ULONG acc = 0, mode = 0;
    IoSb sb{};
    LARGE_INTEGER sz{};
    wchar_t path[1024];
    path[0] = 0;
    // package\ holds the game's immutable archives; a FileOpenWin32 read open (GENERIC_READ, share READ) is read-only.
    // If FileAccessInformation is not available (older Wine), a package file is still accepted: nothing writes there.
    bool accKnown = false;
    if (g_ntQif && g_ntQif(h, &sb, &acc, sizeof(acc), 8 /*FileAccessInformation*/) >= 0) {
        accKnown = true;
        if (acc & (FILE_WRITE_DATA | FILE_APPEND_DATA)) why = "writable";
    }
    if (!why && (!g_ntQif || g_ntQif(h, &sb, &mode, sizeof(mode), 16 /*FileModeInformation*/) < 0)) why = "mode unknown";
    if (!why && !GetFileSizeEx(h, &sz)) why = "no size";
    if (!why) {
        const DWORD n = GetFinalPathNameByHandleW(h, path, 1024, 0);
        if (!n || n >= 1024) why = "no path";
        else if (_wcsnicmp(path, g_pkg, g_pkgLen) != 0) why = "outside package";
    }
    if (!why) {
        e.fid = g_fid.fetch_add(1, std::memory_order_relaxed) + 1;
        e.sync = (mode & 0x30) != 0;  // FILE_SYNCHRONOUS_IO_ALERT | _NONALERT
        e.size = (uint64_t)sz.QuadPart;
        if (g_priv && !e.sync) {  // same file, synchronous, read-only, sharing like the game's own handles
            HANDLE p = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (p == INVALID_HANDLE_VALUE) {
                p = nullptr;
                Add(cPrivFail);
            } else {
                Add(cPriv);
            }
            e.priv = p;
        }
        e.state.store(hElig, std::memory_order_release);
        Add(cClsOk);
    } else {
        e.state.store(hInel, std::memory_order_release);
        Add(cClsNo);
    }
    if (g_log && g_logged.fetch_add(1, std::memory_order_relaxed) < 12) {
        char p8[200];
        WideCharToMultiByte(CP_UTF8, 0, path[0] ? path : L"?", -1, p8, sizeof(p8), nullptr, nullptr);
        g_log("ReadFix: %s handle %s - %s (%s%s%s, %llu MB)", lazy ? "lazy" : "opened", p8, why ? "not served" : "eligible",
              why ? why : (e.sync ? "synchronous" : "overlapped"), why || accKnown ? "" : ", access unknown",
              !why && e.priv ? ", private sync handle" : "", (unsigned long long)(sz.QuadPart >> 20));
    }
}

struct FileInfo {
    uint32_t fid;
    bool sync;
    uint64_t size;
    HANDLE priv;
};
bool Take(const HEnt& e, int site, FileInfo& fi) {
    if (site == 6 && !e.fromOpen) return false;  // FileStream handles: only those FileOpenWin32 made after the hooks
    fi.fid = e.fid;
    fi.sync = e.sync != 0;
    fi.size = e.size;
    fi.priv = e.priv;
    return true;
}
// Fills fi for an eligible handle. Never blocks on another thread's classification (that read just passes through).
bool Eligible(HANDLE h, int site, FileInfo& fi) {
    HEnt* e = FindH(h, false);
    if (e) {
        const uint32_t st = e->state.load(std::memory_order_acquire);
        if (st == hElig) return Take(*e, site, fi);
        if (st == hInel) return false;
    }
    if (!((kLazySites >> site) & 1) && !(e && e->fromOpen)) return false;
    if (!e) e = FindH(h, true);
    if (!e || !TryLock(*e)) return false;
    if (e->state.load(std::memory_order_relaxed) == hUnknown) Classify(*e, h, !e->fromOpen);
    const bool ok = e->state.load(std::memory_order_relaxed) == hElig && Take(*e, site, fi);
    Unlock(*e);
    return ok;
}

// ---- RC block cache (one SRW lock: ~7k reads/s is far below where it matters) ----
enum BState : uint8_t { bFree = 0, bLoading = 1, bReady = 2, bFailed = 3 };
struct Blk {
    uint64_t key;
    std::atomic<int32_t> pins;
    std::atomic<uint8_t> state;
    std::atomic<uint8_t> ref;
    uint32_t len;
    int32_t next;
};
Blk* g_blk = nullptr;
int32_t* g_head = nullptr;
uint32_t g_nBucket = 0, g_hand = 0;
uint8_t* g_mem = nullptr;
bool g_committed = false;  // under g_lock
SRWLOCK g_lock = SRWLOCK_INIT;
CONDITION_VARIABLE g_cv = CONDITION_VARIABLE_INIT;

inline uint32_t Bucket(uint64_t k) { return (uint32_t)((k * 0x9E3779B97F4A7C15ull) >> 32) & (g_nBucket - 1); }
inline uint64_t Key(uint32_t fid, uint64_t blk) { return (uint64_t)fid << 36 | (blk & ((1ull << 36) - 1)); }
inline uint8_t* Mem(int32_t i) { return g_mem + ((size_t)i << g_blkShift); }

int32_t FindB(uint64_t k) {
    for (int32_t i = g_head[Bucket(k)]; i >= 0; i = g_blk[i].next)
        if (g_blk[i].key == k) return i;
    return -1;
}
void LinkB(int32_t i) {
    const uint32_t b = Bucket(g_blk[i].key);
    g_blk[i].next = g_head[b];
    g_head[b] = i;
}
void UnlinkB(int32_t i) {
    int32_t* p = &g_head[Bucket(g_blk[i].key)];
    while (*p >= 0 && *p != i) p = &g_blk[*p].next;
    if (*p == i) *p = g_blk[i].next;
    g_blk[i].next = -1;
}
// exclusive lock held. Free, failed-and-unpinned, or ready-unpinned-unreferenced (CLOCK second chance).
int32_t Victim() {
    for (uint32_t s = 0; s < 2 * g_nBlk; ++s) {
        const int32_t i = (int32_t)g_hand;
        g_hand = g_hand + 1 == g_nBlk ? 0 : g_hand + 1;
        Blk& b = g_blk[i];
        const uint8_t st = b.state.load(std::memory_order_relaxed);
        if (st == bFree) return i;
        if (st == bLoading || b.pins.load(std::memory_order_relaxed) != 0) continue;
        if (st == bReady && b.ref.load(std::memory_order_relaxed)) {
            b.ref.store(0, std::memory_order_relaxed);
            continue;
        }
        if (st == bReady) UnlinkB(i);
        b.state.store(bFree, std::memory_order_relaxed);
        return i;
    }
    return -1;
}
inline void Unpin(int32_t i) { g_blk[i].pins.fetch_sub(1, std::memory_order_release); }

int32_t WaitReady(int32_t i) {
    uint8_t st = g_blk[i].state.load(std::memory_order_acquire);
    if (st == bLoading) {
        AcquireSRWLockShared(&g_lock);
        while ((st = g_blk[i].state.load(std::memory_order_acquire)) == bLoading)
            SleepConditionVariableSRW(&g_cv, &g_lock, INFINITE, CONDITION_VARIABLE_LOCKMODE_SHARED);
        ReleaseSRWLockShared(&g_lock);
    }
    if (st == bReady) {
        g_blk[i].ref.store(1, std::memory_order_relaxed);
        return i;
    }
    Unpin(i);
    return -1;
}

// Pinned ready block or -1 (not cached and !load, no victim, cache off, or the block read failed).
int32_t Acquire(const FileInfo& e, HANDLE h, uint64_t blk, bool load, bool* missed) {
    const uint64_t k = Key(e.fid, blk);
    AcquireSRWLockShared(&g_lock);
    int32_t i = g_committed ? FindB(k) : -1;
    const bool on = g_committed;
    if (i >= 0) g_blk[i].pins.fetch_add(1, std::memory_order_acquire);
    ReleaseSRWLockShared(&g_lock);
    if (i >= 0) return WaitReady(i);
    if (!on) return -1;
    *missed = true;
    if (!load) return -1;
    AcquireSRWLockExclusive(&g_lock);
    if (!g_committed) {
        ReleaseSRWLockExclusive(&g_lock);
        return -1;
    }
    i = FindB(k);
    if (i >= 0) {
        g_blk[i].pins.fetch_add(1, std::memory_order_acquire);
        ReleaseSRWLockExclusive(&g_lock);
        return WaitReady(i);
    }
    i = Victim();
    if (i < 0) {
        ReleaseSRWLockExclusive(&g_lock);
        Add(cPassBusy);
        return -1;
    }
    Blk& b = g_blk[i];
    b.key = k;
    b.pins.store(1, std::memory_order_relaxed);
    b.ref.store(1, std::memory_order_relaxed);
    b.state.store(bLoading, std::memory_order_relaxed);
    LinkB(i);
    ReleaseSRWLockExclusive(&g_lock);
    const uint64_t off = blk << g_blkShift;
    const uint64_t left = e.size - off;
    const DWORD want = (DWORD)(left < (1ull << g_blkShift) ? left : (1ull << g_blkShift));
    DWORD got = 0;
    LONG st = 0;
    const uint64_t t0 = Qpc();
    const DWORD le = GetLastError();
    bool ok;
    if (e.priv) {
        ok = PrivRead(e.priv, off, Mem(i), want);
        got = ok ? want : 0;
    } else {
        ok = OwnRead(h, off, Mem(i), want, &got, &st) && got == want;
    }
    SetLastError(le);
    Add(cBlk);
    Add(cBlkB, got);
    Add(cBlkT, Qpc() - t0);
    AcquireSRWLockExclusive(&g_lock);
    if (ok) {
        b.len = want;
        b.state.store(bReady, std::memory_order_release);
    } else {
        UnlinkB(i);
        b.state.store(bFailed, std::memory_order_release);
        Add(cBlkFail);
    }
    ReleaseSRWLockExclusive(&g_lock);
    WakeAllConditionVariable(&g_cv);
    if (!ok) {
        Unpin(i);
        return -1;
    }
    return i;
}

enum Serve { sServed, sMiss, sPass };
// Copies [off, off+n) of an eligible file into buf from the cache. sMiss = not cached and !load (or load failed).
Serve RcServe(const FileInfo& e, HANDLE h, uint64_t off, DWORD n, void* buf, bool load) {
    if (!g_mem) return sPass;
    if (n == 0 || n > g_maxRead) {
        Add(cPassBig);
        return sPass;
    }
    if (off >= e.size || e.size - off < n) {
        Add(cPassEof);
        return sPass;
    }
    const uint64_t b0 = off >> g_blkShift, b1 = (off + n - 1) >> g_blkShift;
    bool missed = false;
    const int32_t i0 = Acquire(e, h, b0, load, &missed);
    if (i0 < 0) return sMiss;
    int32_t i1 = -1;
    if (b1 != b0) {
        i1 = Acquire(e, h, b1, load, &missed);
        if (i1 < 0) {
            Unpin(i0);
            return sMiss;
        }
    }
    const uint64_t in0 = off - (b0 << g_blkShift);
    const DWORD part0 = (DWORD)(b1 != b0 ? (1ull << g_blkShift) - in0 : n);
    bool ok = SafeCopy(buf, Mem(i0) + in0, part0);
    if (ok && i1 >= 0) ok = SafeCopy((uint8_t*)buf + part0, Mem(i1), n - part0);
    Unpin(i0);
    if (i1 >= 0) Unpin(i1);
    if (!ok) return sPass;  // bad buffer: the real call returns the error (ERROR_NOACCESS) itself
    if (missed) Add(cMiss);
    else Add(cHit);
    Add(cHitB, n);
    return sServed;
}

// Completes a served read exactly like a synchronous ReadFile success.
BOOL CompleteSync(HANDLE h, const FileInfo& e, uint64_t off, DWORD n, LPDWORD got, LPOVERLAPPED ov, DWORD le0) {
    if (got) *got = n;
    ov->Internal = 0;
    ov->InternalHigh = n;
    if (e.sync) {
        LARGE_INTEGER p;
        p.QuadPart = (LONGLONG)(off + n);
        SetFilePointerEx(h, p, nullptr, FILE_BEGIN);
    }
    if (ov->hEvent) SetEvent((HANDLE)((uintptr_t)ov->hEvent & ~(uintptr_t)1));
    SetLastError(le0);
    return TRUE;
}

// ---- AQ ----
struct Req {
    HANDLE h;
    void* buf;
    DWORD n;
    LPOVERLAPPED ov;
    HANDLE ev;
    uint64_t off;
    FileInfo e;
};
constexpr uint32_t kQ = 4096;
Req g_q[kQ];
uint32_t g_qHead = 0, g_qN = 0;
SRWLOCK g_qLock = SRWLOCK_INIT;
CONDITION_VARIABLE g_qCv = CONDITION_VARIABLE_INIT;

bool Enqueue(const Req& r) {
    AcquireSRWLockExclusive(&g_qLock);
    if (g_qN == kQ) {
        ReleaseSRWLockExclusive(&g_qLock);
        Add(cAqFull);
        return false;
    }
    g_q[(g_qHead + g_qN) % kQ] = r;
    ++g_qN;
    const uint32_t d = g_qN;
    ReleaseSRWLockExclusive(&g_qLock);
    WakeConditionVariable(&g_qCv);
    uint32_t m = g_aqMaxDepth.load(std::memory_order_relaxed);
    while (d > m && !g_aqMaxDepth.compare_exchange_weak(m, d, std::memory_order_relaxed)) {}
    Add(cAqQ);
    return true;
}

DWORD WINAPI AqWorker(LPVOID) {
    for (;;) {
        AcquireSRWLockExclusive(&g_qLock);
        while (!g_qN) SleepConditionVariableSRW(&g_qCv, &g_qLock, INFINITE, 0);
        const Req r = g_q[g_qHead];
        g_qHead = (g_qHead + 1) % kQ;
        --g_qN;
        ReleaseSRWLockExclusive(&g_qLock);
        const uint64_t t0 = Qpc();
        DWORD got = 0;
        LONG st = 0;
        if (g_rcMode && RcServe(r.e, r.h, r.off, r.n, r.buf, true) == sServed) {
            got = r.n;
        } else {
            if (r.e.priv && PrivRead(r.e.priv, r.off, r.buf, r.n)) got = r.n;
            else OwnRead(r.h, r.off, r.buf, r.n, &got, &st);
            Add(cAqDirect);
        }
        r.ov->InternalHigh = got;
        r.ov->Internal = (ULONG_PTR)st;
        SetEvent(r.ev);  // last touch of the game's OVERLAPPED: the game may free it once its WFMO returns
        Add(cAqT, Qpc() - t0);
    }
}

// ---- open / close hooks (through readprobe's save thunks) ----
HANDLE WINAPI HkCreateFileW(LPCWSTR name, DWORD acc, DWORD share, LPSECURITY_ATTRIBUTES sa, DWORD disp, DWORD flags, HANDLE tmpl) {
    const HANDLE h = (*reinterpret_cast<CreateFileWFn*>(g_base + kIatCreateFileW))(name, acc, share, sa, disp, flags, tmpl);
    if (h && h != INVALID_HANDLE_VALUE) {
        const DWORD le = GetLastError();
        ResetHandle(h, true, acc == GENERIC_READ && !(share & (FILE_SHARE_WRITE | FILE_SHARE_DELETE)));
        SetLastError(le);
    }
    return h;
}
BOOL WINAPI HkCloseHandle(HANDLE h) {
    ResetHandle(h, false, false);
    return (*reinterpret_cast<CloseFn*>(g_base + kIatCloseHandle))(h);
}

// ---- gate: load windows (G+58Ah), RC commit / decommit, per-window log ----
bool ReadGm58A(bool& on) {
    uintptr_t gm = 0;
    if (!SafeCopy(&gm, (const void*)(g_base + kGameMainPtr), 8) || gm < 0x10000) return false;
    uint8_t b = 0;
    if (!SafeCopy(&b, (const void*)(gm + kGm58A), 1)) return false;
    on = b != 0;
    return true;
}

bool Commit(bool on) {  // RC memory; exclusive lock; false = blocks still in use (decommit retried later)
    if (!g_mem) return true;
    AcquireSRWLockExclusive(&g_lock);
    bool ok = true;
    if (on && !g_committed) {
        ok = VirtualAlloc(g_mem, (size_t)g_nBlk << g_blkShift, MEM_COMMIT, PAGE_READWRITE) != nullptr;
        g_committed = ok;
    } else if (!on && g_committed) {
        for (uint32_t i = 0; i < g_nBlk && ok; ++i)
            if (g_blk[i].pins.load() != 0 || g_blk[i].state.load() == bLoading) ok = false;
        if (ok) {
            for (uint32_t i = 0; i < g_nBlk; ++i) {
                g_blk[i].state.store(bFree);
                g_blk[i].ref.store(0);
                g_blk[i].next = -1;
            }
            for (uint32_t b = 0; b < g_nBucket; ++b) g_head[b] = -1;
            VirtualFree(g_mem, (size_t)g_nBlk << g_blkShift, MEM_DECOMMIT);
            g_committed = false;
        }
    }
    ReleaseSRWLockExclusive(&g_lock);
    return ok;
}

uint64_t g_w0[cN];
void LogWindow(int n, double secs) {
    uint64_t d[cN];
    for (int i = 0; i < cN; ++i) {
        const uint64_t v = g_c[i].load(std::memory_order_relaxed);
        d[i] = v - g_w0[i];
        g_w0[i] = v;
    }
    const double served = (double)(d[cHit] + d[cMiss]);
    const double all = served + (double)(d[cPassBig] + d[cPassEof] + d[cPassInel] + d[cPassBusy]);
    g_log("ReadFix #%d SR: %llu reads served (%.0f MB, %.2f s in the private reads), %llu passed to the real call | private handles %llu "
          "opened, %llu failed", n, (unsigned long long)d[cSr], (double)d[cSrB] / 1048576.0,
          (double)d[cSrT] * g_sPerTick, (unsigned long long)d[cSrPass],
          (unsigned long long)d[cPriv], (unsigned long long)d[cPrivFail]);
    g_log("ReadFix #%d (%.1f s): RC served %llu of %llu routed (%.1f %%; %llu hits, %llu via a block read) %.0f MB | blocks %llu read "
          "%.0f MB %.2f s, %llu failed | pass: big %llu eof %llu not-eligible %llu no-block %llu | AQ queued %llu (max depth %u, "
          "%llu full) workers %.2f s, %llu direct | files: %llu eligible, %llu not",
          n, secs, (unsigned long long)served, (unsigned long long)all, all > 0 ? 100.0 * served / all : 0.0,
          (unsigned long long)d[cHit], (unsigned long long)d[cMiss], (double)d[cHitB] / 1048576.0, (unsigned long long)d[cBlk],
          (double)d[cBlkB] / 1048576.0, (double)d[cBlkT] * g_sPerTick, (unsigned long long)d[cBlkFail], (unsigned long long)d[cPassBig],
          (unsigned long long)d[cPassEof], (unsigned long long)d[cPassInel], (unsigned long long)d[cPassBusy], (unsigned long long)d[cAqQ],
          g_aqMaxDepth.exchange(0), (unsigned long long)d[cAqFull], (double)d[cAqT] * g_sPerTick, (unsigned long long)d[cAqDirect],
          (unsigned long long)d[cClsOk], (unsigned long long)d[cClsNo]);
}

DWORD WINAPI Gate(LPVOID) {
    bool was = false;
    int n = 0;
    uint64_t tOn = 0, tOff = 0;
    bool decommitPending = false;
    for (;;) {
        Sleep(10);
        bool on = false;
        if (!ReadGm58A(on)) on = false;
        const uint64_t now = Qpc();
        if (on && !was) {
            ++n;
            tOn = now;
            decommitPending = false;
            if (g_rcMode == 1 && !Commit(true)) g_log("ReadFix: RC could not commit %u MB - cache off for this load", (g_nBlk << g_blkShift) >> 20);
            if (g_rcMode == 1) g_rcActive.store(true, std::memory_order_release);
            if (g_aqMode == 1) g_aqActive.store(true, std::memory_order_release);
            if (g_srMode == 1) g_srActive.store(true, std::memory_order_release);
            g_log("ReadFix: load window #%d - fixes on", n);
        } else if (!on && was) {
            if (g_rcMode == 1) g_rcActive.store(false, std::memory_order_release);
            if (g_aqMode == 1) g_aqActive.store(false, std::memory_order_release);
            if (g_srMode == 1) g_srActive.store(false, std::memory_order_release);
            tOff = now;
            decommitPending = g_rcMode == 1;
            LogWindow(n, (double)(now - tOn) * g_sPerTick);
        }
        if (decommitPending && (double)(now - tOff) * g_sPerTick >= 10.0 && Commit(false)) {
            decommitPending = false;
            g_log("ReadFix: RC cache released (10 s after load window #%d)", n);
        }
        was = on;
    }
}

bool g_started = false;

// harness entry: everything Start does except the ini, the image checks, the hooks and the gate thread
bool Init(int rcMode, int rcMB, int rcKB, int rcMaxKB, int aqMode, int aqThreads, int srMode, bool priv, const wchar_t* pkgFinalPath) {
    LARGE_INTEGER fq;
    QueryPerformanceFrequency(&fq);
    g_sPerTick = 1.0 / (double)fq.QuadPart;
    if (HMODULE nt = GetModuleHandleW(L"ntdll.dll")) g_ntQif = reinterpret_cast<NtQifFn>(GetProcAddress(nt, "NtQueryInformationFile"));
    wcsncpy_s(g_pkg, pkgFinalPath, _TRUNCATE);
    g_pkgLen = wcslen(g_pkg);
    if (!g_pkgLen) return false;
    if (g_pkg[g_pkgLen - 1] != L'\\' && g_pkgLen + 1 < 1024) {
        g_pkg[g_pkgLen++] = L'\\';
        g_pkg[g_pkgLen] = 0;
    }
    g_tls = TlsAlloc();
    g_rcMode = rcMode;
    g_aqMode = aqMode;
    g_srMode = srMode;
    g_priv = priv || srMode;
    if (rcMode) {
        uint32_t sh = 14;
        while ((1u << sh) < (uint32_t)rcKB * 1024u && sh < 20) ++sh;
        g_blkShift = sh;
        g_maxRead = (uint32_t)rcMaxKB * 1024u;
        if (g_maxRead > (1u << g_blkShift)) g_maxRead = 1u << g_blkShift;
        g_nBlk = (uint32_t)(((uint64_t)rcMB << 20) >> g_blkShift);
        g_nBucket = 1;
        while (g_nBucket < 2 * g_nBlk) g_nBucket <<= 1;
        g_blk = new (std::nothrow) Blk[g_nBlk];
        g_head = new (std::nothrow) int32_t[g_nBucket];
        g_mem = (uint8_t*)VirtualAlloc(nullptr, (size_t)g_nBlk << g_blkShift, MEM_RESERVE, PAGE_READWRITE);
        if (!g_blk || !g_head || !g_mem) return false;
        for (uint32_t i = 0; i < g_nBlk; ++i) {
            g_blk[i].key = 0;
            g_blk[i].pins.store(0);
            g_blk[i].state.store(bFree);
            g_blk[i].ref.store(0);
            g_blk[i].len = 0;
            g_blk[i].next = -1;
        }
        for (uint32_t b = 0; b < g_nBucket; ++b) g_head[b] = -1;
        if (rcMode == 2 && !Commit(true)) return false;
    }
    if (aqMode) {
        g_aqThreads = aqThreads;
        for (int i = 0; i < aqThreads; ++i) {
            HANDLE t = CreateThread(nullptr, 0, &AqWorker, nullptr, 0, nullptr);
            if (!t) return false;
            CloseHandle(t);
        }
    }
    g_rcActive.store(rcMode == 2, std::memory_order_release);
    g_aqActive.store(aqMode == 2, std::memory_order_release);
    g_srActive.store(srMode == 2, std::memory_order_release);
    g_live.store(true, std::memory_order_release);
    return true;
}

int IniInt(const wchar_t* ini, const wchar_t* key, int def, int lo, int hi) {
    int v = (int)GetPrivateProfileIntW(L"Patches", key, def, ini);
    return v < lo ? lo : v > hi ? hi : v;
}

// ---- TSY / SH (boot) ----
int g_tsyWanted = 0;
DWORD WINAPI TsyReport(LPVOID) {
    volatile uint8_t* cnt = (volatile uint8_t*)(g_base + kPoolCount);
    volatile uint8_t* base = (volatile uint8_t*)(g_base + kPoolBase);
    const uint8_t base0 = *base;
    for (int i = 0; i < 1200; ++i) {  // up to 300 s
        if (*cnt != 3 || (base0 == 0 && *base != 0)) break;
        Sleep(250);
    }
    Sleep(500);
    const int c = *cnt;
    if (g_log)
        g_log(c == g_tsyWanted ? "TSY: TrySync pool sized to %d threads (wanted %d) - on"
                               : "TSY: TrySync pool has %d threads (wanted %d) - NOT in effect (pools sized before the write, or a Zen1 divisor)",
              c, g_tsyWanted);
    return 0;
}

}  // namespace

int SrMode(const wchar_t* iniPath, int* iniOut, bool* wineOut) {
    // 1.7.28: SR = -1 auto (default): 1 (load windows) under Wine/Proton, 0 on native Windows; 0/1/2 force
    const int ini = iniPath ? (int)GetPrivateProfileIntW(L"Patches", L"SR", -1, iniPath) : 0;
    HMODULE ntm = GetModuleHandleW(L"ntdll.dll");
    const bool wine = ntm && GetProcAddress(ntm, "wine_get_version");
    if (iniOut) *iniOut = ini;
    if (wineOut) *wineOut = wine;
    if (ini < 0) return wine ? 1 : 0;
    return ini > 2 ? 2 : ini;
}

bool Wants(const wchar_t* iniPath) {
    return iniPath && (GetPrivateProfileIntW(L"Patches", L"RC", 0, iniPath) > 0 || GetPrivateProfileIntW(L"Patches", L"AQ", 0, iniPath) > 0 ||
                       SrMode(iniPath, nullptr, nullptr) > 0);
}

bool Start(const StartInfo& info) {
    g_log = info.log;
    g_base = info.base;
    if (!g_log || !g_base || !info.iniPath || !Wants(info.iniPath)) return false;
    if (!info.knownBuild || !info.hookCall) {
        g_log("ReadFix: refused (unknown build; addresses are v2.02)");
        return false;
    }
    if (g_started) return g_live.load();
    g_started = true;
    const int rc = IniInt(info.iniPath, L"RC", 0, 0, 2);
    const int mb = IniInt(info.iniPath, L"RCMB", 64, 16, 256);
    const int kb = IniInt(info.iniPath, L"RCKB", 64, 16, 1024);
    const int mx = IniInt(info.iniPath, L"RCMaxReadKB", 64, 1, 1024);
    const int aq = IniInt(info.iniPath, L"AQ", 0, 0, 2);
    const int at = IniInt(info.iniPath, L"AQThreads", 4, 1, 16);
    bool wine = false;
    int sr = SrMode(info.iniPath, nullptr, &wine);
    if (sr && !wine) {
        g_log("ReadFix: SR refused - not running under Wine/Proton (Windows overlapped reads are already cheap; a synchronous file "
              "object serializes its I/O)");
        sr = 0;
    }
    if (!rc && !aq && !sr) return false;
    const bool priv = sr || wine;  // under Wine a synchronous read costs ~2 us vs ~55 us overlapped (Ally 1.7.23 calib)
    // <game>\package\ as GetFinalPathNameByHandleW spells it (same normalization as the handles we compare)
    wchar_t dir[MAX_PATH];
    const DWORD dl = GetModuleFileNameW(nullptr, dir, MAX_PATH);
    if (!dl || dl >= MAX_PATH) return false;
    if (wchar_t* sl = wcsrchr(dir, L'\\')) sl[1] = 0;
    wcscat_s(dir, L"package");
    wchar_t fin[1024] = {0};
    HANDLE d = CreateFileW(dir, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (d != INVALID_HANDLE_VALUE) {
        const DWORD n = GetFinalPathNameByHandleW(d, fin, 1024, 0);
        if (!n || n >= 1024) fin[0] = 0;
        CloseHandle(d);
    }
    if (!fin[0]) {
        g_log("ReadFix: no final path for the package folder - off");
        return false;
    }
    if (!Init(rc, mb, kb, mx, aq, at, sr, priv, fin)) {
        g_log("ReadFix: no memory for the %d MB cache or no AQ threads - off", mb);
        g_live.store(false);
        return false;
    }
    // the reuse guards: without both hooks a reused handle value could see another file's blocks -> refuse
    const int ho = info.hookCall(kOpenSite, kIatCreateFileW, (const void*)&HkCreateFileW, 3);
    const int hc = info.hookCall(kCloseSite, kIatCloseHandle, (const void*)&HkCloseHandle, 0);
    if (ho || hc) {
        g_log("ReadFix: open/close hooks failed (CreateFileW 314199: %d, CloseHandle 313AE6: %d) - off", ho, hc);
        g_live.store(false);
        return false;
    }
    if (HANDLE t = CreateThread(nullptr, 0, &Gate, nullptr, 0, nullptr)) CloseHandle(t);
    g_log("ReadFix: SR %d, RC %d (%u MB, %u KiB blocks, reads <= %u KiB), AQ %d (%d threads), private sync handles %s; mode 1 = load "
          "windows only, 2 = always - package %ls", sr, rc, rc ? (g_nBlk << g_blkShift) >> 20 : 0, rc ? (1u << g_blkShift) >> 10 : 0,
          rc ? g_maxRead >> 10 : 0, aq, aq ? g_aqThreads : 0, priv ? "on" : "off", g_pkg);
    return true;
}

int __stdcall Read(int site, void* hv, void* buf, unsigned long n, unsigned long* got, void* ovv) {
    const HANDLE h = (HANDLE)hv;
    const LPOVERLAPPED ov = (LPOVERLAPPED)ovv;
    if (!ov || site < 0 || site > 31 || !((kRcSites >> site) & 1)) return RealRead()(h, buf, n, got, ov);
    const bool rc = g_rcMode && g_rcActive.load(std::memory_order_acquire);
    const bool aqOn = g_aqMode && ((kAqSites >> site) & 1) && g_aqActive.load(std::memory_order_acquire);
    const bool srOn = g_srMode && g_srActive.load(std::memory_order_acquire);
    if (!rc && !aqOn && !srOn) return RealRead()(h, buf, n, got, ov);
    const DWORD le0 = GetLastError();
    FileInfo e;
    if (!Eligible(h, site, e)) {
        Add(cPassInel);
        SetLastError(le0);
        return RealRead()(h, buf, n, got, ov);
    }
    Add(cElig);
    const uint64_t off = (uint64_t)ov->Offset | (uint64_t)ov->OffsetHigh << 32;
    const bool aq = aqOn && !e.sync && ov->hEvent && n && off < e.size && e.size - off >= n;
    const bool sr = srOn && !e.sync && e.priv && n && off < e.size && e.size - off >= n;
    if (rc && RcServe(e, h, off, n, buf, !aq || sr) == sServed) return CompleteSync(h, e, off, n, got, ov, le0);
    if (sr) {  // SR: the same bytes through the private synchronous handle, completed like a synchronous success
        const uint64_t t0 = Qpc();
        const bool ok = PrivRead(e.priv, off, buf, n);
        Add(cSrT, Qpc() - t0);
        if (ok) {
            Add(cSr);
            Add(cSrB, n);
            return CompleteSync(h, e, off, n, got, ov, le0);
        }
        Add(cSrPass);  // any failure: the real call decides (same error semantics)
    }
    if (aq) {
        const HANDLE ev = (HANDLE)((uintptr_t)ov->hEvent & ~(uintptr_t)1);
        ResetEvent(ev);
        ov->Internal = 0x103;  // STATUS_PENDING
        ov->InternalHigh = 0;
        if (got) *got = 0;
        if (Enqueue(Req{h, buf, n, ov, ev, off, e})) {
            SetLastError(ERROR_IO_PENDING);
            return FALSE;
        }
    }
    SetLastError(le0);
    return RealRead()(h, buf, n, got, ov);
}

bool BootWanted(const wchar_t* iniPath, BootCfg& cfg) {
    if (!iniPath) return false;
    const int t = (int)GetPrivateProfileIntW(L"Patches", L"TSY", 0, iniPath);
    cfg.tsy = t <= 0 ? 0 : t < 3 ? 3 : t > 8 ? 8 : t;
    cfg.sh = GetPrivateProfileIntW(L"Patches", L"SH", 0, iniPath) != 0;
    return cfg.tsy || cfg.sh;
}

bool BootTry(uintptr_t base, BootCfg& cfg, int wcCap, unsigned long logicalCpus, void (*log)(const char*, ...)) {
    if (!g_base) g_base = base;
    if (!g_log) g_log = log;
    if (cfg.sh) {
        static bool checked = false;
        if (!checked) {
            checked = true;
            HMODULE nt = GetModuleHandleW(L"ntdll.dll");
            if (!nt || !GetProcAddress(nt, "wine_get_version")) {
                log("SH: refused - not running under Wine/Proton (synchronous file objects serialize I/O on Windows)");
                cfg.sh = false;
            }
        }
    }
    if (cfg.sh) {
        uint8_t* p = (uint8_t*)(base + kShSite);
        if (memcmp(p, kShOrig, sizeof(kShOrig)) == 0) {
            DWORD old, t;
            if (VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old)) {
                p[2] = 0x00;  // and eax,200h -> and eax,0 : no FILE_FLAG_OVERLAPPED on any FileOpenWin32 open
                VirtualProtect(p, 5, old, &t);
                FlushInstructionCache(GetCurrentProcess(), p, 5);
                log("SH: archive handles open synchronous (FileOpenWin32 314181; AQ has no effect on them)");
            } else {
                log("SH: VirtualProtect failed (%lu)", GetLastError());
            }
            cfg.sh = false;
        }
    }
    if (cfg.tsy) {
        volatile uint32_t* cnt = (volatile uint32_t*)(base + kPoolCount);
        volatile int32_t* w = (volatile int32_t*)(base + kPoolWeight);
        uint32_t c = 0;
        int32_t wv = 0;
        if (SafeCopy(&c, (const void*)cnt, 4) && SafeCopy(&wv, (const void*)w, 4) && wv == 4 && (c & 0xFF) == 3) {
            // SizeWorkerPools: base = clamp(logical, 6, 32) (WC: cap instead of 32); pool 9 uses min(base, 12);
            // count = (int)((float)b / (float)13 * (float)weight) (divisor 16 on Zen1: not handled, the report shows it)
            int bse = (int)logicalCpus;
            const int cap = wcCap > 0 ? wcCap : 32;
            bse = bse > cap ? cap : bse < 6 ? 6 : bse;
            const int bi = bse < 12 ? bse : 12;
            int wt = 4;
            while (wt < 64 && (int)((float)bi / 13.0f * (float)wt) < cfg.tsy) ++wt;
            const uint8_t sized = *(volatile uint8_t*)(base + kPoolBase);
            *w = wt;
            g_tsyWanted = (int)((float)bi / 13.0f * (float)wt);
            log("TSY: TrySync pool weight 4 -> %d (pool base %d: %d threads, stock 3)%s", wt, bi, g_tsyWanted,
                sized ? " - the pool base is already set, the pools may be sized already" : "");
            if (HANDLE t = CreateThread(nullptr, 0, &TsyReport, nullptr, 0, nullptr)) CloseHandle(t);
            cfg.tsy = 0;
        }
    }
    return cfg.tsy || cfg.sh;
}

void BootGiveUp(BootCfg& cfg, void (*log)(const char*, ...)) {
    if (cfg.tsy) log("TSY: pool table never held its stock values (weight 4 / count 3) - skipped");
    if (cfg.sh) log("SH: FileOpenWin32 314181 not in its original state after decryption - skipped");
    cfg.tsy = 0;
    cfg.sh = false;
}

}  // namespace readfix
