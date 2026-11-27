// TASK #137305 (human #137303): soul core -> yokai name chain (src/soul_core_name.h).
// Part 1: synthetic tables (discovery skips decoys, misses stay 0). Part 2, when the extracted game files exist (argv[1] = repo root):
// a fake GameData whose item / soul-core / character slots point at the real param files, then every soul core item is walked and
// printed as 'core<TAB>spawn<TAB>name_id' for soul_core_name_test.py to join with the text dump.
#include <stdio.h>
#include <stdlib.h>
#include "../src/soul_core_name.h"
static int g_fail;
static void expect(int cond, const char* msg) { if (!cond) { fprintf(stderr, "FAIL %s\n", msg); g_fail = 1; } }
// fake address space: only registered blocks are readable (the game's SEH-guarded reads fail the same way past a table's end)
static struct { uintptr_t a; size_t n; } g_reg[64]; static int g_nreg;
static void* Reg(void* p, size_t n) { g_reg[g_nreg].a = (uintptr_t)p; g_reg[g_nreg].n = n; ++g_nreg; return p; }
static int MemRead(void* c, uint64_t a, void* out, unsigned n) {
    (void)c;
    for (int i = 0; i < g_nreg; ++i) if (a >= g_reg[i].a && a + n <= g_reg[i].a + g_reg[i].n) { memcpy(out, (const void*)(uintptr_t)a, n); return 1; }
    return 0;
}
static void* Wrap(void* hdr) { void** w = (void**)Reg(malloc(8), 8); *w = hdr; return w; }
static uint8_t* Table(uint32_t n, uint32_t stride) { const size_t z = 8 + (size_t)n * stride; uint8_t* h = (uint8_t*)Reg(calloc(1, z), z); uint32_t m = SCN_MAGIC; memcpy(h, &m, 4); memcpy(h + 4, &n, 4); return h; }
static void W16(uint8_t* p, uint16_t v) { memcpy(p, &v, 2); }
static void W32(uint8_t* p, uint32_t v) { memcpy(p, &v, 4); }
static uint8_t* LoadFile(const char* p, long* len) {
    FILE* f = fopen(p, "rb"); if (!f) return 0;
    fseek(f, 0, SEEK_END); *len = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t* d = (uint8_t*)malloc((size_t)*len); if (fread(d, 1, (size_t)*len, f) != (size_t)*len) { fclose(f); free(d); return 0; }
    fclose(f); return d;
}
int main(int argc, char** argv) {
    // --- ASCII cleanup
    char o[48];
    const uint16_t a1[] = {'^', '0', '1', '~', 'C', 'Y', 'A', 'N', '~', 'Y', 'a', 's', 'h', 'a', '^', '0', '1', '~', 'W', 'H', 'I', 'T', 'E', '~', 0};
    expect(ScnAscii(a1, 64, o, sizeof o) == 5 && !strcmp(o, "Yasha"), "colour markup dropped");
    const uint16_t a2[] = {'O', 0x14D, 'g', 'a', 'm', 'a', ' ', 'S', 0x16B, ' ', ' ', 0};
    expect(ScnAscii(a2, 64, o, sizeof o) && !strcmp(o, "Oogama Su"), "macrons fold, spaces trimmed");
    const uint16_t a3[] = {0x9B3C, 0x7AE5, 0};
    expect(ScnAscii(a3, 64, o, sizeof o) == 0, "Japanese -> 0 (caller keeps Core <id>)");
    const uint16_t a4[] = {'A', 'B', 'C', 'D', 'E', 'F', 0};
    expect(ScnAscii(a4, 64, o, 4) == 3 && !strcmp(o, "ABC"), "cap holds");
    expect(ScnAscii(a4, 64, o, 0) == 0, "cap 0");

    // --- synthetic chain: core 0x1234 -> key 0x77 -> yang seal 0x4321 -> spawn 0xABCDE -> name 0x400123 (group 1)
    uint8_t* it = Table(3, SCN_ITEM_STRIDE);
    uint8_t* r0 = it + 8; W16(r0 + 0x152, 0x1234); W32(r0 + 0x64, 900000000u); W16(r0 + 0x180, 0x77);
    uint8_t* r1 = it + 8 + SCN_ITEM_STRIDE; W16(r1 + 0x152, 0x4321); W32(r1 + 0x64, 700000000u); W32(r1 + 0x54, 0xABCDE);
    uint8_t* r2 = it + 8 + 2 * SCN_ITEM_STRIDE; W16(r2 + 0x152, 0x5555); W32(r2 + 0x64, 900000000u); W16(r2 + 0x180, 0x99);  // key not in the sc table
    uint8_t* sc = Table(2, SCN_SC_STRIDE); W32(sc + 8 + 0x0C, 0x10); W32(sc + 8 + SCN_SC_STRIDE + 0x0C, 0x77); W32(sc + 8 + SCN_SC_STRIDE + 0x18, 0x4321);
    uint8_t* decoy = Table(2, SCN_CHR_STRIDE); W32(decoy + 8 + 0x64, 0xABCDE); W32(decoy + 8 + 0xA0, 0x1C00005);  // right spawn, group 7 text: not a name
    uint8_t* ch = Table(3, SCN_CHR_STRIDE); W32(ch + 8 + SCN_CHR_STRIDE + 0x64, 0xABCDE); W32(ch + 8 + SCN_CHR_STRIDE + 0xA0, 0x400123);
    uint64_t* gd = (uint64_t*)Reg(calloc(SCN_GD_SLOTS, 8), SCN_GD_SLOTS * 8);
    gd[0x68 / 8] = (uint64_t)(uintptr_t)Wrap(it); gd[0x118 / 8] = (uint64_t)(uintptr_t)Wrap(sc);
    gd[0x20 / 8] = (uint64_t)(uintptr_t)Wrap(decoy); gd[0x30 / 8] = 0x1234;  /* junk pointer below the floor */ gd[0x2A8 / 8] = (uint64_t)(uintptr_t)Wrap(ch);
    const uint64_t g = (uint64_t)(uintptr_t)gd; ScnTable ti, ts, tc;
    expect(ScnTableAt(MemRead, 0, g, 0x68, &ti) && ti.n == 3 && ScnTableAt(MemRead, 0, g, 0x118, &ts) && ts.n == 2, "tables read");
    expect(!ScnTableAt(MemRead, 0, g, 0x30, &tc) && !ScnTableAt(MemRead, 0, g, 0x40, &tc), "junk / empty slots refused");
    const uint32_t sp = ScnSpawnId(MemRead, 0, &ti, &ts, 0x1234);
    expect(sp == 0xABCDE, "core -> seal spawn id");
    expect(ScnSpawnId(MemRead, 0, &ti, &ts, 0x5555) == 0 && ScnSpawnId(MemRead, 0, &ti, &ts, 0x4321) == 0 && ScnSpawnId(MemRead, 0, &ti, &ts, 0x9999) == 0,
           "unmapped core, seal item (not a core), unknown id -> 0");
    const int off = ScnFindChr(MemRead, 0, g, sp);
    expect(off == 0x2A8, "character table found past the decoy");
    expect(ScnTableAt(MemRead, 0, g, (uint32_t)off, &tc) && ScnNameInChr(MemRead, 0, &tc, sp) == 0x400123, "name id");
    expect(ScnNameInChr(MemRead, 0, &tc, 0x12345) == 0 && ScnFindChr(MemRead, 0, g, 0x12345) == -1, "unknown spawn -> none");

    // --- the real game tables
    if (argc > 1) {
        char p[3][1024]; long len[3]; uint8_t* d[3];
        const char* f[3] = {"/data/extract/lfm/archive_03/2E/D4E1DB3B337317531B772F2D5F1017CA47C8D556D94855532421522EE24A79",
                            "/data/extract/lfm/archive_03/23/DAC95BA0146EC13EC2F5D25530800A24F092D403B890FE53419274CEACCBAF",
                            "/data/extract/lfm/archive_03/C4/E1CBAE0841F7ECE87BD581BA0998F242FE69C4449DB57A473D072E0EB85FAC"};
        int have = 1;
        for (int i = 0; i < 3; ++i) { snprintf(p[i], sizeof p[i], "%s%s", argv[1], f[i]); d[i] = LoadFile(p[i], &len[i]); if (!d[i]) have = 0; }
        if (!have) { printf("SKIP real tables (game files not extracted here)\n"); }
        else {
            for (int i = 0; i < 3; ++i) Reg(d[i], (size_t)len[i]);
            uint64_t* rg = (uint64_t*)Reg(calloc(SCN_GD_SLOTS, 8), SCN_GD_SLOTS * 8);
            rg[0x68 / 8] = (uint64_t)(uintptr_t)Wrap(d[0]); rg[0x118 / 8] = (uint64_t)(uintptr_t)Wrap(d[1]); rg[0x150 / 8] = (uint64_t)(uintptr_t)Wrap(d[2]);
            // a decoy with the item table's magic in another slot: discovery must still land on 0x150
            rg[0x10 / 8] = (uint64_t)(uintptr_t)Wrap(d[0]);
            const uint64_t G = (uint64_t)(uintptr_t)rg; ScnTable I, S, C;
            expect(ScnTableAt(MemRead, 0, G, 0x68, &I) && ScnTableAt(MemRead, 0, G, 0x118, &S), "real tables");
            int cores = 0, named = 0, chr = -2;
            for (uint32_t i = 0; i < I.n; ++i) {
                const uint8_t* r = (const uint8_t*)(uintptr_t)(I.rows + (uint64_t)i * SCN_ITEM_STRIDE); uint32_t cat; uint16_t id;
                memcpy(&cat, r + 0x64, 4); memcpy(&id, r + 0x152, 2);
                if (cat != 900000000u || r[0x182] != 9) continue;
                ++cores;
                const uint32_t s = ScnSpawnId(MemRead, 0, &I, &S, id); uint32_t nid = 0;
                if (s && chr == -2) chr = ScnFindChr(MemRead, 0, G, s);
                if (s && chr >= 0 && ScnTableAt(MemRead, 0, G, (uint32_t)chr, &C)) nid = ScnNameInChr(MemRead, 0, &C, s);
                if (nid) ++named;
                printf("CORE\t%X\t%X\t%u\n", id, s, nid);
            }
            expect(chr == 0x150, "real character table found");
            printf("real: %d soul core items, %d with a name id\n", cores, named);
            expect(cores >= 100 && named * 10 >= cores * 9, "at least 90% of real cores get a name id");
        }
    }
    puts(g_fail ? "soul_core_name_test FAIL" : "soul_core_name_test PASS");
    return g_fail;
}
