// #35870 Ki press probe, pure part. One blue bar: open, an R1 rising edge, six ticks later, and the close.
#pragma once
#include <stdint.h>

enum { KP_OPEN = 1, KP_R1 = 2, KP_AFTER = 3, KP_CLOSE = 4 };

typedef struct { int wasOpen, frame, wasR1, wait; float ki0, kiR; } KpRun;
typedef struct { int kind, frame, end80; float t74, pool, stored, ki, dKi; uint32_t act; } KpEv;

static inline void KpReset(KpRun* s) { s->wasOpen = 0; s->frame = 0; s->wasR1 = 0; s->wait = 0; s->ki0 = 0.f; s->kiR = 0.f; }

static inline int KpPush(KpEv* ev, int n, int cap, int kind, int frame, float t74, float pool, float stored, float ki, float dKi, int end80, uint32_t act) {
    if (!ev || n >= cap) return n;
    ev[n].kind = kind; ev[n].frame = frame; ev[n].t74 = t74; ev[n].pool = pool; ev[n].stored = stored;
    ev[n].ki = ki; ev[n].dKi = dKi; ev[n].end80 = end80; ev[n].act = act;
    return n + 1;
}

// open is the blue bar. r1 is held this tick. frame 0 is the tick the bar opens. Returns how many events were written.
static inline int KpStep(KpRun* s, int open, int r1, float t74, float pool, float stored, float ki, int end80, uint32_t act, KpEv* ev, int cap) {
    int n = 0;
    if (open) {
        if (!s->wasOpen) { s->frame = 0; s->ki0 = ki; s->wait = 0; n = KpPush(ev, n, cap, KP_OPEN, 0, t74, pool, stored, ki, 0.f, end80, act); }
        if (r1 && !s->wasR1) {
            s->kiR = ki; s->wait = 6;
            n = KpPush(ev, n, cap, KP_R1, s->frame, t74, pool, stored, ki, 0.f, end80, act);
        } else if (s->wait > 0) {
            s->wait--;
            if (s->wait == 0) n = KpPush(ev, n, cap, KP_AFTER, s->frame, t74, pool, stored, ki, ki - s->kiR, end80, act);
        }
        s->frame++;
    } else if (s->wasOpen) {
        s->wait = 0;
        n = KpPush(ev, n, cap, KP_CLOSE, s->frame, t74, pool, stored, ki, ki - s->ki0, end80, act);
    }
    s->wasOpen = open ? 1 : 0;
    s->wasR1 = r1 ? 1 : 0;
    return n;
}
