// user #14320 / #14323 / #14324: per-boss HP / Ki decisions (boss_vitals.h), offline.
#include <stdint.h>
#include <stdio.h>
#undef NDEBUG
#include <assert.h>
#include "../src/boss_vitals.h"
int main(void) {
    const uint64_t M = 51712; const float K = 1040.0f;  // live log 'Boss HP 51712/51712 Ki 1040/1040'
    BvCfg c; BvDefaults(&c);
    // defaults: everything OFF, levels 100 %: no write ever
    uint64_t h = 0; float k = 0;
    assert(c.infHp == 0 && c.infKi == 0 && c.hpPct == 100 && c.kiPct == 100);
    assert(!BvHpHold(&c, 0, 0, 1, M, &h) && !BvKiHold(&c, 0, 0, 0, 1.0f, K, &k));
    // targets
    assert(BvHpTarget(M, 100) == M && BvHpTarget(M, 50) == 25856 && BvHpTarget(M, 1) == 517 && BvHpTarget(M, 0) == 517 /* clamped to 1 % */);
    assert(BvHpTarget(50, 1) == 1 && BvHpTarget(0, 50) == 0 && BvHpTarget(M, 250) == M);
    assert(BvHpTarget(UINT64_MAX / 50, 100) == UINT64_MAX / 50);  // no overflow
    assert(BvKiTarget(K, 100) == K && BvKiTarget(K, 50) == 520.0f && BvKiTarget(K, 0) == 0.5f && BvKiTarget(0, 50) == 0);
    // infinite health: any HP below the level is refilled; never on a dead boss or in the NOFIN HP-0 window
    c.infHp = 1;
    assert(!BvHpHold(&c, 0, 0, M, M, &h));
    assert(BvHpHold(&c, 0, 0, M - 1, M, &h) && h == M);
    assert(BvHpHold(&c, 0, 0, 1, M, &h) && h == M);
    assert(!BvHpHold(&c, 1, 0, 1, M, &h) && !BvHpHold(&c, 0, 1, 1, M, &h) && !BvHpHold(&c, 0, 0, 1, 0, &h));
    c.hpPct = 50;  // level 50 %: above it nothing, below it back to 50 %
    assert(!BvHpHold(&c, 0, 0, 30000, M, &h) && BvHpHold(&c, 0, 0, 20000, M, &h) && h == 25856);
    // infinite Ki is optional: OFF leaves Ki breaks alone; ON refills, but never while Ki broken (the break plays out)
    assert(!BvKiHold(&c, 0, 0, 0, 0.0f, K, &k));
    c.infKi = 1;
    assert(BvKiHold(&c, 0, 0, 0, 900.0f, K, &k) && k == K);
    assert(!BvKiHold(&c, 0, 0, 0, K, K, &k));
    assert(!BvKiHold(&c, 0, 0, 1, 0.0f, K, &k));  // broken: untouched
    assert(!BvKiHold(&c, 1, 0, 0, 10.0f, K, &k) && !BvKiHold(&c, 0, 1, 0, 10.0f, K, &k) && !BvKiHold(&c, 0, 0, 0, 10.0f, 0.0f, &k));
    c.kiPct = 0;  // level 0 %: Ki kept at 0.5 = one hit from a break
    assert(BvKiHold(&c, 0, 0, 0, 0.2f, K, &k) && k == 0.5f && !BvKiHold(&c, 0, 0, 0, 0.5f, K, &k));
    // menu steps
    assert(BvStepPct(100, 1, 1) == 100 && BvStepPct(100, -1, 1) == 90 && BvStepPct(10, -1, 1) == 5 && BvStepPct(5, -1, 1) == 1 && BvStepPct(1, -1, 1) == 1);
    assert(BvStepPct(1, -1, 0) == 0 && BvStepPct(0, 1, 0) == 1 && BvStepPct(0, -1, 0) == 0 && BvStepPct(37, 1, 1) == 40 && BvStepPct(37, -1, 1) == 30);
    assert(BvStepPct(0, 0, 1) == 1 && BvStepPct(150, 0, 0) == 100);
    // NOFIN hidden reserve (real = shown + off): kept when it fits under max, else dropped
    int keep = 0;
    assert(BvHpRaw(20000, M, 10342, &keep) == 30342 && keep == 1);
    assert(BvHpRaw(M, M, 10342, &keep) == M && keep == 0);
    assert(BvHpRaw(20000, M, 0, &keep) == 20000 && keep == 0);
    // Ki ceiling loss (+0x88, #22592 live: 2.91 on 1664 -> KiSetCur stopped at 1661.1): cleared only when the target is above max - loss
    assert(BvKiCeilClear(1664.0f, 1664.0f, 2.91f) && !BvKiCeilClear(1661.0f, 1664.0f, 2.91f) && !BvKiCeilClear(832.0f, 1664.0f, 2.91f));
    assert(!BvKiCeilClear(1664.0f, 1664.0f, 0.0f) && !BvKiCeilClear(1664.0f, 1664.0f, -1.0f) && !BvKiCeilClear(1664.0f, 1664.0f, 5000.0f));
    puts("PASS: boss vitals: defaults OFF, infinite health / Ki hold at the set level, Ki left alone while broken, NOFIN reserve kept (#14320)");
    return 0;
}
