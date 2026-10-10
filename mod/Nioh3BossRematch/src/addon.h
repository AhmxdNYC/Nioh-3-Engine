// addon.h - Nioh3BossRematch: shared declarations (see notes\boss-rematch-addon.md).
// A separate ASI so a game patch that breaks one feature (or the perf ASI) breaks nothing else: every site is resolved by a
// wildcard byte pattern or a script-native name (src/sites_gen.h, tools/derive_sites.py), each feature group self-disables on its
// own, and the add-on never refuses to load because one group failed. Works with or without Nioh3EngineFix.asi.
#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstddef>
#include <cstdint>

#define ADDON_VERSION "1.4.0"

namespace addon {

// ---- log (Nioh3BossRematch.log next to the ASI) ----
void Log(const char* fmt, ...);
const wchar_t* IniPath();
const wchar_t* ModDir();  // with trailing backslash
int IniInt(const wchar_t* sec, const wchar_t* key, int def);

// ---- SEH-guarded memory access (POD only) ----
template <class T>
inline bool Rd(uint64_t a, T& v) {
    if (a < 0x10000) return false;
    __try {
        v = *(const volatile T*)a;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
bool Copy(void* dst, uint64_t src, size_t n);
bool WrU32(uint64_t a, uint32_t v);
bool WrBytes(uint64_t a, const void* src, size_t n);  // VirtualProtect if needed, restores the protection
void Hex(char* out, size_t cap, uint64_t addr, size_t len);

// ---- sites (sites.cpp) ----
struct Image {
    uintptr_t base = 0, end = 0;
    uint32_t timestamp = 0, sizeOfImage = 0;
    bool knownBuild = false;
};
extern Image g_img;
// resolved address of a site by name (0 = not resolved); globals return the variable's address
uintptr_t Site(const char* name);
// a feature group resolved completely? why = the first failure ("pattern not found: X", "2 matches: X", "head differs: X")
bool GroupOk(const char* group, const char** why = nullptr);
void ResolveAll();  // fills the table; logs one line per group
bool CodeReady();  // the code is decrypted (sentinel patterns match)

// ---- hooks (hooks.cpp) ----
using Handler = void (*)(uint64_t rcx, uint64_t rdx, uint64_t r8, uint64_t r9, const uint64_t* sp);
// register a handler on a hook / native site (several handlers per site are fine: one thunk calls them in order)
void OnEntry(const char* site, Handler h);
// install every registered entry hook of `group` (all or nothing per group: a failure leaves the group's installed hooks as
// pass-through and returns false with *why set)
bool InstallGroup(const char* group, const char** why);
// retarget the per-frame call site (PUMP) to call `tick` first, then whatever it called before (chains onto another module)
bool InstallPump(void (*tick)(), const char** why);
// retarget an E8 call site (kind "call") to fn (same signature); *orig = the callee it had
bool InstallCallRetarget(const char* site, void* fn, void** orig, const char** why);

// ---- features ----
namespace bosslog {
void Register(bool boss, bool rm, bool inv);  // handlers; installed by main
void FightBegin(uint64_t ctx);                 // also used by rematch
}
namespace fight {  // FIGHT group state (always registered when any feature needs it)
extern volatile LONG g_fights;          // BossBattleMode starts this session
extern volatile LONG g_active;          // inside BossBattleMode .. End
// placements created in the 60 s before the last BossBattleMode start (" id id ..."), copied under a lock
void LastFightPlacements(char* buf, size_t n);
bool PlacementSeenRecently(uint32_t id, double withinSec);
double Now();
}
namespace rematch {
bool Start();  // reads [Rematch], loads the catalogue, needs PUMP + FIGHT + CORE
bool Live();
// ui side (any thread; snapshots / atomics only)
struct UiRow {
    char text[112];
    uint32_t color;
    bool sel;
};
int UiFill(UiRow* rows, int cap, char* footer, size_t nf);
void UiMove(int dir);
bool UiAct();   // A; true = a request was queued (close the menu)
bool UiBack();  // B; true = leave the menu
void UiOpen();
bool Toast(char* buf, size_t n, uint32_t& color);
// pipe commands (remote.cpp): returns the reply text
void Command(const char* line, char* reply, size_t n);
}
namespace ui {
bool Start();  // overlay window + input (XInputGetState IAT hook for the gesture / masking)
}
namespace remote {
bool Start();  // \\.\pipe\nioh3bossrematch ([Remote] Pipe = 1)
}

}  // namespace addon
