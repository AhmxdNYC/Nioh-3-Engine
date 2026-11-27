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
    {   // TASK #141195 Yasha 5B402 (live 02:29): bound in C53 with 3 bursts, finish link to an action outside the file -> cut, not certain
        std::vector<uint8_t> y; Row(y, 20, 26, 50); Row(y, 30, 40, 50); Row(y, 45, 60, 50);
        SdhAction ya[2]; ya[0].id = 0xC53; ya[0].next = 0xC54; ya[0].hb = y.data(); ya[0].nhb = 3; ya[1].id = 0x200; ya[1].next = 0x200;
        SdhResult yr = SdhCount(ya, 2, 0xC53); assert(yr.bursts == 3 && yr.cut && !yr.certain());
        ya[0].next = 0xFFFF; yr = SdhCount(ya, 2, 0xC53); assert(yr.bursts == 3 && !yr.cut && yr.certain());  // chain ends in the file: certain
    }
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
    {   // TASK #140119: frame exit = start of the last damaging row of the last-burst action, only on a fully mapped seal-only data count
        std::vector<uint8_t> y, k;
        Row(y, 20, 30, 50); Row(y, 31, 40, 50); Row(y, 100, 120, 60); Row(y, 110, 115, 60); Row(y, 200, 0xFFFF, 0, 0);  // 2 bursts; last row starts f110
        SdhAction c[3];
        c[0].id = 0xC5A; c[0].next = 1; c[0].hb = y.data(); c[0].nhb = 5;
        c[1].id = 1; c[1].next = 1;                                        // idle self loop with no hits: fine
        c[2].id = 0xC5B; c[2].next = 0xC5A;
        SdhResult fr = SdhCount(c, 3, 0xC5A);
        assert(fr.certain() && fr.bursts == 2 && fr.lastIdx == 0 && fr.lastBurstStart == 100 && fr.lastStart == 110 && !fr.loopHits && !fr.cut);
        SdhWant fw; fw.Bind(0, 2, 1);
        assert(SdhEndFrameOf(true, fr, fw) == 110);
        assert(!SdhEndFrameReached(fr, 110, 0xC5A, 109.9f) && SdhEndFrameReached(fr, 110, 0xC5A, 110.2f));
        assert(!SdhEndFrameReached(fr, 110, 1, 500.f) && !SdhEndFrameReached(fr, -1, 0xC5A, 500.f));  // other act / off
        assert(SdhEndFrameOf(false, fr, fw) == -1);                          // data not read
        fw.Bind(3, 2, 1); assert(SdhEndFrameOf(true, fr, fw) == -1);          // learned above data: count path
        fw.Bind(0, 2, 1); assert(fw.Take(0xA1)); fw.Join(1); assert(SdhEndFrameOf(true, fr, fw) == -1);  // helper data joined
        fw.Bind(0, 0, 1); assert(SdhEndFrameOf(true, fr, fw) == -1);          // data not certain
        fw.Bind(0, 2, 0); assert(SdhEndFrameOf(true, fr, fw) == -1);          // SealHitsData off
        fr = SdhCount(c, 3, 0xC5B);                                         // C5B -> C5A -> 1: last burst in the 2nd action
        assert(fr.chain == 3 && fr.lastIdx == 1 && fr.ids[1] == 0xC5A && fr.lastStart == 110);
        fw.Bind(0, 2, 1); assert(SdhEndFrameOf(true, fr, fw) == 110 && !SdhEndFrameReached(fr, 110, 0xC5B, 200.f));
        c[0].next = 0xC5A; fr = SdhCount(c, 3, 0xC5A); assert(fr.loopHits && SdhEndFrameOf(true, fr, fw) == -1);  // hitting self loop: can play again
        c[0].next = 0xC5B; fr = SdhCount(c, 3, 0xC5A); assert(fr.loopHits && SdhEndFrameOf(true, fr, fw) == -1);  // cycle back into a hitting action
        c[0].next = 0x777; fr = SdhCount(c, 3, 0xC5A); assert(fr.cut && SdhEndFrameOf(true, fr, fw) == -1);       // next action not in the file
        Row(k, 0xFFFF, 0xFFFF, 30); c[1].hb = k.data(); c[1].nhb = 1; c[0].next = 1;
        fr = SdhCount(c, 3, 0xC5A); assert(fr.untimed == 1 && SdhEndFrameOf(true, fr, fw) == -1);                    // untimed row: count path
        char d2[256]; c[1].nhb = 0; fr = SdhCount(c, 3, 0xC5A); SdhDescribe(fr, d2, sizeof d2); puts(d2);
    }
    {   // TASK #141441: universal 0 ms turn-back - the seal's own data is done past the last source frame of the last source action
        std::vector<uint8_t> y, u;
        Row(y, 20, 30, 50); Row(y, 100, 120, 60); Row(y, 110, 115, 60); Row(y, 200, 0xFFFF, 0, 0);  // last hitbox ends f120 (no-damage row ignored)
        SdhAction c[4];
        c[0].id = 0xC53; c[0].next = 0xC54; c[0].hb = y.data(); c[0].nhb = 4;
        c[1].id = 0xC54; c[1].next = 1;                                    // recovery, no source
        c[2].id = 1; c[2].next = 1;                                        // idle
        c[3].id = 0xC60; c[3].next = 0xC60;
        for (auto& x : c) { x.ev[0] = 20001; x.evFrame[0] = 0; }          // ambient body object: not a source
        SdhResult zr = SdhCount(c, 4, 0xC53);
        assert(zr.srcIdx == 0 && zr.srcEnd == 120 && zr.certain());
        assert(!SdhSealSourcesDone(true, zr, 0xC53, 119.f, true));        // last hitbox still due
        assert(SdhSealSourcesDone(true, zr, 0xC53, 120.f, true));          // past it: nothing left in the seal data
        assert(SdhSealSourcesDone(true, zr, 0xC54, 3.f, true));            // recovery action after the source action
        assert(SdhSealSourcesDone(true, zr, 1, 3.f, true));                // idle
        assert(!SdhSealSourcesDone(true, zr, 1, 3.f, false));              // never seen in the source action: no proof
        assert(!SdhSealSourcesDone(true, zr, 0xC53, 120.f, true, 1, 2));   // 19C1F: live hit 1 of a wanted 2 - not done
        assert(SdhSealSourcesDone(true, zr, 0xC53, 120.f, true, 2, 2));
        assert(!SdhSealSourcesDone(true, zr, 0xC99, 3.f, true));           // 19C1F: an action the chain does not name - no proof
        assert(!SdhSealSourcesDone(true, zr, 0xFFFFFFFF, 500.f, true));    // unreadable action
        assert(!SdhSealSourcesDone(false, zr, 0xC53, 500.f, true));        // data not read
        c[1].ev[1] = 20009; c[1].evFrame[1] = 40;                          // projectile thrown in the recovery action at f40: the source moves on
        zr = SdhCount(c, 4, 0xC53);
        assert(zr.srcIdx == 1 && zr.srcEnd == 40 && !zr.certain());        // not certain, still mapped
        assert(!SdhSealSourcesDone(true, zr, 0xC53, 500.f, true));         // an earlier source action is not the end
        assert(!SdhSealSourcesDone(true, zr, 0xC54, 39.f, true) && SdhSealSourcesDone(true, zr, 0xC54, 40.f, true));
        c[1].ev[1] = 0xFFFF; Row(u, 0xFFFF, 0xFFFF, 30); Row(u, 5, 9, 30); c[1].hb = u.data(); c[1].nhb = 2;  // untimed + timed row in C54
        zr = SdhCount(c, 4, 0xC53); assert(zr.srcIdx == 1 && zr.srcEnd == 9 && zr.untimed == 1 && zr.srcUntimed);
        // TASK #141595: timed+untimed source action - its timed end (f9) is not the end, the untimed row can still hit while the seal is in it
        assert(!SdhSealSourcesDone(true, zr, 0xC54, 9.f, true) && !SdhSealSourcesDone(true, zr, 0xC54, 500.f, true));
        assert(SdhSealSourcesDone(true, zr, 1, 3.f, true));                 // left it for idle after being seen in it: done
        c[1].nhb = 1; zr = SdhCount(c, 4, 0xC53); assert(zr.srcIdx == 1 && zr.srcEnd == 0 && zr.srcUntimed);  // untimed only: a source with no frame of its own
        assert(!SdhSealSourcesDone(true, zr, 0xC54, 0.f, true));            // srcEnd 0 is NOT done on entry (early escape #141595)
        assert(!SdhSealSourcesDone(true, zr, 0xC54, 300.f, true));
        assert(!SdhSealSourcesDone(true, zr, 1, 3.f, false) && SdhSealSourcesDone(true, zr, 1, 3.f, true));
        c[1].nhb = 0; c[0].next = 0xC53; zr = SdhCount(c, 4, 0xC53);
        assert(zr.loopHits && !SdhSealSourcesDone(true, zr, 0xC53, 500.f, true));   // hitting self loop: can play again
        c[0].next = 0x777; zr = SdhCount(c, 4, 0xC53);
        assert(zr.cut && !SdhSealSourcesDone(true, zr, 0xC53, 500.f, true));        // chain leaves the file
        c[0].next = 0xC54; c[1].next = 0xC53; zr = SdhCount(c, 4, 0xC53);
        assert(zr.loopHits && !SdhSealSourcesDone(true, zr, 0xC54, 5.f, true));     // cycle back into the hitting action
        {   // 72E4F: a spawn event in the idle action (0x1) is not a hit source
            SdhAction m[2]; m[0].id = 0xC53; m[0].next = 1; m[0].hb = y.data(); m[0].nhb = 4; m[1].id = 1; m[1].next = 1;
            m[1].ev[0] = 20044; m[1].evFrame[0] = 5;
            SdhResult mr = SdhCount(m, 3, 0xC53); assert(mr.srcIdx == 0 && mr.srcEnd == 120 && mr.spawns == 1);
            assert(SdhSealSourcesDone(true, mr, 1, 0.f, true));
        }
        SdhAction e[1]; e[0].id = 0xC70; e[0].next = 1; zr = SdhCount(e, 1, 0xC70);
        assert(zr.srcIdx == -1 && !SdhSealSourcesDone(true, zr, 0xC70, 500.f, true)); // no source at all: no data proof
    }
    {   // TASK #141728: a spawn whose object tree has no damaging hitbox is no hit source (D15AC: last burst f60, effect spawn f115)
        std::vector<uint8_t> hb, ob; Row(hb, 40, 60, 50);                // C53 slot 0: timed damaging row
        { uint16_t ff = 0xFFFF; Row(ob, 0, 0, 0); memcpy(&ob[8], &ff, 2); }                             // C60 slot 0: untimed, no damage
        { uint16_t ff = 0xFFFF; size_t o = ob.size(); Row(ob, 0, 0, 30); memcpy(&ob[o + 8], &ff, 2); }  // C60 slot 1: damaging untimed row (object hitbox)
        std::vector<uint8_t> o7(3 * SDH_OBJ, 0), o8(1 * SDH_OBJC, 0);
        auto obj = [&](int N, uint16_t act, uint8_t slot) { uint8_t* b = &o7[(size_t)N * SDH_OBJ]; memcpy(b + 0x12, &act, 2); b[0x15] = slot;
                                                            for (int k = 0; k < 6; ++k) { uint16_t ff = 0xFFFF; memcpy(b + 0x110 + 2 * k, &ff, 2); } };
        obj(0, 0xFFFF, 0xFF);            // effect: no own hitbox, no components
        obj(1, 0xC60, 1);                // projectile: own slot 1 = damaging untimed row
        obj(2, 0xC60, 0);                // own slot 0 has no damage, but component 0 = child object 1 (hits)
        { uint16_t c0 = 0, ty = 3, ch = 20001; memcpy(&o7[2 * SDH_OBJ + 0x110], &c0, 2); memcpy(&o8[0x4C], &ty, 2); memcpy(&o8[4], &ch, 2); }
        SdhAction m[3]; m[0].id = 0xC53; m[0].next = 1; m[0].hb = hb.data(); m[0].nhb = 1; m[1].id = 1; m[1].next = 1;
        m[2].id = 0xC60; m[2].next = 0xC60; m[2].hb = ob.data(); m[2].nhb = 2;   // the projectile's own action (not in the chain)
        m[0].ev[0] = 20000; m[0].evFrame[0] = 115;
        SdhResult before = SdhCount(m, 3, 0xC53); assert(before.spawns == 1 && before.srcEnd == 115 && !before.certain());
        std::vector<uint8_t> memo(3, 0); SdhMarkSpawnHits(m, 3, o7.data(), 3, o8.data(), 1, memo.data());
        assert(m[0].evHits[0] == 0);
        SdhResult r2 = SdhCount(m, 3, 0xC53);
        assert(r2.spawns == 0 && r2.freeSpawns == 1 && r2.srcEnd == 60);
        assert(SdhSealSourcesDone(true, r2, 0xC53, 60.f, true) && !SdhSealSourcesDone(true, r2, 0xC53, 59.f, true));
        m[0].ev[1] = 20001; m[0].evFrame[1] = 90; m[0].ev[2] = 20002; m[0].evFrame[2] = 100;   // projectile + its parent: both may hit
        std::fill(memo.begin(), memo.end(), 0); SdhMarkSpawnHits(m, 3, o7.data(), 3, o8.data(), 1, memo.data());
        assert(m[0].evHits[0] == 0 && m[0].evHits[1] == 1 && m[0].evHits[2] == 1);
        r2 = SdhCount(m, 3, 0xC53); assert(r2.spawns == 2 && r2.freeSpawns == 1 && r2.srcEnd == 100);
        std::fill(memo.begin(), memo.end(), 0); SdhMarkSpawnHits(m, 3, nullptr, 0, nullptr, 0, memo.data());  // no tables: may hit
        assert(m[0].evHits[0] == 1 && SdhCount(m, 3, 0xC53).srcEnd == 115);
        uint8_t* b0 = &o7[0]; b0[0x14] = 3; std::fill(memo.begin(), memo.end(), 0);                        // byte +0x14 > 2: may hit
        SdhMarkSpawnHits(m, 3, o7.data(), 3, o8.data(), 1, memo.data()); assert(m[0].evHits[0] == 1);
    }
    {   // TASK #141883 CA2E9 (live 09:48, BR 9b5f673e): bound in C56 (no hits, finish link outside the file = cut), plays C57 (its hit) -> idle
        std::vector<uint8_t> c7h; Row(c7h, 40, 46, 50);
        SdhAction ca[3]; ca[0].id = 0xC56; ca[0].next = 0xC99; ca[1].id = 0xC57; ca[1].next = 1; ca[1].hb = c7h.data(); ca[1].nhb = 1; ca[2].id = 1; ca[2].next = 1;
        SdhResult bound = SdhCount(ca, 3, 0xC56); assert(bound.cut && bound.srcIdx < 0);
        assert(!SdhSealSourcesDone(true, bound, 0xC57, 100.f, true, 1, 1));        // the bind map can never prove it (the old idle wait)
        assert(!SdhNeedsRemap(true, bound, 0xC56) && SdhNeedsRemap(true, bound, 0xC57));
        assert(!SdhNeedsRemap(true, bound, 1) && !SdhNeedsRemap(true, bound, 0xFFFFFFFF) && !SdhNeedsRemap(true, bound, 0xFFFF));
        SdhResult live = SdhCount(ca, 3, 0xC57); assert(!live.cut && live.srcIdx == 0 && live.srcEnd == 46);
        assert(!SdhRemapDone(true, live, 0xC57, 45.f, true, true, 1, 1, true));         // its hitbox is still due
        assert(SdhRemapDone(true, live, 0xC57, 46.f, true, true, 1, 1, true));          // past it: 0 ms turn-back, no idle wait
        assert(SdhRemapDone(true, live, 1, 2.f, true, true, 1, 1, true));               // idle after the source action
        assert(!SdhRemapDone(true, live, 0xC57, 46.f, true, true, 1, 2, true));         // live count below the wanted count: not done
        ca[1].next = 0xC57; live = SdhCount(ca, 3, 0xC57); assert(live.loopHits);  // the hit action loops: never done from data
        assert(!SdhRemapDone(true, live, 0xC57, 99.f, true, true, 1, 1, true));
    }
    {   // TASK #141883 16127: map C80 > 1, live C80 -> C81 (hit-free) -> despawn. A rebuilt map with no source is done only after a source action.
        SdhAction ta[2]; ta[0].id = 0xC81; ta[0].next = 0xFFFF; ta[1].id = 1; ta[1].next = 1;
        SdhResult tail = SdhCount(ta, 2, 0xC81); assert(!tail.cut && !tail.loopHits && tail.srcIdx < 0);
        assert(SdhRemapDone(true, tail, 0xC81, 3.f, false, true, 9, 9, true));
        assert(!SdhRemapDone(true, tail, 0xC81, 3.f, false, false, 9, 9, true));       // never in a source action: no proof (3B1AB rule)
        assert(!SdhRemapDone(true, tail, 0xC81, 3.f, false, true, 8, 9, true));
        ta[0].next = 0xC99; tail = SdhCount(ta, 2, 0xC81); assert(tail.cut && !SdhRemapDone(true, tail, 0xC81, 3.f, false, true, 9, 9, true));
    }
    {   // TASK #141995: want == 0 (no data count, nothing learned) on a rebuilt map never proves the end - the seal already left its finish
        // links once (16127 C80 -> C81), so a later hit reached by a non-link change could be cut. The bind map keeps its old rule.
        std::vector<uint8_t> h1; Row(h1, 10, 20, 30);
        SdhAction ra[3]; ra[0].id = 0xC80; ra[0].next = 0xC81; ra[0].hb = h1.data(); ra[0].nhb = 1; ra[1].id = 0xC81; ra[1].next = 1; ra[2].id = 1; ra[2].next = 1;
        SdhResult live = SdhCount(ra, 3, 0xC80); assert(!live.cut && !live.loopHits && live.srcIdx == 0 && live.srcEnd == 20);
        assert(SdhRemapDone(true, live, 0xC80, 20.f, true, true, 1, 1, true));          // known count met: fires
        assert(!SdhRemapDone(true, live, 0xC80, 20.f, true, true, 1, 0, true));         // want 0, past the source frame: no proof
        assert(!SdhRemapDone(true, live, 0xC81, 3.f, true, true, 3, 0, true));          // want 0, in the hit-free tail after 3 hits: no proof
        assert(!SdhRemapDone(true, live, 1, 2.f, true, true, 1, 0, true));              // want 0, idle: no proof (the yokai's own end decides)
        assert(!SdhRemapDone(true, live, 0xC81, 3.f, true, true, 1, -1, true));         // unknown count
        SdhAction ta[2]; ta[0].id = 0xC81; ta[0].next = 0xFFFF; ta[1].id = 1; ta[1].next = 1;
        SdhResult tail = SdhCount(ta, 2, 0xC81); assert(tail.srcIdx < 0);
        assert(!SdhRemapDone(true, tail, 0xC81, 3.f, false, true, 1, 0, true));        // source-free rebuilt tail, want 0: passedSrc + 1 burst is not enough
        assert(SdhRemapDone(true, tail, 0xC81, 3.f, false, true, 1, 1, true));
        assert(SdhSealSourcesDone(true, live, 0xC80, 20.f, true, 1, 0));          // the bind map (not rebuilt) is unchanged for want 0
    }
    {   // TASK #142009: the tail raise never goes above the complete-form minimum (16127 full 9, tail 10; 1DCD1 full 3, tail 4)
        assert(SdhTailRaise(9, 10, 9) == 0 && SdhTailRaise(3, 4, 3) == 0);
        assert(SdhTailRaise(2, 7, 0) == 7);                       // 98AB: no complete form seen yet - the #139948 raise stays
        assert(SdhTailRaise(2, 7, 5) == 5 && SdhTailRaise(5, 5, 0) == 0 && SdhTailRaise(4, 3, 0) == 0);
        assert(SdhFullMinNote(0, 3) == 3 && SdhFullMinNote(4, 3) == 3 && SdhFullMinNote(3, 4) == 0 && SdhFullMinNote(3, 0) == 0);
    }
    {   // TASK #142146 BEF02 (live A/B 11-27 11:23, BR 995feb67): 50E4 > 50E2 > 50F4 > 5014 > idle, every link '> 1'; lands 2 or 3 bursts.
        // The map rebuilt from 50F4 (source ends f83) proved the end at f83.2 with the learned 2; burst 3 came in 5014 at +2.969.
        std::vector<uint8_t> h4, h5; Row(h4, 70, 83, 90); Row(h5, 20, 40, 50);
        SdhAction ba[3]; ba[0].id = 0x50F4; ba[0].next = 1; ba[0].hb = h4.data(); ba[0].nhb = 1; ba[1].id = 1; ba[1].next = 1;
        SdhResult m4 = SdhCount(ba, 2, 0x50F4); assert(!m4.cut && !m4.loopHits && m4.srcIdx == 0 && m4.srcEnd == 83);
        assert(SdhCountAgreed(2, 2, 2) && !SdhCountAgreed(2, 3, 2) && !SdhCountAgreed(0, 0, 2) && !SdhCountAgreed(0, 3, 3));
        assert(!SdhCountAgreed(2, 0, 2) && !SdhCountAgreed(3, 0, 3) && !SdhCountAgreed(3, 2, 2));  // #142162: low without max / max below low
        assert(!SdhCountAgreed(3, 3, 2) && !SdhCountAgreed(2, 2, 3) && !SdhCountAgreed(2, 2, 0));  // #142184: the agreed count must be the wanted count
        assert(SdhRemapDone(true, m4, 0x50F4, 83.2f, true, true, 2, 2, true));    // the failing case, had the counts agreed
        assert(!SdhRemapDone(true, m4, 0x50F4, 83.2f, true, true, 2, 2, false));  // complete forms 2..3: past the source frame is no proof
        assert(!SdhRemapDone(true, m4, 0x50F4, 200.f, true, true, 2, 2, false));  // nor any later frame of the source action
        assert(SdhNeedsRemap(true, m4, 0x5014));                                  // 5014 is not mapped: rebuilt again, no proof there
        assert(!SdhRemapDone(true, m4, 0x5014, 1.f, true, true, 2, 2, false));
        assert(SdhRemapDone(true, m4, 1, 1.f, true, true, 2, 2, false));          // changed into the mapped next (idle): done
        assert(!SdhRemapDone(true, m4, 1, 1.f, false, true, 2, 2, false));        // never seen in the source action: no proof
        ba[0].id = 0x5014; ba[0].hb = h5.data(); SdhResult m5 = SdhCount(ba, 2, 0x5014); assert(m5.srcIdx == 0);
        assert(!SdhRemapDone(true, m5, 0x5014, 41.f, true, true, 3, 2, false) && SdhRemapDone(true, m5, 1, 1.f, true, true, 3, 2, false));
        SdhAction ta[2]; ta[0].id = 0xC81; ta[0].next = 0xFFFF; ta[1].id = 1; ta[1].next = 1;   // source-free rebuilt tail
        SdhResult tail = SdhCount(ta, 2, 0xC81); assert(tail.srcIdx < 0);
        assert(SdhRemapDone(true, tail, 0xC81, 3.f, false, true, 3, 3, true) && !SdhRemapDone(true, tail, 0xC81, 3.f, false, true, 3, 3, false));
        assert(SdhRemapDone(true, tail, 1, 3.f, false, true, 3, 3, false));      // variable: only after the action changed
        // TASK #142162: a sourced map that ends at a despawn (idle not in the map). A variable core that changes into idle after the
        // source action is done; never seen in the source action, or below the wanted count, it is not.
        SdhAction da[1]; da[0].id = 0x50F4; da[0].next = 0xFFFF; da[0].hb = h4.data(); da[0].nhb = 1;
        SdhResult md = SdhCount(da, 1, 0x50F4); assert(md.srcIdx == 0 && md.chain == 1 && !md.cut);
        assert(SdhRemapDone(true, md, 1, 1.f, true, true, 3, 2, false));
        assert(!SdhRemapDone(true, md, 1, 1.f, false, true, 3, 2, false) && !SdhRemapDone(true, md, 1, 1.f, true, true, 1, 2, false));
        assert(!SdhRemapDone(true, md, 0x50F4, 200.f, true, true, 3, 2, false));
    }
    {   // TASK #142146 78F80 (constant, 2 every form): fires mid-C55 f68 and despawns 0.7 s later with no hit - keeps the 0 ms end
        std::vector<uint8_t> h; Row(h, 50, 68, 80);
        SdhAction ca[2]; ca[0].id = 0xC55; ca[0].next = 1; ca[0].hb = h.data(); ca[0].nhb = 1; ca[1].id = 1; ca[1].next = 1;
        SdhResult m = SdhCount(ca, 2, 0xC55); assert(m.srcIdx == 0 && m.srcEnd == 68);
        const bool agreed = SdhCountAgreed(2, 2, 2);
        assert(agreed && SdhRemapDone(true, m, 0xC55, 68.f, true, true, 2, 2, agreed) && !SdhRemapDone(true, m, 0xC55, 67.f, true, true, 2, 2, agreed));
    }
    {   // TASK #142146: the complete-form maximum (idle edge, own despawn, tail up to the despawn)
        assert(SdhFullMaxNote(0, 2) == 2 && SdhFullMaxNote(2, 3) == 3 && SdhFullMaxNote(3, 2) == 0 && SdhFullMaxNote(3, 3) == 0);
        assert(SdhFullMaxNote(0, 0) == 0 && SdhFullMaxNote(0, 33) == 0);
        int mn = 0, mx = 0; for (int b : {2, 3, 2, 3}) { if (int v = SdhFullMinNote(mn, b)) mn = v; if (int v = SdhFullMaxNote(mx, b)) mx = v; }
        assert(mn == 2 && mx == 3 && !SdhCountAgreed(mn, mx, 2));              // BEF02 history: variable
        mn = mx = 0; for (int b : {2, 2, 2}) { if (int v = SdhFullMinNote(mn, b)) mn = v; if (int v = SdhFullMaxNote(mx, b)) mx = v; }
        assert(SdhCountAgreed(mn, mx, 2));                                     // 78F80 history: constant
        // #142184: an ini with no [SealHitsFullLow*] / [SealHitsFullMax*] (the human's, or the A/B's) is not agreed; the tail watch of one
        // ghost-exited use (the only complete source while the exit is on) writes both, so the 2nd use of a constant core is agreed.
        mn = 0; mx = 0; assert(!SdhCountAgreed(mn, mx, 2));
        if (int v = SdhFullMinNote(mn, 2)) mn = v; if (int v = SdhFullMaxNote(mx, 2)) mx = v; assert(SdhCountAgreed(mn, mx, 2));  // 78F80 use 1 tail 2
        if (int v = SdhFullMinNote(mn, 3)) mn = v; if (int v = SdhFullMaxNote(mx, 3)) mx = v; assert(!SdhCountAgreed(mn, mx, 2));  // BEF02: then 3
        mn = 0; mx = 0; if (int v = SdhFullMinNote(mn, 3)) mn = v; if (int v = SdhFullMaxNote(mx, 3)) mx = v;
        assert(!SdhCountAgreed(mn, mx, 2));                                    // BEF02 3 and 3 but [SealHits] 2: never cut at 2
    }
    puts("seal_data_hits_test PASS"); return 0;
}
