#ifndef N3_KB_SLOT_H
#define N3_KB_SLOT_H
#include <stdint.h>
// Boss Ki-break / action / no-finisher slots are keyed by actor address. The game
// reuses that address for the next boss. A known id (gauge row +4, or the boss-mode
// handle) that differs is a new occupant, even inside the 3-10 s stale window.
// A zero id is unknown and is not treated as a change. No boss list is involved.
static int KbIdChanged(uint32_t slot_id, uint32_t id) {
    return slot_id != 0 && id != 0 && slot_id != id;
}
// *reset is 1 when the caller must clear broken / finisher / valid state before use.
// Returns -1 when every slot still belongs to a different fresh occupant.
static int KbPickSlot(const uint64_t* addr, const uint32_t* id, const uint32_t* seen,
                      int n, uint64_t a, uint32_t who, uint32_t now, uint32_t stale_ms, int* reset) {
    int same = -1, reused = -1, steal = -1;
    if (reset) *reset = 0;
    if (!a || n <= 0) return -1;
    for (int i = 0; i < n; ++i) {
        if (addr[i] != a) continue;
        if (KbIdChanged(id[i], who)) { if (reused < 0) reused = i; }
        else { same = i; break; }
    }
    if (same >= 0) return same;
    if (reused >= 0) { if (reset) *reset = 1; return reused; }
    for (int i = 0; i < n; ++i)
        if (!addr[i] || (uint32_t)(now - seen[i]) > stale_ms) { steal = i; break; }
    if (steal < 0) return -1;
    if (reset) *reset = 1;
    return steal;
}
#endif
