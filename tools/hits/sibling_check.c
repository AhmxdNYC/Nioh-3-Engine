// human #123773: same-motion sibling stand-in check. 2026-11-20: pairs agree 91.2%; only 145/693 no-row attacks have a counted sibling -> not a full-coverage rule.
#include <stdio.h>
#include <stdlib.h>
#include "../../mod/Nioh3Training/src/auto_moves.h"
// For attacks with no own timed rows: can a same-motion sibling's count stand in? Validate on pairs where both are counted.
int main(int argc, char** argv) {
    static AmTable t; long gap = 0, gapSib = 0, gapSibAgree = 0, pairs = 0, agree = 0;
    char nm[1024]; FILE* lst = fopen(argv[1], "r");
    while (fgets(nm, sizeof nm, lst)) {
        nm[strcspn(nm, "\r\n")] = 0;
        FILE* f = fopen(nm, "rb"); if (!f) continue;
        fseek(f, 0, SEEK_END); long L = ftell(f); rewind(f);
        uint8_t* d = malloc(L); fread(d, 1, L, f); fclose(f);
        int na = AmParse(d, (uint32_t)L, &t);
        for (int r = 0; r < na; r++) {
            AmMove* m = &t.m[r]; if (!m->attack || m->motion == 0) continue;
            if (m->hits) {  // validation: every other counted sibling on the same motion
                for (int q = r + 1; q < na; q++) { AmMove* o = &t.m[q]; if (o->attack && o->hits && o->motion == m->motion) { pairs++; agree += o->hits == m->hits; } }
                continue;
            }
            if (m->spawns || m->untimed || m->nwin) continue;
            gap++;
            int v = -1, consistent = 1;
            for (int q = 0; q < na; q++) { AmMove* o = &t.m[q]; if (q == r || !o->hits || o->motion != m->motion) continue; if (v < 0) v = o->hits; else if (v != o->hits) consistent = 0; }
            if (v > 0) { gapSib++; gapSibAgree += consistent; }
        }
        free(d);
    }
    printf("validation: same-motion counted pairs %ld, equal %ld (%.1f%%)\n", pairs, agree, pairs ? 100.0 * agree / pairs : 0);
    printf("no-own-rows attacks %ld, with a counted same-motion sibling %ld, siblings all agree %ld\n", gap, gapSib, gapSibAgree);
}
