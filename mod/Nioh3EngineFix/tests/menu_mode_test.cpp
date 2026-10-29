#include "../src/menu_mode.h"
#include <cassert>
#include <cstdio>
int main() {
    menu_mode::Selection s;
    assert(s.Reopen(0) == 0);
    assert(s.Reopen(1) == 1);
    s.Choose(2, 1); // switch the menu to Training before starting a new session
    assert(s.Reopen(1) == 2);
    assert(s.Reopen(1) == 2); // closing/reopening again must not revert to Boss Matches
    assert(s.Reopen(2) == 2); // actual Training session starts
    assert(s.Reopen(1) == 1); // later Boss Match must override the old menu choice
    s.Choose(1, 2);
    assert(s.Reopen(2) == 1); // reverse explicit switch
    assert(s.Reopen(0) == 0); // session ended: first page, not a stale mode
    s.Choose(2, 0);
    assert(s.Reopen(0) == 2); // Training selected while no boss is spawned
    assert(s.Reopen(1) == 1); // new actual match takes priority
    puts("PASS: 10 current-mode reopen transition checks");
}
