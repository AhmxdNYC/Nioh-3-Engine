// pc2 #23907: the LIVE boss move catalogue (Nioh3Training and Nioh3BossRematch). Include it right after mod/boss_moves.h and before any code that
// reads the move list. It keeps the old generated tables (boss_moves.h / boss_sequences.h / boss_move_facts.h / move_motion.h) only as an optional
// seed (N3CatInit(hand): [Debug] HandMoves, default 1 = fallback for a boss whose action data was not read yet; 0 = start empty, nothing by hand),
// and N3CatMerge puts the runtime list of a boss (auto_moves.h, read from the game's memory when it spawns) over it: the game data wins.
// From here on kN3Moves / N3_MOVE_COUNT / N3MoveFind, kN3Next / N3_NEXT_COUNT, kN3MoveFacts / N3MoveFactFind / N3MoveFactsBoss and kN3Motion /
// N3_MOTION_COUNT name the live arrays (macros below), so every existing reader uses the runtime data unchanged. Arrays are append-only (a move's
// index never changes, the menus and the per-move preferences use indexes); a superseded hand link / fact is retired by setting its chr to 0.
#pragma once
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "boss_moves.h"
#include "boss_sequences.h"
#include "boss_move_facts.h"
#include "Nioh3Training/src/move_motion.h"
#include "Nioh3Training/src/auto_moves_types.h"
#define N3CAT_CAP 8192  // a macro: move_toggle.h sizes its per-move preference array with it
enum { N3CAT_NEXT_CAP = 4096, N3CAT_FACT_CAP = 8192, N3CAT_MOTION_CAP = 8192 };
// the hand tables as generated (the proof compares against these, auto_moves_cmp.h)
static const N3Move* const kN3MovesHand = kN3Moves; static const int kN3MovesHandN = N3_MOVE_COUNT;
static const N3Next* const kN3NextHand = kN3Next; static const int kN3NextHandN = N3_NEXT_COUNT;
static const N3MoveFact* const kN3FactsHand = kN3MoveFacts; static const int kN3FactsHandN = N3_MOVE_FACT_COUNT;
static N3Move g_n3cat[N3CAT_CAP]; static volatile int g_n3cat_n;
static N3Next g_n3nx[N3CAT_NEXT_CAP]; static volatile int g_n3nx_n;
static N3MoveFact g_n3fa[N3CAT_FACT_CAP]; static volatile int g_n3fa_n;
static N3Motion g_n3mo[N3CAT_MOTION_CAP]; static volatile int g_n3mo_n;
typedef struct { uint8_t known, zero, hits; char damage[96]; } N3ActionRecord;
static N3ActionRecord g_n3records[N3CAT_CAP];
static volatile uint32_t g_n3auto[256]; static volatile int g_n3auto_n;  // bosses whose runtime list is merged
static int g_n3cat_init;
static void N3CatInit(int hand) {
    if (g_n3cat_init) return;
    g_n3cat_init = 1;
    if (!hand) return;
    for (int i = 0; i < N3_MOVE_COUNT && i < N3CAT_CAP; ++i) g_n3cat[i] = kN3Moves[i];
    for (int i = 0; i < N3_NEXT_COUNT && i < N3CAT_NEXT_CAP; ++i) g_n3nx[i] = kN3Next[i];
    // Hit/damage metadata never seeds from hand facts; archive records or unknown only.
    for (int i = 0; i < N3_MOTION_COUNT && i < N3CAT_MOTION_CAP; ++i) g_n3mo[i] = kN3Motion[i];
    g_n3cat_n = N3_MOVE_COUNT < N3CAT_CAP ? N3_MOVE_COUNT : N3CAT_CAP; g_n3nx_n = N3_NEXT_COUNT < N3CAT_NEXT_CAP ? N3_NEXT_COUNT : N3CAT_NEXT_CAP;
    g_n3fa_n = 0; g_n3mo_n = N3_MOTION_COUNT < N3CAT_MOTION_CAP ? N3_MOTION_COUNT : N3CAT_MOTION_CAP;
}
static inline int N3CatFind(uint32_t chr, uint32_t id) {
    const int n = g_n3cat_n;
    for (int i = 0; i < n; ++i) if (g_n3cat[i].chr == chr && g_n3cat[i].id == id) return i;
    return -1;
}
static inline const N3ActionRecord* N3CatRecordFind(uint32_t chr, uint32_t id) {
    int i = N3CatFind(chr, id); return i >= 0 && g_n3records[i].known ? &g_n3records[i] : 0;
}
static inline const N3MoveFact* N3CatFactFind(uint32_t chr, uint32_t id) {
    const int n = g_n3fa_n;
    for (int i = 0; i < n; ++i) if (g_n3fa[i].chr == chr && g_n3fa[i].id == id) return &g_n3fa[i];
    return 0;
}
static inline int N3CatFactsBoss(uint32_t chr) { const int n = g_n3fa_n; for (int i = 0; i < n; ++i) if (g_n3fa[i].chr == chr) return 1; return 0; }
static inline int N3CatIsAuto(uint32_t chr) { const int n = g_n3auto_n; for (int i = 0; i < n; ++i) if (g_n3auto[i] == chr) return 1; return 0; }
// label of a runtime row (cosmetic; hits / unblockable come from the facts rows, the menus group by them)
static inline const char* N3CatLabel(const AmMove* m) {
    if (m->kind == 'I') return "idle (game data)";
    if (m->kind == 'M') return m->motion >= 30000 ? "hit reaction / Ki-break motion (game data)" : "movement / other (game data)";
    return m->great ? "attack, unblockable (game data)" : m->block ? "attack (game data)" : "attack-family motion, no attack block (game data)";
}
// The runtime list of chr wins: kinds of known rows are replaced, new ids are appended, the hand links / facts of chr are retired and the runtime
// ones appended. boss = display name (copied) for new rows; the hand name is kept when the boss has hand rows. Returns rows appended.
static int N3CatMerge(uint32_t chr, const AmMove* m, int n, const char* boss) {
    if (!chr || n <= 0 || N3CatIsAuto(chr)) return 0;
    const char* name = 0;
    for (int i = 0; i < g_n3cat_n && !name; ++i) if (g_n3cat[i].chr == chr) name = g_n3cat[i].boss;
    if (!name) { size_t L = strlen(boss && *boss ? boss : "boss"); char* c = (char*)malloc(L + 16); if (!c) return 0;
                 if (boss && *boss) memcpy(c, boss, L + 1); else snprintf(c, L + 16, "boss %u", chr); name = c; }
    int added = 0;
    for (int k = 0; k < n; ++k) {
        const char kind = m[k].kind;
        const int i = N3CatFind(chr, m[k].id);
        if (i >= 0) { g_n3cat[i].kind = kind; continue; }
        if (g_n3cat_n >= N3CAT_CAP) break;
        N3Move r; r.chr = chr; r.id = m[k].id; r.boss = name; r.label = N3CatLabel(&m[k]); r.kind = kind; r.ms = 0;
        g_n3cat[g_n3cat_n] = r; g_n3cat_n = g_n3cat_n + 1; ++added;
    }
    for (int k = 0; k < n; ++k) {
        int i = N3CatFind(chr, m[k].id); if (i < 0) continue;
        N3ActionRecord* r = &g_n3records[i]; r->known = m[k].record_known; r->zero = m[k].zero_power; r->hits = m[k].hits;
        memcpy(r->damage, m[k].damage, sizeof r->damage); r->damage[sizeof r->damage - 1] = 0;
    }
    for (int i = 0; i < g_n3nx_n; ++i) if (g_n3nx[i].chr == chr) g_n3nx[i].chr = 0;
    for (int k = 0; k < n && g_n3nx_n < N3CAT_NEXT_CAP; ++k) if (m[k].link) {
        N3Next l; l.chr = chr; l.id = m[k].id; l.next = m[k].next; l.loop = m[k].loop;
        g_n3nx[g_n3nx_n] = l; g_n3nx_n = g_n3nx_n + 1;
    }
    for (int i = 0; i < g_n3fa_n; ++i) if (g_n3fa[i].chr == chr) g_n3fa[i].chr = 0;
    for (int k = 0; k < n && g_n3fa_n < N3CAT_FACT_CAP; ++k) if (m[k].fact) {
        N3MoveFact f; f.chr = chr; f.id = m[k].id; f.hits = m[k].record_known ? m[k].hits : 0; f.great = m[k].great; f.attack = m[k].attack; f.grab = m[k].grab;
        g_n3fa[g_n3fa_n] = f; g_n3fa_n = g_n3fa_n + 1;
    }
    for (int k = 0; k < n; ++k) {
        int j = -1; for (int i = 0; i < g_n3mo_n; ++i) if (g_n3mo[i].chr == chr && g_n3mo[i].id == m[k].id) { j = i; break; }
        if (j >= 0) g_n3mo[j].motion = m[k].motion;
        else if (g_n3mo_n < N3CAT_MOTION_CAP) { N3Motion x; x.chr = chr; x.id = m[k].id; x.motion = m[k].motion; g_n3mo[g_n3mo_n] = x; g_n3mo_n = g_n3mo_n + 1; }
    }
    if (g_n3auto_n < 256) { g_n3auto[g_n3auto_n] = chr; g_n3auto_n = g_n3auto_n + 1; }
    return added;
}
#define AMC_MOVES kN3MovesHand
#define AMC_MOVE_COUNT kN3MovesHandN
#define AMC_NEXT kN3NextHand
#define AMC_NEXT_COUNT kN3NextHandN
#define AMC_FACTS kN3FactsHand
#define AMC_FACT_COUNT kN3FactsHandN
#define kN3Moves g_n3cat
#undef N3_MOVE_COUNT
#define N3_MOVE_COUNT ((int)g_n3cat_n)
#define N3MoveFind N3CatFind
#define kN3Next g_n3nx
#undef N3_NEXT_COUNT
#define N3_NEXT_COUNT ((int)g_n3nx_n)
#define kN3MoveFacts g_n3fa
#undef N3_MOVE_FACT_COUNT
#define N3_MOVE_FACT_COUNT ((int)g_n3fa_n)
#define N3MoveFactFind N3CatFactFind
#define N3MoveFactsBoss N3CatFactsBoss
#define kN3Motion g_n3mo
#undef N3_MOTION_COUNT
#define N3_MOTION_COUNT ((int)g_n3mo_n)
