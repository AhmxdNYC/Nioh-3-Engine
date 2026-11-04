#include "../src/hud_patterns.h"
#include <windows.h>
#include <cassert>
#include <fstream>
#include <iterator>
#include <vector>
#include <cstdio>
using namespace hudhide::patterns;

int main(int argc, char** argv) {
    const uint8_t bytes[] = {0xCC, 0x48, 0x8B, 0x01, 0xC3, 0x48, 0x8B, 0xFF, 0xC3};
    unsigned count = 0;
    assert(Find(bytes, 5, "48 8B ?? C3", count) == bytes + 1 && count == 1);
    count = 0;
    assert(Find(bytes, sizeof(bytes), "48 8B ?? C3", count) == bytes + 1 && count == 2);
    count = 0;
    assert(!Find(bytes, 4, "48 8B ?? C3", count) && count == 0);
    count = 0;
    assert(!Find(bytes, sizeof(bytes), "48 89 ?? C3", count) && count == 0);
    assert(argc == 2);
    std::ifstream file(argv[1], std::ios::binary);
    std::vector<uint8_t> image((std::istreambuf_iterator<char>(file)), {});
    assert(image.size() > sizeof(IMAGE_DOS_HEADER));
    auto* dos = (const IMAGE_DOS_HEADER*)image.data();
    auto* nt = (const IMAGE_NT_HEADERS*)(image.data() + dos->e_lfanew);
    auto* sec = IMAGE_FIRST_SECTION(nt);
    const uint8_t* hits[2] = {};
    const char* pats[] = {kMaster, kVisibility};
    for (int p = 0; p < 2; ++p) {
        count = 0;
        for (unsigned s = 0; s < nt->FileHeader.NumberOfSections; ++s) {
            if (!(sec[s].Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
            assert((size_t)sec[s].VirtualAddress + sec[s].Misc.VirtualSize <= image.size());
            auto* hit = Find(image.data() + sec[s].VirtualAddress, sec[s].Misc.VirtualSize, pats[p], count);
            if (hit) hits[p] = hit;
        }
        assert(count == 1);
        printf("pattern %d: one match at %llX\n", p, (unsigned long long)(hits[p] - image.data()));
    }
    assert((hits[0] - image.data()) % 8 == 0);
    const uint8_t* call = hits[1] + kCallOffset;
    assert(call[0] == 0xE8 && (call + 1 - image.data()) % 4 == 0);
    int32_t disp;
    memcpy(&disp, call + 1, 4);
    auto* target = call + 5 + disp;
    assert(target >= image.data() && target + 13 <= image.data() + image.size());
    assert(Match(target, "8B 81 ?? ?? ?? ?? 83 F8 FF 75 03 B0 01"));
    printf("PASS: wildcard, duplicate, truncated, missing, real-image unique patterns and call target\n");
}
