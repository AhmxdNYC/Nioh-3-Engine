// user #14150 / #14267: Infinite health decision (player_hp.h), offline.
#include <stdint.h>
#include <stdio.h>
#undef NDEBUG
#include <assert.h>
#include "../src/player_hp.h"
int main(void) {
    const uint64_t M = 2658;
    // ON (+ session NoDeath): any lost HP is refilled at once, never 'beaten'
    assert(PhDecide(1, 1, 0, 0, M, M) == PH_NONE);
    assert(PhDecide(1, 1, 0, 0, M - 1, M) == PH_REFILL);
    assert(PhDecide(1, 1, 0, 0, 1, M) == PH_REFILL);   // NoDeath clamp (HP 1) = refill, not a respawn
    assert(PhDecide(1, 1, 0, 0, 0, M) == PH_REFILL);
    // OFF: damage stays; the floor (NoDeath keeps 1 HP; <= 1 % of max) = beaten -> respawn in the arena
    assert(PhDecide(0, 1, 0, 0, M - 500, M) == PH_NONE);
    assert(PhDecide(0, 1, 0, 0, 27, M) == PH_NONE);     // 27 > 1 % of 2658
    assert(PhDecide(0, 1, 0, 0, 26, M) == PH_BEATEN);
    assert(PhDecide(0, 1, 0, 0, 1, M) == PH_BEATEN);    // 19:44:51 'player beaten (HP 1 / 2658)'
    assert(PhDecide(0, 1, 0, 0, 0, M) == PH_BEATEN);
    // already beaten / lost / unreadable max: nothing new
    assert(PhDecide(0, 1, 1, 0, 1, M) == PH_NONE && PhDecide(0, 1, 0, 1, 1, M) == PH_NONE && PhDecide(1, 1, 0, 0, 1, 0) == PH_NONE);
    // ON without any NoDeath (SessionNoDeath 0 + PlayerNoDeath 0): old floor rule (the game may kill first)
    assert(PhDecide(1, 0, 0, 0, M - 1, M) == PH_NONE && PhDecide(1, 0, 0, 0, 1, M) == PH_BEATEN);
    puts("PASS: infinite health ON refills any lost HP (never beaten); OFF keeps damage and respawns at the HP floor (#14267)");
    return 0;
}
