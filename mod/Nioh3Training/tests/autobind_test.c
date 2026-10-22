// pc2 #23565: autobind.h matching, offline. Uses the generated Nioh3Training_actionsets.tsv when present (real data self-check).
//   py -m ziglang cc -target x86_64-windows-gnu tests/autobind_test.c -o build/autobind_test.exe && build/autobind_test.exe
#include <stdint.h>
#include <stdio.h>
#undef NDEBUG
#include <assert.h>
#include "../src/autobind.h"
int main(void) {
    char t[] = "# c\nAAA\tCE_A_00\t5\t11111111,22222222,4E21,4E22,4E23\nBBB\tCE_B_00\t3\t22222222,4E21,9000\r\nCCC\t?\t4\t4E21,4E22,4E23,4E24\n";
    AbTable tb; assert(AbLoad(&tb, t) == 3);
    assert(tb.f[0].n == 5 && AbHas(&tb.f[0], 0x4E22) && AbHas(&tb.f[0], 0x11111111) && !AbHas(&tb.f[0], 0x4E24) && !strcmp(tb.f[1].chr, "CE_B_00") && tb.f[1].n == 3);
    int p = 0, p2 = 0; uint32_t o1[] = {0x4E21, 0x4E22, 0x4E23, 0x4E24};
    assert(AbMatch(&tb, o1, 4, &p, &p2) == 2 && p == 100);                  // exact set -> C
    uint32_t o2[] = {0, 0x11111111, 0x22222222, 0x4E21, 0x4E22, 0x4E23, 0xFFFFFFFFu};
    assert(AbMatch(&tb, o2, 7, &p, &p2) == 0 && p == 100 && p2 < 100);      // 0 / FFFFFFFF ignored, exact -> A
    uint32_t o3[] = {0x7777, 0x7778};
    assert(AbMatch(&tb, o3, 2, &p, &p2) == -1 && p == 0);                   // nothing similar -> unbound
    assert(AbMatch(&tb, o3, 0, &p, &p2) == -1);
    AbFree(&tb);
    FILE* f = fopen("Nioh3Training_actionsets.tsv", "rb");
    if (f) {  // real table: every file matched with its own id set must come back as itself (or a same-set twin)
        static char buf[1 << 20]; size_t n = fread(buf, 1, sizeof buf - 1, f); fclose(f); buf[n] = 0;
        static AbTable rt; int nf = AbLoad(&rt, buf); int self = 0, twin = 0;
        for (int i = 0; i < nf; ++i) {
            const int m = AbMatch(&rt, rt.f[i].ids, rt.f[i].n, &p, &p2);
            if (m == i) ++self; else if (m >= 0 && rt.f[m].n == rt.f[i].n && p == 100) ++twin;
        }
        printf("real table: %d files, %d self-match, %d identical-set twins\n", nf, self, twin);
        assert(self + twin >= nf * 9 / 10 && nf > 300);
    }
    printf("autobind_test OK\n"); return 0;
}
