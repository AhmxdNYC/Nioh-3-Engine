// user #14306 / #14309 / #14313 / #14317 / #14336: Practice > Training Bosses. Included by rematch.cpp inside its UI namespace, after
// boss_vitals_menu.h (TrainingExport, g_msg, ToastF, SlotChr, SlotBossName, g_uiPages, MoveBoss, g_bossSlot, the move helpers).
//   Training Bosses page (uTools): one row per spawned boss (Boss A, B, C ... unlimited letters), 'Summon another boss', the timed
//     'switch all bosses CPU <-> idle' slider (1-10 s), 'Remove all summoned bosses (A: x, B: y ...)' (only when one is spawned), Return.
//   Boss X page (uBoss): every per-boss setting in ONE place (#14336): stance, aggro, gap between repeats, reset position, health and Ki,
//     the move groups (Attacks ... + Sequences) and Remove this boss. Each boss is independent (Training per-slot exports).
//   Sequences (#14317): chains of the decoded 'next action on finish' links (mod/boss_sequences.h). A on a sequence = repeat it: the boss is
//     made to start the first move; the game plays the rest of the chain by itself (the links are native), then the gap, then again.
#include "../../boss_sequences.h"
constexpr int uBoss = 940;
constexpr int kItSeqBase = 0xA0000, kItSeqEnd = kItSeqBase + 0x10000;          // + kN3Moves index of the sequence's first move
constexpr int kItBossOpenBase = 0xB0000, kItBossOpenEnd = kItBossOpenBase + 64; // + slot: 'Boss C: name   A: open'
constexpr int kItBossStance = -60, kItBossAggro = -61, kItBossGap = -62, kItBossReset = -63, kItBossRemove = -64, kItSumAnother = -65, kItStanceTimer = -66;
constexpr int kMcSeq = kMcCount;  // the Sequences group row (kItMoveCatBase - kMcSeq) and page (g_moveCat == kMcSeq)
bool IsBossOpenItem(int item) { return item >= kItBossOpenBase && item < kItBossOpenEnd; }
bool IsSeqItem(int item) { return item >= kItSeqBase && item < kItSeqBase + N3_MOVE_COUNT; }
bool IsBossPageItem(int item) { return item <= kItBossStance && item >= kItBossRemove; }
const char* CatName(int c) { return c == kMcSeq ? "Move chains" : N3MoveCatName(c); }
int SlotCountUi() {
    using Fn = int (*)();
    const auto f = (Fn)TrainingExport("Nioh3Training_SlotCount");
    const int n = f ? f() : 2;
    return n < 1 ? 1 : n > 64 ? 64 : n;
}
// user #14333: already in an arena / a Training session (or a stuck no-boss arena): 'Train here' and 'Train in a boss arena' are hidden
bool InTrainingArena() {
    using Fn = int (*)();
    const auto f = (Fn)TrainingExport("Nioh3Training_SessionActive");
    return LeaveArenaOffered() || (f && (f() & 1)) || fight::g_active;
}
int SpawnedCount() { int k = 0; for (int s = 0; s < SlotCountUi(); ++s) k += SlotChr(s) != 0; return k; }
int FreeSlot() { for (int s = 0; s < SlotCountUi(); ++s) if (!SlotChr(s)) return s; return -1; }
// -2 empty, -1 stand still failed, 0 Fight normally, 1 getting ready, 2 stand still, 3 stand still + repeating a move; -3 = Training too old
int SlotStance(int s) {
    using Fn = int (*)(int);
    const auto f = (Fn)TrainingExport("Nioh3Training_GetSlotStance");
    return f ? f(s) : -3;
}
const char* StanceName(int st) {
    return st == 0 ? "Fight normally" : st == 1 ? "Stand still (pending)" : st == 2 ? "Stand still" : st == 3 ? "Stand still + repeat"
         : st == -1 ? "Fight normally (wait failed)" : st == -2 ? "gone" : "?";
}
bool StanceIdle(int st) { return st >= 1; }
// ---- per-boss rows (g_bossSlot) ----
void BossGone() { snprintf(g_msg, sizeof g_msg, "Boss %c is no longer active. B: return to Active bosses.", 'A' + g_bossSlot); }
void BossStanceSet(int dir) {  // A flips, Right = stand still, Left = fight normally
    using Fn = int (*)(int, int);
    const auto f = (Fn)TrainingExport("Nioh3Training_SetSlotStance");
    const int st = SlotStance(g_bossSlot);
    if (!f || st == -3) { snprintf(g_msg, sizeof g_msg, "Needs the Nioh3Training.asi of this build (per-boss stance)."); return; }
    if (st == -2) { BossGone(); return; }
    const int idle = dir ? dir > 0 : !StanceIdle(st);
    const int rc = f(g_bossSlot, idle);
    Log("Rematch: menu Boss %c stance -> %s (%d)", 'A' + g_bossSlot, idle ? "Stand still" : "Fight normally", rc);
    snprintf(g_msg, sizeof g_msg, !rc ? "Boss %c: could not change the stance (no live boss)." : idle ? "Boss %c stands still (idle) from its next safe moment. A repeating move still plays every gap." :
             "Boss %c fights normally (CPU, its own attacks). The other bosses keep their own stance.", 'A' + g_bossSlot);
}
int BossAggroGet() {
    using Fn = int (*)(int);
    const auto f = (Fn)TrainingExport("Nioh3Training_GetSlotAggro");
    return f ? f(g_bossSlot) : -3;
}
void BossAggroSet(int dir) {
    using Fn = int (*)(int, int);
    const auto f = (Fn)TrainingExport("Nioh3Training_SetSlotAggro");
    const int v = BossAggroGet();
    if (!f || v == -3) { snprintf(g_msg, sizeof g_msg, "Needs the Nioh3Training.asi of this build (per-boss aggro)."); return; }
    if (v < 0) { BossGone(); return; }
    const int on = f(g_bossSlot, dir ? dir > 0 : !v);
    Log("Rematch: menu Boss %c aggro -> %d", 'A' + g_bossSlot, on);
    snprintf(g_msg, sizeof g_msg, on ? "Boss %c aggro ON: it keeps you as its target and walks up to you (5 m) before its repeating move." : "Boss %c aggro OFF: it behaves as before.", 'A' + g_bossSlot);
}
int BossGapGet() {
    using Fn = int (*)(uint32_t);
    const auto f = (Fn)TrainingExport("Nioh3Training_GetRepeatGapChr");
    const uint32_t chr = SlotChr(g_bossSlot);
    return f && chr ? f(chr) : -1;
}
void BossGapStep(int dir) {  // Left / Right 0.25 s, A = longer, wraps to 0 after 10 s
    using Fn = int (*)(uint32_t, int, int);
    const auto f = (Fn)TrainingExport("Nioh3Training_SetRepeatGapChr");
    const uint32_t chr = SlotChr(g_bossSlot);
    int v = BossGapGet();
    if (!chr) { BossGone(); return; }
    if (!f || v < 0) { snprintf(g_msg, sizeof g_msg, "Needs the Nioh3Training.asi of this build (per-boss gap)."); return; }
    const int step = 250, hi = 10000;
    v = dir < 0 ? v - step : v + step;
    if (dir == 0 && v > hi) v = 0;
    v = v < 0 ? 0 : v > hi ? hi : v;
    v = f(chr, v, 1);
    Log("Rematch: menu Boss %c gap between repeats -> %d ms", 'A' + g_bossSlot, v);
    g_msg[0] = 0;
}
void BossResetPos() {
    using Fn = int (*)(int);
    const auto f = (Fn)TrainingExport("Nioh3Training_ResetSlotPos");
    const int rc = f ? f(g_bossSlot) : 0;
    Log("Rematch: menu Boss %c reset position -> %d", 'A' + g_bossSlot, rc);
    if (!f) { snprintf(g_msg, sizeof g_msg, "Needs the Nioh3Training.asi of this build (per-boss reset)."); return; }
    if (!rc) { snprintf(g_msg, sizeof g_msg, "Boss %c has no start position to go back to.", 'A' + g_bossSlot); return; }
    snprintf(g_msg, sizeof g_msg, "Boss %c goes back to where it first appeared, facing you.", 'A' + g_bossSlot);
    ToastF(cGreen, 2000, "Boss %c put back at its start position.", 'A' + g_bossSlot);
}
bool BossRemove() {  // true = the page was left
    using Fn = int (*)(int);
    const auto f = (Fn)TrainingExport("Nioh3Training_DespawnSlot");
    const int rc = f ? f(g_bossSlot) : 0;
    Log("Rematch: menu remove Boss %c -> %d", 'A' + g_bossSlot, rc);
    if (!rc) { snprintf(g_msg, sizeof g_msg, "Busy (a summon or load is running): try again in a moment."); return false; }
    ToastF(cYellow, 2000, "Boss %c removed.", 'A' + g_bossSlot);
    return true;
}
// ---- all bosses: timed CPU <-> idle switch (#14313) ----
int g_stSec = 3;  // slider 1..10 s (session)
int StanceTimerLeft(int* toIdle) {
    using Fn = int (*)(int*);
    const auto f = (Fn)TrainingExport("Nioh3Training_GetStanceTimer");
    int t = 0; const int left = f ? f(&t) : -1;
    if (toIdle) *toIdle = t;
    return left;
}
int StanceTimerTarget() {  // 1 = the switch makes every boss stand still, 0 = fight: any boss standing still -> all fight, else all stand still
    for (int s = 0; s < SlotCountUi(); ++s) if (SlotChr(s) && StanceIdle(SlotStance(s))) return 0;
    return 1;
}
bool StanceTimerAct() {  // A: arm (closes the menu so you can get ready) / cancel a running one
    using Fn = int (*)(int, int);
    const auto f = (Fn)TrainingExport("Nioh3Training_StanceTimer");
    if (!f) { snprintf(g_msg, sizeof g_msg, "Needs the Nioh3Training.asi of this build (timed stance switch)."); return false; }
    if (StanceTimerLeft(nullptr) > 0) { f(0, 0); snprintf(g_msg, sizeof g_msg, "Timed switch cancelled."); Log("Rematch: menu stance timer cancelled"); return false; }
    if (!SpawnedCount()) { snprintf(g_msg, sizeof g_msg, "No Training boss yet: summon a boss or start a Training session first."); return false; }
    const int idle = StanceTimerTarget();
    f(g_stSec * 1000, idle);
    Log("Rematch: menu stance timer %d s -> all %s", g_stSec, idle ? "Stand still" : "Fight normally");
    ToastF(cGreen, (ULONGLONG)g_stSec * 1000 + 1000, "In %d s every boss switches to %s.", g_stSec, idle ? "Stand still" : "Fight normally");
    return true;
}
// ---- summon another boss (#14336 item 7: no slot picker) ----
bool SummonAnother(int row) {  // true = queued (menu closes)
    if (row < 0 || (size_t)row >= g_rows.size() || !g_rows[(size_t)row].missionId) { snprintf(g_msg, sizeof g_msg, "No boss mission id for this row: cannot summon it."); return false; }
    const int s = FreeSlot();
    if (s < 0) { snprintf(g_msg, sizeof g_msg, "Every boss letter is taken (%d bosses): remove one first.", SlotCountUi()); return false; }
    using Fn = int (*)(int, uint32_t);
    const auto f = (Fn)TrainingExport("Nioh3Training_SummonSlot");
    const int rc = f ? f(s, g_rows[(size_t)row].missionId) : 0;
    Log("Rematch: menu summon another boss %s as Boss %c -> %d", g_rows[(size_t)row].id, 'A' + s, rc);
    if (!rc) { snprintf(g_msg, sizeof g_msg, "Busy (a summon or load is running) or too many bosses: try again in a moment."); return false; }
    ToastF(cGreen, 4000, "Adding %s as Boss %c. Behavior follows the current session...", g_rows[(size_t)row].name, 'A' + s);
    return true;
}
// ---- user #22984: the Nioh3Training roster (summon-only bosses: every boss, e.g. Kajiwara Kagetoki, not only single-boss catalogue rows) ----
int RosterCount() { using Fn = int (*)(); const auto f = (Fn)TrainingExport("Nioh3Training_RosterCount"); return f && TrainingExport("Nioh3Training_SummonSlotChr") ? f() : 0; }
uint32_t RosterEntry(int i, char* name, int cap) {
    using Fn = uint32_t (*)(int, char*, int); const auto f = (Fn)TrainingExport("Nioh3Training_RosterEntry");
    if (name && cap > 0) name[0] = 0;
    return f ? f(i, name, cap) : 0;
}
bool SummonRoster(int i) {  // true = queued (menu closes)
    char nm[64]; const uint32_t chr = RosterEntry(i, nm, sizeof nm);
    if (!chr) { snprintf(g_msg, sizeof g_msg, "No character id for this boss: cannot summon it."); return false; }
    const int s = FreeSlot();
    if (s < 0) { snprintf(g_msg, sizeof g_msg, "Every boss letter is taken (%d bosses): remove one first.", SlotCountUi()); return false; }
    using Fn = int (*)(int, uint32_t);
    const auto f = (Fn)TrainingExport("Nioh3Training_SummonSlotChr");
    const int rc = f ? f(s, chr) : 0;
    Log("Rematch: menu summon roster boss %s (char %u) as Boss %c -> %d", nm, chr, 'A' + s, rc);
    if (!rc) { snprintf(g_msg, sizeof g_msg, "Busy (a summon or load is running) or too many bosses: try again in a moment."); return false; }
    ToastF(cGreen, 4000, "Adding %s as Boss %c. Behavior follows the current session...", nm, 'A' + s);
    return true;
}
// ---- sequences (#14317) ----
const N3Next* SeqLink(uint32_t chr, uint32_t id) { for (int i = 0; i < N3_NEXT_COUNT; ++i) if (kN3Next[i].chr == chr && kN3Next[i].id == id) return &kN3Next[i]; return nullptr; }
int SeqChain(uint32_t chr, uint32_t head, uint32_t* ids, int cap, bool* loop);
bool SeqHead(uint32_t chr, uint32_t id) {
    if (!SeqLink(chr, id) || N3MoveFind(chr, id) < 0 || N3MoveProtected(id)) return false;
    for (int i = 0; i < N3_NEXT_COUNT; ++i) if (kN3Next[i].chr == chr && kN3Next[i].next == id) return false;  // part of a longer chain
    // user #14702: heads of the Yoritomo 1100-1103 combo family whose full chain is not proven (the table link skips hits the AI plays): hidden
    if (chr == 16102u && (id == 0x4E67u || id == 0x4E70u || id == 0x4E8Cu || id == 0x4E76u || id == 0x4E7Du)) return false;
    // user #14655/#14665: no sequence with an unsafe (boss-killing) part; only real attack sequences (some part is an attack)
    uint32_t ids[8]; bool loop = false, anyUseful = false;
    const int k = SeqChain(chr, id, ids, 8, &loop);
    for (int i = 0; i < k; ++i) {
        const int mi = N3MoveFind(chr, ids[i]);
        if (N3MoveUnsafe(chr, ids[i])) return false;
        if (mi >= 0 && N3MoveAttack(&kN3Moves[mi])) anyUseful = true;
    }
    return anyUseful;
}
int SeqChain(uint32_t chr, uint32_t head, uint32_t* ids, int cap, bool* loop) {  // head, next, ... (max cap); loop = ends in a looping part
    int n = 0; *loop = false;
    for (uint32_t c = head; n < cap;) {
        ids[n++] = c;
        const N3Next* l = SeqLink(chr, c);
        if (!l) break;
        bool seen = false; for (int i = 0; i < n; ++i) seen |= ids[i] == l->next;
        if (seen) { *loop = true; break; }
        c = l->next;
        if (l->loop && n < cap) { ids[n++] = c; *loop = true; break; }
    }
    return n;
}
int SeqCount(uint32_t chr) { int k = 0; for (int i = 0; i < N3_MOVE_COUNT; ++i) k += kN3Moves[i].chr == chr && SeqHead(chr, kN3Moves[i].id); return k; }
void SeqText(const N3Move& m, char* b, size_t n) {
    uint32_t ids[8]; bool loop = false;
    const int k = SeqChain(m.chr, m.id, ids, 8, &loop);
    size_t o = 0;
    for (int i = 0; i < k && o < n; ++i) o += (size_t)snprintf(b + o, n - o, "%s0x%04X", i ? " > " : "", ids[i]);
    if (loop && o < n) snprintf(b + o, n - o, " (loops)");
}
void SeqToggle(int item, int dir) {  // A flips the repeat of this sequence, Right = repeat, Left = stop
    const N3Move& m = kN3Moves[item - kItSeqBase];
    if (m.chr != MoveBoss()) { snprintf(g_msg, sizeof g_msg, "The boss changed; reopen its page."); return; }
    const bool on = MoveRepeating(m) && RepeatIsSeq(m.chr);  // user #14423: the whole sequence repeats (not only its first move)
    if (dir < 0 || (dir == 0 && on)) { if (on) RepeatStop("sequence row"); return; }
    if (RepeatArm(&m, "sequence row", true)) {
        char seq[80]; SeqText(m, seq, sizeof seq);
        snprintf(g_msg, sizeof g_msg, "Repeating chain %s, then waiting for Repeat delay. A again stops.", seq);
    }
}
