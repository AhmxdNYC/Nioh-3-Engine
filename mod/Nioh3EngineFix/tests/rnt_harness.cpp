// Offline checks for the render-node timer. No game.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include "rnt.h"

static int g_fail = 0;
static void Check(bool c, const char* m) {
    if (!c) { std::printf("FAIL %s\n", m); ++g_fail; }
    else std::printf("ok %s\n", m);
}

static void Plant(std::vector<uint8_t>& img) {
    auto put = [&](uint32_t rva, const uint8_t* b, size_t n) { memcpy(img.data() + rva, b, n); };
    const uint8_t head[] = {0x48, 0x8B, 0x05};
    const uint8_t tail[] = {0x48, 0x85, 0xC0, 0x74, 0x07, 0x8B, 0x80, 0xC0, 0x06, 0x00, 0x00, 0xC3};
    const int32_t rel = (int32_t)(0x4759910 - (0x31A048 + 7));
    put(0x31A048, head, 3);
    memcpy(img.data() + 0x31A048 + 3, &rel, 4);
    put(0x31A04F, tail, sizeof tail);
    const uint8_t stats[] = {0x48, 0x8B, 0x7F, 0x40};
    const uint8_t next[] = {0x48, 0x8B, 0x76, 0x38};
    const uint8_t first[] = {0x48, 0x8B, 0x36};
    const uint8_t graph[] = {0x48, 0x8B, 0x83, 0x28, 0x01, 0x00, 0x00};
    const uint8_t box[] = {0x48, 0x8B, 0x40, 0x28};
    put(0x113292, stats, sizeof stats);
    put(0x112F93, next, sizeof next);
    put(0x112F25, first, sizeof first);
    put(0x3D1F81, graph, sizeof graph);
    put(0x3D1F93, box, sizeof box);
}

int main() {
    std::vector<uint8_t> img(0x3D2000, 0);
    Check(!rnt::BytesMatch(img.data(), img.size()), "blank image does not match");
    Plant(img);
    Check(rnt::BytesMatch(img.data(), img.size()), "v2.02 bytes match");
    img[0x113292] ^= 0xFF;
    Check(!rnt::BytesMatch(img.data(), img.size()), "a flipped stats load does not match");

    alignas(16) uint8_t e0[0x80]{}, e1[0x80]{};
    uint8_t node[16]{}, td[16]{}, inner[0x40]{}, nameObj[8]{};
    void* vt[2]{};
    vt[1] = (uint8_t*)(0x140000000ull + 0x10F060);
    void* vtP = vt;
    memcpy(node, &vtP, sizeof vtP);
    uint32_t hash = 0xAABBCCDD;
    memcpy(td + 0x0C, &hash, 4);
    const char* hello = "Hello";
    memcpy(inner + 0x20, hello, 5);
    uint32_t n = 5, off = 0x20;
    memcpy(inner + 0x10, &n, 4);
    memcpy(inner + 0x14, &off, 4);
    void* innerP = inner;
    memcpy(nameObj, &innerP, sizeof innerP);
    void* nameP = nameObj;
    void* nodeP = node;
    void* tdP = td;
    void* e1p = e1;
    memcpy(e0 + 0x20, &tdP, 8);
    memcpy(e0 + 0x28, &nameP, 8);
    memcpy(e0 + 0x30, &nodeP, 8);
    memcpy(e0 + 0x38, &e1p, 8);

    rnt::Rec recs[4]{};
    rnt::Slot slots[4]{};
    int stopped = 0;
    const uintptr_t base = 0x140000000ull;
    int added = rnt::ArmEntries(e0, recs, slots, 4, base, base + 0x200000, &stopped);
    Check(added == 2, "two entries armed");
    Check(stopped == 0, "a null tail is not a cut");
    void* stats = nullptr;
    memcpy(&stats, e0 + 0x40, 8);
    Check(stats == &recs[0], "stats pointer is the record, not the link");
    void* link = nullptr;
    memcpy(&link, e0 + 0x38, 8);
    Check(link == e1, "next pointer was left alone");
    uint64_t mn = 0;
    memcpy(&mn, recs[0].b + 0x18, 8);
    Check(mn == ~0ull, "min starts at all ones");
    Check(std::strcmp(slots[0].name, "Hello") == 0, "name copied");
    Check(slots[0].typeHash == hash, "type hash copied");
    Check(slots[0].execRva == 0x10F060, "execute RVA");

    int stopped2 = 0;
    int again = rnt::ArmEntries(e0, recs + 2, slots + 2, 2, base, base + 0x200000, &stopped2);
    Check(again == 0, "a second arm does not replace the record");
    memcpy(&stats, e0 + 0x40, 8);
    Check(stats == &recs[0], "original record stays");

    alignas(16) uint8_t loop[0x80]{};
    void* self = loop;
    memcpy(loop + 0x38, &self, 8);
    rnt::Rec one[2]{};
    rnt::Slot oneSlot[2]{};
    int cut = 0;
    int nloop = rnt::ArmEntries(loop, one, oneSlot, 2, base, base + 0x200000, &cut);
    Check(nloop == 1 && cut == 1, "a self-loop arms once and stops");

    Check(rnt::MsFromTicks(1000.0, 10.0) == 100.0, "ticks to ms");
    char line[128];
    rnt::FormatPass(line, 128, 1.204, 2.1, 600, "SceneRenderView", 0x10F060);
    Check(std::strstr(line, "RNT: 1.204 ms max 2.100 n=600 SceneRenderView exec=10F060") != nullptr, "pass line");
    std::printf("%s\n", line);

    if (g_fail) { std::printf("%d FAILED\n", g_fail); return 1; }
    std::printf("ALL PASSED\n");
    return 0;
}
