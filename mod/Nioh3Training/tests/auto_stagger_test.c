// user #23432/#23436: automatic Ki-break stagger fallback (auto_stagger.h), offline.
//   py -m ziglang cc -O2 -Wall -fms-extensions tests/auto_stagger_test.c -o build/auto_stagger_test.exe && build/auto_stagger_test.exe
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#undef NDEBUG
#include <assert.h>
#include "../src/auto_stagger.h"
static int badFA6(void* ctx, uint32_t id) { (void)ctx; return id == 0xFA6u; }
// Feeds one run through AstStep the way NfTick does: action `id` starts at t (hit `hitAgo` ms earlier, -1 = no hit), lasts `ms`.
static void run(AstTrack* t, AstStat* st, uint32_t* now, uint32_t id, int hitAgo, uint32_t ms, int learnable) {
    uint32_t rid, rms; int rhit;
    uint32_t hitT = hitAgo >= 0 ? *now - (uint32_t)hitAgo : 0;
    if (AstStep(t, id, *now, hitT, learnable, &rid, &rhit, &rms)) AstAdd(st, AST_N, rid, rhit, rms);
    for (uint32_t k = 16; k < ms; k += 16) if (AstStep(t, id, *now + k, hitT, learnable, &rid, &rhit, &rms)) AstAdd(st, AST_N, rid, rhit, rms);
    *now += ms;
}
static const AstStat* find(const AstStat* st, uint32_t id) { for (int i = 0; i < AST_N; ++i) if (st[i].id == id) return &st[i]; return 0; }
int main(void) {
    AstStat st[AST_N]; AstTrack t; uint32_t now = 100000;
    memset(st, 0, sizeof st); memset(&t, 0, sizeof t);
    assert(AstPick(st, AST_N, 0, 0) == 0);                                     // nothing seen: no fallback
    // a fight: AI attacks (never right after a hit), flinch 0x4E37 (short, frequent), stagger 0x4E38 (longer), idle in between
    for (int r = 0; r < 4; ++r) {
        run(&t, st, &now, 0x2, -1, 500, 1);
        run(&t, st, &now, 0x4E50, -1, 1200, 1);                                  // AI attack
        run(&t, st, &now, 0x4E37, 30, 400, 1);                                   // flinch right after a hit
        run(&t, st, &now, 0x2, -1, 300, 1);
        if (r & 1) run(&t, st, &now, 0x4E38, 16, 1500, 1);                       // big hit reaction (2 of 4 rounds)
        run(&t, st, &now, 0x4E51, r == 0 ? 120 : -1, 900, 1);                    // AI attack that once happened to follow a hit
    }
    run(&t, st, &now, 0x2, -1, 100, 1);
    const AstStat* f = find(st, 0x4E37); assert(f && f->nHit == 4 && f->n == 4 && AstIsReact(f));
    const AstStat* g = find(st, 0x4E38); assert(g && g->nHit == 2 && AstIsReact(g));
    const AstStat* x = find(st, 0x4E51); assert(x && x->n == 4 && x->nHit == 1 && !AstIsReact(x));       // ratio: an AI move that once followed a hit is not a reaction
    assert(!find(st, 0x2));                                                     // idle never stored
    assert(AstScore(g) > AstScore(f));                                          // 2 x 1500 > 4 x 400
    assert(AstPick(st, AST_N, 0, 0) == 0x4E38);
    // the learned bare-crouch id is never the fallback
    AstStat b[AST_N]; memset(b, 0, sizeof b);
    for (int i = 0; i < 3; ++i) AstAdd(b, AST_N, 0xFA6, 1, 3000);
    for (int i = 0; i < 3; ++i) AstAdd(b, AST_N, 0x4E3B, 1, 700);
    assert(AstPick(b, AST_N, 0, 0) == 0xFA6 && AstPick(b, AST_N, badFA6, 0) == 0x4E3B);
    // a single reaction is not enough; very short twitches (< AST_MIN_MS) never qualify; a long knock-down is capped
    memset(b, 0, sizeof b); AstAdd(b, AST_N, 0x4E40, 1, 900); assert(AstPick(b, AST_N, 0, 0) == 0);
    AstAdd(b, AST_N, 0x4E41, 1, 60); AstAdd(b, AST_N, 0x4E41, 1, 60); AstAdd(b, AST_N, 0x4E41, 1, 60); assert(AstPick(b, AST_N, 0, 0) == 0);
    AstAdd(b, AST_N, 0x4E42, 1, 9000); AstAdd(b, AST_N, 0x4E42, 1, 9000);       // knock-down 2 x 9 s -> 2 x cap 2000 = 4000
    for (int i = 0; i < 3; ++i) AstAdd(b, AST_N, 0x4E43, 1, 1500);             // stagger 3 x 1.5 s = 4500
    assert(AstPick(b, AST_N, 0, 0) == 0x4E43);
    // runs that touch a Ki break / HP-0 window (learnable 0 on any frame) are dropped
    memset(st, 0, sizeof st); memset(&t, 0, sizeof t);
    run(&t, st, &now, 0x4E3E, 20, 1000, 0); run(&t, st, &now, 0x4E3E, 20, 1000, 0); run(&t, st, &now, 0x2, -1, 50, 1);
    assert(!find(st, 0x4E3E) && AstPick(st, AST_N, 0, 0) == 0);
    { uint32_t rid, rms; int rhit; memset(&t, 0, sizeof t);                     // a run that turns unlearnable mid-way
      assert(!AstStep(&t, 0x4E37, 1000, 990, 1, &rid, &rhit, &rms)); assert(!AstStep(&t, 0x4E37, 1100, 990, 0, &rid, &rhit, &rms));
      assert(!AstStep(&t, 0x2, 1400, 990, 1, &rid, &rhit, &rms)); }
    // hit window edge: <= AST_HIT_MS counts, later does not
    { uint32_t rid, rms; int rhit; memset(&t, 0, sizeof t);
      AstStep(&t, 0x4E37, 5000, 5000 - AST_HIT_MS, 1, &rid, &rhit, &rms); assert(AstStep(&t, 0x2, 5500, 0, 1, &rid, &rhit, &rms) && rid == 0x4E37 && rhit == 1 && rms == 500);
      memset(&t, 0, sizeof t); AstStep(&t, 0x4E37, 5000, 5000 - AST_HIT_MS - 1, 1, &rid, &rhit, &rms); assert(AstStep(&t, 0x2, 5500, 0, 1, &rid, &rhit, &rms) && rhit == 0); }
    // full table: the least-seen entry is replaced, frequent reactions survive
    memset(b, 0, sizeof b);
    for (int i = 0; i < 3; ++i) AstAdd(b, AST_N, 0x4E38, 1, 1200);
    for (uint32_t id = 0x5000; id < 0x5000 + 100; ++id) AstAdd(b, AST_N, id, 0, 800);
    assert(find(b, 0x4E38) && find(b, 0x4E38)->nHit == 3 && AstPick(b, AST_N, 0, 0) == 0x4E38);
    assert(AstSkipId(0) && AstSkipId(2) && AstSkipId(0xFFFF) && AstSkipId(0xFFFFFFFFu) && !AstSkipId(0x4E38));
    puts("auto_stagger_test OK");
    return 0;
}
