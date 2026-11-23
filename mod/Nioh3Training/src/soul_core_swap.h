// soul_core_swap.h (Nioh3Training, C) - human #129277: pure logic behind the mod menu 'Equip core' row (no game access; tests/soul_core_swap_test.c).
// Layout (pc2 #128901 coremem.py, proven live #129274/#129275):
//   inv = **(Grva(0x4751530)); equipped slot u32 at inv+0x23B6B8+slot*4 (0xFFFFFFFF = empty; 0,1 Yang, 2.. Yin); item count u64 at inv+0x927D0;
//   item record inv+0x10+i*0xF0 (u16 item id at +0). Item master rows stride 0x1A0: soul core = u32 +0x64 == 900000000 and byte +0x182 == 9,
//   item id u16 at +0x152.
#ifndef N3_SOUL_CORE_SWAP_H
#define N3_SOUL_CORE_SWAP_H
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define SCW_SLOTS_OFF 0x23B6B8u
#define SCW_COUNT_OFF 0x927D0u
#define SCW_REC_OFF 0x10u
#define SCW_REC_STRIDE 0xF0u
#define SCW_NSLOT 8
#define SCW_ROW_STRIDE 0x1A0u
#define SCW_MAX_INV 20000
#define SCW_MAX_IDS 1024
#define SCW_EMPTY 0xFFFFFFFFu

// result codes of a request (Nioh3Training_SoulCoreEquipResult)
enum { SCW_IDLE = 0, SCW_PENDING = 1, SCW_DONE = 8, SCW_REFUSED = -1, SCW_BAD_SLOT = -2, SCW_NOT_CORE = -3, SCW_SAME = -4,
       SCW_NO_GAME = -5, SCW_FAULT = -6, SCW_UNRESOLVED = -7 };

static inline int ScwIsCoreRow(const uint8_t* r) {
    uint32_t cat; memcpy(&cat, r + 0x64, 4);
    return cat == 900000000u && r[0x182] == 9;
}
static inline uint16_t ScwRowId(const uint8_t* r) { uint16_t v; memcpy(&v, r + 0x152, 2); return v; }

// sorted insert, no duplicates; returns the new count
static inline int ScwAddId(uint16_t* ids, int n, int cap, uint16_t id) {
    int lo = 0, hi = n;
    while (lo < hi) { int m = (lo + hi) / 2; if (ids[m] < id) lo = m + 1; else hi = m; }
    if (lo < n && ids[lo] == id) return n;
    if (n >= cap) return n;
    memmove(ids + lo + 1, ids + lo, (size_t)(n - lo) * sizeof *ids);
    ids[lo] = id;
    return n + 1;
}
static inline int ScwHasId(const uint16_t* ids, int n, uint16_t id) {
    int lo = 0, hi = n;
    while (lo < hi) { int m = (lo + hi) / 2; if (ids[m] < id) lo = m + 1; else hi = m; }
    return lo < n && ids[lo] == id;
}

// A request is sent to the game only for a real slot, an owned core and a different core than the one already there.
// The game's own shrine check (Nioh3.exe 2285C48) still runs first on the game thread and may refuse.
static inline int ScwCheck(int slot, uint32_t idx, const uint32_t* slots, const uint32_t* owned, int nowned) {
    if (slot < 0 || slot >= SCW_NSLOT) return SCW_BAD_SLOT;
    int found = 0;
    for (int i = 0; i < nowned; ++i) if (owned[i] == idx) { found = 1; break; }
    if (!found) return SCW_NOT_CORE;
    if (slots[slot] == idx) return SCW_SAME;
    return 0;
}

// Next candidate for the picker: owned cores in inventory order, the one already in `slot` skipped. cur = current pick (inventory idx or
// SCW_EMPTY), dir -1 / +1. Returns SCW_EMPTY when there is no other core.
static inline uint32_t ScwStep(uint32_t cur, int dir, uint32_t inSlot, const uint32_t* owned, int nowned) {
    int pos = -1, cand = 0;
    for (int i = 0; i < nowned; ++i) if (owned[i] != inSlot) ++cand;
    if (!cand) return SCW_EMPTY;
    for (int i = 0; i < nowned; ++i) if (owned[i] == cur) { pos = i; break; }
    if (pos < 0 || cur == inSlot) {  // nothing picked yet: first (Right) or last (Left) candidate
        for (int k = 0; k < nowned; ++k) { int i = dir < 0 ? nowned - 1 - k : k; if (owned[i] != inSlot) return owned[i]; }
        return SCW_EMPTY;
    }
    for (int k = 1; k <= nowned; ++k) {
        int i = ((pos + (dir < 0 ? -k : k)) % nowned + nowned) % nowned;
        if (owned[i] != inSlot) return owned[i];
    }
    return SCW_EMPTY;
}

static inline const char* ScwSlotName(int slot, char* b, int n) {
    if (slot < 2) snprintf(b, (size_t)n, "Yang %d", slot + 1);
    else snprintf(b, (size_t)n, "Yin %d", slot - 1);
    return b;
}
#endif
