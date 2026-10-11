// tour_core.h - pure logic of the Tour mode (tour.cpp), header-only so tests\tour_harness.cpp can check it offline:
// TourList parsing, the resume/progress file, farthest-point sampling of warp points, frame-time statistics.
// No Windows, no game memory here.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace tour {

// ---- TourList -------------------------------------------------------------------------------------------------------
// One stage per line: "<mission key> [# comment]". "<key>@x,y,z" adds an extra point (world position) to that stage; a key
// that appears only with @ lines still becomes a stage (in first-seen order). Keys are the u16 mission keys at master record
// +0x158 (notes\missions.csv column 2). Blank lines and '#' comments are ignored; bad lines are reported in `bad`.
struct Pos {
    float x = 0, y = 0, z = 0;
};
struct Stage {
    uint32_t key = 0;
    std::string label;          // comment text after '#', trimmed (may be empty)
    std::vector<Pos> extra;     // @x,y,z points from the list
};

inline std::string Trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
    return s.substr(a, b - a);
}

inline std::vector<Stage> ParseTourList(const std::string& text, int* bad = nullptr) {
    std::vector<Stage> out;
    int nbad = 0;
    size_t p = 0;
    while (p <= text.size()) {
        size_t e = text.find('\n', p);
        if (e == std::string::npos) e = text.size();
        std::string line = text.substr(p, e - p);
        p = e + 1;
        std::string label;
        const size_t h = line.find('#');
        if (h != std::string::npos) {
            label = Trim(line.substr(h + 1));
            line = line.substr(0, h);
        }
        line = Trim(line);
        if (line.empty()) {
            if (e >= text.size()) break;
            continue;
        }
        const size_t at = line.find('@');
        const std::string ks = Trim(line.substr(0, at));
        char* end = nullptr;
        const unsigned long k = strtoul(ks.c_str(), &end, 0);
        if (ks.empty() || !end || *end || k == 0 || k > 0xFFFF) {
            ++nbad;
            if (e >= text.size()) break;
            continue;
        }
        Pos pos;
        bool hasPos = false;
        if (at != std::string::npos) {
            float v[3] = {};
            const char* q = line.c_str() + at + 1;
            int got = 0;
            for (; got < 3; ++got) {
                while (*q == ' ' || *q == '\t') ++q;
                char* qe = nullptr;
                v[got] = strtof(q, &qe);
                if (!qe || qe == q) break;
                q = qe;
                while (*q == ' ' || *q == '\t') ++q;
                if (got < 2) {
                    if (*q != ',') break;
                    ++q;
                }
            }
            while (*q == ' ' || *q == '\t') ++q;
            pos.x = v[0], pos.y = v[1], pos.z = v[2];
            if (got != 3 || *q || !std::isfinite(pos.x) || !std::isfinite(pos.y) || !std::isfinite(pos.z)) {
                ++nbad;
                if (e >= text.size()) break;
                continue;
            }
            hasPos = true;
        }
        // a plain key line always adds a stage (a key may be listed twice on purpose); an @ line joins the latest
        // stage with that key, or starts one
        Stage* st = nullptr;
        if (hasPos)
            for (Stage& s : out)
                if (s.key == (uint32_t)k) st = &s;
        if (!st) {
            out.push_back(Stage{(uint32_t)k, hasPos ? std::string() : label, {}});
            st = &out.back();
        }
        if (hasPos) st->extra.push_back(pos);
        if (e >= text.size()) break;
    }
    if (bad) *bad = nbad;
    return out;
}

// ---- progress file (resume after a crash) ---------------------------------------------------------------------------
// Appended + flushed by the game thread:
//   L <pass> <stage> <key>          load requested
//   A <pass> <stage> <key> <what>   arrival: ok / stall / refused / locked / missing / crash (written on resume)
//   B <pass> <stage> <pt>           dwell begins at point <pt>
//   D <pass> <stage> <pt>           dwell done
//   S <pass> <stage>                stage done (all points)
//   C <pass> <stage> <pt>           written on resume for a B without D (the previous run died there)
//   E                               tour finished
struct Progress {
    std::set<std::tuple<int, int, int>> donePts;   // D or C
    std::set<std::pair<int, int>> doneStages;      // S, or A with a non-ok result, or a load that crashed
    std::vector<std::tuple<int, int, int>> crashPts;  // B without D (new crashes found when parsing)
    std::vector<std::pair<int, int>> crashLoads;      // L without A (new)
    std::vector<std::tuple<int, int, int>> oldCrashPts;  // C lines from earlier resumes
    bool finished = false;
    int lines = 0;
};

inline Progress ParseProgress(const std::string& text) {
    Progress pr;
    int openPass = -1, openStage = -1, openPt = -1;   // last B without D
    int loadPass = -1, loadStage = -1;                // last L without A
    size_t p = 0;
    while (p < text.size()) {
        size_t e = text.find('\n', p);
        if (e == std::string::npos) e = text.size();
        const std::string line = Trim(text.substr(p, e - p));
        p = e + 1;
        if (line.empty()) continue;
        ++pr.lines;
        // "<c> <int> <int> <int> <word>" (later fields optional); n = fields read, like sscanf
        const char c = line[0];
        int a = -1, b = -1, d = -1;
        char what[32] = {};
        int n = 1;
        {
            const char* q = line.c_str() + 1;
            int* dst[3] = {&a, &b, &d};
            for (int k = 0; k < 3; ++k) {
                char* qe = nullptr;
                const long v = strtol(q, &qe, 10);
                if (!qe || qe == q) break;
                *dst[k] = (int)v;
                q = qe;
                ++n;
            }
            while (*q == ' ' || *q == '\t') ++q;
            if (n == 4 && *q) {
                size_t w = 0;
                while (q[w] && q[w] != ' ' && q[w] != '\t' && w < sizeof(what) - 1) what[w] = q[w], ++w;
                ++n;
            }
        }
        switch (c) {
            case 'L':
                if (n >= 3) loadPass = a, loadStage = b;
                break;
            case 'A':
                if (n >= 5) {
                    if (a == loadPass && b == loadStage) loadPass = loadStage = -1;
                    if (strcmp(what, "ok") != 0) pr.doneStages.insert({a, b});
                }
                break;
            case 'B':
                if (n >= 4) openPass = a, openStage = b, openPt = d;
                break;
            case 'D':
                if (n >= 4) {
                    pr.donePts.insert({a, b, d});
                    if (a == openPass && b == openStage && d == openPt) openPass = openStage = openPt = -1;
                }
                break;
            case 'C':
                if (n >= 4) {
                    pr.donePts.insert({a, b, d});
                    pr.oldCrashPts.push_back({a, b, d});
                    if (a == openPass && b == openStage && d == openPt) openPass = openStage = openPt = -1;
                }
                break;
            case 'S':
                if (n >= 3) pr.doneStages.insert({a, b});
                break;
            case 'E': pr.finished = true; break;
            default: break;
        }
    }
    if (openPass >= 0 && !pr.donePts.count({openPass, openStage, openPt})) {
        pr.crashPts.push_back({openPass, openStage, openPt});
        pr.donePts.insert({openPass, openStage, openPt});
        loadPass = loadStage = -1;  // the load itself completed (a B followed it)
    }
    if (loadPass >= 0 && !pr.doneStages.count({loadPass, loadStage})) {
        pr.crashLoads.push_back({loadPass, loadStage});
        pr.doneStages.insert({loadPass, loadStage});
    }
    return pr;
}

// ---- farthest-point sampling ------------------------------------------------------------------------------------------
// Picks up to k candidates: each next pick is the candidate farthest (XZ+Y euclidean) from everything chosen so far,
// starting from `start` (the arrival spot). Candidates closer than minDist to an already chosen spot are never picked.
// Deterministic (ties -> lowest index). Returns candidate indices in pick order.
inline std::vector<int> FarthestPoints(const Pos& start, const std::vector<Pos>& cand, int k, float minDist) {
    std::vector<int> pick;
    if (k <= 0 || cand.empty()) return pick;
    std::vector<double> best(cand.size());
    auto d2 = [](const Pos& a, const Pos& b) {
        const double dx = (double)a.x - b.x, dy = (double)a.y - b.y, dz = (double)a.z - b.z;
        return dx * dx + dy * dy + dz * dz;
    };
    for (size_t i = 0; i < cand.size(); ++i) best[i] = d2(start, cand[i]);
    const double min2 = (double)minDist * minDist;
    while ((int)pick.size() < k) {
        int bi = -1;
        double bv = -1;
        for (size_t i = 0; i < cand.size(); ++i)
            if (best[i] > bv && best[i] >= min2) bv = best[i], bi = (int)i;
        if (bi < 0) break;
        pick.push_back(bi);
        for (size_t i = 0; i < cand.size(); ++i) best[i] = std::min(best[i], d2(cand[bi], cand[i]));
    }
    return pick;
}

// a usable world position (not zero, finite, inside +-1e7)
inline bool PosOk(const Pos& p) {
    if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) return false;
    if (p.x == 0 && p.y == 0 && p.z == 0) return false;
    return std::fabs(p.x) < 1e7f && std::fabs(p.y) < 1e7f && std::fabs(p.z) < 1e7f;
}

// ---- frame statistics -------------------------------------------------------------------------------------------------
struct FrameStats {
    int frames = 0;
    double avgFps = 0, p50 = 0, p99 = 0, maxMs = 0, avgMs = 0;
    int over33 = 0;  // frames > 33.3 ms (hitches)
};
// ms = frame times in milliseconds (unsorted; the vector is reordered)
inline FrameStats ComputeFrameStats(std::vector<double>& ms) {
    FrameStats s;
    s.frames = (int)ms.size();
    if (ms.empty()) return s;
    double sum = 0;
    for (double v : ms) {
        sum += v;
        if (v > 33.334) ++s.over33;
    }
    s.avgMs = sum / ms.size();
    s.avgFps = sum > 0 ? 1000.0 * ms.size() / sum : 0;
    std::sort(ms.begin(), ms.end());
    auto pct = [&](double q) {  // nearest-rank
        size_t i = (size_t)std::ceil(q * ms.size());
        if (i < 1) i = 1;
        if (i > ms.size()) i = ms.size();
        return ms[i - 1];
    };
    s.p50 = pct(0.50);
    s.p99 = pct(0.99);
    s.maxMs = ms.back();
    return s;
}

// ---- save-root match (IAT guard) --------------------------------------------------------------------------------------
// true when the path is inside ...\KoeiTecmo\NIOH3\Savedata (also Savedata_backup); ASCII case-insensitive, either slash.
inline bool IsSavePath(const wchar_t* p) {
    if (!p) return false;
    static const wchar_t kNeedle[] = L"\\koeitecmo\\nioh3\\savedata";
    const size_t n = sizeof(kNeedle) / sizeof(wchar_t) - 1;
    for (const wchar_t* s = p; *s; ++s) {
        size_t i = 0;
        for (; i < n && s[i]; ++i) {
            wchar_t c = s[i];
            if (c == L'/') c = L'\\';
            if (c >= L'A' && c <= L'Z') c = (wchar_t)(c - L'A' + L'a');
            if (c != kNeedle[i]) break;
        }
        if (i == n) return true;
    }
    return false;
}

}  // namespace tour
