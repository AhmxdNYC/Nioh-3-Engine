// keepgear.cpp - site report #7 (human #65467): a mod Fight-now trip into a sandbox mission (tutorial, dojo) must keep the player's own
// items and gear. Those missions' scripts call Item::ReserveAllItem* (stash every item), Item::ClearEquipment, then give the mission's
// own kit (AddEquipment*, AddFixItem, AddItemNoNotice, AddMagicItem, SetShortCut, Player::EquipNinjutsu, AddSkill*). While
// Nioh3Training_KeepGear() is 1 (a Fight-now trip of the mod is running) those script natives are skipped (hooks.cpp InstallVeto):
//   * strip natives (reserve / clear equipment): always skipped during a trip; the first one marks the trip as a sandbox trip.
//   * kit natives: skipped only once the trip is a sandbox trip, so a normal boss mission's own script rewards are untouched.
// Generic: decided by what the mission script calls, no boss / mission / world lists.
// Safety net (pump): if the game's reserve flag went up during a trip anyway (a path we did not veto), the game's own restore
// (what Item::ResetReserveItem does) is run when the trip ends, so the stashed items come back.
#include "addon.h"
#include "sites_gen.h"

#include <atomic>
#include <cstdio>
#include <cstring>

namespace addon {
int SiteIndex(const char* name);
namespace keepgear {
namespace {

const char* const kStrip[] = {"Item::ReserveAllItemAll", "Item::ReserveAllItemDojo", "Item::ReserveAllItemEdo", "Item::ClearEquipment"};
const char* const kKit[] = {"Item::AddEquipment", "Item::AddEquipmentBySlot", "Item::AddEquipmentPreset", "Item::AddFixItem",
                            "Item::AddItem",      "Item::AddItemNoNotice",    "Item::AddMagicItem",       "Item::SetShortCut",
                            "Player::EquipNinjutsu", "Player::AddSkill",      "Player::AddSkillDojo"};
constexpr int kNStrip = sizeof(kStrip) / sizeof(kStrip[0]), kNKit = sizeof(kKit) / sizeof(kKit[0]);

std::atomic<int> g_sandbox{0}, g_skipped{0}, g_wasTrip{0}, g_reservedAtStart{-1};
// #65732 safety net: every inventory add during a trip (InventoryAddItem, where all script / engine adds end), logged with its id at the trip end
std::atomic<int> g_added{0};
uint16_t g_addedIds[16];
// #66083: ids the player already owns are not new items. Every add seen outside a trip, and every id in a world-load
// inventory re-sync burst (>= kBurst adds within 0.5 s), is marked owned; later adds of an owned id are ignored.
constexpr int kBurst = 8, kPendCap = 256;
uint8_t g_owned[65536 / 8];
uint16_t g_pend[kPendCap];
std::atomic<int> g_pendN{0}, g_resync{0};
std::atomic<DWORD> g_pendLast{0};
bool Owned(uint16_t id) { return (g_owned[id >> 3] >> (id & 7)) & 1; }
void Own(uint16_t id) { g_owned[id >> 3] |= (uint8_t)(1u << (id & 7)); }
void Flush(bool force) {  // game thread (pump): classify the pending adds once they have settled
    const int n = g_pendN.load();
    if (!n || (!force && GetTickCount() - g_pendLast.load() < 500)) return;
    const int m = n < kPendCap ? n : kPendCap;
    if (n >= kBurst) {
        for (int i = 0; i < m; ++i) Own(g_pend[i]);
        g_resync.fetch_add(n);
    } else {
        for (int i = 0; i < m; ++i) {
            const uint16_t id = g_pend[i];
            if (Owned(id)) continue;
            Own(id);
            const int k = g_added.fetch_add(1);
            if (k < 16) g_addedIds[k] = id;
            Log("KeepGear: WARNING a new item (id %u) was added to your inventory during the trip (not a vetoed script call)", id);
        }
    }
    g_pendN.store(0);
}
int (*g_trip)() = nullptr;

// game reserve flag + restore, parsed from Item::ResetReserveItem (mov rcx,[g1]; call IsReserved; test ax,ax; jz; ... mov rcx,[g2]; call Restore)
uint64_t g_isRes = 0, g_isResArg = 0, g_restore = 0, g_restoreArg = 0;

int TripLevel() {  // 0 no trip, 1 mod Fight-now trip, 2 trip into a world whose scripts strip items / gear (Training's game-data scan)
    if (!g_trip) {
        HMODULE m = GetModuleHandleW(L"Nioh3Training.asi");
        if (m) g_trip = (int (*)())GetProcAddress(m, "Nioh3Training_KeepGear");
        if (!g_trip) return 0;
    }
    return g_trip();
}
bool Trip() { return TripLevel() != 0; }
const char* Name(int site) { return site >= 0 && site < sites::kCount ? sites::kDefs[site].name : "?"; }

bool VetoStrip(int site) {
    if (!Trip()) return false;
    if (!g_sandbox.exchange(1)) Log("KeepGear: sandbox mission script detected (%s) - its item/gear changes are skipped this trip", Name(site));
    g_skipped.fetch_add(1);
    Log("KeepGear: skipped script %s (your items and gear stay)", Name(site));
    return true;
}
bool VetoKit(int site) {
    const int lvl = TripLevel();
    if (!lvl || (!g_sandbox.load() && lvl < 2)) return false;  // #65732: a sandbox world vetoes the kit from the first call (kit can come before the strip)
    g_skipped.fetch_add(1);
    Log("KeepGear: skipped script %s (sandbox kit not given)", Name(site));
    return true;
}

uint64_t RipTarget(const uint8_t* b, uint64_t at, int len) {  // [rip+disp32] whose disp is the last 4 bytes of a len-byte instruction
    int32_t d = 0;
    memcpy(&d, b + len - 4, 4);
    return at + len + (int64_t)d;
}
uint64_t Call(const uint8_t* b, uint64_t at) {
    if (b[0] != 0xE8) return 0;
    int32_t r = 0;
    memcpy(&r, b + 1, 4);
    const uint64_t t = at + 5 + (int64_t)r;
    return t > g_img.base + 0x1000 && t < g_img.end ? t : 0;
}
void ParseRestore() {
    const uint64_t fn = Site("Item::ResetReserveItem");
    uint8_t b[0x120];
    if (!fn || !Copy(b, fn, sizeof(b))) return;
    for (int i = 0; i + 40 < (int)sizeof(b); ++i) {
        // 48 8B 0D d32 | E8 r32 | 66 85 C0 | 74 xx
        if (b[i] != 0x48 || b[i + 1] != 0x8B || b[i + 2] != 0x0D || b[i + 7] != 0xE8 || b[i + 12] != 0x66 || b[i + 13] != 0x85 ||
            b[i + 14] != 0xC0 || b[i + 15] != 0x74)
            continue;
        const uint64_t g1 = RipTarget(b + i, fn + i, 7), f1 = Call(b + i + 7, fn + i + 7);
        for (int j = i + 17; j + 12 < (int)sizeof(b); ++j) {
            if (b[j] != 0x48 || b[j + 1] != 0x8B || b[j + 2] != 0x0D || b[j + 7] != 0xE8) continue;
            const uint64_t g2 = RipTarget(b + j, fn + j, 7), f2 = Call(b + j + 7, fn + j + 7);
            if (g1 && f1 && g2 && f2) { g_isResArg = g1; g_isRes = f1; g_restoreArg = g2; g_restore = f2; }
            return;
        }
        return;
    }
}
int Reserved() {  // 1 / 0, -1 = unknown
    if (!g_isRes) return -1;
    uint64_t a = 0;
    if (!Copy(&a, g_isResArg, 8) || !a) return -1;
    __try {
        return ((int16_t (*)(uint64_t))g_isRes)(a) != 0 ? 1 : 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return -1;
    }
}

void HkAdd(uint64_t, uint64_t, uint64_t desc, uint64_t, const uint64_t*) {
    uint16_t id = 0;
    if (!Copy(&id, desc, 2)) return;
    if (!Trip()) {  // outside a trip every add is the player's own item
        Own(id);
        return;
    }
    if (Owned(id)) return;  // already owned (#66083: the inventory re-sync on every world load re-adds the same ids)
    const int n = g_pendN.fetch_add(1);
    if (n < kPendCap) g_pend[n] = id;
    g_pendLast.store(GetTickCount());
}

}  // namespace

void Install() {
    if (!IniInt(L"Rematch", L"KeepGearInArenas", 1)) {
        Log("KeepGear: off ([Rematch] KeepGearInArenas = 0) - sandbox arenas (tutorial / dojo) can take your items and gear");
        return;
    }
    int ok = 0, n = 0;
    for (int i = 0; i < kNStrip; ++i) {
        const char* why = nullptr;
        ++n;
        if (InstallVeto(kStrip[i], &VetoStrip, &why)) ++ok;
        else Log("KeepGear: %s not protected - %s", kStrip[i], why ? why : "?");
    }
    for (int i = 0; i < kNKit; ++i) {
        const char* why = nullptr;
        ++n;
        if (InstallVeto(kKit[i], &VetoKit, &why)) ++ok;
        else Log("KeepGear: %s not protected - %s", kKit[i], why ? why : "?");
    }
    ParseRestore();
    {
        const char* why = nullptr;
        if (!InstallEntry("InventoryAddItem", &HkAdd, &why)) Log("KeepGear: item-add watch off - %s", why ? why : "?");
    }
    Log("KeepGear: %d/%d script natives guarded during mod arena trips; reserve safety net %s", ok, n, g_isRes ? "on" : "OFF (ResetReserveItem layout not recognised)");
}

// Per frame (shared pump, game thread): trip start/end bookkeeping + the reserve safety net.
void Tick() {
    static DWORD last = 0;
    const DWORD now = GetTickCount();
    if (now - last < 250) return;
    last = now;
    Flush(false);
    const int trip = Trip() ? 1 : 0;
    const int was = g_wasTrip.exchange(trip);
    if (trip && !was) {
        g_reservedAtStart.store(Reserved());  // g_sandbox / g_skipped are reset at the trip end (a veto can come before this tick)
    } else if (!trip && was) {
        const int r = Reserved();
        Log("KeepGear: trip ended - %d script item/gear calls skipped%s, reserve flag start %d now %d", g_skipped.load(),
            g_sandbox.load() ? " (sandbox mission)" : "", g_reservedAtStart.load(), r);
        if (r == 1 && g_reservedAtStart.load() == 0 && g_restore) {
            uint64_t a = 0;
            if (Copy(&a, g_restoreArg, 8) && a) {
                __try {
                    ((void (*)(uint64_t))g_restore)(a);
                    Log("KeepGear: SAFETY NET - items were stashed during the trip; the game's own restore put them back (now %d)", Reserved());
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    Log("KeepGear: SAFETY NET restore raised an exception");
                }
            }
        }
        Flush(true);
        if (const int rs = g_resync.exchange(0)) Log("KeepGear: %d inventory re-sync add(s) of items you already own ignored this trip", rs);
        if (const int a = g_added.load()) {
            char ids[16 * 7 + 1] = "";
            for (int k = 0, o = 0; k < a && k < 16; ++k) o += snprintf(ids + o, sizeof(ids) - o, "%s%u", k ? "," : "", g_addedIds[k]);
            Log("KeepGear: SAFETY NET - %d item(s) were added during the trip (ids %s); they stay in the inventory (no removal yet)", a, ids);
        }
        g_added.store(0);
        g_sandbox.store(0);
        g_skipped.store(0);
    }
}

}  // namespace keepgear
}  // namespace addon
