// human #58117: 'Soul core tuning' page (LEFT soul core form start / exit look). Included by rematch.cpp inside its UI namespace right after
// practice_stats_menu.h (same helpers: g_msg, g_sel, g_uiPages, Log, colours). Game side: bosslog::SoulTune (bosslog.cpp) = the live globals the
// effect reads at use time (next LEFT use picks a change up) + WritePrivateProfileStringW to Nioh3BossRematch.ini [Spirit].
//   Player > Soul core tuning: A opens the page; on it Left / Right (A = Right) step the value.
constexpr int uSoulTune = 950;
constexpr int kItSctOpen = 0xE0000, kItSctBase = 0xE0010, kSctN = 22;  // TASK #136502: +1 Fast summon speed row (21); TASK #136492: +1 move sensitivity row (20); TASK #136113: +1 after-last-projectile row (19); TASK #135613: +4 exit rows (15-18);  // human #128855: +3 throw layers; #129070: row 14 = start + end speed together (menu-only, steps 11 and 6);  // kItSctBase + bosslog::SoulTune 'which' (0xE0000: no other range uses it)
bool IsSctItem(int item) { return item == kItSctOpen || (item >= kItSctBase && item < kItSctBase + kSctN); }
// human #61869: Player settings row 'Soul core transformations: ON/OFF' (bosslog::SoulCoreFormsSet, [Spirit] SoulCoreForms, default ON).
// OFF = the game's own soul cores, as if the form code were not there; live from the next use (a form already up ends normally).
constexpr int kItScForms = 0xE0100;
template <class Add> void SctAddHubRow(Add&& add) { add(kItSctOpen); }
// human #81220: per-core 'Soul transformation' rows (cores seen so far, id + d-pad direction): A / Left / Right flips transform ON / OFF
// (OFF = the game's own summon for that core). [Spirit] FormCoresOff.
constexpr int kItScCore = 0xE0200, kScCoreN = 12;  // two equipped + eight learned IDs + two pending unknown seals
bool IsScCoreItem(int item) { return item >= kItScCore && item < kItScCore + kScCoreN; }
template <class Add> void SctAddPage(Add&& add) { for (int i = 0; i < kSctN; ++i) add(kItSctBase + i); }
// Two selectable seal placeholders exist immediately; learning replaces them with rows keyed by core ID.
template <class Add> void SctAddCoreRows(Add&& add) {
    uint32_t ids[kScCoreN]; int dirs[kScCoreN];
    const int n = bosslog::FormCoreSeen(ids, dirs, kScCoreN);
    for (int i = 0; i < n; ++i) add(kItScCore + i);
}
int ScCoreAt(int slot, uint32_t* id, int* dir) {
    uint32_t ids[kScCoreN]; int dirs[kScCoreN];
    const int n = bosslog::FormCoreSeen(ids, dirs, kScCoreN);
    if (slot < 0 || slot >= n) return 0;
    *id = ids[slot]; *dir = dirs[slot];
    return 1;
}
const char* const kSctLabel[kSctN] = {"Player reappears after:", "Exit smoke forward:", "Exit smoke sideways:", "Exit smoke repeat after:",
                                      "Player hides after:", "Transform speed:", "Exit tail speed:", "Freeze start smoke:", "Freeze end smoke:", "Exit after last hit starts:",
                                      "Crouch cancel window:", "Core toss speed:", "Toss speed-up max time:", "Exit speed blend time:", "Start + end speed:",
                                      "Hidden yokai exit:", "Exit no earlier than:", "Exit after a projectile:", "Exit at % of attack:", "After last projectile:", "Move sensitivity:", "Fast summon speed:"};  // TASK #136502  // TASK #136492  // TASK #136113  // TASK #135613  // human #122288, #128855, #129054 plain names
const double kSctDefault[kSctN] = {100, 100, 0, 0, 400, 1.6, 2, 0, 0, 0, 200, 8, 1200, 250, 0, 1, 0, 0, 80, 700, 5, 0};
// human #129070: row 14 has no own value; it steps Core toss speed (11) and Exit tail speed (6) one notch each, same direction.
constexpr int kSctBoth = 14;
void SctBothText(bool* live, bool* changed, char* b, size_t n) {
    bool l1 = true, l2 = true;
    const double toss = bosslog::SoulTune(11, 0, &l1), tail = bosslog::SoulTune(6, 0, &l2);
    if (live) *live = l1 && l2;
    if (changed) *changed = toss > kSctDefault[11] + 1e-6 || toss < kSctDefault[11] - 1e-6 || tail > kSctDefault[6] + 1e-6 || tail < kSctDefault[6] - 1e-6;
    snprintf(b, n, "toss x%d, exit x%.1f", toss <= 1 ? 1 : (int)toss, tail);
}
void SctValueText(int w, double v, char* b, size_t n) {
    if (w == 9 && v == -9999) snprintf(b, n, "Legacy close");
    else if (w == 10 && v <= 0) snprintf(b, n, "OFF");  // human #122288
    else if (w == 7 || w == 8 || w == 15) snprintf(b, n, "%s", v > 0 ? "ON" : "OFF");
    else if ((w == 17 && v < 0) || (w == 18 && v <= 0) || (w == 19 && v < 0)) snprintf(b, n, "OFF");  // TASK #135613 / #136113
    else if (w == 18) snprintf(b, n, "%d %%", (int)v);
    else if (w == 5 || w == 6) snprintf(b, n, "x%.1f", v);
    else if (w == 20) { if (v <= 1) snprintf(b, n, "x1 (game)"); else snprintf(b, n, "x%d", (int)v); }  // TASK #136492
    else if (w == 21) { if (v <= 0) snprintf(b, n, "= Transform"); else snprintf(b, n, "x%.1f", v); }  // TASK #136502
    else if (w == 11) { if (v <= 1) snprintf(b, n, "x1 (off)"); else snprintf(b, n, "x%d", (int)v); }
    else if (w == 12 && v <= 0) snprintf(b, n, "OFF");
    else if (w == 1 || w == 2) snprintf(b, n, "%+d", (int)v);
    else if (w == 3 && v <= 0) snprintf(b, n, "OFF (once)");
    else snprintf(b, n, "%d ms", (int)v);
}
// human #129277: 'Equip core' rows (Player > Soul cores). Nioh3Training.asi runs the shrine's own equip order on the game thread
// (Nioh3Training_SoulCoreEquip / SoulCoreEquipItem, soul_core_swap_game.h); these rows only pick a slot and a core and send one request on A.
// TASK #136129: when SoulCoreCatalog is present the picker is every soul core in the item master, not only ones this save owns.
#include "../../Nioh3Training/src/soul_core_swap.h"
constexpr int kItScEqSlot = 0xE0300, kItScEqCore = 0xE0301, kItScOverlay = 0xE0302;
bool IsScEqItem(int item) { return item == kItScEqSlot || item == kItScEqCore || item == kItScOverlay; }
// TASK #137000 (human #136998 / #137004): small bottom-left gameplay overlay, one line per Yang slot 'Core <item> | S / T / TM'
// (S = the game's own summon or Fast summon, T = Transform, TM = Transform whose yokai the detection has seen move). [Menu] SoulCoreOverlay.
std::atomic<int> g_scOverlay{-1};
bool ScOverlayOn() { int v = g_scOverlay.load(); if (v < 0) { v = IniInt(L"Menu", L"SoulCoreOverlay", 1) ? 1 : 0; g_scOverlay.store(v); } return v != 0; }
const char* ScOverlayTag(uint32_t chr, bool formsOn) {
    if (!chr) return "?";
    const int c = bosslog::FormCoreCallGet(chr);
    if (!formsOn || sc_call_game_path(c)) return "S";
    return bosslog::FormCoreMoved(chr) ? "TM" : "T";
}
// TASK #137007 (human #137004): the overlay's look, on its own page (Player > Soul core overlay look), styled like the style meter rows.
// [Menu] SoulCoreOverlay* keys, live (the overlay thread reads them every repaint) and saved.
constexpr int uScOverlay = 951;
constexpr int kItScOvOpen = 0xE0303, kItScOvBase = 0xE0310, kScOvN = 7;
enum { kScOvSize = 0, kScOvX, kScOvY, kScOvAlpha, kScOvBox, kScOvColors, kScOvFont };
struct ScOvRow { const char* label; const wchar_t* key; int def, kind, lo, hi; int steps[12]; const char* foot; };  // kind 0 on/off, 1 %, 4 per-mille (1% steps), 5 choice
const ScOvRow kScOv[kScOvN] = {
    {"Overlay size:", L"SoulCoreOverlaySize", 100, 1, 50, 200, {50, 60, 75, 85, 100, 115, 130, 150, 175, 200, -1}, "Size of the soul core overlay text. 100% = the mod menu's text size. Applies at once. Saved."},
    {"Overlay left/right:", L"SoulCoreOverlayX", 15, 4, 0, 1000, {}, "Where the overlay sits across the screen (its left edge): 0% = left edge, 100% = right edge. 1% steps. Applies at once. Saved."},
    {"Overlay up/down:", L"SoulCoreOverlayY", 970, 4, 0, 1000, {}, "Where the overlay sits down the screen (its bottom edge): 0% = top, 100% = bottom. 1% steps. Applies at once. Saved."},
    {"Overlay opacity:", L"SoulCoreOverlayOpacity", 90, 1, 20, 100, {20, 30, 40, 50, 60, 70, 80, 90, 100, -1}, "How see-through the whole overlay is. 100% = solid. Applies at once. Saved."},
    {"Overlay background:", L"SoulCoreOverlayBox", 1, 0, 0, 1, {}, "ON = text on a dark panel. OFF = outlined text only, nothing behind it (like the style meter). Applies at once. Saved."},
    {"Overlay colors:", L"SoulCoreOverlayColors", 0, 5, 0, 2, {}, "Menu = the mod menu's white / green / gray. Style meter = the style meter's purple. Gold = warm gold. S lines are dimmer, TM lines brightest. Applies at once. Saved."},
    {"Overlay font:", L"SoulCoreOverlayFont", 0, 5, 0, 2, {}, "Letter style: Normal (the menu's), Wide (more space between letters) or Bold (heavier strokes). Applies at once. Saved."},
};
const char* const kScOvChoice[2][3] = {{"Menu", "Style meter", "Gold"}, {"Normal", "Wide", "Bold"}};
std::atomic<int> g_scOv[kScOvN];  // value + 1 (0 = not read from the ini yet)
bool IsScOvItem(int item) { return item == kItScOvOpen || (item >= kItScOvBase && item < kItScOvBase + kScOvN); }
int ScOvClamp(int w, int v) { return v < kScOv[w].lo ? kScOv[w].lo : v > kScOv[w].hi ? kScOv[w].hi : v; }
int ScOvGet(int w) {
    if (w < 0 || w >= kScOvN) return 0;
    int v = g_scOv[w].load();
    if (!v) { v = ScOvClamp(w, IniInt(L"Menu", kScOv[w].key, kScOv[w].def)) + 1; g_scOv[w].store(v); }
    return v - 1;
}
int ScOvSet(int w, int v) {
    v = ScOvClamp(w, v);
    g_scOv[w].store(v + 1);
    wchar_t t[16]; swprintf(t, 16, L"%d", v);
    WritePrivateProfileStringW(L"Menu", kScOv[w].key, t, IniPath());
    return v;
}
void ScOvReset() { for (int w = 0; w < kScOvN; ++w) ScOvSet(w, kScOv[w].def); }
void ScOvPageReset() { ScOvReset(); g_scOverlay.store(1); WritePrivateProfileStringW(L"Menu", L"SoulCoreOverlay", L"1", IniPath()); }
int ScOvStep(int w, int v, int dir) {
    const int k = kScOv[w].kind;
    if (k == 0) return dir > 0 ? 1 : dir < 0 ? 0 : !v;
    if (k == 5) { const int span = kScOv[w].hi - kScOv[w].lo + 1, d = dir < 0 ? -1 : 1; return kScOv[w].lo + ((v - kScOv[w].lo + d) % span + span) % span; }
    if (k == 4) { const int x = (v + 5) / 10 * 10 + (dir < 0 ? -10 : 10); return ScOvClamp(w, x); }
    const int* st = kScOv[w].steps; int cnt = 0; while (cnt < 12 && st[cnt] >= 0) ++cnt;
    if (dir >= 0) { for (int i = 0; i < cnt; ++i) if (st[i] > v) return st[i]; return dir ? st[cnt - 1] : st[0]; }  // A past the top wraps
    for (int i = cnt - 1; i >= 0; --i) if (st[i] < v) return st[i];
    return st[0];
}
void ScOvValueText(int w, int v, char* b, size_t n) {
    const int k = kScOv[w].kind;
    if (k == 0) snprintf(b, n, "%s", v ? "ON" : "OFF");
    else if (k == 5) snprintf(b, n, "%s", kScOvChoice[w == kScOvFont][v < 0 ? 0 : v > 2 ? 2 : v]);
    else if (k == 4) snprintf(b, n, "%d.%d %%", v / 10, v % 10);
    else snprintf(b, n, "%d %%", v);
}
// Overlay colours (COLORREF): [theme][0 = S, 1 = T, 2 = TM, 3 = panel]
const uint32_t kScOvTheme[3][4] = {
    {0xB4B4B4, 0xFFFFFF, 0x6CFF6C, 0x181210},   // Menu: the mod menu's gray / white / green on its dark panel
    {0xBE8CA0, 0xFFE1EB, 0xFF78D6, 0x260E1A},   // Style meter: muted / pale / bright purple on dark purple
    {0x6E96AA, 0x96E1FF, 0x3CBEFF, 0x081218},   // Gold
};
int g_scEqSlot = 0;
uint32_t g_scEqPick = SCW_EMPTY;
int g_scEqSeq = -1;  // finished-request count when our request was sent (-1 = none outstanding)
struct ScEqState { uint32_t slots[SCW_NSLOT]; uint32_t idx[64]; uint16_t item[64]; int n; };
bool ScEqRead(ScEqState* st) {
    using F = int (*)(uint32_t*, uint32_t*, uint16_t*, int);
    const auto f = (F)TrainingExport("Nioh3Training_SoulCoreState");
    st->n = f ? f(st->slots, st->idx, st->item, 64) : -1;
    return st->n >= 0;
}
template <class Add> void ScEqAddRows(Add&& add) {
    add(kItScOverlay);  // TASK #137000
    add(kItScOvOpen);  // TASK #137007: overlay look page
    if (!TrainingExport("Nioh3Training_SoulCoreEquip")) return;  // older Nioh3Training.asi: no rows
    add(kItScEqSlot); add(kItScEqCore);
}
int ScEqItemOf(const ScEqState& st, uint32_t idx) { for (int i = 0; i < st.n; ++i) if (st.idx[i] == idx) return st.item[i]; return -1; }
struct ScCat { uint16_t ids[SCW_MAX_IDS]; int n; uint16_t slotItem[SCW_NSLOT]; };
bool ScCatRead(ScCat* c) {
    using F = int (*)(uint16_t*, int, uint16_t*);
    const auto f = (F)TrainingExport("Nioh3Training_SoulCoreCatalog");
    if (!f) return false;
    const int n = f(c->ids, SCW_MAX_IDS, c->slotItem);
    if (n <= 0) return false;
    c->n = n > SCW_MAX_IDS ? SCW_MAX_IDS : n;
    return true;
}
// Item id of the core in Yang slot y (0/1), 0xFFFF = empty or not readable.
uint16_t ScSlotItem(int y) {
    ScCat cat;
    if (ScCatRead(&cat)) return cat.slotItem[y];
    ScEqState st;
    if (!ScEqRead(&st)) return 0xFFFF;
    const int it = st.slots[y] != SCW_EMPTY ? ScEqItemOf(st, st.slots[y]) : -1;
    return it >= 0 ? (uint16_t)it : 0xFFFF;
}
// TASK #137007: overlay look rows (A / Left / Right). The open row enters uScOverlay.
bool ScOvChange(int item, int dir) {
    if (!IsScOvItem(item)) return false;
    if (item == kItScOvOpen) {
        if (dir) return true;  // the open row has no value
        g_uiPages.Enter(uScOverlay, g_sel);
        g_sel = 0; g_msg[0] = 0; g_confirm = -9;
        Log("Rematch: Soul core overlay look page opened (TASK #137007)");
        return true;
    }
    const int w = item - kItScOvBase, cur = ScOvGet(w);
    const int v = ScOvSet(w, ScOvStep(w, cur, dir));
    char t[24]; ScOvValueText(w, v, t, sizeof t);
    Log("Rematch: menu %s -> %d (TASK #137007)", kScOv[w].label, v);
    snprintf(g_msg, sizeof g_msg, "%s %s. Applies now. Saved.", kScOv[w].label, t);
    return true;
}
bool ScOvRowText(int item, bool sel, const char* mk, char* buf, size_t n, uint32_t* color) {
    if (!IsScOvItem(item)) return false;
    if (item == kItScOvOpen) { snprintf(buf, n, "%s Soul core overlay look   A: open", mk); *color = sel ? cWhite : cGray; return true; }
    const int w = item - kItScOvBase, v = ScOvGet(w);
    char t[24]; ScOvValueText(w, v, t, sizeof t);
    snprintf(buf, n, "%s %-22s %-12s Left / Right", mk, kScOv[w].label, t);
    *color = sel ? cWhite : v != kScOv[w].def ? cYellow : cGray;
    return true;
}
const char* ScOvFooter(int item) {
    if (item == kItScOvOpen) return "Size, place, opacity, background, colors and letters of the soul core overlay (bottom left while you play). F2 changes the core pair, F3 / F4 flip Yang 1 / Yang 2 between summon and Transform.";
    if (item >= kItScOvBase && item < kItScOvBase + kScOvN) return kScOv[item - kItScOvBase].foot;
    return nullptr;
}
// TASK #137007 (human #137004): F3 / F4 flip the core in Yang slot 1 / 2 between summon and Transform. Transform never picks 'moves':
// the detection decides (TASK #136987). Going back to summon restores the summon kind the core had (the game's own or Fast summon).
uint32_t g_scKeyId[16]; int g_scKeySummon[16]; int g_scKeyN = 0;
void ScCallKey(int y) {
    uint32_t chr[2] = {};
    const bool eq = bosslog::FormCoreEquippedIds(chr);
    const uint16_t item = ScSlotItem(y);
    if (!eq || !chr[y]) {
        if (item == 0xFFFF || !item) ToastF(cYellow, 3000, "Yang %d: no soul core in this slot.", y + 1);
        else ToastF(cYellow, 3000, "Yang %d (core %X): not detected yet. Load into the game first, then press F%d again.", y + 1, item, y + 3);
        Log("Rematch: F%d soul core call: Yang %d not detected (eq=%d id=%X item=%X, TASK #137007)", y + 3, y + 1, eq ? 1 : 0, chr[y], item);
        return;
    }
    const uint32_t id = chr[y];
    const int cur = bosslog::FormCoreCallGet(id);
    int k = 0; while (k < g_scKeyN && g_scKeyId[k] != id) ++k;
    int want;
    if (sc_call_forms(cur)) want = k < g_scKeyN ? g_scKeySummon[k] : SC_CALL_GAME;
    else {
        if (k == g_scKeyN && g_scKeyN < 16) ++g_scKeyN;
        if (k < 16) { g_scKeyId[k] = id; g_scKeySummon[k] = cur; }
        want = SC_CALL_TRANSFORM;
    }
    const int now = bosslog::FormCoreCallSet(id, want);
    const bool forms = bosslog::SoulCoreForms(nullptr);
    Log("Rematch: F%d Yang %d core %X (item %X): %s -> %s (TASK #137007)", y + 3, y + 1, id, item, sc_call_name(cur), sc_call_name(now));
    const char* what = sc_call_forms(now) ? "Transform" : now == SC_CALL_FAST_SUMMON ? "Fast summon" : "Summon";
    if (sc_call_forms(now) && !forms) ToastF(cYellow, 4000, "Yang %d core %X: Transform (saved), but Soul core transformations is OFF in the menu.", y + 1, item);
    else if (now == want) ToastF(sc_call_forms(now) ? cGreen : cWhite, 3000, "Yang %d core %X: %s from the next use.", y + 1, item, what);
    else ToastF(cYellow, 4000, "Yang %d core %X: stays %s (the saved core list is full).", y + 1, item, what);
}
bool ScEqChange(int item, int dir) {
    if (ScOvChange(item, dir)) return true;  // TASK #137007
    if (!IsScEqItem(item)) return false;
    if (item == kItScOverlay) {  // TASK #137000: A flips, Left OFF, Right ON
        const bool v = dir > 0 ? true : dir < 0 ? false : !ScOverlayOn();
        g_scOverlay.store(v ? 1 : 0);
        WritePrivateProfileStringW(L"Menu", L"SoulCoreOverlay", v ? L"1" : L"0", IniPath());
        Log("Rematch: soul core overlay %s (TASK #137000)", v ? "ON" : "OFF");
        snprintf(g_msg, sizeof g_msg, v ? "Soul core overlay ON: bottom left, one line per Yang slot. Saved." : "Soul core overlay OFF. Saved.");
        return true;
    }
    ScEqState st;
    if (!ScEqRead(&st)) { snprintf(g_msg, sizeof g_msg, "Soul cores cannot be read right now (load a save first)."); return true; }
    if (item == kItScEqSlot) {
        g_scEqSlot = ((g_scEqSlot + (dir ? dir : 1)) % SCW_NSLOT + SCW_NSLOT) % SCW_NSLOT;
        g_scEqPick = SCW_EMPTY; g_msg[0] = 0;
        return true;
    }
    ScCat cat;
    if (ScCatRead(&cat)) {
        const uint16_t inItem = cat.slotItem[g_scEqSlot];
        char sn[16]; ScwSlotName(g_scEqSlot, sn, sizeof sn);
        if (dir) {
            g_scEqPick = ScwStepId(g_scEqPick, dir, inItem, cat.ids, cat.n);
            g_msg[0] = 0;
            return true;
        }
        if (g_scEqPick == SCW_EMPTY || ScwCheckId(g_scEqSlot, (uint16_t)g_scEqPick, 1, inItem)) {
            snprintf(g_msg, sizeof g_msg, "Pick a core with Left / Right first (one that is not already in %s).", sn);
            return true;
        }
        using E = int (*)(int, int); using R = int (*)(int*);
        const auto eq = (E)TrainingExport("Nioh3Training_SoulCoreEquipItem"); const auto res = (R)TrainingExport("Nioh3Training_SoulCoreEquipResult");
        int seq = 0; if (res) res(&seq);
        const int q = eq ? eq(g_scEqSlot, (int)(uint16_t)g_scEqPick) : -1;
        if (q == 1) g_scEqSeq = seq;
        Log("Rematch: menu Equip core %X into %s (any core in the game) -> %d (human #136123)", (unsigned)(uint16_t)g_scEqPick, sn, q);
        if (q == 1) snprintf(g_msg, sizeof g_msg, "Equipping core %X into %s...", (unsigned)(uint16_t)g_scEqPick, sn);
        else snprintf(g_msg, sizeof g_msg, "%s", q == 0 ? "A core change is already running; try again in a moment." : "This Nioh3Training.asi cannot equip cores on this game version.");
        return true;
    }
    const uint32_t inSlot = st.slots[g_scEqSlot];
    if (dir) {
        g_scEqPick = ScwStep(g_scEqPick, dir, inSlot, st.idx, st.n);
        g_msg[0] = 0;
        return true;
    }
    char sn[16]; ScwSlotName(g_scEqSlot, sn, sizeof sn);
    if (g_scEqPick == SCW_EMPTY || ScwCheck(g_scEqSlot, g_scEqPick, st.slots, st.idx, st.n)) {
        snprintf(g_msg, sizeof g_msg, "Pick a core with Left / Right first (one that is not already in %s).", sn);
        return true;
    }
    using E = int (*)(int, int); using R = int (*)(int*);
    const auto eq = (E)TrainingExport("Nioh3Training_SoulCoreEquip"); const auto res = (R)TrainingExport("Nioh3Training_SoulCoreEquipResult");
    int seq = 0; if (res) res(&seq);
    const int q = eq ? eq(g_scEqSlot, (int)g_scEqPick) : -1;
    if (q == 1) g_scEqSeq = seq;
    Log("Rematch: menu Equip core %X into %s (idx %u) -> %d (human #129277)", ScEqItemOf(st, g_scEqPick), sn, g_scEqPick, q);
    if (q == 1) snprintf(g_msg, sizeof g_msg, "Equipping core %X into %s...", ScEqItemOf(st, g_scEqPick), sn);
    else snprintf(g_msg, sizeof g_msg, "%s", q == 0 ? "A core change is already running; try again in a moment." : "This Nioh3Training.asi cannot equip cores on this game version.");
    return true;
}
void ScEqPoll() {  // finished request -> message (called from the row text, menu thread)
    if (g_scEqSeq < 0) return;
    using R = int (*)(int*);
    const auto res = (R)TrainingExport("Nioh3Training_SoulCoreEquipResult");
    int seq = 0; const int r = res ? res(&seq) : SCW_FAULT;
    if (res && seq == g_scEqSeq) return;
    g_scEqSeq = -1; g_scEqPick = SCW_EMPTY;
    if (r == SCW_DONE) {  // TASK #137000: finish like F2 (full spirit gauge, a toast), so a menu pick is ready to use at once
        char rep[256]; Command("spirit gauge max", rep, sizeof rep);
        Log("Rematch: menu Equip core done; %s (TASK #137000)", rep);
        ToastF(cGreen, 4000, "Soul core equipped. Spirit gauge full.");
    }
    snprintf(g_msg, sizeof g_msg, "%s", r == SCW_DONE ? "Core equipped. The d-pad shortcut moved to the new core." : r == SCW_REFUSED ? "The game refused that core for this slot (same as at a shrine)."
             : r == SCW_SAME ? "That core is already in this slot." : r == SCW_NO_GAME ? "No save loaded: nothing changed."
             : r == SCW_NO_ADD ? "That core is not in this save, and it could not be added, so nothing was equipped."
             : "Core change failed: nothing more was done (see Nioh3Training.log).");
}
bool ScEqRowText(int item, bool sel, const char* mk, char* buf, size_t n, uint32_t* color) {
    if (ScOvRowText(item, sel, mk, buf, n, color)) return true;  // TASK #137007
    if (!IsScEqItem(item)) return false;
    if (item == kItScOverlay) {  // TASK #137000
        const bool v = ScOverlayOn();
        snprintf(buf, n, "%s Soul core overlay: %s   A / Left / Right", mk, v ? "ON" : "OFF");
        *color = sel ? cWhite : v ? cGray : cYellow;
        return true;
    }
    ScEqPoll();
    ScEqState st; const bool ok = ScEqRead(&st);
    char sn[16]; ScwSlotName(g_scEqSlot, sn, sizeof sn);
    ScCat cat; const bool catOk = ScCatRead(&cat);
    if (item == kItScEqSlot) {
        const uint32_t cur = ok ? st.slots[g_scEqSlot] : SCW_EMPTY;
        const int it = ok && cur != SCW_EMPTY ? ScEqItemOf(st, cur) : -1;
        char now[24];
        if (catOk && cat.slotItem[g_scEqSlot] != 0xFFFF) snprintf(now, sizeof now, "core %X", cat.slotItem[g_scEqSlot]);
        else if (it >= 0) snprintf(now, sizeof now, "core %X", it);
        else snprintf(now, sizeof now, "%s", cur == SCW_EMPTY ? "empty" : "?");
        snprintf(buf, n, "%s Equip core slot: %s (now %s)   Left / Right", mk, sn, ok ? now : "not readable");
        *color = sel ? cWhite : ok ? cGray : cDim;
        return true;
    }
    if (catOk) {
        const uint16_t inItem = cat.slotItem[g_scEqSlot];
        int pos = 0, cand = 0;
        for (int i = 0; i < cat.n; ++i) if (cat.ids[i] != inItem) { ++cand; if (g_scEqPick != SCW_EMPTY && cat.ids[i] == (uint16_t)g_scEqPick) pos = cand; }
        if (g_scEqPick != SCW_EMPTY && !pos) g_scEqPick = SCW_EMPTY;
        if (g_scEqPick == SCW_EMPTY) snprintf(buf, n, "%s Equip core: pick one (%d cores in the game)   Left / Right, A: equip", mk, cat.n);
        else snprintf(buf, n, "%s Equip core: core %X (%d of %d in the game)   Left / Right, A: equip into %s", mk, (unsigned)(uint16_t)g_scEqPick, pos, cat.n, sn);
        *color = sel ? cWhite : g_scEqPick == SCW_EMPTY ? cGray : cYellow;
        return true;
    }
    if (!ok) { snprintf(buf, n, "%s Equip core: (load a save)", mk); *color = cDim; return true; }
    int pos = 0, cand = 0;
    for (int i = 0; i < st.n; ++i) if (st.idx[i] != st.slots[g_scEqSlot]) { ++cand; if (st.idx[i] == g_scEqPick) pos = cand; }
    if (g_scEqPick != SCW_EMPTY && !pos) g_scEqPick = SCW_EMPTY;  // picked core gone / now in this slot
    if (g_scEqPick == SCW_EMPTY) snprintf(buf, n, "%s Equip core: pick one (%d cores)   Left / Right, A: equip", mk, cand);
    else snprintf(buf, n, "%s Equip core: core %X (%d of %d)   Left / Right, A: equip into %s", mk, ScEqItemOf(st, g_scEqPick), pos, cand, sn);
    *color = sel ? cWhite : g_scEqPick == SCW_EMPTY ? cGray : cYellow;
    return true;
}
// human #135018: hands-on pair stepping from the keyboard (no menu, no chat). F2 = next pair (TASK #136451: every soul core in the game when
// Nioh3Training has SoulCoreCatalog, else owned cores; distinct items, list order, wraps after the last)
// into Yang 1 + Yang 2, Shift+F2 = previous pair, F6 = flag the pair now equipped (Nioh3BossRematch.log + Nioh3BossRematch_flags.txt).
// Uses the same Nioh3Training_SoulCoreEquip request as the menu rows, one slot at a time; then fills the spirit gauge.
struct ScPairList { uint32_t idx[256]; uint16_t item[256]; int n; uint32_t slots[SCW_NSLOT]; };
bool ScPairRead(ScPairList* L) {
    using F = int (*)(uint32_t*, uint32_t*, uint16_t*, int);
    const auto f = (F)TrainingExport("Nioh3Training_SoulCoreState");
    uint32_t idx[1024]; uint16_t item[1024];
    int n = f ? f(L->slots, idx, item, 1024) : -1;
    for (int k = 0; f && n < 0 && k < 6; ++k) { Sleep(100); n = f(L->slots, idx, item, 1024); }  // first read after a load resolves the list
    L->n = 0;
    if (n < 0) return false;
    for (int i = 0; i < n && L->n < 256; ++i) {
        bool dup = false;
        for (int j = 0; j < L->n; ++j) if (L->item[j] == item[i]) { dup = true; break; }
        if (!dup) { L->idx[L->n] = idx[i]; L->item[L->n] = item[i]; ++L->n; }
    }
    return true;
}
int ScPairItemAt(const ScPairList& L, uint32_t idx) {
    for (int i = 0; i < L.n; ++i) if (L.idx[i] == idx) return i;
    return -1;
}
int g_scPairStage = 0;        // 0 idle, 1 first slot request out, 2 second slot pending, 3 second slot request out
int g_scPairSeq = 0, g_scPairOrder[2] = {0, 1};
uint32_t g_scPairWant[2] = {SCW_EMPTY, SCW_EMPTY};
uint16_t g_scPairItems[2] = {0, 0};
int g_scPairNo = 0, g_scPairOf = 0;
ULONGLONG g_scPairT0 = 0;
bool g_scPairCat = false;
int g_scPairCatP = -1, g_scPairCatDir = 1, g_scPairSkips = 0;  // last catalog pair tried; a refused one is skipped, the next F2 counts from it  // TASK #136451 (human #136449): this step walks every soul core in the item master (SoulCoreCatalog), not only owned ones
// Send one slot's request; 1 sent, 0 busy (retry), 2 nothing to do (already there), -1 failed.
int ScPairSend(int slot) {
    using E = int (*)(int, int); using R = int (*)(int*);
    const auto eq = (E)TrainingExport("Nioh3Training_SoulCoreEquip"); const auto res = (R)TrainingExport("Nioh3Training_SoulCoreEquipResult");
    if (!eq || !res) return -1;
    if (g_scPairCat) {  // catalog step: equip by item id (an unowned core is added by the game's own item add, as the menu row does)
        const auto eqi = (E)TrainingExport("Nioh3Training_SoulCoreEquipItem");
        ScCat c; if (!eqi || !ScCatRead(&c)) return -1;
        if (c.slotItem[slot] == g_scPairItems[slot]) return 2;
        int sq = 0; res(&sq);
        const int qi = eqi(slot, (int)g_scPairItems[slot]);
        if (qi == 1) g_scPairSeq = sq;
        return qi == 1 ? 1 : qi == 0 ? 0 : -1;
    }
    ScPairList L; if (!ScPairRead(&L)) return -1;
    const uint32_t want = g_scPairWant[slot];
    if (L.slots[slot] == want) return 2;
    int seq = 0; res(&seq);
    const int q = eq(slot, (int)want);
    if (q == 1) g_scPairSeq = seq;
    return q == 1 ? 1 : q == 0 ? 0 : -1;
}
bool ScPairDone(int* r) {  // request we sent finished?
    using R = int (*)(int*);
    const auto res = (R)TrainingExport("Nioh3Training_SoulCoreEquipResult");
    int seq = 0; *r = res ? res(&seq) : SCW_FAULT;
    return !res || seq != g_scPairSeq;
}
void ScPairStep(int dir);
void ScPairFinish(bool ok, int r) {
    g_scPairStage = 0;
    if (!ok && g_scPairCat && g_scPairSkips < 8) {  // TASK #136451: a core the game will not put in a Yang slot must not stop the walk
        ++g_scPairSkips;
        Log("Rematch: F2 pair %d/%d (core %X + core %X) refused by the game r=%d: skipped, next pair (TASK #136451)", g_scPairNo, g_scPairOf, g_scPairItems[0], g_scPairItems[1], r);
        ScPairStep(g_scPairCatDir);
        return;
    }
    g_scPairSkips = 0;
    if (!ok) {
        Log("Rematch: pair step %d/%d failed r=%d (human #135018)", g_scPairNo, g_scPairOf, r);
        ToastF(cYellow, 4000, "Soul core pair change failed (%d). F2 tries the next pair.", r);
        return;
    }
    char rep[256]; Command("spirit gauge max", rep, sizeof rep);
    Log("Rematch: pair %d/%d on: Yang 1 core %X, Yang 2 core %X; %s (human #135018)", g_scPairNo, g_scPairOf, g_scPairItems[0], g_scPairItems[1], rep);
    ToastF(cGreen, 5000, "Pair %d of %d: core %X (Yang 1) + core %X (Yang 2). F6 flag, F2 next", g_scPairNo, g_scPairOf, g_scPairItems[0], g_scPairItems[1]);
}
// Catalog pairs: entries (2p, 2p+1) of the item-master list, so F2 reaches the last core in the game, then wraps to pair 1.
int ScPairCatPick(const uint16_t* ids, int n, uint16_t inYang1, int dir, int* a, int* b, int lastP = -1) {
    const int npairs = (n + 1) / 2;
    int cur = -1;
    for (int i = 0; i < n; ++i) if (ids[i] == inYang1) { cur = i; break; }
    if (lastP >= 0 && lastP < npairs && (cur < 0 || cur / 2 != lastP)) cur = lastP * 2;  // the last pair tried did not land: go on from it
    const int p = cur < 0 ? (dir > 0 ? 0 : npairs - 1) : ((cur / 2 + dir) % npairs + npairs) % npairs;
    *a = p * 2; *b = *a + 1 < n ? *a + 1 : (*a ? 0 : -1);
    return p;
}
void ScPairStep(int dir) {
    if (g_scPairStage) { ToastF(cYellow, 2000, "Still changing cores..."); return; }
    ScCat cat;
    if (TrainingExport("Nioh3Training_SoulCoreEquipItem") && ScCatRead(&cat) && cat.n >= 2) {
        int a = 0, b = 0;
        const int p = ScPairCatPick(cat.ids, cat.n, cat.slotItem[0], dir, &a, &b, g_scPairCatP);
        g_scPairCat = true; g_scPairCatP = p; g_scPairCatDir = dir;
        g_scPairItems[0] = cat.ids[a]; g_scPairItems[1] = cat.ids[b];
        g_scPairWant[0] = g_scPairWant[1] = SCW_EMPTY;
        g_scPairNo = p + 1; g_scPairOf = (cat.n + 1) / 2;
        const bool sw = cat.slotItem[1] == g_scPairItems[0];
        g_scPairOrder[0] = sw ? 1 : 0; g_scPairOrder[1] = sw ? 0 : 1;
        g_scPairStage = 1; g_scPairT0 = Ms();
        Log("Rematch: F2 pair %d/%d of every core in the game (%d cores): Yang 1 core %X, Yang 2 core %X (TASK #136451)", g_scPairNo, g_scPairOf, cat.n, g_scPairItems[0], g_scPairItems[1]);
        const int s = ScPairSend(g_scPairOrder[0]);
        if (s == 2) g_scPairStage = 2; else if (s == 0) g_scPairStage = 4; else if (s < 0) ScPairFinish(false, s);
        ToastF(cWhite, 3000, "Changing to pair %d of %d (all %d cores): core %X + core %X...", g_scPairNo, g_scPairOf, cat.n, g_scPairItems[0], g_scPairItems[1]);
        return;
    }
    g_scPairCat = false;
    ScPairList L;
    if (!TrainingExport("Nioh3Training_SoulCoreEquip") || !ScPairRead(&L)) { ToastF(cYellow, 3000, "Soul cores cannot be read right now (load a save first)."); return; }
    if (L.n < 2) { ToastF(cYellow, 3000, "Fewer than 2 soul cores owned."); return; }
    const int npairs = (L.n + 1) / 2;
    int cur = L.slots[0] == SCW_EMPTY ? -1 : ScPairItemAt(L, L.slots[0]);
    int p = cur < 0 ? (dir > 0 ? 0 : npairs - 1) : ((cur / 2 + dir) % npairs + npairs) % npairs;
    const int a = p * 2, b = a + 1 < L.n ? a + 1 : 0;
    g_scPairWant[0] = L.idx[a]; g_scPairWant[1] = L.idx[b];
    g_scPairItems[0] = L.item[a]; g_scPairItems[1] = L.item[b];
    g_scPairNo = p + 1; g_scPairOf = npairs;
    // the core for Yang 1 sitting in Yang 2 now: change Yang 2 first so the two do not trade places
    const bool swapFirst = L.slots[1] == g_scPairWant[0];
    g_scPairOrder[0] = swapFirst ? 1 : 0; g_scPairOrder[1] = swapFirst ? 0 : 1;
    g_scPairStage = 1; g_scPairT0 = Ms();
    const int s = ScPairSend(g_scPairOrder[0]);
    if (s == 2) g_scPairStage = 2;
    else if (s == 0) g_scPairStage = 4;  // busy: retry the first slot from the tick
    else if (s < 0) ScPairFinish(false, s);
    ToastF(cWhite, 3000, "Changing to pair %d of %d: core %X + core %X...", g_scPairNo, g_scPairOf, g_scPairItems[0], g_scPairItems[1]);
}
void ScPairFlag() {
    ScPairList L;
    if (!ScPairRead(&L)) { ToastF(cYellow, 3000, "Soul cores cannot be read right now."); return; }
    int it[2] = {-1, -1};
    for (int s = 0; s < 2; ++s) { const int k = L.slots[s] == SCW_EMPTY ? -1 : ScPairItemAt(L, L.slots[s]); if (k >= 0) it[s] = L.item[k]; }
    SYSTEMTIME t; GetLocalTime(&t);
    Log("Rematch: FLAG pair Yang 1 core %X, Yang 2 core %X (human #135018)", it[0], it[1]);
    wchar_t path[MAX_PATH]; const DWORD m = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (m && m < MAX_PATH) {
        wchar_t* sl = wcsrchr(path, L'\\'); if (sl) wcscpy_s(sl + 1, MAX_PATH - (sl + 1 - path), L"Nioh3BossRematch_flags.txt");
        FILE* f = nullptr;
        if (!_wfopen_s(&f, path, L"a") && f) {
            fprintf(f, "%04u-%02u-%02u %02u:%02u:%02u flag Yang1=%X Yang2=%X\n", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, it[0], it[1]);
            fclose(f);
        }
    }
    ToastF(cYellow, 4000, "Flagged: core %X + core %X", it[0], it[1]);
}
void ScPairTickImpl(bool next, bool prev, bool flag) {
    if (flag) ScPairFlag();
    if (next || prev) ScPairStep(next ? 1 : -1);
    if (!g_scPairStage) return;
    if (Ms() - g_scPairT0 > 20000) { ScPairFinish(false, SCW_FAULT); return; }
    int r = 0;
    if (g_scPairStage == 4) {  // first slot was busy
        const int s = ScPairSend(g_scPairOrder[0]);
        if (s == 1) g_scPairStage = 1; else if (s == 2) g_scPairStage = 2; else if (s < 0) ScPairFinish(false, s);
        return;
    }
    if (g_scPairStage == 1 || g_scPairStage == 3) {
        if (!ScPairDone(&r)) return;
        if (r != SCW_DONE && r != SCW_SAME) { ScPairFinish(false, r); return; }
        if (g_scPairStage == 3) { ScPairFinish(true, r); return; }
        g_scPairStage = 2;
    }
    if (g_scPairStage == 2) {
        const int s = ScPairSend(g_scPairOrder[1]);
        if (s == 1) g_scPairStage = 3; else if (s == 2) ScPairFinish(true, SCW_SAME); else if (s < 0) ScPairFinish(false, s);
    }
}
// A (dir 0) / Left (-1) / Right (+1). Returns true when the item was ours.
bool SctChange(int item, int dir) {
    if (ScEqChange(item, dir)) return true;  // human #129277
    if (IsScCoreItem(item)) {  // human #81220
        uint32_t id = 0; int d = 0;
        if (!ScCoreAt(item - kItScCore, &id, &d)) return true;
        if (!id) {
            const bool cur = bosslog::FormCorePendingTransforms(d);
            const bool want = dir ? dir > 0 : !cur;
            // Save explicit Left/Right even when it matches the displayed default: the next core may differ from the cached one.
            const bool v = bosslog::FormCorePendingSet(d, want);
            if (v != want) { snprintf(g_msg, sizeof g_msg, "The saved core list is full. Enable another core before disabling this seal."); return true; }
            snprintf(g_msg, sizeof g_msg, "%s seal: %s on the next use. Saved; follows the core once identified.",
                d == 1 ? "LEFT" : "DOWN", v ? "transform" : "game summon");
            return true;
        }
        // TASK #136197 (human #136194): each detected core picks its call. A / Right = next, Left = previous.
        const int cur = bosslog::FormCoreCallGet(id);
        const int v = bosslog::FormCoreCallSet(id, sc_call_step(cur, dir < 0 ? -1 : 1));
        Log("Rematch: menu soul core %X call -> %s (TASK #136197)", id, sc_call_name(v));
        snprintf(g_msg, sizeof g_msg, "Core %X: %s from the next use. Saved.", id,
                 sc_call_forms(v) ? "transform (moves if the yokai can move, detected)"  // TASK #136987
                 : v == SC_CALL_FAST_SUMMON ? "fast summon (the game's own summon, sped up)" : "the game's own summon");  // TASK #136502
        return true;
    }
    if (item == kItScForms) {  // human #61869: A flips, Left OFF, Right ON
        bool up = false;
        const bool v = bosslog::SoulCoreFormsSet(dir, &up);
        Log("Rematch: menu Soul core transformations -> %s (human #61869)", v ? "ON" : "OFF");
        snprintf(g_msg, sizeof g_msg, "%s", v ? "Soul core transformations ON: soul cores transform again from the next use. Saved."
                 : up ? "Soul core transformations OFF: the form now up ends normally, then the game's own soul cores. Saved."
                      : "Soul core transformations OFF: the game's own soul cores from the next use. Saved.");
        return true;
    }
    if (!IsSctItem(item)) return false;
    if (item == kItSctOpen) {
        if (dir) return true;  // the open row has no value
        g_uiPages.Enter(uSoulTune, g_sel);
        g_sel = 0; g_msg[0] = 0; g_confirm = -9;
        Log("Rematch: Soul core tuning page opened (human #58117)");
        return true;
    }
    const int w = item - kItSctBase;
    bool live = true;
    if (w == kSctBoth) {  // human #129070
        const int d = dir ? dir : 1;
        bosslog::SoulTune(11, d, nullptr); bosslog::SoulTune(6, d, nullptr);
        char bt[40]; SctBothText(&live, nullptr, bt, sizeof bt);
        Log("Rematch: menu Soul core Start + end speed %s -> %s (human #129070)", d > 0 ? "up" : "down", bt);
        snprintf(g_msg, sizeof g_msg, live ? "Start + end speed: %s. Applies on the next LEFT soul core use. Saved."
                 : "Start + end speed: %s. Saved; the speed hooks are off this session, applies next launch.", bt);
        return true;
    }
    const double v = bosslog::SoulTune(w, dir ? dir : 1, &live);
    char t[24]; SctValueText(w, v, t, sizeof t);
    Log("Rematch: menu Soul core %s -> %s (human #58117)", kSctLabel[w], t);
    if (w == 9) { snprintf(g_msg, sizeof g_msg, "%s %s. Live and saved; hidden yokai finishes its attack.", kSctLabel[w], t); return true; }
    if (w == 10) { snprintf(g_msg, sizeof g_msg, "%s %s. Live from the next soul core press. Saved.", kSctLabel[w], t); return true; }  // human #122288
    if (w >= 15) { snprintf(g_msg, sizeof g_msg, "%s %s. Live from the next soul core use. Saved.", kSctLabel[w], t); return true; }  // TASK #135613
    snprintf(g_msg, sizeof g_msg, live ? "%s %s. Applies on the next LEFT soul core use. Saved." : "%s %s. Saved; the speed hooks are off this session, applies next launch.",
             kSctLabel[w], t);
    return true;
}
bool SctRowText(int item, bool sel, const char* mk, char* buf, size_t n, uint32_t* color) {
    if (ScEqRowText(item, sel, mk, buf, n, color)) return true;  // human #129277
    if (IsScCoreItem(item)) {  // human #81220
        uint32_t id = 0; int d = 0;
        if (!ScCoreAt(item - kItScCore, &id, &d)) { buf[0] = 0; *color = cDim; return true; }
        if (!id) {
            const bool v = bosslog::FormCorePendingTransforms(d);
            snprintf(buf, n, "%s %s seal (not used yet): %s   A / Left / Right", mk, d == 1 ? "LEFT" : "DOWN", v ? "transforms" : "game summon");
            *color = sel ? cWhite : v ? cGray : cYellow;
            return true;
        }
        const int c = bosslog::FormCoreCallGet(id);  // TASK #136197
        const bool v = c != SC_CALL_GAME;
        const char* label = (d & 12) == 12 ? "(Yang 1 + 2)" : (d & 4) ? "(Yang 1)" : (d & 8) ? "(Yang 2)" :
            d == 1 ? "(LEFT last used)" : d == 2 ? "(DOWN last used)" : d == 3 ? "(LEFT/DOWN last used)" : "";
        snprintf(buf, n, "%s Core %X %s: %s   A / Left / Right", mk, id, label, sc_call_name(c));
        *color = sel ? cWhite : v ? cGray : cYellow;
        return true;
    }
    if (item == kItScForms) {  // human #61869
        bool hooked = true;
        const bool v = bosslog::SoulCoreForms(&hooked);
        snprintf(buf, n, "%s Soul core transformations: %s   A / Left / Right%s", mk, v ? "ON" : "OFF", hooked ? "" : "  (FormKey off)");
        *color = sel ? cWhite : !hooked ? cDim : v ? cGray : cYellow;
        return true;
    }
    if (!IsSctItem(item)) return false;
    if (item == kItSctOpen) { snprintf(buf, n, "%s Soul core tuning   A: open", mk); *color = sel ? cWhite : cGray; return true; }
    const int w = item - kItSctBase;
    bool live = true;
    if (w == kSctBoth) {  // human #129070
        bool ch = false; char bt[40]; SctBothText(&live, &ch, bt, sizeof bt);
        snprintf(buf, n, "%s %-26s %-20s Left / Right%s", mk, kSctLabel[w], bt, live ? "" : "  (next launch)");
        *color = sel ? cWhite : !live ? cDim : ch ? cYellow : cGray;
        return true;
    }
    const double v = bosslog::SoulTune(w, 0, &live);
    char t[24]; SctValueText(w, v, t, sizeof t);
    snprintf(buf, n, "%s %-26s %-11s Left / Right%s", mk, kSctLabel[w], t, live ? "" : "  (next launch)");
    *color = sel ? cWhite : !live ? cDim : (v > kSctDefault[w] + 1e-6 || v < kSctDefault[w] - 1e-6) ? cYellow : cGray;
    return true;
}
const char* SctFooter(int item) {
    if (const char* f = ScOvFooter(item)) return f;  // TASK #137007
    if (item == kItScOverlay) return "Small text at the bottom left while you play: each Yang slot's core and how it plays. S = summon (the game's own or Fast summon), T = Transform, TM = Transform whose yokai has been seen moving (detected, nothing to set). Updates at once. [Menu] SoulCoreOverlay. Saved.";  // TASK #137000
    if (IsScEqItem(item)) return item == kItScEqSlot ? "Which soul core slot to change (Yang 1-2, then Yin). Shows the core in it now."
        : TrainingExport("Nioh3Training_SoulCoreCatalog")
            ? "Left / Right: every soul core in the game, including ones this save does not own. A: equip it into the slot above. One you do not own is added with the game's own item add, then equipped the same way as one you own. Live."
            : "Left / Right: pick an owned core. A: equip it into the slot above, like at a shrine (the game's own check; the d-pad shortcut follows). Live.";  // human #129277, #136123
    if (IsScCoreItem(item)) return "Each core: Game's own / Transform (the left stick steers only if that yokai moves, detected; still cores stay locked) / Fast summon (the game's own summon, faster toss and yokai; the game ends it). [Spirit] FormCoreCalls. Yang rows show equipped cores. Choices follow the core. If equipment cannot be read, choose an unidentified seal's next use; its choice follows the core once identified. Saved.";
    if (item == kItScForms)  // human #61869
        return "ON: soul cores transform you into the yokai (mod forms). OFF: the game's original soul cores. Live from the next use. [Spirit] SoulCoreForms. Saved.";
    if (!IsSctItem(item)) return nullptr;
    switch (item - kItSctBase) {
    case 0: return "Exit: delay between the exit smoke and the player showing again. 50 ms steps, 0-2000. [Spirit] SealExitRevealMs. Live, saved.";
    case 1: return "Exit and start smoke: units in front of the player (minus = behind). 10-unit steps, -200..200. [Spirit] SealExitSmokeFwd. Live, saved.";
    case 2: return "Exit and start smoke: units to the side of the player (minus = other side). 10-unit steps, -200..200. [Spirit] SealExitSmokeSide. Live, saved.";
    case 3: return "Exit smoke spawned once more this long after the first (longer smoke). OFF = once. 100 ms steps, 0-2000. [Spirit] SealExitSmokeRepeatMs. Live, saved.";
    case 4: return "Form start: delay before the player is hidden (after the LEFT press / seal capture). 0 = at once. 50 ms steps, 0-1000. [Spirit] FormHideDelayMs. Live, saved.";
    case 5: return "Yokai form animation speed from the transform to the last hit. x0.1 steps, x0.5-x3.0. [Spirit] SealSpeed (needs FormHide = 4). Live, saved.";
    case 6: return "Exit: yokai animation speed after its last hit (ramps up from Transform speed). x0.5 steps, x1-x8 (x1 = off). [Spirit] SealTailSpeed. Live, saved.";
    case 7: return "ON: the start smoke stays up on you (spawned again and again); the forward / sideways rows move it live. OFF = normal.";
    case 8: return "ON: the end smoke stays up on you (spawned again and again); the forward / sideways rows move it live. OFF = normal.";
    case 10: return "Crouch (L3) this soon after a soul core press cancels the transformation (no real crouch); the core is not spent. OFF = no cancel. 50 ms steps, 0-1000. [Spirit] FormCrouchCancel. Live, saved.";
    case 11: return "How fast you toss the soul core before you turn into the yokai (standing or running). Higher = you transform sooner. x1 = the game's speed. 1 steps, x1-x16. [Spirit] FastThrow. Live, saved.";
    case 12: return "Safety cap for the row above: the faster toss never lasts longer than this after you press the core; it also ends as soon as the throw ends. OFF = no throw speed-up. 100 ms steps, 0-3000. [Spirit] RunThrowMaxMs. Live, saved.";
    case 14: return "Speeds up the start and the end together: each step moves Core toss speed (the throw before you transform) and Exit tail speed (after the last hit) one notch. Change either row on its own to fine-tune. Live, saved.";
    case 13: return "After the yokai's last hit, how long it takes to speed up from Transform speed to Exit tail speed (smooth vs sudden). 0 = jump at once. 50 ms steps, 0-1000. [Spirit] SealTailRampMs. Live, saved.";
    case 15: return "ON: at the exit the yokai turns invisible and finishes its attack (projectiles keep flying) while you are back. OFF = the form waits for the yokai. [Spirit] SealGhostExit. Live, saved.";
    case 16: return "The yokai's idle pose ends the form only this long after it appeared (shorter = earlier possible exit). 50 ms steps, 0-3000. [Spirit] SealIdleMinMs. Live, saved.";
    case 17: return "A core whose last hit is a projectile / helper: the form ends this long after that hit starts, the projectile stays visible. OFF = wait for the yokai. 50 ms steps, 0-5000. [Spirit] SealExitOnHelperMs. Live, saved.";
    case 18: return "A core whose hit count is not certain from game data exits no earlier than this % of its attack (bind to the yokai's own end, seen on a full use this session; the first use plays in full). OFF = exit at the last hit. 5 % steps, 0-100. [Spirit] SealExitEndPct. Live, saved.";
    case 19: return "After the last projectile of any soul core, turn back this soon. A bit quicker than waiting out the rest of the attack. OFF = the older wait. 50 ms steps, 0-3000. [Spirit] SealExitAfterShotMs. Live, saved.";
    case 20: return "Every soul core yokai that moves (detected): how far the left stick carries the yokai once it steers. One value for all cores. x1 = the game's own step. 1 steps, x1-x8. [Spirit] FormSteerMove. Live, saved.";  // TASK #136492
    case 21: return "Every Fast summon soul core: the yokai's animation speed (the game's own summon, the game ends it). = Transform: same as Transform speed. x0.1 steps, x1.0-x3.0. [Spirit] FastSummonSpeed. Live, saved.";  // TASK #136502
    case 9: return "Exit smoke when the final hit starts, plus this delay. 50 ms steps, -500..1000. Negative values fire at the observed start; no prediction. Unknown counts use idle fallback. Live, saved.";
    default: return "A: open. Live LEFT soul core values: exit reveal delay, smoke position / repeat, hide delay, transform speed. Saved to Nioh3BossRematch.ini [Spirit].";
    }
}
