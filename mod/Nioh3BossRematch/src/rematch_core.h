// rematch_core.h - the pure parts of the rematch (no game calls), shared by rematch.cpp and tests/rematch_harness.cpp:
// catalogue + verified-results parsing, the E1 Emaki map redirect (snapshot / CRC / restore), the E lifecycle tracker and the
// "Fight now" (E1 straight to the boss) request check.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace rcore {

enum Route : uint8_t { kR = 0, kH = 1, kD = 2, kE = 3, kX = 4 };
inline const char* RouteName(Route r) {
    static const char* const n[] = {"replay", "hidden duel", "direct", "picture scroll", "no route"};
    return n[r];
}

struct Row {
    char id[24] = "", name[64] = "", mission[64] = "", region[24] = "", reason[200] = "";
    Route route = kX;
    uint16_t missionId = 0;
    int emakiIndex = -1;       // E rows: Emaki record index (0..19)
    uint32_t placement = 0;    // expected boss placement (BossLog Create id) or 0
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
              cAbox = ci("arena_box"), cAsrc = ci("arena_src"), cAconf = ci("arena_conf"), cAgeo = ci("arena_boxgeo"), cGid = ci("gate_id"), cGk = ci("gate_kind"), cBp = ci("boss_pos");
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
        r.emakiIndex = col(cEi).empty() ? -1 : atoi(col(cEi).c_str());
        r.placement = (uint32_t)strtoul(col(cPl).c_str(), nullptr, 0);
        r.hidden = col(cHid) == "1";
        r.verifiedCat = col(cVer) == "1";
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
        if (r.route == kE && (r.emakiIndex < 0 || r.emakiIndex > 255)) continue;
        rows.push_back(r);
    }
    return (int)rows.size();
}

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
    uint8_t snap[kMax * 8] = {};
    uint32_t crcWritten = 0;
    uint32_t target = 0;
    bool active = false;
};
// writer: memcpy-like callback (so the live build can use a guarded write); returns false on failure
using WriteFn = bool (*)(void* dst, const void* src, size_t n);
inline bool MapRedirect(MapPatch& mp, uint8_t* entries, int n, uint8_t emptyKey, uint32_t target, uint32_t count, WriteFn wr) {
    if (mp.active || !entries || n <= 0 || n > MapPatch::kMax || target >= count) return false;
    memcpy(mp.snap, entries, (size_t)n * 8);
    std::vector<uint8_t> nw(mp.snap, mp.snap + (size_t)n * 8);
    int valid = 0;
    for (int i = 0; i < n; ++i) {
        if (nw[(size_t)i * 8] == emptyKey) continue;
        memcpy(&nw[(size_t)i * 8 + 4], &target, 4);
        ++valid;
    }
    if (!valid) return false;
    if (!wr(entries, nw.data(), (size_t)n * 8)) return false;
    mp.entries = entries;
    mp.n = n;
    mp.target = target;
    mp.crcWritten = Crc32(nw.data(), nw.size());
    mp.active = true;
    return true;
}
// 0 restored, 1 restored but the bytes had changed since our write (someone else wrote: logged), -1 nothing active, -2 write failed
inline int MapRestore(MapPatch& mp, WriteFn wr) {
    if (!mp.active) return -1;
    const uint32_t cur = Crc32(mp.entries, (size_t)mp.n * 8);
    const bool ok = wr(mp.entries, mp.snap, (size_t)mp.n * 8);
    mp.active = false;
    if (!ok) return -2;
    return cur == mp.crcWritten ? 0 : 1;
}

// ---- E lifecycle (one call per pump tick) ----
// Idle -> Armed (request made) -> InMission (charMgr +0x2C18 == 1 seen) -> exit (+0x2C18 == 0 for 5 s, not loading) -> restore.
// Armed with no InMission within kArmTimeoutMs -> restore + refuse. A retry inside the scroll keeps +0x2C18 = 1 (patch stays live).
enum EState : int { kEIdle = 0, kEArmed = 1, kEIn = 2 };
struct ETrack {
    EState st = kEIdle;
    uint64_t armedAt = 0, zeroSince = 0;
    static constexpr uint64_t kArmTimeoutMs = 30000, kExitMs = 5000;
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
            if (now - t.armedAt > ETrack::kArmTimeoutMs && !loading) {
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

}  // namespace rcore
