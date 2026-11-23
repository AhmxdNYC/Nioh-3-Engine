// human #129277: pure logic of the mod menu Equip core row (src/soul_core_swap.h).
#include <stdio.h>
#include "../src/soul_core_swap.h"
static int g_fail;
static void expect(int cond, const char* msg) {
    if (cond) return;
    fprintf(stderr, "FAIL %s\n", msg);
    g_fail = 1;
}
int main(void) {
    // item master row: soul core = u32 +0x64 == 900000000 and byte +0x182 == 9, id u16 +0x152
    uint8_t row[SCW_ROW_STRIDE] = {0};
    uint32_t cat = 900000000u; memcpy(row + 0x64, &cat, 4); row[0x182] = 9; row[0x152] = 0x34; row[0x153] = 0x12;
    expect(ScwIsCoreRow(row) && ScwRowId(row) == 0x1234, "core row detected with its id");
    row[0x182] = 8; expect(!ScwIsCoreRow(row), "wrong type byte is not a core");
    row[0x182] = 9; cat = 900000001u; memcpy(row + 0x64, &cat, 4); expect(!ScwIsCoreRow(row), "wrong category is not a core");

    uint16_t ids[4]; int n = 0;
    n = ScwAddId(ids, n, 4, 30); n = ScwAddId(ids, n, 4, 10); n = ScwAddId(ids, n, 4, 20); n = ScwAddId(ids, n, 4, 10);
    expect(n == 3 && ids[0] == 10 && ids[1] == 20 && ids[2] == 30, "sorted, no duplicates");
    n = ScwAddId(ids, n, 4, 5); n = ScwAddId(ids, n, 4, 40);
    expect(n == 4 && ids[0] == 5 && ids[3] == 30, "cap holds: extra id dropped");
    expect(ScwHasId(ids, n, 20) && !ScwHasId(ids, n, 21) && !ScwHasId(ids, n, 40), "lookup");

    uint32_t slots[SCW_NSLOT] = {3, 7, SCW_EMPTY, SCW_EMPTY, SCW_EMPTY, SCW_EMPTY, SCW_EMPTY, SCW_EMPTY};
    uint32_t owned[4] = {3, 5, 7, 9};
    expect(ScwCheck(0, 5, slots, owned, 4) == 0, "owned core into Yang 1 ok");
    expect(ScwCheck(0, 3, slots, owned, 4) == SCW_SAME, "same core refused before the game");
    expect(ScwCheck(0, 4, slots, owned, 4) == SCW_NOT_CORE, "not an owned core refused");
    expect(ScwCheck(8, 5, slots, owned, 4) == SCW_BAD_SLOT && ScwCheck(-1, 5, slots, owned, 4) == SCW_BAD_SLOT, "slot range");
    expect(ScwCheck(1, 3, slots, owned, 4) == 0, "core from the other slot may be picked (game check decides)");

    // picker skips the core already in the slot and wraps both ways
    expect(ScwStep(SCW_EMPTY, 1, 3, owned, 4) == 5, "Right from nothing = first other core");
    expect(ScwStep(SCW_EMPTY, -1, 3, owned, 4) == 9, "Left from nothing = last core");
    expect(ScwStep(5, 1, 3, owned, 4) == 7 && ScwStep(9, 1, 3, owned, 4) == 5, "Right wraps past the slot core");
    expect(ScwStep(5, -1, 3, owned, 4) == 9, "Left wraps past the slot core");
    uint32_t one[1] = {3};
    expect(ScwStep(SCW_EMPTY, 1, 3, one, 1) == SCW_EMPTY, "no other core = nothing to pick");
    expect(ScwStep(SCW_EMPTY, 1, SCW_EMPTY, one, 1) == 3, "empty slot: any owned core");

    char b[16];
    expect(!strcmp(ScwSlotName(0, b, sizeof b), "Yang 1") && !strcmp(ScwSlotName(1, b, sizeof b), "Yang 2") && !strcmp(ScwSlotName(2, b, sizeof b), "Yin 1"), "slot names");
    if (!g_fail) printf("soul_core_swap_test PASS\n");
    return g_fail;
}
