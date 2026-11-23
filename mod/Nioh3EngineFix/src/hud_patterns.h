// Shared by the runtime resolver and the offline image audit. Displacements are
// wildcarded; only a unique match in executable sections is accepted.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace hudhide::patterns {
inline constexpr char kMaster[] = "48 8B 05 ?? ?? ?? ?? 48 8B 48 18 48 85 C9 74 07 8A 81 C0 03 00 00 C3";
inline constexpr char kVisibility[] = "48 8B 03 48 8B CB FF 90 28 01 00 00 8B D0 48 8B CF E8 ?? ?? ?? ?? EB 02 32 C0 48 8B 5C 24 30 48 83 C4 20 5F C3";
// Weapon-slot widget (id 10 on v2.02, live sweep #66108): bottom-right weapon silhouettes + stance.
inline constexpr char kWeapons[] = "48 89 5C 24 08 57 48 83 EC 20 45 33 C0 48 8B F9 41 8D 50 ?? E8 ?? ?? ?? ?? 48 8D 05 ?? ?? ?? ?? 33 D2 48 8D 9F 40 01 00 00 48 89 07 48 8B CB 41 B8 80 02 00 00 E8 ?? ?? ?? ?? 48 8B CB E8 ?? ?? ?? ?? 33 D2 83 C9 FF 89 8F C0 03 00 00 33 C0 88 97 C4 03 00 00 89 97 C8 03 00 00 88 97 CC 03 00 00 48 89 97 D0 03 00 00 48 89 87 D8 03 00 00 89 87 E0 03 00 00 8D 41 06 89 87 F0 03 00 00";
// Guardian-spirit widget (id 13 on v2.02, live sweep #66108): green spirit circle + the two held soul-core circles;
// the game has no separate cores-only widget (ids 0-255 swept).
inline constexpr char kSoulCores[] = "48 89 5C 24 08 57 48 83 EC 20 45 33 C0 48 8B F9 41 8D 50 ?? E8 ?? ?? ?? ?? 48 8D 05 ?? ?? ?? ?? 33 D2 48 8D 9F 40 01 00 00 48 89 07 48 8B CB 41 B8 80 02 00 00 E8 ?? ?? ?? ?? 48 8B CB E8 ?? ?? ?? ?? 33 C9 B8 01 00 00 00 89 87 C0 03 00 00 48 89 87 C4 03 00 00 88 87 DA 03 00 00 48 8B C7 48 89 8F CC 03 00 00 89 8F D4 03 00 00 66 89 8F D8 03 00 00";
inline constexpr size_t kCallOffset = 17;
inline unsigned Hex(char c) { return c <= '9' ? c - '0' : c - 'A' + 10; }
inline bool Match(const uint8_t* p, const char* pattern) {
    for (; *pattern; ++p, pattern += pattern[2] ? 3 : 2)
        if (*pattern != '?' && *p != (Hex(pattern[0]) * 16 + Hex(pattern[1]))) return false;
    return true;
}
inline const uint8_t* Find(const uint8_t* p, size_t size, const char* pattern, unsigned& count) {
    const size_t n = (strlen(pattern) + 1) / 3;
    const uint8_t* first = nullptr;
    if (!n || size < n) return nullptr;
    const uint8_t* end = p + size - n + 1;
    const int lead = (int)(Hex(pattern[0]) * 16 + Hex(pattern[1]));
    while (p < end) {
        p = (const uint8_t*)memchr(p, lead, (size_t)(end - p));
        if (!p) break;
        if (Match(p, pattern)) { if (!first) first = p; if (++count > 1) break; }
        ++p;
    }
    return first;
}
}  // namespace hudhide::patterns
