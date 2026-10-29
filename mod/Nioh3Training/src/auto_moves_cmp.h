// pc2 #23907: compare the runtime move table (auto_moves.h) with the old hand / generated headers (boss_moves.h kN3Moves, boss_sequences.h kN3Next,
// boss_move_facts.h kN3MoveFacts). Pure logic; used by the offline test and by the in-game proof log line (auto_moves_game.h). The hand headers are only
// the reference here: nothing in the runtime path reads them.
#pragma once
#include "auto_moves.h"
#include "../../boss_moves.h"
#include "../../boss_sequences.h"
#include "../../boss_move_facts.h"
#include <stdio.h>
#ifndef AMC_MOVES  // move_catalog.h points these at the generated tables when the live catalogue (macros over kN3Moves ...) is in use
#define AMC_MOVES kN3Moves
#define AMC_MOVE_COUNT N3_MOVE_COUNT
#define AMC_NEXT kN3Next
#define AMC_NEXT_COUNT N3_NEXT_COUNT
#define AMC_FACTS kN3MoveFacts
#define AMC_FACT_COUNT N3_MOVE_FACT_COUNT
#endif
static int AmcFind(uint32_t chr, uint32_t id) { for (int i = 0; i < AMC_MOVE_COUNT; ++i) if (AMC_MOVES[i].chr == chr && AMC_MOVES[i].id == id) return i; return -1; }
static const N3MoveFact* AmcFact(uint32_t chr, uint32_t id) { for (int i = 0; i < AMC_FACT_COUNT; ++i) if (AMC_FACTS[i].chr == chr && AMC_FACTS[i].id == id) return &AMC_FACTS[i]; return 0; }
static inline int AmcHasChr(uint32_t chr) { for (int i = 0; i < AMC_MOVE_COUNT; ++i) if (AMC_MOVES[i].chr == chr) return 1; return 0; }
static int AmcFactsBoss(uint32_t chr) { for (int i = 0; i < AMC_FACT_COUNT; ++i) if (AMC_FACTS[i].chr == chr) return 1; return 0; }
typedef struct {
    int ids_both, ids_hand_only, ids_rt_only;           // kN3Moves ids vs action rows
    int fact_both, fact_hand_only, fact_rt_only;        // which moves have a facts row
    int atk_ok, atk_bad, great_ok, great_bad, hits_ok, hits_bad;  // per facts row in both
    int next_ok, next_bad, next_rt_only;                // kN3Next links vs +0xE0 links (next and loop flag)
    int kind_ok, kind_bad, kind_skip;                   // attack vs non-attack: hand kind A/S/static attack row vs M/I, runtime A vs M/I
    int seq_rt;                                         // runtime sequence heads
    char bad[512];                                      // first disagreements, "what:id hand/rt"
} AmCmp;
static void AmCmpBad(AmCmp* c, const char* what, uint32_t id, int h, int r) {
    const size_t L = strlen(c->bad);
    if (L + 40 < sizeof c->bad) snprintf(c->bad + L, sizeof c->bad - L, "%s%s:0x%X %d/%d", L ? " " : "", what, id, h, r);
}
static void AmCompare(const AmTable* t, uint32_t chr, AmCmp* c) {
    memset(c, 0, sizeof *c);
    for (int i = 0; i < AMC_MOVE_COUNT; ++i) {
        if (AMC_MOVES[i].chr != chr) continue;
        const int k = AmFind(t, AMC_MOVES[i].id);
        if (k < 0) { ++c->ids_hand_only; AmCmpBad(c, "id", AMC_MOVES[i].id, 1, 0); continue; }
        ++c->ids_both;
        const char hk = AMC_MOVES[i].kind, rk = t->m[k].kind;
        // hand A / S = attack / wind-up seen in a fight; 0 = static attack row (add_static_boss_rows.py) or not classified (Yoritomo); T twitch / L link = log timing
        // classes (how long it ran, what the AI chained): no static twin, not compared
        // compared as attack vs non-attack: hand A / S (seen hitting / winding up) or a static attack row ("static table: attack-family") = attack,
        // hand M / I = non-attack; runtime A = attack, M / I = non-attack
        const int hAtk = hk == 'A' || hk == 'S' || (hk == 0 && AMC_MOVES[i].label && !strncmp(AMC_MOVES[i].label, "static table: attack-family", 27));
        const int hNon = hk == 'M' || hk == 'I';
        if (!hAtk && !hNon) ++c->kind_skip;
        else if (hAtk == (rk == 'A')) ++c->kind_ok;
        else { ++c->kind_bad; AmCmpBad(c, "kind", AMC_MOVES[i].id, hk ? hk : '0', rk); }
    }
    for (int k = 0; k < t->n; ++k) if (AmcFind(chr, t->m[k].id) < 0) { ++c->ids_rt_only; AmCmpBad(c, "id", t->m[k].id, 0, 1); }
    for (int i = 0; i < AMC_FACT_COUNT; ++i) {
        const N3MoveFact* f = &AMC_FACTS[i];
        if (f->chr != chr) continue;
        const int k = AmFind(t, f->id);
        if (k < 0 || !t->m[k].fact) { ++c->fact_hand_only; AmCmpBad(c, "fact", f->id, 1, 0); continue; }
        const AmMove* m = &t->m[k]; ++c->fact_both;
        if (m->attack == f->attack) ++c->atk_ok; else { ++c->atk_bad; AmCmpBad(c, "atk", f->id, f->attack, m->attack); }
        if (m->great == f->great) ++c->great_ok; else { ++c->great_bad; AmCmpBad(c, "great", f->id, f->great, m->great); }
        if (m->hits == f->hits) ++c->hits_ok; else { ++c->hits_bad; AmCmpBad(c, "hits", f->id, f->hits, m->hits); }
    }
    if (AmcFactsBoss(chr)) for (int k = 0; k < t->n; ++k) if (t->m[k].fact && !AmcFact(chr, t->m[k].id)) { ++c->fact_rt_only; AmCmpBad(c, "fact", t->m[k].id, 0, 1); }
    int handNext = 0;
    for (int i = 0; i < AMC_NEXT_COUNT; ++i) {
        if (AMC_NEXT[i].chr != chr) continue;
        ++handNext;
        const int k = AmFind(t, AMC_NEXT[i].id);
        if (k >= 0 && t->m[k].link && t->m[k].next == AMC_NEXT[i].next && t->m[k].loop == AMC_NEXT[i].loop) ++c->next_ok;
        else { ++c->next_bad; AmCmpBad(c, "next", AMC_NEXT[i].id, (int)AMC_NEXT[i].next, k >= 0 ? t->m[k].next : -1); }
    }
    if (handNext) for (int k = 0; k < t->n; ++k) {
        if (!t->m[k].link) continue;
        int has = 0; for (int i = 0; i < AMC_NEXT_COUNT; ++i) has |= AMC_NEXT[i].chr == chr && AMC_NEXT[i].id == t->m[k].id;
        if (!has) { ++c->next_rt_only; AmCmpBad(c, "next", t->m[k].id, -1, t->m[k].next); }
    }
    for (int k = 0; k < t->n; ++k) c->seq_rt += AmSeqHead(t, k);
}
static int AmCmpText(const AmCmp* c, char* b, size_t n) {
    return snprintf(b, n, "ids %d agree / %d hand-only / %d runtime-only; facts rows %d/%d/%d; attack %d/%d, great %d/%d, hits %d/%d (agree/disagree); "
                    "next links %d agree / %d disagree / %d runtime-only; kind %d/%d (%d not comparable); sequences(runtime) %d; first diffs: %s",
                    c->ids_both, c->ids_hand_only, c->ids_rt_only, c->fact_both, c->fact_hand_only, c->fact_rt_only, c->atk_ok, c->atk_bad, c->great_ok,
                    c->great_bad, c->hits_ok, c->hits_bad, c->next_ok, c->next_bad, c->next_rt_only, c->kind_ok, c->kind_bad, c->kind_skip, c->seq_rt,
                    c->bad[0] ? c->bad : "-");
}
