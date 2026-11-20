// human #58117: 'Soul core tuning' page (LEFT soul core form start / exit look). Included by rematch.cpp inside its UI namespace right after
// practice_stats_menu.h (same helpers: g_msg, g_sel, g_uiPages, Log, colours). Game side: bosslog::SoulTune (bosslog.cpp) = the live globals the
// effect reads at use time (next LEFT use picks a change up) + WritePrivateProfileStringW to Nioh3BossRematch.ini [Spirit].
//   Player > Soul core tuning: A opens the page; on it Left / Right (A = Right) step the value.
constexpr int uSoulTune = 950;
constexpr int kItSctOpen = 0xE0000, kItSctBase = 0xE0010, kSctN = 11;  // kItSctBase + bosslog::SoulTune 'which' (0xE0000: no other range uses it)
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
                                      "Crouch cancel window:"};  // human #122288
const double kSctDefault[kSctN] = {100, 100, 0, 0, 400, 1.4, 4, 0, 0, 0, 250};
void SctValueText(int w, double v, char* b, size_t n) {
    if (w == 9 && v == -9999) snprintf(b, n, "Legacy close");
    else if (w == 10 && v <= 0) snprintf(b, n, "OFF");  // human #122288
    else if (w == 7 || w == 8) snprintf(b, n, "%s", v > 0 ? "ON" : "OFF");
    else if (w == 5 || w == 6) snprintf(b, n, "x%.1f", v);
    else if (w == 1 || w == 2) snprintf(b, n, "%+d", (int)v);
    else if (w == 3 && v <= 0) snprintf(b, n, "OFF (once)");
    else snprintf(b, n, "%d ms", (int)v);
}
// A (dir 0) / Left (-1) / Right (+1). Returns true when the item was ours.
bool SctChange(int item, int dir) {
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
        const bool cur = bosslog::FormCoreTransforms(id);
        const bool want = dir ? dir > 0 : !cur;
        const bool v = want == cur ? cur : bosslog::FormCoreToggle(id);
        Log("Rematch: menu soul core %X transformation -> %s (human #81220)", id, v ? "ON" : "OFF");
        snprintf(g_msg, sizeof g_msg, "Core %X: %s from the next use. Saved.", id, v ? "transforms you into the yokai" : "the game's own summon");
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
    const double v = bosslog::SoulTune(w, dir ? dir : 1, &live);
    char t[24]; SctValueText(w, v, t, sizeof t);
    Log("Rematch: menu Soul core %s -> %s (human #58117)", kSctLabel[w], t);
    if (w == 9) { snprintf(g_msg, sizeof g_msg, "%s %s. Live and saved; hidden yokai finishes its attack.", kSctLabel[w], t); return true; }
    if (w == 10) { snprintf(g_msg, sizeof g_msg, "%s %s. Live from the next soul core press. Saved.", kSctLabel[w], t); return true; }  // human #122288
    snprintf(g_msg, sizeof g_msg, live ? "%s %s. Applies on the next LEFT soul core use. Saved." : "%s %s. Saved; the speed hooks are off this session, applies next launch.",
             kSctLabel[w], t);
    return true;
}
bool SctRowText(int item, bool sel, const char* mk, char* buf, size_t n, uint32_t* color) {
    if (IsScCoreItem(item)) {  // human #81220
        uint32_t id = 0; int d = 0;
        if (!ScCoreAt(item - kItScCore, &id, &d)) { buf[0] = 0; *color = cDim; return true; }
        if (!id) {
            const bool v = bosslog::FormCorePendingTransforms(d);
            snprintf(buf, n, "%s %s seal (not used yet): %s   A / Left / Right", mk, d == 1 ? "LEFT" : "DOWN", v ? "transforms" : "game summon");
            *color = sel ? cWhite : v ? cGray : cYellow;
            return true;
        }
        const bool v = bosslog::FormCoreTransforms(id);
        const char* label = (d & 12) == 12 ? "(Yang 1 + 2)" : (d & 4) ? "(Yang 1)" : (d & 8) ? "(Yang 2)" :
            d == 1 ? "(LEFT last used)" : d == 2 ? "(DOWN last used)" : d == 3 ? "(LEFT/DOWN last used)" : "";
        snprintf(buf, n, "%s Core %X %s: %s   A / Left / Right", mk, id, label, v ? "transforms" : "game summon");
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
    const double v = bosslog::SoulTune(w, 0, &live);
    char t[24]; SctValueText(w, v, t, sizeof t);
    snprintf(buf, n, "%s %-26s %-11s Left / Right%s", mk, kSctLabel[w], t, live ? "" : "  (next launch)");
    *color = sel ? cWhite : !live ? cDim : (v > kSctDefault[w] + 1e-6 || v < kSctDefault[w] - 1e-6) ? cYellow : cGray;
    return true;
}
const char* SctFooter(int item) {
    if (IsScCoreItem(item)) return "Yang rows show equipped cores. Choices follow the core. If equipment cannot be read, choose an unidentified seal's next use; its choice follows the core once identified. Saved.";
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
    case 10: return "Crouch (L3) this soon after a soul core press cancels the transformation; the core is not spent. OFF = no cancel. 50 ms steps, 0-1000. [Spirit] FormCrouchCancel. Live, saved.";
    case 9: return "Exit smoke when the final hit starts, plus this delay. 50 ms steps, -500..1000. Negative values fire at the observed start; no prediction. Unknown counts use idle fallback. Live, saved.";
    default: return "A: open. Live LEFT soul core values: exit reveal delay, smoke position / repeat, hide delay, transform speed. Saved to Nioh3BossRematch.ini [Spirit].";
    }
}
