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
//      Two lines per boss: "Fight now (in the arena)" = the start above + a warp to a point just inside the arena trigger box
//      ([Rematch] FightNowWarp = 1) once the mission has loaded (and again after every reload inside it, until the fight starts),
//      through the game's own warp (the tail of the script native Player::SetPos 23AB4DC, replayed with our x y z yaw), so the
//      game's trigger starts the fight; "Shrine before the boss" = the start alone. Every BossBattleMode start logs the player
//      position; [Rematch] ArenaCapture = 1 also appends it to Nioh3BossRematch_arenas.tsv, which overrides the catalogue point.
// Every game call and every write runs on the game thread (the per-frame pump); the UI / pipe only post a request word. No story
// flag is ever written. Solo / offline only (gates re-checked on the game thread right before every call).
#include "addon.h"
#include "rematch_core.h"
#include "transition_cover.h"
#include "transition_native_read.h"
#include "../../menu_navigation.h"
#include "../../boss_moves.h"
#include "../../move_catalog.h"  // pc2 #23907: live move catalogue; the runtime list comes from Nioh3Training_AutoMoves
#include "move_cats.h"
#include "../../settings/store.h"

#include <atomic>
#include <new>
#include <cmath>
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
std::atomic<bool> g_live{false};
bool g_prepared = false, g_raOk = false, g_emakiOk = false, g_fnowOk = false, g_warpOk = false;
const char *g_raWhy = "", *g_emakiWhy = "", *g_fnowWhy = "", *g_warpWhy = "";
// WARP: Player::SetPos tail (sites chk:SetPos_tail_a / _b): call targets read from the two matches
uintptr_t F_SETPOSYAW, F_ACTORSYNC, F_POSNOTIFY, F_SCENEPOS, F_STAGEPATH, G_SCENE;
uintptr_t F_REQRETURN;  // EXIT: MissionRequestReturn 22396D8
// GATE: the boss gate's exclusive right (Fight now for gate bosses). The check core 3B2354 is called from Online::IsExclusiveRightsValue
// (3B2323) while the boss start function waits in Function::WaitWarpStart(gate); the key it polls is copied here and requested with
// RightsRequest 1CA52C4(slot, key) = what the door interaction (StartWarpByGadgetRights) / StartWarp do. Mission-local runtime state.
uintptr_t F_RIGHTSREQ;
bool g_gateOk = false;
const char* g_gateWhy = "";
using RightsCheckFn = uint8_t (*)(uint32_t, const uint8_t*, uint8_t);
RightsCheckFn g_rightsCheckOrig = nullptr;
std::atomic<uint32_t> g_gateWant{0}, g_gateChecks{0};
std::atomic<int> g_gateSlot{-1};
std::atomic<bool> g_gateSeen{false};
alignas(16) uint8_t g_gateKey[16];
uint8_t RightsCheckHook(uint32_t slot, const uint8_t* key, uint8_t mode) {
    const uint32_t want = g_gateWant.load(std::memory_order_relaxed);
    if (want && key) {
        uint32_t a = 0, b = 0;
        memcpy(&a, key + 4, 4);
        memcpy(&b, key + 8, 4);
        if ((a == want || b == want) && !g_gateSeen.load()) {
            memcpy(g_gateKey, key, 16);
            g_gateSlot.store((int)slot);
            g_gateSeen.store(true);
        }
        g_gateChecks.fetch_add(1, std::memory_order_relaxed);
    }
    return g_rightsCheckOrig(slot, key, mode);
}
bool g_exitOk = false;
const char* g_exitWhy = "";
// SPAWN (1.4.0): appear in the arena from the first frame. The game places the player (1C0F24C) and centres the stage streaming
// (223B8F0) on the warp-point record of the retry point = our start point; the add-on writes the arena point into that record's
// spawn x y z yaw (+40h..+4Fh) and clears its arrival-camera bit (+8 bit 4, 2239C3C) while the Fight now runs. In memory only:
// the start point id (the retry point the game may save) never changes, and the record is restored when the Fight now ends.
uintptr_t F_WPFIND;
bool g_spawnOk = false;
const char* g_spawnWhy = "";

struct Cfg {
    bool hiddenDuels = false, directStart = false, showAll = true, showUnverified = false, fightNow = true, fightWarp = true, arenaCapture = false,
         fightSpawn = true, anyBoss = true;  // anyBoss (#38216): unbeaten bosses open as a Practice boss right here
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

// Nioh3Training.asi arena swap (bridge 4506/4616): "Fight now (in the arena)" for a boss of the current world needs no mission load
void* TrainingExport(const char* name) {
    HMODULE m = GetModuleHandleW(L"Nioh3Training.asi");
    return m ? (void*)GetProcAddress(m, name) : nullptr;
}
// user #6779: Boss Rematch "Return to where I was" state
int ReturnInfo() {  // Nioh3Training_ReturnInfo bits: 1 spot stored, 2 boss defeated, 4 no boss, 8 swap running, 16 returning; -1 = no export
    using Fn = int (*)();
    const auto f = (Fn)TrainingExport("Nioh3Training_ReturnInfo");
    return f ? f() : -1;
}
// Set by ExecDirect when Nioh3Training swapped the boss in place: no native attempt exists for a hosted cover to wait on,
// so CoverTick ends that cover at once (Nioh3Training draws its own fade through Nioh3EngineFix_Fade).
std::atomic<bool> g_swappedInPlace{false};

// ---- requests ----
enum ReqKind : uint32_t { kReqNone = 0, kReqOpen = 1, kReqDirect = 2, kReqAbort = 3, kReqEmaki = 4, kReqScroll = 5, kReqFight = 6, kReqShrine = 7, kReqExit = 8, kReqPractice = 9, kReqPracticeExit = 10, kReqTeleport = 11 };
bool g_noSwap = false;  // game thread: kReqTeleport = Fight now through the NORMAL mission load (never Nioh3Training's in-place swap)
// #12444: the teleport's mission load is not plain: once the target world's stage is ready inside a load, end the loading screen early (lsfade) and
// request the boss gate sooner. [Rematch] TeleportFastLoad (1), TeleportLoadingScreenEndMs (-2 = Nioh3Training.ini [ArenaSwap] LoadingScreenEndMs, the
// Fight now value; -1 off), TeleportCoverReleaseMs (-2 = [ArenaSwap] CoverReleaseMs), TeleportGateMs (gate request delay after landing, 2500 = plain).
struct TfState {
    bool on = false, inLoad = false, lsDone = false;
    int row = -1, world = -1, lsMs = -1, coverMs = -1, gateMs = 2500, loads = 0, lastWorld = -2, lastStage = -2, covers = 0;
    ULONGLONG t0 = 0, loadAt = 0, readyAt = 0;
} g_tf;
std::atomic<uint32_t> g_req{0};  // kind | (row + 1) << 8
enum State : int { kIdle = 0, kMenuOpen = 1, kInReplay = 2, kEmaki = 3 };
std::atomic<int> g_state{kIdle};
std::atomic<int> g_activeRow{-1};
std::atomic<bool> g_replayIn{false};
std::atomic<bool> g_fight{false};  // the active replay is a Fight now start
std::atomic<bool> g_practiceOwned{false};  // game thread publishes only after the native attempt begins
std::atomic<uint32_t> g_loadMs{0};  // request -> mission load finished, last Fight now / replay (status 'loadms', the slow-load check)
std::atomic<ULONGLONG> g_lastTick{0}, g_qChangedAt{0};
uint64_t g_qHash = 0;

const char* RowName(int row) { return row >= 0 && (size_t)row < g_rows.size() ? g_rows[(size_t)row].name : ""; }

void Post(uint32_t kind, int row, const char* via) {
    g_req.store(kind | ((uint32_t)(row + 1) << 8));
    static const char* const kn[] = {"", "R-a open the replay menu", "R-b direct start", "abort", "Picture Scroll (E1 redirect)", "Picture Scroll (game's pick)",
                                     "Fight now (in the arena)", "Shrine before the boss", "leave the mission (return)", "Practice Yamagata (native shrine)", "return from practice", "Teleport to arena (fast load)"};
    Log("Rematch: request %s%s%s queued from %s", kn[kind], row >= 0 ? " for " : "", RowName(row), via);
}

// ---- snapshot (memory reads only; any thread) ----
struct Snap {
    bool gm = false, player = false, msOk = false, queueBusy = false, emaki = false, online = false, guest = false;
    bool queueOk = false, emakiOk = false, onlineOk = false, guestOk = false;
    int32_t playMode = 0;  // meaningful only with onlineOk; only native mode 0 is proven solo
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
        s.queueOk = true;
    }
    s.queueBusy = Ms() - g_qChangedAt.load() < 10000;
    uint64_t a = 0, ch = 0;
    uint8_t e = 0;
    s.emakiOk = Rd(G_CH, a) && a && Rd(a, ch) && ch && Rd(ch + 0x2C18, e);
    if (s.emakiOk) s.emaki = e != 0;
    uint64_t pm = 0, net = 0;
    uint8_t conn = 0;
    s.onlineOk = Rd(G_PM, pm) && pm && Rd(pm + 0x34, s.playMode);
    if (s.onlineOk) s.online = s.playMode == 1 || s.playMode == 2;
    s.guestOk = Rd(G_NET, net) && net && Rd(net + 0x1DC, conn);
    if (s.guestOk) s.guest = conn != 0;
    return s;
}
// Game-thread-owned admission evidence survives UI Idle and every cleanup path.
// Only the phase is published to UI readers. No reset API clears quarantine.
enum AttemptPhase : int { aNone, aAwaitLoad, aActive, aReturning, aQuarantined };
enum Admission : int { aNoEvidence, aPossible, aMatched };
struct NativeAttempt {
    uint64_t sequence = 0;
    Admission evidence = aNoEvidence;
    uint32_t mission = 0, requestedStart = 0, observedStart = 0;
    uint32_t sourceMission = 0, sourceRetry = 0, returnMission = 0, returnRetry = 0;
    uint8_t journey = 0;
    int row = -1, queueSlot = -1;
    bool scroll = false, callAttempted = false, callCompleted = false;
    bool afterValid = false, loadSeen = false, loadDown = false;
    bool returnAttempted = false, returnCompleted = false, standaloneReturn = false, standaloneDone = false;
    ULONGLONG requestedAt = 0, settledAt = 0;
    uint8_t before[3 * 0x58] = {}, after[3 * 0x58] = {};
    const char* reason = nullptr;
} g_attempt;
uint64_t g_attemptSequence = 0;
std::atomic<int> g_attemptPhase{aNone};
const char* AttemptStartGate() {
    const int phase = g_attemptPhase.load();
    return phase == aNone ? nullptr : phase == aQuarantined ? "native request uncertain; restart game before another replay" : "native replay/return still pending";
}
void AttemptQuarantine(const char* why) {
    if (g_attemptPhase.load() == aNone) return;
    if (!g_attempt.reason) g_attempt.reason = why;
    g_attemptPhase.store(aQuarantined);
}
void AttemptBegin(uint32_t mission, uint32_t start, uint8_t journey, int row, int slot,
                  ULONGLONG t, const Snap& s, const uint8_t* before, bool scroll = false) {
    g_attempt = NativeAttempt{};
    g_attempt.sequence = ++g_attemptSequence;
    g_attempt.mission = mission;
    g_attempt.requestedStart = start;
    g_attempt.journey = journey;
    g_attempt.sourceMission = s.ms28;
    g_attempt.sourceRetry = s.ms14;
    g_attempt.row = row;
    g_attempt.queueSlot = slot;
    g_attempt.scroll = scroll;
    g_attempt.requestedAt = t;
    memcpy(g_attempt.before, before, sizeof(g_attempt.before));
    g_attempt.evidence = aPossible;
    g_attempt.callAttempted = true;
    g_attemptPhase.store(aAwaitLoad);  // before a native call can enqueue then throw
}
bool AttemptMatch(const uint8_t* after, bool valid, bool completed, uint64_t carrier = 0, const uint8_t* key = nullptr) {
    g_attempt.callCompleted = completed;
    g_attempt.afterValid = valid;
    if (valid) memcpy(g_attempt.after, after, sizeof(g_attempt.after));
    if (!completed || !valid) {
        AttemptQuarantine(completed ? "post-call queue unreadable" : "request call exception");
        return false;
    }
    const uint8_t* q = after + g_attempt.queueSlot * 0x58;
    uint32_t mission = 0, start = 0;
    uint64_t mode = 0;
    memcpy(&mission, q + 4, 4); memcpy(&start, q + 0xC, 4); memcpy(&mode, q + 0x14, 8);
    g_attempt.observedStart = start;
    if (mission != g_attempt.mission || mode != 2 ||
        (g_attempt.requestedStart && start != g_attempt.requestedStart)) {
        AttemptQuarantine("post-call mission/start/mode mismatch");
        return false;
    }
    if (g_attempt.scroll) {
        uint64_t observedCarrier = 0;
        memcpy(&observedCarrier, q + 0x30, sizeof(observedCarrier));
        if (!key || observedCarrier != carrier || memcmp(q + 0x20, key, 12)) {
            AttemptQuarantine("scroll carrier/key mismatch");
            return false;
        }
    }
    g_attempt.evidence = aMatched;
    return true;
}

// Unknown session reads are never evidence of offline play. Cleanup of an owned
// spawn record remains available separately; this gates new native actions.
const char* SessionGate(const Snap& s) {
    if (!s.onlineOk || !s.guestOk) return "session state unreadable";
    if (s.online) return "online";
    if (s.guest) return "co-op session";
    if (s.playMode != 0) return "unsupported session mode";
    return nullptr;
}
const char* NativeGate(const Snap& s) {
    if (const char* why = SessionGate(s)) return why;
    if (!s.gm) return "game not ready";
    if (!s.msOk) return "mission state unreadable";
    return nullptr;
}
const char* ExitGate(const Snap& s) {
    if (const char* why = NativeGate(s)) return why;
    if (s.st[2]) return "loading";
    if (!s.ms28) return "not in a mission";
    return nullptr;
}
void AttemptObserve(const Snap& s, ULONGLONG t) {
    const int phase = g_attemptPhase.load();
    if (phase == aNone || phase == aQuarantined) return;
    if (SessionGate(s)) { AttemptQuarantine("session lost or connected during native attempt"); return; }
    if (phase == aReturning && g_attempt.standaloneReturn) {
        if (!s.gm) { g_attempt.settledAt = 0; return; }
        if (s.st[2]) { g_attempt.loadSeen = true; g_attempt.settledAt = 0; return; }
        if (!g_attempt.loadSeen || !s.msOk || !s.player || s.st[3] || s.st[4] ||
            !s.ms28 || s.ms28 == g_attempt.sourceMission) { g_attempt.settledAt = 0; return; }
        if (!g_attempt.settledAt || s.ms28 != g_attempt.returnMission || s.ms14 != g_attempt.returnRetry) {
            g_attempt.returnMission = s.ms28;
            g_attempt.returnRetry = s.ms14;
            g_attempt.settledAt = t;
        } else if (t - g_attempt.settledAt > 5000) g_attempt.standaloneDone = true;
        return;
    }
    if (phase != aAwaitLoad) return;
    if (!s.gm) { g_attempt.settledAt = 0; return; }
    if (s.st[2]) { g_attempt.loadSeen = true; g_attempt.settledAt = 0; return; }
    if (!s.msOk || !s.player || s.st[3] || s.st[4]) { g_attempt.settledAt = 0; return; }
    if (!g_attempt.loadSeen || g_attempt.evidence != aMatched || s.ms28 != g_attempt.mission ||
        (!g_attempt.scroll && s.ms14 != g_attempt.observedStart)) { g_attempt.settledAt = 0; return; }
    g_attempt.loadDown = true;
    if (!g_attempt.settledAt) g_attempt.settledAt = t;
    else if (t - g_attempt.settledAt > 3000) g_attemptPhase.store(aActive);
}
bool AttemptEffectsAllowed() {
    const int phase = g_attemptPhase.load();
    return phase != aQuarantined && phase != aReturning;
}
const char* AttemptReturnGate() {
    const int phase = g_attemptPhase.load();
    if (phase == aQuarantined) return "native request uncertain; no automatic recovery return";
    if (phase == aReturning) return "return already requested";
    if (phase == aAwaitLoad) return "native replay has not settled";
    return nullptr;
}
void AttemptReturning(const Snap& s) {
    // Legacy menu replay has no strict admission witness, but its return still
    // must not be retried after an exception with possible native side effects.
    if (g_attemptPhase.load() == aNone) {
        g_attempt = NativeAttempt{};
        g_attempt.sequence = ++g_attemptSequence;
        g_attempt.standaloneReturn = g_state.load() == kIdle;
        g_attempt.sourceMission = s.ms28;
        g_attempt.sourceRetry = s.ms14;
    }
    g_attempt.returnAttempted = true;
    g_attemptPhase.store(aReturning);
}
// Nioh3Training prewarm world load pending and unclaimed: not a reason to refuse, Fight now claims it (bridge 6473)
static bool XwPending() {
    using Fn = int (*)();
    static const Fn f = [] { HMODULE m = GetModuleHandleW(L"Nioh3Training.asi"); return m ? (Fn)GetProcAddress(m, "Nioh3Training_XwPending") : nullptr; }();
    return f && f();
}
// user 20261018: Nioh3Training owns the fight state while a session / summon is live or a swap / return runs (the stale-flag heal leaves it alone)
static bool TrainingOwnsFight() {
    using Fn = int (*)();
    if (const auto f = (Fn)TrainingExport("Nioh3Training_SessionActive")) if (f()) return true;
    const int ri = ReturnInfo();
    return XwPending() || (ri > 0 && (ri & (8 | 16)));
}
// user 20261018: our fight flag was found stale (BossBattleMode at a save load with no mission, no boss): the native boss mode itself may still
// be on (no fast travel), so 'Leave arena' stays offered until a load, a mission, a real fight or Leave arena itself ends it.
std::atomic<bool> g_staleBbm{false};
const char* Gates(const Snap& s, bool uiSide) {
    if (!g_live) return "not installed";

    if (const char* why = AttemptStartGate()) return why;
    if (const char* why = NativeGate(s)) return why;
    if (!s.queueOk) return "mission queue unreadable";
    if (!s.emakiOk) return "Picture Scroll state unreadable";
    if (s.st[2] && !XwPending()) return "loading";
    if (!uiSide && (s.st[3] | s.st[4])) return "game paused";  // uiSide: our own menu pauses the game (menu pause), so a click from it is not "paused"
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
// #12444 teleport fast load: the same two native calls Nioh3Training's [ArenaSwap] LoadingScreenEndMs (lsfade, #9182) makes for a cross-world Fight now
// (UiLoadingScreenEnd 3B1B6C closes the loading-screen widget; BlackFadeIn 345508(0, 30, 0) lifts the black fader that seq 0x12 would lift later). Game thread.
int CallLoadingScreenEnd() {
    if (!g_img.base) return 2;
    __try {
        ((void (*)())(g_img.base + 0x3B1B6C))();
        ((void (*)(uint32_t, float, float))(g_img.base + 0x345508))(0, 30.0f, 0.0f);
        return 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 1;
    }
}
int StageWorld(int* stage) {  // StageMgr [45B9E58]: +0x500 current world, +0x889 streamer state (5 = ready), as Nioh3Training CurWorld/StageState
    uint64_t sm = 0;
    uint8_t w = 0, st = 0;
    if (!g_img.base || !Rd(g_img.base + 0x45B9E58, sm) || !sm || !Rd(sm + 0x500, w) || !Rd(sm + 0x889, st)) { *stage = -1; return -1; }
    *stage = st;
    return w;
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

// ---- player position + the game's own warp (game thread) ----
uint64_t PlayerChar() {  // PlayerCharacterGet(0) 20A914 = [[PlayerTab] + 0xCD*0x18]
    uint64_t pt = 0, pl = 0;
    return Rd(G_PT, pt) && pt && Rd(pt + 0xCD * 0x18, pl) ? pl : 0;
}
bool PlayerPos(float out[4]) {  // character +0x110 = position (Player::SetPos reads it right after the write: chk:SetPos_tail_a)
    const uint64_t ch = PlayerChar();
    return ch && Copy(out, ch + 0x110, 16);
}
// The tail of Player::SetPos 23AB4DC (23AB658..23AB6F4) with our values: CharacterSetPositionYaw(ch, pos, yawDeg), the physics
// controller vcall +D0h, actor manager [[45B9E40]] +48h -> +D0h = pos + A429B4(manager) sync, 1C2A3CC(pos), scene cfg +10B0h = pos / +10C0h = 1 +
// 3A083C(cfg+500h, cfg+10B0h), StagePathInitStep(cfg+500h, 1) (stage streaming around the new position).
int g_warpStep = 0;  // the last CallWarp step reached (for the log when it fails)
int CallWarp(float x, float y, float z, float yawDeg) {
    const uint64_t ch = PlayerChar();
    if (!ch) return 1;
    alignas(16) float pos[4] = {x, y, z, 1.0f};
    g_warpStep = 0;
    __try {
        g_warpStep = 1;
        ((void (*)(uint64_t, const float*, float))F_SETPOSYAW)(ch, pos, yawDeg);
        const uint64_t a = *(const uint64_t*)(ch + 0x3B0);
        uint64_t c = *(const uint64_t*)(a + 0x28);
        if (!c) c = *(const uint64_t*)(a + 0x18);
        const uint64_t vt = *(const uint64_t*)c;
        g_warpStep = 2;
        ((void (*)(uint64_t)) * (const uint64_t*)(vt + 0xD0))(c);
        g_warpStep = 3;
        const uint64_t am = *(const uint64_t*)*(const uint64_t*)G_CH;  // 23AB685: mov rax,[rip+g]; 23AB693: mov rcx,[rax] (1.2.0 missed this load)
        memcpy((void*)(*(const uint64_t*)(am + 0x48) + 0xD0), (const void*)(ch + 0x110), 16);
        g_warpStep = 4;
        ((void (*)(uint64_t))F_ACTORSYNC)(am);
        g_warpStep = 5;
        ((void (*)(uint64_t))F_POSNOTIFY)(ch + 0x110);
        const uint64_t sc = *(const uint64_t*)G_SCENE;
        memcpy((void*)(sc + 0x10B0), (const void*)(ch + 0x110), 16);
        *(volatile uint8_t*)(sc + 0x10C0) = 1;
        g_warpStep = 6;
        ((void (*)(uint64_t, uint64_t))F_SCENEPOS)(sc + 0x500, sc + 0x10B0);
        g_warpStep = 7;
        ((void (*)(uint64_t, uint8_t))F_STAGEPATH)(sc + 0x500, 1);
        return 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 2;
    }
}
uintptr_t Rel32(uintptr_t insn, int immOff, int len) {
    int32_t d = 0;
    return Rd(insn + immOff, d) ? insn + len + d : 0;
}
bool ResolveWarp(const char** why) {
    const uintptr_t a = Site("chk:SetPos_tail_a"), b = Site("chk:SetPos_tail_b");
    uint8_t e[5] = {};
    if (!a || !b || !Copy(e, a + 0xB, 1) || !Copy(e + 1, a + 0x4A, 1) || !Copy(e + 2, b + 7, 1) || !Copy(e + 3, b + 0x33, 1) ||
        !Copy(e + 4, b + 0x48, 1) || e[0] != 0xE8 || e[1] != 0xE8 || e[2] != 0xE8 || e[3] != 0xE8 || e[4] != 0xE8) {
        *why = "Player::SetPos tail calls not where expected";
        return false;
    }
    F_SETPOSYAW = Rel32(a + 0xB, 1, 5);
    F_ACTORSYNC = Rel32(a + 0x4A, 1, 5);
    F_POSNOTIFY = Rel32(b + 7, 1, 5);
    F_SCENEPOS = Rel32(b + 0x33, 1, 5);
    F_STAGEPATH = Rel32(b + 0x48, 1, 5);
    G_SCENE = Rel32(b + 0xC, 3, 7);
    const uintptr_t am = Rel32(a + 0x2D, 3, 7);
    const uintptr_t lo = g_img.base, hi = g_img.end;
    for (uintptr_t f : {F_SETPOSYAW, F_ACTORSYNC, F_POSNOTIFY, F_SCENEPOS, F_STAGEPATH, G_SCENE})
        if (f < lo || f >= hi) {
            *why = "a Player::SetPos call target is outside the image";
            return false;
        }
    if (am != G_CH) {
        *why = "Player::SetPos actor manager differs from g:CharMgr";
        return false;
    }
    return true;
}

// SPAWN sites: the player placement's WarpPointFind / pos / yaw calls and the arrival camera's lookup must be the ones we patch for
bool ResolveSpawn(const char** why) {
    F_WPFIND = Site("fn:WarpPointFind");
    const uintptr_t pos = Site("chk:WarpPointPos_40"), yaw = Site("chk:WarpPointYaw_4C"), pr = Site("chk:PlayerPlace_Retry14"),
                    pf = Site("chk:PlayerPlace_Find"), cam = Site("chk:CamFlag4_Retry14");
    if (!F_WPFIND || !pos || !yaw || !pr || !pf || !cam) {
        *why = "a SPAWN site is missing";
        return false;
    }
    // 3B271C (pos) starts 0xA bytes before its +48h read, 3B2678 (yaw) 0xD bytes before its +4Ch read (sites_gen.h windows)
    if (Rel32(pf, 1, 5) != F_WPFIND || Rel32(pf + 0x18, 1, 5) != pos - 0xA || Rel32(pf + 0x2B, 1, 5) != yaw - 0xD) {
        *why = "the player placement does not call WarpPointFind / the record pos / yaw getters";
        return false;
    }
    if (Rel32(cam + 0xE, 1, 5) != F_WPFIND || Rel32(pr, 3, 7) != G_MS || Rel32(cam, 3, 7) != G_MS) {
        *why = "the placement / arrival camera do not read the retry point of g:MissionMgr";
        return false;
    }
    return true;
}
uint64_t CallWpFind(uint32_t id) {  // WarpPointFind 137DCC: a pure lookup in the point-set registry, 0 when not loaded
    __try {
        return ((uint64_t (*)(uint32_t))F_WPFIND)(id);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}
struct Spawn {
    int st = 0;            // 0 off, 1 wanted, 2 applied, 3 recovery pending, 4 conflict latched
    uint64_t rec = 0;      // the patched warp-point record
    uint32_t point = 0;    // its id = the Fight now start point
    float orig[4] = {}, mine[4] = {};
    uint32_t origFlags = 0, mineFlags = 0;
    int reapplied = 0;
    bool landCheck = false;  // log where the first frame after a load put the player
    ULONGLONG nextCheck = 0;
    char last[12] = "-";     // status 'spawn': - / pending / applied / arena / start / failed
} g_sp;
void SpawnLast(const char* v) { strncpy_s(g_sp.last, v, _TRUNCATE); }
bool SpawnWrite(uint64_t rec, const float* xyzw, uint32_t flags) { return WrBytes(rec + 0x40, xyzw, 16) && WrBytes(rec + 8, &flags, 4); }
bool SpawnRestore(const char* why);
// Game thread: apply once; ambiguous reload/rewrite retains recovery ownership.
void SpawnApply(const Row& r, const char* when) {
    if (!AttemptEffectsAllowed()) return;
    if (g_sp.st >= 3) return;
    const uint64_t rec = CallWpFind(g_sp.point);
    if (!rec) {
        if (g_sp.st == 2) g_sp.st = 3;
        return;  // lookup failure cannot discard an existing recovery record
    }
    uint32_t id = 0, flags = 0;
    float cur[4] = {};
    if (!Rd(rec, id) || id != g_sp.point || !Rd(rec + 8, flags) || !Copy(cur, rec + 0x40, 16)) {
        g_sp.st = g_sp.st == 2 ? 3 : 0;
        SpawnLast("failed");
        Log("Rematch: F %s: spawn override NOT applied - warp-point record %u at %016llX is not readable as expected (id %u)", r.id, g_sp.point,
            (unsigned long long)rec, id);
        return;
    }
    if (rec == g_sp.rec && g_sp.st == 2 && !memcmp(cur, g_sp.mine, 16) && flags == g_sp.mineFlags) return;  // still ours
    if (g_sp.st == 2) {
        // Reload/rewrite does not prove the old object's lifetime. Retain its
        // recovery record rather than adopting game-owned bytes as originals.
        g_sp.st = 4;
        SpawnLast("recovery");
        return;
    }
    if (g_sp.st != 2) {  // first time: the record must be the catalogue's start point (catalogue warp_x.. = the shipped +40h)
        if (!rcore::SpawnRecordMatches(r, cur)) {
            g_sp.st = 0;
            SpawnLast("failed");
            Log("Rematch: F %s: spawn override NOT applied - record %u spawns at %.0f %.0f %.0f, the catalogue says %.0f %.0f %.0f (warp fallback)", r.id,
                g_sp.point, cur[0], cur[1], cur[2], r.warp[0], r.warp[1], r.warp[2]);
            return;
        }
    } else
        ++g_sp.reapplied;
    memcpy(g_sp.orig, cur, 16);
    g_sp.origFlags = flags;
    memcpy(g_sp.mine, r.arena, 16);
    g_sp.mineFlags = rcore::SpawnFlags(flags);  // no arrival-camera preset (it is aimed for the start point); the camera follows the player
    g_sp.rec = rec;
    g_sp.st = 3;  // retain recovery BEFORE either write, including partial failure
    if (!SpawnWrite(rec, g_sp.mine, g_sp.mineFlags) || CallWpFind(g_sp.point) != rec ||
        !Rd(rec, id) || id != g_sp.point || !Copy(cur, rec + 0x40, 16) ||
        !Rd(rec + 8, flags) || memcmp(cur, g_sp.mine, 16) || flags != g_sp.mineFlags) {
        SpawnRestore("apply failed");
        SpawnLast("failed");
        Log("Rematch: F %s: spawn override NOT applied - record %u not writable (warp fallback)", r.id, g_sp.point);
        return;
    }
    g_sp.rec = rec;
    g_sp.st = 2;
    SpawnLast("applied");
    Log("Rematch: F %s: spawn override %s (%s): start point %u record %016llX spawn %.0f %.0f %.0f yaw %.0f flags %X -> arena %.0f %.0f %.0f yaw %.0f flags %X",
        r.id, g_sp.reapplied ? "RE-applied (the game rewrote the record)" : "applied", when, g_sp.point, (unsigned long long)rec, g_sp.orig[0], g_sp.orig[1],
        g_sp.orig[2], g_sp.orig[3], g_sp.origFlags, g_sp.mine[0], g_sp.mine[1], g_sp.mine[2], g_sp.mine[3], g_sp.mineFlags);
}
bool SpawnRestore(const char* why) {
    if (g_sp.st == 4) return false;
    if (g_sp.st >= 2) {
        g_sp.st = 3;
        g_sp.landCheck = false;
        float cur[4] = {};
        uint32_t flags = 0, id = 0;
        const uint64_t current = CallWpFind(g_sp.point);
        if (!current) return false;
        if (current != g_sp.rec) { g_sp.st = 4; return false; }
        if (!Rd(current, id) || !Copy(cur, current + 0x40, 16) || !Rd(current + 8, flags)) return false;
        if (id != g_sp.point ||
            (memcmp(cur, g_sp.mine, 16) && memcmp(cur, g_sp.orig, 16)) ||
            (flags != g_sp.mineFlags && flags != g_sp.origFlags)) { g_sp.st = 4; return false; }
        // Each field may already have been restored by a partial attempt.
        if (memcmp(cur, g_sp.orig, 16) && !WrBytes(current + 0x40, g_sp.orig, 16)) return false;
        if (flags != g_sp.origFlags && !WrBytes(current + 8, &g_sp.origFlags, 4)) return false;
        if (CallWpFind(g_sp.point) != current || !Rd(current, id) || id != g_sp.point ||
            !Copy(cur, current + 0x40, 16) || !Rd(current + 8, flags) ||
            memcmp(cur, g_sp.orig, 16) || flags != g_sp.origFlags) return false;
        Log("Rematch: spawn override restored and verified (%s): record %u", why, g_sp.point);
    }
    g_sp.st = 0;
    g_sp.rec = 0;
    g_sp.landCheck = false;
    return true;
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

bool MapReadGuard(void* dst, const void* src, size_t n) { return Copy(dst, (uint64_t)src, n); }
bool MapRestoreNow(const char* why) {
    if (!g_map.active) return true;
    if (g_map.conflict) return false;
    EmakiTab current;
    if (!ReadEmakiTab(current)) return false;  // unreadable is not permission to forget ownership
    if (current.vb != (uint64_t)g_map.entries || current.n != g_map.n) { g_map.conflict = true; return false; }
    const int r = rcore::MapRestore(g_map, &WrGuard, &MapReadGuard);
    Log("Rematch: E1 map recovery (%s): %s", why, r == 0 ? "original bytes verified" : "unresolved; new entries blocked");
    return r == 0;
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
bool g_gateReq = false;      // the gate right was requested for this start
ULONGLONG g_gateDue = 0;     // when to request it (landing + 1 s, after the arena warp when there is one)
// player position on the last frame before the latest load began (the game's boss intro can load + move the player after the
// arena trigger: Yamagata 2026-10-09 trigger at ~337700 -> load -> BossBattleMode at 334650), for the arena capture
float g_posLive[4] = {}, g_posAtLoad[4] = {};
bool g_posLiveOk = false, g_posAtLoadOk = false, g_wasLoading = false;
ULONGLONG g_loadRiseAt = 0;
bool g_fightByMission = false;                   // exit detection on mission state +28h (seen == the mission after the load)
bool g_fightLoading = false;
int g_fightLoads = 0;
bool g_fightWarp = false;        // "Fight now (in the arena)": warp after the load
bool g_warpPending = false;      // a warp is due once the mission is idle (load done, player, no fight)
ULONGLONG g_warpDue = 0;
int g_warps = 0, g_fightRow = -1;
// Fight now verification (per start): armed -> warped -> PASS (a BossBattleMode after the first warp) / FAIL (none within 8 s of
// the first warp, loading time not counted; up to 3 warps when the player is not inside the trigger box 1.5 s after a warp)
enum FnState : int { kFnIdle = 0, kFnArmed = 1, kFnWarped = 2, kFnPass = 3, kFnFail = 4 };
std::atomic<int> g_fn{kFnIdle};
LONG g_fnFights0 = 0;
ULONGLONG g_fnFirst = 0, g_fnCheckAt = 0, g_fnDeadline = 0, g_fnLastT = 0;
const char* FnName(int v) {
    static const char* const n[] = {"idle", "armed", "warped", "pass", "fail"};
    return v >= 0 && v <= 4 ? n[v] : "?";
}
uint64_t g_qHashAtOpen = 0;

void Refuse(const char* why) {
    Log("Rematch: refused: %s", why);
    Result("refused: %s", why);
    ToastF(cOrange, 6000, "Boss match refused: %s", why);
}
void ToIdle(const char* why, bool normalExit = false) {
    if (!normalExit) AttemptQuarantine(why);
    UnhideRestore(why);
    const bool restored = SpawnRestore(why);
    const bool mapRestored = MapRestoreNow(why);
    const int phase = g_attemptPhase.load();
    if (normalExit && restored && mapRestored && (phase == aActive || phase == aReturning)) {
        g_attempt = NativeAttempt{};
        g_attemptPhase.store(aNone);  // legacy operational exit, not training completion
        g_practiceOwned.store(false);
    } else if (phase != aNone) AttemptQuarantine(why);
    g_et.st = rcore::kEIdle;
    g_state.store(kIdle);
    g_replayIn.store(false);
    g_activeRow.store(-1);
    g_fight.store(false);
    g_fightMission = g_fightStart = 0;
    g_fightByMission = g_fightLoading = false;
    g_fightLoads = 0;
    g_fightWarp = g_warpPending = false;
    g_warps = 0;
    g_fightRow = -1;
    if (g_fn.load() == kFnArmed || g_fn.load() == kFnWarped) g_fn.store(kFnIdle);
    g_gateWant.store(0);
    g_gateReq = false;
}

void ExecOpen(int row, const Snap& s) {
    if (!SpawnRestore("before replay menu")) return Refuse("spawn recovery unresolved; no new replay admitted");
    if (!MapRestoreNow("before replay menu")) return Refuse("scroll-map recovery unresolved; no new entry admitted");
    if (const char* why = AttemptStartGate()) return Refuse(why);
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
    if (const auto pre = (void (*)())TrainingExport("Nioh3Training_Preload")) pre();  // boss models of this world start loading while the menu is open
    g_state.store(kMenuOpen);
    g_activeRow.store(row);
    g_openAt = Ms();
    g_menuSeen = false;
    g_closedSince = 0;
    g_msAtOpen = s.ms14;
    g_qHashAtOpen = s.qHash;
    Log("Rematch: R-a MenuOpenMissionReplay(0, 0, 6) called (row %s)", row >= 0 ? g_rows[(size_t)row].id : "-");
    Result("R-a opened the replay menu");
    ToastF(cGreen, 8000, row >= 0 ? "Boss Matches: pick '%s' in the replay list" : "Boss Matches: replay menu opened", row >= 0 ? g_rows[(size_t)row].mission : "");
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

// Nioh3Training.asi (optional, ally arena-swap 4506): swaps the boss in place with no mission load; absent -> native load as before.
HMODULE TrainingAsi() { return GetModuleHandleW(L"Nioh3Training.asi"); }
int TrainingFightNow(uint32_t missionId) {
    using Fn = int (*)(uint32_t);
    static Fn fn = [] { HMODULE m = TrainingAsi(); return m ? (Fn)GetProcAddress(m, "Nioh3Training_FightNow") : nullptr; }();
    return fn ? fn(missionId) : 0;
}
bool TrainingSwapAvailable() {
    const HMODULE m = TrainingAsi();
    return m && GetProcAddress(m, "Nioh3Training_FightNow");
}
void TrainingPreload() {
    using Fn = void (*)();
    static Fn fn = [] { HMODULE m = TrainingAsi(); return m ? (Fn)GetProcAddress(m, "Nioh3Training_Preload") : nullptr; }();
    static ULONGLONG last = 0;
    const ULONGLONG now = GetTickCount64();
    if (fn && now - last > 15000) { last = now; fn(); }
}

void ExecDirect(int row, const Snap& s, ULONGLONG t, bool fight, bool warp) {
    if (!SpawnRestore("before mission request")) return Refuse("spawn recovery unresolved; no new mission admitted");
    if (!MapRestoreNow("before mission request")) return Refuse("scroll-map recovery unresolved; no new entry admitted");
    if (const char* why = AttemptStartGate()) return Refuse(why);
    const Row& r = g_rows[(size_t)row];
    const char* why = nullptr;
    const uint32_t start = fight ? r.fightStart : 0;
    if (fight) {
        if (!g_cfg.fightNow) return Refuse("Fight now is off ([Rematch] FightNow = 0)");
        if (!g_fnowOk) return Refuse(g_fnowWhy);
        if (!start) return Refuse("no start point next to this boss in the catalogue (use the replay)");
        if (!warp && r.atFight) return Refuse("no separate shrine: this mission starts at the fight");
        if (warp && !r.atFight && (!g_cfg.fightWarp || !g_warpOk || !r.arenaOk))
            return Refuse(!g_cfg.fightWarp ? "the arena warp is off ([Rematch] FightNowWarp = 0)" : !g_warpOk ? g_warpWhy : "no arena position for this boss yet");
        Log("Rematch: F %s: gates ok (solo/offline, not loading, not paused, no fight, nothing active); checking the clear on this journey", r.id);
    }
    if (!ClearedNow(r.missionId, &why)) return Refuse(why);
    if (fight && warp && !g_noSwap) {  // swapped in place: no queue slot, no cover, nothing armed (the add-on keeps the native request for every other case)
        using FightNowFn = int (*)(uint32_t);
        static const auto swap = (FightNowFn)TrainingExport("Nioh3Training_FightNow");
        if (swap && swap(r.missionId)) {
            Log("Rematch: F %s: arena swap in place by Nioh3Training (no mission load)", r.id);
            g_swappedInPlace.store(true);
            Result("started %s (fight now, in place)", r.id);
            ToastF(cGreen, 6000, "Boss Matches: into the fight with %s...", r.name);
            return;
        }
    }
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
    AttemptBegin(r.missionId, start, slot, row, freeSlot, t, s, before);
    const int callResult = CallRequestById(r.missionId, slot, start);
    if (callResult != 0) {
        AttemptMatch(nullptr, false, false);
        return Refuse("native request outcome uncertain after exception; restart game");
    }
    const uint8_t* q = after + freeSlot * 0x58;
    uint32_t qid = 0;
    uint64_t mode = 0;
    const bool rd = Copy(after, G_QUEUE, sizeof(after));
    if (rd) {
        memcpy(&qid, q + 4, 4);
        memcpy(&mode, q + 0x14, 8);
    }
    if (!AttemptMatch(after, rd, true)) {
        Log("Rematch: native request #%llu uncertain (id %u, mode %llu): %s", (unsigned long long)g_attempt.sequence,
            qid, (unsigned long long)mode, g_attempt.reason);
        return Refuse("native request outcome uncertain; no fight effects armed; restart game");
    }
    char hb[0x58 * 3 + 4];
    Hex(hb, sizeof(hb), (uint64_t)q, 0x58);
    if (fight) {
        uint32_t sp = 0;
        memcpy(&sp, q + 0xC, 4);
        Log("Rematch: F %s: queued - slot %d +4 mission 0x%04X, +0xC start point %u, +0x14 mode 2", r.id, freeSlot, r.missionId, sp);
        g_fight.store(true);
        g_fightMission = r.missionId;
        g_fightStart = start;
        g_fightByMission = g_fightLoading = false;
        g_fightLoads = 0;
        g_fightRow = row;
        g_fightWarp = warp && !r.atFight && r.arenaOk && g_cfg.fightWarp && g_warpOk;
        g_gateSeen.store(false);
        g_gateSlot.store(-1);
        g_gateChecks.store(0);
        g_gateReq = false;
        g_gateDue = 0;
        g_gateWant.store(warp && !r.atFight && r.GateDoor() && g_gateOk ? r.gateId : 0);
        if (g_gateWant.load())
            Log("Rematch: F %s: boss gate %u (%s) armed: its exclusive right will be requested once the boss start waits for it", r.id, r.gateId, r.gateKind);
        g_warpPending = false;
        g_warps = 0;
        g_fn.store(warp ? kFnArmed : kFnIdle);
        g_fnFights0 = fight::g_fights;
        if (g_fightWarp)
            Log("Rematch: F %s: arena warp armed: %.0f %.0f %.0f yaw %.0f (%s, %s) after the load", r.id, r.arena[0], r.arena[1], r.arena[2], r.arena[3],
                r.arenaSrc, r.arenaConf);
        SpawnRestore("new Fight now");
        SpawnLast("-");
        g_sp.reapplied = 0;
        if (g_fightWarp && g_cfg.fightSpawn && g_spawnOk) {  // appear there from the first frame; the warp stays as the fallback
            g_sp.st = 1;
            g_sp.point = start;
            SpawnLast("pending");
            Log("Rematch: F %s: spawn override armed: start point %u will spawn at the arena point once the load begins (record %s now)", r.id, start,
                CallWpFind(start) ? "loaded" : "not loaded yet");
        } else if (g_fightWarp)
            Log("Rematch: F %s: no spawn override (%s) - warp after the landing", r.id, !g_cfg.fightSpawn ? "[Rematch] FightNowSpawn = 0" : g_spawnWhy);
        else if (warp)
            Log("Rematch: F %s: no warp needed - the mission starts at the fight", r.id);
        g_tf = TfState{};
        if (g_noSwap && IniInt(L"Rematch", L"TeleportFastLoad", 1)) {
            const std::wstring tini = std::wstring(ModDir()) + L"Nioh3Training.ini";
            auto pick = [&](const wchar_t* key, const wchar_t* tkey) {
                const int v = IniInt(L"Rematch", key, -2);
                return v != -2 ? v : (int)GetPrivateProfileIntW(L"ArenaSwap", tkey, -1, tini.c_str());
            };
            g_tf.on = true;
            g_tf.row = row;
            g_tf.world = r.world;
            g_tf.t0 = t;
            g_tf.lsMs = pick(L"TeleportLoadingScreenEndMs", L"LoadingScreenEndMs");
            g_tf.coverMs = pick(L"TeleportCoverReleaseMs", L"CoverReleaseMs");
            g_tf.gateMs = IniInt(L"Rematch", L"TeleportGateMs", 300);
            int st = -1;
            const int w = StageWorld(&st);
            Log("Rematch: TELEPORT fast load armed for %s (#12444): target world 0x%02X (now 0x%02X, stage %d), LoadingScreenEndMs %d, CoverReleaseMs %d, gate %d ms after landing",
                r.id, r.world & 0xFF, w & 0xFF, st, g_tf.lsMs, g_tf.coverMs, g_tf.gateMs);
        }
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
    Result("started %s (%s, mission 0x%04X%s)", r.id, fight ? (warp ? "fight now" : "shrine before the boss") : rcore::RouteName(r.route), r.missionId,
           fight ? ", at the boss" : "");
    ToastF(cGreen, 8000, fight ? (warp ? "Boss Matches: into the fight with %s..." : "Boss Matches: to the shrine before %s...") : "Boss Matches: starting %s", r.name);
}

// E (row >= 0: E1 redirect to its record) / game's pick (row < 0)
int PracticeRow() {
    int found = -1;
    for (size_t i = 0; i < g_rows.size(); ++i) {
        const Row& r = g_rows[i];
        if (strcmp(r.id, "R1CBC")) continue;
        if (found >= 0 || r.route != rcore::kR || r.missionId != 0x1CBC || r.placement != 65092 ||
            r.fightStart != 67064 || r.atFight) return -1;
        found = (int)i;
    }
    return found;
}
const char* PracticeReturnGate(const Snap& s) {
    if (!g_practiceOwned.load()) return "no owned Yamagata practice session";
    const int row = PracticeRow();
    if (row < 0 || g_activeRow.load() != row) return "practice route ownership does not match";
    if (const char* why = ExitGate(s)) return why;
    if (s.ms28 != 0x1CBC) return "not in the owned practice mission";
    if (const char* why = AttemptReturnGate()) return why;
    return g_attemptPhase.load() == aActive ? nullptr : "practice transaction is not active";
}
void ExecPractice(int row, const Snap& s, ULONGLONG t) {
    if (row < 0 || row != PracticeRow()) return Refuse("Yamagata practice catalogue tuple missing or changed");
    if (const char* why = Gates(s, false)) return Refuse(why);
    const uint64_t previous = g_attempt.sequence;
    ExecDirect(row, s, t, true, false); // native shrine only: no warp, spawn override or gate injection
    if (g_attempt.sequence != previous && g_attemptPhase.load() != aNone) g_practiceOwned.store(true);
}

void ExecEmaki(int row, ULONGLONG t) {
    if (!SpawnRestore("before scroll request")) return Refuse("spawn recovery unresolved; no new scroll admitted");
    if (!MapRestoreNow("before scroll request")) return Refuse("scroll-map recovery unresolved; no new entry admitted");
    if (const char* why = AttemptStartGate()) return Refuse(why);
    if (!g_emakiOk) return Refuse(g_emakiWhy);
    Scroll sc;
    if (!FindScroll(sc)) return Refuse("no Picture Scroll in the inventory (the carrier item)");
    EmakiTab tab;
    if (!ReadEmakiTab(tab)) return Refuse("Picture Scroll table not readable");
    uint8_t before[3 * 0x58];
    if (!Copy(before, G_QUEUE, sizeof(before))) return Refuse("mission queue not readable");
    int freeSlot = -1;
    for (int i = 0; i < 3 && freeSlot < 0; ++i) {
        uint32_t mission = 0;
        memcpy(&mission, before + i * 0x58 + 4, 4);
        if (!mission && !before[i * 0x58 + 8]) freeSlot = i;
    }
    if (freeSlot < 0) return Refuse("mission queue full");
    const Snap current = ReadSnap();
    if (const char* why = NativeGate(current)) return Refuse(why);
    if (row >= 0) {
        const Row& r = g_rows[(size_t)row];
        if ((uint32_t)r.emakiIndex >= tab.count) return Refuse("record index out of range");
        if (!rcore::MapRedirect(g_map, (uint8_t*)tab.vb, tab.n, tab.emptyKey, (uint32_t)r.emakiIndex, tab.count, &WrGuard, &MapReadGuard))
            return Refuse("could not redirect the scroll map");
        Log("Rematch: E1 %s: %d map entries -> record %d (snapshot CRC %08X)", r.id, tab.n, r.emakiIndex, rcore::Crc32(g_map.snap, (size_t)tab.n * 8));
    }
    char kb[40];
    Hex(kb, sizeof(kb), (uint64_t)sc.key, 12);
    AttemptBegin(0xCC96, 0, 0, row, freeSlot, t, current, before, true);
    const int rc = CallEmakiRequest(sc.ref, sc.key);
    uint8_t after[3 * 0x58] = {};
    const bool readable = rc == 0 && Copy(after, G_QUEUE, sizeof(after));
    const bool matched = AttemptMatch(after, readable, rc == 0, sc.ref, sc.key);
    if (!matched) {
        MapRestoreNow("uncertain scroll request");
        return Refuse("native scroll request outcome uncertain; restart game");
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
    ToastF(cGreen, 8000, "Boss Matches: Picture Scroll %s", row >= 0 ? g_rows[(size_t)row].name : "(game's pick)");
}

// game thread, inside the Fight now mission: warp into the arena trigger once the mission is idle
void DoWarp(const Snap& s, ULONGLONG t) {
    if (NativeGate(s) || !AttemptEffectsAllowed()) return;  // unknown/quarantined requests cannot authorize mutation
    if (s.ms28 != g_fightMission) return;  // a different mission cannot supply fight/position evidence
    if (g_fightRow < 0) return;
    const Row& r = g_rows[(size_t)g_fightRow];
    if (s.st[2] || !s.player || s.st[3] || s.st[4]) return;  // loading / no player / paused: wait
    if (fight::g_active) {
        g_warpPending = false;
        Log("Rematch: F %s: the fight is already on - no warp", r.id);
        if (g_fn.load() == kFnArmed) {
            g_fn.store(kFnPass);
            Log("Rematch: F PASS %s: the fight began before the warp", r.id);
            Result("fight now PASS %s (fight before the warp)", r.id);
        }
        return;
    }
    if (g_sp.st == 2) {  // spawned in the arena: only a player still standing at the start point (override did not take) is warped
        float p[4] = {};
        if (!PlayerPos(p)) return;  // the controller is not readable yet: try next frame
        const float sx = p[0] - r.warp[0], sz = p[2] - r.warp[2], ax = p[0] - r.arena[0], az = p[2] - r.arena[2];
        if (!rcore::AtStartPoint(r, p)) {
            g_warpPending = false;
            Log("Rematch: F %s: no warp - the player spawned in the arena (start point override): %.0f %.0f %.0f, %.1f m from the arena point, %.1f m from "
                "the start point",
                r.id, p[0], p[1], p[2], sqrtf(ax * ax + az * az) / 100.0f, sqrtf(sx * sx + sz * sz) / 100.0f);
            if (g_fn.load() == kFnArmed) {
                g_fn.store(kFnWarped);
                g_fnFirst = g_fnLastT = t;
                g_fnDeadline = t + 60000;
                g_fnCheckAt = t + 1500;
            }
            return;
        }
        Log("Rematch: F %s: the player is at the start point despite the spawn override - warping as before", r.id);
    }
    if (s.ms28 != g_fightMission || s.ms14 != g_fightStart) {
        g_warpPending = false;
        Log("Rematch: F %s: not warping - mission 0x%X / retry point %u is not the Fight now start (the script moved the retry point)", r.id, s.ms28,
            s.ms14);
        if (g_fn.load() == kFnArmed) {
            g_fn.store(kFnFail);
            Result("fight now FAIL %s: not at the start point (mission 0x%X, retry point %u)", r.id, s.ms28, s.ms14);
        }
        return;
    }
    if (s.online || s.guest) {
        g_warpPending = false;
        if (g_fn.load() == kFnArmed) g_fn.store(kFnFail);
        return Refuse("online / co-op during Fight now: no warp");
    }
    float before[4] = {}, after[4] = {};
    if (!PlayerPos(before)) return;  // the controller is not readable yet: try next frame
    g_warpPending = false;
    const int rc = CallWarp(r.arena[0], r.arena[1], r.arena[2], r.arena[3]);
    PlayerPos(after);
    ++g_warps;
    if (g_fn.load() == kFnArmed) {
        g_fn.store(kFnWarped);
        g_fnFirst = t;
        g_fnDeadline = t + 60000;  // an intro cutscene after the trigger can be long; loading / paused time is added on top
    }
    g_fnCheckAt = t + 1500;
    g_fnLastT = t;
    Log("Rematch: F %s: warp #%d into the arena (%s): Player::SetPos path -> %s (step %d); player %.0f %.0f %.0f -> %.0f %.0f %.0f (target %.0f %.0f %.0f yaw %.0f) %.1f s "
        "after the start",
        r.id, g_warps, r.arenaSrc, rc == 0 ? "ok" : rc == 1 ? "NO PLAYER" : "EXCEPTION", g_warpStep, before[0], before[1], before[2], after[0], after[1], after[2],
        r.arena[0], r.arena[1], r.arena[2], r.arena[3], (double)(t - g_replayAt) / 1000.0);
    if (rc != 0) {
        g_fightWarp = false;
        if (g_fn.load() == kFnWarped) {
            g_fn.store(kFnFail);
            Log("Rematch: F FAIL %s: the warp call failed (step %d)", r.id, g_warpStep);
            Result("fight now FAIL %s: warp call failed (step %d)", r.id, g_warpStep);
        }
        ToastF(cRed, 8000, "Boss Matches: the arena warp failed (see the log); you are at the start point");
    } else {
        Log("Rematch: F %s: after warp #%d the player is %s the trigger box", r.id, g_warps, rcore::InTriggerBox(r, after) ? "inside" : r.boxOk ? "OUTSIDE" : "(no box data)");
        ToastF(cGreen, 5000, "Boss Matches: into the arena - %s", r.name);
    }
}

// game thread, every tick inside the Fight now mission: PASS = a BossBattleMode after the first warp; re-warp (<= 3) when the
// player is not in the trigger box 1.5 s after a warp; FAIL = no fight 8 s after the first warp (loading time not counted)
void FnVerify(const Snap& s, ULONGLONG t) {
    if (NativeGate(s) || !AttemptEffectsAllowed()) return;  // unknown/quarantined requests cannot authorize mutation
    if (s.ms28 != g_fightMission) return;  // replay exit settles later; never act on the destination mission
    if (g_fn.load() != kFnWarped || g_fightRow < 0) return;
    const Row& r = g_rows[(size_t)g_fightRow];
    const ULONGLONG dt = g_fnLastT ? t - g_fnLastT : 0;
    g_fnLastT = t;
    if (fight::g_fights > g_fnFights0) {
        float p[4] = {};
        PlayerPos(p);
        g_fn.store(kFnPass);
        Log("Rematch: F PASS %s: BossBattleMode %.1f s after warp #1 (%d warp%s), player %.0f %.0f %.0f", r.id, (double)(t - g_fnFirst) / 1000.0, g_warps,
            g_warps == 1 ? "" : "s", p[0], p[1], p[2]);
        Result("fight now PASS %s %.1fs warps=%d", r.id, (double)(t - g_fnFirst) / 1000.0, g_warps);
        return;
    }
    if (s.st[2] || s.st[3] || s.st[4]) {  // loading (a boss intro) / paused: the clock stops
        g_fnDeadline += dt;
        g_fnCheckAt += dt;
        return;
    }
    if (t >= g_fnDeadline) {
        float p[4] = {};
        PlayerPos(p);
        g_fn.store(kFnFail);
        Log("Rematch: F FAIL %s: no BossBattleMode within %s (%d warps, gate %s); player %.0f %.0f %.0f %s the trigger box (%s)", r.id, g_gateReq ? "180 s of the gate request" : g_fightWarp ? "60 s of warp #1" : "60 s of the landing", g_warps,
            g_gateReq ? "requested" : g_gateWant.load() ? "never polled" : "-", p[0], p[1],
            p[2], rcore::InTriggerBox(r, p) ? "inside" : "outside", r.arenaSrc);
        Result("fight now FAIL %s: no fight in time (warps=%d)", r.id, g_warps);
        return;
    }
    if (s.ms14 == g_fightStart && !g_fightWarp && !g_gateReq && r.atFight && r.bossOk && !g_warps && t - g_fnFirst > 10000 && !fight::g_active) {
        float p[4] = {};
        if (PlayerPos(p)) {
            float dx = p[0] - r.boss[0], dz = p[2] - r.boss[2];
            const float d = sqrtf(dx * dx + dz * dz);
            if (d > 600.0f) {  // farther than 6 m: the duel range is 8 m
                dx /= d;
                dz /= d;
                const float x = r.boss[0] + dx * 400.0f, z = r.boss[2] + dz * 400.0f, y = r.boss[1] + 100.0f;
                const float yaw = atan2f(r.boss[0] - x, r.boss[2] - z) * 57.2957795f;
                const int rc = g_warpOk ? CallWarp(x, y, z, yaw) : 9;
                ++g_warps;
                Log("Rematch: F %s: no fight 10 s after the landing at %.1f m from the boss (a duel starts inside 8 m) - %s to 4 m in front of it", r.id,
                    d / 100.0f, rc == 0 ? "warped" : "warp FAILED");
            }
        }
    }
    if (t >= g_fnCheckAt) {
        float p[4] = {};
        if (!PlayerPos(p)) return;
        const float dx = p[0] - r.arena[0], dz = p[2] - r.arena[2];
        const bool in = rcore::InTriggerBox(r, p) || dx * dx + dz * dz < 600.0f * 600.0f;
        // re-warp only when a load put the player back at the start point (the 1.2.1 failure), never when the game's own intro moved them
        const float sx = p[0] - r.warp[0], sz = p[2] - r.warp[2];
        const bool backAtStart = sx * sx + sz * sz < 500.0f * 500.0f;
        if (s.ms14 == g_fightStart && !in && backAtStart && g_fightWarp && !g_gateReq && g_warps < 3 && !fight::g_active) {
            Log("Rematch: F %s: player %.0f %.0f %.0f is not in the trigger box 1.5 s after warp #%d - warping again", r.id, p[0], p[1], p[2], g_warps);
            g_warpPending = true;
            g_warpDue = t;
        }
        g_fnCheckAt = t + 1500;
    }
}

// game thread, inside the Fight now mission: open the boss gate the way the game does (door / StartWarp), once
void GateTick(const Snap& s, ULONGLONG t) {
    if (NativeGate(s) || !AttemptEffectsAllowed()) return;  // unknown/quarantined requests cannot authorize mutation
    const uint32_t want = g_gateWant.load();
    if (!want || g_gateReq || !g_gateDue || t < g_gateDue || g_fightRow < 0) return;
    if (s.st[2] || s.st[3] || s.st[4] || !s.player || fight::g_active) {
        static ULONGLONG last = 0;
        if (last && t > last && t - last < 1000) g_gateDue += t - last;  // loading / paused time does not count
        last = t;
        return;
    }
    const Row& r = g_rows[(size_t)g_fightRow];
    if (s.online || s.guest || s.ms28 != g_fightMission) {
        g_gateWant.store(0);
        return;
    }
    if (!g_gateSeen.load()) {
        if (t - g_gateDue > 30000) {
            g_gateWant.store(0);
            Log("Rematch: F %s: the boss start never waited for gate %u in 30 s of play (%u exclusive-right checks seen) - not requesting", r.id, want,
                g_gateChecks.load());
            if (g_fn.load() == kFnArmed || (g_fn.load() == kFnWarped && !g_fightWarp)) {
                g_fn.store(kFnFail);
                Result("fight now FAIL %s: boss start not waiting for gate %u", r.id, want);
            }
        }
        return;
    }
    g_gateReq = true;
    alignas(16) uint8_t key[16];
    memcpy(key, g_gateKey, 16);
    const int slot = g_gateSlot.load();
    char hb[64];
    Hex(hb, sizeof(hb), (uint64_t)key, 16);
    int rc = 0;
    __try {
        ((void (*)(uint32_t, const uint8_t*))F_RIGHTSREQ)((uint32_t)slot, key);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        rc = 1;
    }
    Log("Rematch: F %s: requested the boss gate's exclusive right (slot %d, key %s) as the %s would -> %s", r.id, slot, hb,
        r.GateDoor() ? "boss door" : "arena box", rc ? "EXCEPTION" : "ok");
    if (g_fn.load() == kFnArmed) {
        g_fn.store(kFnWarped);
        g_fnFirst = g_fnLastT = t;
        g_fnCheckAt = t + 3600000;
    }
    if (g_fn.load() == kFnWarped) g_fnDeadline = t + 180000;  // the gate intro: fade, RTM cutscene (Okita, Solitary Blade: ~60 s unskipped), create
    if (rc) {
        g_fn.store(kFnFail);
        Result("fight now FAIL %s: gate request exception", r.id);
    }
}

// pipe 'exit' (the self-test): leave the mission the way the mission menu's leave does
int CallReturn() {
    alignas(16) uint32_t p[8] = {0x101, 0x1000000, 0x10100, 0, 0, 0, 0, 0};  // 1BF7D68's defaults; movaps [rcx] at 2239757
    __try {
        ((void (*)(const uint32_t*, uint8_t))F_REQRETURN)(p, 1);
        return 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 1;
    }
}

void TrackReplay(const Snap& s, ULONGLONG t) {
    if (!s.gm) { g_loadDoneAt = 0; g_exitSince = 0; return; }  // an unreadable stop block is not a load-down edge
    const int st = g_state.load();
    if (st == kMenuOpen) {
        const int busy = ReplayMenuBusy();
        if (busy == 1) {
            g_menuSeen = true;
            g_closedSince = 0;
        }
        const bool enq = s.queueOk && s.qHash != g_qHashAtOpen;
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
                if (const auto cancel = (void (*)())TrainingExport("Nioh3Training_PreloadCancel")) cancel();  // bridge 6550: a world prewarm must not outlive the menu
                ToIdle("replay menu not seen");
            }
            return;
        }
        if (busy != 1) {
            if (!g_closedSince) g_closedSince = t;
            else if (t - g_closedSince > 5000) {
                Log("Rematch: replay menu closed without a start");
                if (const auto cancel = (void (*)())TrainingExport("Nioh3Training_PreloadCancel")) cancel();  // bridge 6550: a world prewarm must not outlive the menu
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
        } else if (g_loadSeen && s.gm && s.player && s.msOk && !s.st[3] && !s.st[4]) {
            if (!g_loadDoneAt) g_loadDoneAt = t;
            else if (t - g_loadDoneAt > (g_tf.on ? 1000u : 3000u)) {  // #12444 teleport: 1 s settle instead of 3 s
                g_replayMs = s.ms14;
                g_replayIn.store(true);
                Log("Rematch: IN replay mission (stage %u) %.1f s after the start (the load itself ended %.1f s after it)", s.ms14,
                    (double)(t - g_replayAt) / 1000.0, (double)(g_loadDoneAt - g_replayAt) / 1000.0);
                g_loadMs.store((uint32_t)(g_loadDoneAt - g_replayAt));
                Result("in mission %s", RowName(g_activeRow.load()));
                if (g_fight.load()) {
                    g_fightByMission = s.ms28 == g_fightMission;
                    Log("Rematch: F landed: retry point (+14h) %u %s start point %u; mission state +28h = 0x%X (%s)", s.ms14,
                        s.ms14 == g_fightStart ? "== the requested" : "!= the requested", g_fightStart, s.ms28,
                        g_fightByMission ? "the requested mission: exit = it changes" : "not the mission id: exit = the retry point changes, as R-b");
                    if (s.ms14 != g_fightStart && strcmp(g_sp.last, "arena") != 0)
                        ToastF(cOrange, 8000, "Boss Matches: the game did not use the boss start point (see the log)");
                    if (g_fightWarp && !g_warps) {  // landed (3 s with no load and a player) + 1 s settle
                        g_warpPending = true;
                        g_warpDue = t + 1000;
                    }
                    if (g_gateWant.load()) g_gateDue = t + (g_fightWarp ? 2500 : 1000);
                    if (g_gateWant.load() && g_tf.on && g_tf.gateMs >= 0 && g_tf.gateMs < 2500 && strcmp(g_sp.last, "arena") == 0) {  // #12444: spawned in the arena, nothing to wait for
                        g_gateDue = t + (ULONGLONG)g_tf.gateMs;
                        Log("Rematch: TELEPORT gate request in %d ms (spawned in the arena; plain = 2500)", g_tf.gateMs);
                    }
                    if (!g_fightWarp && g_gateWant.load()) {
                        // gate only: GateTick requests the right and starts the verification
                    } else if (!g_fightWarp && g_fn.load() == kFnArmed) {  // the mission starts at the fight: no warp, just verify
                        g_fn.store(kFnWarped);
                        g_fnFirst = g_fnLastT = t;
                        g_fnDeadline = t + 60000;
                        g_fnCheckAt = t + 3600000;
                    }
                }
            }
        }
        if (!s.gm || !s.player || !s.msOk || s.st[3] || s.st[4]) g_loadDoneAt = 0;  // consecutive valid, unpaused frames
        // state-based: give up only when no load began within 120 s of the request, or a begun load never finished in 20 min
        if (!g_replayMs && ((!g_loadSeen && t - g_replayAt > 120000) || t - g_replayAt > 1200000)) {
            Log("Rematch: %s - idle", g_loadSeen ? "the mission load did not finish in 20 min" : "no mission load began within 120 s of the request");
            ToIdle("replay load timeout");
        }
        return;
    }
    if (g_fight.load() && g_fightByMission) {  // Fight now: the mission id decides (a retry or a script retry point keeps it)
        if (s.st[2] && !g_fightLoading) {
            ++g_fightLoads;
            Log("Rematch: F load inside the mission #%d (death / retry, or leaving) - retry point %u", g_fightLoads, s.ms14);
            if (fight::g_active) {  // dying never runs BossBattleModeEnd: the stale flag made DoWarp skip the re-entry (user saw the player left at the door)
                InterlockedExchange(&fight::g_active, 0);
                Log("Rematch: F load during a boss fight (death / retry) - fight flag cleared so the warp re-enters the arena");
                if (g_gateWant.load()) {  // the boss script restarts and waits for the gate again: re-arm the gate request after the warp
                    g_gateReq = false;
                    g_gateSeen.store(false);
                    g_gateDue = 0;
                }
            }
        }
        if (!s.st[2] && g_fightLoading && g_fightWarp && !(g_gateReq && g_fn.load() == kFnWarped)) {  // a reload (death / retry) ended: back into the arena; never during a gate intro
            g_warpPending = true;
            g_warpDue = t + 4500;  // 1.5 s after the 3 s settle the landing uses
            if (g_gateWant.load() && !g_gateDue) g_gateDue = t + 4500 + 2500;  // retry: request the gate right again after the warp
        }
        g_fightLoading = s.st[2] != 0;
        if (g_warpPending && t >= g_warpDue) DoWarp(s, t);
        GateTick(s, t);
        FnVerify(s, t);
        if (!s.msOk || s.ms28 == g_fightMission || s.st[2]) {
            g_exitSince = 0;
            return;
        }
    } else if (s.msOk && s.ms14 == g_replayMs && !s.st[2]) {
        if (g_warpPending && t >= g_warpDue) DoWarp(s, t);
        GateTick(s, t);
        FnVerify(s, t);
        g_exitSince = 0;
        return;
    }
    if (!s.msOk || !s.player || s.st[2] || s.st[3] || s.st[4]) { g_exitSince = 0; return; }
    if (!g_exitSince) g_exitSince = t;
    else if (t - g_exitSince > 5000) {
        Log("Rematch: replay mission exited (stage now %u, mission state +28h 0x%X)%s", s.ms14, s.ms28,
            g_fight.load() ? (g_sp.st == 2 ? " - Fight now over (the spawn record is restored next)" : " - Fight now over (nothing was patched, nothing to restore)") : "");
        Result("exited %s", RowName(g_activeRow.load()));
        ToIdle("replay mission exit", true);
    }
}

void TrackEmaki(const Snap& s, ULONGLONG t) {
    if (g_state.load() != kEmaki) return;
    if (!s.gm || !s.msOk || !s.emakiOk || s.st[3] || s.st[4]) { g_et.zeroSince = 0; return; }
    if (s.online && g_map.active) {
        static ULONGLONG warned = 0;
        if (t - warned > 30000) {
            warned = t;
            Log("Rematch: WARNING online session during a redirected Picture Scroll (peers load their own map)");
            ToastF(cRed, 8000, "Boss Matches: online session during a patched Picture Scroll");
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
        ToIdle("picture scroll exit", true);
    } else if (ev == 3) {
        Log("Rematch: the Picture Scroll never loaded within 30 s - restored");
        ToIdle("armed timeout");
        Refuse("the Picture Scroll did not start (timeout, restored)");
    }
}

// ---- [Rematch] SelfTest = R1CBC,REA75,...: the add-on runs Fight now on each row by itself (no pipe, no input; for the Ally /
// Proton and any device without the pipe). Per row: wait until idle in the field for 8 s -> fight -> PASS / FAIL (the Fight now
// verification) -> 4 s -> leave the mission (MissionRequestReturn) -> next. Log lines 'SELFTEST ...'; never runs online.
struct SelfTest {
    std::vector<int> rows;
    size_t next = 0;
    int phase = 0;  // 0 wait idle, 1 started, 2 result seen -> exit, 3 left, 4 done
    ULONGLONG since = 0, idleSince = 0;
    int pass = 0, fail = 0, skip = 0;
    bool exitSent = false;
    char table[600] = "";
} g_st;
void SelfTestTick(const Snap& s, ULONGLONG t) {
    SelfTest& st = g_st;
    if (st.rows.empty() || st.phase == 4) return;
    const int stt = g_state.load();
    if (st.phase == 0) {
        if (st.next >= st.rows.size()) {
            st.phase = 4;
            Log("Rematch: SELFTEST DONE pass=%d fail=%d skip=%d:%s", st.pass, st.fail, st.skip, st.table);
            ToastF(cCyan, 15000, "Boss Matches self-test: %d pass, %d fail, %d skip (see the log)", st.pass, st.fail, st.skip);
            return;
        }
        const bool idle = stt == kIdle && !Gates(s, false);
        if (!idle) {
            st.idleSince = 0;
            return;
        }
        if (!st.idleSince) st.idleSince = t;
        if (t - st.idleSince < 8000) return;
        const int row = st.rows[st.next];
        const Row& r = g_rows[(size_t)row];
        const char* why = nullptr;
        if (!ClearedNow(r.missionId, &why)) {
            Log("Rematch: SELFTEST %s SKIP: %s", r.id, why);
            const size_t k = strlen(st.table);
            snprintf(st.table + k, sizeof(st.table) - k, " %s=skip", r.id);
            ++st.skip;
            ++st.next;
            st.idleSince = 0;
            return;
        }
        Log("Rematch: SELFTEST %s (%zu/%zu): Fight now", r.id, st.next + 1, st.rows.size());
        Post(kReqFight, row, "self-test");
        st.phase = 1;
        st.since = t;
        return;
    }
    const Row& r = g_rows[(size_t)st.rows[st.next]];
    if (st.phase == 1) {
        const int fn = g_fn.load();
        const bool over = fn == kFnPass || fn == kFnFail;
        const bool gaveUp = (stt == kIdle && t - st.since > 15000 && fn != kFnWarped && fn != kFnArmed) || t - st.since > 300000;
        if (!over && !gaveUp) return;
        const bool ok = fn == kFnPass;
        float p[4] = {};
        PlayerPos(p);
        Log("Rematch: SELFTEST %s %s (%.1f s after the request, %d warp%s, spawn %s, player %.0f %.0f %.0f)", r.id, ok ? "PASS" : "FAIL",
            (double)(t - st.since) / 1000.0, g_warps, g_warps == 1 ? "" : "s", g_sp.last, p[0], p[1], p[2]);
        const size_t k = strlen(st.table);
        snprintf(st.table + k, sizeof(st.table) - k, " %s=%s", r.id, ok ? "PASS" : "FAIL");
        (ok ? st.pass : st.fail)++;
        st.exitSent = false;
        st.phase = stt == kIdle ? 3 : 2;
        st.since = t;
        return;
    }
    if (st.phase == 2) {
        if (t - st.since < 4000) return;
        if (stt == kIdle) {
            st.phase = 3;
            st.since = t;
            return;
        }
        if (!st.exitSent) {
            st.exitSent = true;
            Post(kReqExit, -1, "self-test");
        } else if (t - st.since > 120000) {  // never left: try again
            Post(kReqExit, -1, "self-test (again)");
            st.since = t - 4000;
        }
        return;
    }
    if (st.phase == 3) {
        ++st.next;
        st.phase = 0;
        st.idleSince = 0;
    }
}

// game thread, every tick while a spawn override is armed / applied
void SpawnTick(const Snap& s, ULONGLONG t, bool loadEnded) {
    if (!AttemptEffectsAllowed()) return;
    if (g_sp.st >= 3) return;  // no automatic mutation/reapply during recovery
    if (!g_sp.st || g_fightRow < 0 || !g_fight.load()) return;
    const Row& r = g_rows[(size_t)g_fightRow];
    if (s.online || s.guest) {
        Log("Rematch: F %s: online / co-op during Fight now - spawn override removed", r.id);
        SpawnRestore("online");
        SpawnLast("failed");
        return;
    }
    if (NativeGate(s)) return;  // retain ownership while state is unknown; do not apply/reapply
    if (g_sp.st == 1) {
        if (s.st[2]) SpawnApply(r, "load began");  // apply once the mission load runs (never in the mission we are leaving)
        else if (loadEnded) {
            g_sp.st = 0;
            SpawnLast("failed");
            Log("Rematch: F %s: spawn override NOT applied - warp-point record %u was never found during the load (warp fallback)", r.id, g_sp.point);
        }
        return;
    }
    if (loadEnded) g_sp.landCheck = true;
    if (s.st[2] || t >= g_sp.nextCheck) {  // keep it ours while loading (death / retry reload) and once a second otherwise
        g_sp.nextCheck = t + 1000;
        SpawnApply(r, "check");
    }
    if (g_sp.landCheck && !s.st[2] && s.player) {
        float p[4] = {};
        if (!PlayerPos(p)) return;
        g_sp.landCheck = false;
        const float ax = p[0] - r.arena[0], az = p[2] - r.arena[2], sx = p[0] - r.warp[0], sz = p[2] - r.warp[2];
        const bool atStart = rcore::AtStartPoint(r, p);
        if (strcmp(g_sp.last, "failed") != 0) SpawnLast(atStart ? "start" : "arena");
        Log("Rematch: F %s: first frame after the load (#%d in the mission): player %.0f %.0f %.0f - %s (%.1f m from the arena point, %.1f m from the start "
            "point)",
            r.id, g_fightLoads, p[0], p[1], p[2], atStart ? "AT THE START POINT (override did not take)" : "in the arena", sqrtf(ax * ax + az * az) / 100.0f,
            sqrtf(sx * sx + sz * sz) / 100.0f);
    }
}

// Only presentation state is shared here. Never clear native attempt/recovery state.
SRWLOCK g_coverLock = SRWLOCK_INIT;
transition_cover::Model g_cover;
uint64_t g_coverNext=0, g_coverHostAt=0;
bool g_hostedUiAction=false, g_coverReadOk=false;
bool g_coverControl=false,g_coverAlgo=false,g_coverLife=false,g_coverPlayer=false;
uint64_t g_coverSampleAt=0;
bool CoverRead(void*,uint64_t address,void* out,size_t n){return Copy(out,address,n);}
bool CoverAnchors() {
    struct Anchor{uint32_t rva;const char* bytes;size_t n;};
    const Anchor anchors[]={
        {0x23AB184,"\x48\x8B\x0D\xCD\x0B\x21\x02",7},
        {0x23AB18B,"\xB2\x48",2},{0x9DC710,"\xC6\x40\x01\x01",4},
        {0x563F1C,"\xC6\x40\x01\x00",4},{0x19D37F0,"\x38\x50\x08",3},
        {0x705B5D,"\x89\x41\x34",3},
        {0x486BE0,"\x48\x8B\x81\xB0\x03\x00\x00\x48\x85\xC0\x74\x14\x48\x8B\x40\x50",16},
        {0x486BF0,"\x48\x85\xC0\x74\x0B\x83\xB8\x74\x15\x00\x00\x01\x0F\x94\xC0\xC3",16}};
    // Pin the native DEAD predicate, supported default condition dispatch and character-ID getter.
    const Anchor lifeAnchors[]={
        {0x23A9110,"\x48\x85\xC0\x74\x15\x48\x8B\x80\xA0\x03\x00\x00\x48\x85\xC0\x74\x09\x83\x78\x48\x01\x0F\x94\x44\x24\x20",26},
        {0x23A910B,"\xE8\x04\x18\xE6\xFD",5},
        {0x4892A8,"\x40\x53\x48\x83\xEC\x20\x48\x8B\x81\xA0\x03\x00\x00\x48\x8B\xD9\x48\x85\xC0\x0F\x84\xB6\x00\x00\x00\x83\x78\x48\x01\x0F\x84\xA8\x00\x00\x00\x48\x8B\x81\xB0\x03\x00\x00\x48\x8B\x48\x08\x48\x85\xC9\x74\x16\x48\x8B\x01\xBA\x35\x00\x00\x00\xFF\x90\xC0\x00\x00\x00\x84\xC0\x0F\x85\x86\x00\x00\x00\x48\x8B\x83\xB0\x03\x00\x00\x48\x8B\x48\x08\x48\x85\xC9\x74\x12\x48\x8B\x01\xBA\xB0\x00\x00\x00\xFF\x90\xC0\x00\x00\x00\x84\xC0\x75\x2B\x48\x8B\x83\xB0\x03\x00\x00\x48\x8B\x48\x08\x48\x85\xC9\x74\x12\x48\x8B\x01\xBA\x2C\x00\x00\x00\xFF\x90\xC0\x00\x00\x00\x84\xC0\x75\x3E\x83\xBB\xD4\x08\x00\x00\x01\x7D\x35\x8B\x03\x83\xE0\xF0\x3D\xE0\x0F\x93\x00\x75\x2D\x48\x8B\x8B\xB0\x03\x00\x00\x48\x85\xC9\x74\x21\xE8\x6D\x68\xFF\xFF\x3D\x52\x4E\x00\x00\x75\x15\xBA\xAB\x07\x00\x00\x48\x8B\xCB\xE8\xB1\x73\xDF\xFF\x84\xC0\x74\x04\xB0\x01\xEB\x02\x32\xC0\x48\x83\xC4\x20\x5B\xC3",215},
        {0x2E9934,"\x4C\x8B\x81\xE8\x00\x00\x00",7},
        {0x2EA1A3,"\x48\x8B\x86\xA8\x00\x00\x00\x48\xC1\xE8\x2E\xE9\x85\xF9\xFF\xFF",16},
        {0x2EA447,"\x8B\x43\x18\xE9\x46\xF8\xFF\xFF",8},
        {0x2EB1CB,"\x48\x8B\x86\x18\x01\x00\x00\x48\x85\xC0\x0F\x84\x9F\xEF\xFF\xFF\x48\x8B\x40\x20\x8B\x40\x18\xE9\xAE\xEA\xFF\xFF",28},
        {0x2E9C95,"\x48\xC1\xE8\x08\xE9\x9A\xFE\xFF\xFF",9},
        {0x47FBC8,"\x48\x8B\x41\x08\x48\x85\xC0\x74\x1C\x48\x8B\x80\xE8\x00\x00\x00\x48\x85\xC0\x74\x10\x48\x83\x78\x20\x00\x74\x09\x48\x8B\x40\x20\x0F\xB7\x40\x0C\xC3\x33\xC0\xC3",40},
        {0x2EA0FA,"\x41\x83\xE9\x28\x0F\x84\x6F\x01\x00\x00\x45\x2B\xCE\x0F\x84\xE3\x00\x00\x00\x45\x2B\xCE\x0F\x84\xCF\x00\x00\x00\x45\x2B\xCE\x0F\x84\x94\x00\x00\x00\x45\x2B\xCE\x74\x7F",42},
        {0x2EA351,"\x41\x83\xE9\x32\x0F\x84\x10\x01\x00\x00\x45\x2B\xCE\x0F\x84\xFB\x00\x00\x00\x45\x2B\xCE\x0F\x84\xE2\x00\x00\x00\x45\x2B\xCE\x0F\x84\xD1\x00\x00\x00",37},
        {0x2EB152,"\x41\x81\xE9\xAC\x00\x00\x00\x0F\x84\xEB\x00\x00\x00\x45\x2B\xCE\x0F\x84\xCF\x00\x00\x00\x45\x2B\xCE\x0F\x84\x99\x00\x00\x00\x45\x2B\xCE\x74\x71\x45\x2B\xCE\x74\x50",41}};
    if(!g_img.base)return false;
    for(const auto& a:lifeAnchors){char bytes[256];const uint64_t at=g_img.base+a.rva;
        if(at<g_img.base||at+a.n>g_img.end||!Copy(bytes,at,a.n)||memcmp(bytes,a.bytes,a.n))return false;}
    for(const auto& a:anchors){char bytes[16];const uint64_t at=g_img.base+a.rva;
        if(at<g_img.base||at+a.n>g_img.end||!Copy(bytes,at,a.n)||memcmp(bytes,a.bytes,a.n))return false;}
    return true;
}
struct CoverLock {
    bool held=TryAcquireSRWLockExclusive(&g_coverLock)!=FALSE;
    ~CoverLock(){if(held)ReleaseSRWLockExclusive(&g_coverLock);}
};
bool CoverBegin(int row,uint64_t* token=nullptr) {
    CoverLock lock;if(!lock.held)return false;
    if(!g_coverReadOk)return false;
    const Row& r=g_rows[(size_t)row];
    if(r.missionId==0x1A5E && r.fightStart==5021112 && r.placement==5027806) {
        const Snap source=ReadSnap();
        if(!source.msOk || source.ms28==r.missionId) {
            Log("Rematch: covered dojo entry refused: leave the current dojo before starting a fresh encounter");
            return false;
        }
    }
    const bool started=g_cover.Begin(++g_coverNext,(uint32_t)row,r.missionId,r.fightStart,r.placement,Ms());
    if(started){g_swappedInPlace.store(false);g_coverHostAt=0;g_coverControl=g_coverAlgo=g_coverLife=g_coverPlayer=false;g_coverSampleAt=0;Log("Rematch: cover token=%llu phase=%u attempt=0 event=begin mission=0x%X start=%u placement=%u",g_cover.token,(unsigned)g_cover.phase,r.missionId,r.fightStart,r.placement);}
    if(started&&token)*token=g_cover.token;
    return started;
}
void CoverTick(const Snap& s, ULONGLONG) {
    CoverLock lock;if(!lock.held)return;
    // Tick's entry time can precede a concurrently accepted Present/Begin.
    // Sample after ownership of the cover state; genuine future stamps still fail.
    const ULONGLONG t=Ms();
    g_cover.Expire(t);
    if(g_cover.Active() && g_coverHostAt && (t<g_coverHostAt || t-g_coverHostAt>=1500))
        g_cover.End(transition_cover::Failed,"Cover renderer stale; native recovery remains owned");
    if(!g_cover.Active())return;
    if(g_cover.issued && g_swappedInPlace.exchange(false)) {
        Log("Rematch: cover token=%llu event=swapped_in_place (Nioh3Training arena swap, no native attempt to wait for)",g_cover.token);
        g_cover.End(transition_cover::Ready,"Swapped in place by Nioh3Training");
        return;
    }
    if(g_cover.phase==transition_cover::AwaitNative && !g_cover.issued) {
        Log("Rematch: cover token=%llu phase=%u attempt=0 event=present_ack",g_cover.token,(unsigned)g_cover.phase);
        if(g_req.load() || g_pend){g_cover.End(transition_cover::Failed,"Another request is pending; no transition request sent");return;}
        if(const char* why=Gates(s,false)){g_cover.End(transition_cover::Failed,why);return;}
        if(g_cover.PreplacedRoute() && (!s.msOk || s.ms28==g_cover.mission)) {
            g_cover.End(transition_cover::Failed,"Leave this dojo before starting another boss match");return;
        }
        if(g_cover.Issue(g_attemptSequence+1)){
            Log("Rematch: cover token=%llu phase=%u attempt=%llu event=native_post",g_cover.token,(unsigned)g_cover.phase,g_cover.attempt);
            Post(kReqFight,(int)g_cover.row,"presented hosted cover");
        }
    }
    const uint64_t playerBefore=PlayerChar();
    const auto player=transition_native_read::ReadPlayer(g_img.base,&CoverRead,nullptr);
    const auto control=transition_native_read::ReadControl(g_img.base,&CoverRead,nullptr);
    const auto actor=transition_native_read::ReadActor(g_img.base,g_cover.ActorPlacement(),&CoverRead,nullptr);
    const auto life=transition_native_read::ReadLife(g_img.base,g_cover.ActorPlacement(),actor,&CoverRead,nullptr);
    const Snap after=ReadSnap();
    const bool same=after.msOk&&s.msOk&&after.ms28==s.ms28&&after.ms14==s.ms14&&
        NativeGate(after)==nullptr&&after.player==s.player&&after.gm&&s.gm&&
        after.st[2]==s.st[2]&&after.st[3]==s.st[3]&&after.st[4]==s.st[4];
    const bool playerReady=player.known&&player.notDead&&player.live&&player.live==playerBefore&&player.live==PlayerChar();
    // A stable native DEAD result ends presentation immediately; missing/loading
    // player reads are not death, and this never cancels the native transaction.
    const bool playerKnown=player.known&&player.live&&player.live==playerBefore&&player.live==PlayerChar();
    g_cover.PlayerState(g_attempt.sequence,
        g_coverReadOk&&same&&g_attempt.evidence==aMatched&&g_attempt.mission==g_cover.mission&&
        s.ms28==g_cover.mission&&s.ms14==g_cover.start,playerKnown,player.notDead);
    if(!g_cover.Active())return;
    const bool controlReady=g_coverReadOk&&same&&control.known&&control.clear&&playerReady;
    // Conservative passive inverse of the native DEAD predicate; unknown is not ready.
    const bool lifeReady=life.known&&life.notDead;
    const bool bossReady=g_coverReadOk&&same&&actor.known&&actor.present&&actor.algoReady&&lifeReady;
    g_coverPlayer=playerReady;g_coverControl=controlReady;g_coverAlgo=actor.known&&actor.present&&actor.algoReady;g_coverLife=lifeReady;g_coverSampleAt=t;
    if(!controlReady)g_cover.reason="Waiting for player control restrictions to clear";
    else if(!bossReady)g_cover.reason="Waiting for the requested boss to be ready";
    const bool hadPlacement=g_cover.placementEntry;
    g_cover.Observe(t,g_attempt.sequence,g_attempt.evidence==aMatched&&g_attempt.mission==g_cover.mission,
        g_attemptPhase==aQuarantined,NativeGate(s)==nullptr,
        s.msOk && s.ms28==g_cover.mission && s.ms14==g_cover.start,
        s.st[2]!=0,s.player,s.st[3]||s.st[4],fight::g_active!=0,controlReady,bossReady);
    if(!hadPlacement && g_cover.placementEntry)
        Log("Rematch: cover token=%llu attempt=%llu event=existing_actor mission=0x%X start=%u actor=%u",g_cover.token,g_cover.attempt,s.ms28,s.ms14,g_cover.observedPlacement);
}
void HkCoverCreate(uint64_t placement,uint64_t,uint64_t,uint64_t,const uint64_t*) {
    CoverLock lock;if(!lock.held)return;
    const Snap s=ReadSnap();
    if(s.msOk&&s.ms14==g_cover.start){
        const uint32_t before=g_cover.observedPlacement;
        g_cover.Entry(g_attempt.sequence,s.ms28,(uint32_t)placement,false);
        if(g_cover.observedPlacement!=before)
            Log("Rematch: cover token=%llu attempt=%llu event=branch_actor mission=0x%X start=%u requested=%u actor=%u",g_cover.token,g_cover.attempt,s.ms28,s.ms14,g_cover.placement,g_cover.observedPlacement);
    }
}
// #12444 game thread: Teleport to arena fast load. Every load of the teleport (the mission load, then the load the boss gate starts) gets the
// lsfade treatment once the target world's stage streamer is ready (state 5) with a player: UiLoadingScreenEnd + BlackFadeIn after LoadingScreenEndMs.
void TfTick(const Snap& s, ULONGLONG t) {
    if (!g_tf.on) return;
    const char* end = !g_fight.load() || g_state.load() != kInReplay ? "the fight attempt ended"
                      : g_fn.load() == kFnPass                        ? "BossBattleMode started (Fight now PASS)"
                      : g_fn.load() == kFnFail                        ? "Fight now FAIL"
                      : t - g_tf.t0 > 300000                          ? "timeout 300 s"
                                                                      : nullptr;
    if (end && !(s.gm && s.st[2])) {
        Log("Rematch: TELEPORT fast load over +%.1f s: %s (%d loads)", (double)(t - g_tf.t0) / 1000.0, end, g_tf.loads);
        g_tf.on = false;
        return;
    }
    if (!s.gm) return;
    int stage = -1;
    const int w = StageWorld(&stage);
    if (w != g_tf.lastWorld || stage != g_tf.lastStage) {
        Log("Rematch: TELEPORT +%.1f s world 0x%02X stage %d (GAMESTATE[2] %u, player %d)", (double)(t - g_tf.t0) / 1000.0, w & 0xFF, stage, s.st[2], s.player ? 1 : 0);
        g_tf.lastWorld = w;
        g_tf.lastStage = stage;
    }
    if (s.st[2] && !g_tf.inLoad) {
        g_tf.inLoad = true;
        g_tf.lsDone = false;
        g_tf.readyAt = 0;
        g_tf.loadAt = t;
        ++g_tf.loads;
        Log("Rematch: TELEPORT load #%d began +%.1f s", g_tf.loads, (double)(t - g_tf.t0) / 1000.0);
    } else if (!s.st[2] && g_tf.inLoad) {
        g_tf.inLoad = false;
        Log("Rematch: TELEPORT load #%d ended +%.1f s (%.1f s long%s)", g_tf.loads, (double)(t - g_tf.t0) / 1000.0, (double)(t - g_tf.loadAt) / 1000.0,
            g_tf.readyAt ? ", stage was ready before" : ", stage never ready inside it");
        return;
    }
    if (!g_tf.inLoad) return;
    const bool ready = s.player && stage == 5 && (g_tf.world < 0 || w == g_tf.world) && t - g_tf.loadAt >= 300;
    if (ready && !g_tf.readyAt) {
        g_tf.readyAt = t;
        Log("Rematch: TELEPORT load #%d ready +%.1f s (%.1f s into the load): world 0x%02X stage 5, player", g_tf.loads, (double)(t - g_tf.t0) / 1000.0,
            (double)(t - g_tf.loadAt) / 1000.0, w & 0xFF);
    }
    if (!g_tf.readyAt) return;
    if (g_tf.lsMs >= 0 && !g_tf.lsDone && t - g_tf.readyAt >= (ULONGLONG)g_tf.lsMs) {
        g_tf.lsDone = true;
        const int rc = CallLoadingScreenEnd();
        Log("Rematch: TELEPORT LSEND load #%d +%.1f s: UiLoadingScreenEnd + BlackFadeIn(0, 30, 0) %s, %llu ms after ready (LoadingScreenEndMs %d)", g_tf.loads,
            (double)(t - g_tf.t0) / 1000.0, rc == 0 ? "called" : "EXCEPTION", (unsigned long long)(t - g_tf.readyAt), g_tf.lsMs);
    }
    if (g_tf.coverMs >= 0 && t - g_tf.readyAt >= (ULONGLONG)g_tf.coverMs) {
        uint64_t gm = 0;
        if (Rd(G_GM, gm) && gm) {
            __try {
                *(volatile uint8_t*)(gm + 0x588 + 2) = 0;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
            }
            if (g_tf.covers++ < 3)
                Log("Rematch: TELEPORT COVER load #%d: GAMESTATE[2] cleared by hand %llu ms after ready (CoverReleaseMs %d)", g_tf.loads,
                    (unsigned long long)(t - g_tf.readyAt), g_tf.coverMs);
        }
    }
}

void AutoMovesTick();  // pc2 #23907, end of file
void Tick() {
    const ULONGLONG t = Ms();
    g_lastTick.store(t);
    { static ULONGLONG nx; if (t >= nx) { nx = t + 1000; AutoMovesTick(); } }
    Snap s = ReadSnap();
    CoverTick(s,t);
    AttemptObserve(s, t);
    if (g_attemptPhase.load() == aReturning && g_attempt.standaloneDone) ToIdle("standalone return observed", true);
    if (g_attemptPhase.load() == aQuarantined && g_state.load() != kIdle) ToIdle("native attempt quarantined");
    if (s.gm && s.st[2] && !g_wasLoading) {  // a load begins: keep where the player stood on the frame before
        memcpy(g_posAtLoad, g_posLive, sizeof(g_posAtLoad));
        g_posAtLoadOk = g_posLiveOk;
        g_loadRiseAt = t;
        // user 20261018: a load ends any native boss mode; BossBattleModeEnd never runs for it, so the flag would block Boss Matches /
        // Practice for the rest of the session (seen after a reload out of a no-boss arena fight: 'in fight' flag lines after the load)
        const bool idle = g_state.load() == kIdle;
        if (rcore::ClearFightOnLoad(fight::g_active != 0, idle, TrainingOwnsFight())) {
            fight::ForceEnd("world load began");
            Log("Rematch: a world load began during a boss fight without BossBattleModeEnd - fight flag cleared");
        }
        if (idle && g_staleBbm.exchange(false)) Log("Rematch: world load - stale native boss mode no longer offered for Leave arena");
    }
    const bool loadEnded = s.gm && g_wasLoading && !s.st[2];
    if (s.gm) g_wasLoading = s.st[2] != 0;
    {   // user 20261018: BossBattleMode at a save load with no mission and no session = the no-boss arena mode: our flag is stale
        static ULONGLONG loadEndAt = 0;
        static bool wasActive = false, riseNearLoad = false;
        if (loadEnded) loadEndAt = t;
        const bool active = fight::g_active != 0;
        if (active && !wasActive) {
            riseNearLoad = rcore::FightRiseNearLoad(s.gm && s.st[2], t, loadEndAt);
            if (!riseNearLoad) g_staleBbm.store(false);  // a real fight began: not the stale case any more
        }
        wasActive = active;
        if (active && riseNearLoad && rcore::StaleFight(active, riseNearLoad, s.msOk, s.ms28, g_state.load() == kIdle, TrainingOwnsFight())) {
            fight::ForceEnd("no-boss boss mode at a save load");
            g_staleBbm.store(true);
            wasActive = false;
            Log("Rematch: BossBattleMode began at a save load with no mission (0x%X) and no Nioh3Training session: no-boss arena mode - fight flag "
                "cleared (Boss Matches / Practice / summon allowed); 'Leave arena' ends the native boss mode", s.ms28);
        }
    }
    if (s.gm && !s.st[2]) g_posLiveOk = s.player && PlayerPos(g_posLive);
    else if (!s.gm) g_posLiveOk = false;
    SpawnTick(s, t, loadEnded);
    // leaving a mission during a boss fight (the menu's leave, quit to title) never runs BossBattleModeEnd: the "in a boss fight"
    // gate would stay set for the rest of the session (self-test 2026-10-09 21:14). A mission change ends any fight.
    {
        static uint32_t lastMission = 0;
        if (s.msOk && s.ms28 != lastMission) {
            if (fight::g_active && lastMission) {
                InterlockedExchange(&fight::g_active, 0);
                Log("Rematch: the mission changed (0x%X -> 0x%X) during a boss fight without BossBattleModeEnd - fight flag cleared", lastMission, s.ms28);
            }
            if (s.ms28) g_staleBbm.store(false);  // user 20261018: a mission runs: not the no-boss arena mode
            lastMission = s.ms28;
        }
    }
    if (s.queueOk && s.qHash != g_qHash) {
        if (g_qHash) g_qChangedAt.store(t);
        g_qHash = s.qHash;
    }
    if (g_wantScan.exchange(false)) DoScan(s);
    {   // user #6779: once the swapped-in boss is dead, point at the way back
        static int lastRet = 0;
        const int f = ReturnInfo();
        if (f > 0 && (f & 2) && !(lastRet & 2) && (f & 1)) ToastF(cGreen, 8000, "Boss defeated. The Return gate (or Active bosses > Return to where I was) takes you back.");
        lastRet = f > 0 ? f : 0;
    }
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
        if (kind == kReqExit || kind == kReqPracticeExit) {
            g_pend = 0;
            if (!g_exitOk) Refuse(g_exitWhy);
            else if (kind == kReqPracticeExit && PracticeReturnGate(s)) Refuse(PracticeReturnGate(s));
            else if (const char* why = ExitGate(s)) Refuse(why);
            else if (const char* attemptWhy = AttemptReturnGate()) Refuse(attemptWhy);
            else {
                AttemptReturning(s);
                const int rc = CallReturn();
                g_attempt.returnCompleted = rc == 0;
                if (rc) AttemptQuarantine("return call exception");
                Log("Rematch: exit: MissionRequestReturn(params, 1) from mission 0x%X (retry point %u) -> %s", s.ms28, s.ms14, rc ? "EXCEPTION" : "requested");
                Result(rc ? "exit failed" : "exit requested");
            }
        } else if (kind == kReqAbort) {
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
            if (kind == kReqDirect) ExecDirect(row, s, t, false, false);
            else if (kind == kReqFight) ExecDirect(row, s, t, true, true);
            else if (kind == kReqShrine) ExecDirect(row, s, t, true, false);
            else if (kind == kReqTeleport) { g_noSwap = true; ExecDirect(row, s, t, true, true); g_noSwap = false; }
            else if (kind == kReqPractice) ExecPractice(row, s, t);
            else if (kind == kReqEmaki) ExecEmaki(row, t);
            else if (kind == kReqScroll) ExecEmaki(-1, t);
            else ExecOpen(row, s);
        }
    }
    TrackReplay(s, t);
    TfTick(s, t);
    SelfTestTick(s, t);
    TrackEmaki(s, t);
    CoverTick(s,t);
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

std::wstring ArenaPath() { return Beside(L"Arenas", L"Nioh3BossRematch_arenas.tsv"); }

// BossBattleMode entry (game thread): the player position at every fight start; ArenaCapture = 1 appends it to the arenas file
void HkArena(uint64_t, uint64_t, uint64_t, uint64_t, const uint64_t*) {
    { CoverLock lock; if(lock.held){const Snap s=ReadSnap();if(s.msOk&&s.ms14==g_cover.start)g_cover.Entry(g_attempt.sequence,s.ms28,0,true);} }
    float p[4] = {}, cur[4] = {};
    const bool curOk = PlayerPos(cur);
    const bool beforeLoad = g_posAtLoadOk && Ms() - g_loadRiseAt < 5000;  // the fight began right after a load: the intro moved the player
    if (beforeLoad) memcpy(p, g_posAtLoad, sizeof(p));
    else memcpy(p, cur, sizeof(p));
    const bool ok = beforeLoad || curOk;
    const char* src = beforeLoad ? "before-load" : "fight-start";
    uint64_t g = 0, ms = 0;
    uint32_t mis = 0, rp = 0;
    if (Rd(G_MS, g) && g && Rd(g + 8, ms) && ms) {
        Rd(ms + 0x28, mis);
        Rd(ms + 0x14, rp);
    }
    char pl[256];
    fight::LastFightPlacements(pl, sizeof(pl));
    const int fr = g_fightRow;
    Log("Rematch: arena: BossBattleMode start - player now %.0f %.0f %.0f; %s %.0f %.0f %.0f (%s); mission 0x%X, retry point %u, placements created:%s%s%s",
        cur[0], cur[1], cur[2], beforeLoad ? "before the intro load" : "trigger point = now", p[0], p[1], p[2], src, mis, rp, pl[0] ? pl : " none",
        fr >= 0 ? " - Fight now row " : "", fr >= 0 ? g_rows[(size_t)fr].id : "");
    if (!g_cfg.arenaCapture || !ok || !mis) return;
    // 1.4.0: with the spawn override the player starts on the arena point itself, and a long intro load (Yamagata 2026-10-10: 6.5 s,
    // past the 5 s window) made the capture take the post-intro spot 334650 (outside BOSSAREABOX) as 'fight-start' - a capture
    // there would break the row. The point is known already: no capture while the override is on.
    if (g_sp.st == 2) {
        Log("Rematch: arena: not captured - the player spawned on the arena point (start point override)");
        return;
    }
    char ids[256] = "";
    size_t k = 0;
    for (const char* q = pl; *q && k + 12 < sizeof(ids);) {  // " 65092(-0.4s) ..." -> "65092 ..."
        while (*q == ' ') ++q;
        char* e = nullptr;
        const unsigned long v = strtoul(q, &e, 10);
        if (e == q) break;
        k += (size_t)snprintf(ids + k, sizeof(ids) - k, "%s%lu", k ? " " : "", v);
        q = e;
        while (*q && *q != ' ') ++q;
    }
    SYSTEMTIME st;
    GetLocalTime(&st);
    char line[400];
    const int n = snprintf(line, sizeof(line), "0x%04X\t%s\t%.0f\t%.0f\t%.0f\t%04u-%02u-%02u_%02u:%02u\t%s\r\n", mis, ids, p[0], p[1], p[2], st.wYear,
                           st.wMonth, st.wDay, st.wHour, st.wMinute, src);
    const std::wstring path = ArenaPath();
    const bool fresh = GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES;
    HANDLE h = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD w = 0;
    if (fresh) {
        static const char kHdr[] = "# Nioh3BossRematch arena captures ([Rematch] ArenaCapture = 1): the player position at BossBattleMode start. Read at launch; "
                                   "overrides the catalogue arena point of that mission's boss. src before-load = where the player stood when the boss intro load began "
                                   "(= at the trigger); fight-start = no load before the fight. Lines without src (1.2.0) are ignored.\r\n"
                                   "mission\tplacements\tx\ty\tz\twhen\tsrc\r\n";
        WriteFile(h, kHdr, (DWORD)strlen(kHdr), &w, nullptr);
    }
    WriteFile(h, line, (DWORD)n, &w, nullptr);
    CloseHandle(h);
    Log("Rematch: arena: captured -> %ls", path.c_str());
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
const char* CanShrine(int row) {
    if (const char* w = CanFight(row)) return w;
    if (g_rows[(size_t)row].atFight) return "no separate shrine: this mission starts at the fight";
    return nullptr;
}
const char* CanArena(int row) {
    if (const char* w = CanFight(row)) return w;
    const Row& r = g_rows[(size_t)row];
    if (r.atFight) return nullptr;
    if (!g_cfg.fightWarp) return "arena warp off ([Rematch] FightNowWarp = 1 turns it on)";
    if (!g_warpOk) return g_warpWhy;
    if (!r.arenaOk) return "no arena position yet (fight it once with [Rematch] ArenaCapture = 1)";
    return nullptr;
}

// ---- UI model (UI thread; snapshots / atomics from the game thread only) ----
int g_sel = 0, g_confirm = -9;  // g_sel -2 = top row 'Mod menu', -1 = 'Close' (#38077), >= 0 = the page's own rows
bool g_hostMenuReq = false;
enum UiPage { uRoot, uBosses, uActions, uSettings, uPractice, uSettingFirst };
#include "settings_catalog.h"
menu_nav::Trail<> g_uiPages;
int g_uiBoss = -1;
uint32_t g_pageGen0 = 0;
char g_msg[160] = "";
constexpr int kItOpen = -1, kItAbort = -2, kItScroll = -3;
constexpr int kItBrowse = -4, kItBack = -5, kItClose = -6;
constexpr int kItSettings = -7, kItSave = -8, kItGroupBase = -100, kItSettingBase = -1000;
constexpr int kItPractice = -10, kItPracticeStart = -11, kItPracticeReturn = -12;
// user #6618: summoned-boss death toggles at the top of the root page (Nioh3Training [Summon] keys, live + saved to its ini)
constexpr int kItDeathCut = -20, kItDeathBlow = -21, kItDeathLoot = -22;
// user #6779: 'Return to where I was' = Nioh3Training_ReturnHome (ends the arena session, back to the spot / world before Fight now; also mid-swap)
constexpr int kItReturnHome = -23;
// user #6789: the physical Return gate in the arena opens page uGate: Return to where I was / Fight again (Nioh3Training_GateFightAgain = queued round reset)
constexpr int kItFightAgain = -24;
// user round-2 test + #7118 (jumpfix 'Instant respawn on death', merged into this one row): AutoRetryOnDeath toggle (Nioh3Training [Summon]; 0 = no instant player+boss reset on defeat, the Return gate offers Fight again)
// user #13229/#13230: the row is no longer listed (always on, not toggleable); id kept so the old handlers stay inert
constexpr int kItAutoRetry = -25;
// user #7465: KiBreakNoFinisher (Nioh3Training [Summon]): no grapple finisher on a Ki-broken boss, gold Ki bar, HP-0 window
constexpr int kItNoFin = -26;
// user #9585: sliders for Nioh3Training [Summon] FinisherKillTimeoutMs (CutsceneOnKill finisher window) and KiBreakHp0WindowMs (HP-0 no-hit window).
// Live + saved through Nioh3Training_Get/SetTuning; the kill toggles themselves are not touched.
constexpr int kItFinT = -27, kItHp0W = -28;
constexpr int uGate = 900;
constexpr int uMoves = 901, kItMoves = -29, kItMoveBase = 0x30000;
constexpr int kItIdleHold = -30;
// bridge 12958: 'Repeat delay' slider on the move page (Nioh3Training_Get/SetRepeatGap, live + saved as [Training] RepeatGapMs)
constexpr int kItRepGap = -34;
// user #13293: 'Repeating move' row (A / Left stops). user #13618: 'Repeat selected move' row (A / Right repeats the picked move)
constexpr int kItRepNow = -51, kItRepSel = -52;
// user #13787: 'Boss aggro on me' (Nioh3Training_Get/SetBossAggro, saved as [Training] BossAggro) and 'Reset boss position' (Nioh3Training_ResetBossPos)
constexpr int kItAggro = -53, kItResetPos = -54;
// user #13786: 'Show twitches' on the move page. Catalogue rows of kind T (twitch: short state change, ends at once when forced), L (combo link /
// fragment) and M (movement) are hidden unless this is ON; a hidden-kind row still shows while it is OFF or repeating so its state is never invisible.
constexpr int kItShowTw = -55;
constexpr int kItCantDie = -56;  // user #14150: Player can't die (Nioh3Training_Get/SetPlayerCantDie, [Summon] PlayerCantDie)
// user #14267: player options on their own 'Player' page (Practice hub row + Return-object Training / Boss Match page row); more player rows later
constexpr int uPlayer = 909, kItPlayerOpen = -57;
// user #14291: Boss moves = drill rows + one row per move group (move_cats.h); A opens that group's page uMoveCat (its move rows, same
// per-row controls: A on / off, Right REPEAT, Left stop). B / Back returns to the group list.
constexpr int uMoveCat = 915, kItMoveCatBase = -70;  // kItMoveCatBase - group (-70 .. -90, #22349 hit groups; -94.. = boss vitals)
int g_moveCat = 0;
bool IsMoveCatItem(int item) { return item <= kItMoveCatBase && item > kItMoveCatBase - kMcAll; }  // + the Sequences row (#14317) and hit groups (#22349)
static_assert(kItMoveCatBase - kMcAll + 1 > -94, "move group rows must not reach the boss vitals rows");
// bridge 12666: SF6-style Training rows. kItSummonA/B + boss row = summon that boss as A / B next to you (no load, replaces that slot's boss);
// kItDespawnAll removes every summoned boss; kItMovesA/B open the move switches for the boss in slot A / B.
constexpr int kItDespawnAll = -31, kItMovesA = -32, kItMovesB = -33;
constexpr int kItSummonA = 0x50000, kItSummonB = 0x60000, kItSummonEnd = 0x70000;
// user #22984: kItSummonRoster + i = Nioh3Training roster boss i (summon-only rows: bosses with no single-boss catalogue row, e.g. Kajiwara Kagetoki)
constexpr int kItSummonRoster = 0x70000, kItSummonRosterEnd = 0x78000;
// user #13164: the root page holds exactly three submenus (+ Close): Boss Rematch (the rematch rows as before Training was merged in),
// Training (Training controls, then the arena list: A = teleport to that arena) and Summon Boss (boss list + passive-time slider + slot).
constexpr int uRematch = 902, uTraining = 903, uSummon = 904;
constexpr int kItRootRematch = -40, kItRootTraining = -41, kItRootSummon = -42;
constexpr int kItSumPassive = -43, kItSumSlot = -44;
int g_sumSlot = 0;  // Summon Boss page: slot (0 = A, 1 = B) the next boss row summons into
// user #13197: root = Settings / Boss Rematch / Practice (Training arenas, arena rows only) / Summon Boss. The in-fight controls (stance, moves,
// Boss A/B moves, repeat delay, despawn, leave session) live on their own 'Practice tools' page: a root row while a Training session or a
// summoned boss is live, and always a row on the Settings page.
constexpr int uTools = 905, kItRootTools = -45;
// user #13223: the host's first page is Settings / Boss Rematch / Practice. Practice page = Training here (boss summoned where you stand, no teleport,
// retry in place) / Arenas (arena rows only) / Boss summoning (summon list + passive time) / Practice tools.
constexpr int uPracticeHub = 906, uTrainHere = 907;
constexpr int kItTrainHereOpen = -46, kItArenas = -47, kItBossSumm = -48;
constexpr int kItTrainHere = 0x80000, kItTrainHereEnd = 0x90000;
// user 20261018: a save made inside a Fight-now arena loads IN the arena with no mission and a native BossBattleMode with no boss (no exit, no travel).
// 'Leave arena' = Nioh3Training_LeaveArena (ends the session + native boss mode, returns to the stored spot if any) + our fight flag cleared.
constexpr int kItLeaveArena = -49;
bool LeaveArenaOffered() { return (fight::g_active || g_staleBbm.load()) && !ReadSnap().ms28 && TrainingExport("Nioh3Training_LeaveArena"); }
int LeaveArena(const char* src) {
    using Fn = int (*)();
    const auto f = (Fn)TrainingExport("Nioh3Training_LeaveArena");
    const int rc = f ? f() : 0;
    if (rc > 0) { fight::ForceEnd(src); g_staleBbm.store(false); }
    Log("Rematch: leave arena (%s) -> %d", src, rc);
    return rc;
}  // kItTrainHere + row = start a Practice session with that boss right here
// user #13265: the boss-fight toggles (death / finisher options of the Boss Rematch page) on their own page, reachable from Practice, Practice tools
// and the Training Return gate. user #13262: the Return gate page adapts to the session mode (Nioh3Training_SessionMode).
constexpr int uFightOpts = 908, kItFightOpts = -50  /* -49 = kItLeaveArena */;
template <class Add> void AddBossFightItems(Add&& add) {  // shared by Boss Rematch, Boss fight settings and the Rematch Return gate
    if (TrainingExport("Nioh3Training_SetDeathCfg")) { add(kItDeathCut); add(kItDeathBlow); add(kItDeathLoot); }
    /* #13229: auto retry always on, row removed */
    if (TrainingExport("Nioh3Training_SetNoFinisher")) add(kItNoFin);
    if (TrainingExport("Nioh3Training_SetTuning")) { add(kItFinT); add(kItHp0W); }
}
bool BossFightItemsAvailable() {
    return TrainingExport("Nioh3Training_SetDeathCfg") || TrainingExport("Nioh3Training_SetNoFinisher") || TrainingExport("Nioh3Training_SetTuning");
}
int GateMode() {  // 0 = unknown / no session (old 2-option page), 1 = Boss Rematch fight, 2 = Training
    using Fn = int (*)();
    const auto f = (Fn)TrainingExport("Nioh3Training_SessionMode");
    return f ? f() : 0;
}
void SetPracticeNext(int on) {
    using Fn = void (*)(int);
    if (const auto f = (Fn)TrainingExport("Nioh3Training_SetPracticeNext")) f(on);
}
uint32_t SlotChr(int slot);
uint32_t MoveBoss();
bool PracticeSessionLive() {
    using Fn = int (*)();
    if (const auto f = (Fn)TrainingExport("Nioh3Training_SessionActive")) return f() != 0;
    return MoveBoss() != 0 || SlotChr(0) != 0 || SlotChr(1) != 0;  // older Nioh3Training.asi without the export
}
bool PracticeToolsAvailable() {
    return TrainingExport("Nioh3Training_SetIdleHold") || TrainingExport("Nioh3Training_SetMoveAllowed") || TrainingExport("Nioh3Training_SummonSlot") ||
           TrainingExport("Nioh3Training_SetRepeatGap");
}
int SumPassiveGet() {
    using G = int (*)();
    const auto g = (G)TrainingExport("Nioh3Training_GetSummonPassive");
    return g ? g() : -1;
}
void SumPassiveStep(int dir) {  // dir > 0 longer, < 0 shorter, 0 (A) = longer, wrapping to 0 after the maximum
    using S = int (*)(int, int);
    const auto set = (S)TrainingExport("Nioh3Training_SetSummonPassive");
    int v = SumPassiveGet();
    if (!set || v < 0) { snprintf(g_msg, sizeof(g_msg), "Needs a Nioh3Training.asi with the wait-before-attacking slider."); return; }
    const int step = 1000, hi = 30000;
    v = (v / step) * step;
    v = dir < 0 ? v - step : v + step;
    if (dir == 0 && v > hi) v = 0;
    v = v < 0 ? 0 : v > hi ? hi : v;
    v = set(v, 1);
    Log("Rematch: menu slider SummonPassiveMs -> %d ms", v);
    g_msg[0] = 0;
}
uint32_t SlotChr(int slot) {
    using Fn = uint32_t (*)(int);
    const auto f = (Fn)TrainingExport("Nioh3Training_SlotChr");
    return f ? f(slot) : 0;
}
const char* SlotBossName(uint32_t chr) {
    static char bufs[2][40];  // two names in one printf (Remove all summoned bosses (A: x, B: y))
    static int nb;
    char* buf = bufs[nb++ & 1];
    if (!chr) return "empty";
    for (int i = 0; i < N3_MOVE_COUNT; ++i) if (kN3Moves[i].chr == chr) return kN3Moves[i].boss;
    using NameFn = int (*)(uint32_t, char*, int);  // ally #89: bosses with no recorded move list still show their name
    if (const auto f = (NameFn)TrainingExport("Nioh3Training_BossName")) if (f(chr, buf, (int)sizeof bufs[0]) && buf[0]) return buf;
    snprintf(buf, sizeof bufs[0], "boss %u", chr);
    return buf;
}
void SetMoveSlot(int slot) {
    using Fn = void (*)(int);
    if (const auto f = (Fn)TrainingExport("Nioh3Training_SetMoveSlot")) f(slot);
}
int IdleHoldStatus() {
    using Fn = int (*)();
    const auto f = (Fn)TrainingExport("Nioh3Training_GetIdleHold");
    return f ? f() : -1;
}
int DrillState() {  // user #13529: 0 none, 1 drill (boss stands still), 2 drill with the AI on (fallback)
    using Fn = int (*)();
    const auto f = (Fn)TrainingExport("Nioh3Training_GetDrill");
    return f ? f() : 0;
}
const N3Move* RepeatingMove();
void RepeatStop(const char* src);
void IdleHoldToggle(int dir) {
    using Fn = int (*)(int);
    const auto f = (Fn)TrainingExport("Nioh3Training_SetIdleHold");
    const int on = dir ? dir > 0 : IdleHoldStatus() <= 0;
    const int ok = f ? f(on) : 0;
    // user #13529: plain stance words (fighting-game training): 'Stand still (idle)' / 'Fight normally'
    snprintf(g_msg, sizeof g_msg, "%s", !ok ? "Start a Training session with a live boss first." :
        on ? "Stand still: the boss stops at its next safe moment. A repeating move still plays every gap (drill)." : "Fight normally: the boss attacks with its own AI again.");
}
uint32_t g_uiMoveChr = 0;
int g_bossSlot = -1;  // user #14309: the Boss X page being shown (A = 0); its moves, repeat and settings are that boss's
// pc2 #23907: the boss's move list from the game's own action data (Nioh3Training reads it when the boss spawns): merged into the live catalogue
// (move_catalog.h) the first time its page is built after Training has it. Hand rows stay only for bosses Training has not read.
static const int g_n3catBoot = (N3CatInit(1), 1);
void AutoMovesSync(uint32_t chr) {
    if (!chr || N3CatIsAuto(chr)) return;
    static DWORD last; const DWORD now = GetTickCount();
    if (now - last < 500) return;
    last = now;
    using Fn = int (*)(uint32_t, void*, int, int);
    const auto f = (Fn)TrainingExport("Nioh3Training_AutoMoves");
    if (!f) return;
    static AmMove buf[AM_MAXMOVES];
    const int n = f(chr, buf, AM_MAXMOVES, (int)sizeof(AmMove));
    if (n > 0) { const int add = N3CatMerge(chr, buf, n, SlotBossName(chr)); Log("Rematch: AUTOMOVES chr %u: Boss moves page from game data (%d moves, %d new rows)", chr, n, add); }
    else if (n < 0) Log("Rematch: AUTOMOVES chr %u: Nioh3Training.asi move data layout differs (update both mods)", chr);
}
uint32_t MoveBoss() {
    uint32_t chr = 0;
    if (g_bossSlot >= 0) chr = SlotChr(g_bossSlot);
    else { using Fn = uint32_t (*)(); const auto f = (Fn)TrainingExport("Nioh3Training_GetMoveBoss"); chr = f ? f() : 0; }
    AutoMovesSync(chr);
    return chr;
}
int MoveAllowed(const N3Move& move) {
    using Fn = int (*)(uint32_t, uint32_t);
    const auto f = (Fn)TrainingExport("Nioh3Training_GetMoveAllowed");
    return f ? f(move.chr, move.id) : -1;
}
bool MoveRepeating(const N3Move& move) {
    using Fn = uint32_t (*)(uint32_t);
    const auto f = (Fn)TrainingExport("Nioh3Training_GetMoveRepeat");
    if (!f) return false;
    const uint32_t rep = f(move.chr);
    return rep && rep == move.id;  // 0 = no repeat (move 0x0000 must not show RPT)
}
// user #14423: 1 = this boss repeats the whole sequence (not just the move); older Training builds have no flag (= single move)
bool RepeatIsSeq(uint32_t chr) { using Fn = int (*)(uint32_t); const auto f = (Fn)TrainingExport("Nioh3Training_GetMoveRepeatSeq"); return f && f(chr) == 1; }
bool IsMoveItem(int item) { return item >= kItMoveBase && item < kItMoveBase + N3_MOVE_COUNT; }
// user #13618: fighting-game repeat controls. On a move row A = ON / OFF, Right = ALWAYS 'repeat this move' (an OFF move is switched ON
// first), Left = stop the repeat. 'Repeat selected move' (top row) repeats the last move the user pressed a button on, else the first ON
// move. Every arm / stop / failure shows a 2 s toast with the reason and writes a 'Rematch: REPEAT ...' log line (before #13618 the only
// feedback was the footer, and Right on an OFF move merely switched it ON, so the user never armed a repeat: log had MOVEMENU lines only).
int g_repPick = -1;  // kN3Moves index of the last move row the user pressed A / Left / Right on
int RepGapGet();
// 'Boss does 0x4E30 every 1.25 s, stands still between' (Stand still) / 'Boss uses 0x4E30 each time it attacks (gap 1.25 s)' (Fight normally)
int BossGapGet(); int SlotStance(int s);
void RepeatWhat(const N3Move& m, char* b, size_t n) {
    const int v = g_bossSlot >= 0 ? BossGapGet() : RepGapGet(), st = g_bossSlot >= 0 ? SlotStance(g_bossSlot) : -9;
    const int hold = st == -9 ? IdleHoldStatus() : st >= 2 ? 2 : st == 1 ? 1 : 0;
    char gap[32];
    if (v < 0) snprintf(gap, sizeof gap, "?");
    else if (v == 0) snprintf(gap, sizeof gap, "0 s (back to back)");  // 22349: "every 0.00 s" read as a bug
    else snprintf(gap, sizeof gap, "%d.%02d s", v / 1000, (v % 1000) / 10);
    if (hold == 2) snprintf(b, n, "Boss does 0x%04X every %s, stands still between", m.id, gap);
    else if (hold == 1) snprintf(b, n, "Boss does 0x%04X every %s once it stands still", m.id, gap);
    else snprintf(b, n, "Boss uses 0x%04X each time it attacks (gap %s)", m.id, gap);
}
// human #44361: the right-column type of a move row: Attack / Grab / Unblockable / Wind-up / Link / Twitch / Movement / Idle
void MoveTypeTag(const N3Move& m, char* b, size_t n) {
    const int c = N3MoveCat(&m);
    if (c == kMcGrab) { snprintf(b, n, "Grab"); return; }
    if (c == kMcGreat) { snprintf(b, n, "Unblockable"); return; }  // its hit count is in the footer (the row stays one line)
    if (m.kind == 'S') { snprintf(b, n, "Wind-up"); return; }
    if (m.kind == 'L') { snprintf(b, n, "Link"); return; }
    if (N3MoveHitCat(c) || c == kMcHitUnknown || c == kMcAttack || c == kMcUnnamedAtk) { snprintf(b, n, "Attack"); return; }
    snprintf(b, n, "%s", m.kind == 'T' ? "Twitch" : m.kind == 'M' ? "Movement" : m.kind == 'I' ? "Idle" : c == kMcKiBreak ? "Ki break" : "Other");
}
const char* MoveKindName(char k) {  // user #13786: N3Move.kind from tools/rematch/actlog_moves.py
    return k == 'A' ? "attack" : k == 'S' ? "wind-up" : k == 'L' ? "link" : k == 'T' ? "twitch" : k == 'M' ? "movement" : k == 'I' ? "idle" : "unsorted";
}
const char* MoveName(const N3Move& m) {
    if (N3MoveProtected(m.id)) return "Idle / initialization";
    if (const char* ki = N3KiNote(m.chr, m.id)) return ki;  // user #14291: live-logged Ki-break ids get a plain name
    if (*m.label) return m.label;
    return m.kind == 'A' ? "Unlabelled attack" : m.kind == 'S' ? "Unlabelled wind-up" : m.kind == 'L' ? "Combo link (fragment)" : m.kind == 'T' ? "Twitch (state change)" :
           m.kind == 'M' ? "Step / turn" : "Unlabelled action";
}
bool g_showTwitch = false;  // user #13786: session only, default hidden
bool MoveListed(const N3Move& m) { return g_showTwitch || N3MoveProtected(m.id) || !N3MoveTwitch(&m) || MoveAllowed(m) == 0 || MoveRepeating(m); }
// user #14291: moves of boss chr in group c; off = how many are OFF, rpt = the repeating move if it is in this group
int MoveCatCount(uint32_t chr, int c, int* off = nullptr, const N3Move** rpt = nullptr) {
    int n = 0;
    if (off) *off = 0;
    if (rpt) *rpt = nullptr;
    for (int i = 0; i < N3_MOVE_COUNT; ++i) {
        const N3Move& m = kN3Moves[i];
        if (m.chr != chr || N3MoveCat(&m) != c) continue;
        ++n;
        if (off && MoveAllowed(m) == 0) ++*off;
        if (rpt && MoveRepeating(m)) *rpt = &m;
    }
    return n;
}
const N3Move* RepeatCandidate() {
    const uint32_t chr = MoveBoss();
    if (!chr) return nullptr;
    if (g_repPick >= 0 && g_repPick < N3_MOVE_COUNT && kN3Moves[g_repPick].chr == chr && !N3MoveProtected(kN3Moves[g_repPick].id)) return &kN3Moves[g_repPick];
    for (int i = 0; i < N3_MOVE_COUNT; ++i)
        if (kN3Moves[i].chr == chr && !N3MoveProtected(kN3Moves[i].id) && (g_showTwitch || !N3MoveTwitch(&kN3Moves[i])) && MoveAllowed(kN3Moves[i]) > 0) return &kN3Moves[i];
    return nullptr;
}
void RepeatFail(const char* src, const N3Move* m, const char* why) {
    Log("Rematch: REPEAT arm FAILED (%s) chr %u 0x%X: %s", src, m ? m->chr : 0u, m ? m->id : 0u, why);
    snprintf(g_msg, sizeof g_msg, "Repeat failed: %s", why);
    ToastF(cRed, 2000, "Repeat failed: %s", why);
}
// user #14423: seq = repeat the whole sequence that starts with m (Training follows the chain: the game's links play, then the gap)
bool RepeatArm(const N3Move* m, const char* src, bool seq = false) {
    using RepFn = int (*)(uint32_t, uint32_t, int);
    using SetFn = int (*)(uint32_t, uint32_t, int, int);
    const auto rep = (RepFn)TrainingExport(seq ? "Nioh3Training_SetMoveRepeatSeq" : "Nioh3Training_SetMoveRepeat");
    if (!m) { RepeatFail(src, m, "no move to repeat. Open a move group and press A on a move."); return false; }
    if (m->chr != MoveBoss()) { RepeatFail(src, m, "the boss changed; reopen its page."); return false; }
    if (N3MoveProtected(m->id)) { RepeatFail(src, m, "idle / initialization cannot be repeated. Pick an attack."); return false; }
    if (!rep || !TrainingExport("Nioh3Training_GetMoveRepeat")) { RepeatFail(src, m, seq ? "this Nioh3Training.asi has no sequence repeat (update it)." : "this Nioh3Training.asi has no repeat (update it)."); return false; }
    const int old = MoveAllowed(*m);
    bool switchedOn = false;
    if (old == 0) {  // repeat on an OFF move: switch it ON first, then repeat it
        const auto set = (SetFn)TrainingExport("Nioh3Training_SetMoveAllowed");
        const int rc = set ? set(m->chr, m->id, 1, 1) : 0;
        if (rc == 0) { RepeatFail(src, m, "could not switch the move ON."); return false; }
        switchedOn = true;
    }
    const int rr = rep(m->chr, m->id, 1);
    if (!rr || !MoveRepeating(*m)) {
        RepeatFail(src, m, !rr ? "the Training mod refused this move (see Nioh3Training.log)." : "the Training mod did not keep the repeat.");
        return false;
    }
    char what[96];
    RepeatWhat(*m, what, sizeof what);
    using StatusFn = int (*)();
    const auto hs = (StatusFn)TrainingExport("Nioh3Training_GetMoveHookStatus");
    const int hook = hs ? hs() : 0, hold = IdleHoldStatus();
    const bool noHook = hook < 0 && hold != 2;  // Fight normally needs the select hook; Stand still forces the move itself (drill)
    Log("Rematch: REPEAT arm (%s) chr %u 0x%X%s %s%s -> ON, stance %d, gap %d ms, hooks %d: %s", src, m->chr, m->id, seq ? " sequence" : "", MoveName(*m),
        switchedOn ? " (was OFF, switched ON)" : "", hold, RepGapGet(), hook, what);
    snprintf(g_msg, sizeof g_msg, "Repeat ON 0x%04X: %s.%s A again stops.", m->id, what, switchedOn ? " (Move switched ON.)" : "");
    if (noHook) ToastF(cOrange, 2000, "Repeat 0x%04X set, but move hooks failed: only works in Stand still. %s", m->id, what);
    else ToastF(cGreen, 2000, "Repeating 0x%04X: %s%s", m->id, what, switchedOn ? " (switched ON)" : "");
    return true;
}
void MoveToggle(int item, int dir) {
    if (!IsMoveItem(item)) return;
    const auto& move = kN3Moves[item - kItMoveBase];
    if (!N3MoveProtected(move.id)) g_repPick = item - kItMoveBase;
    if (move.chr != MoveBoss()) { snprintf(g_msg, sizeof g_msg, "The boss changed; reopen its page."); return; }
    // user #13623 / #14336 item 4: A = 'Repeat this move' ON / OFF (one repeat per boss: ON here moves it off any other move).
    // Left / Right = may the boss use this move on its own (OFF = never). No hidden key tricks.
    if (dir == 0) { if (MoveRepeating(move) && !RepeatIsSeq(move.chr)) RepeatStop("A on move"); else RepeatArm(&move, "A on move"); return; }  // #14423: A on the head of a repeating sequence = repeat just this move
    if (N3MoveProtected(move.id)) { snprintf(g_msg, sizeof g_msg, "Idle and initialization stay ON so the boss can resume after a skipped move."); return; }
    using Fn = int (*)(uint32_t, uint32_t, int, int);
    const auto f = (Fn)TrainingExport("Nioh3Training_SetMoveAllowed");
    const int old = MoveAllowed(move);
    if (!f || old < 0) { snprintf(g_msg, sizeof g_msg, "Move controls unavailable in this Training build."); return; }
    const int on = dir > 0;
    if (on == old) { snprintf(g_msg, sizeof g_msg, "%s", on ? "Already allowed. Left blocks this move." : "Already blocked. Right allows this move."); return; }
    const bool repeating = MoveRepeating(move);
    const int rc = f(move.chr, move.id, on, 1);
    if (!on && repeating) {  // an OFF move must not keep repeating
        using RepFn = int (*)(uint32_t, uint32_t, int);
        if (const auto rep = (RepFn)TrainingExport("Nioh3Training_SetMoveRepeat")) rep(move.chr, move.id, 0);
        Log("Rematch: REPEAT stop (A turned 0x%X OFF) chr %u", move.id, move.chr);
        ToastF(cYellow, 2000, "0x%04X OFF: repeat stopped, the boss picks its own moves again.", move.id);
    }
    snprintf(g_msg, sizeof g_msg, "%s", rc == 1 ? (on ? "Allow ON (saved): the boss may choose this move. A toggles repeat." : "Allow OFF (saved): block the boss from choosing this move.") :
        rc == -1 ? "Changed this session, but saving failed. Retry to save." : "This action is required by the idle fallback and stays ON.");
}
bool DeathCfgGet(int& loot, int& blow, int& cut) {
    using Fn = void (*)(int*, int*, int*);
    const auto f = (Fn)TrainingExport("Nioh3Training_GetDeathCfg");
    if (!f) return false;
    f(&loot, &blow, &cut);
    return true;
}
bool DeathCfgToggle(int item, int dir) {  // dir 0 flips, > 0 on, < 0 off
    using Fn = void (*)(int, int, int, int);
    const auto set = (Fn)TrainingExport("Nioh3Training_SetDeathCfg");
    int loot = 0, blow = 1, cut = 0;
    if (!set || !DeathCfgGet(loot, blow, cut)) { snprintf(g_msg, sizeof(g_msg), "Needs a Nioh3Training.asi with the kill rules."); return false; }
    int& v = item == kItDeathCut ? cut : item == kItDeathBlow ? blow : loot;
    v = dir ? (dir > 0) : !v;
    set(item == kItDeathLoot ? loot : -1, item == kItDeathBlow ? blow : -1, item == kItDeathCut ? cut : -1, 1);
    Log("Rematch: menu death toggle -> CutsceneOnKill %d DeathBlowRequired %d BossLoot %d", cut, blow, loot);
    g_msg[0] = 0;
    return true;
}
void AutoRetryToggle(int dir) {
    using G = int (*)();
    using S = void (*)(int, int);
    const auto g = (G)TrainingExport("Nioh3Training_GetAutoRetry");
    const auto s = (S)TrainingExport("Nioh3Training_SetAutoRetry");
    if (!g || !s) return;
    const int v = dir ? (dir > 0) : !g();
    s(v, 1);
    Log("Rematch: menu AutoRetryOnDeath -> %d", v);
    g_msg[0] = 0;
}
int TuneGet(int which) {
    using G = int (*)(int);
    const auto g = (G)TrainingExport("Nioh3Training_GetTuning");
    return g ? g(which) : -1;
}
void TuneStep(int item, int dir) {  // dir > 0 longer, < 0 shorter, 0 (A) = longer, wrapping to the minimum after the maximum
    using S = int (*)(int, int, int);
    const auto set = (S)TrainingExport("Nioh3Training_SetTuning");
    const int which = item == kItFinT ? 0 : 1, step = which == 0 ? 500 : 100, lo = which == 0 ? 500 : 200, hi = which == 0 ? 30000 : 5000;
    int v = TuneGet(which);
    if (!set || v < 0) { snprintf(g_msg, sizeof(g_msg), "Needs a Nioh3Training.asi with the kill / finisher sliders."); return; }
    v = dir < 0 ? v - step : v + step;
    if (dir == 0 && v > hi) v = lo;
    v = v < lo ? lo : v > hi ? hi : v;
    v = set(which, v, 1);
    Log("Rematch: menu slider %s -> %d ms", which == 0 ? "FinisherKillTimeoutMs" : "KiBreakHp0WindowMs", v);
    g_msg[0] = 0;
}
int AggroGet() {
    using G = int (*)();
    const auto g = (G)TrainingExport("Nioh3Training_GetBossAggro");
    return g ? g() : -1;
}
void AggroToggle(int dir) {  // user #13787: dir 0 flips, > 0 on, < 0 off
    using S = int (*)(int, int);
    const auto set = (S)TrainingExport("Nioh3Training_SetBossAggro");
    const int v = AggroGet();
    if (!set || v < 0) { snprintf(g_msg, sizeof g_msg, "Needs a Nioh3Training.asi with 'Boss aggro on me'."); return; }
    const int on = set(dir ? dir > 0 : !v, 1);
    Log("Rematch: menu Boss aggro on me -> %d", on);
    snprintf(g_msg, sizeof g_msg, "%s", on ? "Boss aggro ON: the boss keeps you as its target and walks up to you before its repeating move (Fight normally or Stand still). Saved."
                                         : "Boss aggro OFF: the boss behaves as before. Saved.");
}
int CantDieGet() {
    using G = int (*)();
    const auto g = (G)TrainingExport("Nioh3Training_GetPlayerCantDie");
    return g ? g() : -1;
}
void CantDieToggle(int dir) {  // user #14150: dir 0 flips, > 0 on, < 0 off
    using S = int (*)(int, int);
    const auto set = (S)TrainingExport("Nioh3Training_SetPlayerCantDie");
    const int v = CantDieGet();
    if (!set || v < 0) { snprintf(g_msg, sizeof g_msg, "Needs a Nioh3Training.asi with 'Player can't die'."); return; }
    const int on = set(dir ? dir > 0 : !v, 1);
    Log("Rematch: menu Player can't die -> %d", on);
    snprintf(g_msg, sizeof g_msg, "%s", on ? "Infinite health ON: refill your HP during Training and Boss Matches. Saved."
                                         : "Infinite health OFF: normal damage. Training and arena Boss Matches retry in place after defeat. Saved.");
}
void ResetBossPos() {  // user #13787
    using F = int (*)();
    const auto f = (F)TrainingExport("Nioh3Training_ResetBossPos");
    const int rc = f ? f() : 0;
    Log("Rematch: menu Reset boss position -> %d", rc);
    snprintf(g_msg, sizeof g_msg, "%s", !f ? "Needs a Nioh3Training.asi with 'Reset boss position'." : rc ? "The boss goes back to where it first appeared, facing you." : "No Training boss to put back (start a Training session or summon a boss first).");
    if (rc) ToastF(cGreen, 2000, "Boss put back at its start position.");
}
int RepGapGet() {
    using G = int (*)();
    const auto g = (G)TrainingExport("Nioh3Training_GetRepeatGap");
    return g ? g() : -1;
}
void RepGapStep(int dir) {  // dir > 0 longer, < 0 shorter, 0 (A) = longer, wrapping to 0 after the maximum
    using S = int (*)(int, int);
    const auto set = (S)TrainingExport("Nioh3Training_SetRepeatGap");
    int v = RepGapGet();
    if (!set || v < 0) { snprintf(g_msg, sizeof(g_msg), "Needs a Nioh3Training.asi with the repeat gap slider."); return; }
    const int step = 250, hi = 10000;
    v = dir < 0 ? v - step : v + step;
    if (dir == 0 && v > hi) v = 0;
    v = v < 0 ? 0 : v > hi ? hi : v;
    v = set(v, 1);
    Log("Rematch: menu slider RepeatGapMs -> %d ms", v);
    g_msg[0] = 0;
}
const N3Move* RepeatingMove() {
    const uint32_t chr = MoveBoss();
    for (int i = 0; chr && i < N3_MOVE_COUNT; ++i) if (kN3Moves[i].chr == chr && MoveRepeating(kN3Moves[i])) return &kN3Moves[i];
    return nullptr;
}
void RepeatStop(const char* src) {
    using RepFn = int (*)(uint32_t, uint32_t, int);
    const auto rep = (RepFn)TrainingExport("Nioh3Training_SetMoveRepeat");
    const N3Move* m = RepeatingMove();
    if (!rep || !m) {
        Log("Rematch: REPEAT stop (%s): nothing was repeating (boss chr %u)", src, MoveBoss());
        snprintf(g_msg, sizeof g_msg, "No move is repeating. A on a move starts one.");
        ToastF(cGray, 2000, "No move is repeating. A on a move starts one.");
        return;
    }
    rep(m->chr, m->id, 0);
    Log("Rematch: REPEAT stop (%s) chr %u 0x%X -> %s", src, m->chr, m->id, RepeatingMove() ? "STILL repeating" : "stopped");
    snprintf(g_msg, sizeof g_msg, "Repeat of 0x%04X stopped; the boss picks its own moves again.", m->id);
    ToastF(cYellow, 2000, "Repeat stopped (0x%04X): the boss picks its own moves again.", m->id);
}
void NoFinToggle(int dir) {
    using G = int (*)();
    using S = void (*)(int, int);
    const auto g = (G)TrainingExport("Nioh3Training_GetNoFinisher");
    const auto s = (S)TrainingExport("Nioh3Training_SetNoFinisher");
    if (!g || !s) return;
    const int v = dir ? (dir > 0) : !g();
    s(v, 1);
    Log("Rematch: menu KiBreakNoFinisher -> %d", v);
    g_msg[0] = 0;
}
#include "boss_vitals_menu.h"  // user #14320: per-boss HP / Ki page (uBossVitals)
#include "practice_stats_menu.h"  // user #22389: player infinite Ki, damage multipliers, boss Ki drain
#include "training_bosses_menu.h"  // user #14309 / #14313 / #14317 / #14336: Training Bosses, Boss X pages, timer, Sequences
constexpr int kItFight = 0x10000;  // kItFight + row = "Fight now (in the arena)" under that boss row
constexpr int kItTeleport = 0x40000;  // kItTeleport + row = "Teleport to arena (fast load)": native mission load + arena warp, no live swap
constexpr int kItShrine = 0x20000;  // kItShrine + row = "Shrine before the boss" (the start point alone)

int Items(int* out, int cap) {
    int n = 0;
    auto add = [&](int item) { if (n < cap) out[n++] = item; };
    if (g_uiPages.page == uRoot) {  // user #13197: Settings / Boss Rematch / Practice / Summon Boss (+ Practice tools while a session is live)
        add(kItSettings);  // user #13223: Settings / Boss Rematch / Practice (Summon Boss and the tools live inside Practice)
        add(kItRootRematch);
        add(kItRootTraining);
        add(kItClose);
        return n;
    }
    if (g_uiPages.page == uPracticeHub) {  // user #13223 / #13293: Train here / arena / Training Bosses / Summon a boss here / Boss Matches
        if (LeaveArenaOffered()) add(kItLeaveArena);  // user 20261018: the hosted menu opens here, never on uRoot (the row was unreachable)
        const bool inArena = InTrainingArena();  // user #14333 / #14336 item 8: no 'Train here' / 'Train in a boss arena' while already in one
        if (!inArena && TrainingExport("Nioh3Training_TrainHere")) add(kItTrainHereOpen);
        if (!inArena) add(kItArenas);
        if (TrainingExport("Nioh3Training_SetPlayerCantDie")) add(kItPlayerOpen);  // user #14267: Player options
        if (PracticeToolsAvailable()) add(kItRootTools);  // Training Bosses
        PstAddHubRows(add);  // Time of day / Weather / Freeze / Reset fight
        if (TrainingExport("Nioh3Training_SummonSlot")) add(kItBossSumm);
        add(kItRootRematch);  // user #13293: Boss Matches (its kill / finisher rules apply to Training and summoned bosses too)
        add(kItBack);
        return n;
    }
    if (g_uiPages.page == uPlayer) {  // user #14267: player options (infinite health for now)
        if (TrainingExport("Nioh3Training_SetPlayerCantDie")) add(kItCantDie);
        PstAddPlayerRows(add);  // user #22389
        add(kItBack);
        return n;
    }
    if (g_uiPages.page == uBossVitals) { BvAddPage(add); PstAddBossRows(add); add(kItBack); return n; }  // user #14320: Boss A / B health and Ki
    if (g_uiPages.page == uFightOpts) {  // user #13265: the Boss Rematch boss-fight toggles, also for Training
        AddBossFightItems(add);
        add(kItBack);
        return n;
    }
    if (g_uiPages.page == uTrainHere) {  // user #13223: every boss with a mission id, beaten first
        for (int pass = 0; pass < 2; ++pass)
            for (size_t i = 0; i < g_rows.size() && n < cap; ++i)
                if (g_rows[i].missionId && (g_status[i].load() == 2 ? 0 : 1) == pass) add(kItTrainHere + (int)i);
        add(kItBack);
        return n;
    }
    if (g_uiPages.page == uTools) {  // Training Bosses (#14301): one row per spawned boss (#14309), summon another, timed CPU / idle switch (#14313)
        for (int s = 0; s < SlotCountUi(); ++s) if (SlotChr(s)) add(kItBossOpenBase + s);
        if (TrainingExport("Nioh3Training_SummonSlot") && FreeSlot() >= 0) add(kItSumAnother);
        if (TrainingExport("Nioh3Training_StanceTimer") && SpawnedCount()) add(kItStanceTimer);
        if (TrainingExport("Nioh3Training_DespawnAll") && SpawnedCount()) add(kItDespawnAll);  // #14336 item 6: only with a boss to remove
        if (TrainingExport("Nioh3Training_ReturnHome")) add(kItReturnHome);
        add(kItBack);
        return n;
    }
    if (g_uiPages.page == uBoss) {  // user #14336 item 2: every per-boss setting lives here, once
        const uint32_t chr = SlotChr(g_bossSlot);
        if (g_uiMoveChr != chr) { g_uiMoveChr = chr; }
        add(kItBossStance);
        if (TrainingExport("Nioh3Training_SetSlotAggro")) add(kItBossAggro);
        if (TrainingExport("Nioh3Training_SetRepeatGapChr")) add(kItBossGap);
        if (TrainingExport("Nioh3Training_ResetSlotPos")) add(kItBossReset);
        if (BvAvailable()) add(kItBvOpenBase + g_bossSlot);
        if (TrainingExport("Nioh3Training_SetMoveAllowed")) {
            for (int c : kMcOrder) if (MoveCatCount(chr, c)) add(kItMoveCatBase - c);  // user #22349: non-empty groups, attacks by hits first
            if (SeqCount(chr)) add(kItMoveCatBase - kMcSeq);  // user #14317
        }
        if (TrainingExport("Nioh3Training_DespawnSlot")) add(kItBossRemove);
        add(kItBack);
        return n;
    }
    if (g_uiPages.page == uTraining) {  // user #13197: arena rows only (details in the footer)
        // the arenas: every boss with a start point next to it (beaten first); A = Teleport to arena (fast load)
        if (g_live.load())
            for (int pass = 0; pass < 2; ++pass)
                for (size_t i = 0; i < g_rows.size() && n < cap; ++i) {
                    const Row& r = g_rows[i];
                    if (r.route == rcore::kX || !r.fightStart) continue;
                    if ((g_status[i].load() == 2 ? 0 : 1) != pass) continue;
                    add(kItTeleport + (int)i);
                }
        add(kItBack);
        return n;
    }
    if (g_uiPages.page == uSummon) {  // user #14336 item 7: 'Summon another boss' = boss list, the boss takes the next free letter (no slot picker)
        if (TrainingExport("Nioh3Training_SummonSlot")) {
            // user #22984: only catalogue rows Training resolves to one boss (multi-boss rows: their bosses are roster rows), then the roster
            using CanFn = int (*)(uint32_t);
            const auto can = (CanFn)TrainingExport("Nioh3Training_CanSummon");
            for (size_t i = 0; i < g_rows.size() && n < cap; ++i)
                if (g_rows[i].missionId && (!can || can(g_rows[i].missionId))) add(kItSummonA + (int)i);
            const int nr = RosterCount();
            for (int i = 0; i < nr && i < kItSummonRosterEnd - kItSummonRoster && n < cap; ++i) add(kItSummonRoster + i);
        }
        add(kItBack);
        return n;
    }
    if (g_uiPages.page == uRematch) {  // the Boss Rematch menu as it was before Training was merged in
        AddBossFightItems(add);
        // user #13197: no 'Return to where I was' here (Practice tools / the arena Return gate have it)
        // user #6718: the boss list stays on the root page (beaten / startable first, then the rest)
        if (g_live.load())
            for (int pass = 0; pass < 3; ++pass)
                for (size_t i = 0; i < g_rows.size() && n < cap; ++i) {
                    const Row& r = g_rows[i];
                    const uint8_t st = g_status[i].load();
                    const int grp = r.route == rcore::kX ? 2 : (r.route == rcore::kE || st == 2) ? 0 : 1;
                    if (grp != pass) continue;
                    if (grp == 1 && !g_cfg.showAll) continue;
                    if (grp == 2 && !g_cfg.showAll) continue;
                    if (r.route == rcore::kH && !g_cfg.hiddenDuels && !g_cfg.directStart && !g_cfg.showAll) continue;
                    add((int)i);
                }
        // #14336 item 10: legacy / developer rows only with [Rematch] ShowUnverified = 1
        if (g_cfg.showUnverified) {
            if (g_live.load()) add(kItPractice);
            if (g_raOk) add(kItOpen);
            if (g_emakiOk) add(kItScroll);
        }
        if (g_state.load() != kIdle && !g_replayIn.load() && g_et.st != rcore::kEIn) add(kItAbort);
        add(kItBack);
        return n;
    }
    if (g_uiPages.page == uMoves) {
        const uint32_t chr = MoveBoss();
        if (g_uiMoveChr != chr) { g_uiMoveChr = chr; g_sel = 0; g_msg[0] = 0; }
        // #14336 item 2: stance / gap / aggro / reset / repeat rows live on the Boss X page only. user #14291: one row per move group (non-empty groups only) instead of the flat list + 'Show twitches' filter (#13786)
        for (int c : kMcOrder) if (MoveCatCount(chr, c)) add(kItMoveCatBase - c);  // user #22349
        add(kItBack);
        return n;
    }
    if (g_uiPages.page == uMoveCat) {  // user #14291: one move group of the current boss, every move of it (no filter)
        const uint32_t chr = MoveBoss();
        if (g_uiMoveChr != chr) { g_uiMoveChr = chr; g_sel = 0; g_msg[0] = 0; }
        if (g_moveCat == kMcSeq) {  // user #14317
            for (int i = 0; i < N3_MOVE_COUNT; ++i) if (kN3Moves[i].chr == chr && SeqHead(chr, kN3Moves[i].id)) add(kItSeqBase + i);
            add(kItBack);
            return n;
        }
        // live-logged Ki-break ids first on the Ki break page, then catalogue order
        for (int pass = 0; pass < 2; ++pass)
            for (int i = 0; i < N3_MOVE_COUNT; ++i) {
                const N3Move& m = kN3Moves[i];
                if (m.chr != chr || N3MoveCat(&m) != g_moveCat) continue;
                if ((N3KiNote(m.chr, m.id) ? 0 : 1) != pass) continue;
                add(kItMoveBase + i);
            }
        add(kItBack);
        return n;
    }
    if (g_uiPages.page == uGate) {
        if (TrainingExport("Nioh3Training_GateFightAgain")) add(kItFightAgain);  // #9994: Fight again first, Return second
        const int mode = GateMode();  // user #13262: the gate page follows the session mode (0 = old 2-option page)
        if (mode == 2) {  // Training (#14336 item 11): short list, everything else is one level down in Training Bosses
            if (TrainingExport("Nioh3Training_SetPlayerCantDie")) add(kItPlayerOpen);  // user #14267
            if (PracticeToolsAvailable()) add(kItRootTools);
        } else if (mode == 1) {
            AddBossFightItems(add);  // Boss Rematch fight: the rematch boss-fight options
            if (TrainingExport("Nioh3Training_SetPlayerCantDie")) add(kItPlayerOpen);  // user #14267
        }
        if (LeaveArenaOffered()) add(kItLeaveArena);
        add(kItReturnHome);
        add(kItBack);  // user #13529: every page has a Back row (here: back to the mod menu's first page)
        add(kItClose);
        return n;
    }
    if (g_uiPages.page == uPractice) {
        add(kItPracticeStart);
        add(kItPracticeReturn);
        add(kItBack);
        return n;
    }
    if (g_uiPages.page == uSettings) {
        for (int group = 0; group < kConfigGroupN; ++group) add(kItGroupBase - group);
        add(kItBack);  // user #13197: no Save row, every edit is written at once (#14336: the Practice hub is the only Training door)
        return n;
    }
    if (g_uiPages.page >= uSettingFirst && g_uiPages.page < uSettingFirst + kConfigGroupN) {
        for (int i = 0; i < kConfigN; ++i)
            if (kConfigSettings[i].group == g_uiPages.page - uSettingFirst) add(kItSettingBase - i);
        add(kItBack);
        return n;
    }
    if (g_uiPages.page == uActions) {
        if (g_uiBoss >= 0 && (size_t)g_uiBoss < g_rows.size()) {
            add(kItFight + g_uiBoss);
            add(kItShrine + g_uiBoss);  // user #13164: teleport moved to Training (arenas), summon to Summon Boss
        }
        add(kItBack);
        return n;
    }
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
            add((int)i);
        }
    add(kItBack);
    return n;
}

// pc2 #23907: once a boss's runtime list is merged, log what its Boss moves page shows (the same counts as the page rows / title).
void AutoMovesTick() {
    static uint32_t logged[64]; static int nlog;
    const uint32_t chr = MoveBoss();  // syncs (AutoMovesSync)
    if (!chr || !N3CatIsAuto(chr)) return;
    for (int i = 0; i < nlog; ++i) if (logged[i] == chr) return;
    if (nlog < 64) logged[nlog++] = chr;
    char b[700]; size_t o = 0; int tot = 0;
    for (int c : kMcOrder) { const int k = MoveCatCount(chr, c); tot += k; if (k && o + 48 < sizeof b) o += (size_t)snprintf(b + o, sizeof b - o, "%s%s %d", o ? ", " : "", N3MoveCatName(c), k); }
    Log("Rematch: AUTOMOVES chr %u Boss moves page (game data): %d moves in groups: %s; Sequences %d", chr, tot, o ? b : "none", SeqCount(chr));
}

}  // namespace

bool CoverSnapshot(CoverInfo& out) {
    CoverLock lock;if(!lock.held)return false;
    const uint64_t pump=g_lastTick.load();
    const uint64_t now=Ms();g_cover.Expire(now);
    if(g_cover.Active() && g_coverHostAt && (now<g_coverHostAt || now-g_coverHostAt>=1500))
        g_cover.End(transition_cover::Failed,"Cover renderer stale; native recovery remains owned");
    if(g_cover.Active() && g_cover.issued && (!pump || now<pump || now-pump>=1500))
        g_cover.End(transition_cover::Failed,"Native update owner stale; recovery remains owned");
    static uint64_t loggedToken=0;static uint32_t loggedPhase=0;
    if(loggedToken!=g_cover.token||loggedPhase!=(uint32_t)g_cover.phase){
        Log("Rematch: cover token=%llu phase=%u attempt=%llu event=phase reason=%s",g_cover.token,(unsigned)g_cover.phase,g_cover.attempt,g_cover.reason);
        loggedToken=g_cover.token;loggedPhase=(uint32_t)g_cover.phase;
    }
    out={};out.token=g_cover.token;out.deadline=g_cover.deadline;out.phase=g_cover.phase;
    snprintf(out.reason,sizeof(out.reason),"%s",g_cover.reason);return true;
}
void CoverPresented(uint64_t token) {
    CoverLock lock;if(!lock.held||token!=g_cover.token||!g_cover.Active())return;
    const uint64_t now=Ms();g_cover.Expire(now);if(!g_cover.Active())return;
    g_coverHostAt=now;g_cover.Presented(token,now); // renderer callback: bounded state only, no logging/file I/O
}
void CoverCancel(uint64_t token) { CoverLock lock;if(lock.held)g_cover.Cancel(token); }
// Shared final front-end gate for manual hosted confirmation and the opt-in
// local test dispatcher. UI-worker only; native work still requires Present ACK.
bool BeginHostedFight(int row,uint64_t* token,char* reason,size_t n) {
    const char* why=nullptr;
    if(!g_live.load())why="Boss Matches unavailable";
    else if(row<0||(size_t)row>=g_rows.size())why="unknown exact row";
    else if((why=Gates(ReadSnap(),true))!=nullptr){}
    else if(g_status[(size_t)row].load()!=2)why="not beaten on this journey";
    else why=CanArena(row);
    if(why){snprintf(reason,n,"%s",why);return false;}
    if(!CoverBegin(row,token)){snprintf(reason,n,"transition unavailable or already active");return false;}
    snprintf(reason,n,"awaiting actual covered Present");return true;
}
bool UiTestFight(const char* id,uint64_t& token,char* reason,size_t n) {
    token=0;
    int row=-1;for(size_t i=0;i<g_rows.size();++i)if(!strcmp(g_rows[i].id,id)){row=(int)i;break;}
    if(!BeginHostedFight(row,&token,reason,n))return false;
    g_uiPages.page=uActions;g_uiBoss=row;g_sel=0;g_confirm=-9;
    snprintf(g_msg,sizeof(g_msg),"Preparing Fight now...");return true;
}
bool UiActHosted(){g_hostedUiAction=true;const bool result=UiAct();g_hostedUiAction=false;return result;}

bool Prepare() {
    const wchar_t* ini = IniPath();
    if (!GetPrivateProfileIntW(L"Rematch", L"Enable", 1, ini)) {
        Log("Rematch: off ([Rematch] Enable = 0)");
        return false;
    }
    g_cfg.hiddenDuels = IniInt(L"Rematch", L"HiddenDuels", 0) != 0;
    g_cfg.directStart = IniInt(L"Rematch", L"DirectStart", 0) != 0;
    g_cfg.showAll = IniInt(L"Rematch", L"ShowAll", 1) != 0;
    g_cfg.showUnverified = IniInt(L"Rematch", L"ShowUnverified", 0) != 0;
    g_cfg.fightNow = IniInt(L"Rematch", L"FightNow", 1) != 0;
    g_cfg.fightWarp = IniInt(L"Rematch", L"FightNowWarp", 1) != 0;
    g_cfg.anyBoss = IniInt(L"Rematch", L"AnyBoss", 1) != 0;
    g_cfg.arenaCapture = IniInt(L"Rematch", L"ArenaCapture", 0) != 0;
    g_cfg.fightSpawn = IniInt(L"Rematch", L"FightNowSpawn", 1) != 0;
    wchar_t stw[512] = {};
    GetPrivateProfileStringW(L"Rematch", L"SelfTest", L"", stw, 512, ini);
    const char* why = nullptr;
    if (!GroupOk("CORE", &why) || !GroupOk("FIGHT", &why) || !GroupOk("PUMP", &why)) {
        Log("Rematch: off - %s", why);
        return false;
    }
    g_coverReadOk=CoverAnchors();
    Log("Rematch: transition readiness anchors %s",g_coverReadOk?"matched":"unavailable");
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
    g_warpOk = GroupOk("WARP", &g_warpWhy) && ResolveWarp(&g_warpWhy);
    g_exitOk = GroupOk("EXIT", &g_exitWhy);
    g_spawnOk = GroupOk("SPAWN", &g_spawnWhy) && ResolveSpawn(&g_spawnWhy);
    g_gateOk = GroupOk("GATE", &g_gateWhy);
    if (g_gateOk) {
        F_RIGHTSREQ = Site("fn:RightsRequest");
        void* orig = nullptr;
        if (!InstallCallRetarget("call:RightsCheck", (void*)&RightsCheckHook, &orig, &g_gateWhy)) g_gateOk = false;
        else g_rightsCheckOrig = (RightsCheckFn)orig;
    }
    if (g_exitOk) F_REQRETURN = Site("fn:MissionRequestReturn");
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
    std::string at;
    int na = 0;
    if (ReadText(ArenaPath(), at)) na = rcore::ApplyArenas(at, g_rows);
    if (na) Log("Rematch: %d rows take their arena point from %ls (captured)", na, ArenaPath().c_str());
    {  // ally #8093: rows still without an arena point fall back to Nioh3Training's arena table (same folder), the table the in-place swap uses
        const std::wstring tpath = std::wstring(ModDir()) + L"Nioh3Training_arenas.tsv";
        std::string tt;
        const int nt = ReadText(tpath, tt) ? rcore::ApplyTrainingArenas(tt, g_rows) : 0;
        if (nt) Log("Rematch: %d rows without an arena point take it from %ls (Nioh3Training table)", nt, tpath.c_str());
    }
    Log("Startup: registering Rematch FIGHT callbacks");
    OnEntry("Character::Create", &HkCoverCreate);
    OnEntry("BossBattleMode", &HkArena);  // player position at every fight start (+ capture); FIGHT is installed below
    if (stw[0]) {  // SelfTest = id,id,... or 'arena' (every row with an arena point)
        char st8[512] = {};
        WideCharToMultiByte(CP_ACP, 0, stw, -1, st8, sizeof(st8), nullptr, nullptr);
        if (!_stricmp(st8, "arena")) {
            for (size_t i = 0; i < g_rows.size(); ++i)
                if (g_rows[i].FightNowReady() && g_rows[i].route != rcore::kX) g_st.rows.push_back((int)i);
        } else {
            char* ctx = nullptr;
            for (char* tok = strtok_s(st8, ", ", &ctx); tok; tok = strtok_s(nullptr, ", ", &ctx))
                for (size_t i = 0; i < g_rows.size(); ++i)
                    if (!_stricmp(g_rows[i].id, tok)) g_st.rows.push_back((int)i);
        }
        Log("Rematch: SELFTEST armed: %zu rows (%ls); starts 8 s after the add-on is idle in the field", g_st.rows.size(), stw);
    }
    Log("Startup: installing Rematch FIGHT group");
    if (!InstallGroup("FIGHT", &why)) {
        Log("Rematch: off - FIGHT hooks: %s", why);
        return false;
    }
    g_prepared = true;
    int c[5] = {}, nf = 0, nfh = 0, nar = 0, nat = 0;
    for (const Row& r : g_rows) {
        c[r.route]++;
        nf += r.fightStart != 0;
        nfh += r.fightStart != 0 && !strcmp(r.fightConf, "high");
        nar += r.fightStart && r.arenaOk;
        nat += r.fightStart && r.atFight;
    }
    Log("Rematch: prepared - %zu catalogue rows (R %d, H %d, D %d, E %d, X %d; %d verified by verify_all); R-a %s%s, E %s%s; HiddenDuels %d, DirectStart %d, "
        "ShowAll %d, ShowUnverified %d. Solo/offline only; no story-flag writes.",
        g_rows.size(), c[0], c[1], c[2], c[3], c[4], nv, g_raOk ? "on" : "OFF: ", g_raOk ? "" : g_raWhy, g_emakiOk ? "on" : "OFF: ",
        g_emakiOk ? "" : g_emakiWhy, g_cfg.hiddenDuels, g_cfg.directStart, g_cfg.showAll, g_cfg.showUnverified);
    Log("Rematch: Fight now %s - %d rows with a start point next to the boss (%d high confidence)%s%s; arena warp %s (%d rows with an arena point, %d "
        "start at the fight)%s%s; ArenaCapture %d",
        g_cfg.fightNow ? "ON" : "off", nf, nfh, g_fnowOk ? "" : "; sites FNOW failed: ", g_fnowOk ? "" : g_fnowWhy, g_cfg.fightWarp ? "ON" : "off", nar, nat,
        g_warpOk ? "" : "; WARP: ", g_warpOk ? "" : g_warpWhy, g_cfg.arenaCapture);
    Log("Rematch: boss gates (door / box exclusive rights) %s%s", g_gateOk ? "on" : "OFF: ", g_gateOk ? "" : g_gateWhy);
    Log("Rematch: arena spawn (start point override, FightNowSpawn %d) %s%s", g_cfg.fightSpawn, g_spawnOk ? "on" : "OFF: ", g_spawnOk ? "" : g_spawnWhy);
    return true;
}

bool Live() { return g_live.load(); }
void SetPumpLive(bool installed) { g_live.store(g_prepared && installed); }
void Pump() { if (Live()) Tick(); }

std::atomic<int> g_configSaveState{0};  // 1 writing, 2 saved, 3 failed
std::atomic<DWORD> g_configSaveError{0};
struct ConfigSaveJob { std::wstring ini; std::vector<std::wstring> entries; };
DWORD WINAPI SaveConfigThread(void* param) {
    ConfigSaveJob* job = static_cast<ConfigSaveJob*>(param);
    DWORD error = 0;
    for (size_t i = 0; i < job->entries.size(); i += 3)
        if (!WritePrivateProfileStringW(job->entries[i].c_str(), job->entries[i + 1].c_str(), job->entries[i + 2].c_str(), job->ini.c_str()))
            error = GetLastError() ? GetLastError() : ERROR_WRITE_FAULT;
    if (!WritePrivateProfileStringW(nullptr, nullptr, nullptr, job->ini.c_str()))
        error = GetLastError() ? GetLastError() : ERROR_WRITE_FAULT;
    delete job;
    g_configSaveError.store(error);
    g_configSaveState.store(error ? 3 : 2);
    return 0;
}
void UiSettingsPrepare() { g_configValues.Load(kConfigSettings, IniPath()); }
bool g_configSavePending = false;  // user #13197 auto-save: an edit made while a write was running is written once that write ends (UiFill)
void SaveUiSettings() {
    if (g_configSaveState.load() == 1) { g_configSavePending = true; return; }
    g_configSavePending = false;
    const wchar_t* ini = IniPath();
    if (!ini || !ini[0]) return;  // no ini (offline harness): edits stay in this menu
    ConfigSaveJob* job = new (std::nothrow) ConfigSaveJob;
    if (!job) { snprintf(g_msg, sizeof(g_msg), "Save failed: no memory. Preferences remain in this menu."); return; }
    job->ini = ini;
    g_configValues.Save(kConfigSettings, [&](const wchar_t* section, const wchar_t* key, const std::wstring& value) {
        job->entries.push_back(section); job->entries.push_back(key); job->entries.push_back(value);
    });
    if (job->entries.empty()) { delete job; snprintf(g_msg, sizeof(g_msg), "No edited startup preferences to save."); return; }
    g_configSaveError.store(0); g_configSaveState.store(1);
    if (HANDLE thread = CreateThread(nullptr, 0, SaveConfigThread, job, 0, nullptr)) {
        CloseHandle(thread);
    } else {
        g_configSaveError.store(GetLastError()); g_configSaveState.store(3); delete job;
    }
}
void UiChange(int dir) {
    static int items[kMaxRows + 4];
    const int n = Items(items, kMaxRows + 4);
    if (!n || g_sel < 0) return;  // #38077: the two top rows (Mod menu / Close) have no Left / Right
    if (g_sel >= n) g_sel = n - 1;
    const int item = items[g_sel];
    if (item == kItDeathCut || item == kItDeathBlow || item == kItDeathLoot) { DeathCfgToggle(item, dir); return; }
    if (item == kItAutoRetry) { AutoRetryToggle(dir); return; }
    if (item == kItNoFin) { NoFinToggle(dir); return; }
    if (item == kItFinT || item == kItHp0W) { TuneStep(item, dir); return; }
    if (IsMoveItem(item)) { MoveToggle(item, dir); return; }
    if (IsSeqItem(item)) { SeqToggle(item, dir); return; }  // user #14317
    if (item == kItBossStance) { BossStanceSet(dir); return; }  // user #14309: per boss
    if (item == kItBossAggro) { BossAggroSet(dir); return; }
    if (item == kItBossGap) { BossGapStep(dir); return; }
    if (item == kItStanceTimer) {  // user #14313: 1..10 s
        if (dir) { g_stSec = g_stSec + (dir > 0 ? 1 : -1); g_stSec = g_stSec < 1 ? 1 : g_stSec > 10 ? 10 : g_stSec; g_msg[0] = 0; }
        return;
    }
    if (IsBossOpenItem(item) || item == kItBossReset || item == kItBossRemove || item == kItSumAnother) return;
    if (item == kItIdleHold) { IdleHoldToggle(dir); return; }
    if (item == kItRepGap) { RepGapStep(dir); return; }
    if (item == kItAggro) { AggroToggle(dir); return; }  // user #13787
    if (item == kItCantDie) { CantDieToggle(dir); return; }  // user #14150
    if (BvChange(item, dir)) return;  // user #14320
    if (PstChange(item, dir)) return;  // user #22389
    if (item == kItShowTw) { g_showTwitch = dir == 0 ? !g_showTwitch : dir > 0; Log("Rematch: move list Show twitches %s (#13786)", g_showTwitch ? "ON" : "OFF"); return; }
    if (item == kItRepNow) { if (dir <= 0) RepeatStop("Left on Repeating move"); else RepeatArm(RepeatCandidate(), "Right on Repeating move"); return; }
    if (item == kItRepSel) { if (dir < 0) RepeatStop("Left on Repeat selected move"); else RepeatArm(RepeatCandidate(), "Repeat selected move"); return; }
    if (item == kItSumPassive) { SumPassiveStep(dir); return; }
    if (item == kItSumSlot) { g_sumSlot = dir ? (dir > 0) : !g_sumSlot; g_msg[0] = 0; return; }
    if (item > kItSettingBase) return;
    const int index = kItSettingBase - item;
    if (index >= 0 && index < kConfigN && g_configValues.Change(kConfigSettings, (size_t)index, dir)) {
        const auto& d = kConfigSettings[index];
        snprintf(g_msg, sizeof(g_msg), "[%s] %s: saved automatically. Applies after a restart; current session unchanged.", d.section, d.key);
        SaveUiSettings();  // user #13197: no Save row, every edit is written at once
    }
}

void UiOpen() {
    g_hostMenuReq = false;
    g_uiPages.Reset();
    g_uiBoss = -1;
    g_bossSlot = -1;
    g_sel = 0;
    g_confirm = -9;
    g_msg[0] = 0;
    g_pageGen0 = g_scanGen.load();
    if (g_live.load()) g_wantScan.store(true);
}
void UiGatePage() {  // user #13529: the gate page is a base page (no add-on root behind it): Back returns to the host's first page
    UiOpen();
    g_uiPages.page = uGate;
    g_sel = 0;
}
void UiOpenAt(int which) {  // user #13223: host first page -> Boss Rematch (1) / Practice (2) as the base page; Back at it closes to the host menu
    UiOpen();
    g_uiPages.page = which == 1 ? uRematch : uPracticeHub;
}
void UiMove(int dir) {
    static int it[kMaxRows + 4];
    const int n = Items(it, kMaxRows + 4);
    // #38077: virtual rows -2 'Mod menu' and -1 'Close' above the page's own rows; the cursor runs over [-2, n)
    const int pos = (dir == 10 || dir == -10) ? menu_nav::PageStep(g_sel + 2, dir, n + 2) : menu_nav::Wrap(g_sel + 2, dir, n + 2);
    g_sel = pos - 2;
    g_confirm = -9;
    g_msg[0] = 0;
}
// #38077: hosted 'Mod menu' row pressed: the host leaves this page for its own first page (sticky until reopened)
bool UiTakeHostMenu() { const bool r = g_hostMenuReq; return r; }
void UiClearHostMenu() { g_hostMenuReq = false; }
// #38216: a boss not beaten on this journey cannot be replayed (a real mission would write progress to the save). With [Rematch] AnyBoss = 1
// its Fight now / arena rows start a Practice boss right here instead (Nioh3Training_TrainHere: a no-load summon, no mission, no save write).
bool AnyBossOk(int row) { return g_cfg.anyBoss && row >= 0 && (size_t)row < g_rows.size() && g_rows[(size_t)row].missionId && TrainingExport("Nioh3Training_TrainHere"); }
bool UiAct() {
    static int it[kMaxRows + 4];
    const int n = Items(it, kMaxRows + 4);
    if (g_sel == -1) return true;  // #38077 top row 'Close': close the whole menu (hosted: the host closes with it)
    if (g_sel == -2) {  // #38077 top row 'Mod menu': jump to the menu's top-level page
        if (g_hostedUiAction) { g_hostMenuReq = true; return true; }  // hosted: the EngineFix menu's first page is the top level
        g_uiPages.Reset(); g_sel = 0; g_confirm = -9; g_msg[0] = 0; g_bossSlot = -1;
        return false;
    }
    if (!n) return false;
    if (g_sel >= n) g_sel = n - 1;
    const int item = it[g_sel];
    if (item == kItClose) return true;
    if (item == kItDeathCut || item == kItDeathBlow || item == kItDeathLoot) { DeathCfgToggle(item, 0); return false; }
    if (item == kItAutoRetry) { AutoRetryToggle(0); return false; }
    if (item == kItNoFin) { NoFinToggle(0); return false; }
    if (item == kItFinT || item == kItHp0W) { TuneStep(item, 0); return false; }
    if (IsMoveItem(item)) { MoveToggle(item, 0); return false; }
    if (IsSeqItem(item)) { SeqToggle(item, 0); return false; }  // user #14317
    if (IsBossOpenItem(item)) {  // user #14309: Boss X page
        g_bossSlot = item - kItBossOpenBase;
        SetMoveSlot(g_bossSlot);
        g_uiPages.Enter(uBoss, g_sel);
        g_uiMoveChr = SlotChr(g_bossSlot); g_sel = 0; g_msg[0] = 0; g_confirm = -9;
        Log("Rematch: Boss %c page opened (%s)", 'A' + g_bossSlot, SlotBossName(g_uiMoveChr));
        return false;
    }
    if (item == kItBossStance) { BossStanceSet(0); return false; }
    if (item == kItBossAggro) { BossAggroSet(0); return false; }
    if (item == kItBossGap) { BossGapStep(0); return false; }
    if (item == kItBossReset) { BossResetPos(); return false; }
    if (item == kItBossRemove) { if (BossRemove()) { g_msg[0] = 0; g_uiPages.Back(g_sel); } return false; }
    if (item == kItStanceTimer) return StanceTimerAct();  // user #14313: arming closes the menu (get ready)
    if (item == kItSumAnother) { g_uiPages.Enter(uSummon, g_sel); g_sel = 0; g_confirm = -9; g_msg[0] = 0; return false; }
    if (item == kItIdleHold) { IdleHoldToggle(0); return false; }
    if (item == kItRepGap) { RepGapStep(0); return false; }
    if (item == kItAggro) { AggroToggle(0); return false; }  // user #13787
    if (item == kItCantDie) { CantDieToggle(0); return false; }  // user #14150
    if (BvAct(item)) return false;  // user #14320
    if (PstChange(item, 0)) return false;  // user #22389
    if (item == kItShowTw) { g_showTwitch = !g_showTwitch; Log("Rematch: move list Show twitches %s (#13786)", g_showTwitch ? "ON" : "OFF"); return false; }
    if (item == kItResetPos) { ResetBossPos(); return false; }
    if (item == kItRepNow) { RepeatStop("A on Repeating move"); return false; }
    if (item == kItRepSel) { RepeatArm(RepeatCandidate(), "A on Repeat selected move"); return false; }  // user #13618
    if (item == kItSumPassive || item == kItSumSlot) { UiChange(0); return false; }
    if (item == kItRootRematch || item == kItRootTraining || item == kItRootSummon || item == kItRootTools ||
        item == kItTrainHereOpen || item == kItArenas || item == kItBossSumm || item == kItFightOpts || item == kItPlayerOpen) {  // user #13164 / #13197 / #13223 / #13265 / #14267
        g_uiPages.Enter(item == kItRootRematch ? uRematch : item == kItRootTraining ? uPracticeHub : item == kItRootSummon || item == kItBossSumm ? uSummon :
                        item == kItArenas ? uTraining : item == kItTrainHereOpen ? uTrainHere : item == kItFightOpts ? uFightOpts : item == kItPlayerOpen ? uPlayer : uTools, g_sel);
        g_sel = 0; g_confirm = -9; g_msg[0] = 0;
        // ally #89: Boss A is the Training session boss (arena boss): a new summon goes to the free slot B instead of replacing it
        if ((item == kItRootSummon || item == kItBossSumm) && SlotChr(0) && !SlotChr(1)) g_sumSlot = 1;
        return false;
    }
    if (item >= kItTrainHere && item < kItTrainHereEnd) {  // user #13223: Practice > Training here
        const int row = item - kItTrainHere;
        if (row < 0 || (size_t)row >= g_rows.size() || !g_rows[(size_t)row].missionId) { snprintf(g_msg, sizeof g_msg, "No boss mission id for this row."); return false; }
        using Fn = int (*)(uint32_t);
        const auto f = (Fn)TrainingExport("Nioh3Training_TrainHere");
        const int rc = f ? f(g_rows[(size_t)row].missionId) : 0;
        Log("Rematch: menu 'Training here' %s -> %d", g_rows[(size_t)row].id, rc);
        if (!rc) { snprintf(g_msg, sizeof g_msg, "Busy (a summon or load is running): try again in a moment."); return false; }
        ToastF(cGreen, 5000, "Training: %s, right here (boss stands still, retry in place)...", g_rows[(size_t)row].name);
        return true;
    }
    if (item == kItDespawnAll) {
        using Fn = int (*)();
        const auto f = (Fn)TrainingExport("Nioh3Training_DespawnAll");
        const int rc = f ? f() : 0;
        Log("Rematch: menu 'Despawn all bosses' -> %d", rc);
        snprintf(g_msg, sizeof g_msg, "%s", rc ? "Removing every summoned boss." : "Busy: try again in a moment.");
        return false;
    }
    if (item >= kItSummonA && item < kItSummonB) return SummonAnother(item - kItSummonA);  // user #14336 item 7: next free letter
    if (item >= kItSummonRoster && item < kItSummonRosterEnd) return SummonRoster(item - kItSummonRoster);  // user #22984
    if (item >= kItSummonA && item < kItSummonEnd) {
        const int slot = item >= kItSummonB ? 1 : 0;
        const int row = item - (slot ? kItSummonB : kItSummonA);
        if (row < 0 || (size_t)row >= g_rows.size() || !g_rows[(size_t)row].missionId) {
            snprintf(g_msg, sizeof g_msg, "No boss mission id for this row: cannot summon it.");
            return false;
        }
        using Fn = int (*)(int, uint32_t);
        const auto f = (Fn)TrainingExport("Nioh3Training_SummonSlot");
        const int rc = f ? f(slot, g_rows[(size_t)row].missionId) : 0;
        Log("Rematch: menu summon %s as Boss %c -> %d", g_rows[(size_t)row].id, 'A' + slot, rc);
        if (!rc) { snprintf(g_msg, sizeof g_msg, "Busy (a summon or load is running): try again in a moment."); return false; }
        ToastF(cGreen, 4000, "Summoning %s as Boss %c...", g_rows[(size_t)row].name, 'A' + slot);
        return true;
    }
    if (item == kItMovesA || item == kItMovesB) {
        const int slot = item == kItMovesB;
        if (!SlotChr(slot)) { snprintf(g_msg, sizeof g_msg, "Boss %c is empty: summon a boss into slot %c first (Training > Summon a boss here).", 'A' + slot, 'A' + slot); return false; }
        SetMoveSlot(slot);
        g_uiPages.Enter(uMoves, g_sel);
        g_uiMoveChr = MoveBoss(); g_sel = 0; g_msg[0] = 0; g_confirm = -9;
        return false;
    }
    if (IsMoveCatItem(item)) {  // user #14291: open that move group's page
        g_moveCat = kItMoveCatBase - item;
        g_uiPages.Enter(uMoveCat, g_sel);
        g_uiMoveChr = MoveBoss(); g_msg[0] = 0; g_confirm = -9;
        g_sel = 0;  // #14336 item 4: the page holds move rows only
        Log("Rematch: move group '%s' opened (#14291)", CatName(g_moveCat));
        return false;
    }
    if (item == kItMoves) {
        SetMoveSlot(-1);
        g_uiPages.Enter(uMoves, g_sel);
        g_uiMoveChr = MoveBoss(); g_sel = 0; g_msg[0] = 0; g_confirm = -9;
        return false;
    }
    if (item == kItLeaveArena) {
        const int rc = LeaveArena("menu");
        if (rc == 0) { snprintf(g_msg, sizeof(g_msg), "Nioh3Training is missing or too old: Leave arena needs it."); return false; }
        ToastF(cWhite, 6000, "Boss Matches: boss mode ended. Open the map and travel to a shrine.");
        return true;
    }
    if (item == kItReturnHome) {  // not behind the rematch gates: it is the way out of a running swap / arena session
        using Fn = int (*)();
        const auto ret = (Fn)TrainingExport("Nioh3Training_ReturnHome");
        const int rc = ret ? ret() : 0;
        Log("Rematch: menu 'Return to where I was' -> %d (info %d)", rc, ReturnInfo());
        if (rc == -1) { snprintf(g_msg, sizeof(g_msg), "Already returning..."); return false; }
        if (rc == 0) { snprintf(g_msg, sizeof(g_msg), "Nothing to return from: no Fight now arena swap since the game started (or you are already back)."); return false; }
        ToastF(cWhite, 5000, rc == 1 ? "Boss Matches: back to where you were..." : "Boss Matches: arena session ended (no stored spot to return to).");
        return true;
    }
    if (item == kItFightAgain) {
        using Fn = int (*)();
        const auto fa = (Fn)TrainingExport("Nioh3Training_GateFightAgain");
        const int rc = fa ? fa() : 0;
        Log("Rematch: gate 'Fight again' -> %d", rc);
        if (rc <= 0) { snprintf(g_msg, sizeof(g_msg), "No arena session to fight again in."); return false; }
        ToastF(cWhite, 4000, "Boss Matches: fight again...");
        return true;
    }
    if (item <= kItSettingBase) { UiChange(0); return false; }
    if (item == kItSettings || (item <= kItGroupBase && item > kItSettingBase)) {
        g_uiPages.Enter(item == kItSettings ? uSettings : uSettingFirst + kItGroupBase - item, g_sel);
        g_sel = 0; g_confirm = -9; g_msg[0] = 0;
        return false;
    }
    if (item == kItBack) return UiBack();
    if (item == kItPractice) {
        g_uiPages.Enter(uPractice, g_sel);
        g_sel = 0; g_confirm = -9; g_msg[0] = 0;
        return false;
    }
    if (item == kItBrowse || ((g_uiPages.page == uBosses || g_uiPages.page == uRematch) && item >= 0 && item < kItFight)) {
        if (item >= 0) g_uiBoss = item;
        g_uiPages.Enter(item == kItBrowse ? uBosses : uActions, g_sel);
        g_sel = 0;
        g_confirm = -9;
        g_msg[0] = 0;
        return false;
    }
    if (item == kItAbort) {
        Post(kReqAbort, -1, "the menu");
        return true;
    }
    if (!g_live.load()) {
        snprintf(g_msg, sizeof(g_msg), "Boss Matches are unavailable this session. Settings can still be changed for the next launch.");
        return false;
    }
    // Return is an operation on the owned transaction, not a new start. The
    // generic start gate deliberately refuses all active native attempts.
    if (item == kItPracticeReturn) {
        const char* why = !g_exitOk ? g_exitWhy : PracticeReturnGate(ReadSnap());
        if (why) { snprintf(g_msg, sizeof(g_msg), "Not now: %s.", why); return false; }
        if (g_confirm != item) {
            g_confirm = item;
            snprintf(g_msg, sizeof(g_msg), "Leave practice using the native mission return? A confirm / B back.");
            return false;
        }
        g_confirm = -9; Post(kReqPracticeExit, -1, "the practice menu");
        return true;
    }
    const char* why = Gates(ReadSnap(), true);
    if (why) {
        snprintf(g_msg, sizeof(g_msg), "Not now: %s.", why);
        return false;
    }
    if (item == kItPracticeStart) {
        const int row = PracticeRow();
        const char* whyPractice = row < 0 ? "Yamagata practice route missing or changed" : CanFight(row);
        if (whyPractice || g_status[(size_t)row].load() != 2) {
            snprintf(g_msg, sizeof(g_msg), "%s", whyPractice ? whyPractice : "Beat Yamagata on this journey first.");
            return false;
        }
        if (g_confirm != item) {
            g_confirm = item;
            snprintf(g_msg, sizeof(g_msg), "Practice Yamagata from the shrine? Normal rewards and progression apply. A confirm / B back.");
            return false;
        }
        g_confirm = -9; Post(kReqPractice, row, "the practice menu");
        ToastF(cWhite, 8000, "Training: walk from the shrine into Yamagata's arena. Normal native rewards apply.");
        return true;
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
        const bool tele = item >= kItTeleport;
        const bool arena = item < kItShrine || tele;
        const int row = item - (tele ? kItTeleport : arena ? kItFight : kItShrine);
        if (g_status[(size_t)row].load() != 2) {
            if (arena && AnyBossOk(row)) {  // #38216: not beaten -> Practice boss right here, nothing of the save is touched
                if (g_confirm != item) { g_confirm = item; snprintf(g_msg, sizeof(g_msg), "Boss not cleared: train against %s here without mission progress? A: confirm / B: back.", g_rows[(size_t)row].name); return false; }
                g_confirm = -9;
                using Fn = int (*)(uint32_t);
                const int rc = ((Fn)TrainingExport("Nioh3Training_TrainHere"))(g_rows[(size_t)row].missionId);
                Log("Rematch: menu '%s' not beaten -> Practice here %s -> %d (#38216)", tele ? "Teleport" : "Fight now", g_rows[(size_t)row].id, rc);
                if (!rc) { snprintf(g_msg, sizeof(g_msg), "Busy (a summon or load is running): try again in a moment."); return false; }
                ToastF(cGreen, 5000, "Training: %s here (no mission progress)...", g_rows[(size_t)row].name);
                return true;
            }
            snprintf(g_msg, sizeof(g_msg), "Not beaten on this journey: Fight now only replays bosses you have beaten.");
            return false;
        }
        if (const char* w = arena ? CanArena(row) : CanShrine(row)) {
            snprintf(g_msg, sizeof(g_msg), "%s", w);
            return false;
        }
        if (g_confirm != item) {
            g_confirm = item;
            const Row& r = g_rows[(size_t)row];
            if (tele)
                snprintf(g_msg, sizeof(g_msg), "Enter the %s arena and summon its boss (idle)? A confirm / B back. Solo only.", r.name);
            else if (arena)
                snprintf(g_msg, sizeof(g_msg), "Fight %s now, inside the arena? A confirm / B back. Solo only.", r.name);
            else
                snprintf(g_msg, sizeof(g_msg), "Start at the %s %d m before %s? A confirm / B back. Solo only.", r.fightKind, r.fightDist, r.name);
            return false;
        }
        g_confirm = -9;
        // Nioh3Training loaded: the hosted Fight now takes the same path as the overlay (kReqFight -> ExecDirect ->
        // Nioh3Training_FightNow in-place swap with its own fade). The cover waits for a native mission load the swap never does.
        if(arena && !tele && g_hostedUiAction && !TrainingSwapAvailable()) {
            BeginHostedFight(row,nullptr,g_msg,sizeof(g_msg));
            return false; // keep the menu until the renderer presents its cover
        }
        if (arena) SetPracticeNext(tele ? 1 : 0);  // user #13223: Practice arena sessions retry in place; Boss Rematch Fight now keeps AutoRetryOnDeath
        Post(arena ? kReqFight : kReqShrine, row, "the menu");  // #13167: Training arena = Fight now (Nioh3Training session, boss summoned in idle stance)
        ToastF(cWhite, 6000, arena ? "Boss Matches: into the fight with %s..." : "Boss Matches: to the shrine before %s...", g_rows[(size_t)row].name);
        return true;
    }
    uint32_t kind = 0;
    if (const char* w = CanStart(item, kind)) {
        snprintf(g_msg, sizeof(g_msg), "%s", w);
        return false;
    }
    if (g_confirm != item) {
        g_confirm = item;
        snprintf(g_msg, sizeof(g_msg), "Start boss match: %s? A confirm / B back. Solo only. Drops: as the game's replay / scroll.", g_rows[(size_t)item].name);
        return false;
    }
    g_confirm = -9;
    Post(kind, item, "the menu");
    ToastF(cWhite, 6000, "Boss match queued: %s...", g_rows[(size_t)item].name);
    return true;
}
bool UiBack() {
    if (g_confirm != -9) {
        g_confirm = -9;
        g_msg[0] = 0;
        return false;
    }
    g_msg[0] = 0;
    return !g_uiPages.Back(g_sel);
}

// user #13279/#13293/#13296: plain page titles ('Rematch' is 'Boss Matches' everywhere the player reads it)
const char* PageTitle(const char* movesTitle) {
    const int p = g_uiPages.page;
    if (p == uGate) return GateMode() == 2 ? "Return - Training" : GateMode() == 1 ? "Return - Boss Match" : "Return gate";
    if (p >= uSettingFirst && p < uSettingFirst + kConfigGroupN) return kConfigGroupLabels[p - uSettingFirst];
    return p == uRoot ? "Mod menu" : p == uRematch ? "Boss Matches" : p == uTraining ? "Training - Choose an arena" : p == uPracticeHub ? "Training"
         : p == uTrainHere ? "Training - Choose a boss" : p == uSummon ? "Training - Add a boss" : p == uTools ? "Training - Active bosses"
         : p == uBoss ? movesTitle
         : p == uBosses ? "Bosses" : p == uActions ? "Boss actions" : p == uSettings ? "Settings" : p == uPractice ? "Yamagata mission replay"
         : p == uFightOpts ? "Death and loot - Training / Boss Matches" : p == uPlayer ? "Player settings - Training / Boss Matches" : p == uBossVitals ? BvTitle() : p == uMoves || p == uMoveCat ? movesTitle : "Mod menu";
}
bool IsFightRule(int item) { return item == kItDeathCut || item == kItDeathBlow || item == kItDeathLoot || item == kItNoFin || item == kItFinT || item == kItHp0W; }
// section header shown above an item (nullptr = no headers on this page / Back / Close)
const char* ItemGroup(int item) {
    const int p = g_uiPages.page;
    if (item == kItBack || item == kItClose) return nullptr;
    if (p == uRematch) return IsFightRule(item) ? "Death and loot (Training / Boss Matches)" : item >= 0 && item < kItFight ? "Bosses you can fight again" : "More";
    if (p == uGate) return item == kItFightAgain ? "This fight" : item == kItPlayerOpen ? "Player" : IsFightRule(item) || item == kItFightOpts ? "Boss Match rules" : item == kItReturnHome || item == kItLeaveArena ? "Leave" : "Active bosses";
    if (p == uBossVitals) return IsPstItem(item) ? "Boss settings (see each setting for mode)" : BvGroup(item);  // user #14320 / #22389
    if (p == uPlayer) return IsPstItem(item) ? "Player stats (Training / spawned Boss Matches)" : item == kItCantDie ? "Health" : nullptr;  // user #22389
    if (p == uTools) return IsBossOpenItem(item) ? "Active bosses (Training / Boss Matches)" : item == kItSumAnother ? "Add a boss" : item == kItStanceTimer || item == kItDespawnAll ? "All bosses" : item == kItReturnHome ? "Leave" : nullptr;
    if (p == uBoss) return item == kItBossStance || item == kItBossAggro || item == kItBossGap || item == kItBossReset ? "This boss" : IsBvOpenItem(item) ? "Health and Ki"
                         : IsMoveCatItem(item) ? "Moves by type and hit count" : item == kItBossRemove ? "Remove" : nullptr;
    if (p == uSummon) return item >= kItSummonA && item < kItSummonRosterEnd ? "Add a boss (Training / Boss Matches)" : nullptr;
    if (p == uMoves) return IsMoveCatItem(item) ? "Pick a move group (A: open)" : nullptr;  // user #14291
    if (p == uMoveCat) return IsSeqItem(item) ? "Move chains (A: repeat on/off)" : IsMoveItem(item) ? "A: repeat | Left: block move | Right: allow move" : nullptr;
    return nullptr;
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
    TrainingPreload();
    if (g_configSavePending && g_configSaveState.load() != 1) SaveUiSettings();  // user #13197: edit made during a running write
    const Snap s = ReadSnap();
    const char* gate = !g_live.load() ? "Boss Matches off; settings available" : g_state.load() != kIdle ? nullptr : Gates(s, true);
    const int st = g_state.load();
    static char movesTitle[128];
    snprintf(movesTitle, sizeof movesTitle, "Boss moves (start Training or a Boss Match)");
    if (g_uiPages.page == uMoves || g_uiPages.page == uMoveCat || g_uiPages.page == uBoss) {
        const uint32_t chr = MoveBoss();
        char who[16] = "Boss moves";
        if (g_bossSlot >= 0) snprintf(who, sizeof who, "Boss %c", 'A' + g_bossSlot);
        if (chr) snprintf(movesTitle, sizeof movesTitle, "%s - %.40s%s", who, SlotBossName(chr), g_uiPages.page == uBoss ? "" : " (no moves recorded yet)");
        else if (g_bossSlot >= 0) snprintf(movesTitle, sizeof movesTitle, "%s - gone", who);
        for (int i = 0; g_uiPages.page != uBoss && i < N3_MOVE_COUNT; ++i)
            if (kN3Moves[i].chr == chr) {
                if (g_uiPages.page == uMoveCat)  // user #14291: 'Boss A - Minamoto no Yoritomo - Attacks (24)'
                    snprintf(movesTitle, sizeof movesTitle, "%s - %.40s - %s (%d)", who, kN3Moves[i].boss, CatName(g_moveCat), g_moveCat == kMcSeq ? SeqCount(chr) : MoveCatCount(chr, g_moveCat));
                else snprintf(movesTitle, sizeof movesTitle, "%s - %.60s", who, kN3Moves[i].boss);
                break;
            }
    }
    add(cCyan, false, "%s (" ADDON_VERSION ") - %s", PageTitle(movesTitle), st == kMenuOpen ? "active: replay menu" : st == kInReplay ? (g_practiceOwned.load() ? "active: native practice" : g_fight.load() ? "active: fight now" : "active: replay")
                                                                                  : st == kEmaki ? "active: picture scroll" : gate ? gate : "ready");
    static int it[kMaxRows + 4];
    const int n = Items(it, kMaxRows + 4);
    if (g_sel >= n) g_sel = n ? n - 1 : 0;
    if (g_sel < -2) g_sel = -2;
    // #38077: every page starts with 'Mod menu' (jump to the top-level page) and 'Close' (close the menu)
    add(g_sel == -2 ? cWhite : cGray, g_sel == -2, "%s Mod menu   A: open", g_sel == -2 ? ">" : " ");
    add(g_sel == -1 ? cWhite : cGray, g_sel == -1, "%s Close   A: close the mod menu", g_sel == -1 ? ">" : " ");
    const bool scanned = g_scanGen.load() != g_pageGen0;
    const bool grouped = ItemGroup(it[0]) != nullptr || (n > 1 && ItemGroup(it[1]) != nullptr);
    // user #13529: every page shows its Back row. A long list (arenas, boss moves) scrolled the last row (Back) off the panel, so the Back row is
    // pinned below the scrolled window; the list window covers the other rows. Up from the top row still wraps to it.
    const bool pinBack = n > 1 && it[n - 1] == kItBack && n > cap - 2 - (grouped ? 5 : 0);
    const int nList = pinBack ? n - 1 : n;
    const int vis = cap - 4 - (grouped ? 5 : 0) - (pinBack ? 1 : 0);  // user #13279/#13296: room for the section header rows
    const int selList = g_sel < 0 ? 0 : g_sel < nList ? g_sel : nList - 1;
    int first = selList - vis / 2;
    if (first > nList - vis) first = nList - vis;
    if (first < 0) first = 0;
    const char* lastGroup = nullptr;
    for (int i = first; i < nList && k < cap - 1 - (pinBack ? 1 : 0); ++i) {
        const bool sel = i == g_sel;
        const char* mk = sel ? ">" : " ";
        if (const char* grp = ItemGroup(it[i])) {  // section header (not selectable): "-- Name --"
            if (grp != lastGroup && k < cap - 2) add(cCyan, false, "-- %s --", grp);
            lastGroup = grp;
        }
        char bvText[sizeof rows[0].text]; uint32_t bvColor = cGray;  // user #14320
        if (BvRowText(it[i], sel, mk, bvText, sizeof bvText, &bvColor) || PstRowText(it[i], sel, mk, bvText, sizeof bvText, &bvColor)) add(bvColor, sel, "%s", bvText);  // + #22389
        else if (IsBossOpenItem(it[i])) {  // user #14309: 'Boss C: Minamoto no Yoritomo   Stand still (idle)   A: open'
            const int sl = it[i] - kItBossOpenBase, stn = SlotStance(sl);
            add(sel ? cWhite : stn == 0 ? cGreen : stn >= 1 ? cYellow : cGray, sel, "%s Boss %c: %.40s   %s   A: open", mk, 'A' + sl, SlotBossName(SlotChr(sl)), StanceName(stn));
        } else if (it[i] == kItSumAnother) {
            const int fs = FreeSlot();
            add(sel ? cWhite : cGray, sel, "%s Add a boss (slot %c)   A: choose", mk, fs >= 0 ? 'A' + fs : '?');
        } else if (it[i] == kItStanceTimer) {  // user #14313
            int toIdle = 0; const int left = StanceTimerLeft(&toIdle);
            if (left > 0) add(sel ? cWhite : cYellow, sel, "%s Switching every boss to %s in %d.%d s   A: cancel", mk, toIdle ? "Stand still" : "Fight (CPU)", left / 1000, (left % 1000) / 100);
            else add(sel ? cWhite : cGray, sel, "%s Switch every boss to %s after: %2d s   Left / Right 1-10 s, A: start", mk, StanceTimerTarget() ? "Stand still" : "Fight (CPU)", g_stSec);
        } else if (it[i] == kItBossStance) {
            const int stn = SlotStance(g_bossSlot);
            add(sel ? cWhite : stn == 0 ? cGreen : stn >= 1 ? cYellow : cDim, sel, "%s Behavior: %s   Left: fight / Right: wait", mk, StanceName(stn));
        } else if (it[i] == kItBossAggro) {
            const int v = BossAggroGet();
            add(sel ? cWhite : v < 0 ? cDim : cGray, sel, "%s Follow and target me: %s   A / Left / Right", mk, v < 0 ? "?" : v ? "ON" : "OFF");
        } else if (it[i] == kItBossGap) {
            const int v = BossGapGet();
            char val[24];
            if (v < 0) snprintf(val, sizeof val, "?"); else snprintf(val, sizeof val, "%d.%02d s", v / 1000, (v % 1000) / 10);
            add(sel ? cWhite : v < 0 ? cDim : cYellow, sel, "%s Repeat delay: %-7s   Left / Right", mk, val);
        } else if (it[i] == kItBossReset) add(sel ? cWhite : cGray, sel, "%s Reset boss position   A: apply", mk);
        else if (it[i] == kItBossRemove) add(sel ? cWhite : cGray, sel, "%s Remove this boss   press A", mk);
        else if (IsSeqItem(it[i])) {  // user #14317: '0x4E5C > 0x4E60 > 0x4E61 (loops)   Repeat ON'
            const auto& m = kN3Moves[it[i] - kItSeqBase];
            const bool rpt = MoveRepeating(m) && RepeatIsSeq(m.chr);
            char seq[72]; SeqText(m, seq, sizeof seq);
            add(sel ? cWhite : rpt ? cYellow : cGray, sel, "%s %-44s %.24s   Repeat %s", mk, seq, MoveName(m), rpt ? "ON" : "OFF");
        }
        else if (IsMoveItem(it[i])) {  // #14336 item 4: one 'Repeat ON / OFF' per row, 'Used ON / OFF' = the boss may pick it
            const auto& move = kN3Moves[it[i] - kItMoveBase];
            const int allowed = MoveAllowed(move);
            const bool rpt = MoveRepeating(move) && !RepeatIsSeq(move.chr);  // #14423: a sequence repeat shows on its Sequences row
            char msTxt[12] = "";  // user #13786: typical natural run time from the Training logs
            if (move.ms && !N3MoveProtected(move.id)) snprintf(msTxt, sizeof msTxt, "%u.%us", move.ms / 1000u, (move.ms % 1000u) / 100u);
            if (N3MoveProtected(move.id)) add(sel ? cWhite : cDim, sel, "%s 0x%04X  Idle / initialization (required, always used)", mk, move.id);
            else {
                // human #44361: LEFT '<N> : Anim <x.y>s' (N = running number in this group) + the player-facing id (1Hit3) and the 0x id when it fits;
                // a TAB starts the RIGHT column (EF host right-aligns it): the move type and one state marker [ON] / [OFF] / [REPEAT]. No colour
                // carries the state (the selected row is white), no 'static move / attack family' label text; the name is in the footer.
                int num = 0;
                for (int j = 0; j <= i; ++j) num += IsMoveItem(it[j]) && !N3MoveProtected(kN3Moves[it[j] - kItMoveBase].id);
                char lid[24], left[64], right[48];
                N3MoveLabelId(&move, lid, sizeof lid);
                snprintf(left, sizeof left, "%d : Anim %s", num, msTxt[0] ? msTxt : "?");
                if (lid[0]) { const size_t l = strlen(left); snprintf(left + l, sizeof left - l, "  %s", lid); }
                if (strlen(left) + 8 <= 30) { const size_t l = strlen(left); snprintf(left + l, sizeof left - l, "  0x%04X", move.id); }
                char type[32];
                MoveTypeTag(move, type, sizeof type);
                snprintf(right, sizeof right, "%s  %s", type, rpt ? "[REPEAT]" : allowed < 0 ? "[?]" : allowed ? "[ON]" : "[OFF]");
                add(sel ? cWhite : rpt ? cYellow : allowed == 0 ? cDim : cGray, sel, "%s %s\t%s", mk, left, right);
            }
        } else if (IsMoveCatItem(it[i])) {  // user #14291: 'Attacks   24 moves, 2 OFF, repeating 0x4E64   A: open'
            const int c = kItMoveCatBase - it[i];
            int off = 0, cnt = 0;
            const N3Move* rpt = nullptr;
            if (c == kMcSeq) { cnt = SeqCount(g_uiMoveChr); for (int j = 0; j < N3_MOVE_COUNT; ++j) if (kN3Moves[j].chr == g_uiMoveChr && SeqHead(g_uiMoveChr, kN3Moves[j].id) && MoveRepeating(kN3Moves[j])) rpt = &kN3Moves[j]; }
            else cnt = MoveCatCount(g_uiMoveChr, c, &off, &rpt);
            char extra[48] = "";
            if (off > 0 && rpt) snprintf(extra, sizeof extra, ", %d OFF, repeating 0x%04X", off, rpt->id);
            else if (off > 0) snprintf(extra, sizeof extra, ", %d OFF", off);
            else if (rpt) snprintf(extra, sizeof extra, ", repeating 0x%04X", rpt->id);
            add(sel ? cWhite : rpt ? cYellow : cGray, sel, "%s %-24s %3d %s%s%s   A: open", mk, CatName(c), cnt, c == kMcSeq ? "sequence" : "move", cnt == 1 ? "" : "s", extra);
        } else if (it[i] <= kItSettingBase) {
            const int index = kItSettingBase - it[i];
            const auto& d = kConfigSettings[index];
            const auto& v = g_configValues.values[(size_t)index];
            // #13296: "Label: ON" (pill), the edit keys as a hint, a short note only when it matters
            const bool manual = d.kind == setting_ui::Kind::Manual;
            add(sel ? cWhite : manual ? cDim : cGray, sel, "%s %.48s: %.44s%s%s", mk, d.label, setting_ui::Shown(d, v.text),
                manual ? "   edit in the ini file" : "   A / Left / Right", !manual && v.changed ? "   changed, restart to apply" : "");
        } else if (it[i] <= kItGroupBase) add(sel ? cWhite : cGray, sel, "%s %s   A: open", mk, kConfigGroupLabels[kItGroupBase - it[i]]);
        else if (it[i] == kItDeathCut || it[i] == kItDeathBlow || it[i] == kItDeathLoot) {
            int loot = 0, blow = 1, cut = 0;
            const bool ok = DeathCfgGet(loot, blow, cut);
            const int v = it[i] == kItDeathCut ? cut : it[i] == kItDeathBlow ? blow : loot;
            // #8827 toggle matrix: No finisher (Ki break) ON owns the HP floor (no finisher, kill after the 2 s window) -> 'Final blow required' and 'Cutscene on
            // kill' do not apply; 'Cutscene on kill' also needs 'Final blow required' ON. Shown dim with the reason (values stay saved).
            using G = int (*)();
            const auto gnf = (G)TrainingExport("Nioh3Training_GetNoFinisher");
            const bool nofin = gnf && gnf() > 0;
            const char* why = it[i] == kItDeathLoot ? "" : nofin ? "   (inactive: No grapple is ON)" : (it[i] == kItDeathCut && !blow) ? "   (inactive: final blow is OFF)" : "";
            add(sel ? cWhite : !ok || *why ? cDim : cGray, sel, "%s %-24s %s   A / Left / Right%s", mk,
                it[i] == kItDeathCut ? "Boss death animation:" : it[i] == kItDeathBlow ? "Require final blow:" : "Boss loot:", !ok ? "?" : v ? "ON" : "OFF", why);
        }
        else if (it[i] == kItReturnHome) {
            const int f = ReturnInfo();
            const bool can = f > 0 && (f & (1 | 8)) != 0;
            const char* tail = f < 0 ? "" : (f & 16) ? "   returning..." : !(f & 1) && !(f & 8) ? "   (nothing to return from)" : (f & 2) ? "   <- boss defeated: A takes you back"
                             : (f & 8) ? "   (cancels the arena switch)" : (f & 4) ? "   <- no boss here: A takes you back" : "";
            add(sel ? cWhite : (f > 0 && (f & (2 | 4)) && (f & 1)) ? cGreen : can ? cGray : cDim, sel, "%s Return to where I was%s", mk, tail);
        }
        else if (it[i] == kItAutoRetry) {
            using G = int (*)();
            const auto g = (G)TrainingExport("Nioh3Training_GetAutoRetry");
            const int v = g ? g() : -1;
            add(sel ? cWhite : cGray, sel, "%s %-24s %s   A / Left / Right", mk, "Auto retry on death:", v < 0 ? "?" : v ? "ON" : "OFF");
        }
        else if (it[i] == kItNoFin) {
            using G = int (*)();
            const auto g = (G)TrainingExport("Nioh3Training_GetNoFinisher");
            const int v = g ? g() : -1;
            add(sel ? cWhite : cGray, sel, "%s %-24s %s   A / Left / Right", mk, "No grapple in Ki break:", v < 0 ? "?" : v ? "ON" : "OFF");
        }
        else if (it[i] == kItFinT || it[i] == kItHp0W) {
            const int v = TuneGet(it[i] == kItFinT ? 0 : 1);
            using G = int (*)();
            const auto gnf = (G)TrainingExport("Nioh3Training_GetNoFinisher");
            const bool nofin = gnf && gnf() > 0;
            int loot = 0, blow = 1, cut = 0;
            DeathCfgGet(loot, blow, cut);
            const char* why = it[i] == kItFinT ? (nofin ? "   (inactive: No grapple is ON)" : !blow ? "   (inactive: final blow is OFF)" : !cut && !loot ? "   (needs death animation or loot)" : "")
                                               : (!nofin ? "   (needs No grapple ON)" : "");
            char val[24];
            if (v < 0) snprintf(val, sizeof(val), "?");
            else snprintf(val, sizeof(val), "%d.%d s", v / 1000, (v % 1000) / 100);
            add(sel ? cWhite : v < 0 || *why ? cDim : cGreen, sel, "%s %-24s %-7s   Left / Right%s", mk,
                it[i] == kItFinT ? "Final-blow timeout:" : "Zero-HP combo timeout:", val, why);
        }
        else if (it[i] == kItFightAgain) add(sel ? cWhite : cGreen, sel, "%s %s", mk, GateMode() == 2 ? "Fight again / Reset round (same boss)" : "Fight again (same boss, this arena)");
        else if (it[i] == kItFightOpts) add(sel ? cWhite : cGray, sel, "%s Death and loot rules   A: open", mk);  // user #13265
        else if (it[i] == kItMoves) add(sel ? cWhite : cGray, sel, "%s Boss moves and hit counts   A: open", mk);
        else if (it[i] == kItMovesA || it[i] == kItMovesB) {
            const int slot = it[i] == kItMovesB;
            const uint32_t c = SlotChr(slot);
            add(sel ? cWhite : c ? cGray : cDim, sel, "%s Boss %c moves: %s", mk, 'A' + slot, SlotBossName(c));
        }
        else if (it[i] == kItDespawnAll) {  // #14336 item 6: the bosses actually spawned
            char list[80] = ""; size_t o = 0;
            for (int sl = 0; sl < SlotCountUi() && o < sizeof list; ++sl) if (SlotChr(sl)) o += (size_t)snprintf(list + o, sizeof list - o, "%s%c: %.24s", o ? ", " : "", 'A' + sl, SlotBossName(SlotChr(sl)));
            add(sel ? cWhite : cGray, sel, "%s Remove all summoned bosses (%s)", mk, list);
        }
        else if (it[i] >= kItSummonRoster && it[i] < kItSummonRosterEnd) { char nm[64]; RosterEntry(it[i] - kItSummonRoster, nm, sizeof nm); add(sel ? cWhite : cGreen, sel, "%s %s", mk, nm); }
        else if (it[i] >= kItSummonA && it[i] < kItSummonEnd) {
            const int slot = it[i] >= kItSummonB ? 1 : 0;
            const int row = it[i] - (slot ? kItSummonB : kItSummonA);
            if (g_uiPages.page == uSummon && row >= 0 && (size_t)row < g_rows.size()) add(sel ? cWhite : cGreen, sel, "%s %s", mk, g_rows[(size_t)row].name);
            else add(sel ? cWhite : cGreen, sel, "%s   Summon as Boss %c (now: %s)", mk, 'A' + slot, SlotBossName(SlotChr(slot)));
        }
        else if (it[i] == kItRootRematch) add(sel ? cWhite : cGray, sel, "%s Boss Matches   A: open", mk);
        else if (it[i] == kItRootTraining) add(sel ? cWhite : cGray, sel, "%s Training   A: open", mk);
        else if (it[i] == kItLeaveArena) add(sel ? cWhite : cGreen, sel, "%s End stuck arena session", mk);
        else if (it[i] == kItTrainHereOpen) add(sel ? cWhite : cGray, sel, "%s Train here   A: open", mk);
        else if (it[i] == kItArenas) add(sel ? cWhite : cGray, sel, "%s Train in a boss arena   A: open", mk);
        else if (it[i] == kItBossSumm) add(sel ? cWhite : cGray, sel, "%s Summon a boss here   A: open", mk);
        else if (it[i] == kItPlayerOpen) {  // user #14267
            const int v = CantDieGet();
            add(sel ? cWhite : cGray, sel, "%s Player settings (infinite health: %s)   A: open", mk, v < 0 ? "?" : v ? "ON" : "OFF");
        }
        else if (it[i] >= kItTrainHere && it[i] < kItTrainHereEnd) {
            const int row = it[i] - kItTrainHere;
            add(sel ? cWhite : g_status[(size_t)row].load() == 2 ? cGreen : cGray, sel, "%s %s", mk, g_rows[(size_t)row].name);
        }
        else if (it[i] == kItRootSummon) add(sel ? cWhite : cGray, sel, "%s Summon a boss here   A: open", mk);
        else if (it[i] == kItRootTools) { const int nb = SpawnedCount(); add(sel ? cWhite : nb ? cGreen : cGray, sel, "%s Active bosses (%d boss%s)   A: open", mk, nb, nb == 1 ? "" : "es"); }
        else if (it[i] == kItSumPassive) {
            const int v = SumPassiveGet();
            char val[24];
            if (v < 0) snprintf(val, sizeof(val), "?");
            else snprintf(val, sizeof(val), "%d s", v / 1000);
            add(sel ? cWhite : v < 0 ? cDim : cYellow, sel, "%s Wait before attacking: %-5s  Left / Right", mk, val);
        }
        else if (it[i] == kItSumSlot)
            add(sel ? cWhite : cGray, sel, "%s Summon into: Boss %c (now: %.20s)", mk, 'A' + g_sumSlot, SlotBossName(SlotChr(g_sumSlot)));
        else if (it[i] == kItIdleHold) {
            const int hold = IdleHoldStatus();
            add(sel ? cWhite : hold == 2 ? cGreen : hold ? cYellow : cGray, sel,
                "%s Boss stance: %s   A / Left fight / Right stand still", mk,  // user #13529: no 'CPU' / 'IDLE' jargon
                hold == 2 ? (RepeatingMove() ? "Stand still + drill" : "Stand still (idle)") : hold == 1 ? "Stand still (getting ready)" : hold < 0 ? "Fight normally (stand still failed)" : "Fight normally");
        }
        else if (it[i] == kItRepNow) {  // user #13293 / #13618: names the move and says what the boss does with it
            const N3Move* m = RepeatingMove();
            char what[96];
            if (m) RepeatWhat(*m, what, sizeof what);
            if (m) add(sel ? cWhite : cYellow, sel, "%s Repeating move: 0x%04X %.24s   %s", mk, m->id, MoveName(*m), what);
            else add(sel ? cWhite : cGray, sel, "%s Repeating move: none   A on a move starts a repeat", mk);
        }
        else if (it[i] == kItRepSel) {  // user #13618: explicit row, A repeats the move shown (no guessing Right)
            const N3Move* m = RepeatCandidate();
            if (m) add(sel ? cWhite : cGreen, sel, "%s Repeat selected move: 0x%04X %.36s   press A", mk, m->id, MoveName(*m));
            else add(sel ? cWhite : cDim, sel, "%s Repeat selected move: none   choose a move first", mk);
        }
        else if (it[i] == kItAggro) {  // user #13787
            const int v = AggroGet();
            add(sel ? cWhite : v < 0 ? cDim : cGray, sel, "%s Follow and target me: %s   A / Left / Right", mk, v < 0 ? "?" : v ? "ON" : "OFF");
        }
        else if (it[i] == kItCantDie) {  // user #14150
            const int v = CantDieGet();
            add(sel ? cWhite : v < 0 ? cDim : cGray, sel, "%s Infinite health: %s   A / Left / Right", mk, v < 0 ? "?" : v ? "ON" : "OFF");
        }
        else if (it[i] == kItResetPos) add(sel ? cWhite : cGray, sel, "%s Reset boss position   press A", mk);  // user #13787
        else if (it[i] == kItShowTw) {  // user #13786
            int tw = 0;
            for (int j = 0; j < N3_MOVE_COUNT; ++j) if (kN3Moves[j].chr == g_uiMoveChr && N3MoveTwitch(&kN3Moves[j])) ++tw;
            add(sel ? cWhite : cGray, sel, "%s Show non-attack moves: %s (%d)   A / Left / Right", mk, g_showTwitch ? "ON" : "OFF", tw);
        }
        else if (it[i] == kItRepGap) {
            const int v = RepGapGet();
            char val[24];
            if (v < 0) snprintf(val, sizeof(val), "?");
            else snprintf(val, sizeof(val), "%d.%02d s", v / 1000, (v % 1000) / 10);
            add(sel ? cWhite : v < 0 ? cDim : cYellow, sel, "%s Repeat delay: %-7s   Left / Right", mk, val);
        }
        else if (it[i] == kItSettings) add(sel ? cWhite : cGray, sel, "%s Startup settings   A: open", mk);
        else if (it[i] == kItBrowse) add(sel ? cWhite : cGray, sel, "%s Boss Matches: browse bosses   A / Enter opens", mk);
        else if (it[i] == kItPractice) add(sel ? cWhite : cGray, sel, "%s Yamagata mission replay   A: open", mk);
        else if (it[i] == kItPracticeStart) add(sel ? cWhite : cGray, sel, "%s Replay from the boss shrine", mk);
        else if (it[i] == kItPracticeReturn) add(sel ? cWhite : g_practiceOwned.load() ? cGray : cDim, sel, "%s End mission replay", mk);
        else if (it[i] == kItBack) add(sel ? cWhite : cGray, sel, "%s Back   B / Esc / Backspace", mk);
        else if (it[i] == kItClose) add(sel ? cWhite : cGray, sel, "%s Close menu", mk);
        else if (it[i] == kItOpen) add(sel ? cWhite : cGray, sel, "%s Open Battle Scroll replays", mk);
        else if (it[i] == kItScroll) add(sel ? cWhite : cGray, sel, "%s Play first owned Picture Scroll", mk);
        else if (it[i] == kItAbort) add(cYellow, sel, "%s Cancel pending replay", mk);
        else if (it[i] >= kItTeleport) {
            const int row = it[i] - kItTeleport;
            const char* why = CanArena(row);
            const uint8_t stt = g_status[(size_t)row].load();
            if (g_uiPages.page == uTraining)  // user #13164: Training arena list (details in the footer)
                add(sel ? cWhite : (stt != 2 && !AnyBossOk(row)) || (stt == 2 && why) ? cDim : cGreen, sel, "%s Arena: %s", mk, g_rows[(size_t)row].name /* ally #89: full name, the host panel word-wraps (58 cols) */);
            else if (stt != 2) add(sel ? cWhite : cDim, sel, AnyBossOk(row) ? "%s    Train here (boss not cleared; no mission progress)" : "%s    Teleport to arena (fast load)   - not beaten on this journey", mk);
            else if (why) add(sel ? cWhite : cDim, sel, "%s    Teleport to arena (fast load)   - %s", mk, why);
            else add(sel ? cWhite : cGreen, sel, "%s    Train in this arena (boss starts standing still)", mk);
        } else if (it[i] >= kItShrine) {
            const Row& r = g_rows[(size_t)(it[i] - kItShrine)];
            const uint8_t stt = g_status[(size_t)(it[i] - kItShrine)].load();
            const char* why = CanShrine(it[i] - kItShrine);
            const uint32_t c = stt == 2 && !why ? (!strcmp(r.fightConf, "high") ? cGreen : !strcmp(r.fightConf, "medium") ? cYellow : cOrange) : cDim;
            if (stt != 2) add(sel ? cWhite : cDim, sel, "%s    Shrine before the boss   - not beaten on this journey", mk);
            else if (why) add(sel ? cWhite : c, sel, "%s    Shrine before the boss   - %s", mk, why);
            else add(sel ? cWhite : c, sel, "%s    Shrine before the boss   %s, %d m   [%s]", mk, r.fightKind, r.fightDist, r.fightConf);
        } else if (it[i] >= kItFight) {
            const int row = it[i] - kItFight;
            const Row& r = g_rows[(size_t)row];
            const uint8_t stt = g_status[(size_t)row].load();
            const char* why = CanArena(row);
            const char* action = "Fight now";
            const char* conf = r.atFight ? r.fightConf : r.arenaConf;
            const uint32_t c = why || stt != 2 ? cDim : (!strcmp(conf, "high") || !strcmp(conf, "captured")) ? cGreen : !strcmp(conf, "medium") ? cYellow : cOrange;
            if (stt != 2 && AnyBossOk(row)) add(sel ? cWhite : cGreen, sel, "%s    Train here (boss not cleared; no mission progress)", mk);
            else if (stt != 2) add(sel ? cWhite : cDim, sel, "%s    %s   - not beaten on this journey", mk, action);
            else if (why) add(sel ? cWhite : c, sel, "%s    %s   - %s", mk, action, why);
            else add(sel ? cWhite : c, sel, "%s    %s   %s   [%s]", mk, action, r.atFight ? "the mission starts at the fight" : "warp into the trigger", conf);
        } else {
            const Row& r = g_rows[(size_t)it[i]];
            const uint8_t stt = g_status[(size_t)it[i]].load();
            const char* status = r.route == rcore::kX ? "-" : r.route == rcore::kE ? "" : !scanned ? "?" : stt == 2 ? "Replay" : stt == 1 ? (AnyBossOk(it[i]) ? "Train here" : "Not cleared") : "error";
            const uint32_t c = r.route == rcore::kX ? cDim : (r.route != rcore::kE && stt != 2 && !AnyBossOk(it[i])) ? cDim : !r.Verified() ? cGray : cGreen;
            if (g_uiPages.page == uActions)
                add(sel ? cWhite : c, sel, "%s Replay mission: %s   %s %s", mk, r.name, rcore::RouteName(r.route), r.Verified() ? "" : "unverified");
            else
                add(sel ? cWhite : c, sel, "%s %-40.40s %-10.10s %-10.10s %-14.14s %s", mk, r.name, r.region, status, rcore::RouteName(r.route),
                    r.route == rcore::kX ? "" : r.Verified() ? "" : "unverified");
        }
    }
    if (pinBack && k < cap) add(g_sel == n - 1 ? cWhite : cGray, g_sel == n - 1, "%s Back   B / Esc / Backspace", g_sel == n - 1 ? ">" : " ");
    if (footer && nf) {
        footer[0] = 0;
        const int item = g_sel < 0 ? kItClose : n ? it[g_sel] : kItOpen;
        if (const char* bvf = BvFooter(item)) snprintf(footer, nf, "%s", g_msg[0] ? g_msg : bvf);  // user #14320
        else if (const char* psf = PstFooter(item)) snprintf(footer, nf, "%s", g_msg[0] ? g_msg : psf);  // user #22389
        else if (IsBossOpenItem(item)) snprintf(footer, nf, "%s", g_msg[0] ? g_msg : "Training and spawned Boss Matches: behavior, repeat delay, moves, and health for this boss. Shared stat controls are labeled separately.");
        else if (item == kItSumAnother) snprintf(footer, nf, "%s", g_msg[0] ? g_msg : "Add a boss beside you. During Training it waits; otherwise it fights normally. Change Behavior under Active bosses.");
        else if (item == kItStanceTimer) snprintf(footer, nf, "%s", g_msg[0] ? g_msg : "Training and spawned Boss Matches: switch all active bosses after 1-10 seconds. If any boss is waiting, all will fight; otherwise all will wait. A starts or cancels.");
        else if (item == kItBossStance) snprintf(footer, nf, "%s", g_msg[0] ? g_msg : "This boss only: Stand still waits between repeats. Fight normally uses its own attacks. A toggles; Left fights; Right waits.");
        else if (item == kItBossAggro) snprintf(footer, nf, "%s", g_msg[0] ? g_msg : "ON: this boss targets you and approaches within 5 m before repeating a move. Also saves the default for newly added bosses.");
        else if (item == kItBossGap) snprintf(footer, nf, "%s", g_msg[0] ? g_msg : "Delay after this boss repeats a move or chain. Left / Right changes it by 0.25 s. Saved for this boss type.");
        else if (item == kItBossReset) snprintf(footer, nf, "%s", g_msg[0] ? g_msg : "A: puts this boss back where it first appeared, facing you.");
        else if (item == kItBossRemove) snprintf(footer, nf, "%s", g_msg[0] ? g_msg : "A: removes this boss (and its health bar). The other bosses stay.");
        else if (IsSeqItem(item)) snprintf(footer, nf, "%s", g_msg[0] ? g_msg : "A toggles repetition of this move chain. The game plays its linked moves, then waits for Repeat delay. Left stops; Right repeats.");
        else if (IsMoveItem(item)) {
            using StatusFn = int (*)();
            const auto status = (StatusFn)TrainingExport("Nioh3Training_GetMoveHookStatus");
            const int hook = status ? status() : 0;
            const int allowed = MoveAllowed(kN3Moves[item - kItMoveBase]);
            snprintf(footer, nf, "%s", g_msg[0] ? g_msg : hook < 0 ? "Move hooks unavailable: OFF switches cannot veto actions and repeat works only in Stand still." :
                !hook && allowed == 0 ? "Saved OFF; activation is pending until the game resumes. A: repeat this move." :
                g_uiPages.page == uMoveCat && g_moveCat == kMcKiBreak ?  // user #14291
                "A toggles repeat; Left blocks; Right allows. Seen live means observed during a Ki break; other entries are related animations." :
                "A toggles repeat (one per boss). Left blocks the move; Right allows it. Saved by boss type. (?) means the name is unverified.");
        } else if (IsMoveCatItem(item)) {  // user #14291
            snprintf(footer, nf, "%s", g_msg[0] ? g_msg : kItMoveCatBase - item == kMcSeq ? "Linked move chains. Open the group, then press A to repeat a chain." : N3MoveCatHelp(kItMoveCatBase - item));
        } else if (item == kItIdleHold) {
            const int hold = IdleHoldStatus();
            snprintf(footer, nf, "%s", hold < 0 ? "The boss could not be made to stand still (it was busy). Right tries again once it settles." :
                g_msg[0] ? g_msg : hold == 2 && DrillState() == 2 ? "Stand still + drill: the boss AI is on for the drill (forced moves did not play), it only uses the repeating move. Left: fight normally." :
                hold == 2 && RepeatingMove() ? "Stand still + drill: the boss waits and does only the repeating move, once every gap. Left: fight normally (own attacks)." :
                hold == 2 ? "Stand still: the boss waits and does not attack (hits still make it flinch). Pick a move to repeat below for a drill. Left: fight normally." :
                hold == 1 ? "Getting the boss to stand still at its next safe moment (its current move may finish). Left: fight normally." :
                "Fight normally: the boss uses its own attacks. Right: stand still (Training starts like that). A repeating move is used whenever it attacks.");
        } else if (item == kItRepGap) {
            snprintf(footer, nf, "%s", g_msg[0] ? g_msg : "How long the boss waits after each repeat of its repeating move before doing it again. Left / Right 0.25 s. Applies now, saved.");
        } else if (item == kItShowTw) {  // user #13786
            snprintf(footer, nf, "%s", g_msg[0] ? g_msg : "OFF: only real attacks / wind-ups are listed. ON: also twitches (short state changes, ~0.05-0.15 s when forced), combo links and steps. Kinds come from Training logs.");
        } else if (item == kItAggro) {  // user #13787
            snprintf(footer, nf, "%s", g_msg[0] ? g_msg : "ON: the boss keeps you as its target and walks up to you, then does the repeating move at you. Works in both stances (Stand still: it walks up when you are out of range (5 m), then drills). Saved.");
        } else if (item == kItCantDie) {  // user #14150
            snprintf(footer, nf, "%s", g_msg[0] ? g_msg : "Training and Boss Matches: ON refills your health. OFF allows damage. Training and arena Boss Matches retry in place after defeat. Saved.");
        } else if (item == kItResetPos) {
            snprintf(footer, nf, "%s", g_msg[0] ? g_msg : "A: puts the boss back where it first appeared in this Training session (summoned bosses: where they appeared), facing you.");
        } else if (item == kItRepNow) {  // user #13293
            snprintf(footer, nf, "%s", g_msg[0] ? g_msg : "The move the boss repeats. Stand still: it waits and does only this move every gap (drill). Fight normally: it uses this move whenever it attacks. A stops.");
        } else if (item == kItRepSel) {  // user #13618
            snprintf(footer, nf, "%s", g_msg[0] ? g_msg : "A: the boss repeats the move shown. Or press A on any move inside a group.");
        } else if (item == kItDespawnAll) {
            snprintf(footer, nf, "%s", g_msg[0] ? g_msg : "Removes every boss you summoned (all letters) and their health bars.");
        } else if (item == kItMovesA || item == kItMovesB) {
            snprintf(footer, nf, "%s", g_msg[0] ? g_msg : "Move switches for the boss in this slot. Each boss keeps its own list. A opens, B returns.");
        } else if (item >= kItSummonA && item < kItSummonRosterEnd) {
            snprintf(footer, nf, "%s", g_msg[0] ? g_msg : "Add this boss beside you in the next free slot. During Training it waits; otherwise it fights normally. A: summon.");
        } else if (item == kItSumPassive) {
            snprintf(footer, nf, "%s", g_msg[0] ? g_msg : "Seconds a summoned boss waits (AI off) before it attacks. 0 = at once. Left / Right 1 s. Applies to the next summon, saved.");
        } else if (item == kItSumSlot) {
            snprintf(footer, nf, "%s", g_msg[0] ? g_msg : "Old slot picker (not shown any more): summoned bosses take the next free letter.");
        } else if (item == kItRootRematch) {
            snprintf(footer, nf, "Boss Matches: replay a cleared boss in its arena or from its shrine. Death and loot rules are shared with Training.");
        } else if (item == kItLeaveArena) {
            snprintf(footer, nf, "Recovery for an empty, stuck arena: end boss mode so map and shrine travel work again.");
        } else if (item == kItRootTraining) {
            snprintf(footer, nf, "Training: choose a location, add bosses, and control their moves. Shared player and boss stats also affect spawned Boss Matches. Meter and saves: Mod menu.");
        } else if (item == kItTrainHereOpen) {
            snprintf(footer, nf, "Training only: choose a boss to spawn here, standing still. No arena load. If you die, the round restarts here.");
        } else if (item == kItArenas) {
            snprintf(footer, nf, "Training only: move to a boss arena with its boss standing still. If you die, retry in the arena.");
        } else if (item == kItPlayerOpen) {  // user #14267
            snprintf(footer, nf, "%s", "Player health, Ki, ninjutsu, damage, and animation speed. Stat changes apply in Training and spawned Boss Matches; check each setting for details.");
        } else if (item == kItBossSumm) {
            snprintf(footer, nf, "Add bosses beside you without loading an arena. They wait during Training; otherwise they fight normally. Change Behavior under Active bosses.");
        } else if (item >= kItTrainHere && item < kItTrainHereEnd) {
            snprintf(footer, nf, "%s", g_msg[0] ? g_msg : "Training only: start with this boss here. Active bosses controls behavior and repeats. Return to where I was ends the session.");
        } else if (item == kItRootTools) {
            snprintf(footer, nf, "Active bosses in Training and spawned Boss Matches. Open a boss to change its behavior, health, Ki, or moves. Add or remove bosses here.");
        } else if (item == kItFightOpts) {  // user #13265
            snprintf(footer, nf, "Shared by Training and spawned Boss Matches: death animation, final blow, loot, and grapple rules. Boss Matches start with loot and No grapple OFF.");
        } else if (item == kItSettings) {
            snprintf(footer, nf, "Advanced startup settings. Changes are saved immediately and take effect after restarting the game.");
        } else if (item == kItRootSummon) {
            snprintf(footer, nf, "Add a boss beside you without loading. It waits during Training; otherwise it fights normally. Change Behavior under Active bosses.");
        } else if (IsMoveItem(item) && !g_msg[0]) {  // human #44361: the row has no name text any more; the selected move's name + id here
            const auto& m = kN3Moves[item - kItMoveBase];
            char tag[32];
            N3MoveHitTag(&m, tag, sizeof tag);
            snprintf(footer, nf, "0x%04X  %.60s%s%s. A: repeat | Left: block | Right: allow.", m.id, MoveName(m), tag[0] ? ", " : "", tag);
        } else if (item == kItMoves || g_uiPages.page == uMoves || g_uiPages.page == uMoveCat) {
            snprintf(footer, nf, "%s", g_msg[0] ? g_msg : "Moves grouped by hit count and type. A toggles repeat; Left blocks a move; Right allows it. Hit labels describe available data, not guaranteed damage taken.");
        } else if (item <= kItSettingBase) {
            const auto& d = kConfigSettings[kItSettingBase - item];
            snprintf(footer, nf, "[%s] %s: %s %s", d.section, d.key, d.help,
                d.kind == setting_ui::Kind::Manual ? "INI only; no controller edit." : g_configSaveState.load() == 3 ? "Auto-save FAILED; the next edit retries." :
                "A / Left / Right edits, saved at once; applies after a restart.");
        } else if (g_msg[0]) snprintf(footer, nf, "%s", g_msg);
        else if (item == kItPractice || g_uiPages.page == uPractice) snprintf(footer, nf, "Normal rewards, drops and progression apply. Walk to the arena; death uses the native retry. No automatic restart. A selects / B back.");
        else if (item <= kItGroupBase) snprintf(footer, nf, "Startup preferences. Every change is saved at once and applies after a game restart.");
        else if (item == kItBrowse) snprintf(footer, nf, "Choose a boss, then Fight now or Shrine before the boss. Existing unlock and solo checks apply.");
        else if (item == kItBack) snprintf(footer, nf, "Return one level. B cancels a confirmation first; B at the root closes.");
        else if (item == kItDeathCut) snprintf(footer, nf, "Training / spawned Boss Matches: play the boss death animation after one final blow. No story cutscene. Needs Require final blow ON and No grapple OFF. Saved.");
        else if (item == kItDeathBlow) snprintf(footer, nf, "Training / spawned Boss Matches: ON requires a final blow at zero HP; OFF kills at zero HP. No grapple overrides this rule. Saved.");
        else if (item == kItDeathLoot) snprintf(footer, nf, "Training / spawned Boss Matches: drop boss loot and Amrita, with one final blow. No first-clear rewards. Boss Matches start this OFF; you can enable it during the match.");
        else if (item == kItReturnHome) snprintf(footer, nf, "Ends the arena session (boss removed) and puts you back where you stood before it started, in that world. Works after a kill, with no boss, or mid-switch.");
        else if (item == kItAutoRetry) snprintf(footer, nf, "Training and arena Boss Matches retry in place after defeat. Auto retry is always enabled.");
        else if (item == kItNoFin) snprintf(footer, nf, "Training / spawned Boss Matches: block grapples during Ki break. At zero HP, keep attacking until the combo timeout expires. Boss Matches start this OFF.");
        else if (item == kItFinT) snprintf(footer, nf, "Death animation / Boss loot: kill when the first final-blow animation ends, or after this timeout. Left / Right: 0.5 s. Saved.");
        else if (item == kItHp0W) snprintf(footer, nf, "With No grapple ON, a zero-HP boss dies after this long without a hit. Left / Right: 0.1 s. Shared by Training and spawned Boss Matches. Saved.");
        else if (item == kItFightAgain) snprintf(footer, nf, "Respawns the same boss here and resets the round (no load). B / Close keeps you at the gate.");
        else if (item == kItClose) snprintf(footer, nf, "Close the mod menu and return to the game.");
        else if (item >= kItTeleport) {
            const char* why = CanArena(item - kItTeleport);
            if (g_status[(size_t)(item - kItTeleport)].load() != 2) snprintf(footer, nf, AnyBossOk(item - kItTeleport) ? "Boss not cleared: A starts Training with this boss here, without mission progress. B: back." : "Not beaten on this journey: arenas open for bosses you have beaten. B back.");
            else if (why) snprintf(footer, nf, "%s. B back.", why);
            else snprintf(footer, nf, "Training only: move to this arena with its boss standing still. Moving between worlds may take longer. A: start / B: back.");
        } else if (item >= kItShrine) {
            const Row& r = g_rows[(size_t)(item - kItShrine)];
            const char* why = CanShrine(item - kItShrine);
            if (why) snprintf(footer, nf, "%s. B back.", why);
            else snprintf(footer, nf, "%s: starts the mission at start point %u (%s, %d m from the boss, %s confidence). A start, B back.",
                     r.mission[0] ? r.mission : r.name, r.fightStart, r.fightKind, r.fightDist, r.fightConf);
        } else if (item >= kItFight) {
            const Row& r = g_rows[(size_t)(item - kItFight)];
            const char* why = CanArena(item - kItFight);
            if (why) snprintf(footer, nf, "%s. B back.", why);
            else if (r.atFight) snprintf(footer, nf, "%s: the mission's only start point is at the fight (%d m). A start, B back.", r.mission[0] ? r.mission : r.name, r.fightDist);
            else snprintf(footer, nf, "%s: start point %u, then a warp to %.0f %.0f %.0f inside the arena trigger (%s). A start, B back.",
                          r.mission[0] ? r.mission : r.name, r.fightStart, r.arena[0], r.arena[1], r.arena[2], r.arenaSrc);
        } else if (item >= 0) {
            const Row& r = g_rows[(size_t)item];
            if (g_uiPages.page == uBosses || g_uiPages.page == uRematch) snprintf(footer, nf, "%s: A opens boss actions. Boss %d of %d; LB/RB or PgUp/PgDn skips ten rows.", r.name, g_sel + 1, n - 1);
            else if (r.route == rcore::kX) snprintf(footer, nf, "No route: %s", r.reason);
            else snprintf(footer, nf, "%s - %s%s. A start, B back.", r.mission[0] ? r.mission : r.name, rcore::RouteName(r.route),
                          r.placement ? "" : "");
        } else if (item == kItOpen) snprintf(footer, nf, "Opens the game's own replay list (whole mission). Best at a shrine, menus closed.");
        else if (item == kItScroll) snprintf(footer, nf, "Starts the first Picture Scroll in your inventory exactly as the item menu does (unverified).");
        else snprintf(footer, nf, "Restores everything now (only before the mission has loaded).");
        if (g_sel == -2) snprintf(footer, nf, "Jump to the mod menu's top-level page. B goes back one page; Close (next row) closes the menu.");
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
        snprintf(reply, n, "pong Nioh3BossRematch %s live=%d", ADDON_VERSION, g_live.load());
        return;
    }
    if (!g_live) {
        snprintf(reply, n, "error rematch not live (see Nioh3BossRematch.log)");
        return;
    }
    if (!strcmp(cmd, "cover")) {
        CoverLock lock;if(!lock.held){snprintf(reply,n,"error cover busy");return;}
        snprintf(reply,n,"token=%llu phase=%u attempt=%llu mission=0x%X start=%u placement=%u presented=%d issued=%d bossEntry=%d placementEntry=%d deadline=%llu control=%d algo=%d life=%d playerAlive=%d sample=%llu row=%s observedActor=%u reason=%s",
            g_cover.token,(unsigned)g_cover.phase,g_cover.attempt,g_cover.mission,g_cover.start,g_cover.placement,
            (int)g_cover.presented,(int)g_cover.issued,(int)g_cover.bossEntry,(int)g_cover.placementEntry,g_cover.deadline,(int)g_coverControl,(int)g_coverAlgo,(int)g_coverLife,(int)g_coverPlayer,g_coverSampleAt,g_cover.row<g_rows.size()?g_rows[g_cover.row].id:"-",g_cover.observedPlacement,g_cover.reason);
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
        float pp[4] = {};
        const bool pok = PlayerPos(pp);
        snprintf(reply, n, "state=%d row=%s fights=%ld infight=%ld emaki=%d loading=%u player=%d stage=%u mission=0x%X fnow=%s warps=%d loadms=%u spawn=%s pos=%.0f,%.0f,%.0f gate=%s "
                 "placements=[%s] last=%s",
                 g_state.load(), g_activeRow.load() >= 0 ? g_rows[(size_t)g_activeRow.load()].id : "-", fight::g_fights, fight::g_active, s.emaki,
                 s.st[2], s.player, s.ms14, s.ms28, FnName(g_fn.load()), g_warps, g_loadMs.load(), g_sp.last, pok ? pp[0] : 0.f, pok ? pp[1] : 0.f, pok ? pp[2] : 0.f, gate ? gate : "ok", pl, last);
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
                const size_t k = strlen(reply);
                snprintf(reply + k, n - k, " arena=%d atfight=%d arenaconf=%s", g_rows[i].arenaOk, g_rows[i].atFight, g_rows[i].arenaConf[0] ? g_rows[i].arenaConf : "-");
                return;
            }
        snprintf(reply, n, "error no row %s", arg);
        return;
    }
    if (!strcmp(cmd, "leave")) {  // user 20261018: stuck in an arena with no mission: end the session + native boss mode (then map / shrine travel)
        const int rc = LeaveArena("pipe");
        snprintf(reply, n, rc > 0 ? "queued: boss mode ends on the next game tick; then open the map and travel to a shrine" : rc < 0 ? "already queued" : "error Nioh3Training_LeaveArena missing");
        return;
    }
    if (!strcmp(cmd, "exit")) {  // leave the running mission (MissionRequestReturn, as the mission menu does)
        Post(kReqExit, -1, "pipe");
        snprintf(reply, n, "queued");
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
    if (!strcmp(cmd, "fight") || !strcmp(cmd, "shrine")) {  // fight <id>: Fight now (in the arena); shrine <id>: the start point alone
        const bool arena = !strcmp(cmd, "fight");
        for (size_t i = 0; i < g_rows.size(); ++i)
            if (!strcmp(g_rows[i].id, arg)) {
                if (const char* w = arena ? CanArena((int)i) : CanFight((int)i)) {
                    snprintf(reply, n, "refused %s", w);
                    return;
                }
                Post(arena ? kReqFight : kReqShrine, (int)i, "pipe");
                snprintf(reply, n, "queued %s %s start=%u", g_rows[i].id, cmd, g_rows[i].fightStart);
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
    snprintf(reply, n, "error unknown command (ping status scan row start fight shrine exit abort open scroll)");
}

}  // namespace rematch
}  // namespace addon

// user #13274: Nioh3Training pauses the game while our mod menu is open in a Boss Match. 1 while a Boss Match replay mission, a Fight now
// start or a native practice attempt is running (read from Nioh3Training's menu-pause thread; atomics only).
extern "C" __declspec(dllexport) int Nioh3BossRematch_InMatch() {
    namespace r = addon::rematch;
    return r::g_replayIn.load() || r::g_fight.load() || r::g_practiceOwned.load() ? 1 : 0;
}
