// GitHub #126: the live move catalogue must hold the hand rows plus a long session of runtime bosses. With 8192 rows the 5th runtime boss
// (live sweep 11-05: Giant Toad 210803 after 3 human duelists) got 0 rows, so its Boss moves page was empty and 'Repeat last move' never
// captured an attack. Replays the sweep's spawn order (row counts from captures/rematch/fightnow-20261105-140459) and checks every boss
// gets its rows, the index answers exactly like the old linear scan, and an overflow is counted instead of silently lost.
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "../../move_catalog.h"
static int LinearFind(uint32_t chr, uint32_t id) { for (int i = 0; i < g_n3cat_n; ++i) if (g_n3cat[i].chr == chr && g_n3cat[i].id == id) return i; return -1; }
static int LinearMotion(uint32_t chr, uint32_t id) { for (int i = 0; i < g_n3mo_n; ++i) if (g_n3mo[i].chr == chr && g_n3mo[i].id == id) return i; return -1; }
int main(void) {
    static const struct { uint32_t chr; int n; } sweep[] = {
        {502003, 767}, {131751, 767}, {132808, 1488}, {352684, 90}, {445093, 107}, {847229, 767}, {816820, 90}, {451004, 149},
        {586715, 1646}, {810814, 118}, {264763, 93}, {776661, 1488}, {83953, 134}, {96976, 109}, {42450, 136}, {665148, 92},
        {405836, 107}, {614441, 767}, {412493, 490}, {96759, 104}, {485999, 767}, {210803, 126}, {969076, 725}, {63946, 1498},
        {602366, 1174}, {38227, 767}, {671570, 143}, {766640, 767}, {64826, 1454}};
    const int nb = (int)(sizeof sweep / sizeof sweep[0]);
    N3CatInit(1);
    const int hand = g_n3cat_n; assert(hand == kN3MovesHandN);
    for (int i = 0; i < hand; ++i) assert(N3CatFind(g_n3cat[i].chr, g_n3cat[i].id) == LinearFind(g_n3cat[i].chr, g_n3cat[i].id));
    for (int i = 0; i < g_n3mo_n; ++i) assert(N3CatMotionFind(g_n3mo[i].chr, g_n3mo[i].id) == LinearMotion(g_n3mo[i].chr, g_n3mo[i].id));
    AmTable* t = (AmTable*)calloc(1, sizeof(AmTable)); assert(t);
    for (int b = 0; b < nb; ++b) {
        memset(t, 0, sizeof *t); t->n = sweep[b].n;
        for (int k = 0; k < t->n; ++k) { AmMove* m = &t->m[k]; m->id = (uint16_t)(k < 2 ? k * 2 : 0x4E20 + k); m->motion = (uint16_t)(1000 + k); m->kind = k < 2 ? 'I' : (k % 9 == 0 ? 'A' : 'M'); m->next = 0xFFFF; m->link = (k % 50 == 7); m->next = m->link ? (uint16_t)(0x4E20 + k + 1) : 0xFFFF; m->fact = m->kind == 'A'; }
        N3CatMerge(sweep[b].chr, t->m, t->n, "boss");
        for (int k = 0; k < t->n; ++k) { const int i = N3CatFind(sweep[b].chr, t->m[k].id); assert(i >= 0 && i == LinearFind(sweep[b].chr, t->m[k].id)); { const int j = N3CatMotionFind(sweep[b].chr, t->m[k].id); assert(j >= 0 && j == LinearMotion(sweep[b].chr, t->m[k].id) && g_n3mo[j].motion == t->m[k].motion); } }
    }
    assert(!g_n3cat_dropped);
    const int toad = N3CatFind(210803u, 0x4E20 + 9); assert(toad >= 0 && g_n3cat[toad].kind == 'A');  // a capturable attack of the 22nd boss
    // misses stay misses
    assert(N3CatFind(210803u, 0x9999) == -1 && N3CatFind(1u, 0x4E64) == -1 && N3CatMotionFind(1u, 2) == -1);
    // past the cap: rows are counted as dropped, earlier rows still found
    const int before = g_n3cat_n; uint32_t chr = 900000; int fed = 0;
    while (g_n3cat_n < N3CAT_CAP && chr < 900100) { memset(t, 0, sizeof *t); t->n = 2000; for (int k = 0; k < t->n; ++k) { t->m[k].id = (uint16_t)(0x4E20 + k); t->m[k].kind = 'A'; t->m[k].next = 0xFFFF; } N3CatMerge(chr++, t->m, t->n, "fill"); fed += t->n; }
    assert(g_n3cat_n == N3CAT_CAP && g_n3cat_dropped == before + fed - N3CAT_CAP);
    assert(N3CatFind(210803u, 0x4E20 + 9) == toad);
    free(t);
    printf("PASS: %d hand rows + %d sweep bosses (%d rows, Giant Toad 210803 row %d), index == linear scan, cap %d, overflow counted (%ld)\n",
           hand, nb, before - hand, toad, N3CAT_CAP, (long)g_n3cat_dropped);
    return 0;
}
