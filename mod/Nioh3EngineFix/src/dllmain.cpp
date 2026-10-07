// Nioh3EngineFix - native packaging of the Nioh 3 (Steam v2.02) engine patches:
//   W1/W2 (write-combined memory RMW fixes), VRR (variable-dt frame timing v2 + P9), W3 (message pump wait),
//   R1 (render-begin wait), L1 (high-resolution timer frame limiter).
// Every site is located by an AOB pattern from patches.json (via the generated patches_gen.h), its original bytes
// are verified, and all writes of a group happen with every other thread of the process suspended and no thread
// stopped inside a site. Anything unexpected -> the patch (group) is skipped and logged; the game is never touched
// otherwise. The SteamStub DRM is never touched: we only wait until it has decrypted the code.
//
// Logic mirrors the live-tested scripts patcher\cpu_patches.fsx, ft_vrr_patch.fsx, cpu_caves.fsx.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#include <intrin.h>

#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "patches_gen.h"
#include "menu.h"
#include "osd.h"

#define MOD_NAME "Nioh3EngineFix"
#define MOD_VERSION "1.0.0"

// ---- symbols shared with caves.asm -----------------------------------------------------------------------------
extern "C" {
void caveFps();
void caveScale();
void caveLim();
void caveTier();
void caveMenu();
void caveL1();
extern uint64_t vrr_minNs[3];
extern uint64_t vrr_maxNs[3];
extern uint64_t vrr_pNear;
extern uint64_t vrr_getDelta60;
extern uint64_t l1_callee;
extern uint32_t vrr_enable;
extern uint32_t vrr_tier;
extern uint32_t vrr_capCur;
extern uint32_t vrr_arm[3];
extern uint32_t vrr_cap[3];
BOOL WINAPI hookPeek(LPMSG msg, HWND wnd, UINT fmin, UINT fmax, UINT remove);
int L1Wait(uint64_t now, uint64_t last, uint64_t ticksPerFrame, uint64_t freq);
void fgWait(int64_t target);
void caveWakeGate();
extern uint32_t wk_enable, wk_skipped, wk_kept;
}

namespace {

// ---- near page layout (within +-2 GB of Nioh3.exe) ------------------------------------------------------------
struct NearData {
    volatile uint32_t ticks120;   // P3 x7: mov eax,[ticks120]
    volatile uint32_t ticks60;    // P6:    mov eax,[ticks60]
    volatile uint32_t r1Timeout;  // R1:    mov edx,[r1Timeout]  (0 = stock poll)
    uint32_t pad;
};
constexpr size_t kTrampOff = 0x40;
constexpr size_t kTrampSize = 16;
constexpr size_t kNearSize = 0x1000;

HMODULE g_self = nullptr;
uint8_t* g_base = nullptr;
size_t g_imageSize = 0;
uint8_t* g_near = nullptr;
FILE* g_log = nullptr;

// W3 / L1 runtime state (the W3/L1/F1/WK/VRR/R1 switches below are also flipped live by the mod menu, menu.cpp)
using PeekMessageA_t = BOOL(WINAPI*)(LPMSG, HWND, UINT, UINT, UINT);
PeekMessageA_t* g_peekSlot = nullptr;  // the game's IAT slot (keeps overlay hooks in the chain)
volatile LONG g_w3Enable = 0;
DWORD g_w3Ms = 1;
// Under Wine/Proton MsgWaitForMultipleObjectsEx(QS_ALLINPUT, MWMO_INPUTAVAILABLE) returns almost immediately (measured
// on a ROG Ally / Bazzite: ~24,000 wakes/s, each with a wineserver round trip), so W3 sleeps there instead.
volatile LONG g_w3UseSleep = 0;
volatile LONG g_l1Enable = 0;
HANDLE g_l1Timer = nullptr;
uint64_t g_l1MarginUs = 1000;
uint64_t g_l1MinUs = 500;
volatile LONG g_w3Count = 0;
volatile LONG g_l1Count = 0;
// F1 (FSR frame-generation presenter pacing wait) runtime state
volatile LONG g_fgEnable = 0;
HANDLE g_fgTimer = nullptr;
int64_t g_fgThreshTicks = 0;  // sleep only when more than margin+min remain
int64_t g_fgMarginTicks = 0;  // the last margin is the original QPC spin
int64_t g_qpcFreq = 0;
volatile LONG g_fgCount = 0;

// ---- logging ---------------------------------------------------------------------------------------------------
void Log(const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    SYSTEMTIME st;
    GetLocalTime(&st);
    char line[1100];
    snprintf(line, sizeof(line), "[%02u:%02u:%02u.%03u] %s\n", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, buf);
    OutputDebugStringA(line);
    if (g_log) {
        fputs(line, g_log);
        fflush(g_log);
    }
}

std::wstring SelfPathWithExt(const wchar_t* ext) {
    wchar_t p[MAX_PATH * 2] = {};
    DWORD n = GetModuleFileNameW(g_self, p, (DWORD)(sizeof(p) / sizeof(p[0])));
    std::wstring s(p, n);
    size_t dot = s.find_last_of(L'.');
    size_t sep = s.find_last_of(L"\\/");
    if (dot != std::wstring::npos && (sep == std::wstring::npos || dot > sep)) s.resize(dot);
    return s + ext;
}

// ---- config ----------------------------------------------------------------------------------------------------
struct Config {
    int enabled = 1, log = 1, allowUnknownBuild = 0, decryptWaitSec = 60;
    int w1 = 1, w2 = 1, vrr = 1, w3 = 1, r1 = 1, l1 = 1, l1OnWine = 0, fg = 1, fgOnWine = 0, p12 = 0, w3WineSleep = 1, wk = 1, workerCap = 0;
    int arm[3] = {1, 1, 1};
    int minF[3] = {20, 24, 48};
    int maxF[3] = {30, 60, 120};
    int maxFps = 120, menuQuantize = 0;
    int w3Ms = 1, r1Ms = 2, l1MarginUs = 1000, l1MinUs = 500;
};

Config LoadConfig() {
    Config c;
    std::wstring ini = SelfPathWithExt(L".ini");
    const wchar_t* f = ini.c_str();
    auto I = [&](const wchar_t* sec, const wchar_t* key, int def) { return (int)GetPrivateProfileIntW(sec, key, def, f); };
    c.enabled = I(L"General", L"Enabled", c.enabled);
    c.log = I(L"General", L"Log", c.log);
    c.allowUnknownBuild = I(L"General", L"AllowUnknownBuild", c.allowUnknownBuild);
    c.decryptWaitSec = std::clamp(I(L"General", L"DecryptWaitSeconds", c.decryptWaitSec), 1, 600);
    c.w1 = I(L"Patches", L"W1", c.w1);
    c.w2 = I(L"Patches", L"W2", c.w2);
    c.vrr = I(L"Patches", L"VRR", c.vrr);
    c.w3 = I(L"Patches", L"W3", c.w3);
    c.r1 = I(L"Patches", L"R1", c.r1);
    c.l1 = I(L"Patches", L"L1", c.l1);
    c.l1OnWine = I(L"Patches", L"L1OnWine", c.l1OnWine);
    c.fg = I(L"Patches", L"FG", c.fg);
    c.fgOnWine = I(L"Patches", L"FGOnWine", c.fgOnWine);
    c.p12 = I(L"Patches", L"P12", c.p12);
    c.w3WineSleep = I(L"Patches", L"W3WineSleep", c.w3WineSleep);
    c.wk = I(L"Patches", L"WK", c.wk);
    c.workerCap = I(L"Patches", L"WorkerCap", c.workerCap);
    const wchar_t* armK[3] = {L"Tier30", L"Tier60", L"Tier120"};
    const wchar_t* minK[3] = {L"Min30", L"Min60", L"Min120"};
    const wchar_t* maxK[3] = {L"Max30", L"Max60", L"Max120"};
    for (int i = 0; i < 3; ++i) {
        c.arm[i] = I(L"VRR", armK[i], c.arm[i]);
        c.minF[i] = I(L"VRR", minK[i], c.minF[i]);
        c.maxF[i] = I(L"VRR", maxK[i], c.maxF[i]);
    }
    c.maxFps = I(L"VRR", L"MaxFps", c.maxFps);
    c.menuQuantize = I(L"VRR", L"MenuQuantize", c.menuQuantize);
    c.w3Ms = std::clamp(I(L"Tuning", L"W3_TimeoutMs", c.w3Ms), 0, 16);
    c.r1Ms = std::clamp(I(L"Tuning", L"R1_TimeoutMs", c.r1Ms), 0, 16);
    c.l1MarginUs = std::clamp(I(L"Tuning", L"L1_MarginUs", c.l1MarginUs), 200, 20000);
    c.l1MinUs = std::clamp(I(L"Tuning", L"L1_MinSleepUs", c.l1MinUs), 0, 20000);
    return c;
}

// ---- PE helpers ------------------------------------------------------------------------------------------------
IMAGE_NT_HEADERS64* Nt(uint8_t* base) {
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    return reinterpret_cast<IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
}

struct Range {
    uint8_t* p;
    size_t n;
};

// committed, readable sub-ranges of every executable section (same scope as tests/test_patterns.py)
std::vector<Range> ExecRanges() {
    std::vector<Range> out;
    auto nt = Nt(g_base);
    auto sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
        if (!(sec->Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        uint8_t* p = g_base + sec->VirtualAddress;
        uint8_t* end = p + sec->Misc.VirtualSize;
        while (p < end) {
            MEMORY_BASIC_INFORMATION mbi{};
            if (!VirtualQuery(p, &mbi, sizeof(mbi))) break;
            uint8_t* rEnd = std::min(end, (uint8_t*)mbi.BaseAddress + mbi.RegionSize);
            const DWORD readable = PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY | PAGE_READONLY |
                                   PAGE_READWRITE | PAGE_WRITECOPY;
            if (mbi.State == MEM_COMMIT && (mbi.Protect & readable) && !(mbi.Protect & PAGE_GUARD)) {
                if (!out.empty() && out.back().p + out.back().n == p)
                    out.back().n += (size_t)(rEnd - p);
                else
                    out.push_back({p, (size_t)(rEnd - p)});
            }
            p = rEnd;
        }
    }
    return out;
}

// ---- AOB scanning ----------------------------------------------------------------------------------------------
struct Pattern {
    std::vector<uint8_t> bytes;
    std::vector<uint8_t> wild;
    size_t anchorOff = 0, anchorLen = 0;
};

bool ParsePattern(const char* s, Pattern& p) {
    while (*s) {
        while (*s == ' ') ++s;
        if (!*s) break;
        if (s[0] == '?' && s[1] == '?') {
            p.bytes.push_back(0);
            p.wild.push_back(1);
            s += 2;
            continue;
        }
        char h[3] = {s[0], s[1], 0};
        char* e = nullptr;
        unsigned long v = strtoul(h, &e, 16);
        if (e != h + 2) return false;
        p.bytes.push_back((uint8_t)v);
        p.wild.push_back(0);
        s += 2;
    }
    size_t cur = 0, cs = 0;
    for (size_t i = 0; i < p.bytes.size(); ++i) {
        if (p.wild[i]) { cur = 0; continue; }
        if (cur == 0) cs = i;
        if (++cur > p.anchorLen) { p.anchorLen = cur; p.anchorOff = cs; }
    }
    return p.anchorLen > 0;
}

// number of matches (stops at 2); *first = first match start
int FindPattern(const Pattern& p, const std::vector<Range>& ranges, uint8_t** first) {
    int n = 0;
    *first = nullptr;
    const uint8_t* aB = p.bytes.data() + p.anchorOff;
    std::boyer_moore_horspool_searcher<const uint8_t*> searcher(aB, aB + p.anchorLen);
    for (const Range& r : ranges) {
        const uint8_t* lo = r.p;
        const uint8_t* hi = r.p + r.n;
        const uint8_t* cur = lo;
        while (cur < hi) {
            auto [hit, hitEnd] = searcher(cur, hi);
            if (hit == hi) break;
            const uint8_t* st = hit - p.anchorOff;
            if (st >= lo && st + p.bytes.size() <= hi) {
                bool ok = true;
                for (size_t k = 0; k < p.bytes.size() && ok; ++k)
                    if (!p.wild[k] && st[k] != p.bytes[k]) ok = false;
                if (ok) {
                    if (n == 0) *first = const_cast<uint8_t*>(st);
                    if (++n >= 2) return n;
                }
            }
            cur = hit + 1;
        }
    }
    return n;
}

// ---- near allocation -------------------------------------------------------------------------------------------
uint8_t* AllocNear(uintptr_t lo, uintptr_t hi, size_t size) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    const uintptr_t gran = si.dwAllocationGranularity;
    const uintptr_t reach = 0x7FF00000;  // stay comfortably inside rel32 from every byte of the image
    const uintptr_t minA = (hi > reach) ? ((hi - reach + gran - 1) & ~(gran - 1)) : gran;
    const uintptr_t maxA = lo + reach - size;
    // downward from the image
    for (uintptr_t p = (lo - gran) & ~(gran - 1); p >= minA && p < lo;) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery((void*)p, &mbi, sizeof(mbi))) break;
        if (mbi.State == MEM_FREE) {
            if (void* r = VirtualAlloc((void*)p, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE)) return (uint8_t*)r;
        }
        uintptr_t next = ((uintptr_t)mbi.AllocationBase ? (uintptr_t)mbi.AllocationBase : p) - gran;
        if (next >= p) break;
        p = next & ~(gran - 1);
    }
    // upward from the image end
    for (uintptr_t p = (hi + gran - 1) & ~(gran - 1); p <= maxA;) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery((void*)p, &mbi, sizeof(mbi))) break;
        if (mbi.State == MEM_FREE) {
            if (void* r = VirtualAlloc((void*)p, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE)) return (uint8_t*)r;
        }
        uintptr_t next = ((uintptr_t)mbi.BaseAddress + mbi.RegionSize + gran - 1) & ~(gran - 1);
        if (next <= p) break;
        p = next;
    }
    return nullptr;
}

bool Rel32(const uint8_t* next, const void* target, int32_t* out) {
    int64_t d = (int64_t)(uintptr_t)target - (int64_t)(uintptr_t)next;
    if (d < INT32_MIN || d > INT32_MAX) return false;
    *out = (int32_t)d;
    return true;
}

void* TargetFn(gen::Target t) {
    switch (t) {
        case gen::Target::caveFps: return (void*)&caveFps;
        case gen::Target::caveScale: return (void*)&caveScale;
        case gen::Target::caveLim: return (void*)&caveLim;
        case gen::Target::caveTier: return (void*)&caveTier;
        case gen::Target::caveMenu: return (void*)&caveMenu;
        case gen::Target::hookPeek: return (void*)&hookPeek;
        case gen::Target::caveL1: return (void*)&caveL1;
        case gen::Target::fgWait: return (void*)&fgWait;
        case gen::Target::caveWakeGate: return (void*)&caveWakeGate;
        default: return nullptr;
    }
}

uint8_t* Trampoline(gen::Target t) { return g_near + kTrampOff + kTrampSize * (size_t)t; }

void WriteTrampolines() {
    for (int t = 1; t <= (int)gen::Target::caveWakeGate; ++t) {
        uint8_t* p = Trampoline((gen::Target)t);
        const uint8_t jmp[6] = {0xFF, 0x25, 0, 0, 0, 0};  // jmp qword ptr [rip+0]
        memcpy(p, jmp, 6);
        uint64_t a = (uint64_t)(uintptr_t)TargetFn((gen::Target)t);
        memcpy(p + 6, &a, 8);
        p[14] = 0xCC;
        p[15] = 0xCC;
    }
    FlushInstructionCache(GetCurrentProcess(), g_near, kNearSize);
}

// ---- site resolution -------------------------------------------------------------------------------------------
struct Item {
    const gen::Site* s = nullptr;
    uint8_t* addr = nullptr;
    bool found = false;
    std::vector<uint8_t> repl;
};

bool Resolve(const gen::Site& s, const std::vector<Range>& ranges, bool known, Item& it) {
    it.s = &s;
    Pattern p;
    if (!ParsePattern(s.pattern, p)) { Log("  %-4s bad pattern", s.id); return false; }
    uint8_t* first = nullptr;
    int n = FindPattern(p, ranges, &first);
    if (n != 1) {
        Log("  %-4s pattern %s (skipped)", s.id, n == 0 ? "NOT FOUND (other mod / different build?)" : "NOT UNIQUE");
        return false;
    }
    uint8_t* a = first + s.offset;
    uint32_t rva = (uint32_t)(a - g_base);
    if (known && rva != s.rva) {
        Log("  %-4s found at RVA %X, expected %X on this build (skipped)", s.id, rva, s.rva);
        return false;
    }
    // original bytes: exact on the known build; on another build the pattern's wildcards inside the site are allowed
    for (int k = 0; k < s.len; ++k) {
        bool wildHere = p.wild[s.offset + k] != 0;
        if (a[k] != s.orig[k] && (known || !wildHere)) {
            Log("  %-4s original bytes differ at RVA %X+%d (already patched by another mod?) (skipped)", s.id, rva, k);
            return false;
        }
    }
    it.addr = a;
    it.found = true;
    return true;
}

bool BuildRepl(Item& it) {
    const gen::Site& s = *it.s;
    std::vector<uint8_t>& r = it.repl;
    r.clear();
    if (s.kind == gen::Kind::Bytes) {
        r.assign(s.repl, s.repl + s.len);
        return true;
    }
    if (s.kind == gen::Kind::Call) {
        for (int k = 0; k < s.callAt; ++k) r.push_back(0x90);
        int32_t rel;
        if (!Rel32(it.addr + s.callAt + 5, Trampoline(s.target), &rel)) return false;
        r.push_back(0xE8);
        for (int k = 0; k < 4; ++k) r.push_back((uint8_t)(rel >> (8 * k)));
        static const uint8_t pad1[] = {0x90}, pad2[] = {0x66, 0x90}, pad3[] = {0x0F, 0x1F, 0x00},
                             pad5[] = {0x0F, 0x1F, 0x44, 0x00, 0x00};
        int pad = s.len - s.callAt - 5;
        const uint8_t* pp = nullptr;
        switch (pad) {
            case 0: break;
            case 1: pp = pad1; break;
            case 2: pp = pad2; break;
            case 3: pp = pad3; break;
            case 5: pp = pad5; break;
            default: return false;
        }
        if (pp) r.insert(r.end(), pp, pp + pad);
        return (int)r.size() == s.len;
    }
    if (s.kind == gen::Kind::RipLoad) {
        auto nd = reinterpret_cast<NearData*>(g_near);
        const void* slot = nullptr;
        switch (s.data) {
            case gen::Data::ticks120: slot = (const void*)&nd->ticks120; break;
            case gen::Data::ticks60: slot = (const void*)&nd->ticks60; break;
            case gen::Data::r1Timeout: slot = (const void*)&nd->r1Timeout; break;
            default: return false;
        }
        int32_t rel;
        if (!Rel32(it.addr + 6, slot, &rel)) return false;
        r.push_back(s.opcode[0]);
        r.push_back(s.opcode[1]);
        for (int k = 0; k < 4; ++k) r.push_back((uint8_t)(rel >> (8 * k)));
        return s.len == 6;
    }
    return false;
}

// ---- frozen write ----------------------------------------------------------------------------------------------
// Writes every item's replacement while all other threads are suspended and none of them is stopped strictly
// inside a site (a thread exactly at a site start, or at its end, is at an instruction boundary in both encodings).
// No heap / CRT / loader use while threads are suspended.
bool ApplyBatch(const char* name, std::vector<Item*>& items) {
    if (items.empty()) return true;
    for (Item* it : items)
        if (!BuildRepl(*it)) { Log("%s: cannot encode %s (out of rel32 range?) - group skipped", name, it->s->id); return false; }

    std::vector<DWORD> oldProt(items.size());
    for (size_t i = 0; i < items.size(); ++i)
        if (!VirtualProtect(items[i]->addr, items[i]->s->len, PAGE_EXECUTE_READWRITE, &oldProt[i])) {
            Log("%s: VirtualProtect failed (%lu) - group skipped", name, GetLastError());
            for (size_t j = 0; j < i; ++j) { DWORD t; VirtualProtect(items[j]->addr, items[j]->s->len, oldProt[j], &t); }
            return false;
        }

    const DWORD self = GetCurrentThreadId(), pid = GetCurrentProcessId();
    bool written = false;
    int attempts = 0;
    for (; attempts < 200 && !written; ++attempts) {
        std::vector<HANDLE> hs;
        {
            std::vector<DWORD> tids;
            HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
            if (snap != INVALID_HANDLE_VALUE) {
                THREADENTRY32 te{};
                te.dwSize = sizeof(te);
                for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te))
                    if (te.th32OwnerProcessID == pid && te.th32ThreadID != self) tids.push_back(te.th32ThreadID);
                CloseHandle(snap);
            }
            hs.reserve(tids.size());
            for (DWORD tid : tids)
                if (HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, tid))
                    hs.push_back(h);
        }
        std::vector<uint8_t> suspended(hs.size(), 0);
        // ---- frozen section: no allocation from here until resume ----
        for (size_t i = 0; i < hs.size(); ++i) suspended[i] = (SuspendThread(hs[i]) != (DWORD)-1);
        bool clash = false;
        for (size_t i = 0; i < hs.size() && !clash; ++i) {
            if (!suspended[i]) continue;
            CONTEXT ctx{};
            ctx.ContextFlags = CONTEXT_CONTROL;
            if (!GetThreadContext(hs[i], &ctx)) continue;  // exited / not inspectable
            for (Item* it : items) {
                uintptr_t a = (uintptr_t)it->addr;
                if (ctx.Rip > a && ctx.Rip < a + it->s->len) { clash = true; break; }
            }
        }
        if (!clash) {
            for (Item* it : items) memcpy(it->addr, it->repl.data(), it->repl.size());
            FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
            written = true;
        }
        for (size_t i = 0; i < hs.size(); ++i)
            if (suspended[i]) ResumeThread(hs[i]);
        // ---- end frozen section ----
        for (HANDLE h : hs) CloseHandle(h);
        if (!written) Sleep(1 + attempts % 4);
    }
    for (size_t i = 0; i < items.size(); ++i) { DWORD t; VirtualProtect(items[i]->addr, items[i]->s->len, oldProt[i], &t); }
    if (!written) { Log("%s: a thread kept sitting inside a site after %d tries - nothing written", name, attempts); return false; }
    // read back
    for (Item* it : items)
        if (memcmp(it->addr, it->repl.data(), it->repl.size()) != 0) { Log("%s: READ-BACK MISMATCH at %s", name, it->s->id); return false; }
    for (Item* it : items) {
        char hex[3 * 32] = {};
        for (size_t k = 0; k < it->repl.size() && k < 31; ++k) snprintf(hex + 3 * k, 4, "%02X ", it->repl[k]);
        Log("  %-4s RVA %-8X patched: %s", it->s->id, (unsigned)(it->addr - g_base), hex);
    }
    Log("%s: applied (%zu site(s), %d attempt(s))", name, items.size(), attempts);
    return true;
}

// ---- VRR configuration (mirrors ft_vrr_patch.fsx v2 + maxFps / P9) ---------------------------------------------
void ConfigureVrr(const Config& c) {
    static const int tiers[3] = {30, 60, 120};
    const int maxFpsCfg = c.maxFps <= 0 ? 1000 : std::max(120, c.maxFps);
    for (int i = 0; i < 3; ++i) {
        int maxF = tiers[i] == 120 ? (maxFpsCfg > 120 ? maxFpsCfg : std::min(120, std::max(1, c.maxF[i])))
                                   : std::min(tiers[i], std::max(1, c.maxF[i]));
        int minF = std::clamp(c.minF[i], 1, maxF);
        vrr_arm[i] = c.arm[i] ? 1u : 0u;
        vrr_cap[i] = (uint32_t)maxF;
        vrr_minNs[i] = 1000000000ull / (uint64_t)maxF;  // shortest interval
        vrr_maxNs[i] = 1000000000ull / (uint64_t)minF;  // longest interval
        Log("VRR tier %3d: armed=%u clamp %d..%d fps, limiter cap %d", tiers[i], vrr_arm[i], minF, maxF, maxF);
    }
    // stock-equivalent values until the first P1 frame (tier assumed 60 there; overwritten every frame by caveFps)
    auto nd = reinterpret_cast<NearData*>(g_near);
    nd->ticks120 = 2;
    nd->ticks60 = 1;
    vrr_tier = 60;
    vrr_capCur = 60;
    vrr_pNear = (uint64_t)(uintptr_t)g_near;
    vrr_enable = 1;
}

bool InModule(uintptr_t a) { return a >= (uintptr_t)g_base && a < (uintptr_t)g_base + g_imageSize; }

// ---- main worker ------------------------------------------------------------------------------------------------
DWORD WINAPI Worker(LPVOID) {
    Config cfg = LoadConfig();
    if (cfg.log) {
        std::wstring lp = SelfPathWithExt(L".log");
        g_log = _wfsopen(lp.c_str(), L"w", _SH_DENYWR);
    }
    Log(MOD_NAME " " MOD_VERSION " (built " __DATE__ ") - native port of the live-tested Nioh 3 engine patches");
    if (!cfg.enabled) { Log("disabled in ini ([General] Enabled=0)"); return 0; }

    // one instance per process (e.g. both an ASI loader and Nioh3ModLoader present)
    wchar_t mtx[64];
    swprintf(mtx, 64, L"Local\\Nioh3EngineFix.%lu", GetCurrentProcessId());
    HANDLE hm = CreateMutexW(nullptr, FALSE, mtx);
    if (hm && GetLastError() == ERROR_ALREADY_EXISTS) { Log("another Nioh3EngineFix instance is already active - this copy does nothing"); return 0; }

    g_base = (uint8_t*)GetModuleHandleW(nullptr);
    auto nt = Nt(g_base);
    g_imageSize = nt->OptionalHeader.SizeOfImage;
    const bool known = nt->FileHeader.TimeDateStamp == gen::kBuildTimestamp && nt->OptionalHeader.SizeOfImage == gen::kBuildSizeOfImage;
    Log("Nioh3.exe at %p: TimeDateStamp %08lX SizeOfImage %zX -> %s", g_base, nt->FileHeader.TimeDateStamp, g_imageSize,
        known ? "known build (v2.02)" : "UNKNOWN BUILD");
    if (!known && !cfg.allowUnknownBuild) {
        Log("refusing to patch an unknown build (set [General] AllowUnknownBuild=1 to try pattern-only matching at your own risk)");
        return 0;
    }

    // ---- wait for SteamStub to decrypt .text (never touch the stub; just poll the bytes)
    // sentinels spread over .text: lowest site (W1), highest (P3g) and two in between
    const gen::Site* sentinels[4] = {nullptr, nullptr, nullptr, nullptr};
    for (int i = 0, k = 0; i < gen::kNumSites && k < 4; ++i) {
        const char* id = gen::kSites[i].id;
        if (!strcmp(id, "W1") || !strcmp(id, "P1") || !strcmp(id, "L1") || !strcmp(id, "P3g")) sentinels[k++] = &gen::kSites[i];
    }
    Pattern sentinelPat;
    ParsePattern(sentinels[0]->pattern, sentinelPat);
    // P12 (worker-pool penalty lift) must land before the engine sizes its pools, which happens once, soon after
    // the code is decrypted - so it is written from inside this wait loop the moment its bytes appear (known build
    // only; an unknown build can't be pattern-scanned in time).
    const gen::Site* p12 = nullptr;
    for (int i = 0; i < gen::kNumSites; ++i)
        if (!strcmp(gen::kSites[i].id, "P12")) p12 = &gen::kSites[i];
    bool p12Pending = cfg.p12 && known && p12;
    if (cfg.p12 && !known) Log("P12: skipped on an unknown build (it has to be written before the pools are sized)");
    auto tryP12 = [&]() {
        uint8_t* a = g_base + p12->rva;
        if (memcmp(a, p12->orig, p12->len) != 0) return;
        DWORD old, t;
        if (!VirtualProtect(a, p12->len, PAGE_EXECUTE_READWRITE, &old)) { Log("P12: VirtualProtect failed (%lu)", GetLastError()); p12Pending = false; return; }
        memcpy(a, gen::repl_P12, p12->len);
        VirtualProtect(a, p12->len, old, &t);
        FlushInstructionCache(GetCurrentProcess(), a, p12->len);
        p12Pending = false;
        Log("P12: worker-pool penalty lifted (KIDS_RESOURCE/StageUpdate keep 3 more threads on CPUs with <= 12 threads)");
    };
    // WC (WorkerCap, handhelds): lower the worker-pool base clamp 32 -> cap in SizeWorkerPools (1FE300B / 1FE3011),
    // so every weighted pool shrinks (Ally 16 threads: ~127 -> ~78 pool threads at cap 10). Boot-time like P12.
    const gen::Site* wca = nullptr;
    const gen::Site* wcb = nullptr;
    for (int i = 0; i < gen::kNumSites; ++i) {
        if (!strcmp(gen::kSites[i].id, "WCa")) wca = &gen::kSites[i];
        if (!strcmp(gen::kSites[i].id, "WCb")) wcb = &gen::kSites[i];
    }
    const DWORD logical = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    bool wcPending = cfg.workerCap > 0 && known && wca && wcb;
    if (wcPending && (cfg.workerCap < 6 || cfg.workerCap > 31)) { Log("WC: WorkerCap must be 6..31 - skipped"); wcPending = false; }
    if (wcPending && logical <= (DWORD)cfg.workerCap) { Log("WC: %lu logical CPUs <= WorkerCap %d - nothing to cap", logical, cfg.workerCap); wcPending = false; }
    auto tryWC = [&]() {
        uint8_t* a = g_base + wca->rva;
        uint8_t* b = g_base + wcb->rva;
        if (memcmp(a, wca->orig, wca->len) != 0 || memcmp(b, wcb->orig, wcb->len) != 0) return;
        DWORD old, t;
        if (!VirtualProtect(a, 0x10, PAGE_EXECUTE_READWRITE, &old)) { Log("WC: VirtualProtect failed (%lu)", GetLastError()); wcPending = false; return; }
        b[1] = (uint8_t)cfg.workerCap;  // `mov al,cap` first: a half-written state still behaves like stock
        a[3] = (uint8_t)cfg.workerCap;  // `cmp r11d,cap`
        VirtualProtect(a, 0x10, old, &t);
        FlushInstructionCache(GetCurrentProcess(), a, 0x10);
        wcPending = false;
        Log("WC: worker-pool base capped at %d (of %lu logical CPUs)", cfg.workerCap, logical);
    };
    const DWORD t0 = GetTickCount();
    bool ready = false;
    while (!ready) {
        if (p12Pending) tryP12();
        if (wcPending) tryWC();
        if (known) {
            ready = true;
            for (const gen::Site* s : sentinels)
                if (!s || memcmp(g_base + s->rva, s->orig, s->len) != 0) ready = false;
        } else {
            uint8_t* f = nullptr;
            ready = FindPattern(sentinelPat, ExecRanges(), &f) == 1;
        }
        if (ready || GetTickCount() - t0 > (DWORD)cfg.decryptWaitSec * 1000) break;
        Sleep((p12Pending || wcPending) ? 0 : known ? 20 : 250);
    }
    if (p12Pending) tryP12();
    if (wcPending) tryWC();
    if (wcPending) Log("WC: sites not in their original state after decryption - skipped");
    if (p12Pending) Log("P12: site not in its original state after decryption - skipped");
    if (ready) Sleep(200);  // let the unpacker finish whatever it does after the last byte
    Log(ready ? "code is decrypted (%lu ms after start)" : "code did not reach the expected state within %lu ms - scanning anyway, each site is still verified",
        GetTickCount() - t0);

    // ---- resolve every site of the enabled groups
    std::vector<Range> ranges = ExecRanges();
    std::vector<Item> items(gen::kNumSites);
    auto groupOn = [&](const char* g) {
        if (!strcmp(g, "W1")) return cfg.w1 != 0;
        if (!strcmp(g, "W2")) return cfg.w2 != 0;
        if (!strcmp(g, "VRR")) return cfg.vrr != 0;
        if (!strcmp(g, "VRRMENU")) return cfg.vrr != 0 && cfg.menuQuantize != 0;
        if (!strcmp(g, "W3")) return cfg.w3 != 0;
        if (!strcmp(g, "R1")) return cfg.r1 != 0;
        if (!strcmp(g, "L1")) return cfg.l1 != 0;
        if (!strcmp(g, "FG")) return cfg.fg != 0;
        if (!strcmp(g, "WK")) return cfg.wk != 0;
        return false;  // P12 is handled in the decrypt-wait loop above
    };
    Log("resolving sites:");
    for (int i = 0; i < gen::kNumSites; ++i) {
        const gen::Site& s = gen::kSites[i];
        items[i].s = &s;
        if (!groupOn(s.group)) continue;
        if (Resolve(s, ranges, known, items[i])) Log("  %-4s OK at RVA %X", s.id, (unsigned)(items[i].addr - g_base));
    }
    auto collect = [&](const char* g) {
        std::vector<Item*> v;
        for (Item& it : items)
            if (!strcmp(it.s->group, g)) v.push_back(&it);
        return v;
    };
    auto allFound = [](const std::vector<Item*>& v) {
        for (Item* it : v)
            if (!it->found) return false;
        return !v.empty();
    };

    // ---- near page (trampolines + data the game reads RIP-relatively)
    g_near = AllocNear((uintptr_t)g_base, (uintptr_t)g_base + g_imageSize, kNearSize);
    if (!g_near) { Log("could not allocate memory within 2 GB of Nioh3.exe - only in-place patches (W1/W2) will be applied"); }
    else {
        memset(g_near, 0, kNearSize);
        WriteTrampolines();
        Log("near page at %p", g_near);
    }

    int applied = 0, skipped = 0;
    std::string skippedNames;  // OSD popup
    osd::State l1State = osd::State::Off, f1State = osd::State::Off;
    auto run = [&](const char* name, std::vector<Item*> v, bool ok) {
        if (v.empty()) return false;
        if (!ok || !ApplyBatch(name, v)) { ++skipped; skippedNames += name; skippedNames += ' '; Log("%s: SKIPPED", name); return false; }
        ++applied;
        return true;
    };

    // W1: in place
    if (cfg.w1) { auto v = collect("W1"); run("W1", v, allFound(v)); }
    // W2a-d: in place, independent
    if (cfg.w2)
        for (Item* it : collect("W2")) {
            std::vector<Item*> one{it};
            run(it->s->id, one, it->found);
        }
    // VRR: all-or-nothing
    bool vrrDone = false;
    if (cfg.vrr) {
        auto v = collect("VRR");
        bool ok = allFound(v) && g_near;
        if (!allFound(v)) Log("VRR: not every site was found - the whole group is skipped (a partial VRR install would run the game at the wrong speed)");
        if (ok) ConfigureVrr(cfg);
        // keep the scripts' safe order: data-only users first, P1 (activation) last; written together anyway
        vrrDone = run("VRR", v, ok);
        if (vrrDone && cfg.menuQuantize) {
            auto m = collect("VRRMENU");
            bool mok = allFound(m);
            if (mok) {
                uint8_t* a = m[0]->addr;
                uintptr_t callee = (uintptr_t)(a + 5 + *(int32_t*)(a + 1));
                mok = InModule(callee);
                if (mok) vrr_getDelta60 = callee;
            }
            run("VRR menu quantize (P7)", m, mok);
        }
    }
    // W3: message pump
    bool w3Done = false, r1Done = false, wkDone = false;
    if (cfg.w3) {
        auto v = collect("W3");
        bool ok = allFound(v) && g_near;
        if (ok) {
            uint8_t* a = v[0]->addr;  // FF 15 disp32
            auto slot = reinterpret_cast<PeekMessageA_t*>(a + 6 + *(int32_t*)(a + 2));
            ok = InModule((uintptr_t)slot) && *slot != nullptr;
            if (ok) {
                g_peekSlot = slot;
                g_w3Ms = (DWORD)cfg.w3Ms;
                g_w3Enable = 1;
            } else Log("W3: IAT slot looks wrong - skipped");
        }
        w3Done = run("W3", v, ok);
    }
    // R1: render-begin wait (two sites)
    if (cfg.r1) {
        auto v = collect("R1");
        bool ok = allFound(v) && g_near;
        if (ok) reinterpret_cast<NearData*>(g_near)->r1Timeout = (uint32_t)cfg.r1Ms;
        r1Done = run("R1", v, ok);
    }
    // L1: frame limiter. Under Wine/Proton the high-resolution timer flag may be accepted but not honoured, which
    // would make the limiter overshoot - so L1 stays off there unless L1OnWine=1.
    const char* wine = nullptr;
    if (HMODULE ntdll = GetModuleHandleW(L"ntdll.dll"))
        if (auto wgv = reinterpret_cast<const char*(__cdecl*)()>(GetProcAddress(ntdll, "wine_get_version"))) wine = wgv();
    if (wine) Log("running under Wine/Proton %s", wine);
    if (wine && g_w3Enable && cfg.w3WineSleep) {
        g_w3UseSleep = 1;
        Log("W3: Wine/Proton -> empty-queue wait is Sleep(%lu ms) (Wine returns from MsgWaitForMultipleObjectsEx at once)",
            g_w3Ms ? g_w3Ms : 1);
    }
    if (cfg.l1 && wine && !cfg.l1OnWine) { Log("L1: skipped under Wine/Proton (set [Patches] L1OnWine=1 to try it)"); l1State = osd::State::WineSkipped; }
    if (cfg.l1 && (!wine || cfg.l1OnWine)) {
        auto v = collect("L1");
        bool ok = allFound(v) && g_near;
        if (ok) {
            g_l1Timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
            if (!g_l1Timer) {
                Log("L1: high-resolution waitable timer not supported here (older Windows / Wine-Proton?) - L1 skipped, stock limiter kept");
                ok = false;
            }
        }
        if (ok) {
            uint8_t* a = v[0]->addr;  // 33 D2 48 8B CE E8 rel32
            uintptr_t callee = (uintptr_t)(a + 10 + *(int32_t*)(a + 6));
            ok = InModule(callee);
            if (ok) {
                l1_callee = callee;
                g_l1MarginUs = (uint64_t)cfg.l1MarginUs;
                g_l1MinUs = (uint64_t)cfg.l1MinUs;
                g_l1Enable = 1;
            }
        }
        l1State = run("L1", v, ok) ? osd::State::On : osd::State::Failed;
    }
    // F1: FSR frame-generation presenter pacing wait (only runs with FSR frame generation on). Same timer caveat
    // under Wine/Proton as L1.
    if (cfg.fg && wine && !cfg.fgOnWine) { Log("F1: skipped under Wine/Proton (set [Patches] FGOnWine=1 to try it)"); f1State = osd::State::WineSkipped; }
    if (cfg.fg && (!wine || cfg.fgOnWine)) {
        auto v = collect("FG");
        bool ok = allFound(v) && g_near;
        if (ok) {
            uint8_t* a = v[0]->addr;  // E8 rel32 -> the stock waitUntil(QPC): push rbx; ...; QueryPerformanceCounter loop
            uintptr_t callee = (uintptr_t)(a + 5 + *(int32_t*)(a + 1));
            ok = InModule(callee);
            if (!ok) Log("F1: call target outside the module - skipped");
        }
        if (ok) {
            g_fgTimer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
            LARGE_INTEGER f;
            QueryPerformanceFrequency(&f);
            g_qpcFreq = f.QuadPart;
            if (!g_fgTimer || g_qpcFreq <= 0) {
                Log("F1: high-resolution waitable timer not supported here - F1 skipped, stock wait kept");
                ok = false;
            } else {
                g_fgMarginTicks = (int64_t)cfg.l1MarginUs * g_qpcFreq / 1000000;
                g_fgThreshTicks = (int64_t)(cfg.l1MarginUs + cfg.l1MinUs) * g_qpcFreq / 1000000;
                g_fgEnable = 1;
            }
        }
        f1State = run("F1", v, ok) ? osd::State::On : osd::State::Failed;
    }
    // periodic stats so a log from a handheld shows the wake rates without a profiler
    if (HANDLE h = CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
            LONG w3 = g_w3Count, l1 = g_l1Count, fg = g_fgCount;
            uint32_t ks = wk_skipped, kk = wk_kept;
            for (;;) {
                Sleep(30000);
                LONG w3n = g_w3Count, l1n = g_l1Count, fgn = g_fgCount;
                uint32_t ksn = wk_skipped, kkn = wk_kept;
                Log("stats/s: W3 waits %ld (%s), L1 timer sleeps %ld, F1 timer sleeps %ld, WK wakes kept %u / skipped %u",
                    (w3n - w3) / 30, g_w3UseSleep ? "Sleep" : "MsgWait", (l1n - l1) / 30, (fgn - fg) / 30,
                    (kkn - kk) / 30, (ksn - ks) / 30);
                w3 = w3n; l1 = l1n; fg = fgn; ks = ksn; kk = kkn;
            }
        }, nullptr, 0, nullptr)) CloseHandle(h);
    // WK: job-system wake gate (skip the futile "wake one more worker" when no queue of the pool holds work)
    if (cfg.wk) {
        auto v = collect("WK");
        bool ok = allFound(v) && g_near;
        if (ok) wk_enable = 1;
        wkDone = run("WK", v, ok);
    }
    {  // in-game overlay: FPS counter + startup popup (osd.cpp) + mod menu (menu.cpp)
        std::wstring iniPath = SelfPathWithExt(L".ini");
        menu::Host mh;
        mh.version = MOD_VERSION;
        mh.log = &Log;
        mh.iniPath = iniPath.c_str();
        mh.base = g_base;
        mh.knownBuild = known;
        mh.wine = wine != nullptr;
        if (vrrDone) mh.vrrEnable = reinterpret_cast<volatile uint32_t*>(&vrr_enable);
        if (w3Done) {
            mh.w3Enable = &g_w3Enable;
            mh.w3UseSleep = &g_w3UseSleep;
        }
        if (r1Done) {
            mh.r1Timeout = &reinterpret_cast<NearData*>(g_near)->r1Timeout;
            mh.r1Default = (uint32_t)cfg.r1Ms;
        }
        if (l1State == osd::State::On) mh.l1Enable = &g_l1Enable;
        if (f1State == osd::State::On) mh.fgEnable = &g_fgEnable;
        if (wkDone) mh.wkEnable = reinterpret_cast<volatile uint32_t*>(&wk_enable);
        mh.l1 = l1State == osd::State::On ? menu::FixState::Installed
                : l1State == osd::State::WineSkipped ? menu::FixState::WineSkipped : menu::FixState::NotInstalled;
        mh.f1 = f1State == osd::State::On ? menu::FixState::Installed
                : f1State == osd::State::WineSkipped ? menu::FixState::WineSkipped : menu::FixState::NotInstalled;
        mh.w3Count = &g_w3Count;
        mh.l1Count = &g_l1Count;
        mh.fgCount = &g_fgCount;
        mh.wkKept = reinterpret_cast<volatile uint32_t*>(&wk_kept);
        mh.wkSkipped = reinterpret_cast<volatile uint32_t*>(&wk_skipped);
        mh.workerCap = cfg.workerCap;
        mh.p12 = cfg.p12;
        mh.l1OnWine = cfg.l1OnWine;
        mh.fgOnWine = cfg.fgOnWine;
        menu::Configure(mh);  // copies everything; must run before osd::Start (which asks menu::Enabled)
        osd::StartupInfo si;
        si.version = MOD_VERSION;
        si.log = &Log;
        si.iniPath = iniPath.c_str();
        si.base = g_base;
        si.knownBuild = known;
        si.applied = applied;
        si.skipped = skipped;
        si.skippedNames = skippedNames.c_str();
        si.wine = wine;
        si.vrr = vrrDone;
        si.l1 = l1State;
        si.f1 = f1State;
        osd::Start(si);  // copies everything; returns at once
    }
    Log("done: %d group(s) applied, %d skipped. Patches live until the game exits (nothing is written to disk).", applied, skipped);
    return 0;
}

bool HostIsNioh3() {
    wchar_t p[MAX_PATH * 2] = {};
    DWORD n = GetModuleFileNameW(nullptr, p, (DWORD)(sizeof(p) / sizeof(p[0])));
    std::wstring s(p, n);
    size_t sep = s.find_last_of(L"\\/");
    std::wstring name = sep == std::wstring::npos ? s : s.substr(sep + 1);
    return _wcsicmp(name.c_str(), L"Nioh3.exe") == 0;
}

}  // namespace

// ---- W3: PeekMessageA, and when the queue was empty, block until input or W3_TimeoutMs ---------------------------
extern "C" BOOL WINAPI hookPeek(LPMSG msg, HWND wnd, UINT fmin, UINT fmax, UINT remove) {
    BOOL r = (*g_peekSlot)(msg, wnd, fmin, fmax, remove);
    // mod menu: drop the menu key (F10 would put Windows into system-menu mode) and, while the menu is open, the
    // navigation keys, before the game translates/dispatches them
    if (r && msg && (remove & PM_REMOVE) && msg->message >= WM_KEYDOWN && msg->message <= WM_SYSKEYUP &&
        menu::SwallowMessage(msg))
        msg->message = WM_NULL;
    if (!r && g_w3Enable) {
        if (g_w3UseSleep)
            Sleep(g_w3Ms ? g_w3Ms : 1);  // Wine: ~1 kHz cap; window messages are picked up on the next Peek
        else
            MsgWaitForMultipleObjectsEx(0, nullptr, g_w3Ms, 0x4FF /* QS_ALLINPUT (Win7-era mask, as in the script) */,
                                        MWMO_INPUTAVAILABLE);
        InterlockedIncrement(&g_w3Count);
    }
    return r;
}

// ---- L1: sleep on a high-resolution waitable timer until (deadline - margin) when enough slack is left ------------
extern "C" int L1Wait(uint64_t now, uint64_t last, uint64_t ticksPerFrame, uint64_t freq) {
    if (!g_l1Enable || freq == 0) return 0;
    const uint64_t elapsed = now - last;  // caller guarantees now > last
    if (ticksPerFrame <= elapsed) return 0;
    const uint64_t remaining = ticksPerFrame - elapsed;
    uint64_t hi;
    uint64_t rem;
    uint64_t lo = _umul128(g_l1MarginUs + g_l1MinUs, freq, &hi);
    const uint64_t thresh = _udiv128(hi, lo, 1000000ull, &rem);
    if (remaining <= thresh) return 0;
    lo = _umul128(g_l1MarginUs, freq, &hi);
    const uint64_t margin = _udiv128(hi, lo, 1000000ull, &rem);
    const uint64_t sleepTicks = remaining - margin;
    lo = _umul128(sleepTicks, 10000000ull, &hi);
    if (hi >= freq) return 0;  // absurd interval, don't sleep
    const uint64_t units100ns = _udiv128(hi, lo, freq, &rem);
    LARGE_INTEGER due;
    due.QuadPart = -(LONGLONG)units100ns;
    if (!SetWaitableTimer(g_l1Timer, &due, 0, nullptr, nullptr, FALSE)) {
        g_l1Enable = 0;  // timer unusable: stock limiter from now on (as the script's cave did)
        return 0;
    }
    WaitForSingleObject(g_l1Timer, 100);
    InterlockedIncrement(&g_l1Count);
    return 1;
}

// ---- F1: replaces the FSR presenter's waitUntil(t) = `do QueryPerformanceCounter(&now) while (now < t)` -----------
// Sleeps on a high-resolution waitable timer until (t - margin) when enough time is left, then spins the rest
// exactly like the original, so frame-generation pacing accuracy is unchanged.
extern "C" void fgWait(int64_t target) {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    if (g_fgEnable && target - now.QuadPart > g_fgThreshTicks && target - now.QuadPart < g_qpcFreq) {
        const int64_t sleepTicks = target - now.QuadPart - g_fgMarginTicks;
        LARGE_INTEGER due;
        due.QuadPart = -(sleepTicks * 10000000 / g_qpcFreq);  // relative, 100 ns units (sleepTicks < ~1 s here)
        if (SetWaitableTimer(g_fgTimer, &due, 0, nullptr, nullptr, FALSE)) {
            WaitForSingleObject(g_fgTimer, 100);
            InterlockedIncrement(&g_fgCount);
        } else {
            g_fgEnable = 0;  // timer unusable: plain spin from now on
        }
    }
    do QueryPerformanceCounter(&now);
    while (now.QuadPart < target);
}

BOOL APIENTRY DllMain(HMODULE mod, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = mod;
        DisableThreadLibraryCalls(mod);
        if (!HostIsNioh3()) return TRUE;  // loaded into something else: do nothing
        if (HANDLE h = CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr)) CloseHandle(h);
    }
    return TRUE;
}
