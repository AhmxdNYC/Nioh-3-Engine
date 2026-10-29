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
// Character family of a file: its character key without the trailing "_<digits>" variant (CE_KAPPA_00 -> CE_KAPPA). "?" (no owner) has no family.
static int AbFamily(const AbFile* f, char* out, size_t n) {
    if (!f->chr[0] || !strcmp(f->chr, "?")) return 0;
    snprintf(out, n, "%s", f->chr);
    size_t L = strlen(out), e = L; while (e && out[e - 1] >= '0' && out[e - 1] <= '9') --e;
    if (e < L && e && out[e - 1] == '_') out[e - 1] = 0;
    return 1;
}
static int AbSameFamily(const AbFile* a, const AbFile* b) {
    char x[AB_NAME], y[AB_NAME];
    return AbFamily(a, x, sizeof x) && AbFamily(b, y, sizeof y) && !strcmp(x, y);
}
static int AbMatch(const AbTable* t, const uint32_t* obs, int nobs, int* pct, int* pct2) {
    int best = -1, bestPct = 0, second = 0, sec = -1;
    int nv = 0; for (int i = 0; i < nobs; ++i) if (AbOk(obs[i])) ++nv;
    for (int k = 0; k < t->n && nv; ++k) {
        const AbFile* f = &t->f[k]; int inter = 0;
        for (int i = 0; i < nobs; ++i) if (AbOk(obs[i]) && AbHas(f, obs[i])) ++inter;
        const int uni = nv + f->n - inter, p = uni ? inter * 100 / uni : 0;
        if (p > bestPct || (p == bestPct && best >= 0 && f->n < t->f[best].n)) { second = bestPct; sec = best; bestPct = p; best = k; }
        else if (p > second) { second = p; sec = k; }
    }
    if (pct) *pct = bestPct; if (pct2) *pct2 = second;
    if (bestPct <= AB_MIN_SCORE_PCT) return -1;
    if (bestPct - second >= AB_MIN_GAP_PCT) return best;
    // pc #23911: a near-twin runner-up of the SAME character family (Red Kappa CE_KAPPA_00 100 vs 91, Ippon-Datara CE_IPPONDATARA_00 100 vs 90:
    // variant files of one boss with near-identical move sets) does not make the match ambiguous: bind the top one. A different family stays unbound.
    return sec >= 0 && second < bestPct && AbSameFamily(&t->f[best], &t->f[sec]) ? best : -1;
}
// Shared / generic action files (the CP_HUMAN_* family: Common, Katana, Nitou ... used by many different humans) are never a boss's own file: stay unbound.
static inline int AbIsShared(const AbFile* f) { return !strncmp(f->chr, "CP_HUMAN_", 9); }

// pc2 #24074: multi-block bosses (Himiko: 5 row blocks, none a whole file). A boss's own action file can be spread over several memory blocks, so no single block reaches
// the Jaccard bar. Fallback: the union of the observed ids (blocks that bound a shared CP_HUMAN_* file left out) is matched by COVERAGE: share of a file's own ids present in
// the union (>= AB_UNION_COV_PCT, file of >= AB_UNION_MIN ids, never a shared file). Most covered ids win; a different-family file within 85 % of it makes the call ambiguous -> -1.
enum { AB_UNION_COV_PCT = 90, AB_UNION_MIN = 20 };
static int AbMatchUnion(const AbTable* t, const uint32_t* obs, int nobs, int* cov, int* inter2) {
    int best = -1, bi = 0, sec = -1, si = 0;
    for (int k = 0; k < t->n; ++k) {
        const AbFile* f = &t->f[k]; if (f->n < AB_UNION_MIN || AbIsShared(f)) continue;
        int inter = 0; for (int i = 0; i < nobs; ++i) if (AbOk(obs[i]) && AbHas(f, obs[i])) ++inter;
        if (inter * 100 < f->n * AB_UNION_COV_PCT) continue;
        if (inter > bi || (inter == bi && best >= 0 && f->n < t->f[best].n)) { sec = best; si = bi; best = k; bi = inter; }
        else if (inter > si) { sec = k; si = inter; }
    }
    if (cov) *cov = best >= 0 ? bi * 100 / t->f[best].n : 0;
    if (inter2) *inter2 = si;
    if (best < 0) return -1;
    if (sec >= 0 && si * 100 >= bi * 85 && !AbSameFamily(&t->f[best], &t->f[sec])) return -1;
    return best;
}
// pc2 #24074: a block that is a fragment of a file (Himiko: her 5 blocks are 100 % inside the shared CP_HUMAN_NAGIGAMA / CP_HUMAN_COMMON files, her own CN_HIMIKO file is not mapped).
// Containment = share of the BLOCK's ids found in a file. The file holding the most ids wins when >= AB_CONTAIN_PCT of the block is inside it and the runner-up holds <= 85 % as many
// (ties: the smaller file). Returns the index or -1; *pct = containment of the winner.
enum { AB_CONTAIN_PCT = 90, AB_CONTAIN_MIN = 8 };
static int AbBlockFile(const AbTable* t, const uint32_t* obs, int nobs, int* pct) {
    int best = -1, sec = -1, bi = 0, si = 0, nv = 0; for (int i = 0; i < nobs; ++i) if (AbOk(obs[i])) ++nv;
    if (pct) *pct = 0; if (nv < AB_CONTAIN_MIN) return -1;
    for (int k = 0; k < t->n; ++k) {
        const AbFile* f = &t->f[k]; int inter = 0; for (int i = 0; i < nobs; ++i) if (AbOk(obs[i]) && AbHas(f, obs[i])) ++inter;
        if (inter > bi || (inter == bi && best >= 0 && f->n < t->f[best].n)) { si = bi; sec = best; best = k; bi = inter; } else if (inter > si) { si = inter; sec = k; }
    }
    if (pct) *pct = best >= 0 ? bi * 100 / nv : 0;
    if (best < 0 || bi * 100 < nv * AB_CONTAIN_PCT) return -1;
    // pc2 #24140: a runner-up of the SAME family (CP_HUMAN_KATANA x3 variants, the CP_HUMAN_COMMON parts) holds the same moves under another file key: not ambiguous, the top one wins.
    if (sec >= 0 && si * 100 > bi * 85 && !AbSameFamily(&t->f[best], &t->f[sec])) return -1;
    return best;
}

// Conservative composed-human classifier: both shared common and weapon families.
static inline int AbHumanFamily(const char* name) {
    if(!strcmp(name,"CP_HUMAN_COMMON")) return 1;
    return !strncmp(name,"CP_HUMAN_",9) && name[9] ? 2 : 0;
}
static inline int AbComposedHuman(int own,int common,int weapon,int explained,int rows) {
    return !own && common && weapon && rows>0 && explained*100>=rows*80;
}
