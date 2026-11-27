// Best style scores per boss (style_scores.h) and the D floor of a boss fight (style_meter.h SmKeepD), offline.
//   py -m ziglang cc -target x86_64-windows-gnu -O1 -Wall -Isrc tests/style_scores_test.c -o build/t/style_scores_test.exe
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#undef NDEBUG
#include <assert.h>
#include "../src/style_meter.h"
#include "../src/style_result.h"
#include "../src/style_scores.h"
static int g_checks;
#define CHECK(x) do { assert(x); ++g_checks; } while (0)
static ScTable T;
static ScRec Rec(float sc, const char* ver) { ScRec r; memset(&r, 0, sizeof r); r.score = sc; r.grade = (int)sc; r.ms = 61000; r.hits = 2; snprintf(r.ver, sizeof r.ver, "%s", ver); snprintf(r.date, sizeof r.date, "2026-11-26 19:40"); return r; }

static void Keys(void) {
    char k[SC_KEY]; uint32_t a[3] = {0x2B, 0, 0x1A}, none[2] = {0, 0}, dup[3] = {7, 7, 7};
    CHECK(ScKeyOf(a, 3, k, sizeof k) && !strcmp(k, "c:1A+2B"));            // sorted, unknown ids skipped
    uint32_t b[2] = {0x1A, 0x2B}; char k2[SC_KEY]; ScKeyOf(b, 2, k2, sizeof k2); CHECK(!strcmp(k, k2));  // the hit order does not matter
    CHECK(!ScKeyOf(none, 2, k, sizeof k) && !k[0]);                        // a HUD-only boss: nothing kept
    CHECK(ScKeyOf(dup, 3, k, sizeof k) && !strcmp(k, "c:7"));              // a second form with the same id
}
static void Top3(void) {
    memset(&T, 0, sizeof T);
    ScRec r = Rec(3.5f, "1.8.48"); CHECK(ScInsert(&T, "c:1", "Demon of Pride", &r) == 1);
    r = Rec(5.0f, "1.8.48"); CHECK(ScInsert(&T, "c:1", "Demon of Pride", &r) == 1);
    r = Rec(4.0f, "1.8.47"); CHECK(ScInsert(&T, "c:1", "Demon of Pride", &r) == 2);
    r = Rec(1.0f, "1.8.48"); CHECK(ScInsert(&T, "c:1", "Demon of Pride", &r) == 0);   // 4th: not kept
    r = Rec(4.0f, "1.8.49"); CHECK(ScInsert(&T, "c:1", "Demon of Pride", &r) == 3);   // tie: older first
    CHECK(T.n == 1 && T.b[0].n == 3 && T.b[0].r[0].score == 5.0f && T.b[0].r[1].score == 4.0f && !strcmp(T.b[0].r[1].ver, "1.8.47") && !strcmp(T.b[0].r[2].ver, "1.8.49"));
    r = Rec(2.0f, "1.8.48"); CHECK(ScInsert(&T, "c:9", "anayama & Baba", &r) == 1);
    r = Rec(2.0f, "1.8.48"); CHECK(ScInsert(&T, "c:5", "Yamagata Masakage", &r) == 1);
    CHECK(T.n == 3 && !strcmp(T.b[0].key, "c:9") && !strcmp(T.b[1].key, "c:1") && !strcmp(T.b[2].key, "c:5"));  // name order, any case
}
static void Delete(void) {
    CHECK(ScDelete(&T, 1, 0) && T.b[1].n == 2 && T.b[1].r[0].score == 4.0f);  // the best one of Demon of Pride
    CHECK(!ScDelete(&T, 1, 2) && !ScDelete(&T, 7, 0));
    CHECK(ScDelete(&T, 0, -1) && T.n == 2 && !strcmp(T.b[0].key, "c:1"));     // all of a boss: it leaves the list
    CHECK(ScDelete(&T, 1, 0) && T.n == 1);                                    // its only score: the boss leaves
}
static void File(void) {
    char buf[4096]; int o = 0;
    for (int i = 0; i < T.n; ++i) for (int j = 0; j < T.b[i].n; ++j) o += ScFmtLine(buf + o, (int)sizeof buf - o, &T.b[i], &T.b[i].r[j]);
    ScTable* u = (ScTable*)calloc(1, sizeof *u);
    CHECK(!ScParseLine(u, "# comment") && !ScParseLine(u, "junk\tline") && !ScParseLine(u, "c:1\tX\t9.5\t3\t1\t0\tv\td"));  // bad score
    for (char* p = buf; *p;) { char* e = strchr(p, '\n'); *e = 0; CHECK(ScParseLine(u, p)); p = e + 1; }
    CHECK(u->n == 1 && u->b[0].n == 2 && u->b[0].r[0].score == 4.0f && u->b[0].r[0].hits == 2 && u->b[0].r[0].ms == 61000 && !strcmp(u->b[0].r[0].ver, "1.8.47")
          && !strcmp(u->b[0].r[0].date, "2026-11-26 19:40") && !strcmp(u->b[0].name, "Demon of Pride"));
    free(u);
    char nm[SC_NAME]; ScCopy(nm, SC_NAME, "a\tb\nc"); CHECK(!strcmp(nm, "a b c"));  // no tab / newline in a name
}
// human 2026-11-26: in a boss fight the meter never goes unranked: drained or hit at an empty D it stays at D (shown). Off: idle as before.
static void DFloor(void) {
    SmCfg c; SmCfgDefaults(&c); SmState s; SmReset(&s);
    CHECK(!s.active && !s.keepD);
    SmKeepD(&s, 1); CHECK(s.active && s.rank == 0 && s.pts == 0.f);              // a fight started: D, empty
    SmAdd(&s, &c, 50.f, SM_PC_HIT); s.lastScore = 0;
    for (double t = 0; t < 30000; t += 16) SmTick(&s, &c, t, 16);                // ran away for 30 s
    CHECK(s.active && s.rank == 0 && s.pts == 0.f);
    SmOnDamaged(&s, &c, 40000); CHECK(s.active && s.rank == 0);                   // hit at an empty D
    SmAdd(&s, &c, c.rankPts[0] + 10.f, SM_PC_HIT); CHECK(s.rank == 1);
    for (double t = 50000; t < 90000; t += 16) SmTick(&s, &c, t, 16);             // C drains down to D, not below
    CHECK(s.active && s.rank == 0);
    SmKeepD(&s, 0);                                                               // fight over: an empty D goes idle with the next decay
    for (double t = 90000; t < 92000; t += 16) SmTick(&s, &c, t, 16);
    CHECK(!s.active);
    // the card: time at an empty D is time at D (it always was)
    SrFight f; SrReset(&f); SmReset(&s); SrBossHit(&f, &s, 0x10); SmKeepD(&s, 1);
    for (int i = 0; i < 100; ++i) SrTick(&f, &s, &c, 10);
    CHECK(f.ms == 1000 && f.atRank[0] == 1000);
    SrBossChr(&f, 0x10, 0x2B); SrBossChr(&f, 0x10, 0x99); SrBossChr(&f, 0x20, 0x5);  // first id wins; unknown boss ignored
    const SrResult r = SrFinish(&f, &s, SR_HOLD_PCT); CHECK(r.nChr == 1 && r.chr[0] == 0x2B);
}
// human 2026-11-26 (1.8.48): rank ups cost 30 % more (SSS bar kept), A and up drain 30 % faster.
static void Tuning48(void) {
    SmCfg c; SmCfgDefaults(&c);
    static const float was[SM_RANKS] = {120, 180, 130, 180, 250, 350, 500};
    for (int r = 0; r < SM_RANKS - 1; ++r) CHECK(c.rankPts[r] > was[r] * 1.299f && c.rankPts[r] < was[r] * 1.301f);
    CHECK(c.rankPts[SM_RANKS - 1] == 500.f && c.decayHighRank == 3 && c.decayHighPct == 130);
    c.decayRankPct = 0; c.decayRampPct = 0; c.decayLowPct = 100; c.decayDelayMs = 0;
    float lost[SM_RANKS];
    for (int r = 2; r <= 4; ++r) { SmState s; SmReset(&s); s.active = 1; s.rank = r; s.pts = c.rankPts[r]; s.lastScore = 0; SmTick(&s, &c, 1000, 1000); lost[r] = c.rankPts[r] - s.pts; }
    CHECK(lost[3] > lost[2] * 1.299f && lost[3] < lost[2] * 1.301f && lost[4] > lost[2] * 1.299f && lost[4] < lost[2] * 1.301f);  // B normal, A / S x1.3
}
// 1.8.48: the whole card is kept with the score and comes back from the file (both pages' data); a card of another size is dropped, not the score.
static void Card(void) {
    ScTable* t = (ScTable*)calloc(1, sizeof *t); ScTable* u = (ScTable*)calloc(1, sizeof *u);
    ScRec r = Rec(4.25f, "1.8.48"); r.hasCard = 1; r.card.score = 4.25f; r.card.grade = 4; r.card.n.damaged = 3; r.card.n.earned[2] = 123.5f; r.card.comboBest = 41;
    snprintf(r.card.name, sizeof r.card.name, "Demon of Pride"); r.card.pctAt[4] = 61.f;
    CHECK(ScInsert(t, "c:1", "Demon of Pride", &r) == 1);
    static char line[SC_LINE]; const int k = ScFmtLine(line, sizeof line, &t->b[0], &t->b[0].r[0]);
    CHECK(k > 2 * (int)sizeof(SrResult) && line[k - 1] == '\n');
    CHECK(ScParseLine(u, line) && u->b[0].r[0].hasCard && u->b[0].r[0].card.grade == 4 && u->b[0].r[0].card.n.earned[2] == 123.5f
          && u->b[0].r[0].card.comboBest == 41 && !strcmp(u->b[0].r[0].card.name, "Demon of Pride") && u->b[0].r[0].card.pctAt[4] == 61.f);
    char* bad = strstr(line, "\tR"); bad[2] = '9';  // another build's card size
    memset(u, 0, sizeof *u); CHECK(ScParseLine(u, line) && !u->b[0].r[0].hasCard && u->b[0].r[0].score == 4.25f);
    free(t); free(u);
}
int main(void) { Card(); Tuning48(); Keys(); Top3(); Delete(); File(); DFloor(); printf("style scores ok: %d checks\n", g_checks); return 0; }
