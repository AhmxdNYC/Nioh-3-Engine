// human #135146 / TASK #135148: a soul core's hit count from the game's own action data, the way Training gets boss hit counts
// (Nioh3Training auto_moves.h AmWindows). Pure logic, no game access (glue: bosslog.cpp SealDataHits; test: tests/seal_data_hits_test.cpp).
// The seal (the yokai of a soul-core form) plays a short action chain from its own action file (live log 11-25: 80D5F C52 -> C58 -> C57,
// 5216E C53 -> C54 -> C55 -> C56 -> C57). Each action names its own hitbox rows (192 bytes: u16 +0x08 start frame, +0x0A end frame or FFFF,
// +0x0C length, +0x10 power, +0x12 element, byte +0xA5 hit group). The live counter ([SealHits*]) counts BURSTS: the attack collider pool going
// from empty to non-empty. So here a burst = damaging timed windows of one action merged when they overlap or touch, whatever their group.
// The chain follows the finish link (row +0xE0) from the action the seal is bound in, not into a self loop, max 8 actions.
// The count is only CERTAIN when nothing in the chain can hit outside its own timed rows: no damaging untimed row (fired by a spawned object)
// and no spawn event (+0x40 events, 20000+N) that is not ambient (a spawn at frame 0 present in >= 80 % of the file's actions is a standing
// body / weapon object, the Training rule). Helper characters spawned through 65A8A8 are not in the file: the live burst counter stays the
// fallback, and the bound log line names both numbers so a live run can check them.
#pragma once
#include <stdint.h>
#include <string.h>
#include <stdio.h>

enum { SDH_ROW = 0x150, SDH_HB = 192, SDH_MAXROWS = 64, SDH_MAXCHAIN = 8, SDH_EVENTS = 16 };

struct SdhAction {
    uint16_t id = 0xFFFF, next = 0xFFFF;
    uint16_t ev[SDH_EVENTS] = {}, evFrame[SDH_EVENTS] = {};
    int nhb = 0;
    const uint8_t* hb = nullptr;  // nhb x 192-byte hitbox rows (caller's buffer)
};

struct SdhResult {
    int bursts = 0;       // merged damaging windows over the chain
    int windows = 0;      // windows before the cross-group merge (AmWindows-like count, for the log)
    int untimed = 0;      // damaging rows with no start frame (fired by something else)
    int spawns = 0;       // non-ambient spawn events in the chain
    int chain = 0;        // actions walked
    uint16_t ids[SDH_MAXCHAIN] = {};
    int perAct[SDH_MAXCHAIN] = {};
    uint16_t lastEnd = 0; // end frame of the last burst, in its own action's frames
    bool certain() const { return bursts > 0 && !untimed && !spawns; }
};

static inline uint16_t SdhU16(const uint8_t* p, int o) { uint16_t v; memcpy(&v, p + o, 2); return v; }

// Bursts of one action: damaging timed rows, sorted by start, merged when start <= previous end + 1.
static inline int SdhActionBursts(const SdhAction& a, int* untimed, int* windows, uint16_t* lastEnd) {
    uint16_t st[SDH_MAXROWS], en[SDH_MAXROWS]; int n = 0;
    for (int i = 0; i < a.nhb && i < SDH_MAXROWS; ++i) {
        const uint8_t* r = a.hb + (size_t)i * SDH_HB;
        const uint16_t s = SdhU16(r, 8), ef = SdhU16(r, 0xA), ln = SdhU16(r, 0xC), pw = SdhU16(r, 0x10), el = SdhU16(r, 0x12);
        if (!pw && !el) continue;
        if (s == 0xFFFF) { ++*untimed; continue; }
        st[n] = s; en[n] = ef != 0xFFFF ? ef : (uint16_t)(s + ln); if (en[n] < s) en[n] = s; ++n;
    }
    for (int i = 1; i < n; ++i) { uint16_t s = st[i], e = en[i]; int j = i - 1; while (j >= 0 && st[j] > s) { st[j + 1] = st[j]; en[j + 1] = en[j]; --j; } st[j + 1] = s; en[j + 1] = e; }
    // distinct (start) keys ~ windows for the log
    for (int i = 0; i < n; ++i) if (!i || st[i] != st[i - 1]) ++*windows;
    int b = 0; int curEnd = -2;
    for (int i = 0; i < n; ++i) {
        if (st[i] <= curEnd + 1) { if (en[i] > curEnd) curEnd = en[i]; }
        else { ++b; curEnd = en[i]; }
    }
    if (b) *lastEnd = (uint16_t)curEnd;
    return b;
}

// acts = every action of the seal's file (for the ambient rule and the chain lookup); start = the action id the seal is bound in.
static inline SdhResult SdhCount(const SdhAction* acts, int na, uint16_t start) {
    SdhResult r;
    if (!acts || na <= 0) return r;
    // ambient spawn ids: at frame 0 in >= 80 % of the actions
    uint16_t amb[64]; int namb = 0;
    {
        uint16_t seen[256]; int cnt[256]; int ns = 0;
        for (int i = 0; i < na; ++i) {
            uint16_t mine[SDH_EVENTS]; int nm = 0;
            for (int k = 0; k < SDH_EVENTS; ++k) {
                const uint16_t e = acts[i].ev[k];
                if (e < 20000 || e == 0xFFFF || acts[i].evFrame[k] != 0) continue;
                bool dup = false; for (int q = 0; q < nm; ++q) dup |= mine[q] == e;
                if (dup) continue; mine[nm++] = e;
                int s = -1; for (int q = 0; q < ns; ++q) if (seen[q] == e) { s = q; break; }
                if (s < 0 && ns < 256) { s = ns++; seen[s] = e; cnt[s] = 0; }
                if (s >= 0) ++cnt[s];
            }
        }
        for (int q = 0; q < ns && namb < 64; ++q) if (cnt[q] * 5 >= na * 4) amb[namb++] = seen[q];
    }
    uint16_t cur = start;
    for (int depth = 0; depth < SDH_MAXCHAIN && cur != 0xFFFF; ++depth) {
        const SdhAction* a = nullptr;
        for (int i = 0; i < na; ++i) if (acts[i].id == cur) { a = &acts[i]; break; }
        if (!a) break;
        bool again = false; for (int q = 0; q < r.chain; ++q) again |= r.ids[q] == cur;
        if (again) break;
        r.ids[r.chain] = cur;
        uint16_t le = 0;
        const int b = SdhActionBursts(*a, &r.untimed, &r.windows, &le);
        r.perAct[r.chain] = b; r.bursts += b; if (b) r.lastEnd = le;
        for (int k = 0; k < SDH_EVENTS; ++k) {
            const uint16_t e = a->ev[k];
            if (e < 20000 || e == 0xFFFF) continue;
            bool isAmb = false; for (int q = 0; q < namb; ++q) isAmb |= amb[q] == e;
            if (!isAmb) ++r.spawns;
        }
        ++r.chain;
        if (a->next == a->id) break;
        cur = a->next;
    }
    return r;
}

static inline void SdhDescribe(const SdhResult& r, char* out, size_t cap) {
    size_t p = 0; out[0] = 0;
    for (int i = 0; i < r.chain && p + 16 < cap; ++i) p += (size_t)snprintf(out + p, cap - p, "%s%X:%d", i ? " > " : "", r.ids[i], r.perAct[i]);
    if (p + 64 < cap) snprintf(out + p, cap - p, " (windows %d, untimed %d, spawns %d, last burst ends f%u)", r.windows, r.untimed, r.spawns, r.lastEnd);
}
