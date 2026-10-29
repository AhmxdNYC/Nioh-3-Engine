// user #13796: Return object gate zone (gate_zone.h), synthetic input, no game access.
#include <stdio.h>
#undef NDEBUG
#include <assert.h>
#include "../src/gate_zone.h"

int main(void) {
    GzState s = {0}; int r;
    // walking in: mask from the first frame inside the zone, nothing opens without a button
    r = GzStep(&s, 1000, 1, 0, 0, 0, 350, 300); assert(r == GZ_MASK);
    // tap A (press + release 120 ms): opens on the release, masked the whole time
    r = GzStep(&s, 1010, 1, 0, 0, 1, 350, 300); assert(r == GZ_MASK);
    r = GzStep(&s, 1130, 1, 0, 0, 0, 350, 300); assert(r == (GZ_MASK | GZ_OPEN));
    // our page is open: A presses belong to our menu, never re-open; mask stays on
    r = GzStep(&s, 1500, 1, 1, 0, 1, 350, 300); assert(r == GZ_MASK);
    r = GzStep(&s, 1600, 1, 1, 0, 0, 350, 300); assert(r == GZ_MASK);
    // B closed our page with the button still down: its release must not open the page again
    r = GzStep(&s, 2000, 1, 1, 0, 1, 350, 300); r = GzStep(&s, 2050, 1, 0, 0, 1, 350, 300); r = GzStep(&s, 2100, 1, 0, 0, 0, 350, 300);
    assert(!(r & GZ_OPEN));
    // hold A: cancelled after tapMs, the release opens nothing
    r = GzStep(&s, 3000, 1, 0, 0, 1, 350, 300); assert(r == GZ_MASK);
    r = GzStep(&s, 3400, 1, 0, 0, 1, 350, 300); assert(r == (GZ_MASK | GZ_HOLD_CANCEL));
    r = GzStep(&s, 3800, 1, 0, 0, 0, 350, 300); assert(r == GZ_MASK);
    // press outside the zone: no open, no mask after holdMs
    r = GzStep(&s, 5000, 0, 0, 0, 1, 350, 300); assert(r == 0);
    r = GzStep(&s, 5100, 0, 0, 0, 0, 350, 300); assert(r == 0);
    // leaving the zone keeps the mask holdMs (no flicker gap), then drops it
    r = GzStep(&s, 6000, 1, 0, 0, 0, 350, 300); assert(r == GZ_MASK);
    r = GzStep(&s, 6200, 0, 0, 0, 0, 350, 300); assert(r == GZ_MASK);
    r = GzStep(&s, 6400, 0, 0, 0, 0, 350, 300); assert(r == 0);
    // safety net: a native modal appears while at the gate with our page open -> GZ_NATIVE once, no mask, no open,
    // and every A (answering the native box) passes to the game without re-opening our page
    r = GzStep(&s, 7000, 1, 1, 1, 0, 350, 300); assert(r == GZ_NATIVE);
    r = GzStep(&s, 7050, 1, 0, 1, 1, 350, 300); assert(r == 0);
    r = GzStep(&s, 7150, 1, 0, 1, 0, 350, 300); assert(r == 0);
    r = GzStep(&s, 7200, 1, 0, 1, 1, 350, 300); assert(r == 0);
    // box answered and gone with A still down: its release does not open our page
    r = GzStep(&s, 7300, 1, 0, 0, 1, 350, 300); assert(r == GZ_MASK);
    r = GzStep(&s, 7350, 1, 0, 0, 0, 350, 300); assert(r == GZ_MASK);
    // next clean tap works again
    r = GzStep(&s, 9000, 1, 0, 0, 1, 350, 300); r = GzStep(&s, 9100, 1, 0, 0, 0, 350, 300); assert(r == (GZ_MASK | GZ_OPEN));
    // double tap within 800 ms opens once
    r = GzStep(&s, 9200, 1, 0, 0, 1, 350, 300); r = GzStep(&s, 9300, 1, 0, 0, 0, 350, 300); assert(!(r & GZ_OPEN));
    puts("gate_zone_test: ok");
    return 0;
}
