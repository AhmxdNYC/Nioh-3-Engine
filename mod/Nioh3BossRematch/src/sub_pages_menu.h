// sub_pages_menu.h - feature sub pages of the mod menu (TASK #137249, human #137196).
// MENU RULE (human #137196): a feature with 3 or more settings gets its own 'Feature' page, opened by one 'Feature   A: open' row
// with Back. A page never lists 3+ rows of one feature flat under a header. tests/sub_pages_offline.py enforces it.
// Moving rows here changes no ini key, default or behaviour: each page lists the same row ids its parent listed before.
// Included inside the rematch UI namespace after practice_stats_menu.h and soul_core_tuning_menu.h.
#pragma once

constexpr int uStyle = 960, uPlayerStats = 961, uBossKi = 962, uSoulCores = 963;
constexpr int kItSubOpenBase = 0xF1000;  // + SubPageId (0xF1000: no other range uses it)
enum SubPageId { kSubSoulCores, kSubStyle, kSubPlayerStats, kSubBossKi, kSubN };
struct SubPageDef { int page; const char* label; const char* title; const char* foot; };
const SubPageDef kSubPages[kSubN] = {
    {uSoulCores, "Soul cores", "Soul cores (live, saved)",
     "Soul core transformations on / off, each core's summon or Transform, equip a core, the overlay and its look, and Soul core tuning."},
    {uStyle, "Style meter", "Style meter (saved)",
     "Style meter on / off, always on, size, place, opacity, pop-ups, effects, ranks, drain and the points for each move type."},
    {uPlayerStats, "Player stats", "Player stats - Training / spawned Boss Matches",
     "Infinite Ki / ninjutsu / soul core, damage you deal, soul core damage, Ki damage you deal and your speed."},
    {uBossKi, "Boss damage and Ki", "Boss damage and Ki - Training / spawned Boss Matches",
     "Damage the boss deals, boss Ki drain, one hit Ki break, hold Ki break, Ki floor, Ki log, negative Ki meter and boss speed."},
};
bool IsSubOpenItem(int item) { return item >= kItSubOpenBase && item < kItSubOpenBase + kSubN; }
bool IsSubPage(int page) { for (const auto& s : kSubPages) if (s.page == page) return true; return false; }

// Open rows. A row is listed only when its page has rows (older Nioh3Training.asi: no style / stat rows).
bool SubPageHasRows(int which) {
    switch (which) {
    case kSubStyle: return TrainingExport("Nioh3Training_StyleSet") != nullptr;
    case kSubPlayerStats: case kSubBossKi: return PstAvailable();
#if BR_SOULCORE
    case kSubSoulCores: return true;
#endif
    default: return false;
    }
}
template <class Add> void SubAddOpenRow(Add&& add, int which) { if (SubPageHasRows(which)) add(kItSubOpenBase + which); }

// The rows of the open sub page. false = the current page is not a sub page.
template <class Add> bool SubAddPage(Add&& add) {
    const int p = g_uiPages.page;
    if (p == uStyle) { PstAddStyleRows(add); return true; }
    if (p == uPlayerStats) { PstAddPlayerStatRows(add); return true; }
    if (p == uBossKi) { PstAddBossRows(add); return true; }
#if BR_SOULCORE
    if (p == uSoulCores) {
        add(kItScForms);  // human #61869: Soul core transformations ON / OFF
        SctAddCoreRows(add);  // human #81220: per-core transformation ON / OFF
        ScEqAddRows(add);  // human #129277: overlay, overlay look, Equip core slot / Equip core
        SctAddHubRow(add);  // Soul core tuning
        return true;
    }
#endif
    return false;
}
bool SubOpenChange(int item, int dir) {
    if (!IsSubOpenItem(item)) return false;
    if (dir) return true;  // the open row has no value
    const SubPageDef& s = kSubPages[item - kItSubOpenBase];
    g_uiPages.Enter(s.page, g_sel);
    g_sel = 0; g_msg[0] = 0; g_confirm = -9;
    Log("Rematch: %s page opened (TASK #137249)", s.label);
    return true;
}
bool SubOpenRowText(int item, bool sel, const char* mk, char* buf, size_t n, uint32_t* color) {
    if (!IsSubOpenItem(item)) return false;
    snprintf(buf, n, "%s %s   A: open", mk, kSubPages[item - kItSubOpenBase].label);
    *color = sel ? cWhite : cGray;
    return true;
}
const char* SubOpenFooter(int item) { return IsSubOpenItem(item) ? kSubPages[item - kItSubOpenBase].foot : nullptr; }
const char* SubPageTitle(int page) { for (const auto& s : kSubPages) if (s.page == page) return s.title; return nullptr; }
// Section header on a sub page: its feature name (the parent page header moved with the rows).
const char* SubPageGroup(int page, int item) {
    (void)item;
    if (page == uSoulCores) return "Soul cores";
    if (page == uStyle) return "Style meter";
    if (page == uPlayerStats) return "Player stats (Training / spawned Boss Matches)";
    if (page == uBossKi) return "Boss settings (see each setting for mode)";
    return nullptr;
}
