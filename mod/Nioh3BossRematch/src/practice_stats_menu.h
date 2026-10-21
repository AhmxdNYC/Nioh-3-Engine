// user #22389: Practice stats rows. Included by rematch.cpp inside its UI namespace right after boss_vitals_menu.h (same helpers: TrainingExport,
// g_msg, Log, colours). Training side: Nioh3Training practice_stats_game.h (exports Nioh3Training_PracticeStatGet / _PracticeStatSet, constants in
// practice_stats.h). Values are saved in Nioh3Training.ini [Practice]; every one defaults OFF / 1.0x.
//   Practice > Player page : Infinite Ki (stamina), Damage you deal (x0 .. x10), Damage you take (x0 .. x10).
//   Boss health and Ki page: Boss Ki drain (% of max Ki per second, all Training bosses).
// Also: Player page Infinite ninjutsu, Boss page Boss speed (notes/boss-speed-lever-22389.md).
#include "../../Nioh3Training/src/practice_stats.h"
constexpr int kItPstBase = 0xD0000;  // kItPstBase + PS_* (positive ids like kItBvOpenBase; no other range uses 0xD0000)
bool IsPstItem(int item) { return item >= kItPstBase && item < kItPstBase + PS_N; }
bool PstAvailable() { return TrainingExport("Nioh3Training_PracticeStatSet") != nullptr && TrainingExport("Nioh3Training_PracticeStatGet") != nullptr; }
int PstGet(int what) {
    using Fn = int (*)(int);
    const auto f = (Fn)TrainingExport("Nioh3Training_PracticeStatGet");
    return f ? f(what) : -1;
}
int PstSet(int what, int value) {
    using Fn = int (*)(int, int, int);
    const auto f = (Fn)TrainingExport("Nioh3Training_PracticeStatSet");
    return f ? f(what, value, 1) : -1;
}
template <class Add> void PstAddPlayerRows(Add&& add) {
    if (!PstAvailable()) return;
    add(kItPstBase + PS_PLAYER_INF_KI); add(kItPstBase + PS_DMG_DEALT); add(kItPstBase + PS_DMG_TAKEN); add(kItPstBase + PS_PLAYER_INF_NIN);
}
template <class Add> void PstAddBossRows(Add&& add) { if (PstAvailable()) { add(kItPstBase + PS_BOSS_KI_DRAIN); add(kItPstBase + PS_BOSS_SPEED); } }
const char* PstLabel(int what) {
    return what == PS_BOSS_SPEED ? "Boss speed:" : what == PS_PLAYER_INF_NIN ? "Infinite ninjutsu:" : what == PS_PLAYER_INF_KI ? "Infinite Ki (stamina):" : what == PS_DMG_DEALT ? "Damage you deal:" : what == PS_DMG_TAKEN ? "Damage you take:" : "Boss Ki drain:";
}
void PstValueText(int what, int v, char* buf, size_t n) {
    if (v < 0) snprintf(buf, n, "?");
    else if (what == PS_PLAYER_INF_KI || what == PS_PLAYER_INF_NIN) snprintf(buf, n, "%s", v ? "ON" : "OFF");
    else if (what == PS_BOSS_KI_DRAIN) { if (v) snprintf(buf, n, "%d %% / s", v); else snprintf(buf, n, "OFF"); }
    else snprintf(buf, n, "x%d.%02d", v / 100, v % 100);
}
// A (dir 0) / Left (-1) / Right (+1). Returns true when the item was ours.
bool PstChange(int item, int dir) {
    if (!IsPstItem(item)) return false;
    const int what = item - kItPstBase, cur = PstGet(what);
    if (cur < 0) { snprintf(g_msg, sizeof g_msg, "Needs a Nioh3Training.asi with the Practice stats (#22389)."); return true; }
    const int want = PstStep(what, cur, dir);
    if (what != PS_PLAYER_INF_KI && what != PS_PLAYER_INF_NIN && want == cur && dir) return true;
    const int v = PstSet(what, want);
    char t[24]; PstValueText(what, v, t, sizeof t);
    Log("Rematch: menu Practice %s -> %d (#22389)", PstLabel(what), v);
    if (what == PS_PLAYER_INF_KI) snprintf(g_msg, sizeof g_msg, v ? "Infinite Ki ON: your Ki (stamina) is refilled whenever it drops. Practice only. Saved." : "Infinite Ki OFF: normal Ki. Saved.");
    else if (what == PS_PLAYER_INF_NIN) snprintf(g_msg, sizeof g_msg, v ? "Infinite ninjutsu ON: your ninjutsu gauges stay full. Practice only. Saved." : "Infinite ninjutsu OFF. Saved.");
    else if (what == PS_BOSS_SPEED) snprintf(g_msg, sizeof g_msg, "Boss speed %s: Training bosses animate at that rate (from their next move). Practice only. Saved.", t);
    else if (what == PS_BOSS_KI_DRAIN) snprintf(g_msg, sizeof g_msg, v ? "Boss Ki drain %s: every Training boss loses that much of its max Ki each second (stops one hit from a Ki break). Saved." : "Boss Ki drain OFF. Saved.", t);
    else snprintf(g_msg, sizeof g_msg, "%s %s (HP damage of hits%s). Practice only. Saved.", PstLabel(what), t, what == PS_DMG_DEALT ? " you land on bosses" : " bosses land on you");
    return true;
}
bool PstRowText(int item, bool sel, const char* mk, char* buf, size_t n, uint32_t* color) {
    if (!IsPstItem(item)) return false;
    const int what = item - kItPstBase, v = PstGet(what);
    char t[24]; PstValueText(what, v, t, sizeof t);
    const bool changed = v >= 0 && v != PstDefault(what);
    snprintf(buf, n, "%s %-22s %s   %s", mk, PstLabel(what), t, what == PS_PLAYER_INF_KI || what == PS_PLAYER_INF_NIN ? "A / Left / Right" : "Left / Right");
    *color = sel ? cWhite : v < 0 ? cDim : changed ? (what == PS_PLAYER_INF_KI || what == PS_PLAYER_INF_NIN ? cGreen : cYellow) : cGray;
    return true;
}
const char* PstFooter(int item) {
    if (!IsPstItem(item)) return nullptr;
    switch (item - kItPstBase) {
    case PS_PLAYER_INF_KI: return "ON: your Ki (stamina) is refilled to full whenever it drops. Only while a Practice session runs or a Training boss is alive. Saved.";
    case PS_DMG_DEALT: return "Multiplies the HP damage of your hits on bosses (x0 = no damage .. x10). x1.00 = normal. Practice only (session / Training boss alive). Saved.";
    case PS_DMG_TAKEN: return "Multiplies the HP damage you take from hits (x0 = none .. x10). x1.00 = normal. Practice only (session / Training boss alive). Saved.";
    case PS_PLAYER_INF_NIN: return "ON: your ninjutsu gauges are kept full. Only while a Practice session runs or a Training boss is alive. Saved.";
    case PS_BOSS_SPEED: return "Animation speed of every Training boss (x0.10 .. x3.00, x1.00 = normal). Applies from the boss's next move; attack timing follows. Saved.";
    default: return "Every Training boss loses this % of its max Ki per second (OFF = normal). Stops at one hit from a Ki break; not during a break or with that boss's infinite Ki ON. Saved.";
    }
}
