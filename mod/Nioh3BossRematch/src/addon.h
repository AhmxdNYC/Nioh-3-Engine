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

#define ADDON_VERSION "1.4.22"

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
void ForceEnd(const char* why);
double Now();
}
namespace training {
// Copied read-only evidence; no native readiness or mutation API.
struct EntryCounts { uint64_t create, begin, end, pumps; };
struct MissionSnapshot {
    uint64_t sequence = 0, monotonicMs = 0, missionEpoch = 0;
    uint32_t mission = 0, retry = 0;
    bool missionValid = false, retryValid = false;
    bool missionGap = false; // unknown/skipped samples through first recovery
    uint64_t publicationLosses = 0; // sampled loss total; later losses may be absent
    int32_t playMode = 0;
    uint8_t loading = 0, connection = 0;
    bool loadingValid = false, playModeValid = false, connectionValid = false;
    bool loadingGap = false, playModeGap = false, connectionGap = false;
    // Observed raw changes/recovery boundaries, never native attempts/session IDs.
    uint64_t loadGeneration = 0, sessionGeneration = 0;
    bool sessionValid = false, sessionGap = false;
};
enum class EntryKind : uint8_t { Create, Begin, End };
struct EntryEvent {
    uint64_t sequence = 0, monotonicMs = 0;
    EntryKind kind = EntryKind::Create;
    MissionSnapshot observation; // cached before entry; age/validity remain explicit
};
struct EventLossCounts { uint64_t contention, capacity; };
size_t Events(EntryEvent* out, size_t capacity); // retained oldest-first copy, no drain
EventLossCounts EventLosses(); // independent totals; not native event loss
bool Prepare();
void SetPumpLive(bool installed);
void Pump();
bool Live();
EntryCounts Counts();  // independent atomic totals, not a coherent session snapshot
bool Snapshot(MissionSnapshot& out); // cached copy only; false before publication/when off
uint64_t SnapshotLosses(); // contended pump publications; not native event loss
}
namespace rematch {
bool Prepare();  // reads [Rematch], loads catalogue/entry hooks; not pump-live
void SetPumpLive(bool installed);  // Init publishes pump installation outcome
void Pump();  // shared dispatcher callback; no work until successfully prepared + live
bool Live();
// UI model: one UI worker only. The in-game renderer uses the snapshot/action ABI.
struct UiRow {
    char text[112];
    uint32_t color;
    bool sel;
};
int UiFill(UiRow* rows, int cap, char* footer, size_t nf);
void UiMove(int dir);
struct CoverInfo { uint64_t token, deadline; uint32_t phase; char reason[160]; };
bool UiActHosted();
bool UiTakeHostMenu();   // #38077: true while the hosted 'Mod menu' row asked the host to show its own first page
void UiClearHostMenu();
bool UiTestFight(const char* id,uint64_t& token,char* reason,size_t n); // UI-worker only, opt-in test caller
bool CoverSnapshot(CoverInfo& out);
void CoverPresented(uint64_t token);
void CoverCancel(uint64_t token);
bool UiAct();   // A; enter child, confirm existing action; true = close
bool UiBack();  // B; cancel confirmation, then parent; true = close at root
void UiOpen();
void UiOpenAt(int which);  // user #13223: 1 = Boss Rematch page, 2 = Practice page, as the base page (Back closes to the host menu)
void UiGatePage();  // user #6789: the arena Return gate's 2-option page (Return to where I was / Fight again)
void UiChange(int dir);  // Left/Right: edit only a selected startup preference
void UiSettingsPrepare();  // overlay initialization only; no game hooks
bool Toast(char* buf, size_t n, uint32_t& color);
// pipe commands (remote.cpp): returns the reply text
void Command(const char* line, char* reply, size_t n);
}
namespace ui {
void HostedFightCommand(const char* row,char* reply,size_t n);
bool Start();  // overlay window + input (XInputGetState IAT hook for the gesture / masking)
}
namespace remote {
bool Start();  // \\.\pipe\nioh3bossrematch ([Remote] Pipe = 1)
}

}  // namespace addon
