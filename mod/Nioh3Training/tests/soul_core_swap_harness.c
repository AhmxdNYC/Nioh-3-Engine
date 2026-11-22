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
static uint8_t* g_tab; static uint64_t g_db68[1]; static uint8_t g_db[0x100]; static uint64_t g_dbslot;
static char g_calls[256]; static int g_ok = 1; static int32_t g_sc = 2;
static uint64_t a_ctx, a_rec; static int a_slot, a_scarg;
static void C(char c) { size_t n = strlen(g_calls); g_calls[n] = c; g_calls[n + 1] = 0; }
static uint8_t f_ok(uint8_t s, uint64_t rec) { C('1'); a_slot = s; a_rec = rec; return (uint8_t)g_ok; }
static int32_t f_sc(uint64_t ctx, uint8_t s) { C('2'); if (ctx != a_ctx || s != a_slot) C('!'); return g_sc; }
static void f_3(uint64_t ctx, uint64_t rec) { C('3'); if (ctx != a_ctx || rec != a_rec) C('!'); }
static void f_4(uint64_t ctx, uint8_t s, uint64_t rec) { C('4'); if (ctx != a_ctx || s != a_slot || rec != a_rec) C('!'); *(uint32_t*)(g_inv + 0x23B6B8 + 4 * s) = (uint32_t)((rec - (uint64_t)(g_inv + 0x10)) / 0xF0); }
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
    }
    return 0;
}
#include "../src/soul_core_swap_game.h"
static int g_fail;
static void expect(int c, const char* m) { if (!c) { fprintf(stderr, "FAIL %s\n", m); g_fail = 1; } }
static void put_row(int i, uint16_t id, int core) {
    uint8_t* r = g_tab + 8 + (size_t)i * 0x1A0;
    if (core) { uint32_t c = 900000000u; memcpy(r + 0x64, &c, 4); r[0x182] = 9; }
    memcpy(r + 0x152, &id, 2);
}
static void tick_until_ids(void) { for (int i = 0; i < 3 && !g_scw_ids_done; ++i) { g_scw_ids_t = 0; ScwTick(); } }
int main(void) {
    g_inv = (uint8_t*)calloc(1, 0x23B6B8 + 64);
    g_invp = (uint64_t)g_inv; g_ctxslot = (uint64_t)&g_invp; a_ctx = g_ctxslot;
    *(uint64_t*)(g_inv + 0x927D0) = 5;
    uint16_t items[5] = {0x100, 0x2A1, 0x2A2, 0x100, 0x2A3};
    for (int i = 0; i < 5; ++i) memcpy(g_inv + 0x10 + i * 0xF0, &items[i], 2);
    for (int s = 0; s < 8; ++s) *(uint32_t*)(g_inv + 0x23B6B8 + 4 * s) = 0xFFFFFFFFu;
    *(uint32_t*)(g_inv + 0x23B6B8) = 1;  // Yang 1 = idx 1 (core 2A1)
    g_tab = (uint8_t*)calloc(1, 8 + 4 * 0x1A0); *(uint32_t*)(g_tab + 4) = 4;
    put_row(0, 0x100, 0); put_row(1, 0x2A1, 1); put_row(2, 0x2A2, 1); put_row(3, 0x2A3, 1);
    g_db68[0] = (uint64_t)g_tab; *(uint64_t*)(g_db + 0x68) = (uint64_t)g_db68; g_dbslot = (uint64_t)g_db;

    // nothing runs without a request; the item master is not read until the menu asks
    ScwTick(); ScwTick();
    expect(!g_calls[0] && !g_scw_ids_done, "idle: no game call, no item master read");
    uint32_t sl[8], idx[8]; uint16_t it[8];
    expect(Nioh3Training_SoulCoreState(sl, idx, it, 8) == -1, "state before the ids are built = -1");
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
    if (!g_fail) printf("soul_core_swap_harness PASS\n");
    return g_fail;
}
