// human #123773: boss hit-count coverage survey over data/extract/lfm/archive_01 (arg 1 = file list). 2026-11-20: 373 files, 6251/8251 attacks counted; gap 844 spawn, 346 untimed, 116 both, 694 no own rows.
// 2026-11-20 (pc2/hits-gap-126228): damaging-untimed rule -> 6873/8251; gap 285 spawn, 245 untimed, 88 both, 760 no own timed rows.
#include <stdio.h>
#include <stdlib.h>
#include "../../mod/Nioh3Training/src/auto_moves.h"
int main(int argc, char** argv) {
    static AmTable t; long A = 0, K = 0, S = 0, U = 0, N = 0, SU = 0, files = 0;
    char nm[1024]; FILE* lst = fopen(argv[1], "r");
    while (fgets(nm, sizeof nm, lst)) {
        nm[strcspn(nm, "\r\n")] = 0;
        FILE* f = fopen(nm, "rb"); if (!f) continue;
        fseek(f, 0, SEEK_END); long L = ftell(f); rewind(f);
        uint8_t* d = malloc(L); fread(d, 1, L, f); fclose(f);
        int na = AmParse(d, (uint32_t)L, &t);
        if (na <= 0) { free(d); continue; }
        files++;
        long a = 0, k = 0;
        for (int r = 0; r < na; r++) {
            AmMove* m = &t.m[r]; if (!m->attack) continue; A++; a++;
            if (m->hits) { K++; k++; }
            else if (m->spawns && m->untimed) SU++;
            else if (m->spawns) S++;
            else if (m->untimed) U++;
            else N++;
        }
        if (argc > 2) printf("%s\t%ld\t%ld\n", nm, a, k);
        free(d);
    }
    printf("files %ld attacks %ld counted %ld | unknown: spawn-only %ld untimed-only %ld both %ld no-timed-rows %ld\n", files, A, K, S, U, SU, N);
}
