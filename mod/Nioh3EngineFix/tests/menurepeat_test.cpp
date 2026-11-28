// #137071: hold-to-repeat in game menus (menurepeat.h) - timing, gating, one-poll release at any poll rate.
#include "../src/menurepeat.h"
#include <cstdio>
static int g_fail = 0, g_n = 0;
#define CHECK(x) do { ++g_n; if (!(x)) { ++g_fail; printf("FAIL line %d: %s\n", __LINE__, #x); } } while (0)
using namespace menurepeat;

// simulate polls every `step` ms for `ms` ms while holding `raw`; count released->pressed edges the game sees
static int Edges(State& s, uint32_t raw, bool active, uint64_t& t, int ms, int step, int speed, uint32_t bit, bool* sawPress = nullptr) {
    int edges = 0;
    bool prevDown = true;  // the game already counted the initial press
    for (int e = 0; e < ms; e += step, t += step) {
        const uint32_t rel = Step(s, raw, active, t, speed);
        const bool down = (raw & bit) && !(rel & bit);
        if (down && !prevDown) ++edges;
        if (down && sawPress) *sawPress = true;
        prevDown = down;
    }
    return edges;
}

int main() {
    {   // inactive (gameplay): never touches the pad
        State s; uint64_t t = 1000;
        CHECK(Edges(s, kDDown | kLB | kLT | kSD, false, t, 3000, 8, 1, kDDown) == 0);
        CHECK(s.repeats == 0);
    }
    {   // Normal at 120 polls/s: nothing before 400 ms, then ~every 90 ms
        State s; uint64_t t = 1000;
        CHECK(Edges(s, kDDown, true, t, 399, 8, 1, kDDown) == 0);
        uint64_t t2 = t; State s2 = s;
        const int e = Edges(s2, kDDown, true, t2, 900, 8, 1, kDDown);
        CHECK(e >= 9 && e <= 11);  // 900 ms / 90 ms
    }
    {   // a 30 fps poller (33 ms) still sees every release and the press after it
        State s; uint64_t t = 1000;
        const int e = Edges(s, kDUp, true, t, 2400, 33, 1, kDUp);
        CHECK(e >= 15 && e <= 23);
    }
    {   // a very slow poller (100 ms): the release poll and a press poll alternate, never stuck released
        State s; uint64_t t = 1000; bool press = false;
        const int e = Edges(s, kRB, true, t, 2000, 100, 2, kRB, &press);
        CHECK(e >= 7 && press);
    }
    {   // speeds: Fast > Normal > Slow
        State a, b, c; uint64_t ta = 0, tb = 0, tc = 0;
        const int f = Edges(a, kDRight, true, ta, 3000, 4, 2, kDRight), n = Edges(b, kDRight, true, tb, 3000, 4, 1, kDRight),
                  sl = Edges(c, kDRight, true, tc, 3000, 4, 0, kDRight);
        CHECK(f > n && n > sl && sl > 0);
    }
    {   // release only the held repeatable bits: A / B / Start never released
        State s; uint64_t t = 1000;
        uint32_t any = 0;
        for (int i = 0; i < 300; ++i, t += 8) any |= Step(s, 0x1000 | 0x2000 | 0x0010 | kLT, true, t, 1);  // A, B, Start, LT
        CHECK(any == 0);  // #137566: LT is analog, never released
    }
    {   // menu opens while the button is already held: full delay from the open, no instant repeat
        State s; uint64_t t = 1000;
        for (int i = 0; i < 100; ++i, t += 8) Step(s, kDDown, false, t, 1);  // held in gameplay
        uint32_t r = 0;
        for (int i = 0; i < 45; ++i, t += 8) r |= Step(s, kDDown, true, t, 1);  // 360 ms into the menu
        CHECK(r == 0);
    }
    {   // let go and press again: the delay restarts
        State s; uint64_t t = 1000;
        Edges(s, kDLeft, true, t, 1000, 8, 1, kDLeft);
        Step(s, 0, true, t, 1); t += 8;
        uint32_t r = 0;
        for (int i = 0; i < 45; ++i, t += 8) r |= Step(s, kDLeft, true, t, 1);
        CHECK(r == 0);
    }
    {   // the menu closes mid-hold: the pad passes through at once
        State s; uint64_t t = 1000;
        Edges(s, kDDown, true, t, 1000, 8, 1, kDDown);
        uint32_t r = 0;
        for (int i = 0; i < 200; ++i, t += 8) r |= Step(s, kDDown, false, t, 1);
        CHECK(r == 0);
    }
    {   // #137478 control-off gate; #183: only bit 0 (the game's own getter 0x27A8B8) counts, no reason ids / mask lists
        CHECK(!ControlOff(false, 0x7));
        CHECK(!ControlOff(true, 0));
        // real control-off reasons seen in PC1 logs: 9A load 007, FE fade-out 187, 48 SetControlOff 2905, 9A+9C 1807
        CHECK(ControlOff(true, 0x007) && ControlOff(true, 0x187) && ControlOff(true, 0x2905) && ControlOff(true, 0x1807) && ControlOff(true, 0x2907));
        // play reasons the game keeps on while the player plays: 9C 1800 + 9F 2040 (fights), 39 1AC0 (after a win), 17 794 (soul core form)
        CHECK(!ControlOff(true, 0x1800) && !ControlOff(true, 0x2040) && !ControlOff(true, 0x3840));
        CHECK(!ControlOff(true, 0x1AC0) && !ControlOff(true, 0x794));
        CHECK(!ControlOff(true, 0x3FD4));  // form inside a boss fight (17 + 9C + 9F): still gameplay (was counted as a menu before #183)
        CHECK(ControlOff(true, 0x3FD5) && ControlOff(true, 0x1AC7));  // a real control-off on top of play reasons still counts
        // any play-only combination of the 13 other bits stays off: no unknown play reason can arm the gate (#183)
        bool anyOn = false;
        for (uint32_t m = 0; m < 0x4000u; m += 2) anyOn |= ControlOff(true, m);
        CHECK(!anyOn);
    }
    {   // debounce: a short blip (< 120 ms) never arms; 120 ms continuous does; off ~150 ms after it clears
        Debounce d; uint64_t t = 1000; bool any = false;
        for (int i = 0; i < 12; ++i, t += 8) any |= d.Update(true, t);   // 96 ms
        CHECK(!any);
        for (int i = 0; i < 10; ++i, t += 8) any |= d.Update(false, t);
        CHECK(!any);
        for (int i = 0; i < 20; ++i, t += 8) any = d.Update(true, t);    // 160 ms
        CHECK(any);
        bool on = true; int n = 0;
        for (; on && n < 100; ++n, t += 8) on = d.Update(false, t);
        CHECK(n >= 18 && n <= 20);
    }
    {   // shrine case end to end: control-off on, menu getters off -> a held D-down repeats; field (aggregate 0) never
        Debounce d; State s; uint64_t t = 1000; uint32_t r = 0;
        for (int i = 0; i < 200; ++i, t += 8) r |= Step(s, kDDown, d.Update(ControlOff(true, 0x7), t), t, 1);
        CHECK(r == kDDown && s.repeats >= 8);
        State g; Debounce e; uint32_t q = 0;
        for (int i = 0; i < 200; ++i, t += 8) q |= Step(g, kDDown, e.Update(ControlOff(true, 0), t), t, 1);
        CHECK(q == 0);
    }
    {   // #183 end to end: after the death screen / in a fight / in a soul core form a play reason stays on (bit 0 clear) and the
        // menu signal and stop flags are off -> a spun left stick never becomes D-pad presses and a held D-pad never repeats
        const uint32_t masks[] = {0x1800, 0x2040, 0x3840, 0x1AC0, 0x794, 0x3FD4};
        for (uint32_t m : masks) {
            Debounce d; StickState ss; State s; uint64_t t = 1000; uint32_t dp = 0, rel = 0;
            const uint32_t dirs[] = {kSU, kSR, kSD, kSL};
            for (int i = 0; i < 400; ++i, t += 8) {
                const bool act = d.Update(ControlOff(true, m), t);
                dp |= StickStep(ss, dirs[(i / 8) % 4], 0, act, t, 1);
                rel |= Step(s, kDLeft, act, t, 1);
            }
            CHECK(dp == 0 && ss.pulses == 0 && rel == 0);
        }
        Debounce d; StickState ss; uint64_t t = 1000; uint32_t dp = 0;  // a real control-off menu (bit 0) still steps from the stick
        for (int i = 0; i < 100; ++i, t += 8) dp |= StickStep(ss, (i / 30) % 2 ? kSD : 0, 0, d.Update(ControlOff(true, 0x187), t), t, 1);
        CHECK(dp == kDDown && ss.pulses >= 1);
    }
    {   // #138311 left stick = D-pad; #138487: a fresh tilt steps at once, then nothing more before 400 ms, then ~1 press per period
        StickState s; uint64_t t = 1000; int edges = 0; bool prev = false; uint32_t any = 0;
        StickStep(s, 0, 0, true, t, 1); t += 8;                                            // menu open, stick centred
        CHECK(StickStep(s, kSD, 0, true, t, 1) == kDDown); t += 8;                         // instant first step
        int first = 0; bool pv = true;
        for (int i = 0; i < 49; ++i, t += 8) { const bool d = (StickStep(s, kSD, 0, true, t, 1) & kDDown) != 0; if (d && !pv) ++first; pv = d; }
        CHECK(first == 0 && s.pulses == 1);
        for (int i = 0; i < 113; ++i, t += 8) { const uint32_t o = StickStep(s, kSD, 0, true, t, 1); any |= o; const bool d = (o & kDDown) != 0; if (d && !prev) ++edges; prev = d; }
        CHECK(any == kDDown && edges >= 9 && edges <= 11 && s.pulses >= 10);
        StickState g; uint64_t u = 1000; uint32_t q = 0;
        for (int i = 0; i < 300; ++i, u += 8) q |= StickStep(g, kSU | kSL, 0, false, u, 1);
        CHECK(q == 0 && g.pulses == 0);
    }
    {   // a slow poller (100 ms) alternates pulse / release, never stuck pressed
        StickState s; uint64_t t = 1000; int edges = 0; bool prev = false, sawUp = false;
        for (int i = 0; i < 30; ++i, t += 100) { const bool d = (StickStep(s, kSR, 0, true, t, 1) & kDRight) != 0; if (d && !prev) ++edges; if (!d && prev) sawUp = true; prev = d; }
        CHECK(edges >= 8 && sawUp);
    }
    {   // a real D-pad of the same direction wins (no extra pulse); stick released = no pulse; menu reopen = full delay
        StickState s; uint64_t t = 1000; uint32_t any = 0;
        for (int i = 0; i < 200; ++i, t += 8) any |= StickStep(s, kSU, kDUp, true, t, 1);
        CHECK(any == 0);
        for (int i = 0; i < 100; ++i, t += 8) StickStep(s, kSU, 0, true, t, 1);
        CHECK(StickStep(s, 0, 0, true, t, 1) == 0);
        StickStep(s, kSU, 0, false, t += 8, 1);
        any = 0; for (int i = 0; i < 49; ++i, t += 8) any |= StickStep(s, kSU, 0, true, t, 1);
        CHECK(any == 0);
    }
    {   // #138487 flick up -> down without a centre pause: down steps on its first poll, no 400 ms gap
        StickState s; uint64_t t = 1000;
        StickStep(s, 0, 0, true, t, 1); t += 8;
        CHECK(StickStep(s, kSU, 0, true, t, 1) == kDUp); t += 8;
        for (int i = 0; i < 10; ++i, t += 8) StickStep(s, kSU, 0, true, t, 1);
        CHECK(StickStep(s, 0, 0, true, t, 1) == 0); t += 8;                                // passes the centre for one poll
        CHECK(StickStep(s, kSD, 0, true, t, 1) == kDDown); t += 8;
        for (int i = 0; i < 10; ++i, t += 8) StickStep(s, kSD, 0, true, t, 1);
        CHECK(StickStep(s, kSL, 0, true, t, 1) == kDLeft);                                 // axis change, no centre poll at all
        CHECK(s.pulses == 3);
    }
    {   // #138487 a stick held when the menu opens: no instant step, full delay; released and tilted again = instant
        StickState s; uint64_t t = 1000; uint32_t any = 0;
        for (int i = 0; i < 49; ++i, t += 8) any |= StickStep(s, kSR, 0, true, t, 1);
        CHECK(any == 0);
        StickStep(s, 0, 0, true, t += 8, 1);
        CHECK(StickStep(s, kSR, 0, true, t += 8, 1) == kDRight);
    }
    {   // #140978 the left stick is held back from the game only while the menu signal is on and game time runs
        CHECK(StickHeldBack(true, false));    // pause / inventory menu, game time running: the character could walk
        CHECK(!StickHeldBack(true, true));    // game time stopped (58B/58C): the player cannot move, analog screens keep the stick
        CHECK(!StickHeldBack(false, true));   // stop flags only
        CHECK(!StickHeldBack(false, false));  // gameplay, or control-off only (shrine / blacksmith: the game takes control itself)
    }
    {   // dominant axis: a mostly-down diagonal pulses only D-down
        CHECK(StickDominant(kSD | kSR, 22000, -30000) == kSD);
        CHECK(StickDominant(kSD | kSR, 30000, -22000) == kSR);
        CHECK(StickDominant(0, 30000, 0) == 0);
    }
    printf("%s: %d/%d checks\n", g_fail ? "FAIL" : "PASS", g_n - g_fail, g_n);
    return g_fail ? 1 : 0;
}
