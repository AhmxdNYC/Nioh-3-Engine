// user #22389: Practice stats rows. Included by rematch.cpp inside its UI namespace right after boss_vitals_menu.h (same helpers: TrainingExport,
// g_msg, Log, colours). Training side: Nioh3Training practice_stats_game.h (exports Nioh3Training_PracticeStatGet / _PracticeStatSet, constants in
// practice_stats.h). Values are saved in Nioh3Training.ini [Practice]; every one defaults OFF / 1.0x (Negative Ki meter: ON).
//   Practice > Player page : Infinite Ki (stamina), Damage I deal (x0.1 .. x10), Ki damage I deal (x0 .. x10, user #22588), Infinite ninjutsu, Player speed.
//   Boss X health and Ki page (below that boss's own infinite health / infinite Ki rows): Damage boss deals (x0 .. x10), Boss Ki drain
//   (Ki lost per second, 0 = OFF), One-hit Ki break (user #22588), Hold Ki break (user #22918), then (user #23013) Hold Ki break v2, Ki floor depth and
//   Ki break log, (user #23400) Negative Ki meter, Boss speed (notes/boss-speed-lever-22389.md). These apply to all Training bosses.
//   Time of day / Weather are global settings in EngineFix Display Overlays (#38871), not Practice hub rows.
#include "../../Nioh3Training/src/practice_stats.h"
constexpr int kItPstBase = 0xD0000;  // kItPstBase + PS_* (positive ids like kItBvOpenBase; no other range uses 0xD0000)
constexpr int kItPstResetFight = kItPstBase + 0x80;  // user #22592: 'Reset fight' action row on the Practice hub (not a PS_* setting)
constexpr int kItPstFreeze = kItPstBase + 0x81;  // user #22589: 'Freeze fight' toggle (Nioh3Training practice_freeze_game.h, not saved)
constexpr int kItPstStep = kItPstBase + 0x82;    // user #22589: 'Step 1 frame' action row
constexpr int kItPstAllRows = kItPstBase + 0x83;  // human #38180: master for the frame-meter rows (boss / player / i-frames)
constexpr int kItPstAllKeys = kItPstBase + 0x84;  // human #38180: master for the KEY (legend) and every KEY entry group
bool IsPstItem(int item) { return (item >= kItPstBase && item < kItPstBase + PS_N) || item == kItPstResetFight || item == kItPstFreeze || item == kItPstStep || item == kItPstAllRows || item == kItPstAllKeys; }
// the settings each master covers; value 1 = all ON, 0 = all OFF, 2 = mixed, -1 = Nioh3Training too old
constexpr int kPstRowSet[] = {PS_FRAME_METER, PS_PLAYER_METER, PS_FRAME_PROW, PS_FRAME_IFR};
constexpr int kPstKeySet[] = {PS_FRAME_LEGEND, PS_KEY_STARTUP, PS_KEY_ACTIVE, PS_KEY_RECOVERY, PS_KEY_UNBLOCK, PS_KEY_GRAB, PS_KEY_OTHER, PS_KEY_TIMING, PS_KEY_KI, PS_KEY_IFR};
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
    add(kItPstBase + PS_PLAYER_INF_KI); add(kItPstBase + PS_PLAYER_INF_NIN); add(kItPstBase + PS_PLAYER_INF_SOUL); add(kItPstBase + PS_DMG_DEALT); add(kItPstBase + PS_KI_DEALT); add(kItPstBase + PS_PLAYER_SPEED);  // Player frame meter: Display Overlays only
}
// Practice hub rows. The frame meter switch, frame rows and KEY entries live only in the EngineFix menu's
// Display Overlays > Frame meter settings page (human request: frame meter settings out of Practice).
template <class Add> void PstAddHubRows(Add&& add) {
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
    else if (!rc) snprintf(g_msg, sizeof g_msg, "Nothing to reset: start a Training session or summon a Training boss first.");
    else {
        snprintf(g_msg, sizeof g_msg, "Fight reset: bosses %s, you %s%s.", (rc & 1) ? "at full HP and Ki" : "(none alive)", (rc & 4) ? "at full HP and Ki" : "unchanged", (rc & 8) ? ", back at your session start" : " (no session start recorded, your position is unchanged)");
        ToastF(cGreen, 2000, "Fight reset.");
    }
}
template <class Add> void PstAddBossRows(Add&& add) { if (PstAvailable()) { add(kItPstBase + PS_DMG_TAKEN); add(kItPstBase + PS_BOSS_KI_DRAIN); add(kItPstBase + PS_ONE_HIT_KI); add(kItPstBase + PS_HOLD_KI_BREAK); add(kItPstBase + PS_HOLD_KI_V2); add(kItPstBase + PS_KI_FLOOR); add(kItPstBase + PS_KI_LOG); add(kItPstBase + PS_NEG_KI_METER); add(kItPstBase + PS_BOSS_SPEED); } }
int PstMasterState(const int* set, int n) {
    int on = 0, off = 0;
    for (int i = 0; i < n; ++i) { const int v = PstGet(set[i]); if (v < 0) return -1; (v ? on : off)++; }
    return !off ? 1 : !on ? 0 : 2;
}
const char* PstLabel(int what) {
    if (what == PS_PLAYER_INF_SOUL) return "Infinite soul cores:";  // pc #57367 (before the PS_KEY_STARTUP.. table: it is past that range)
    if (what == PS_FRAME_SIZE) return "Ring size:";
    if (what == PS_FRAME_STYLE) return "Frame meter style:";
    if (what == PS_FRAME_PROW) return "Player attack lane:";  // human #38180
    if (what == PS_FRAME_IFR) return "Invincibility lane:";
    if (what >= PS_KEY_STARTUP) { static const char* const k[] = {"Key: Startup:", "Key: Active:", "Key: Recovery:", "Key: Unblockable:", "Key: Grab:", "Key: No hit/Pending/Empty:", "Key: Action start/Total/Stun:", "Key: Ki Pulse/Ki debt/Perfect:", "Key: I-frames:"}; return k[what - PS_KEY_STARTUP]; }
    if (what == PS_HOLD_KI_V2) return "Hold with negative Ki:";  // user #23013
    if (what == PS_KI_FLOOR) return "Negative Ki depth:";
    if (what == PS_KI_LOG) return "Log Ki breaks:";
    if (what == PS_NEG_KI_METER) return "Negative Ki meter:";  // user #23400
    return what == PS_TIME_OF_DAY ? "Time of day:" : what == PS_WEATHER ? "Weather:" : what == PS_HOLD_KI_BREAK ? "Hold Ki break:" : what == PS_PLAYER_METER ? "Player frame meter:" : what == PS_KI_DEALT ? "Player Ki damage:" : what == PS_ONE_HIT_KI ? "One-hit Ki break:" : what == PS_FRAME_METER ? "Frame meter:" : what == PS_BOSS_SPEED ? "Boss animation speed:" : what == PS_PLAYER_SPEED ? "Player animation speed:" : what == PS_PLAYER_INF_NIN ? "Infinite ninjutsu:" : what == PS_PLAYER_INF_KI ? "Infinite Ki:" : what == PS_DMG_DEALT ? "Player damage:" : what == PS_DMG_TAKEN ? "Boss damage:" : "Boss Ki drain:";
}
void PstValueText(int what, int v, char* buf, size_t n) {
    if (v < 0) snprintf(buf, n, "?");
    else if (what == PS_FRAME_SIZE) snprintf(buf,n,"%s",v==0?"Small":v==1?"Medium":"Large");
    else if (what == PS_FRAME_STYLE) snprintf(buf, n, "%s", v ? "Ring" : "Row");
    else if (PstIsToggle(what)) snprintf(buf, n, "%s", v ? "ON" : "OFF");
    else if (what == PS_TIME_OF_DAY) { const int m = PstTodMinutes(v); if (m < 0) snprintf(buf, n, "Stock"); else snprintf(buf, n, "%02d:%02d", m / 60, m % 60); }
    else if (what == PS_WEATHER) snprintf(buf, n, "%s", PstWeatherName(v));
    else if (what == PS_BOSS_KI_DRAIN) { if (v) snprintf(buf, n, "%d Ki / s", v); else snprintf(buf, n, "OFF"); }
    else snprintf(buf, n, "%d %%", v);
}
// A (dir 0) / Left (-1) / Right (+1). Returns true when the item was ours.
bool PstChange(int item, int dir) {
    if (!IsPstItem(item)) return false;
    if (item == kItPstAllRows || item == kItPstAllKeys) {  // human #38180: set every setting of the group
        const bool rows = item == kItPstAllRows;
        const int* set = rows ? kPstRowSet : kPstKeySet; const int n = rows ? (int)(sizeof kPstRowSet / sizeof *kPstRowSet) : (int)(sizeof kPstKeySet / sizeof *kPstKeySet);
        const int st = PstMasterState(set, n);
        if (st < 0) { snprintf(g_msg, sizeof g_msg, "Needs a newer Nioh3Training.asi."); return true; }
        const int want = dir > 0 ? 1 : dir < 0 ? 0 : st == 1 ? 0 : 1;
        for (int i = 0; i < n; ++i) PstSet(set[i], want);
        Log("Rematch: menu %s -> %d (#38180)", rows ? "All frame rows" : "All key indicators", want);
        snprintf(g_msg, sizeof g_msg, rows ? "All meter lanes %s (boss, player, invincibility). Saved." : "All legend entries %s. Applies to Row and Ring styles. Saved.", want ? "ON" : "OFF");
        return true;
    }
    if (item == kItPstResetFight) { if (!dir) PstResetFight(); return true; }  // action row: Left / Right do nothing
    if (item == kItPstFreeze || item == kItPstStep) {  // user #22589
        char kf[16], ks[16]; PfzKey(0, kf, sizeof kf); PfzKey(1, ks, sizeof ks);
        const int op = item == kItPstStep ? (dir ? -9 : 2) : dir > 0 ? 1 : dir < 0 ? 0 : -1;
        if (op == -9) return true;
        const int r = PfzCall(op);
        Log("Rematch: menu Practice freeze op %d -> %d (#22589)", op, r);
        if (r == -1) snprintf(g_msg, sizeof g_msg, "Needs a Nioh3Training.asi with the Practice freeze.");
        else if (r == -2) snprintf(g_msg, sizeof g_msg, "Freeze works in a Training session or with a Training boss alive.");
        else if (item == kItPstStep && (r & 1)) snprintf(g_msg, sizeof g_msg, "Step 1 frame requested (was +%d since the freeze). %s = step in game, hold = repeat; the meter shows it with the menu closed.", r >> 8, ks);
        else if (item == kItPstStep) snprintf(g_msg, sizeof g_msg, "Fight freezing first; press again to step 1 frame. %s = step in game.", ks);
        else snprintf(g_msg, sizeof g_msg, (r & 2) ? "Fight FROZEN (boss + you). Close the menu: %s = step 1 frame (hold = repeat), %s = resume. Auto-released on a load, boss despawn or death." : "Fight running again (%s / %s).", ks, kf);
        return true;
    }
    const int what = item - kItPstBase, cur = PstGet(what);
    if (cur < 0) { snprintf(g_msg, sizeof g_msg, what == PS_FRAME_METER ? "Needs a Nioh3Training.asi with the frame meter." : "Update Nioh3Training.asi to use player and boss settings."); return true; }
    const int want = PstStep(what, cur, dir);
    if (!PstIsToggle(what) && want == cur && dir) return true;
    const int v = PstSet(what, want);
    char t[24]; PstValueText(what, v, t, sizeof t);
    Log("Rematch: menu Practice %s -> %d (#22389)", PstLabel(what), v);
    if (what == PS_PLAYER_INF_SOUL) snprintf(g_msg, sizeof g_msg, v ? "Infinite soul cores ON: soul-core uses are not used up, anywhere. Saved." : "Infinite soul cores OFF: normal uses. Saved.");
    else if (what == PS_FRAME_PROW || what == PS_FRAME_IFR || what >= PS_KEY_STARTUP) snprintf(g_msg, sizeof g_msg, "%s %s. Saved.", PstLabel(what), t);
    else if (what == PS_PLAYER_METER) snprintf(g_msg, sizeof g_msg, v ? "Player frame meter ON: show player timing in the selected Row or Ring style. Close the menu to view. Saved." : "Player frame meter OFF. Saved.");
    else if (what == PS_FRAME_METER) snprintf(g_msg, sizeof g_msg, v ? "Frame meter ON: show the first spawned boss in the selected Row or Ring style. Close the menu to view. Saved." : "Frame meter OFF. Saved.");
    else if (what == PS_PLAYER_INF_KI) snprintf(g_msg, sizeof g_msg, v ? "Infinite Ki ON: keep your Ki full in Training and spawned Boss Matches. Saved." : "Infinite Ki OFF: normal Ki. Saved.");
    else if (what == PS_KI_DEALT) snprintf(g_msg, sizeof g_msg, "Player Ki damage %s: combines with Player damage in Training and spawned Boss Matches. Saved.", t);
    else if (what == PS_TIME_OF_DAY) snprintf(g_msg, sizeof g_msg, v ? "Sky time %s during gameplay. Stock restores the area time. Saved." : "Time of day Stock: the area's own time again. Saved.", t);
    else if (what == PS_WEATHER) snprintf(g_msg, sizeof g_msg, v ? "Weather %s during gameplay. Stock restores the area weather. Saved." : "Weather Stock: the area's own weather again. Saved.", t);
    else if (what == PS_HOLD_KI_BREAK) snprintf(g_msg, sizeof g_msg, v ? "Hold Ki break ON: hold broken bosses and stop attacks. Turn OFF to release; grapples or death can end the break. Saved for Training; not applied during Boss Matches." : "Hold Ki break OFF: a held boss recovers normally now. Saved for Training; not applied during Boss Matches.");
    else if (what == PS_HOLD_KI_V2) snprintf(g_msg, sizeof g_msg, v ? "Hold with negative Ki ON: use the Ki floor to hold breaks. Needs Hold Ki break ON. Experimental. Saved for Training; not applied during Boss Matches." : "Hold with negative Ki OFF: use the timer hold. Saved for Training; not applied during Boss Matches.");
    else if (what == PS_KI_FLOOR) snprintf(g_msg, sizeof g_msg, v == 100 ? "Negative Ki depth %s: normal depth for spawned bosses. Saved for Training; not applied during Boss Matches." : "Negative Ki depth %s of normal for spawned bosses. Experimental. Saved for Training; not applied during Boss Matches.", t);
    else if (what == PS_NEG_KI_METER) snprintf(g_msg, sizeof g_msg, v ? "Negative Ki meter ON: when a Training boss's Ki goes below 0, a thin red bar under its Ki bar shows how deep (close the menu). Saved." : "Negative Ki meter OFF. Saved.");
    else if (what == PS_KI_LOG) snprintf(g_msg, sizeof g_msg, v ? "Ki break log ON: Ki-break details of Training bosses go to Nioh3Training.log (KILOG lines). Saved." : "Ki break log OFF. Saved.");
    else if (what == PS_ONE_HIT_KI) snprintf(g_msg, sizeof g_msg, v ? "One-hit Ki break ON: eligible hits empty spawned bosses' Ki. Saved for Training; not applied during Boss Matches." : "One-hit Ki break OFF. Saved for Training; not applied during Boss Matches.");
    else if (what == PS_PLAYER_INF_NIN) snprintf(g_msg, sizeof g_msg, v ? "Infinite ninjutsu ON: gauges stay full in Training and spawned Boss Matches. Saved." : "Infinite ninjutsu OFF. Saved.");
    else if (what == PS_PLAYER_SPEED) snprintf(g_msg, sizeof g_msg, "Player animation speed %s, from your next move. Training / spawned Boss Matches. Saved.", t);
    else if (what == PS_BOSS_SPEED) snprintf(g_msg, sizeof g_msg, "Boss animation speed %s, from their next move. All spawned bosses. Saved for Training; not applied during Boss Matches.", t);
    else if (what == PS_BOSS_KI_DRAIN) snprintf(g_msg, sizeof g_msg, v ? "Boss Ki drain %s: all spawned bosses lose Ki, stopping just above zero. Saved for Training; not applied during Boss Matches." : "Boss Ki drain OFF. Saved for Training; not applied during Boss Matches.", t);
    else snprintf(g_msg, sizeof g_msg, "%s %s: scales HP and Ki damage of hits%s. Training / spawned Boss Matches. Saved.", PstLabel(what), t, what == PS_DMG_DEALT ? " you land" : " spawned bosses land");
    return true;
}
bool PstRowText(int item, bool sel, const char* mk, char* buf, size_t n, uint32_t* color) {
    if (!IsPstItem(item)) return false;
    if (item == kItPstAllRows || item == kItPstAllKeys) {
        const bool rows = item == kItPstAllRows;
        const int st = rows ? PstMasterState(kPstRowSet, (int)(sizeof kPstRowSet / sizeof *kPstRowSet)) : PstMasterState(kPstKeySet, (int)(sizeof kPstKeySet / sizeof *kPstKeySet));
        snprintf(buf, n, "%s %-22s %s   A / Left / Right", mk, rows ? "All meter lanes:" : "All legend entries:", st < 0 ? "?" : st == 2 ? "MIXED" : st ? "ON" : "OFF");
        *color = sel ? cWhite : st < 0 ? cDim : st == 1 ? cGray : cGreen;
        return true;
    }
    if (item == kItPstResetFight) { snprintf(buf, n, "%s Reset fight   A: refill and return to start", mk); *color = sel ? cWhite : cGreen; return true; }
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
    *color = sel ? cWhite : v < 0 ? cDim : changed && !PstIsToggle(what) ? cYellow : cGray;
    return true;
}
const char* PstFooter(int item) {
    if (!IsPstItem(item)) return nullptr;
    if (item == kItPstFreeze || item == kItPstStep) {  // user #22589
        static char f[400]; char kf[16], ks[16]; PfzKey(0, kf, sizeof kf); PfzKey(1, ks, sizeof ks);
        if (item == kItPstFreeze) snprintf(f, sizeof f, "Training / spawned Boss Matches: pause the fight. %s toggles pause; %s advances one game frame (hold to repeat). Released on loading or session end. Not saved.", kf, ks);
        else snprintf(f, sizeof f, "Advance one game frame, then pause again. %s in game; hold to repeat. If the fight is running, the first press pauses it. Training / spawned Boss Matches.", ks);
        return f;
    }
    if (item == kItPstAllRows) return "Show or hide the boss, player, and invincibility lanes together. Individual lane switches work in Row and Ring styles. Saved.";
    if (item == kItPstAllKeys) return "Show or hide the frame-meter legend and all its entries. Works in Row and Ring styles. Saved.";
    if (item - kItPstBase == PS_FRAME_PROW) return "Show player attack timing. Hiding this lane keeps the invincibility lane available. Requires Player frame meter ON. Row and Ring styles. Saved.";
    if (item - kItPstBase == PS_FRAME_IFR) return "Show a separate gold lane for player invincibility frames. Requires Player frame meter ON. Row and Ring styles. Saved.";
    if (item - kItPstBase >= PS_KEY_STARTUP && item - kItPstBase <= PS_KEY_IFR) return "Show or hide this legend entry. Meter colors stay unchanged. Applies to Row and Ring styles. Saved.";
    if (item == kItPstResetFight) return "Training / spawned Boss Matches: refill player and boss HP/Ki and reset boss positions. You return to the session start if one is recorded.";
    switch (item - kItPstBase) {
    case PS_FRAME_STYLE: return "Choose straight rows or concentric rings. Each lane keeps its own visibility switch. Display only. Saved.";
    case PS_FRAME_SIZE: return "Ring size: Small, Medium, or Large. Applies only to Ring style; timing and lane settings stay the same. Saved.";
    case PS_PLAYER_INF_KI: return "Training / spawned Boss Matches: keep your Ki full. Does not apply to ordinary story fights. Saved.";
    case PS_DMG_DEALT: return "Training / spawned Boss Matches: scale your HP and Ki damage. 100% = normal. Ki damage also uses Player Ki damage, with a combined 1000% cap. Saved.";
    case PS_DMG_TAKEN: return "All spawned bosses in Training / Boss Matches: scale HP damage and Ki damage against you. 100% = normal; 0% = none. Saved.";
    case PS_KI_DEALT: return "Training / spawned Boss Matches: extra multiplier for your Ki damage only. 100% = normal; 0% = none. Combines with Player damage, capped at 1000%. Saved.";
    case PS_ONE_HIT_KI: return "All spawned bosses: your next eligible hit empties boss Ki. Skips bosses already broken, dying, or using infinite Ki. Saved for Training; not applied during Boss Matches.";
    case PS_TIME_OF_DAY: return "Global sky time during gameplay. Stock uses the area time; Left / Right changes it by 30 minutes. Cutscenes keep their lighting. Also in Display Overlays. Saved.";
    case PS_WEATHER: return "Global gameplay weather: Stock, Clear, Overcast, Rain, Storm, or Snow. Changes sky and weather effects; cutscenes keep their settings. Saved.";
    case PS_HOLD_KI_BREAK: return "All spawned bosses: hold an existing Ki break and stop attacks until OFF or death. A grapple can end the break. Does not cause a break. Saved for Training; not applied during Boss Matches.";
    case PS_HOLD_KI_V2: return "Experimental hold method. Requires Hold Ki break ON; keeps Ki at the negative floor and blocks regeneration. OFF uses the timer hold. All spawned bosses. Saved for Training; not applied during Boss Matches.";
    case PS_KI_FLOOR: return "All spawned bosses: negative Ki limit. 100% = normal depth; 0% = no negative Ki; larger values allow deeper breaks. Experimental. Saved for Training; not applied during Boss Matches.";
    case PS_NEG_KI_METER: return "Show negative Ki below spawned bosses' Ki bars. Appears only below zero; shows current Ki, reserve, and floor. Display only; OFF by default. Saved.";
    case PS_KI_LOG: return "Write spawned bosses' Ki-break details to Nioh3Training.log every 0.1 s and on hits. Diagnostic only; does not change the fight. Saved.";
    case PS_PLAYER_INF_SOUL: return "Soul cores (d-pad) are not used up: each use is put back right after the throw. Works everywhere, for testing. Other items are not touched. Saved.";
    case PS_PLAYER_INF_NIN: return "Training / spawned Boss Matches: keep your ninjutsu gauges full. Does not apply to ordinary story fights. Saved.";
    case PS_PLAYER_SPEED: return "Training / spawned Boss Matches: your animation speed, including attacks and dodges. 100% = normal; range 10-300%. Applies from your next move. Saved.";
    case PS_PLAYER_METER: return "Show player attack timing in the frame meter. One cell = one game frame at 60 fps. Choose Row or Ring and individual lanes in Mod menu > Display Overlays. Saved.";
    case PS_FRAME_METER: return "Show timing for the first spawned boss. One cell = one game frame at 60 fps. Row/Ring style, lanes, and legend: Mod menu > Display Overlays. Saved.";
    case PS_BOSS_SPEED: return "All spawned bosses in Training: animation speed, including attack timing. 100% = normal; range 10-300%. Applies from their next move. Saved for Training; not applied during Boss Matches.";
    default: return "All spawned bosses: Ki lost per second. Stops just above zero; skips Ki breaks and infinite Ki. OFF = no extra drain. Saved for Training; not applied during Boss Matches.";
    }
}
