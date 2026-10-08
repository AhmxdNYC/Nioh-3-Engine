// bosslog.cpp - BossLog ([Debug] BossLog = 0 by default). See bosslog.h. Read-only: every hook runs the logger, then the
// original instructions, with all argument registers intact.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "bosslog.h"

namespace bosslog {
namespace {

void (*g_log)(const char*, ...) = nullptr;
uintptr_t g_base = 0;
double g_qpcSec = 0;
uint64_t g_qpc0 = 0;

uint64_t Qpc() {
    LARGE_INTEGER q;
    QueryPerformanceCounter(&q);
    return (uint64_t)q.QuadPart;
}
double Now() { return (double)(Qpc() - g_qpc0) * g_qpcSec; }

// ---- state (fights are sequential; the script thread runs all of these) ----
std::atomic<int> g_fight{0};
std::atomic<bool> g_active{false};
std::atomic<uint64_t> g_t0{0};
std::atomic<double> g_endAt{-1e9};
std::atomic<int> g_flagCount{0};
std::atomic<uint32_t> g_lastMission{0};

constexpr int kRing = 16;
struct RingEnt {
    std::atomic<uint32_t> id{0};
    std::atomic<double> t{-1e9};
};
RingEnt g_ring[kRing];
std::atomic<uint32_t> g_ringN{0};

bool InFightOrGrace() { return g_active.load() || Now() - g_endAt.load() < 20.0; }
double FightSec() { return (double)(Qpc() - g_t0.load()) * g_qpcSec; }

// ---- loggers: rcx,rdx,r8,r9 are the hooked function's own arguments ----
void HkBegin(uint64_t ctx, uint64_t, uint64_t, uint64_t) {
    const int n = g_fight.fetch_add(1) + 1;
    const double t = Now();
    g_t0.store(Qpc());
    g_flagCount.store(0);
    g_active.store(true);
    g_log("BossLog #%d: BossBattleMode start t=%.3f s (script ctx %p, last mission-clear id %u)", n, t, (void*)ctx,
          g_lastMission.load());
    char buf[256];
    size_t bl = 0;
    const uint32_t rn = g_ringN.load();
    for (uint32_t i = rn > kRing ? rn - kRing : 0; i < rn && bl + 40 < sizeof(buf); ++i) {
        const RingEnt& e = g_ring[i % kRing];
        const double age = t - e.t.load();
        if (age < 0 || age > 60.0) continue;
        bl += (size_t)snprintf(buf + bl, sizeof(buf) - bl, " %u(-%.1fs)", e.id.load(), age);
    }
    g_log("BossLog #%d: Character::Create placement ids in the last 60 s:%s", n, bl ? buf : " none");
}

void HkEnd(uint64_t, uint64_t, uint64_t, uint64_t) {
    const double t = Now();
    const double dur = g_active.load() ? FightSec() : 0.0;
    g_log("BossLog #%d: BossBattleModeEnd t=%.3f s, fight %.1f s, %d mission flag sets inside", g_fight.load(), t, dur,
          g_flagCount.load());
    g_endAt.store(t);
    g_active.store(false);
}

void HkCreate(uint64_t placementId, uint64_t arg2, uint64_t arg3, uint64_t arg4) {
    const uint32_t i = g_ringN.fetch_add(1);
    RingEnt& e = g_ring[i % kRing];
    e.id.store((uint32_t)placementId);
    e.t.store(Now());
    if (g_active.load())
        g_log("BossLog #%d: Character::Create placement %u (arg2 %u, flag %u, arg4 %u) t=%.3f s +%.1f s", g_fight.load(),
              (uint32_t)placementId, (uint32_t)arg2, (uint32_t)(uint8_t)arg3, (uint32_t)arg4, Now(), FightSec());
}

void HkFlag(uint64_t, uint64_t flagId, uint64_t val, uint64_t) {
    if (!InFightOrGrace()) return;
    g_flagCount.fetch_add(1);
    g_log("BossLog #%d: SetMissionFlag id %u value %u t=%.3f s (%s)", g_fight.load(), (uint32_t)flagId, (uint32_t)(uint8_t)val,
          Now(), g_active.load() ? "in fight" : "after fight");
}

void HkGauge(uint64_t actorId, uint64_t, uint64_t, uint64_t) {
    g_log("BossLog #%d: ShowBossGauge actor id %u t=%.3f s%s", g_fight.load(), (uint32_t)actorId, Now(),
          g_active.load() ? "" : " (outside BossBattleMode)");
}

void HkClear(uint64_t missionId, uint64_t, uint64_t, uint64_t) {
    g_lastMission.store((uint32_t)missionId);
    g_log("BossLog #%d: SetMissionClearFlag mission id %u t=%.3f s (%s)", g_fight.load(), (uint32_t)missionId, Now(),
          InFightOrGrace() ? "fight window" : "no fight window");
}

// ---- reward grants (1.7.20) ----
// The reward natives are script bindings: rcx = the script VM context, the arguments live on the VM value stack and are
// popped by the wrapper's own argument reader (0x139938 -> 0x13BDB0). The hook runs BEFORE the original, so the args are
// still on the stack; they are read straight from memory (no game call, no mutation): vm = [ctx+8]; nargs = u16
// [[vm+8]+0xA6]; top = u16 [[vm+0x20]]; entry k = [vm+0x18] + (top - nargs + k) * 8 = {u32 value, u16 type}. Types as in
// 0x13BDB0: 1 int, 2 float, 3 byte/bool, 4 object pointer (not printed), other raw. Every read is SEH-guarded.
// Item/guardian/skill ids are logged as ids: the id -> text-id mapping is not known, so TextGetById (0x5A4B04) is not used.
constexpr double kRewardGrace = 30.0;
bool RewardWindow() { return g_active.load() || Now() - g_endAt.load() < kRewardGrace; }

const char* const kRewardNames[] = {
    "Character::AddDropTable",
    "Character::AddDropFixItem",
    "Character::AddDropKeyItemDirectGet",
    "Character::ForceDropAmurita",
    "Character::ForceDropItem",
    "Character::ForceDropItemAndGet",
    "Character::EnemyForceKillWithItemDrop",
    "Item::AddItem",
    "Item::AddFixItem",
    "Item::AddMagicItem",
    "Item::AddItemNoNotice",
    "Item::AddEquipment",
    "Item::AddEquipmentBySlot",
    "Item::AddEquipmentPreset",
    "Player::AddGuardianSpirit",
    "Player::AddGuardianSpiritNoNotice",
    "Player::AddGuardianSpiritWithTimer",
    "Player::AddSkill",
    "Player::AddSkillDojo",
    "Player::AddMoney",
    "Mission::SetReward",
    "Mission::GetGameClearRewardNormal",
    "Mission::GetGameClearRewardHard",
};

int ReadVmArgs(uint64_t ctx, uint32_t* val, uint16_t* ty, int maxn) {
    __try {
        const uint64_t vm = *(const uint64_t*)(ctx + 8);
        const uint64_t x = *(const uint64_t*)(vm + 8);
        const int nargs = *(const uint16_t*)(x + 0xA6);
        const uint64_t stk = *(const uint64_t*)(vm + 0x20);
        const int top = *(const uint16_t*)stk;
        const uint64_t arr = *(const uint64_t*)(vm + 0x18);
        if (nargs > 32 || top < nargs) return -1;
        const int base = top - nargs;
        for (int i = 0; i < nargs && i < maxn; ++i) {
            val[i] = *(const uint32_t*)(arr + (uint64_t)(base + i) * 8);
            ty[i] = *(const uint16_t*)(arr + (uint64_t)(base + i) * 8 + 4);
        }
        return nargs;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return -1;
    }
}

void LogReward(int id, uint64_t ctx) {
    if (!RewardWindow()) return;
    uint32_t val[8] = {0};
    uint16_t ty[8] = {0};
    const int n = ReadVmArgs(ctx, val, ty, 8);
    char buf[200];
    size_t bl = 0;
    for (int i = 0; i < n && i < 8 && bl + 24 < sizeof(buf); ++i) {
        if (ty[i] == 2) {
            float f;
            memcpy(&f, &val[i], 4);
            bl += (size_t)snprintf(buf + bl, sizeof(buf) - bl, " a%d=%g", i, (double)f);
        } else if (ty[i] == 4) {
            bl += (size_t)snprintf(buf + bl, sizeof(buf) - bl, " a%d=<obj>", i);
        } else {
            bl += (size_t)snprintf(buf + bl, sizeof(buf) - bl, " a%d=%u%s", i, val[i], ty[i] == 1 ? "" : (ty[i] == 3 ? "(b)" : "(?)"));
        }
    }
    g_log("BossLog #%d: REWARD %s nargs=%d%s t=%.3f s (%s +%.1f s)", g_fight.load(), kRewardNames[id], n, n > 0 ? buf : "", Now(),
          g_active.load() ? "in fight" : "after fight", g_active.load() ? FightSec() : Now() - g_endAt.load());
}
template <int I>
void HkRew(uint64_t ctx, uint64_t, uint64_t, uint64_t) {
    LogReward(I, ctx);
}

// GameClearRewardGrant(cl): 1 = Normal, 2 = Hard (not a script native; cl is a plain argument).
void HkGrant(uint64_t cl, uint64_t, uint64_t, uint64_t) {
    if (!RewardWindow()) return;
    g_log("BossLog #%d: REWARD GameClearRewardGrant difficulty %u (1 normal, 2 hard) t=%.3f s (%s)", g_fight.load(), (uint32_t)(uint8_t)cl,
          Now(), g_active.load() ? "in fight" : "after fight");
}

// ---- patching (same scheme as loadstats.cpp) ----
uint8_t* g_thunks = nullptr;
size_t g_thunkUsed = 0;
constexpr size_t kThunkPage = 4096;

uint8_t* AllocIn(uintptr_t from, uintptr_t to) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    const uintptr_t g = si.dwAllocationGranularity;
    MEMORY_BASIC_INFORMATION mbi;
    for (uintptr_t a = (from + g - 1) & ~(g - 1); a < to;) {
        if (!VirtualQuery((void*)a, &mbi, sizeof(mbi))) break;
        const uintptr_t rb = (uintptr_t)mbi.BaseAddress, re = rb + mbi.RegionSize;
        if (mbi.State == MEM_FREE) {
            const uintptr_t c = (rb + g - 1) & ~(g - 1);
            if (c >= from && c + kThunkPage <= re && c + kThunkPage <= to)
                if (void* p = VirtualAlloc((void*)c, kThunkPage, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE))
                    return (uint8_t*)p;
        }
        if (re <= a) break;
        a = re;
    }
    return nullptr;
}
uint8_t* AllocNear(uintptr_t lo, uintptr_t hi) {
    const uintptr_t reach = 0x7FF00000ull - (hi - lo);
    if (uint8_t* p = AllocIn(hi, hi + reach)) return p;
    return AllocIn(lo > reach + 0x10000 ? lo - reach : 0x10000, lo);
}

bool PatchAtomic(uint8_t* p, const uint8_t* bytes, size_t len) {
    const uintptr_t a = (uintptr_t)p, w8 = a & ~uintptr_t(7), w16 = a & ~uintptr_t(15);
    const bool fit8 = a + len <= w8 + 8, fit16 = a + len <= w16 + 16;
    if (!fit8 && !fit16) return false;
    void* win = fit8 ? (void*)w8 : (void*)w16;
    const size_t wlen = fit8 ? 8 : 16;
    DWORD old = 0;
    if (!VirtualProtect(win, wlen, PAGE_EXECUTE_READWRITE, &old)) return false;
    bool ok = false;
    if (fit8) {
        volatile LONG64* q = (volatile LONG64*)w8;
        const LONG64 cur = *q;
        LONG64 nw = cur;
        memcpy((uint8_t*)&nw + (a - w8), bytes, len);
        ok = InterlockedCompareExchange64(q, nw, cur) == cur;
    } else {
        volatile LONG64* q = (volatile LONG64*)w16;
        alignas(16) LONG64 cur[2] = {q[0], q[1]};
        LONG64 nw[2] = {cur[0], cur[1]};
        memcpy((uint8_t*)nw + (a - w16), bytes, len);
        ok = _InterlockedCompareExchange128(q, nw[1], nw[0], cur) != 0;
    }
    VirtualProtect(win, wlen, old, &old);
    FlushInstructionCache(GetCurrentProcess(), win, wlen);
    return ok;
}

// Instruction run straddling a 16-byte boundary: spin-park the first two bytes, write the tail, then the head.
bool PatchSpin(uint8_t* p, const uint8_t* bytes, size_t len) {
    static const uint8_t spin[2] = {0xEB, 0xFE};
    uint8_t orig[2] = {p[0], p[1]};
    if (len < 3 || !PatchAtomic(p, spin, 2)) return false;
    DWORD old = 0;
    if (!VirtualProtect(p + 2, len - 2, PAGE_EXECUTE_READWRITE, &old)) {
        PatchAtomic(p, orig, 2);
        return false;
    }
    memcpy(p + 2, bytes + 2, len - 2);
    VirtualProtect(p + 2, len - 2, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, len);
    return PatchAtomic(p, bytes, 2);
}
bool Patch(uint8_t* p, const uint8_t* bytes, size_t len) { return PatchAtomic(p, bytes, len) || PatchSpin(p, bytes, len); }

struct Hook {
    const char* name;
    uint32_t rva;
    uint8_t n;         // whole-instruction prologue bytes relocated (>= 5, <= 16, no rip-relative / branch)
    uint8_t orig[16];  // expected bytes
    void (*fn)(uint64_t, uint64_t, uint64_t, uint64_t);
};
const Hook kHooks[] = {
    {"BossBattleMode", 0x23A2DCC, 5, {0x48, 0x89, 0x5C, 0x24, 0x10}, &HkBegin},
    {"BossBattleModeEnd", 0x23A2F70, 9, {0x40, 0x53, 0x48, 0x81, 0xEC, 0x60, 0x01, 0x00, 0x00}, &HkEnd},
    {"Character::Create", 0x1180740, 5, {0x48, 0x89, 0x5C, 0x24, 0x08}, &HkCreate},
    {"SetMissionFlag", 0x70275C, 5, {0x48, 0x89, 0x5C, 0x24, 0x08}, &HkFlag},
    {"ShowBossGauge", 0x23B8678, 9, {0x40, 0x53, 0x48, 0x81, 0xEC, 0x70, 0x01, 0x00, 0x00}, &HkGauge},
    {"SetMissionClearFlag", 0x1C1DB88, 7, {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x08}, &HkClear},
    {"Character::AddDropTable", 0x238B3EC, 5, {0x48, 0x89, 0x5C, 0x24, 0x10}, &HkRew<0>},
    {"Character::AddDropFixItem", 0x238B194, 5, {0x48, 0x89, 0x5C, 0x24, 0x10}, &HkRew<1>},
    {"Character::AddDropKeyItemDirectGet", 0x238B2C0, 5, {0x48, 0x89, 0x5C, 0x24, 0x10}, &HkRew<2>},
    {"Character::ForceDropAmurita", 0x238DD00, 9, {0x40, 0x53, 0x48, 0x81, 0xEC, 0x70, 0x01, 0x00, 0x00}, &HkRew<3>},
    {"Character::ForceDropItem", 0x238DED0, 9, {0x40, 0x53, 0x48, 0x81, 0xEC, 0x70, 0x01, 0x00, 0x00}, &HkRew<4>},
    {"Character::ForceDropItemAndGet", 0x238DF64, 9, {0x40, 0x53, 0x48, 0x81, 0xEC, 0x70, 0x01, 0x00, 0x00}, &HkRew<5>},
    {"Character::EnemyForceKillWithItemDrop", 0x238DAA0, 9, {0x40, 0x53, 0x48, 0x81, 0xEC, 0xA0, 0x01, 0x00, 0x00}, &HkRew<6>},
    {"Item::AddItem", 0x23A0A98, 5, {0x48, 0x89, 0x5C, 0x24, 0x10}, &HkRew<7>},
    {"Item::AddFixItem", 0x23A07D4, 7, {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x10}, &HkRew<8>},
    {"Item::AddMagicItem", 0x23A0D48, 5, {0x48, 0x89, 0x5C, 0x24, 0x10}, &HkRew<9>},
    {"Item::AddItemNoNotice", 0x23A0C64, 5, {0x48, 0x89, 0x5C, 0x24, 0x10}, &HkRew<10>},
    {"Item::AddEquipment", 0x23A0208, 7, {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x10}, &HkRew<11>},
    {"Item::AddEquipmentBySlot", 0x23A0414, 7, {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x10}, &HkRew<12>},
    {"Item::AddEquipmentPreset", 0x23A0650, 7, {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x10}, &HkRew<13>},
    {"Player::AddGuardianSpirit", 0x23A7714, 9, {0x40, 0x53, 0x48, 0x81, 0xEC, 0x70, 0x01, 0x00, 0x00}, &HkRew<14>},
    {"Player::AddGuardianSpiritNoNotice", 0x23A77AC, 9, {0x40, 0x53, 0x48, 0x81, 0xEC, 0x70, 0x01, 0x00, 0x00}, &HkRew<15>},
    {"Player::AddGuardianSpiritWithTimer", 0x23A783C, 9, {0x40, 0x53, 0x48, 0x81, 0xEC, 0x70, 0x01, 0x00, 0x00}, &HkRew<16>},
    {"Player::AddSkill", 0x23A7A20, 5, {0x48, 0x89, 0x5C, 0x24, 0x10}, &HkRew<17>},
    {"Player::AddSkillDojo", 0x23A7B68, 5, {0x48, 0x89, 0x5C, 0x24, 0x10}, &HkRew<18>},
    {"Player::AddMoney", 0x23A79A4, 9, {0x40, 0x53, 0x48, 0x81, 0xEC, 0x70, 0x01, 0x00, 0x00}, &HkRew<19>},
    {"Mission::SetReward", 0x23A5C5C, 9, {0x40, 0x53, 0x48, 0x81, 0xEC, 0x70, 0x01, 0x00, 0x00}, &HkRew<20>},
    {"Mission::GetGameClearRewardNormal", 0x23A444C, 9, {0x40, 0x53, 0x48, 0x81, 0xEC, 0x60, 0x01, 0x00, 0x00}, &HkRew<21>},
    {"Mission::GetGameClearRewardHard", 0x23A43EC, 9, {0x40, 0x53, 0x48, 0x81, 0xEC, 0x60, 0x01, 0x00, 0x00}, &HkRew<22>},
    {"GameClearRewardGrant", 0x2280FA0, 7, {0x48, 0x83, 0xEC, 0x28, 0x80, 0xF9, 0x01}, &HkGrant},
};

// 0 ok, 1 bytes differ, 2 no room / not atomic.
int Install(const Hook& h) {
    uint8_t* p = (uint8_t*)(g_base + h.rva);
    if (memcmp(p, h.orig, h.n) != 0) return 1;
    if (g_thunkUsed + 80 > kThunkPage) return 2;
    uint8_t* t = g_thunks + g_thunkUsed;
    uint8_t* w = t;
    auto put = [&](std::initializer_list<uint8_t> b) {
        for (uint8_t x : b) *w++ = x;
    };
    // push rcx, rdx, r8, r9, r10, r11: callers in this LTCG build can keep values in r10/r11 across a call to a function
    // known not to touch them (1.7.17 LG crash, see loadstats.cpp NewSaveThunk), so the C++ hook must not leak clobbers.
    put({0x51, 0x52, 0x41, 0x50, 0x41, 0x51, 0x41, 0x52, 0x41, 0x53});
    put({0x48, 0x83, 0xEC, 0x28});              // sub rsp,28h (entry rsp = 8 mod 16 -> 16-aligned at the call)
    put({0x48, 0xB8});                          // mov rax, imm64
    const uint64_t fn = (uint64_t)h.fn;
    memcpy(w, &fn, 8);
    w += 8;
    put({0xFF, 0xD0});  // call rax
    put({0x48, 0x83, 0xC4, 0x28});
    put({0x41, 0x5B, 0x41, 0x5A, 0x41, 0x59, 0x41, 0x58, 0x5A, 0x59});  // pop r11, r10, r9, r8, rdx, rcx
    memcpy(w, h.orig, h.n);                     // relocated prologue (rsp is back at its entry value)
    w += h.n;
    put({0xFF, 0x25, 0, 0, 0, 0});  // jmp [rip+0] -> entry + n
    const uint64_t back = (uint64_t)(p + h.n);
    memcpy(w, &back, 8);
    w += 8;
    g_thunkUsed += ((size_t)(w - t) + 15) & ~size_t(15);
    const intptr_t d = (intptr_t)t - (intptr_t)(p + 5);
    if (d < INT32_MIN || d > INT32_MAX) return 2;
    uint8_t nb[16];
    memset(nb, 0x90, sizeof(nb));
    nb[0] = 0xE9;
    const int32_t rel = (int32_t)d;
    memcpy(nb + 1, &rel, 4);
    return Patch(p, nb, h.n) ? 0 : 2;
}

}  // namespace

bool Start(const StartInfo& info) {
    g_log = info.log;
    g_base = info.base;
    if (!g_log || !g_base || !info.iniPath) return false;
    if (GetPrivateProfileIntW(L"Debug", L"BossLog", 0, info.iniPath) == 0) return false;
    if (!info.knownBuild) {
        g_log("BossLog: refused (unknown build; addresses are v2.02)");
        return false;
    }
    LARGE_INTEGER fq;
    QueryPerformanceFrequency(&fq);
    g_qpcSec = 1.0 / (double)fq.QuadPart;
    g_qpc0 = Qpc();
    const auto* dos = (const IMAGE_DOS_HEADER*)g_base;
    const auto* nt = (const IMAGE_NT_HEADERS64*)(g_base + dos->e_lfanew);
    g_thunks = AllocNear(g_base, g_base + nt->OptionalHeader.SizeOfImage);
    if (!g_thunks) {
        g_log("BossLog: no executable page within 2 GB of the image - off");
        return false;
    }
    int ok = 0;
    char bad[160] = {0};
    size_t bl = 0;
    for (const Hook& h : kHooks) {
        const int r = Install(h);
        if (r == 0) {
            ++ok;
            continue;
        }
        if (bl < sizeof(bad) - 40) bl += (size_t)snprintf(bad + bl, sizeof(bad) - bl, " %s(%s)", h.name, r == 1 ? "bytes" : "atomic");
    }
    g_log("BossLog: %d/%d hooks installed (log only)%s%s", ok, (int)(sizeof(kHooks) / sizeof(kHooks[0])), bl ? " - skipped:" : "", bad);
    return ok > 0;
}

}  // namespace bosslog
