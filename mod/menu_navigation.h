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
// #44353 tab bar: one row of tab names, the current one as "[name]", the others as " name ", one column apart. When the row
// is wider than `cols` it shows a window that keeps the current tab and as many neighbours as fit (right first), with "<" / ">"
// marking hidden tabs. Fills out[] (columns from 0) and returns the number of tabs placed.
struct TabSeg { int tab; int col; int len; };
inline int TabBar(const int* lens, int n, int cur, int cols, TabSeg* out, bool& moreLeft, bool& moreRight) {
    moreLeft = moreRight = false;
    if (n <= 0 || cols <= 0) return 0;
    if (cur < 0 || cur >= n) cur = 0;
    auto width = [&](int a, int b) { int w = 0; for (int i = a; i <= b; ++i) w += lens[i] + 2 + (i > a); return w; };
    int a = 0, b = n - 1;
    if (width(a, b) > cols) {
        a = b = cur;
        auto fits = [&](int l, int r) { return width(l, r) + (l > 0 ? 2 : 0) + (r < n - 1 ? 2 : 0) <= cols; };
        for (bool grew = true; grew;) {
            grew = false;
            if (b + 1 < n && fits(a, b + 1)) { ++b; grew = true; }
            if (a > 0 && fits(a - 1, b)) { --a; grew = true; }
        }
        moreLeft = a > 0;
        moreRight = b < n - 1;
    }
    int col = moreLeft ? 2 : 0, k = 0;
    for (int i = a; i <= b; ++i) {
        out[k++] = {i, col, lens[i] + 2};
        col += lens[i] + 3;
    }
    return k;
}
}  // namespace menu_nav
