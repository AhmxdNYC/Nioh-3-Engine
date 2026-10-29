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
    // user #22918: damage cut (any session) + Boss Match refill of a living player
    assert(PhDamageCut(1, 1, 1, 4000) == 0 /* one-hit lethal damage never lands */ && PhDamageCut(1, 1, 0, 4000) == 4000 /* boss HP untouched */ && PhDamageCut(0, 1, 1, 4000) == 4000 && PhDamageCut(1, 0, 1, 4000) == 4000 && PhDamageCut(1, 1, 1, -5) == -5);
    assert(PhMatchRefill(1, 1, M - 1, M) && PhMatchRefill(1, 1, 1, M) && !PhMatchRefill(1, 1, M, M) && !PhMatchRefill(1, 1, 0, M) /* dead: not revived here */ && !PhMatchRefill(0, 1, 5, M) && !PhMatchRefill(1, 0, 5, M) && !PhMatchRefill(1, 1, 5, 0));
    puts("PASS: infinite health ON refills any lost HP (never beaten); OFF keeps damage and respawns at the HP floor (#14267)");
    return 0;
}
