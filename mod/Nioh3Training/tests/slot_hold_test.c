// user #14462 / #14304: slot idle hold decisions (src/slot_hold.h), offline.
// Live 22:35:55-22:42: Boss A summoned from the menu, sequence 0x4E21 > 0x4E22 picked -> 'SUMPASSIVE slot A: AI on (repeat move picked)'
// -> the boss fought with its own AI (random moves). Now a repeat keeps the hold; only Fight normally / session end release it.
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#undef NDEBUG
#include <assert.h>
#include "../src/slot_hold.h"
int main(void) {
    // Training hold (idle by default): a repeat is no release reason any more; Fight normally and the session end are
    assert(SphReleaseWhy(SPH_TRAINING, 19250, 0, 0, 0) == 0);          // the 22:35 case: summoned, repeat picked, no session
    assert(SphReleaseWhy(SPH_TRAINING, 999999, 1, 1, 0) == 0);         // in a session, held for ever until the user picks a stance
    assert(!strcmp(SphReleaseWhy(SPH_TRAINING, 10, 0, 0, 1), "this boss set to Fight normally"));
    assert(!strcmp(SphReleaseWhy(SPH_TRAINING, 10, 1, 0, 0), "Training session ended"));
    assert(SphReleaseWhy(SPH_TRAINING, 10, 0, 0, 0) == 0);            // summoned outside a session: no session to end
    // the old SummonPassiveMs timer hold is unchanged
    assert(SphReleaseWhy(3000, 2999, 0, 0, 0) == 0);
    assert(!strcmp(SphReleaseWhy(3000, 3000, 0, 0, 0), "passive time over"));
    // AI while held: off unless the aggro walk-up runs; unreadable AI flag = leave it
    assert(SphAiAction(0, 1) == 0);    // the game switched it on: hold again
    assert(SphAiAction(0, 0) == -1);
    assert(SphAiAction(1, 0) == 1);    // walk-up: AI on (locomotion only via g_mv_chase)
    assert(SphAiAction(1, 1) == -1);
    assert(SphAiAction(1, -1) == -1 && SphAiAction(0, -1) == -1);
    // walk-up start / end (range 500 cm): start only beyond range + 1 m and not mid-move / mid-sequence, end inside the range
    assert(!SphChaseWanted(0, 550.0f, 500.0f, 0));   // hysteresis band: no start
    assert(SphChaseWanted(0, 650.0f, 500.0f, 0));
    assert(!SphChaseWanted(0, 900.0f, 500.0f, 1));   // the sequence / a move still plays: never cut it
    assert(SphChaseWanted(1, 550.0f, 500.0f, 1));    // already walking: keeps walking until in range
    assert(!SphChaseWanted(1, 480.0f, 500.0f, 0));   // arrived: AI held again, the repeat goes on
    // a full round: summoned far away (9 m), sequence picked, aggro ON
    int chase = 0, ai = 0;  // AI held
    const float path[] = {900, 820, 700, 600, 520, 470, 470, 470};
    int on = 0, off = 0;
    for (unsigned k = 0; k < sizeof path / sizeof path[0]; ++k) {
        chase = SphChaseWanted(chase, path[k], 500.0f, 0);
        const int a = SphAiAction(chase, ai);
        if (a == 1) { ai = 1; ++on; } else if (a == 0) { ai = 0; ++off; }
        assert(SphReleaseWhy(SPH_TRAINING, 1000 * k, 1, 1, 0) == 0);  // never released by the repeat
    }
    assert(on == 1 && off == 1 && ai == 0 && chase == 0);
    puts("PASS: slot hold (#14462): a repeat keeps a summoned boss's AI held (no 'repeat move picked' release), Fight normally / session end release it, aggro walk-up switches the AI on beyond range + 1 m and off in range");
    return 0;
}
