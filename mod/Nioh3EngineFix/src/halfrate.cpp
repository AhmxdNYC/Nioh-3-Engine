// halfrate.cpp - entry hooks that skip one record in two. See halfrate.h and halfrate_core.h.
// Offline harness: tests/run_halfrate_harness.py. The game is not launched from that test.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>

#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <initializer_list>

#include "halfrate.h"
#include "halfrate_core.h"

namespace halfrate {
namespace {

constexpr uint32_t kRvaShadow = 0x1F5C2C;  // ShadowCascadeRender
constexpr uint32_t kRvaGi = 0x542C08;      // ReflectionProbePassRecord
constexpr uint32_t kRvaCloud = 0x5F2A88;   // NatureSkyDrawClouds
constexpr uint32_t kRvaCam = 0x244264;     // CameraBuildLookAtView, eye at +0x10
constexpr float kCutMeters = 4.0f;

// v2.02 prologues. Shadow and GI: rex push rbp; push rbx; push rsi; push rdi.
// Cloud: mov [rsp+20h], rbx. Camera: mov rax, rsp; sub rsp, 78h.
const uint8_t kShadowBytes[] = {0x40, 0x55, 0x53, 0x56, 0x57};
const uint8_t kGiBytes[] = {0x40, 0x55, 0x53, 0x56, 0x57};
const uint8_t kCloudBytes[] = {0x48, 0x89, 0x5C, 0x24, 0x20};
const uint8_t kCamBytes[] = {0x48, 0x8B, 0xC4, 0x48, 0x83, 0xEC, 0x78};

void (*g_log)(const char*, ...) = nullptr;
volatile LONG g_on[kChannels] = {};
volatile LONG64 g_updates[kChannels] = {};
volatile LONG64 g_reuses[kChannels] = {};
volatile LONG64 g_cuts = 0;
CRITICAL_SECTION g_cs;
bool g_csInit = false;
bool g_installed = false;
World g_world;

void Log(const char* fmt, ...) {
    if (!g_log) return;
    char b[400];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    g_log("%s", b);
}

int Key(const wchar_t* k, const wchar_t* ini) {
    return (int)GetPrivateProfileIntW(L"HalfRate", k, 0, ini);
}

void Lock() {
    if (g_csInit) EnterCriticalSection(&g_cs);
}
void Unlock() {
    if (g_csInit) LeaveCriticalSection(&g_cs);
}

int Gate(int channel, bool allow) {
    if (channel < 0 || channel >= kChannels) return 0;
    if (!g_on[channel] || !allow) return 0;
    Lock();
    const Act a = Decide(g_world, channel, allow);
    Unlock();
    if (a == Act::Reuse) {
        InterlockedIncrement64(&g_reuses[channel]);
        return 1;
    }
    InterlockedIncrement64(&g_updates[channel]);
    return 0;
}

// Caller of ShadowCascadeRender stores cascade at [rsp+F8h] and the vsm byte at [rsp+100h]
// (p32 / p33). After CALL those are [rsp+100h] and [rsp+108h]. The cave's frame is 7 pushes
// plus a 0x20 shadow (0x58). The slots are then [rsp+158h] and [rsp+160h].
extern "C" int HrShadowGate(uint32_t cascade, uint32_t vsm) {
    return Gate(kShadow, ShadowMaySkip(cascade, vsm));
}
extern "C" int HrPassGate(int channel) { return Gate(channel, true); }
extern "C" void HrCamera(void* cam) {
    if (!cam) return;
    float e[4] = {};
    memcpy(e, (const uint8_t*)cam + 0x10, sizeof e);
    Lock();
    if (ObserveCamera(g_world, e[0], e[1], e[2], kCutMeters)) InterlockedIncrement64(&g_cuts);
    Unlock();
}

struct Write {
    uint8_t* at;
    uint8_t bytes[16];
    unsigned len;
};

bool FrozenWrite(Write* w, int n) {
    DWORD oldProt[4] = {};
    for (int i = 0; i < n; ++i)
        if (!VirtualProtect(w[i].at, w[i].len, PAGE_EXECUTE_READWRITE, &oldProt[i])) {
            for (int j = 0; j < i; ++j) {
                DWORD t;
                VirtualProtect(w[j].at, w[j].len, oldProt[j], &t);
            }
            return false;
        }
    const DWORD self = GetCurrentThreadId(), pid = GetCurrentProcessId();
    DWORD tids[1024];
    HANDLE hs[1024];
    bool sus[1024];
    int nt = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        THREADENTRY32 te{};
        te.dwSize = sizeof te;
        for (BOOL ok = Thread32First(snap, &te); ok && nt < 1024; ok = Thread32Next(snap, &te))
            if (te.th32OwnerProcessID == pid && te.th32ThreadID != self) tids[nt++] = te.th32ThreadID;
        CloseHandle(snap);
    }
    int nh = 0;
    for (int i = 0; i < nt; ++i)
        if (HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, tids[i]))
            hs[nh++] = h;
    for (int i = 0; i < nh; ++i) sus[i] = SuspendThread(hs[i]) != (DWORD)-1;
    bool clash = false;
    for (int i = 0; i < nh && !clash; ++i) {
        if (!sus[i]) continue;
        CONTEXT ctx{};
        ctx.ContextFlags = CONTEXT_CONTROL;
        if (!GetThreadContext(hs[i], &ctx)) continue;
        for (int k = 0; k < n; ++k)
            if (ctx.Rip > (uintptr_t)w[k].at && ctx.Rip < (uintptr_t)w[k].at + w[k].len) clash = true;
    }
    bool written = false;
    if (!clash) {
        for (int k = 0; k < n; ++k) memcpy(w[k].at, w[k].bytes, w[k].len);
        FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
        written = true;
    }
    for (int i = 0; i < nh; ++i)
        if (sus[i]) ResumeThread(hs[i]);
    for (int i = 0; i < nh; ++i) CloseHandle(hs[i]);
    for (int i = 0; i < n; ++i) {
        DWORD t;
        VirtualProtect(w[i].at, w[i].len, oldProt[i], &t);
    }
    return written;
}

uint8_t* AllocNear(uintptr_t base) {
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    const uintptr_t gran = si.dwAllocationGranularity ? si.dwAllocationGranularity : 0x10000;
    uintptr_t lo = base > 0x70000000ull ? base - 0x70000000ull : 0x10000;
    uintptr_t hi = base + 0x70000000ull;
    for (uintptr_t a = (base + 0x10000000ull) & ~(gran - 1); a > lo; a -= gran) {
        if (void* p = VirtualAlloc((void*)a, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE)) return (uint8_t*)p;
    }
    for (uintptr_t a = (base - 0x10000000ull) & ~(gran - 1); a < hi; a += gran) {
        if (void* p = VirtualAlloc((void*)a, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE)) return (uint8_t*)p;
    }
    return nullptr;
}

void Put(uint8_t*& w, std::initializer_list<uint8_t> b) {
    for (uint8_t x : b) *w++ = x;
}
void Put64(uint8_t*& w, uint64_t v) {
    memcpy(w, &v, 8);
    w += 8;
}

// Entry RSP is 8 mod 16. Seven pushes leave it 0 mod 16, and sub rsp, 20h keeps the call aligned.
// Caller slots [rsp+100h]/[rsp+108h] are then [rsp+158h] (cascade) and [rsp+160h] (vsm byte).
// Returns the address past the cave, or null if the skip branch does not fit a short jump.
uint8_t* Emit(uint8_t* w, bool shadow, int channel, const uint8_t* stolen, int n, const void* resume) {
    Put(w, {0x50, 0x51, 0x52, 0x41, 0x50, 0x41, 0x51, 0x41, 0x52, 0x41, 0x53});  // rax rcx rdx r8 r9 r10 r11
    Put(w, {0x48, 0x83, 0xEC, 0x20});
    if (shadow) {
        Put(w, {0x8B, 0x84, 0x24, 0x58, 0x01, 0x00, 0x00});              // mov eax, [rsp+158h] cascade
        Put(w, {0x0F, 0xB6, 0x94, 0x24, 0x60, 0x01, 0x00, 0x00});        // movzx edx, byte [rsp+160h] vsm
        Put(w, {0x89, 0xC1});                                            // mov ecx, eax
    } else {
        Put(w, {0xB9});                                                  // mov ecx, imm32 channel
        memcpy(w, &channel, 4);
        w += 4;
    }
    const void* gate = shadow ? (const void*)HrShadowGate : (const void*)HrPassGate;
    Put(w, {0x48, 0xB8});  // mov rax, imm64 (rax is already saved)
    Put64(w, (uint64_t)(uintptr_t)gate);
    Put(w, {0xFF, 0xD0});  // call rax
    Put(w, {0x48, 0x83, 0xC4, 0x20});
    Put(w, {0x41, 0x5B, 0x41, 0x5A, 0x41, 0x59, 0x41, 0x58, 0x5A, 0x59});  // r11..rcx
    Put(w, {0x85, 0xC0});
    Put(w, {0x75});
    uint8_t* jnz = w;
    *w++ = 0;
    Put(w, {0x58});  // pop rax
    memcpy(w, stolen, (size_t)n);
    w += n;
    Put(w, {0xFF, 0x25, 0, 0, 0, 0});
    Put64(w, (uint64_t)(uintptr_t)resume);
    const int rel = (int)(w - (jnz + 1));
    if (rel < -128 || rel > 127) return nullptr;
    *jnz = (uint8_t)rel;
    Put(w, {0x58, 0xC3});  // pop rax; ret
    return w;
}

uint8_t* EmitCam(uint8_t* w, const uint8_t* stolen, int n, const void* resume) {
    Put(w, {0x50, 0x52, 0x41, 0x50, 0x41, 0x51, 0x41, 0x52, 0x41, 0x53});  // rax rdx r8 r9 r10 r11; rcx stays
    Put(w, {0x48, 0x83, 0xEC, 0x28});
    Put(w, {0x48, 0xB8});
    Put64(w, (uint64_t)(uintptr_t)HrCamera);
    Put(w, {0xFF, 0xD0});
    Put(w, {0x48, 0x83, 0xC4, 0x28});
    Put(w, {0x41, 0x5B, 0x41, 0x5A, 0x41, 0x59, 0x41, 0x58, 0x5A, 0x58});  // r11..rdx, rax
    memcpy(w, stolen, (size_t)n);
    w += n;
    Put(w, {0xFF, 0x25, 0, 0, 0, 0});
    Put64(w, (uint64_t)(uintptr_t)resume);
    return w;
}

bool FillJmp(Write& w, uint8_t* site, unsigned len, uint8_t* cave) {
    if (!cave || len < 5 || len > sizeof w.bytes) return false;
    const intptr_t d = (intptr_t)cave - (intptr_t)(site + 5);
    if (d < (intptr_t)INT32_MIN || d > (intptr_t)INT32_MAX) return false;
    w.at = site;
    w.len = len;
    memset(w.bytes, 0x90, sizeof w.bytes);
    w.bytes[0] = 0xE9;
    const int32_t r = (int32_t)d;
    memcpy(w.bytes + 1, &r, 4);
    return true;
}

}  // namespace

bool Installed() { return g_installed; }
int Enabled(int channel) { return (channel >= 0 && channel < kChannels && g_on[channel]) ? 1 : 0; }
void SetEnabled(int channel, int on) {
    if (channel < 0 || channel >= kChannels) return;
    const int v = on ? 1 : 0;
    InterlockedExchange(&g_on[channel], v);
    Lock();
    g_world.ch[channel].enabled = v;
    if (!v) {
        g_world.ch[channel].have = 0;
        g_world.ch[channel].arm = 0;
    }
    Unlock();
}

const char* StatusLine(char* buf, size_t n) {
    if (!buf || !n) return "";
    if (!g_installed) {
        snprintf(buf, n, "half-rate: not installed");
        return buf;
    }
    snprintf(buf, n, "half-rate HRS %d HRG %d HRC %d  updates %lld/%lld/%lld reuses %lld/%lld/%lld cuts %lld",
             Enabled(0), Enabled(1), Enabled(2), (long long)g_updates[0], (long long)g_updates[1], (long long)g_updates[2],
             (long long)g_reuses[0], (long long)g_reuses[1], (long long)g_reuses[2], (long long)g_cuts);
    return buf;
}

void Start(const StartInfo& info) {
    g_log = info.log;
    if (!info.base || !info.knownBuild) {
        Log("HalfRate: v2.02 only - not installed");
        return;
    }
    auto same = [&](uint32_t rva, const uint8_t* b, size_t n) {
        return memcmp((const void*)(info.base + rva), b, n) == 0;
    };
    if (!same(kRvaShadow, kShadowBytes, sizeof kShadowBytes) || !same(kRvaGi, kGiBytes, sizeof kGiBytes) ||
        !same(kRvaCloud, kCloudBytes, sizeof kCloudBytes) || !same(kRvaCam, kCamBytes, sizeof kCamBytes)) {
        Log("HalfRate: record prologues differ from v2.02 - not installed");
        return;
    }
    uint8_t* page = AllocNear(info.base);
    if (!page) {
        Log("HalfRate: no executable page within 2 GB - not installed");
        return;
    }
    uint8_t* c0 = page;
    uint8_t* c1 = Emit(c0, true, kShadow, kShadowBytes, (int)sizeof kShadowBytes,
                       (const void*)(info.base + kRvaShadow + sizeof kShadowBytes));
    uint8_t* c2 = c1 ? Emit(c1, false, kGi, kGiBytes, (int)sizeof kGiBytes,
                            (const void*)(info.base + kRvaGi + sizeof kGiBytes))
                     : nullptr;
    uint8_t* c3 = c2 ? Emit(c2, false, kCloud, kCloudBytes, (int)sizeof kCloudBytes,
                            (const void*)(info.base + kRvaCloud + sizeof kCloudBytes))
                     : nullptr;
    if (c3) EmitCam(c3, kCamBytes, (int)sizeof kCamBytes, (const void*)(info.base + kRvaCam + sizeof kCamBytes));
    if (!g_csInit) {
        InitializeCriticalSection(&g_cs);
        g_csInit = true;
    }
    Write ws[4]{};
    const bool ok = FillJmp(ws[0], (uint8_t*)(info.base + kRvaShadow), (unsigned)sizeof kShadowBytes, c0) &&
                    FillJmp(ws[1], (uint8_t*)(info.base + kRvaGi), (unsigned)sizeof kGiBytes, c1) &&
                    FillJmp(ws[2], (uint8_t*)(info.base + kRvaCloud), (unsigned)sizeof kCloudBytes, c2) &&
                    FillJmp(ws[3], (uint8_t*)(info.base + kRvaCam), (unsigned)sizeof kCamBytes, c3) &&
                    FrozenWrite(ws, 4);
    if (!ok) {
        Log("HalfRate: entry hook was not written");
        return;
    }
    g_installed = true;
    if (info.iniPath) {
        SetEnabled(kShadow, Key(L"HRS", info.iniPath) == 1);
        SetEnabled(kGi, Key(L"HRG", info.iniPath) == 1);
        SetEnabled(kCloud, Key(L"HRC", info.iniPath) == 1);
    }
    Log("HalfRate: installed, default off unless the ini says 1 (HRS %d HRG %d HRC %d). Far shadows, GI gather, clouds. "
        "Estimate Ally %.2f-%.2f / %.2f-%.2f / %.2f-%.2f ms. Not in a preset.",
        Enabled(0), Enabled(1), Enabled(2), Estimate(kShadow).lo, Estimate(kShadow).hi, Estimate(kGi).lo, Estimate(kGi).hi,
        Estimate(kCloud).lo, Estimate(kCloud).hi);
}

}  // namespace halfrate
