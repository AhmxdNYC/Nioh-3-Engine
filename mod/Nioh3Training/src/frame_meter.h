// user #22356 follow-up: SF6-style boss frame meter for Practice. Pure state machine, no game access (tests/frame_meter_test.c).
// Game glue + drawing: frame_meter_game.h (fed once per game tick from the FRAMEPROBE pump, frame_probe_game.h). Generic for every boss:
// it only uses the live probe signals (boss action id, "a hit window is open", "that window is unblockable" = hit +0x13C bit 10), no per-boss table.
//   One run = one boss action (from its start to the next action id). Cells are 60 fps frames of a pausable meter clock (ms * 0.06).
//   While a run has no hit yet its cells are PENDING; at the first hit window they turn STARTUP; a run that ends without a hit turns NEUTRAL.
//   Active cells are ACTIVE or ACTIVE_UB (unblockable); after the last window the cells are RECOVERY until the next action.
//   Windows that reopen within FM_TOL_MS of the previous window's end merge into one hit (0x4E6E makes 8 colliders = 2 hits), as in
//   tools/frame_probe_summary.py. Numbers (frames since the action start, as notes/frame-probe-22356.md):
//   ALL displayed frame numbers are 1-based (frame 1 = the action's first frame): startup = first active frame, active a-b = first .. last active frame
//   (the struct keeps ws 0-based; FmText prints ws+1, so "startup 95" and "active 95-106" agree), recovery = total - end of the last window, total = action length.
#pragma once
#include <stdint.h>
#include <stdio.h>
#define FM_CELLS 360    // 6 s at 60 fps (0x4E6E is 333 frames)
#define FM_MAXW 8       // merged windows (hits) kept per run; more are merged into the last one
#define FM_TOL_MS 17.0  // one 60 fps frame
enum { FM_EMPTY = 0, FM_NEUTRAL, FM_PENDING, FM_STARTUP, FM_ACTIVE, FM_ACTIVE_UB, FM_RECOVERY, FM_GRAB };
#define FM_EDGE 0x80    // cell flag: first cell of an action
typedef struct { uint32_t act; int hits, ub, startup, total, recovery, done; int ws[FM_MAXW], we[FM_MAXW]; uint8_t wub[FM_MAXW]; } FmSum;
typedef struct {
    uint8_t cell[FM_CELLS];  // ring, global cell g at [g % FM_CELLS]
    int64_t lastCell;        // newest written global cell (-1 = none)
    uint8_t prevSt;          // state of the previous sample (fills skipped cells)
    int on, nw, open;
    uint32_t act; double t0; int64_t c0;
    double ws[FM_MAXW], we[FM_MAXW]; uint8_t wub[FM_MAXW];
    FmSum cur, last; int haveLast;
    uint8_t frozen[FM_CELLS]; int frozenN;  // the last completed attack as drawn when it ended (its first cell first)
    uint8_t kiwin[FM_CELLS];                // 0 closed, 1 open, 2 perfect (K+0x74 in (0.05, 4]) on that cell
    int64_t kiwinCell;                      // last global cell stamped into kiwin (-1 = none)
    uint8_t kiwinFrozen[FM_CELLS]; int kiwinFrozenN;
    uint8_t kipulse[FM_CELLS];              // 1 on the cell where a Ki pulse landed
    int64_t kipulseCell;
    uint8_t kipulseFrozen[FM_CELLS]; int kipulseFrozenN;
    uint8_t nodmg[FM_CELLS];                // #35176: 1 while a game no-hit / no-damage flag was set on that cell (i-frames, notes/iframes.md)
    int64_t nodmgCell;                      // last global cell stamped into nodmg (-1 = none)
    int nd;                                 // cells of the current run with nodmg set (a dodge with i-frames is shown like an attack)
    uint8_t nodmgFrozen[FM_CELLS]; int nodmgFrozenN;
} FmMeter;
static inline void FmReset(FmMeter* m) { for (int i = 0; i < (int)sizeof *m; ++i) ((uint8_t*)m)[i] = 0; m->lastCell = -1; m->kiwinCell = -1; m->kipulseCell = -1; m->nodmgCell = -1; }
static inline int64_t FmCellOf(double t) { return t <= 0 ? 0 : (int64_t)(t * 0.06); }
static inline int FmRound(double f) { return f <= 0 ? 0 : (int)(f + 0.5); }
static inline void FmRecolor(FmMeter* m, int64_t from, int64_t to, uint8_t a, uint8_t b) {
    if (from < m->lastCell - (FM_CELLS - 1)) from = m->lastCell - (FM_CELLS - 1);
    if (from < 0) from = 0;
    if (to > m->lastCell) to = m->lastCell;
    for (int64_t g = from; g <= to; ++g) { uint8_t* c = &m->cell[g % FM_CELLS]; if ((*c & 0x7F) == a) *c = (uint8_t)((*c & FM_EDGE) | b); }
}
static inline int FmIsActive(uint8_t st) { return st == FM_ACTIVE || st == FM_ACTIVE_UB || st == FM_GRAB; }
static inline void FmPut(FmMeter* m, int64_t g, uint8_t st, int edge) {
    if (g < m->lastCell) return;  // clock went back: ignore
    if (g == m->lastCell) {        // several ticks per 60 fps frame: active wins, the edge flag sticks
        uint8_t* c = &m->cell[g % FM_CELLS]; const uint8_t old = *c & 0x7F;
        const uint8_t ns = st == FM_GRAB || old == FM_GRAB ? FM_GRAB : FmIsActive(st) ? st : FmIsActive(old) ? old : st;
        *c = (uint8_t)((*c & FM_EDGE) | (edge ? FM_EDGE : 0) | ns);
        m->prevSt = st; return;
    }
    int64_t from = m->lastCell + 1;
    if (from < g - FM_CELLS) from = g - FM_CELLS;  // long gap: only the last ring's worth
    for (int64_t k = from; k < g; ++k) m->cell[k % FM_CELLS] = m->lastCell < 0 ? FM_EMPTY : m->prevSt;
    m->cell[g % FM_CELLS] = (uint8_t)(st | (edge ? FM_EDGE : 0));
    m->lastCell = g; m->prevSt = st;
}
static inline void FmSummary(const FmMeter* m, double t, int done, FmSum* s) {
    s->act = m->act; s->hits = m->nw; s->done = done; s->ub = 0;
    s->total = FmRound((t - m->t0) * 0.06);
    for (int i = 0; i < m->nw; ++i) { s->ws[i] = (int)(m->ws[i] * 0.06); s->we[i] = FmRound(m->we[i] * 0.06); s->wub[i] = m->wub[i]; if (m->wub[i]) s->ub++; }
    s->startup = m->nw ? s->ws[0] + 1 : -1;  // window starts are the frame they opened in (floor), ends are rounded
    s->recovery = m->nw ? s->total - s->we[m->nw - 1] : -1;
}
static inline void FmEnd(FmMeter* m, double t) {
    if (!m->on) return;
    if (!m->nw) FmRecolor(m, m->c0, m->lastCell, FM_PENDING, FM_NEUTRAL);
    if (m->nw || m->nd) {  // #35176: a run with i-frames but no hit (a dodge) is frozen too, so its i-frames stay on screen
        FmSummary(m, t, 1, &m->last); m->haveLast = 1;
        int64_t from = m->c0; if (from < m->lastCell - (FM_CELLS - 1)) from = m->lastCell - (FM_CELLS - 1); if (from < 0) from = 0;
        m->frozenN = 0; m->kiwinFrozenN = 0; m->kipulseFrozenN = 0; m->nodmgFrozenN = 0;
        for (int64_t g = from; g <= m->lastCell && m->frozenN < FM_CELLS; ++g) {
            m->frozen[m->frozenN] = m->cell[g % FM_CELLS];
            m->kiwinFrozen[m->kiwinFrozenN] = m->kiwin[g % FM_CELLS];
            m->kipulseFrozen[m->kipulseFrozenN] = m->kipulse[g % FM_CELLS];
            m->nodmgFrozen[m->nodmgFrozenN] = m->nodmg[g % FM_CELLS];
            m->frozenN++; m->kiwinFrozenN++; m->kipulseFrozenN++; m->nodmgFrozenN++;
        }
    }
    m->cur = m->last; m->on = 0;
}
// The view shows the live run while it has a hit or an i-frame cell; otherwise the frozen last one.
static inline int FmLiveRun(const FmMeter* m) { return m->on && (m->nw || m->nd); }
// One sample per game tick. t = meter clock (ms, pausable), act = the boss action id, active = a hit window is open now or a hit was created
// since the previous sample, ub = one of those windows is unblockable.
static inline void FmSampleEx(FmMeter* m, double t, uint32_t act, int active, int ub, int grab) {
    int edge = 0;
    if (!m->on || act != m->act) {
        FmEnd(m, t);
        m->on = 1; m->act = act; m->t0 = t; m->c0 = FmCellOf(t); m->nw = 0; m->nd = 0; m->open = 0; edge = 1;
    }
    const double rel = t - m->t0; const int64_t g = FmCellOf(t);
    if (active) {
        if (m->open) { m->we[m->nw - 1] = rel; m->wub[m->nw - 1] |= (uint8_t)!!ub; }
        else if (m->nw && (rel - m->we[m->nw - 1] <= FM_TOL_MS || m->nw == FM_MAXW)) { m->open = 1; m->we[m->nw - 1] = rel; m->wub[m->nw - 1] |= (uint8_t)!!ub; }
        else {
            m->ws[m->nw] = m->we[m->nw] = rel; m->wub[m->nw] = (uint8_t)!!ub; m->nw++; m->open = 1;
            if (m->nw == 1) FmRecolor(m, m->c0, g - 1, FM_PENDING, FM_STARTUP);
        }
    } else if (m->open) { m->we[m->nw - 1] = rel; m->open = 0; }
    const uint8_t st = active ? (grab ? FM_GRAB : ub ? FM_ACTIVE_UB : FM_ACTIVE) : m->nw ? FM_RECOVERY : FM_PENDING;
    FmPut(m, g, st, edge);
    if (m->nw) FmSummary(m, t, 0, &m->cur); else if (m->haveLast) m->cur = m->last; else m->cur.act = 0, m->cur.hits = 0, m->cur.total = -1;
}
static inline void FmSample(FmMeter* m, double t, uint32_t act, int active, int ub) { FmSampleEx(m,t,act,active,ub,0); }
// Which boss slot the meter follows (user #22918: it always followed the first valid slot = stuck on a stale / idle Boss A, labelled 'Boss A', while another boss
// attacked). A slot that is busy now (hit this tick or an open window of its current action) wins; the current slot wins a tie; else keep the current one while it is
// valid; else the first valid slot; -1 = none.
static inline int FmPickSlot(int cur, int n, const int* valid, const int* busy) {
    int first = -1, firstBusy = -1;
    for (int i = 0; i < n; ++i) { if (!valid[i]) continue; if (first < 0) first = i; if (busy[i]) { if (i == cur) return i; if (firstBusy < 0) firstBusy = i; } }
    if (firstBusy >= 0) return firstBusy;
    if (cur >= 0 && cur < n && valid[cur]) return cur;
    return first;
}
// Cell i of the bar, 0 = oldest .. FM_CELLS-1 = newest (state | FM_EDGE).
static inline uint8_t FmCellAt(const FmMeter* m, int i) {
    const int64_t g = m->lastCell - (FM_CELLS - 1) + i;
    return m->lastCell < 0 || g < 0 ? FM_EMPTY : m->cell[g % FM_CELLS];
}
// View (SF6 style): while the current action has a hit, the bar starts at its first cell (it scrolls only if the action outgrows FM_CELLS);
// otherwise the last completed attack stays frozen (snapshot taken when it ended) until the next attack's first hit. Before any attack it scrolls.
static inline int64_t FmViewStart(const FmMeter* m) {
    if (FmLiveRun(m) && m->lastCell - m->c0 < FM_CELLS && m->c0 >= m->lastCell - (FM_CELLS - 1)) return m->c0;
    return m->lastCell - (FM_CELLS - 1);
}
// KiTick K+0x74 counts down from about 30. Closed at 0 and below, and a stuck high timer is not the window.
static inline int FmKiWindowOpen(float kt74) { return kt74 == kt74 && kt74 > 0.05f && kt74 <= 31.f; }
// #36095: the game's perfect flag is on while this same timer is in (0, 4]. Same floor as the open window.
static inline int FmKiPerfect(float kt74) { return kt74 == kt74 && kt74 > 0.05f && kt74 <= 4.f; }
// Cyan is the press, not the timer. A swing with no R1 still counts through 4 (#36404).
static inline int FmKiPressPerfect(int open, int r1Edge, float kt74) { return open && r1Edge && FmKiPerfect(kt74); }
// 0 off, 1 a Practice session, 2 the player row with no boss. KiWindow or PlayerFrameMeter is enough for the field row.
static inline int FmScope(int wantB, int wantP, int live) {
    if (live && (wantB || wantP)) return 1;
    if (wantP) return 2;
    return 0;
}
// Stamp this sample onto the current cell and any cells the meter just filled across a hitch.
// mark is 0 closed, 1 the rest of the bar, 2 the perfect end.
static inline void FmMarkKiWin(FmMeter* m, double t, int mark) {
    if (m->lastCell < 0) return;
    const int64_t g = FmCellOf(t);
    if (g < 0 || g > m->lastCell) return;
    int64_t from = m->kiwinCell < 0 ? g : m->kiwinCell + 1;
    if (from > g) from = g;
    if (from < g - (FM_CELLS - 1)) from = g - (FM_CELLS - 1);
    if (from < 0) from = 0;
    const uint8_t v = (uint8_t)(mark == 2 ? 2 : mark ? 1 : 0);
    for (int64_t k = from; k <= g; ++k) m->kiwin[k % FM_CELLS] = v;
    m->kiwinCell = g;
}
// The R1 press often starts a new action on the same tick K+0x74 is still in (0, 4].
// The bar freezes before that tick is drawn, so paint the last open cell of the frozen bar.
static inline int FmSealPerfectEnd(FmMeter* m) {
    int i;
    if (!m->haveLast || m->kiwinFrozenN <= 0) return 0;
    i = m->kiwinFrozenN - 1;
    while (i > 0 && m->kiwinFrozen[i] == 0) --i;
    if (m->kiwinFrozen[i] != 1) return 0;
    m->kiwinFrozen[i] = 2;
    return 1;
}
// A perfect R1 press on a frozen bar: the check goes on the last open Ki cell, the same cell FmSealPerfectEnd paints cyan.
static inline int FmSealPerfectCheck(FmMeter* m) {
    int i;
    if (!m->haveLast || m->kiwinFrozenN <= 0 || m->kipulseFrozenN < m->kiwinFrozenN) return 0;
    i = m->kiwinFrozenN - 1;
    while (i > 0 && m->kiwinFrozen[i] == 0) --i;
    if (m->kiwinFrozen[i] == 0) return 0;
    m->kipulseFrozen[i] = 1;
    return 1;
}
// Same index as FmViewCell. 1 = that cell was inside the Ki Pulse bar.
// [Practice] KiWindowPerfectOnly (default 1, human #37066): draw only the perfect part (mark 2); 0 draws the whole bar.
static int g_fm_ki_perfect_only = 1;
static inline int FmViewKiWinRaw(const FmMeter* m, int i) {
    if (!FmLiveRun(m) && m->haveLast) return i >= 0 && i < m->kiwinFrozenN ? m->kiwinFrozen[i] : 0;
    const int64_t g = FmViewStart(m) + i;
    return m->lastCell < 0 || g < 0 || g > m->lastCell ? 0 : m->kiwin[g % FM_CELLS];
}
static inline int FmViewKiWin(const FmMeter* m, int i) {
    const int v = FmViewKiWinRaw(m, i);
    return g_fm_ki_perfect_only && v == 1 ? 0 : v;
}
// The one measured land (log #2231): KiTick +0x5C sat near 53.94, then jumped to 133.43 as Ki moved by about 98.
// The swing that only arms the bar sets +0x5C to about 54 from a low value, so it does not match.
// The jump and the Ki move are not always the same sample. FmPulseArm keeps the Ki from when +0x5C entered 40..80.
static inline int FmKiPulseLanded(float prevKt, float kt, float prevKi, float ki) {
    float d;
    if (!(prevKt == prevKt && kt == kt && prevKi == prevKi && ki == ki)) return 0;
    if (prevKt < 40.f || prevKt > 80.f) return 0;
    if (kt < prevKt + 40.f) return 0;
    d = ki - prevKi; if (d < 0.f) d = -d;
    return d >= 20.f;
}
typedef struct { int have, age; float kt, ki; } FmPulseArm;
static inline int FmKiPulseStep(FmPulseArm* a, float kt, float ki) {
    int hit = a->have && a->age <= 45 && FmKiPulseLanded(a->kt, kt, a->ki, ki);
    if (hit) a->have = 0;
    else if (kt >= 40.f && kt <= 80.f) {
        if (!a->have) { a->kt = kt; a->ki = ki; a->have = 1; a->age = 0; }
        else if (++a->age > 45) a->have = 0;
    } else if (a->have && ++a->age > 45) a->have = 0;
    return hit;
}
// v2 (pc/ki-pulse-success). The old rule above matched log #2231 (probe-1332 t+290906): action 0x6248 -> 0x6249, motion
// 4100 -> 4110, Ki 1019.5 -> 921.1. Ki went DOWN by 98: that is the second hit of the combo, not a pulse, so the old check
// only lit on a second attack press. A pulse gives Ki back. In the vpad misses (#2301) the window K+0x74 counts down from
// about 30 to 0 and the pool K+0x5C drains into Ki by at most about 3.5 Ki per sample (~10 ms) near the end; Ki never
// jumps. Landed = while the window still had time left (previous K+0x74 above 3), either Ki jumps up by at least 15 and
// by at least 60 % of the pool in one sample, or the window closes early (K+0x74 <= 0) with Ki going up.
// Ki going down in the same sample (a new attack) is never a land. Unproven in game: no real pulse is logged yet.
typedef struct { int have; float t74, pool, ki; } FmPulseWatch;
static inline int FmKiPulseLanded2(float pt74, float t74, float ppool, float pki, float ki) {
    float rise;
    if (!(pt74 == pt74 && t74 == t74 && ppool == ppool && pki == pki && ki == ki)) return 0;
    if (pt74 <= 3.f || pt74 > 31.f) return 0;
    rise = ki - pki;
    if (rise <= 0.f) return 0;
    if (rise >= 15.f && rise >= 0.6f * ppool) return 1;
    return t74 <= 0.05f && rise >= 5.f;
}
static inline int FmKiPulseStep2(FmPulseWatch* w, float t74, float pool, float ki) {
    int hit = w->have && FmKiPulseLanded2(w->t74, t74, w->pool, w->ki, ki);
    w->have = 1; w->t74 = t74; w->pool = pool; w->ki = ki;
    return hit;
}
// #36086 idle refill: the window is already closed and the after-time is 0, so a Ki rise to the cap is not a press.
// 0 = not a press, 1 = full window, 2 = perfect (previous timer <= 4).
static inline int FmKiPulseGrade(float pt74, float t74, float pki, float ki, float slop, float pt7c) {
    if (!(pt74 == pt74 && t74 == t74 && pki == pki && ki == ki)) return 0;
    if (pt74 < 0.f || pt74 > 31.f) return 0;
    if (t74 > -0.5f || t74 < -1.5f) return 0;
    if (ki - pki < (pt74 > 0.f ? 1.f : 8.f)) return 0;
    if (pt74 <= 0.f && !(pt7c > 0.f)) return 0;
    return pt74 <= 4.f + slop ? 2 : 1;
}
static inline int FmKiPulseRecent(const FmMeter* m, int64_t g, int span) {
    int64_t k, from;
    if (!m || g < 0 || span < 1) return 0;
    from = g - span; if (from < 0) from = 0;
    for (k = from; k < g; ++k) if (m->kipulse[k % FM_CELLS]) return 1;
    return 0;
}
// Stamp a single cell. Gaps between samples stay empty so an old check is not smeared forward.
static inline void FmMarkKiPulse(FmMeter* m, double t, int landed) {
    if (m->lastCell < 0) return;
    const int64_t g = FmCellOf(t);
    if (g < 0 || g > m->lastCell) return;
    int64_t from = m->kipulseCell < 0 ? g : m->kipulseCell + 1;
    if (from > g) {
        if (landed) m->kipulse[g % FM_CELLS] = 1;
        m->kipulseCell = g;
        return;
    }
    if (from < g - (FM_CELLS - 1)) from = g - (FM_CELLS - 1);
    if (from < 0) from = 0;
    for (int64_t k = from; k <= g; ++k) m->kipulse[k % FM_CELLS] = 0;
    if (landed) m->kipulse[g % FM_CELLS] = 1;
    m->kipulseCell = g;
}
static inline int FmViewKiPulse(const FmMeter* m, int i) {
    if (!FmLiveRun(m) && m->haveLast) return i >= 0 && i < m->kipulseFrozenN ? m->kipulseFrozen[i] : 0;
    const int64_t g = FmViewStart(m) + i;
    return m->lastCell < 0 || g < 0 || g > m->lastCell ? 0 : m->kipulse[g % FM_CELLS];
}
// Cell i of the view, 0 = left .. FM_CELLS-1 = right (state | FM_EDGE, FM_EMPTY where nothing is drawn).
static inline uint8_t FmViewCell(const FmMeter* m, int i) {
    if (!FmLiveRun(m) && m->haveLast) return i < m->frozenN ? m->frozen[i] : FM_EMPTY;
    const int64_t g = FmViewStart(m) + i;
    return m->lastCell < 0 || g < 0 || g > m->lastCell ? FM_EMPTY : m->cell[g % FM_CELLS];
}
// #35176: stamp the game's no-hit / no-damage state (i-frames) on the current cell and the cells filled across a hitch.
// Call after FmSampleEx for the same t. Counts the run's i-frame cells (m->nd) so a dodge without hits is shown.
enum { FMF_NODMG = 1, FMF_EAA = 2, FMF_GUARD = 4, FMF_EA7 = 8 };  // human #37361: special-move flags on the I-FR row
static inline void FmMarkNoDmg(FmMeter* m, double t, int on) {
    if (m->lastCell < 0) return;
    const int64_t g = FmCellOf(t);
    if (g < 0 || g > m->lastCell) return;
    int64_t from = m->nodmgCell < 0 ? g : m->nodmgCell + 1;
    if (from > g) from = g;
    if (from < g - (FM_CELLS - 1)) from = g - (FM_CELLS - 1);
    if (from < 0) from = 0;
    for (int64_t k = from; k <= g; ++k) {
        uint8_t* c = &m->nodmg[k % FM_CELLS]; const uint8_t v = (uint8_t)(on & 0xF);  // bit0 = no-damage (counted in nd), bits 1..3 = FMF_* flags (drawn only)
        if (k > m->nodmgCell) *c = 0;  // a new cell: the ring slot still holds an old value
        if (k >= m->c0 && (v & 1) && !(*c & 1)) m->nd++;
        else if (k >= m->c0 && !(v & 1) && (*c & 1) && m->nd > 0) m->nd--;
        *c = v;
    }
    m->nodmgCell = g;
}
// Same index as FmViewCell. bit0 (1) = the player could not be hit / damaged on that cell.
static inline int FmViewNoDmg(const FmMeter* m, int i) {
    if (!FmLiveRun(m) && m->haveLast) return i >= 0 && i < m->nodmgFrozenN ? m->nodmgFrozen[i] : 0;
    const int64_t g = FmViewStart(m) + i;
    return m->lastCell < 0 || g < 0 || g > m->lastCell ? 0 : m->nodmg[g % FM_CELLS];
}
// Two text lines of a summary: "act 0x4E6E  hits 2 (2 unblockable)" / "startup 95 / active 94-106, 141-157 / recovery 176 / total 333".
static inline void FmText(const FmSum* s, char* l1, size_t n1, char* l2, size_t n2) {
    if (!s->hits) { snprintf(l1, n1, "Frame meter: waiting for a boss attack"); snprintf(l2, n2, "%s", ""); return; }
    snprintf(l1, n1, "act 0x%X  hits %d%s%s", s->act, s->hits, s->ub ? (s->ub == s->hits ? "  UNBLOCKABLE" : "  some unblockable") : "", s->done ? "" : "  (in progress)");
    char a[160]; int o = 0; a[0] = 0;
    for (int i = 0; i < s->hits && i < FM_MAXW && o < (int)sizeof a - 24; ++i) o += snprintf(a + o, sizeof a - o, "%s%d-%d%s", i ? ", " : "", s->ws[i] + 1, s->we[i] > s->ws[i] ? s->we[i] : s->ws[i] + 1, s->wub[i] ? "!" : "");
    snprintf(l2, n2, "startup %d / active %s / recovery %d%s / total %d%s", s->startup, a, s->recovery, s->done ? "" : "+", s->total, s->done ? "" : "+");
}
