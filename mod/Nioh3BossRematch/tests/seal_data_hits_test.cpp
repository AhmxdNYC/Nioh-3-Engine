// TASK #135148: offline check of seal_data_hits.h (cl /EHsc /std:c++17 /Od tests\seal_data_hits_test.cpp; asserts stay on: no /DNDEBUG).
#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <vector>
#include "../src/seal_data_hits.h"
static void Row(std::vector<uint8_t>& b, uint16_t st, uint16_t en, uint16_t pw, uint16_t el = 0) {
    size_t o = b.size(); b.resize(o + SDH_HB, 0);
    memcpy(&b[o + 8], &st, 2); memcpy(&b[o + 0xA], &en, 2); memcpy(&b[o + 0x10], &pw, 2); memcpy(&b[o + 0x12], &el, 2);
}
int main() {
    std::vector<uint8_t> h1, h2, h3;
    Row(h1, 10, 14, 50); Row(h1, 12, 20, 40); Row(h1, 30, 35, 60);  // overlap -> 1, then a gap -> 2 bursts
    Row(h1, 40, 45, 0, 0);                                            // no damage: ignored
    Row(h2, 5, 8, 70); Row(h2, 9, 12, 70);                           // touching -> 1 burst
    SdhAction a[4];
    a[0].id = 0xC52; a[0].next = 0xC58;                               // intro, no hits
    a[1].id = 0xC58; a[1].next = 0xC57; a[1].hb = h1.data(); a[1].nhb = 4;
    a[2].id = 0xC57; a[2].next = 0xC57; a[2].hb = h2.data(); a[2].nhb = 2;  // self loop ends the chain after counting it
    a[3].id = 0x100; a[3].next = 0x100;
    for (auto& x : a) { x.ev[0] = 20001; x.evFrame[0] = 0; }          // ambient body object in every action
    SdhResult r = SdhCount(a, 4, 0xC52);
    assert(r.chain == 3 && r.bursts == 3 && r.perAct[1] == 2 && r.perAct[2] == 1 && r.spawns == 0 && r.certain());
    assert(r.firstIdx == 1 && r.firstStart == 10 && r.lastIdx == 2 && r.lastEnd == 12);  // TASK #137323 Fast summon phase anchors
    a[1].ev[1] = 20007; a[1].evFrame[1] = 15;                         // projectile spawn in the attack: not certain
    r = SdhCount(a, 4, 0xC52); assert(r.spawns == 1 && !r.certain());
    a[1].ev[1] = 0xFFFF;
    Row(h3, 0xFFFF, 0xFFFF, 30); a[3].hb = h3.data(); a[3].nhb = 1; a[2].next = 0x100;  // untimed damaging row: not certain
    r = SdhCount(a, 4, 0xC52); assert(r.untimed == 1 && !r.certain() && r.chain == 4);
    r = SdhCount(a, 4, 0x999); assert(r.chain == 0 && !r.certain() && r.firstIdx == -1 && r.lastIdx == -1);  // unknown start
    // #135408: helpers noted before the seal binds and from the press-source list are read once each; never below learned.
    SdhWant w; w.Bind(0, 1, 1); assert(w.fromData && w.want == 1);          // 5129B-like: seal data 1, nothing learned
    assert(w.Take(0xA1)); w.Join(1); assert(w.want == 2);                    // pre-bind helper (read at bind): its hit counts
    assert(!w.Take(0xA1)); assert(w.want == 2);                              // same actor again (other list): not counted twice
    assert(w.Take(0xA2)); w.Join(2); assert(w.want == 4 && w.helperData == 3);
    w.Bind(5, 1, 1); assert(w.want == 5 && w.nSeen == 0);                    // learned above data: kept
    assert(w.Take(0xB1)); w.Join(1); assert(w.want == 5);                    // 1+1 < learned 5: stays 5
    assert(w.Take(0xB2)); w.Join(0); assert(w.want == 5 && !w.fromData);     // helper not certain: back to learned, data dropped
    assert(!w.Take(0xB3)); assert(w.want == 5);                              // data mode off: later helpers ignored
    w.Bind(0, 2, 1); assert(w.Take(0xC1)); w.Join(-1); assert(w.want == 0 && !w.fromData);  // unread helper, nothing learned -> idle fallback
    w.Bind(3, 2, 0); assert(!w.fromData && w.want == 3 && !w.Take(0xD1));    // SealHitsData off: learned only
    w.Bind(0, 0, 1); assert(!w.fromData && w.want == 0);                     // seal data not certain: no data count
    w.Bind(0, 1, 1); for (uint64_t a = 1; a <= 20; ++a) if (w.Take(a)) w.Join(1); assert(w.nSeen == 16 && w.want == 17);  // cap
    // TASK #135613: low-confidence exit = max(hit exit, bind + pct % of the duration); -1 = hold for the live end; certain / off unchanged
    w.Bind(1, 0, 1); assert(!w.fromData && w.want == 1);                     // live 80D5F: data 0 (not certain), learned 1 -> want 1
    assert(SdhExitAtMs(w.fromData, 90, 1337, 506, 0) == -1);                 // no duration yet: no exit at hit 1
    assert(SdhExitAtMs(w.fromData, 90, 1337, 506, 3324) == 506 + 2991);      // bind + 90 % of 3324 ms, not 1337
    assert(SdhExitAtMs(w.fromData, 90, 4000, 506, 3324) == 4000);            // hit exit later than the end %: the hit exit
    assert(SdhExitAtMs(w.fromData, 0, 1337, 506, 3324) == 1337);             // SealExitEndPct=0: old rule
    assert(SdhExitAtMs(w.fromData, 250, 1337, 0, 1000) == 1337 && SdhExitAtMs(w.fromData, 250, 900, 0, 1000) == 1000);  // pct capped at 100
    w.Bind(1, 1, 1); assert(w.fromData && SdhExitAtMs(w.fromData, 90, 1337, 506, 3324) == 1337);  // certain data: unchanged
    SdhDurTable t; assert(t.Get(0x80D5F) == 0 && t.At(0x80D5F, 1.4f) == 0);
    assert(t.Note(0x80D5F, 3324, 1.4f) == 4654 && t.At(0x80D5F, 1.4f) == 3324 && t.At(0x80D5F, 2.f) == 2327);  // x1 normalised
    assert(t.Note(0x80D5F, 2000, 1.4f) == 4654);                             // a shorter form never shortens it
    assert(t.Note(0x80D5F, 4000, 1.f) == 4654 && t.Note(0x80D5F, 5000, 1.f) == 5000);  // longest kept
    assert(t.Note(0, 100, 1.f) == 0 && t.Note(0x5129B, 0, 1.f) == 0);        // no core / no time: nothing
    for (uint32_t c = 1; c <= 40; ++c) t.Note(0x1000 + c, 100 * c, 1.f);
    assert(t.n == 32 && t.Get(0x1000 + 40) == 4000);                         // full table: replaces a slot, newest readable
    char d[256]; SdhDescribe(r, d, sizeof d); puts(d);
    {   // TASK #136328: SdhAfterShotOk (early escape: count not certain)
        assert(SdhAfterShotOk(true, 2, 2, 0, 500, 0, 150));     // certain count: as before
        assert(!SdhAfterShotOk(true, 1, 2, 0, 500, 0, 150));    // not all hits in
        assert(!SdhAfterShotOk(false, 2, 2, 0, 900, 0, 150));   // no full form yet
        assert(!SdhAfterShotOk(false, 2, 2, 0, 900, 2100, 150)); // a full form hit again later: a gap, not the end
        assert(!SdhAfterShotOk(false, 2, 2, 0, 1960, 2100, 150) && !SdhAfterShotOk(false, 2, 2, 0, 1940, 2100, 150));  // #138952: not certain never turns back
        assert(!SdhAfterShotOk(false, 2, 2, 3, 2200, 2100, 150)); // data has 3 timed bursts
        assert(!SdhAfterShotOk(false, 0, 0, 0, 2200, 2100, 150));
    }
    {   // TASK #135803: SdhHitsSettled (80D5F: no own window, 1 spawn; live burst 1 closed at 2042 ms, settle 200)
        SdhResult r; r.chain = 2; r.spawns = 1;
        assert(SdhMinBursts(r) == 0);
        assert(!SdhHitsSettled(true, r, 1, 1, false, 2042, 2241, 200) && !SdhHitsSettled(true, r, 1, 1, false, 2042, 2242, 200));  // #138952: spawn = not certain, no settle
        assert(!SdhHitsSettled(true, r, 1, 1, true, 2042, 3000, 200));    // a burst open
        assert(!SdhHitsSettled(true, r, 1, 2, false, 2042, 3000, 200));   // learned wants 2
        assert(!SdhHitsSettled(false, r, 1, 1, false, 2042, 3000, 200));  // data not read at bind
        assert(!SdhHitsSettled(true, r, 1, 1, false, 2042, 3000, -1));    // off
        assert(!SdhHitsSettled(true, r, 1, 0, false, 2042, 3000, 200));   // nothing learned
        SdhResult u; u.untimed = 2; assert(SdhMinBursts(u) == 1);
        // TASK #138494: live 3B1AB (13:57): chain with no window / untimed / spawn, learned 1, hit 1 closed 755 ms -> never settled
        SdhResult z; z.chain = 7; assert(!SdhHitsSettled(true, z, 1, 1, false, 755, 961, 200) && !SdhHitsSettled(true, z, 3, 3, false, 755, 5000, 200));
        // live 14:03: first full form learned 2375 ms, EndPct 80% then cut the next press at 1.907 s (idle was 2.375 s)
        assert(SdhNoHitSource(z) && SdhLowConfExitMs(true, z, 80, 1907, 0, 2375) == -1);
        assert(SdhLowConfExitMs(false, z, 80, 1907, 0, 2375) == -1);  // #138952: unread data holds too
        assert(!SdhAfterShotOk(false, 3, 1, 0, 1590, 1590, 150, true));  // empty chain: after-shot does not turn back
        assert(SdhAfterShotOk(true, 2, 2, 0, 500, 0, 150, true));        // a certain count is unchanged
        SdhResult sp; sp.spawns = 1; assert(!SdhNoHitSource(sp) && SdhLowConfExitMs(true, sp, 80, 600, 0, 2375) == -1);  // #138952 (C383D cut at 80 %)
        assert(!SdhHitsSettled(true, u, 1, 1, false, 755, 955, 200));  // #138952: untimed rows can still hit, no settle
        // live E0E94 (16:54): own 2 + 1 spawn that never hit, learned 2 -> released at close 2640 + 200
        SdhResult o; o.bursts = 2; o.spawns = 1; assert(SdhMinBursts(o) == 2 && !SdhHitsSettled(true, o, 2, 2, false, 2640, 2840, 200));  // #138952: spawn can still hit
        SdhResult c; c.bursts = 2; assert(c.certain() && SdhHitsSettled(true, c, 2, 2, false, 2640, 2840, 200) && !SdhHitsSettled(true, c, 2, 2, false, 2640, 2839, 200));  // certain: unchanged
        assert(!SdhHitsSettled(true, o, 1, 1, false, 2640, 3000, 200));   // own data has 2 timed bursts, 1 seen
    }
    puts("seal_data_hits_test PASS"); return 0;
}
