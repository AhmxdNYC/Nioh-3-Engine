// Shared by the runtime resolver and the offline image audit. Displacements are
// wildcarded; only a unique match in executable sections is accepted.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace hudhide::patterns {
inline constexpr char kMaster[] = "48 8B 05 ?? ?? ?? ?? 48 8B 48 18 48 85 C9 74 07 8A 81 C0 03 00 00 C3";
inline constexpr char kVisibility[] = "48 8B 03 48 8B CB FF 90 28 01 00 00 8B D0 48 8B CF E8 ?? ?? ?? ?? EB 02 32 C0 48 8B 5C 24 30 48 83 C4 20 5F C3";
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
