// fhlinger.cpp - FileLinger ([Patches] FileLinger = 0 by default). Contract, game objects and hooks: fhlinger.h.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>

#include <atomic>
#include <cstdint>
#include <cstring>

#include "fhlinger.h"
#include "gamerva.h"

namespace fhlinger {
namespace {

// ---- v2.02 (RVAs; bytes from the decrypted image, notes/streaming-guard-117226.md) ----
constexpr uint32_t kVtable = 0x38F4E38;    // archive file entry vtable
constexpr uint32_t kAcquire = 0x491B30;    // vt+10h
constexpr uint32_t kRelease = 0x493050;    // vt+28h
constexpr uint32_t kClose = 0x751784;      // close-on-last (handle, in-flight wait, open--)
constexpr uint32_t kSite = 0x4930B5;       // in Release: mov rcx,rdi ; [E8 -> kClose] ; jmp 493083
constexpr uint32_t kBlock = 0x4930B0;      // 16-byte aligned block holding the site (one cmpxchg16b)
const uint8_t kBlockBytes[16] = {0xEB, 0xC5, 0x48, 0x8B, 0xCF, 0xE8, 0xCA, 0xE6, 0x2B, 0x00, 0xEB, 0xC7, 0x33, 0xC9, 0xFF, 0x15};
const uint8_t kCloseBytes[16] = {0x48, 0x89, 0x5C, 0x24, 0x18, 0x48, 0x89, 0x74, 0x24, 0x20, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x48};
const uint8_t kAcqBytes[16] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x57};
const uint8_t kRelBytes[16] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x57};
constexpr uint32_t kOffOwner = 0x08, kOffHandle = 0x10, kOffUsers = 0x2C, kOffSpin = 0x34;
constexpr uint32_t kOffCap = 0x84, kOffOpen = 0x88;

using CloseFn = void(__fastcall*)(uintptr_t entry);
using AcquireFn = uint64_t(__fastcall*)(uintptr_t entry, uint64_t mode);  // al = acquired; rest passed through

void (*g_log)(const char*, ...) = nullptr;
uintptr_t g_vt = 0;
CloseFn g_close = nullptr;
AcquireFn g_acquire = nullptr;
int g_max = 32, g_free = 24;

constexpr int kRing = 64;
SRWLOCK g_lock = SRWLOCK_INIT;
uintptr_t g_ring[kRing];
int g_head = 0, g_count = 0;  // under g_lock

std::atomic<uint64_t> g_kept{0}, g_closedNow{0}, g_evicted{0}, g_evictBusy{0}, g_evictInUse{0}, g_reused{0}, g_capEvict{0};

inline int I32(uintptr_t a) { return *reinterpret_cast<volatile int*>(a); }
inline uintptr_t Ptr(uintptr_t a) { return *reinterpret_cast<volatile uintptr_t*>(a); }

bool IsEntry(uintptr_t e) { return e && Ptr(e) == g_vt; }

// Appends e as the newest; an older copy of e (kept, used again, released again) is taken out first, so the ring holds
// each entry once and its order is least recently released first.
void Push(uintptr_t e) {
    AcquireSRWLockExclusive(&g_lock);
    for (int i = 0; i < g_count; ++i) {
        if (g_ring[(g_head + i) % kRing] != e) continue;
        for (int j = i; j + 1 < g_count; ++j) g_ring[(g_head + j) % kRing] = g_ring[(g_head + j + 1) % kRing];
        --g_count;
        break;
    }
    if (g_count < kRing) {
        g_ring[(g_head + g_count) % kRing] = e;
        ++g_count;
    }
    ReleaseSRWLockExclusive(&g_lock);
}

uintptr_t PopOldest() {
    uintptr_t e = 0;
    AcquireSRWLockExclusive(&g_lock);
    if (g_count) {
        e = g_ring[g_head];
        g_head = (g_head + 1) % kRing;
        --g_count;
    }
    ReleaseSRWLockExclusive(&g_lock);
    return e;
}

int Count() {
    AcquireSRWLockShared(&g_lock);
    const int n = g_count;
    ReleaseSRWLockShared(&g_lock);
    return n;
}

// Close one lingering entry the way the game would have. Never waits on a spinlock. self = an entry whose spinlock the
// caller already holds (skipped: it is the one being kept). Returns true when a handle was closed.
bool EvictOne(uintptr_t self) {
    for (int tries = Count(); tries > 0; --tries) {
        const uintptr_t e = PopOldest();
        if (!e) return false;
        if (e == self || !IsEntry(e)) continue;
        volatile LONG* spin = reinterpret_cast<volatile LONG*>(e + kOffSpin);
        if (InterlockedCompareExchange(spin, 1, 0) != 0) {  // busy: someone is acquiring/releasing it right now
            g_evictBusy.fetch_add(1, std::memory_order_relaxed);
            Push(e);
            continue;
        }
        bool closed = false;
        if (I32(e + kOffUsers) == 0 && Ptr(e + kOffHandle) != 0) {
            g_close(e);
            closed = true;
        } else {
            g_evictInUse.fetch_add(1, std::memory_order_relaxed);  // in use again: it re-enters the ring at its last release
        }
        InterlockedCompareExchange(spin, 0, 1);
        if (closed) {
            g_evicted.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
    }
    return false;
}

// Replaces the call to kClose inside Release: the entry's spinlock is held and its users count just reached 0.
void __fastcall OnLastRelease(uintptr_t e) {
    if (!IsEntry(e) || Ptr(e + kOffHandle) == 0) {
        g_close(e);
        return;
    }
    const uintptr_t owner = Ptr(e + kOffOwner);
    const int cap = owner ? I32(owner + kOffCap) : 0;
    if (cap <= g_free) {  // unknown/small cap: stock behaviour
        g_closedNow.fetch_add(1, std::memory_order_relaxed);
        g_close(e);
        return;
    }
    // room for this one: ring below FileLingerMax and the open count below cap - FileLingerFree; else make room first
    for (int i = 0; i < 2 && (Count() >= g_max || I32(owner + kOffOpen) > cap - g_free); ++i)
        if (!EvictOne(e)) break;
    if (Count() >= g_max || I32(owner + kOffOpen) > cap - g_free) {
        g_closedNow.fetch_add(1, std::memory_order_relaxed);
        g_close(e);
        return;
    }
    Push(e);
    g_kept.fetch_add(1, std::memory_order_relaxed);
}

uint64_t __fastcall AcquireWrap(uintptr_t e, uint64_t mode) {
    if (IsEntry(e)) {
        if (Ptr(e + kOffHandle) != 0) {
            g_reused.fetch_add(1, std::memory_order_relaxed);
        } else if (const uintptr_t owner = Ptr(e + kOffOwner)) {
            const int cap = I32(owner + kOffCap);
            for (int i = 0; i < 4 && cap > 0 && I32(owner + kOffOpen) >= cap - 2; ++i) {
                if (!EvictOne(0)) break;
                g_capEvict.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }
    return g_acquire(e, mode);
}

DWORD WINAPI Reporter(LPVOID) {
    for (;;) {
        Sleep(30000);
        g_log("FileLinger: kept %llu, reused open %llu, evicted %llu (busy %llu, in use %llu, at cap %llu), closed at once %llu, "
              "lingering now %d",
              (unsigned long long)g_kept.load(), (unsigned long long)g_reused.load(), (unsigned long long)g_evicted.load(),
              (unsigned long long)g_evictBusy.load(), (unsigned long long)g_evictInUse.load(),
              (unsigned long long)g_capEvict.load(), (unsigned long long)g_closedNow.load(), Count());
    }
}

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

}  // namespace

void Start(const StartInfo& info) {
    if (!info.log || !info.iniPath || !info.base) return;
    g_log = info.log;
    const int want = (int)GetPrivateProfileIntW(L"Patches", L"FileLinger", 0, info.iniPath);
    if (want != 1) return;
    if (!info.knownBuild) {
        g_log("FileLinger: not installed (unknown game build)");
        return;
    }
    g_max = (int)GetPrivateProfileIntW(L"Patches", L"FileLingerMax", 32, info.iniPath);
    g_free = (int)GetPrivateProfileIntW(L"Patches", L"FileLingerFree", 24, info.iniPath);
    if (g_max < 1) g_max = 1;
    if (g_max > kRing - 4) g_max = kRing - 4;
    if (g_free < 4) g_free = 4;

    const uintptr_t b = info.base;
    uint8_t* blk = (uint8_t*)(b + Grva(kBlock));
    uintptr_t* slotAcq = (uintptr_t*)(b + Grva(kVtable) + 0x10);
    const uintptr_t* slotRel = (const uintptr_t*)(b + Grva(kVtable) + 0x28);
    if (memcmp(blk, kBlockBytes, 16) != 0 || memcmp((void*)(b + Grva(kClose)), kCloseBytes, 16) != 0 ||
        memcmp((void*)(b + Grva(kAcquire)), kAcqBytes, 16) != 0 || memcmp((void*)(b + Grva(kRelease)), kRelBytes, 16) != 0 ||
        *slotAcq != b + Grva(kAcquire) || *slotRel != b + Grva(kRelease) || ((uintptr_t)blk & 15) != 0 ||
        (uintptr_t)(blk + (kSite - kBlock)) != b + Grva(kSite)) {
        g_log("FileLinger: not installed (game bytes differ)");
        return;
    }
    g_vt = b + Grva(kVtable);
    g_close = (CloseFn)(b + Grva(kClose));
    g_acquire = (AcquireFn)(b + Grva(kAcquire));

    const IMAGE_NT_HEADERS* nt = (const IMAGE_NT_HEADERS*)(b + ((const IMAGE_DOS_HEADER*)b)->e_lfanew);
    uint8_t* page = AllocNear(b, b + nt->OptionalHeader.SizeOfImage, 4096);
    if (!page) {
        g_log("FileLinger: not installed (no page within rel32 reach)");
        return;
    }
    // thunk: mov rax, OnLastRelease ; jmp rax
    page[0] = 0x48; page[1] = 0xB8;
    const uint64_t fn = (uint64_t)&OnLastRelease;
    memcpy(page + 2, &fn, 8);
    page[10] = 0xFF; page[11] = 0xE0;
    DWORD old = 0;
    VirtualProtect(page, 4096, PAGE_EXECUTE_READ, &old);
    FlushInstructionCache(GetCurrentProcess(), page, 4096);

    const int64_t rel = (int64_t)(uintptr_t)page - (int64_t)(b + Grva(kSite) + 5);
    if (rel < INT32_MIN || rel > INT32_MAX) {
        g_log("FileLinger: not installed (thunk out of rel32 reach)");
        return;
    }
    // 1) acquire slot (aligned qword in .rdata)
    if (!VirtualProtect(slotAcq, 8, PAGE_READWRITE, &old)) {
        g_log("FileLinger: not installed (vtable protect failed)");
        return;
    }
    InterlockedExchange64((volatile LONG64*)slotAcq, (LONG64)(uintptr_t)&AcquireWrap);
    VirtualProtect(slotAcq, 8, old, &old);
    // 2) the E8 rel32 at kSite, inside one aligned 16-byte cmpxchg
    alignas(16) LONG64 cmp[2], nw[2];
    memcpy(cmp, kBlockBytes, 16);
    memcpy(nw, kBlockBytes, 16);
    const int32_t r32 = (int32_t)rel;
    memcpy((uint8_t*)nw + (kSite - kBlock) + 1, &r32, 4);
    bool ok = false;
    if (VirtualProtect(blk, 16, PAGE_EXECUTE_READWRITE, &old)) {
        ok = InterlockedCompareExchange128((volatile LONG64*)blk, nw[1], nw[0], cmp) != 0;
        VirtualProtect(blk, 16, old, &old);
        FlushInstructionCache(GetCurrentProcess(), blk, 16);
    }
    if (!ok) {
        // put the acquire slot back: the wrapper alone is harmless, but keep the pair consistent
        if (VirtualProtect(slotAcq, 8, PAGE_READWRITE, &old)) {
            InterlockedExchange64((volatile LONG64*)slotAcq, (LONG64)(uintptr_t)g_acquire);
            VirtualProtect(slotAcq, 8, old, &old);
        }
        g_log("FileLinger: not installed (release site patch failed)");
        return;
    }
    if (HANDLE t = CreateThread(nullptr, 0, &Reporter, nullptr, 0, nullptr)) CloseHandle(t);
    g_log("FileLinger: on (keep up to %d released archive handles, %d slots kept free under the game's cap)", g_max, g_free);
}

}  // namespace fhlinger
