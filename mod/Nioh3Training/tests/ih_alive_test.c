// Replays the pc2 log case (xworldprobe-20261017-033135): Yoritomo R7B35, HP 51712/51712, IDLEHOLD stopped "boss defeated".
#include <stdint.h>
#include <stdio.h>
#undef NDEBUG
#include <assert.h>
// Same semantics as training.c ActorDeadNow: nonzero return = dead (hp+0x48 == 1, or max && cur == 0); 0 on unreadable.
static uint64_t g_hp_block_ok, g_cur, g_max; static int g_flag48;
static int ActorDeadNow(uint64_t a, int* dead, uint64_t* cur, uint64_t* mx) {
    (void)a; *dead = 0; *cur = 1; *mx = 0;
    if (g_hp_block_ok) { *dead = g_flag48 == 1; *cur = g_cur; *mx = g_max; }
    return *dead || (*mx && *cur == 0);
}
#include "../src/ih_alive.h"
int main(void) {
    g_hp_block_ok = 1; g_cur = g_max = 51712; g_flag48 = 0;
    assert(IhAlive(0x1C86D1C4140ULL));            // the logged live boss: must be alive
    g_cur = 25000; assert(IhAlive(1));
    g_cur = 1; assert(!IhAlive(1));               // finisher window HP <= 1
    g_cur = 0; assert(!IhAlive(1));
    g_cur = g_max; g_flag48 = 1; assert(!IhAlive(1)); // death applied
    g_flag48 = 0; g_hp_block_ok = 0; assert(!IhAlive(1)); // unreadable fails closed
    puts("ih_alive_test OK"); return 0;
}
