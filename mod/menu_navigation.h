// Small, platform-independent helpers shared by the two independent mod menus.
// No game calls, row keys or feature state live here.
#pragma once

namespace menu_nav {
inline int Wrap(int index, int delta, int count) {
    if (count <= 0) return 0;
    const int next = (index + delta) % count;
    return next < 0 ? next + count : next;
}
inline int PageStep(int index, int delta, int count) {
    if (count <= 0) return 0;
    const int next = index + delta;
    return next < 0 ? 0 : next >= count ? count - 1 : next;
}

// A page remembers its cursor while moving to a child and back. Reopening resets
// to the root; one Back pops one level, and Back at the root asks the caller to close.
template <int MaxDepth = 8>
struct Trail {
    int page = 0;
    int depth = 0;
    int pages[MaxDepth] = {};
    int selections[MaxDepth] = {};
    void Reset() { page = depth = 0; }
    bool Enter(int child, int selection) {
        if (depth >= MaxDepth || child == page) return false;
        pages[depth] = page;
        selections[depth++] = selection;
        page = child;
        return true;
    }
    bool Back(int& selection) {
        if (!depth) return false;
        page = pages[--depth];
        selection = selections[depth];
        return true;
    }
};
}  // namespace menu_nav
