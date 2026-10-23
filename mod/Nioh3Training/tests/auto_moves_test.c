// pc2 #23907: auto_moves.h on real extracted archive_01 action files (Yoritomo, Baba Nobuharu, Takeda Shingen), compared with the old hand headers.
//   py -m ziglang cc -target x86_64-windows-gnu tests/auto_moves_test.c -o build/auto_moves_test.exe && build/auto_moves_test.exe
// The extracted files are not in git: looked up in data/extract (this checkout, the main checkout, sibling worktrees) or N3_EXTRACT. Missing -> the
// real-file part is skipped (stated), the synthetic header search still runs.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#undef NDEBUG
#include <assert.h>
#include "../src/auto_moves_cmp.h"

static uint8_t* ReadAll(const char* rel, uint32_t* n) {
    static const char* roots[] = {"../../data/extract", "../../../nioh3-engine/data/extract", "../../../nioh3-autobind/data/extract",  // cwd mod/Nioh3Training
                                  "../../../nioh3-autoframe/data/extract", "../../../data/extract", "../../../../nioh3-engine/data/extract",  // cwd tests
                                  "../../../../nioh3-autobind/data/extract", "../../../../nioh3-autoframe/data/extract", "data/extract"};
    char p[512]; const char* env = getenv("N3_EXTRACT");
    for (int i = -1; i < (int)(sizeof roots / sizeof roots[0]); ++i) {
        if (i < 0 && !env) continue;
        snprintf(p, sizeof p, "%s/lfm/archive_01/%s", i < 0 ? env : roots[i], rel);
        FILE* f = fopen(p, "rb"); if (!f) continue;
        fseek(f, 0, SEEK_END); long L = ftell(f); fseek(f, 0, SEEK_SET);
        uint8_t* d = (uint8_t*)malloc((size_t)L); *n = (uint32_t)fread(d, 1, (size_t)L, f); fclose(f);
        return d;
    }
    return 0;
}
static AmTable g_t;
int main(void) {
    setvbuf(stdout, 0, _IONBF, 0);
    // synthetic: header search must reject a lone 16 and find a real header before the row
    {
        static uint8_t buf[8192]; memset(buf, 0xCD, sizeof buf);
        const uint32_t base = 1000;  // not 8-aligned on purpose (4-aligned)
        uint32_t* w = (uint32_t*)(buf + 16); *w = 16;  // decoy
        uint8_t* h = buf + base; uint32_t off = 132, len[16] = {72, 2 * AM_ROW, AM_HB, 0};
        memcpy(h, &(uint32_t){16}, 4);
        for (int i = 0; i < 16; ++i) { memcpy(h + 4 + 8 * i, &off, 4); memcpy(h + 8 + 8 * i, &len[i], 4); off += len[i]; }
        const uint32_t w1 = 2 | (AM_ROW << 16), w2 = 1 | (AM_HB << 16); memcpy(h + 132 + 4, &w1, 4); memcpy(h + 132 + 8, &w2, 4);
        const uint64_t bb = 0x10000000ull, row = bb + base + 132 + 72 + AM_ROW;
        assert(AmFindBase(buf, bb, sizeof buf, row) == base);
        assert(AmFindBase(buf, bb, sizeof buf, row + 8) == -1);   // not on a row boundary
        assert(AmHeader(buf + 16, sizeof buf - 16) == 0);
    }
    static const struct { uint32_t chr; const char* name; const char* rel; } B[] = {
        {16102u, "Minamoto no Yoritomo", "FA/5FBD421BF96B8514DA056584B9507A22DC7956B1793995A113FBA714CFFA54"},
        {482895u, "Baba Nobuharu", "5E/B7FA064C0CC4146F0D8543E8BA5E5551669A50436A9C0B5FC327059F858A22"},
        {42450u, "Takeda Shingen", "E5/D7F8918446ABDAF10213EA35BE55B598075FD371A0733C32B6C96C7094F6DE"},
    };
    int real = 0;
    for (int b = 0; b < 3; ++b) {
        uint32_t n = 0; uint8_t* d = ReadAll(B[b].rel, &n);
        if (!d) { printf("SKIP %s: action file not extracted\n", B[b].name); continue; }
        ++real;
        const int na = AmParse(d, n, &g_t); assert(na > 0 && g_t.size == n);
        // header search inside a padded copy, row in the middle of the row block
        uint8_t* pad = (uint8_t*)malloc(n + 4096); memset(pad, 0x10, 4096); memcpy(pad + 4096, d, n);
        const uint64_t bb = 0x7FF000000000ull, row = bb + 4096 + g_t.off[1] + (uint32_t)(na / 2) * AM_ROW;
        assert(AmFindBase(pad, bb, n + 4096, row) == 4096); free(pad);
        AmCmp c; AmCompare(&g_t, B[b].chr, &c); char line[2048]; AmCmpText(&c, line, sizeof line);
        int nA = 0, nM = 0, nI = 0, nAtk = 0, nBlk = 0; for (int i = 0; i < na; ++i) { nA += g_t.m[i].kind == 'A'; nM += g_t.m[i].kind == 'M'; nI += g_t.m[i].kind == 'I'; nAtk += g_t.m[i].attack; nBlk += g_t.m[i].block; }
        printf("%s (chr %u): %d actions, attack-block %d, attack %d, kinds A %d M %d I %d\n  %s\n  sequence heads:", B[b].name, B[b].chr, na, nBlk, nAtk, nA, nM, nI, line);
        for (int i = 0; i < na; ++i) if (AmSeqHead(&g_t, i)) {
            int ids[8], lp; const int k = AmSeqChain(&g_t, i, ids, 8, &lp);
            printf(" ["); for (int q = 0; q < k; ++q) printf("%s%X", q ? ">" : "", g_t.m[ids[q]].id); printf("%s]", lp ? " loop" : "");
        }
        printf("\n");
        // the generated headers came from these files with the same rules: ids, facts, great and hits must agree exactly
        assert(c.ids_hand_only == 0 && c.ids_rt_only == 0 && c.fact_hand_only == 0 && c.fact_rt_only == 0);
        assert(c.great_bad == 0 && c.hits_bad == 0);
        if (B[b].chr == 16102u) {
            const int k = AmFind(&g_t, 0x4E6E); assert(k >= 0 && g_t.m[k].hits == 2 && g_t.m[k].great && g_t.m[k].block);
            assert(nBlk == 88);
            const int k2 = AmFind(&g_t, 0x4E68); assert(g_t.m[k2].hits == 3 && !g_t.m[k2].great);
            assert(c.next_bad <= 1);  // the only hand edit: 0x4E78 -> 0x4E79 (AI continuation, gen_boss_sequences.py OVERRIDES); the file says 0x4E7A
        } else assert(c.atk_bad == 0);
        free(d);
    }
    printf("auto_moves_test ok (%d real files)\n", real);
    return 0;
}
