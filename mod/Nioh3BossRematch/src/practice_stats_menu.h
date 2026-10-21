// user #22389: Practice stats rows. Included by rematch.cpp inside its UI namespace right after boss_vitals_menu.h (same helpers: TrainingExport,
// g_msg, Log, colours). Training side: Nioh3Training practice_stats_game.h (exports Nioh3Training_PracticeStatGet / _PracticeStatSet, constants in
// practice_stats.h). Values are saved in Nioh3Training.ini [Practice]; every one defaults OFF / 1.0x.
//   Practice > Player page : Infinite Ki (stamina), Damage I deal (x0.1 .. x10), Infinite ninjutsu, Player speed.
//   Boss X health and Ki page (below that boss's own infinite health / infinite Ki rows): Damage boss deals (x0 .. x10), Boss Ki drain
//   (Ki lost per second, 0 = OFF), Boss speed (notes/boss-speed-lever-22389.md). These three apply to all Training bosses.
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
    add(kItPstBase + PS_PLAYER_INF_KI); add(kItPstBase + PS_DMG_DEALT); add(kItPstBase + PS_PLAYER_INF_NIN); add(kItPstBase + PS_PLAYER_SPEED);
}
// Practice hub row 'Frame meter' (PS_FRAME_METER, Nioh3Training frame_meter_game.h): an older Training answers -1 and the row says so.
template <class Add> void PstAddHubRows(Add&& add) { if (PstAvailable()) add(kItPstBase + PS_FRAME_METER); }
template <class Add> void PstAddBossRows(Add&& add) { if (PstAvailable()) { add(kItPstBase + PS_DMG_TAKEN); add(kItPstBase + PS_BOSS_KI_DRAIN); add(kItPstBase + PS_BOSS_SPEED); } }
const char* PstLabel(int what) {
    return what == PS_FRAME_METER ? "Frame meter:" : what == PS_BOSS_SPEED ? "Boss speed:" : what == PS_PLAYER_SPEED ? "Player speed:" : what == PS_PLAYER_INF_NIN ? "Infinite ninjutsu:" : what == PS_PLAYER_INF_KI ? "Infinite Ki (stamina):" : what == PS_DMG_DEALT ? "Damage I deal:" : what == PS_DMG_TAKEN ? "Damage boss deals:" : "Boss Ki drain:";
}
void PstValueText(int what, int v, char* buf, size_t n) {
    if (v < 0) snprintf(buf, n, "?");
    else if (PstIsToggle(what)) snprintf(buf, n, "%s", v ? "ON" : "OFF");
    else if (what == PS_BOSS_KI_DRAIN) { if (v) snprintf(buf, n, "%d Ki / s", v); else snprintf(buf, n, "OFF"); }
    else snprintf(buf, n, "x%d.%02d", v / 100, v % 100);
}
// A (dir 0) / Left (-1) / Right (+1). Returns true when the item was ours.
bool PstChange(int item, int dir) {
    if (!IsPstItem(item)) return false;
    const int what = item - kItPstBase, cur = PstGet(what);
    if (cur < 0) { snprintf(g_msg, sizeof g_msg, what == PS_FRAME_METER ? "Needs a Nioh3Training.asi with the frame meter." : "Needs a Nioh3Training.asi with the Practice stats (#22389)."); return true; }
    const int want = PstStep(what, cur, dir);
    if (!PstIsToggle(what) && want == cur && dir) return true;
    const int v = PstSet(what, want);
    char t[24]; PstValueText(what, v, t, sizeof t);
    Log("Rematch: menu Practice %s -> %d (#22389)", PstLabel(what), v);
    if (what == PS_FRAME_METER) snprintf(g_msg, sizeof g_msg, v ? "Frame meter ON: the first Training boss's moves show as frames at the top of the screen (close the menu). Saved." : "Frame meter OFF. Saved.");
    else if (what == PS_PLAYER_INF_KI) snprintf(g_msg, sizeof g_msg, v ? "Infinite Ki ON: your Ki (stamina) is refilled whenever it drops. Practice only. Saved." : "Infinite Ki OFF: normal Ki. Saved.");
    else if (what == PS_PLAYER_INF_NIN) snprintf(g_msg, sizeof g_msg, v ? "Infinite ninjutsu ON: your ninjutsu gauges stay full. Practice only. Saved." : "Infinite ninjutsu OFF. Saved.");
    else if (what == PS_PLAYER_SPEED) snprintf(g_msg, sizeof g_msg, "Player speed %s: your animations run at that rate (from your next move). Practice only. Saved.", t);
    else if (what == PS_BOSS_SPEED) snprintf(g_msg, sizeof g_msg, "Boss speed %s: Training bosses animate at that rate (from their next move). Practice only. Saved.", t);
    else if (what == PS_BOSS_KI_DRAIN) snprintf(g_msg, sizeof g_msg, v ? "Boss Ki drain %s: every Training boss loses that much Ki each second (stops one hit from a Ki break). Saved." : "Boss Ki drain OFF. Saved.", t);
    else snprintf(g_msg, sizeof g_msg, "%s %s (HP damage of hits%s). Practice only. Saved.", PstLabel(what), t, what == PS_DMG_DEALT ? " you land" : " Training bosses land");
    return true;
}
bool PstRowText(int item, bool sel, const char* mk, char* buf, size_t n, uint32_t* color) {
    if (!IsPstItem(item)) return false;
    const int what = item - kItPstBase, v = PstGet(what);
    char t[24]; PstValueText(what, v, t, sizeof t);
    const bool changed = v >= 0 && v != PstDefault(what);
    snprintf(buf, n, "%s %-22s %s   %s", mk, PstLabel(what), t, PstIsToggle(what) ? "A / Left / Right" : "Left / Right");
    *color = sel ? cWhite : v < 0 ? cDim : changed ? (PstIsToggle(what) ? cGreen : cYellow) : cGray;
    return true;
}
const char* PstFooter(int item) {
    if (!IsPstItem(item)) return nullptr;
    switch (item - kItPstBase) {
    case PS_PLAYER_INF_KI: return "ON: your Ki (stamina) is refilled to full whenever it drops. Only while a Practice session runs or a Training boss is alive. Saved.";
    case PS_DMG_DEALT: return "Multiplies the HP damage of the hits you land (x0.10 .. x10). x1.00 = normal. Ki damage is not changed. Practice only (session / Training boss alive). Saved.";
    case PS_DMG_TAKEN: return "Multiplies the HP damage of every Training boss's hits (x0 = none .. x10). x1.00 = normal. Ki damage is not changed. Practice only. Saved.";
    case PS_PLAYER_INF_NIN: return "ON: your ninjutsu gauges are kept full. Only while a Practice session runs or a Training boss is alive. Saved.";
    case PS_PLAYER_SPEED: return "Animation speed of your character (x0.10 .. x3.00, x1.00 = normal). Applies from your next move; attacks and dodges follow. Only while a Practice session runs or a Training boss is alive. Saved.";
    case PS_FRAME_METER: return "ON: SF6-style frame meter for the first Training boss (1 cell = 1 frame at 60 fps, last 6 s): green startup, red active, purple unblockable, blue recovery, plus the numbers. Saved.";
    case PS_BOSS_SPEED: return "Animation speed of every Training boss (x0.10 .. x3.00, x1.00 = normal). Applies from the boss's next move; attack timing follows. Saved.";
    default: return "Every Training boss loses this much Ki per second (OFF = normal; a boss has about 1000 Ki). Stops at one hit from a Ki break; not during a break or with that boss's infinite Ki ON. Saved.";
    }
}
