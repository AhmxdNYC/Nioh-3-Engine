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
    {   // #137478 control-off gate: unreadable or zero = off; nonzero aggregate = on
        CHECK(!ControlOff(false, 0x40));
        CHECK(!ControlOff(true, 0));
        CHECK(ControlOff(true, 0x40));
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
        for (int i = 0; i < 200; ++i, t += 8) r |= Step(s, kDDown, d.Update(ControlOff(true, 0x40), t), t, 1);
        CHECK(r == kDDown && s.repeats >= 8);
        State g; Debounce e; uint32_t q = 0;
        for (int i = 0; i < 200; ++i, t += 8) q |= Step(g, kDDown, e.Update(ControlOff(true, 0), t), t, 1);
        CHECK(q == 0);
    }
    printf("%s: %d/%d checks\n", g_fail ? "FAIL" : "PASS", g_n - g_fail, g_n);
    return g_fail ? 1 : 0;
}
