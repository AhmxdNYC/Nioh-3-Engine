// user #13796 (shot-13796.png, live log 23:25:17-23:26:30): the Return object is the game's own mission clear gadget (type 0x353). Its native
// interact opens 'This will end the mission. Proceed?'. None of the game-side blockers (12A1F80 / 1DAE4E0 / 1DB58B0 / 348274) ever fired in
// any log, so hiding the pad buttons from the game was the only thing that kept the native box away. With the boss alive (Practice /
// Stand still, ReturnGateAlways) that mask covered only ReturnGateRadius 110 u, less than the native interact reach (the idle path uses
// ReturnGateMaskRadius 400 u, 5/5 ours-only in selftest 10106). An A at the edge reached the game and the native box opened. The next A
// (meant to answer it) fell inside 110 u: we hid it from the game and our raw-A read opened our page on top, every time = stuck.
//
// One gate zone for every mode now (pure logic here, unit-tested in tests/gate_zone_test.c):
//  - inside the zone (or while our page is open, or holdMs after leaving it) the face buttons are hidden from the game (GZ_MASK);
//  - a TAP of an open button (press inside the zone, released within tapMs) opens our page (GZ_OPEN) on the release;
//    a longer hold does nothing (GZ_HOLD_CANCEL) and the game never sees it (EngineFix keeps a masked held button hidden until released);
//  - safety net: while a native modal (end-mission box) is up, nothing is masked and nothing is opened (GZ_NATIVE = it just appeared),
//    so the player can always answer the game's box; the caller also closes our menu then.
#pragma once
#include <stdint.h>

enum { GZ_MASK = 1, GZ_OPEN = 2, GZ_HOLD_CANCEL = 4, GZ_NATIVE = 8 };
typedef struct { int armed, bWas, nativeWas; uint32_t tDown, tNear, tOpen; } GzState;

static int GzStep(GzState* s, uint32_t now, int inZone, int page, int nativeOpen, int btn, int tapMs, int holdMs) {
    int out = 0;
    if (nativeOpen) {
        if (!s->nativeWas) out |= GZ_NATIVE;
        s->nativeWas = 1; s->armed = 0; s->tNear = 0; s->bWas = btn;
        return out;
    }
    s->nativeWas = 0;
    if (inZone || page) s->tNear = now ? now : 1;
    if (inZone || page || (s->tNear && now - s->tNear < (uint32_t)holdMs)) out |= GZ_MASK;
    if (btn && !s->bWas) { s->armed = inZone && !page; s->tDown = now; }
    else if (btn && s->armed && now - s->tDown > (uint32_t)tapMs) { s->armed = 0; out |= GZ_HOLD_CANCEL; }
    else if (!btn && s->bWas && s->armed) {
        s->armed = 0;
        if (!page && (!s->tOpen || now - s->tOpen > 800)) { s->tOpen = now ? now : 1; out |= GZ_OPEN; }
    }
    s->bWas = btn;
    return out;
}
