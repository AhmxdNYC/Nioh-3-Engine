// ptstart.cpp - PTS present-thread start guard (see ptstart.h).
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <cstring>
#include <initializer_list>

#include "gamerva.h"
#include "ptstart.h"

namespace ptstart {

namespace {

// 39 bytes from the thread proc's entry (1A82298), '??' = the rip-relative hook slot at 1A822AD:
//   push rbx / sub rsp,20h / mov rbx,rcx / mov rcx,[rcx+7720h] (SITE) / jmp 1A822FC / mov rax,[rip+x] / test rax,rax /
//   je +2 / call rax / mov rcx,[rbx+7730h]
const uint8_t kCtx[] = {0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0xD9, 0x48, 0x8B, 0x89, 0x20, 0x77, 0x00, 0x00, 0xEB, 0x52, 0x48, 0x8B,
                        0x05, 0x00, 0x00, 0x00, 0x00, 0x48, 0x85, 0xC0, 0x74, 0x02, 0xFF, 0xD0, 0x48, 0x8B, 0x8B, 0x30, 0x77, 0x00, 0x00};
const bool kWild[sizeof(kCtx)] = {false, false, false, false, false, false, false, false, false, false, false, false, false,
                                  false, false, false, false, false, false, false, false, true,  true,  true,  true};
const uint8_t kOrig[7] = {0x48, 0x8B, 0x89, 0x20, 0x77, 0x00, 0x00};  // mov rcx,[rcx+7720h]
static_assert(sizeof(kCtx) == 39, "context length");

volatile LONG g_waits = 0;  // stub rounds that found the handle still 0 (each = one Sleep(1))
bool g_applied = false;

uint8_t* AllocIn(uintptr_t lo, uintptr_t hi, size_t size) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    const uintptr_t g = si.dwAllocationGranularity;
    uintptr_t a = (lo + g - 1) & ~(g - 1);
    while (a + size <= hi) {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery((void*)a, &mbi, sizeof(mbi))) break;
        const uintptr_t rb = (uintptr_t)mbi.BaseAddress, re = rb + mbi.RegionSize;
        if (mbi.State == MEM_FREE) {
            const uintptr_t s = (std::max)(a, (rb + g - 1) & ~(g - 1));
            if (s + size <= re && s + size <= hi)
                if (void* p = VirtualAlloc((void*)s, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE)) return (uint8_t*)p;
        }
        if (re <= a) break;
        a = re;
    }
    return nullptr;
}

// a page within rel32 reach of [lo, hi)
uint8_t* AllocNear(uintptr_t lo, uintptr_t hi, size_t size) {
    const uintptr_t reach = 0x7FF00000ull - (hi - lo);
    if (uint8_t* p = AllocIn(hi, hi + reach, size)) return p;
    return AllocIn(lo > reach + 0x10000 ? lo - reach : 0x10000, lo, size);
}

// The stub (rbx = the GfxDevice, as at the site; every volatile register is dead there: 1A822FC sets edx and calls
// WaitForSingleObject(rcx, edx), rax is the wait result). Stack on entry: thread proc rsp 16-aligned + return address.
//   00 mov rcx,[rbx+7720h]      07 test rcx,rcx      0A jnz 37 (done)
//   0C sub rsp,28h
//   10 spin: mov rax,&g_waits   1A lock inc dword [rax]
//   1D mov ecx,1                22 mov rax,&Sleep    2C call rax
//   2E mov rcx,[rbx+7720h]      35 test rcx,rcx      38 jz 10 (spin)
//   3A add rsp,28h              3E done: ret
size_t BuildStub(uint8_t* p, void* sleepFn, volatile LONG* counter) {
    size_t n = 0;
    auto b = [&](std::initializer_list<uint8_t> v) {
        for (uint8_t x : v) p[n++] = x;
    };
    auto q = [&](uint64_t v) {
        memcpy(p + n, &v, 8);
        n += 8;
    };
    b({0x48, 0x8B, 0x8B, 0x20, 0x77, 0x00, 0x00});  // 00 mov rcx,[rbx+7720h]
    b({0x48, 0x85, 0xC9});                          // 07 test rcx,rcx
    const size_t jnzAt = n;
    b({0x75, 0x00});                                // 0A jnz done
    b({0x48, 0x83, 0xEC, 0x28});                    // sub rsp,28h
    const size_t spin = n;
    b({0x48, 0xB8});                                // mov rax,imm64
    q((uint64_t)(uintptr_t)counter);
    b({0xF0, 0xFF, 0x00});                          // lock inc dword [rax]
    b({0xB9, 0x01, 0x00, 0x00, 0x00});              // mov ecx,1
    b({0x48, 0xB8});                                // mov rax,imm64
    q((uint64_t)(uintptr_t)sleepFn);
    b({0xFF, 0xD0});                                // call rax
    b({0x48, 0x8B, 0x8B, 0x20, 0x77, 0x00, 0x00});  // mov rcx,[rbx+7720h]
    b({0x48, 0x85, 0xC9});                          // test rcx,rcx
    b({0x74, (uint8_t)(int8_t)((int)spin - (int)(n + 2))});  // jz spin
    b({0x48, 0x83, 0xC4, 0x28});                    // add rsp,28h
    const size_t done = n;
    b({0xC3});                                      // ret
    p[jnzAt + 1] = (uint8_t)(done - (jnzAt + 2));
    return n;
}

// one interlocked 8-byte compare-exchange (the 7-byte site sits inside one aligned qword on the known build)
bool PatchAtomic8(uint8_t* p, const uint8_t* bytes, size_t len) {
    const uintptr_t a = (uintptr_t)p, w8 = a & ~uintptr_t(7);
    if (a + len > w8 + 8) return false;
    DWORD old = 0, t = 0;
    if (!VirtualProtect((void*)w8, 8, PAGE_EXECUTE_READWRITE, &old)) return false;
    volatile LONG64* qw = (volatile LONG64*)w8;
    const LONG64 cur = *qw;
    LONG64 nw = cur;
    memcpy((uint8_t*)&nw + (a - w8), bytes, len);
    const bool ok = InterlockedCompareExchange64(qw, nw, cur) == cur;
    VirtualProtect((void*)w8, 8, old, &t);
    FlushInstructionCache(GetCurrentProcess(), (void*)w8, 8);
    return ok;
}

bool CtxMatches(const uint8_t* c) {
    for (size_t i = 0; i < sizeof(kCtx); ++i)
        if (!kWild[i] && c[i] != kCtx[i]) return false;
    return true;
}

}  // namespace

bool BootWanted(const wchar_t* iniPath) { return GetPrivateProfileIntW(L"Patches", L"PTS", 1, iniPath) != 0; }

bool Applied() { return g_applied; }

bool Apply(uint8_t* ctx, LogFn log) {
    uint8_t* site = ctx + kCtxOffset;
    if (!CtxMatches(ctx) || memcmp(site, kOrig, sizeof(kOrig)) != 0) {
        if (log) log("PTS: present thread entry is not in the v2.02 state - skipped");
        return false;
    }
    void* sleepFn = (void*)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "Sleep");
    uint8_t* stub = sleepFn ? AllocNear((uintptr_t)site, (uintptr_t)site + 8, 4096) : nullptr;
    if (!stub) {
        if (log) log("PTS: no stub page within reach of the present thread - skipped");
        return false;
    }
    BuildStub(stub, sleepFn, &g_waits);
    FlushInstructionCache(GetCurrentProcess(), stub, 64);
    const intptr_t rel = (intptr_t)stub - (intptr_t)(site + 5);
    if (rel < INT32_MIN || rel > INT32_MAX) {
        VirtualFree(stub, 0, MEM_RELEASE);
        if (log) log("PTS: stub out of rel32 reach - skipped");
        return false;
    }
    uint8_t repl[7] = {0xE8, 0, 0, 0, 0, 0x66, 0x90};  // call stub ; 2-byte nop
    const int32_t r32 = (int32_t)rel;
    memcpy(repl + 1, &r32, 4);
    if (!PatchAtomic8(site, repl, sizeof(repl))) {
        VirtualFree(stub, 0, MEM_RELEASE);
        if (log) log("PTS: site write failed - skipped");
        return false;
    }
    g_applied = true;
    if (log) log("PTS: present thread waits for its wake event before the first Present (game start race, crash 1A822CC)");
    return true;
}

bool BootTry(uintptr_t base, LogFn log) {
    uint8_t* ctx = (uint8_t*)base + Grva(kCtxRva);  // known build only (dllmain): Grva is the identity there
    if (memcmp(ctx + kCtxOffset, kOrig, sizeof(kOrig)) != 0) return true;  // still encrypted: try again
    Apply(ctx, log);
    return false;
}

long WaitRounds() { return g_waits; }

}  // namespace ptstart
