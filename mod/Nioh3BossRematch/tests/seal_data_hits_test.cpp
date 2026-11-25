// TASK #135148: offline check of seal_data_hits.h (cl /EHsc /std:c++17 /Od tests\seal_data_hits_test.cpp; asserts stay on: no /DNDEBUG).
#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <vector>
#include "../src/seal_data_hits.h"
static void Row(std::vector<uint8_t>& b, uint16_t st, uint16_t en, uint16_t pw, uint16_t el = 0) {
    size_t o = b.size(); b.resize(o + SDH_HB, 0);
    memcpy(&b[o + 8], &st, 2); memcpy(&b[o + 0xA], &en, 2); memcpy(&b[o + 0x10], &pw, 2); memcpy(&b[o + 0x12], &el, 2);
}
int main() {
    std::vector<uint8_t> h1, h2, h3;
    Row(h1, 10, 14, 50); Row(h1, 12, 20, 40); Row(h1, 30, 35, 60);  // overlap -> 1, then a gap -> 2 bursts
    Row(h1, 40, 45, 0, 0);                                            // no damage: ignored
    Row(h2, 5, 8, 70); Row(h2, 9, 12, 70);                           // touching -> 1 burst
    SdhAction a[4];
    a[0].id = 0xC52; a[0].next = 0xC58;                               // intro, no hits
    a[1].id = 0xC58; a[1].next = 0xC57; a[1].hb = h1.data(); a[1].nhb = 4;
    a[2].id = 0xC57; a[2].next = 0xC57; a[2].hb = h2.data(); a[2].nhb = 2;  // self loop ends the chain after counting it
    a[3].id = 0x100; a[3].next = 0x100;
    for (auto& x : a) { x.ev[0] = 20001; x.evFrame[0] = 0; }          // ambient body object in every action
    SdhResult r = SdhCount(a, 4, 0xC52);
    assert(r.chain == 3 && r.bursts == 3 && r.perAct[1] == 2 && r.perAct[2] == 1 && r.spawns == 0 && r.certain());
    a[1].ev[1] = 20007; a[1].evFrame[1] = 15;                         // projectile spawn in the attack: not certain
    r = SdhCount(a, 4, 0xC52); assert(r.spawns == 1 && !r.certain());
    a[1].ev[1] = 0xFFFF;
    Row(h3, 0xFFFF, 0xFFFF, 30); a[3].hb = h3.data(); a[3].nhb = 1; a[2].next = 0x100;  // untimed damaging row: not certain
    r = SdhCount(a, 4, 0xC52); assert(r.untimed == 1 && !r.certain() && r.chain == 4);
    r = SdhCount(a, 4, 0x999); assert(r.chain == 0 && !r.certain());  // unknown start
    char d[256]; SdhDescribe(r, d, sizeof d); puts(d);
    puts("seal_data_hits_test PASS"); return 0;
}
