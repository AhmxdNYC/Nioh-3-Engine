// drawprep.cpp - DQ (main-loop retry quantum) and P1E7 (texture-loop prefetch), both default off. See drawprep.h.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "drawprep.h"

extern "C" {
void caveDq();
void caveP1e7();
extern volatile uint32_t dp_dq_enable, dp_p1e7_enable, dp_dq_early;
extern float dp_dq_quantum;
extern uint64_t dp_dq_loop, dp_dq_exit, dp_p1e7_resume;
}

namespace drawprep {
namespace {

// ---- v2.02 (patches.json sites DQ / P1E7 + deps, verified by tests\test_patterns.py) ----
// DQ: 1A74C45 `comiss xmm6,xmm0; ja 1A74BD0`, then 1A74C4A `jmp 1A744F1`
constexpr uint32_t kDqSite = 0x1A74C45, kDqLoop = 0x1A74BD0, kDqExit = 0x1A74C4A;
const uint8_t kDqOrig[] = {0x0F, 0x2F, 0xF0, 0x77, 0x86};
// xmm6 = [3BD4688] (0.001f), loaded once before the loop; the only reader of xmm6 is the site
const uint8_t kDqXmm6[] = {0xF3, 0x0F, 0x10, 0x35, 0x9C, 0x01, 0x16, 0x02};
constexpr uint32_t kDqXmm6Rva = 0x1A744E4;
// loop head up to (excluding) the QPC call 1A74BDA, which DS may have re-pointed: cmp byte [rdx+12h],0; je; lea rcx,[rbp-60h]
const uint8_t kDqHead[] = {0x80, 0x7A, 0x12, 0x00, 0x74, 0x10, 0x48, 0x8D, 0x4D, 0xA0};
// after the QPC call up to the site: (now - start) / [timer+18h] in xmm0 (xmm1 = freq), unchanged
const uint8_t kDqBody[] = {0x48, 0x8B, 0x4D, 0xA0, 0xEB, 0x08, 0xE8, 0xB1, 0xEB, 0x16, 0xFF, 0x48, 0x63, 0xC8, 0x48, 0x2B, 0xCB, 0x0F,
                           0x57, 0xC0, 0x78, 0x07, 0xF3, 0x48, 0x0F, 0x2A, 0xC1, 0xEB, 0x15, 0x48, 0x8B, 0xC1, 0x83, 0xE1, 0x01, 0x48,
                           0xD1, 0xE8, 0x48, 0x0B, 0xC1, 0xF3, 0x48, 0x0F, 0x2A, 0xC0, 0xF3, 0x0F, 0x58, 0xC0, 0x48, 0x8B, 0x15, 0x67,
                           0x5B, 0xB4, 0x02, 0x0F, 0x57, 0xC9, 0x48, 0x8B, 0x4A, 0x18, 0x48, 0x85, 0xC9, 0x78, 0x07, 0xF3, 0x48, 0x0F,
                           0x2A, 0xC9, 0xEB, 0x15, 0x48, 0x8B, 0xC1, 0x83, 0xE1, 0x01, 0x48, 0xD1, 0xE8, 0x48, 0x0B, 0xC1, 0xF3, 0x48,
                           0x0F, 0x2A, 0xC8, 0xF3, 0x0F, 0x58, 0xC9, 0xF3, 0x0F, 0x5E, 0xC1};
constexpr uint32_t kDqBodyRva = 0x1A74BE0;
const uint8_t kDqTail[] = {0xE9, 0xA2, 0xF8, 0xFF, 0xFF};  // 1A74C4A jmp 1A744F1
// P1E7: 1E72C6 `mov r14,[r12]; mov r8,[r11+1C8h]` in the texture loop of 1E7050; resume 1E72D1 `test r14,r14`
constexpr uint32_t kPfSite = 0x1E72C6, kPfResume = 0x1E72D1;
const uint8_t kPfOrig[] = {0x4D, 0x8B, 0x34, 0x24, 0x4D, 0x8B, 0x83, 0xC8, 0x01, 0x00, 0x00};
// loop head 1E72C0 `cmp r15d,[rdx+18h]; jge 1E731F` + site + body 1E72D1..1E732E (r12 stride 8, r15d = k, r11 = this)
const uint8_t kPfLoop[] = {0x44, 0x3B, 0x7A, 0x18, 0x7D, 0x59, 0x4D, 0x8B, 0x34, 0x24, 0x4D, 0x8B, 0x83, 0xC8, 0x01, 0x00, 0x00, 0x4D,
                           0x85, 0xF6, 0x74, 0x49, 0x49, 0x8B, 0x4E, 0x68, 0x48, 0x85, 0xC9, 0x74, 0x40, 0x49, 0x8D, 0x90, 0xE0, 0x74,
                           0x00, 0x00, 0x48, 0x81, 0xC1, 0x08, 0x01, 0x00, 0x00, 0x4D, 0x8B, 0x80, 0xB0, 0x6E, 0x00, 0x00, 0xE8, 0xF7,
                           0x1B, 0xF4, 0xFF, 0x49, 0x8B, 0x4E, 0x68, 0xBA, 0xFF, 0x00, 0x00, 0x00, 0x41, 0xB8, 0x10, 0x00, 0x00, 0x00,
                           0xE8, 0xAB, 0x1D, 0xF4, 0xFF, 0x48, 0x8B, 0x54, 0x24, 0x30, 0x48, 0x8B, 0x44, 0x24, 0x40, 0x4C, 0x8B, 0x9C,
                           0x24, 0x90, 0x00, 0x00, 0x00, 0x41, 0xFF, 0xC7, 0x49, 0xFF, 0xC5, 0x49, 0x83, 0xC4, 0x08, 0x4C, 0x3B, 0xE8,
                           0x7C, 0x92};
constexpr uint32_t kPfLoopRva = 0x1E72C0;

void (*g_log)(const char*, ...) = nullptr;
uint8_t* g_page = nullptr;  // near trampolines: +0 caveDq, +10h caveP1e7 (jmp [rip]; dq target)
bool g_dq = false, g_pf = false;
int g_dqUs = 200;

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

// Same scheme as dllmain.cpp ApplyBatch: all other threads suspended, none stopped strictly inside a site (a thread at
// a site's first byte is at an instruction boundary in both encodings). No heap / CRT use while threads are suspended.
bool FrozenWrite(Write* w, int n) {
    DWORD oldProt[2] = {};
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

bool Same(uintptr_t base, uint32_t rva, const uint8_t* b, size_t n) { return memcmp((const void*)(base + rva), b, n) == 0; }

// E9 rel32 from `site` to the trampoline at `tr`, padded to `len` with one multi-byte NOP.
bool JmpBytes(Write& w, uint8_t* site, const uint8_t* tr, unsigned len) {
    const intptr_t d = (intptr_t)tr - (intptr_t)(site + 5);
    if (d < INT32_MIN || d > INT32_MAX) return false;
    w.at = site;
    w.len = len;
    w.bytes[0] = 0xE9;
    const int32_t r = (int32_t)d;
    memcpy(w.bytes + 1, &r, 4);
    static const uint8_t nop6[] = {0x66, 0x0F, 0x1F, 0x44, 0x00, 0x00};
    if (len == 5) return true;
    if (len == 11) { memcpy(w.bytes + 5, nop6, 6); return true; }
    return false;
}

void Tramp(uint8_t* at, void (*cave)()) {
    static const uint8_t jmpRip[] = {0xFF, 0x25, 0x00, 0x00, 0x00, 0x00};  // jmp qword [rip+0]
    memcpy(at, jmpRip, sizeof(jmpRip));
    const uint64_t t = (uint64_t)(uintptr_t)cave;
    memcpy(at + 6, &t, 8);
}

int Key(const wchar_t* sec, const wchar_t* k, int def, const wchar_t* ini) { return (int)GetPrivateProfileIntW(sec, k, def, ini); }

}  // namespace

int64_t DqQuantumUs() { return (g_dq && dp_dq_enable) ? g_dqUs : 0; }
volatile uint32_t* DqSwitch() { return g_dq ? &dp_dq_enable : nullptr; }
volatile uint32_t* P1e7Switch() { return g_pf ? &dp_p1e7_enable : nullptr; }

void StatusLine(char* buf, unsigned n) {
    if (!n) return;
    buf[0] = 0;
    if (!g_dq && !g_pf) return;
    snprintf(buf, n, "drawprep: DQ %s (%d us, %lu early retries), P1E7 %s", !g_dq ? "-" : dp_dq_enable ? "ON" : "off", g_dqUs,
             (unsigned long)dp_dq_early, !g_pf ? "-" : dp_p1e7_enable ? "ON" : "off");
}

void Start(const StartInfo& info) {
    g_log = info.log;
    if (!g_log || !info.iniPath || !info.base) return;
    const int dq = Key(L"Patches", L"DQ", 0, info.iniPath), pf = Key(L"Patches", L"P1E7", 0, info.iniPath);
    int us = Key(L"Tuning", L"DQ_QuantumUs", 200, info.iniPath);
    if (us < 50) us = 50;
    if (us > 1000) us = 1000;
    if (!dq && !pf) return;
    if (!info.knownBuild) {
        g_log("DQ/P1E7: v2.02 only - skipped on this build");
        return;
    }
    const uintptr_t b = info.base;
    bool wantDq = dq != 0, wantPf = pf != 0;
    if (wantDq && !(Same(b, kDqSite, kDqOrig, sizeof(kDqOrig)) && Same(b, kDqXmm6Rva, kDqXmm6, sizeof(kDqXmm6)) &&
                    Same(b, kDqLoop, kDqHead, sizeof(kDqHead)) && Same(b, kDqBodyRva, kDqBody, sizeof(kDqBody)) &&
                    Same(b, kDqExit, kDqTail, sizeof(kDqTail)))) {
        g_log("DQ: main-loop retry spin (1A74BD0..1A74C4F) differs from v2.02 (another mod?) - skipped");
        wantDq = false;
    }
    if (wantPf && !Same(b, kPfLoopRva, kPfLoop, sizeof(kPfLoop))) {
        g_log("P1E7: texture loop of 1E7050 (1E72C0..1E732E) differs from v2.02 (another mod?) - skipped");
        wantPf = false;
    }
    if (!wantDq && !wantPf) return;
    const auto* dos = (const IMAGE_DOS_HEADER*)b;
    const auto* nt = (const IMAGE_NT_HEADERS64*)(b + dos->e_lfanew);
    g_page = AllocNear(b, b + nt->OptionalHeader.SizeOfImage, 4096);
    if (!g_page) {
        g_log("DQ/P1E7: no executable page within 2 GB of the image - skipped");
        return;
    }
    Tramp(g_page, &caveDq);
    Tramp(g_page + 0x10, &caveP1e7);
    FlushInstructionCache(GetCurrentProcess(), g_page, 0x20);
    dp_dq_loop = b + kDqLoop;
    dp_dq_exit = b + kDqExit;
    dp_p1e7_resume = b + kPfResume;
    dp_dq_quantum = (float)us * 1e-6f;
    g_dqUs = us;
    dp_dq_enable = dq == 1 ? 1u : 0u;
    dp_p1e7_enable = pf == 1 ? 1u : 0u;
    Write w[2];
    int n = 0;
    bool ok = true;
    if (wantDq && !JmpBytes(w[n++], (uint8_t*)(b + kDqSite), g_page, 5)) ok = false;
    if (wantPf && !JmpBytes(w[n++], (uint8_t*)(b + kPfSite), g_page + 0x10, 11)) ok = false;
    if (!ok || !FrozenWrite(w, n)) {
        g_log("DQ/P1E7: site write failed (out of rel32 reach, or a thread kept sitting inside a site) - nothing installed");
        dp_dq_enable = dp_p1e7_enable = 0;
        return;
    }
    g_dq = wantDq;
    g_pf = wantPf;
    if (g_dq)
        g_log("DQ: main-loop retry quantum 1 ms -> %d us at 1A74C45 (switch %s)", us, dp_dq_enable ? "ON" : "off");
    if (g_pf) g_log("P1E7: texture-loop prefetch at 1E72C6 (switch %s)", dp_p1e7_enable ? "ON" : "off");
}

}  // namespace drawprep
