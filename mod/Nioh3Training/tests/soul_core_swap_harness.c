// human #129277: soul_core_swap_game.h against a fake game: call order (= swapcore.py / shrine 1DECB78), arguments, refusal, the
// shortcut skip when the old core had none, and that nothing runs without a menu request.
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uintptr_t B = 0;
static int g_log_n;
static void Log(const char* f, ...) { va_list a; va_start(a, f); vprintf(f, a); va_end(a); printf("\n"); ++g_log_n; }
static __attribute__((noinline)) int Rd64(uintptr_t a, uint64_t* out) { __try { *out = *(volatile uint64_t*)a; return 1; } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; } }
#define N3T_API static

// fake game memory
static uint8_t* g_inv;            // inventory object
static uint64_t g_invp;           // [ctx] = inv
static uint64_t g_ctxslot;        // global 4751530 -> ctx (= &g_invp)
static uint8_t g_hud;
static uint8_t* g_tab; static uint64_t g_db68[1]; static uint8_t g_db[0x200]; static uint64_t g_dbslot;
static char g_calls[256]; static int g_ok = 1; static int32_t g_sc = 2;
static uint64_t a_ctx, a_rec; static int a_slot, a_scarg;
static int g_add_keeps = 1;
static void C(char c) { size_t n = strlen(g_calls); g_calls[n] = c; g_calls[n + 1] = 0; }
static void f_build(void* desc, uint32_t id, uint32_t n) { C('B'); (void)n; memset(desc, 0, 8); memcpy(desc, &id, 2); }
static void f_add(uint64_t ctx, void* out, void* desc, uint64_t a, uint64_t b) {
    C('A'); (void)out; (void)a; (void)b;
    if (ctx != a_ctx) C('!');
    if (!g_add_keeps) return;
    uint64_t cnt = *(uint64_t*)(g_inv + 0x927D0);
    memcpy(g_inv + 0x10 + cnt * 0xF0, desc, 2);
    *(uint64_t*)(g_inv + 0x927D0) = cnt + 1;
}
static uint8_t f_ok(uint8_t s, uint64_t rec) { C('1'); a_slot = s; a_rec = rec; return (uint8_t)g_ok; }
static int32_t f_sc(uint64_t ctx, uint8_t s) { C('2'); if (ctx != a_ctx || s != a_slot) C('!'); return g_sc; }
// TASK #141562: the live game's usable-core list (notes/soulcore-form-source-128972.md sec 5, 8260b39d live run8). 21607A4 unequips rec
// from any other slot; 2183744 writes the slot and 54B734 rebuilds inv+0x1E0AD0 (0xE8 rows: u16 use id, u16 base id, u32 count): one row
// per equipped core that makes a usable item. Most cores make one in every slot; a core like live CE1B/9F09 makes none in the d-pad slots
// 0-1 and x3 in slots 2-3 (g_nodpad_id).
static uint16_t g_nodpad_id;
static uint16_t use_of(uint16_t core) { return (uint16_t)(core ^ 0x7000); }
static void rebuild_usable(void) {
    uint8_t* u = g_inv + 0x1E0AD0; memset(u, 0, (size_t)1200 * 0xE8); int n = 0;
    for (int s = 0; s < 8; ++s) {
        const uint32_t i = *(uint32_t*)(g_inv + 0x23B6B8 + 4 * s); if (i == 0xFFFFFFFFu) continue;
        uint16_t core = 0; memcpy(&core, g_inv + 0x10 + (size_t)i * 0xF0, 2);
        if (core == g_nodpad_id && s <= 1) continue;
        const uint16_t id = use_of(core); const uint32_t c = 3;
        memcpy(u + n * 0xE8, &id, 2); memcpy(u + n * 0xE8 + 2, &id, 2); memcpy(u + n * 0xE8 + 4, &c, 4); ++n;
    }
}
static void f_3(uint64_t ctx, uint64_t rec) {
    C('3'); if (ctx != a_ctx || rec != a_rec) C('!');
    const uint32_t i = (uint32_t)((rec - (uint64_t)(g_inv + 0x10)) / 0xF0);
    for (int s = 0; s < 8; ++s) if (*(uint32_t*)(g_inv + 0x23B6B8 + 4 * s) == i) *(uint32_t*)(g_inv + 0x23B6B8 + 4 * s) = 0xFFFFFFFFu;
}
static void f_4(uint64_t ctx, uint8_t s, uint64_t rec) { C('4'); if (ctx != a_ctx || s != a_slot || rec != a_rec) C('!'); *(uint32_t*)(g_inv + 0x23B6B8 + 4 * s) = (uint32_t)((rec - (uint64_t)(g_inv + 0x10)) / 0xF0); rebuild_usable(); }
static void f_5(uint64_t ctx) { C('5'); if (ctx != a_ctx) C('!'); }
static void f_6(uint64_t ctx, int32_t sc, uint8_t s) { C('6'); a_scarg = sc; if (ctx != a_ctx || s != a_slot) C('!'); }
static void f_7(uint32_t h, uint64_t z) { C('7'); if (h != 0xF53E7B0Cu || z) C('!'); }
static uintptr_t Grva(uint32_t rva) {
    switch (rva) {
    case 0x4751530: return (uintptr_t)&g_ctxslot;
    case 0x45B9E30: return (uintptr_t)&g_dbslot;
    case 0x45C6F2F: return (uintptr_t)&g_hud;
    case 0x2285C48: return (uintptr_t)f_ok;
    case 0x216B9D0: return (uintptr_t)f_sc;
    case 0x21607A4: return (uintptr_t)f_3;
    case 0x2183744: return (uintptr_t)f_4;
    case 0x215F85C: return (uintptr_t)f_5;
    case 0x2182968: return (uintptr_t)f_6;
    case 0x34EDF8: return (uintptr_t)f_7;
    case 0x54CCD0: return (uintptr_t)f_build;
    case 0x54D324: return (uintptr_t)f_add;
    }
    return 0;
}
#include "../src/soul_core_swap_game.h"
static int g_fail;
static void expect(int c, const char* m) { if (!c) { fprintf(stderr, "FAIL %s\n", m); g_fail = 1; } }
static void put_row(int i, uint16_t id, int core) {
    uint8_t* r = g_tab + 8 + (size_t)i * 0x1A0;
    if (core) { uint32_t c = 900000000u; memcpy(r + 0x64, &c, 4); r[0x182] = 9; uint16_t key = (uint16_t)(0x10 + (id & 0xF)); memcpy(r + 0x180, &key, 2); }
    memcpy(r + 0x152, &id, 2);
}
// TASK #141276: Summoning Seal row (+0x64 700000000, +0x182 7, spawn character +0x54) and the soul-core table [db+0x118] (stride 0x54)
static void put_seal(int i, uint16_t id, uint32_t chr) {
    uint8_t* r = g_tab + 8 + (size_t)i * 0x1A0; uint32_t c = 700000000u;
    memcpy(r + 0x64, &c, 4); r[0x182] = 7; memcpy(r + 0x54, &chr, 4); memcpy(r + 0x152, &id, 2);
}
static uint8_t* g_sct; static uint64_t g_db118[1];
static void put_sc(int i, uint32_t key, uint32_t yang) { uint8_t* r = g_sct + 8 + (size_t)i * 0x54; memcpy(r + 0x0C, &key, 4); memcpy(r + 0x18, &yang, 4); }
static void tick_until_ids(void) { for (int i = 0; i < 3 && !g_scw_ids_done; ++i) { g_scw_ids_t = 0; ScwTick(); } }
int main(void) {
    g_inv = (uint8_t*)calloc(1, 0x23B6B8 + 64);
    g_invp = (uint64_t)g_inv; g_ctxslot = (uint64_t)&g_invp; a_ctx = g_ctxslot;
    *(uint64_t*)(g_inv + 0x927D0) = 5;
    uint16_t items[5] = {0x100, 0x2A1, 0x2A2, 0x100, 0x2A3};
    for (int i = 0; i < 5; ++i) memcpy(g_inv + 0x10 + i * 0xF0, &items[i], 2);
    for (int s = 0; s < 8; ++s) *(uint32_t*)(g_inv + 0x23B6B8 + 4 * s) = 0xFFFFFFFFu;
    *(uint32_t*)(g_inv + 0x23B6B8) = 1;  // Yang 1 = idx 1 (core 2A1)
    rebuild_usable();
    g_tab = (uint8_t*)calloc(1, 8 + 12 * 0x1A0); *(uint32_t*)(g_tab + 4) = 12;
    put_row(0, 0x100, 0); put_row(1, 0x2A1, 1); put_row(2, 0x2A2, 1); put_row(3, 0x2A3, 1); put_row(4, 0x2A4, 1); put_row(5, 0x2A5, 1);
    put_row(6, 0x2A6, 1); put_row(7, 0x2A7, 1);
    put_seal(8, 0x5A1, 0x111); put_seal(9, 0x5A2, 0x222); put_seal(10, 0x5A3, 0x333); put_seal(11, 0x5A4, 0x444);
    g_sct = (uint8_t*)calloc(1, 8 + 8 * 0x54); *(uint32_t*)(g_sct + 4) = 8;
    // keys 11..14 -> seals 5A1..5A4 (soul cores); 15 -> 0x100 (not a seal: an item, like live 64CF / 6DB5); 16 -> no seal row; 17 twice (ambiguous)
    put_sc(0, 0x11, 0x5A1); put_sc(1, 0x12, 0x5A2); put_sc(2, 0x13, 0x5A3); put_sc(3, 0x14, 0x5A4); put_sc(4, 0x15, 0x100); put_sc(5, 0x16, 0x5A9);
    put_sc(6, 0x17, 0x5A1); put_sc(7, 0x17, 0x5A2);
    g_db68[0] = (uint64_t)g_tab; *(uint64_t*)(g_db + 0x68) = (uint64_t)g_db68; g_dbslot = (uint64_t)g_db;
    g_db118[0] = (uint64_t)g_sct; *(uint64_t*)(g_db + 0x118) = (uint64_t)g_db118;

    // nothing runs without a request; the item master is not read until the menu asks
    ScwTick(); ScwTick();
    expect(!g_calls[0] && !g_scw_ids_done, "idle: no game call, no item master read");
    uint32_t sl[8], idx[8]; uint16_t it[8];
    expect(Nioh3Training_SoulCoreState(sl, idx, it, 8) == -1, "state before the ids are built = -1");
    expect(Nioh3Training_SoulCoreCatalog(0, 0, 0) == -1, "catalog before the ids are built = -1");
    tick_until_ids();
    g_scw_snap.t = 0;
    int n = Nioh3Training_SoulCoreState(sl, idx, it, 8);
    expect(n == 3 && idx[0] == 1 && idx[1] == 2 && idx[2] == 4 && it[2] == 0x2A3 && sl[0] == 1 && sl[1] == 0xFFFFFFFFu, "state: 3 owned cores, Yang 1 = idx 1");

    // equip idx 4 into Yang 1: full shrine order, shortcut moved
    expect(Nioh3Training_SoulCoreEquip(0, 4) == 1, "queued");
    expect(Nioh3Training_SoulCoreEquip(0, 2) == 0, "second request while one waits = busy");
    expect(!g_calls[0], "not run off the game thread");
    ScwTick();
    int seq = 0;
    expect(!strcmp(g_calls, "1234567") && g_hud == 1 && a_scarg == 2 && a_slot == 0, "order 1..7 + HUD byte, shortcut 2 passed on");
    expect(a_rec == (uint64_t)(g_inv + 0x10 + 4 * 0xF0), "record of idx 4");
    expect(Nioh3Training_SoulCoreEquipResult(&seq) == SCW_DONE && seq == 1, "result done, seq 1");
    expect(*(uint32_t*)(g_inv + 0x23B6B8) == 4, "slot now idx 4");

    // same core again: refused before any game call
    g_calls[0] = 0; Nioh3Training_SoulCoreEquip(0, 4); ScwTick();
    expect(!g_calls[0] && Nioh3Training_SoulCoreEquipResult(&seq) == SCW_SAME && seq == 2, "same core: no call");
    // not a core (idx 0 = item 0x100)
    Nioh3Training_SoulCoreEquip(0, 0); ScwTick();
    expect(!g_calls[0] && Nioh3Training_SoulCoreEquipResult(0) == SCW_NOT_CORE, "non-core item: no call");
    // the game's own check refuses: stops after step 1
    g_ok = 0; Nioh3Training_SoulCoreEquip(1, 2); ScwTick();
    expect(!strcmp(g_calls, "1") && Nioh3Training_SoulCoreEquipResult(0) == SCW_REFUSED, "game refuses: only the check ran");
    // old core without a shortcut: step 6 skipped
    g_ok = 1; g_sc = -1; g_calls[0] = 0; a_scarg = 99; Nioh3Training_SoulCoreEquip(1, 2); ScwTick();
    expect(!strcmp(g_calls, "123457") && a_scarg == 99 && Nioh3Training_SoulCoreEquipResult(0) == SCW_DONE, "no shortcut: 6 skipped");
    // no save: ctx null
    g_ctxslot = 0; g_calls[0] = 0; Nioh3Training_SoulCoreEquip(2, 1); ScwTick();
    expect(!g_calls[0] && Nioh3Training_SoulCoreEquipResult(0) == SCW_NO_GAME, "no save loaded: no call");

    // TASK #136129: every item-master core, owned or not, then the same shrine order
    g_ctxslot = (uint64_t)&g_invp; g_calls[0] = 0; g_ok = 1; g_sc = 2; g_hud = 0;
    uint16_t cids[8], slotItem[8];
    int cn = Nioh3Training_SoulCoreCatalog(cids, 8, slotItem);
    expect(cn == 4 && cids[0] == 0x2A1 && cids[3] == 0x2A4 && slotItem[0] == 0x2A3, "catalog is every yokai-linked core; Yang 1 is item 2A3");
    // TASK #141276: core-category ids the game links to no yokai (seal is an item / missing) are not in the catalog
    { int has = 0; for (int i = 0; i < cn; ++i) if (cids[i] == 0x2A5 || cids[i] == 0x2A6 || cids[i] == 0x2A7) has = 1; expect(!has, "2A5 (item seal), 2A6 (no seal row), 2A7 (two key rows) left out"); }
    g_calls[0] = 0; Nioh3Training_SoulCoreEquipItem(0, 0x2A3); ScwTick();
    expect(!g_calls[0] && Nioh3Training_SoulCoreEquipResult(0) == SCW_SAME, "owned same item: no add, no shrine");
    g_calls[0] = 0; Nioh3Training_SoulCoreEquipItem(0, 0x100); ScwTick();
    expect(!g_calls[0] && Nioh3Training_SoulCoreEquipResult(0) == SCW_NOT_CORE, "item master non-core: no add");
    g_add_keeps = 0; g_calls[0] = 0; Nioh3Training_SoulCoreEquipItem(0, 0x2A4); ScwTick();
    expect(!strcmp(g_calls, "BA") && Nioh3Training_SoulCoreEquipResult(0) == SCW_NO_ADD, "add that does not insert: no shrine");
    g_add_keeps = 1; g_calls[0] = 0;
    expect(Nioh3Training_SoulCoreEquipItem(0, 0x2A4) == 1, "unowned queued");
    expect(Nioh3Training_SoulCoreEquipItem(0, 0x2A5) == 0, "second item request busy");
    ScwTick();
    expect(!strcmp(g_calls, "BA1234567") && Nioh3Training_SoulCoreEquipResult(0) == SCW_DONE, "unowned core: add then the same shrine order");
    expect(*(uint32_t*)(g_inv + 0x23B6B8) == 5, "slot now the new idx 5");
    uint16_t got = 0; memcpy(&got, g_inv + 0x10 + 5 * 0xF0, 2);
    expect(got == 0x2A4, "new record is item 2A4");
    g_calls[0] = 0; Nioh3Training_SoulCoreEquipItem(1, 0x2A1); ScwTick();
    expect(!strcmp(g_calls, "1234567") && Nioh3Training_SoulCoreEquipResult(0) == SCW_DONE, "owned core: shrine only, no add");
    // TASK #140431 / #141562: a core with no usable item in a d-pad slot (live CE1B) goes into slot 2, the d-pad shortcut moves onto it
    g_nodpad_id = 0x2A2; g_sc = 2; g_calls[0] = 0; a_scarg = 99;
    expect(*(uint32_t*)(g_inv + 0x23B6B8 + 4 * 2) == 0xFFFFFFFFu, "slot 2 empty before");
    Nioh3Training_SoulCoreEquip(0, 2); ScwTick();
    expect(!strcmp(g_calls, "1234567134567") && Nioh3Training_SoulCoreEquipResult(0) == SCW_DONE, "no d-pad item: d-pad equip, then slot 2 + shortcut");
    expect(a_slot == 2 && a_scarg == 2, "slot 2 equip got the d-pad shortcut 2");
    expect(*(uint32_t*)(g_inv + 0x23B6B8 + 4 * 2) == 2 && *(uint32_t*)(g_inv + 0x23B6B8) == 0xFFFFFFFFu, "core now in slot 2, gone from slot 0");
    { int f = 0; for (int i = 0; i < 8; ++i) { uint16_t u0 = 0; memcpy(&u0, g_inv + 0x1E0AD0 + i * 0xE8, 2); if (u0 == use_of(0x2A2)) f = 1; } expect(f, "its usable item is in the list"); }
    // a usable core in the d-pad slot makes its own usable item: no fallback
    g_nodpad_id = 0; g_calls[0] = 0; Nioh3Training_SoulCoreEquip(0, 4); ScwTick();
    expect(!strcmp(g_calls, "1234567") && a_slot == 0 && Nioh3Training_SoulCoreEquipResult(0) == SCW_DONE, "usable core: d-pad equip only");
    if (!g_fail) printf("soul_core_swap_harness PASS\n");
    return g_fail;
}
