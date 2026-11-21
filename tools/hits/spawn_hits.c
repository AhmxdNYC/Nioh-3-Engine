// human #123773: spawned-object hit check (auto_moves.h AmObjMayHit) over data/extract/lfm/archive_01 (arg 1 = file list, arg 2 "-v" = list moves).
// 1. layout: sub-table 7 / 8 record sizes, object child / component / hitbox-slot references in range;
// 2. moves the rule newly counts (non-ambient spawns, all hit-free) vs their same-motion siblings counted without any spawn;
// 3. control: counted-vs-counted same-motion siblings. 2026-11-20: 373 files, layout all in range; 587 newly counted, 304 / 324 sibling pairs
//    agree (93.8 %), control 91.2 %.
#include <stdio.h>
#include <stdlib.h>
#include "../../mod/Nioh3Training/src/auto_moves.h"
static int RawSpawns(const uint8_t* d, const AmTable* t, int r) {  // non-ambient spawn events, before the object check (old AmParse rule)
    const uint32_t o = t->off[1] + (uint32_t)r * AM_ROW; int n = 0;
    for (int k = 0; k < 16; ++k) {
        const uint16_t ev = AmU16(d, o + 0x40 + 6 * k), fr = AmU16(d, o + 0x42 + 6 * k);
        if (ev < 20000 || ev == 0xFFFF) continue;
        int amb = 0;
        if (fr == 0) { int c = 0; for (int q = 0; q < t->n; ++q) { const uint32_t oq = t->off[1] + (uint32_t)q * AM_ROW; for (int z = 0; z < 16; ++z) if (AmU16(d, oq + 0x40 + 6 * z) == ev && AmU16(d, oq + 0x42 + 6 * z) == 0) { ++c; break; } } amb = c * 10 >= t->n * 8; }
        n += !amb;
    }
    return n;
}
int main(int argc, char** argv) {
    static AmTable t; static uint8_t newc[AM_MAXMOVES];
    long files = 0, badsz = 0, kids = 0, kidbad = 0, comps = 0, compbad = 0, slots = 0, slotbad = 0, added = 0, pairs = 0, agree = 0, cp = 0, ca = 0;
    char nm[1024]; FILE* lst = argc > 1 ? fopen(argv[1], "r") : 0;
    if (!lst) { puts("usage: spawn_hits <file list> [-v]"); return 2; }
    while (fgets(nm, sizeof nm, lst)) {
        nm[strcspn(nm, "\r\n")] = 0;
        FILE* f = fopen(nm, "rb"); if (!f) continue;
        fseek(f, 0, SEEK_END); long L = ftell(f); rewind(f);
        uint8_t* d = malloc(L); fread(d, 1, L, f); fclose(f);
        const int na = AmParse(d, (uint32_t)L, &t);
        if (na <= 0) { free(d); continue; }
        ++files;
        const uint32_t o0 = t.off[0], n7 = AmU16(d, o0 + 30), n8 = AmU16(d, o0 + 34);
        if (AmU16(d, o0 + 32) != 336 || n7 * 336u != t.len[7] || (t.len[8] && (AmU16(d, o0 + 36) != 80 || n8 * 80u != t.len[8]))) ++badsz;
        for (uint32_t i = 0; i < n7; ++i) {
            const uint32_t b = t.off[7] + i * 336;
            for (int k = 0; k < 4; ++k) { const uint16_t c = AmU16(d, b + 0x100 + 2 * k); if (c && c != 0xFFFF) { ++kids; kidbad += c < 20000 || c - 20000u >= n7; } }
            for (int k = 0; k < 6; ++k) { const uint16_t c = AmU16(d, b + 0x110 + 2 * k); if (c != 0xFFFF) { ++comps; compbad += c >= n8; } }
            if (d[b + 0x15] != 0xFF) { ++slots; const int r = AmFind(&t, AmU16(d, b + 0x12)); slotbad += r < 0 || d[b + 0x15] >= t.m[r].nrows; }
        }
        for (int r = 0; r < na; ++r) { const AmMove* m = &t.m[r]; newc[r] = m->attack && m->hits && RawSpawns(d, &t, r) > 0; added += newc[r]; if (newc[r] && argc > 2) printf("%s\t0x%04X\tmotion %u\thits %u\n", nm, m->id, m->motion, m->hits); }
        for (int r = 0; r < na; ++r) {
            const AmMove* m = &t.m[r]; if (!m->attack || !m->hits || !m->motion) continue;
            for (int q = 0; q < na; ++q) {
                const AmMove* s = &t.m[q]; if (q == r || !s->attack || !s->hits || newc[q] || s->motion != m->motion) continue;
                if (newc[r]) { ++pairs; agree += s->hits == m->hits; } else if (q > r) { ++cp; ca += s->hits == m->hits; }
            }
        }
        free(d);
    }
    printf("files %ld, sub-table 7/8 size mismatch %ld; object children %ld (out of range %ld), components %ld (%ld), hitbox slots %ld (%ld)\n", files, badsz, kids, kidbad, comps, compbad, slots, slotbad);
    printf("newly counted (hit-free spawns) %ld; same-motion sibling pairs %ld agree %ld (%.1f%%); control counted pairs %ld agree %.1f%%\n", added, pairs, agree, pairs ? 100.0 * agree / pairs : 0, cp, cp ? 100.0 * ca / cp : 0);
    return 0;
}
