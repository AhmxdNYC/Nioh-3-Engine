// bootnotice.cpp - SkipBootNotice ([Debug] SkipBootNotice, default -1 = on only in hands-free test sessions). See
// bootnotice.h and notes\autosave-notice.md.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bootnotice.h"

namespace bootnotice {
namespace {

// ---- v2.02 (RVAs; patches.json BN1 / BN2, verified by tests\test_patterns.py) ----
constexpr uint32_t kSite1 = 0x1C35393;    // E8 -> SystemMessageAdd E6DA30 (the autosave notice, text 0x3C4ABF1)
constexpr uint32_t kShow = 0xE6DA30;      // SystemMessageAdd(xmm0 unused, wstring* text, u8 confirm); frees *text at its end
constexpr uint32_t kFree = 0x310C18;      // StringReset(wstring*): free the heap buffer when cap >= 8, set it empty
constexpr uint32_t kSite2 = 0x1C353A2;    // E8 -> GfxSettingsTooHeavy 1FE3D38 (the Yes/No "reset all settings" warning)
constexpr uint32_t kCheck = 0x1FE3D38;
constexpr uint32_t kSaveMgr = 0x45C8480;  // SaveMgr* g_SaveMgr; [+10h] = state the boot sequencer waits on in states 7-9
// mov ecx,3C4ABF1h; call 5A4B04; mov rdx,rax; lea rcx,[rbp-50h]; call 310374; mov esi,1; mov rdx,rax; mov r8b,sil; call E6DA30;
// cmp [rdi+55h],r13b; jne; call 1FE3D38; test al,al; je   (rdi = the sequencer object for the whole function)
const uint8_t kSeqBytes[] = {0xB9, 0xF1, 0xAB, 0xC4, 0x03, 0xE8, 0x88, 0xF7, 0x96, 0xFE, 0x48, 0x8B, 0xD0, 0x48, 0x8D, 0x4D, 0xB0,
                             0xE8, 0xEC, 0xAF, 0x6D, 0xFE, 0xBE, 0x01, 0x00, 0x00, 0x00, 0x48, 0x8B, 0xD0, 0x44, 0x8A, 0xC6,
                             0xE8, 0x98, 0x86, 0x23, 0xFF, 0x44, 0x38, 0x6F, 0x55, 0x0F, 0x85, 0xAD, 0x01, 0x00, 0x00,
                             0xE8, 0x91, 0xE9, 0x3A, 0x00, 0x84, 0xC0, 0x0F, 0x84, 0xA0, 0x01, 0x00, 0x00};
constexpr uint32_t kSeqAt = 0x1C35372;
// StringReset head: push rbx; sub rsp,20h; cmp qword [rcx+18h],8; mov rbx,rcx; jb
const uint8_t kFreeBytes[] = {0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x83, 0x79, 0x18, 0x08, 0x48, 0x8B, 0xD9, 0x72};

// thunk page layout
constexpr size_t kT1 = 0x00, kT2 = 0x40;
constexpr size_t kObj = 0x100, kSwitch = 0x108, kSkip1 = 0x10C, kSkip2 = 0x110, kCalls1 = 0x114;
constexpr size_t kPFree = 0x118, kPShow = 0x120, kPCheck = 0x128;

void (*g_log)(const char*, ...) = nullptr;
uint8_t* g_page = nullptr;
uintptr_t g_base = 0;

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

// One interlocked 8-byte compare-exchange (both sites fit one aligned qword: 1C35393..97 in 1C35390, 1C353A2..A6 in 1C353A0).
bool Patch8(uint8_t* p, const uint8_t* bytes, size_t len) {
    const uintptr_t a = (uintptr_t)p, w8 = a & ~uintptr_t(7);
    if (a + len > w8 + 8) return false;
    DWORD old = 0;
    if (!VirtualProtect((void*)w8, 8, PAGE_EXECUTE_READWRITE, &old)) return false;
    volatile LONG64* q = (volatile LONG64*)w8;
    const LONG64 cur = *q;
    LONG64 nw = cur;
    memcpy((uint8_t*)&nw + (a - w8), bytes, len);
    const bool ok = InterlockedCompareExchange64(q, nw, cur) == cur;
    VirtualProtect((void*)w8, 8, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void*)w8, 8);
    return ok;
}

// tiny emitter: rip-relative operands are given as page offsets
struct Emit {
    uint8_t* p;
    size_t at;
    void b(std::initializer_list<uint8_t> v) {
        for (uint8_t x : v) p[at++] = x;
    }
    void rel(size_t target, size_t tail) {  // disp32 to page+target, counted from the end of the instruction (tail bytes follow)
        const int32_t d = (int32_t)((intptr_t)target - (intptr_t)(at + 4 + tail));
        memcpy(p + at, &d, 4);
        at += 4;
    }
};

bool ReadI32(uintptr_t a, int32_t* out) {
    MEMORY_BASIC_INFORMATION mbi;
    if (!a || !VirtualQuery((void*)a, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT ||
        (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) || !(mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                                                                       PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)))
        return false;
    if (a + 4 > (uintptr_t)mbi.BaseAddress + mbi.RegionSize) return false;
    *out = *(volatile int32_t*)a;
    return true;
}
bool ReadPtr(uintptr_t a, uintptr_t* out) {
    int32_t lo = 0, hi = 0;
    if (!ReadI32(a, &lo) || !ReadI32(a + 4, &hi)) return false;
    *out = (uintptr_t)(uint32_t)lo | ((uintptr_t)(uint32_t)hi << 32);
    return true;
}

// Boot sequencer state log: one line per change of [obj+14h] (and of [g_SaveMgr]+10h while in states 7-9), from the first
// time a thunk sees the object until the title flow is past the save load (state >= 0Ch) or 15 minutes.
DWORD WINAPI Watch(void* param) {
    uint8_t* const page = (uint8_t*)param;  // this install's page (a later Start in the same process gets its own)
    const ULONGLONG t0 = GetTickCount64();
    uintptr_t obj = 0;
    int32_t last = -1, lastSm = INT32_MIN;
    long s1 = 0, s2 = 0;
    while (GetTickCount64() - t0 < 15ull * 60 * 1000) {
        Sleep(50);
        const uintptr_t o = *(volatile uintptr_t*)(page + kObj);
        if (o && o != obj) {
            obj = o;
            last = -1;
        }
        const long k1 = *(volatile long*)(page + kSkip1), k2 = *(volatile long*)(page + kSkip2);
        if (k1 > s1) g_log("BootNotice: skipped the 'This game has an autosave feature' notice (#%ld)", s1 = k1);
        if (k2 > s2)
            g_log("BootNotice: skipped the 'Current graphics settings may lead to unstable gameplay - reset to defaults?' "
                  "warning (#%ld; answered as No: settings unchanged)", s2 = k2);
        if (!obj) continue;
        int32_t st = -1;
        if (!ReadI32(obj + 0x14, &st)) continue;
        const double t = (GetTickCount64() - t0) / 1000.0;
        if (st != last) {
            g_log("BootNotice: boot sequencer state %d -> %d (+%.1f s after install)", last, st, t);
            last = st;
        }
        if (st >= 7 && st <= 9) {
            uintptr_t sm = 0;
            int32_t v = 0;
            if (ReadPtr(g_base + kSaveMgr, &sm) && sm && ReadI32(sm + 0x10, &v) && v != lastSm) {
                g_log("BootNotice: save manager state [+10h] = %d (sequencer state %d waits for it)", v, st);
                lastSm = v;
            }
        }
        if (st >= 0x0C && st < 0x40) {
            g_log("BootNotice: boot sequencer past the save-data load (state %d) - state log done", st);
            return 0;
        }
    }
    return 0;
}

}  // namespace

long NoticesSkipped() { return g_page ? *(volatile long*)(g_page + kSkip1) : 0; }
long WarningsSkipped() { return g_page ? *(volatile long*)(g_page + kSkip2) : 0; }

void Start(const StartInfo& info) {
    g_log = info.log;
    if (!g_log || !info.base || !info.iniPath) return;
    g_base = info.base;
    int want = (int)GetPrivateProfileIntW(L"Debug", L"SkipBootNotice", -1, info.iniPath);
    const char* why = "[Debug] SkipBootNotice";
    if (want < 0) {
        const int tour = (int)GetPrivateProfileIntW(L"Debug", L"Tour", 0, info.iniPath);
        const int vp = (int)GetPrivateProfileIntW(L"Debug", L"VirtualPad", 0, info.iniPath);
        want = (tour != 0 || vp != 0) ? 1 : 0;
        why = tour ? "default: on because [Debug] Tour != 0" : vp ? "default: on because [Debug] VirtualPad != 0"
                                                                    : "default: off ([Debug] Tour = VirtualPad = 0)";
    }
    if (want == 0) {
        if (strncmp(why, "default", 7) != 0) g_log("BootNotice: off (%s = 0) - the boot notices are stock", why);
        return;
    }
    if (!info.knownBuild) {
        g_log("BootNotice: not installed (unknown build; addresses are v2.02)");
        return;
    }
    const uint8_t* seq = (const uint8_t*)(info.base + kSeqAt);
    if (memcmp(seq, kSeqBytes, sizeof(kSeqBytes)) != 0 || memcmp((const void*)(info.base + kFree), kFreeBytes, sizeof(kFreeBytes)) != 0) {
        g_log("BootNotice: not installed (boot sequencer code bytes differ - another patch or build)");
        return;
    }
    uint8_t* s1 = (uint8_t*)(info.base + kSite1);
    uint8_t* s2 = (uint8_t*)(info.base + kSite2);
    int32_t r1 = 0, r2 = 0;
    memcpy(&r1, s1 + 1, 4);
    memcpy(&r2, s2 + 1, 4);
    if ((uintptr_t)(s1 + 5 + r1) != info.base + kShow || (uintptr_t)(s2 + 5 + r2) != info.base + kCheck) {
        g_log("BootNotice: not installed (call targets differ)");
        return;
    }
    const auto* dos = (const IMAGE_DOS_HEADER*)info.base;
    const auto* nt = (const IMAGE_NT_HEADERS64*)(info.base + dos->e_lfanew);
    g_page = AllocNear(info.base, info.base + nt->OptionalHeader.SizeOfImage, 4096);
    if (!g_page) {
        g_log("BootNotice: not installed (no executable page within 2 GB of the image)");
        return;
    }
    uint8_t* p = g_page;
    const uint64_t pFree = info.base + kFree, pShow = info.base + kShow, pCheck = info.base + kCheck;
    memcpy(p + kPFree, &pFree, 8);
    memcpy(p + kPShow, &pShow, 8);
    memcpy(p + kPCheck, &pCheck, 8);
    p[kSwitch] = want == 1 ? 1 : 0;

    // T1 (BN1, notice): record rdi; count; if on: StringReset(rdx) instead of SystemMessageAdd (tail jump, returns to 1C35398)
    Emit e{p, kT1};
    e.b({0x48, 0x89, 0x3D}), e.rel(kObj, 0);           // mov [obj], rdi
    e.b({0xF0, 0xFF, 0x05}), e.rel(kCalls1, 0);        // lock inc dword [calls1]
    e.b({0x80, 0x3D}), e.rel(kSwitch, 1), e.b({0x00});  // cmp byte [switch], 0
    const size_t j1 = e.at;
    e.b({0x74, 0x00});                                 // je orig
    e.b({0xF0, 0xFF, 0x05}), e.rel(kSkip1, 0);         // lock inc dword [skip1]
    e.b({0x48, 0x8B, 0xCA});                           // mov rcx, rdx
    e.b({0xFF, 0x25}), e.rel(kPFree, 0);               // jmp [StringReset]
    p[j1 + 1] = (uint8_t)(e.at - (j1 + 2));
    e.b({0xFF, 0x25}), e.rel(kPShow, 0);               // orig: jmp [SystemMessageAdd]
    // T2 (BN2, graphics warning): record rdi; if on: call the check, count a "true", answer 0
    e = Emit{p, kT2};
    e.b({0x48, 0x89, 0x3D}), e.rel(kObj, 0);           // mov [obj], rdi
    e.b({0x80, 0x3D}), e.rel(kSwitch, 1), e.b({0x00});  // cmp byte [switch], 0
    const size_t j2 = e.at;
    e.b({0x74, 0x00});                                 // je orig
    e.b({0x48, 0x83, 0xEC, 0x28});                     // sub rsp, 28h (entry rsp = 8 mod 16 -> aligned call)
    e.b({0xFF, 0x15}), e.rel(kPCheck, 0);              // call [GfxSettingsTooHeavy]
    e.b({0x48, 0x83, 0xC4, 0x28});                     // add rsp, 28h
    e.b({0x84, 0xC0});                                 // test al, al
    const size_t j3 = e.at;
    e.b({0x74, 0x00});                                 // je done (al = 0 already)
    e.b({0xF0, 0xFF, 0x05}), e.rel(kSkip2, 0);         // lock inc dword [skip2]
    e.b({0x31, 0xC0});                                 // xor eax, eax
    p[j3 + 1] = (uint8_t)(e.at - (j3 + 2));
    e.b({0xC3});                                       // done: ret
    p[j2 + 1] = (uint8_t)(e.at - (j2 + 2));
    e.b({0xFF, 0x25}), e.rel(kPCheck, 0);              // orig: jmp [GfxSettingsTooHeavy]
    FlushInstructionCache(GetCurrentProcess(), p, 0x140);

    const intptr_t d1 = (intptr_t)(p + kT1) - (intptr_t)(s1 + 5), d2 = (intptr_t)(p + kT2) - (intptr_t)(s2 + 5);
    if (d1 < INT32_MIN || d1 > INT32_MAX || d2 < INT32_MIN || d2 > INT32_MAX) {
        g_log("BootNotice: not installed (thunk out of rel32 reach)");
        return;
    }
    uint8_t c1[5] = {0xE8}, c2[5] = {0xE8};
    const int32_t n1 = (int32_t)d1, n2 = (int32_t)d2;
    memcpy(c1 + 1, &n1, 4);
    memcpy(c2 + 1, &n2, 4);
    const bool ok1 = Patch8(s1, c1, 5), ok2 = Patch8(s2, c2, 5);
    if (!ok1 || !ok2) {
        // undo the one that went in: both thunks are self-contained, but keep the pair consistent
        uint8_t o1[5] = {0xE8}, o2[5] = {0xE8};
        memcpy(o1 + 1, &r1, 4);
        memcpy(o2 + 1, &r2, 4);
        if (ok1) Patch8(s1, o1, 5);
        if (ok2) Patch8(s2, o2, 5);
        g_log("BootNotice: not installed (site write failed: BN1 %d BN2 %d)", ok1 ? 1 : 0, ok2 ? 1 : 0);
        return;
    }
    if (HANDLE h = CreateThread(nullptr, 0, Watch, g_page, 0, nullptr)) CloseHandle(h);
    g_log("BootNotice: installed (boot sequencer 1C35188 sites BN1 1C35393 notice, BN2 1C353A2 graphics warning), %s - %s",
          want == 1 ? "ON: the autosave notice and the 'reset settings?' warning are skipped (hands-free boot)"
                    : "state log only (stock notices)",
          why);
}

}  // namespace bootnotice
