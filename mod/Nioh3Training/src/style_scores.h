// human 2026-11-26: best style scores per boss. The top SC_KEEP boss result card scores of each boss (or boss pair), with the mod build that
// scored them, kept in <game>\Nioh3Training_scores.tsv (local only, never sent anywhere). The mod menu lists them (Style meter > Best scores)
// and can delete one score or all of a boss's scores. Pure (no game access, no Win32 file calls); tests/style_scores_test.c.
// Glue: style_meter_game.h (ScRecordResult when a card is made, the Nioh3Training_Score* exports for the menu).
//
// Boss identity (key): the fight's boss character ids, sorted, "c:<hex>[+<hex>..]" (a duo or a second form = its own entry, same every time).
// A fight with no character id (a Picture Scroll boss on the HUD bar only) is not kept. The name is the card's title name.
// File: one line per score, tab separated: key, name, score (2 decimals), grade 0..6, fight ms, hits taken, mod version, date. '#' = comment.
#ifndef STYLE_SCORES_H
#define STYLE_SCORES_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SC_KEEP 3
#define SC_BOSSES 256
#define SC_KEY 64
#define SC_NAME 96
typedef struct { float score; int grade; double ms; uint32_t hits; char ver[16]; char date[20]; } ScRec;
typedef struct { char key[SC_KEY]; char name[SC_NAME]; ScRec r[SC_KEEP]; int n; } ScBoss;
typedef struct { ScBoss b[SC_BOSSES]; int n; } ScTable;

// Key of a fight from its boss character ids (0 = unknown, skipped). Returns 0 when no id is known (nothing to keep).
static inline int ScKeyOf(const uint32_t* chr, int n, char* key, int cap) {
    uint32_t v[16]; int k = 0;
    for (int i = 0; i < n && k < 16; ++i) {
        if (!chr[i]) continue;
        int dup = 0; for (int j = 0; j < k; ++j) dup |= v[j] == chr[i];
        if (!dup) v[k++] = chr[i];
    }
    if (!k || cap < 4) { if (cap > 0) key[0] = 0; return 0; }
    for (int i = 1; i < k; ++i) for (int j = i; j > 0 && v[j - 1] > v[j]; --j) { const uint32_t t = v[j]; v[j] = v[j - 1]; v[j - 1] = t; }
    int o = snprintf(key, (size_t)cap, "c:");
    for (int i = 0; i < k && o < cap; ++i) o += snprintf(key + o, (size_t)(cap - o), i ? "+%X" : "%X", v[i]);
    return 1;
}
static inline int ScFind(const ScTable* t, const char* key) { for (int i = 0; i < t->n; ++i) if (!strcmp(t->b[i].key, key)) return i; return -1; }
static inline int ScNameCmp(const char* a, const char* b) {
    for (;; ++a, ++b) {
        int x = (unsigned char)*a, y = (unsigned char)*b;
        if (x >= 'A' && x <= 'Z') x += 32; if (y >= 'A' && y <= 'Z') y += 32;
        if (x != y || !x) return x - y;
    }
}
static inline void ScCopy(char* d, int cap, const char* s) { int i = 0; for (; s && s[i] && i < cap - 1; ++i) d[i] = s[i] == '\t' || s[i] == '\n' || s[i] == '\r' ? ' ' : s[i]; d[i] = 0; }
// The boss entry for key (made, in name order, if new). -1 = the table is full.
static inline int ScEntry(ScTable* t, const char* key, const char* name) {
    int i = ScFind(t, key);
    if (i >= 0) { if (name && name[0]) ScCopy(t->b[i].name, SC_NAME, name); return i; }
    if (t->n >= SC_BOSSES) return -1;
    int at = 0; while (at < t->n && ScNameCmp(t->b[at].name, name ? name : "") <= 0) ++at;
    memmove(&t->b[at + 1], &t->b[at], sizeof t->b[0] * (size_t)(t->n - at));
    memset(&t->b[at], 0, sizeof t->b[0]); ScCopy(t->b[at].key, SC_KEY, key); ScCopy(t->b[at].name, SC_NAME, name ? name : "");
    t->n++; return at;
}
// Add a score. Returns its place 1..SC_KEEP, or 0 when it is not in the boss's top SC_KEEP (nothing changed). A tie keeps the older score first.
static inline int ScInsert(ScTable* t, const char* key, const char* name, const ScRec* r) {
    const int i = ScEntry(t, key, name); if (i < 0) return 0;
    ScBoss* b = &t->b[i];
    int at = 0; while (at < b->n && b->r[at].score >= r->score) ++at;
    if (at >= SC_KEEP) { if (!b->n) { memmove(&t->b[i], &t->b[i + 1], sizeof t->b[0] * (size_t)(t->n - i - 1)); t->n--; } return 0; }
    const int move = (b->n < SC_KEEP ? b->n : SC_KEEP - 1) - at;
    if (move > 0) memmove(&b->r[at + 1], &b->r[at], sizeof b->r[0] * (size_t)move);
    b->r[at] = *r; if (b->n < SC_KEEP) b->n++;
    return at + 1;
}
// Delete score j of boss i (j < 0 = all of that boss's scores; the boss leaves the list when it has none). Returns 1 when something went.
static inline int ScDelete(ScTable* t, int i, int j) {
    if (i < 0 || i >= t->n) return 0;
    ScBoss* b = &t->b[i];
    if (j >= 0) { if (j >= b->n) return 0; memmove(&b->r[j], &b->r[j + 1], sizeof b->r[0] * (size_t)(b->n - j - 1)); b->n--; }
    else b->n = 0;
    if (!b->n) { memmove(&t->b[i], &t->b[i + 1], sizeof t->b[0] * (size_t)(t->n - i - 1)); t->n--; }
    return 1;
}
// One file line -> table (bad lines are skipped). Returns 1 when the line held a score.
static inline int ScParseLine(ScTable* t, const char* line) {
    if (!line || !line[0] || line[0] == '#') return 0;
    char buf[512]; ScCopy(buf, (int)sizeof buf, "");
    { int i = 0; for (; line[i] && line[i] != '\n' && line[i] != '\r' && i < (int)sizeof buf - 1; ++i) buf[i] = line[i]; buf[i] = 0; }
    char* f[8]; int n = 0; char* p = buf;
    while (n < 8) { f[n++] = p; char* tab = strchr(p, '\t'); if (!tab) break; *tab = 0; p = tab + 1; }
    if (n < 8 || strncmp(f[0], "c:", 2) || !f[0][2]) return 0;
    ScRec r; memset(&r, 0, sizeof r);
    r.score = (float)atof(f[2]); r.grade = atoi(f[3]); r.ms = atof(f[4]); r.hits = (uint32_t)strtoul(f[5], 0, 10);
    if (!(r.score >= 0.f && r.score <= 7.01f) || r.grade < 0 || r.grade > 6) return 0;
    ScCopy(r.ver, (int)sizeof r.ver, f[6]); ScCopy(r.date, (int)sizeof r.date, f[7]);
    return ScInsert(t, f[0], f[1], &r) > 0;
}
static inline int ScFmtLine(char* o, int cap, const ScBoss* b, const ScRec* r) {
    return snprintf(o, (size_t)cap, "%s\t%s\t%.2f\t%d\t%.0f\t%u\t%s\t%s\n", b->key, b->name, r->score, r->grade, r->ms, r->hits, r->ver[0] ? r->ver : "?", r->date[0] ? r->date : "?");
}
#endif
