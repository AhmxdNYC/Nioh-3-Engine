// rematch_core.h - the pure parts of the rematch (no game calls), shared by rematch.cpp and tests/rematch_harness.cpp:
// catalogue + verified-results parsing, the E1 Emaki map redirect (snapshot / CRC / restore), the E lifecycle tracker and the
// "Fight now" (E1 straight to the boss) request check.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>
#include "../../Nioh3Training/src/fallback_rosters_gen.h"

namespace rcore {

enum Route : uint8_t { kR = 0, kH = 1, kD = 2, kE = 3, kX = 4 };
inline const char* RouteName(Route r) {
    static const char* const n[] = {"replay", "hidden duel", "direct", "waves", "no route"};
    return n[r];
}

struct Row {
    char id[24] = "", name[64] = "", mission[64] = "", region[24] = "", reason[200] = "";
    Route route = kX;
    uint16_t missionId = 0;
    uint32_t storyMission = 0; // catalogue story_mission. Scroll rows name the scroll mission here; replay_mission_id stays empty.
    int emakiIndex = -1;       // E rows: Emaki record index (0..19)
    int wakeStep = 0;          // E rows: catalogue wake_step = the WaveNudgeAt offset that woke the record in the test runs (0 = record warp / unmeasured)
    uint32_t placement = 0;    // expected boss placement (BossLog Create id) or 0
    int world = -1;            // catalogue region_byte: the world the boss stands in (-1 = unknown)
    bool hidden = false;
    bool verifiedCat = false;  // catalogue column
    bool verifiedRun = false;  // Nioh3BossRematch_verified.tsv (verify_all.ps1)
    // Fight now (R / H / D rows): the mission start point next to the boss (catalogue stage_mission), 0 = not offered
    uint32_t fightStart = 0;
    int fightDist = -1;        // metres from the boss placement
    char fightConf[8] = "";    // high / medium / low
    char fightKind[20] = "";   // boss retry point / shrine / spawn point / gadget point / mission start
    float warp[4] = {};        // the start point's spawn x y z yaw (game units / degrees), for the log
    // Fight now v2 (in the arena): a point just inside the arena trigger box (catalogue arena_x..arena_yaw) or a captured position
    bool arenaOk = false;      // arena[] valid: warp there after the load
    bool atFight = false;      // the mission already starts at the fight (one start point <= 30 m): no warp needed
    bool trFillTried = false;  // #65732: start point looked up in Nioh3Training's arena table once
    float arena[4] = {};
    uint32_t arenaBox = 0;
    char arenaConf[12] = "";   // high / medium / low / captured
    char arenaSrc[72] = "";
    bool boxOk = false;
    float box[7] = {};         // the trigger box: centre x y z, yaw deg, full size x y z (catalogue arena_boxgeo)
    // the boss gate: the id the boss start function waits on (Function::WaitWarpStart); door = the boss gate gadget, box = an arena box
    bool bossOk = false;
    float boss[3] = {};        // the boss placement position (catalogue boss_pos)
    uint32_t gateId = 0;
    char gateKind[8] = "";
    bool GateDoor() const { return gateId && strcmp(gateKind, "box") != 0; }
    bool FightNowReady() const { return fightStart && (arenaOk || atFight || gateId); }
    bool Verified() const { return verifiedCat || verifiedRun; }
    // The mission script's running function calls Character::IsDeathGroupEnemy and never Mission::BossBattleMode.
    // Filled by tools/rematch_catalog.py from the script, not from a row list. The menu reads it before that script runs.
    bool lesson = false;
};

inline void Ascii(char* dst, size_t n, const std::string& src) {
    size_t k = 0;
    for (unsigned char c : src) {
        if (k + 1 >= n) break;
        dst[k++] = (c >= 0x20 && c < 0x7F) ? (char)c : '?';
    }
    dst[k] = 0;
}

inline std::vector<std::string> SplitTab(const std::string& line) {
    std::vector<std::string> f;
    size_t a = 0;
    for (;;) {
        const size_t b = line.find('\t', a);
        f.push_back(line.substr(a, b == std::string::npos ? std::string::npos : b - a));
        if (b == std::string::npos) break;
        a = b + 1;
    }
    return f;
}
inline std::vector<std::string> Lines(const std::string& text) {
    std::vector<std::string> out;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t e = text.find('\n', pos);
        if (e == std::string::npos) e = text.size();
        std::string line = text.substr(pos, e - pos);
        pos = e + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        out.push_back(line);
    }
    return out;
}

// -1 no header, else rows parsed (by header names; unknown columns ignored)
inline int ParseCatalogue(const std::string& text, std::vector<Row>& rows, size_t cap = 512) {
    const auto ls = Lines(text);
    if (ls.empty()) return -1;
    const auto h = SplitTab(ls[0]);
    auto ci = [&](const char* k) {
        for (size_t i = 0; i < h.size(); ++i)
            if (h[i] == k) return (int)i;
        return -1;
    };
    const int cId = ci("id"), cName = ci("name"), cMis = ci("mission_name"), cReg = ci("region"), cRoute = ci("route"),
              cMid = ci("replay_mission_id"), cHid = ci("hidden"), cVer = ci("verified"), cEi = ci("emaki_index"),
              cPl = ci("placement_id"), cWhy = ci("reason"), cSt = ci("stage_mission"), cFk = ci("fight_kind"),
              cFd = ci("fight_dist_m"), cFc = ci("fight_conf"), cWx = ci("warp_x"), cWy = ci("warp_y"), cWz = ci("warp_z"),
              cWyaw = ci("warp_yaw"), cAx = ci("arena_x"), cAy = ci("arena_y"), cAz = ci("arena_z"), cAyaw = ci("arena_yaw"),
              cAbox = ci("arena_box"), cAsrc = ci("arena_src"), cAconf = ci("arena_conf"), cAgeo = ci("arena_boxgeo"), cGid = ci("gate_id"), cGk = ci("gate_kind"), cBp = ci("boss_pos"), cRb = ci("region_byte"),
              cLesson = ci("lesson"), cStory = ci("story_mission"), cWake = ci("wake_step");
    if (cId < 0 || cName < 0 || cRoute < 0) return -1;
    for (size_t li = 1; li < ls.size() && rows.size() < cap; ++li) {
        const auto f = SplitTab(ls[li]);
        auto col = [&](int c) -> std::string {
            std::string v = c >= 0 && (size_t)c < f.size() ? f[(size_t)c] : std::string();
            return v == "-" ? std::string() : v;
        };
        Row r;
        const std::string rt = col(cRoute);
        r.route = rt == "R" ? kR : rt == "H" ? kH : rt == "D" ? kD : rt == "E0" ? kE : kX;
        if (rt.empty()) continue;
        Ascii(r.id, sizeof(r.id), col(cId));
        Ascii(r.name, sizeof(r.name), col(cName));
        Ascii(r.mission, sizeof(r.mission), col(cMis));
        Ascii(r.region, sizeof(r.region), col(cReg));
        Ascii(r.reason, sizeof(r.reason), col(cWhy));
        r.missionId = (uint16_t)strtoul(col(cMid).c_str(), nullptr, 0);
        r.storyMission = (uint32_t)strtoul(col(cStory).c_str(), nullptr, 0);
        r.emakiIndex = col(cEi).empty() ? -1 : atoi(col(cEi).c_str());
        r.wakeStep = col(cWake).empty() ? 0 : atoi(col(cWake).c_str());  // out of range is clamped where it is used (WaveNudgeAt)
        r.placement = (uint32_t)strtoul(col(cPl).c_str(), nullptr, 0);
        r.hidden = col(cHid) == "1";
        if (!col(cRb).empty()) r.world = (int)strtol(col(cRb).c_str(), nullptr, 0);  // #12444: the boss's world (teleport fast-load ready check)
        r.verifiedCat = col(cVer) == "1";
        r.lesson = col(cLesson) == "1";
        if ((r.route == kR || r.route == kH || r.route == kD) && !r.missionId) continue;
        if ((r.route == kR || r.route == kH || r.route == kD) && !col(cFc).empty() && !col(cSt).empty()) {
            r.fightStart = (uint32_t)strtoul(col(cSt).c_str(), nullptr, 0);
            r.fightDist = col(cFd).empty() ? -1 : atoi(col(cFd).c_str());
            Ascii(r.fightConf, sizeof(r.fightConf), col(cFc));
            Ascii(r.fightKind, sizeof(r.fightKind), col(cFk));
            const int wc[4] = {cWx, cWy, cWz, cWyaw};
            for (int k = 0; k < 4; ++k) r.warp[k] = (float)atof(col(wc[k]).c_str());
            Ascii(r.arenaConf, sizeof(r.arenaConf), col(cAconf));
            Ascii(r.arenaSrc, sizeof(r.arenaSrc), col(cAsrc));
            r.atFight = col(cAsrc).rfind("mission start", 0) == 0;
            if (!col(cAx).empty() && !col(cAy).empty() && !col(cAz).empty()) {
                const int ac[4] = {cAx, cAy, cAz, cAyaw};
                for (int k = 0; k < 4; ++k) r.arena[k] = (float)atof(col(ac[k]).c_str());
                r.arenaBox = (uint32_t)strtoul(col(cAbox).c_str(), nullptr, 0);
                r.arenaOk = true;
            }
            r.gateId = (uint32_t)strtoul(col(cGid).c_str(), nullptr, 0);
            {
                std::string bp = col(cBp);
                for (auto& ch : bp)
                    if (ch == '_') ch = ' ';
                r.bossOk = !bp.empty() && sscanf_s(bp.c_str(), "%f %f %f", &r.boss[0], &r.boss[1], &r.boss[2]) == 3;
            }
            Ascii(r.gateKind, sizeof(r.gateKind), col(cGk));
            const std::string geo = col(cAgeo);
            if (!geo.empty()) {
                std::string g = geo;
                for (auto& ch : g)
                    if (ch == '_') ch = ' ';
                r.boxOk = sscanf_s(g.c_str(), "%f %f %f %f %f %f %f", &r.box[0], &r.box[1], &r.box[2], &r.box[3], &r.box[4], &r.box[5], &r.box[6]) == 7;
            }
        }
        if (r.route == kE) {
            const int wc[4] = {cWx, cWy, cWz, cWyaw};
            for (int k = 0; k < 4; ++k) r.warp[k] = (float)atof(col(wc[k]).c_str());
        }
        if (r.route == kE && (r.emakiIndex < 0 || r.emakiIndex > 255)) continue;
        rows.push_back(r);
    }
    return (int)rows.size();
}

// Picture Scroll mission. A chosen scroll row uses its own story_mission.
// The game's pick (row < 0) uses that value only when every scroll row names the same one.
inline uint32_t ScrollMission(const std::vector<Row>& rows, int row) {
    if (row >= 0 && (size_t)row < rows.size()) {
        const Row& r = rows[(size_t)row];
        if (r.route == kE && r.storyMission) return r.storyMission;
    }
    uint32_t shared = 0;
    for (const Row& r : rows) {
        if (r.route != kE || !r.storyMission) continue;
        if (!shared) shared = r.storyMission;
        else if (shared != r.storyMission) return 0;
    }
    return shared;
}

// Roster member count for this catalogue id. 0 = no generated roster (a single boss, a scroll, or an unresolved row).
inline int RosterMembers(const char* id) {
    const FallbackRoster* row = FallbackRosterFind(id);
    return row ? (int)row->count : 0;
}

// Short era label from the region text already on the row. Cut at '(', drop a standalone year,
// so "Warring States (Toka" and "1572 Warring States" share one label. No boss-name list.
inline void EraKey(const char* in, char* out, int cap) {
    if (!out || cap <= 0) return;
    out[0] = 0;
    if (!in || !in[0]) return;
    char tmp[96];
    int n = 0;
    for (const char* p = in; *p && *p != '(' && n < (int)sizeof(tmp) - 1; ++p) {
        if (*p == '\r' || *p == '\n') break;
        tmp[n++] = *p;
    }
    while (n > 0 && (tmp[n - 1] == ' ' || tmp[n - 1] == '\t')) --n;
    tmp[n] = 0;
    char* w = out;
    int left = cap - 1;
    for (char* tok = tmp; *tok && left > 0;) {
        while (*tok == ' ' || *tok == '\t') ++tok;
        if (!*tok) break;
        char* end = tok;
        while (*end && *end != ' ' && *end != '\t') ++end;
        const int len = (int)(end - tok);
        bool year = len == 4;
        for (int i = 0; year && i < 4; ++i) year = tok[i] >= '0' && tok[i] <= '9';
        if (!year) {
            if (w != out && left > 0) { *w++ = ' '; --left; }
            const int copy = len < left ? len : left;
            memcpy(w, tok, (size_t)copy);
            w += copy;
            left -= copy;
        }
        tok = end;
    }
    *w = 0;
}

// Human #73240: Boss Match list groups from the rows already loaded. No hand-kept boss id list.
// Stops: All, Multi, one per distinct era (catalogue order), Scrolls.
// A scroll stays in its era as well as under Scrolls. An empty region is not an era.
struct ListCycle {
    static constexpr int kMaxEra = 32;
    char era[kMaxEra][40]{};
    int nEra = 0;
    int Stops() const { return 4 + nEra; }
    int Kind(int stop) const {
        if (stop <= 0) return 0;
        if (stop == 1) return 1;
        if (stop >= 2 && stop < 2 + nEra) return 2;
        return stop == 2 + nEra ? 3 : 4;
    }
    const char* Label(int stop) const {
        const int k = Kind(stop);
        if (k == 0) return "All";
        if (k == 1) return "Multi";
        if (k == 2) return era[stop - 2];
        return k == 3 ? "Scrolls" : "Favorites";
    }
    void AddEra(const char* name) {
        if (!name || !name[0] || nEra >= kMaxEra) return;
        for (int i = 0; i < nEra; ++i)
            if (strcmp(era[i], name) == 0) return;
        snprintf(era[nEra], sizeof era[0], "%s", name);
        ++nEra;
    }
};

inline void BuildListCycle(const std::vector<Row>& rows, ListCycle& c) {
    c = ListCycle{};
    char key[40];
    for (const auto& r : rows) {
        EraKey(r.region, key, (int)sizeof key);
        c.AddEra(key);
    }
}

// The Waves label is this row's region, not "every Picture Scroll". E17 and E09 leave the
// Waves group when their region column is a boss era. They stay under Scrolls because route is E0.
inline bool WavesRow(const Row& r) {
    char key[40];
    EraKey(r.region, key, (int)sizeof key);
    return strcmp(key, "Waves") == 0;
}

// Multi is a catalogue row whose generated roster has more than one member. A count of 0 is not multi.
// isFavorite belongs to the list being drawn: Boss Matches and Training keep separate favorites (#79069).
inline bool RowInListStop(const Row& r, const ListCycle& c, int stop, bool isFavorite = false) {
    const int k = c.Kind(stop);
    if (k == 0) return true;
    if (k == 4) return isFavorite;
    if (k == 3) return r.route == kE;
    if (k == 2) {
        const int ei = stop - 2;
        if (ei < 0 || ei >= c.nEra || !r.region[0]) return false;
        char key[40];
        EraKey(r.region, key, (int)sizeof key);
        return key[0] && strcmp(key, c.era[ei]) == 0;
    }
    if (!r.missionId || r.route == kE) return false;
    return RosterMembers(r.id) > 1;
}

// Keep the cursor on anchorItem when that row is still listed. A hidden boss lands on the first boss still shown.
inline int KeepVisibleSel(const int* items, int n, int sel, int anchorItem) {
    if (n <= 0) return 0;
    for (int i = 0; i < n; ++i)
        if (items[i] == anchorItem) return i;
    if (anchorItem >= 0 && anchorItem < 0x10000) {
        for (int i = 0; i < n; ++i)
            if (items[i] >= 0 && items[i] < 0x10000) return i;
        return 0;
    }
    if (sel < -2) return -2;
    if (sel < 0) return sel;
    if (sel >= n) return n - 1;
    return sel;
}

inline int FightNowItemId(int catalogueIndex) { return 0x10000 + catalogueIndex; }

// Captured arena positions (Nioh3BossRematch_arenas.tsv, written at BossBattleMode start with [Rematch] ArenaCapture = 1):
// "mission<TAB>placements<TAB>x<TAB>y<TAB>z<TAB>when<TAB>src" (mission hex/dec, placements space separated, src before-load /
// fight-start). A capture overrides the catalogue arena point of every R / H / D row of that mission whose boss placement was
// created before that fight (last line wins). Lines without src (add-on 1.2.0) hold the position AFTER the boss intro moved the
// player (Yamagata: 334650, outside the trigger box) and are ignored.
inline int ApplyArenas(const std::string& text, std::vector<Row>& rows) {
    int n = 0;
    for (const auto& line : Lines(text)) {
        const auto f = SplitTab(line);
        if (f.size() < 7 || f[0] == "mission" || (f[6] != "before-load" && f[6] != "fight-start")) continue;
        const uint32_t mis = (uint32_t)strtoul(f[0].c_str(), nullptr, 0);
        const std::string pl = " " + f[1] + " ";
        for (auto& r : rows) {
            if (r.missionId != mis || !r.fightStart || !r.placement) continue;
            const std::string key = " " + std::to_string(r.placement) + " ";
            const std::string key2 = " " + std::to_string(r.placement) + "(";
            if (pl.find(key) == std::string::npos && pl.find(key2) == std::string::npos) continue;
            r.arena[0] = (float)atof(f[2].c_str());
            r.arena[1] = (float)atof(f[3].c_str()) + 50.0f;  // the player's feet at the trigger + 0.5 m
            r.arena[2] = (float)atof(f[4].c_str());
            r.arenaOk = true;
            r.atFight = false;
            snprintf(r.arenaConf, sizeof(r.arenaConf), "captured");
            snprintf(r.arenaSrc, sizeof(r.arenaSrc), "captured %s %s", f[5].c_str(), f[6].c_str());
            ++n;
        }
    }
    return n;
}

// ally #8093 (Otoroshi 0x2EE9): a row with neither a catalogue arena point nor a capture took 'no arena position yet' even though
// Nioh3Training_arenas.tsv (tools/gen_arena_table.py, the table the in-place swap uses) has it. Fallback, applied after the captures:
// "mission<TAB>start<TAB>world<TAB>char<TAB>placement<TAB>x<TAB>y<TAB>z<TAB>yaw<TAB>bx<TAB>by<TAB>bz<TAB>kind<TAB>id<TAB>name[...]"
// (kind arena = inside the trigger, start = the Fight-now start point). Only fills rows still without an arena point.
inline int ApplyTrainingArenas(const std::string& text, std::vector<Row>& rows) {
    int n = 0;
    for (const auto& line : Lines(text)) {
        if (line.empty() || line[0] == '#') continue;
        const auto f = SplitTab(line);
        if (f.size() < 15) continue;
        const uint32_t mis = (uint32_t)strtoul(f[0].c_str(), nullptr, 0);
        if (!mis) continue;
        for (auto& r : rows) {
            if (r.missionId != mis || !r.fightStart || r.arenaOk || r.atFight) continue;
            for (int k = 0; k < 4; ++k) r.arena[k] = (float)atof(f[(size_t)(5 + k)].c_str());
            r.arenaOk = true;
            snprintf(r.arenaConf, sizeof(r.arenaConf), "training");
            snprintf(r.arenaSrc, sizeof(r.arenaSrc), "Nioh3Training_arenas.tsv %s", f[12].c_str());
            ++n;
        }
    }
    return n;
}

// verify_all.ps1 results: "id<TAB>verified<TAB>..." (header line "id\tverified..."); returns rows marked
inline int ApplyVerified(const std::string& text, std::vector<Row>& rows) {
    int n = 0;
    for (const auto& line : Lines(text)) {
        const auto f = SplitTab(line);
        if (f.size() < 2 || f[0] == "id") continue;
        for (auto& r : rows)
            if (f[0] == r.id) {
                r.verifiedRun = f[1] == "1";
                n += r.verifiedRun;
            }
    }
    return n;
}

// ---- Fight now: is a position inside the row's trigger box? (box +0x28 = full size, rotated by its yaw about y; 1 m margin) ----
inline bool InTriggerBox(const Row& r, const float p[3]) {
    if (!r.boxOk) return false;
    const float dx = p[0] - r.box[0], dz = p[2] - r.box[2], a = r.box[3] * 0.01745329252f;
    const float lx = dx * cosf(a) - dz * sinf(a), lz = dx * sinf(a) + dz * cosf(a);
    return fabsf(lx) <= r.box[4] / 2 + 100 && fabsf(lz) <= r.box[6] / 2 + 100 && fabsf(p[1] - r.box[1]) <= r.box[5] / 2 + 300;
}

// ---- Fight now: did MissionRequestById(mission, slot, startPoint, params) queue what we asked? (0x58-byte queue slot) ----
// +4 u32 mission id, +0xC u32 start point (the request's third argument; 0 would have become the mission default), +0x14 u64 mode 2
inline bool FightSlotOk(const uint8_t* slot, uint16_t mission, uint32_t start) {
    uint32_t m = 0, sp = 0;
    uint64_t mode = 0;
    memcpy(&m, slot + 4, 4);
    memcpy(&sp, slot + 0xC, 4);
    memcpy(&mode, slot + 0x14, 8);
    return m == mission && sp == start && start != 0 && mode == 2;
}

// ---- Fight now SPAWN (1.4.0): the start point's warp-point record (0xA4 B: +8 flags, +40h spawn x y z yaw) gets the arena point ----
// Only a record that still spawns where the catalogue says (warp_x..z, the shipped +40h, within 1 m) is patched; bit 4 of the flags
// (the arrival-camera preset +7Ch/+80h, aimed for the start point) is cleared while it spawns in the arena.
inline bool SpawnRecordMatches(const Row& r, const float cur[4]) {
    const float dx = cur[0] - r.warp[0], dy = cur[1] - r.warp[1], dz = cur[2] - r.warp[2];
    return dx * dx + dy * dy + dz * dz <= 100.0f * 100.0f;
}
inline uint32_t SpawnFlags(uint32_t flags) { return flags & ~4u; }
// a player within 5 m (horizontal) of the start point's spawn = the override did not take (the warp fallback runs)
inline bool AtStartPoint(const Row& r, const float p[3]) {
    const float dx = p[0] - r.warp[0], dz = p[2] - r.warp[2];
    return dx * dx + dz * dz <= 500.0f * 500.0f;
}

inline uint32_t Crc32(const void* p, size_t n, uint32_t c = 0) {
    c = ~c;
    const uint8_t* b = (const uint8_t*)p;
    for (size_t i = 0; i < n; ++i) {
        c ^= b[i];
        for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
    }
    return ~c;
}

// ---- E1: redirect every entry of the Emaki byte->record map to one record (8-byte entries {u8 byte, pad[3], u32 index}) ----
// The carrier's key resolves to SOME byte (a key-seeded lottery, 102BB60 / 102B840); with every valid entry pointing at the target
// index, whatever byte it lands on resolves to the target record, for the request and for every EmakiRecordGet during the mission.
struct MapPatch {
    static constexpr int kMax = 256;
    uint8_t* entries = nullptr;  // live map vector
    int n = 0;
    uint8_t snap[kMax * 8] = {}, written[kMax * 8] = {};
    uint32_t crcWritten = 0;
    uint32_t target = 0;
    bool active = false, conflict = false;
};
// writer: memcpy-like callback (so the live build can use a guarded write); returns false on failure
using WriteFn = bool (*)(void* dst, const void* src, size_t n);
using ReadFn = bool (*)(void* dst, const void* src, size_t n);
inline bool MapReadBytes(void* dst, const void* src, size_t n, ReadFn rd) {
    if (rd) return rd(dst, src, n);
    memcpy(dst, src, n); return true;  // pure harness callers own their memory
}
inline bool MapRedirect(MapPatch& mp, uint8_t* entries, int n, uint8_t emptyKey, uint32_t target, uint32_t count, WriteFn wr, ReadFn rd = nullptr) {
    if (mp.active || !entries || n <= 0 || n > MapPatch::kMax || target >= count) return false;
    if (!MapReadBytes(mp.snap, entries, (size_t)n * 8, rd)) return false;
    std::vector<uint8_t> nw(mp.snap, mp.snap + (size_t)n * 8);
    int firstValid = -1;
    for (int i = 0; i < n; ++i) {
        if (firstValid < 0 && nw[(size_t)i * 8] != emptyKey) firstValid = i;
        memcpy(&nw[(size_t)i * 8 + 4], &target, 4);
    }
    if (firstValid < 0) return false;
    // The game turns a key into a byte by picking a record index from the key, then scanning this map for that index (102B840). When
    // nothing matches it returns the byte of slot 0. With every index on the target, that is every key except the one that happens to
    // pick the target. Slot 0 is empty in the shipped map, so those keys got the empty byte: the lookup missed, the request lost its
    // region and the load never ended. Slot 0 now holds a real key. Empty slots carry the target too, so the scan cannot stop on one.
    if (nw[0] == emptyKey) nw[0] = nw[(size_t)firstValid * 8];
    mp.entries = entries;
    mp.n = n;
    mp.target = target;
    mp.crcWritten = Crc32(nw.data(), nw.size());
    memcpy(mp.written, nw.data(), nw.size());
    mp.active = true;  // recovery ownership precedes a possibly partial write
    return wr(entries, nw.data(), (size_t)n * 8);
}
// 0 restored/read back, -1 inactive, -2 write/readback failed, -3 foreign data, -4 unreadable.
// Foreign identity/bytes latch a conflict; never overwrite another owner's data.
inline int MapRestore(MapPatch& mp, WriteFn wr, ReadFn rd = nullptr) {
    if (!mp.active) return -1;
    if (mp.conflict) return -3;
    uint8_t current[MapPatch::kMax * 8] = {};
    const size_t size = (size_t)mp.n * 8;
    if (!MapReadBytes(current, mp.entries, size, rd)) return -4;
    if (memcmp(current, mp.snap, size)) {
        if (memcmp(current, mp.written, size)) { mp.conflict = true; return -3; }
        if (!wr(mp.entries, mp.snap, size)) return -2;
    }
    if (!MapReadBytes(current, mp.entries, size, rd) || memcmp(current, mp.snap, size)) return -2;
    mp.active = false;
    return 0;
}

// ---- E lifecycle (one call per pump tick) ----
// Idle -> Armed (request made) -> InMission (charMgr +0x2C18 == 1 seen) -> exit (+0x2C18 == 0 for 5 s, not loading) -> restore.
// Armed with no InMission within kArmTimeoutMs (kArmLoadTimeoutMs while the load flag is on) -> restore + refuse.
// A missed record lookup leaves the player on the mission's default start, and that load never ends.
// A retry inside the scroll keeps +0x2C18 = 1 (patch stays live). The in-mission exit still waits out a reload.
enum EState : int { kEIdle = 0, kEArmed = 1, kEIn = 2 };
struct ETrack {
    EState st = kEIdle;
    uint64_t armedAt = 0, zeroSince = 0;
    static constexpr uint64_t kArmTimeoutMs = 30000, kArmLoadTimeoutMs = 180000, kExitMs = 5000;
};
// returns: 0 nothing, 1 entered the mission, 2 exited (restore now), 3 armed timeout (restore now, refuse)
inline int ETick(ETrack& t, uint64_t now, bool isEmaki, bool loading) {
    switch (t.st) {
        case kEIdle: return 0;
        case kEArmed:
            if (isEmaki) {
                t.st = kEIn;
                t.zeroSince = 0;
                return 1;
            }
            // A slow load is not a stuck one: a return queued now would run right after it and throw the player out.
            if (now - t.armedAt > (loading ? ETrack::kArmLoadTimeoutMs : ETrack::kArmTimeoutMs)) {
                t.st = kEIdle;
                return 3;
            }
            return 0;
        case kEIn:
            if (isEmaki || loading) {
                t.zeroSince = 0;
                return 0;
            }
            if (!t.zeroSince) t.zeroSince = now ? now : 1;
            else if (now - t.zeroSince > ETrack::kExitMs) {
                t.st = kEIdle;
                return 2;
            }
            return 0;
    }
    return 0;
}

// One offset list for every Waves row (#77650). Step 0 is the live record warp. The next four are the
// 600 cm steps from Temp/s77376 and Temp/s77377 (those tries used y = warp y + 100). 1500 is the same
// four directions when 600 never reaches wave state 3. No per-row point.
struct WaveNudge { float dx, dz, dy; };
inline constexpr int kWaveNudges = 13;
inline WaveNudge WaveNudgeAt(int step) {
    static const WaveNudge k[kWaveNudges] = {
        {0.f, 0.f, 0.f},
        {600.f, 0.f, 100.f}, {-600.f, 0.f, 100.f}, {0.f, 600.f, 100.f}, {0.f, -600.f, 100.f},
        {1500.f, 0.f, 100.f}, {-1500.f, 0.f, 100.f}, {0.f, 1500.f, 100.f}, {0.f, -1500.f, 100.f},
        // E07 woke only at z-1200 (pc2 #77817), between the 600 and 1500 steps; main PC + pc2 walked past it.
        {0.f, -1200.f, 100.f}, {0.f, 1200.f, 100.f}, {1200.f, 0.f, 100.f}, {-1200.f, 0.f, 100.f},
    };
    if (step < 0 || step >= kWaveNudges) return {0.f, 0.f, 0.f};
    return k[step];
}
// 0 wait, 1 take the next offset, 2 stop because wave state is 3, 3 the list is finished
inline int WaveNudgeDecide(int step, int state, bool dwellDone, int count = kWaveNudges) {
    if (state == 3) return 2;
    if (step < 0) return 1;
    if (!dwellDone) return 0;
    if (step + 1 >= count) return 3;
    return 1;
}
// #78985: the offset that woke each record in the test runs, so the first warp under the loading screen lands on it. The numbers live in
// the catalogue column wake_step (tools/rematch_catalog.py WAKE_STEP; sources pc2 #78351 E00-E09, Ally #78283 E10-E19, E07 Ally #78389).
inline int WaveWakeStep(const Row& r) {
    return r.route == kE && r.wakeStep > 0 && r.wakeStep < kWaveNudges ? r.wakeStep : 0;
}
// The known offset first, then the whole old walk: a first warp that came too early gets its offset again in the walk.
inline int WaveWalkCount(int wakeStep) { return wakeStep > 0 ? kWaveNudges + 1 : kWaveNudges; }
inline int WaveWalkOffset(int wakeStep, int tryNo) {
    if (wakeStep <= 0) return tryNo;
    return tryNo == 0 ? wakeStep : tryNo - 1;
}
inline constexpr uint64_t kWaveDwellCoverMs = 4000, kWaveDwellMs = 9000;

// #79069 Training at a scroll arena: the same spot the waves wake at, twice, because a first warp can land before the mission
// takes it. Then the player keeps control. The wave request itself is skipped by VetoWaveRequest, so these warps wake nothing.
inline constexpr int kWaveTrainWarps = 2;
inline int WaveTryOffset(bool training, int wakeStep, int tryNo) { return training ? wakeStep : WaveWalkOffset(wakeStep, tryNo); }
inline int WaveTryCount(bool training, int wakeStep) { return training ? kWaveTrainWarps : WaveWalkCount(wakeStep); }

// Wave state 1 also means "waiting for the next wave" (wave 2+, members still listed). The run is over only at wave 0 with no members (#75963 log, BR fd4eca18).
inline bool WaveRunOver(int state, unsigned waveNo, unsigned members) { return state == 1 && waveNo == 0 && members == 0; }

// ---- stale fight flag (user 20261018) ----
// A save made inside an arena reloads with no mission (0) and the area script runs BossBattleMode with no boss; BossBattleModeEnd never
// comes, so fight::g_active blocked every Boss Match / Practice start ("in a boss fight") for the rest of the session.
// Our flag is the stale one when: it went up while loading or within kStaleNearLoadMs after a load ended, no mission is running, no rematch
// flow is active and Nioh3Training owns no session / summon / swap / return.
constexpr uint64_t kStaleNearLoadMs = 5000;
inline bool FightRiseNearLoad(bool loading, uint64_t now, uint64_t loadEndAt) {
    return loading || (loadEndAt && now >= loadEndAt && now - loadEndAt < kStaleNearLoadMs);
}
inline bool StaleFight(bool active, bool riseNearLoad, bool msOk, uint32_t mission, bool idle, bool trainingOwns) {
    return active && riseNearLoad && msOk && mission == 0 && idle && !trainingOwns;
}
// A world load ends any native boss mode (the area script runs BossBattleMode again after the load if it wants one): the flag is
// dropped when a load begins, unless a rematch flow or a Nioh3Training session owns that load (they handle their own fight state).
inline bool ClearFightOnLoad(bool active, bool idle, bool trainingOwns) { return active && idle && !trainingOwns; }

// Resume hold (#75648). MissionStartFromSave reloads [MissionMgr+8]+28h (0xCC96 -> EmakiMissionRequest,
// any other nonzero id -> MissionRequestById) and the start at +14h. A mission load autosaves those fields
// while the trip is current. MissionRequestReturn uses +1Ch, so Return can walk home in memory while the
// file still says the trip. The trip id is the one this request just passed, not a mission list.
// ResumeBlocksWrite is the disk decision. ResumeHold adds the game-thread result:
// 0 = not holding, 1 = keep holding, 2 = the trip is over, so the game's own autosave may run.
inline bool ResumeBlocksWrite(bool armed, uint32_t trip, uint32_t source, uint32_t live, bool liveOk, bool session) {
    if (!armed) return false;
    if (session) return true;
    if (!liveOk) return true;
    return trip != source && trip != 0 && live == trip;
}
inline int ResumeHold(bool armed, uint32_t trip, uint32_t source, uint32_t live, bool liveOk,
                      bool loading, bool session, bool entered) {
    if (!armed) return 0;
    if (ResumeBlocksWrite(armed, trip, source, live, liveOk, session) || loading) return 1;
    if (!entered) return 0;
    return 2;
}

// #79069 favorites file, one row a line: "B <row id>" Boss Matches, "T <row id>" Training. Row ids, never list positions: the
// catalogue can reorder. A line this build cannot read is dropped, so a damaged file loses that line and nothing else.
inline bool FavIdOk(const char* id) {
    if (!id || !id[0]) return false;
    int n = 0;
    for (; id[n]; ++n) {
        const char c = id[n];
        if (n >= 31 || !((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == '-')) return false;
    }
    return true;
}
inline std::string FavKey(char list, const char* id) { return std::string(1, list) + " " + id; }
inline void FavParse(const std::string& text, std::set<std::string>& out) {
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(at, end - at);
        at = end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.size() > 2 && (line[0] == 'B' || line[0] == 'T') && line[1] == ' ' && FavIdOk(line.c_str() + 2)) out.insert(line);
    }
}
inline std::string FavText(const std::set<std::string>& favs) {
    std::string text;
    for (const auto& f : favs) { text += f; text += "\r\n"; }
    return text;
}

// Empty Picture Scroll bag: the shrine return still asks to reopen the list. Close that request
// only when the list's own item query is 0. A nonzero value keeps the game's list.
inline bool CloseScrollListWhenEmpty(uint32_t count, uint8_t reopenFlag) {
    return reopenFlag != 0 && count == 0;
}

// #78735 + #78985 scroll row loading screen: 1 = the trip ended, 2 = waves started (first enemy about 0.3 s later, E07 log),
// 3 = the walk tried every offset, 4 = safety timeout. 0 = keep it up. The whole walk is under 60 s after landing.
inline constexpr uint64_t kScrollCoverSafetyMs = 75000;
inline int ScrollCoverDone(bool tripLive, bool inMission, bool waves, bool walkOver, uint64_t inMs) {
    if (!tripLive) return 1;
    if (!inMission) return 0;
    if (waves) return 2;
    if (walkOver) return 3;
    return inMs >= kScrollCoverSafetyMs ? 4 : 0;
}

// #79955 title exit: Return to Title leaves the rematch state set, and the game's per-frame pump may not run at the title, so the Tick-side
// check never saw it. The UI worker polls this: state not idle + no player + no load + mission 0 for kTitleIdleMs in a row = at the title.
// A load (loading flag, or a mission id, or a player) restarts the count. Returns true once, when it is time to go idle.
inline constexpr uint64_t kTitleIdleMs = 20000;
struct TitleWatch {
    uint64_t since = 0;
    bool Step(bool stateActive, bool player, bool loading, uint32_t mission, uint64_t t) {
        if (!stateActive || player || loading || mission != 0) { since = 0; return false; }
        if (!since) { since = t; return false; }
        if (t - since < kTitleIdleMs) return false;
        since = 0;
        return true;
    }
    // Title test without the mission id: it keeps its last value at the title. Unreadable game state = may be loading, never idle.
    bool StepSnap(bool stateActive, bool player, bool gmReadable, bool loading, uint64_t t) {
        return Step(stateActive, player, loading || !gmReadable, 0, t);
    }
};

}  // namespace rcore
