// resinfoguard.cpp - ResInfoGuard ([General] ResInfoGuard = 1 by default; GitHub #145). See resinfoguard.h.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "gamerva.h"

#include <cstdint>
#include <cstring>

#include "resinfoguard.h"

namespace resinfoguard {
namespace {

void (*g_log)(const char*, ...) = nullptr;
bool g_on = false;

bool Readable(const void* p, size_t n) {
    const uint8_t* b = static_cast<const uint8_t*>(p);
    __try {
        volatile uint8_t x = 0;
        for (size_t i = 0; i < n; ++i) x = b[i];
        (void)x;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Cold block: assert argument setup, call, then `xor al, al; jmp` (the false return).
bool ColdIsAssertThenFalse(const uint8_t* cold) {
    static const uint8_t kHead[] = {0x44, 0x89, 0x5C, 0x24, 0x28, 0x48, 0x8D, 0x0D};
    static const uint8_t kMid[] = {0x41, 0xB8, 0x38, 0x00, 0x00, 0x00, 0x44, 0x89, 0x54, 0x24, 0x20, 0xE8};
    static const char kMsg[] = "Resource Info Header is corrupted";
    if (!Readable(cold, 0x20)) return false;
    if (memcmp(cold, kHead, sizeof kHead) != 0) return false;
    if (memcmp(cold + 12, kMid, sizeof kMid) != 0) return false;
    if (cold[0x1C] != 0x32 || cold[0x1D] != 0xC0 || cold[0x1E] != 0xE9) return false;
    int32_t disp = 0;
    memcpy(&disp, cold + 8, 4);
    const uint8_t* msg = cold + 12 + disp;
    if (!Readable(msg, sizeof kMsg - 1)) return false;
    return memcmp(msg, kMsg, sizeof kMsg - 1) == 0;
}

bool Arm(uint8_t* site) {
    if (!Readable(site, 8)) return false;
    if (site[0] != 0x0F || site[1] != 0x85 || site[6] != 0xB0 || site[7] != 0x01) return false;
    int32_t rel = 0;
    memcpy(&rel, site + 2, 4);
    if (!ColdIsAssertThenFalse(site + 6 + rel)) return false;
    const int32_t neu = rel + 0x1C;  // land on `xor al, al` instead of the assert call
    DWORD old = 0;
    if (!VirtualProtect(site, 8, PAGE_EXECUTE_READWRITE, &old)) return false;
    memcpy(site + 2, &neu, 4);
    DWORD back = 0;
    VirtualProtect(site, 8, old, &back);
    FlushInstructionCache(GetCurrentProcess(), site, 8);
    return true;
}

}  // namespace

bool Installed() { return g_on; }

void Start(const StartInfo& info) {
    g_log = info.log;
    if (!g_log || !info.base || !info.iniPath || g_on) return;
    const int want = static_cast<int>(GetPrivateProfileIntW(L"General", L"ResInfoGuard", 1, info.iniPath));
    if (want == 0) {
        g_log("RESINFOGUARD: off ([General] ResInfoGuard = 0) - a bad resource header still hits the game breakpoint");
        return;
    }
    const uint32_t rva = Grva(kJne);
    if (!rva) {
        g_log("RESINFOGUARD: not installed (resource-header check not found)");
        return;
    }
    uint8_t* site = reinterpret_cast<uint8_t*>(info.base + Grva(kJne));
    if (!Arm(site)) {
        g_log("RESINFOGUARD: not installed (header-check bytes differ - another patch or build)");
        return;
    }
    g_on = true;
    g_log("RESINFOGUARD: installed at +%X (%s) - a resource header whose sizes do not add up is rejected "
          "instead of closing the game (#145) - [General] ResInfoGuard = %d",
          rva, info.knownBuild ? "v2.02" : "found by pattern", want);
}

}  // namespace resinfoguard
