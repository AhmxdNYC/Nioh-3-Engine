#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include "../src/owned_registry.h"
static OrIdentity actor(uint32_t h, uint32_t owner, uint64_t incarnation) {
    OrIdentity a = {1, 0x1000 + h, incarnation, h, 16102, owner}; return a;
}
static uint64_t live(OrRegistry *r, uint32_t h, uint32_t owner) {
    uint64_t t = OrAdd(r, 16102, owner, 1, 1); assert(t);
    assert(OrIssue(r, t, h)); assert(OrObserve(r, t, actor(h, owner, t), 1)); return t;
}
static void balanced(OrRegistry *r) {
    unsigned n = 0; for (unsigned i = 0; i < OR_CAP; ++i) n += r->jobs[i].state != OR_FREE;
    assert(r->acquired - r->released == n);
}
int main(void) {
    OrRegistry r; OrInit(&r, 1, 0);
    assert(!OrAdd(&r, 16102, 3, 1, 0)); assert(!OrAdd(&r, 16102, 3, 0, 1));
    uint64_t a = OrAdd(&r, 16102, 3, 1, 1), b = OrAdd(&r, 16102, 4, 1, 1);
    assert(a && b); assert(!OrAdd(&r, 16102, 5, 1, 1));
    assert(OrIssue(&r, a, 40)); assert(!OrIssue(&r, b, 41));
    OrTimeout(&r, a); assert(OrFind(&r, a)->state == OR_UNKNOWN);
    assert(!OrIssue(&r, b, 41)); assert(!OrReject(&r, a, 0));
    OrClear(&r); assert(!OrFind(&r, b)); assert(!OrFinishClear(&r)); balanced(&r);
    // Timed-out issuance can still arrive after clear; it never reactivates AI/HUD.
    OrIdentity id = actor(40, 3, 1);
    assert(!OrObserve(&r, a, id, 0)); assert(OrObserve(&r, a, id, 1));
    assert(!OrFind(&r, a)->observer); assert(!OrCallback(&r, a, 1, 1));
    OrIdentity reused = id; ++reused.incarnation;
    assert(!OrRemove(&r, a, reused)); assert(OrRemove(&r, a, id));
    assert(!OrRemove(&r, a, id)); // exception before native return cannot retry
    assert(!OrAbsent(&r, a, 0, 0, 1)); assert(!OrAbsent(&r, a, 1, 0, 0));
    assert(!OrAbsent(&r, a, 1, 1, 1));
    OrFind(&r, a)->hud_pending = 1; assert(!OrAbsent(&r, a, 1, 0, 1));
    OrFind(&r, a)->hud_pending = 0; assert(OrAbsent(&r, a, 1, 0, 1));
    assert(OrFinishClear(&r)); assert(!r.player_no_death); balanced(&r);
    // Recycled slots never accept stale tokens or callback generations.
    uint64_t c = live(&r, 42, 3); assert(c != a && c != b);
    assert(!OrFind(&r, a)); assert(!OrCallback(&r, c, 1, 1));
    assert(OrCallback(&r, c, r.generation, 1));
    assert(!OrAdd(&r, 16102, 3, 1, 1));
    uint64_t d = live(&r, 43, 4);
    OrIdentity ci = OrFind(&r, c)->id;
    assert(OrRemove(&r, c, ci)); OrDeleteReturned(&r, c);
    assert(OrFind(&r, c)->state == OR_REMOVING);
    assert(OrCallback(&r, d, r.generation, 1)); // other actor unaffected
    OrLoseWorld(&r); assert(!OrRemove(&r, d, OrFind(&r, d)->id));
    assert(!OrAbsent(&r, c, 1, 0, 1)); assert(!OrFinishClear(&r)); balanced(&r);
    // More than 32 cycles, both captured player baselines, balanced logical pins.
    for (unsigned baseline = 0; baseline < 2; ++baseline) {
        OrInit(&r, 1, baseline);
        for (unsigned cycle = 0; cycle < 100; ++cycle) {
            uint64_t t = live(&r, 80, 7); OrIdentity current = OrFind(&r, t)->id;
            OrClear(&r); uint64_t gen = r.generation; OrClear(&r); assert(gen == r.generation);
            assert(OrRemove(&r, t, current)); OrDeleteReturned(&r, t);
            assert(OrAbsent(&r, t, 1, 0, 1)); assert(OrFinishClear(&r));
            assert(r.player_no_death == baseline); balanced(&r);
        }
        assert(r.acquired == 100 && r.released == 100);
    }
    OrInit(&r, 1, 0); a = OrAdd(&r, 16102, 3, 1, 1); OrTimeout(&r, a);
    assert(!OrFind(&r, a)); balanced(&r);
    a = OrAdd(&r, 16102, 3, 1, 1); assert(OrIssue(&r, a, 40));
    assert(OrReject(&r, a, 1)); assert(!OrFind(&r, a)); balanced(&r);
    r.serial = UINT64_MAX; assert(!OrAdd(&r, 16102, 3, 1, 1));
    puts("owned registry PASS: cancellation, late arrival, proof gates, stale identities, world loss, 200 recycle cycles");
    return 0;
}
