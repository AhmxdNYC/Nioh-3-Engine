// rematch_core.h - the pure parts of the rematch (no game calls), shared by rematch.cpp and tests/rematch_harness.cpp:
// catalogue + verified-results parsing, the E1 Emaki map redirect (snapshot / CRC / restore), the E lifecycle tracker and the
// "Fight now" (E1 straight to the boss) request check.
#pragma once
#include <cstdint>
#include <cstdlib>
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
              cWyaw = ci("warp_yaw");
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
        }
        if (r.route == kE && (r.emakiIndex < 0 || r.emakiIndex > 255)) continue;
        rows.push_back(r);
    }
    return (int)rows.size();
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
