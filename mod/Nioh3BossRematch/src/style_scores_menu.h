// style_scores_menu.h - Style meter > Best scores (human 2026-11-26). Included inside the rematch UI namespace right after sub_pages_menu.h.
// Nioh3Training keeps each boss's 3 best result card scores with the mod build that scored them (Nioh3Training style_scores.h, file
// Nioh3Training_scores.tsv, local only). Pages:
//   Best scores (uScores): one row per boss 'Demon of Pride   best SS 5.42   3 scores   A: open', in name order.
//   <Boss> (uScoreBoss): its scores, best first, 'A: delete' (press A twice), and 'Delete all scores of this boss' (press A twice).
#pragma once

constexpr int uScores = 964, uScoreBoss = 965;
constexpr int kItScoresOpen = 0xF2000, kItScoreBossBase = 0xF2100, kItScoreBossEnd = kItScoreBossBase + 256;  // + boss index
constexpr int kItScoreRowBase = 0xF2400, kItScoreDelAll = 0xF2410;  // + score index 0..2
int g_scoreBoss = -1;  // boss index of the uScoreBoss page
bool IsScoresItem(int item) {
    return item == kItScoresOpen || (item >= kItScoreBossBase && item < kItScoreBossEnd) || (item >= kItScoreRowBase && item < kItScoreRowBase + 3) || item == kItScoreDelAll;
}
bool ScoresAvailable() { return TrainingExport("Nioh3Training_ScoreBosses") != nullptr; }
int ScoresBosses() { using F = int (*)(); const auto f = (F)TrainingExport("Nioh3Training_ScoreBosses"); return f ? f() : 0; }
int ScoresBoss(int i, char* name, int cap) {
    using F = int (*)(int, char*, int); const auto f = (F)TrainingExport("Nioh3Training_ScoreBoss");
    if (cap > 0) name[0] = 0;
    return f ? f(i, name, cap) : 0;
}
bool ScoresText(int i, int j, char* out, int cap) {
    using F = int (*)(int, int, char*, int); const auto f = (F)TrainingExport("Nioh3Training_ScoreText");
    if (cap > 0) out[0] = 0;
    return f && f(i, j, out, cap) > 0;
}
bool ScoresDelete(int i, int j) { using F = int (*)(int, int); const auto f = (F)TrainingExport("Nioh3Training_ScoreDelete"); return f && f(i, j) > 0; }

template <class Add> void ScoresAddOpenRow(Add&& add) { if (ScoresAvailable()) add(kItScoresOpen); }
// The rows of a Best scores page. false = the current page is not one of them.
template <class Add> bool ScoresAddPage(Add&& add) {
    const int p = g_uiPages.page;
    if (p == uScores) { const int n = ScoresBosses(); for (int i = 0; i < n && i < 256; ++i) add(kItScoreBossBase + i); return true; }
    if (p == uScoreBoss) {
        char nm[96]; const int n = ScoresBoss(g_scoreBoss, nm, sizeof nm);
        for (int j = 0; j < n && j < 3; ++j) add(kItScoreRowBase + j);
        if (n > 0) add(kItScoreDelAll);
        return true;
    }
    return false;
}
// Left / Right: nothing to change. A: open / delete (twice). Returns true when the item is a Best scores row.
bool ScoresAct(int item, int dir) {
    if (!IsScoresItem(item)) return false;
    if (dir) return true;
    if (item == kItScoresOpen) {
        g_uiPages.Enter(uScores, g_sel); g_sel = 0; g_msg[0] = 0; g_confirm = -9;
        if (!ScoresBosses()) snprintf(g_msg, sizeof g_msg, "No best scores yet: finish a boss fight with the style meter and the result card on.");
        Log("Rematch: Best scores page opened");
        return true;
    }
    if (item >= kItScoreBossBase && item < kItScoreBossEnd) {
        g_scoreBoss = item - kItScoreBossBase;
        g_uiPages.Enter(uScoreBoss, g_sel); g_sel = 0; g_msg[0] = 0; g_confirm = -9;
        return true;
    }
    char nm[96]; ScoresBoss(g_scoreBoss, nm, sizeof nm);
    if (g_confirm != item) {
        g_confirm = item;
        if (item == kItScoreDelAll) snprintf(g_msg, sizeof g_msg, "Press A again to delete every score of %s.", nm);
        else snprintf(g_msg, sizeof g_msg, "Press A again to delete score %d of %s.", item - kItScoreRowBase + 1, nm);
        return true;
    }
    g_confirm = -9;
    const int j = item == kItScoreDelAll ? -1 : item - kItScoreRowBase;
    const bool ok = ScoresDelete(g_scoreBoss, j);
    Log("Rematch: menu Best scores delete %s of %s -> %s", j < 0 ? "all" : "one", nm, ok ? "ok" : "failed");
    snprintf(g_msg, sizeof g_msg, !ok ? "Could not delete that score." : j < 0 ? "Every score of %s deleted." : "Score deleted (%s).", nm);
    if (ok && ScoresBoss(g_scoreBoss, nm, sizeof nm) <= 0) { g_uiPages.Back(g_sel); g_sel = 0; }  // no scores left: back to the boss list
    else if (ok) g_sel = 0;
    return true;
}
bool ScoresRowText(int item, bool sel, const char* mk, char* buf, size_t n, uint32_t* color) {
    if (!IsScoresItem(item)) return false;
    *color = sel ? cWhite : cGray;
    if (item == kItScoresOpen) { snprintf(buf, n, "%s Best scores   A: open", mk); return true; }
    if (item >= kItScoreBossBase && item < kItScoreBossEnd) {
        const int i = item - kItScoreBossBase; char nm[96], best[160];
        const int k = ScoresBoss(i, nm, sizeof nm);
        ScoresText(i, 0, best, sizeof best); best[8] = 0;  // 'SS  5.42' (grade + score of the best one)
        snprintf(buf, n, "%s %-40.40s best %s   %d score%s   A: open", mk, nm, best, k, k == 1 ? "" : "s");
        return true;
    }
    if (item == kItScoreDelAll) { snprintf(buf, n, "%s Delete all scores of this boss   A (twice)", mk); *color = sel ? cWhite : g_confirm == item ? cOrange : cGray; return true; }
    const int j = item - kItScoreRowBase; char t[160];
    ScoresText(g_scoreBoss, j, t, sizeof t);
    snprintf(buf, n, "%s %d. %s   A: delete", mk, j + 1, t);
    if (!sel) *color = g_confirm == item ? cOrange : j == 0 ? cYellow : cGray;
    return true;
}
const char* ScoresFooter(int item) {
    if (item == kItScoresOpen) return "Your 3 best boss result card scores for each boss, with the mod build and date they were scored. Kept on this PC only. Open a boss to delete scores.";
    if (item >= kItScoreBossBase && item < kItScoreBossEnd) return "A: this boss's best scores (grade, score out of 7, fight time, hits taken, mod build, date).";
    if (item >= kItScoreRowBase && item < kItScoreRowBase + 3) return "Grade, score out of 7, fight time, hits taken, the mod build that scored it and the date. Press A twice to delete this score.";
    if (item == kItScoreDelAll) return "Deletes all of this boss's best scores (press A twice). Other bosses keep theirs.";
    return nullptr;
}
const char* ScoresTitle(int page) {
    if (page == uScores) return "Best scores (style meter result cards)";
    if (page == uScoreBoss) { static char t[128]; char nm[96]; ScoresBoss(g_scoreBoss, nm, sizeof nm); snprintf(t, sizeof t, "Best scores - %s", nm[0] ? nm : "boss"); return t; }
    return nullptr;
}
const char* ScoresGroup(int page, int item) {
    if (page == uScores) return "Bosses (A: open)";
    if (page == uScoreBoss) return item == kItScoreDelAll ? "Delete" : "Best first";
    return nullptr;
}
