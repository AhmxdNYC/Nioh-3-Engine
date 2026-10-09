// rematch.cpp - the boss rematch engine of Nioh3BossRematch (notes\boss-rematch-addon.md, notes\rematch-all-bosses.md).
// Routes (catalogue column `route`):
//   R  listable Battle Scroll mission: R-a = the game's replay menu (MenuOpenMissionReplay(0,0,6)), R-b = MissionRequestById(id,
//      slot byte, 0, params) = the shrine replay confirm (session 2026-10-08). R-b only for a mission cleared on this journey.
//   H  hidden duel: R-b direct (no patch needed), or R-a with the reverted un-hide ([Rematch] HiddenDuels = 1).
//   D  unlisted shipped record (master duels): R-b direct, cleared-on-this-journey only.
//   E  one of the 20 Picture Scroll records: an owned Picture Scroll is the carrier; every entry of the Emaki byte->record map is
//      pointed at the chosen record (snapshot + CRC), EmakiMissionRequest(item+28h, key, 0, params) as the item menu does; the map
//      is restored when the scroll mission ends (charMgr +0x2C18 back to 0 for 5 s) or 30 s after a request that never loaded.
//      "Use an owned Picture Scroll (game's pick)" = the same call without the redirect.
//   X  no route: listed greyed with the reason.
//   F  "Fight now (straight to the boss)" = route E1 of the build note, for R / H / D rows ([Rematch] FightNow = 1): the same R-b call
//      with the boss's start point as the third argument, MissionRequestById(id, slot byte, startPoint, params). The argument is
//      the request's +0xC start point (0 = the mission default, 1C17D8C), the value Continue passes for the saved retry point
//      (MissionStartFromSave 22375A9: [[MissionMgr]+8]+14h). The start point (catalogue stage_mission) is the one next to the boss
//      arena (tools/rematch_catalog.py pick_fight_start). Nothing is patched, so nothing has to be restored; a retry reloads at the
//      same start point because the mission state's retry point (+14h) is that start point. The Emaki-record patch of the design
//      cannot host a story boss (notes/rematch-all-bosses.md s.3: 0xCC96 only builds its own Picture Scroll placements).
// Every game call and every write runs on the game thread (the per-frame pump); the UI / pipe only post a request word. No story
// flag is ever written. Solo / offline only (gates re-checked on the game thread right before every call).
#include "addon.h"
#include "rematch_core.h"

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace addon {
namespace rematch {
namespace {

using rcore::Row;
constexpr uint32_t cWhite = 0xFFFFFF, cGray = 0xB4B4B4, cDim = 0x7A7A7A, cGreen = 0x6CFF6C, cYellow = 0x40E6FF, cOrange = 0x3C8CFF,
                   cCyan = 0xFFE08C, cRed = 0x5050FF;  // 0xBBGGRR (GDI COLORREF)
ULONGLONG Ms() { return GetTickCount64(); }

// ---- resolved sites ----
uintptr_t G_DATA, G_CLEAR, G_MS, G_QUEUE, G_PM, G_NET, G_CH, G_GM, G_PT, F_CLEARED, F_REQBYID, F_OPENREPLAY, G_MENUMGR, F_EMAKIREQ, G_INV;
bool g_live = false, g_raOk = false, g_emakiOk = false, g_fnowOk = false;
const char *g_raWhy = "", *g_emakiWhy = "", *g_fnowWhy = "";

struct Cfg {
    bool hiddenDuels = false, directStart = false, showAll = true, showUnverified = false, fightNow = false;
} g_cfg;

std::vector<Row> g_rows;
constexpr int kMaxRows = 512;
std::atomic<uint8_t> g_status[kMaxRows];  // 0 unknown, 1 not beaten, 2 beaten, 3 getter failed
std::atomic<uint32_t> g_scanGen{0};
std::atomic<int> g_scanNote{0};
std::atomic<bool> g_wantScan{false};

// ---- toast + last result (any thread) ----
SRWLOCK g_lock = SRWLOCK_INIT;
char g_toast[160] = "", g_last[200] = "none";
uint32_t g_toastColor = cWhite;
ULONGLONG g_toastUntil = 0;
void ToastF(uint32_t color, ULONGLONG ms, const char* fmt, ...) {
    char b[160];
    va_list a;
    va_start(a, fmt);
    vsnprintf(b, sizeof(b), fmt, a);
    va_end(a);
    AcquireSRWLockExclusive(&g_lock);
    memcpy(g_toast, b, sizeof(b));
    g_toastColor = color;
    g_toastUntil = Ms() + ms;
    ReleaseSRWLockExclusive(&g_lock);
}
void Result(const char* fmt, ...) {
    char b[200];
    va_list a;
    va_start(a, fmt);
    vsnprintf(b, sizeof(b), fmt, a);
    va_end(a);
    AcquireSRWLockExclusive(&g_lock);
    memcpy(g_last, b, sizeof(b));
    ReleaseSRWLockExclusive(&g_lock);
}

// ---- requests ----
enum ReqKind : uint32_t { kReqNone = 0, kReqOpen = 1, kReqDirect = 2, kReqAbort = 3, kReqEmaki = 4, kReqScroll = 5, kReqFight = 6 };
std::atomic<uint32_t> g_req{0};  // kind | (row + 1) << 8
enum State : int { kIdle = 0, kMenuOpen = 1, kInReplay = 2, kEmaki = 3 };
std::atomic<int> g_state{kIdle};
std::atomic<int> g_activeRow{-1};
std::atomic<bool> g_replayIn{false};
std::atomic<bool> g_fight{false};  // the active replay is a Fight now start
std::atomic<ULONGLONG> g_lastTick{0}, g_qChangedAt{0};
uint64_t g_qHash = 0;

const char* RowName(int row) { return row >= 0 && (size_t)row < g_rows.size() ? g_rows[(size_t)row].name : ""; }

void Post(uint32_t kind, int row, const char* via) {
    g_req.store(kind | ((uint32_t)(row + 1) << 8));
    static const char* const kn[] = {"", "R-a open the replay menu", "R-b direct start", "abort", "Picture Scroll (E1 redirect)", "Picture Scroll (game's pick)",
                                     "Fight now (E1 straight to the boss)"};
    Log("Rematch: request %s%s%s queued from %s", kn[kind], row >= 0 ? " for " : "", RowName(row), via);
}

// ---- snapshot (memory reads only; any thread) ----
struct Snap {
    bool gm = false, player = false, msOk = false, queueBusy = false, emaki = false, online = false, guest = false;
    uint64_t qHash = 0;
    uint8_t st[6] = {};
    uint32_t ms14 = 0;  // mission state +14h: the retry point = the start point the mission was (re)loaded at
    uint32_t ms28 = 0;  // mission state +28h: the running mission id (MissionStartFromSave 22372C3 passes it to MissionRequestById)
};
Snap ReadSnap() {
    Snap s;
    uint64_t gm = 0;
    if (Rd(G_GM, gm) && gm) s.gm = Copy(s.st, gm + 0x588, 6);
    uint64_t pt = 0, pl = 0;
    if (Rd(G_PT, pt) && pt && Rd(pt + 0xCD * 0x18, pl) && pl) s.player = true;
    uint64_t g = 0, ms = 0;
    if (Rd(G_MS, g) && g && Rd(g + 8, ms) && ms) s.msOk = Rd(ms + 0x14, s.ms14) && Rd(ms + 0x28, s.ms28);
    uint8_t q[3 * 0x58];
    if (Copy(q, G_QUEUE, sizeof(q))) {
        uint64_t h = 0xCBF29CE484222325ull;
        for (uint8_t c : q) h = (h ^ c) * 0x100000001B3ull;
        s.qHash = h;
    }
    s.queueBusy = Ms() - g_qChangedAt.load() < 10000;
    uint64_t a = 0, ch = 0;
    uint8_t e = 0;
    if (Rd(G_CH, a) && a && Rd(a, ch) && ch && Rd(ch + 0x2C18, e)) s.emaki = e != 0;
    uint64_t pm = 0, net = 0;
    int32_t mode = 0;
    uint8_t conn = 0;
    if (Rd(G_PM, pm) && pm && Rd(pm + 0x34, mode)) s.online = mode == 1 || mode == 2;
    if (Rd(G_NET, net) && net && Rd(net + 0x1DC, conn)) s.guest = conn != 0;
    return s;
}
const char* Gates(const Snap& s, bool uiSide) {
    if (!g_live) return "not installed";
    if (s.online) return "online";
    if (s.guest) return "co-op session";
    if (!s.gm) return "game not ready";
    if (s.st[2]) return "loading";
    if (s.st[3] | s.st[4]) return "game paused";
    if (!s.player) return "no save loaded";
    if (s.queueBusy) return "mission queue busy";
    if (s.emaki) return "in a Picture Scroll";
    if (fight::g_active) return "in a boss fight";
    if (g_state.load() != kIdle) return "rematch active";
    if (uiSide && Ms() - g_lastTick.load() > 2000) return "not in game (no frame tick)";
    return nullptr;
}

// ---- game calls (game thread only) ----
int ReplayMenuBusy() {
    __try {
        const uint64_t mm = *(const volatile uint64_t*)G_MENUMGR;
        if (!mm) return -1;
        const uint64_t menu = *(const volatile uint64_t*)(mm + 0x4E0);
        if (!menu) return -2;
        const uint64_t vt = *(const volatile uint64_t*)menu;
        if (vt < g_img.base || vt >= g_img.end) return -3;
        const uint64_t f48 = *(const volatile uint64_t*)(vt + 0x48);
        if (f48 < g_img.base || f48 >= g_img.end) return -3;
        return (((uint8_t(*)(uint64_t))f48)(menu) & 0xFF) != 0 ? 1 : 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return -5;
    }
}
int CallOpenReplay() {
    const int busy = ReplayMenuBusy();
    if (busy < 0) return -busy;
    if (busy) return 4;
    __try {
        ((void (*)(uint64_t, uint32_t, uint8_t))F_OPENREPLAY)(0, 0, 6);
        return 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 5;
    }
}
bool CallCleared(uint64_t db, uint32_t id, uint8_t& out) {
    __try {
        out = (uint8_t)(((uint64_t(*)(uint64_t, uint32_t))F_CLEARED)(db, id) & 0xFF);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
// start = 0: the mission's default start point (the shrine replay); else the start point the request carries in +0xC (Fight now)
int CallRequestById(uint16_t id, uint8_t slot, uint32_t start) {
    alignas(16) uint32_t p[8] = {0x101, 0x1000000, 0x10100, 0, 0, 0, 0, 0};  // movaps [r9] (2239660): 16-byte aligned
    __try {
        ((void (*)(uint32_t, uint8_t, uint32_t, const uint32_t*))F_REQBYID)(id, slot, start, p);
        return 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 1;
    }
}
int CallEmakiRequest(uint64_t ref, const uint8_t* key12) {
    alignas(16) uint32_t p[8] = {0x101, 0x1000000, 0x10100, 0, 0, 0, 0, 0};  // movaps [params] (10DC0CD): 16-byte aligned
    alignas(16) uint8_t k[16] = {};
    memcpy(k, key12, 12);
    __try {
        ((void (*)(uint64_t, const uint8_t*, uint32_t, const uint32_t*))F_EMAKIREQ)(ref, k, 0, p);
        return 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 1;
    }
}

// ---- mission records + hidden duel un-hide (game thread) ----
uint64_t FindMissionRecord(uint16_t id) {
    uint64_t dm = 0, tab = 0;
    uint32_t count = 0;
    if (!Rd(G_DATA, dm) || !dm || !Rd(dm, tab) || !tab || !Rd(tab + 4, count) || count == 0 || count > 1000) return 0;
    for (uint32_t i = 0; i < count; ++i) {
        uint16_t v = 0;
        const uint64_t rec = tab + 8 + (uint64_t)i * 0x198;
        if (!Rd(rec + 0x158, v)) return 0;
        if (v == id) return rec;
    }
    return 0;
}
struct Unhide {
    uint64_t addr;
    uint32_t orig, written;
    uint16_t id;
};
Unhide g_unh[3];
std::atomic<int> g_unhN{0};
const uint16_t kHiddenIds[3] = {0xD22D, 0xB3F8, 0xFB96};
int UnhideApply() {
    int n = 0;
    char note[200] = "";
    size_t nl = 0;
    for (uint16_t id : kHiddenIds) {
        const uint64_t rec = FindMissionRecord(id);
        uint32_t v = 0;
        if (!rec || !Rd(rec + 0x40, v) || !(v & 0x4000)) {
            nl += (size_t)snprintf(note + nl, sizeof(note) - nl, " %04X:skip", id);
            continue;
        }
        const uint32_t w = v & ~0x4000u;
        if (!WrBytes(rec + 0x40, &w, 4)) {
            nl += (size_t)snprintf(note + nl, sizeof(note) - nl, " %04X:write-failed", id);
            continue;
        }
        g_unh[n++] = Unhide{rec + 0x40, v, w, id};
        nl += (size_t)snprintf(note + nl, sizeof(note) - nl, " %04X:%08X->%08X", id, v, w);
    }
    g_unhN.store(n);
    Log("Rematch: HiddenDuels: %d of 3 records un-hidden (+0x40 bit 0x4000, game data, never saved):%s", n, note);
    return n;
}
void UnhideRestore(const char* why) {
    const int n = g_unhN.exchange(0);
    if (!n) return;
    char note[200] = "";
    size_t nl = 0;
    for (int i = 0; i < n; ++i) {
        Unhide& u = g_unh[i];
        uint32_t cur = 0;
        Rd(u.addr, cur);
        const uint32_t back = cur == u.written ? u.orig : (cur | (u.orig & 0x4000));
        const bool ok = WrBytes(u.addr, &back, 4);
        nl += (size_t)snprintf(note + nl, sizeof(note) - nl, " %04X:%08X->%08X%s%s", u.id, cur, back, cur == u.written ? "" : "(changed by game)",
                               ok ? "" : "(WRITE FAILED)");
    }
    Log("Rematch: HiddenDuels restored (%s):%s", why, note);
}

// ---- Picture Scrolls (game thread) ----
struct Scroll {
    int idx = -1;
    uint16_t itemId = 0;
    uint64_t ref = 0;
    uint8_t key[12] = {};
};
uint8_t KindOf(uint16_t id) { return id == 0x1E82 ? 1 : id == 0x516D ? 2 : id == 0xDD82 ? 4 : id == 0xD523 ? 5 : 3; }
bool IsScroll(uint16_t id) { return id == 0x1E82 || id == 0x516D || id == 0xE604 || id == 0xDD82 || id == 0xD523; }
bool FindScroll(Scroll& s) {  // the item-menu path 0 of EmakiItemUse_MenuHandler F1F060
    uint64_t a = 0, inv = 0;
    if (!Rd(G_INV, a) || !a || !Rd(a, inv) || !inv) return false;
    for (int i = 0; i < 4096; ++i) {
        const uint64_t it = inv + 0x224A60 + (uint64_t)i * 0xE8;
        uint16_t id = 0;
        if (!Rd(it, id)) break;
        if (!IsScroll(id)) continue;
        uint32_t k0 = 0;
        uint16_t w12 = 0;
        uint8_t b0f = 0;
        uint64_t ref = 0;
        if (!Rd(it + 0x20, k0) || !k0 || !Rd(it + 0x12, w12) || !Rd(it + 0x0F, b0f) || !Rd(it + 0x28, ref)) continue;
        s.idx = i;
        s.itemId = id;
        s.ref = ref;
        memset(s.key, 0, sizeof(s.key));
        memcpy(s.key, &k0, 4);
        memcpy(s.key + 4, &w12, 2);
        s.key[6] = b0f;
        s.key[7] = KindOf(id);
        s.key[8] = 1;  // use mode (path 0)
        return true;
    }
    return false;
}
struct EmakiTab {
    uint64_t B = 0, M = 0;
    uint32_t count = 0;
    uint8_t emptyKey = 0;
    uint64_t vb = 0;
    int n = 0;
};
bool ReadEmakiTab(EmakiTab& t) {
    uint64_t d = 0, T = 0, vb = 0, ve = 0;
    if (!Rd(G_DATA, d) || !d || !Rd(d + 0xA88, T) || !T || !Rd(T, t.B) || !t.B || !Rd(T + 0x20, t.M) || !t.M) return false;
    if (!Rd(t.B + 4, t.count) || t.count == 0 || t.count > 255) return false;
    if (!Rd(t.M + 4, t.emptyKey) || !Rd(t.M + 8, vb) || !Rd(t.M + 0x10, ve) || ve <= vb || (ve - vb) / 8 > rcore::MapPatch::kMax) return false;
    t.vb = vb;
    t.n = (int)((ve - vb) / 8);
    return true;
}
rcore::MapPatch g_map;
rcore::ETrack g_et;
bool WrGuard(void* dst, const void* src, size_t n) { return WrBytes((uint64_t)dst, src, n); }
int g_eRow = -1;
uint8_t g_eByteSeen = 0;

void MapRestoreNow(const char* why) {
    if (!g_map.active) return;
    const int r = rcore::MapRestore(g_map, &WrGuard);
    Log("Rematch: E1 map restored (%s): %s", why, r == 0 ? "CRC ok" : r == 1 ? "the map had changed since our write (someone else wrote; restored anyway)" : "WRITE FAILED");
}

// ---- scan (game thread) ----
void DoScan(const Snap& s) {
    uint64_t db = 0;
    if (!s.player || s.st[2] || !Rd(G_CLEAR, db) || !db) {
        g_scanNote.store(!s.player || s.st[2] ? 1 : 2);
        g_scanGen.fetch_add(1);
        return;
    }
    int beaten = 0;
    for (size_t i = 0; i < g_rows.size() && i < (size_t)kMaxRows; ++i) {
        const Row& r = g_rows[i];
        if (r.route == rcore::kE || r.route == rcore::kX) {
            g_status[i].store(0);
            continue;
        }
        uint8_t c = 0;
        const bool ok = CallCleared(db, r.missionId, c);
        g_status[i].store(ok ? ((c & 1) ? 2 : 1) : 3);
        beaten += ok && (c & 1);
    }
    g_scanNote.store(0);
    g_scanGen.fetch_add(1);
    Log("Rematch: scan: %d catalogue missions cleared on the current journey", beaten);
}

// ---- execution (game thread) ----
uint32_t g_pend = 0;
ULONGLONG g_pendAt = 0, g_openAt = 0, g_closedSince = 0, g_replayAt = 0, g_exitSince = 0, g_loadDoneAt = 0;
bool g_menuSeen = false, g_loadSeen = false;
uint32_t g_msAtOpen = 0, g_replayMs = 0;
uint32_t g_fightMission = 0, g_fightStart = 0;  // Fight now: requested mission + start point
bool g_fightByMission = false;                   // exit detection on mission state +28h (seen == the mission after the load)
bool g_fightLoading = false;
int g_fightLoads = 0;
uint64_t g_qHashAtOpen = 0;

void Refuse(const char* why) {
    Log("Rematch: refused: %s", why);
    Result("refused: %s", why);
    ToastF(cOrange, 6000, "Rematch refused: %s", why);
}
void ToIdle(const char* why) {
    UnhideRestore(why);
    MapRestoreNow(why);
    g_et.st = rcore::kEIdle;
    g_state.store(kIdle);
    g_replayIn.store(false);
    g_activeRow.store(-1);
    g_fight.store(false);
    g_fightMission = g_fightStart = 0;
    g_fightByMission = g_fightLoading = false;
    g_fightLoads = 0;
}

void ExecOpen(int row, const Snap& s) {
    if (!g_raOk) return Refuse(g_raWhy);
    const bool unh = g_cfg.hiddenDuels;
    if (unh) UnhideApply();
    const int r = CallOpenReplay();
    if (r != 0) {
        static const char* const kWhy[] = {"", "menu manager not ready", "replay menu object not created", "replay menu vtable not in the image",
                                           "the replay menu is already open", "exception in the game call"};
        UnhideRestore("open failed");
        return Refuse(kWhy[r]);
    }
    g_state.store(kMenuOpen);
    g_activeRow.store(row);
    g_openAt = Ms();
    g_menuSeen = false;
    g_closedSince = 0;
    g_msAtOpen = s.ms14;
    g_qHashAtOpen = s.qHash;
    Log("Rematch: R-a MenuOpenMissionReplay(0, 0, 6) called (row %s)", row >= 0 ? g_rows[(size_t)row].id : "-");
    Result("R-a opened the replay menu");
    ToastF(cGreen, 8000, row >= 0 ? "Rematch: pick '%s' in the replay list" : "Rematch: replay menu opened", row >= 0 ? g_rows[(size_t)row].mission : "");
}

bool ClearedNow(uint16_t id, const char** why) {
    uint64_t db = 0;
    uint8_t c = 0;
    if (!Rd(G_CLEAR, db) || !db || !CallCleared(db, id, c)) {
        *why = "clear db not readable";
        return false;
    }
    if (!(c & 1)) {
        *why = "mission not cleared on this journey (the game's replay only offers cleared missions)";
        return false;
    }
    return true;
}

void ExecDirect(int row, const Snap& s, ULONGLONG t, bool fight) {
    const Row& r = g_rows[(size_t)row];
    const char* why = nullptr;
    const uint32_t start = fight ? r.fightStart : 0;
    if (fight) {
        if (!g_cfg.fightNow) return Refuse("Fight now is off ([Rematch] FightNow = 0)");
        if (!g_fnowOk) return Refuse(g_fnowWhy);
        if (!start) return Refuse("no start point next to this boss in the catalogue (use the replay)");
        Log("Rematch: F %s: gates ok (solo/offline, not loading, not paused, no fight, nothing active); checking the clear on this journey", r.id);
    }
    if (!ClearedNow(r.missionId, &why)) return Refuse(why);
    uint64_t g = 0, ms = 0;
    uint8_t slot = 0;
    if (!Rd(G_MS, g) || !g || !Rd(g + 8, ms) || !ms || !Rd(ms + 0x10, slot)) return Refuse("mission state not readable");
    uint8_t before[3 * 0x58], after[3 * 0x58];
    if (!Copy(before, G_QUEUE, sizeof(before))) return Refuse("mission queue not readable");
    int freeSlot = -1;
    for (int i = 0; i < 3 && freeSlot < 0; ++i) {
        uint32_t m = 0;
        memcpy(&m, before + i * 0x58 + 4, 4);
        if (m == 0 && before[i * 0x58 + 8] == 0) freeSlot = i;
    }
    if (freeSlot < 0) return Refuse("mission queue full");
    if (fight)
        Log("Rematch: F %s: calling MissionRequestById(0x%04X, slot byte %u, start point %u, {101 1000000 10100 0 0 0}) - %s, %d m from the boss, "
            "spawn %.0f %.0f %.0f yaw %.0f, confidence %s; queue slot %d free",
            r.id, r.missionId, slot, start, r.fightKind, r.fightDist, r.warp[0], r.warp[1], r.warp[2], r.warp[3], r.fightConf, freeSlot);
    if (CallRequestById(r.missionId, slot, start) != 0) return Refuse("exception in the game call");
    const uint8_t* q = after + freeSlot * 0x58;
    uint32_t qid = 0;
    uint64_t mode = 0;
    const bool rd = Copy(after, G_QUEUE, sizeof(after));
    if (rd) {
        memcpy(&qid, q + 4, 4);
        memcpy(&mode, q + 0x14, 8);
    }
    if (!rd || qid != r.missionId || mode != 2) {
        Log("Rematch: R-b MissionRequestById(0x%04X, %u, %u, params) left slot %d without the request (id %u, mode %llu)", r.missionId, slot, start,
            freeSlot, qid, (unsigned long long)mode);
        return Refuse("the game did not take the request");
    }
    char hb[0x58 * 3 + 4];
    Hex(hb, sizeof(hb), (uint64_t)q, 0x58);
    if (fight) {
        uint32_t sp = 0;
        memcpy(&sp, q + 0xC, 4);
        if (!rcore::FightSlotOk(q, r.missionId, start))  // queued, but not at our start point: the mission starts there (a plain replay)
            Log("Rematch: F %s: WARNING the queued request carries start point %u, not %u: the mission will start at that point (a plain replay)",
                r.id, sp, start);
        else
            Log("Rematch: F %s: queued - slot %d +4 mission 0x%04X, +0xC start point %u, +0x14 mode 2", r.id, freeSlot, r.missionId, sp);
        g_fight.store(true);
        g_fightMission = r.missionId;
        g_fightStart = start;
        g_fightByMission = g_fightLoading = false;
        g_fightLoads = 0;
    }
    g_state.store(kInReplay);
    g_activeRow.store(row);
    g_openAt = g_replayAt = t;
    g_msAtOpen = s.ms14;
    g_qHashAtOpen = s.qHash;
    g_replayMs = 0;
    g_exitSince = 0;
    g_loadSeen = s.st[2] != 0;
    g_loadDoneAt = 0;
    Log("Rematch: %s %s %s: MissionRequestById(0x%04X, slot byte %u, %u, {101 1000000 10100 0 0 0}) -> queue slot %d req[58]=%s", fight ? "F" : "R-b",
        r.id, fight ? "fight now" : rcore::RouteName(r.route), r.missionId, slot, start, freeSlot, hb);
    Result("started %s (%s, mission 0x%04X%s)", r.id, fight ? "fight now" : rcore::RouteName(r.route), r.missionId, fight ? ", at the boss" : "");
    ToastF(cGreen, 8000, fight ? "Rematch: straight to %s..." : "Rematch: starting %s", r.name);
}

// E (row >= 0: E1 redirect to its record) / game's pick (row < 0)
void ExecEmaki(int row, ULONGLONG t) {
    if (!g_emakiOk) return Refuse(g_emakiWhy);
    Scroll sc;
    if (!FindScroll(sc)) return Refuse("no Picture Scroll in the inventory (the carrier item)");
    EmakiTab tab;
    if (!ReadEmakiTab(tab)) return Refuse("Picture Scroll table not readable");
    uint8_t before[3 * 0x58];
    if (!Copy(before, G_QUEUE, sizeof(before))) return Refuse("mission queue not readable");
    if (row >= 0) {
        const Row& r = g_rows[(size_t)row];
        if ((uint32_t)r.emakiIndex >= tab.count) return Refuse("record index out of range");
        if (!rcore::MapRedirect(g_map, (uint8_t*)tab.vb, tab.n, tab.emptyKey, (uint32_t)r.emakiIndex, tab.count, &WrGuard))
            return Refuse("could not redirect the scroll map");
        Log("Rematch: E1 %s: %d map entries -> record %d (snapshot CRC %08X)", r.id, tab.n, r.emakiIndex, rcore::Crc32(g_map.snap, (size_t)tab.n * 8));
    }
    char kb[40];
    Hex(kb, sizeof(kb), (uint64_t)sc.key, 12);
    if (CallEmakiRequest(sc.ref, sc.key) != 0) {
        MapRestoreNow("request exception");
        return Refuse("exception in EmakiMissionRequest");
    }
    uint8_t after[3 * 0x58];
    bool queued = false;
    if (Copy(after, G_QUEUE, sizeof(after)))
        for (int i = 0; i < 3; ++i) {
            uint32_t m = 0;
            memcpy(&m, after + i * 0x58 + 4, 4);
            queued |= m == 0xCC96 && memcmp(after + i * 0x58, before + i * 0x58, 0x58) != 0;
        }
    if (!queued) {
        MapRestoreNow("no slot took the request");
        return Refuse("the game did not queue the Picture Scroll (queue full?)");
    }
    g_state.store(kEmaki);
    g_activeRow.store(row);
    g_eRow = row;
    g_eByteSeen = 0;
    g_et.st = rcore::kEArmed;
    g_et.armedAt = t;
    Log("Rematch: E %s: EmakiMissionRequest(ref %016llX, key %s [item %04X idx %d], 0, params) queued 0xCC96", row >= 0 ? g_rows[(size_t)row].id : "(game's pick)",
        (unsigned long long)sc.ref, kb, sc.itemId, sc.idx);
    Result("started %s (picture scroll)", row >= 0 ? g_rows[(size_t)row].id : "game's pick");
    ToastF(cGreen, 8000, "Rematch: Picture Scroll %s", row >= 0 ? g_rows[(size_t)row].name : "(game's pick)");
}

void TrackReplay(const Snap& s, ULONGLONG t) {
    const int st = g_state.load();
    if (st == kMenuOpen) {
        const int busy = ReplayMenuBusy();
        if (busy == 1) {
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
            Log("Rematch: replay start seen %.1f s after the menu opened", (double)(t - g_openAt) / 1000.0);
            return;
        }
        if (!g_menuSeen) {
            if (t - g_openAt > 6000) {
                Log("Rematch: the replay menu never showed (+0x48) in 6 s - idle");
                ToIdle("replay menu not seen");
            }
            return;
        }
        if (busy != 1) {
            if (!g_closedSince) g_closedSince = t;
            else if (t - g_closedSince > 5000) {
                Log("Rematch: replay menu closed without a start");
                ToIdle("replay menu closed");
            }
        }
        return;
    }
    if (st != kInReplay) return;
    if (!g_replayMs) {
        if (s.st[2]) {
            g_loadSeen = true;
            g_loadDoneAt = 0;
        } else if (g_loadSeen && s.player && s.msOk) {
            if (!g_loadDoneAt) g_loadDoneAt = t;
            else if (t - g_loadDoneAt > 3000) {
                g_replayMs = s.ms14;
                g_replayIn.store(true);
                Log("Rematch: IN replay mission (stage %u) %.1f s after the start", s.ms14, (double)(t - g_replayAt) / 1000.0);
                Result("in mission %s", RowName(g_activeRow.load()));
                if (g_fight.load()) {
                    g_fightByMission = s.ms28 == g_fightMission;
                    Log("Rematch: F landed: retry point (+14h) %u %s start point %u; mission state +28h = 0x%X (%s)", s.ms14,
                        s.ms14 == g_fightStart ? "== the requested" : "!= the requested", g_fightStart, s.ms28,
                        g_fightByMission ? "the requested mission: exit = it changes" : "not the mission id: exit = the retry point changes, as R-b");
                    if (s.ms14 != g_fightStart) ToastF(cOrange, 8000, "Rematch: the game did not use the boss start point (see the log)");
                }
            }
        }
        if (!g_replayMs && t - g_replayAt > 180000) {
            Log("Rematch: no mission load within 180 s - idle");
            ToIdle("replay load timeout");
        }
        return;
    }
    if (g_fight.load() && g_fightByMission) {  // Fight now: the mission id decides (a retry or a script retry point keeps it)
        if (s.st[2] && !g_fightLoading) {
            ++g_fightLoads;
            Log("Rematch: F load inside the mission #%d (death / retry, or leaving) - retry point %u", g_fightLoads, s.ms14);
        }
        g_fightLoading = s.st[2] != 0;
        if (!s.msOk || s.ms28 == g_fightMission || s.st[2]) {
            g_exitSince = 0;
            return;
        }
    } else if (s.msOk && s.ms14 == g_replayMs && !s.st[2]) {
        g_exitSince = 0;
        return;
    }
    if (!g_exitSince) g_exitSince = t;
    else if (t - g_exitSince > 5000) {
        Log("Rematch: replay mission exited (stage now %u, mission state +28h 0x%X)%s", s.ms14, s.ms28,
            g_fight.load() ? " - Fight now over (nothing was patched, nothing to restore)" : "");
        Result("exited %s", RowName(g_activeRow.load()));
        ToIdle("replay mission exit");
    }
}

void TrackEmaki(const Snap& s, ULONGLONG t) {
    if (g_state.load() != kEmaki) return;
    if (s.online && g_map.active) {
        static ULONGLONG warned = 0;
        if (t - warned > 30000) {
            warned = t;
            Log("Rematch: WARNING online session during a redirected Picture Scroll (peers load their own map)");
            ToastF(cRed, 8000, "Rematch: online session during a patched Picture Scroll");
        }
    }
    const int ev = rcore::ETick(g_et, t, s.emaki, s.st[2] != 0);
    if (ev == 1) {
        uint64_t a = 0, ch = 0;
        if (Rd(G_CH, a) && a && Rd(a, ch) && ch) Rd(ch + 0x2BEF, g_eByteSeen);
        Log("Rematch: IN Picture Scroll mission (emaki byte %02X)%s", g_eByteSeen, g_map.active ? " - redirect live until the mission ends" : "");
        Result("in mission %s (emaki byte %02X)", g_eRow >= 0 ? g_rows[(size_t)g_eRow].id : "game's pick", g_eByteSeen);
    } else if (ev == 2) {
        Log("Rematch: Picture Scroll mission ended");
        Result("exited %s", g_eRow >= 0 ? g_rows[(size_t)g_eRow].id : "game's pick");
        ToIdle("picture scroll exit");
    } else if (ev == 3) {
        Log("Rematch: the Picture Scroll never loaded within 30 s - restored");
        ToIdle("armed timeout");
        Refuse("the Picture Scroll did not start (timeout, restored)");
    }
}

void Tick() {
    const ULONGLONG t = Ms();
    g_lastTick.store(t);
    Snap s = ReadSnap();
    if (s.qHash != g_qHash) {
        if (g_qHash) g_qChangedAt.store(t);
        g_qHash = s.qHash;
    }
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
            else if (g_replayIn.load() || g_et.st == rcore::kEIn) Refuse("inside the mission (it restores on exit)");
            else {
                Log("Rematch: abort - restoring now");
                Result("aborted");
                ToIdle("abort");
            }
        } else if (const char* why = Gates(s, false)) {
            if (t - g_pendAt > 5000) {
                g_pend = 0;
                Refuse(why);
            }
        } else {
            g_pend = 0;
            if (kind == kReqDirect) ExecDirect(row, s, t, false);
            else if (kind == kReqFight) ExecDirect(row, s, t, true);
            else if (kind == kReqEmaki) ExecEmaki(row, t);
            else if (kind == kReqScroll) ExecEmaki(-1, t);
            else ExecOpen(row, s);
        }
    }
    TrackReplay(s, t);
    TrackEmaki(s, t);
}

// ---- catalogue ----
bool ReadText(const std::wstring& p, std::string& out) {
    HANDLE h = CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    char buf[8192];
    DWORD got = 0;
    while (ReadFile(h, buf, sizeof(buf), &got, nullptr) && got) out.append(buf, got);
    CloseHandle(h);
    return true;
}
std::wstring Beside(const wchar_t* key, const wchar_t* def) {
    wchar_t v[MAX_PATH] = {};
    GetPrivateProfileStringW(L"Rematch", key, def, v, MAX_PATH, IniPath());
    std::wstring p = v;
    if (p.find(L':') == std::wstring::npos && (p.empty() || (p[0] != L'\\' && p[0] != L'/'))) p = std::wstring(ModDir()) + p;
    return p;
}

// ---- request validation shared by UI and pipe; nullptr = ok ----
const char* CanStart(int row, uint32_t& kind) {
    const Row& r = g_rows[(size_t)row];
    if (r.route == rcore::kX) return r.reason[0] ? r.reason : "no route";
    if (!r.Verified() && !g_cfg.showUnverified) return "unverified ([Rematch] ShowUnverified = 1 allows it; verify_all.ps1 verifies)";
    if (r.route == rcore::kE) {
        if (!g_emakiOk) return g_emakiWhy;
        kind = kReqEmaki;
        return nullptr;
    }
    if (r.route == rcore::kD || ((r.route == rcore::kR || r.route == rcore::kH) && g_cfg.directStart)) {
        kind = kReqDirect;
        return nullptr;
    }
    if (!g_raOk) return g_raWhy;
    if (r.route == rcore::kH && !g_cfg.hiddenDuels) return "hidden duel: set [Rematch] HiddenDuels = 1 (or DirectStart = 1)";
    kind = kReqOpen;
    return nullptr;
}
// Fight now (straight to the boss): R / H / D rows with a start point next to the boss; opt-in, so no 'verified' gate
const char* CanFight(int row) {
    const Row& r = g_rows[(size_t)row];
    if (!g_cfg.fightNow) return "Fight now is off ([Rematch] FightNow = 1 turns it on)";
    if (!g_fnowOk) return g_fnowWhy;
    if (r.route != rcore::kR && r.route != rcore::kH && r.route != rcore::kD) return "Fight now is for replayable missions only";
    if (!r.fightStart) return "no start point next to this boss in the catalogue (use the replay)";
    return nullptr;
}

// ---- UI model (any thread) ----
int g_sel = 0, g_confirm = -9;
uint32_t g_pageGen0 = 0;
char g_msg[160] = "";
constexpr int kItOpen = -1, kItAbort = -2, kItScroll = -3;
constexpr int kItFight = 0x10000;  // kItFight + row = "Fight now (straight to the boss)" under that boss row

int Items(int* out, int cap) {
    int n = 0;
    if (g_raOk) out[n++] = kItOpen;
    if (g_emakiOk) out[n++] = kItScroll;
    if (g_state.load() != kIdle && !g_replayIn.load() && g_et.st != rcore::kEIn) out[n++] = kItAbort;
    // beaten / startable first, then the rest, then X
    for (int pass = 0; pass < 3; ++pass)
        for (size_t i = 0; i < g_rows.size() && n < cap; ++i) {
            const Row& r = g_rows[i];
            const uint8_t st = g_status[i].load();
            const int grp = r.route == rcore::kX ? 2 : (r.route == rcore::kE || st == 2) ? 0 : 1;
            if (grp != pass) continue;
            if (grp == 1 && !g_cfg.showAll) continue;
            if (grp == 2 && !g_cfg.showAll) continue;
            if (r.route == rcore::kH && !g_cfg.hiddenDuels && !g_cfg.directStart && !g_cfg.showAll) continue;
            out[n++] = (int)i;
            if (g_cfg.fightNow && g_fnowOk && r.fightStart && grp != 2 && n < cap) out[n++] = kItFight + (int)i;
        }
    return n;
}

}  // namespace

bool Start() {
    const wchar_t* ini = IniPath();
    if (!GetPrivateProfileIntW(L"Rematch", L"Enable", 0, ini)) {
        Log("Rematch: off ([Rematch] Enable = 0)");
        return false;
    }
    g_cfg.hiddenDuels = IniInt(L"Rematch", L"HiddenDuels", 0) != 0;
    g_cfg.directStart = IniInt(L"Rematch", L"DirectStart", 0) != 0;
    g_cfg.showAll = IniInt(L"Rematch", L"ShowAll", 1) != 0;
    g_cfg.showUnverified = IniInt(L"Rematch", L"ShowUnverified", 0) != 0;
    g_cfg.fightNow = IniInt(L"Rematch", L"FightNow", 0) != 0;
    const char* why = nullptr;
    if (!GroupOk("CORE", &why) || !GroupOk("FIGHT", &why) || !GroupOk("PUMP", &why)) {
        Log("Rematch: off - %s", why);
        return false;
    }
    G_DATA = Site("g:DataDb");
    G_CLEAR = Site("g:ClearDb");
    G_MS = Site("g:MissionMgr");
    G_QUEUE = Site("g:MissionQueue");
    G_PM = Site("g:PlayMode");
    G_NET = Site("g:NetMgr");
    G_CH = Site("g:CharMgr");
    G_GM = Site("g:GameMain");
    G_PT = Site("g:PlayerTab");
    F_CLEARED = Site("fn:MissionClearedGetSlot");
    F_REQBYID = Site("fn:MissionRequestById");
    g_raOk = GroupOk("RA", &g_raWhy);
    if (g_raOk) {
        F_OPENREPLAY = Site("fn:MenuOpenMissionReplay");
        G_MENUMGR = Site("g:MenuMgr");
    }
    g_emakiOk = GroupOk("EMAKI", &g_emakiWhy);
    g_fnowOk = GroupOk("FNOW", &g_fnowWhy);
    if (g_emakiOk) {
        F_EMAKIREQ = Site("fn:EmakiMissionRequest");
        G_INV = Site("g:Inventory");
    }
    const std::wstring cpath = Beside(L"Catalog", L"Nioh3BossRematch_bosses.tsv");
    std::string text;
    if (!ReadText(cpath, text) || rcore::ParseCatalogue(text, g_rows, kMaxRows) <= 0) {
        Log("Rematch: off - catalogue %ls not found or without id/name/route columns", cpath.c_str());
        g_rows.clear();
        return false;
    }
    std::string vt;
    int nv = 0;
    const std::wstring vpath = Beside(L"Verified", L"Nioh3BossRematch_verified.tsv");
    if (ReadText(vpath, vt)) nv = rcore::ApplyVerified(vt, g_rows);
    if (!InstallGroup("FIGHT", &why)) {
        Log("Rematch: off - FIGHT hooks: %s", why);
        return false;
    }
    if (!InstallPump(&Tick, &why)) {
        Log("Rematch: off - pump: %s", why);
        return false;
    }
    g_live = true;
    int c[5] = {}, nf = 0, nfh = 0;
    for (const Row& r : g_rows) {
        c[r.route]++;
        nf += r.fightStart != 0;
        nfh += r.fightStart != 0 && !strcmp(r.fightConf, "high");
    }
    Log("Rematch: on - %zu catalogue rows (R %d, H %d, D %d, E %d, X %d; %d verified by verify_all); R-a %s%s, E %s%s; HiddenDuels %d, DirectStart %d, "
        "ShowAll %d, ShowUnverified %d. Solo/offline only; no story-flag writes.",
        g_rows.size(), c[0], c[1], c[2], c[3], c[4], nv, g_raOk ? "on" : "OFF: ", g_raOk ? "" : g_raWhy, g_emakiOk ? "on" : "OFF: ",
        g_emakiOk ? "" : g_emakiWhy, g_cfg.hiddenDuels, g_cfg.directStart, g_cfg.showAll, g_cfg.showUnverified);
    Log("Rematch: Fight now (E1 straight to the boss) %s - %d rows with a start point next to the boss (%d high confidence)%s%s", g_cfg.fightNow ? "ON" : "off",
        nf, nfh, g_fnowOk ? "" : "; sites FNOW failed: ", g_fnowOk ? "" : g_fnowWhy);
    return true;
}

bool Live() { return g_live; }

void UiOpen() {
    g_sel = 0;
    g_confirm = -9;
    g_msg[0] = 0;
    g_pageGen0 = g_scanGen.load();
    g_wantScan.store(true);
}
void UiMove(int dir) {
    static int it[kMaxRows + 4];
    const int n = Items(it, kMaxRows + 4);
    g_sel = n ? (g_sel + dir + n) % n : 0;
    g_confirm = -9;
    g_msg[0] = 0;
}
bool UiAct() {
    static int it[kMaxRows + 4];
    const int n = Items(it, kMaxRows + 4);
    if (!n) return false;
    if (g_sel >= n) g_sel = n - 1;
    const int item = it[g_sel];
    if (item == kItAbort) {
        Post(kReqAbort, -1, "the menu");
        return true;
    }
    const char* why = Gates(ReadSnap(), true);
    if (why) {
        snprintf(g_msg, sizeof(g_msg), "Not now: %s.", why);
        return false;
    }
    if (item == kItOpen) {
        Post(kReqOpen, -1, "the menu");
        return true;
    }
    if (item == kItScroll) {
        if (!g_cfg.showUnverified) {
            snprintf(g_msg, sizeof(g_msg), "Unverified: [Rematch] ShowUnverified = 1 allows it.");
            return false;
        }
        Post(kReqScroll, -1, "the menu");
        return true;
    }
    if (item >= kItFight) {
        const int row = item - kItFight;
        if (const char* w = CanFight(row)) {
            snprintf(g_msg, sizeof(g_msg), "%s", w);
            return false;
        }
        if (g_confirm != item) {
            g_confirm = item;
            const Row& r = g_rows[(size_t)row];
            snprintf(g_msg, sizeof(g_msg), "Fight %s now? Starts at the %s %d m from it. A confirm / B back. Solo only.", r.name, r.fightKind, r.fightDist);
            return false;
        }
        g_confirm = -9;
        Post(kReqFight, row, "the menu");
        ToastF(cWhite, 6000, "Rematch: straight to %s...", g_rows[(size_t)row].name);
        return true;
    }
    uint32_t kind = 0;
    if (const char* w = CanStart(item, kind)) {
        snprintf(g_msg, sizeof(g_msg), "%s", w);
        return false;
    }
    if (g_confirm != item) {
        g_confirm = item;
        snprintf(g_msg, sizeof(g_msg), "Start rematch: %s? A confirm / B back. Solo only. Drops: as the game's replay / scroll.", g_rows[(size_t)item].name);
        return false;
    }
    g_confirm = -9;
    Post(kind, item, "the menu");
    ToastF(cWhite, 6000, "Rematch queued: %s...", g_rows[(size_t)item].name);
    return true;
}
bool UiBack() {
    if (g_confirm != -9) {
        g_confirm = -9;
        g_msg[0] = 0;
        return false;
    }
    return true;
}

int UiFill(UiRow* rows, int cap, char* footer, size_t nf) {
    int k = 0;
    auto add = [&](uint32_t c, bool sel, const char* fmt, ...) {
        if (k >= cap) return;
        va_list a;
        va_start(a, fmt);
        vsnprintf(rows[k].text, sizeof(rows[k].text), fmt, a);
        va_end(a);
        rows[k].color = c;
        rows[k].sel = sel;
        ++k;
    };
    const Snap s = ReadSnap();
    const char* gate = g_state.load() != kIdle ? nullptr : Gates(s, true);
    const int st = g_state.load();
    add(cCyan, false, "Boss rematch (Nioh3BossRematch " ADDON_VERSION ")  -  %s", st == kMenuOpen ? "active: replay menu" : st == kInReplay ? (g_fight.load() ? "active: fight now" : "active: replay")
                                                                                  : st == kEmaki ? "active: picture scroll" : gate ? gate : "ready");
    static int it[kMaxRows + 4];
    const int n = Items(it, kMaxRows + 4);
    if (g_sel >= n) g_sel = n ? n - 1 : 0;
    const bool scanned = g_scanGen.load() != g_pageGen0;
    const int vis = cap - 2;
    int first = g_sel - vis / 2;
    if (first > n - vis) first = n - vis;
    if (first < 0) first = 0;
    for (int i = first; i < n && k < cap - 1; ++i) {
        const bool sel = i == g_sel;
        const char* mk = sel ? ">" : " ";
        if (it[i] == kItOpen) add(sel ? cWhite : cGray, sel, "%s Open the game's Battle Scroll replay menu   (R-a)", mk);
        else if (it[i] == kItScroll) add(sel ? cWhite : cGray, sel, "%s Use an owned Picture Scroll (the game's pick)   (E0)", mk);
        else if (it[i] == kItAbort) add(cYellow, sel, "%s Abort and restore now", mk);
        else if (it[i] >= kItFight) {
            const Row& r = g_rows[(size_t)(it[i] - kItFight)];
            const uint8_t stt = g_status[(size_t)(it[i] - kItFight)].load();
            const uint32_t c = stt == 2 ? (!strcmp(r.fightConf, "high") ? cGreen : !strcmp(r.fightConf, "medium") ? cYellow : cOrange) : cDim;
            add(sel ? cWhite : c, sel, "%s    Fight now (straight to the boss)   %s, %d m   [%s]", mk, r.fightKind, r.fightDist, r.fightConf);
        } else {
            const Row& r = g_rows[(size_t)it[i]];
            const uint8_t stt = g_status[(size_t)it[i]].load();
            const char* status = r.route == rcore::kX ? "-" : r.route == rcore::kE ? "" : !scanned ? "?" : stt == 2 ? "beaten" : stt == 1 ? "not beaten" : "error";
            const uint32_t c = r.route == rcore::kX ? cDim : (r.route != rcore::kE && stt != 2) ? cDim : !r.Verified() ? cGray : cGreen;
            add(sel ? cWhite : c, sel, "%s %-40.40s %-10.10s %-10.10s %-14.14s %s", mk, r.name, r.region, status, rcore::RouteName(r.route),
                r.route == rcore::kX ? "" : r.Verified() ? "" : "unverified");
        }
    }
    if (footer && nf) {
        footer[0] = 0;
        const int item = n ? it[g_sel] : kItOpen;
        if (g_msg[0]) snprintf(footer, nf, "%s", g_msg);
        else if (item >= kItFight) {
            const Row& r = g_rows[(size_t)(item - kItFight)];
            snprintf(footer, nf, "%s: starts the mission at start point %u (%s, %d m from the boss, %s confidence). A start, B back.",
                     r.mission[0] ? r.mission : r.name, r.fightStart, r.fightKind, r.fightDist, r.fightConf);
        } else if (item >= 0) {
            const Row& r = g_rows[(size_t)item];
            if (r.route == rcore::kX) snprintf(footer, nf, "No route: %s", r.reason);
            else snprintf(footer, nf, "%s - %s%s. A start, B back.", r.mission[0] ? r.mission : r.name, rcore::RouteName(r.route),
                          r.placement ? "" : "");
        } else if (item == kItOpen) snprintf(footer, nf, "Opens the game's own replay list (whole mission). Best at a shrine, menus closed.");
        else if (item == kItScroll) snprintf(footer, nf, "Starts the first Picture Scroll in your inventory exactly as the item menu does (unverified).");
        else snprintf(footer, nf, "Restores everything now (only before the mission has loaded).");
    }
    return k;
}

bool Toast(char* buf, size_t n, uint32_t& color) {
    bool on = false;
    AcquireSRWLockShared(&g_lock);
    if (g_toast[0] && Ms() < g_toastUntil) {
        snprintf(buf, n, "%s", g_toast);
        color = g_toastColor;
        on = true;
    }
    ReleaseSRWLockShared(&g_lock);
    return on;
}

// ---- pipe commands (remote.cpp thread) ----
void Command(const char* line, char* reply, size_t n) {
    char cmd[32] = "", arg[64] = "";
    sscanf_s(line, "%31s %63s", cmd, (unsigned)sizeof(cmd), arg, (unsigned)sizeof(arg));
    if (!strcmp(cmd, "ping")) {
        snprintf(reply, n, "pong Nioh3BossRematch %s live=%d", ADDON_VERSION, g_live);
        return;
    }
    if (!g_live) {
        snprintf(reply, n, "error rematch not live (see Nioh3BossRematch.log)");
        return;
    }
    if (!strcmp(cmd, "status")) {
        const Snap s = ReadSnap();
        char pl[256], last[200];
        fight::LastFightPlacements(pl, sizeof(pl));
        AcquireSRWLockShared(&g_lock);
        memcpy(last, g_last, sizeof(last));
        ReleaseSRWLockShared(&g_lock);
        const char* gate = Gates(s, true);
        snprintf(reply, n, "state=%d row=%s fights=%ld infight=%ld emaki=%d loading=%u player=%d stage=%u gate=%s placements=[%s] last=%s",
                 g_state.load(), g_activeRow.load() >= 0 ? g_rows[(size_t)g_activeRow.load()].id : "-", fight::g_fights, fight::g_active, s.emaki,
                 s.st[2], s.player, s.ms14, gate ? gate : "ok", pl, last);
        return;
    }
    if (!strcmp(cmd, "scan")) {
        g_wantScan.store(true);
        snprintf(reply, n, "ok gen=%u", g_scanGen.load());
        return;
    }
    if (!strcmp(cmd, "row")) {  // row <id>: status of one row after a scan
        for (size_t i = 0; i < g_rows.size(); ++i)
            if (!strcmp(g_rows[i].id, arg)) {
                snprintf(reply, n, "id=%s route=%s mission=0x%04X emaki=%d placement=%u verified=%d status=%u gen=%u fight=%u/%s/%d", g_rows[i].id,
                         rcore::RouteName(g_rows[i].route), g_rows[i].missionId, g_rows[i].emakiIndex, g_rows[i].placement, g_rows[i].Verified(),
                         g_status[i].load(), g_scanGen.load(), g_rows[i].fightStart, g_rows[i].fightConf[0] ? g_rows[i].fightConf : "-", g_rows[i].fightDist);
                return;
            }
        snprintf(reply, n, "error no row %s", arg);
        return;
    }
    if (!strcmp(cmd, "abort")) {
        Post(kReqAbort, -1, "pipe");
        snprintf(reply, n, "queued");
        return;
    }
    if (!strcmp(cmd, "open") || !strcmp(cmd, "scroll")) {
        Post(!strcmp(cmd, "open") ? kReqOpen : kReqScroll, -1, "pipe");
        snprintf(reply, n, "queued");
        return;
    }
    if (!strcmp(cmd, "fight")) {  // fight <id>: Fight now (straight to the boss)
        for (size_t i = 0; i < g_rows.size(); ++i)
            if (!strcmp(g_rows[i].id, arg)) {
                if (const char* w = CanFight((int)i)) {
                    snprintf(reply, n, "refused %s", w);
                    return;
                }
                Post(kReqFight, (int)i, "pipe");
                snprintf(reply, n, "queued %s fight start=%u", g_rows[i].id, g_rows[i].fightStart);
                return;
            }
        snprintf(reply, n, "error no row %s", arg);
        return;
    }
    if (!strcmp(cmd, "start")) {
        for (size_t i = 0; i < g_rows.size(); ++i)
            if (!strcmp(g_rows[i].id, arg)) {
                uint32_t kind = 0;
                if (const char* w = CanStart((int)i, kind)) {
                    snprintf(reply, n, "refused %s", w);
                    return;
                }
                Post(kind, (int)i, "pipe");
                snprintf(reply, n, "queued %s kind=%u", g_rows[i].id, kind);
                return;
            }
        snprintf(reply, n, "error no row %s", arg);
        return;
    }
    snprintf(reply, n, "error unknown command (ping status scan row start fight abort open scroll)");
}

}  // namespace rematch
}  // namespace addon
