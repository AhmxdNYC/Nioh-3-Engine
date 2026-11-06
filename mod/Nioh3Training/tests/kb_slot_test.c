#include <stdio.h>
#include "../src/kb_slot.h"
static int g_fail;
static void expect(int cond, const char* msg) {
    if (cond) return;
    fprintf(stderr, "FAIL %s\n", msg);
    g_fail = 1;
}
int main(void) {
    uint64_t addr[4] = {0x1000, 0, 0, 0};
    uint32_t id[4] = {11, 0, 0, 0};
    uint32_t seen[4] = {1000, 0, 0, 0};
    int reset = 0, s;
    s = KbPickSlot(addr, id, seen, 4, 0x1000, 11, 2000, 10000, &reset);
    expect(s == 0 && reset == 0, "same boss keeps the slot");
    s = KbPickSlot(addr, id, seen, 4, 0x1000, 22, 2000, 10000, &reset);
    expect(s == 0 && reset == 1, "new id at the same address resets inside 10 s");
    s = KbPickSlot(addr, id, seen, 4, 0x1000, 22, 1500, 3000, &reset);
    expect(s == 0 && reset == 1, "new id resets inside the 3 s window too");
    s = KbPickSlot(addr, id, seen, 4, 0x1000, 0, 2000, 10000, &reset);
    expect(s == 0 && reset == 0, "unknown id does not drop the slot");
    id[0] = 0;
    s = KbPickSlot(addr, id, seen, 4, 0x1000, 22, 2000, 10000, &reset);
    expect(s == 0 && reset == 0, "unknown stored id is not a change");
    id[0] = 11;
    addr[1] = 0x2000; id[1] = 30; seen[1] = 2000;
    addr[2] = 0x3000; id[2] = 31; seen[2] = 2000;
    addr[3] = 0x4000; id[3] = 32; seen[3] = 2000;
    seen[0] = seen[1] = seen[2] = seen[3] = 2000;
    s = KbPickSlot(addr, id, seen, 4, 0x5000, 40, 2500, 10000, &reset);
    expect(s < 0, "a full fresh table does not steal");
    seen[0] = seen[1] = seen[2] = 11900; seen[3] = 0;
    s = KbPickSlot(addr, id, seen, 4, 0x5000, 40, 12000, 10000, &reset);
    expect(s == 3 && reset == 1, "a 10 s stale slot can be taken");
    addr[1] = 0; id[1] = 0; seen[1] = 0;
    seen[0] = seen[2] = seen[3] = 11900;
    s = KbPickSlot(addr, id, seen, 4, 0x5000, 40, 12000, 10000, &reset);
    expect(s == 1 && reset == 1, "an empty slot is a fresh occupant");
    if (g_fail) return 1;
    puts("PASS kb slot identity");
    return 0;
}
