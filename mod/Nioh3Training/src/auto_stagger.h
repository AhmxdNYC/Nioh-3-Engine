// user #23432/#23436: automatic Ki-break stagger fallback (pure decisions, no game access; tests/auto_stagger_test.c). Game side: NfTick in training.c.
// KiBreakActFix swaps a 'bare' Ki break (break bit 0x3A without the stagger bits = crouch / no animation) to the boss's learned GOOD action, but that
// is only learned from a REAL stagger the boss plays natively. Bosses that never play one (Yagyu Jubei: learned.ini good=0x0) kept the bare break.
// Fallback, learned per boss from its own behaviour (no per-boss table, no new game address): the boss's HIT-REACTION action = an action id that
// (outside any Ki break) almost only starts right after a hit landed on the boss (hit probe 326E81 timestamp, already hooked by KiBreakNoFinisher).
// Yoritomo's visually good break pose 0x4E37/0x4E38 is exactly such a hit reaction (notes/kibreak-deathstance-9895.md), so the family generalises.
//   run    = one stretch of the same action id (start -> change); hit run = started <= AST_HIT_MS after a landed hit
//   react  = id with >= AST_MIN_HITS hit runs and >= 75 % of its runs being hit runs (AI-picked moves that merely follow a hit fail the ratio)
//   pick   = react id with the highest  hitRuns * min(avg hit-run ms, AST_CAP_MS)  (common AND long: a stagger / knockback over a short flinch; the cap
//            keeps a rare long knock-down from winning on length alone). Idle 0x2, 0, 0xFFFF and the boss's learned BAD (bare crouch) ids never qualify.
#pragma once
#include <stdint.h>
enum { AST_N = 24, AST_HIT_MS = 200, AST_MIN_HITS = 2, AST_CAP_MS = 2000, AST_MIN_MS = 150 };
typedef struct { uint32_t id; uint16_t n, nHit; uint32_t ms; } AstStat;            // ms = total held ms of the id's hit runs
typedef struct { uint32_t cur, t0; uint8_t on, hit, learn; } AstTrack;               // the run being watched
static inline int AstSkipId(uint32_t id) { return id == 0 || id == 2 || id >= 0xFFFF; }
// Adds one finished run. Full table: the entry with the fewest runs (no hit runs first) is replaced.
static inline void AstAdd(AstStat* s, int n, uint32_t id, int hit, uint32_t ms) {
    if (AstSkipId(id)) return;
    int f = -1, low = -1;
    for (int i = 0; i < n; ++i) {
        if (s[i].id == id) { f = i; break; }
        if (!s[i].id) { if (f < 0) f = i; continue; }
        if (low < 0 || s[i].nHit < s[low].nHit || (s[i].nHit == s[low].nHit && s[i].n < s[low].n)) low = i;
    }
    if (f < 0 || (s[f].id != id && s[f].id)) f = low;
    if (f < 0) return;
    if (s[f].id != id) { s[f].id = id; s[f].n = 0; s[f].nHit = 0; s[f].ms = 0; }
    if (s[f].n < 0xFFFF) s[f].n++;
    if (hit && s[f].nHit < 0xFFFF) { s[f].nHit++; s[f].ms += ms > 60000u ? 60000u : ms; }
}
static inline int AstIsReact(const AstStat* e) { return e->id && e->nHit >= AST_MIN_HITS && (uint32_t)e->nHit * 4u >= (uint32_t)e->n * 3u; }
static inline uint32_t AstScore(const AstStat* e) {
    if (!AstIsReact(e)) return 0;
    uint32_t avg = e->ms / e->nHit; if (avg < AST_MIN_MS) return 0; if (avg > AST_CAP_MS) avg = AST_CAP_MS;
    return (uint32_t)e->nHit * avg;
}
// Best fallback stagger id (0 = none yet). bad(ctx, id) != 0 excludes an id (learned bare-crouch ids); bad may be 0.
static inline uint32_t AstPick(const AstStat* s, int n, int (*bad)(void*, uint32_t), void* ctx) {
    uint32_t best = 0, bs = 0;
    for (int i = 0; i < n; ++i) { uint32_t sc = AstScore(&s[i]); if (sc > bs && !(bad && bad(ctx, s[i].id))) { bs = sc; best = s[i].id; } }
    return best;
}
// One frame: cur = current action id, lastHit = tick of the last landed hit (0 = none), learnable = boss alive, not Ki-broken, not in the HP-0 window.
// Returns 1 and fills *id / *hit / *ms when a learnable run just ended (a run that saw any non-learnable frame is dropped).
static inline int AstStep(AstTrack* t, uint32_t cur, uint32_t now, uint32_t lastHit, int learnable, uint32_t* id, int* hit, uint32_t* ms) {
    int out = 0;
    if (t->on && cur != t->cur) {
        if (t->learn && !AstSkipId(t->cur)) { *id = t->cur; *hit = t->hit; *ms = now - t->t0; out = 1; }
        t->on = 0;
    }
    if (!t->on) { t->on = 1; t->cur = cur; t->t0 = now; t->hit = lastHit && now - lastHit <= (uint32_t)AST_HIT_MS; t->learn = (uint8_t)(learnable != 0); }
    else if (!learnable) t->learn = 0;
    return out;
}
