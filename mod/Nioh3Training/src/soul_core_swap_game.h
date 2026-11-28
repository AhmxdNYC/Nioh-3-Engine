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

static int32_t g_scw_sc[SCW_NSLOT];      // last shortcut id 216B9D0 returned for this slot
static uint8_t g_scw_sc_known[SCW_NSLOT];
static volatile LONG g_scw_req;          // 1 = request waiting for the game thread
static volatile LONG g_scw_slot, g_scw_idx;
static volatile LONG g_scw_yang_only;   // human #143767: 1 = F2 equip, Yang slot only, no Yin fallback
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
    // TASK #141276 (human #141268: 'F2 still cycles thru items too'): a core-category row is a soul core only when the game links it to a yokai:
    // core +0x180 key -> exactly one soul-core row ([db+0x118], +0x0C key, +0x18 Yang seal item) -> exactly one item row with that id, a Summoning
    // Seal (+0x64 == 700000000, +0x182 == 7) with a spawn character at +0x54. The same chain as BR's formcore::BuildMap. Unlinked ids (live: 64CF,
    // 6DB5 - refused by the shrine check / equipped as a plain item) are left out of the catalog, the owned list, F2 and the menu picker.
    uint64_t s0 = 0, S = 0; uint32_t sn = 0;
    if (!Rd64(db + 0x118, &s0) || s0 < 0x10000ull || !Rd64(s0, &S) || S < 0x10000ull || !ScwRd32((uintptr_t)S + 4, &sn) || !sn || sn > 200000) return;
    static uint16_t cid[SCW_MAX_IDS], ckey[SCW_MAX_IDS], cyang[SCW_MAX_IDS]; static uint8_t cseal[SCW_MAX_IDS];
    int nc = 0; uint8_t r[SCW_ROW_STRIDE];
    for (uint32_t i = 0; i < n; ++i) {
        if (!ScwRow((uintptr_t)(T + 8 + (uint64_t)i * SCW_ROW_STRIDE), r)) return;  // table not ready: try again later
        if (ScwIsCoreRow(r) && nc < SCW_MAX_IDS) { cid[nc] = ScwRowId(r); memcpy(&ckey[nc], r + 0x180, 2); cyang[nc] = 0; cseal[nc] = 0; ++nc; }
    }
    for (int c = 0; c < nc; ++c) {  // key -> Yang seal item, exactly one soul-core row
        int m = 0; uint32_t yang = 0;
        if (!ckey[c] || ckey[c] == 0xFFFF) continue;
        for (uint32_t j = 0; j < sn; ++j) {
            uint32_t k = 0, y = 0; const uintptr_t row = (uintptr_t)(S + 8 + (uint64_t)j * SCW_SC_STRIDE);
            if (!ScwRd32(row + 0x0C, &k)) return;
            if (k == ckey[c]) { if (!ScwRd32(row + 0x18, &y)) return; yang = y; ++m; }
        }
        if (m == 1 && yang && yang < 0xFFFF) cyang[c] = (uint16_t)yang;
    }
    for (uint32_t i = 0; i < n; ++i) {  // seal rows: count every row with the Yang item id (exactly one, a seal with a spawn character)
        if (!ScwRow((uintptr_t)(T + 8 + (uint64_t)i * SCW_ROW_STRIDE), r)) return;
        const uint16_t id = ScwRowId(r);
        for (int c = 0; c < nc; ++c) if (cyang[c] && cyang[c] == id) cseal[c] = (uint8_t)(cseal[c] < 2 ? cseal[c] + (ScwIsSealRow(r) ? 1 : 2) : 2);
    }
    int k = 0, drop = 0; char dl[160]; int dlo = 0; dl[0] = 0;
    for (int c = 0; c < nc; ++c) {
        if (cseal[c] == 1) k = ScwAddId(g_scw_ids, k, SCW_MAX_IDS, cid[c]);
        else { ++drop; if (dlo < (int)sizeof dl - 8) dlo += snprintf(dl + dlo, sizeof dl - (size_t)dlo, "%s%X", drop > 1 ? "," : "", cid[c]); }
    }
    if (!k) return;
    g_scw_nids = k; InterlockedExchange(&g_scw_ids_done, 1);
    Log("SOULCORE swap: %d soul core ids in the item master (%u rows); %d core-category ids with no yokai link left out: %s (TASK #141276)", k, n, drop, drop ? dl : "none");
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
        *stage = 2; int32_t sc = ((ScwScFn)(B + Grva(0x216B9D0)))(ctx, s);
        // Live 04:18: the first swap returns shortcut 1 and the d-pad follows the new core. The next swap
        // returns -1, so the move was skipped and DOWN kept throwing the first yokai. Reuse the last real shortcut.
        if (sc != -1) { g_scw_sc[s] = sc; g_scw_sc_known[s] = 1; }
        else if (g_scw_sc_known[s]) sc = g_scw_sc[s];
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
// TASK #140431 (human #140428/#140539): some cores make no usable item in the d-pad slots 0-1 (live: CE1B/9F09 none there, 7209/ED03 x3
// in slots 2-3), so the d-pad showed an empty slot. Count the usable-core list the equip rebuilds (inv+0x1E0AD0, 1200 x 0xE8, u16 id ==
// u16 base id, u32 count at +4; the table the infinite soul cores use). If a d-pad slot equip lost a usable item, equip the core into
// slot 2 or 3 instead and move that d-pad shortcut onto it with the game's own shortcut call (step 6). No id list: every core.
// ids of the live usable-core list (0 = none); returns the count, -1 unreadable
static int ScwUsableIds(uint64_t inv, uint16_t* ids, int cap) {
    if (!inv) return -1;
    int n = 0;
    for (int i = 0; i < 1200; ++i) {
        uint16_t id = 0, base = 0; uint32_t c = 0;
        const uintptr_t a = (uintptr_t)(inv + 0x1E0AD0 + (uint64_t)i * 0xE8);
        if (!ScwRd16(a, &id) || !ScwRd16(a + 2, &base) || !ScwRd32(a + 4, &c)) return -1;
        if (id && id == base && c <= 999) { if (n < cap) ids[n] = id; ++n; }
    }
    return n < cap ? n : cap;
}
static int ScwHasNewId(const uint16_t* now, int nn, const uint16_t* was, int nw) {  // 1 = some id in now is not in was
    for (int i = 0; i < nn; ++i) { int f = 0; for (int j = 0; j < nw; ++j) if (was[j] == now[i]) { f = 1; break; } if (!f) return 1; }
    return 0;
}
static __attribute__((noinline)) int ScwApplyAs(int target, uint32_t idx, int32_t sc, int* stage) {
    uint64_t ctx = 0, inv = ScwInv(&ctx);
    if (!inv || !ctx) return SCW_NO_GAME;
    const uint64_t rec = inv + SCW_REC_OFF + (uint64_t)idx * SCW_REC_STRIDE;
    const uint8_t t = (uint8_t)target;
    __try {
        *stage = 21;
        if (!((ScwOkFn)(B + Grva(0x2285C48)))(t, rec)) return SCW_REFUSED;
        *stage = 23; ((ScwRecFn)(B + Grva(0x21607A4)))(ctx, rec);
        *stage = 24; ((ScwEqFn)(B + Grva(0x2183744)))(ctx, t, rec);
        *stage = 25; ((ScwCtxFn)(B + Grva(0x215F85C)))(ctx);
        *stage = 26; ((ScwShFn)(B + Grva(0x2182968)))(ctx, sc, t);
        *stage = 27; ((ScwSndFn)(B + Grva(0x34EDF8)))(0xF53E7B0Cu, 0);
        *(volatile uint8_t*)(B + Grva(0x45C6F2F)) = 1;
        *stage = SCW_DONE;
    } __except (EXCEPTION_EXECUTE_HANDLER) { Log("SOULCORE swap: fault %08lX at stage %d", GetExceptionCode(), *stage); return SCW_FAULT; }
    return SCW_DONE;
}
static int ScwApplyUsable(int slot, uint32_t idx, const uint32_t slots[SCW_NSLOT], int* stage) {
    uint16_t was[64], mid[64], now[64];
    const int nw = ScwUsableIds(ScwInv(0), was, 64);
    int r = ScwApply(slot, idx, slots, stage);
    if (r != SCW_DONE || slot > 1 || nw < 0) return r;
    const int nm = ScwUsableIds(ScwInv(0), mid, 64);
    if (nm < 0 || ScwHasNewId(mid, nm, was, nw)) return r;  // the core made its own usable item in this d-pad slot
    const int32_t sc = g_scw_sc_known[slot] ? g_scw_sc[slot] : -1;
    if (sc < 0) { Log("SOULCORE swap: idx %u has no usable item in slot %d (usable %d -> %d), no d-pad shortcut known", idx, slot, nw, nm); return r; }
    for (int k = 0; k < 2; ++k) {  // d-pad slot y -> slot 2 + y first (the overlay shows it there, #140454), then the other
        const int t = k == 0 ? 2 + slot : 3 - slot;
        int st = 0;
        const int r2 = ScwApplyAs(t, idx, sc, &st);
        const int nn = ScwUsableIds(ScwInv(0), now, 64);
        const int gained = nn > 0 && ScwHasNewId(now, nn, mid, nm);
        Log("SOULCORE swap: idx %u made no usable item in slot %d (usable %d -> %d); slot %d + d-pad shortcut %d -> code %d, usable %d, new id %s",
            idx, slot, nw, nm, t, sc, r2, nn, gained ? "yes" : "no");
        if (r2 == SCW_DONE && gained) return SCW_DONE;
    }
    return r;
}
// human #143767 ('F2 still cycles thru items as well as soul cores'): F2's equip. Same Yang-slot equip; when it makes no usable item the
// core is refused (F2 skips it) instead of the #140431 Yin slot + d-pad shortcut move, so F2 never changes slots 2-7 or the shortcuts.
// live 11-28 07:11 (83 run try2): the equip of 1515 (in Yang slot 1) into slot 0 also moved the d-pad shortcuts (slot 0: shortcut 3 -> 2),
// so after the slot put-back DOWN no longer pointed at slot 0 and 19 cores never threw. The refusal also gives each slot its shortcut back.
static int32_t ScwShortcutOf(int slot) {  // the game's own lookup (+216B9D0), -1 = none / unreadable
    uint64_t ctx = 0; if (!ScwInv(&ctx) || !ctx) return -1;
    int32_t sc = -1;
    __try { sc = ((ScwScFn)(B + Grva(0x216B9D0)))(ctx, (uint8_t)slot); } __except (EXCEPTION_EXECUTE_HANDLER) { sc = -1; }
    return sc;
}
static int ScwShortcutSet(int slot, int32_t sc) {  // the game's own shortcut move (+2182968), as ScwApply step 6
    uint64_t ctx = 0; if (!ScwInv(&ctx) || !ctx || sc < 0) return 0;
    __try {
        ((ScwCtxFn)(B + Grva(0x215F85C)))(ctx);
        ((ScwShFn)(B + Grva(0x2182968)))(ctx, sc, (uint8_t)slot);
        *(volatile uint8_t*)(B + Grva(0x45C6F2F)) = 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    return 1;
}
static int ScwApplyYang(int slot, uint32_t idx, const uint32_t slots[SCW_NSLOT], int* stage) {
    uint16_t was[64], mid[64];
    const int nw = ScwUsableIds(ScwInv(0), was, 64);
    int32_t scb[2] = {ScwShortcutOf(0), ScwShortcutOf(1)};  // Yang slots' d-pad shortcuts before the equip
    const int r = ScwApply(slot, idx, slots, stage);
    if (r != SCW_DONE || slot > 1 || nw < 0) return r;
    const int nm = ScwUsableIds(ScwInv(0), mid, 64);
    if (nm < 0 || ScwHasNewId(mid, nm, was, nw)) return r;
    Log("SOULCORE swap: F2 idx %u made no usable item in Yang slot %d (usable %d -> %d): refused, Yin slots and d-pad left alone (human #143767)", idx, slot, nw, nm);
    // human #144438/#144581 ('F2 still rarely cycling items'): the refused core stayed equipped in the Yang slot, so that slot held a core with
    // no usable item and the d-pad showed the next item. Put the core that was there back (the game's own equip), so a refused core never
    // leaves the slot changed. An empty slot before stays as the game left it.
    if (slots[slot] != SCW_EMPTY && slots[slot] != idx) {
        int st = 0;
        const int rb = ScwApply(slot, slots[slot], slots, &st);
        uint16_t now[64]; const int nn = ScwUsableIds(ScwInv(0), now, 64);
        Log("SOULCORE swap: F2 put idx %u back in Yang slot %d after the refusal: code %d, usable %d (was %d) (human #144438)", slots[slot], slot, rb, nn, nw);
    }
    // live 11-28 06:58 (83-core run, core 1515 in Yang slot 1): the game's equip into slot 0 MOVED the core out of slot 1, so putting slot 0
    // back left slot 1 empty (usable 3 -> 2) and the next 3 cores never threw. A refused core leaves every slot as it was: each other slot
    // the equip changed gets its own core back with the same game equip.
    const uint64_t inv = ScwInv(0);
    if (inv) {
        for (int o = 0; o < SCW_NSLOT; ++o) {
            if (o == slot || slots[o] == SCW_EMPTY) continue;
            uint32_t cur[SCW_NSLOT];
            int ok = 1;
            for (int q = 0; q < SCW_NSLOT && ok; ++q) ok = ScwRd32((uintptr_t)(inv + SCW_SLOTS_OFF + 4u * q), &cur[q]);
            if (!ok || cur[o] == slots[o]) continue;
            int st = 0;
            const int ro = ScwApply(o, slots[o], cur, &st);
            uint16_t now[64]; const int nn = ScwUsableIds(ScwInv(0), now, 64);
            Log("SOULCORE swap: F2 refusal also changed slot %d (idx %d, was %u): put back, code %d, usable %d (was %d) (human #144438)", o,
                cur[o] == SCW_EMPTY ? -1 : (int)cur[o], slots[o], ro, nn, nw);
        }
    }
    for (int o = 0; o < 2; ++o) {
        const int32_t now = ScwShortcutOf(o);
        if (scb[o] < 0 || now == scb[o]) continue;
        const int ok = ScwShortcutSet(o, scb[o]);
        Log("SOULCORE swap: F2 refusal moved slot %d's d-pad shortcut %d -> %d: put back (%s), now %d (human #144438)", o, scb[o], now,
            ok ? "ok" : "failed", ScwShortcutOf(o));
        if (ok) { g_scw_sc[o] = scb[o]; g_scw_sc_known[o] = 1; }
    }
    return SCW_REFUSED;
}
static __attribute__((noinline)) int ScwRun(int slot, uint32_t idx, int* stage) {
    uint32_t slots[SCW_NSLOT], oidx[64]; uint16_t oit[64];
    const int n = ScwRead(slots, oidx, oit, 64);
    if (n < 0) return SCW_NO_GAME;
    const int chk = ScwCheck(slot, idx, slots, oidx, n);
    if (chk) return chk;
    return ScwApplyUsable(slot, idx, slots, stage);
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
    return g_scw_yang_only ? ScwApplyYang(slot, idx, slots, stage) : ScwApplyUsable(slot, idx, slots, stage);
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
    InterlockedExchange(&g_scw_by_item, 0); InterlockedExchange(&g_scw_yang_only, 0);
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
    InterlockedExchange(&g_scw_by_item, 1); InterlockedExchange(&g_scw_yang_only, 0);
    InterlockedExchange(&g_scw_result, SCW_PENDING);
    InterlockedExchange(&g_scw_req, 1);
    Log("SOULCORE swap: menu asks slot %d item %X (human #136123)", slot, itemId);
    return 1;
}
// human #143767: F2's equip by item id, Yang slots 0-1 only (no Yin slot / d-pad fallback; such a core is refused). 1 queued, 0 busy, -1 n/a.
N3T_API int Nioh3Training_SoulCoreEquipItemYang(int slot, int itemId) {
    if (!ScwResolved()) return -1;
    if (slot < 0 || slot > 1 || itemId < 0 || itemId > 0xFFFF) return -1;
    if (g_scw_req) return 0;
    InterlockedExchange(&g_scw_slot, slot); InterlockedExchange(&g_scw_idx, itemId);
    InterlockedExchange(&g_scw_by_item, 1); InterlockedExchange(&g_scw_yang_only, 1);
    InterlockedExchange(&g_scw_result, SCW_PENDING);
    InterlockedExchange(&g_scw_req, 1);
    Log("SOULCORE swap: F2 asks Yang slot %d item %X (human #143767)", slot, itemId);
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
