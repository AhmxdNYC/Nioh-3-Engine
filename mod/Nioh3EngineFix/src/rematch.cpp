// rematch.cpp - Boss rematch Phase 1 MVP (route R-a + gates + scan + hidden-duel un-hide + sub-page), [Rematch] Enable = 0 by
// default. See rematch.h and notes\boss-rematch-build.md. All game calls and writes run on the game thread (MissionLoadStep
// entry hook); the menu (Present thread) only posts a request word and reads snapshots.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>

#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

#include "rematch.h"

namespace rematch {
namespace {

void (*g_log)(const char*, ...) = nullptr;
uintptr_t g_base = 0, g_imgEnd = 0;
int (*g_pauseReason)() = nullptr;

// ---- v2.02 addresses (code references byte-checked in kSites; the rest are the same reads bosslog/vpad already make) ----
constexpr uint32_t kRvaMenuMgr = 0x45C84A8;       // g_MenuMgr (disp in 1DE28E8); +0x4E0 = mission replay menu object
constexpr uint32_t kOffReplayMenu = 0x4E0;
constexpr uint32_t kRvaOpenReplay = 0x1DE28E8;    // MenuOpenMissionReplay(owner, u32, u8): [g_MenuMgr+0x4E0] ? 1DDA258(menu, ...) : nop
constexpr uint32_t kRvaClearedGet = 0x702B64;     // MissionClearedGetSlot(db, id) = Mission::IsMissionAlreadyClear (current slot)
constexpr uint32_t kRvaUnlockedGet = 0x909218;    // MissionUnlockedGet(db, id): logged next to it on the first scan (s.4 conflict)
constexpr uint32_t kRvaClearDb = 0x4751848;       // rcx = [0x4751848] (list builder 378C93)
constexpr uint32_t kRvaDataMgr = 0x45B9E30;       // [[0x45B9E30]] = {records*, ..}: count u32 at +4, record i at +8 + i*0x198
constexpr uint32_t kRecSize = 0x198, kRecFlags = 0x40, kRecId = 0x158, kFlagHidden = 0x4000;
// The replay list builder skips a record when +0x40 has bit 1, 0x40 or 0x4000 (378B83). The three duels carry 0x24180: their
// "never listed" bit is 0x4000, not 0x40 as boss-rematch-design.md s.5.5 / battle-scrolls.md wrote (flags40 column of the CSV).
constexpr uint32_t kRvaPm = 0x4709B50;            // online session: +0x34 == 1 / 2 = online (IsOnlinePlay 85834)
constexpr uint32_t kRvaNet = 0x45C84F8;           // net: +0x1DC != 0 = in a co-op session
constexpr uint32_t kRvaQueue = 0x45BC420;         // mission-start queue: 3 x 0x58 (free-slot scan 2239549: +4 == 0 && u8 +8 == 0)
// A consumed slot is NOT cleared: every RematchLog capture shows slot 0 = {+0 1, +8 E0, +C stage} from the save load for the
// whole session. So "busy" here = the slot bytes changed within the last kQueueQuietMs (a request was just made), and a replay
// start is detected as any change of the 3 slots after the menu opened.
constexpr ULONGLONG kQueueQuietMs = 10000;
constexpr uint32_t kRvaChMgr = 0x45B9E40;         // [[..]] char manager: +0x2C18 = in an Emaki (Picture Scroll) mission
constexpr uint32_t kRvaGameMain = 0x4759910;      // G: +0x588.. stop flags (58A loading, 58B / 58C paused)
constexpr uint32_t kRvaPlayerTab = 0x4745348;     // player 0 = [P + 0xCD * 0x18] (vpad.cpp / tour.cpp)
constexpr uint32_t kRvaMissionState = 0x47514D0;  // [[..]+8] mission state: +0x14 running mission id
const uint16_t kHiddenIds[3] = {0xD22D, 0xB3F8, 0xFB96};  // Honda Tadakatsu, Yoshitsune, Jubei/Chuya (record +0x40 bit 0x4000)

constexpr uint32_t cWhite = 0xFFFFFFFF, cGray = 0xFFB4B4B4, cDim = 0xFF8C8C8C, cGreen = 0xFF6CFF6C, cYellow = 0xFF40E6FF,
                   cOrange = 0xFF3C8CFF, cCyan = 0xFFFFE08C;

ULONGLONG Ms() { return GetTickCount64(); }

// ---- SEH helpers (POD only) ----
template <class T>
bool Rd(uint64_t a, T& v) {
    if (a < 0x10000) return false;
    __try {
        v = *(const volatile T*)a;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
bool Copy(void* dst, uint64_t src, size_t n) {
    __try {
        memcpy(dst, (const void*)src, n);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
bool WrU32(uint64_t a, uint32_t v) {
    __try {
        *(volatile uint32_t*)a = v;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
bool InImage(uint64_t a) { return a >= g_base && a < g_imgEnd; }

// ---- config ----
struct Cfg {
    bool enable = false, hiddenDuels = false, directStart = false, showAll = false, showUnverified = false;
} g_cfg;
bool g_live = false;

// ---- catalogue (read-only after Start) ----
struct Row {
    char name[64], mission[64], region[24], route[16];
    uint16_t missionId;
    bool hidden, verified;
};
std::vector<Row> g_rows;
constexpr int kMaxRows = 256;
std::atomic<uint8_t> g_status[kMaxRows];  // 0 unknown, 1 not beaten, 2 beaten, 3 getter failed
std::atomic<uint32_t> g_scanGen{0};
std::atomic<int> g_scanNote{0};            // 0 ok, 1 no save loaded, 2 clear db null
std::atomic<bool> g_wantScan{false};
int g_scanCount = 0;                       // game thread

// ---- toast (any thread) ----
SRWLOCK g_toastLock = SRWLOCK_INIT;
char g_toast[160] = "";
uint32_t g_toastColor = cWhite;
ULONGLONG g_toastUntil = 0;
void ToastF(uint32_t color, ULONGLONG ms, const char* fmt, ...) {
    char b[160];
    va_list a;
    va_start(a, fmt);
    vsnprintf(b, sizeof(b), fmt, a);
    va_end(a);
    AcquireSRWLockExclusive(&g_toastLock);
    memcpy(g_toast, b, sizeof(b));
    g_toastColor = color;
    g_toastUntil = Ms() + ms;
    ReleaseSRWLockExclusive(&g_toastLock);
}

// ---- requests (menu -> game thread) and state ----
enum ReqKind : uint32_t { kReqNone = 0, kReqOpen = 1, kReqDirect = 2, kReqAbort = 3 };
std::atomic<uint32_t> g_req{0};  // kind | (row + 1) << 8
enum State : int { kIdle = 0, kMenuOpen = 1, kInReplay = 2 };
std::atomic<int> g_state{kIdle};
std::atomic<bool> g_inFight{false};
std::atomic<ULONGLONG> g_fightAt{0}, g_lastTick{0};
std::atomic<int> g_activeRow{-1};
std::atomic<bool> g_replayIn{false};  // the replay mission is running (abort no longer allowed)

std::atomic<ULONGLONG> g_qChangedAt{0};  // game thread: last time the queue slots changed
uint64_t g_qHash = 0, g_qHashAtOpen = 0;

void Post(uint32_t kind, int row, const char* via) {
    g_req.store(kind | ((uint32_t)(row + 1) << 8));
    g_log("Rematch: request %s%s%s queued from %s", kind == kReqOpen ? "R-a open replay menu" : kind == kReqDirect ? "R-b direct start" : "abort",
          row >= 0 ? " for " : "", row >= 0 ? g_rows[(size_t)row].name : "", via);
}

// ---- snapshot of the game state (memory reads only; any thread) ----
struct Snap {
    bool gm = false, player = false, msOk = false, queueBusy = false, emaki = false, online = false, guest = false;
    uint64_t qHash = 0;  // FNV-1a of the 3 queue slots
    uint8_t st[6] = {};
    uint32_t ms14 = 0;
};
Snap ReadSnap() {
    Snap s;
    uint64_t gm = 0;
    if (Rd(g_base + kRvaGameMain, gm) && gm) s.gm = Copy(s.st, gm + 0x588, 6);
    uint64_t pt = 0, pl = 0;
    if (Rd(g_base + kRvaPlayerTab, pt) && pt && Rd(pt + 0xCD * 0x18, pl) && pl) s.player = true;
    uint64_t g = 0, ms = 0;
    if (Rd(g_base + kRvaMissionState, g) && g && Rd(g + 8, ms) && ms) s.msOk = Rd(ms + 0x14, s.ms14);
    uint8_t q[3 * 0x58];
    if (Copy(q, g_base + kRvaQueue, sizeof(q))) {
        uint64_t h = 0xCBF29CE484222325ull;
        for (uint8_t c : q) h = (h ^ c) * 0x100000001B3ull;
        s.qHash = h;
    }
    s.queueBusy = Ms() - g_qChangedAt.load() < kQueueQuietMs;
    uint64_t a = 0, ch = 0;
    uint8_t e = 0;
    if (Rd(g_base + kRvaChMgr, a) && a && Rd(a, ch) && ch && Rd(ch + 0x2C18, e)) s.emaki = e != 0;
    uint64_t pm = 0, net = 0;
    int32_t mode = 0;
    uint8_t conn = 0;
    if (Rd(g_base + kRvaPm, pm) && pm && Rd(pm + 0x34, mode)) s.online = mode == 1 || mode == 2;
    if (Rd(g_base + kRvaNet, net) && net && Rd(net + 0x1DC, conn)) s.guest = conn != 0;
    return s;
}

// s.2 gates; nullptr = all hold. `menuSide`: also says when the game-thread pump has not run lately.
const char* Gates(const Snap& s, bool menuSide) {
    if (!g_live) return "not installed";
    if (s.online) return "online";
    if (s.guest) return "co-op session";
    if (!s.gm) return "game not ready";
    if (s.st[2]) return "loading";
    if (s.st[3] | s.st[4]) return "game paused";
    if (!s.player) return "no save loaded";
    if (s.queueBusy) return "mission queue busy";
    if (s.emaki) return "in a Picture Scroll";
    if (g_inFight.load()) return "in a boss fight";
    if (g_pauseReason && g_pauseReason() == 1) return "game menu open";
    if (g_state.load() != kIdle) return "rematch active";
    if (menuSide && Ms() - g_lastTick.load() > 2000) return "not in game (no mission tick)";
    return nullptr;
}

// ---- game calls (game thread only) ----
// The replay menu object's vcall +0x48: BattleScrollTop_Select calls it right before its MenuOpenMissionReplay(menu, 0, 6) and skips the
// open when it returns non-zero (1DAFF17), i.e. "already open / busy". 1 / 0, or < 0: 1 menu manager null, 2 replay menu null,
// 3 vtable not in the image, 5 exception.
int ReplayMenuBusy() {
    __try {
        const uint64_t mm = *(const volatile uint64_t*)(g_base + kRvaMenuMgr);
        if (!mm) return -1;
        const uint64_t menu = *(const volatile uint64_t*)(mm + kOffReplayMenu);
        if (!menu) return -2;
        const uint64_t vt = *(const volatile uint64_t*)menu;
        const uint64_t f48 = InImage(vt) ? *(const volatile uint64_t*)(vt + 0x48) : 0;
        if (!InImage(vt) || !InImage(f48)) return -3;
        return (((uint8_t(*)(uint64_t))f48)(menu) & 0xFF) != 0 ? 1 : 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return -5;
    }
}
// 0 ok; 1 menu manager null, 2 replay menu null, 3 vtable not in the image, 4 already open, 5 exception
int CallOpenReplay() {
    const int busy = ReplayMenuBusy();
    if (busy < 0) return -busy;
    if (busy) return 4;
    __try {
        ((void (*)(uint64_t, uint32_t, uint8_t))(g_base + kRvaOpenReplay))(0, 0, 6);  // = UI::OpenMissionReplayMenu 23F90CC
        return 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 5;
    }
}
// pure getter (db, id) -> al; false = exception
bool CallGetter(uint32_t rva, uint64_t db, uint32_t id, uint8_t& out) {
    __try {
        out = (uint8_t)(((uint64_t (*)(uint64_t, uint32_t))(g_base + rva))(db, id) & 0xFF);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// ---- mission records + hidden-duel un-hide (game thread) ----
uint64_t FindMissionRecord(uint16_t id) {
    uint64_t dm = 0, tab = 0;
    uint32_t count = 0;
    if (!Rd(g_base + kRvaDataMgr, dm) || !dm || !Rd(dm, tab) || !tab || !Rd(tab + 4, count) || count == 0 || count > 1000) return 0;
    for (uint32_t i = 0; i < count; ++i) {
        uint16_t v = 0;
        const uint64_t rec = tab + 8 + (uint64_t)i * kRecSize;
        if (!Rd(rec + kRecId, v)) return 0;
        if (v == id) return rec;
    }
    return 0;
}
struct Unhide {
    uint64_t addr;
    uint32_t orig, written;
    DWORD oldProt;
    bool prot;
    uint16_t id;
};
Unhide g_unh[3];
std::atomic<int> g_unhN{0};  // written by the game thread, read by the menu

int UnhideApply() {
    int n = 0;
    char note[160] = "";
    size_t nl = 0;
    for (uint16_t id : kHiddenIds) {
        const uint64_t rec = FindMissionRecord(id);
        uint32_t v = 0;
        if (!rec || !Rd(rec + kRecFlags, v)) {
            nl += (size_t)snprintf(note + nl, sizeof(note) - nl, " %04X:no-record", id);
            continue;
        }
        if (!(v & kFlagHidden)) {
            nl += (size_t)snprintf(note + nl, sizeof(note) - nl, " %04X:already-listed(%08X)", id, v);
            continue;
        }
        Unhide& u = g_unh[n];
        u = Unhide{rec + kRecFlags, v, v & ~kFlagHidden, 0, false, id};
        MEMORY_BASIC_INFORMATION mbi;
        const DWORD rw = PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY;
        if (VirtualQuery((void*)u.addr, &mbi, sizeof(mbi)) && !(mbi.Protect & rw)) {
            if (!VirtualProtect((void*)u.addr, 4, PAGE_READWRITE, &u.oldProt)) {
                nl += (size_t)snprintf(note + nl, sizeof(note) - nl, " %04X:protect-failed", id);
                continue;
            }
            u.prot = true;
        }
        if (!WrU32(u.addr, u.written)) {
            if (u.prot) VirtualProtect((void*)u.addr, 4, u.oldProt, &u.oldProt);
            nl += (size_t)snprintf(note + nl, sizeof(note) - nl, " %04X:write-failed", id);
            continue;
        }
        nl += (size_t)snprintf(note + nl, sizeof(note) - nl, " %04X:%08X->%08X%s", id, u.orig, u.written, u.prot ? "(prot)" : "");
        ++n;
    }
    g_unhN.store(n);
    g_log("Rematch: HiddenDuels: %d of 3 mission records un-hidden (+0x40 bit 0x4000 cleared, game data, never saved):%s", n, note);
    return n;
}
void UnhideRestore(const char* why) {
    const int n = g_unhN.load();
    if (!n) return;
    char note[200] = "";
    size_t nl = 0;
    for (int i = 0; i < n; ++i) {
        Unhide& u = g_unh[i];
        uint32_t cur = 0;
        Rd(u.addr, cur);
        const uint32_t back = cur == u.written ? u.orig : (cur | (u.orig & kFlagHidden));  // someone else wrote it: put our bit back only
        const bool ok = WrU32(u.addr, back);
        if (u.prot) VirtualProtect((void*)u.addr, 4, u.oldProt, &u.oldProt);
        nl += (size_t)snprintf(note + nl, sizeof(note) - nl, " %04X:%08X->%08X%s%s", u.id, cur, back, cur == u.written ? "" : "(changed by game!)",
                               ok ? "" : "(WRITE FAILED)");
    }
    g_log("Rematch: HiddenDuels restored (%s):%s", why, note);
    g_unhN.store(0);
}

// ---- cleared-mission scan (game thread) ----
void DoScan(const Snap& s) {
    if (!s.player || s.st[2]) {
        g_scanNote.store(1);
        g_scanGen.fetch_add(1);
        return;
    }
    uint64_t db = 0;
    if (!Rd(g_base + kRvaClearDb, db) || !db) {
        g_scanNote.store(2);
        g_scanGen.fetch_add(1);
        return;
    }
    const bool detail = g_scanCount++ == 0;
    int beaten = 0, fails = 0;
    for (size_t i = 0; i < g_rows.size() && i < (size_t)kMaxRows; ++i) {
        uint8_t c = 0, u = 0;
        const bool ok = CallGetter(kRvaClearedGet, db, g_rows[i].missionId, c);
        g_status[i].store(ok ? ((c & 1) ? 2 : 1) : 3);
        beaten += ok && (c & 1);
        fails += !ok;
        if (detail) {
            const bool uok = CallGetter(kRvaUnlockedGet, db, g_rows[i].missionId, u);
            g_log("Rematch: scan mission 0x%04X %-36s cleared(702B64)=%s unlocked(909218)=%s", g_rows[i].missionId, g_rows[i].mission,
                  ok ? (c & 1 ? "1" : "0") : "ERR", uok ? (u & 1 ? "1" : "0") : "ERR");
        }
    }
    g_scanNote.store(0);
    g_scanGen.fetch_add(1);
    g_log("Rematch: scan #%d: %d of %zu catalogue missions cleared on the current journey%s", g_scanCount, beaten, g_rows.size(),
          fails ? " (some getter calls FAILED)" : "");
}

// ---- execution + lifecycle (game thread) ----
uint32_t g_pend = 0;
ULONGLONG g_pendAt = 0, g_openAt = 0, g_closedSince = 0, g_replayAt = 0, g_exitSince = 0, g_loadDoneAt = 0;
bool g_menuSeen = false, g_loadSeen = false;
uint32_t g_msAtOpen = 0, g_replayMs = 0;
int g_busyLogged = -9;

const char* RowName(int row) { return row >= 0 && (size_t)row < g_rows.size() ? g_rows[(size_t)row].name : ""; }

void Refuse(const char* why) {
    g_log("Rematch: refused: %s", why);
    ToastF(cOrange, 6000, "Rematch refused: %s", why);
}
void ToIdle(const char* why) {
    UnhideRestore(why);
    g_state.store(kIdle);
    g_replayIn.store(false);
    g_activeRow.store(-1);
}

void ExecOpen(int row, const Snap& s) {
    const int unh = g_cfg.hiddenDuels ? UnhideApply() : 0;
    const int r = CallOpenReplay();
    if (r != 0) {
        static const char* const kWhy[] = {"", "menu manager not ready", "replay menu object not created", "replay menu vtable not in the image",
                                           "the replay menu is already open", "exception in the game call"};
        UnhideRestore("open failed");
        Refuse(kWhy[r]);
        return;
    }
    g_state.store(kMenuOpen);
    g_activeRow.store(row);
    g_openAt = Ms();
    g_menuSeen = false;
    g_closedSince = 0;
    g_busyLogged = -9;
    g_msAtOpen = s.ms14;
    g_qHashAtOpen = s.qHash;
    g_log("Rematch: R-a MenuOpenMissionReplay(0, 0, 6) called (row %d %s, mission 0x%04X '%s'; mission now %u; %d hidden duel(s) un-hidden)", row,
          RowName(row), row >= 0 ? g_rows[(size_t)row].missionId : 0, row >= 0 ? g_rows[(size_t)row].mission : "", s.ms14, unh);
    if (row >= 0) ToastF(cGreen, 8000, "Rematch: pick '%s' in the replay list", g_rows[(size_t)row].mission);
    else ToastF(cGreen, 5000, "Rematch: replay menu opened");
}

// R-b (DirectStart = 1). Design s.5.5: replay the official request verbatim, MissionRequestEnqueue(params.lo8, &req58) 0x223953C (or
// MissionRequestById 0x2239600), with the 0x58-byte request copied from the L7 log of a MENU-started replay of the same mission and only
// the mission id changed; success = a queue slot holds the id at +4.
// TODO(Phase 0, Q10): only the Continue/load path has been logged so far (L7 MissionRequestEnqueue from the save load). Needs an
// `RematchLog L7: MissionRequestById / MissionRequestEnqueue` capture from a real Battle Scroll replay start (tools\rematch\p0_vpad.ps1),
// then a request template + mission-id offset here, and test R-13. Until then this path refuses and starts nothing.
void ExecDirect(int row) {
    g_log("Rematch: R-b direct start for %s (mission 0x%04X) not built: no L7 capture of a menu-started replay yet (Q10)", RowName(row),
          row >= 0 ? g_rows[(size_t)row].missionId : 0);
    Refuse("direct start needs the L7 replay capture (set DirectStart = 0 to use the replay menu)");
}

void Track(const Snap& s, ULONGLONG t) {
    const int st = g_state.load();
    if (st == kMenuOpen) {
        // open = the game's own menu signal (menu.cpp, 5A37D4 / D63C0) OR the replay menu's +0x48 "busy" vcall
        const int busy = ReplayMenuBusy();
        if (busy != g_busyLogged) {
            g_log("Rematch: replay menu +0x48 = %d, game menu signal %d, %.1f s after the open call", busy, g_pauseReason ? g_pauseReason() : -1,
                  (double)(t - g_openAt) / 1000.0);
            g_busyLogged = busy;
        }
        const bool menu = (g_pauseReason && g_pauseReason() == 1) || busy == 1;
        if (menu) {
            g_menuSeen = true;
            g_closedSince = 0;
        }
        const bool enq = s.qHash != g_qHashAtOpen;
        if (enq || s.st[2] || (s.msOk && s.ms14 != g_msAtOpen)) {
            g_state.store(kInReplay);
            g_replayAt = t;
            g_replayMs = 0;
            g_exitSince = 0;
            g_loadSeen = s.st[2] != 0;
            g_loadDoneAt = 0;
            g_log("Rematch: replay start seen (queue slots changed %d, loading %u, mission %u -> %u) %.1f s after the menu opened", enq, s.st[2],
                  g_msAtOpen, s.ms14, (double)(t - g_openAt) / 1000.0);
            return;
        }
        if (!g_menuSeen) {
            if (t - g_openAt > 6000) {
                g_log("Rematch: neither the game menu signal nor +0x48 showed the replay menu (6 s) - back to idle");
                ToastF(cOrange, 6000, "Rematch: the replay menu did not open");
                ToIdle("replay menu not seen");
            }
            return;
        }
        if (!menu) {
            if (!g_closedSince) g_closedSince = t;
            else if (t - g_closedSince > 5000) {  // 5 s: a start may fade out before it enqueues
                g_log("Rematch: replay menu closed without a start");
                ToIdle("replay menu closed");
            }
        }
        return;
    }
    if (st != kInReplay) return;
    if (!g_replayMs) {  // the replay mission = the running mission id once its load has finished (3 s settled)
        if (s.st[2]) {
            g_loadSeen = true;
            g_loadDoneAt = 0;
        } else if (g_loadSeen && s.player && s.msOk) {
            if (!g_loadDoneAt) g_loadDoneAt = t;
            else if (t - g_loadDoneAt > 3000) {
                g_replayMs = s.ms14;
                g_replayIn.store(true);
                g_log("Rematch: IN replay mission %u (0x%X) %.1f s after the start%s", s.ms14, s.ms14, (double)(t - g_replayAt) / 1000.0,
                      s.ms14 == g_msAtOpen ? " (same id as before the menu)" : "");
            }
        }
        if (!g_replayMs && t - g_replayAt > 180000) {
            g_log("Rematch: no replay mission load within 180 s - back to idle");
            ToIdle("replay load timeout");
        }
        return;
    }
    if (s.msOk && s.ms14 == g_replayMs && !s.st[2]) {
        g_exitSince = 0;
        return;
    }
    if (!g_exitSince) g_exitSince = t;
    else if (t - g_exitSince > 5000) {
        g_log("Rematch: replay mission %u exited (mission now %u, loading %u)", g_replayMs, s.ms14, s.st[2]);
        if (g_unhN.load()) ToastF(cGray, 5000, "Rematch: hidden duels restored");
        ToIdle("replay mission exit");
    }
}

void Tick() {
    const ULONGLONG t = Ms();
    g_lastTick.store(t);
    Snap s = ReadSnap();
    if (s.qHash != g_qHash) {
        if (g_qHash) g_qChangedAt.store(t);
        g_qHash = s.qHash;
        s.queueBusy = g_qChangedAt.load() == t;
    }
    if (g_inFight.load() && (s.st[2] || !s.player || t - g_fightAt.load() > 30 * 60 * 1000ull)) g_inFight.store(false);
    if (g_wantScan.exchange(false)) DoScan(s);
    if (const uint32_t r = g_req.exchange(0)) {
        if (g_pend) Refuse("a request is already pending");
        else {
            g_pend = r;
            g_pendAt = t;
        }
    }
    if (g_pend) {
        const uint32_t kind = g_pend & 0xFF;
        const int row = (int)(g_pend >> 8) - 1;
        if (kind == kReqAbort) {
            g_pend = 0;
            if (g_state.load() == kIdle) Refuse("nothing active");
            else if (g_replayIn.load()) Refuse("inside the replay mission (it restores on exit)");
            else {
                g_log("Rematch: abort requested - restoring now");
                ToastF(cGray, 4000, "Rematch: aborted, restored");
                ToIdle("abort");
            }
        } else if (const char* why = Gates(s, false)) {
            if (t - g_pendAt > 5000) {
                g_pend = 0;
                Refuse(why);
            }
        } else {
            g_pend = 0;
            if (kind == kReqDirect) ExecDirect(row);
            else ExecOpen(row, s);
        }
    }
    Track(s, t);
}

// ---- hooks: rcx, rdx, r8, r9 = the hooked function's own arguments; sp = its entry rsp ----
void HkTick(uint64_t, uint64_t, uint64_t, uint64_t, const uint64_t*) { Tick(); }
void HkFightBegin(uint64_t, uint64_t, uint64_t, uint64_t, const uint64_t*) {
    g_fightAt.store(Ms());
    g_inFight.store(true);
}
void HkFightEnd(uint64_t, uint64_t, uint64_t, uint64_t, const uint64_t*) { g_inFight.store(false); }

// ---- sites (tests\test_patterns.py checks every entry against the dump and patches.json) ----
// head = prologue bytes: relocated by our thunk (fn != nullptr), or bytes another module (bosslog.cpp) may have replaced with
// E9 rel32 + NOPs; bytes [head, len) must always match. fn == nullptr: a call target / code reference checked only.
struct Site {
    const char* name;
    uint32_t rva;
    uint8_t head, len;
    uint8_t b[48];
    void (*fn)(uint64_t, uint64_t, uint64_t, uint64_t, const uint64_t*);
};
const Site kSites[] = {
    {"MissionLoadStep", 0x223AF50, 10, 21, {0x48, 0x89, 0x5C, 0x24, 0x18, 0x48, 0x89, 0x6C, 0x24, 0x20, 0x56, 0x57, 0x41, 0x56, 0x48, 0x81, 0xEC, 0xB0, 0x00, 0x00, 0x00}, &HkTick},
    {"BossBattleMode", 0x23A2DCC, 5, 14, {0x48, 0x89, 0x5C, 0x24, 0x10, 0x55, 0x48, 0x8D, 0xAC, 0x24, 0x70, 0xFF, 0xFF, 0xFF}, &HkFightBegin},
    {"BossBattleModeEnd", 0x23A2F70, 9, 19, {0x40, 0x53, 0x48, 0x81, 0xEC, 0x60, 0x01, 0x00, 0x00, 0x48, 0x8B, 0x05, 0xB0, 0x4F, 0x11, 0x02, 0x48, 0x33, 0xC4}, &HkFightEnd},
    {"MenuOpenMissionReplay", 0x1DE28E8, 0, 45, {0x48, 0x83, 0xEC, 0x28, 0x48, 0x8B, 0x05, 0xB5, 0x5B, 0x7E, 0x02, 0x4C, 0x8B, 0xD1, 0x48, 0x8B, 0x88, 0xE0, 0x04, 0x00, 0x00, 0x48, 0x85, 0xC9, 0x74, 0x0E, 0x45, 0x8A, 0xC8, 0x44, 0x8B, 0xC2, 0x49, 0x8B, 0xD2, 0xE8, 0x48, 0x79, 0xFF, 0xFF, 0x48, 0x83, 0xC4, 0x28, 0xC3}, nullptr},
    {"MissionClearedGetSlot", 0x702B64, 10, 19, {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x18, 0x89, 0x50, 0x10, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0x59, 0x08}, nullptr},
    {"MissionUnlockedGet", 0x909218, 14, 20, {0x4C, 0x8B, 0x49, 0x08, 0x49, 0xB8, 0x25, 0x23, 0x22, 0x84, 0xE4, 0x9C, 0xF2, 0xCB, 0x33, 0xC9, 0x89, 0x54, 0x24, 0x10}, nullptr},
    {"MissionRecordGet", 0x377B38, 9, 43, {0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0xD9, 0x48, 0x8B, 0x49, 0x20, 0xE8, 0x42, 0x00, 0x00, 0x00, 0x48, 0x8B, 0x13, 0x3B, 0x42, 0x04, 0x72, 0x08, 0x33, 0xC0, 0x48, 0x83, 0xC4, 0x20, 0x5B, 0xC3, 0x8B, 0xC0, 0x48, 0x69, 0xC8, 0x98, 0x01, 0x00, 0x00}, nullptr},
    {"IsOnlinePlay", 0x85834, 0, 22, {0x48, 0x83, 0xEC, 0x28, 0x48, 0x8B, 0x05, 0x11, 0x43, 0x68, 0x04, 0x33, 0xC9, 0x48, 0x85, 0xC0, 0x74, 0x06, 0x83, 0x78, 0x34, 0x01}, nullptr},
    {"MissionQueueScan", 0x2239549, 0, 25, {0x48, 0x8D, 0x0D, 0xD8, 0x2E, 0x38, 0x02, 0x45, 0x33, 0xC0, 0x83, 0x79, 0xFC, 0x00, 0x75, 0x05, 0x80, 0x39, 0x00, 0x74, 0x17, 0x48, 0x83, 0xC1, 0x58}, nullptr},
    {"ClearDbRef_ListBuild", 0x378C93, 0, 19, {0x48, 0x8B, 0x0D, 0xAE, 0x8B, 0x3D, 0x04, 0x41, 0x0F, 0xB7, 0x34, 0x24, 0x8B, 0xD6, 0xE8, 0xBE, 0x9E, 0x38, 0x00}, nullptr},
    {"MissionTableRef_RequestById", 0x223963C, 0, 15, {0x48, 0x8B, 0x0D, 0xED, 0x07, 0x38, 0x02, 0x8B, 0xD7, 0x41, 0x8A, 0xF4, 0x48, 0x8B, 0x09}, nullptr},
    {"ListFilterRef_ListBuild", 0x378B83, 0, 23, {0x41, 0xF6, 0x03, 0x01, 0x4C, 0x89, 0x45, 0x80, 0x75, 0x55, 0x41, 0xF6, 0x03, 0x40, 0x75, 0x4F, 0x41, 0xF7, 0x03, 0x00, 0x40, 0x00, 0x00}, nullptr},
    {"MissionIdRef_ShrineItem", 0x1D9FDD5, 0, 15, {0x0F, 0xB7, 0x93, 0x58, 0x01, 0x00, 0x00, 0x48, 0x8B, 0xC8, 0xE8, 0x80, 0x2D, 0x96, 0xFE}, nullptr},
};

// 0 = original bytes, 1 = another module's entry hook over `head` (*prev = its thunk), -1 = mismatch
int Probe(const Site& s, uint64_t* prev) {
    uint8_t cur[48];
    const uint64_t p = g_base + s.rva;
    if (!Copy(cur, p, s.len)) return -1;
    if (memcmp(cur + s.head, s.b + s.head, (size_t)(s.len - s.head)) != 0) return -1;
    if (memcmp(cur, s.b, s.head) == 0) return 0;
    if (s.head < 5 || cur[0] != 0xE9) return -1;
    for (int i = 5; i < s.head; ++i)
        if (cur[i] != 0x90) return -1;
    int32_t rel = 0;
    memcpy(&rel, cur + 1, 4);
    const uint64_t t = p + 5 + (int64_t)rel;
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery((void*)t, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT || mbi.Type != MEM_PRIVATE ||
        !(mbi.Protect & (PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)))
        return -1;
    if (prev) *prev = t;
    return 1;
}

// ---- patching (same scheme as bosslog.cpp / loadstats.cpp) ----
uint8_t* g_thunks = nullptr;
size_t g_thunkUsed = 0;
constexpr size_t kThunkPage = 4096;
constexpr size_t kThunkSize = 176;

uint8_t* AllocIn(uintptr_t from, uintptr_t to) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    const uintptr_t g = si.dwAllocationGranularity;
    MEMORY_BASIC_INFORMATION mbi;
    for (uintptr_t a = (from + g - 1) & ~(g - 1); a < to;) {
        if (!VirtualQuery((void*)a, &mbi, sizeof(mbi))) break;
        const uintptr_t rb = (uintptr_t)mbi.BaseAddress, re = rb + mbi.RegionSize;
        if (mbi.State == MEM_FREE) {
            const uintptr_t c = (rb + g - 1) & ~(g - 1);
            if (c >= from && c + kThunkPage <= re && c + kThunkPage <= to)
                if (void* p = VirtualAlloc((void*)c, kThunkPage, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE)) return (uint8_t*)p;
        }
        if (re <= a) break;
        a = re;
    }
    return nullptr;
}
uint8_t* AllocNear(uintptr_t lo, uintptr_t hi) {
    const uintptr_t reach = 0x7FF00000ull - (hi - lo);
    if (uint8_t* p = AllocIn(hi, hi + reach)) return p;
    return AllocIn(lo > reach + 0x10000 ? lo - reach : 0x10000, lo);
}
bool PatchAtomic(uint8_t* p, const uint8_t* bytes, size_t len) {
    const uintptr_t a = (uintptr_t)p, w8 = a & ~uintptr_t(7), w16 = a & ~uintptr_t(15);
    const bool fit8 = a + len <= w8 + 8, fit16 = a + len <= w16 + 16;
    if (!fit8 && !fit16) return false;
    void* win = fit8 ? (void*)w8 : (void*)w16;
    const size_t wlen = fit8 ? 8 : 16;
    DWORD old = 0;
    if (!VirtualProtect(win, wlen, PAGE_EXECUTE_READWRITE, &old)) return false;
    bool ok = false;
    if (fit8) {
        volatile LONG64* q = (volatile LONG64*)w8;
        const LONG64 cur = *q;
        LONG64 nw = cur;
        memcpy((uint8_t*)&nw + (a - w8), bytes, len);
        ok = InterlockedCompareExchange64(q, nw, cur) == cur;
    } else {
        volatile LONG64* q = (volatile LONG64*)w16;
        alignas(16) LONG64 cur[2] = {q[0], q[1]};
        LONG64 nw[2] = {cur[0], cur[1]};
        memcpy((uint8_t*)nw + (a - w16), bytes, len);
        ok = _InterlockedCompareExchange128(q, nw[1], nw[0], cur) != 0;
    }
    VirtualProtect(win, wlen, old, &old);
    FlushInstructionCache(GetCurrentProcess(), win, wlen);
    return ok;
}
bool PatchSpin(uint8_t* p, const uint8_t* bytes, size_t len) {  // run straddling 16 bytes: park the head, write the tail, then the head
    static const uint8_t spin[2] = {0xEB, 0xFE};
    uint8_t orig[2] = {p[0], p[1]};
    if (len < 3 || !PatchAtomic(p, spin, 2)) return false;
    DWORD old = 0;
    if (!VirtualProtect(p + 2, len - 2, PAGE_EXECUTE_READWRITE, &old)) {
        PatchAtomic(p, orig, 2);
        return false;
    }
    memcpy(p + 2, bytes + 2, len - 2);
    VirtualProtect(p + 2, len - 2, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, len);
    return PatchAtomic(p, bytes, 2);
}
bool Patch(uint8_t* p, const uint8_t* bytes, size_t len) { return PatchAtomic(p, bytes, len) || PatchSpin(p, bytes, len); }

// Entry thunk: save rcx, rdx, r8-r11 and xmm0-5 (the 1.7.17 / 1.7.19 lessons, bosslog.cpp Install), call fn(rcx, rdx, r8, r9, sp),
// restore, then either the relocated prologue + jmp back (fresh site) or jmp to the previous hook's thunk (chained site; it runs the
// original prologue itself). 0 ok, 2 no room / not patchable.
int Install(const Site& s, int probe, uint64_t prev) {
    uint8_t* p = (uint8_t*)(g_base + s.rva);
    if (g_thunkUsed + kThunkSize > kThunkPage) return 2;
    uint8_t* t = g_thunks + g_thunkUsed;
    uint8_t* w = t;
    auto put = [&](std::initializer_list<uint8_t> b) {
        for (uint8_t x : b) *w++ = x;
    };
    put({0x51, 0x52, 0x41, 0x50, 0x41, 0x51, 0x41, 0x52, 0x41, 0x53});  // push rcx, rdx, r8, r9, r10, r11
    put({0x48, 0x81, 0xEC, 0x88, 0x00, 0x00, 0x00});                    // sub rsp,88h (16-aligned at the call)
    for (uint8_t i = 0; i < 6; ++i) put({0xF3, 0x0F, 0x7F, (uint8_t)(0x44 | (i << 3)), 0x24, (uint8_t)(0x28 + 0x10 * i)});  // movdqu [rsp+..],xmm_i
    put({0x48, 0x8D, 0x84, 0x24, 0xB8, 0x00, 0x00, 0x00});  // lea rax,[rsp+0B8h] = entry rsp
    put({0x48, 0x89, 0x44, 0x24, 0x20});                    // mov [rsp+20h],rax -> 5th argument
    put({0x48, 0xB8});                                      // mov rax, imm64
    const uint64_t fn = (uint64_t)s.fn;
    memcpy(w, &fn, 8);
    w += 8;
    put({0xFF, 0xD0});  // call rax
    for (uint8_t i = 0; i < 6; ++i) put({0xF3, 0x0F, 0x6F, (uint8_t)(0x44 | (i << 3)), 0x24, (uint8_t)(0x28 + 0x10 * i)});  // movdqu xmm_i,[rsp+..]
    put({0x48, 0x81, 0xC4, 0x88, 0x00, 0x00, 0x00});                    // add rsp,88h
    put({0x41, 0x5B, 0x41, 0x5A, 0x41, 0x59, 0x41, 0x58, 0x5A, 0x59});  // pop r11, r10, r9, r8, rdx, rcx
    uint64_t back = prev;
    if (probe == 0) {  // fresh: relocated prologue (no rip-relative / branch in it), then back to entry + head
        memcpy(w, s.b, s.head);
        w += s.head;
        back = (uint64_t)(p + s.head);
    }
    put({0xFF, 0x25, 0, 0, 0, 0});  // jmp [rip+0]
    memcpy(w, &back, 8);
    w += 8;
    g_thunkUsed += ((size_t)(w - t) + 15) & ~size_t(15);
    const intptr_t d = (intptr_t)t - (intptr_t)(p + 5);
    if (d < INT32_MIN || d > INT32_MAX) return 2;
    uint8_t nb[16];
    memset(nb, 0x90, sizeof(nb));
    nb[0] = 0xE9;
    const int32_t rel = (int32_t)d;
    memcpy(nb + 1, &rel, 4);
    return Patch(p, nb, probe == 0 ? s.head : 5) ? 0 : 2;  // chained: only the rel32 changes, the NOP tail stays
}

// ---- catalogue ----
std::wstring IniDir(const wchar_t* ini) {
    std::wstring s(ini);
    const size_t k = s.find_last_of(L"\\/");
    return k == std::wstring::npos ? std::wstring() : s.substr(0, k + 1);
}
void Ascii(char* dst, size_t n, const std::string& src) {  // OSD font: printable ASCII only
    size_t k = 0;
    for (unsigned char c : src) {
        if (k + 1 >= n) break;
        dst[k++] = (c >= 0x20 && c < 0x7F) ? (char)c : '?';
    }
    dst[k] = 0;
}
int LoadCatalogue(const std::wstring& path) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return -1;
    std::string text;
    char buf[8192];
    DWORD got = 0;
    while (ReadFile(h, buf, sizeof(buf), &got, nullptr) && got) text.append(buf, got);
    CloseHandle(h);
    std::vector<std::string> head;
    int cId = -1, cName = -1, cMission = -1, cRegion = -1, cRoute = -1, cMid = -1, cHidden = -1, cVer = -1;
    size_t pos = 0;
    bool first = true;
    while (pos < text.size()) {
        size_t e = text.find('\n', pos);
        if (e == std::string::npos) e = text.size();
        std::string line = text.substr(pos, e - pos);
        pos = e + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        std::vector<std::string> f;
        size_t a = 0;
        for (;;) {
            const size_t b = line.find('\t', a);
            f.push_back(line.substr(a, b == std::string::npos ? std::string::npos : b - a));
            if (b == std::string::npos) break;
            a = b + 1;
        }
        if (first) {
            first = false;
            for (int i = 0; i < (int)f.size(); ++i) {
                const std::string& k = f[(size_t)i];
                if (k == "id") cId = i;
                else if (k == "name") cName = i;
                else if (k == "mission_name") cMission = i;
                else if (k == "region") cRegion = i;
                else if (k == "route") cRoute = i;
                else if (k == "replay_mission_id") cMid = i;
                else if (k == "hidden") cHidden = i;
                else if (k == "verified") cVer = i;
            }
            if (cName < 0 || cRoute < 0 || cMid < 0) return -2;
            continue;
        }
        auto col = [&](int c) -> std::string { return c >= 0 && (size_t)c < f.size() ? f[(size_t)c] : std::string(); };
        if (col(cRoute) != "R") continue;  // only route R in this build (E0 / E1 rows are Phase 1b / 2)
        const unsigned long mid = strtoul(col(cMid).c_str(), nullptr, 0);
        if (!mid || mid > 0xFFFF || g_rows.size() >= (size_t)kMaxRows) continue;
        Row r{};
        Ascii(r.name, sizeof(r.name), col(cName));
        Ascii(r.mission, sizeof(r.mission), col(cMission).empty() ? col(cName) : col(cMission));
        Ascii(r.region, sizeof(r.region), col(cRegion));
        Ascii(r.route, sizeof(r.route), col(cRoute));
        r.missionId = (uint16_t)mid;
        r.hidden = col(cHidden) == "1";
        r.verified = col(cVer) == "1";
        (void)cId;
        g_rows.push_back(r);
    }
    return (int)g_rows.size();
}

// ---- sub-page (Present thread) ----
bool g_page = false;
int g_sel = 0, g_confirm = -1;
uint32_t g_pageGen0 = 0;
char g_pageMsg[96] = "";

// selectable items: -1 open the replay menu, -2 abort, >= 0 catalogue row
int Items(int* out, int cap, bool* scanned) {
    int n = 0;
    out[n++] = -1;
    if (g_state.load() != kIdle && !g_replayIn.load()) out[n++] = -2;
    const bool sc = g_scanGen.load() != g_pageGen0 && g_scanNote.load() == 0;
    if (scanned) *scanned = sc;
    if (!sc) return n;
    for (size_t i = 0; i < g_rows.size() && n < cap; ++i) {
        const Row& r = g_rows[i];
        if (r.hidden && !g_cfg.hiddenDuels) continue;
        if (!g_cfg.showAll && g_status[i].load() != 2) continue;
        out[n++] = (int)i;
    }
    return n;
}

}  // namespace

// =====================================================================================================================
bool Start(const StartInfo& info) {
    g_log = info.log;
    g_base = info.base;
    g_pauseReason = info.pauseReason;
    if (!g_log || !g_base || !info.iniPath) return false;
    const wchar_t* ini = info.iniPath;
    g_cfg.enable = GetPrivateProfileIntW(L"Rematch", L"Enable", 0, ini) != 0;
    if (!g_cfg.enable) return false;
    g_cfg.hiddenDuels = GetPrivateProfileIntW(L"Rematch", L"HiddenDuels", 0, ini) != 0;
    g_cfg.directStart = GetPrivateProfileIntW(L"Rematch", L"DirectStart", 0, ini) != 0;
    g_cfg.showAll = GetPrivateProfileIntW(L"Rematch", L"ShowAll", 0, ini) != 0;
    g_cfg.showUnverified = GetPrivateProfileIntW(L"Rematch", L"ShowUnverified", 0, ini) != 0;
    if (!info.knownBuild) {
        g_log("Rematch: refused (unknown build; addresses are v2.02)");
        return false;
    }
    const auto* dos = (const IMAGE_DOS_HEADER*)g_base;
    const auto* nt = (const IMAGE_NT_HEADERS64*)(g_base + dos->e_lfanew);
    g_imgEnd = g_base + nt->OptionalHeader.SizeOfImage;
    wchar_t cat[MAX_PATH] = {};
    GetPrivateProfileStringW(L"Rematch", L"Catalog", L"Nioh3EngineFix_bosses.tsv", cat, MAX_PATH, ini);
    std::wstring cpath = cat;
    if (cpath.find(L':') == std::wstring::npos && (cpath.empty() || (cpath[0] != L'\\' && cpath[0] != L'/'))) cpath = IniDir(ini) + cpath;
    const int nrows = LoadCatalogue(cpath);
    if (nrows <= 0) {
        g_log("Rematch: off - catalogue %ls %s", cpath.c_str(), nrows == -1 ? "not found" : nrows == -2 ? "has no id/route/replay_mission_id header" : "has no route R rows");
        return false;
    }
    // verify every site first; nothing is patched unless all of them match
    int probe[sizeof(kSites) / sizeof(kSites[0])];
    uint64_t prev[sizeof(kSites) / sizeof(kSites[0])] = {};
    char bad[200] = "";
    size_t bl = 0;
    for (size_t i = 0; i < sizeof(kSites) / sizeof(kSites[0]); ++i) {
        probe[i] = Probe(kSites[i], &prev[i]);
        if (probe[i] < 0 && bl < sizeof(bad) - 40) bl += (size_t)snprintf(bad + bl, sizeof(bad) - bl, " %s", kSites[i].name);
    }
    if (bl) {
        g_log("Rematch: off - bytes differ at:%s", bad);
        return false;
    }
    g_thunks = AllocNear(g_base, g_imgEnd);
    if (!g_thunks) {
        g_log("Rematch: off - no executable page within 2 GB of the image");
        return false;
    }
    char how[200] = "";
    size_t hl = 0;
    for (size_t i = 0; i < sizeof(kSites) / sizeof(kSites[0]); ++i) {
        const Site& s = kSites[i];
        if (!s.fn) continue;
        if (Install(s, probe[i], prev[i]) != 0) {
            g_log("Rematch: off - could not patch %s (atomic write / thunk room); %s", s.name, hl ? "earlier hooks stay as pass-through" : "nothing patched");
            return false;  // earlier hooks only update g_lastTick / g_inFight: harmless with g_live = false
        }
        hl += (size_t)snprintf(how + hl, sizeof(how) - hl, " %s%s", s.name, probe[i] == 1 ? "(chained after BossLog)" : "");
    }
    g_live = true;
    g_log("Rematch: on (route R-a: the game's own Battle Scroll replay menu; solo/offline gates; %d catalogue rows from %ls; HiddenDuels %d, "
          "DirectStart %d (refused in this build), ShowAll %d, ShowUnverified %d). Hooks:%s",
          nrows, cpath.c_str(), g_cfg.hiddenDuels, g_cfg.directStart, g_cfg.showAll, g_cfg.showUnverified, how);
    return true;
}

bool Installed() { return g_live; }

void RowValue(char* buf, size_t n, uint32_t& color) {
    const int st = g_state.load();
    if (st != kIdle) {
        snprintf(buf, n, "%s", st == kMenuOpen ? "active: replay menu" : g_replayIn.load() ? "active: in replay" : "active: starting");
        color = cYellow;
        return;
    }
    const Snap s = ReadSnap();
    const char* why = Gates(s, true);
    snprintf(buf, n, "%s", why ? why : "ready (A: list)");
    color = why ? cDim : cGreen;
}

bool PageActive() { return g_page; }
void PageOpen() {
    g_page = true;
    g_sel = 0;
    g_confirm = -1;
    g_pageMsg[0] = 0;
    g_pageGen0 = g_scanGen.load();
    g_wantScan.store(true);
}
void PageClose() {
    g_page = false;
    g_confirm = -1;
}
void PageMove(int dir) {
    int it[kMaxRows + 2];
    const int n = Items(it, kMaxRows + 2, nullptr);
    g_sel = n ? (g_sel + dir + n) % n : 0;
    g_confirm = -1;
    g_pageMsg[0] = 0;
}
bool PageAct(int dir) {
    if (dir != 0) return false;
    int it[kMaxRows + 2];
    const int n = Items(it, kMaxRows + 2, nullptr);
    if (g_sel >= n) g_sel = n - 1;
    const int item = it[g_sel < 0 ? 0 : g_sel];
    if (item == -2) {
        Post(kReqAbort, -1, "the menu");
        return true;
    }
    const char* why = Gates(ReadSnap(), true);
    if (why) {
        snprintf(g_pageMsg, sizeof(g_pageMsg), "Not now: %s.", why);
        g_confirm = -1;
        return false;
    }
    if (item == -1) {
        Post(kReqOpen, -1, "the menu");
        ToastF(cWhite, 6000, "Rematch queued...");
        return true;
    }
    const Row& r = g_rows[(size_t)item];
    if (!r.verified && !g_cfg.showUnverified) {
        snprintf(g_pageMsg, sizeof(g_pageMsg), "Unverified row (test R-12 pending): set [Rematch] ShowUnverified = 1 to allow it.");
        return false;
    }
    if (g_confirm != item) {
        g_confirm = item;
        g_pageMsg[0] = 0;
        return false;
    }
    g_confirm = -1;
    Post(g_cfg.directStart ? kReqDirect : kReqOpen, item, "the menu");
    ToastF(cWhite, 6000, "Rematch queued: %s...", r.name);
    return true;
}
void PageBack() {
    if (g_confirm >= 0) {
        g_confirm = -1;
        return;
    }
    PageClose();
}

int PageFill(PageRow* rows, int cap) {
    int k = 0;
    auto add = [&](const char* label, const char* tech, const char* value, uint32_t lc, uint32_t vc, bool sel) {
        if (k >= cap) return;
        PageRow& r = rows[k++];
        snprintf(r.label, sizeof(r.label), "%s", label);
        snprintf(r.tech, sizeof(r.tech), "%s", tech);
        snprintf(r.value, sizeof(r.value), "%s", value);
        r.lc = lc;
        r.vc = vc;
        r.sel = sel;
    };
    char v[64];
    uint32_t vc = cWhite;
    RowValue(v, sizeof(v), vc);
    add("Boss rematch (solo/offline)", "", v, cCyan, vc, false);
    int it[kMaxRows + 2];
    bool scanned = false;
    const int n = Items(it, kMaxRows + 2, &scanned);
    if (g_sel >= n) g_sel = n - 1;
    if (g_sel < 0) g_sel = 0;
    for (int i = 0; i < n; ++i) {
        const bool sel = i == g_sel;
        char lab[96];
        if (it[i] == -1) {
            snprintf(lab, sizeof(lab), "%sOpen the game's Battle Scroll replay menu", sel ? "> " : "  ");
            add(lab, "(R-a)", "", sel ? cWhite : cGray, cWhite, sel);
        } else if (it[i] == -2) {
            snprintf(lab, sizeof(lab), "%sAbort and restore now", sel ? "> " : "  ");
            add(lab, "", g_unhN.load() ? "hidden duels un-hidden" : "", sel ? cWhite : cGray, cYellow, sel);
        } else {
            const Row& r = g_rows[(size_t)it[i]];
            const uint8_t stt = g_status[(size_t)it[i]].load();
            snprintf(lab, sizeof(lab), "%s%s", sel ? "> " : "  ", r.name);
            char val[48];
            const char* status = stt == 2 ? "beaten" : stt == 1 ? "not beaten" : stt == 3 ? "error" : "?";
            if (g_confirm == it[i]) snprintf(val, sizeof(val), "A = confirm, B = back");
            else snprintf(val, sizeof(val), "%-10s %s", r.verified ? status : "unverified", r.hidden ? "hidden duel" : "replay");
            const uint32_t c = g_confirm == it[i] ? cYellow : !r.verified ? cDim : stt == 2 ? cGreen : cGray;
            add(lab, r.region, val, sel ? cWhite : (r.verified ? cGray : cDim), c, sel);
        }
    }
    if (!scanned) {
        const int note = g_scanNote.load();
        add(g_scanGen.load() == g_pageGen0 ? "  scanning the clear record..." : note == 1 ? "  load a save first (no player)" : "  clear record not readable",
            "", "", cDim, cDim, false);
    } else if (n <= 2 && !g_cfg.showAll) {
        add("  no cleared catalogue mission on this journey ([Rematch] ShowAll = 1 lists all)", "", "", cDim, cDim, false);
    }
    return k;
}

void PageHelp(char* what, size_t nw, char* effect, size_t ne) {
    int it[kMaxRows + 2];
    const int n = Items(it, kMaxRows + 2, nullptr);
    const int item = n ? it[g_sel < n ? (g_sel < 0 ? 0 : g_sel) : n - 1] : -1;
    if (item == -1) {
        snprintf(what, nw, "Opens the game's own Battle Scroll replay list (official replay, whole mission). Solo/offline only.");
        snprintf(effect, ne, "%s", g_pageMsg[0] ? g_pageMsg
                                                 : (g_cfg.hiddenDuels ? "HiddenDuels = 1: the 3 hidden duels are listed while it is open (restored after)."
                                                                      : "Nothing is patched. Best at a shrine with its menu closed."));
        return;
    }
    if (item == -2) {
        snprintf(what, nw, "Ends the active rematch now: restores the hidden-duel records and goes back to idle.");
        snprintf(effect, ne, "Only before the replay mission has loaded; after that it restores by itself on exit.");
        return;
    }
    const Row& r = g_rows[(size_t)item];
    snprintf(what, nw, "Replays '%s' (%s): the game's replay list opens, pick that mission there.", r.mission, r.region);
    if (g_pageMsg[0]) snprintf(effect, ne, "%s", g_pageMsg);
    else if (g_confirm == item)
        snprintf(effect, ne, "Start rematch: %s? A confirm / B back. Solo only. Drops: as the game's replay.", r.name);
    else if (r.hidden)
        snprintf(effect, ne, "Hidden duel: un-hidden in memory while the menu / mission is open, restored after. Untested (R-12).");
    else
        snprintf(effect, ne, "A: start. Status = the game's clear record (current journey), the getter the replay list uses.");
}

bool Toast(char* buf, size_t n, uint32_t& color) {
    bool on = false;
    AcquireSRWLockShared(&g_toastLock);
    if (g_toast[0] && Ms() < g_toastUntil) {
        snprintf(buf, n, "%s", g_toast);
        color = g_toastColor;
        on = true;
    }
    ReleaseSRWLockShared(&g_toastLock);
    return on;
}

int StateCode() { return g_state.load(); }
void RemoteRequest(int value) {
    if (!g_live) return;
    if (value == 1) {
        Post(kReqOpen, -1, "remote (vpad pipe)");
        ToastF(cWhite, 6000, "Rematch queued...");
    } else if (value == 2) {
        Post(kReqAbort, -1, "remote (vpad pipe)");
    }
}

}  // namespace rematch
