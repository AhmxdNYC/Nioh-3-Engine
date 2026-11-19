// GitHub #126 (review #119317): the low-id attack rule of move_last_rule.h on the REAL game data path: AmParse of the extracted action files,
// N3CatMerge into the live catalogue, then the rule on the merged kind / motion / facts rows (what MlAttack sees in game). Giant Toad's
// low-id attacks must count; the step / turn / guard rows that every boss has below 0x4E20 (motions 30-83) and attack-less recoveries must not.
//   py -m ziglang cc -O0 -fms-extensions tests/last_attack_rule_test.c -o build/last_attack_rule_test.exe   (cwd mod/Nioh3Training)
// The extracted files are not in git (data/extract, the main checkout or N3_EXTRACT); missing -> that boss is skipped (stated).
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#undef NDEBUG
#include <assert.h>
#include "../src/auto_moves_cmp.h"
#include "../../move_catalog.h"
#include "../../move_last_rule.h"
static uint8_t* ReadAll(const char* rel, uint32_t* n) {
    static const char* roots[] = {"../../data/extract", "../../../nioh3-engine/data/extract", "../../../../nioh3-engine/data/extract", "data/extract"};
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
static int Rule(uint32_t chr, uint32_t id) {
    const int i = N3CatFind(chr, id); if (i < 0) return -1;
    const int j = N3CatMotionFind(chr, id);
    return N3LowIdAttack(g_n3cat[i].kind, j >= 0 ? g_n3mo[j].motion : 0, N3CatFactFind(chr, id));
}
static AmTable g_t;
int main(void) {
    setvbuf(stdout, 0, _IONBF, 0);
    static const struct { uint32_t chr; const char* name; const char* rel; uint16_t yes[6], no[8]; } B[] = {
        {210803u, "Giant Toad", "98/A39544553277A061AB34F7A3EB9B7A0FBFA834260AC9BA7CBEFD1C69D3E7EF",
         {0xC61, 0xC67, 0xC7C, 0xC83, 0x1770, 0xC9B}, {0x1E, 0x1F, 0x20, 0x21, 0xC52, 0xC5A, 0x10, 0xC7F}},
        {482895u, "Baba Nobuharu", "5E/B7FA064C0CC4146F0D8543E8BA5E5551669A50436A9C0B5FC327059F858A22",
         {0x1869}, {0x3FA, 0x3FB, 0x3FC, 0x3FD}},
        {42450u, "Takeda Shingen", "E5/D7F8918446ABDAF10213EA35BE55B598075FD371A0733C32B6C96C7094F6DE",
         {0x1875, 0x187B}, {0x3FA, 0x3FB, 0x3FC, 0x3FD}},
        {16102u, "Minamoto no Yoritomo", "FA/5FBD421BF96B8514DA056584B9507A22DC7956B1793995A113FBA714CFFA54",
         {0x18D1}, {0x3FB, 0x3FC}},
    };
    N3CatInit(1);
    int real = 0;
    for (int b = 0; b < 4; ++b) {
        uint32_t n = 0; uint8_t* d = ReadAll(B[b].rel, &n);
        if (!d) { printf("SKIP %s: action file not extracted\n", B[b].name); continue; }
        ++real;
        assert(AmParse(d, n, &g_t) > 0);
        N3CatMerge(B[b].chr, g_t.m, g_t.n, B[b].name);
        int low = 0, taken = 0;
        for (int k = 0; k < g_t.n; ++k) {
            const AmMove* m = &g_t.m[k]; if (m->id >= 0x4E20) continue;
            ++low; const int r = Rule(B[b].chr, m->id); assert(r >= 0);
            if (r) { ++taken; assert(m->motion >= 1000 && m->motion < 30000 && m->kind == 'A'); }  // never a step / guard / hit reaction / Ki break
        }
        for (int k = 0; k < 6 && B[b].yes[k]; ++k) { if (Rule(B[b].chr, B[b].yes[k]) != 1) { printf("FAIL %s 0x%X should count\n", B[b].name, B[b].yes[k]); assert(0); } }
        for (int k = 0; k < 8 && B[b].no[k]; ++k) { if (Rule(B[b].chr, B[b].no[k]) == 1) { printf("FAIL %s 0x%X should not count\n", B[b].name, B[b].no[k]); assert(0); } }
        printf("PASS %s: %d low-id rows, %d count as attacks\n", B[b].name, low, taken);
        free(d);
    }
    printf("PASS: low-id attack rule on %d real action files (GitHub #126)\n", real);
    return 0;
}
