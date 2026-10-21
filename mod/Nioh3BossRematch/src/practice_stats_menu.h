// user #22389: Practice stats rows. Included by rematch.cpp inside its UI namespace right after boss_vitals_menu.h (same helpers: TrainingExport,
// g_msg, Log, colours). Training side: Nioh3Training practice_stats_game.h (exports Nioh3Training_PracticeStatGet / _PracticeStatSet, constants in
// practice_stats.h). Values are saved in Nioh3Training.ini [Practice]; every one defaults OFF / 1.0x.
//   Practice > Player page : Infinite Ki (stamina), Damage I deal (x0.1 .. x10), Infinite ninjutsu, Player speed.
//   Boss X health and Ki page (below that boss's own infinite health / infinite Ki rows): Damage boss deals (x0 .. x10), Boss Ki drain
//   (Ki lost per second, 0 = OFF), Boss speed (notes/boss-speed-lever-22389.md). These three apply to all Training bosses.
#include "../../Nioh3Training/src/practice_stats.h"
constexpr int kItPstBase = 0xD0000;  // kItPstBase + PS_* (positive ids like kItBvOpenBase; no other range uses 0xD0000)
constexpr int kItPstResetFight = kItPstBase + 0x80;  // user #22592: 'Reset fight' action row on the Practice hub (not a PS_* setting)
constexpr int kItPstFreeze = kItPstBase + 0x81;  // user #22589: 'Freeze fight' toggle (Nioh3Training practice_freeze_game.h, not saved)
constexpr int kItPstStep = kItPstBase + 0x82;    // user #22589: 'Step 1 frame' action row
bool IsPstItem(int item) { return (item >= kItPstBase && item < kItPstBase + PS_N) || item == kItPstResetFight || item == kItPstFreeze || item == kItPstStep; }
// Practice freeze export: op 0 off, 1 on, -1 toggle, 2 step, 3 query -> -2 not live, else bit 0 frozen, bit 1 requested, bit 2 step ok, frames << 8.
int PfzCall(int op) { using F = int (*)(int); const auto f = (F)TrainingExport("Nioh3Training_PracticeFreeze"); return f ? f(op) : -1; }
const char* PfzKey(int which, char* b, size_t n) {  // which 0 freeze, 1 step
    using F = int (*)(); const auto f = (F)TrainingExport("Nioh3Training_PracticeFreezeKeys");
    const int vk = f ? ((f() >> (which * 8)) & 0xFF) : 0;
    if (vk >= 0x70 && vk <= 0x87) snprintf(b, n, "F%d", vk - 0x6F); else if (vk) snprintf(b, n, "key 0x%X", vk); else snprintf(b, n, "no key");
    return b;
}
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
template <class Add> void PstAddHubRows(Add&& add) {
    if (PstAvailable()) add(kItPstBase + PS_FRAME_METER);
    if (TrainingExport("Nioh3Training_PracticeFreeze")) { add(kItPstFreeze); add(kItPstStep); }  // user #22589
    if (TrainingExport("Nioh3Training_ResetFight")) add(kItPstResetFight);
}
// 'Reset fight': bosses full HP/Ki + back at their start, player full HP/Ki + back at the Practice start (Nioh3Training_ResetFight, bit mask 1/2/4/8).
void PstResetFight() {
    using F = int (*)();
    const auto f = (F)TrainingExport("Nioh3Training_ResetFight");
    const int rc = f ? f() : -1;
    Log("Rematch: menu Reset fight -> %d (#22592)", rc);
    if (rc < 0) snprintf(g_msg, sizeof g_msg, "Needs a Nioh3Training.asi with 'Reset fight'.");
    else if (!rc) snprintf(g_msg, sizeof g_msg, "Nothing to reset: start a Practice session or summon a Training boss first.");
    else {
        snprintf(g_msg, sizeof g_msg, "Fight reset: bosses %s, you %s%s.", (rc & 1) ? "at full HP and Ki" : "(none alive)", (rc & 4) ? "at full HP and Ki" : "unchanged", (rc & 8) ? ", back at your Practice start" : " (no Practice start recorded, your position is unchanged)");
        ToastF(cGreen, 2000, "Fight reset.");
    }
}
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
    if (item == kItPstResetFight) { if (!dir) PstResetFight(); return true; }  // action row: Left / Right do nothing
    if (item == kItPstFreeze || item == kItPstStep) {  // user #22589
        char kf[16], ks[16]; PfzKey(0, kf, sizeof kf); PfzKey(1, ks, sizeof ks);
        const int op = item == kItPstStep ? (dir ? -9 : 2) : dir > 0 ? 1 : dir < 0 ? 0 : -1;
        if (op == -9) return true;
        const int r = PfzCall(op);
        Log("Rematch: menu Practice freeze op %d -> %d (#22589)", op, r);
        if (r == -1) snprintf(g_msg, sizeof g_msg, "Needs a Nioh3Training.asi with the Practice freeze.");
        else if (r == -2) snprintf(g_msg, sizeof g_msg, "Freeze works in a Practice session or with a Training boss alive.");
        else if (item == kItPstStep && (r & 1)) snprintf(g_msg, sizeof g_msg, "Step 1 frame requested (was +%d since the freeze). %s = step in game, hold = repeat; the meter shows it with the menu closed.", r >> 8, ks);
        else if (item == kItPstStep) snprintf(g_msg, sizeof g_msg, "Fight freezing first; press again to step 1 frame. %s = step in game.", ks);
        else snprintf(g_msg, sizeof g_msg, (r & 2) ? "Fight FROZEN (boss + you). Close the menu: %s = step 1 frame (hold = repeat), %s = resume. Auto-released on a load, boss despawn or death." : "Fight running again (%s / %s).", ks, kf);
        return true;
    }
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
    if (item == kItPstResetFight) { snprintf(buf, n, "%s Reset fight (bosses + you: full HP and Ki, back to start)   press A", mk); *color = sel ? cWhite : cGreen; return true; }
    if (item == kItPstFreeze || item == kItPstStep) {  // user #22589
        const int r = PfzCall(3); char kf[16], ks[16]; PfzKey(0, kf, sizeof kf); PfzKey(1, ks, sizeof ks);
        if (item == kItPstFreeze) snprintf(buf, n, "%s %-22s %s   A / Left / Right  (%s)", mk, "Freeze fight:", r < 0 ? "?" : (r & 1) ? "FROZEN" : (r & 2) ? "freezing..." : "OFF", kf);
        else if (r >= 0 && (r & 1)) snprintf(buf, n, "%s Step 1 frame   press A  (%s, hold = repeat)   now +%d frames", mk, ks, r >> 8);
        else snprintf(buf, n, "%s Step 1 frame   press A  (%s; freezes first)", mk, ks);
        *color = sel ? cWhite : r < 0 ? cDim : (r & 1) ? cYellow : cGray;
        return true;
    }
    const int what = item - kItPstBase, v = PstGet(what);
    char t[24]; PstValueText(what, v, t, sizeof t);
    const bool changed = v >= 0 && v != PstDefault(what);
    snprintf(buf, n, "%s %-22s %s   %s", mk, PstLabel(what), t, PstIsToggle(what) ? "A / Left / Right" : "Left / Right");
    *color = sel ? cWhite : v < 0 ? cDim : changed ? (PstIsToggle(what) ? cGreen : cYellow) : cGray;
    return true;
}
const char* PstFooter(int item) {
    if (!IsPstItem(item)) return nullptr;
    if (item == kItPstFreeze || item == kItPstStep) {  // user #22589
        static char f[400]; char kf[16], ks[16]; PfzKey(0, kf, sizeof kf); PfzKey(1, ks, sizeof ks);
        if (item == kItPstFreeze) snprintf(f, sizeof f, "Freezes the whole fight (Training bosses + you) with the game's own pause. %s toggles it in game, %s steps 1 frame (1/60 s of game time; hold = repeat). The frame meter stops and counts +1 per step. Not saved; released on a load, boss despawn / death or session end.", kf, ks);
        else snprintf(f, sizeof f, "While frozen: advances the game by exactly one frame (1/60 s of game time = 1 meter cell), then freezes again. %s in game (hold = repeat). Pressed while running it freezes first.", ks);
        return f;
    }
    if (item == kItPstResetFight) return "One press: every Training boss gets full HP and Ki and goes back to its start point; you get full HP and Ki and go back to where the Practice session started.";
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
