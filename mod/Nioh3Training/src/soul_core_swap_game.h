// soul_core_swap_game.h (Nioh3Training, C) - human #129277: equip a soul core from the mod menu, on the game thread (ScwTick in the pump).
// Port of tools/soulcore/swapcore.py (pc/2 #129231, proven live #129274/#129275): the shrine handler Nioh3.exe 1DECB78 valid path, in order:
//   ok = 2285C48(cl=slot, rdx=rec); if !ok stop          (the game's own "can this core go in this slot" check)
//   sc = 216B9D0(ctx, dl=slot)                            (d-pad shortcut of the old core, -1 = none)
//   21607A4(ctx, rec); 2183744(ctx, dl=slot, r8=rec); 215F85C(ctx)    (write the slot, rebuild the core list)
//   if sc != -1: 2182968(ctx, edx=sc, r8b=slot)           (move the shortcut onto the new core; without it the old count shows 0)
//   34EDF8(0xF53E7B0C, 0); byte [45C6F2F] = 1             (menu sound / HUD shortcut refresh, as Item::SetShortCut)
// ctx = [4751530] (read on the game thread). Every address is Grva() (pattern on another build, 0 = off). Nothing runs unless the menu
// row asks: Nioh3Training_SoulCoreEquip queues one request, the next pump tick runs it.
#include "soul_core_swap.h"

static volatile LONG g_scw_req;          // 1 = request waiting for the game thread
static volatile LONG g_scw_slot, g_scw_idx;
static volatile LONG g_scw_by_item;      // 1 = g_scw_idx is an item id (any soul core); 0 = inventory index
static volatile LONG g_scw_result;       // SCW_* of the last request
static volatile LONG g_scw_seq;          // bumped when a request finishes
static uint16_t g_scw_ids[SCW_MAX_IDS];  // soul core item ids from the item master (immutable game data, built once)
static int g_scw_nids;
static volatile LONG g_scw_ids_done;
static volatile LONG g_scw_want;         // the menu has asked for the state once: the game thread may read the item master
static DWORD g_scw_ids_t;
static SRWLOCK g_scw_lock = SRWLOCK_INIT;
static struct { DWORD t; int ok; uint32_t slots[SCW_NSLOT]; uint32_t idx[64]; uint16_t item[64]; int n; } g_scw_snap;

static int ScwResolved(void) {
    return Grva(0x4751530) && Grva(0x45B9E30) && Grva(0x45C6F2F) && Grva(0x2285C48) && Grva(0x216B9D0) && Grva(0x21607A4) &&
           Grva(0x2183744) && Grva(0x215F85C) && Grva(0x2182968) && Grva(0x34EDF8);
}
static __attribute__((noinline)) int ScwRd32(uintptr_t a, uint32_t* o) { __try { *o = *(volatile uint32_t*)a; return 1; } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; } }
static __attribute__((noinline)) int ScwRd16(uintptr_t a, uint16_t* o) { __try { *o = *(volatile uint16_t*)a; return 1; } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; } }
static __attribute__((noinline)) int ScwRow(uintptr_t a, uint8_t* r) { __try { memcpy(r, (const void*)a, SCW_ROW_STRIDE); return 1; } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; } }

static uint64_t ScwCtx(void) { uint64_t c = 0; return Rd64(B + Grva(0x4751530), &c) && c > 0x10000ull ? c : 0; }
static uint64_t ScwInv(uint64_t* ctxOut) {
    uint64_t ctx = ScwCtx(), inv = 0;
    if (ctxOut) *ctxOut = ctx;
    return ctx && Rd64(ctx, &inv) && inv > 0x10000ull ? inv : 0;
}
static void ScwBuildIds(void) {  // item master: db = [45B9E30], T = [[db+0x68]], u32 rows at T+4, rows from T+8
    if (g_scw_ids_done) return;
    uint64_t db = 0, t0 = 0, T = 0; uint32_t n = 0;
    if (!Rd64(B + Grva(0x45B9E30), &db) || db < 0x10000ull || !Rd64(db + 0x68, &t0) || t0 < 0x10000ull || !Rd64(t0, &T) || T < 0x10000ull ||
        !ScwRd32((uintptr_t)T + 4, &n) || !n || n > 200000) return;
    int k = 0; uint8_t r[SCW_ROW_STRIDE];
    for (uint32_t i = 0; i < n; ++i) {
        if (!ScwRow((uintptr_t)(T + 8 + (uint64_t)i * SCW_ROW_STRIDE), r)) return;  // table not ready: try again later
        if (ScwIsCoreRow(r)) k = ScwAddId(g_scw_ids, k, SCW_MAX_IDS, ScwRowId(r));
    }
    if (!k) return;
    g_scw_nids = k; InterlockedExchange(&g_scw_ids_done, 1);
    Log("SOULCORE swap: %d soul core ids in the item master (%u rows)", k, n);
}
// equipped slots + owned cores (inventory idx, item id). Returns the owned count, -1 if the inventory is not there.
static int ScwRead(uint32_t slots[SCW_NSLOT], uint32_t* idx, uint16_t* item, int cap) {
    uint64_t inv = ScwInv(0), cnt = 0;
    if (!inv || !Rd64(inv + SCW_COUNT_OFF, &cnt) || cnt > SCW_MAX_INV) return -1;
    for (int s = 0; s < SCW_NSLOT; ++s) if (!ScwRd32((uintptr_t)(inv + SCW_SLOTS_OFF + 4u * s), &slots[s])) return -1;
    if (!g_scw_ids_done) return -1;  // built on the game thread (ScwTick), never on the menu thread
    int n = 0;
    for (uint32_t i = 0; i < (uint32_t)cnt && n < cap; ++i) {
        uint16_t id = 0;
        if (!ScwRd16((uintptr_t)(inv + SCW_REC_OFF + (uint64_t)i * SCW_REC_STRIDE), &id)) return -1;
        if (ScwHasId(g_scw_ids, g_scw_nids, id)) { idx[n] = i; item[n] = id; ++n; }
    }
    return n;
}

typedef uint8_t (*ScwOkFn)(uint8_t, uint64_t);
typedef int32_t (*ScwScFn)(uint64_t, uint8_t);
typedef void (*ScwRecFn)(uint64_t, uint64_t);
typedef void (*ScwEqFn)(uint64_t, uint8_t, uint64_t);
typedef void (*ScwCtxFn)(uint64_t);
typedef void (*ScwShFn)(uint64_t, int32_t, uint8_t);
typedef void (*ScwSndFn)(uint32_t, uint64_t);

static uint16_t ScwSlotItem(uint64_t inv, uint32_t slotIdx) {
    if (!inv || slotIdx == SCW_EMPTY) return 0xFFFF;
    uint16_t id = 0xFFFF;
    if (!ScwRd16((uintptr_t)(inv + SCW_REC_OFF + (uint64_t)slotIdx * SCW_REC_STRIDE), &id)) return 0xFFFF;
    return id;
}
// First inventory record with this item id. 1 = found, 0 = not in the save, -1 = unreadable.
static int ScwFindItem(uint64_t inv, uint64_t cnt, uint16_t want, uint32_t* outIdx) {
    if (!inv || cnt > SCW_MAX_INV) return -1;
    for (uint32_t i = 0; i < (uint32_t)cnt; ++i) {
        uint16_t id = 0;
        if (!ScwRd16((uintptr_t)(inv + SCW_REC_OFF + (uint64_t)i * SCW_REC_STRIDE), &id)) return -1;
        if (id == want) { *outIdx = i; return 1; }
    }
    return 0;
}
// The game's own item add (notes/session-2026-10-08-gamedata.md): 54CCD0 builds the 0xE8 record, 54D324 adds it.
// rcx is the save at [4751530] (ScwCtx), the same object scroll give passes. One path for every item id.
static int ScwGrant(uint64_t ctx, uint16_t itemId) {
    const uintptr_t build = Grva(0x54CCD0), add = Grva(0x54D324);
    if (!build || !add || !ctx) return 0;
    _Alignas(16) uint8_t desc[0x200];
    _Alignas(16) uint8_t outb[0x200];
    memset(desc, 0, sizeof desc);
    memset(outb, 0, sizeof outb);
    __try {
        ((void (*)(void*, uint32_t, uint32_t))(B + build))(desc, itemId, 1);
        ((void (*)(uint64_t, void*, void*, uint64_t, uint64_t))(B + add))(ctx, outb, desc, 0, 1);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    return 1;
}
static __attribute__((noinline)) int ScwApply(int slot, uint32_t idx, const uint32_t slots[SCW_NSLOT], int* stage) {
    uint64_t ctx = 0, inv = ScwInv(&ctx);
    if (!inv || !ctx) return SCW_NO_GAME;
    const uint64_t rec = inv + SCW_REC_OFF + (uint64_t)idx * SCW_REC_STRIDE;
    const uint8_t s = (uint8_t)slot;
    __try {
        *stage = 1;
        if (!((ScwOkFn)(B + Grva(0x2285C48)))(s, rec)) return SCW_REFUSED;
        *stage = 2; const int32_t sc = ((ScwScFn)(B + Grva(0x216B9D0)))(ctx, s);
        *stage = 3; ((ScwRecFn)(B + Grva(0x21607A4)))(ctx, rec);
        *stage = 4; ((ScwEqFn)(B + Grva(0x2183744)))(ctx, s, rec);
        *stage = 5; ((ScwCtxFn)(B + Grva(0x215F85C)))(ctx);
        *stage = 6; if (sc != -1) ((ScwShFn)(B + Grva(0x2182968)))(ctx, sc, s);
        *stage = 7; ((ScwSndFn)(B + Grva(0x34EDF8)))(0xF53E7B0Cu, 0);
        *(volatile uint8_t*)(B + Grva(0x45C6F2F)) = 1;
        *stage = SCW_DONE;
        Log("SOULCORE swap: slot %d old idx %d -> idx %u (item %X), shortcut %d", slot, slots[slot] == SCW_EMPTY ? -1 : (int)slots[slot], idx,
            (unsigned)*(volatile uint16_t*)rec, sc);
    } __except (EXCEPTION_EXECUTE_HANDLER) { Log("SOULCORE swap: fault %08lX at stage %d", GetExceptionCode(), *stage); return SCW_FAULT; }
    return SCW_DONE;
}
static __attribute__((noinline)) int ScwRun(int slot, uint32_t idx, int* stage) {
    uint32_t slots[SCW_NSLOT], oidx[64]; uint16_t oit[64];
    const int n = ScwRead(slots, oidx, oit, 64);
    if (n < 0) return SCW_NO_GAME;
    const int chk = ScwCheck(slot, idx, slots, oidx, n);
    if (chk) return chk;
    return ScwApply(slot, idx, slots, stage);
}
// One equip for every item-master soul core. Owned: the inventory record that already has this id. Not owned: the game's
// item add, then that new record. Both then take the shrine order above. No per-boss id table.
static int ScwRunItem(int slot, uint16_t itemId, int* stage) {
    uint32_t slots[SCW_NSLOT], oidx[64]; uint16_t oit[64];
    const int n = ScwRead(slots, oidx, oit, 64);
    if (n < 0 || !g_scw_ids_done) return SCW_NO_GAME;
    (void)oidx; (void)oit;
    if (!ScwHasId(g_scw_ids, g_scw_nids, itemId)) return SCW_NOT_CORE;
    uint64_t ctx = 0, inv = ScwInv(&ctx);
    if (!inv || !ctx) return SCW_NO_GAME;
    uint64_t cnt = 0;
    if (!Rd64(inv + SCW_COUNT_OFF, &cnt) || cnt > SCW_MAX_INV) return SCW_NO_GAME;
    const uint16_t inItem = ScwSlotItem(inv, slots[slot]);
    const int chk = ScwCheckId(slot, itemId, 1, inItem);
    if (chk) return chk;
    uint32_t idx = 0;
    int found = ScwFindItem(inv, cnt, itemId, &idx);
    if (found < 0) return SCW_NO_GAME;
    if (!found) {
        *stage = 10;
        if (!ScwGrant(ctx, itemId)) return SCW_NO_ADD;
        if (!Rd64(inv + SCW_COUNT_OFF, &cnt) || cnt > SCW_MAX_INV) return SCW_NO_ADD;
        found = ScwFindItem(inv, cnt, itemId, &idx);
        if (found <= 0) return SCW_NO_ADD;
        Log("SOULCORE swap: added item %X (was not owned) at idx %u, then the shrine equip", itemId, idx);
        for (int s = 0; s < SCW_NSLOT; ++s) if (!ScwRd32((uintptr_t)(inv + SCW_SLOTS_OFF + 4u * s), &slots[s])) return SCW_NO_GAME;
    }
    return ScwApply(slot, idx, slots, stage);
}
static void ScwTick(void) {  // game thread (pump)
    if (g_scw_want && !g_scw_ids_done) { const DWORD now = GetTickCount(); if (!g_scw_ids_t || now - g_scw_ids_t > 2000) { g_scw_ids_t = now ? now : 1; ScwBuildIds(); } }
    if (!g_scw_req) return;
    const int slot = (int)g_scw_slot; const uint32_t idx = (uint32_t)g_scw_idx;
    int stage = 0;
    const int r = g_scw_by_item ? ScwRunItem(slot, (uint16_t)idx, &stage) : ScwRun(slot, idx, &stage);
    if (r != SCW_DONE) Log("SOULCORE swap: slot %d idx %u not equipped (code %d, stage %d)", slot, idx, r, stage);
    InterlockedExchange(&g_scw_result, r);
    InterlockedExchange(&g_scw_req, 0);
    InterlockedIncrement(&g_scw_seq);
    AcquireSRWLockExclusive(&g_scw_lock); g_scw_snap.t = 0; ReleaseSRWLockExclusive(&g_scw_lock);  // next menu read is fresh
}

// ---- C API for the Boss Rematch menu (any thread). Read-only state, cached 250 ms.
// Returns the owned soul core count (inventory order, at most cap), -1 = not available (no save loaded / addresses not found on this build).
N3T_API int Nioh3Training_SoulCoreState(uint32_t* slots8, uint32_t* idx, uint16_t* item, int cap) {
    if (!ScwResolved()) return -1;
    InterlockedExchange(&g_scw_want, 1);
    AcquireSRWLockExclusive(&g_scw_lock);
    const DWORD now = GetTickCount();
    if (!g_scw_snap.t || now - g_scw_snap.t > 250) {
        const int n = ScwRead(g_scw_snap.slots, g_scw_snap.idx, g_scw_snap.item, 64);
        g_scw_snap.ok = n >= 0; g_scw_snap.n = n < 0 ? 0 : n; g_scw_snap.t = now ? now : 1;
    }
    int n = g_scw_snap.ok ? g_scw_snap.n : -1;
    if (n >= 0) {
        if (slots8) memcpy(slots8, g_scw_snap.slots, sizeof g_scw_snap.slots);
        if (n > cap) n = cap;
        for (int i = 0; i < n; ++i) { if (idx) idx[i] = g_scw_snap.idx[i]; if (item) item[i] = g_scw_snap.item[i]; }
    }
    ReleaseSRWLockExclusive(&g_scw_lock);
    return n;
}
// Queue one equip (slot 0..7, idx = inventory index from SoulCoreState). 1 = queued, 0 = one already waiting, -1 = not available.
N3T_API int Nioh3Training_SoulCoreEquip(int slot, int idx) {
    if (!ScwResolved()) return -1;
    if (slot < 0 || slot >= SCW_NSLOT || idx < 0) return -1;
    if (g_scw_req) return 0;
    InterlockedExchange(&g_scw_slot, slot); InterlockedExchange(&g_scw_idx, idx);
    InterlockedExchange(&g_scw_by_item, 0);
    InterlockedExchange(&g_scw_result, SCW_PENDING);
    InterlockedExchange(&g_scw_req, 1);
    Log("SOULCORE swap: menu asks slot %d idx %d (human #129277)", slot, idx);
    return 1;
}
// Result of the last request (SCW_*: 1 pending, 8 done, <0 refused / failed); *seq = finished request count.
N3T_API int Nioh3Training_SoulCoreEquipResult(int* seq) { if (seq) *seq = (int)g_scw_seq; return (int)g_scw_result; }
// Every soul core id in the item master (sorted). *slotItem8 = item id in each equip slot, or 0xFFFF if empty / unreadable.
// Returns the full count, -1 if the list is not built yet. Copies at most cap ids.
N3T_API int Nioh3Training_SoulCoreCatalog(uint16_t* ids, int cap, uint16_t* slotItem8) {
    if (!ScwResolved()) return -1;
    InterlockedExchange(&g_scw_want, 1);
    if (!g_scw_ids_done) return -1;
    const int all = g_scw_nids;
    if (ids && cap > 0) {
        const int n = all > cap ? cap : all;
        memcpy(ids, g_scw_ids, (size_t)n * sizeof *ids);
    }
    if (slotItem8) {
        uint64_t inv = ScwInv(0);
        for (int s = 0; s < SCW_NSLOT; ++s) slotItem8[s] = 0xFFFF;
        if (inv) {
            uint32_t slots[SCW_NSLOT];
            int ok = 1;
            for (int s = 0; s < SCW_NSLOT; ++s) if (!ScwRd32((uintptr_t)(inv + SCW_SLOTS_OFF + 4u * s), &slots[s])) ok = 0;
            if (ok) for (int s = 0; s < SCW_NSLOT; ++s) slotItem8[s] = ScwSlotItem(inv, slots[s]);
        }
    }
    return all;
}
// Queue one equip by item id (any soul core in the item master, owned or not). 1 = queued, 0 = busy, -1 = not available.
N3T_API int Nioh3Training_SoulCoreEquipItem(int slot, int itemId) {
    if (!ScwResolved()) return -1;
    if (slot < 0 || slot >= SCW_NSLOT || itemId < 0 || itemId > 0xFFFF) return -1;
    if (g_scw_req) return 0;
    InterlockedExchange(&g_scw_slot, slot); InterlockedExchange(&g_scw_idx, itemId);
    InterlockedExchange(&g_scw_by_item, 1);
    InterlockedExchange(&g_scw_result, SCW_PENDING);
    InterlockedExchange(&g_scw_req, 1);
    Log("SOULCORE swap: menu asks slot %d item %X (human #136123)", slot, itemId);
    return 1;
}

// TASK #137305 (human #137303): the overlay names each equipped core. soul_core_name.h walks core -> soul-core row -> Yang seal -> spawn
// character -> name text id; the text comes from the game's own TextGetById (5A4B04, as training.c's mission-end check). Read-only, any thread:
// every read is SEH-guarded, the result is cached per item id, nothing is written to the game.
#include "soul_core_name.h"
static __attribute__((noinline)) int ScnGameRead(void* c, uint64_t a, void* out, unsigned n) {
    (void)c; if (a < 0x10000ull) return 0;
    __try { memcpy(out, (const void*)(uintptr_t)a, n); return 1; } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
static __attribute__((noinline)) int ScnGameText(uint32_t nid, uint16_t* w, int cap) {
    const uintptr_t f = Grva(0x5A4B04); if (!f || cap < 2) return 0;
    __try {
        const uint16_t* t = (const uint16_t*)((const wchar_t* (*)(uint32_t))(B + f))(nid);
        if (!t) return 0;
        int i = 0; for (; i < cap - 1 && t[i]; ++i) w[i] = t[i];
        w[i] = 0; return i;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
#define SCN_CACHE 256
static SRWLOCK g_scn_lock = SRWLOCK_INIT;
static struct { uint16_t id; int8_t state; char name[48]; } g_scn[SCN_CACHE];  // state 1 = named, -1 = no name in the game data
static int g_scn_n, g_scn_chr = -2;  // character table GameData offset; -2 = not searched, -1 = searched, not found
static DWORD g_scn_retry;
// 1 = out holds the yokai's name (ASCII), 0 = not known (yet): the caller shows 'Core <id>'.
N3T_API int Nioh3Training_SoulCoreName(int itemId, char* out, int cap) {
    if (!out || cap < 2 || itemId <= 0 || itemId >= 0xFFFF) return 0;
    out[0] = 0;
    int got = 0, known = 0;
    AcquireSRWLockShared(&g_scn_lock);
    for (int i = 0; i < g_scn_n; ++i) if (g_scn[i].id == (uint16_t)itemId) {
        known = 1; if (g_scn[i].state > 0) { snprintf(out, (size_t)cap, "%s", g_scn[i].name); got = 1; } break;
    }
    ReleaseSRWLockShared(&g_scn_lock);
    if (known) return got;
    const DWORD now = GetTickCount();
    if (!ScwResolved() || !g_scw_ids_done || (g_scn_retry && now - g_scn_retry < 2000)) return 0;  // game data not ready: try later
    AcquireSRWLockExclusive(&g_scn_lock);
    for (int i = 0; i < g_scn_n; ++i) if (g_scn[i].id == (uint16_t)itemId) {  // another thread just did it
        if (g_scn[i].state > 0) { snprintf(out, (size_t)cap, "%s", g_scn[i].name); got = 1; }
        ReleaseSRWLockExclusive(&g_scn_lock); return got;
    }
    uint64_t gd = 0; ScnTable it, sc, ch; int definite = 0; char name[48] = {0};
    if (Rd64(B + Grva(0x45B9E30), &gd) && gd > 0x10000ull && ScnTableAt(ScnGameRead, 0, gd, 0x68, &it) && ScnTableAt(ScnGameRead, 0, gd, 0x118, &sc)) {
        const uint32_t spawn = ScnSpawnId(ScnGameRead, 0, &it, &sc, (uint16_t)itemId);
        if (!spawn) definite = 1;  // not a mapped core in this game's tables
        else {
            if (g_scn_chr == -2) {
                g_scn_chr = ScnFindChr(ScnGameRead, 0, gd, spawn);
                Log("SOULCORE name: character table at GameData+0x%X (core %X spawn %X, TASK #137305)", g_scn_chr, itemId, spawn);
            }
            uint32_t nid = 0;
            if (g_scn_chr >= 0 && ScnTableAt(ScnGameRead, 0, gd, (uint32_t)g_scn_chr, &ch)) nid = ScnNameInChr(ScnGameRead, 0, &ch, spawn);
            uint16_t w[96];
            if (nid && ScnGameText(nid, w, 96) && ScnAscii(w, 96, name, (int)sizeof name)) got = 1;
            definite = g_scn_chr >= 0 && (got || !nid);  // a text miss may only be the language not loaded yet: retry later
            Log("SOULCORE name: core %X spawn %X name id %u -> %s", itemId, spawn, nid, got ? name : "(none)");
        }
    }
    if ((got || definite) && g_scn_n < SCN_CACHE) {
        g_scn[g_scn_n].id = (uint16_t)itemId; g_scn[g_scn_n].state = got ? 1 : -1; memcpy(g_scn[g_scn_n].name, name, sizeof name); ++g_scn_n;
    } else g_scn_retry = now ? now : 1;
    ReleaseSRWLockExclusive(&g_scn_lock);
    if (got) snprintf(out, (size_t)cap, "%s", name);
    return got;
}
