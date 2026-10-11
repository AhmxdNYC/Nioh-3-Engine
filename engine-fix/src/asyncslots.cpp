// asyncslots.cpp - AS: engine async-compute slots at boot + live producer switch (default off). See asyncslots.h.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>

#include "asyncslots.h"

extern "C" {
void caveAs0();
void caveAs1();
void caveAs2();
void caveAs3();
void caveAs4();
void caveAs5();
extern volatile uint32_t as_enable, as_and_on, as_and_off;
extern uint64_t as_resume;
extern volatile uint32_t as_calls[8];
}

namespace asyncslots {
namespace {

// ---- v2.02 (patches.json sites ASp* + deps AS, verified by tests\test_patterns.py) ----
constexpr uint32_t kTable = 0x44C04D0;  // g_AsyncSlotTable (.data): {u8 enable, f32 param, u32 id} x 13
constexpr uint32_t kLive = 0x45C0460;   // g_AsyncSlotTableLive (.bss): zero until RenderAsyncSlotsInit ran
constexpr int kSlots = 13;
// RenderAsyncSlotsInit vendor gate: cmp dword [rbp+70h],10DEh ; je 226A4AD (skip the slot-0 disable loop)
constexpr uint32_t kGateCmp = 0x226A490, kGate = 0x226A497;
const uint8_t kGateCmpBytes[] = {0x81, 0x7D, 0x70, 0xDE, 0x10, 0x00, 0x00};
const uint8_t kGateOrig[] = {0x74, 0x14};
const uint8_t kGateNew[] = {0xEB, 0x14};  // = patches.json AS0 new
// AsyncSlotGetCtx 52BE1C..52BE3D: mov eax,[rcx+1CCh]; bt eax,edx; jb +3; xor eax,eax; ret; mov eax,edx;
// movsxd rdx,[rcx+rax*4+17Ch]; test edx,edx; js -17; mov rax,[rcx+rdx*8+10h]; ret. The caves resume at +6 (bt eax,edx).
constexpr uint32_t kGetCtx = 0x52BE1C, kGetCtxResume = 0x52BE22;
const uint8_t kGetCtxBytes[] = {0x8B, 0x81, 0xCC, 0x01, 0x00, 0x00, 0x0F, 0xA3, 0xD0, 0x72, 0x03, 0x33, 0xC0, 0xC3, 0x8B, 0xC2, 0x48,
                                0x63, 0x94, 0x81, 0x7C, 0x01, 0x00, 0x00, 0x85, 0xD2, 0x78, 0xEF, 0x48, 0x8B, 0x44, 0xD1, 0x10, 0xC3};
// The one producer call (`call 52BE1C`) of each switchable slot. All 32 call sites of 52BE1C were classified (no jmp / pointer
// / inlined mask test exists): slot 0's other callers are the KIDS job 3D16E1, the deferred-batch joins (EffectViewPrepare
// 3D37FD, RefProbe 544634, fog cold 194E70E, 19CFAFC; entered only when the producer set RS+13ACh) and AsyncSlotsCreate.
struct Site {
    uint32_t rva;
    int slot;
    const char* name;
};
const Site kSites[] = {
    {0x3D0C67, 0, "effects"},     // EffectRenderNodeExecute 3D0AE0
    {0x5F328D, 3, "VCLOUD"},      // NatureSkyDrawClouds 5F2A88
    {0x14906E, 4, "fog"},         // VolumetricFogScatterNodeExecute 14871C
    {0x1A25524, 6, "ocean"},      // slot-6 producer (cold chunk 1A135FA region)
    {0x52B142, 11, "GVD"},        // GrassVertexDisplacementNodeExecute 52AF08 -> 52AF94 body
    {0x418643, 12, "Tree2Wind"},  // Tree2WindNodeExecute 41805C
};
constexpr int kNumSites = (int)(sizeof(kSites) / sizeof(kSites[0]));
// Slot 0's deferred joins: entered only while RS+13ACh != 0, which only the producer's async branch sets (3D0D3A). Masking
// the producer alone is therefore safe at any moment. Checked before the slot-0 site is installed (patches.json deps AS).
struct Gate {
    uint32_t rva;
    uint8_t len;
    uint8_t b[25];
};
const Gate kSlot0Gates[] = {
    {0x5443B7, 22, {0x41, 0x3B, 0x9D, 0x98, 0x13, 0x00, 0x00, 0x75, 0x0D, 0x41, 0x38, 0xB5, 0xAC, 0x13, 0x00, 0x00, 0x0F, 0x85, 0x5D, 0x02, 0x00, 0x00}},
    {0x3D37DA, 25, {0x3B, 0x87, 0x98, 0x13, 0x00, 0x00, 0x0F, 0x85, 0xC5, 0x00, 0x00, 0x00, 0x41, 0x38, 0xBF, 0xAC, 0x13, 0x00, 0x00, 0x0F, 0x84,
                    0xB8, 0x00, 0x00, 0x00}},
    {0x1489B7, 22, {0x44, 0x3B, 0xBF, 0x98, 0x13, 0x00, 0x00, 0x75, 0x0D, 0x80, 0xBF, 0xAC, 0x13, 0x00, 0x00, 0x00, 0x0F, 0x85, 0x33, 0x5D, 0x80, 0x01}},
    {0x547721, 22, {0x45, 0x3B, 0x8E, 0x98, 0x13, 0x00, 0x00, 0x75, 0x0D, 0x41, 0x38, 0x9E, 0xAC, 0x13, 0x00, 0x00, 0x0F, 0x85, 0xBB, 0x83, 0x48, 0x01}},
    {0x3D0D3A, 7, {0xC6, 0x86, 0xAC, 0x13, 0x00, 0x00, 0x01}},
};
constexpr uint32_t kSwitchable = (1u << 0) | (1u << 3) | (1u << 4) | (1u << 6) | (1u << 11) | (1u << 12);
constexpr uint32_t kSyncAllowed = (1u << 3) | (1u << 4) | (1u << 6) | (1u << 0);  // 0 only matters where it is stock-on (NVIDIA)

void (*g_log)(const char*, ...) = nullptr;
uint8_t* g_page = nullptr;
bool g_installed = false;
uint32_t g_siteMask = 0;  // bit k = kSites[k] installed
BootCfg g_boot;

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
    uint8_t bytes[8];
    unsigned len;
};
constexpr int kMaxWrites = kNumSites;

// Same scheme as drawprep.cpp FrozenWrite: all other threads suspended, none stopped strictly inside a site (a thread at
// a site's first byte is at an instruction boundary in both encodings; a thread inside 52BE1C called from a re-pointed
// site returns to the instruction after it, unchanged). No heap / CRT use while threads are suspended.
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

bool Same(uintptr_t base, uint32_t rva, const uint8_t* b, size_t n) { return memcmp((const void*)(base + rva), b, n) == 0; }

// `call rel32` at site must reach `to`
bool CallTargets(uintptr_t base, uint32_t site, uintptr_t to) {
    const uint8_t* p = (const uint8_t*)(base + site);
    if (p[0] != 0xE8) return false;
    int32_t r;
    memcpy(&r, p + 1, 4);
    return (uintptr_t)(p + 5) + (intptr_t)r == to;
}

bool CallBytes(Write& w, uint8_t* site, const uint8_t* tr) {
    const intptr_t d = (intptr_t)tr - (intptr_t)(site + 5);
    if (d < INT32_MIN || d > INT32_MAX) return false;
    w.at = site;
    w.len = 5;
    w.bytes[0] = 0xE8;
    const int32_t r = (int32_t)d;
    memcpy(w.bytes + 1, &r, 4);
    return true;
}

void Tramp(uint8_t* at, void (*cave)()) {
    static const uint8_t jmpRip[] = {0xFF, 0x25, 0x00, 0x00, 0x00, 0x00};  // jmp qword [rip+0]
    memcpy(at, jmpRip, sizeof(jmpRip));
    const uint64_t t = (uint64_t)(uintptr_t)cave;
    memcpy(at + 6, &t, 8);
}

void SlotList(char* out, size_t n, uint32_t mask) {
    size_t o = 0;
    out[0] = 0;
    for (int i = 0; i < kSlots && o + 4 < n; ++i)
        if (mask & (1u << i)) o += (size_t)snprintf(out + o, n - o, " %d", i);
    if (!o) snprintf(out, n, " -");
}

// PCI vendor of the primary display adapter ("PCI\VEN_1002&..."), 0 if unknown. user32 only: no DXGI before the game's own.
uint32_t PrimaryVendor() {
    DISPLAY_DEVICEW dd{};
    dd.cb = sizeof(dd);
    uint32_t first = 0;
    for (DWORD i = 0; EnumDisplayDevicesW(nullptr, i, &dd, 0); ++i) {
        const wchar_t* v = wcsstr(dd.DeviceID, L"VEN_");
        const uint32_t ven = v ? (uint32_t)wcstoul(v + 4, nullptr, 16) : 0;
        if (ven && !first) first = ven;
        if (ven && (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)) return ven;
        dd.cb = sizeof(dd);
    }
    return first;
}

}  // namespace

bool BootWanted(const wchar_t* iniPath, BootCfg& b, void (*log)(const char*, ...)) {
    b = BootCfg{};
    if (!iniPath) return false;
    b.mode = (int)GetPrivateProfileIntW(L"Patches", L"AS", 0, iniPath);
    if (b.mode != 1 && b.mode != 2) { b.mode = 0; return false; }
    wchar_t s[128] = {};
    GetPrivateProfileStringW(L"Tuning", L"AS_Slots", L"0", s, 128, iniPath);
    for (wchar_t* p = s; *p;) {
        while (*p == L' ' || *p == L',' || *p == L'\t') ++p;
        if (!*p) break;
        const bool neg = *p == L'-';
        if (neg) ++p;
        wchar_t* e = nullptr;
        const long v = wcstol(p, &e, 10);
        if (e == p) { ++p; continue; }  // junk: skip one char
        p = e;
        if (v < 0 || v >= kSlots) continue;
        if (neg) b.sync |= 1u << v;
        else b.want |= 1u << v;
    }
    b.vendor = PrimaryVendor();
    if ((b.want & 1u) && b.vendor == 0x8086) {
        b.want &= ~1u;
        if (log) log("AS: slot 0 refused on an Intel GPU (KT never ships the async effects path there; hardware-compatibility.md)");
    }
    if (b.sync & ~kSyncAllowed) {
        if (log) log("AS: AS_Slots \"-N\" only for slots 0 3 4 6 (audited producers) - other \"-N\" ignored");
        b.sync &= kSyncAllowed;
    }
    return b.want != 0 || b.sync != 0;
}

bool BootTry(uintptr_t base, BootCfg& b, void (*log)(const char*, ...)) {
    if (!b.mode || !base) return false;
    // decrypted yet? the gate (74 or EB, AS0 may have run first), AsyncSlotGetCtx, and the static table's ids
    const uint8_t* gate = (const uint8_t*)(base + kGate);
    if (!Same(base, kGateCmp, kGateCmpBytes, sizeof(kGateCmpBytes)) || !(gate[0] == 0x74 || gate[0] == 0xEB) || gate[1] != 0x14) return true;
    if (!Same(base, kGetCtx, kGetCtxBytes, sizeof(kGetCtxBytes))) return true;
    uint8_t* tab = (uint8_t*)(base + kTable);
    for (int i = 0; i < kSlots; ++i) {
        uint32_t id;
        memcpy(&id, tab + 12 * i + 8, 4);
        if (id != (uint32_t)i || tab[12 * i] > 1) return true;
    }
    uint32_t liveId1;
    memcpy(&liveId1, (const void*)(base + kLive + 12 * 1 + 8), 4);
    if (liveId1 != 0) {
        if (log) log("AS: renderer already initialised (async slot table is live) - boot slots NOT added");
        return false;
    }
    uint32_t added = 0;
    if (b.want & 1u) {  // slot 0: enable byte ships 1; the vendor rule clears it on non-NVIDIA unless the gate jumps
        bool ok = gate[0] == 0xEB;
        if (!ok) {
            DWORD old, t;
            if (VirtualProtect((void*)(base + kGate), 2, PAGE_EXECUTE_READWRITE, &old)) {
                memcpy((void*)(base + kGate), kGateNew, 2);
                VirtualProtect((void*)(base + kGate), 2, old, &t);
                FlushInstructionCache(GetCurrentProcess(), (void*)(base + kGate), 2);
                ok = true;
            }
        }
        if (ok && tab[0] == 0) {  // someone cleared it: set it like the other slots
            DWORD old, t;
            if (VirtualProtect(tab, 1, PAGE_READWRITE, &old)) { tab[0] = 1; VirtualProtect(tab, 1, old, &t); }
        }
        b.gateWritten = ok && tab[0] == 1;
        if (b.gateWritten && b.vendor != 0x10DE) added |= 1u;  // NVIDIA: slot 0 is stock-on
    }
    for (int i = 1; i < kSlots; ++i) {
        if (!(b.want & (1u << i))) continue;
        if (tab[12 * i] == 1) continue;  // shipped on (3 4 6): nothing to add
        DWORD old, t;
        if (VirtualProtect(tab + 12 * i, 1, PAGE_READWRITE, &old)) {
            tab[12 * i] = 1;
            VirtualProtect(tab + 12 * i, 1, old, &t);
        }
        if (tab[12 * i] == 1) added |= 1u << i;
    }
    b.added = added;
    b.applied = true;
    if (log) {
        char a[64], w[64], s[64];
        SlotList(a, sizeof a, added);
        SlotList(w, sizeof w, b.want);
        SlotList(s, sizeof s, b.sync);
        log("AS: boot: async slots requested%s -> added%s (vendor %04X; stock = table + vendor rule), kept synchronous while ON%s. "
            "EXPERIMENT: queues are created at the renderer init; KT ships slot 0 on NVIDIA only and 11/12 nowhere",
            w, a, b.vendor, s);
    }
    return false;
}

volatile uint32_t* Switch() { return g_installed ? &as_enable : nullptr; }

void StatusLine(char* buf, unsigned n, uintptr_t base) {
    if (!n) return;
    buf[0] = 0;
    if (!g_installed) return;
    static uint32_t prev[kNumSites] = {};
    static DWORD prevT = 0;
    const DWORD now = GetTickCount();
    const double sec = prevT ? (double)(now - prevT) / 1000.0 : 0.0;
    char calls[200];
    size_t o = 0;
    calls[0] = 0;
    for (int k = 0; k < kNumSites; ++k) {
        if (!(g_siteMask & (1u << k))) continue;
        const uint32_t c = as_calls[k];
        o += (size_t)snprintf(calls + o, sizeof calls - o, " %s(%d) %.1f/s", kSites[k].name, kSites[k].slot,
                              sec > 0 ? (double)(c - prev[k]) / sec : 0.0);
        prev[k] = c;
        if (o >= sizeof calls) break;
    }
    prevT = now;
    char live[64] = " -";
    if (base) {
        const uint8_t* tb = (const uint8_t*)(base + kLive);
        uint32_t m = 0;
        for (int i = 0; i < kSlots; ++i) {
            uint32_t id;
            memcpy(&id, tb + 12 * i + 8, 4);
            if (tb[12 * i] && id == (uint32_t)i) m |= 1u << i;
        }
        SlotList(live, sizeof live, m);
    }
    char a[64], s[64];
    SlotList(a, sizeof a, g_boot.added);
    SlotList(s, sizeof s, g_boot.sync);
    snprintf(buf, n, "AS: switch %s, added%s, kept synchronous while ON%s | producer calls:%s | created slots%s",
             as_enable ? "ON" : "off", a, s, calls[0] ? calls : " -", live);
}

void Start(const StartInfo& info) {
    g_log = info.log;
    if (!g_log || !info.base || !info.boot || !info.boot->mode) return;
    g_boot = *info.boot;
    if (!info.knownBuild) {
        g_log("AS: v2.02 only - skipped on this build");
        return;
    }
    const uintptr_t b = info.base;
    const uint32_t want = (g_boot.added | g_boot.sync) & kSwitchable;
    if (const uint32_t bootOnly = g_boot.added & ~kSwitchable) {
        char l[64];
        SlotList(l, sizeof l, bootOnly);
        g_log("AS: slots%s have no live switch (producers not audited): on for this whole launch", l);
    }
    if (!want) {
        g_log("AS: nothing to switch live (no switchable slot added or kept synchronous)");
        return;
    }
    if (!Same(b, kGetCtx, kGetCtxBytes, sizeof(kGetCtxBytes))) {
        g_log("AS: AsyncSlotGetCtx 52BE1C differs from v2.02 (another mod?) - live switch not installed");
        return;
    }
    const auto* dos = (const IMAGE_DOS_HEADER*)b;
    const auto* nt = (const IMAGE_NT_HEADERS64*)(b + dos->e_lfanew);
    g_page = AllocNear(b, b + nt->OptionalHeader.SizeOfImage, 4096);
    if (!g_page) {
        g_log("AS: no executable page within 2 GB of the image - live switch not installed");
        return;
    }
    void (*const caves[kNumSites])() = {&caveAs0, &caveAs1, &caveAs2, &caveAs3, &caveAs4, &caveAs5};
    for (int k = 0; k < kNumSites; ++k) Tramp(g_page + 0x10 * k, caves[k]);
    FlushInstructionCache(GetCurrentProcess(), g_page, 0x10 * kNumSites);
    as_resume = b + kGetCtxResume;
    as_and_on = ~g_boot.sync;
    as_and_off = ~g_boot.added;
    as_enable = g_boot.mode == 1 ? 1u : 0u;
    Write w[kMaxWrites];
    int n = 0;
    uint32_t sites = 0;
    bool ok = true;
    bool gatesOk = true;
    for (const Gate& g : kSlot0Gates) gatesOk &= Same(b, g.rva, g.b, g.len);
    for (int k = 0; k < kNumSites; ++k) {
        if (!(want & (1u << kSites[k].slot))) continue;
        if (kSites[k].slot == 0 && !gatesOk) {
            g_log("AS: slot-0 join gates (RS+13ACh) differ from v2.02 - no live switch for slot 0 (it stays as created)");
            continue;
        }
        if (!CallTargets(b, kSites[k].rva, b + kGetCtx)) {
            g_log("AS: producer site %X (%s, slot %d) is not `call 52BE1C` - not installed", kSites[k].rva, kSites[k].name, kSites[k].slot);
            ok = false;
            break;
        }
        if (!CallBytes(w[n++], (uint8_t*)(b + kSites[k].rva), g_page + 0x10 * k)) { ok = false; break; }
        sites |= 1u << k;
    }
    if (!ok || !FrozenWrite(w, n)) {
        g_log("AS: producer-site write failed - live switch not installed (boot slots stay as created)");
        as_enable = 0;
        return;
    }
    g_siteMask = sites;
    g_installed = true;
    char l[96];
    size_t o = 0;
    l[0] = 0;
    for (int k = 0; k < kNumSites; ++k)
        if (sites & (1u << k)) o += (size_t)snprintf(l + o, sizeof l - o, " %s(%d)", kSites[k].name, kSites[k].slot);
    g_log("AS: live switch at the producers%s (switch %s; off = stock slot set, on = AS slots)", l, as_enable ? "ON" : "off");
}

}  // namespace asyncslots
