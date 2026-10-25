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
    uint8_t kiwin[FM_CELLS];                // 1 while the player Ki window (KiTick K+0x74) was open on that cell
    int64_t kiwinCell;                      // last global cell stamped into kiwin (-1 = none)
    uint8_t kiwinFrozen[FM_CELLS]; int kiwinFrozenN;
} FmMeter;
static inline void FmReset(FmMeter* m) { for (int i = 0; i < (int)sizeof *m; ++i) ((uint8_t*)m)[i] = 0; m->lastCell = -1; m->kiwinCell = -1; }
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
    else {
        FmSummary(m, t, 1, &m->last); m->haveLast = 1;
        int64_t from = m->c0; if (from < m->lastCell - (FM_CELLS - 1)) from = m->lastCell - (FM_CELLS - 1); if (from < 0) from = 0;
        m->frozenN = 0; m->kiwinFrozenN = 0;
        for (int64_t g = from; g <= m->lastCell && m->frozenN < FM_CELLS; ++g) {
            m->frozen[m->frozenN] = m->cell[g % FM_CELLS];
            m->kiwinFrozen[m->kiwinFrozenN] = m->kiwin[g % FM_CELLS];
            m->frozenN++; m->kiwinFrozenN++;
        }
    }
    m->cur = m->last; m->on = 0;
}
// One sample per game tick. t = meter clock (ms, pausable), act = the boss action id, active = a hit window is open now or a hit was created
// since the previous sample, ub = one of those windows is unblockable.
static inline void FmSampleEx(FmMeter* m, double t, uint32_t act, int active, int ub, int grab) {
    int edge = 0;
    if (!m->on || act != m->act) {
        FmEnd(m, t);
        m->on = 1; m->act = act; m->t0 = t; m->c0 = FmCellOf(t); m->nw = 0; m->open = 0; edge = 1;
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
    if (m->on && m->nw && m->lastCell - m->c0 < FM_CELLS && m->c0 >= m->lastCell - (FM_CELLS - 1)) return m->c0;
    return m->lastCell - (FM_CELLS - 1);
}
// KiTick K+0x74 counts down from about 30. Closed at 0 and below, and a stuck high timer is not the window.
static inline int FmKiWindowOpen(float kt74) { return kt74 == kt74 && kt74 > 0.05f && kt74 <= 31.f; }
// Stamp this sample onto the current cell and any cells the meter just filled across a hitch.
static inline void FmMarkKiWin(FmMeter* m, double t, int open) {
    if (m->lastCell < 0) return;
    const int64_t g = FmCellOf(t);
    if (g < 0 || g > m->lastCell) return;
    int64_t from = m->kiwinCell < 0 ? g : m->kiwinCell + 1;
    if (from > g) from = g;
    if (from < g - (FM_CELLS - 1)) from = g - (FM_CELLS - 1);
    if (from < 0) from = 0;
    for (int64_t k = from; k <= g; ++k) m->kiwin[k % FM_CELLS] = open ? 1 : 0;
    m->kiwinCell = g;
}
// Same index as FmViewCell. 1 = that cell was inside the Ki window.
static inline int FmViewKiWin(const FmMeter* m, int i) {
    if (!(m->on && m->nw) && m->haveLast) return i >= 0 && i < m->kiwinFrozenN ? m->kiwinFrozen[i] : 0;
    const int64_t g = FmViewStart(m) + i;
    return m->lastCell < 0 || g < 0 || g > m->lastCell ? 0 : m->kiwin[g % FM_CELLS];
}
// Cell i of the view, 0 = left .. FM_CELLS-1 = right (state | FM_EDGE, FM_EMPTY where nothing is drawn).
static inline uint8_t FmViewCell(const FmMeter* m, int i) {
    if (!(m->on && m->nw) && m->haveLast) return i < m->frozenN ? m->frozen[i] : FM_EMPTY;
    const int64_t g = FmViewStart(m) + i;
    return m->lastCell < 0 || g < 0 || g > m->lastCell ? FM_EMPTY : m->cell[g % FM_CELLS];
}
// Two text lines of a summary: "act 0x4E6E  hits 2 (2 unblockable)" / "startup 95 / active 94-106, 141-157 / recovery 176 / total 333".
static inline void FmText(const FmSum* s, char* l1, size_t n1, char* l2, size_t n2) {
    if (!s->hits) { snprintf(l1, n1, "Frame meter: waiting for a boss attack"); snprintf(l2, n2, "%s", ""); return; }
    snprintf(l1, n1, "act 0x%X  hits %d%s%s", s->act, s->hits, s->ub ? (s->ub == s->hits ? "  UNBLOCKABLE" : "  some unblockable") : "", s->done ? "" : "  (in progress)");
    char a[160]; int o = 0; a[0] = 0;
    for (int i = 0; i < s->hits && i < FM_MAXW && o < (int)sizeof a - 24; ++i) o += snprintf(a + o, sizeof a - o, "%s%d-%d%s", i ? ", " : "", s->ws[i] + 1, s->we[i] > s->ws[i] ? s->we[i] : s->ws[i] + 1, s->wub[i] ? "!" : "");
    snprintf(l2, n2, "startup %d / active %s / recovery %d%s / total %d%s", s->startup, a, s->recovery, s->done ? "" : "+", s->total, s->done ? "" : "+");
}
