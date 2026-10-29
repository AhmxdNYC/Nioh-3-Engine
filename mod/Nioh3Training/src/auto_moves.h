// pc2 #23907: boss move list from the game's own action file (pure logic, no game access; tests/auto_moves_test.c). Game side: auto_moves_game.h.
// "Nothing by hand": when a boss spawns, its loaded action data is read from memory (auto_moves_game.h), rebuilt into a file image (AmBuildImage)
// and parsed here. Gives per action: id, motion, next action on finish (+0xE0), attack block, GreatAttack (unblockable) flag, hit count (hit windows of
// the action's own hitbox rows), kind (attack / movement / idle). The same rules as tools/yoritomo_move_facts.py + tools/gen_boss_move_facts.py +
// tools/gen_boss_sequences.py, so the result can be checked 1:1 against the generated mod/boss_*.h headers. No names, no fixed ids, no RVAs.
// File layout (notes/yoritomo-action-fields.md, notes/yoritomo-hits-and-unblockable.md):
//   u32 n = 16, then 16 x (u32 offset, u32 size) sub-tables, offsets from the file start; sub-table 0 = header + per-action packed hitbox lists,
//   sub-table 1 = 336-byte action rows, sub-table 2 = 192-byte hitbox rows.
#pragma once
#include <stdint.h>
#include <string.h>
#include "auto_moves_types.h"
static inline uint16_t AmU16(const uint8_t* d, uint32_t o) { uint16_t v; memcpy(&v, d + o, 2); return v; }
static inline uint32_t AmU32(const uint8_t* d, uint32_t o) { uint32_t v; memcpy(&v, d + o, 4); return v; }

// Header check at d (avail bytes readable from d). Returns the whole file size, 0 if this is not an action-file header.
static uint32_t AmHeader(const uint8_t* d, uint32_t avail) {
    if (avail < 4 + 8 * AM_NSUB || AmU32(d, 0) != AM_NSUB) return 0;
    uint32_t off[AM_NSUB], len[AM_NSUB];
    for (int i = 0; i < AM_NSUB; ++i) { off[i] = AmU32(d, 4 + 8 * i); len[i] = AmU32(d, 8 + 8 * i); }
    if (off[0] != 4 + 8 * AM_NSUB || len[0] < 64 + 8) return 0;
    for (int i = 0; i + 1 < AM_NSUB; ++i) if (off[i] + len[i] != off[i + 1]) return 0;
    if (!len[1] || len[1] % AM_ROW || len[2] % AM_HB || len[1] / AM_ROW > AM_MAXMOVES) return 0;
    if (avail < off[0] + 12) return 0;
    // sub-table 0 word 1 / 2 = (count | record size << 16) of sub-tables 1 and 2
    const uint32_t w1 = AmU32(d, off[0] + 4), w2 = AmU32(d, off[0] + 8);
    if ((w1 >> 16) != AM_ROW || (w1 & 0xFFFF) * AM_ROW != len[1]) return 0;
    if (len[2] && ((w2 >> 16) != AM_HB || (w2 & 0xFFFF) * AM_HB != len[2])) return 0;
    return off[AM_NSUB - 1] + len[AM_NSUB - 1];
}
// In memory (proven live on Yoritomo, pc2 2026-10-22, captures/pc2-automoves): sub-tables 1-15 are byte-identical to the file but each is its own
// allocation, and sub-table 0 is gone: each action's packed hitbox list became (first, count) in its action object, a contiguous range of the
// file's hitbox rows. AmBuildImage rebuilds a minimal file image from those pieces (sub-table 0 = header words + record sizes + packed lists,
// 1 = action rows, 2 = hitbox rows) so AmParse runs unchanged on file data and on memory data. Returns the image size, 0 if it does not fit.
static uint32_t AmImageSize(int na, int nhb, const uint16_t* cnt) {
    uint32_t rec = 0; for (int r = 0; r < na; ++r) rec += 2u + 2u * cnt[r];
    return 4 + 8 * AM_NSUB + 64 + 2u * (uint32_t)na + rec + (uint32_t)na * AM_ROW + (uint32_t)nhb * AM_HB;
}
static uint32_t AmBuildImage(uint8_t* out, uint32_t cap, const uint8_t* rows, int na, const uint8_t* hb, int nhb, const uint16_t* first, const uint16_t* cnt) {
    if (na <= 0 || na > AM_MAXMOVES || nhb < 0 || nhb > 0xFFFF) return 0;
    const uint32_t size = AmImageSize(na, nhb, cnt);
    if (size > cap) return 0;
    memset(out, 0, size);
    uint32_t rec = 0; for (int r = 0; r < na; ++r) rec += 2u + 2u * cnt[r];
    const uint32_t len[3] = {64 + 2u * (uint32_t)na + rec, (uint32_t)na * AM_ROW, (uint32_t)nhb * AM_HB};
    uint32_t off = 4 + 8 * AM_NSUB, v = AM_NSUB; memcpy(out, &v, 4);
    for (int i = 0; i < AM_NSUB; ++i) { const uint32_t L = i < 3 ? len[i] : 0; memcpy(out + 4 + 8 * i, &off, 4); memcpy(out + 8 + 8 * i, &L, 4); off += L; }
    const uint32_t o0 = 4 + 8 * AM_NSUB, w1 = (uint32_t)na | (AM_ROW << 16), w2 = (uint32_t)nhb | (AM_HB << 16);
    memcpy(out + o0 + 4, &w1, 4); memcpy(out + o0 + 8, &w2, 4);
    uint32_t p = o0 + 64 + 2u * (uint32_t)na;
    for (int r = 0; r < na; ++r) {
        const uint16_t sz = (uint16_t)(2 + 2 * cnt[r]); memcpy(out + o0 + 64 + 2 * r, &sz, 2);
        memcpy(out + p, &cnt[r], 2);
        for (int k = 0; k < cnt[r]; ++k) { const uint16_t ix = (uint16_t)(first[r] + k); memcpy(out + p + 2 + 2 * k, &ix, 2); }
        p += sz;
    }
    memcpy(out + o0 + len[0], rows, len[1]);
    if (len[2]) memcpy(out + o0 + len[0] + len[1], hb, len[2]);
    return size;
}

typedef struct { uint16_t start, end, pow, elem; uint8_t grp; int row, nrows; } AmWin;
static uint8_t AmHitGroup(const uint8_t* d, uint32_t b) {  // RVA 0x2B5B98: byte +0xA5; 0xFF -> qword +0x00 bit 17 + 2 * bit 18
    uint8_t g = d[b + 0xA5];
    if (g == 0xFF) { const uint32_t lo = AmU32(d, b); g = (uint8_t)(((lo >> 17) & 1) + 2 * ((lo >> 18) & 1)); }
    return g;
}
static void AmSortWin(AmWin* w, int n) {  // by (start, row), insertion sort (n is small)
    for (int i = 1; i < n; ++i) { AmWin x = w[i]; int j = i - 1; while (j >= 0 && (w[j].start > x.start || (w[j].start == x.start && w[j].row > x.row))) { w[j + 1] = w[j]; --j; } w[j + 1] = x; }
}
// Hit windows of one action's hitbox rows (tools/yoritomo_move_facts.py windows()). idx = the action's hitbox row indexes.
static int AmWindows(const uint8_t* d, const AmTable* t, const uint16_t* idx, int cnt, int* untimed) {
    AmWin rows[AM_MAXROWS], keys[AM_MAXROWS], out[AM_MAXROWS]; int nr = 0, nk = 0, no = 0; *untimed = 0;
    const uint32_t nhb = t->len[2] / AM_HB;
    for (int i = 0; i < cnt && nr < AM_MAXROWS; ++i) {
        if (idx[i] >= nhb) continue;
        const uint32_t b = t->off[2] + (uint32_t)idx[i] * AM_HB;
        const uint16_t st = AmU16(d, b + 8), ef = AmU16(d, b + 0xA), ln = AmU16(d, b + 0xC);
        if (st == 0xFFFF) { ++*untimed; continue; }
        AmWin w = {st, (uint16_t)(ef != 0xFFFF ? ef : st + ln), AmU16(d, b + 0x10), AmU16(d, b + 0x12), AmHitGroup(d, b), idx[i], 1};
        if (w.pow || w.elem) rows[nr++] = w;
    }
    AmSortWin(rows, nr);
    for (int i = 0; i < nr; ++i) {  // 1. same (start frame, hit group) = one hit, power = max (RVA 0x78EADF..0x78EB74)
        int k = -1; for (int j = 0; j < nk; ++j) if (keys[j].start == rows[i].start && keys[j].grp == rows[i].grp) { k = j; break; }
        if (k >= 0) {
            if (rows[i].end > keys[k].end) keys[k].end = rows[i].end;
            if (rows[i].pow > keys[k].pow) keys[k].pow = rows[i].pow;
            if (rows[i].elem > keys[k].elem) keys[k].elem = rows[i].elem;
            keys[k].nrows++;
        } else keys[nk++] = rows[i];
    }
    AmSortWin(keys, nk);
    for (int i = 0; i < nk; ++i) {  // 2. same-group keys that overlap / touch merge (group 0 also needs the same power / element): one swept swing
        int merged = 0;
        for (int j = 0; j < no; ++j) {
            const int same = out[j].grp == keys[i].grp && (keys[i].grp || (out[j].pow == keys[i].pow && out[j].elem == keys[i].elem));
            if (same && keys[i].start <= out[j].end + 1) { if (keys[i].end > out[j].end) out[j].end = keys[i].end; out[j].nrows += keys[i].nrows; merged = 1; break; }
        }
        if (!merged) out[no++] = keys[i];
    }
    return no;
}
static int AmFind(const AmTable* t, uint32_t id) { for (int i = 0; i < t->n; ++i) if (t->m[i].id == id) return i; return -1; }

// human #44217: grabs. Hitbox row u16 +0x14 is the hit type: 12 on normal melee swings (4 / 5 / 7 / 16 / 17 elemental ...), 0xFFFF on grab rows.
// A THROW is an action whose every frame-timed row (+0x08 != 0xFFFF) has type 0xFFFF and at least one deals damage (+0x10 / +0x12 > 0):
// the damage ticks on the caught player (Yoritomo 0x18D1: frames 0 / 47 / 119, power 15 / 30 / 45). A CATCH is the same with power 0 (the
// grab check; Yoritomo 0x4EA7 / 0x4E7C, frame 30). A grab is the throw plus the moves of its motion family (motion / 10: Yoritomo 1060 start
// 0x4E7B, 1061 throw, 1062 catches) that deal no normal-type damage and are a catch, have an attack block or own hitbox rows.
// Data rule (offline survey of the 108 archive characters, human #44217): 80 of the 93 throws are the paired 0x16xx-0x18xx ids next to a
// xx00 / xx01 grab motion pair; motions < 1000 (Arachne 130-132, single body-drop rows) and >= 30000 (reactions) are not counted as throws.
// No action with a damaging normal-type row is ever flagged, so ordinary swings cannot become grabs.
// Proven live only for Yoritomo's 4E7B > 4EA7 > 4E7C > 18D1 chain (human #44217); other bosses are inferred.
static void AmGrabs(const uint8_t* d, AmTable* t, const uint16_t (*rows)[AM_MAXROWS], const int* nrows) {
    static uint8_t fl[AM_MAXMOVES];  // bit 0 throw, 1 catch, 2 normal-type damage
    const uint32_t nhb = t->len[2] / AM_HB;
    for (int r = 0; r < t->n; ++r) {
        int timed = 0, ff = 0, ffdmg = 0, norm = 0;
        for (int k = 0; k < nrows[r]; ++k) {
            if (rows[r][k] >= nhb) continue;
            const uint32_t b = t->off[2] + (uint32_t)rows[r][k] * AM_HB;
            if (AmU16(d, b + 8) == 0xFFFF) continue;
            const int dmg = AmU16(d, b + 0x10) || AmU16(d, b + 0x12);
            ++timed;
            if (AmU16(d, b + 0x14) == 0xFFFF) { ++ff; ffdmg += dmg; } else norm += dmg;
        }
        const uint16_t mo = t->m[r].motion;
        fl[r] = (uint8_t)((timed && ff == timed && ffdmg && mo >= 1000 && mo < 30000) | (timed && ff == timed && !ffdmg) << 1 | (norm > 0) << 2);
    }
    for (int r = 0; r < t->n; ++r) {
        AmMove* m = &t->m[r];
        m->grab = fl[r] & 1;
        if (m->grab || (fl[r] & 4) || m->motion >= 30000 || !((fl[r] & 2) || m->block || m->nrows)) continue;
        for (int q = 0; q < t->n && !m->grab; ++q) m->grab = (fl[q] & 1) && t->m[q].motion / 10 == m->motion / 10;
    }
}

// Parse a whole action-file image (d, size). Returns the number of actions (0 = not an action file).
static int AmParse(const uint8_t* d, uint32_t size, AmTable* t) {
    memset(t, 0, sizeof *t);
    const uint32_t tot = AmHeader(d, size);
    if (!tot || tot > size) return 0;
    t->size = tot;
    for (int i = 0; i < AM_NSUB; ++i) { t->off[i] = AmU32(d, 4 + 8 * i); t->len[i] = AmU32(d, 8 + 8 * i); }
    const int na = (int)(t->len[1] / AM_ROW);
    // action -> own hitbox rows: sub-table 0 packed records (u16 count + u16 indexes), their byte sizes = u16[na] at sub-table 0 + 64
    if (t->len[0] < 64 + 2u * (uint32_t)na) return 0;
    uint32_t sum = 0; for (int r = 0; r < na; ++r) sum += AmU16(d, t->off[0] + 64 + 2 * r);
    if (sum > t->len[0]) return 0;
    uint32_t p = t->off[0] + t->len[0] - sum;
    static uint16_t sp[AM_MAXMOVES][16]; static int nsp[AM_MAXMOVES];  // spawn events at frame 0 (for the ambient rule), up to 16 per action
    static uint16_t own[AM_MAXMOVES][AM_MAXROWS]; static int nown[AM_MAXMOVES];  // own hitbox rows per action (grab rule, AmGrabs)
    for (int r = 0; r < na; ++r) {
        const uint32_t o = t->off[1] + (uint32_t)r * AM_ROW, rec = AmU16(d, t->off[0] + 64 + 2 * r);
        AmMove* m = &t->m[r];
        m->anim_hash = AmU32(d, o + 0xF0);
        m->row = (uint16_t)r; m->id = AmU16(d, o + 0xC); m->motion = AmU16(d, o + 0x20); m->next = AmU16(d, o + 0xE0);
        m->block = AmU16(d, o + 0x13C) != 0xFFFF || AmU16(d, o + 0x142) != 0xFFFF;
        m->great = (AmU16(d, o + 0x04) & 0x2000) != 0;
        uint16_t idx[AM_MAXROWS]; int cnt = 0;
        if (rec >= 2 && p + rec <= t->off[0] + t->len[0]) {
            cnt = AmU16(d, p); if (cnt > AM_MAXROWS) cnt = AM_MAXROWS;
            if (2u + 2u * (uint32_t)cnt > rec) cnt = (int)(rec - 2) / 2;
            for (int k = 0; k < cnt; ++k) idx[k] = AmU16(d, p + 2 + 2 * k);
        }
        p += rec;
        memcpy(own[r], idx, sizeof(uint16_t) * (size_t)cnt); nown[r] = cnt;
        int untimed = 0; const int nw = AmWindows(d, t, idx, cnt, &untimed);
        m->nrows = (uint8_t)(cnt > 255 ? 255 : cnt); m->untimed = (uint8_t)untimed; m->nwin = (uint8_t)(nw > 255 ? 255 : nw);
        nsp[r] = 0;
        for (int k = 0; k < 16; ++k) {  // events +0x40: 20000+N = spawn of sub-table 7 row N
            const uint16_t ev = AmU16(d, o + 0x40 + 6 * k), fr = AmU16(d, o + 0x42 + 6 * k);
            if (ev >= 20000 && ev != 0xFFFF && fr == 0 && nsp[r] < 16) { int dup = 0; for (int q = 0; q < nsp[r]; ++q) dup |= sp[r][q] == ev; if (!dup) sp[r][nsp[r]++] = ev; }
        }
    }
    t->n = na;
    // a spawn at frame 0 in >= 80 % of the actions is a standing body / weapon object, not a projectile of the move (gen_boss_move_facts.py)
    for (int r = 0; r < na; ++r) {
        const uint32_t o = t->off[1] + (uint32_t)r * AM_ROW; AmMove* m = &t->m[r]; int nonamb = 0;
        for (int k = 0; k < 16; ++k) {
            const uint16_t ev = AmU16(d, o + 0x40 + 6 * k), fr = AmU16(d, o + 0x42 + 6 * k), ln = AmU16(d, o + 0x44 + 6 * k);
            if (ev < 20000 || ev == 0xFFFF || (ev == 0xFFFF && fr == 0 && ln == 0xFFFF)) continue;
            int amb = 0;
            if (fr == 0) { int c = 0; for (int q = 0; q < na; ++q) for (int z = 0; z < nsp[q]; ++z) c += sp[q][z] == ev; amb = c * 10 >= na * 8; }
            if (!amb) ++nonamb;
        }
        m->spawns = (uint8_t)(nonamb > 255 ? 255 : nonamb);
        m->hits = (m->spawns || m->untimed) ? 0 : m->nwin;
        m->attack = m->block || m->nwin || m->great;
        m->fact = m->block || m->nrows || m->great;
    }
    // idle: row 0 (initialization), the action most actions end in (the idle stance) and a self-looping non-attack state that 5+ actions end in
    // (an alt idle stance). Data rule (Yoritomo / Nobuharu / Shingen: {0x0, 0x2, 0x3F1} = the IDLE set of tools/gen_boss_sequences.py).
    static int incoming[AM_MAXMOVES]; int top = -1;
    for (int r = 0; r < na; ++r) incoming[r] = 0;
    for (int r = 0; r < na; ++r) { const int k = AmFind(t, t->m[r].next); if (k >= 0 && k != r) ++incoming[k]; }
    for (int r = 0; r < na; ++r) if (top < 0 || incoming[r] > incoming[top]) top = r;
    for (int r = 0; r < na; ++r) {
        AmMove* m = &t->m[r];
        const int idle = r == 0 || r == top || (m->next == m->id && !m->attack && !m->nrows && incoming[r] >= 5);
        m->kind = idle ? 'I' : (m->attack || m->nrows) ? 'A' : 'M';
    }
    for (int r = 0; r < na; ++r) {  // sequence links (+0xE0), boss_sequences.h rule: not into idle, not a self loop
        AmMove* m = &t->m[r]; const int k = AmFind(t, m->next);
        m->link = m->next != 0xFFFF && m->next != m->id && !(k >= 0 && t->m[k].kind == 'I');
        m->loop = m->link && k >= 0 && t->m[k].next == t->m[k].id;
    }
    AmGrabs(d, t, (const uint16_t (*)[AM_MAXROWS])own, nown);
    return na;
}
// Sequence heads (training_bosses_menu.h SeqHead without hand lists): has a link, is no link target, not idle, the chain has 2+ moves and an attack.
static int AmSeqChain(const AmTable* t, int head, int* out, int cap, int* loop) {
    int n = 0; *loop = 0;
    for (int c = head; c >= 0 && n < cap;) {
        out[n++] = c;
        if (!t->m[c].link) break;
        const int k = AmFind(t, t->m[c].next);
        if (k < 0) break;  // the target is not in the table (another file): the chain ends here
        int seen = 0; for (int i = 0; i < n; ++i) seen |= out[i] == k;
        if (seen) { *loop = 1; break; }
        if (t->m[c].loop && n < cap) { out[n++] = k; *loop = 1; break; }
        c = k;
    }
    return n;
}
static int AmSeqHead(const AmTable* t, int i) {
    if (!t->m[i].link || t->m[i].kind == 'I') return 0;
    for (int j = 0; j < t->n; ++j) if (t->m[j].link && t->m[j].next == t->m[i].id) return 0;
    int ids[8], loop = 0; const int k = AmSeqChain(t, i, ids, 8, &loop);
    if (k < 2) return 0;  // a chain of one move is no sequence
    for (int q = 0; q < k; ++q) if (t->m[ids[q]].attack) return 1;
    return 0;
}
