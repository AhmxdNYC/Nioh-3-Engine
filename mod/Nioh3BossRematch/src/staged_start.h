// Session staged-wave start (#88778). Full climbs every generated wave. Final requests only the last
// wave and may set that boss's health to the chain's pre-final fraction. No mission-flag or save writes.
#pragma once
#include <stdint.h>
static inline uint16_t StagedStartIndex(uint16_t n, int finalMode) {
    if (finalMode && n > 1) return (uint16_t)(n - 1);
    return 0;
}
// 1 = the last wave of a Final chain still needs its pre-final health write.
static inline int StagedFinalHpPending(int finalMode, uint16_t n, uint16_t index, uint16_t thou, uint32_t place, int done) {
    return finalMode && n > 1 && (uint16_t)(index + 1) == n && thou && place && !done;
}
static inline uint64_t StagedHpFromThou(uint64_t max, int thou) {
    if (!max || thou < 1) return 0;
    if (thou > 1000) thou = 1000;
    uint64_t t = max / 1000 * (uint64_t)thou + (max % 1000) * (uint64_t)thou / 1000;
    if (t < 1) t = 1;
    if (t > max) t = max;
    return t;
}
