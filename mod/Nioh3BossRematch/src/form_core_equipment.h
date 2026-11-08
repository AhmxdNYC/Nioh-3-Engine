// Read-only mapping of copied game tables. No game calls, memory scans, or hardcoded core IDs.
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace formcore {
constexpr size_t kItemStride = 0x1A0, kParamStride = 0x54;
struct Entry { uint16_t item; uint32_t chr; };
template<class T> T Field(const uint8_t* row, size_t offset) {
    T v; std::memcpy(&v, row + offset, sizeof v); return v;
}
inline std::vector<Entry> BuildMap(const std::vector<uint8_t>& items, const std::vector<uint8_t>& params) {
    std::vector<Entry> result;
    if (items.empty() || params.empty() || items.size() % kItemStride || params.size() % kParamStride) return result;
    for (size_t i = 0; i < items.size(); i += kItemStride) {
        const uint8_t* core = items.data() + i;
        if (Field<uint32_t>(core, 0x64) != 900000000 || core[0x182] != 9) continue;
        const uint16_t item = Field<uint16_t>(core, 0x152), key = Field<uint16_t>(core, 0x180);
        if (!item || item == 0xFFFF || !key || key == 0xFFFF) continue;
        uint32_t spell = 0;
        int matches = 0;
        for (size_t j = 0; j < params.size(); j += kParamStride) {
            const uint8_t* param = params.data() + j;
            if (Field<uint32_t>(param, 0x0C) == key) { spell = Field<uint32_t>(param, 0x18); ++matches; }
        }
        if (matches != 1 || !spell || spell >= 0xFFFF) continue;
        uint32_t chr = 0;
        matches = 0;
        for (size_t j = 0; j < items.size(); j += kItemStride) {
            const uint8_t* seal = items.data() + j;
            if (Field<uint16_t>(seal, 0x152) != spell) continue;
            ++matches;
            if (Field<uint32_t>(seal, 0x64) == 700000000 && seal[0x182] == 7) chr = Field<uint32_t>(seal, 0x54);
        }
        if (matches == 1 && chr && chr != 0xFFFFFFFF) result.push_back({item, chr});
    }
    std::sort(result.begin(), result.end(), [](const Entry& a, const Entry& b) { return a.item < b.item; });
    // Ambiguous duplicate item IDs are not trustworthy, even if their character IDs happen to match.
    std::vector<Entry> unique;
    for (size_t i = 0; i < result.size();) {
        size_t end = i + 1;
        while (end < result.size() && result[end].item == result[i].item) ++end;
        if (end == i + 1) unique.push_back(result[i]);
        i = end;
    }
    return unique;
}
inline uint32_t Lookup(const std::vector<Entry>& map, uint16_t item) {
    const auto it = std::lower_bound(map.begin(), map.end(), item, [](const Entry& e, uint16_t id) { return e.item < id; });
    return it != map.end() && it->item == item ? it->chr : 0;
}
// inv is the already-resolved inventory data, not a live process handle. The reader is SEH-guarded by the caller.
// Double-read the indices and count so a concurrent save/equipment update is discarded, never partly published.
template<class Read> bool EquippedItems(uint64_t inv, Read&& read, uint16_t (&items)[2]) {
    uint64_t count = 0, countAfter = 0;
    uint32_t indices[2] = {}, after[2] = {};
    items[0] = items[1] = 0;
    if (!inv || !read(inv + 0x927D0, count) || count > 2500 ||
        !read(inv + 0x23B6B8, indices[0]) || !read(inv + 0x23B6BC, indices[1])) return false;
    for (int i = 0; i < 2; ++i) {
        if (indices[i] == 0xFFFFFFFF) continue;  // empty Yang slot
        if (indices[i] >= count || !read(inv + 0x10 + uint64_t(indices[i]) * 0xF0, items[i]) ||
            !items[i] || items[i] == 0xFFFF) return false;
    }
    return read(inv + 0x927D0, countAfter) && countAfter == count &&
        read(inv + 0x23B6B8, after[0]) && read(inv + 0x23B6BC, after[1]) &&
        after[0] == indices[0] && after[1] == indices[1];
}
} // namespace formcore
