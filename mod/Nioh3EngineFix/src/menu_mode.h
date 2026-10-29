#pragma once
namespace menu_mode {
// An explicit menu choice can precede starting a session. Keep that page until
// the live session mode changes; a real mode transition always takes priority.
struct Selection {
    int selected = 0;
    int observed = 0;
    void Choose(int page, int live) { selected = page; observed = live; }
    int Reopen(int live) {
        if (live != observed) { selected = 0; observed = live; }
        return selected == 1 || selected == 2 ? selected : live;
    }
};
}
