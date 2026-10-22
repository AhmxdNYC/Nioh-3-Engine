// pc2 #23565: automatic boss action-file binding (pure logic, no game access; tests/autobind_test.c). Game side: autobind_game.h.
// The boss's current action object -> [obj+0x20] = its 336-byte action row inside the loaded archive_01 action file (id +0xC, action object hash +0xF0).
// The rows of that file are contiguous, so walking them gives the file's action-object-hash set, matched against every archive_01 action file
// (Nioh3Training_actionsets.tsv, tools/gen_actionsets.py): best Jaccard wins. No boss names, no fixed ids / RVAs.
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
enum { AB_MAX_FILES = 1024, AB_NAME = 40, AB_MIN_SCORE_PCT = 70, AB_MIN_GAP_PCT = 15 };  // bind only if best > 70 % and the runner-up is >= 15 points lower (#23770)
typedef struct { char file[72]; char chr[AB_NAME]; int rows; int n; uint32_t* ids; } AbFile;  // ids sorted ascending, unique
typedef struct { AbFile f[AB_MAX_FILES]; int n; } AbTable;
static inline int AbOk(uint32_t h) { return h && h != 0xFFFFFFFFu; }
static int AbCmp32(const void* a, const void* b) { const uint32_t x = *(const uint32_t*)a, y = *(const uint32_t*)b; return x < y ? -1 : x > y; }
static inline int AbHas(const AbFile* f, uint32_t id) {
    int lo = 0, hi = f->n - 1;
    while (lo <= hi) { const int m = (lo + hi) >> 1; if (f->ids[m] == id) return 1; if (f->ids[m] < id) lo = m + 1; else hi = m - 1; }
    return 0;
}
// text = whole TSV (modified in place: tabs/newlines become NULs). Returns the number of files loaded.
static int AbLoad(AbTable* t, char* text) {
    t->n = 0;
    for (char* line = text; line && *line && t->n < AB_MAX_FILES; ) {
        char* nl = strchr(line, '\n'); if (nl) *nl = 0;
        char* next = nl ? nl + 1 : 0;
        size_t L = strlen(line); if (L && line[L - 1] == '\r') line[L - 1] = 0;
        char* c1 = strchr(line, '\t'); char* c2 = c1 ? strchr(c1 + 1, '\t') : 0; char* c3 = c2 ? strchr(c2 + 1, '\t') : 0;
        if (line[0] != '#' && c3) {
            *c1 = *c2 = *c3 = 0;
            AbFile* f = &t->f[t->n];
            snprintf(f->file, sizeof f->file, "%s", line); snprintf(f->chr, sizeof f->chr, "%s", c1 + 1); f->rows = atoi(c2 + 1);
            int cap = 64; f->n = 0; f->ids = (uint32_t*)malloc(cap * sizeof(uint32_t));
            for (char* p = c3 + 1; *p; ) {
                char* e; unsigned long v = strtoul(p, &e, 16);
                if (e == p) break;
                if (f->n == cap) { cap *= 2; f->ids = (uint32_t*)realloc(f->ids, cap * sizeof(uint32_t)); }
                f->ids[f->n++] = (uint32_t)v; p = *e == ',' ? e + 1 : e;
            }
            qsort(f->ids, f->n, sizeof(uint32_t), AbCmp32);
            t->n++;
        }
        line = next;
    }
    return t->n;
}
static void AbFree(AbTable* t) { for (int i = 0; i < t->n; ++i) free(t->f[i].ids); t->n = 0; }
// Best file for an observed id set (ids unique, 0xFFFF ignored). Returns the index or -1; *pct = Jaccard in percent (0..100); *pct2 = runner-up.
// Ties go to the file with fewer ids (the more specific one: a part file that is a subset of the main file does not steal it).
static int AbMatch(const AbTable* t, const uint32_t* obs, int nobs, int* pct, int* pct2) {
    int best = -1, bestPct = 0, second = 0;
    int nv = 0; for (int i = 0; i < nobs; ++i) if (AbOk(obs[i])) ++nv;
    for (int k = 0; k < t->n && nv; ++k) {
        const AbFile* f = &t->f[k]; int inter = 0;
        for (int i = 0; i < nobs; ++i) if (AbOk(obs[i]) && AbHas(f, obs[i])) ++inter;
        const int uni = nv + f->n - inter, p = uni ? inter * 100 / uni : 0;
        if (p > bestPct || (p == bestPct && best >= 0 && f->n < t->f[best].n)) { second = bestPct > p ? second : bestPct; bestPct = p; best = k; }
        else if (p > second) second = p;
    }
    if (pct) *pct = bestPct; if (pct2) *pct2 = second;
    return bestPct > AB_MIN_SCORE_PCT && bestPct - second >= AB_MIN_GAP_PCT ? best : -1;
}
// Shared / generic action files (the CP_HUMAN_* family: Common, Katana, Nitou ... used by many different humans) are never a boss's own file: stay unbound.
static inline int AbIsShared(const AbFile* f) { return !strncmp(f->chr, "CP_HUMAN_", 9); }
