// Clear ground item pickups on a Boss Match rematch (human #65174).
// Item::CollectAllDropItemInAreaMission (the script binding) walks one global drop manager and, for each
// object whose type word masked with ~0xF is 0x10, collects it. Item::EraseDropItemInArea walks that same
// list and only marks a box. A fresh fight has no such objects because the area reloaded. An in-place
// rematch does not reload, so the pickups stay. This calls the manager's own remove (the function the
// list uses to free one object) for every type-0x10 object, which does not grant the item.
// All four sites are found by byte pattern. Nothing here is a fixed address.
#include "addon.h"

#include <cstdint>
#include <cstring>

namespace {

struct Iter {
    uint64_t cur;
    uint64_t sent;
    uint64_t bucket;
    uint64_t bucketEnd;
};

using GetFn = void(__fastcall*)(Iter*);
using BindFn = Iter*(__fastcall*)(Iter*, uint64_t, uint64_t);
using NextFn = void(__fastcall*)(Iter*);
using KillFn = void(__fastcall*)(void*, void*);

struct Pat {
    const uint8_t* b;
    const uint8_t* m;  // 1 = must match
    size_t n;
};

bool MatchAt(const uint8_t* p, const Pat& pt) {
    for (size_t i = 0; i < pt.n; ++i)
        if (pt.m[i] && p[i] != pt.b[i]) return false;
    return true;
}

int Scan(const Pat& pt, const uint8_t** hit, int cap) {
    const HMODULE game = GetModuleHandleW(L"Nioh3.exe");
    if (!game || cap <= 0) return -1;
    const auto* dos = (const IMAGE_DOS_HEADER*)game;
    const auto* nt = (const IMAGE_NT_HEADERS64*)((const uint8_t*)game + dos->e_lfanew);
    const auto* sh = IMAGE_FIRST_SECTION(nt);
    int n = 0;
    for (int i = 0; i < nt->FileHeader.NumberOfSections && n < cap; ++i) {
        if (!(sh[i].Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        const auto* lo = (const uint8_t*)game + sh[i].VirtualAddress;
        const auto* hi = lo + sh[i].Misc.VirtualSize;
        if (hi < lo + pt.n) continue;
        for (const uint8_t* cur = lo; cur + pt.n <= hi && n < cap;) {
            const void* f = memchr(cur, pt.b[0], (size_t)(hi - cur) - pt.n + 1);
            if (!f) break;
            cur = (const uint8_t*)f;
            if (MatchAt(cur, pt)) hit[n++] = cur;
            ++cur;
        }
    }
    return n;
}

// Drop-list head. The rel32 at +10 is the manager global (mov rcx, [rip+disp]).
// 48 83 EC 28 48 8B D1 48 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 8B C2 48 83 C4 28 C3
const uint8_t kGetB[] = {
    0x48, 0x83, 0xEC, 0x28, 0x48, 0x8B, 0xD1, 0x48, 0x8B, 0x0D, 0, 0, 0, 0, 0xE8, 0, 0, 0, 0, 0x48, 0x8B, 0xC2, 0x48, 0x83, 0xC4, 0x28, 0xC3};
const uint8_t kGetM[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1};

// Bind the iterator the collect walk uses (and qword [rcx],0 / [rcx+8],0; store rdx, r8; cur = *rdx).
const uint8_t kBindB[] = {
    0x48, 0x83, 0x21, 0x00, 0x48, 0x83, 0x61, 0x08, 0x00, 0x48, 0x89, 0x51, 0x10, 0x4C, 0x89, 0x41, 0x18, 0x49, 0x3B, 0xD0, 0x74, 0x06, 0x48, 0x8B, 0x02, 0x48, 0x89, 0x01, 0x48, 0x8B, 0xC1, 0xC3};
const uint8_t kBindM[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

// Next node. The game has several identical copies of this leaf; any one is the same walk.
const uint8_t kNextB[] = {
    0x48, 0x8B, 0x01, 0x45, 0x33, 0xC0, 0x48, 0x8B, 0x10, 0x48, 0x89, 0x11, 0x48, 0x8B, 0x41, 0x08, 0x48, 0x3B, 0xD0, 0x74, 0x01, 0xC3,
    0x48, 0x8B, 0x51, 0x10, 0x48, 0x83, 0xC2, 0x10, 0x48, 0x89, 0x51, 0x10, 0x48, 0x3B, 0x51, 0x18, 0x74, 0x0F, 0x48, 0x8B, 0x12, 0x49, 0x8B, 0xC0,
    0x48, 0x89, 0x11, 0x4C, 0x89, 0x41, 0x08, 0xEB, 0xD9, 0x4C, 0x89, 0x41, 0x08, 0x4C, 0x89, 0x01, 0xC3};
const uint8_t kNextM[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

// Manager remove. rdx is the object base (type word at +0); the body adds 0xE0 to reach the list node.
// 48 85 D2 0F 84 ?? ?? ?? ?? 53 57 48 83 EC 28 44 8B 02 48 8B DA 41 83 E0 F0 ... 48 81 C2 E0 00 00 00
const uint8_t kKillB[] = {
    0x48, 0x85, 0xD2, 0x0F, 0x84, 0, 0, 0, 0, 0x53, 0x57, 0x48, 0x83, 0xEC, 0x28, 0x44, 0x8B, 0x02, 0x48, 0x8B, 0xDA, 0x41, 0x83, 0xE0, 0xF0,
    0x48, 0x89, 0x6C, 0x24, 0x40, 0x41, 0xC1, 0xE0, 0x0C, 0x48, 0x8B, 0xF9, 0x44, 0x0B, 0x42, 0x04, 0x48, 0x81, 0xC2, 0xE0, 0x00, 0x00, 0x00};
const uint8_t kKillM[] = {
    1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
static_assert(sizeof kGetB == sizeof kGetM, "get pattern");
static_assert(sizeof kBindB == sizeof kBindM, "bind pattern");
static_assert(sizeof kNextB == sizeof kNextM, "next pattern");
static_assert(sizeof kKillB == sizeof kKillM, "kill pattern");

struct Fns {
    GetFn get = nullptr;
    BindFn bind = nullptr;
    NextFn next = nullptr;
    KillFn kill = nullptr;
    uint64_t mgrSlot = 0;
    bool ok = false;
};

// Call sites of `target` in the game's code, each preceded by `mov rcx, [rip+disp]` (48 8B 0D rel32): stores the
// global each one loads into rcx. The drop manager's own remove is always called on the drop manager's global.
int RcxGlobalsBeforeCall(const uint8_t* target, uint64_t* out, int cap) {
    const HMODULE game = GetModuleHandleW(L"Nioh3.exe");
    if (!game || cap <= 0) return 0;
    const auto* dos = (const IMAGE_DOS_HEADER*)game;
    const auto* nt = (const IMAGE_NT_HEADERS64*)((const uint8_t*)game + dos->e_lfanew);
    const auto* sh = IMAGE_FIRST_SECTION(nt);
    int n = 0;
    for (int i = 0; i < nt->FileHeader.NumberOfSections && n < cap; ++i) {
        if (!(sh[i].Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        const auto* lo = (const uint8_t*)game + sh[i].VirtualAddress;
        const auto* hi = lo + sh[i].Misc.VirtualSize;
        for (const uint8_t* cur = lo + 7; cur + 5 <= hi && n < cap;) {
            const void* f = memchr(cur, 0xE8, (size_t)(hi - cur) - 4);
            if (!f) break;
            cur = (const uint8_t*)f;
            int32_t rel;
            memcpy(&rel, cur + 1, 4);
            if (cur + 5 + rel == target && cur[-7] == 0x48 && cur[-6] == 0x8B && cur[-5] == 0x0D) {
                int32_t d;
                memcpy(&d, cur - 4, 4);
                out[n++] = (uint64_t)cur + d;  // rip after the 7-byte mov = cur
            }
            ++cur;
        }
    }
    return n;
}

Fns Resolve() {
    Fns f;
    const uint8_t* hit[8] = {};

    // The remove is unique; it decides which drop-list head is the drop manager's (pc #65944: the head shape
    // matches twice in the 11-04 game, 0x320000 and 0x725CA4; only the second one's manager is ever passed to remove).
    const Pat kill{kKillB, kKillM, sizeof kKillB};
    const int nk = Scan(kill, hit, 8);
    if (nk != 1) {
        addon::Log("Rematch loot: remove pattern matched %d times (want 1) - clear off", nk);
        return f;
    }
    f.kill = (KillFn)hit[0];
    uint64_t killMgr[32] = {};
    const int nkm = RcxGlobalsBeforeCall(hit[0], killMgr, 32);

    const Pat get{kGetB, kGetM, sizeof kGetB};
    const int ng = Scan(get, hit, 8);
    if (ng < 1) {
        addon::Log("Rematch loot: drop-list pattern matched 0 times - clear off");
        return f;
    }
    int pick = -1, picks = 0;
    for (int i = 0; i < ng; ++i) {
        int32_t disp = 0;
        if (!addon::Rd((uint64_t)(hit[i] + 10), disp)) continue;
        const uint64_t slot = (uint64_t)(hit[i] + 14) + disp;
        int uses = 0;
        for (int k = 0; k < nkm; ++k) uses += killMgr[k] == slot;
        if (uses > 0) { pick = i; ++picks; f.mgrSlot = slot; }
    }
    if (picks != 1) {
        addon::Log("Rematch loot: drop-list pattern matched %d times, %d of them are the remove's manager (want 1) - clear off", ng, picks);
        f.mgrSlot = 0;
        return f;
    }
    f.get = (GetFn)hit[pick];

    // The bind and next leaves are fully fixed bytes ending in ret; the game has identical copies, any one is the same code.
    const Pat bind{kBindB, kBindM, sizeof kBindB};
    const int nb = Scan(bind, hit, 8);
    if (nb < 1) {
        addon::Log("Rematch loot: iterator bind pattern matched 0 times - clear off");
        return f;
    }
    f.bind = (BindFn)hit[0];

    const Pat next{kNextB, kNextM, sizeof kNextB};
    const int nn = Scan(next, hit, 8);
    if (nn < 1) {
        addon::Log("Rematch loot: iterator next pattern matched 0 times - clear off");
        return f;
    }
    f.next = (NextFn)hit[0];
    f.ok = true;
    addon::Log("Rematch loot: clear ready (drop-list head %d of %d picked by the remove's manager, %d remove calls on it; bind x%d, next x%d identical copies)",
               pick + 1, ng, nkm, nb, nn);
    return f;
}

int ClearOnce(const Fns& f, void* mgr) {
    Iter raw{};
    f.get(&raw);
    Iter it{};
    f.bind(&it, raw.cur, raw.sent);
    void* objs[64];
    int n = 0;
    for (int steps = 0; it.cur && steps < 512; ++steps) {
        const uint64_t p = it.cur;
        f.next(&it);
        uint32_t ty = 0;
        if (!addon::Rd(p - 0xE0, ty)) continue;
        if ((ty & 0xFFFFFFF0u) != 0x10u) continue;
        if (n < 64) objs[n++] = (void*)(p - 0xE0);
    }
    int killed = 0;
    for (int i = 0; i < n; ++i) {
        uint32_t ty = 0;
        if (!addon::Rd((uint64_t)objs[i], ty) || (ty & 0xFFFFFFF0u) != 0x10u) continue;
        f.kill(mgr, objs[i]);
        ++killed;
    }
    return killed;
}

}  // namespace

extern "C" __declspec(dllexport) int Nioh3BossRematch_ClearDroppedLoot(void) {
    static Fns f;
    static int ready = 0;
    if (!ready) {
        ready = 1;
        f = Resolve();
    }
    if (!f.ok) return -1;
    void* mgr = nullptr;
    if (!addon::Rd(f.mgrSlot, mgr) || !mgr) return 0;
    int n = 0;
    __try {
        n = ClearOnce(f, mgr);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        addon::Log("Rematch loot: clear faulted %08lX", GetExceptionCode());
        return -2;
    }
    return n;
}
