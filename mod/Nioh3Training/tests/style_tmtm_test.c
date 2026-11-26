// style_tmtm.h: crouch (TMTM) cancel counter.
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include "../src/style_tmtm.h"
static int g_checks;
#define CHECK(x) do { assert(x); ++g_checks; } while (0)

// feeds (act, attack) for `ticks` ticks of 17 ms
static int Feed(SmTmtm* m, const SmTmtmCfg* c, double* t, uint32_t act, int attack, int ticks) {
    int n = 0;
    for (int i = 0; i < ticks; ++i) { n += SmTmtmStep(m, c, *t, act, attack); *t += 17; }
    return n;
}
int main(void) {
    SmTmtmCfg c; SmTmtmCfgDefaults(&c);
    CHECK(c.nThr == 1 && c.thr[0] == 0x28FD && c.nCont == 2 && c.nCrouch == 5 && c.crouch[4] == 0x82A && c.needAttack == 0);
    uint32_t ids[SM_TMTM_IDS]; CHECK(SmTmtmParse("10493, 0x830 ,junk", ids) == 2 && ids[0] == 0x28FD && ids[1] == 0x830);
    CHECK(SmTmtmParse("", ids) == 0);
    SmTmtm m; SmTmtmReset(&m); double t = 0; int n = 0;
    // log example: martial art -> 0x28FD 1 f -> 0x830 crouch -> 0x6EF
    n += Feed(&m, &c, &t, 25568, 1, 8); n += Feed(&m, &c, &t, 0x28FD, 0, 1); n += Feed(&m, &c, &t, 0x830, 0, 1); n += Feed(&m, &c, &t, 0x6EF, 0, 2);
    CHECK(n == 1 && m.n == 1);
    // a real summon: 0x28FD -> 0x28FB -> 0x28AF: not a cancel
    Feed(&m, &c, &t, 25000, 1, 10); Feed(&m, &c, &t, 0x28FD, 0, 1); Feed(&m, &c, &t, 0x28FB, 0, 5); Feed(&m, &c, &t, 0x28AF, 0, 7);
    CHECK(m.n == 1);
    // throw held too long before the crouch: not a cancel
    Feed(&m, &c, &t, 25001, 1, 10); Feed(&m, &c, &t, 0x28FD, 0, 6); Feed(&m, &c, &t, 0x830, 0, 2);
    CHECK(m.n == 1);
    // cut off by something that is not a crouch: not counted
    Feed(&m, &c, &t, 25002, 1, 10); Feed(&m, &c, &t, 0x28FD, 0, 2); Feed(&m, &c, &t, 0x812, 0, 2);
    CHECK(m.n == 1);
    // seal + crouch from neutral (no attack before): counted by default; not with TmtmNeedAttack 1
    Feed(&m, &c, &t, 0x829, 0, 10); Feed(&m, &c, &t, 0x28FD, 0, 1); Feed(&m, &c, &t, 0x82A, 0, 2);
    CHECK(m.n == 2);
    SmTmtmCfg o = c; o.needAttack = 1;
    Feed(&m, &o, &t, 0x829, 0, 10); Feed(&m, &o, &t, 0x28FD, 0, 1); Feed(&m, &o, &t, 0x830, 0, 2);
    CHECK(m.n == 2);
    Feed(&m, &o, &t, 25003, 0, 5); Feed(&m, &o, &t, 25003, 1, 1); Feed(&m, &o, &t, 0x28FD, 0, 1); Feed(&m, &o, &t, 0x830, 0, 2);
    CHECK(m.n == 3);  // a hit window late in the run still makes it an attack
    // act 0 after the throw is not a cancel
    Feed(&m, &c, &t, 25004, 1, 5); Feed(&m, &c, &t, 0x28FD, 0, 1); Feed(&m, &c, &t, 0, 0, 2);
    CHECK(m.n == 3);
    printf("style_tmtm_test: all %d checks passed\n", g_checks);
    return 0;
}
