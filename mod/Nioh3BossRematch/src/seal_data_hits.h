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
    // TASK #137323: chain positions of the first / last action with a burst (-1 none) and the first burst's start frame (Fast summon phases)
    int firstIdx = -1, lastIdx = -1;
    uint16_t firstStart = 0;
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

// TASK #137323: start frame of the first damaging timed row of one action (0xFFFF none).
static inline uint16_t SdhActionFirstStart(const SdhAction& a) {
    uint16_t best = 0xFFFF;
    for (int i = 0; i < a.nhb && i < SDH_MAXROWS; ++i) {
        const uint8_t* r = a.hb + (size_t)i * SDH_HB;
        const uint16_t s = SdhU16(r, 8), pw = SdhU16(r, 0x10), el = SdhU16(r, 0x12);
        if ((pw || el) && s != 0xFFFF && s < best) best = s;
    }
    return best;
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
        r.perAct[r.chain] = b; r.bursts += b; if (b) { r.lastEnd = le; r.lastIdx = r.chain; }
        if (b && r.firstIdx < 0) { r.firstIdx = r.chain; r.firstStart = SdhActionFirstStart(*a); }
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

// TASK #135408 (regression gap in f3b2e14f): the form's wanted burst count from game data. Seal data at bind, plus every helper
// actor's data, whether the helper joined BEFORE the seal bound (pre-bind sources) or after, from either source list. Each actor is
// read once. Never below the learned count; a helper whose data is not certain/unreadable drops the data count (learned or idle fallback).
struct SdhWant {
    int learned = 0, sealData = 0, helperData = 0, want = 0;
    bool fromData = false;
    uint64_t seen[16] = {}; int nSeen = 0;  // helpers already folded in (cap: more helpers than this are ignored, never double counted)
    void Reset() { *this = SdhWant(); }
    // dataMode = [SealHitsData] mode (1 = use the data count). data > 0 = certain count; 0 not certain; < 0 unread.
    void Bind(int learnedHits, int data, int dataMode) {
        Reset(); learned = learnedHits; sealData = data; want = learnedHits;
        fromData = dataMode == 1 && data > 0;
        if (fromData) want = data > learned ? data : learned;
    }
    bool Seen(uint64_t actor) const { for (int i = 0; i < nSeen; ++i) if (seen[i] == actor) return true; return false; }
    // true when the helper should be read now (bound, data mode active, not read before). Marks it read.
    bool Take(uint64_t actor) {
        if (!fromData || !actor || Seen(actor) || nSeen >= 16) return false;
        seen[nSeen++] = actor; return true;
    }
    // Fold one helper's data count h in (call after Take returned true).
    void Join(int h) {
        if (!fromData) return;
        if (h > 0) { helperData += h; const int d = sealData + helperData; want = d > learned ? d : learned; }
        else { want = learned; fromData = false; }
    }
};

// TASK #135613 (human report, live 80D5F on d5bb22d5: data 0 (not certain), learned 1 -> the ghost exit fired at hit burst 1, 1.3 s after the
// press, while the yokai attacked on for ~2 s more and then despawned by itself). [Spirit] SealExitEndPct (default 90, 0 = off = the old rule).
// A hit count that is not a CERTAIN game-data count (data <= 0 / unread / dropped by a helper, or SealHitsData off) is low-confidence: the
// hit-N exit may not fire on the hit alone, only at max(hit-N exit time, bind + EndPct % of the attack's duration). The action data has no
// clip length (the action rows only time their hit windows; the motion player's end frame reads 0 on every seal, #61061), so the duration is
// the seal's own live end, bind -> idle edge (act 0x1 / end frame) or bind -> its own despawn, seen on an earlier FULL form of the same core
// this session (memory only, nothing written to the ini: human #61089, no stored timings). None seen yet -> no hit exit at all: the form
// ends on that live end signal (idle edge / despawn / FormMaxSec), which then gives the duration for the next use. Certain counts: unchanged.
// Press-relative ms (all exits are timed from the press). Returns the earliest exit time; -1 = hold for the live end signal.
static inline int SdhExitAtMs(bool certain, int pct, int hitExitMs, int boundMs, int durMs) {
    if (certain || pct <= 0) return hitExitMs;
    if (durMs <= 0) return -1;
    if (pct > 100) pct = 100;
    const int endMs = boundMs + (int)((long long)durMs * pct / 100);
    return hitExitMs > endMs ? hitExitMs : endMs;
}
// Per-core attack duration (bind -> live end) of full forms, normalised to x1 seal speed (ms * SealSpeed) so a SealSpeed change keeps it right.
// The longest one is kept (an exit is never moved earlier by a shorter, odd form). 32 cores; more replace the oldest slot.
struct SdhDurTable {
    uint32_t sid[32] = {}; int ms1x[32] = {}; int n = 0;
    void Reset() { *this = SdhDurTable(); }
    int Get(uint32_t id) const { for (int i = 0; i < n; ++i) if (sid[i] == id) return ms1x[i]; return 0; }
    // returns the stored value after the note
    int Note(uint32_t id, int ms, float mul) {
        if (!id || ms <= 0) return Get(id);
        const int v = (int)(ms * (mul > 0.f ? mul : 1.f) + 0.5f);
        for (int i = 0; i < n; ++i) if (sid[i] == id) { if (v > ms1x[i]) ms1x[i] = v; return ms1x[i]; }
        const int k = n < 32 ? n++ : (int)(id % 32);
        sid[k] = id; ms1x[k] = v; return v;
    }
    int At(uint32_t id, float mul) const { const int v = Get(id); return v > 0 ? (int)(v / (mul > 0.f ? mul : 1.f) + 0.5f) : 0; }
};

// TASK #135803 (human #135779/#135783, live 80D5F on d37fc753: hit burst 1 closed 2.04 s after the press, the SealExitEndPct hold kept
// the form up to 3.08 s, a ~1 s idle pause before the player turned back). [Spirit] SealExitSettleMs (default 200, -1 = off).
// The hold of a low-confidence count lets go once the game data says every hit the attack can make is in: the live burst count has reached
// both the wanted count and the seal's own timed bursts from data, no burst is open, and none opened for settleMs since the last close. Read from the seal's own action file at bind; no per-core ids.
// Live 16:54 (92e4a695, E0E94: own 2 bursts + 1 spawn event, the spawn never made a burst of its own, the form waited 1.35 s for the
// despawn): a spawn event does not always hit, so it is not counted. The wanted count is the learned count of FULL forms (every burst,
// the seal's and its spawns'), so the data adds only the floor of the seal's own timed bursts (untimed-only rows: at least 1).
static inline int SdhMinBursts(const SdhResult& r) { return r.bursts > 0 ? r.bursts : r.untimed ? 1 : 0; }

// TASK #136328 (human #136111 item 2, early escape; regression #136265). SealExitAfterShotMs turned back SealExitAfterShotMs after the
// close of hit 'want' even when that count was not certain. A learned count is the lowest of full forms and the gaps between bursts
// (0.7 s Ibaraki, 1.2 s A2ACB) are as long as 700 ms, so the form ended between two bursts with the yokai attack still going.
// Certain data count: unchanged. Not certain: the live count must also reach the data minimum, and this form's last close must be at or
// past the latest last-hit close (bind -> close) of a full form of this core this session (lastHitMs, SdhDurTable at x1). No full form yet
// = no early turn back (the EndPct / settle / idle hold decides, the rule before #136113). Same rule for every core id.
// A chain with no damaging window, no untimed row and no spawn (live 3B1AB: data 0/0/0) says nothing about when the
// hits end. The learned count is then a truncated form, and EndPct / after-shot must not turn the player back.
static inline bool SdhNoHitSource(const SdhResult& r) {
    return r.bursts <= 0 && !r.untimed && !r.spawns;
}
// Low-confidence EndPct time. Empty chain: always -1 (hold for the yokai's idle edge / despawn), even after a
// duration was learned. Otherwise SdhExitAtMs. Unread data (shapeOk false) keeps the old rule.
static inline int SdhLowConfExitMs(bool shapeOk, const SdhResult& r, int pct, int hitExitMs, int boundMs, int durMs) {
    if (shapeOk && SdhNoHitSource(r)) return -1;
    return SdhExitAtMs(false, pct, hitExitMs, boundMs, durMs);
}
static inline bool SdhAfterShotOk(bool certain, int bursts, int want, int minBursts, int closeFromBindMs, int lastHitMs, int tolMs, bool noHitSource = false) {
    if (noHitSource && !certain) return false;
    if (want <= 0 || bursts < want) return false;
    if (certain) return true;
    if (bursts < minBursts || lastHitMs <= 0) return false;
    return closeFromBindMs >= lastHitMs - (tolMs > 0 ? tolMs : 0);
}
static inline bool SdhHitsSettled(bool shapeOk, const SdhResult& r, int bursts, int want, bool open, int lastCloseMs, int nowMs, int settleMs) {
    if (!shapeOk || settleMs < 0 || open || lastCloseMs <= 0 || bursts <= 0 || want <= 0) return false;
    if (bursts < want || bursts < SdhMinBursts(r)) return false;
    // TASK #138494 (human #138491, Ninja 3B1AB on 1.8.44): a chain with no damaging window, no untimed row and no spawn says nothing
    // about the hits (they come from actors outside it), so it can never say 'every hit is in'. 3B1AB: data 0/0/0, learned 1, the form
    // ended after hit 1 of 3. Same rule for every core: the EndPct hold / the yokai's own end decides.
    if (r.bursts <= 0 && !r.untimed && !r.spawns) return false;
    return nowMs - lastCloseMs >= settleMs;
}
