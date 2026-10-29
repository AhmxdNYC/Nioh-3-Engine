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
    uint32_t o4[] = {0x4E21, 0x4E22, 0x4E23};   // C 3/4 = 75 %, A 3/5 = 60 % -> gap exactly 15: binds C
    { const int r = AbMatch(&tb, o4, 3, &p, &p2); assert(r == 2 && p == 75 && p2 == 60); }
    { char t2[] = "X\tCE_X_00\t4\t1,2,3,4\nY\tCE_Y_00\t4\t1,2,3,5\n"; AbTable b; AbLoad(&b, t2); uint32_t o[] = {1, 2, 3, 4, 5};
      assert(AbMatch(&b, o, 5, &p, &p2) == -1 && p == 80 && p2 == 80); AbFree(&b); }   // runner-up as good (80 % vs 80 %) -> unbound
    assert(AbIsShared(&(AbFile){.chr = "CP_HUMAN_COMMON"}) && !AbIsShared(&tb.f[0]));
    // pc #23911: near-twin runner-up of the same family (Red Kappa 100 vs 91, Ippon-Datara 100 vs 90) binds the top file; another family stays unbound
    { char t3[] = "K1\tCE_KAPPA_00\t10\t1,2,3,4,5,6,7,8,9,A\nK2\tCE_KAPPA_00\t11\t1,2,3,4,5,6,7,8,9,A,B\nO\tCE_ONI_00\t3\t70,71,72\n";
      AbTable b; AbLoad(&b, t3); uint32_t o[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA};
      assert(AbMatch(&b, o, 10, &p, &p2) == 0 && p == 100 && p2 == 90); AbFree(&b); }
    { char t4[] = "K1\tCE_KAPPA_00\t10\t1,2,3,4,5,6,7,8,9,A\nN\tCE_NUREONNA_00\t11\t1,2,3,4,5,6,7,8,9,A,B\n";
      AbTable b; AbLoad(&b, t4); uint32_t o[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA};
      assert(AbMatch(&b, o, 10, &p, &p2) == -1 && p == 100 && p2 == 90); AbFree(&b); }
    { char t5[] = "Q1\t?\t10\t1,2,3,4,5,6,7,8,9,A\nQ2\t?\t11\t1,2,3,4,5,6,7,8,9,A,B\n";   // no owner = no family: stays unbound
      AbTable b; AbLoad(&b, t5); uint32_t o[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA};
      assert(AbMatch(&b, o, 10, &p, &p2) == -1); AbFree(&b); }
    { char x[AB_NAME]; AbFile f = {.chr = "CE_KAPPA_00"}; assert(AbFamily(&f, x, sizeof x) && !strcmp(x, "CE_KAPPA"));
      AbFile g = {.chr = "CN_SHINGEN"}; assert(AbFamily(&g, x, sizeof x) && !strcmp(x, "CN_SHINGEN")); }
    AbFree(&tb);
    FILE* f = fopen("Nioh3Training_actionsets.tsv", "rb");
    if (f) {  // real table: every file matched with its own id set must come back as itself (or a same-set twin)
        static char buf[1 << 20]; size_t n = fread(buf, 1, sizeof buf - 1, f); fclose(f); buf[n] = 0;
        static AbTable rt; int nf = AbLoad(&rt, buf); int self = 0, twin = 0;
        for (int i = 0; i < nf; ++i) {
            const int m = AbMatch(&rt, rt.f[i].ids, rt.f[i].n, &p, &p2);
            if (m == i) ++self; else if (m < 0 && p == 100 && p2 >= 100 - AB_MIN_GAP_PCT) ++twin; else if (m >= 0 && AbSameFamily(&rt.f[m], &rt.f[i])) ++twin; else if (m >= 0) printf("WRONG %s -> %s\n", rt.f[i].chr, rt.f[m].chr);  // identical-set twin: ambiguous -> unbound
        }
        printf("real table: %d files, %d self-match, %d identical-set twins\n", nf, self, twin);
        assert(self + twin >= nf * 8 / 10 && nf > 300);
        // pc2 #24074: multi-block union. Each non-shared file of >= 20 ids plus a third of an unrelated file as noise must bind itself (or an identical-set / same-family twin),
        // never another family.
        { int ok = 0, tw = 0, un = 0, bad = 0, cnt = 0;
          for (int i = 0; i < nf; ++i) {
              const AbFile* f = &rt.f[i]; if (f->n < AB_UNION_MIN || AbIsShared(f)) continue;
              static uint32_t u[8192]; int n = 0; for (int k = 0; k < f->n; ++k) u[n++] = f->ids[k];
              const AbFile* g = &rt.f[(i * 7 + 13) % nf]; for (int k = 0; k < g->n / 3 && n < 8000; ++k) u[n++] = g->ids[k];
              int c = 0, i2 = 0; const int m = AbMatchUnion(&rt, u, n, &c, &i2); ++cnt;
              if (m == i) ++ok; else if (m < 0) ++un; else if (AbSameFamily(&rt.f[m], f) || (rt.f[m].n == f->n && c == 100)) ++tw; else { ++bad; printf("UNION WRONG %s -> %s\n", f->chr, rt.f[m].chr); }
          }
          printf("union: %d files, %d self, %d twin, %d unbound(ambiguous), %d wrong\n", cnt, ok, tw, un, bad);
          assert(bad == 0 && ok >= cnt / 2); }
        // pc2 #24074: a fragment (every 2nd id, >= 8 ids) of a file is contained in that file -> binds it (or an identical-set / same-family twin), never another family.
        { int ok = 0, tw = 0, un = 0, bad = 0, cnt = 0;
          for (int i = 0; i < nf; ++i) {
              const AbFile* f = &rt.f[i]; if (f->n < 2 * AB_CONTAIN_MIN) continue;
              static uint32_t u[8192]; int n = 0; for (int k = 0; k < f->n; k += 2) u[n++] = f->ids[k];
              int c = 0; const int m = AbBlockFile(&rt, u, n, &c); ++cnt;
              if (m == i) ++ok; else if (m < 0) ++un; else if (AbSameFamily(&rt.f[m], f) || c == 100) ++tw; else { ++bad; printf("BLOCK WRONG %s -> %s\n", f->chr, rt.f[m].chr); }
          }
          printf("fragments: %d, %d self, %d twin/contained, %d unresolved, %d wrong\n", cnt, ok, tw, un, bad);
          assert(bad == 0 && ok + tw >= cnt / 2); }
    }
    printf("autobind_test OK\n"); return 0;
}
