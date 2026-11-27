// soul_core_name.h (Nioh3Training, C) - TASK #137305 (human #137303): the yokai name of a soul core item, from the game's own tables.
// Pure logic (no game access; tests/soul_core_name_test.c). Chain (pc2 #81523 core_chain.py for steps 1-3, static check 11-26 for 4):
//   1. core item row (item table [GameData+0x68], stride 0x1A0): u16 +0x180 = soul-core key   (soul cores have no name text of their own)
//   2. soul-core table [GameData+0x118], stride 0x54: u32 +0x0C = key, u32 +0x18 = Yang item = its Summoning Seal item
//   3. seal item row (item table): u32 +0x54 = the seal's spawn character id (the desc[0] BR logs at a seal press)
//   4. character param table (file archive_03 C4/E1CBAE08.., 1022 rows, stride 0x398): u32 +0x64 = spawn id, u32 +0xA0 = name text id
//      (text group 1 = names, id >> 22 == 1). The game itself shows "Summoning Seal: {}" filled with that name.
// The character table's GameData slot is not a fixed offset here: ScnFindChr tries each GameData slot and keeps the one whose rows hold the
// chain's own spawn id with a group-1 name. Every value comes from the running game; nothing is per core.
#ifndef N3_SOUL_CORE_NAME_H
#define N3_SOUL_CORE_NAME_H
#include <stdint.h>
#include <string.h>

#define SCN_ITEM_STRIDE 0x1A0u
#define SCN_SC_STRIDE 0x54u
#define SCN_CHR_STRIDE 0x398u
#define SCN_MAGIC 0x20042200u
#define SCN_GD_SLOTS 0x200  // GameData qword slots tried for the character table (0x1000 bytes)

// reader: 1 = read n bytes at a, 0 = fault
typedef int (*ScnRead)(void* ctx, uint64_t a, void* out, unsigned n);
typedef struct { uint64_t rows; uint32_t n; } ScnTable;

static inline int ScnRd32(ScnRead rd, void* c, uint64_t a, uint32_t* v) { return rd(c, a, v, 4); }
static inline int ScnRd16(ScnRead rd, void* c, uint64_t a, uint16_t* v) { return rd(c, a, v, 2); }
// wrapper = [gd+off], header = [wrapper]; header +0 magic, +4 count, rows from +8
static inline int ScnTableAt(ScnRead rd, void* c, uint64_t gd, uint32_t off, ScnTable* t) {
    uint64_t w = 0, h = 0; uint32_t m = 0, n = 0;
    if (!rd(c, gd + off, &w, 8) || w < 0x10000u || !rd(c, w, &h, 8) || h < 0x10000u) return 0;
    if (!ScnRd32(rd, c, h, &m) || m != SCN_MAGIC || !ScnRd32(rd, c, h + 4, &n) || !n || n > 200000u) return 0;
    t->rows = h + 8; t->n = n; return 1;
}
static inline int ScnItemRow(ScnRead rd, void* c, const ScnTable* it, uint16_t id, uint64_t* row) {
    for (uint32_t i = 0; i < it->n; ++i) {
        uint16_t v; const uint64_t r = it->rows + (uint64_t)i * SCN_ITEM_STRIDE;
        if (!ScnRd16(rd, c, r + 0x152, &v)) return 0;
        if (v == id) { *row = r; return 1; }
    }
    return 0;
}
// steps 1-3: core item id -> seal spawn character id (0 = not a mapped core)
static inline uint32_t ScnSpawnId(ScnRead rd, void* c, const ScnTable* it, const ScnTable* sc, uint16_t core) {
    uint64_t r = 0, sr = 0; uint16_t key = 0; uint32_t cat = 0, yang = 0, spawn = 0;
    if (!ScnItemRow(rd, c, it, core, &r) || !ScnRd32(rd, c, r + 0x64, &cat) || cat != 900000000u || !ScnRd16(rd, c, r + 0x180, &key)) return 0;
    for (uint32_t i = 0; i < sc->n; ++i) {
        const uint64_t s = sc->rows + (uint64_t)i * SCN_SC_STRIDE; uint32_t k = 0;
        if (!ScnRd32(rd, c, s + 0x0C, &k)) return 0;
        if (k == key) { if (!ScnRd32(rd, c, s + 0x18, &yang)) return 0; break; }
    }
    if (!yang || yang > 0xFFFFu || !ScnItemRow(rd, c, it, (uint16_t)yang, &sr) || !ScnRd32(rd, c, sr + 0x54, &spawn)) return 0;
    return spawn;
}
// step 4 in one table: the name text id of the first row with +0x64 == spawn and a group-1 name (0 = none)
static inline uint32_t ScnNameInChr(ScnRead rd, void* c, const ScnTable* ch, uint32_t spawn) {
    if (!spawn || ch->n > 20000u) return 0;
    for (uint32_t i = 0; i < ch->n; ++i) {
        const uint64_t r = ch->rows + (uint64_t)i * SCN_CHR_STRIDE; uint32_t s = 0, nid = 0;
        if (!ScnRd32(rd, c, r + 0x64, &s)) return 0;
        if (s == spawn && ScnRd32(rd, c, r + 0xA0, &nid) && nid && (nid >> 22) == 1u) return nid;
    }
    return 0;
}
// find the character table's GameData slot with a known spawn id (-1 = not found). Skips the item and soul-core slots.
static inline int ScnFindChr(ScnRead rd, void* c, uint64_t gd, uint32_t spawn) {
    for (uint32_t k = 0; k < SCN_GD_SLOTS; ++k) {
        const uint32_t off = k * 8u; ScnTable t;
        if (off == 0x68u || off == 0x118u || !ScnTableAt(rd, c, gd, off, &t)) continue;
        if (ScnNameInChr(rd, c, &t, spawn)) return (int)off;
    }
    return -1;
}
// game text (UTF-16) -> plain ASCII for the overlay's stroke font: drops ^XX~TAG~ colour / ruby markup, folds Latin accents (o-macron -> o).
// 0 = nothing printable or a letter the font cannot show (e.g. Japanese): the caller keeps 'Core <id>'.
static inline int ScnAscii(const uint16_t* w, int maxw, char* out, int n) {
    int k = 0;
    if (n <= 0) return 0;
    for (int i = 0; i < maxw && w[i]; ++i) {
        uint16_t ch = w[i];
        if (ch == '^' && i + 3 < maxw && w[i + 3] == '~') {  // ^XX~TAG~
            int j = i + 4; while (j < maxw && w[j] && w[j] != '~') ++j;
            if (j < maxw && w[j] == '~') { i = j; continue; }
        }
        if (ch == '\n' || ch == '\r' || ch == '\t') ch = ' ';
        char a = 0;
        if (ch >= 0x20 && ch < 0x7F) a = (char)ch;
        else if (ch >= 0xC0 && ch <= 0x17F) {  // Latin-1 / Latin Extended-A letters -> base letter
            static const char* const kL1 = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";
            static const char* const kExtA = "AaAaAaCcCcCcCcDdDdEeEeEeEeEeGgGgGgGgHhHhIiIiIiIiIiIiJjKkkLlLlLlLlLlNnNnNnnNnOoOoOoOoRrRrRrSsSsSsSsTtTtTtUuUuUuUuUuUuWwYyYZzZzZzs";
            a = ch < 0x100 ? kL1[ch - 0xC0] : kExtA[ch - 0x100];
            if (a == 'x' || a == '/') return 0;
        } else if (ch == 0x2019 || ch == 0x2018) a = '\'';
        else if (ch == 0x2013 || ch == 0x2014) a = '-';
        else return 0;
        if (a == ' ' && (k == 0 || out[k - 1] == ' ')) continue;
        if (k + 1 >= n) break;
        out[k++] = a;
    }
    while (k > 0 && out[k - 1] == ' ') --k;
    out[k] = 0;
    return k;
}
#endif
