// rematch.cpp - the boss rematch engine of Nioh3BossRematch (notes\boss-rematch-addon.md, notes\rematch-all-bosses.md).
// Routes (catalogue column `route`):
//   R  listable Battle Scroll mission: R-a = the game's replay menu (MenuOpenMissionReplay(0,0,6)), R-b = MissionRequestById(id,
//      slot byte, 0, params) = the shrine replay confirm (session 2026-10-08). R-b only for a mission cleared on this journey.
//   H  hidden duel: R-b direct (no patch needed), or R-a with the reverted un-hide ([Rematch] HiddenDuels = 1).
//   D  unlisted shipped record (master duels): R-b direct, cleared-on-this-journey only.
//   E  one of the 20 Waves rows (Picture Scroll records, mission 0xCC96). DEV default searches a key seed and does not
//      rewrite the emaki map (scrollctl noseedpick turns that rewrite back on). After the load, one shared warp walks the
//      live record point, then x±600, z±600, then those four directions at 1500, until wave state is 3. State 1 after that
//      ends the wave fight. These rows are not boss fights and do not arm a boss bar. Training arenas do not pick them.
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
#include "gamerva.h"
#include "native_entry_gates_gen.h"
#include "native_entry_waves_gen.h"
#include "staged_start.h"
#include "rematch_core.h"
#include "transition_cover.h"
#include "transition_native_read.h"
#include "../../menu_navigation.h"
#include "../../boss_moves.h"
#include "../../move_catalog.h"  // pc2 #23907: live move catalogue; the runtime list comes from Nioh3Training_AutoMoves
#include "move_cats.h"
#include "../../settings/store.h"

#include <atomic>
#include <set>
#include <new>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace addon {
namespace rematch {
void SaveUiSettings(); // page-default dispatch is defined before the persistence worker
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

// BAG: the call at 1E0C0BD leaves rcx as the shrine menu. 22D2454 ignores that rcx. When it returns ready and the
// reopen flag is set, count item 1E82 in the bag: the list builds its rows from that item. Count 0 clears the flag
// before the cmp at 1E0C0CA, so the else opens a menu that takes buttons. Any other count leaves the game's list.
// #78739: the 6E43B0 call read a garbage count (2730937083, board #332), so the bag is read directly.
int CountBagItem(uint16_t want);
using ScrollReadyFn = uint8_t (*)(uint64_t);
ScrollReadyFn g_scrollReadyOrig = nullptr;
uint8_t ScrollReturnReadyHook(uint64_t menu) {
    const uint8_t ready = g_scrollReadyOrig ? g_scrollReadyOrig(menu) : 0;
    if (!ready || menu < 0x10000) return ready;
    uint8_t flag = 0;
    if (!Rd(menu + 0x430, flag) || flag == 0) return ready;
    const int bag = CountBagItem(0x1E82);
    const uint32_t count = bag < 0 ? 0xFFFFFFFFu : (uint32_t)bag;
    if (count == 0xFFFFFFFFu) {
        Log("Scroll return: picture-scroll count unreadable, list stays");
        return ready;
    }
    Log("Scroll return: picture-scroll count %u, %s", count, count == 0 ? "shrine menu" : "list stays");
    if (rcore::CloseScrollListWhenEmpty(count, flag)) {
        const uint8_t zero = 0;
        WrBytes(menu + 0x430, &zero, 1);
    }
    return ready;
}

static uint32_t EntryGateChoice(uint32_t mission, uint32_t placement, uint32_t legacyGate,
                                bool door, bool atFight, bool training, bool warp, bool available) {
    if (!warp || !available) return 0;
    // Training moves the player; only request an observed script right. Generated
    // WaitWarpStart dependencies include box gates and indirect/wave creators.
    if (training) {
        const uint32_t generated = NativeEntryGateFind(mission, placement);
        if (generated) return generated;
    }
    return !atFight && door ? legacyGate : 0;
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
         fightSpawn = true, anyBoss = true, fightAgainDirect = false,  // #100232: [Rematch] FightAgainDirect
         ignoreProgress = false;
    int fightSettleMs = 1000, fightNudgeMs = 1000;  // #70586; anyBoss: legacy ini compatibility; ignoreProgress = [Dev] IgnoreProgress (#69305 test)
} g_cfg;

// #256 Boss Match without save progress: an unbeaten mission started by IgnoreProgress is fought with the game's
// Mission::SetMissionClearFlag script call skipped (hooks.cpp InstallVeto), so no clear / unlock / first-clear reward is written.
// Armed when the request is queued; active only while the running mission (ms28) is that id; disarmed once the player
// has been in it and then is anywhere else. The clear byte is read at arm and at disarm and logged (must stay 0).
bool g_clearGuardOk = false;
uint16_t g_bypassId = 0;              // set by ClearedNow when it let an uncleared id through (game thread)
std::atomic<uint32_t> g_unbeaten{0};  // mission id of the running unbeaten trip, 0 = none
std::atomic<int> g_unbeatenSeen{0}, g_unbeatenVetoes{0};
ULONGLONG g_unbeatenAt = 0;
uint8_t g_unbeatenClear0 = 0xFF;
uint32_t g_runningMission = 0;        // ms28, refreshed every Tick (the veto runs on the script thread)

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
enum ReqKind : uint32_t { kReqNone = 0, kReqOpen = 1, kReqDirect = 2, kReqAbort = 3, kReqEmaki = 4, kReqScroll = 5, kReqFight = 6, kReqShrine = 7, kReqExit = 8, kReqPractice = 9, kReqPracticeExit = 10, kReqTeleport = 11, kReqNativeAgain = 12, kReqEmakiTrain = 13, kReqField = 14, kReqCount };
// Native gate retry returns through the owned transaction, then starts a fresh guarded request.
int g_nativeAgainRow = -1;
ULONGLONG g_nativeAgainAt = 0;
uint64_t g_nativeAgainSequence = 0, g_nativeAgainEpoch = 0;
std::atomic<uint64_t> g_requestEpoch{0};
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
    g_requestEpoch.fetch_add(1);
    g_req.store(kind | ((uint32_t)(row + 1) << 8));
    static const char* const kn[] = {"", "R-a open the replay menu", "R-b direct start", "abort", "Picture Scroll (E1 redirect)", "Picture Scroll (game's pick)",
                                     "Fight now (in the arena)", "Shrine before the boss", "leave the mission (return)", "Practice Yamagata (native shrine)", "return from practice", "Teleport to arena (fast load)", "return and fight the same native boss again",
                                     "Training at a Picture Scroll (no waves)", "Open world (teleport and summon)"};
    static_assert(sizeof(kn) / sizeof(kn[0]) == kReqCount, "every ReqKind needs a name: #79507 crashed reading past this table");
    Log("Rematch: request %s%s%s queued from %s", kind < kReqCount ? kn[kind] : "unknown", row >= 0 ? " for " : "", RowName(row), via);
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
    bool returnLoadSeen = false, returnDeparted = false;
    uint32_t exitMission = 0, exitRetry = 0;
    ULONGLONG requestedAt = 0, settledAt = 0;
    uint8_t before[3 * 0x58] = {}, after[3 * 0x58] = {};
    const char* reason = nullptr;
} g_attempt;
uint64_t g_attemptSequence = 0;
std::atomic<int> g_attemptPhase{aNone};
// 1.8.9 (#80065): a native Fight now win leaves the attempt in aActive while the player stands at the Proceed gate, which refused every new
// start until the mission was left. The win is the script's boss-mode end / mission-clear (rematch::NoteNativeWin). Once the boss is dead,
// the player is in the world, not loading or paused, the attempt is settled: the pump retires it with a normal exit (ToIdle normalExit).
std::atomic<uint64_t> g_attemptWonSeq{0};  // attempt sequence a win was seen for
std::atomic<bool> g_attemptWonDone{false};  // set by AttemptObserve (game thread) when that win is settled at the gate
const char* AttemptStartGate() {
    const int phase = g_attemptPhase.load();
    return phase == aNone ? nullptr : phase == aQuarantined ? "native request uncertain; restart game before another replay" : "native replay/return still pending";
}
void AttemptQuarantine(const char* why) {
    if (g_attemptPhase.load() == aNone) return;
    if (!g_attempt.reason) g_attempt.reason = why;
    g_attemptPhase.store(aQuarantined);
}
// #75648: while a trip we requested is the resume mission, the game's save writer is told it succeeded
// and the file is left untouched. Story flags are not written or cleared. The hold ends when that resume
// mission is no longer the trip, and the game's own AutosaveRequest then writes home.
struct ResumeHoldState {
    std::atomic<int> armed{0}, inPlace{0}, session{0}, entered{0}, skipped{0}, deferMap{0};
    std::atomic<uint32_t> trip{0}, source{0};
    ULONGLONG armedAt = 0, homeSince = 0, deferAt = 0;
} g_resume;
using SaveWriteFn = uint8_t (*)(void*, void*, void*, uint32_t, uint32_t*);
using BackupWriteFn = uint8_t (*)(void*, void*, void*, uint32_t);
using AutosaveFn = void (*)();
SaveWriteFn g_saveWrite = nullptr;
BackupWriteFn g_backupWrite = nullptr;
AutosaveFn g_autoSave = nullptr;
bool ReadLiveMission(uint32_t& live) {
    uint64_t g = 0, ms = 0;
    live = 0;
    return Rd(G_MS, g) && g && Rd(g + 8, ms) && ms && Rd(ms + 0x28, live);
}
bool ResumeSkipNow() {
    if (!g_resume.armed.load()) return false;
    uint32_t live = 0;
    const bool ok = ReadLiveMission(live);
    return rcore::ResumeBlocksWrite(true, g_resume.trip.load(), g_resume.source.load(), live, ok, g_resume.session.load() != 0);
}
uint8_t ResumeSaveWrite(void* dir, void* name, void* buf, uint32_t size, uint32_t* err) {
    if (ResumeSkipNow()) {
        if (err) *err = 0;
        g_resume.skipped.fetch_add(1);
        g_resume.entered.store(1);
        return 1;
    }
    return g_saveWrite(dir, name, buf, size, err);
}
uint8_t ResumeBackupWrite(void* a, void* b, void* c, uint32_t n) {
    if (ResumeSkipNow()) {
        g_resume.skipped.fetch_add(1);
        g_resume.entered.store(1);
        return 1;
    }
    return g_backupWrite(a, b, c, n);
}
void ResumeArm(uint32_t trip, uint32_t source, ULONGLONG t, bool inPlace) {
    // v3 (#80182): a trip started from the gate of a retired attempt (we are still in the old trip's mission) keeps the ORIGINAL home
    // and the saves already held for it. The current mission is a boss mission, never a home.
    const bool chained = g_resume.armed.load() && g_resume.source.load() && g_resume.trip.load() == source && g_resume.source.load() != source;
    // Log text only: the mission the player really returns to. A trip armed from the gate of an earlier trip returns to that trip's home
    // (possibly 0 = hub), not to the boss mission we stand in.
    const uint32_t logHome = (g_resume.armed.load() && g_resume.trip.load() == source) ? g_resume.source.load() : source;
    if (chained) {
        Log("Rematch: resume hold kept for the first trip: home stays 0x%X (not 0x%X)", g_resume.source.load(), source);
        source = g_resume.source.load();
    }
    g_resume.trip.store(trip);
    g_resume.source.store(source);
    g_resume.inPlace.store(inPlace ? 1 : 0);
    g_resume.session.store(0);
    g_resume.entered.store(0);
    if (!chained) g_resume.skipped.store(0);
    g_resume.homeSince = 0;
    g_resume.armedAt = t;
    g_resume.armed.store(1);
    Log("Rematch: resume hold armed. Trip mission 0x%X, resume mission was 0x%X%s", trip, logHome, inPlace ? " (same mission)" : "");
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
    ResumeArm(mission, s.ms28, t, mission == s.ms28);
    g_attempt.row = row;
    g_attempt.queueSlot = slot;
    g_attempt.scroll = scroll;
    g_attempt.requestedAt = t;
    memcpy(g_attempt.before, before, sizeof(g_attempt.before));
    g_attemptWonSeq.store(0);
    g_attemptWonDone.store(false);
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
// Defined with the scroll state. True only before the scroll flag is set, while the load flag is on.
bool ScrollLoadStuck(const Snap& s);
const char* ExitGate(const Snap& s) {
    if (const char* why = NativeGate(s)) return why;
    if (s.st[2] && !ScrollLoadStuck(s)) return "loading";
    if (!s.ms28) return "not in a mission";
    return nullptr;
}
// Training still owns a multi-boss roster (an intermediate boss-mode end, or the roster result not final yet).
bool RosterStillPending() {
    using Fn = int (*)();
    const auto f = (Fn)TrainingExport("Nioh3Training_RosterPending");
    return f && f() != 0;
}
void AttemptObserve(const Snap& s, ULONGLONG t) {
    const int phase = g_attemptPhase.load();
    if (phase == aNone || phase == aQuarantined) return;
    if (SessionGate(s)) { AttemptQuarantine("session lost or connected during native attempt"); return; }
    if (phase == aReturning) {
        // A completed return owns only its transition, not a later real mission.
        // Latch departure across UI pauses/another load; never confuse the same
        // mission entered at its ordinary start with the old Boss Match.
        if (!g_attempt.returnCompleted || !s.gm) return;
        if (s.st[2]) { g_attempt.returnLoadSeen = true; return; }
        if (!g_attempt.returnLoadSeen || !s.msOk) return;
        if (s.ms28 != g_attempt.exitMission || s.ms14 != g_attempt.exitRetry)
            g_attempt.returnDeparted = true;
        if (!s.player || s.st[3] || s.st[4] || !g_attempt.returnDeparted) return;
        g_attempt.returnMission = s.ms28; g_attempt.returnRetry = s.ms14;
        g_attempt.standaloneDone = true; // all explicit returns; restore via ToIdle
        return;
    }
    if (phase == aActive) {
        if (g_attempt.sequence && g_attemptWonSeq.load() == g_attempt.sequence && !fight::g_active && s.gm && s.msOk && s.player &&
            !s.st[2] && !s.st[3] && !s.st[4]) {
            if (!RosterStillPending()) g_attemptWonDone.store(true);
            else { static ULONGLONG nx; if (t >= nx) { nx = t + 10000; Log("Rematch: native win seen at the gate, waiting for Training's roster to finish"); } }
        }
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
    g_attempt.exitMission = s.ms28; g_attempt.exitRetry = s.ms14;
    g_attempt.returnLoadSeen = g_attempt.returnDeparted = g_attempt.standaloneDone = false;
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
        ((void (*)())(g_img.base + Grva(0x3B1B6C)))();
        ((void (*)(uint32_t, float, float))(g_img.base + Grva(0x345508)))(0, 30.0f, 0.0f);
        return 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 1;
    }
}
int StageWorld(int* stage) {  // StageMgr [45B9E58]: +0x500 current world, +0x889 streamer state (5 = ready), as Nioh3Training CurWorld/StageState
    uint64_t sm = 0;
    uint8_t w = 0, st = 0;
    if (!g_img.base || !Rd(g_img.base + Grva(0x45B9E58), sm) || !sm || !Rd(sm + 0x500, w) || !Rd(sm + 0x889, st)) { *stage = -1; return -1; }
    *stage = st;
    return w;
}
// start 0 lets EmakiMissionRequest use the record it resolved, or the mission default (17000009) when the lookup misses.
// A nonzero start is kept even when the lookup misses (0x10DBED0: param_3 == 0 is the only path that substitutes the default).
int CallEmakiRequest(uint64_t ref, const uint8_t* key12, uint32_t start) {
    alignas(16) uint32_t p[8] = {0x101, 0x1000000, 0x10100, 0, 0, 0, 0, 0};  // movaps [params] (10DC0CD): 16-byte aligned
    alignas(16) uint8_t k[16] = {};
    memcpy(k, key12, 12);
    __try {
        ((void (*)(uint64_t, const uint8_t*, uint32_t, const uint32_t*))F_EMAKIREQ)(ref, k, start, p);
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
int g_q6Count = -1;  // last Picture Scroll item count seen at a chosen-record request; -1 = inventory unreadable
int CountScrollItems() {
    uint64_t a = 0, inv = 0;
    if (!Rd(G_INV, a) || !a || !Rd(a, inv) || !inv) return -1;
    int n = 0;
    for (int i = 0; i < 4096; ++i) {
        const uint64_t it = inv + 0x224A60 + (uint64_t)i * 0xE8;
        uint16_t id = 0;
        if (!Rd(it, id)) break;
        if (IsScroll(id)) ++n;
    }
    return n;
}
int CountBagItem(uint16_t want) {
    uint64_t a = 0, inv = 0;
    if (!Rd(G_INV, a) || !a || !Rd(a, inv) || !inv) return -1;
    int n = 0;
    for (int i = 0; i < 4096; ++i) {
        uint16_t id = 0;
        if (!Rd(inv + 0x224A60 + (uint64_t)i * 0xE8, id)) break;
        if (id == want) ++n;
    }
    return n;
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
// Live record +0x0C. No row list: this is the start the game copies into the queue when the lookup hits.
bool ReadEmakiStart(const EmakiTab& t, uint32_t index, uint32_t& start) {
    start = 0;
    if (index >= t.count || !t.B) return false;
    return Rd(t.B + 8 + (uint64_t)index * 0x34 + 0x0C, start) && start != 0;
}
rcore::MapPatch g_map;
rcore::ETrack g_et;
struct WavePlace {
    int step = -1;
    int seenFight = 0;
    int cleared = 0;
    int exhausted = 0;
    int loggedWait = 0;
    int loggedStop = 0;
    int offset = -1;
    ULONGLONG at = 0;
    float base[4] = {};
    bool have = false;
} g_wavePlace;
bool ScrollLoadStuck(const Snap& s) {
    return g_state.load() == kEmaki && g_et.st == rcore::kEArmed && !s.emaki && s.st[2];
}
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
bool g_trGateArmed = false;  // #71926: Training owns the destination, the door right is timed from the landing here
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
bool g_trainingDestination = false; // sticky for this attempt, even after Training finishes entry
// human #107514: the next Fight now of this row is 'Train in this arena' (no boss gate request, Training stops at the arena warp)
std::atomic<int> g_arenaOnlyRow{-1};
bool g_fightArenaOnly = false;
int g_trainingRoute = -1;
bool g_warpPending = false;      // a warp is due once the mission is idle (load done, player, no fight)
ULONGLONG g_warpDue = 0;
int g_warps = 0, g_fightRow = -1;
int g_lastFightRow = -1; ULONGLONG g_lastFightEnd = 0;  // #106298: the row an attempt had when it ended, for the post-win white dot
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
    // #107514/#107642: a refused request never leaves a 'Train in this arena' pick (BR row or Training arena-only) for a later Fight now
    rcore::ArenaOnlyRefuseClear(g_arenaOnlyRow, (void (*)(int))TrainingExport("Nioh3Training_SetArenaOnlyNext"));
    Log("Rematch: refused: %s", why);
    Result("refused: %s", why);
    ToastF(cOrange, 6000, "Boss match refused: %s", why);
}
// #79069 Training at a Picture Scroll arena. In memory only: set by a Training request, cleared by every ExecEmaki and by ToIdle.
std::atomic<bool> g_scrollNoWaves{false};
void StagedWaveReset();
void ToIdle(const char* why, bool normalExit = false) {
    StagedWaveReset();
    g_scrollNoWaves.store(false);
    if (!normalExit) AttemptQuarantine(why);
    UnhideRestore(why);
    const bool restored = SpawnRestore(why);
    uint32_t liveMission = 0;
    const bool liveOk = ReadLiveMission(liveMission);
    const bool deferMap = g_map.active && rcore::ResumeBlocksWrite(g_resume.armed.load() != 0, g_resume.trip.load(), g_resume.source.load(),
                                                                    liveMission, liveOk, g_resume.session.load() != 0);
    const bool mapRestored = deferMap ? true : MapRestoreNow(why);
    if (deferMap) {
        g_resume.deferMap.store(1);
        g_resume.deferAt = Ms();
        Log("Rematch: map restore deferred (%s) until the resume mission leaves the trip", why);
    }
    const int phase = g_attemptPhase.load();
    if (normalExit && restored && mapRestored && (phase == aActive || phase == aReturning)) {
        g_attempt = NativeAttempt{};
        g_attemptPhase.store(aNone);  // legacy operational exit, not training completion
        g_practiceOwned.store(false);
    } else if (phase != aNone) AttemptQuarantine(why);
    g_et.st = rcore::kEIdle;
    g_wavePlace = {};
    g_state.store(kIdle);
    g_replayIn.store(false);
    g_activeRow.store(-1);
    g_fight.store(false);
    g_fightMission = g_fightStart = 0;
    g_fightByMission = g_fightLoading = false;
    g_fightLoads = 0;
    g_fightWarp = g_warpPending = false;
    g_trainingDestination = false; g_trainingRoute = -1;
    g_warps = 0;
    if (g_fightRow >= 0) { g_lastFightRow = g_fightRow; g_lastFightEnd = Ms(); }
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

// ---- #256 unbeaten trip guard ----
uint8_t ClearByte(uint16_t id) {
    uint64_t db = 0;
    uint8_t c = 0xFF;
    if (!Rd(G_CLEAR, db) || !db || !CallCleared(db, id, c)) return 0xFF;
    return c;
}
bool VetoClear(int) {  // script thread: Mission::SetMissionClearFlag (no args; clears the RUNNING mission)
    const uint32_t m = g_unbeaten.load();
    uint64_t g = 0, ms = 0;
    uint32_t run = 0;
    if (!Rd(G_MS, g) || !g || !Rd(g + 8, ms) || !ms || !Rd(ms + 0x28, run)) run = g_runningMission;
    fight::ConfirmNativeWin(run);  // outcome observation precedes (and never disables) save protection
    if (!m || run != m) return false;
    g_unbeatenVetoes.fetch_add(1);
    Log("Rematch: UNBEATEN 0x%04X: the game's mission-clear call was SKIPPED (boss beaten in a Boss Match of a mission this save has not cleared; no clear/unlock/story write) (#256)", m);
    return true;
}
// The mission script can finish its native actor while a generated roster still
// has a missing member. Do not put that remaining fight into clear-gadget wait
// mode. The native is resolved by its registered name; no mission/boss/address list.
bool VetoRosterClearWait(int) {
    using Pending = int (*)();
    const auto pending = (Pending)TrainingExport("Nioh3Training_NativeRosterPending");
    if (!pending || !pending()) return false;
    Log("Rematch: roster still pending; ClearGadgetAccessWaitMode SKIPPED (keep remaining boss in combat)");
    return true;
}
// The D2DF script's Emaki::RequestWaveBattle is skipped on that trip only. The wave list is the scroll's only enemy source and the
// call pushes no script value, so the script sits in its next wait (wave clear or player dead). A normal start, or a save that
// resumes into a scroll, never has the flag; the state + mission checks refuse a stale one.
bool g_waveVetoOk = false;
bool ScrollTrainingNow() {
    if (!g_scrollNoWaves.load() || g_state.load() != kEmaki) return false;
    uint64_t g = 0, ms = 0;
    uint32_t run = 0;
    const uint32_t scroll = rcore::ScrollMission(g_rows, g_activeRow.load());  // the running row's catalogue story_mission (was 0xCC96)
    return scroll && Rd(G_MS, g) && g && Rd(g + 8, ms) && ms && Rd(ms + 0x28, run) && run == scroll;
}
bool VetoWaveRequest(int) {  // script thread
    if (!ScrollTrainingNow()) return false;
    static std::atomic<int> logged{0};
    if (logged.fetch_add(1) < 8) Log("Rematch: scroll Training: Emaki::RequestWaveBattle SKIPPED (no waves on this trip)");
    return true;
}
// Staged invade-box waves (#88648, #88701). RequestID is a script binding: rcx at a binding's entry is the VM
// wrapper. Push one int, call it, then restore the stack count so the interrupted native keeps its own arguments.
// The engine native Time::DebugDispPoppingWait is not what the mission script calls (that name is a user function
// which only yields), so the pump is Character::Create plus the wait-loop natives GetTimeStep and IsEnterPlacementBox.
// No mission-flag writes. A row with a boss gate waits until that gate's exclusive right has been requested.
enum { kWvOff = 0, kWvReq = 1, kWvAct = 2, kWvClr = 3, kWvDone = 4 };
struct StagedWaves {
    const uint16_t* id = nullptr;
    uint16_t n = 0;
    uint16_t index = 0;
    uint16_t hpThou = 0;
    uint32_t hpPlace = 0;
    uint32_t mission = 0;
    int final = 0;
    volatile LONG step = kWvOff;
    volatile LONG busy = 0;
    volatile LONG polls = 0;
    volatile LONG waitLogged = 0;
    volatile LONG pushLogged = 0;
    volatile LONG hpDone = 0;
    volatile LONG hpTries = 0;
    volatile LONG bossSeen = 0;   // Character::Create of hpPlace
    volatile LONG standBox = 0;   // station box to place into; 0 = no place pending
    int standX = 0, standY = 0, standZ = 0;
    volatile LONG standDone = 0;  // index+1 already handled
    uint32_t pollArg[4] = {};
    volatile LONG pollN = 0;      // distinct IsEnterPlacementBox args logged this station
    volatile LONG waitBoss = 0;   // one log while the place waits for the chain placement
} g_wv;
volatile int g_stagedFinal = 0;  // 0 = Full climb, 1 = Final wave. This session only. Not written to the save.
struct WaveFrame {
    uint64_t countObj = 0, values = 0, inner = 0, count = 0;
    uint16_t base = 0;
    bool pushed = false;
};
bool WavePush(uint64_t wrapper, uint32_t id, WaveFrame* f) {
    uint64_t script = 0, cap = 0;
    if (!f || wrapper < 0x10000 || !Rd(wrapper + 8, script) || script < 0x10000) return false;
    if (!Rd(script + 0x18, f->values) || !Rd(script + 0x20, f->countObj) || !Rd(script + 8, f->inner)) return false;
    if (f->values < 0x10000 || f->countObj < 0x10000 || f->inner < 0x10000) return false;
    if (!Rd(f->countObj, f->count) || !Rd(f->countObj + 8, cap) || f->count >= cap || cap > 100000) return false;
    if (!Rd(f->inner + 0xA6, f->base)) return false;
    uint8_t slot[8] = {};
    memcpy(slot, &id, 4);
    const uint16_t typ = 1;
    memcpy(slot + 4, &typ, 2);
    if (!WrBytes(f->values + f->count * 8, slot, 8)) return false;
    const uint64_t next = f->count + 1;
    if (!WrBytes(f->countObj, &next, 8)) return false;
    const uint16_t nb = (uint16_t)f->count;
    if (!WrBytes(f->inner + 0xA6, &nb, 2)) {
        WrBytes(f->countObj, &f->count, 8);
        return false;
    }
    f->pushed = true;
    return true;
}
void WaveRestore(const WaveFrame& f) {
    if (!f.pushed) return;
    WrBytes(f.inner + 0xA6, &f.base, 2);
    WrBytes(f.countObj, &f.count, 8);
}
// No C++ object in this function: MSVC rejects __try where a destructor would run.
int WaveInvoke(uintptr_t fn, uint64_t wrapper, WaveFrame* f, bool* outBool) {
    __try {
        ((void (*)(uint64_t))fn)(wrapper);
        if (outBool) {
            uint64_t now = 0;
            uint8_t top[8] = {};
            *outBool = false;
            if (Rd(f->countObj, now) && now > 0 && Copy(top, f->values + (now - 1) * 8, 8)) *outBool = top[0] != 0;
        }
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return -1;
    }
}
// 1 called, 0 the stack was not ready, -1 the binding faulted. A bool result is read before the count is restored.
int WaveCall(uint64_t wrapper, const char* site, uint32_t id, bool* outBool) {
    const uintptr_t fn = Site(site);
    if (!fn) return 0;
    WaveFrame f = {};
    if (!WavePush(wrapper, id, &f)) return 0;
    const int rc = WaveInvoke(fn, wrapper, &f, outBool);
    WaveRestore(f);
    return rc;
}
void StagedWaveReset() {
    g_wv.id = nullptr;
    g_wv.n = 0;
    g_wv.index = 0;
    g_wv.hpThou = 0;
    g_wv.hpPlace = 0;
    g_wv.mission = 0;
    g_wv.final = 0;
    g_wv.standX = g_wv.standY = g_wv.standZ = 0;
    InterlockedExchange(&g_wv.polls, 0);
    InterlockedExchange(&g_wv.waitLogged, 0);
    InterlockedExchange(&g_wv.pushLogged, 0);
    InterlockedExchange(&g_wv.hpDone, 0);
    InterlockedExchange(&g_wv.hpTries, 0);
    InterlockedExchange(&g_wv.bossSeen, 0);
    InterlockedExchange(&g_wv.standBox, 0);
    InterlockedExchange(&g_wv.standDone, 0);
    InterlockedExchange(&g_wv.pollN, 0);
    InterlockedExchange(&g_wv.waitBoss, 0);
    InterlockedExchange(&g_wv.step, kWvOff);
}
void StagedWaveArm(uint32_t mission) {
    if (g_wv.step != kWvOff) return;
    uint16_t n = 0;
    const uint16_t* w = NativeEntryWaves(mission, &n);
    if (!w || !n) {
        InterlockedExchange(&g_wv.step, kWvDone);
        return;
    }
    const int final = g_stagedFinal && n > 1;
    uint16_t thou = 0;
    uint32_t place = NativeEntryWavePlace(mission);
    if (final) NativeEntryWaveFinal(mission, &thou, &place);
    g_wv.id = w;
    g_wv.n = n;
    g_wv.index = StagedStartIndex(n, g_stagedFinal);
    g_wv.final = final;
    g_wv.hpThou = thou;
    g_wv.hpPlace = place;
    g_wv.mission = mission;
    InterlockedExchange(&g_wv.polls, 0);
    InterlockedExchange(&g_wv.hpDone, 0);
    InterlockedExchange(&g_wv.hpTries, 0);
    InterlockedExchange(&g_wv.step, kWvReq);
    Log("Rematch: staged waves armed mission 0x%X %s %u/%u id %u hp %u/1000 place %u (request after the door right, no save writes)",
        mission, final ? "final" : "full", (unsigned)g_wv.index + 1, n, w[g_wv.index], thou, place);
}
void StagedWaveWatch(const Snap& s, uint32_t mission) {
    if (!mission || g_wv.step != kWvOff) return;
    if (!s.msOk || !s.player || s.ms28 != mission || s.st[2] || s.st[3] || s.st[4]) return;
    StagedWaveArm(mission);
}
void StagedHpTry() {
    if (!StagedFinalHpPending(g_wv.final, g_wv.n, g_wv.index, g_wv.hpThou, g_wv.hpPlace, (int)g_wv.hpDone)) return;
    if ((InterlockedIncrement(&g_wv.hpTries) % 15) != 1) return;
    using SetHp = int (*)(uint32_t, int);
    const auto set = (SetHp)TrainingExport("Nioh3Training_SetPlacementHpPct");
    if (!set) {
        InterlockedExchange(&g_wv.hpDone, 1);
        Log("Rematch: staged final HP not set: Training has no placement HP export");
        return;
    }
    const int rc = set(g_wv.hpPlace, (int)g_wv.hpThou);
    if (rc > 0) {
        InterlockedExchange(&g_wv.hpDone, 1);
        Log("Rematch: staged final HP placement %u set to %u/1000 (session memory, no save)", g_wv.hpPlace, g_wv.hpThou);
    } else if (rc < 0) {
        InterlockedExchange(&g_wv.hpDone, 1);
        Log("Rematch: staged final HP placement %u refused (%d)", g_wv.hpPlace, rc);
    }
}
int LessonVmArgs(uint64_t ctx, uint32_t* val, uint16_t* ty, int maxn);
// Wait_InvadeBox returns before RequestID, so once the wave is active the script is in IsClearID
// and is not still polling that station box. The centre is the generated chain's PlacementBox.
// The game thread places there once, after the chain's boss placement has been created.
// A one-station chain is left alone. No mission-flag writes. Only when the player is more than 20 m out.
// IsEnterPlacementBox args are logged once per station and are not the place trigger.
void OnStagedBoxPoll(uint64_t rcx, uint64_t, uint64_t, uint64_t, const uint64_t*) {
    if (g_wv.n < 2 || g_wv.step != kWvClr || g_wv.pollN >= 4) return;
    uint32_t val[4] = {};
    uint16_t ty[4] = {};
    const int n = LessonVmArgs(rcx, val, ty, 1);
    if (n < 1) {
        if (InterlockedCompareExchange(&g_wv.pollN, 4, 0) == 0)
            Log("Rematch: staged box poll unreadable (%d) while the wave is active", n);
        return;
    }
    for (LONG i = 0; i < g_wv.pollN && i < 4; ++i) if (g_wv.pollArg[i] == val[0]) return;
    const LONG i = g_wv.pollN;
    if (i < 0 || i >= 4) return;
    g_wv.pollArg[i] = val[0];
    InterlockedIncrement(&g_wv.pollN);
    Log("Rematch: staged box poll arg %u type %u (sample %ld, not used to place)", val[0], (unsigned)ty[0], (long)i + 1);
}
void StagedPlaceTick(const Snap& s) {
    const LONG box = g_wv.standBox;
    if (!box || g_wv.n < 2 || g_wv.step != kWvClr) return;
    if (g_wv.standDone == (LONG)g_wv.index + 1) return;
    if (g_wv.hpPlace && !g_wv.bossSeen) {
        if (!InterlockedExchange(&g_wv.waitBoss, 1))
            Log("Rematch: staged stand box %ld waits for placement %u", box, g_wv.hpPlace);
        return;
    }
    if (s.online || s.guest || s.st[2] || s.st[3] || s.st[4] || !s.player) return;
    const int x = g_wv.standX, y = g_wv.standY, z = g_wv.standZ;
    if (!g_warpOk) {
        InterlockedExchange(&g_wv.standDone, (LONG)g_wv.index + 1);
        InterlockedExchange(&g_wv.standBox, 0);
        Log("Rematch: staged stand place skipped - the Player::SetPos path is off");
        return;
    }
    float p[4] = {};
    if (!PlayerPos(p)) return;
    InterlockedExchange(&g_wv.standDone, (LONG)g_wv.index + 1);
    InterlockedExchange(&g_wv.standBox, 0);
    const float dx = p[0] - (float)x, dy = p[1] - (float)y, dz = p[2] - (float)z;
    if (dx * dx + dy * dy + dz * dz < 2000.f * 2000.f) {
        Log("Rematch: staged stand box %ld already within 20 m (%.0f %.0f %.0f); no place", box, p[0], p[1], p[2]);
        return;
    }
    const int rc = CallWarp((float)x, (float)y, (float)z, 0.f);
    float after[4] = {};
    PlayerPos(after);
    Log("Rematch: staged stand place box %ld -> %.0f %.0f %.0f %s (from %.0f %.0f %.0f, no flag writes)",
        box, after[0], after[1], after[2], rc == 0 ? "ok" : rc == 1 ? "NO PLAYER" : "EXCEPTION", p[0], p[1], p[2]);
}
void OnStagedWave(uint64_t rcx, uint64_t, uint64_t, uint64_t, const uint64_t*) {
    const LONG step = g_wv.step;
    if (step != kWvReq && step != kWvAct && step != kWvClr) return;
    if (g_gateWant.load() && !g_gateReq) {
        if (!InterlockedExchange(&g_wv.waitLogged, 1)) Log("Rematch: staged wave waiting for the door right");
        return;
    }
    if (InterlockedExchange(&g_wv.busy, 1)) return;
    __try {
        if (g_wv.index >= g_wv.n || !g_wv.id) {
            g_wv.step = kWvDone;
        } else {
            const uint32_t id = g_wv.id[g_wv.index];
            if (step == kWvReq) {
                const int rc = WaveCall(rcx, "WaveBattle::RequestID", id, nullptr);
                if (rc < 0) {
                    g_wv.step = kWvDone;
                    Log("Rematch: staged wave RequestID %u faulted; chain stopped", id);
                } else if (rc > 0) {
                    g_wv.step = kWvAct;
                    InterlockedExchange(&g_wv.polls, 0);
                    Log("Rematch: staged wave RequestID %u (%u/%u)", id, (unsigned)g_wv.index + 1, g_wv.n);
                } else if (!InterlockedExchange(&g_wv.pushLogged, 1)) {
                    Log("Rematch: staged wave RequestID %u stack not ready", id);
                }
            } else {
                bool on = false;
                const char* site = step == kWvAct ? "WaveBattle::IsActiveID" : "WaveBattle::IsClearID";
                const int rc = WaveCall(rcx, site, id, &on);
                if (rc < 0) {
                    g_wv.step = kWvDone;
                    Log("Rematch: staged wave %s %u faulted; chain stopped", site, id);
                } else if (rc > 0 && on) {
                    if (step == kWvAct) {
                        g_wv.step = kWvClr;
                        Log("Rematch: staged wave %u active", id);
                        uint32_t stand = 0;
                        int sx = 0, sy = 0, sz = 0;
                        if (g_wv.n > 1 && NativeEntryWaveStand(g_wv.mission, g_wv.index, &stand, &sx, &sy, &sz)) {
                            g_wv.standX = sx;
                            g_wv.standY = sy;
                            g_wv.standZ = sz;
                            InterlockedExchange(&g_wv.standBox, (LONG)stand);
                            Log("Rematch: staged stand armed box %u at %d %d %d (place after placement %u exists)", stand, sx, sy, sz, g_wv.hpPlace);
                        }
                    } else {
                        Log("Rematch: staged wave %u clear", id);
                        g_wv.index++;
                        InterlockedExchange(&g_wv.polls, 0);
                        InterlockedExchange(&g_wv.pushLogged, 0);
                        InterlockedExchange(&g_wv.standBox, 0);
                        InterlockedExchange(&g_wv.pollN, 0);
                        InterlockedExchange(&g_wv.waitBoss, 0);
                        g_wv.step = g_wv.index < g_wv.n ? (LONG)kWvReq : (LONG)kWvDone;
                    }
                } else if (step == kWvAct) {
                    const LONG n = InterlockedIncrement(&g_wv.polls);
                    if (n == 30 || n == 90 || n == 180) {
                        const int again = WaveCall(rcx, "WaveBattle::RequestID", id, nullptr);
                        Log("Rematch: staged wave RequestID %u retry %ld (%s)", id, n, again > 0 ? "called" : again < 0 ? "fault" : "stack not ready");
                        if (again < 0) g_wv.step = kWvDone;
                    }
                }
            }
            if (g_wv.step == kWvAct || g_wv.step == kWvClr) StagedHpTry();
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_wv.step = kWvDone;
        Log("Rematch: staged wave hook faulted; chain stopped");
    }
    InterlockedExchange(&g_wv.busy, 0);
}
void InstallClearGuard() {
    const char* why = nullptr;
    g_waveVetoOk = InstallVeto("Emaki::RequestWaveBattle", &VetoWaveRequest, &why);
    Log("Rematch: scroll Training wave guard %s%s", g_waveVetoOk ? "installed" : "NOT installed: ", g_waveVetoOk ? "" : (why ? why : "?"));
    const char* pumps[] = {"Character::Create", "Time::GetTimeStep", "Player::IsEnterPlacementBox"};
    int stagedOk = 0;
    for (const char* pump : pumps) {
        why = nullptr;
        if (InstallEntry(pump, &OnStagedWave, &why)) ++stagedOk;
        else Log("Rematch: staged wave pump %s NOT installed: %s", pump, why ? why : "?");
    }
    Log("Rematch: staged wave pumps %d/3", stagedOk);
    why = nullptr;
    if (!InstallEntry("Player::IsEnterPlacementBox", &OnStagedBoxPoll, &why))
        Log("Rematch: staged box poll log NOT installed: %s", why ? why : "?");
    why = nullptr;
    g_clearGuardOk = InstallVeto("Mission::SetMissionClearFlag", &VetoClear, &why);
    Log("Rematch: IgnoreProgress %s; clear guard %s%s%s", g_cfg.ignoreProgress ? "ON (unbeaten bosses can be fought)" : "off",
        g_clearGuardOk ? "installed" : "NOT installed - ", g_clearGuardOk ? "" : (why ? why : "?"), g_clearGuardOk ? "" : " (unbeaten rows stay refused)");
}
void UnbeatenArm(uint16_t id, const char* row) {
    g_unbeatenClear0 = ClearByte(id);
    g_unbeatenSeen.store(0);
    g_unbeatenVetoes.store(0);
    g_unbeatenAt = Ms();
    g_unbeaten.store(id);
    Log("Rematch: UNBEATEN 0x%04X (%s) armed: clear byte now 0x%02X; the game's clear call is skipped while this mission runs", id, row, g_unbeatenClear0);
}
void UnbeatenTick(const Snap& s, ULONGLONG t) {
    if (s.msOk) g_runningMission = s.ms28;
    const uint32_t m = g_unbeaten.load();
    if (!m) return;
    if (s.msOk && s.ms28 == m) {
        if (!g_unbeatenSeen.exchange(1)) Log("Rematch: UNBEATEN 0x%04X: the mission is running (guard active)", m);
        return;
    }
    const bool left = g_unbeatenSeen.load() && s.msOk && s.ms28 != m;
    const bool never = !g_unbeatenSeen.load() && t - g_unbeatenAt > 180000 &&
        s.msOk && s.queueOk && !s.st[2] && g_attemptPhase.load() == aNone;  // never expire protection during an owned/uncertain native load
    if (!left && !never) return;
    const uint8_t c = ClearByte((uint16_t)m);
    g_unbeaten.store(0);
    Log("Rematch: UNBEATEN 0x%04X disarmed (%s): %d clear call(s) skipped; clear byte before 0x%02X after 0x%02X -> %s", m,
        left ? "left the mission" : "mission never loaded", g_unbeatenVetoes.load(), g_unbeatenClear0, c,
        c == 0xFF ? "unreadable" : (c & 1) == (g_unbeatenClear0 & 1) ? "save progress unchanged" : "WARNING the clear changed");
}

bool ClearedNow(uint16_t id, const char** why, bool bossMatch = false) {
    // Scroll rows have no replay mission id. Zero is also the field's ms28;
    // it must never be treated as an active unbeaten replay or passed to the DB.
    if (!id) {
        g_bypassId = 0;
        *why = "row has no replay mission id (Picture Scrolls use scroll entry)";
        return false;
    }
    uint64_t db = 0;
    uint8_t c = 0;
    if (!Rd(G_CLEAR, db) || !db || !CallCleared(db, id, c)) {
        *why = "clear db not readable";
        return false;
    }
    g_bypassId = 0;
    if (!(c & 1)) {
        if (bossMatch || g_cfg.ignoreProgress) {
            if (!g_clearGuardOk) {
                *why = "Unbeaten entry requires the clear guard (Mission::SetMissionClearFlag veto) is not installed - refused so the save cannot change";
                return false;
            }
            const Snap s = ReadSnap();
            if (s.msOk && s.ms28 == id) {
                *why = "you are playing this unbeaten mission right now - beat it normally first";
                return false;
            }
            Log("Rematch: mission 0x%04X not cleared on this journey - protected Boss Match/dev entry; the game's clear call is skipped for this trip (#256)", id);
            g_bypassId = id;
            return true;
        }
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

// Cancel Training's pending auto-enter on every refusal/uncertain queue result.
// Older Training builds have no callback and preserve their existing behavior.
struct TrainingEntryAdmission {
    uint32_t mission; bool accepted = false;
    ~TrainingEntryAdmission() {
        using Fn = void (*)(uint32_t, int);
        const auto fn = (Fn)TrainingExport("Nioh3Training_NativeEntryResult");
        if (fn) fn(mission, accepted ? 1 : 0);
    }
};
void ExecDirect(int row, const Snap& s, ULONGLONG t, bool fight, bool warp) {
    if (!SpawnRestore("before mission request")) return Refuse("spawn recovery unresolved; no new mission admitted");
    if (!MapRestoreNow("before mission request")) return Refuse("scroll-map recovery unresolved; no new entry admitted");
    if (const char* why = AttemptStartGate()) return Refuse(why);
    const bool arenaOnly = g_arenaOnlyRow.exchange(-1) == row && fight && warp;  // #107514: consumed by this request only
    const Row& r = g_rows[(size_t)row];
    TrainingEntryAdmission entryAdmission{r.missionId};
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
    if (!ClearedNow(r.missionId, &why, fight && warp)) return Refuse(why);
    if (fight) {  // #107514: Training's next Fight now entry is arena-only (no boss wait, no fallback) or a normal fight
        if (const auto ao = (void (*)(int))TrainingExport("Nioh3Training_SetArenaOnlyNext")) ao(arenaOnly ? 1 : 0);
        else if (arenaOnly) return Refuse("Train in this arena needs Nioh3Training with SetArenaOnlyNext");
    }
    if (fight && warp && !g_noSwap) {  // swapped in place: no queue slot, no cover, nothing armed (the add-on keeps the native request for every other case)
        using FightNowFn = int (*)(uint32_t);
        static const auto swap = (FightNowFn)TrainingExport("Nioh3Training_FightNow");
        if (swap && swap(r.missionId)) {
            Log("Rematch: F %s: arena swap in place by Nioh3Training (no mission load)", r.id);
            ResumeArm(s.ms28, s.ms28, t, true);
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
    // Arm before the request can execute any native script, including an uncertain admission.
    if (fight && g_bypassId == r.missionId) UnbeatenArm(r.missionId, r.id);
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
        entryAdmission.accepted = true;
        const auto owns = (int (*)(uint32_t))TrainingExport("Nioh3Training_NativeEntryRoute");
        g_trainingDestination = warp && owns && owns(r.missionId) != 0;
        g_trainingRoute = -1;
        if (g_trainingDestination) Log("Rematch: F %s: destination owner=Training; BR spawn override, warp, rewarp and nudge disabled", r.id);
        g_fight.store(true);
        g_fightMission = r.missionId;
        g_fightStart = start;
        g_fightByMission = g_fightLoading = false;
        g_fightLoads = 0;
        g_fightRow = row;
        g_fightArenaOnly = arenaOnly;
        g_fightWarp = !g_trainingDestination && warp && !r.atFight && r.arenaOk && g_cfg.fightWarp && g_warpOk;
        g_gateSeen.store(false);
        g_gateSlot.store(-1);
        g_gateChecks.store(0);
        g_gateReq = false;
        g_gateDue = 0;
        g_trGateArmed = false;
        // #71926: door-waiting boss scripts (WaitWarpStart) never create the boss without this right, and it moves nothing, so it stays on when Training owns the destination.
        g_gateWant.store(rcore::ArenaOnlyGate(arenaOnly, EntryGateChoice(r.missionId, r.placement, r.gateId, r.GateDoor(), r.atFight, g_trainingDestination, warp, g_gateOk)));
        if (arenaOnly) Log("Rematch: F %s: Train in this arena - boss gate not requested (the boss start waits on it), no waves, no boss", r.id);
        if (g_gateWant.load()) {
            Log("Rematch: F %s: boss gate %u (%s) armed: its exclusive right will be requested once the boss start waits for it", r.id, g_gateWant.load(), g_trainingDestination && NativeEntryGateFind(r.missionId, r.placement) ? "script WaitWarpStart" : r.gateKind);
            // #88516: tell Training a door-waiting script owns this boss, so idle same-character bodies are not taken as it.
            if (g_trainingDestination && NativeEntryGateFind(r.missionId, r.placement))
                if (const auto pend = (void (*)(uint32_t))TrainingExport("Nioh3Training_NativeGatePending")) pend(r.missionId);
        }
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

// #75963 DEV control for the Picture Scroll wave. Off unless the pipe command scrollctl sets it; not for release.
std::atomic<int> g_scNoRedirect{0}, g_scLevel{0}, g_scB0f{0};
std::atomic<int> g_scStart0{0};  // #76364 DEV: request the redirected record with start 0, as the game does
std::atomic<int> g_scSeedPick{1};  // #77650 DEV default: E rows seed-pick and skip the map rewrite. noseedpick stores 0.
std::atomic<int> g_scUseItem{0};  // #76364 DEV: a chosen record starts from the held scroll instead of ref -1
std::atomic<bool> g_scGive{false};
std::atomic<uint32_t> g_scGiveId{0x1E82}, g_scGiveSeed{0}, g_scGiveLevel{0}, g_scGiveB0f{0};
// The game's own add path: 54CCD0 builds the 0xE8 record, 54D324 adds it (same calls as Item::AddItemNoNotice 23A0C64).
void ExecScrollGive() {
    alignas(16) uint8_t desc[0x200] = {}, out[0x200] = {};
    uint64_t inv = 0;
    const uint16_t id = (uint16_t)g_scGiveId.load();
    if (!g_img.base || !Rd(G_INV, inv) || !inv) { Log("Rematch: scrollctl give: inventory not ready"); return; }
    const int before = CountScrollItems();
    const uint32_t seed = g_scGiveSeed.load();
    const uint16_t lv = (uint16_t)g_scGiveLevel.load();
    const uint8_t b0f = (uint8_t)g_scGiveB0f.load();
    int rc = 0;
    __try {
        ((void* (*)(void*, uint32_t, uint32_t))(g_img.base + Grva(0x54CCD0)))(desc, id, 1);
        memcpy(desc + 0x20, &seed, 4);
        memcpy(desc + 0x12, &lv, 2);
        desc[0x0F] = b0f;
        ((void* (*)(uint64_t, void*, void*, uint64_t, uint64_t))(g_img.base + Grva(0x54D324)))(inv, out, desc, 0, 1);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        rc = 1;
    }
    Scroll sc;
    const bool found = FindScroll(sc);
    Log("Rematch: scrollctl give id %04X seed %u level %u b0f %u: rc %d, scroll items %d -> %d, found %d idx %d ref %016llX", id, seed, lv, b0f, rc, before,
        CountScrollItems(), found ? 1 : 0, found ? sc.idx : -1, found ? (unsigned long long)sc.ref : 0ULL);
}
// #77374 DEV: the game's own key -> byte draw (10DBED0 calls these two with the key's first dword). No patch involved.
int ScrollDrawByte(uint32_t seed) {
    if (!F_EMAKIREQ || F_EMAKIREQ - g_img.base != 0x10DBED0) return -2;  // only on the build these RVAs were read from
    __try {
        const uint8_t tier = ((uint8_t (*)(uint64_t, uint32_t))(g_img.base + 0x102BB60))(0, seed);
        return ((uint8_t (*)(uint64_t, uint32_t, uint32_t))(g_img.base + 0x102B840))(0, seed, tier);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return -1;
    }
}
// first seed for every byte, one scan per process; logged so the reachable records are on record
int32_t g_scSeedOf[256];
bool g_scSeedScanned = false;
void ScrollSeedScan() {
    if (g_scSeedScanned) return;
    g_scSeedScanned = true;
    memset(g_scSeedOf, 0xFF, sizeof(g_scSeedOf));
    const ULONGLONG t0 = GetTickCount64();
    int hits = 0, bad = 0;
    for (uint32_t seed = 1; seed <= 20000 && GetTickCount64() - t0 < 1500; ++seed) {
        const int b = ScrollDrawByte(seed);
        if (b < 0) { ++bad; break; }
        if (g_scSeedOf[b] < 0) { g_scSeedOf[b] = (int32_t)seed; ++hits; Log("Rematch: seed %u draws byte %02X", seed, b); }
    }
    Log("Rematch: seed scan: %d different bytes, seed 1 -> %d, %d failed, %llu ms", hits, ScrollDrawByte(1), bad, GetTickCount64() - t0);
}

// #78735: a scroll row is Boss Rematch's own trip, so it asks Training for the same loading screen as a boss row.
bool g_scrollCover = false;
ULONGLONG g_scrollCoverAt = 0, g_scrollCoverIn = 0;
void ScrollCoverSet(const char* name, int on) {
    using Fn = int (*)(const char*, int);
    if (const auto f = (Fn)TrainingExport("Nioh3Training_ScrollCover")) f(name, on);
}
void ScrollCoverOn(const char* name, ULONGLONG t) {
    g_scrollCover = true; g_scrollCoverAt = t; g_scrollCoverIn = 0;
    ScrollCoverSet(name, 1);
}
int g_emakiNoteRow = -1;

void ExecEmaki(int row, ULONGLONG t, bool noWaves = false) {
    g_scrollNoWaves.store(false);  // every request decides for itself; only the Training one sets it, below
    if (noWaves && !g_waveVetoOk) return Refuse("Training at a Picture Scroll needs the wave guard, it did not install");
    if (!SpawnRestore("before scroll request")) return Refuse("spawn recovery unresolved; no new scroll admitted");
    if (!MapRestoreNow("before scroll request")) return Refuse("scroll-map recovery unresolved; no new entry admitted");
    if (const char* why = AttemptStartGate()) return Refuse(why);
    if (!g_emakiOk) return Refuse(g_emakiWhy);
    // A chosen record must not touch the save. The clear guard vetoes SetMissionClearFlag of the
    // mission that is running. That veto takes no mission id.
    if (row >= 0 && !g_clearGuardOk) return Refuse("Picture Scroll needs the clear guard so the save cannot change");
    const uint32_t scrollMission = rcore::ScrollMission(g_rows, row);
    if (!scrollMission) return Refuse("scroll catalogue has no story mission");
    Scroll sc;
    if (row < 0 || g_scUseItem.load()) {
        if (!FindScroll(sc)) return Refuse("no Picture Scroll in the inventory (the carrier item)");
    } else {
        // No inventory item. Ref -1 is the game's own no-item path: MissionLoadStep 223B180 then keeps the key itself (+0x344 = 1), and
        // the unique-enemy flag readers E537A0 / E5D8E0 skip the inventory. Any other ref is treated as an item: 107B060 inserts an
        // unknown ref into the inventory's ref map with index 0, so ref 0 tied the scroll to whatever sits in inventory slot 0.
        memset(sc.key, 0, sizeof(sc.key));
        sc.idx = -1;
        sc.itemId = 0;
        sc.ref = ~0ULL;
        sc.key[0] = 1;
        sc.key[7] = 1;
        sc.key[8] = 0;
        const uint16_t ctlLevel = (uint16_t)g_scLevel.load();
        memcpy(sc.key + 4, &ctlLevel, 2);
        sc.key[6] = (uint8_t)g_scB0f.load();
        g_q6Count = CountScrollItems();
        Log("Rematch: Q6 scroll-item count %d at request (mode 0, no item index)", g_q6Count);
    }
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
    uint32_t start = 0;
    const bool seedPick = row >= 0 && g_scSeedPick.load() != 0;
    if (seedPick) {
        ScrollSeedScan();
        const uint32_t want = (uint32_t)g_rows[(size_t)row].emakiIndex;
        int key = -1;
        for (int i = 0; i < tab.n && key < 0; ++i) {
            uint8_t e[8] = {};
            if (Copy(e, tab.vb + (uint64_t)i * 8, 8) && e[0] != tab.emptyKey) {
                uint32_t idx = 0;
                memcpy(&idx, e + 4, 4);
                if (idx == want) key = e[0];
            }
        }
        if (key < 0 || g_scSeedOf[key] < 0) {
            Log("Rematch: seed pick %s: record %u key %d has no seed", g_rows[(size_t)row].id, want, key);
            return Refuse("no key seed reaches this scroll record");
        }
        memcpy(sc.key, &g_scSeedOf[key], 4);
        Log("Rematch: seed pick %s: record %u key %02X seed %d", g_rows[(size_t)row].id, want, key, g_scSeedOf[key]);
    }
    if (row >= 0 && !seedPick && !g_scNoRedirect.load()) {
        const Row& r = g_rows[(size_t)row];
        if ((uint32_t)r.emakiIndex >= tab.count) return Refuse("record index out of range");
        if (!ReadEmakiStart(tab, (uint32_t)r.emakiIndex, start)) return Refuse("this scroll record has no start point");
        if (!rcore::MapRedirect(g_map, (uint8_t*)tab.vb, tab.n, tab.emptyKey, (uint32_t)r.emakiIndex, tab.count, &WrGuard, &MapReadGuard))
            return Refuse("could not redirect the scroll map");
        Log("Rematch: E1 %s: %d map entries -> record %d start %u (snapshot CRC %08X)", r.id, tab.n, r.emakiIndex, start,
            rcore::Crc32(g_map.snap, (size_t)tab.n * 8));
    }
    if (g_scStart0.load()) start = 0;
    char kb[40];
    Hex(kb, sizeof(kb), (uint64_t)sc.key, 12);
    AttemptBegin(scrollMission, start, 0, row, freeSlot, t, current, before, true);
    g_emakiNoteRow = row;
    const int rc = CallEmakiRequest(sc.ref, sc.key, start);
    g_emakiNoteRow = -1;
    uint8_t after[3 * 0x58] = {};
    const bool readable = rc == 0 && Copy(after, G_QUEUE, sizeof(after));
    const bool matched = AttemptMatch(after, readable, rc == 0, sc.ref, sc.key);
    if (!matched) {
        MapRestoreNow("uncertain scroll request");
        return Refuse("native scroll request outcome uncertain; restart game");
    }
    const uint8_t queuedRegion = after[freeSlot * 0x58 + 8];
    uint32_t queuedStart = 0;
    memcpy(&queuedStart, after + freeSlot * 0x58 + 0xC, 4);
    Log("Rematch: E queue slot %d: region %02X start %u%s", freeSlot, queuedRegion, queuedStart,
        queuedRegion ? "" : " - WARNING: region 00 means the record lookup missed");
    if (row >= 0) UnbeatenArm((uint16_t)scrollMission, g_rows[(size_t)row].id);
    g_state.store(kEmaki);
    g_activeRow.store(row);
    g_eRow = row;
    g_eByteSeen = 0;
    g_et.st = rcore::kEArmed;
    g_et.armedAt = t;
    g_wavePlace = {};
    g_scrollNoWaves.store(noWaves && row >= 0);
    if (row >= 0) ScrollCoverOn(g_rows[(size_t)row].name, t);
    Log("Rematch: E %s: EmakiMissionRequest(ref %016llX, key %s [item %04X idx %d], start %u, params) queued 0x%X",
        row >= 0 ? g_rows[(size_t)row].id : "(game's pick)", (unsigned long long)sc.ref, kb, sc.itemId, sc.idx, start, scrollMission);
    Result("started %s (%s)", row >= 0 ? g_rows[(size_t)row].id : "game's pick", g_scrollNoWaves.load() ? "training, no waves" : "waves");
    ToastF(cGreen, 8000, "%s", row >= 0 ? g_rows[(size_t)row].name : "Picture Scroll: the game's pick");
}

// game thread, inside the Fight now mission: warp into the arena trigger once the mission is idle
void DoWarp(const Snap& s, ULONGLONG t) {
    if (g_trainingDestination) return;
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
    if (g_trainingDestination) {
        const auto routeFn = (int (*)(uint32_t))TrainingExport("Nioh3Training_NativeEntryRoute");
        const int route = routeFn ? routeFn(g_fightMission) : 0;
        if (route != g_trainingRoute) {
            g_trainingRoute = route;
            const char* label = route == 1 ? "native pending" : route == 2 ? "native" : route == 3 ? "fallback pending" : route == 4 ? "fallback" : route == 5 ? "failed" : route == 6 ? "arena only (no boss)" : "ended";
            const char* id = g_fightRow >= 0 ? g_rows[(size_t)g_fightRow].id : "?";
            Log("Rematch: F %s: route=%s (Training owns destination; BR warps=%d)", id, label, g_warps);
            Result("fight now %s: route=%s", id, label);
            if (route == 2 || route == 4 || route == 6) g_fn.store(kFnPass);
            else if (rcore::FnFailOnRoute(route, g_fn.load() == kFnPass)) g_fn.store(kFnFail);
        }
        return; // never fall through to global fight-count PASS or BR rewarp/nudge
    }
    if (g_fn.load() != kFnWarped || g_fightRow < 0) return;
    const Row& r = g_rows[(size_t)g_fightRow];
    if (g_fightArenaOnly) {  // #107514: in the arena with no boss is the goal: no fight deadline, no nudge, no rewarp
        g_fn.store(kFnPass);
        Log("Rematch: F %s: Train in this arena - in the arena after warp #%d, no boss requested", r.id, g_warps);
        Result("train in arena %s: in the arena, no boss", r.id);
        return;
    }
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
    if (s.ms14 == g_fightStart && !g_fightWarp && !g_gateReq && r.atFight && r.bossOk && !g_warps && t - g_fnFirst > (ULONGLONG)g_cfg.fightNudgeMs && !fight::g_active) {  // #70586: was a fixed 10 s, most of a real load's 20 s
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
                Log("Rematch: F %s: no fight %.1f s after the landing at %.1f m from the boss (a duel starts inside 8 m) - %s to 4 m in front of it", r.id, (double)(t - g_fnFirst) / 1000.0,
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
    if (g_trainingDestination && want) {
        const auto routeFn = (int (*)(uint32_t))TrainingExport("Nioh3Training_NativeEntryRoute");
        // #88390: route 2 (native alive) can be an idle same-character body (RB24D 574946); a gate the script itself waits on
        // (RightsCheck sees its key) is still requested then. Fallback / failed / unowned routes keep the door shut.
        const int route = routeFn ? routeFn(g_fightMission) : 0;
        if (route != 1 && !(route == 2 && g_fightRow >= 0 && NativeEntryGateFind(g_fightMission, g_rows[(size_t)g_fightRow].placement))) { g_gateWant.store(0); return; }
        if (!g_trGateArmed) {
            if (s.st[2] || s.st[3] || s.st[4] || !s.player || s.ms28 != g_fightMission) return;
            g_trGateArmed = true;
            g_gateDue = t + 5000;  // after Training's own move (3 s settle)
        }
    }
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
            if (!g_trainingDestination && (g_fn.load() == kFnArmed || (g_fn.load() == kFnWarped && !g_fightWarp))) {
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
    if (g_trainingDestination) {
        const auto opened = (void (*)(uint32_t))TrainingExport("Nioh3Training_NativeGateOpened");
        if (!rc && opened) opened(g_fightMission);
        return;
    }
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
    {   // #79802/#79884: Return to Title never reaches the exit branches below (they need a player), so 'rematch active' outlived the title.
        // At the title the game manager pointer is null (pc2: state held 2.5 min with player=0 loading=0), so gm is not required.
        // gm readable: no player and no load for 20 s. gm null (load flag unreadable): 90 s, longer than any load seen (73.7 s).
        static ULONGLONG titleSince = 0;
        if (g_state.load() != kIdle && !s.player && !s.st[2]) {
            if (!titleSince) titleSince = t;
            else if (t - titleSince > (s.gm ? 20000u : 90000u)) {
                titleSince = 0;
                Log("Rematch: no player and no load for %u s (Return to Title) - idle", s.gm ? 20u : 90u);
                Result("exited %s (title)", RowName(g_activeRow.load()));
                ToIdle("returned to title", true);  // normal exit: restores run, a clean active/returning phase clears (a quarantine here would block every later attempt)
                return;
            }
        } else titleSince = 0;
    }
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
            else if (t - g_loadDoneAt > (g_fight.load() ? (ULONGLONG)g_cfg.fightSettleMs : g_tf.on ? 1000u : 3000u)) {  // #12444 teleport: 1 s settle instead of 3 s; #70586 every Fight now: FightSettleMs
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
            if (fight::g_active || fight::BossModeOpen()) {  // dying never runs BossBattleModeEnd: the stale flag made DoWarp skip the re-entry (user saw the player left at the door)
                fight::CloseBossMode("F load inside the mission");
                Log("Rematch: F load during a boss fight (death / retry) - fight flag cleared so the warp re-enters the arena");
                if (g_gateWant.load()) {  // the boss script restarts and waits for the gate again: re-arm the gate request after the warp
                    g_gateReq = false;
                    g_gateSeen.store(false);
                    g_gateDue = 0;
                }
                StagedWaveReset();  // the script restarts; the next idle arms Full or Final again
            }
        }
        if (!s.st[2] && g_fightLoading && g_fightWarp && !(g_gateReq && g_fn.load() == kFnWarped)) {  // a reload (death / retry) ended: back into the arena; never during a gate intro
            g_warpPending = true;
            g_warpDue = t + 4500;  // 1.5 s after the 3 s settle the landing uses
            if (g_gateWant.load() && !g_gateDue) g_gateDue = t + 4500 + 2500;  // retry: request the gate right again after the warp
        }
        g_fightLoading = s.st[2] != 0;
        StagedWaveWatch(s, g_fightMission);
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

// #77250: one line per record at charMgr+0x2BD0. Word 0 of each 0x14 slot is a generated placement
// (3900 and up), the key the wave manager uses for woke/dead. The enemy id is slot+4. The catalogue
// star and name match that id. Kind byte +0x1A stays off this line: DataDb+0xA88 is the emaki table,
// and this log does not walk an uncounted 0x1C def array. The line stays under 120 characters.
static void LogEmakiRecords(uint64_t vb, uint32_t waves, uint64_t mb, uint64_t me) {
    if (!vb || !waves) return;
    const uint32_t nRec = waves > 8 ? 8u : waves;
    const uint32_t members = (me >= mb && mb) ? (uint32_t)((me - mb) / 0xC) : 0;
    for (uint32_t i = 0; i < nRec; ++i) {
        uint64_t eb = 0, ee = 0;
        int32_t d20 = 0;
        char line[120];
        if (!Rd(vb + (uint64_t)i * 0x28, eb) || !Rd(vb + (uint64_t)i * 0x28 + 8, ee) || ee < eb) {
            Log("Rematch: rec %u unreadable", i);
            continue;
        }
        const uint32_t nEn = (uint32_t)((ee - eb) / 0x14);
        int n = snprintf(line, sizeof(line), "Rematch: rec %u n=%u", i, nEn);
        if (n < 0) continue;
        if (n >= (int)sizeof(line)) n = (int)sizeof(line) - 1;
        if (Rd(vb + (uint64_t)i * 0x28 + 0x20, d20)) {
            char bit[24];
            snprintf(bit, sizeof(bit), " d20=%d", d20);
            if (n + (int)strlen(bit) < (int)sizeof(line) - 1) n += snprintf(line + n, sizeof(line) - (size_t)n, "%s", bit);
        }
        uint32_t shown = 0;
        const uint32_t cap = nEn > 12 ? 12u : nEn;
        for (uint32_t q = 0; q < cap; ++q) {
            uint32_t place = 0, eid = 0;
            if (!Rd(eb + (uint64_t)q * 0x14, place) || !Rd(eb + (uint64_t)q * 0x14 + 4, eid)) break;
            const char* name = nullptr;
            bool boss = false;
            for (size_t r = 0; r < g_rows.size(); ++r) {
                if (!g_rows[r].placement || g_rows[r].placement != eid) continue;
                boss = true;
                if (g_rows[r].name[0]) name = g_rows[r].name;
                break;
            }
            int woke = -1, dead = -1;
            const uint32_t mCap = members > 16 ? 16u : members;
            for (uint32_t m = 0; m < mCap; ++m) {
                uint32_t mid = 0;
                uint8_t w = 0, d = 0;
                if (!Rd(mb + (uint64_t)m * 0xC, mid) || mid != place) continue;
                Rd(mb + (uint64_t)m * 0xC + 9, w);
                Rd(mb + (uint64_t)m * 0xC + 10, d);
                woke = w;
                dead = d;
                break;
            }
            char nm[9] = "";
            if (name) {
                size_t k = 0;
                for (; name[k] && k < 8; ++k) nm[k] = (name[k] >= 0x20 && name[k] < 0x7F && name[k] != '%') ? name[k] : '?';
                nm[k] = 0;
            }
            char piece[48];
            if (nm[0] && woke >= 0) snprintf(piece, sizeof(piece), " %u:%s%s p%u w%dd%d", eid, nm, boss ? "*" : "", place, woke, dead);
            else if (nm[0]) snprintf(piece, sizeof(piece), " %u:%s%s p%u", eid, nm, boss ? "*" : "", place);
            else if (woke >= 0) snprintf(piece, sizeof(piece), " %u p%u w%dd%d", eid, place, woke, dead);
            else snprintf(piece, sizeof(piece), " %u p%u", eid, place);
            if (n + (int)strlen(piece) >= (int)sizeof(line) - 8) break;
            n += snprintf(line + n, sizeof(line) - (size_t)n, "%s", piece);
            ++shown;
        }
        const uint32_t left = nEn > shown ? nEn - shown : 0;
        if (left && n < (int)sizeof(line) - 6) n += snprintf(line + n, sizeof(line) - (size_t)n, " +%u", left);
        if (waves > nRec && i + 1 == nRec && n < (int)sizeof(line) - 6)
            snprintf(line + n, sizeof(line) - (size_t)n, " +rec");
        Log("%s", line);
    }
}

// Diagnostic for the Picture Scroll wave (#75727). Logs only when a value changes. The wave manager global 45BAC40 has no site of its own,
// so it is taken relative to the mission queue, same image. Word 0 is the active wave id (90EABC), 0xFFFF = the scroll's wave.
void LogEmakiWave() {
    uint64_t a = 0, ch = 0, vb = 0, ve = 0, mgr = 0, ref = 0;
    if (!G_QUEUE || !Rd(G_CH, a) || !a || !Rd(a, ch) || !ch) return;
    Rd(ch + 0x2BD0, vb);
    Rd(ch + 0x2BD8, ve);
    Rd(ch + 0x2AA8, ref);
    uint8_t tier = 0, byte = 0, mode = 0, noItem = 0;
    Rd(ch + 0x2BEE, tier);
    Rd(ch + 0x2BEF, byte);
    Rd(ch + 0x2BF4, mode);
    Rd(ch + 0x2BA4, noItem);
    uint16_t active = 0;
    const bool mgrOk = Rd(G_QUEUE - 0x17E0, mgr) && mgr && Rd(mgr, active);
    const uint32_t waves = ve >= vb ? (uint32_t)((ve - vb) / 0x28) : 0;
    uint32_t enemies = 0;
    for (uint32_t i = 0; i < waves && i < 8; ++i) {
        uint64_t eb = 0, ee = 0;
        if (Rd(vb + i * 0x28, eb) && Rd(vb + i * 0x28 + 8, ee) && ee >= eb) enemies += (uint32_t)((ee - eb) / 0x14);
    }
    // #75963: why the listed enemies do not appear. Manager: +2 wave no, +0x10 state (1 wait, 2 wake, 3 fight), +0x14 timer, +0x28 pause, members +0x38 (12 B: id, delay, wave, woken, dead).
    {
        static ULONGLONG lastFull = 0;
        static uint64_t lastSig = 0;
        const ULONGLONG now = GetTickCount64();
        uint8_t waveNo = 0, pause = 0, ready = 0;
        int32_t st = 0;
        float timer = 0;
        uint64_t mb = 0, me = 0, etab = 0, arr = 0, cnt = 0;
        if (mgrOk) { Rd(mgr + 2, waveNo); Rd(mgr + 0x10, st); Rd(mgr + 0x14, timer); Rd(mgr + 0x28, pause); Rd(mgr + 0x38, mb); Rd(mgr + 0x40, me); }
        Rd(ch + 0x2679, ready);
        Rd(ch + 0x48, etab);
        if (etab) { Rd(etab, arr); Rd(etab + 8, cnt); }
        const uint32_t members = me >= mb && mb ? (uint32_t)((me - mb) / 0xC) : 0;
        uint64_t sig = ((uint64_t)st << 40) ^ ((uint64_t)waveNo << 32) ^ ((uint64_t)pause << 24) ^ ((uint64_t)members << 8) ^ ready ^ ((uint64_t)active << 48);
        char ents[8][200] = {};
        for (uint32_t i = 0; i < members && i < 8; ++i) {
            uint32_t id = 0, delay = 0;
            uint8_t wn = 0, woke = 0, dead = 0;
            const uint64_t m = mb + i * 0xC;
            Rd(m, id); Rd(m + 4, delay); Rd(m + 8, wn); Rd(m + 9, woke); Rd(m + 10, dead);
            uint64_t e = 0;
            for (uint64_t k = 0; arr && k < cnt && k < 4096; ++k) {
                uint32_t kid = 0;
                if (Rd(arr + k * 0x10, kid) && kid == id) { Rd(arr + k * 0x10 + 8, e); break; }
            }
            int32_t eSt = 0, eMis = 0, e80 = 0, e88 = 0;
            uint8_t da = 0, db = 0, e0 = 0, ee = 0, f0 = 0, f4 = 0, f8 = 0;
            uint64_t actor = 0, d18 = 0, d70 = 0;
            if (e) { Rd(e, actor); Rd(e + 0x18, d18); Rd(e + 0x24, eMis); Rd(e + 0x50, eSt); Rd(e + 0x70, d70); Rd(e + 0x80, e80); Rd(e + 0x88, e88); Rd(e + 0xDA, da); Rd(e + 0xDB, db); Rd(e + 0xE0, e0); Rd(e + 0xEE, ee); Rd(e + 0xF0, f0); Rd(e + 0xF4, f4); Rd(e + 0xF8, f8); }
            sig ^= ((uint64_t)eSt << (i * 3)) ^ ((uint64_t)woke << (i + 16)) ^ ((uint64_t)(actor != 0) << (i + 24)) ^ ((uint64_t)da << (i + 32)) ^ ((uint64_t)dead << (i + 40));
            snprintf(ents[i], sizeof(ents[i]), "id %u delay %u wave %u woke %u dead %u entry %d actor %d st %d mis %X e80 %d e88 %d da %u db %u e0 %u ee %u f0 %u f4 %u f8 %u d18 %d d70 %d", id, delay, wn, woke, dead, e ? 1 : 0, actor ? 1 : 0, eSt, eMis, e80, e88, da, db, e0, ee, f0, f4, f8, d18 ? 1 : 0, d70 ? 1 : 0);
        }
        if (sig != lastSig || now - lastFull > 10000) {
            lastSig = sig;
            lastFull = now;
            uint32_t lvA = 0, lvB = 0;
            uint64_t ms = 0;
            int32_t curm[3] = {};
            Rd(ch + 0x2BF8, lvA); Rd(ch + 0x2BFC, lvB);
            if (Rd(g_img.base + Grva(0x4751848) + 8, ms) && ms) { Rd(ms, curm[0]); Rd(ms + 4, curm[1]); Rd(ms + 8, curm[2]); }
            Log("Rematch: wave probe: state %d wave %u timer %.1f pause %u ready %u members %u, level %u/%u, current %X %X %X", st, waveNo, timer, pause, ready, members, lvA, lvB, curm[0], curm[1], curm[2]);
            for (uint32_t i = 0; i < members && i < 8; ++i) Log("Rematch: wave member %u: %s", i, ents[i]);
            static uint32_t listSig = 0;
            if (listSig != (uint32_t)(vb ^ waves)) {
                listSig = (uint32_t)(vb ^ waves);
                {
                    EmakiTab et;
                    int rec = -1;
                    if (ReadEmakiTab(et)) for (int k = 0; k < et.n; ++k) {
                        uint8_t kb = 0; uint32_t ki = 0;
                        if (Rd(et.vb + (uint64_t)k * 8, kb) && kb == byte && Rd(et.vb + (uint64_t)k * 8 + 4, ki)) { rec = (int)ki; break; }
                    }
                    if (rec >= 0 && (uint32_t)rec < et.count) {
                        const uint64_t R = et.B + 8 + (uint64_t)rec * 0x34;
                        uint32_t st0 = 0, gad = 0, box = 0; float wp[4] = {};
                        Rd(R + 0xC, st0); Rd(R + 0x10, gad); Rd(R + 0x14, box); Copy(wp, R + 0x1C, sizeof(wp));
                        Log("Rematch: wave record %d (byte %02X): start %u gadget %u box %u warp %.0f %.0f %.0f", rec, byte, st0, gad, box, wp[0], wp[1], wp[2]);
                    }
                }
                LogEmakiRecords(vb, waves, mb, me);
            }
        }
    }
    static uint64_t last = 0;
    const uint64_t cur = ((uint64_t)waves << 48) ^ ((uint64_t)enemies << 32) ^ ((uint64_t)active << 16) ^ ((uint64_t)noItem << 8) ^ (mgrOk ? 1 : 0);
    if (cur == last) return;
    last = cur;
    Log("Rematch: scroll wave: list %u waves %u enemies, active wave %04X (manager %s), tier %02X byte %02X, mode %u, no-item %u, ref %016llX",
        waves, enemies, active, mgrOk ? "ok" : "none", tier, byte, mode, noItem, (unsigned long long)ref);
}

bool WaveManagerState(int32_t* st) {
    uint64_t mgr = 0;
    if (!G_QUEUE || !st || !Rd(G_QUEUE - 0x17E0, mgr) || !mgr) return false;
    return Rd(mgr + 0x10, *st);
}
bool WaveRunIsOver(int32_t st) {
    uint64_t mgr = 0, mb = 0, me = 0;
    uint8_t waveNo = 0;
    if (!G_QUEUE || !Rd(G_QUEUE - 0x17E0, mgr) || !mgr) return false;
    if (!Rd(mgr + 2, waveNo) || !Rd(mgr + 0x38, mb) || !Rd(mgr + 0x40, me)) return false;
    const uint32_t members = me >= mb && mb ? (uint32_t)((me - mb) / 0xC) : 0;
    return rcore::WaveRunOver(st, waveNo, members);
}
bool LiveRecordWarp(float wp[4]) {
    uint64_t a = 0, ch = 0;
    if (!Rd(G_CH, a) || !a || !Rd(a, ch) || !ch) return false;
    uint8_t byte = 0;
    if (!Rd(ch + 0x2BEF, byte)) return false;
    EmakiTab et;
    if (!ReadEmakiTab(et)) return false;
    int rec = -1;
    for (int k = 0; k < et.n; ++k) {
        uint8_t kb = 0;
        uint32_t ki = 0;
        if (Rd(et.vb + (uint64_t)k * 8, kb) && kb == byte && Rd(et.vb + (uint64_t)k * 8 + 4, ki)) { rec = (int)ki; break; }
    }
    if (rec < 0 || (uint32_t)rec >= et.count) return false;
    return Copy(wp, et.B + 8 + (uint64_t)rec * 0x34 + 0x1C, 16);
}
// After the scroll mission is in: warp to the live record point, then the shared offsets, until wave state is 3.
// A later return to state 1 is the clear. This does not arm BossBattleMode or a boss bar.
void WavePlaceTick(const Snap& s, ULONGLONG t) {
    if (s.st[2] || !s.player || g_wavePlace.cleared) return;
    int32_t st = 0;
    const bool ok = WaveManagerState(&st);
    const char* id = g_eRow >= 0 ? g_rows[(size_t)g_eRow].id : "waves";
    if (g_wavePlace.seenFight) {
        if (ok && WaveRunIsOver(st)) {
            g_wavePlace.cleared = 1;
            Log("Rematch: waves clear %s: state back to 1, fight ended (no boss bar)", id);
            Result("waves clear %s (state 1)", id);
        }
        return;
    }
    if (ok && st == 3) {
        g_wavePlace.seenFight = 1;
        Log("Rematch: waves fight %s: state 3 at step %d (no boss bar)", id, g_wavePlace.offset);
        Result("waves fighting %s (state 3, step %d)", id, g_wavePlace.offset);
        return;
    }
    // #78985: under the loading screen the known offset goes first and the walk is quick; a menu row never moves the player once the screen is gone.
    const int wakeStep = g_eRow >= 0 ? rcore::WaveWakeStep(g_rows[(size_t)g_eRow]) : 0;
    const uint64_t dwellMs = g_eRow >= 0 ? rcore::kWaveDwellCoverMs : rcore::kWaveDwellMs;
    const bool dwell = g_wavePlace.step >= 0 && t >= g_wavePlace.at && t - g_wavePlace.at >= dwellMs;
    const bool training = g_scrollNoWaves.load();
    const int dec = rcore::WaveNudgeDecide(g_wavePlace.step, ok ? st : 0, dwell, rcore::WaveTryCount(training, wakeStep));
    if (dec == 0 || dec == 2) return;
    if (dec == 1 && g_eRow >= 0 && !g_scrollCover && g_wavePlace.step >= 0) {
        if (!g_wavePlace.loggedStop) {
            g_wavePlace.loggedStop = 1;
            Log("Rematch: waves %s: loading screen is gone with no wave, walk stopped at try %d, the player keeps control", id, g_wavePlace.step);
        }
        return;
    }
    if (dec == 3) {
        if (!g_wavePlace.exhausted) {
            g_wavePlace.exhausted = 1;
            if (training) {
                Log("Rematch: scroll Training %s: in the arena, no waves (state %d)", id, ok ? st : -1);
                Result("training no waves %s (state %d)", id, ok ? st : -1);
            } else {
                Log("Rematch: waves %s: state stayed %d after the shared offsets", id, ok ? st : -1);
                Result("waves not started %s (state %d)", id, ok ? st : -1);
            }
        }
        return;
    }
    if (!g_wavePlace.have) {
        float live[4] = {};
        if (LiveRecordWarp(live)) {
            memcpy(g_wavePlace.base, live, sizeof live);
            g_wavePlace.have = true;
            Log("Rematch: waves %s: record warp %.0f %.0f %.0f", id, live[0], live[1], live[2]);
        } else if (g_eRow >= 0 && (g_rows[(size_t)g_eRow].warp[0] != 0.f || g_rows[(size_t)g_eRow].warp[2] != 0.f)) {
            memcpy(g_wavePlace.base, g_rows[(size_t)g_eRow].warp, sizeof g_wavePlace.base);
            g_wavePlace.have = true;
            Log("Rematch: waves %s: catalogue warp %.0f %.0f %.0f", id, g_wavePlace.base[0], g_wavePlace.base[1], g_wavePlace.base[2]);
        } else {
            if (!g_wavePlace.loggedWait) {
                g_wavePlace.loggedWait = 1;
                Log("Rematch: waves %s: record warp not readable yet", id);
            }
            return;
        }
    }
    const int next = g_wavePlace.step + 1;
    const int offset = rcore::WaveTryOffset(training, wakeStep, next);
    const rcore::WaveNudge n = rcore::WaveNudgeAt(offset);
    const float x = g_wavePlace.base[0] + n.dx;
    const float y = g_wavePlace.base[1] + n.dy;
    const float z = g_wavePlace.base[2] + n.dz;
    const int rc = CallWarp(x, y, z, g_wavePlace.base[3]);
    g_wavePlace.step = next;
    g_wavePlace.offset = offset;
    g_wavePlace.at = t;
    Log("Rematch: waves %s: warp step %d -> %.0f %.0f %.0f (%s, try %d, known step %d)", id, offset, x, y, z, rc == 0 ? "ok" : rc == 1 ? "no player" : "exception", next, wakeStep);
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
        if (g_eRow >= 0) {
            const int after = CountScrollItems();
            Log("Rematch: Q6 scroll-item count %d after exit (was %d at request)", after, g_q6Count);
        }
        Log("Rematch: Picture Scroll mission ended");
        Result("exited %s", g_eRow >= 0 ? g_rows[(size_t)g_eRow].id : "game's pick");
        ToIdle("picture scroll exit", true);
    } else if (ev == 3) {
        const bool leave = s.st[2] && s.ms28 && g_attemptPhase.load() != aQuarantined && g_attemptPhase.load() != aReturning;
        Log("Rematch: the Picture Scroll never loaded in time%s", leave ? " - return queued, the game reads it only after the load ends" : " - restored");
        if (leave) {
            AttemptReturning(s);
            const int rc = CallReturn();
            g_attempt.returnCompleted = rc == 0;
            Log("Rematch: exit: MissionRequestReturn during a stuck scroll load (mission 0x%X, retry %u) -> %s", s.ms28, s.ms14,
                rc ? "EXCEPTION" : "requested");
            ToIdle(rc ? "armed timeout return failed" : "armed timeout", rc == 0);
        } else {
            ToIdle("armed timeout");
        }
        Refuse("the Picture Scroll did not start (timeout, restored). If the loading screen stays, restart the game");
    }
    if (g_state.load() == kEmaki && g_et.st == rcore::kEIn) WavePlaceTick(s, t);
}

// ---- [Rematch] SelfTest = R1CBC,REA75,...: the add-on runs Fight now on each row by itself (no pipe, no input; for the Ally /
// Proton and any device without the pipe). Per row: wait until idle in the field for 8 s -> fight -> PASS / FAIL (the Fight now
// verification; E0 uses native scroll entry and first active wave) -> SelfTestHoldMs (default 4000) -> leave the mission -> next.
// SelfTestPlaceAt (three numbers, empty = off): 3 s after a PASS, one Player::SetPos warp. No mission-flag writes.
// Log lines 'SELFTEST ...'; never runs online.
struct SelfTest {
    std::vector<int> rows;
    size_t next = 0;
    int phase = 0;  // 0 wait idle, 1 started, 2 result seen -> exit, 3 left, 4 done
    ULONGLONG since = 0, idleSince = 0;
    int pass = 0, fail = 0, skip = 0;
    int holdMs = 4000;  // [Rematch] SelfTestHoldMs; missing or negative stays today's 4 s
    bool exitSent = false;
    bool placeOn = false;  // [Rematch] SelfTestPlaceAt parsed as x y z
    bool placed = false;
    bool lastOk = false;
    float place[3] = {};
    char table[600] = "";
} g_st;
void SelfTestTick(const Snap& s, ULONGLONG t) {
    SelfTest& st = g_st;
    if (st.rows.empty() || st.phase == 4) return;
    const int stt = g_state.load();
    if (st.phase == 0) {
        if (st.next >= st.rows.size()) {
            st.phase = 4;
            StagedWaveReset();
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
        const bool scroll = r.route == rcore::kE;
        if (!scroll && !ClearedNow(r.missionId, &why)) {
            Log("Rematch: SELFTEST %s SKIP: %s", r.id, why);
            const size_t k = strlen(st.table);
            snprintf(st.table + k, sizeof(st.table) - k, " %s=skip", r.id);
            ++st.skip;
            ++st.next;
            st.idleSince = 0;
            return;
        }
        StagedWaveReset();
        Log("Rematch: SELFTEST %s (%zu/%zu): %s", r.id, st.next + 1, st.rows.size(), scroll ? "Picture Scroll (first wave)" : "Fight now");
        Post(scroll ? kReqEmaki : kReqFight, row, "self-test");
        st.phase = 1;
        st.since = t;
        return;
    }
    const Row& r = g_rows[(size_t)st.rows[st.next]];
    if (st.phase == 1) {
        const int fn = g_fn.load();
        const bool scroll = r.route == rcore::kE;
        // ExecEmaki resets seenFight for each accepted request. A prior replay's
        // g_fn PASS/FAIL is not scroll evidence; require this row's owned scroll.
        const bool wave = scroll && stt == kEmaki && g_eRow == st.rows[st.next] &&
            g_et.st == rcore::kEIn && s.msOk && r.storyMission && s.ms28 == r.storyMission &&
            s.emakiOk && s.emaki && !s.st[2] &&
            s.player && !s.online && !s.guest && !g_scrollNoWaves.load() &&
            AttemptEffectsAllowed() && g_wavePlace.seenFight;
        if (!scroll) StagedWaveWatch(s, r.missionId);
        const bool over = scroll ? wave : (fn == kFnPass || fn == kFnFail);
        const bool gaveUp = (stt == kIdle && t - st.since > 15000 &&
            (scroll || (fn != kFnWarped && fn != kFnArmed))) || t - st.since > 300000;
        if (!over && !gaveUp) return;
        const bool ok = scroll ? wave : fn == kFnPass;
        if (ok && scroll) Log("Rematch: SELFTEST %s first wave active (native wave state 3 observed); leaving without waiting for a clear", r.id);
        float p[4] = {};
        PlayerPos(p);
        Log("Rematch: SELFTEST %s %s (%.1f s after the request, %d warp%s, spawn %s, player %.0f %.0f %.0f)", r.id, ok ? "PASS" : "FAIL",
            (double)(t - st.since) / 1000.0, g_warps, g_warps == 1 ? "" : "s", g_sp.last, p[0], p[1], p[2]);
        const size_t k = strlen(st.table);
        snprintf(st.table + k, sizeof(st.table) - k, " %s=%s", r.id, ok ? "PASS" : "FAIL");
        (ok ? st.pass : st.fail)++;
        st.exitSent = false;
        st.lastOk = ok;
        st.placed = false;
        st.phase = stt == kIdle ? 3 : 2;
        st.since = t;
        return;
    }
    if (st.phase == 2) {
        // Test-only. One warp, 3 s after PASS, while the hold is still counting. Loading waits. A FAIL never moves.
        // A mission with staged invade waves is not placed: the script thread requests those waves instead.
        uint16_t waveN = 0;
        const bool staged = NativeEntryWaves(r.missionId, &waveN) && waveN;
        if (st.placeOn && staged && !st.placed) {
            st.placed = true;
            Log("Rematch: SELFTEST place skipped - staged waves run on the script thread");
        }
        if (!staged && st.placeOn && st.lastOk && !st.placed && t - st.since >= 3000 && !(s.st[2] || s.st[3] || s.st[4] || !s.player)) {
            st.placed = true;
            if (!g_warpOk) {
                Log("Rematch: SELFTEST place skipped - the Player::SetPos path is off");
            } else {
                float before[4] = {}, after[4] = {};
                PlayerPos(before);
                const int rc = CallWarp(st.place[0], st.place[1], st.place[2], 0.f);
                PlayerPos(after);
                Log("Rematch: SELFTEST place %.0f %.0f %.0f -> %s (step %d); player %.0f %.0f %.0f -> %.0f %.0f %.0f",
                    st.place[0], st.place[1], st.place[2], rc == 0 ? "ok" : rc == 1 ? "NO PLAYER" : "EXCEPTION", g_warpStep,
                    before[0], before[1], before[2], after[0], after[1], after[2]);
            }
        }
        const ULONGLONG hold = (ULONGLONG)st.holdMs;
        if (t - st.since < hold) return;
        if (stt == kIdle) {
            st.phase = 3;
            st.since = t;
            return;
        }
        if (!st.exitSent) {
            st.exitSent = true;
            Post(kReqExit, -1, "self-test");
        } else if (t - st.since > 120000) {  // never left: try again; stamp so this hold is already elapsed
            Post(kReqExit, -1, "self-test (again)");
            st.since = t - hold;
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
    if (g_trainingDestination) return;
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
    for(const auto& a:lifeAnchors){char bytes[256];const uint64_t at=g_img.base+Grva(a.rva);
        if(at<g_img.base||at+a.n>g_img.end||!Copy(bytes,at,a.n)||memcmp(bytes,a.bytes,a.n))return false;}
    for(const auto& a:anchors){char bytes[16];const uint64_t at=g_img.base+Grva(a.rva);
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
    if (g_wv.hpPlace && (uint32_t)placement == g_wv.hpPlace) InterlockedExchange(&g_wv.bossSeen, 1);
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

uint32_t NativeGateMission() {
    using Fn = uint32_t (*)();
    const auto f = (Fn)TrainingExport("Nioh3Training_NativeGateMission");
    return f ? f() : 0;
}
// #78735: the scroll's end object asks the same 'end the mission' box. While our Waves row runs, our gate owns it.
bool WavesGateNow() {
    const int row = g_activeRow.load();
    if (g_state.load() != kEmaki || g_et.st != rcore::kEIn || row < 0 || (size_t)row >= g_rows.size() || !rcore::WavesRow(g_rows[(size_t)row])) return false;
    const Snap s = ReadSnap();
    const uint32_t scroll = rcore::ScrollMission(g_rows, row);  // the row's catalogue story_mission (was 0xCC96)
    return s.msOk && scroll && s.ms28 == scroll;
}
// #121854 (human v1.8.24): the won native attempt retired at the gate (#80065, so a new Fight now is accepted) left g_activeRow -1,
// so the gate page's Fight again refused 'native boss row is no longer owned' and only Return worked. The retired win keeps owning
// that gate's Fight again while the pump is idle, in the same mission, with no newer attempt (any AttemptBegin / AttemptReturning
// bumps g_attemptSequence) and no load since. Same rule for every boss.
std::atomic<int> g_retiredRow{-1};
std::atomic<uint32_t> g_retiredMission{0};
std::atomic<uint64_t> g_retiredSequence{0};
int RetiredGateRow(const Snap& s) {
    const int row = g_retiredRow.load();
    if (row < 0 || (size_t)row >= g_rows.size() || g_retiredSequence.load() != g_attemptSequence) return -1;
    if (g_attemptPhase.load() != aNone || g_state.load() != kIdle || g_activeRow.load() >= 0) return -1;
    if (!s.msOk || !s.ms28 || s.ms28 != g_retiredMission.load()) return -1;
    return row;
}
void RetireWonAttempt() {
    const int row = g_attempt.row;
    const uint32_t mission = g_attempt.mission;
    Log("Rematch: native attempt won and settled at the gate; retired so a new Fight now is accepted");
    ToIdle("won native attempt settled at the gate", true);
    if (g_attemptPhase.load() != aNone || row < 0) return;  // quarantined or no row: the gate offers no Fight again
    g_retiredMission.store(mission);
    g_retiredSequence.store(g_attemptSequence);
    g_retiredRow.store(row);
    Log("Rematch: the retired win keeps the gate's Fight again for row %d (mission 0x%X)", row, mission);
}
int NativeGateRow() {
    if (WavesGateNow()) return g_activeRow.load();
    const uint32_t mission = NativeGateMission();
    if (!mission) return -1;
    const int row = g_activeRow.load();
    if (row >= 0 && (size_t)row < g_rows.size() && g_rows[(size_t)row].missionId == mission) return row;
    const int won = RetiredGateRow(ReadSnap());
    return won >= 0 && g_rows[(size_t)won].missionId == mission ? won : -1;
}
// The game thread revalidates a queued native Fight again: the open attempt, or the won one retired at this gate (#121854).
bool NativeAgainOwned(int row, const Snap& s) {
    if (row < 0 || (size_t)row >= g_rows.size()) return false;
    if (RetiredGateRow(s) != row && (g_attemptPhase.load() != aActive || g_attempt.row != row || g_attempt.mission != s.ms28)) return false;
    if (!WavesGateNow() && g_rows[(size_t)row].missionId != NativeGateMission()) return false;
    return true;
}
void ScrollCoverTick(const Snap& s, ULONGLONG t) {
    if (!g_scrollCover) return;
    // Fight again (pc2 #78852): the screen stays up for the trip home, then ExecEmaki restarts it for the trip back.
    if (g_nativeAgainRow >= 0 || g_req.load()) { g_scrollCoverIn = 0; return; }
    const bool in = g_et.st == rcore::kEIn && !s.st[2] && s.player;
    if (!in) g_scrollCoverIn = 0;
    else if (!g_scrollCoverIn) g_scrollCoverIn = t;
    const int done = rcore::ScrollCoverDone(g_state.load() == kEmaki, in, g_wavePlace.seenFight != 0, g_wavePlace.exhausted != 0, g_scrollCoverIn ? t - g_scrollCoverIn : 0);
    if (!done) return;
    g_scrollCover = false;
    ScrollCoverSet(nullptr, 0);
    Log("Rematch: scroll cover off (+%llu ms, %s)", (unsigned long long)(t - g_scrollCoverAt),
        done == 1 ? "trip ended" : done == 2 ? "waves running" : done == 3 ? "walk found no wave" : "safety timeout, no wave");
}
// #100232 (RE notes/retry-home-leg-100093.md): native gate 'Fight again' = MissionRequestReturn (home stage, 45-61 s of a 77-124 s
// retry on PC2, file #7507) and then a fresh Fight now. The loader has no same-mission guard and the game's own reload
// (MissionStartFromSave 0x2237244) re-requests the running mission by id, so the Fight now request can go out at once from the arena.
// Opt-in ([Rematch] FightAgainDirect = 1). Waves rows keep the scroll path. kFadStarted: the direct request (or an in-place swap) started.
// kFadFallback: nothing was changed beyond a normal idle restore, and the caller takes the old Return-then-Fight-now path.
// kFadRefused: stopped with exactly one message already posted; the caller must not fall back (review #101032 notes 1 and 2).
enum FadResult { kFadFallback, kFadStarted, kFadRefused };
FadResult FightAgainDirect(int row, const Snap& s, ULONGLONG t) {
    if (!g_cfg.fightAgainDirect || row < 0 || (size_t)row >= g_rows.size() || rcore::WavesRow(g_rows[(size_t)row])) return kFadFallback;
    if (g_attemptPhase.load() != aActive) return kFadFallback;
    ToIdle("fight again direct: retire the won/lost attempt", true);
    // #101032 note 1: a failed spawn/map restore quarantines the attempt. Quarantine forbids any automatic recovery return, so never
    // fall back to AttemptReturning/MissionRequestReturn from here.
    if (g_attemptPhase.load() == aQuarantined) {
        Log("Rematch: Fight again direct %s: attempt quarantined while retiring; no return", g_rows[(size_t)row].id);
        Refuse("native request uncertain; no automatic recovery return");
        return kFadRefused;
    }
    if (g_attemptPhase.load() != aNone) { Log("Rematch: Fight again direct %s: attempt did not retire cleanly; using the return path", g_rows[(size_t)row].id); return kFadFallback; }
    // #100425 live: Gates() refuses 'in a boss fight' (fight::g_active), which is the normal state at a Fight again (the same boss is
    // still up or just beaten). That check is last before the idle-state check (idle after ToIdle), so it is accepted here; every
    // earlier gate (loading, queue busy, scroll) still sends the retry down the return path.
    // #100792 (PC2 #100780, 0/2 direct): the Fight again click comes from our pause-menu Return page, which pauses the game
    // (st[3]|st[4]); the menu path already passed ExitGate, so this is the menu side (uiSide = true), as every other menu action.
    const bool paused = (s.st[3] | s.st[4]) != 0;
    if (const char* why = Gates(s, true)) {
        if (strcmp(why, "in a boss fight") != 0) { Log("Rematch: Fight again direct %s: gate '%s'; using the return path", g_rows[(size_t)row].id, why); return kFadFallback; }
        Log("Rematch: Fight again direct %s: boss fight still flagged active - expected at a retry, continuing", g_rows[(size_t)row].id);
    }
    Log("Rematch: Fight again direct %s: gates passed (menu side; game %s by our menu; not loading, queue free, no scroll, idle, frame tick live)",
        g_rows[(size_t)row].id, paused ? "paused" : "not paused");
    // #100425 (deepseek/4 note D/462, notes/load-fix-99501.md L1): what the stock Return leg does that a direct ById skips. Logged per retry.
    {
        const Row& r = g_rows[(size_t)row];
        uint64_t g = 0, ms = 0;
        uint8_t b10 = 0, b11 = 0, b18 = 0, b19 = 0;
        uint32_t mode = 0;
        const bool rd = Rd(G_MS, g) && g && Rd(g + 8, ms) && ms && Rd(ms + 0x10, b10) && Rd(ms + 0x11, b11) && Rd(ms + 0x18, b18) &&
                        Rd(ms + 0x19, b19) && Rd(ms + 0x24, mode);
        // 0x223B3F8 mode-1 step: MissionMgr slot mission == 0x497 -> 0x2186C60(Mgr, 0x4B0) + 0x1D112E0 + 0x22181B4 (story hand-off, not a boss row).
        const bool step497 = s.ms28 == 0x497 || r.missionId == 0x497;
        // MissionRequestReturn routes slot 0xC0 with params+0xD != 0 to 0x3D08A; our params {101 1000000 10100 0 0 0} have +0xD = 0.
        // 0x21DDBCC warps in place (0x215FDE4) when +0x19 == 0xC0, +0x18 == +0x10 and +0x11 == 0xC0.
        const bool inPlace = rd && b19 == 0xC0 && b18 == b10 && b11 == 0xC0;
        Log("Rematch: Fight again direct %s: L1 check: mission state %s (mode +24h %u, slot +10h %02X, +11h %02X, return tuple +18h %02X +19h %02X); "
            "catalogue id 0x%X %s running 0x%X (remap 1C17BE4 %s); params +0Dh 0 -> no 3D08A route; mode-1 step 497 (2186C60 Mgr,4B0) would run: %s; "
            "mode-1 MissionLoadStep 1C03A180 skipped (body unknown, not called); 21DDBCC warp-in-place (215FDE4) would apply: %s",
            r.id, rd ? "read" : "NOT readable", mode, b10, b11, b18, b19, r.missionId, r.missionId == s.ms28 ? "==" : "!=", s.ms28,
            r.missionId == s.ms28 ? "already applied by the game for this load" : "differs - the catalogue id is requested as Fight now does", step497 ? "YES" : "no",
            inPlace ? "yes (not used in this build)" : "no");
        if (step497) { Log("Rematch: Fight again direct %s: mission 0x497 needs the mode-1 hand-off; using the return path", r.id); return kFadFallback; }
    }
    // #101099 (PC2 #101089: R3513/R7B35/R4D8E gate armed, never requested): the boss mode never ended before this same-mission reload, so
    // the stale flag made GateTick wait forever ('boss fight still flagged active') and the WaitWarpStart boss never spawned. A reload during a
    // fight is exactly the death/retry case in the mission tracker, which clears the flag the same way; the new boss mode sets it again.
    if (InterlockedCompareExchange(&fight::g_active, 0, 0) || fight::BossModeOpen()) {
        fight::CloseBossMode("Fight again direct reload");
        Log("Rematch: Fight again direct %s: boss fight flag cleared for the reload (as a death / retry does), so the boss gate is requested again", g_rows[(size_t)row].id);
    }
    const uint64_t seq0 = g_attemptSequence;
    Log("Rematch: Fight again direct %s: MissionRequestById for mission 0x%X from mission 0x%X (retry point %u), no MissionRequestReturn home leg",
        g_rows[(size_t)row].id, g_rows[(size_t)row].missionId, s.ms28, s.ms14);
    ExecDirect(row, s, t, true, true);
    const bool started = g_swappedInPlace.load() || (g_state.load() != kIdle && g_attemptSequence != seq0);
    // #101032 note 2: ExecDirect already posted its refusal (one message). Do not add a return leg and a second 'exit requested'.
    if (!started) Log("Rematch: Fight again direct %s: request refused; no return leg", g_rows[(size_t)row].id);
    return started ? kFadStarted : kFadRefused;
}
void NativeGateRetryTick(const Snap& s, ULONGLONG t) {
    if (g_nativeAgainRow < 0) return;
    // Ally #105245: the 120 s limit counted from the Fight again press, but a slow device spent ~2 min on the
    // return trip home itself, so every retry was cancelled while still loading. The idle limit now counts from
    // the last tick on which the return was still in progress (load, gates, pending request); a hard cap from
    // the press still ends a real hang.
    static constexpr ULONGLONG kIdleMs = 120000, kHardCapMs = 600000;
    static ULONGLONG busyAt = 0, busyFor = 0;
    if (busyFor != g_nativeAgainAt) { busyFor = g_nativeAgainAt; busyAt = g_nativeAgainAt; }
    const bool timedOut = t - busyAt > kIdleMs || t - g_nativeAgainAt > kHardCapMs;
    if (g_requestEpoch.load() != g_nativeAgainEpoch || g_attemptSequence != g_nativeAgainSequence ||
        g_attemptPhase.load() == aQuarantined || timedOut || SessionGate(s)) {
        if (timedOut)
            Log("Rematch: native gate retry cancelled: timeout (%llu ms since the return was last busy, %llu ms since the press)",
                (unsigned long long)(t - busyAt), (unsigned long long)(t - g_nativeAgainAt));
        else
            Log("Rematch: native gate retry cancelled (new action, changed attempt, session loss or timeout)");
        g_nativeAgainRow = -1; return;
    }
    // Never overlap the old return, restored placement transaction, or any new request.
    // Crash #78: the boss request went out 1.3 s after the home load ended. The new load tore down home
    // resources that worker jobs were still stage-polling (ResourceIsReady 1A6030, handler +20h = 0, AV).
    // A player cannot ask that fast, so wait until home has been clear of every gate for a settle window.
    static constexpr ULONGLONG kSettleMs = 5000;
    // Ally RC2 #105680/#105916: 'home is up' twice ~10.7 s apart = the window really restarted (a gate closed inside it), not a
    // duplicate line. The interruption is now logged with its reason so a device log shows which gate re-closed.
    // #106167 (Ally #106152): 'mission queue busy' is our own 10 s cooldown after any queue change (ReadSnap queueBusy), not a home
    // change: loading, pause and no-player are checked before it in Gates(). Inside an open window it no longer restarts the 5 s
    // settle; the clock keeps running and the boss request waits until the queue is free. Every other gate (or one hidden behind the
    // queue reason) still restarts the full wait. Same rule for every boss.
    static ULONGLONG clearSince = 0, clearFor = 0, windowsFor = 0, coolAt = 0, busySince = 0;
    static bool clear = false;
    static int windows = 0;
    if (windowsFor != g_nativeAgainAt) { windowsFor = g_nativeAgainAt; windows = 0; coolAt = 0; busySince = 0; }
    const char* busy = g_attemptPhase.load() != aNone ? "native attempt still open"
                       : g_state.load() != kIdle      ? "rematch active"
                       : g_req.load()                 ? "request queued"
                       : g_pend                       ? "request pending"
                                                      : Gates(s, false);
    const bool open = clear && clearFor == g_nativeAgainAt;
    if (busy && open && !strcmp(busy, "mission queue busy")) {
        Snap rest = s;
        rest.queueBusy = false;
        if (const char* other = Gates(rest, false)) busy = other;  // a gate behind the queue reason is a real interruption
        else {
            busyAt = t;
            if (!coolAt) {
                coolAt = t;
                Log("Rematch: native gate retry: mission queue cooldown %llu ms into settle window %d; the settle keeps counting, the boss request waits for the queue",
                    (unsigned long long)(t - clearSince), windows);
            }
            return;
        }
    }
    if (busy) {
        if (open)
            Log("Rematch: native gate retry: settle interrupted after %llu ms (%s); the %llu ms wait restarts once home is clear again",
                (unsigned long long)(t - clearSince), busy, (unsigned long long)kSettleMs);
        if (open || !busySince) busySince = t;
        clear = false; coolAt = 0;
        busyAt = t;  // still returning: refresh the idle limit
        return;
    }
    if (coolAt && open) {
        Log("Rematch: native gate retry: mission queue free after %llu ms of cooldown; window %d has settled %llu ms",
            (unsigned long long)(t - coolAt), windows, (unsigned long long)(t - clearSince));
        coolAt = 0;
    }
    if (!open) {
        const ULONGLONG busyMs = busySince ? t - busySince : 0;
        clear = true; clearFor = g_nativeAgainAt; clearSince = t; ++windows; busySince = 0; coolAt = 0;
        if (windows > 1)
            Log("Rematch: native gate retry: home is up, waiting %llu ms for it to settle before the boss request (window %d, %llu ms after the press, home was busy %llu ms)",
                (unsigned long long)kSettleMs, windows, (unsigned long long)(t - g_nativeAgainAt), (unsigned long long)busyMs);
        else
            Log("Rematch: native gate retry: home is up, waiting %llu ms for it to settle before the boss request (window %d, %llu ms after the press)",
                (unsigned long long)kSettleMs, windows, (unsigned long long)(t - g_nativeAgainAt));
        return;
    }
    if (t - clearSince < kSettleMs) return;
    clear = false;
    const int row = g_nativeAgainRow; g_nativeAgainRow = -1;
    Post(rcore::WavesRow(g_rows[(size_t)row]) ? kReqEmaki : kReqFight, row, "native gate after completed return");
}

void ExecField(int row, const Snap& s) {
    if (row < 0 || (size_t)row >= g_rows.size() || !g_rows[(size_t)row].OpenWorld()) {
        Refuse("not an open-world boss");
        return;
    }
    const Row& r = g_rows[(size_t)row];
    using Fn = int (*)(float, float, float, float, uint32_t);
    const auto arrive = (Fn)TrainingExport("Nioh3Training_FieldArrive");
    if (!arrive) {
        Refuse("Training field arrive is missing");
        return;
    }
    // #89181: Training warps, holds the player until the destination cell has its collision mark, then summons.
    const int rc = arrive(r.warp[0], r.warp[1], r.warp[2], r.warp[3], r.summonChr);
    Log("Rematch: open world %s region 0x%X running 0x%X placement %u chr %u arrive %d (no flag writes)",
        r.id, r.missionId, s.ms28, r.placement, r.summonChr, rc);
    if (r.missionId && s.msOk && s.ms28 && r.missionId != s.ms28)
        Log("Rematch: open world region mismatch: spawn mission 0x%X, running 0x%X", r.missionId, s.ms28);
    if (!rc) Refuse("field arrive busy");
    else Result("open world travelling");
}

void AutoMovesTick();  // pc2 #23907, end of file
void Tick() {
    const ULONGLONG t = Ms();
    g_lastTick.store(t);
    { static ULONGLONG nx; if (t >= nx) { nx = t + 1000; AutoMovesTick(); } }
    Snap s = ReadSnap();
    UnbeatenTick(s, t);
    CoverTick(s,t);
    AttemptObserve(s, t);
    if (g_attemptWonDone.exchange(false) && g_attemptPhase.load() == aActive) RetireWonAttempt();
    if (s.st[2] && g_retiredRow.load() >= 0) g_retiredRow.store(-1);  // a load (return, reload, new mission) ends the retired gate
    if (g_resume.armed.load()) {
        const bool sess = g_resume.inPlace.load() && (TrainingOwnsFight() || fight::g_active || g_fight.load() || g_swappedInPlace.load());
        g_resume.session.store(sess ? 1 : 0);
        uint32_t live = 0;
        const bool ok = ReadLiveMission(live);
        const int act = rcore::ResumeHold(true, g_resume.trip.load(), g_resume.source.load(), live, ok, s.st[2] != 0, sess,
                                           g_resume.entered.load() != 0);
        if (act == 1) {
            g_resume.entered.store(1);
            g_resume.homeSince = 0;
        } else if (act == 2 && s.player && !s.st[3] && !s.st[4]) {
            if (!g_resume.homeSince) g_resume.homeSince = t;
            else if (t - g_resume.homeSince >= 1000) {
                const int held = g_resume.skipped.exchange(0);
                const uint32_t trip = g_resume.trip.load();
                g_resume.armed.store(0);
                g_resume.session.store(0);
                Log("Rematch: resume home. Held %d save(s) for trip 0x%X; native autosave now (mission 0x%X)", held, trip, live);
                if (g_autoSave) g_autoSave();
                else Log("Rematch: resume home autosave was not installed");
            }
        } else if (act == 0 && !g_resume.entered.load() && g_resume.armedAt && t - g_resume.armedAt > 20000) {
            g_resume.armed.store(0);
            Log("Rematch: resume hold dropped. The trip never became the resume mission");
        }
    }
    if (g_resume.deferMap.load()) {
        uint32_t live = 0;
        const bool ok = ReadLiveMission(live);
        const bool still = g_resume.armed.load() && rcore::ResumeBlocksWrite(true, g_resume.trip.load(), g_resume.source.load(), live, ok,
                                                                              g_resume.session.load() != 0);
        if (!still || (g_resume.deferAt && t - g_resume.deferAt > 8000)) {
            MapRestoreNow(still ? "resume hold 8 s" : "resume mission left the trip");
            g_resume.deferMap.store(0);
        }
    }
    if (g_attemptPhase.load() == aReturning && g_attempt.standaloneDone) ToIdle("native return landing observed", true);
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
        static rcore::StaleBossModeClose g_staleBossMode;  // #107117 / #107156: Leave Boss Match closes the boss mode after the win window
        if (s.msOk && s.ms28 != lastMission) {
            if (rcore::StaleBossModeOnMissionChange(fight::g_active != 0, fight::BossModeOpen(), lastMission)) {  // #107117: Leave Boss Match
                InterlockedExchange(&fight::g_active, 0);
                if (fight::BossModeOpen()) g_staleBossMode.Arm(t, fight::g_fights);  // #107156: open state closes after the win window
                Log("Rematch: the mission changed (0x%X -> 0x%X) during a boss fight without BossBattleModeEnd - fight flag cleared%s", lastMission,
                    s.ms28, fight::BossModeOpen() ? "; boss mode closes in 20 s unless BossBattleModeEnd" : "");
            }
            if (s.ms28) g_staleBbm.store(false);  // user 20261018: a mission runs: not the no-boss arena mode
            lastMission = s.ms28;
        }
        if (g_staleBossMode.Due(t, fight::g_fights, fight::BossModeOpen()))
            fight::CloseBossMode("mission changed, no BossBattleModeEnd within 20 s");
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
        if (kind == kReqExit || kind == kReqPracticeExit || kind == kReqNativeAgain) {
            const char* why = nullptr;
            if (kind == kReqNativeAgain && !NativeAgainOwned(row, s))
                why = "native gate no longer owns this fight";
            else if (!g_exitOk) why = g_exitWhy;
            else if (kind == kReqPracticeExit) why = PracticeReturnGate(s);
            else if (const char* exitWhy = ExitGate(s)) why = exitWhy;
            else why = AttemptReturnGate();
            // A stuck scroll load never settles. Send the return now. Any other "loading" or
            // "not settled" exit stays queued: the pipe already said queued, and dropping it
            // left the next row "still pending" after a 0.6-11 s boss-intro load.
            const bool scrollStuck = why && ScrollLoadStuck(s) && !strcmp(why, "native replay has not settled");
            const bool hold = why && !scrollStuck && (!strcmp(why, "loading") || !strcmp(why, "native replay has not settled"));
            if (hold && t - g_pendAt < 20000) {
                static ULONGLONG holdLog = 0;
                if (!holdLog || t - holdLog >= 2000) {
                    holdLog = t;
                    Log("Rematch: exit held (%s); it stays queued until the mission can leave", why);
                }
            } else if (why && !scrollStuck) {
                g_pend = 0;
                Refuse(hold ? "exit stayed queued 20 s and the mission was still loading or not settled" : why);
            } else {
                g_pend = 0;
                const FadResult fad = kind == kReqNativeAgain && !scrollStuck ? FightAgainDirect(row, s, t) : kFadFallback;
                if (fad == kFadStarted) {
                    Result("fight again requested directly");
                } else if (fad == kFadRefused) {
                    // one message already posted by FightAgainDirect / ExecDirect
                } else {
                AttemptReturning(s);
                const int rc = CallReturn();
                g_attempt.returnCompleted = rc == 0;
                if (rc) AttemptQuarantine("return call exception");
                if (!rc && kind == kReqNativeAgain) {
                    g_nativeAgainRow = row; g_nativeAgainAt = t;
                    g_nativeAgainSequence = g_attemptSequence; g_nativeAgainEpoch = g_requestEpoch.load();
                    if (rcore::WavesRow(g_rows[(size_t)row])) ScrollCoverOn(g_rows[(size_t)row].name, t);
                    Log("Rematch: native gate retry waits for return cleanup (%s)", g_rows[(size_t)row].id);
                }
                if (scrollStuck) Log("Rematch: exit: MissionRequestReturn during a stuck scroll load (mission 0x%X, retry %u) -> %s", s.ms28, s.ms14, rc ? "EXCEPTION" : "requested");
                Log("Rematch: exit: MissionRequestReturn(params, 1) from mission 0x%X (retry point %u) -> %s", s.ms28, s.ms14, rc ? "EXCEPTION" : "requested");
                Result(rc ? "exit failed" : "exit requested");
                }
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
            // The queue stays busy for 10 s after a return. Five seconds is not enough to start the next scroll.
            const ULONGLONG limit = !strcmp(why, "mission queue busy") ? 12000 : 5000;
            if (t - g_pendAt > limit) {
                g_pend = 0;
                Refuse(why);
            }
        } else {
            g_pend = 0;
            if (kind == kReqDirect) ExecDirect(row, s, t, false, false);
            else if (kind == kReqFight) ExecDirect(row, s, t, true, true);
            else if (kind == kReqShrine) ExecDirect(row, s, t, true, false);
            else if (kind == kReqTeleport) { g_noSwap = true; ExecDirect(row, s, t, true, true); g_noSwap = false; }
            else if (kind == kReqField) ExecField(row, s);
            else if (kind == kReqPractice) ExecPractice(row, s, t);
            else if (kind == kReqEmaki) ExecEmaki(row, t);
            else if (kind == kReqEmakiTrain) ExecEmaki(row, t, true);
            else if (kind == kReqScroll) ExecEmaki(-1, t);
            else ExecOpen(row, s);
        }
    }
    TrackReplay(s, t);
    TfTick(s, t);
    SelfTestTick(s, t);
    StagedPlaceTick(s);
    ScrollCoverTick(s, t);
    if (g_state.load() == kEmaki) LogEmakiWave();
    if (g_scGive.exchange(false)) ExecScrollGive();
    TrackEmaki(s, t);
    NativeGateRetryTick(s, t);
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

// Script-native args, same layout as bosslog ReadVmArgs. POD only: this runs before the original, and it does not change the game.
int LessonVmArgs(uint64_t ctx, uint32_t* val, uint16_t* ty, int maxn) {
    __try {
        const uint64_t vm = *(const uint64_t*)(ctx + 8);
        const uint64_t x = *(const uint64_t*)(vm + 8);
        const int nargs = *(const uint16_t*)(x + 0xA6);
        const uint64_t stk = *(const uint64_t*)(vm + 0x20);
        const int top = *(const uint16_t*)stk;
        const uint64_t arr = *(const uint64_t*)(vm + 0x18);
        if (nargs > 32 || top < nargs) return -1;
        const int base = top - nargs;
        for (int i = 0; i < nargs && i < maxn; ++i) {
            val[i] = *(const uint32_t*)(arr + (uint64_t)(base + i) * 8);
            ty[i] = *(const uint16_t*)(arr + (uint64_t)(base + i) * 8 + 4);
        }
        return nargs;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return -1;
    }
}

// Lesson dojos (novice, and Fuku adept): the mission script keeps the teacher dormant until Character::IsDeathGroupEnemy
// is true for the current lesson group. The first check on a waves row is the fight start. It does not kill, warp, or write temp/mission state.
void HkLesson(uint64_t ctx, uint64_t, uint64_t, uint64_t, const uint64_t*) {
    if (!g_fight.load() || fight::g_active) return;
    uint32_t val[1] = {};
    uint16_t ty[1] = {};
    if (LessonVmArgs(ctx, val, ty, 1) < 1) return;
    static uint32_t seen[8];
    static int nseen = 0;
    static int started = 0;
    static ULONGLONG attempt = 0;
    if (g_fnFirst != attempt) { attempt = g_fnFirst; nseen = 0; started = 0; }
    for (int i = 0; i < nseen; ++i) if (seen[i] == val[0]) return;
    if (nseen < 8) seen[nseen++] = val[0];
    const int fr = g_fightRow;
    const Row* row = (fr >= 0 && (size_t)fr < g_rows.size()) ? &g_rows[(size_t)fr] : nullptr;
    const char* id = row ? row->id : "";
    Log("Rematch: lesson group %u checked, teacher fight not started%s%s", val[0], id[0] ? " - Fight now row " : "", id);
    if (!row || !rcore::WavesFightStarts(row->fightSignal, false, started != 0)) return;
    started = 1;
    Log("Rematch: waves fight start %s group %u", id, val[0]);
    using Fn = void (*)(int);
    if (const auto f = (Fn)TrainingExport("Nioh3Training_WavesFight")) f(1);
}

// #106000 white dot: UI::SetCompassDestination(Index) is the script native that puts the white dot on the compass (lesson end,
// clear gadget spot). Log-only and universal: the target comes from the game's own call, so no row needs a stored point. The
// test harness walks to the newest 'white dot' line. The last three float args are x y z (the Index form has one more arg first).
// #106298: not gated on g_fight. The lesson-end dot comes after the win retires the attempt (15 ms later on RA0A1), so the line
// names the attempt's row for 120 s after it ended; later dots log with no row. Deduped per point, so a per-frame call logs once.
static constexpr ULONGLONG kDotAfterMs = 120000;
static void LogWhiteDot(uint64_t ctx, const char* form) {
    uint32_t val[6] = {};
    uint16_t ty[6] = {};
    const int n = LessonVmArgs(ctx, val, ty, 6);
    if (n < 3 || n > 6) return;
    float xyz[3];
    for (int i = 0; i < 3; ++i) {
        if (ty[n - 3 + i] != 2) return;  // 2 = float in the VM slot (bosslog 0x13BDB0)
        memcpy(&xyz[i], &val[n - 3 + i], 4);
    }
    static float last[3];
    static ULONGLONG attempt = 0;
    if (g_fnFirst == attempt && fabsf(last[0] - xyz[0]) < 1.f && fabsf(last[1] - xyz[1]) < 1.f && fabsf(last[2] - xyz[2]) < 1.f) return;
    attempt = g_fnFirst;
    memcpy(last, xyz, sizeof(last));
    float cur[4] = {};
    const bool ok = PlayerPos(cur);
    const float dx = xyz[0] - cur[0], dz = xyz[2] - cur[2];
    const bool after = g_fightRow < 0 && g_lastFightRow >= 0 && Ms() - g_lastFightEnd < kDotAfterMs;
    const int fr = g_fightRow >= 0 ? g_fightRow : (after ? g_lastFightRow : -1);
    const char* id = (fr >= 0 && (size_t)fr < g_rows.size()) ? g_rows[(size_t)fr].id : "";
    if (ok)
        Log("Rematch: white dot %.0f %.0f %.0f (%s) - player %.0f %.0f %.0f, %.1f m flat%s%s", xyz[0], xyz[1], xyz[2], form, cur[0], cur[1], cur[2],
            sqrtf(dx * dx + dz * dz) / 100.f, id[0] ? " - Fight now row " : "", id);
    else
        Log("Rematch: white dot %.0f %.0f %.0f (%s) - player position unknown%s%s", xyz[0], xyz[1], xyz[2], form, id[0] ? " - Fight now row " : "", id);
}
void HkDotIndex(uint64_t ctx, uint64_t, uint64_t, uint64_t, const uint64_t*) { LogWhiteDot(ctx, "SetCompassDestinationIndex"); }
void HkDot(uint64_t ctx, uint64_t, uint64_t, uint64_t, const uint64_t*) { LogWhiteDot(ctx, "SetCompassDestination"); }

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
// Fight now (straight to the boss): R / H / D rows with a start point next to the boss; opt-in, so no 'verified' gate.
// A row with no placement takes the mission start from the catalogue (#89329). It does not borrow Training's arena table.
const char* CanFight(int row) {
    const Row& r = g_rows[(size_t)row];
    if (!g_cfg.fightNow) return "Fight now is off ([Rematch] FightNow = 1 turns it on)";
    if (!g_fnowOk) return g_fnowWhy;
    if (r.route == rcore::kE) {
        if (!g_emakiOk) return g_emakiWhy;
        if (r.emakiIndex < 0) return "this scroll row has no record";
        return nullptr;
    }
    if (r.route != rcore::kR && r.route != rcore::kH && r.route != rcore::kD) return "Fight now is for replayable missions only";
    if (!r.fightStart) return "no start point next to this boss in the catalogue (use the replay)";
    return nullptr;
}
const char* CanShrine(int row) {
    if (g_rows[(size_t)row].route == rcore::kE) return "a Picture Scroll has no shrine; Fight now starts this record";
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
int g_listStop = 0;  // 0 = All. Stays until the game restarts (pc2 #74482). Closing the menu does not put it back on All.
int g_trainStop = 0;  // #79069: the Training lists have their own filter, same stops as Boss Matches
rcore::ListCycle g_listCycle;
uint32_t g_pageGen0 = 0;
char g_msg[256] = "";  // pc #99125: 160 cut the RB24D confirm + WARNING text
constexpr int kItOpen = -1, kItAbort = -2, kItScroll = -3;
constexpr int kItBrowse = -4, kItBack = -5, kItClose = -6;
constexpr int kItListFilter = -68;  // human #73240: All / Multi / era / Scrolls. Not a catalogue index, not a Fight now id.
constexpr int kItStagedStart = -58;  // #88778: Full climb / Final wave. Session only. Not a catalogue index.
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
bool RowStaged(int row, uint16_t* n, uint16_t* thou, uint32_t* place) {
    if (row < 0 || (size_t)row >= g_rows.size()) return false;
    uint16_t nn = 0;
    if (!NativeEntryWaves(g_rows[(size_t)row].missionId, &nn) || nn < 2) return false;
    if (n) *n = nn;
    if (thou || place) NativeEntryWaveFinal(g_rows[(size_t)row].missionId, thou, place);
    return true;
}
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
    if (NativeGateMission() || WavesGateNow()) return 1;
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
    int n = f(chr, buf, AM_MAXMOVES, (int)sizeof(AmMove));
    if (n < 0) {  // human #126342: an older Training serves the 124-byte record (no proj): read that, unpack in place from the end, proj 0
        n = f(chr, buf, AM_MAXMOVES, AM_ABI_124);  // fits: AM_MAXMOVES x 124 < sizeof buf
        for (int i = n - 1; i >= 0; --i) { AmMove m; memcpy(&m, (const uint8_t*)buf + (size_t)i * AM_ABI_124, AM_ABI_124); m.proj = 0; buf[i] = m; }
    }
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
    // human #126342: 'Projectile' (shockwave, thrown object, spawned blast) instead of Attack / Unblockable / Wind-up / Link; the row stays one
    // line (58 columns), so an unblockable one keeps 'Unblockable<n>' in its left id and says 'unblockable' in the footer.
    if (N3MoveProj(&m) && c != kMcTwitch && c != kMcMovement && c != kMcKiBreak && c != kMcUnsorted) { snprintf(b, n, "Projectile"); return; }
    if (c == kMcGreat) { snprintf(b, n, "Unblockable"); return; }  // its hit count is in the footer (the row stays one line)
    if (m.kind == 'S') { snprintf(b, n, "Wind-up"); return; }
    if (m.kind == 'L') { snprintf(b, n, "Link"); return; }
    if (N3MoveHitCat(c) || c == kMcHit0 || c == kMcHitUnknown || c == kMcAttack || c == kMcUnnamedAtk) { snprintf(b, n, "Attack"); return; }
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
// Display only. Idle / initialization (ids 0 and 2) stays allowed; it is not a row, even with show-twitch on.
bool MoveListed(const N3Move& m) {
    if (N3MoveProtected(m.id)) return false;
    return g_showTwitch || !N3MoveTwitch(&m) || MoveAllowed(m) == 0 || MoveRepeating(m);
}
// user #14291: moves of boss chr in group c; off = how many are OFF, rpt = the repeating move if it is in this group
int MoveCatCount(uint32_t chr, int c, int* off = nullptr, const N3Move** rpt = nullptr) {
    int n = 0;
    if (off) *off = 0;
    if (rpt) *rpt = nullptr;
    for (int i = 0; i < N3_MOVE_COUNT; ++i) {
        const N3Move& m = kN3Moves[i];
        if (m.chr != chr || N3MoveCat(&m) != c || N3MoveProtected(m.id)) continue;  // hidden idle row is not a listed move
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
#if BR_SOULCORE
#include "soul_core_tuning_menu.h"  // human #58117: live LEFT soul core reveal / smoke / speed values
#endif  // BR_SOULCORE
#include "training_bosses_menu.h"  // user #14309 / #14313 / #14317 / #14336: Training Bosses, Boss X pages, timer, Sequences
static_assert(kItRepFlow != kItListFilter && kItBossLast != kItListFilter && kItStanceTimer != kItListFilter, "#126768: Training boss row ids must not reuse the arena list filter id");
constexpr int kItFight = 0x10000;  // kItFight + row = "Fight now (in the arena)" under that boss row
// One sentence for the list footer, the Fight now footer, and the post-cover toast. The catalogue lesson column
// is the script scan (IsDeathGroupEnemy reached, BossBattleMode never called). The list draws before that script runs.
static const char kLessonLine[] = "Lesson: defeat the waves, then follow the white dot and hold RB";
// human #94922: a catalogue warning (known live problem) goes in front of that row's footer. Data-driven, any row.
static void WarnFooter(char* footer, size_t nf, int row) {
    if (!footer || !nf || row < 0 || (size_t)row >= g_rows.size() || !g_rows[(size_t)row].warning[0]) return;
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "WARNING: %s %s", g_rows[(size_t)row].warning, footer);
    snprintf(footer, nf, "%s", tmp);
}
constexpr int kItShrine = 0x20000;  // kItShrine + row = "Shrine before the boss" (the start point alone)
constexpr int kItTeleport = 0x40000;
constexpr int kItArenaTrain = 0x48000;  // human #107514: kItArenaTrain + row = "Train in this arena" (no waves, no boss) on the Boss Match row page  // kItTeleport + row = "Teleport to arena (fast load)": native mission load + arena warp, no live swap
// Free gap above the live catalogue [kItMoveBase, kItMoveBase+N3CAT_CAP) and below Teleport.
// 0x30000 == kItMoveBase, so this row was move 0 ("Idle / initialization").
constexpr int kItBossTrainHere = 0x38000;
static_assert(N3CAT_CAP <= kItBossTrainHere - kItMoveBase, "Train here overlaps the move catalogue cap");
static_assert(kItBossTrainHere + kMaxRows <= kItTeleport, "Train here rows reach Teleport");
static_assert(kItBossTrainHere > kItShrine + kMaxRows, "Train here overlaps Shrine");
static_assert(kItArenaTrain >= kItTeleport + kMaxRows && kItArenaTrain + kMaxRows <= kItSummonA, "Train in this arena overlaps Teleport / Summon");
static_assert(kItFight == 0x10000, "Fight now id base matches FightNowItemId");

// ---- #79069 favorites: Left / Right on a row (no new button, works on every menu input). The file is ours, beside the mod; the game save is never touched. ----
constexpr char kFavBoss = 'B', kFavTrain = 'T';
SRWLOCK g_favLock = SRWLOCK_INIT;
std::set<std::string> g_favs;
bool g_favLoaded = false;
std::wstring FavPath() { return std::wstring(ModDir()) + L"Nioh3BossRematch_favorites.txt"; }
void FavEnsureLoaded() {
    AcquireSRWLockExclusive(&g_favLock);
    if (!g_favLoaded) {
        g_favLoaded = true;
        std::string text;
        if (ReadText(FavPath(), text)) rcore::FavParse(text, g_favs);
        Log("Rematch: favorites loaded (%d)", (int)g_favs.size());
    }
    ReleaseSRWLockExclusive(&g_favLock);
}
bool FavHas(char list, const char* id) {
    FavEnsureLoaded();
    AcquireSRWLockShared(&g_favLock);
    const bool has = g_favs.count(rcore::FavKey(list, id)) != 0;
    ReleaseSRWLockShared(&g_favLock);
    return has;
}
bool FavWriteFile(const std::string& text) {  // the complete file or the old one: a temp file, then an atomic replace
    const std::wstring path = FavPath(), tmp = path + L".tmp";
    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD wrote = 0;
    const bool ok = text.empty() || (WriteFile(h, text.data(), (DWORD)text.size(), &wrote, nullptr) && wrote == text.size());
    CloseHandle(h);
    if (!ok || !MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) { DeleteFileW(tmp.c_str()); return false; }
    return true;
}
bool FavSet(char list, const char* id, bool on) {
    FavEnsureLoaded();
    AcquireSRWLockExclusive(&g_favLock);
    if (on) g_favs.insert(rcore::FavKey(list, id)); else g_favs.erase(rcore::FavKey(list, id));
    const std::string text = rcore::FavText(g_favs);
    ReleaseSRWLockExclusive(&g_favLock);
    const bool saved = FavWriteFile(text);
    Log("Rematch: favorite %c %s %s (%s)", list, id, on ? "on" : "off", saved ? "saved" : "SAVE FAILED");
    return saved;
}
bool IsTrainListPage() { return g_uiPages.page == uTrainHere || g_uiPages.page == uTraining; }
int& ListStopRef() { return IsTrainListPage() ? g_trainStop : g_listStop; }
char ListFavKind() { return IsTrainListPage() ? kFavTrain : kFavBoss; }
void EnsureListCycle() {
    rcore::BuildListCycle(g_rows, g_listCycle, g_cfg.showAll, !IsTrainListPage());
    const int n = g_listCycle.Stops();
    int& stop = ListStopRef();
    if (stop < 0 || stop >= n) stop = 0;
}
const char* ListFilterLabel() {
    EnsureListCycle();
    return g_listCycle.Label(ListStopRef());
}
bool RowInCurrentList(const Row& r) {
    const int stop = ListStopRef();
    return rcore::RowInListStop(r, g_listCycle, stop, g_listCycle.Kind(stop) == 4 && FavHas(ListFavKind(), r.id));
}
// The catalogue row of a list item on the current page, -1 for anything else.
int FavRowOfItem(int item) {
    const int p = g_uiPages.page;
    const int row = (p == uRematch || p == uBosses) && item >= 0 && item < kItFight ? item
                  : p == uTrainHere && item >= kItTrainHere && item < kItTrainHere + kMaxRows ? item - kItTrainHere
                  : p == uTraining && item >= kItTeleport && item < kItTeleport + kMaxRows ? item - kItTeleport : -1;
    return row >= 0 && (size_t)row < g_rows.size() ? row : -1;
}
const char* FavMark(int row) { return row >= 0 && (size_t)row < g_rows.size() && FavHas(ListFavKind(), g_rows[(size_t)row].id) ? "* " : ""; }
bool FavChange(int item, int dir) {  // true = this was a list row. Right marks, Left unmarks.
    const int row = FavRowOfItem(item);
    if (row < 0 || !dir) return false;
    const Row& r = g_rows[(size_t)row];
    if (!rcore::FavIdOk(r.id)) { snprintf(g_msg, sizeof g_msg, "This row has no id, so it cannot be a favorite."); return true; }
    const bool on = dir > 0;
    g_confirm = -9;
    if (FavHas(ListFavKind(), r.id) == on) { g_msg[0] = 0; return true; }
    const bool saved = FavSet(ListFavKind(), r.id, on);
    if (!saved) snprintf(g_msg, sizeof g_msg, "Favorite changed for now, but the file could not be saved.");
    else snprintf(g_msg, sizeof g_msg, on ? "%s is a favorite." : "%s removed from favorites.", r.name);
    return true;
}
// The Training rows of an end gate: the active bosses page (stance = training mode), summon. Training gate and scroll gate share this.
template <class Add> void AddGateTrainingRows(Add&& add) {
    if (PracticeToolsAvailable()) add(kItRootTools);
    if (TrainingExport("Nioh3Training_SummonSlot")) add(kItBossSumm);
}

int ItemsBase(int* out, int cap) {
    int n = 0;
    auto add = [&](int item) { if (n < cap) out[n++] = item; };
    // Collect all eligible rows before capacity truncation, then keep original order within each favorite group.
    auto addFavoritesFirst = [&](std::vector<int>& items) {
        rcore::FavoritesFirst(items, [&](int item) {
            const int row = FavRowOfItem(item);
            return row >= 0 && FavHas(ListFavKind(), g_rows[(size_t)row].id);
        });
        for (int item : items) add(item);
    };
    // Same order the Boss Match list already used. The filter hides rows; it does not renumber them.
    auto addBossList = [&]() {
        EnsureListCycle();
        add(kItListFilter);
        std::vector<int> rows;
        // human #95418: ONE alphabetical run. It used to be three runs (beaten / unbeaten / refused), each sorted by itself, so a
        // row whose status put it in a later run (Saito Fuku (Veteran)) landed after the Z names of the earlier run.
        for (size_t i = 0; i < g_rows.size(); ++i) {
            const Row& r = g_rows[i];
            const uint8_t st = g_status[i].load();
            const int grp = r.route == rcore::kX ? 2 : (r.route == rcore::kE || st == 2) ? 0 : 1;
            if (grp == 1 && !g_cfg.showAll) continue;
            if (grp == 2 && !g_cfg.showAll) continue;
            if (r.route == rcore::kH && !g_cfg.hiddenDuels && !g_cfg.directStart && !g_cfg.showAll) continue;
            if (!RowInCurrentList(r)) continue;
            rows.push_back((int)i);
        }
        rcore::SortItemsByName(rows, 0, 0, g_rows);
        addFavoritesFirst(rows);
    };
    if (g_uiPages.page == uRoot) {  // user #13197: Settings / Boss Rematch / Practice / Summon Boss (+ Practice tools while a session is live)
        add(kItSettings);  // user #13223: Settings / Boss Rematch / Practice (Summon Boss and the tools live inside Practice)
        add(kItRootRematch);
        add(kItRootTraining);
        add(kItPlayerOpen);
        add(kItClose);
        return n;
    }
    if (g_uiPages.page == uPracticeHub) {  // user #13223 / #13293: Train here / arena / Training Bosses / Summon a boss here / Boss Matches
        if (LeaveArenaOffered()) add(kItLeaveArena);  // user 20261018: the hosted menu opens here, never on uRoot (the row was unreachable)
        const bool scrollTraining = ScrollTrainingNow() && WavesGateNow();
        const bool inArena = InTrainingArena() || scrollTraining;  // user #14333 / #14336 item 8: no 'Train here' / 'Train in a boss arena' while already in one
        if (!inArena && TrainingExport("Nioh3Training_TrainHere")) add(kItTrainHereOpen);
        if (!inArena) add(kItArenas);
        if (PracticeToolsAvailable()) add(kItRootTools);  // Training Bosses
        PstAddHubRows(add);  // Time of day / Weather / Freeze / Reset fight
        if (TrainingExport("Nioh3Training_SummonSlot")) add(kItBossSumm);
        add(kItRootRematch);  // user #13293: Boss Matches (its kill / finisher rules apply to Training and summoned bosses too)
        if (scrollTraining) add(kItReturnHome);  // #79069: no wave clear means no end object, so this is the way out. Not the first row: it leaves at one press.
        add(kItBack);
        return n;
    }
    if (g_uiPages.page == uPlayer) {  // user #14267: player options (infinite health for now)
        if (TrainingExport("Nioh3Training_SetPlayerCantDie")) add(kItCantDie);
#if BR_SOULCORE
        add(kItScForms);  // human #61869: Soul core transformations ON / OFF
        SctAddCoreRows(add);  // human #81220: per-core transformation ON / OFF
        SctAddHubRow(add); // Player > Soul core tuning
#endif  // BR_SOULCORE
        PstAddPlayerRows(add);  // user #22389
        add(kItBack);
        return n;
    }
#if BR_SOULCORE
    if (g_uiPages.page == uSoulTune) { SctAddPage(add); add(kItBack); return n; }  // human #58117
#endif  // BR_SOULCORE
    if (g_uiPages.page == uBossVitals) { BvAddPage(add); PstAddBossRows(add); add(kItBack); return n; }  // user #14320: Boss A / B health and Ki
    if (g_uiPages.page == uFightOpts) {  // user #13265: the Boss Rematch boss-fight toggles, also for Training
        AddBossFightItems(add);
        add(kItBack);
        return n;
    }
    if (g_uiPages.page == uTrainHere) {  // user #13223: every boss with a mission id, beaten first. Waves rows have no mission id.
        EnsureListCycle();
        std::vector<int> rows;
        add(kItListFilter);  // #79069
        for (size_t i = 0; i < g_rows.size(); ++i) {  // human #95418: one alphabetical run, not beaten / unbeaten runs
            if (!RowInCurrentList(g_rows[i])) continue;
            if (rcore::WavesRow(g_rows[i]) || (g_rows[i].missionId && g_rows[i].route != rcore::kE)) rows.push_back(kItTrainHere + (int)i);
        }
        rcore::SortItemsByName(rows, 0, kItTrainHere, g_rows);
        addFavoritesFirst(rows);
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
        if (TrainingExport("Nioh3Training_SetRepeatLast")) add(kItBossLast);  // human #68578
        if (TrainingExport("Nioh3Training_SetRepeatFlow")) add(kItRepFlow);  // human #122193
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
        if (g_live.load()) {
            EnsureListCycle();
            std::vector<int> rows;
            add(kItListFilter);  // #79069
            for (size_t i = 0; i < g_rows.size(); ++i) {  // human #95418: one alphabetical run
                const Row& r = g_rows[i];
                if (!RowInCurrentList(r)) continue;
                if (rcore::WavesRow(r)) { rows.push_back(kItTeleport + (int)i); continue; }
                if (r.route == rcore::kX || r.route == rcore::kE || !r.fightStart) continue;
                rows.push_back(kItTeleport + (int)i);
            }
            rcore::SortItemsByName(rows, 0, kItTeleport, g_rows);
            addFavoritesFirst(rows);
        }
        add(kItBack);
        return n;
    }
    if (g_uiPages.page == uSummon) {  // user #14336 item 7: 'Summon another boss' = boss list, the boss takes the next free letter (no slot picker)
        if (TrainingExport("Nioh3Training_SummonSlot")) {
            // user #22984: only catalogue rows Training resolves to one boss (multi-boss rows: their bosses are roster rows), then the roster
            using CanFn = int (*)(uint32_t);
            const auto can = (CanFn)TrainingExport("Nioh3Training_CanSummon");
            // human #95418: catalogue rows and roster-only bosses in one case-insensitive A-Z list (by the shown name)
            std::vector<std::pair<std::string, int>> sm;
            for (size_t i = 0; i < g_rows.size(); ++i)
                if (g_rows[i].missionId && g_rows[i].route != rcore::kE && (!can || can(g_rows[i].missionId))) sm.emplace_back(g_rows[i].name, kItSummonA + (int)i);
            const int nr = RosterCount();
            for (int i = 0; i < nr && i < kItSummonRosterEnd - kItSummonRoster; ++i) { char nm[64] = ""; RosterEntry(i, nm, sizeof nm); sm.emplace_back(nm, kItSummonRoster + i); }
            std::stable_sort(sm.begin(), sm.end(), [](const std::pair<std::string, int>& a, const std::pair<std::string, int>& b) { return rcore::CaseLess(a.first.c_str(), b.first.c_str()); });
            for (size_t k = 0; k < sm.size() && n < cap; ++k) add(sm[k].second);
        }
        add(kItBack);
        return n;
    }
    if (g_uiPages.page == uRematch) {  // the Boss Rematch menu as it was before Training was merged in
        AddBossFightItems(add);
        // user #13197: no 'Return to where I was' here (Practice tools / the arena Return gate have it)
        // pc #99495: but while a Boss Match (or our Picture Scroll) runs, its 'Leave' row is here too: after a win the game hides
        // 'Return from Mission', and Training > Active bosses was the only way out. Same item, same exit path. Not the first row.
        if (NativeGateMission() || WavesGateNow()) add(kItReturnHome);
        // user #6718: the boss list stays on the root page (beaten / startable first, then the rest)
        if (g_live.load()) addBossList();
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
        if (chr && TrainingExport("Nioh3Training_SetRepeatLast")) add(kItBossLast);  // human #68578: also for a Boss Match / Practice boss (no Boss X page)
        if (chr && TrainingExport("Nioh3Training_SetRepeatFlow")) add(kItRepFlow);  // human #122193
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
                if (m.chr != chr || N3MoveCat(&m) != g_moveCat || N3MoveProtected(m.id)) continue;
                if ((N3KiNote(m.chr, m.id) ? 0 : 1) != pass) continue;
                add(kItMoveBase + i);
            }
        add(kItBack);
        return n;
    }
    if (g_uiPages.page == uGate) {
        if (WavesGateNow()) {  // a scroll has no boss rules: same fight, or leave. #79069: and the Training rows, the arena is empty now
            if (!g_scrollNoWaves.load()) add(kItFightAgain);
            AddGateTrainingRows(add);
            add(kItReturnHome); add(kItBack); add(kItClose);
            return n;
        }
        if (rcore::ArenaOnlyGatePage(g_fight.load(), g_fightArenaOnly, NativeGateMission())) {  // #107683: pause > Return on a 'Train in this arena' trip
            AddGateTrainingRows(add);
            add(kItReturnHome); add(kItBack); add(kItClose);
            return n;
        }
        if (TrainingExport("Nioh3Training_GateFightAgain")) add(kItFightAgain);  // #9994: Fight again first, Return second
        const int mode = GateMode();  // user #13262: the gate page follows the session mode (0 = old 2-option page)
        if (mode == 2) {  // Training (#14336 item 11): short list, everything else is one level down in Training Bosses
            AddGateTrainingRows(add);
        } else if (mode == 1) {
            AddBossFightItems(add);  // Boss Rematch fight: the rematch boss-fight options
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
            if (RowStaged(g_uiBoss, nullptr, nullptr, nullptr)) add(kItStagedStart);
            add(kItFight + g_uiBoss);
            add(kItArenaTrain + g_uiBoss);  // human #107514
            add(kItBossTrainHere + g_uiBoss);
            add(kItShrine + g_uiBoss);  // user #13164: teleport moved to Training (arenas), summon to Summon Boss
        }
        add(kItBack);
        return n;
    }
    // beaten / startable first, then the rest, then X (Boss Matches browse page)
    addBossList();
    add(kItBack);
    return n;
}

#include "page_defaults_menu.h"
int Items(int* out, int cap) {
    int n = ItemsBase(out, cap);
    if (PageHasDefaults(g_uiPages.page) && cap >= 2 && n > 0 && out[n-1] == kItBack) {
        if (n < cap) { out[n-1] = kItPageDefaults; out[n++] = kItBack; }
        else { out[n-2] = kItPageDefaults; }
    }
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

// #79955: called by the UI worker every ~33 ms, outside the game's frame pump (which may not run at the title).
void TitleWatchdog() {
    static rcore::TitleWatch w;
    static ULONGLONG nextAt = 0;
    const ULONGLONG t = Ms();
    if (t < nextAt) return;
    nextAt = t + 250;
    if (g_state.load() == kIdle) { w.since = 0; return; }
    const Snap s = ReadSnap();
    // v3 (#80182): the mission field is stale at the title (still the last mission), so it is not used; an unreadable game state counts as loading
    if (w.StepSnap(true, s.player, s.gm != 0, s.st[2] != 0, t)) {
        Log("Rematch: no player, no load and no mission for 20 s (Return to Title) - idle");
        Result("exited %s (title)", RowName(g_activeRow.load()));
        ToIdle("returned to title", true);  // normal exit: restores run, a clean active/returning phase clears (a quarantine here would block every later attempt)
    }
}

uint32_t RematchScrollMission() { return rcore::ScrollMission(g_rows, g_emakiNoteRow); }

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
    g_cfg.ignoreProgress = IniInt(L"Rematch", L"IgnoreProgress", IniInt(L"Dev", L"IgnoreProgress", 0)) != 0;  // #256 ([Dev] = old #255 name)
    InstallClearGuard();
    {
        const char* why = nullptr;
        const bool ok = InstallVeto("Mission::ClearGadgetAccessWaitMode", &VetoRosterClearWait, &why);
        Log("Rematch: remaining-roster combat guard %s%s", ok ? "installed" : "NOT installed: ", ok ? "" : (why ? why : "?"));
    }
    g_cfg.fightNow = IniInt(L"Rematch", L"FightNow", 1) != 0;
    g_cfg.fightWarp = IniInt(L"Rematch", L"FightNowWarp", 1) != 0;
    g_cfg.anyBoss = IniInt(L"Rematch", L"AnyBoss", 1) != 0;
    g_cfg.arenaCapture = IniInt(L"Rematch", L"ArenaCapture", 0) != 0;
    g_cfg.fightSpawn = IniInt(L"Rematch", L"FightNowSpawn", 1) != 0;
    g_cfg.fightAgainDirect = IniInt(L"Rematch", L"FightAgainDirect", 0) != 0;
    Log("Rematch: FightAgainDirect = %d (1 = native gate Fight again requests the boss mission straight away, no trip home; #100232)", g_cfg.fightAgainDirect ? 1 : 0);
    g_cfg.fightSettleMs = IniInt(L"Rematch", L"FightSettleMs", 1000);  // #70586: load end -> "landed" for Fight now (was 3000)
    g_cfg.fightNudgeMs = IniInt(L"Rematch", L"FightNudgeMs", 1000);  // #70586: landed -> warp beside the boss when no fight began and it is out of duel range (was 10000)
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
    {
        const char* bagWhy = nullptr;
        if (!GroupOk("BAG", &bagWhy)) {
            Log("Scroll return: empty-bag unlock OFF - %s", bagWhy ? bagWhy : "?");
        } else {
            if (!InstallCallRetarget("call:ScrollReturnReady", (void*)&ScrollReturnReadyHook, (void**)&g_scrollReadyOrig, &bagWhy))
                Log("Scroll return: empty-bag unlock OFF - %s", bagWhy ? bagWhy : "?");
        }
    }
    if (g_exitOk) F_REQRETURN = Site("fn:MissionRequestReturn");
    {
        auto installWrite = [&](uint32_t rva, uint32_t callee, void* fn, void** orig, const char* name) {
            uint8_t* p = (uint8_t*)(g_img.base + rva);
            uint8_t b[5] = {};
            if (!Copy(b, (uint64_t)p, 5) || b[0] != 0xE8) {
                Log("Rematch: resume hold did not install %s (call site +%X is not a call)", name, rva);
                return;
            }
            int32_t rel = 0;
            memcpy(&rel, b + 1, 4);
            const uintptr_t dest = (uintptr_t)(p + 5 + rel);
            if (dest != g_img.base + callee) {
                Log("Rematch: resume hold did not install %s (call +%X target is +%llX)", name, rva,
                    (unsigned long long)(dest - g_img.base));
                return;
            }
            const char* why = nullptr;
            if (!InstallCallRetargetAt(p, name, fn, orig, &why))
                Log("Rematch: resume hold did not install %s (%s)", name, why ? why : "?");
        };
        void* saveOrig = nullptr;
        void* backupOrig = nullptr;
        installWrite(0x5B6F37, 0x5B7BB4, (void*)&ResumeSaveWrite, &saveOrig, "SaveFileWriteAtomic");
        installWrite(0x1FDF2D0, 0x1FD9A48, (void*)&ResumeBackupWrite, &backupOrig, "BackupBinWrite");
        void* backupOrig2 = nullptr;
        installWrite(0x1FDF62A, 0x1FD9A48, (void*)&ResumeBackupWrite, &backupOrig2, "BackupBinWrite2");
        g_saveWrite = (SaveWriteFn)saveOrig;
        g_backupWrite = (BackupWriteFn)(backupOrig ? backupOrig : backupOrig2);
        uint8_t head[4] = {};
        uint8_t* autosave = (uint8_t*)(g_img.base + 0x5B635C);
        if (Copy(head, (uint64_t)autosave, 4) && head[0] == 0x48 && head[1] == 0x83 && head[2] == 0xEC && head[3] == 0x28)
            g_autoSave = (AutosaveFn)autosave;
        else Log("Rematch: resume hold has no autosave (prologue mismatch at +5B635C)");
        if (!g_saveWrite || !g_backupWrite) Log("Rematch: resume hold is incomplete (a save writer was not hooked)");
        else Log("Rematch: resume hold on. A trip save waits until the resume mission is home");
    }
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
    OnEntry("IsDeathGroupEnemy", &HkLesson);  // log-only: which lesson group the script is still waiting on
    OnEntry("UI::SetCompassDestinationIndex", &HkDotIndex);  // log-only #106000: the white dot the test walks to
    OnEntry("UI::SetCompassDestination", &HkDot);
    if (stw[0]) {  // SelfTest = id,id,... or 'arena' (every row with an arena point)
        int hold = IniInt(L"Rematch", L"SelfTestHoldMs", 4000);
        if (hold < 0) hold = 4000;
        g_st.holdMs = hold;
        wchar_t pw[160] = {};
        GetPrivateProfileStringW(L"Rematch", L"SelfTestPlaceAt", L"", pw, 160, ini);
        char p8[160] = {};
        WideCharToMultiByte(CP_ACP, 0, pw, -1, p8, (int)sizeof(p8), nullptr, nullptr);
        float px = 0, py = 0, pz = 0;
        if (sscanf_s(p8, "%f %f %f", &px, &py, &pz) == 3) {
            g_st.placeOn = true;
            g_st.place[0] = px;
            g_st.place[1] = py;
            g_st.place[2] = pz;
            Log("Rematch: SELFTEST place armed: %.0f %.0f %.0f, 3 s after a PASS (Player::SetPos, no flag writes)", px, py, pz);
        } else if (pw[0]) {
            Log("Rematch: SELFTEST place OFF - SelfTestPlaceAt needs x y z");
        }
        char st8[512] = {};
        WideCharToMultiByte(CP_ACP, 0, stw, -1, st8, sizeof(st8), nullptr, nullptr);
        if (!_stricmp(st8, "arena")) {
            for (size_t i = 0; i < g_rows.size(); ++i)
                if ((g_rows[i].FightNowReady() || g_rows[i].route == rcore::kE) && g_rows[i].route != rcore::kX) g_st.rows.push_back((int)i);
        } else {
            char* ctx = nullptr;
            for (char* tok = strtok_s(st8, ", ", &ctx); tok; tok = strtok_s(nullptr, ", ", &ctx))
                for (size_t i = 0; i < g_rows.size(); ++i)
                    if (!_stricmp(g_rows[i].id, tok)) g_st.rows.push_back((int)i);
        }
        Log("Rematch: SELFTEST armed: %zu rows (%ls); hold %d ms; starts 8 s after the add-on is idle in the field", g_st.rows.size(), stw, g_st.holdMs);
    }
    if (IniInt(L"Rematch", L"SelfTestStagedFinal", 0)) {
        g_stagedFinal = 1;
        Log("Rematch: staged start Final from SelfTestStagedFinal (session only, before the wave arm, no save write)");
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
void CycleListFilter(int dir) {
    static int before[kMaxRows + 4];
    const int n0 = Items(before, kMaxRows + 4);
    int anchor = kItListFilter;
    if (g_sel >= 0 && g_sel < n0) anchor = before[g_sel];
    EnsureListCycle();
    const int nstop = g_listCycle.Stops();
    if (nstop < 1) return;
    int& stop = ListStopRef();
    stop += dir < 0 ? -1 : 1;
    if (stop >= nstop) stop = 0;
    if (stop < 0) stop = nstop - 1;
    const int n1 = Items(before, kMaxRows + 4);
    g_sel = rcore::KeepVisibleSel(before, n1, g_sel, anchor);
    g_confirm = -9;
    g_msg[0] = 0;
    Log("Rematch: boss list filter -> %s", ListFilterLabel());
}
uint64_t NavNowMs() { return GetTickCount64(); }
void UiChange(int dir) {
    static int items[kMaxRows + 4];
    const int n = Items(items, kMaxRows + 4);
    if (!n || g_sel < 0) return;  // #38077: the two top rows (Mod menu / Close) have no Left / Right
    if (g_sel >= n) g_sel = n - 1;
    const int item = items[g_sel];
    static rcore::FavHoldGuard favHold;  // #89941: a held Left must not walk down the Favorites list removing each row
    const uint64_t now = NavNowMs();
    if (dir && favHold.Swallow(now, item)) return;
    if (item == kItListFilter) { CycleListFilter(dir); return; }
    if (item == kItStagedStart) {
        g_stagedFinal = dir == 0 ? !g_stagedFinal : dir > 0;
        g_msg[0] = 0;
        Log("Rematch: staged start -> %s", g_stagedFinal ? "final" : "full");
        return;
    }
    if (item == kItPageDefaults) return; // action: A only
    if (FavChange(item, dir)) {
        const int count = Items(items, kMaxRows + 4);
        g_sel = rcore::KeepVisibleSel(items, count, g_sel, item); // follow the same row after promotion/demotion
        if (g_sel < 0 || g_sel >= count || items[g_sel] != item)  // the row left this list: guard the row the cursor landed on
            favHold.Lock(now, count > 0 ? items[g_sel < 0 ? 0 : (g_sel >= count ? count - 1 : g_sel)] : 0);
        return;
    }
    if (item == kItDeathCut || item == kItDeathBlow || item == kItDeathLoot) { DeathCfgToggle(item, dir); return; }
    if (item == kItAutoRetry) { AutoRetryToggle(dir); return; }
    if (item == kItNoFin) { NoFinToggle(dir); return; }
    if (item == kItFinT || item == kItHp0W) { TuneStep(item, dir); return; }
    if (IsMoveItem(item)) { MoveToggle(item, dir); return; }
    if (IsSeqItem(item)) { SeqToggle(item, dir); return; }  // user #14317
    if (item == kItBossStance) { BossStanceSet(dir); return; }  // user #14309: per boss
    if (item == kItBossAggro) { BossAggroSet(dir); return; }
    if (item == kItBossGap) { BossGapStep(dir); return; }
    if (item == kItBossLast) { RepeatLastStep(MoveBoss(), dir); return; }  // human #68578
    if (item == kItRepFlow) { RepFlowToggle(); return; }  // human #122193
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
#if BR_SOULCORE
    if (SctChange(item, dir)) return;  // human #58117
#endif  // BR_SOULCORE
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

// #79226: what each hosted tab (1 = Boss Matches, 2 = Practice, 3 = Player) looked like when it was left, so LB / RB back to it restores the
// page trail, the row, the boss and the list filters. Saved at the top of every UiOpen (the previous session's state is still intact).
struct TabMemo { bool valid = false; menu_nav::Trail<> pages; int sel = 0, uiBoss = -1, bossSlot = -1, listStop = 0, trainStop = 0; };
TabMemo g_tabMemo[4];
int TabOfPages(const menu_nav::Trail<>& t) {
    const int base = t.depth ? t.pages[0] : t.page;
    return base == uRematch ? 1 : base == uPracticeHub ? 2 : base == uPlayer ? 3 : 0;
}
void RememberTab() {
    const int tab = TabOfPages(g_uiPages);
    if (!tab) return;
    TabMemo& m = g_tabMemo[tab];
    m.valid = true; m.pages = g_uiPages; m.sel = g_sel; m.uiBoss = g_uiBoss; m.bossSlot = g_bossSlot;
    m.listStop = g_listStop; m.trainStop = g_trainStop;
}
void UiOpen() {
    RememberTab();
    g_hostMenuReq = false;
    g_uiPages.Reset();
    g_uiBoss = -1;
    g_bossSlot = -1;
    g_sel = 0;  // pc2 #74482: g_listStop stays across a close. It is 0 only when the game starts.
    g_confirm = -9;
    g_msg[0] = 0;
    g_pageGen0 = g_scanGen.load();
    if (g_live.load()) g_wantScan.store(true);
}
bool WavesGateLive() { return WavesGateNow(); }
void UiGatePage() {  // user #13529: the gate page is a base page (no add-on root behind it): Back returns to the host's first page
    UiOpen();
    g_uiPages.page = uGate;
    g_sel = 0;
}
void UiOpenAt(int which, bool resume) {  // user #13223: host first page -> Boss Rematch (1) / Practice (2) / Player (3) as the base page; Back at it closes to the host menu
    const int keep = g_listStop;
    const int page = g_uiPages.page;
    UiOpen();
    if (page != 0) g_listStop = keep;  // leaving and returning to the Boss tab keeps All / Multi / era / Scrolls
    g_uiPages.page = which == 1 ? uRematch : which == 3 ? uPlayer : uPracticeHub;
    if (resume && which >= 1 && which <= 3 && g_tabMemo[which].valid) {
        const TabMemo& m = g_tabMemo[which];
        g_uiPages = m.pages; g_sel = m.sel; g_uiBoss = m.uiBoss; g_bossSlot = m.bossSlot;
        g_listStop = m.listStop; g_trainStop = m.trainStop;
    }
    // human #122092: the menu shortcut during Training opens Practice; with a live Training boss it lands inside Active bosses (Back = the Practice page).
    if (which == 2 && !resume && g_uiPages.page == uPracticeHub && PracticeToolsAvailable() && SpawnedCount() > 0) {
        g_uiPages.Enter(uTools, 0); g_sel = 0;
        Log("Rematch: menu opened on Active bosses (%d boss%s live, #122092)", SpawnedCount(), SpawnedCount() == 1 ? "" : "es");
    }
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
// Native route readiness and protection, not save progress, decide the action.
bool TrainHereOk(int row) { return row >= 0 && (size_t)row < g_rows.size() && g_rows[(size_t)row].missionId && TrainingExport("Nioh3Training_TrainHere"); }
const char* CanMatchArena(int row) {
    if (row < 0 || (size_t)row >= g_rows.size()) return "boss row unavailable";
    if (const char* why = CanArena(row)) return why;
    if (g_status[(size_t)row].load() != 2 && !g_clearGuardOk) return "progress protection unavailable";
    return nullptr;
}
// Training's TrainHere and arena load both need a boss mission id. A Picture Scroll has none
// (every record shares 0xCC96), so a picked Waves row uses the scroll Fight now already on this branch.
bool StartScrollRecord(int row, int item, bool training) {
    if (const char* why = Gates(ReadSnap(), true)) { snprintf(g_msg, sizeof(g_msg), "Not now: %s.", why); return false; }
    if (row < 0 || (size_t)row >= g_rows.size() || g_rows[(size_t)row].route != rcore::kE) return false;
    if (const char* why = CanFight(row)) { snprintf(g_msg, sizeof(g_msg), "%s", why); return false; }
    if (training && !g_waveVetoOk) { snprintf(g_msg, sizeof(g_msg), "Training here needs the wave guard, and it did not install. Fight now still works."); return false; }
    if (g_confirm != item) {
        g_confirm = item;
        snprintf(g_msg, sizeof(g_msg), training ? "Train at %s? No waves, no item used. A confirm / B back." : "Start %s? No item used. A confirm / B back.", g_rows[(size_t)row].name);
        return false;
    }
    g_confirm = -9;
    Post(training ? kReqEmakiTrain : kReqEmaki, row, "the menu");
    ToastF(cWhite, 6000, training ? "Training: %s, no waves..." : "%s...", g_rows[(size_t)row].name);
    return true;
}
bool ActBossRoute(int item) {
    if (const char* why = Gates(ReadSnap(), true)) { snprintf(g_msg, sizeof(g_msg), "Not now: %s.", why); return false; }
    static bool confirmedTrain = false;
    const bool tele = item >= kItTeleport;
    const bool train = item >= kItBossTrainHere && item < kItBossTrainHere + kMaxRows;
    const bool arena = item < kItShrine || tele;
    const int row = item - (tele ? kItTeleport : train ? kItBossTrainHere : arena ? kItFight : kItShrine);
    if (row < 0 || (size_t)row >= g_rows.size()) return false;
    if (g_rows[(size_t)row].route == rcore::kE) {
        if ((tele || train) && rcore::WavesRow(g_rows[(size_t)row])) return StartScrollRecord(row, item, true);
        snprintf(g_msg, sizeof(g_msg), "A Picture Scroll has no boss arena.");
        return false;
    }
    const char* unavailable = train ? nullptr : arena ? CanMatchArena(row) : CanShrine(row);
    const bool trainFallback = arena && unavailable && TrainHereOk(row);
    if (train || trainFallback) {
        if (!TrainHereOk(row)) { snprintf(g_msg, sizeof(g_msg), "Training is unavailable for this boss."); return false; }
        if (g_confirm != item || !confirmedTrain) {
            g_confirm = item; confirmedTrain = true;
            snprintf(g_msg, sizeof(g_msg), "%sTrain against %s here? A confirm / B back.", trainFallback ? "Arena route unavailable. " : "", g_rows[(size_t)row].name);
            return false;
        }
        g_confirm = -9;
        using Fn = int (*)(uint32_t);
        const int rc = ((Fn)TrainingExport("Nioh3Training_TrainHere"))(g_rows[(size_t)row].missionId);
        if (!rc) { snprintf(g_msg, sizeof(g_msg), "Busy (a summon or load is running): try again in a moment."); return false; }
        ToastF(cGreen, 5000, "Training: %s here...", g_rows[(size_t)row].name);
        return true;
    }
    if (!arena && g_status[(size_t)row].load() != 2) {
        snprintf(g_msg, sizeof(g_msg), "Use Fight now for an unbeaten boss; shrine replay requires a cleared mission."); return false;
    }
    if (unavailable) { snprintf(g_msg, sizeof(g_msg), "%s", unavailable); return false; }
    if (g_confirm != item || confirmedTrain) {
        g_confirm = item; confirmedTrain = false;
        snprintf(g_msg, sizeof(g_msg), tele ? "Train in %s's arena? A confirm / B back. Solo only." : arena ? "Fight %s now in its arena? A confirm / B back. Solo only." : "Start at the shrine before %s? A confirm / B back. Solo only.", g_rows[(size_t)row].name);
        if (g_rows[(size_t)row].warning[0]) {  // human #94922: say the known problem before the player confirms
            const size_t used = strlen(g_msg);
            snprintf(g_msg + used, sizeof(g_msg) - used, " WARNING: %s", g_rows[(size_t)row].warning);
        }
        return false;
    }
    g_confirm = -9;
    if (arena && !tele && g_hostedUiAction && !TrainingSwapAvailable()) {
        BeginHostedFight(row, nullptr, g_msg, sizeof(g_msg));
        return false;
    }
    if (arena) SetPracticeNext(tele ? 1 : 0);
    g_arenaOnlyRow.store(-1);
    Post(arena ? kReqFight : kReqShrine, row, "the menu");
    ToastF(cWhite, 6000, arena ? "Boss Matches: into the fight with %s..." : "Boss Matches: to the shrine before %s...", g_rows[(size_t)row].name);
    return true;
}
int ArenaTrainRoute(int row) {
    if (row < 0 || (size_t)row >= g_rows.size()) return rcore::kAtNone;
    const Row& r = g_rows[(size_t)row];
    return rcore::ArenaTrainRouteFor(r, NativeEntryGateFind(r.missionId, r.placement) != 0);
}
const char* ArenaTrainWhy(int row) {  // nullptr = offered now
    const int route = ArenaTrainRoute(row);
    if (route == rcore::kAtNone) {
        if (row < 0 || (size_t)row >= g_rows.size()) return "no arena route for this row";
        const Row& r = g_rows[(size_t)row];
        const int no = rcore::ArenaTrainNoWhy(r, NativeEntryGateFind(r.missionId, r.placement) != 0);
        return no == rcore::kAtNoGate ? "this boss starts with the mission (no gate to hold), so it would spawn"
             : no == rcore::kAtNoBox ? "this boss starts when you step into its arena (a trigger box, no door to hold), so it would spawn"
             : no == rcore::kAtNoArena ? "no known spot inside this arena yet (the trip would stop outside the boss door)"
                                       : "no arena route for this row";
    }
    if (route == rcore::kAtScroll) return g_waveVetoOk ? CanFight(row) : "the wave guard did not install";
    return CanMatchArena(row);
}
// human #107514: the row's arena, no waves, no boss, no Dojo bosses; Training's menu summons as usual
bool ActArenaTrain(int item) {
    const int row = item - kItArenaTrain;
    if (row < 0 || (size_t)row >= g_rows.size()) return false;
    if (ArenaTrainRoute(row) == rcore::kAtScroll) return StartScrollRecord(row, item, true);  // proven no-waves trip (#79507)
    if (const char* why = Gates(ReadSnap(), true)) { snprintf(g_msg, sizeof(g_msg), "Not now: %s.", why); return false; }
    if (const char* why = ArenaTrainWhy(row)) { snprintf(g_msg, sizeof(g_msg), "Train in this arena unavailable: %s.", why); return false; }
    if (g_confirm != item) {
        g_confirm = item;
        snprintf(g_msg, sizeof(g_msg), "Train in %s's arena? No boss, no waves. A confirm / B back. Solo only.", g_rows[(size_t)row].name);
        return false;
    }
    g_confirm = -9;
    SetPracticeNext(0);
    g_arenaOnlyRow.store(row);
    Post(kReqFight, row, "the menu (train in this arena)");
    ToastF(cWhite, 6000, "Training: to %s's arena, no boss...", g_rows[(size_t)row].name);
    return true;
}
bool ScrollRowItem(int item, int& row, int& kind) {
    if (item >= kItFight && item < kItFight + kMaxRows) { row = item - kItFight; kind = 0; return true; }
    if (item >= kItShrine && item < kItShrine + kMaxRows) { row = item - kItShrine; kind = 1; return true; }
    if (item >= kItBossTrainHere && item < kItBossTrainHere + kMaxRows) { row = item - kItBossTrainHere; kind = 2; return true; }
    return false;
}
bool ActScrollFight(int item) {
    if (const char* why = Gates(ReadSnap(), true)) { snprintf(g_msg, sizeof(g_msg), "Not now: %s.", why); return false; }
    int row = -1, kind = 0;
    if (!ScrollRowItem(item, row, kind) || row < 0 || (size_t)row >= g_rows.size()) return false;
    if (kind != 0) {
        if (kind == 2 && rcore::WavesRow(g_rows[(size_t)row])) return StartScrollRecord(row, item, true);
        snprintf(g_msg, sizeof(g_msg), "This Picture Scroll starts with Fight now. It has no shrine and no Train here.");
        return false;
    }
    return StartScrollRecord(row, item, false);
}
bool ActOpenWorld(int row) {
    if (row < 0 || (size_t)row >= g_rows.size() || !g_rows[(size_t)row].OpenWorld()) {
        snprintf(g_msg, sizeof g_msg, "This boss has no field spawn.");
        return false;
    }
    const char* why = Gates(ReadSnap(), true);
    if (why) { snprintf(g_msg, sizeof g_msg, "Not now: %s.", why); return false; }
    if (!TrainingExport("Nioh3Training_FieldArrive")) {
        snprintf(g_msg, sizeof g_msg, "Training field arrive is missing.");
        return false;
    }
    if (g_confirm != row) {
        g_confirm = row;
        snprintf(g_msg, sizeof g_msg, "Teleport to %s and summon it beside you? No save writes. A confirm / B back.", g_rows[(size_t)row].name);
        return false;
    }
    g_confirm = -9;
    Post(kReqField, row, "the menu");
    ToastF(cWhite, 6000, "Open world: %s...", g_rows[(size_t)row].name);
    return true;
}
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
    if (item == kItPageDefaults) { g_confirm = -9; ResetPageDefaults(); return false; }
    if (item == kItStagedStart) { UiChange(0); return false; }
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
    if (item == kItBossLast) { RepeatLastStep(MoveBoss(), 1); return false; }  // human #68578
    if (item == kItRepFlow) { RepFlowToggle(); return false; }  // human #122193
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
#if BR_SOULCORE
    if (SctChange(item, 0)) return false;  // human #58117: opens the page / A = one step up
#endif  // BR_SOULCORE
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
        if (row < 0 || (size_t)row >= g_rows.size()) return false;
        if (rcore::WavesRow(g_rows[(size_t)row])) return StartScrollRecord(row, item, true);
        if (!g_rows[(size_t)row].missionId || g_rows[(size_t)row].route == rcore::kE) {
            snprintf(g_msg, sizeof g_msg, "A Picture Scroll has no Training arena.");
            return false;
        }
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
    if (item == kItReturnHome && (NativeGateMission() || WavesGateNow())) {
        const Snap s = ReadSnap();
        const char* why = !g_exitOk ? g_exitWhy : ExitGate(s);
        if (!why) why = AttemptReturnGate();
        if (why) { snprintf(g_msg, sizeof(g_msg), "Not now: %s.", why); return false; }
        Post(kReqExit, -1, "native Proceed gate");
        ToastF(cWhite, 5000, "Boss Matches: leaving the mission...");
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
    if (item == kItFightAgain && (NativeGateMission() || WavesGateNow())) {
        const int row = NativeGateRow();
        const Snap s = ReadSnap();
        const char* why = row < 0 ? "native boss row is no longer owned" : !g_exitOk ? g_exitWhy : ExitGate(s);
        if (!why) why = AttemptReturnGate();
        if (why) { snprintf(g_msg, sizeof(g_msg), "Not now: %s.", why); return false; }
        Post(kReqNativeAgain, row, "native Proceed gate");
        ToastF(cWhite, 5000, g_cfg.fightAgainDirect && !rcore::WavesRow(g_rows[(size_t)row]) ? "Boss Matches: starting the same fight..." : "Boss Matches: returning, then starting the same fight...");
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
    if (item == kItListFilter) { CycleListFilter(0); return false; }
    if ((g_uiPages.page == uBosses || g_uiPages.page == uRematch) && item >= 0 && item < kItFight &&
        (size_t)item < g_rows.size() && g_rows[(size_t)item].OpenWorld())
        return ActOpenWorld(item);
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
    if (item >= kItArenaTrain && item < kItArenaTrain + kMaxRows) return ActArenaTrain(item);
    {
        int srow = -1, skind = 0;
        if (ScrollRowItem(item, srow, skind) && srow >= 0 && (size_t)srow < g_rows.size() && g_rows[(size_t)srow].route == rcore::kE)
            return ActScrollFight(item);
    }
    if (item >= kItFight) return ActBossRoute(item);
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
         : p == uFightOpts ? "Death and loot - Training / Boss Matches" : p == uPlayer ? "Player settings" : p == uBossVitals ? BvTitle() :
#if BR_SOULCORE
        p == uSoulTune ? "Soul core tuning - LEFT form (live, saved)" :
#endif  // BR_SOULCORE
        p == uMoves || p == uMoveCat ? movesTitle : "Mod menu";
}
bool IsFightRule(int item) { return item == kItDeathCut || item == kItDeathBlow || item == kItDeathLoot || item == kItNoFin || item == kItFinT || item == kItHp0W; }
// section header shown above an item (nullptr = no headers on this page / Back / Close)
const char* ItemGroup(int item) {
    if (item == kItPageDefaults) return "Page defaults";
    const int p = g_uiPages.page;
    if (item == kItBack || item == kItClose) return nullptr;
    if (p == uRematch) return IsFightRule(item) ? "Death and loot (Training / Boss Matches)" : item == kItReturnHome ? "Leave" : item >= 0 && item < kItFight ? "Bosses you can fight again" : "More";
    if (p == uGate) return item == kItFightAgain ? "This fight" : item == kItPlayerOpen ? "Player" : IsFightRule(item) || item == kItFightOpts ? "Boss Match rules" : item == kItReturnHome || item == kItLeaveArena ? "Leave" : "Active bosses";
    if (p == uBossVitals) return IsPstItem(item) ? "Boss settings (see each setting for mode)" : BvGroup(item);  // user #14320 / #22389
    if (p == uPlayer) return IsStyItem(item) ? "Style meter" : IsPstItem(item) ? "Player stats (Training / spawned Boss Matches)" : item == kItCantDie ? "Health" :
#if BR_SOULCORE
        item == kItScForms || IsScCoreItem(item) ? "Soul cores" :
#endif  // BR_SOULCORE
        nullptr;  // user #22389, human #61869
    if (p == uTools) return IsBossOpenItem(item) ? "Active bosses (Training / Boss Matches)" : item == kItSumAnother ? "Add a boss" : item == kItStanceTimer || item == kItDespawnAll ? "All bosses" : item == kItReturnHome ? "Leave" : nullptr;
    if (p == uBoss) return item == kItBossStance || item == kItBossAggro || item == kItBossLast || item == kItRepFlow || item == kItBossGap || item == kItBossReset ? "This boss" : IsBvOpenItem(item) ? "Health and Ki"
                         : IsMoveCatItem(item) ? "Moves by type and hit count" : item == kItBossRemove ? "Remove" : nullptr;
    if (p == uSummon) return item >= kItSummonA && item < kItSummonRosterEnd ? "Add a boss (Training / Boss Matches)" : nullptr;
    if (p == uMoves) return IsMoveCatItem(item) ? "Pick a move group (A: open)" : nullptr;  // user #14291
    if (p == uActions && item == kItStagedStart) return "This boss";
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
                                                                                  : st == kEmaki ? (g_scrollNoWaves.load() ? "active: training (no waves)" : "active: waves") : gate ? gate : "ready");
    static int it[kMaxRows + 4];
    const int n = Items(it, kMaxRows + 4);
    if (g_sel >= n) g_sel = n ? n - 1 : 0;
    if (g_sel < -2) g_sel = -2;
    // #38077: every page starts with 'Mod menu' (jump to the top-level page) and 'Close' (close the menu)
    add(g_sel == -2 ? cWhite : cGray, g_sel == -2, "%s Mod menu   A: open", g_sel == -2 ? ">" : " ");
    add(g_sel == -1 ? cWhite : cGray, g_sel == -1, "%s Close   A: close the mod menu", g_sel == -1 ? ">" : " ");
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
        if (it[i] == kItPageDefaults) { add(sel ? cWhite : cGray, sel, "%s Reset to defaults   A: reset this page", mk); continue; }
        if (it[i] == kItStagedStart) {
            uint16_t thou = 0;
            RowStaged(g_uiBoss, nullptr, &thou, nullptr);
            char hp[40] = "";
            if (g_stagedFinal && thou) {
                if (thou % 10 == 0) snprintf(hp, sizeof hp, " (%u%% health)", thou / 10u);
                else snprintf(hp, sizeof hp, " (%u.%u%% health)", thou / 10u, thou % 10u);
            }
            add(sel ? cWhite : cGray, sel, "%s Staged start: %s%s   A / Left / Right", mk, g_stagedFinal ? "Final wave" : "Full climb", hp);
            continue;
        }
        char bvText[sizeof rows[0].text]; uint32_t bvColor = cGray;  // user #14320
        if (BvRowText(it[i], sel, mk, bvText, sizeof bvText, &bvColor) || PstRowText(it[i], sel, mk, bvText, sizeof bvText, &bvColor)
#if BR_SOULCORE
            || SctRowText(it[i], sel, mk, bvText, sizeof bvText, &bvColor)
#endif  // BR_SOULCORE
            ) add(bvColor, sel, "%s", bvText);  // + #22389, human #58117
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
        } else if (it[i] == kItBossLast) {  // human #68578: 'Repeat last move: Last move (0x4E21 Overhead slash)   A / Left / Right'
            const uint32_t lchr = MoveBoss(); uint32_t ids[3] = {0};
            const int have = LastMovesGet(lchr, ids, 3), v = RepeatLastGet(lchr);
            static const char* kLm[] = {"Off", "Last move", "Last 2 moves", "Last 3 moves"};
            char lcap[96];
            if (!have) snprintf(lcap, sizeof lcap, "(no attack seen yet)");
            else if (v >= 2) snprintf(lcap, sizeof lcap, "(%s ... %s)", LastMoveText(lchr, ids[v - 1]), LastMoveText(lchr, ids[0]));
            else snprintf(lcap, sizeof lcap, "(last: %s)", LastMoveText(lchr, ids[0]));
            add(sel ? cWhite : v ? cYellow : have ? cGray : cDim, sel, "%s Repeat last move: %-12s %.60s   A / Left / Right", mk, kLm[v < 0 ? 0 : v > 3 ? 3 : v], lcap);
        } else if (it[i] == kItRepFlow) {  // human #122193
            const int v = RepFlowGet();
            add(sel ? cWhite : v < 0 ? cDim : cGray, sel, "%s Between repeated moves: %s   A / Left / Right", mk, v < 0 ? "?" : v ? "Flow (chain like the boss)" : "Pause (back to idle)");
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
        else if (it[i] == kItReturnHome && WavesGateNow()) {
            add(sel ? cWhite : cGreen, sel, "%s Leave Picture Scroll", mk);
        }
        else if (it[i] == kItReturnHome && NativeGateMission()) {
            add(sel ? cWhite : cGreen, sel, "%s Leave Boss Match", mk);
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
        else if (it[i] == kItFightAgain) add(sel ? cWhite : cGreen, sel, "%s %s", mk,
            (NativeGateMission() || WavesGateNow()) ? "Fight again (reloads the mission, same fight)" :
            GateMode() == 2 ? "Fight again / Reset round (same boss)" : "Fight again (same boss, this arena)");
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
            add(sel ? cWhite : g_rows[(size_t)row].warning[0] ? cOrange : (g_status[(size_t)row].load() == 2 || TrainHereOk(row) || rcore::WavesRow(g_rows[(size_t)row])) ? cGreen : cGray, sel, "%s %s%s%s", mk, FavMark(row), g_rows[(size_t)row].name, g_rows[(size_t)row].warning[0] ? "  (known bug)" : "");  // human #94915: fightable = lit, not save progress
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
        else if (it[i] == kItListFilter) add(sel ? cWhite : cGray, sel, "%s List: %s   A / Left / Right", mk, ListFilterLabel());
        else if (it[i] == kItBrowse) add(sel ? cWhite : cGray, sel, "%s Boss Matches: browse bosses   A / Enter opens", mk);
        else if (it[i] == kItPractice) add(sel ? cWhite : cGray, sel, "%s Yamagata mission replay   A: open", mk);
        else if (it[i] == kItPracticeStart) add(sel ? cWhite : cGray, sel, "%s Replay from the boss shrine", mk);
        else if (it[i] == kItPracticeReturn) add(sel ? cWhite : g_practiceOwned.load() ? cGray : cDim, sel, "%s End mission replay", mk);
        else if (it[i] == kItBack) add(sel ? cWhite : cGray, sel, "%s Back   B / Esc / Backspace", mk);
        else if (it[i] == kItClose) add(sel ? cWhite : cGray, sel, "%s Close menu", mk);
        else if (it[i] == kItOpen) add(sel ? cWhite : cGray, sel, "%s Open Battle Scroll replays", mk);
        else if (it[i] == kItScroll) add(sel ? cWhite : cGray, sel, "%s Play first owned Picture Scroll", mk);
        else if (it[i] == kItAbort) add(cYellow, sel, "%s Cancel pending replay", mk);
        else if (it[i] >= kItArenaTrain && it[i] < kItArenaTrain + kMaxRows) {
            const char* why = ArenaTrainWhy(it[i] - kItArenaTrain);
            add(sel ? cWhite : why ? cDim : cGreen, sel, "%s    Train in this arena (no boss, no waves)", mk);
        } else if (it[i] >= kItTeleport) {
            const int row = it[i] - kItTeleport;
            if ((size_t)row < g_rows.size() && rcore::WavesRow(g_rows[(size_t)row])) {
                add(sel ? cWhite : cGreen, sel, "%s %s%s", mk, FavMark(row), g_rows[(size_t)row].name);
            } else {
            const char* why = CanMatchArena(row);
            if (why && TrainHereOk(row)) add(sel ? cWhite : cGreen, sel, "%s %sTrain here (arena unavailable)", mk, FavMark(row));
            else if (why) add(sel ? cWhite : cDim, sel, "%s %sArena unavailable: %s", mk, FavMark(row), why);
            else add(sel ? cWhite : cGreen, sel, "%s %sTrain in arena: %s", mk, FavMark(row), g_rows[(size_t)row].name);
            }
        } else if (it[i] >= kItBossTrainHere && it[i] < kItBossTrainHere + kMaxRows) {
            const int row = it[i] - kItBossTrainHere;
            if ((size_t)row < g_rows.size() && rcore::WavesRow(g_rows[(size_t)row]))
                add(sel ? cWhite : cGreen, sel, "%s    Train here (no waves)", mk);
            else if ((size_t)row < g_rows.size() && g_rows[(size_t)row].route == rcore::kE)
                add(sel ? cWhite : cDim, sel, "%s    Train here does not pick this scroll", mk);
            else add(sel ? cWhite : TrainHereOk(row) ? cGreen : cDim, sel, "%s    Train here", mk);
        } else if (it[i] >= kItShrine) {
            const int srow = it[i] - kItShrine;
            const Row& r = g_rows[(size_t)srow];
            if (r.route == rcore::kE) {
                const char* why = CanShrine(srow);
                add(sel ? cWhite : cDim, sel, "%s    Shrine before the boss   - %s", mk, why ? why : "a Picture Scroll has no shrine");
            } else {
            const uint8_t stt = g_status[(size_t)srow].load();
            const char* why = CanShrine(srow);
            const uint32_t c = stt == 2 && !why ? (!strcmp(r.fightConf, "high") ? cGreen : !strcmp(r.fightConf, "medium") ? cYellow : cOrange) : cDim;
            if (stt != 2) add(sel ? cWhite : cDim, sel, "%s    Shrine before the boss   - not beaten on this journey", mk);
            else if (why) add(sel ? cWhite : c, sel, "%s    Shrine before the boss   - %s", mk, why);
            else add(sel ? cWhite : c, sel, "%s    Shrine before the boss   %s, %d m   [%s]", mk, r.fightKind, r.fightDist, r.fightConf);
            }
        } else if (it[i] >= kItFight) {
            const int row = it[i] - kItFight;
            if ((size_t)row < g_rows.size() && g_rows[(size_t)row].route == rcore::kE) {
                const char* why = CanFight(row);
                if (why) add(sel ? cWhite : cDim, sel, "%s    Fight now - %s", mk, why);
                else add(sel ? cWhite : cGreen, sel, "%s    Fight now (this scroll's waves)", mk);
            } else {
            const char* why = CanMatchArena(row);
            if (why && TrainHereOk(row)) add(sel ? cWhite : cGreen, sel, "%s    Train here (arena unavailable)", mk);
            else if (why) add(sel ? cWhite : cDim, sel, "%s    Fight now - %s", mk, why);
            else add(sel ? cWhite : cGreen, sel, "%s    %s", mk,
                     (size_t)row < g_rows.size() && g_rows[(size_t)row].lesson ? "Fight now (lesson)" : "Fight now (boss arena)");
            }
        } else {
            const Row& r = g_rows[(size_t)it[i]];
            if (r.OpenWorld()) {
                add(sel ? cWhite : cGreen, sel, "%s %s%-40.40s %-10.10s %-11.11s %-14.14s", mk, FavMark(it[i]), r.name, r.region, "Summon", "Open world");
                continue;
            }
            const char* routeWhy = CanMatchArena(it[i]);
            const char* status = r.warning[0] ? "Known bug" : r.lesson ? "Lesson" : r.route == rcore::kE ? "" : !routeWhy ? "Fight now" : TrainHereOk(it[i]) ? "Train here" : "Unavailable";
            const uint32_t c = r.warning[0] ? cOrange : !routeWhy || TrainHereOk(it[i]) ? cGreen : cDim;
            // human #78402: a scroll row is its own full label; the boss columns cut the enemy name and add 'Waves unverified'.
            if (g_uiPages.page != uActions && rcore::WavesRow(r)) { add(sel ? cWhite : cGreen, sel, "%s %s%s", mk, FavMark(it[i]), r.name); continue; }
            if (g_uiPages.page == uActions) add(sel ? cWhite : c, sel, "%s Replay mission: %s   %s %s", mk, r.name, rcore::RouteName(r.route), r.Verified() ? "" : "unverified");
            else
                add(sel ? cWhite : c, sel, "%s %s%-40.40s %-10.10s %-11.11s %-14.14s %s", mk, FavMark(it[i]), r.name, r.region, status, rcore::RouteName(r.route),
                    r.route == rcore::kX ? "" : r.Verified() ? "" : "unverified");
        }
    }
    if (pinBack && k < cap) add(g_sel == n - 1 ? cWhite : cGray, g_sel == n - 1, "%s Back   B / Esc / Backspace", g_sel == n - 1 ? ">" : " ");
    if (footer && nf) {
        footer[0] = 0;
        const int item = g_sel < 0 ? kItClose : n ? it[g_sel] : kItOpen;
        if (item == kItPageDefaults) snprintf(footer, nf, "%s", g_msg[0] ? g_msg : "Restore this page to shipped defaults. Other pages stay unchanged; startup settings apply after restart.");
        else if (item == kItStagedStart) snprintf(footer, nf, "%s", g_msg[0] ? g_msg : "Full climb plays every staged wave in order. Final wave requests only the last one and sets that boss's health to the script's pre-final fraction for this session. Nothing is written to the save.");
        else if (const char* bvf = BvFooter(item)) snprintf(footer, nf, "%s", g_msg[0] ? g_msg : bvf);  // user #14320
        else if (const char* psf = PstFooter(item)) snprintf(footer, nf, "%s", g_msg[0] ? g_msg : psf);  // user #22389
#if BR_SOULCORE
        else if (const char* scf = SctFooter(item)) snprintf(footer, nf, "%s", g_msg[0] ? g_msg : scf);  // human #58117
#endif  // BR_SOULCORE
        else if (IsBossOpenItem(item)) snprintf(footer, nf, "%s", g_msg[0] ? g_msg : "Training and spawned Boss Matches: behavior, repeat delay, moves, and health for this boss. Shared stat controls are labeled separately.");
        else if (item == kItSumAnother) snprintf(footer, nf, "%s", g_msg[0] ? g_msg : "Add a boss beside you. During Training it waits; otherwise it fights normally. Change Behavior under Active bosses.");
        else if (item == kItStanceTimer) snprintf(footer, nf, "%s", g_msg[0] ? g_msg : "Training and spawned Boss Matches: switch all active bosses after 1-10 seconds. If any boss is waiting, all will fight; otherwise all will wait. A starts or cancels.");
        else if (item == kItBossStance) snprintf(footer, nf, "%s", g_msg[0] ? g_msg : "This boss only: Stand still waits between repeats. Fight normally uses its own attacks. A toggles; Left fights; Right waits.");
        else if (item == kItBossAggro) snprintf(footer, nf, "%s", g_msg[0] ? g_msg : "ON: this boss targets you and approaches within 5 m before repeating a move. Also saves the default for newly added bosses.");
        else if (item == kItBossLast) snprintf(footer, nf, "%s", g_msg[0] ? g_msg : "Let the boss do the attack you want, pause, then pick Last move (or Last 2 / Last 3): the boss keeps repeating it, with Repeat delay after each round.");
        else if (item == kItRepFlow) snprintf(footer, nf, "%s", g_msg[0] ? g_msg : "Last 2 / 3 moves: Flow chains them like the boss's own combos (Stand still too, once the boss has chained that move itself); Pause plays each one out to idle.");
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
            const int row = item - kItTrainHere;
            if ((size_t)row < g_rows.size() && rcore::WavesRow(g_rows[(size_t)row]))
                snprintf(footer, nf, "%s", g_msg[0] ? g_msg : "Picture Scroll arena, no waves: summon bosses or practice here. A: start / Right: favorite / Left: unfavorite / B: back.");
            else snprintf(footer, nf, "%s", g_msg[0] ? g_msg : "Training only: start with this boss here. Active bosses controls behavior and repeats. Right: favorite / Left: unfavorite.");
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
            snprintf(footer, nf, "0x%04X  %.60s%s%s.%s A: repeat | Left: block | Right: allow.", m.id, MoveName(m), tag[0] ? ", " : "", tag,
                     N3MoveProj(&m) ? " Projectile. " N3_PROJ_HELP : "");  // human #126342
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
        else if (item == kItListFilter && g_listCycle.Kind(ListStopRef()) == 6) snprintf(footer, nf, "Not recently confirmed: no recorded working live entry for this boss on its current path. Not your beaten / unbeaten progress.");
        else if (item == kItListFilter) snprintf(footer, nf, "List: %s. A / Left / Right cycles All, Multi, each era, Dojos,%s Scrolls, and Favorites. Right on a row marks a favorite, Left unmarks it.", ListFilterLabel(), IsTrainListPage() ? "" : " Not recently confirmed,");
        else if (item == kItBrowse) snprintf(footer, nf, "Choose a boss: Fight now in its arena, or Train here. Beaten and unbeaten bosses use the same guarded route.");
        else if (item == kItBack) snprintf(footer, nf, "Return one level. B cancels a confirmation first; B at the root closes.");
        else if (item == kItDeathCut) snprintf(footer, nf, "Training / spawned Boss Matches: play the boss death animation after one final blow. No story cutscene. Needs Require final blow ON and No grapple OFF. Saved.");
        else if (item == kItDeathBlow) snprintf(footer, nf, "Training / spawned Boss Matches: ON requires a final blow at zero HP; OFF kills at zero HP. No grapple overrides this rule. Saved.");
        else if (item == kItDeathLoot) snprintf(footer, nf, "Training / spawned Boss Matches: drop boss loot and Amrita, with one final blow. No first-clear rewards. Boss Matches start this OFF; you can enable it during the match.");
        else if (item == kItReturnHome && WavesGateNow()) snprintf(footer, nf, "Leaves the Picture Scroll for the shrine.");
        else if (item == kItReturnHome && NativeGateMission()) snprintf(footer, nf, "Return to where you entered this Boss Match.");
        else if (item == kItReturnHome) snprintf(footer, nf, "Ends the arena session (boss removed) and puts you back where you stood before it started, in that world. Works after a kill, with no boss, or mid-switch.");
        else if (item == kItAutoRetry) snprintf(footer, nf, "Training and arena Boss Matches retry in place after defeat. Auto retry is always enabled.");
        else if (item == kItNoFin) snprintf(footer, nf, "Training / spawned Boss Matches: block grapples during Ki break. At zero HP, keep attacking until the combo timeout expires. Boss Matches start this OFF.");
        else if (item == kItFinT) snprintf(footer, nf, "Death animation / Boss loot: kill when the first final-blow animation ends, or after this timeout. Left / Right: 0.5 s. Saved.");
        else if (item == kItHp0W) snprintf(footer, nf, "With No grapple ON, a zero-HP boss dies after this long without a hit. Left / Right: 0.1 s. Shared by Training and spawned Boss Matches. Saved.");
        else if (item == kItFightAgain && (NativeGateMission() || WavesGateNow())) snprintf(footer, nf, "Leaves and reloads the mission, then starts the same fight. B / Close keeps you at the gate.");
        else if (item == kItFightAgain) snprintf(footer, nf, "Respawns the same boss here and resets the round (no load). B / Close keeps you at the gate.");
        else if (item == kItClose) snprintf(footer, nf, "Close the mod menu and return to the game.");
        else if (item >= kItArenaTrain && item < kItArenaTrain + kMaxRows) {
            const int row = item - kItArenaTrain;
            const char* why = ArenaTrainWhy(row);
            if (g_msg[0]) snprintf(footer, nf, "%s", g_msg);
            else if (why) snprintf(footer, nf, "Train in this arena unavailable: %s. B: back.", why);
            else snprintf(footer, nf, "Loads this row's arena with no waves and no boss; summon from the Training menu. A: start / B: back.");
            if (!g_msg[0]) WarnFooter(footer, nf, row);
        } else if (item >= kItTeleport) {
            const int row = item - kItTeleport;
            if ((size_t)row < g_rows.size() && rcore::WavesRow(g_rows[(size_t)row])) {
                snprintf(footer, nf, "%s", g_msg[0] ? g_msg : "Picture Scroll arena, no waves: summon bosses or practice here. A: start / Right: favorite / Left: unfavorite / B: back.");
            } else {
            const char* why = CanMatchArena(row);
            if (why) snprintf(footer, nf, TrainHereOk(row) ? "Arena route unavailable (%s). A: train here / B: back." : "%s. B: back.", why);
            else snprintf(footer, nf, "Train in this boss arena, including unbeaten bosses. Solo and progress guards apply. A: start / Right: favorite / Left: unfavorite / B: back.");
            }
        } else if (item >= kItBossTrainHere && item < kItBossTrainHere + kMaxRows) {
            const int row = item - kItBossTrainHere;
            if ((size_t)row < g_rows.size() && rcore::WavesRow(g_rows[(size_t)row]))
                snprintf(footer, nf, "%s", g_msg[0] ? g_msg : "Picture Scroll arena, no waves: summon bosses or practice here. A: start / B: back.");
            else if ((size_t)row < g_rows.size() && g_rows[(size_t)row].route == rcore::kE)
                snprintf(footer, nf, "This Picture Scroll starts with Fight now. It has no shrine and no Train here.");
            else snprintf(footer, nf, TrainHereOk(row) ? "Start Practice against this boss where you are, without loading its mission. A: start / B: back." : "Training is unavailable for this boss. B: back.");
            if (!g_msg[0]) WarnFooter(footer, nf, row);  // regression #94992: Train here shows the row warning too
        } else if (item >= kItShrine) {
            const Row& r = g_rows[(size_t)(item - kItShrine)];
            const char* why = CanShrine(item - kItShrine);
            if (why) snprintf(footer, nf, "%s. B back.", why);
            else snprintf(footer, nf, "%s: starts the mission at start point %u (%s, %d m from the boss, %s confidence). A start, B back.",
                     r.mission[0] ? r.mission : r.name, r.fightStart, r.fightKind, r.fightDist, r.fightConf);
        } else if (item >= kItFight) {
            const int row = item - kItFight;
            if ((size_t)row < g_rows.size() && g_rows[(size_t)row].route == rcore::kE) {
                const char* why = CanFight(row);
                if (why) snprintf(footer, nf, "%s. B: back.", why);
                else snprintf(footer, nf, "This scroll's own waves. No Picture Scroll item is used. A: start / B: back.");
            } else {
            const char* why = CanMatchArena(row);
            if ((size_t)row < g_rows.size() && g_rows[(size_t)row].lesson)
                snprintf(footer, nf, "%s. A: start / B: back.", kLessonLine);
            else if (why) snprintf(footer, nf, TrainHereOk(row) ? "Arena route unavailable (%s). A: train here, or choose Train here below. B: back." : "%s. B: back.", why);
            else snprintf(footer, nf, "Fight this boss in its arena, beaten or unbeaten. Solo and progress guards apply. Train here is a separate Practice option. A: start / B: back.");
            if (!g_msg[0]) WarnFooter(footer, nf, row);
            }
        } else if (item >= 0) {
            const Row& r = g_rows[(size_t)item];
            if (r.OpenWorld()) snprintf(footer, nf, "Teleport to this boss's field spawn and summon it beside you. No save writes.");
            else if ((g_uiPages.page == uBosses || g_uiPages.page == uRematch) && r.lesson) snprintf(footer, nf, "%s", kLessonLine);
            else if ((g_uiPages.page == uBosses || g_uiPages.page == uRematch) && r.route == rcore::kE)
                snprintf(footer, nf, "%s: A opens the wave fight. Not a boss. PgUp/PgDn skips ten rows. LB/RB switches tab. Right: favorite / Left: unfavorite.", r.name);
            else if (g_uiPages.page == uBosses || g_uiPages.page == uRematch) snprintf(footer, nf, "%s: A opens boss actions. Boss %d of %d. PgUp/PgDn skips ten rows. LB/RB switches tab. Right: favorite / Left: unfavorite.", r.name, g_sel + 1, n - 1);
            if (g_uiPages.page == uBosses || g_uiPages.page == uRematch) WarnFooter(footer, nf, item);
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

#if BR_SOULCORE
// ---- Tenshin probe calls (SEH needs a function without C++ objects) ----
int TCall(int which, uint64_t base, uint64_t a, uint64_t b) {
    __try {
        switch (which) {
            case 1: ((void (*)(uint64_t))(base + Grva(0x843010)))(a); break;
            case 2: ((void (*)(uint64_t))(base + Grva(0x842A64)))(a); break;
            case 3: ((void (*)(uint64_t, int))(base + Grva(0xA550E8)))(a, (int)b); break;
            case 4: ((void (*)(uint64_t, int, int))(base + Grva(0x1AB4C5C)))(a, 1, -1); break;
        }
        return 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 1;
    }
}

// ---- spirit probe: spawn chain 2916C5C(mgr,pos*,yaw,a4,a5); chr = *(*(mgr+0x128)+0x30), [inst]==2 when loaded (ally #46711) ----
std::atomic<uint64_t> g_spMgr{0};
std::atomic<int> g_spPin{0};
std::atomic<int> g_spBody{0};          // ally #53727: 1 = put the summoned yokai's model on the player
std::atomic<uint64_t> g_spBodyChr{0};    // spirit chr already copied (once per spawn)
std::atomic<uint64_t> g_spBodyOld{0};    // player's model ptr before the copy (for undo)
std::atomic<uint64_t> g_spBodySlot{0};   // address written: [chr+0x50]+8+idx*8, idx=[[chr+0x50]+0xA0]
// model container M=[chr+0x50]; active index [M+0xA0] (0x4F3B20); slot ptr = M+8+idx*8 (SetForm writes idx 1 = M+0x10)
uint64_t ModelSlot(uint64_t chr, uint64_t& M, uint32_t& idx) {
    M = 0; idx = 0;
    if (!chr || !Rd(chr + 0x50, M) || !M) return 0;
    Rd(M + 0xA0, idx);
    if (idx > 7) idx = 0;
    return M + 8 + (uint64_t)idx * 8;
}
// copy the spirit's active model onto the player's active slot, once per spawned spirit chr; logs both pointers
void SpiritBodyApply(uint64_t schr) {
    if (g_spBodyChr.load() == schr) return;  // already copied for this spawn (no per-tick log spam)
    const uint64_t pch = PlayerChar();
    if (schr == pch) return;
    static uint64_t lastLogged = 0;  // log a chr whose model is not loaded yet once, then retry silently each tick
    uint64_t pM, sM, pm = 0, sm = 0;
    uint32_t pi, si;
    const uint64_t ps = ModelSlot(pch, pM, pi), ss = ModelSlot(schr, sM, si);
    if (ps) Rd(ps, pm);
    if (ss) Rd(ss, sm);
    if (lastLogged != schr || sm)
        Log("SpiritBody player chr=%llX M=%llX idx=%u model=%llX | spirit chr=%llX M=%llX idx=%u model=%llX", (unsigned long long)pch,
            (unsigned long long)pM, pi, (unsigned long long)pm, (unsigned long long)schr, (unsigned long long)sM, si, (unsigned long long)sm);
    lastLogged = schr;
    if (!ps || !sm || sm == pm) return;
    if (!g_spBodyOld.load()) { g_spBodyOld.store(pm); g_spBodySlot.store(ps); }
    const bool ok = WrBytes(ps, &sm, 8);
    g_spBodyChr.store(schr);
    g_spPin.store(1);  // keep the separate spirit on the player (hidden inside the body)
    Log("SpiritBody WRITE [%llX] %llX -> %llX ok=%d", (unsigned long long)ps, (unsigned long long)pm, (unsigned long long)sm, ok);
}
void SpiritBodyUndo() {
    const uint64_t ps = g_spBodySlot.exchange(0), old = g_spBodyOld.exchange(0);
    if (ps && old) WrBytes(ps, &old, 8);
    g_spBodyChr.store(0);
    Log("SpiritBody UNDO [%llX] <- %llX", (unsigned long long)ps, (unsigned long long)old);
}
// #54000: the Summoning Seal's yokai is NOT a 2916C5C guardian spirit (that path is mission-script only and never ran in the seal test, so
// the old tick never had a chr). It is spawned by 65A8A8 like any chr: rbx = new chr, its first motion goes 66C4A8(state=[[chr+0x3B0]+8],
// [desc+0x6C]) with return address 65ADA6. The MotionReq hook calls this for that caller; the nearest such spawn (< 15 m) while armed is the
// target. [Spirit] BodyMotion=<hex> limits it to one first-motion id (0 = any).
std::atomic<uint64_t> g_spSpawnChr{0};
void SpiritSpawnNote(uint64_t state, uint32_t motion) {
    static const int onlyMotion = IniInt(L"Spirit", L"BodyMotion", 0);
    static std::atomic<int> logged{0};
    uint64_t chr = 0, back = 0, s3b0 = 0;
    if (!Rd(state + 0xE0, chr) || !chr || !Rd(chr + 0x3B0, s3b0) || !s3b0 || !Rd(s3b0 + 8, back) || back != state) return;
    const uint64_t pch = PlayerChar();
    if (!pch || chr == pch) return;
    float pp[4] = {}, cp[4] = {};
    const bool ok = Copy(pp, pch + 0x110, 16) && Copy(cp, chr + 0x110, 16);
    const float dx = cp[0] - pp[0], dy = cp[1] - pp[1], dz = cp[2] - pp[2];
    const float d = ok ? sqrtf(dx * dx + dy * dy + dz * dz) / 100.0f : -1.f;  // position is in cm
    const bool take = g_spBody.load() && ok && d < 15.f && (!onlyMotion || (uint32_t)onlyMotion == motion);
    if (take) g_spSpawnChr.store(chr);
    if (ok) bosslog::FormKeySealNote(chr, motion, d);  // #54190: FormKey follows the same seal spawn
    if (take || logged.fetch_add(1) < 40)
        Log("SpiritBody spawn 65A8A8 chr=%llX state=%llX motion=%X dist=%.1f armed=%d take=%d", (unsigned long long)chr, (unsigned long long)state,
            motion, d, g_spBody.load(), take);
}
int SpiritCall(uint64_t base, uint64_t mgr, const float* pos, float yaw) {
    __try {
        ((void (*)(uint64_t, const float*, float, int, int))(base + Grva(0x2916C5C)))(mgr, pos, yaw, 0, 0);
        return 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 1;
    }
}
// ---- soul-core ids (ally #46829): DB=*(ctx+0x3F0); rec=fn 0x3748E0(DB,id), stride 0x100; yokai-summon cores have *(u32*)(rec+0x38)!=0 ----
uint64_t SpiritRec(uint64_t base, uint64_t db, uint32_t id) {
    __try {
        return ((uint64_t(*)(uint64_t, uint32_t))(base + Grva(0x3748E0)))(db, id);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}
int SpiritGaugeFill(uint64_t base, uint64_t G, float mx) {
    __try {
        ((void (*)(uint64_t, float))(base + Grva(0x916FAC)))(G, mx);
        return 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 1;
    }
}
uint64_t SpiritGaugeEntry(uint64_t base, uint64_t G) {
    __try {
        return ((uint64_t(*)(uint64_t))(base + Grva(0x377D30)))(G);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}
int SpiritRawCall(uint64_t fn, uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t& ret) {
    __try {
        ret = ((uint64_t(*)(uint64_t, uint64_t, uint64_t, uint64_t))fn)(a, b, c, d);
        return 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 1;
    }
}
int SpiritAddCall(uint64_t base, uint64_t pd, uint32_t id) {
    __try {
        ((void (*)(uint64_t, uint32_t))(base + Grva(0x2153E7C)))(pd, id);
        return 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 1;
    }
}
int SpiritEquipCall(uint64_t base, uint64_t pd, uint32_t slot, uint32_t id) {
    __try {
        ((void (*)(uint64_t, uint32_t, uint32_t))(base + Grva(0x217D8B4)))(pd, slot, id);
        return 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 1;
    }
}
int SpiritSetPos(uint64_t chr, const float* pos) {
    __try {
        ((void (*)(uint64_t, const float*, float))F_SETPOSYAW)(chr, pos, 0.f);
        return 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 1;
    }
}
// mgr: G=*(base+0x45BA228) (transient script context, stale outside script ticks); holder=*(*(G+0x240)+0x5E0); mgr=*(holder+8) (ally #46783).
// Falls back to the mgr captured by the 2916C5C hook (SpiritNoteMgr) when the chain is null.
uint64_t SpiritMgr(uint64_t base, char* why, size_t n) {
    uint64_t G = 0, a = 0, holder = 0, mgr = 0;
    Rd(base + Grva(0x45BA228), G);
    if (G && Rd(G + 0x240, a) && a && Rd(a + 0x5E0, holder) && holder && Rd(holder + 8, mgr) && mgr) {
        snprintf(why, n, "G=%llX [G+240]=%llX holder=%llX mgr=%llX", (unsigned long long)G, (unsigned long long)a, (unsigned long long)holder, (unsigned long long)mgr);
        return mgr;
    }
    const uint64_t hk = g_spMgr.load();
    snprintf(why, n, "chain null (G=%llX [G+240]=%llX) hookmgr=%llX", (unsigned long long)G, (unsigned long long)a, (unsigned long long)hk);
    return hk;
}
void SpiritChr(uint64_t mgr, uint64_t& inst, uint64_t& chr, uint64_t& state) {
    inst = chr = state = 0;
    if (mgr && Rd(mgr + 0x128, inst) && inst) {
        Rd(inst + 0x30, chr);
        Rd(inst, state);
    }
}
#endif  // BR_SOULCORE

// ---- pipe commands (remote.cpp thread) ----
void Command(const char* line, char* reply, size_t n) {
    char cmd[32] = "", arg[64] = "";
    sscanf_s(line, "%31s %63s", cmd, (unsigned)sizeof(cmd), arg, (unsigned)sizeof(arg));
    if (!strcmp(cmd, "scrollctl")) {  // #75963 DEV: scrollctl give <hexid> <seed> <level> <b0f> | item | level <n> | b0f <n> | redirect <0|1> | state
        char a[32] = {};
        unsigned v1 = 0, v2 = 0, v3 = 0, v4 = 0;
        const char* rest = strstr(line, "scrollctl") + 9;  // the whole line: arg holds only the first word
        const int na = sscanf(rest, "%31s %x %u %u %u", a, &v1, &v2, &v3, &v4);
        if (na >= 1 && !strcmp(a, "give")) {
            if (na >= 2) g_scGiveId.store(v1);
            g_scGiveSeed.store(na >= 3 ? v2 : 1);
            g_scGiveLevel.store(na >= 4 ? v3 : 0);
            g_scGiveB0f.store(na >= 5 ? v4 : 0);
            g_scGive.store(true);
        } else if (na >= 1 && !strcmp(a, "item")) Post(kReqScroll, -1, "scrollctl");
        else if (na >= 1 && !strcmp(a, "useitem")) g_scUseItem.store(1);
        else if (na >= 1 && !strcmp(a, "noitem")) g_scUseItem.store(0);
        else if (na >= 1 && !strcmp(a, "start0")) g_scStart0.store(1);
        else if (na >= 1 && !strcmp(a, "startrow")) g_scStart0.store(0);
        else if (na >= 1 && !strcmp(a, "seedpick")) g_scSeedPick.store(1);
        else if (na >= 1 && !strcmp(a, "noseedpick")) g_scSeedPick.store(0);
        else if (na >= 2 && !strcmp(a, "keybyte")) {  // #77334 DEV: write the record's real key to charMgr +0x2BEF, redirect stays on
            uint64_t kbA = 0, kbC = 0;
            uint8_t kbOld = 0, kbNew = (uint8_t)v1;
            const bool kbOk = Rd(G_CH, kbA) && kbA && Rd(kbA, kbC) && kbC && Rd(kbC + 0x2BEF, kbOld) && WrBytes(kbC + 0x2BEF, &kbNew, 1);
            Log("Rematch: scrollctl keybyte %02X -> %02X: %s", kbOld, kbNew, kbOk ? "written" : "FAILED");
        }
        else if (na >= 1 && (!strcmp(a, "level") || !strcmp(a, "b0f") || !strcmp(a, "redirect"))) {
            unsigned d = 0;
            sscanf(rest, "%*s %u", &d);
            if (!strcmp(a, "level")) g_scLevel.store((int)d);
            else if (!strcmp(a, "b0f")) g_scB0f.store((int)d);
            else g_scNoRedirect.store(d ? 0 : 1);
        }
        snprintf(reply, n, "scrollctl level %d b0f %d redirect %d items %d", g_scLevel.load(), g_scB0f.load(), g_scNoRedirect.load() ? 0 : 1, CountScrollItems());
        return;
    }
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
#if BR_SOULCORE
    if (!strcmp(cmd, "tenshin") && !strcmp(arg, "equip")) {  // tenshin equip <id> [slot] : write the equipped spirit id dword [[pd]+0x100+slot*4] (Equip 217D8B4 does the same store)
        unsigned id = 0, slotArg = 9;
        const char* p = strstr(line, "equip");
        if (!p || sscanf_s(p + 5, "%i %u", &id, &slotArg) < 1) {
            snprintf(reply, n, "error tenshin equip <id> [slot]");
            return;
        }
        const uint64_t base = (uint64_t)GetModuleHandleW(nullptr);
        uint64_t pd = 0, in = 0, ctx = 0;
        uint32_t sel = 0;
        if (!Rd(base + Grva(0x4751860), pd) || !pd || !Rd(pd, in) || !in || !Rd(base + Grva(0x4751530), ctx) || !ctx) {
            snprintf(reply, n, "error player data not ready");
            return;
        }
        Rd(ctx + 0x23B62C, sel);
        const unsigned slot = slotArg < 2 ? slotArg : (sel ? 1u : 0u);
        uint32_t old = 0, now = 0;
        Rd(in + 0x100 + slot * 4, old);
        WrU32(in + 0x100 + slot * 4, id);
        Rd(in + 0x100 + slot * 4, now);
        Log("Tenshin equip: pd=%llX inner=%llX slot=%u (sel=%u) id %u(0x%X)->%u(0x%X)", (unsigned long long)pd, (unsigned long long)in, slot, sel, old, old, now, now);
        snprintf(reply, n, "ok equip slot=%u id %u->%u (sel=%u)", slot, old, now, sel);
        return;
    }
    if (!strcmp(cmd, "peek")) {  // peek <hex> [qwords<=16] : read memory (an address >= 0x10000000 is absolute, a smaller value is base-relative)
        unsigned long long addr = 0;
        unsigned cnt = 4;
        if (sscanf_s(line + 4, "%llx %u", &addr, &cnt) < 1) {
            snprintf(reply, n, "error peek <hex addr|rva> [qwords]");
            return;
        }
        if (cnt > 16) cnt = 16;
        if (addr < 0x10000000ull) addr += (uint64_t)GetModuleHandleW(nullptr);
        size_t k = (size_t)snprintf(reply, n, "ok peek %llX:", addr);
        for (unsigned i = 0; i < cnt && k + 24 < n; ++i) {
            uint64_t v = 0;
            if (!Rd(addr + i * 8ull, v)) { k += (size_t)snprintf(reply + k, n - k, " ??"); break; }
            k += (size_t)snprintf(reply + k, n - k, " %llX", (unsigned long long)v);
        }
        return;
    }
#endif  // BR_SOULCORE
    if (!BR_SOULCORE && !strcmp(cmd, "spirit")) {
        snprintf(reply, n, "error soul-core commands are not in this build");
        return;
    }
#if BR_SOULCORE
    if (!strcmp(cmd, "spirit")) {  // spirit status|spawn|pin|unpin|tick : the guardian-spirit chr (spawn chain 2916C5C), pinned to the player each input tick
        const uint64_t base = (uint64_t)GetModuleHandleW(nullptr);
        char why[160];
        if (!strcmp(arg, "slots") || !strcmp(arg, "call")) {  // spirit slots : dwords [P]+0xF0..0x130 ; spirit call <rva> [a1..a4] (P=playerdata E=ext C=char, else hex): blind SEH call
            uint64_t pd = 0, in = 0;
            if (!Rd(base + Grva(0x4751860), pd) || !pd || !Rd(pd, in) || !in) {
                snprintf(reply, n, "error player data not ready");
                return;
            }
            if (!strcmp(arg, "slots")) {
                size_t k = (size_t)snprintf(reply, n, "ok slots P=%llX in=%llX @F0:", (unsigned long long)pd, (unsigned long long)in);
                for (unsigned o = 0xF0; o < 0x130 && k + 12 < n; o += 4) {
                    uint32_t v = 0;
                    Rd(in + o, v);
                    k += (size_t)snprintf(reply + k, n - k, " %X", v);
                }
                return;
            }
            const uint64_t ch = PlayerChar();
            uint64_t ext = 0;
            if (ch) Rd(ch + 0x3A0, ext);
            unsigned long long rva = 0;
            char t[4][24] = {};
            const char* p = strstr(line, "call");
            const int got = p ? sscanf_s(p + 4, "%llx %23s %23s %23s %23s", &rva, t[0], 24u, t[1], 24u, t[2], 24u, t[3], 24u) : 0;
            if (got < 1 || !rva) {
                snprintf(reply, n, "error spirit call <rva> [a1..a4]");
                return;
            }
            uint64_t a[4] = {};
            for (int i = 0; i < 4; ++i) {
                if (t[i][0] == 'P') a[i] = pd;
                else if (t[i][0] == 'E') a[i] = ext;
                else if (t[i][0] == 'C') a[i] = ch;
                else if (t[i][0]) a[i] = strtoull(t[i], nullptr, 16);
            }
            uint64_t ret = 0;
            const int rc = SpiritRawCall(base + rva, a[0], a[1], a[2], a[3], ret);
            Log("Spirit call +%llX(%llX,%llX,%llX,%llX) rc=%d ret=%llX", rva, (unsigned long long)a[0], (unsigned long long)a[1], (unsigned long long)a[2], (unsigned long long)a[3], rc, (unsigned long long)ret);
            snprintf(reply, n, "ok call rc=%d ret=%llX", rc, (unsigned long long)ret);
            return;
        }
        if (!strcmp(arg, "gauge")) {  // spirit gauge [max] : ext=*(player+0x3A0), G=ext+0x1D8, max=*(float*)(ext+0x3AC); fill = 0x916FAC(G,max) (ally #46889)
            const uint64_t ch = PlayerChar();
            uint64_t ext = 0;
            if (!ch || !Rd(ch + 0x3A0, ext) || !ext) {
                snprintf(reply, n, "error no player");
                return;
            }
            float mx = 0.f, cur0 = 0.f, cur1 = 0.f;
            Rd(ext + 0x3AC, mx);
            uint64_t ent = SpiritGaugeEntry(base, ext + 0x1D8);
            if (ent) Rd(ent, cur0);
            int rc = -1;
            if (strstr(line, "max") && mx > 0.f) rc = SpiritGaugeFill(base, ext + 0x1D8, mx);
            if (ent) Rd(ent, cur1);
            Log("Spirit gauge ext=%llX ent=%llX max=%.1f cur %.1f->%.1f rc=%d", (unsigned long long)ext, (unsigned long long)ent, mx, cur0, cur1, rc);
            snprintf(reply, n, "ok gauge ext=%llX ent=%llX max=%.1f cur %.1f->%.1f rc=%d", (unsigned long long)ext, (unsigned long long)ent, mx, cur0, cur1, rc);
            return;
        }
        if (!strcmp(arg, "scan") || !strcmp(arg, "equip") || !strcmp(arg, "add")) {  // spirit scan | spirit add <id> | spirit equip <id> [slot] (add first if not owned)
            uint64_t ctx = 0, db = 0, pd = 0, in = 0;
            if (!Rd(base + Grva(0x45B9E30), ctx) || !ctx || !Rd(ctx + 0x3F0, db) || !db) {
                snprintf(reply, n, "error no ctx/DB (ctx=%llX db=%llX)", (unsigned long long)ctx, (unsigned long long)db);
                return;
            }
            if (!strcmp(arg, "scan")) {
                size_t k = (size_t)snprintf(reply, n, "ok scan db=%llX ids(rec+0x38!=0):", (unsigned long long)db);
                int cnt = 0;
                for (uint32_t id = 1; id < 0x4000; ++id) {
                    const uint64_t rec = SpiritRec(base, db, id);
                    uint32_t v = 0;
                    if (!rec || rec < 0x10000 || !Rd(rec + 0x38, v) || !v) continue;
                    ++cnt;
                    Log("SpiritScan id=%u(0x%X) rec=%llX +38=%u", id, id, (unsigned long long)rec, v);
                    if (cnt <= 24 && k + 24 < n) k += (size_t)snprintf(reply + k, n - k, " %X:%u", id, v);
                }
                snprintf(reply + (k < n ? k : n - 1), k < n ? n - k : 1, " total=%d", cnt);
                return;
            }
            unsigned id = 0, slot = 0;
            const bool isAdd = !strcmp(arg, "add");
            const char* p = strstr(line, isAdd ? "add" : "equip");
            if (!p || sscanf_s(p + (isAdd ? 3 : 5), "%i %u", &id, &slot) < 1 || slot > 1) {
                snprintf(reply, n, "error spirit add <id> | equip <id> [slot 0|1]");
                return;
            }
            if (!Rd(base + Grva(0x4751860), pd) || !pd || !Rd(pd, in) || !in) {
                snprintf(reply, n, "error player data not ready");
                return;
            }
            uint32_t before = 0, after = 0;
            Rd(in + 0x100 + slot * 4, before);
            int rc;
            if (isAdd) {
                rc = SpiritAddCall(base, pd, id);
                Log("Spirit add 2153E7C(P=%llX,id=%u) rc=%d", (unsigned long long)pd, id, rc);
                snprintf(reply, n, "ok add rc=%d id=%u", rc, id);
                return;
            }
            rc = SpiritEquipCall(base, pd, slot, id);
            Rd(in + 0x100 + slot * 4, after);
            Log("Spirit equip 217D8B4(P=%llX,slot=%u,id=%u) rc=%d %u->%u", (unsigned long long)pd, slot, id, rc, before, after);
            snprintf(reply, n, "ok equip rc=%d slot=%u id %u->%u", rc, slot, before, after);
            return;
        }
        uint64_t mgr = SpiritMgr(base, why, sizeof(why));
        float pp[4] = {};
        const bool pok = PlayerPos(pp);
        if (!strcmp(arg, "note")) {  // from the 2916C5C hook: rcx = mgr
            unsigned long long m = 0;
            if (sscanf_s(line + 12, "%llx", &m) == 1 && m) g_spMgr.store(m);
            snprintf(reply, n, "ok");
            return;
        }
        if (!strcmp(arg, "formkey")) {  // spirit formkey on [hex X]|off|status : log the summon body key, then one SetForm with that key (pc/soulcore-formkey)
            bosslog::FormKeyCmd(line, reply, n);
            return;
        }
        if (!strcmp(arg, "body")) {  // spirit body on|off|log : ally #53727 copy the summoned yokai model onto the player (applied on the next tick once the spirit is loaded)
            uint64_t inst, chr, st;
            SpiritChr(mgr, inst, chr, st);
            if (strstr(line, "off")) { g_spBody.store(0); SpiritBodyUndo(); }
            else if (strstr(line, "on")) g_spBody.store(1);
            if (chr) SpiritBodyApply(chr);  // with body off this only logs both pointers
            snprintf(reply, n, "ok body=%d chr=%llX state=%llu copied=%llX", g_spBody.load(), (unsigned long long)chr, (unsigned long long)st,
                     (unsigned long long)g_spBodyChr.load());
            return;
        }
        if (!strcmp(arg, "spawn") && mgr && pok) {
            alignas(16) float pos[4] = {pp[0], pp[1], pp[2], 1.0f};
            const int rc = SpiritCall(base, mgr, pos, 0.f);
            Log("Spirit spawn rc=%d mgr=%llX", rc, (unsigned long long)mgr);
        } else if (!strcmp(arg, "pin")) {
            g_spPin.store(1);
        } else if (!strcmp(arg, "unpin")) {
            g_spPin.store(0);
        } else if (!strcmp(arg, "tick")) {  // from the input hook: move the spirit chr onto the player (position only; model/attacks untouched)
            uint64_t inst, chr, st;
            SpiritChr(mgr, inst, chr, st);
            static const int iniBody = BR_SOULCORE ? IniInt(L"Spirit", L"Body", 0) : 0;  // [Spirit] Body=1 arms the copy from boot
            static bool armed = (iniBody && (g_spBody.store(1), true));
            (void)armed;
            if (g_spBody.load() && chr && st == 2) SpiritBodyApply(chr);
            const uint64_t sc = g_spSpawnChr.load();  // the seal's yokai from the 65A8A8 spawn (SpiritSpawnNote)
            if (g_spBody.load() && sc) SpiritBodyApply(sc);
            if (g_spPin.load() && pok && chr && st == 2) {
                alignas(16) float pos[4] = {pp[0], pp[1], pp[2], 1.0f};
                SpiritSetPos(chr, pos);
            }
            if (g_spPin.load() && pok && sc && g_spBodyChr.load() == sc) {
                alignas(16) float pos[4] = {pp[0], pp[1], pp[2], 1.0f};
                SpiritSetPos(sc, pos);
            }
            static double nextWait = 0;  // armed but nothing copied yet: say why every 5 s (the 22:07 run had 0 SpiritBody lines)
            if (g_spBody.load() && !g_spBodyChr.load() && (Ms() / 1000.0) >= nextWait) {
                static int waits = 0;
                nextWait = (Ms() / 1000.0) + 5.0;
                if (waits++ < 60)
                    Log("SpiritBody armed, waiting: guardian mgr=%llX chr=%llX state=%llu | seal spawn chr=%llX", (unsigned long long)mgr,
                        (unsigned long long)chr, (unsigned long long)st, (unsigned long long)sc);
            }
            snprintf(reply, n, "ok");
            return;
        }
        uint64_t inst, chr, st;
        SpiritChr(mgr, inst, chr, st);
        uint32_t id = 0;
        if (mgr) Rd(mgr + 0x10, id);
        float sp[4] = {};
        if (chr) Copy(sp, chr + 0x110, 16);
        snprintf(reply, n, "ok spirit %s id=%u inst=%llX state=%llu chr=%llX pos=%.0f,%.0f,%.0f pin=%d", why, id, (unsigned long long)inst,
                 (unsigned long long)st, (unsigned long long)chr, sp[0], sp[1], sp[2], g_spPin.load());
        return;
    }
    if (!strcmp(cmd, "tenshin")) {  // tenshin status|ready|begin|aura|hide|show|end : Nioh Tenshin (player form) live probe, RVAs from notes/player-transform-nioh-tenshin.md
        const uint64_t base = (uint64_t)GetModuleHandleW(nullptr);
        const uint64_t ch = PlayerChar();
        uint64_t ext = 0;
        if (!ch || !Rd(ch + 0x3A0, ext) || !ext) {
            snprintf(reply, n, "error no player");
            return;
        }
        const uint64_t T = ext + 0x178;
        // the prologue of Begin (0x843010) must match the dump, else the RVA is wrong for this build
        static const uint8_t kBeginHead[8] = {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x08, 0x48};
        uint8_t head[8] = {};
        if (!Copy(head, base + Grva(0x843010), 8) || memcmp(head, kBeginHead, 8)) {
            snprintf(reply, n, "error Begin prologue mismatch %02X%02X%02X%02X", head[0], head[1], head[2], head[3]);
            return;
        }
        uint8_t fl = 0;
        float g0 = 0, g1 = 0;
        uint32_t ft = 0, f188 = 0, hide = 0;
        Rd(T + 0x10, fl); Rd(T + 0x14, ft); Rd(T + 0x20, g0); Rd(T + 0x24, g1); Rd(ext + 0x188, f188); Rd(ch + 0xB440, hide);
        int rc = 0;
        if (!strcmp(arg, "ready")) {
            fl |= 1;
            WrBytes(T + 0x10, &fl, 1);
        } else if (!strcmp(arg, "fill")) {  // gauge to max (and ready) so the real button path can fire
            float mx = 0;
            Rd(T + 0x24, mx);
            WrBytes(T + 0x20, &mx, 4);
            fl |= 1;
            WrBytes(T + 0x10, &fl, 1);
        } else if (!strcmp(arg, "go")) {  // the game's own input path: bit 0xE on the next 2E6C34 call (gauge must be full: use fill first)
            bosslog::TenshinGo();
        } else if (!strncmp(arg, "move", 4)) {  // move <hex>: play action id via 0x66C4A8 (after fill + go has begun the form)
            { const char* fp = strstr(line, "move"); bosslog::TenshinMove(fp ? (uint32_t)strtoul(fp + 4, nullptr, 16) : 0); }
        } else if (!strncmp(arg, "load", 4)) {  // load <hex|all>: ally #47612 pack load via 3F9C10; poll with `packs`/`load` again
            const char* fp = strstr(line, "load") + 4;
            char sb[900];
            bosslog::TenshinLoad(strstr(fp, "all") ? 0 : (uint32_t)strtoul(fp, nullptr, 16), sb, sizeof(sb));
            snprintf(reply, n, "ok%s", sb);
            return;
        } else if (!strncmp(arg, "preload", 7) || !strncmp(arg, "packs", 5)) {  // preload <hex|all> : request the body-pack load then report 2C8; packs <hex|all> : report only (ally #47587)
            const bool req = arg[0] == 'p' && arg[1] == 'r';
            const char* fp = strstr(line, req ? "preload" : "packs");
            fp += req ? 7 : 5;
            char sb[900];
            bosslog::TenshinPreload(strstr(fp, "all") ? 0 : (uint32_t)strtoul(fp, nullptr, 16), req, sb, sizeof(sb));
            snprintf(reply, n, "ok%s", sb);
            return;
        } else if (!strcmp(arg, "slots")) {  // slots: gear-slot ids + form[+58] (pc2 #47582), also written to the log
            char sb[512];
            bosslog::TenshinSlots(ext, sb, sizeof(sb));
            snprintf(reply, n, "ok %s", sb);
            return;
        } else if (!strncmp(arg, "body", 4)) {  // body <0-3>: ally #52022 experiment on the hide-sphere slot
            bosslog::TenshinBody(atoi(arg + 4));
        } else if (!strncmp(arg, "setform", 7)) {
            { const char* fp = strstr(line, "setform"); bosslog::TenshinSetForm(fp ? (uint32_t)strtoul(fp + 7, nullptr, 16) : 0); }
        } else if (!strncmp(arg, "form", 4)) {  // form <hex>: [state+0xA68] value for the next go (0 clears)
            { const char* fp = strstr(line, "form"); bosslog::TenshinForm(fp ? (uint32_t)strtoul(fp + 4, nullptr, 16) : 0); }
        } else if (!strcmp(arg, "req")) {  // fill + the game's own request-bit setter fc1a20(state+0x1060, 0x4E, 1) on the game thread
            bosslog::TenshinReq();
        } else if (!strcmp(arg, "begin")) {
            rc = TCall(1, base, T, 0);
        } else if (!strcmp(arg, "aura")) {
            rc = TCall(2, base, T, 0);
        } else if (!strcmp(arg, "hide") || !strcmp(arg, "show")) {
            const bool h = !strcmp(arg, "hide");
            rc = TCall(3, base, ch, h ? 1 : 0);
        } else if (!strcmp(arg, "end")) {
            rc = TCall(4, base, T, 0);
        } else if (strcmp(arg, "status")) {
            snprintf(reply, n, "error tenshin status|ready|fill|go|req|begin|aura|hide|show|end");
            return;
        }
        uint8_t fl2 = 0;
        uint32_t f2 = 0, h2 = 0;
        Rd(T + 0x10, fl2); Rd(ext + 0x188, f2); Rd(ch + 0xB440, h2);
        Log("Tenshin %s rc=%d T=%llX flags %02X->%02X ext188 %X->%X hide %X->%X type=%u gauge=%.1f/%.1f", arg, rc, (unsigned long long)T, fl, fl2, f188, f2, hide, h2, ft, g0, g1);
        snprintf(reply, n, "ok %s rc=%d flags %02X->%02X transformed %d->%d hide %X->%X type=%u gauge=%.1f/%.1f", arg, rc, fl, fl2, (f188 >> 1) & 1, (f2 >> 1) & 1, hide, h2, ft, g0, g1);
        return;
    }
#endif  // BR_SOULCORE
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
    if (!strcmp(cmd, "again")) {  // #100425 test hook: the gate page's 'Fight again' item (same checks as UiAct), for timed retry runs
        if (!NativeGateMission() && !WavesGateNow()) { snprintf(reply, n, "refused: no native gate mission"); return; }
        const int row = NativeGateRow();
        const Snap s = ReadSnap();
        const char* why = row < 0 ? "native boss row is no longer owned" : !g_exitOk ? g_exitWhy : ExitGate(s);
        if (!why) why = AttemptReturnGate();
        if (why) { snprintf(reply, n, "refused: %s", why); return; }
        Post(kReqNativeAgain, row, "pipe again");
        snprintf(reply, n, "queued %s", g_rows[(size_t)row].id);
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
                // A scroll has no arena: its Fight now is the record start, same as the menu (ActScrollFight).
                const bool scroll = arena && g_rows[i].route == rcore::kE;
                if (const char* w = arena && !scroll ? CanArena((int)i) : CanFight((int)i)) {
                    snprintf(reply, n, "refused %s", w);
                    return;
                }
                Post(scroll ? kReqEmaki : arena ? kReqFight : kReqShrine, (int)i, "pipe");
                snprintf(reply, n, "queued %s %s start=%u", g_rows[i].id, cmd, g_rows[i].fightStart);
                return;
            }
        snprintf(reply, n, "error no row %s", arg);
        return;
    }
    if (!strcmp(cmd, "trainscroll")) {  // trainscroll <id>: #79069 same request as the Training rows, for live tests
        for (size_t i = 0; i < g_rows.size(); ++i)
            if (!strcmp(g_rows[i].id, arg)) {
                if (!rcore::WavesRow(g_rows[i])) { snprintf(reply, n, "refused not a Waves row"); return; }
                if (!g_waveVetoOk) { snprintf(reply, n, "refused wave guard not installed"); return; }
                if (const char* w = CanFight((int)i)) { snprintf(reply, n, "refused %s", w); return; }
                Post(kReqEmakiTrain, (int)i, "pipe");
                snprintf(reply, n, "queued %s trainscroll", g_rows[i].id);
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

bool PlayerExists() { return PlayerChar() != 0; }
void NoteNativeWin() {  // any thread; the boss-mode end or mission clear of the running fight
    if (g_attemptPhase.load() == aActive) g_attemptWonSeq.store(g_attempt.sequence);
}
void NoteWavesEnd() {  // BossBattleModeEnd with no BossBattleMode open, on the Fight now waves row
    if (!g_fight.load()) return;
    const int fr = g_fightRow;
    if (fr < 0 || (size_t)fr >= g_rows.size()) return;
    const Row& r = g_rows[(size_t)fr];
    if (!rcore::WavesUnmatchedEndWins(r.fightSignal, false)) return;
    if (g_attempt.mission && r.missionId && g_attempt.mission != r.missionId) return;
    Log("Rematch: waves win %s (BossBattleModeEnd, no BossBattleMode)", r.id);
    using Fn = void (*)(int);
    if (const auto f = (Fn)TrainingExport("Nioh3Training_WavesFight")) f(2);
}

int FightLogMode() {
    if (g_practiceOwned.load()) return 2;             // Practice
    if (g_replayIn.load() || g_fight.load()) return 1;  // Boss Match
    return 3;                                         // story / field boss
}
const char* FightLogId() {
    if (FightLogMode() == 3 || g_fightRow < 0 || (size_t)g_fightRow >= g_rows.size()) return "";
    return g_rows[(size_t)g_fightRow].id;
}
const char* FightLogName() {
    const char* n = RowName(g_fightRow);
    return n ? n : "";
}
const char* NameForPlacement(uint32_t id) {
    if (!id) return "";
    for (size_t i = 0; i < g_rows.size(); ++i)
        if (g_rows[i].placement == id && g_rows[i].name[0]) return g_rows[i].name;
    return "";
}

}  // namespace rematch
}  // namespace addon

// user #13274: Nioh3Training pauses the game while our mod menu is open in a Boss Match. 1 while a Boss Match replay mission, a Fight now
// start or a native practice attempt is running (read from Nioh3Training's menu-pause thread; atomics only).
extern "C" __declspec(dllexport) int Nioh3BossRematch_InMatch() {
    namespace r = addon::rematch;
    return r::g_replayIn.load() || r::g_fight.load() || r::g_practiceOwned.load() ? 1 : 0;
}

// Training calls this once the Fight now cover has actually ended. 1 only for a lesson row: one closed-menu toast.
extern "C" __declspec(dllexport) int Nioh3BossRematch_LessonNote() {
    namespace r = addon::rematch;
    if (r::g_fightRow < 0 || (size_t)r::g_fightRow >= r::g_rows.size() || !r::g_rows[(size_t)r::g_fightRow].lesson)
        return 0;
    r::ToastF(r::cWhite, 8000, "%s", r::kLessonLine);
    return 1;
}
