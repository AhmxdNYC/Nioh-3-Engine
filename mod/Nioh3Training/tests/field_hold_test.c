// #89181: open-world teleport waits for the destination cell mark. Offline.
#include <stdio.h>
#undef NDEBUG
#include <assert.h>
#include "../src/field_hold.h"
int main(void) {
    assert(FieldHoldStep(0, 497187, 0, 100, 0) == FH_HOLD);
    assert(FieldHoldStep(8000, 497187, 0, 100, 0) == FH_HOLD);          // in the set, no '*'
    assert(FieldHoldStep(8000, 100, 1, 100, 5000) == FH_HOLD);           // star on the cell we left
    assert(FieldHoldStep(8000, 497187, 1, 100, 499) == FH_HOLD);         // mark just flipped
    assert(FieldHoldStep(8000, 497187, 1, 100, 500) == FH_SUMMON);
    assert(FieldHoldStep(20000, 497187, 0, 100, 0) == FH_HOME);
    assert(FieldHoldStep(20000, 497187, 1, 100, 500) == FH_SUMMON);      // a ready cell still summons at the cap
    assert(FieldHoldStep(19999, 0, 0, 100, 0) == FH_HOLD);
    puts("PASS: field hold waits for a new cell's star, then summons; 20 s without it goes home");
    return 0;
}
