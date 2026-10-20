// user #14320 / #14323 / #14324: per-boss HP / Ki page (one page per boss: Boss A, Boss B, ...). Included by rematch.cpp inside its UI namespace
// (after the Practice helpers: TrainingExport, g_msg, ToastF, SlotChr, SlotBossName, g_uiPages). Training side: Nioh3Training boss_vitals_game.h
// (exports Nioh3Training_BossVitalsGet / _BossVitalsSet; shared constants in boss_vitals.h). Settings are per boss slot, session only, default OFF.
// Rows on the page: live HP / Ki, Boss infinite health, Boss infinite Ki (optional, so Ki breaks still happen), Set boss HP %, Set boss Ki %, Refill boss.
#include "../../Nioh3Training/src/boss_vitals.h"
constexpr int uBossVitals = 930;
constexpr int kItBvOpenBase = 0xC0000;  // kItBvOpenBase + slot: 'Health and Ki' row -> uBossVitals for that slot (#14309: 26 letters, so positive ids)
constexpr int kItBvRefill = -94, kItBvStatus = -95, kItBvInfHp = -96, kItBvInfKi = -97, kItBvHpPct = -98, kItBvKiPct = -99;
int g_bvSlot = 0;
bool BvAvailable() { return TrainingExport("Nioh3Training_BossVitalsSet") != nullptr && TrainingExport("Nioh3Training_BossVitalsGet") != nullptr; }
bool IsBvOpenItem(int item) { return item >= kItBvOpenBase && item < kItBvOpenBase + BV_SLOTS; }
bool IsBvPageItem(int item) { return item <= kItBvRefill && item >= kItBvKiPct; }
struct BvView { int live = -1; uint64_t hc = 0, hm = 0; float kc = 0, km = 0; int cfg[4] = {0, 0, 100, 100}; };
BvView BvRead(int slot) {
    BvView v;
    using Fn = int (*)(int, uint64_t*, uint64_t*, float*, float*, int*);
    if (const auto f = (Fn)TrainingExport("Nioh3Training_BossVitalsGet")) v.live = f(slot, &v.hc, &v.hm, &v.kc, &v.km, v.cfg);
    return v;
}
int BvSet(int slot, int what, int value) {
    using Fn = int (*)(int, int, int);
    const auto f = (Fn)TrainingExport("Nioh3Training_BossVitalsSet");
    return f ? f(slot, what, value) : -1;
}
// open rows: one per Training slot (A / B today); on the gate page only the live ones
template <class Add> void BvAddOpenRows(Add&& add, bool liveOnly) {
    if (!BvAvailable()) return;
    for (int s = 0; s < BV_SLOTS; ++s) if (!liveOnly || SlotChr(s)) add(kItBvOpenBase + s);
}
template <class Add> void BvAddPage(Add&& add) {
    add(kItBvStatus); add(kItBvInfHp); add(kItBvInfKi); add(kItBvHpPct); add(kItBvKiPct); add(kItBvRefill);
}
#define BV_NEED_BOSS "Boss %c is gone (%c: no live boss with this letter). Practice > Training Bosses lists the bosses you have."
// A (dir 0) / Left (-1) / Right (+1). Returns true when the item was ours.
bool BvChange(int item, int dir) {
    if (!IsBvPageItem(item) || item == kItBvStatus) return IsBvPageItem(item);
    const int s = g_bvSlot;
    const BvView v = BvRead(s);
    if (v.live < 0) { snprintf(g_msg, sizeof g_msg, "Needs a Nioh3Training.asi with the boss health / Ki controls."); return true; }
    const char slotCh = (char)('A' + s);
    if (item == kItBvInfHp || item == kItBvInfKi) {
        const int hp = item == kItBvInfHp, cur = v.cfg[hp ? 0 : 1];
        const int on = BvSet(s, hp ? BV_INF_HP : BV_INF_KI, dir ? dir > 0 : !cur);
        Log("Rematch: menu Boss %c infinite %s -> %d (#14320)", slotCh, hp ? "health" : "Ki", on);
        if (hp) snprintf(g_msg, sizeof g_msg, on ? "Boss %c infinite health ON: its HP never stays below %d %% (Set boss HP %%). This session only." : "Boss %c infinite health OFF: it takes damage normally.", slotCh, v.cfg[2]);
        else snprintf(g_msg, sizeof g_msg, on ? "Boss %c infinite Ki ON: its Ki never stays below %d %%, so it is not Ki broken (a break already running plays out). This session only." : "Boss %c infinite Ki OFF: normal Ki, Ki breaks happen as usual.", slotCh, v.cfg[3]);
        return true;
    }
    if (item == kItBvHpPct || item == kItBvKiPct) {
        const int hp = item == kItBvHpPct, lo = hp ? 1 : 0;
        const int pct = BvStepPct(v.cfg[hp ? 2 : 3], dir, lo);  // A (dir 0) re-applies the shown value
        const int r = BvSet(s, hp ? BV_HP_PCT : BV_KI_PCT, pct);
        Log("Rematch: menu Boss %c set %s %% -> %d (#14320)", slotCh, hp ? "HP" : "Ki", r);
        if (v.live == 0) snprintf(g_msg, sizeof g_msg, BV_NEED_BOSS, slotCh, slotCh);
        else if (hp) snprintf(g_msg, sizeof g_msg, "Boss %c HP set to %d %%%s.", slotCh, r, v.cfg[0] ? " (infinite health now holds it there)" : "");
        else snprintf(g_msg, sizeof g_msg, "Boss %c Ki set to %d %%%s%s.", slotCh, r, r == 0 ? " (one hit from a Ki break)" : "", v.cfg[1] ? " (infinite Ki now holds it there)" : "");
        return true;
    }
    if (item == kItBvRefill) {
        if (dir) return true;  // Left / Right do nothing on an action row
        BvSet(s, BV_REFILL, 0);
        Log("Rematch: menu Boss %c refill (#14320)", slotCh);
        if (v.live == 0) { snprintf(g_msg, sizeof g_msg, BV_NEED_BOSS, slotCh, slotCh); return true; }
        snprintf(g_msg, sizeof g_msg, "Boss %c refilled: full HP and Ki (a running Ki break plays out first).", slotCh);
        ToastF(cGreen, 2000, "Boss %c refilled (full HP and Ki).", slotCh);
        return true;
    }
    return true;
}
// A on an open row / page row. Returns true when the item was ours.
bool BvAct(int item) {
    if (IsBvOpenItem(item)) {
        g_bvSlot = item - kItBvOpenBase;
        g_uiPages.Enter(uBossVitals, g_sel);
        g_sel = 1; g_msg[0] = 0; g_confirm = -9;  // cursor on 'Boss infinite health', not on the read-only status row
        Log("Rematch: Boss %c health and Ki page opened (#14320)", 'A' + g_bvSlot);
        return true;
    }
    if (IsBvPageItem(item)) { BvChange(item, 0); return true; }
    return false;
}
const char* BvTitle() {
    static char t[96];
    snprintf(t, sizeof t, "Boss %c health and Ki - %.48s", 'A' + g_bvSlot, SlotBossName(SlotChr(g_bvSlot)));
    return t;
}
const char* BvGroup(int item) {
    if (item == kItBvStatus) return "Now";
    if (item == kItBvInfHp || item == kItBvInfKi) return "Keep it up (this session)";
    if (IsBvPageItem(item)) return "Set it now";
    return nullptr;
}
// Row text for our items. Returns false when the item is not ours.
bool BvRowText(int item, bool sel, const char* mk, char* buf, size_t n, uint32_t* color) {
    if (IsBvOpenItem(item)) {
        const int s = item - kItBvOpenBase;
        const BvView v = BvRead(s);
        const uint32_t chr = SlotChr(s);
        char st[48] = "";
        if (v.live > 0 && (v.cfg[0] || v.cfg[1])) snprintf(st, sizeof st, "   (%s%s%s)", v.cfg[0] ? "infinite health" : "", v.cfg[0] && v.cfg[1] ? ", " : "", v.cfg[1] ? "infinite Ki" : "");
        snprintf(buf, n, "%s Health and Ki%s   A: open", mk, st);
        *color = sel ? cWhite : chr ? cGray : cDim;
        return true;
    }
    if (!IsBvPageItem(item)) return false;
    const BvView v = BvRead(g_bvSlot);
    const bool live = v.live > 0;
    if (item == kItBvStatus) {
        if (v.live < 0) snprintf(buf, n, "%s Needs a Nioh3Training.asi with the boss health / Ki controls", mk);
        else if (!live) snprintf(buf, n, "%s Boss %c: empty (no live boss in this slot)", mk, 'A' + g_bvSlot);
        else snprintf(buf, n, "%s HP %llu / %llu (%d %%)   Ki %.0f / %.0f (%d %%)", mk, (unsigned long long)v.hc, (unsigned long long)v.hm, v.hm ? (int)(v.hc * 100 / v.hm) : 0,
                      v.kc, v.km, v.km > 0 ? (int)(v.kc * 100.0f / v.km + 0.5f) : 0);
        *color = sel ? cWhite : live ? cCyan : cDim;
        return true;
    }
    if (item == kItBvInfHp || item == kItBvInfKi) {
        const int on = v.cfg[item == kItBvInfHp ? 0 : 1];
        snprintf(buf, n, "%s %-22s %s   A / Left / Right", mk, item == kItBvInfHp ? "Boss infinite health:" : "Boss infinite Ki:", v.live < 0 ? "?" : on ? "ON" : "OFF");
        *color = sel ? cWhite : v.live < 0 ? cDim : on ? cGreen : cGray;
        return true;
    }
    if (item == kItBvHpPct || item == kItBvKiPct) {
        const int pct = v.cfg[item == kItBvHpPct ? 2 : 3];
        snprintf(buf, n, "%s %-22s %3d %%   Left / Right set, A re-apply", mk, item == kItBvHpPct ? "Set boss HP:" : "Set boss Ki:", pct);
        *color = sel ? cWhite : !live ? cDim : cYellow;
        return true;
    }
    snprintf(buf, n, "%s Refill boss (full HP and Ki)   press A", mk);
    *color = sel ? cWhite : live ? cGreen : cDim;
    return true;
}
const char* BvFooter(int item) {
    if (IsBvOpenItem(item)) return "Health and Ki of this boss: infinite health, infinite Ki, set HP / Ki %, refill. Each boss has its own settings (this session, default OFF).";
    if (item == kItBvStatus) return "The boss's HP and Ki right now (what its bars show).";
    if (item == kItBvInfHp) return "ON: the boss's HP never stays below the Set boss HP level (100 % = it never loses HP, so it cannot be killed by damage). OFF: normal damage. Not saved.";
    if (item == kItBvInfKi) return "ON: the boss's Ki never stays below the Set boss Ki level, so it is not Ki broken (a break already running plays out). OFF (default): normal Ki and Ki breaks.";
    if (item == kItBvHpPct) return "Sets the boss's HP to this % of its max at once (Left / Right 1-100 %). With infinite health ON it is also the level HP is held at.";
    if (item == kItBvKiPct) return "Sets the boss's Ki to this % at once (0 % = one hit from a Ki break). During a Ki break it waits until the break ends. With infinite Ki ON it is the held level.";
    if (item == kItBvRefill) return "Full HP and Ki for this boss now; both levels back to 100 %. A Ki break that is running plays out first.";
    return nullptr;
}
