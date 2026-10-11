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

#include <atomic>
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
bool g_live = false, g_raOk = false, g_emakiOk = false, g_fnowOk = false, g_warpOk = false;
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
    bool hiddenDuels = false, directStart = false, showAll = true, showUnverified = false, fightNow = false, fightWarp = false, arenaCapture = false,
         fightSpawn = true;
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
enum ReqKind : uint32_t { kReqNone = 0, kReqOpen = 1, kReqDirect = 2, kReqAbort = 3, kReqEmaki = 4, kReqScroll = 5, kReqFight = 6, kReqShrine = 7, kReqExit = 8 };
std::atomic<uint32_t> g_req{0};  // kind | (row + 1) << 8
enum State : int { kIdle = 0, kMenuOpen = 1, kInReplay = 2, kEmaki = 3 };
std::atomic<int> g_state{kIdle};
std::atomic<int> g_activeRow{-1};
std::atomic<bool> g_replayIn{false};
std::atomic<bool> g_fight{false};  // the active replay is a Fight now start
std::atomic<uint32_t> g_loadMs{0};  // request -> mission load finished, last Fight now / replay (status 'loadms', the slow-load check)
std::atomic<ULONGLONG> g_lastTick{0}, g_qChangedAt{0};
uint64_t g_qHash = 0;

const char* RowName(int row) { return row >= 0 && (size_t)row < g_rows.size() ? g_rows[(size_t)row].name : ""; }

void Post(uint32_t kind, int row, const char* via) {
    g_req.store(kind | ((uint32_t)(row + 1) << 8));
    static const char* const kn[] = {"", "R-a open the replay menu", "R-b direct start", "abort", "Picture Scroll (E1 redirect)", "Picture Scroll (game's pick)",
                                     "Fight now (in the arena)", "Shrine before the boss", "leave the mission (return)"};
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
    int st = 0;            // 0 off, 1 wanted (record not patched yet), 2 applied
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
// game thread: patch the record (again if the game reloaded / rewrote it)
void SpawnApply(const Row& r, const char* when) {
    const uint64_t rec = CallWpFind(g_sp.point);
    if (!rec) return;  // the point set is not loaded (yet): retried every tick
    uint32_t id = 0, flags = 0;
    float cur[4] = {};
    if (!Rd(rec, id) || id != g_sp.point || !Rd(rec + 8, flags) || !Copy(cur, rec + 0x40, 16)) {
        g_sp.st = 0;
        SpawnLast("failed");
        Log("Rematch: F %s: spawn override NOT applied - warp-point record %u at %016llX is not readable as expected (id %u)", r.id, g_sp.point,
            (unsigned long long)rec, id);
        return;
    }
    if (rec == g_sp.rec && g_sp.st == 2 && !memcmp(cur, g_sp.mine, 16) && flags == g_sp.mineFlags) return;  // still ours
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
    if (!SpawnWrite(rec, g_sp.mine, g_sp.mineFlags)) {
        WrBytes(rec + 0x40, g_sp.orig, 16);
        g_sp.st = 0;
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
void SpawnRestore(const char* why) {
    if (g_sp.st == 2) {
        float cur[4] = {};
        uint32_t flags = 0;
        const bool rd = CallWpFind(g_sp.point) == g_sp.rec && Copy(cur, g_sp.rec + 0x40, 16) && Rd(g_sp.rec + 8, flags);
        if (rd && !memcmp(cur, g_sp.mine, 16) && flags == g_sp.mineFlags) {
            const bool ok = SpawnWrite(g_sp.rec, g_sp.orig, g_sp.origFlags);
            Log("Rematch: spawn override restored (%s): record %u spawn %.0f %.0f %.0f yaw %.0f flags %X%s", why, g_sp.point, g_sp.orig[0], g_sp.orig[1],
                g_sp.orig[2], g_sp.orig[3], g_sp.origFlags, ok ? "" : " - WRITE FAILED");
        } else
            Log("Rematch: spawn override (%s): record %u is no longer ours (reloaded or changed by the game) - left as the game has it", why, g_sp.point);
    }
    g_sp.st = 0;
    g_sp.rec = 0;
    g_sp.landCheck = false;
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
    ToastF(cOrange, 6000, "Rematch refused: %s", why);
}
void ToIdle(const char* why) {
    UnhideRestore(why);
    SpawnRestore(why);
    MapRestoreNow(why);
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

void ExecDirect(int row, const Snap& s, ULONGLONG t, bool fight, bool warp) {
    const Row& r = g_rows[(size_t)row];
    const char* why = nullptr;
    const uint32_t start = fight ? r.fightStart : 0;
    if (fight) {
        if (!g_cfg.fightNow) return Refuse("Fight now is off ([Rematch] FightNow = 0)");
        if (!g_fnowOk) return Refuse(g_fnowWhy);
        if (!start) return Refuse("no start point next to this boss in the catalogue (use the replay)");
        if (warp && !r.atFight && !(r.GateDoor() && g_gateOk) && (!g_cfg.fightWarp || !g_warpOk || !r.arenaOk))
            return Refuse(!g_cfg.fightWarp ? "the arena warp is off ([Rematch] FightNowWarp = 0)" : !g_warpOk ? g_warpWhy : "no arena position for this boss yet");
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
    ToastF(cGreen, 8000, fight ? (warp ? "Rematch: into the fight with %s..." : "Rematch: to the shrine before %s...") : "Rematch: starting %s", r.name);
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

// game thread, inside the Fight now mission: warp into the arena trigger once the mission is idle
void DoWarp(const Snap& s, ULONGLONG t) {
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
        ToastF(cRed, 8000, "Rematch: the arena warp failed (see the log); you are at the start point");
    } else {
        Log("Rematch: F %s: after warp #%d the player is %s the trigger box", r.id, g_warps, rcore::InTriggerBox(r, after) ? "inside" : r.boxOk ? "OUTSIDE" : "(no box data)");
        ToastF(cGreen, 5000, "Rematch: into the arena - %s", r.name);
    }
}

// game thread, every tick inside the Fight now mission: PASS = a BossBattleMode after the first warp; re-warp (<= 3) when the
// player is not in the trigger box 1.5 s after a warp; FAIL = no fight 8 s after the first warp (loading time not counted)
void FnVerify(const Snap& s, ULONGLONG t) {
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
    if (!g_fightWarp && !g_gateReq && r.atFight && r.bossOk && !g_warps && t - g_fnFirst > 10000 && !fight::g_active) {
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
        if (!in && backAtStart && g_fightWarp && !g_gateReq && g_warps < 3 && !fight::g_active) {
            Log("Rematch: F %s: player %.0f %.0f %.0f is not in the trigger box 1.5 s after warp #%d - warping again", r.id, p[0], p[1], p[2], g_warps);
            g_warpPending = true;
            g_warpDue = t;
        }
        g_fnCheckAt = t + 1500;
    }
}

// game thread, inside the Fight now mission: open the boss gate the way the game does (door / StartWarp), once
void GateTick(const Snap& s, ULONGLONG t) {
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
                        ToastF(cOrange, 8000, "Rematch: the game did not use the boss start point (see the log)");
                    if (g_fightWarp && !g_warps) {  // landed (3 s with no load and a player) + 1 s settle
                        g_warpPending = true;
                        g_warpDue = t + 1000;
                    }
                    if (g_gateWant.load()) g_gateDue = t + (g_fightWarp ? 2500 : 1000);
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
        }
        if (!s.st[2] && g_fightLoading && g_fightWarp && !(g_gateReq && g_fn.load() == kFnWarped)) {  // a reload (death / retry) ended: back into the arena; never during a gate intro
            g_warpPending = true;
            g_warpDue = t + 4500;  // 1.5 s after the 3 s settle the landing uses
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
    if (!g_exitSince) g_exitSince = t;
    else if (t - g_exitSince > 5000) {
        Log("Rematch: replay mission exited (stage now %u, mission state +28h 0x%X)%s", s.ms14, s.ms28,
            g_fight.load() ? (g_sp.st == 2 ? " - Fight now over (the spawn record is restored next)" : " - Fight now over (nothing was patched, nothing to restore)") : "");
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
            ToastF(cCyan, 15000, "Rematch self-test: %d pass, %d fail, %d skip (see the log)", st.pass, st.fail, st.skip);
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
    if (!g_sp.st || g_fightRow < 0 || !g_fight.load()) return;
    const Row& r = g_rows[(size_t)g_fightRow];
    if (s.online || s.guest) {
        Log("Rematch: F %s: online / co-op during Fight now - spawn override removed", r.id);
        SpawnRestore("online");
        SpawnLast("failed");
        return;
    }
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

void Tick() {
    const ULONGLONG t = Ms();
    g_lastTick.store(t);
    Snap s = ReadSnap();
    if (s.st[2] && !g_wasLoading) {  // a load begins: keep where the player stood on the frame before
        memcpy(g_posAtLoad, g_posLive, sizeof(g_posAtLoad));
        g_posAtLoadOk = g_posLiveOk;
        g_loadRiseAt = t;
    }
    const bool loadEnded = g_wasLoading && !s.st[2];
    g_wasLoading = s.st[2] != 0;
    if (!s.st[2]) g_posLiveOk = s.player && PlayerPos(g_posLive);
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
            lastMission = s.ms28;
        }
    }
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
        if (kind == kReqExit) {
            g_pend = 0;
            if (!g_exitOk) Refuse(g_exitWhy);
            else if (s.online || s.guest) Refuse("online / co-op: not leaving");
            else if (s.st[2]) Refuse("loading");
            else if (!s.msOk || !s.ms28) Refuse("not in a mission");
            else {
                const int rc = CallReturn();
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
            else if (kind == kReqEmaki) ExecEmaki(row, t);
            else if (kind == kReqScroll) ExecEmaki(-1, t);
            else ExecOpen(row, s);
        }
    }
    TrackReplay(s, t);
    SelfTestTick(s, t);
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

std::wstring ArenaPath() { return Beside(L"Arenas", L"Nioh3BossRematch_arenas.tsv"); }

// BossBattleMode entry (game thread): the player position at every fight start; ArenaCapture = 1 appends it to the arenas file
void HkArena(uint64_t, uint64_t, uint64_t, uint64_t, const uint64_t*) {
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
const char* CanArena(int row) {
    if (const char* w = CanFight(row)) return w;
    const Row& r = g_rows[(size_t)row];
    if (r.atFight) return nullptr;
    if (r.GateDoor() && g_gateOk) return nullptr;
    if (!g_cfg.fightWarp) return "arena warp off ([Rematch] FightNowWarp = 1 turns it on)";
    if (!g_warpOk) return g_warpWhy;
    if (!r.arenaOk) return "no arena position yet (fight it once with [Rematch] ArenaCapture = 1)";
    return nullptr;
}

// ---- UI model (any thread) ----
int g_sel = 0, g_confirm = -9;
uint32_t g_pageGen0 = 0;
char g_msg[160] = "";
constexpr int kItOpen = -1, kItAbort = -2, kItScroll = -3;
constexpr int kItFight = 0x10000;   // kItFight + row = "Fight now (in the arena)" under that boss row
constexpr int kItShrine = 0x20000;  // kItShrine + row = "Shrine before the boss" (the start point alone)

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
            if (g_cfg.fightNow && g_fnowOk && r.fightStart && !r.atFight && grp != 2 && n < cap) out[n++] = kItShrine + (int)i;
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
    g_cfg.fightWarp = IniInt(L"Rematch", L"FightNowWarp", 0) != 0;
    g_cfg.arenaCapture = IniInt(L"Rematch", L"ArenaCapture", 0) != 0;
    g_cfg.fightSpawn = IniInt(L"Rematch", L"FightNowSpawn", 1) != 0;
    wchar_t stw[512] = {};
    GetPrivateProfileStringW(L"Rematch", L"SelfTest", L"", stw, 512, ini);
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
    if (!InstallGroup("FIGHT", &why)) {
        Log("Rematch: off - FIGHT hooks: %s", why);
        return false;
    }
    if (!InstallPump(&Tick, &why)) {
        Log("Rematch: off - pump: %s", why);
        return false;
    }
    g_live = true;
    int c[5] = {}, nf = 0, nfh = 0, nar = 0, nat = 0;
    for (const Row& r : g_rows) {
        c[r.route]++;
        nf += r.fightStart != 0;
        nfh += r.fightStart != 0 && !strcmp(r.fightConf, "high");
        nar += r.fightStart && r.arenaOk;
        nat += r.fightStart && r.atFight;
    }
    Log("Rematch: on - %zu catalogue rows (R %d, H %d, D %d, E %d, X %d; %d verified by verify_all); R-a %s%s, E %s%s; HiddenDuels %d, DirectStart %d, "
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
        const bool arena = item < kItShrine;
        const int row = item - (arena ? kItFight : kItShrine);
        if (g_status[(size_t)row].load() != 2) {
            snprintf(g_msg, sizeof(g_msg), "Not beaten on this journey: Fight now only replays bosses you have beaten.");
            return false;
        }
        if (const char* w = arena ? CanArena(row) : CanFight(row)) {
            snprintf(g_msg, sizeof(g_msg), "%s", w);
            return false;
        }
        if (g_confirm != item) {
            g_confirm = item;
            const Row& r = g_rows[(size_t)row];
            if (arena)
                snprintf(g_msg, sizeof(g_msg), "Fight %s now, inside the arena? A confirm / B back. Solo only.", r.name);
            else
                snprintf(g_msg, sizeof(g_msg), "Start at the %s %d m before %s? A confirm / B back. Solo only.", r.fightKind, r.fightDist, r.name);
            return false;
        }
        g_confirm = -9;
        Post(arena ? kReqFight : kReqShrine, row, "the menu");
        ToastF(cWhite, 6000, arena ? "Rematch: into the fight with %s..." : "Rematch: to the shrine before %s...", g_rows[(size_t)row].name);
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
        else if (it[i] >= kItShrine) {
            const Row& r = g_rows[(size_t)(it[i] - kItShrine)];
            const uint8_t stt = g_status[(size_t)(it[i] - kItShrine)].load();
            const uint32_t c = stt == 2 ? (!strcmp(r.fightConf, "high") ? cGreen : !strcmp(r.fightConf, "medium") ? cYellow : cOrange) : cDim;
            if (stt != 2) add(sel ? cWhite : cDim, sel, "%s    Shrine before the boss   - not beaten on this journey", mk);
            else add(sel ? cWhite : c, sel, "%s    Shrine before the boss   %s, %d m   [%s]", mk, r.fightKind, r.fightDist, r.fightConf);
        } else if (it[i] >= kItFight) {
            const int row = it[i] - kItFight;
            const Row& r = g_rows[(size_t)row];
            const uint8_t stt = g_status[(size_t)row].load();
            const char* why = CanArena(row);
            const char* conf = r.atFight ? r.fightConf : r.arenaConf;
            const uint32_t c = why || stt != 2 ? cDim : (!strcmp(conf, "high") || !strcmp(conf, "captured")) ? cGreen : !strcmp(conf, "medium") ? cYellow : cOrange;
            if (stt != 2) add(sel ? cWhite : cDim, sel, "%s    Fight now (in the arena)   - not beaten on this journey", mk);
            else if (why) add(sel ? cWhite : c, sel, "%s    Fight now (in the arena)   - %s", mk, why);
            else add(sel ? cWhite : c, sel, "%s    Fight now (in the arena)   %s   [%s]", mk, r.atFight ? "the mission starts at the fight" : "warp into the trigger", conf);
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
        else if (item >= kItShrine) {
            const Row& r = g_rows[(size_t)(item - kItShrine)];
            snprintf(footer, nf, "%s: starts the mission at start point %u (%s, %d m from the boss, %s confidence). A start, B back.",
                     r.mission[0] ? r.mission : r.name, r.fightStart, r.fightKind, r.fightDist, r.fightConf);
        } else if (item >= kItFight) {
            const Row& r = g_rows[(size_t)(item - kItFight)];
            if (r.atFight) snprintf(footer, nf, "%s: the mission's only start point is at the fight (%d m). A start, B back.", r.mission[0] ? r.mission : r.name, r.fightDist);
            else snprintf(footer, nf, "%s: start point %u, then a warp to %.0f %.0f %.0f inside the arena trigger (%s). A start, B back.",
                          r.mission[0] ? r.mission : r.name, r.fightStart, r.arena[0], r.arena[1], r.arena[2], r.arenaSrc);
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
