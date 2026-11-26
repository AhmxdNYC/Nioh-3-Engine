// TMTM cancel counter (human 2026-11-26, result card). The More the Merrier (skill): a Summoning Seal throw cancels any attack. Players cut
// the throw off at once (crouch cancel) so the seal is not spent and the attack's recovery is skipped. Pure (no game access);
// tests/style_tmtm_test.c. Game glue: style_meter_game.h, once per tick with the player's action id.
//
// From the 2026-11-26 log (768 cases): a real summon is 0x28FD -> 0x28FB (83 ms) -> 0x28AF -> form; a cancel is 0x28FD for 1 frame
// (17 ms) followed by something else, mostly the crouch family 0x830 / 0x82A / 0x831 / 0x832 / 0x833 (crouch on RS -> 0x830 -> 0x6EF).
// Rule (human 2026-11-26: "TMTM cancels and crouch cancels are the same thing, a crouch input is always needed"; card: "Crouch cancels"):
// the player's action changes to a throw id (TmtmThrowIds), the throw lasts at most TmtmMaxMs and the next action is a crouch (CrouchIds)
// -> one crouch cancel. TmtmNeedAttack 1 also requires a hit window in the run before the throw (default 0: seal + crouch is enough).
// The game fixes these action ids, so they are ini lists (hex or decimal).
#ifndef STYLE_TMTM_H
#define STYLE_TMTM_H
#include <stdint.h>
#include <stdlib.h>

#define SM_TMTM_IDS 8
#define SM_TMTM_MAX_MS 60     // [StyleMeter] TmtmMaxMs: a throw cut off within this many game ms (3 frames at 60 fps; seen: 17 ms)
typedef struct {
    uint32_t thr[SM_TMTM_IDS]; int nThr;     // TmtmThrowIds    (default 0x28FD)
    uint32_t cont[SM_TMTM_IDS]; int nCont;   // TmtmContinueIds (default 0x28FB, 0x28AF)
    uint32_t crouch[SM_TMTM_IDS]; int nCrouch; // CrouchIds     (default 0x830, 0x831, 0x832, 0x833, 0x82A)
    int maxMs, needAttack;                   // TmtmMaxMs (60), TmtmNeedAttack (0)
} SmTmtmCfg;
typedef struct { uint32_t act; int atk; int inThr, thrAtk; double t0, thrT0; uint32_t n; } SmTmtm;

static inline int SmTmtmIn(const uint32_t* a, int n, uint32_t v) { for (int i = 0; i < n; ++i) if (a[i] == v) return 1; return 0; }
// "0x28FD, 10493" -> ids (hex with 0x, else decimal). Returns the count (max SM_TMTM_IDS). Empty / bad = 0.
static inline int SmTmtmParse(const char* s, uint32_t* out) {
    int n = 0;
    while (s && *s && n < SM_TMTM_IDS) {
        while (*s == ' ' || *s == ',' || *s == '\t') ++s;
        if (!*s) break;
        char* e; const unsigned long v = strtoul(s, &e, 0);
        if (e == s) break;
        if (v) out[n++] = (uint32_t)v;
        s = e;
    }
    return n;
}
static inline void SmTmtmCfgDefaults(SmTmtmCfg* c) {
    c->nThr = SmTmtmParse("0x28FD", c->thr); c->nCont = SmTmtmParse("0x28FB,0x28AF", c->cont);
    c->nCrouch = SmTmtmParse("0x830,0x831,0x832,0x833,0x82A", c->crouch); c->maxMs = SM_TMTM_MAX_MS; c->needAttack = 0;
}
static inline void SmTmtmReset(SmTmtm* m) { m->act = 0; m->atk = 0; m->inThr = m->thrAtk = 0; m->t0 = m->thrT0 = 0; m->n = 0; }
// Per tick: act = the player's action id (0 = none / unknown), attack = the current run has had a hit window. Returns 1 on the tick a
// crouch cancel is counted.
static inline int SmTmtmStep(SmTmtm* m, const SmTmtmCfg* c, double t, uint32_t act, int attack) {
    int hit = 0;
    if (act != m->act) {
        if (m->inThr) {  // the throw ended: cut off fast by a crouch (not its own continuation)
            if (t - m->thrT0 <= (double)c->maxMs && act && !SmTmtmIn(c->cont, c->nCont, act) && SmTmtmIn(c->crouch, c->nCrouch, act)
                && (m->thrAtk || !c->needAttack)) { hit = 1; m->n++; }
            m->inThr = 0;
        }
        if (act && SmTmtmIn(c->thr, c->nThr, act)) { m->inThr = 1; m->thrT0 = t; m->thrAtk = m->act && m->atk; }
        m->act = act; m->atk = 0; m->t0 = t;
    }
    if (attack) m->atk = 1;
    return hit;
}
#endif
