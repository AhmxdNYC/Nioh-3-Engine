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
    { static const uint8_t bad[8] = {16, 0, 0, 0}; assert(AmHeader(bad, sizeof bad) == 0); }  // a lone 16 is not a header
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
        // memory form (auto_moves_game.h): rows + hitbox rows as in the file, each action's hitbox list as (first, count). Rebuild and re-parse:
        // every field must be identical to the parse of the file itself. Also proves the file's lists are contiguous ranges.
        {
            static uint16_t first[AM_MAXMOVES], cnt[AM_MAXMOVES]; static AmTable t2;
            const uint32_t o0 = g_t.off[0], l0 = g_t.len[0]; uint32_t sum = 0;
            for (int r = 0; r < na; ++r) sum += AmU16(d, o0 + 64 + 2 * r);
            uint32_t q = o0 + l0 - sum;
            for (int r = 0; r < na; ++r) {
                const uint16_t c0 = AmU16(d, q); first[r] = c0 ? AmU16(d, q + 2) : 0; cnt[r] = c0;
                for (int k = 1; k < c0; ++k) assert(AmU16(d, q + 2 + 2 * k) == first[r] + k);  // contiguous
                q += AmU16(d, o0 + 64 + 2 * r);
            }
            const int nhb = (int)(g_t.len[2] / AM_HB); const uint32_t cap = AmImageSize(na, nhb, cnt);
            uint8_t* img = (uint8_t*)malloc(cap);
            const uint32_t sz = AmBuildImage(img, cap, d + g_t.off[1], na, d + g_t.off[2], nhb, first, cnt);
            assert(sz == cap && AmParse(img, sz, &t2) == na);
            assert(!memcmp(t2.m, g_t.m, sizeof(AmMove) * (size_t)na));
            free(img);
        }
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
            { int ng = 0; for (int q = 0; q < g_t.n; ++q) ng += g_t.m[q].grab; printf("Yoritomo grab-flagged actions: %d\n", ng);
              static const unsigned gid[] = {0x4E7B, 0x4EA7, 0x4E7C, 0x18D1};
              for (int q = 0; q < 4; ++q) { const int kg = AmFind(&g_t, gid[q]); assert(kg >= 0 && g_t.m[kg].grab); }
              const int kn = AmFind(&g_t, 0x4E68); assert(!g_t.m[kn].grab); }
            assert(c.next_bad <= 1);  // the only hand edit: 0x4E78 -> 0x4E79 (AI continuation, gen_boss_sequences.py OVERRIDES); the file says 0x4E7A
        } else assert(c.atk_bad == 0);
        free(d);
    }
    printf("auto_moves_test ok (%d real files)\n", real);
    return 0;
}
