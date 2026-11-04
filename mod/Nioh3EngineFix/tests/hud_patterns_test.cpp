#include "../src/hudhide.h"
#include <windows.h>
#include <cassert>
#include <fstream>
#include <iterator>
#include <vector>
#include <cstdio>
using namespace hudhide::patterns;

void CheckMaskRestoration() {
    using namespace hudhide;
    // Only private test memory is patched; no game process, hook or code is run.
    auto* page = (uint8_t*)VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    assert(page);
    g_scanned = true;
    g_page = page;
    g_call = page + 0x203; // aligned rel32 at +0x204
    g_callOrig = 64;
    g_call[0] = 0xE8;
    memcpy(g_call + 1, &g_callOrig, 4);
    g_weaponsId = 22; g_soulCoresId = 19;
    const int weapons = ElemIndex("HideHudWeapons"), cores = ElemIndex("HideHudSoulCores");
    assert(weapons >= 0 && cores >= 0);
    for (int arm = 0; arm < 4; ++arm) {
        g_elemOn[weapons] = arm & 1;
        g_elemOn[cores] = (arm >> 1) & 1;
        SyncIds();
        const uint32_t expected = ((arm & 1) ? 1u << 22 : 0) | ((arm & 2) ? 1u << 19 : 0);
        assert(Mask()[0] == expected);
        for (int i = 1; i < 8; ++i) assert(Mask()[i] == 0);
        assert(g_redirected == (arm != 0));
    }
    g_elemOn[weapons] = g_elemOn[cores] = 0;
    SyncIds();
    int32_t restored;
    memcpy(&restored, g_call + 1, 4);
    assert(!g_redirected && restored == g_callOrig && Mask()[0] == 0);
    // A failed constructor lookup must never hide widget zero or a guessed id.
    g_weaponsId = 0;
    g_elemOn[weapons] = 1;
    SyncIds();
    assert(!ElementAvailable(weapons) && !g_redirected && Mask()[0] == 0);
    assert(SetIdsLive("7")); SyncIds();
    assert(Mask()[0] == (1u << 7) && g_redirected);
    assert(SetIdsLive("14,255,14")); SyncIds();
    assert(Mask()[0] == (1u << 14) && Mask()[7] == (1u << 31));
    for (const char* bad : {"-1", "256", "7,", "7,,8", "7,x", "999999999999999999999", ""}) {
        assert(!SetIdsLive(bad)); SyncIds();
        assert(Mask()[0] == (1u << 14) && Mask()[7] == (1u << 31));
    }
    assert(SetIdsLive("none")); SyncIds();
    memcpy(&restored, g_call + 1, 4);
    assert(!g_redirected && restored == g_callOrig && Mask()[0] == 0 && Mask()[7] == 0);
    puts("PASS: live raw list replaces mask, duplicate IDs, invalid input atomicity, clear restores call");
    VirtualFree(page, 0, MEM_RELEASE);
    g_page = g_call = nullptr;
    puts("PASS: both HUD masks independently/combined, OFF restoration, missing widget safe");
}

int main(int argc, char** argv) {
    CheckMaskRestoration();
    const uint8_t bytes[] = {0xCC, 0x48, 0x8B, 0x01, 0xC3, 0x48, 0x8B, 0xFF, 0xC3};
    unsigned count = 0;
    assert(Find(bytes, 5, "48 8B ?? C3", count) == bytes + 1 && count == 1);
    count = 0;
    assert(Find(bytes, sizeof(bytes), "48 8B ?? C3", count) == bytes + 1 && count == 2);
    count = 0;
    assert(!Find(bytes, 4, "48 8B ?? C3", count) && count == 0);
    count = 0;
    assert(!Find(bytes, sizeof(bytes), "48 89 ?? C3", count) && count == 0);
    if (argc == 1) {
        puts("PASS: HUD pattern wildcard, duplicate, truncated and missing fixtures (set NIOH3_HUD_DUMP for image audit)");
        return 0;
    }
    assert(argc == 2);
    std::ifstream file(argv[1], std::ios::binary);
    std::vector<uint8_t> image((std::istreambuf_iterator<char>(file)), {});
    assert(image.size() > sizeof(IMAGE_DOS_HEADER));
    auto* dos = (const IMAGE_DOS_HEADER*)image.data();
    auto* nt = (const IMAGE_NT_HEADERS*)(image.data() + dos->e_lfanew);
    auto* sec = IMAGE_FIRST_SECTION(nt);
    const uint8_t* hits[4] = {};
    const char* pats[] = {kMaster, kVisibility, kWeapons, kSoulCores};
    for (int p = 0; p < 4; ++p) {
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
    assert(hits[2][19] == 22 && hits[3][19] == 19);
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
