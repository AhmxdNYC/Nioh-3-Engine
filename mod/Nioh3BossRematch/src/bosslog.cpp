// bosslog.cpp - BossLog / RematchLog / InvLog (moved from Nioh3EngineFix 1.7.40 into the add-on; log-only, game behaviour unchanged)
// and the FIGHT state the rematch uses (fight count, in-fight flag, placements created before the last BossBattleMode).
// [Debug] BossLog / RematchLog / InvLog in Nioh3BossRematch.ini (all 0 by default). Every hook: the handler runs, then the
// original instructions, with all argument registers and xmm0-5 intact (hooks.cpp). Sites resolve by pattern / native name.
#include "addon.h"

#include <intrin.h>
#include <tlhelp32.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <cmath>
#include <algorithm>

namespace addon {
namespace fight {
volatile LONG g_fights = 0, g_active = 0;
}
namespace bosslog {
namespace {
bool g_logOn = false;  // [Debug] BossLog or RematchLog: write BossLog lines (the fight state is tracked either way)
SRWLOCK g_lastLock = SRWLOCK_INIT;
char g_lastPlac[256] = "";
uintptr_t g_imgEnd = 0;

uintptr_t g_base = 0;
double Now() { return fight::Now(); }
uint64_t Qpc() {
    LARGE_INTEGER q;
    QueryPerformanceCounter(&q);
    return (uint64_t)q.QuadPart;
}
double g_qpcSec = 0;
// resolved globals / functions (sites.cpp); 0 when the group did not resolve (every read is SEH-guarded and fails cleanly)
uintptr_t G_NET = 0, G_PM = 0, G_CH = 0, G_INV = 0, G_DATA = 0, G_MS = 0, G_QUEUE = 0, F_CLR0 = 0, F_CLR1 = 0, F_UNL = 0;

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

bool InFightOrGrace();  // defined after the RM state (also true inside a scroll/replay mission window)
double FightSec() { return (double)(Qpc() - g_t0.load()) * g_qpcSec; }

// ---- RematchLog state (1.7.21): a scroll / replay mission window, shared with the fight logger ----
bool g_rm = false;                            // [Debug] RematchLog
std::atomic<uint32_t> g_reqMission{0};        // mission requested by Emaki/replay and not yet seen to end (0 = none)
std::atomic<double> g_postUntil{-1e9};        // 30 s after a scroll/replay mission exit (rewards, clear)
bool RmBoss() { return g_reqMission.load() != 0 || Now() < g_postUntil.load(); }
bool InFightOrGrace() { return g_active.load() || Now() - g_endAt.load() < 20.0 || RmBoss(); }

// ---- loggers: rcx,rdx,r8,r9 are the hooked function's own arguments ----
void HkBegin(uint64_t ctx, uint64_t, uint64_t, uint64_t, const uint64_t*) {
    const int n = g_fight.fetch_add(1) + 1;
    const double t = Now();
    g_t0.store(Qpc());
    g_flagCount.store(0);
    g_active.store(true);
    if (g_logOn) Log("BossLog #%d: BossBattleMode start t=%.3f s (script ctx %p, last mission-clear id %u)", n, t, (void*)ctx,
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
    AcquireSRWLockExclusive(&g_lastLock);
    snprintf(g_lastPlac, sizeof(g_lastPlac), "%s", bl ? buf : "");
    ReleaseSRWLockExclusive(&g_lastLock);
    InterlockedExchange(&fight::g_active, 1);
    InterlockedIncrement(&fight::g_fights);
    if (g_logOn) Log("BossLog #%d: Character::Create placement ids in the last 60 s:%s", n, bl ? buf : " none");
}

void HkEnd(uint64_t, uint64_t, uint64_t, uint64_t, const uint64_t*) {
    const double t = Now();
    const double dur = g_active.load() ? FightSec() : 0.0;
    if (g_logOn) Log("BossLog #%d: BossBattleModeEnd t=%.3f s, fight %.1f s, %d mission flag sets inside", g_fight.load(), t, dur,
          g_flagCount.load());
    g_endAt.store(t);
    g_active.store(false);
    InterlockedExchange(&fight::g_active, 0);
}

void HkCreate(uint64_t placementId, uint64_t arg2, uint64_t arg3, uint64_t arg4, const uint64_t*) {
    const uint32_t i = g_ringN.fetch_add(1);
    RingEnt& e = g_ring[i % kRing];
    e.id.store((uint32_t)placementId);
    e.t.store(Now());
    if (g_active.load() || RmBoss())
        if (g_logOn) Log("BossLog #%d: Character::Create placement %u (arg2 %u, flag %u, arg4 %u) t=%.3f s +%.1f s", g_fight.load(),
              (uint32_t)placementId, (uint32_t)arg2, (uint32_t)(uint8_t)arg3, (uint32_t)arg4, Now(), FightSec());
}

void HkFlag(uint64_t, uint64_t flagId, uint64_t val, uint64_t, const uint64_t*) {
    if (!InFightOrGrace()) return;
    g_flagCount.fetch_add(1);
    Log("BossLog #%d: SetMissionFlag id %u value %u t=%.3f s (%s)", g_fight.load(), (uint32_t)flagId, (uint32_t)(uint8_t)val,
          Now(), g_active.load() ? "in fight" : "after fight");
}

void HkGauge(uint64_t actorId, uint64_t, uint64_t, uint64_t, const uint64_t*) {
    Log("BossLog #%d: ShowBossGauge actor id %u t=%.3f s%s", g_fight.load(), (uint32_t)actorId, Now(),
          g_active.load() ? "" : " (outside BossBattleMode)");
}

void HkClear(uint64_t missionId, uint64_t, uint64_t, uint64_t, const uint64_t*) {
    g_lastMission.store((uint32_t)missionId);
    Log("BossLog #%d: SetMissionClearFlag mission id %u t=%.3f s (%s)", g_fight.load(), (uint32_t)missionId, Now(),
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
bool RewardWindow() { return g_active.load() || Now() - g_endAt.load() < kRewardGrace || RmBoss(); }

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

// Formats the VM args of a script native as " a0=.. a1=.." (types as in ReadVmArgs). Returns nargs (-1 = unreadable).
int FmtVmArgs(uint64_t ctx, char* buf, size_t cap) {
    uint32_t val[8] = {0};
    uint16_t ty[8] = {0};
    const int n = ReadVmArgs(ctx, val, ty, 8);
    size_t bl = 0;
    buf[0] = 0;
    for (int i = 0; i < n && i < 8 && bl + 24 < cap; ++i) {
        if (ty[i] == 2) {
            float f;
            memcpy(&f, &val[i], 4);
            bl += (size_t)snprintf(buf + bl, cap - bl, " a%d=%g", i, (double)f);
        } else if (ty[i] == 4) {
            bl += (size_t)snprintf(buf + bl, cap - bl, " a%d=<obj>", i);
        } else {
            bl += (size_t)snprintf(buf + bl, cap - bl, " a%d=%u%s", i, val[i], ty[i] == 1 ? "" : (ty[i] == 3 ? "(b)" : "(?)"));
        }
    }
    return n;
}

void LogReward(int id, uint64_t ctx) {
    if (!RewardWindow()) return;
    char buf[200];
    const int n = FmtVmArgs(ctx, buf, sizeof(buf));
    Log("BossLog #%d: REWARD %s nargs=%d%s t=%.3f s (%s +%.1f s)", g_fight.load(), kRewardNames[id], n, n > 0 ? buf : "", Now(),
          g_active.load() ? "in fight" : "after fight", g_active.load() ? FightSec() : Now() - g_endAt.load());
}
template <int I>
void HkRew(uint64_t ctx, uint64_t, uint64_t, uint64_t, const uint64_t*) {
    LogReward(I, ctx);
}

// GameClearRewardGrant(cl): 1 = Normal, 2 = Hard (not a script native; cl is a plain argument).
void HkGrant(uint64_t cl, uint64_t, uint64_t, uint64_t, const uint64_t*) {
    if (!RewardWindow()) return;
    Log("BossLog #%d: REWARD GameClearRewardGrant difficulty %u (1 normal, 2 hard) t=%.3f s (%s)", g_fight.load(), (uint32_t)(uint8_t)cl,
          Now(), g_active.load() ? "in fight" : "after fight");
}

// =====================================================================================================================
// RematchLog (1.7.21, [Debug] RematchLog = 1): Phase 0 of notes\boss-rematch-design.md. READ ONLY: the hooks log, then run the
// original instructions. The only game calls are the three pure clear/unlock getters (inside the replay-menu window).
// Every hook gets the caller RVA through `sp` (sp[0] = return address, sp[1..4] shadow, sp[5..] stack args).
// =====================================================================================================================
template <class T>
bool RdB(uint64_t a, T& v) {
    __try {
        v = *(const T*)a;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
bool Plaus(uint64_t a) { return a >= 0x10000 && a < 0x00007FFFFFFFFFFFull; }
void HexSafe(char* out, size_t cap, uint64_t addr, size_t len) {
    uint8_t tmp[0x100];
    if (len > sizeof(tmp)) len = sizeof(tmp);
    bool ok = false;
    if (Plaus(addr)) {
        __try {
            memcpy(tmp, (const void*)addr, len);
            ok = true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
    if (!ok) {
        snprintf(out, cap, "<unreadable>");
        return;
    }
    size_t o = 0;
    for (size_t i = 0; i < len && o + 4 < cap; ++i) o += (size_t)snprintf(out + o, cap - o, i ? " %02X" : "%02X", tmp[i]);
    out[o < cap ? o : cap - 1] = 0;
}
uint64_t CallerRva(const uint64_t* sp) {
    uint64_t r = 0;
    return Rd((uint64_t)sp, r) ? r - g_base : 0;
}
uint64_t StackArg(const uint64_t* sp, int i) {
    uint64_t r = 0;
    Rd((uint64_t)(sp + i), r);
    return r;
}

std::atomic<bool> g_winOpen{false};
std::atomic<double> g_menuUntil{-1e9}, g_reqAt{-1e9}, g_itemWatch{-1e9}, g_lastRate[8];
std::atomic<bool> g_reqEmaki{false}, g_seenIn{false}, g_pendReq{false}, g_pendSetup{false};
std::atomic<double> g_exitSince{-1.0};
std::atomic<uint32_t> g_msAtReq{0}, g_winEpoch{0}, g_tickN{0};
std::atomic<uint32_t> g_recSeenN{0}, g_getSeenN{0}, g_listN{0};
std::atomic<uint32_t> g_recSeen[512], g_getSeen[512];
std::atomic<int> g_capSave{0}, g_capCms{0}, g_capSer{0};
std::atomic<bool> g_inGet{false};

bool RmAny() { return RmBoss() || Now() < g_menuUntil.load(); }
bool RateOk(int slot, double gap) {
    const double n = Now();
    if (n - g_lastRate[slot].load() < gap) return false;
    g_lastRate[slot].store(n);
    return true;
}
void OpenWin(const char* why) {
    if (!g_winOpen.exchange(true)) {
        g_winEpoch.fetch_add(1);
        g_recSeenN.store(0);
        g_getSeenN.store(0);
        g_listN.store(0);
        Log("RematchLog: window OPEN (%s) t=%.3f", why, Now());
    }
}
bool FirstTime(std::atomic<uint32_t>* arr, std::atomic<uint32_t>& cnt, uint32_t key, uint32_t cap) {
    const uint32_t n = cnt.load();
    for (uint32_t i = 0; i < n && i < cap; ++i)
        if (arr[i].load() == key) return false;
    if (n >= cap) return false;
    arr[cnt.fetch_add(1) % cap].store(key);
    return true;
}

void OnlineStr(char* o, size_t cap) {
    int conn = -1, host = -1, mode = -1, sub = -1;
    uint64_t net = 0, pm = 0;
    if (Rd(G_NET, net)) {
        if (!net) conn = 0;
        else {
            uint8_t c = 0, h = 0;
            if (Rd(net + 0x1DC, c)) conn = c;
            if (Rd(net + 0x2C8, h)) host = h;
        }
    }
    if (Rd(G_PM, pm)) {
        if (!pm) mode = -2;
        else {
            int32_t m = 0, s = 0;
            if (Rd(pm + 0x34, m)) mode = m;
            if (Rd(pm + 0x30, s)) sub = s;
        }
    }
    snprintf(o, cap, "net{conn=%d host=%d} pm{mode=%d%s sub=%d}", conn, host, mode, mode == -2 ? "(null)" : "", sub);
}

uint64_t ChMgr() {
    uint64_t a = 0, b = 0;
    if (Rd(G_CH, a) && a && Rd(a, b)) return b;
    return 0;
}
void ChStr(char* o, size_t cap) {
    const uint64_t ch = ChMgr();
    uint8_t e = 0, k = 0, i = 0, l = 0;
    uint32_t a = 0, b = 0;
    if (!ch) {
        snprintf(o, cap, "ch{null}");
        return;
    }
    Rd(ch + 0x2BEF, e);
    Rd(ch + 0x2BF4, k);
    Rd(ch + 0x2C18, i);
    Rd(ch + 0x2BF8, a);
    Rd(ch + 0x2BFC, b);
    Rd(ch + 0x2C00, l);
    snprintf(o, cap, "ch{emakiByte=%02X key8=%02X isEmaki=%u list=%08X/%08X/%02X}", e, k, i, a, b, l);
}

// ---- inventory scroll item watch (Q6) ----
int g_itemIdx = -1;
uint8_t g_itemSnap[0xE8];
uint64_t InvBase() {
    uint64_t a = 0, b = 0;
    if (Rd(G_INV, a) && a && Rd(a, b)) return b;
    return 0;
}
bool CopySafe(void* dst, uint64_t src, size_t n) {
    __try {
        memcpy(dst, (const void*)src, n);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
void FindScrollItem(uint32_t keyId) {
    const uint64_t inv = InvBase();
    g_itemIdx = -1;
    if (!inv) {
        Log("RematchLog L2: inventory base unreadable");
        return;
    }
    uint64_t cnt = 0;
    Rd(inv + 0x23B4E0, cnt);
    const int lim = (cnt > 0 && cnt <= 2000) ? (int)cnt : 1500;
    int hits = 0;
    for (int i = 0; i < lim; ++i) {
        uint32_t id = 0;
        if (!Rd(inv + 0x224A60 + (uint64_t)i * 0xE8 + 0x20, id)) break;
        if (id != keyId) continue;
        if (!hits++) {
            g_itemIdx = i;
            char hb[0xE8 * 3 + 8];
            HexSafe(hb, sizeof(hb), inv + 0x224A60 + (uint64_t)i * 0xE8, 0xE8);
            CopySafe(g_itemSnap, inv + 0x224A60 + (uint64_t)i * 0xE8, 0xE8);
            Log("RematchLog L2: scroll item idx %d (key u32 %08X matches item+0x20) bytes[E8]=%s", i, keyId, hb);
        }
    }
    Log("RematchLog L2: inventory count u64@+23B4E0=%llu scanned %d entries, %d match key %08X", (unsigned long long)cnt, lim, hits, keyId);
    if (g_itemIdx >= 0) g_itemWatch.store(Now() + 3600.0);
}
void WatchItem() {
    if (g_itemIdx < 0 || Now() > g_itemWatch.load()) return;
    const uint64_t inv = InvBase();
    if (!inv) return;
    uint8_t cur[0xE8];
    if (!CopySafe(cur, inv + 0x224A60 + (uint64_t)g_itemIdx * 0xE8, 0xE8)) return;
    if (!memcmp(cur, g_itemSnap, 0xE8)) return;
    char b[600];
    size_t o = 0;
    int n = 0;
    for (int i = 0; i < 0xE8 && n < 24 && o + 24 < sizeof(b); ++i)
        if (cur[i] != g_itemSnap[i]) {
            o += (size_t)snprintf(b + o, sizeof(b) - o, " +%02X:%02X>%02X", i, g_itemSnap[i], cur[i]);
            ++n;
        }
    memcpy(g_itemSnap, cur, 0xE8);
    Log("RematchLog L2: scroll item idx %d changed:%s t=%.3f", g_itemIdx, b, Now());
}

// ---- L1: Emaki table dump (once) ----
bool g_tableDone = false;
void DumpTable() {
    uint64_t d = 0, T = 0, B = 0, M = 0;
    if (!Rd(G_DATA, d) || !d || !Rd(d + 0xA88, T) || !T || !Rd(T, B) || !B || !Rd(T + 0x20, M) || !M) return;
    uint32_t count = 0;
    if (!Rd(B + 4, count) || count == 0 || count > 2000) return;
    uint8_t emptyKey = 0;
    uint64_t vb = 0, ve = 0;
    Rd(M + 4, emptyKey);
    Rd(M + 8, vb);
    Rd(M + 0x10, ve);
    const uint64_t ents = (vb && ve > vb) ? (ve - vb) / 8 : 0;
    Log("RematchLog L1: Emaki table T=%p B=%p count=%u M=%p emptyKey=%02X mapEntries=%llu", (void*)T, (void*)B, count, (void*)M, emptyKey,
          (unsigned long long)ents);
    for (uint32_t i = 0; i < count; ++i) {
        char hb[0x34 * 3 + 4];
        HexSafe(hb, sizeof(hb), B + 8 + (uint64_t)i * 0x34, 0x34);
        Log("RematchLog L1: EMAKI[%u] %s", i, hb);
    }
    for (uint64_t j = 0; j < ents && j < 4000; ++j) {
        uint8_t bb = 0;
        uint32_t idx = 0;
        Rd(vb + j * 8, bb);
        Rd(vb + j * 8 + 4, idx);
        if (bb != emptyKey) Log("RematchLog L1: EMAKIMAP[%llu] byte=%02X recordIndex=%u", (unsigned long long)j, bb, idx);
    }
    g_tableDone = true;
}

// ---- L4: per-tick state (MissionLoadStep entry) ----
struct St {
    uint8_t ms10 = 0, ms11 = 0, b2bef = 0, b2bf4 = 0, b2c18 = 0, b2c00 = 0;
    uint32_t ms14 = 0, ms24 = 0, d2bf8 = 0, d2bfc = 0;
    int conn = -1, host = -1, mode = -1, sub = -1;
    uint64_t invCnt = 0;
    bool msOk = false, chOk = false;
};
St g_prev;
bool g_havePrev = false;
double g_lastL4 = -1e9;
uint8_t g_slotPrev[3][0x58];
bool g_slotHave = false;

void ReadState(St& s) {
    uint64_t g = 0, ms = 0;
    if (Rd(G_MS, g) && g && Rd(g + 8, ms) && ms) {
        s.msOk = Rd(ms + 0x10, s.ms10) & Rd(ms + 0x11, s.ms11) & Rd(ms + 0x14, s.ms14) & Rd(ms + 0x24, s.ms24);
    }
    const uint64_t ch = ChMgr();
    if (ch)
        s.chOk = Rd(ch + 0x2BEF, s.b2bef) & Rd(ch + 0x2BF4, s.b2bf4) & Rd(ch + 0x2C18, s.b2c18) & Rd(ch + 0x2C00, s.b2c00) & Rd(ch + 0x2BF8, s.d2bf8) &
                 Rd(ch + 0x2BFC, s.d2bfc);
    uint64_t net = 0, pm = 0;
    if (Rd(G_NET, net)) {
        if (!net) s.conn = 0;
        else {
            uint8_t c = 0, h = 0;
            if (Rd(net + 0x1DC, c)) s.conn = c;
            if (Rd(net + 0x2C8, h)) s.host = h;
        }
    }
    if (Rd(G_PM, pm)) {
        if (!pm) s.mode = -2;
        else {
            int32_t m = 0, q = 0;
            if (Rd(pm + 0x34, m)) s.mode = m;
            if (Rd(pm + 0x30, q)) s.sub = q;
        }
    }
    const uint64_t inv = InvBase();
    if (inv) Rd(inv + 0x23B4E0, s.invCnt);
}
void LogState(const char* tag, const St& s) {
    Log("RematchLog L4 %s tick=%u t=%.3f ms{lvl=%u row=%u mission=%u(0x%X) mode=%u%s} ch{emakiByte=%02X key8=%02X isEmaki=%u list=%08X/%08X/%02X%s} "
          "net{conn=%d host=%d} pm{mode=%d sub=%d} invCnt=%llu",
          tag, g_tickN.load(), Now(), s.ms10, s.ms11, s.ms14, s.ms14, s.ms24, s.msOk ? "" : " ?", s.b2bef, s.b2bf4, s.b2c18, s.d2bf8, s.d2bfc, s.b2c00,
          s.chOk ? "" : " ?", s.conn, s.host, s.mode, s.sub, (unsigned long long)s.invCnt);
}

void Tick() {
    const uint32_t tn = g_tickN.fetch_add(1) + 1;
    if (!g_tableDone && tn >= 30 && (tn % 30 == 0) && tn < 20000) DumpTable();
    St cur;
    ReadState(cur);
    const double n = Now();
    // state diff (rate limited to 4 lines/s; the diff is kept until it is printed)
    if (!g_havePrev || (memcmp(&cur, &g_prev, sizeof(St)) != 0 && n - g_lastL4 > 0.25)) {
        LogState(g_havePrev ? "change" : "first", cur);
        g_prev = cur;
        g_havePrev = true;
        g_lastL4 = n;
    }
    // request queue slots
    for (int i = 0; i < 3; ++i) {
        uint8_t b[0x58];
        if (!CopySafe(b, G_QUEUE + (uint64_t)i * 0x58, 0x58)) continue;
        if (g_slotHave && !memcmp(b, g_slotPrev[i], 0x58)) continue;
        uint32_t mis, sm, om;
        memcpy(&mis, b + 4, 4);
        memcpy(&sm, b + 0xC, 4);
        memcpy(&om, b + 0x38, 4);
        if (g_slotHave || mis || b[8]) {
            char hb[0x58 * 3 + 4];
            HexSafe(hb, sizeof(hb), (uint64_t)b, 0x58);
            Log("RematchLog L4 slot%d tick=%u t=%.3f mission=%u(0x%X) region=%02X stageMission=%u onlineMode=%u bytes[58]=%s", i, tn, n, mis, mis, b[8],
                  sm, om, hb);
        }
        memcpy(g_slotPrev[i], b, 0x58);
    }
    g_slotHave = true;
    if (g_pendReq.exchange(false)) LogState("after-EmakiMissionRequest", cur);
    if (g_pendSetup.exchange(false)) LogState("after-MissionSetupEmaki", cur);
    WatchItem();
    // lifecycle of the requested mission
    const uint32_t rq = g_reqMission.load();
    if (rq) {
        const bool emaki = g_reqEmaki.load();
        const bool inNow = emaki ? (cur.chOk && cur.b2c18 == 1)
                                 : (cur.msOk && ((cur.ms14 & 0xFFFF) == rq || (cur.ms14 != g_msAtReq.load() && cur.ms14 != 0)));
        if (!g_seenIn.load()) {
            if (inNow) {
                g_seenIn.store(true);
                g_exitSince.store(-1.0);
                Log("RematchLog L4: IN MISSION %u (%s) %.1f s after the request t=%.3f", rq, emaki ? "emaki" : "replay", n - g_reqAt.load(), n);
            } else if (n - g_reqAt.load() > (emaki ? 90.0 : 900.0)) {
                Log("RematchLog L4: request %u never seen in mission (timeout) t=%.3f - window closed", rq, n);
                g_reqMission.store(0);
                g_postUntil.store(n + 10.0);
            }
        } else if (!inNow) {
            const double es = g_exitSince.load();
            if (es < 0) g_exitSince.store(n);
            else if (n - es > 5.0) {
                Log("RematchLog L4: EXIT of mission %u (%s) t=%.3f (state left the mission 5 s ago)", rq, emaki ? "emaki" : "replay", n);
                LogState("exit", cur);
                g_reqMission.store(0);
                g_postUntil.store(n + 30.0);
                g_itemWatch.store(n + 60.0);
            }
        } else {
            g_exitSince.store(-1.0);
        }
    }
    if (g_winOpen.load() && !RmAny()) {
        g_winOpen.store(false);
        Log("RematchLog: window CLOSED t=%.3f", n);
    }
}

void NoteRequest(uint32_t mission, bool emaki, const char* via) {
    uint64_t g = 0, ms = 0;
    uint32_t m14 = 0;
    if (Rd(G_MS, g) && g && Rd(g + 8, ms) && ms) Rd(ms + 0x14, m14);
    g_msAtReq.store(m14);
    g_reqEmaki.store(emaki);
    g_seenIn.store(false);
    g_exitSince.store(-1.0);
    g_reqAt.store(Now());
    g_reqMission.store(mission);
    OpenWin(via);
}

void HkTick(uint64_t, uint64_t, uint64_t, uint64_t, const uint64_t*) { Tick(); }

// L2: EmakiMissionRequest(rcx ref u64, rdx EmakiKey* (12 B), r8d missionId (0 = record+0x0C), r9 StartParams* (0x18 B)). Decompiled layout:
// the slot gets key[7] at +0, 0xCC96 at +4, record byte +0x31 at +8, mission at +0xC, key at +0x20, ref at +0x30, params at +0x40.
void HkEmaki(uint64_t ref, uint64_t key, uint64_t mission, uint64_t params, const uint64_t* sp) {
    char kb[64], pb[96], on[96], cb[120];
    HexSafe(kb, sizeof(kb), key, 12);
    HexSafe(pb, sizeof(pb), params, 0x18);
    OnlineStr(on, sizeof(on));
    ChStr(cb, sizeof(cb));
    uint32_t keyId = 0;
    Rd(key, keyId);
    Log("RematchLog L2: EmakiMissionRequest caller=+%llX ref=%016llX key[12]=%s missionArg=%u params[24]=%s %s %s t=%.3f", (unsigned long long)CallerRva(sp),
          (unsigned long long)ref, kb, (uint32_t)mission, pb, on, cb, Now());
    FindScrollItem(keyId);
    NoteRequest(0xCC96, true, "EmakiMissionRequest");
    g_pendReq.store(true);
}
// L3: MissionSetupEmaki(rcx charMgr** , rdx list*, r8d mission). Reads before the original runs; the "after" state is logged by the next tick.
void HkSetupEmaki(uint64_t cm, uint64_t list, uint64_t mission, uint64_t, const uint64_t* sp) {
    char cb[120], lb[200];
    ChStr(cb, sizeof(cb));
    HexSafe(lb, sizeof(lb), list + 0x1C, 0x1C);
    Log("RematchLog L3: MissionSetupEmaki caller=+%llX charMgr**=%p list=%p mission=%u before %s list[+1C..+37]=%s t=%.3f", (unsigned long long)CallerRva(sp),
          (void*)cm, (void*)list, (uint32_t)mission, cb, lb, Now());
    g_exitSince.store(-1.0);
    g_pendSetup.store(true);
}
// L7: MissionRequestById(ecx mission, dl slotByte, r8d sub (0 = default), r9 StartParams*)
void HkById(uint64_t mission, uint64_t slot, uint64_t sub, uint64_t params, const uint64_t* sp) {
    char pb[96], on[96];
    HexSafe(pb, sizeof(pb), params, 0x18);
    OnlineStr(on, sizeof(on));
    Log("RematchLog L7: MissionRequestById caller=+%llX mission=%u(0x%X) slotByte=%02X sub=%u params[24]=%s %s t=%.3f", (unsigned long long)CallerRva(sp),
          (uint32_t)mission, (uint32_t)mission, (uint32_t)(uint8_t)slot, (uint32_t)sub, pb, on, Now());
    NoteRequest((uint32_t)mission & 0xFFFF, false, "MissionRequestById");
}
// L7: MissionRequestEnqueue(rcx = first 8 StartParams bytes BY VALUE (not a pointer: correction to the design), rdx = 0x58-byte request*)
void HkEnqueue(uint64_t p8, uint64_t req, uint64_t, uint64_t, const uint64_t* sp) {
    char rb[0x58 * 3 + 8];
    HexSafe(rb, sizeof(rb), req, 0x58);
    Log("RematchLog L7: MissionRequestEnqueue caller=+%llX params.lo8=%016llX req[58]=%s t=%.3f", (unsigned long long)CallerRva(sp), (unsigned long long)p8, rb,
          Now());
}
void HkStartSave(uint64_t a, uint64_t b, uint64_t c, uint64_t d, const uint64_t* sp) {
    char on[96], cb[120];
    OnlineStr(on, sizeof(on));
    ChStr(cb, sizeof(cb));
    Log("RematchLog Q9: MissionStartFromSave caller=+%llX rcx=%llX dl=%02X r8b=%02X r9b=%02X %s %s t=%.3f", (unsigned long long)CallerRva(sp),
          (unsigned long long)a, (uint32_t)(uint8_t)b, (uint32_t)(uint8_t)c, (uint32_t)(uint8_t)d, on, cb, Now());
}
// replay menu: Setup(rcx menu, rdx, r8d journey/mission, r9, stack: param5 u32, param6 era byte, param7 byte)
void HkReplaySetup(uint64_t a, uint64_t b, uint64_t c, uint64_t d, const uint64_t* sp) {
    g_menuUntil.store(Now() + 90.0);
    OpenWin("MissionReplayMenu_Setup");
    Log("RematchLog L7: MissionReplayMenu_Setup caller=+%llX menu=%p rdx=%llX r8d=%u r9=%llX p5=%u p6(era)=%u p7=%u t=%.3f", (unsigned long long)CallerRva(sp),
          (void*)a, (unsigned long long)b, (uint32_t)c, (unsigned long long)d, (uint32_t)StackArg(sp, 5), (uint32_t)(uint8_t)StackArg(sp, 6),
          (uint32_t)(uint8_t)StackArg(sp, 7), Now());
}
void HkReplayOpen(uint64_t a, uint64_t b, uint64_t c, uint64_t d, const uint64_t* sp) {
    g_menuUntil.store(Now() + 90.0);
    OpenWin("replay menu open");
    Log("RematchLog L7: replay menu open (1DDA258) caller=+%llX rcx=%llX rdx=%llX r8=%llX r9=%llX t=%.3f", (unsigned long long)CallerRva(sp),
          (unsigned long long)a, (unsigned long long)b, (unsigned long long)c, (unsigned long long)d, Now());
}
void HkReplayNative(uint64_t ctx, uint64_t, uint64_t, uint64_t, const uint64_t* sp) {
    char vb[200];
    const int n = FmtVmArgs(ctx, vb, sizeof(vb));
    g_menuUntil.store(Now() + 90.0);
    OpenWin("UI::OpenMissionReplayMenu");
    Log("RematchLog L7: UI::OpenMissionReplayMenu native caller=+%llX nargs=%d%s t=%.3f", (unsigned long long)CallerRva(sp), n, n > 0 ? vb : "", Now());
}
void HkSelectList(uint64_t a, uint64_t b, uint64_t c, uint64_t d, const uint64_t* sp) {
    g_menuUntil.store(Now() + 90.0);
    OpenWin("MissionSelectListBuild");
    if (g_listN.fetch_add(1) < 12)
        Log("RematchLog L7: MissionSelectListBuild caller=+%llX rcx=%llX rdx=%llX r8=%llX r9=%llX t=%.3f", (unsigned long long)CallerRva(sp),
              (unsigned long long)a, (unsigned long long)b, (unsigned long long)c, (unsigned long long)d, Now());
}
void HkScrollTop(uint64_t a, uint64_t b, uint64_t c, uint64_t d, const uint64_t* sp) {
    g_menuUntil.store(Now() + 90.0);
    OpenWin("BattleScrollTop_Select");
    Log("RematchLog L7: BattleScrollTop_Select caller=+%llX rcx=%llX rdx=%llX r8=%llX r9=%llX t=%.3f", (unsigned long long)CallerRva(sp), (unsigned long long)a,
          (unsigned long long)b, (unsigned long long)c, (unsigned long long)d, Now());
}
void LogItemUse(const char* nm, uint64_t a, uint64_t b, uint64_t c, uint64_t d, const uint64_t* sp) {
    char on[96], hb[0x40 * 3 + 4] = "-", hc[0x40 * 3 + 4] = "-";
    OnlineStr(on, sizeof(on));
    if (Plaus(b)) HexSafe(hb, sizeof(hb), b, 0x40);
    if (Plaus(c)) HexSafe(hc, sizeof(hc), c, 0x40);
    Log("RematchLog Q7: %s caller=+%llX rcx=%llX rdx=%llX r8=%llX r9=%llX [rdx..+40]=%s [r8..+40]=%s %s t=%.3f", nm, (unsigned long long)CallerRva(sp),
          (unsigned long long)a, (unsigned long long)b, (unsigned long long)c, (unsigned long long)d, hb, hc, on, Now());
}
void HkItemUse(uint64_t a, uint64_t b, uint64_t c, uint64_t d, const uint64_t* sp) { LogItemUse("EmakiItemUse_MenuHandler", a, b, c, d, sp); }
// F1FD80 is MissionReplayMenu_Decide (the replay confirm callback; rcx = closure {menu*, bool}, edx = choice), not an item use. Choice 0
// writes the start request straight into the queue (no MissionRequestById / Enqueue call, so nothing opened the replay window in the
// 2026-10-08 session and flags / rewards outside the fight window were not logged): open the window here with menu+0x24F4 (mission id).
void HkItemUse2(uint64_t a, uint64_t b, uint64_t c, uint64_t d, const uint64_t* sp) {
    LogItemUse("MissionReplayMenu_Decide(F1FD80)", a, b, c, d, sp);
    uint64_t menu = 0;
    uint32_t mission = 0;
    if ((uint32_t)b == 0 && Rd(a, menu) && Plaus(menu) && Rd(menu + 0x24F4, mission) && mission && mission < 0x10000)
        NoteRequest(mission, false, "MissionReplayMenu_Decide choice 0 (inline queue write)");
}

// MissionRecordGet(rcx db, edx id): first occurrence of each id per window, with the caller
void HkRecordGet(uint64_t, uint64_t id, uint64_t, uint64_t, const uint64_t* sp) {
    if (!RmAny()) return;
    if (!FirstTime(g_recSeen, g_recSeenN, (uint32_t)id & 0xFFFF, 512)) return;
    Log("RematchLog L7: MissionRecordGet id=%u(0x%X) first in window, caller=+%llX t=%.3f", (uint32_t)id & 0xFFFF, (uint32_t)id & 0xFFFF,
          (unsigned long long)CallerRva(sp), Now());
}
// the three clear / unlock getters, compared on the same (db, id) while the replay menu or a mission window is open
void GetterCmp(int which, uint64_t db, uint64_t id, const uint64_t* sp) {
    if (!RmAny() || g_inGet.exchange(true)) return;
    if (FirstTime(g_getSeen, g_getSeenN, ((uint32_t)which << 16) | ((uint32_t)id & 0xFFFF), 512) && g_getSeenN.load() < 400) {
        const uintptr_t rv[3] = {F_CLR0, F_CLR1, F_UNL};
        uint64_t r[3];
        for (int i = 0; i < 3; ++i) {
            r[i] = ~0ull;
            if (!rv[i]) continue;
            __try {
                r[i] = ((uint64_t(*)(uint64_t, uint64_t))(rv[i]))(db, id & 0xFFFF);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
            }
        }
        Log("RematchLog L7: getters id=%u(0x%X) via=%d caller=+%llX ClearedGetSlot(702B64)=%02X/%08X ClearedGet(37AFC0)=%02X/%08X UnlockedGet(909218)=%02X/%08X t=%.3f",
              (uint32_t)id & 0xFFFF, (uint32_t)id & 0xFFFF, which, (unsigned long long)CallerRva(sp), (uint32_t)(r[0] & 0xFF), (uint32_t)r[0],
              (uint32_t)(r[1] & 0xFF), (uint32_t)r[1], (uint32_t)(r[2] & 0xFF), (uint32_t)r[2], Now());
    }
    g_inGet.store(false);
}
void HkCleared0(uint64_t db, uint64_t id, uint64_t, uint64_t, const uint64_t* sp) { GetterCmp(0, db, id, sp); }
void HkCleared1(uint64_t db, uint64_t id, uint64_t, uint64_t, const uint64_t* sp) { GetterCmp(1, db, id, sp); }
void HkUnlocked(uint64_t db, uint64_t id, uint64_t, uint64_t, const uint64_t* sp) { GetterCmp(2, db, id, sp); }

// L5
void HkUniqueFlag(uint64_t obj, uint64_t ser, uint64_t, uint64_t, const uint64_t* sp) {
    if (g_capSer.fetch_add(1) >= 60) return;
    char hb[40];
    HexSafe(hb, sizeof(hb), obj + 0x50, 8);
    Log("RematchLog L5: emaki_unique_enemy_defeat_flag_ serializer caller=+%llX obj=%p ser=%p obj[+50..]=%s t=%.3f", (unsigned long long)CallerRva(sp), (void*)obj,
          (void*)ser, hb, Now());
}
void HkClearMap(uint64_t, uint64_t id, uint64_t p3, uint64_t p4, const uint64_t* sp) {
    if (g_capCms.fetch_add(1) >= 300) return;
    Log("RematchLog L5: ClearMapSetEntry id=%u(0x%X) p3=%u p4=%u caller=+%llX (%s) t=%.3f", (uint32_t)(uint16_t)id, (uint32_t)(uint16_t)id,
          (uint32_t)(uint8_t)p3, (uint32_t)(uint8_t)p4, (unsigned long long)CallerRva(sp), RmBoss() ? "scroll/replay mission" : "other", Now());
}
void HkTrophy(uint64_t id, uint64_t flag, uint64_t, uint64_t, const uint64_t* sp) {
    Log("RematchLog L5: TrophyUnlock id=%u flag=%u caller=+%llX (%s) t=%.3f", (uint32_t)id, (uint32_t)(uint8_t)flag, (unsigned long long)CallerRva(sp),
          RmBoss() ? "scroll/replay mission" : "other", Now());
}
// mission end / retry / game over
void HkResult(uint64_t a, uint64_t, uint64_t, uint64_t, const uint64_t* sp) {
    if (!RateOk(0, 1.0)) return;
    Log("RematchLog END: MissionResultUpdate (rate 1/s) rcx=%llX caller=+%llX t=%.3f", (unsigned long long)a, (unsigned long long)CallerRva(sp), Now());
}
void HkResurrect(uint64_t a, uint64_t b, uint64_t c, uint64_t d, const uint64_t* sp) {
    Log("RematchLog END: RetryGadget_ResurrectPlayers ecx=%u edx=%u r8b=%u r9b=%u caller=+%llX t=%.3f", (uint32_t)a, (uint32_t)b, (uint32_t)(uint8_t)c,
          (uint32_t)(uint8_t)d, (unsigned long long)CallerRva(sp), Now());
}
void HkDecideRetry(uint64_t a, uint64_t, uint64_t, uint64_t, const uint64_t* sp) {
    Log("RematchLog END: SetDecideRetry impl cl=%u caller=+%llX t=%.3f", (uint32_t)(uint8_t)a, (unsigned long long)CallerRva(sp), Now());
}
const char* const kEndNames[] = {"Mission::SetMissionResultClearType", "UI::SetGameOverUIDeadType", "UI::SetGameOverUIFailType",
                                 "Player::SetRetryPointIDAndSave", "Player::NoGameOver"};
template <int I>
void HkEnd2(uint64_t ctx, uint64_t, uint64_t, uint64_t, const uint64_t* sp) {
    char vb[200];
    const int n = FmtVmArgs(ctx, vb, sizeof(vb));
    Log("RematchLog END: %s nargs=%d%s caller=+%llX t=%.3f", kEndNames[I], n, n > 0 ? vb : "", (unsigned long long)CallerRva(sp), Now());
}
void HkSavePath(uint64_t, uint64_t, uint64_t, uint64_t, const uint64_t* sp) {
    if (g_capSave.fetch_add(1) >= 200) return;
    Log("RematchLog Q9: BuildSaveFilePath caller=+%llX %s t=%.3f", (unsigned long long)CallerRva(sp), RmAny() ? "(scroll/replay window)" : "", Now());
}

// =====================================================================================================================
// InvLog ([Debug] InvLog = 1): every inventory add, session-wide (no fight window). Why: the 2026-10-08 session (chests, enemy and
// boss drops, pickups) logged ZERO BossLog REWARD lines - the reward hooks are script natives (Item::AddItem ..., only mission
// scripts call them) and they only log inside the fight window. All of those natives, and 80+ engine callers, funnel into
// InventoryAddItem 54D324(rcx save [4751530], rdx out, r8 ItemDesc* (0xE8 B = the inventory record: +0 u16 id, +2 u16 base id,
// +0xF u8, +0x18 flags, +0x28 serial, +0x30 u8 from item record +0x184), r9, [rsp+28h] u8 notify), notes\session-2026-10-08-gamedata.md
// s.3. Logs id, the first 0x48 desc bytes, r9, notify, the caller + its function start (pdata) and up to 3 earlier return addresses
// found on the stack (source classification offline). Rate limit 30 lines/s; suppressed calls are counted on the next line.
std::atomic<uint32_t> g_invN{0}, g_invSup{0};
std::atomic<int64_t> g_invSec{-1};
std::atomic<int> g_invInSec{0};
uint32_t FnStart(uint64_t addr) {  // primary function start RVA of a code address (follows chained unwind info), 0 = none
    __try {
        DWORD64 ib = 0;
        PRUNTIME_FUNCTION rf = RtlLookupFunctionEntry(addr, &ib, nullptr);
        for (int k = 0; rf && k < 8; ++k) {
            const uint8_t* ui = (const uint8_t*)(ib + rf->UnwindData);
            if (!((ui[0] >> 3) & 4)) return ib == g_base ? rf->BeginAddress : 0;  // UNW_FLAG_CHAININFO
            rf = (PRUNTIME_FUNCTION)(ui + 4 + ((ui[2] + 1) & ~1) * 2);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return 0;
}
bool LooksLikeRet(uint64_t v) {  // v is in the image and the bytes before it are a call
    if (v < g_base + 0x1000 || v >= g_imgEnd) return false;
    uint8_t b[7] = {};
    if (!CopySafe(b, v - 7, 7)) return false;
    return b[2] == 0xE8                                       // call rel32
           || (b[5] == 0xFF && (b[6] & 0xF8) == 0xD0)         // call reg
           || (b[4] == 0xFF && (b[5] & 0xF8) == 0x50)         // call [reg+d8]
           || (b[1] == 0xFF && (b[2] == 0x15 || (b[2] & 0xF8) == 0x90));  // call [rip+d32] / [reg+d32]
}
const char* InvSource(uint32_t fn) {
    switch (fn) {
        case 0x23A0A98: return "script Item::AddItem";
        case 0x23A0C64: return "script Item::AddItemNoNotice";
        case 0x23A07D4: return "script Item::AddFixItem";
        case 0x23A0414: return "script Item::AddEquipmentBySlot";
        case 0x23A0208: return "script Item::AddEquipment";
        case 0x2280C84: return "game-clear reward (RewardNoticeShowById)";
        case 0x54CD9C: case 0x54EFA0: case 0x54E574: return "inventory internal (see via)";
        default: return "?";
    }
}
void HkInvAdd(uint64_t save, uint64_t out, uint64_t desc, uint64_t r9, const uint64_t* sp) {
    const uint32_t n = g_invN.fetch_add(1) + 1;
    const double t = Now();
    const int64_t sec = (int64_t)t;
    if (g_invSec.exchange(sec) != sec) g_invInSec.store(0);
    if (g_invInSec.fetch_add(1) >= 30) {
        g_invSup.fetch_add(1);
        return;
    }
    uint16_t id = 0, base = 0;
    uint8_t rar = 0;
    Rd(desc, id);
    Rd(desc + 2, base);
    Rd(desc + 0x30, rar);
    const uint64_t ret = StackArg(sp, 0);
    const uint8_t notify = (uint8_t)StackArg(sp, 5);
    const uint32_t fn = FnStart(ret);
    char via[160] = "";
    size_t vl = 0;
    int found = 0;
    for (int k = 6; k < 72 && found < 3; ++k) {  // above our 4 shadow slots + the 5th arg
        const uint64_t v = StackArg(sp, k);
        if (!LooksLikeRet(v)) continue;
        vl += (size_t)snprintf(via + vl, sizeof(via) - vl, " +%llX(fn %X)", (unsigned long long)(v - g_base), FnStart(v));
        ++found;
    }
    char hb[0x48 * 3 + 4];
    HexSafe(hb, sizeof(hb), desc, 0x48);
    const uint32_t sup = g_invSup.exchange(0);
    Log("InvLog #%u: add item id=%u(0x%X) base=%u b30=%u notify=%u r9=%llX caller=+%llX fn=%X src=%s via%s desc[48]=%s t=%.3f", n, id, id,
          base, rar, notify, (unsigned long long)r9, (unsigned long long)(ret - g_base), fn, InvSource(fn), vl ? via : " -", hb, t);
    if (sup) Log("InvLog: %u calls suppressed by the 30/s limit before #%u", sup, n);
    (void)save;
    (void)out;
}

// SC probe (same [Debug] InvLog switch): who runs while a soul core / item is used. Logs the first 4 args and up to 4 return addresses
// above the hook (a call chain; function starts via pdata) for EffectCreate (r8 = effect id), 1320F2C and 23CBF9C. 60 lines/s.
std::atomic<int64_t> g_scSec{-1};
std::atomic<int> g_scInSec{0};
template <int N>
void HkSc(uint64_t a, uint64_t b, uint64_t c, uint64_t d, const uint64_t* sp) {
    if (N == 4) {  // GSpiritWork 2916C5C: rcx = the spirit manager; keep it for `rm spirit status|spawn|pin`
        char np[64], rp[32];
        snprintf(np, sizeof(np), "spirit note %llX", (unsigned long long)a);
        addon::rematch::Command(np, rp, sizeof(rp));
    }
    if (N == 6 && CallerRva(sp) == 0x65ADA6) addon::rematch::SpiritSpawnNote(a, (uint32_t)b);  // #54000: first motion of a 65A8A8 chr spawn
    static const char* const kName[] = {"EffectCreate", "fn1320F2C", "fn23CBF9C", "GSpiritReq", "GSpiritWork", "AsyncLoad", "MotionReq"};
    const double t = Now();
    const int64_t sec = (int64_t)t;
    if (g_scSec.exchange(sec) != sec) g_scInSec.store(0);
    if (g_scInSec.fetch_add(1) >= 60) return;
    char via[200] = "";
    size_t vl = 0;
    int found = 0;
    for (int k = 0; k < 72 && found < 4; ++k) {
        const uint64_t v = StackArg(sp, k);
        if (!LooksLikeRet(v)) continue;
        vl += (size_t)snprintf(via + vl, sizeof(via) - vl, " +%llX(fn %X)", (unsigned long long)(v - g_base), FnStart(v));
        ++found;
    }
    Log("ScLog %s a=%llX b=%llX c=%llX(%u) d=%llX via%s t=%.3f", kName[N], (unsigned long long)a, (unsigned long long)b,
          (unsigned long long)c, (unsigned)c, (unsigned long long)d, vl ? via : " -", t);
}

std::atomic<int> g_scProbes{0};  // pc #56435 [Spirit] Probes: 0 = CoreProbe/SealProbe/TenshinInput/TenshinForm/ChrCreated not installed
// Core-form probe (pc #46959): B611BC / 29EF38 args + 0x40 bytes behind rcx/rdx/r8, so core 0xF vs 0x111 can be diffed in the log.
template <int N>
void HkCoreProbe(uint64_t a, uint64_t b, uint64_t c, uint64_t d, const uint64_t* sp) {
    static std::atomic<int64_t> sec{-1};
    static std::atomic<int> inSec{0};
    const int64_t ts = (int64_t)Now();
    if (sec.exchange(ts) != ts) inSec.store(0);
    if (inSec.fetch_add(1) >= 20) return;
    char via[200] = "";
    size_t vl = 0;
    int found = 0;
    for (int k = 0; k < 72 && found < 4; ++k) {
        const uint64_t v = StackArg(sp, k);
        if (!LooksLikeRet(v)) continue;
        vl += (size_t)snprintf(via + vl, sizeof(via) - vl, " +%llX(fn %X)", (unsigned long long)(v - g_base), FnStart(v));
        ++found;
    }
    char ha[0x40 * 3 + 4], hb[0x40 * 3 + 4], hc[0x40 * 3 + 4];
    HexSafe(ha, sizeof(ha), a, 0x40);
    HexSafe(hb, sizeof(hb), b, 0x40);
    HexSafe(hc, sizeof(hc), c, 0x40);
    const char* nm = N == 0 ? "B611BC" : "29EF38";
    Log("CoreProbe %s a=%llX b=%llX c=%llX d=%llX via%s t=%.3f", nm, (unsigned long long)a, (unsigned long long)b, (unsigned long long)c,
          (unsigned long long)d, vl ? via : " -", Now());
    Log("CoreProbe %s [a]=%s", nm, ha);
    Log("CoreProbe %s [b]=%s", nm, hb);
    Log("CoreProbe %s [c]=%s", nm, hc);
}

// Seal-path probe (pc #53994): who queues the two chrs the Summoning Seal spawns (MotionReq C52/4E5C come from the queue flush
// 5BCBAC -> 65A8A8, so the requester is not on that stack). Deep chain (12 return addrs over 400 slots) + desc dump at: 65A8A8 chr spawn
// (rdx = desc: [0] id, [4] resolved id, +6C/+70 first motion), 68157C / 5BA648 queue insert (rcx / rdx = desc), 322468 / 3203EC item-use
// path that fires the gold-burst effect. 20 lines/s and 300 lines total per hook.
template <int N>
void HkSeal(uint64_t a, uint64_t b, uint64_t c, uint64_t d, const uint64_t* sp) {
    if (N == 0 && b > 0x10000) {  // #54190: desc[0] = chr id of this spawn, for [Spirit] FormKey
        uint32_t d0 = 0;
        if (Rd(b, d0)) FormKeySealDesc(d0);
    }
    if (!g_scProbes.load()) return;
    static const char* const kNm[] = {"ChrSpawn65A8A8", "SpawnReq68157C", "ListIns5BA648", "Item322468", "Item3203EC"};
    static std::atomic<int64_t> sec{-1};
    static std::atomic<int> inSec{0}, total{0};
    const int64_t ts = (int64_t)Now();
    if (sec.exchange(ts) != ts) inSec.store(0);
    if (inSec.fetch_add(1) >= 20 || total.fetch_add(1) >= 300) return;
    char via[600] = "";
    size_t vl = 0;
    int found = 0;
    for (int k = 0; k < 400 && found < 12 && vl + 40 < sizeof(via); ++k) {
        const uint64_t v = StackArg(sp, k);
        if (!LooksLikeRet(v)) continue;
        vl += (size_t)snprintf(via + vl, sizeof(via) - vl, " +%llX(fn %X)", (unsigned long long)(v - g_base), FnStart(v));
        ++found;
    }
    Log("SealProbe %s a=%llX b=%llX c=%llX d=%llX via%s t=%.3f", kNm[N], (unsigned long long)a, (unsigned long long)b, (unsigned long long)c,
          (unsigned long long)d, vl ? via : " -", Now());
    const uint64_t desc = N == 0 || N == 2 ? b : N == 1 ? a : 0;
    char h[0x40 * 3 + 4];
    if (desc) {
        for (uint64_t off = 0; off < 0x120; off += 0x40) {
            HexSafe(h, sizeof(h), desc + off, 0x40);
            Log("SealProbe %s desc+%03llX=%s", kNm[N], (unsigned long long)off, h);
        }
    } else {
        HexSafe(h, sizeof(h), a, 0x40);
        Log("SealProbe %s [a]=%s", kNm[N], h);
        HexSafe(h, sizeof(h), b, 0x40);
        Log("SealProbe %s [b]=%s", kNm[N], h);
    }
}

// ally #54196: log only. 65A8A8 calls 2BFA04 (new chr, rbx) and then 3578E0(chr, desc+0x10) at 65A992, so 3578E0 entered from 65A997 is the
// first point after the chr exists. 65B2C0(desc[0]) is not a model lookup: it hands out a free chr handle id in 0x1388..0x2326 (unique key id<<4
// in mgr+B98), kept in 65A8A8's local [rsp+0x64] (= entry sp+8+0x64 here). Logs desc[0], that id, the chr's model slot [chr+0x50]+8+idx*8
// (idx [M+0xA0]) and the player's, so pc can see whether the new chr already shares the player's model at creation.
void HkChrCreated(uint64_t a, uint64_t b, uint64_t, uint64_t, const uint64_t* sp) {
    if (StackArg(sp, 0) - g_base != 0x65A997) return;
    static std::atomic<int> n{0};
    if (n.fetch_add(1) >= 60) return;
    const uint64_t desc = b - 0x10, frame = (uint64_t)sp + 8;
    uint32_t id0 = 0, id4 = 0, hid = 0, m6c = 0;
    Rd(desc, id0); Rd(desc + 4, id4); Rd(frame + 0x64, hid); Rd(desc + 0x6C, m6c);
    auto slot = [](uint64_t chr, uint64_t& M, uint32_t& idx, uint64_t& m0, uint64_t& m1, uint64_t& act) {
        M = 0; idx = 0; m0 = m1 = act = 0;
        if (!chr || !Rd(chr + 0x50, M) || !M) return;
        Rd(M + 0xA0, idx); Rd(M + 8, m0); Rd(M + 0x10, m1);
        if (idx <= 7) Rd(M + 8 + (uint64_t)idx * 8, act);
    };
    uint64_t cM, c0, c1, ca, pM, p0, p1, pa;
    uint32_t ci, pi;
    slot(a, cM, ci, c0, c1, ca);
    uint64_t pt = 0, pch = 0;  // PlayerCharacterGet(0) = [[g:PlayerTab] + 0xCD*0x18]
    if (const uint64_t g = (uint64_t)Site("g:PlayerTab")) { if (Rd(g, pt) && pt) Rd(pt + 0xCD * 0x18, pch); }
    slot(pch, pM, pi, p0, p1, pa);
    Log("ChrCreated 65A8A8 desc[0]=%X desc[4]=%X id65B2C0=%X motion=%X chr=%llX M=%llX idx=%u slot=%llX m0=%llX m1=%llX | player chr=%llX M=%llX idx=%u "
        "slot=%llX m0=%llX m1=%llX same=%d t=%.3f",
        id0, id4, hid, m6c, (unsigned long long)a, (unsigned long long)cM, ci, (unsigned long long)ca, (unsigned long long)c0, (unsigned long long)c1,
        (unsigned long long)pch, (unsigned long long)pM, pi, (unsigned long long)pa, (unsigned long long)p0, (unsigned long long)p1, ca && ca == pa, Now());
}

// Tenshin input: the game's local input path 2E6C34 (state = rcx; input-command word [state+0x1068], bit 0xE = NIOH_TENSHIN). TenshinGo() arms a
// one-shot: the next call gets the bit OR-ed in before the original runs, so the game's own path starts the form (Begin + activation motion).
std::atomic<int> g_tenGo{0};
std::atomic<uint32_t> g_tenForm{0};  // `rm tenshin form <hex>`: written to [state+0xA68] right before bit 0xE is OR'd (0 = leave)
std::atomic<uint32_t> g_tenSetForm{0};  // `rm tenshin setform <hex>`: game SetForm 0x74C2FC(state,id)+0x74C128(state,id) before bit 0xE (ally #47284)
std::atomic<uint64_t> g_tenState{0};
std::atomic<uint32_t> g_tenMove{0};  // `rm tenshin move <hex>`: game 0x66C4A8(state, id, 0, 0.0f) once on the next input state (soulcore-A experiment 2)
// 842FAC body (after its zero check): rcx = player data, edx = spirit id, r8b = active flag. The Tenshin Begin path passes the EQUIPPED id here.
std::atomic<uint64_t> g_pdSeen{0};
void HkSpiritActive(uint64_t a, uint64_t b, uint64_t c, uint64_t, const uint64_t* sp) {
    g_pdSeen.store(a);
    Log("SpiritActive: pd=%llX id=%u(0x%X) flag=%u caller=+%llX t=%.3f", (unsigned long long)a, (unsigned)(uint32_t)b, (unsigned)(uint32_t)b,
          (unsigned)(c & 0xFF), (unsigned long long)(StackArg(sp, 0) - g_base), Now());
}
// Tenshin request (ally #47009): bit 0xE of [state+0x1068] = request id 0x4E in the bitset at state+0x1060, set by fc1a20(bits, id, 1) only inside
// 0x786C84 (at 0x786DA9) when the gauge/stock gates pass. The hook logs the caller + args + pad so the real binding shows up in the log.
std::atomic<int> g_tenReq{0};
WORD PadButtons(BYTE* lt, BYTE* rt) {
    using XGet = DWORD(WINAPI*)(DWORD, void*);
    static XGet xget = nullptr;
    static bool tried = false;
    if (!tried) {
        tried = true;
        static const wchar_t* const kXi[] = {L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll"};
        for (const wchar_t* dll : kXi)
            if (HMODULE m = GetModuleHandleW(dll)) { xget = (XGet)GetProcAddress(m, "XInputGetState"); if (xget) break; }
    }
    if (lt) *lt = 0;
    if (rt) *rt = 0;
    if (!xget) return 0;
    struct { DWORD pkt; WORD buttons; BYTE lt, rt; SHORT lx, ly, rx, ry; } st = {};
    for (DWORD u = 0; u < 4; ++u)
        if (xget(u, &st) == 0) {
            if (lt) *lt = st.lt;
            if (rt) *rt = st.rt;
            return st.buttons;
        }
    return 0;
}
int TenshinReqCall(uint64_t bits) {
    __try {
        ((void (*)(uint64_t, uint64_t, uint8_t))(g_base + 0xFC1A20))(bits, 0x4E, 1);
        return 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 1;
    }
}
std::atomic<int> g_tenBody{0};  // `rm tenshin body <1|2|3>`: after go, log B440/[[st+50]+A0]/[[st+50]+10]; 2 also zeroes B440, 3 also sets slot A0=1 (ally #52022)
int g_tenBodyTick = 0;
int g_tenSfRet = 0;  // al returned by the last TenshinSetFormCall
int TenshinSetFormCall(uint64_t st, uint32_t id, int which) {
    __try {
        g_tenSfRet = ((uint8_t(*)(uint64_t, uint32_t))(g_base + (which ? 0x74C128 : 0x74C2FC)))(st, id);
        return 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 1;
    }
}
int TenshinMoveCall(uint64_t st, uint32_t id) {
    __try {
        ((void (*)(uint64_t, uint32_t, uint32_t, float))(g_base + 0x66C4A8))(st, id, 0, 0.0f);
        return 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 1;
    }
}
void* (*g_itemDefOrig)(void*, uint64_t) = nullptr;
// call site 0x74C27F in RefreshForm 0x74C1DC: itemdef lookup for the equipped core; form id = [def+0x58] (ally #47284, pc2 #47448)
void* HkItemDefForm(void* tbl, uint64_t id) {
    void* r = g_itemDefOrig(tbl, id);
    uint32_t f = 0xFFFFFFFF;
    __try { if (r) f = *(uint32_t*)((uint8_t*)r + 0x58); } __except (EXCEPTION_EXECUTE_HANDLER) {}
    if (const uint32_t fm = g_tenForm.load()) {  // 74C1DC runs inside 2E6C34 AFTER our A68 write and overwrites it from [def+0x58]: force the def's form id instead
        __try { if (r && f != fm) { DWORD op = 0; VirtualProtect((uint8_t*)r + 0x58, 4, PAGE_READWRITE, &op); *(uint32_t*)((uint8_t*)r + 0x58) = fm; VirtualProtect((uint8_t*)r + 0x58, 4, op, &op); Log("TenshinForm: itemdef %p form %X -> %X (forced)", r, f, fm); f = fm; } } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    Log("TenshinForm: RefreshForm itemid=%u (0x%X) itemdef=%p form[+58]=0x%X", (unsigned)(id & 0xFFFF), (unsigned)(id & 0xFFFF), r, f);
    return r;
}
void HkTenshinReq(uint64_t a, uint64_t b, uint64_t c, uint64_t, const uint64_t* sp) {
    static std::atomic<int64_t> sec{-1};
    static std::atomic<int> inSec{0};
    const int64_t ts = (int64_t)Now();
    if (sec.exchange(ts) != ts) inSec.store(0);
    if (inSec.fetch_add(1) >= 30) return;
    BYTE lt = 0, rt = 0;
    const WORD bt = PadButtons(&lt, &rt);
    Log("TenReq 786C84: ctrl=%llX edx=%u(0x%X) r8=%llX ra=+%llX(fn %X) pad=%04X lt=%u rt=%u t=%.3f", (unsigned long long)a, (unsigned)(uint32_t)b,
          (unsigned)(uint32_t)b, (unsigned long long)c, (unsigned long long)(StackArg(sp, 0) - g_base), FnStart(StackArg(sp, 0)), bt, lt, rt, Now());
}
void HkTenshinInput(uint64_t a, uint64_t, uint64_t, uint64_t, const uint64_t*) {
    if (g_tenState.exchange(a) != a) Log("TenshinInput: state=%llX", (unsigned long long)a);
    {   // real-button trigger: hold L3 + R3 (both stick clicks), rising edge -> fill the gauge and arm the input bit
        using XGet = DWORD(WINAPI*)(DWORD, void*);
        static XGet xget = nullptr;
        static bool tried = false;
        static bool wasDown = false;
        if (!tried) {
            tried = true;
            static const wchar_t* const kXi[] = {L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll"};
            for (const wchar_t* dll : kXi) {
                if (HMODULE m = GetModuleHandleW(dll)) { xget = (XGet)GetProcAddress(m, "XInputGetState"); if (xget) break; }
            }
        }
        if (xget) {
            struct { DWORD pkt; WORD buttons; BYTE lt, rt; SHORT lx, ly, rx, ry; } st = {};
            bool down = false;
            for (DWORD u = 0; u < 4 && !down; ++u)
                if (xget(u, &st) == 0) down = (st.buttons & 0x0040) && (st.buttons & 0x0080);
            if (down && !wasDown) {
                char rp[128];
                addon::rematch::Command("tenshin fill", rp, sizeof(rp));
                g_tenGo.store(24);
                Log("TenshinInput: L3+R3 pressed -> fill %s + go", rp);
            }
            wasDown = down;
        }
    }
    {   // find the real binding: log the pad whenever bit 0xE of the input word turns on without us arming it; Y+B (rising edge) / `rm tenshin req` requests 0x4E
        static bool wasSet = false, wasYB = false;
        uint32_t w0 = 0;
        const bool ours = g_tenGo.load() > 0;
        if (Rd(a + 0x1068, w0)) {
            const bool set = (w0 & 0x4000) != 0;
            if (set && !wasSet && !ours) {
                BYTE lt = 0, rt = 0;
                const WORD bt = PadButtons(&lt, &rt);
                Log("TenshinInput: NATURAL bit 0xE state=%llX word=%08X pad=%04X lt=%u rt=%u", (unsigned long long)a, w0, bt, lt, rt);
            }
            wasSet = set;
        }
        const WORD bt2 = PadButtons(nullptr, nullptr);
        const bool yb = (bt2 & 0x8000) && (bt2 & 0x2000);
        if (yb && !wasYB) { g_tenReq.store(1); Log("TenshinInput: Y+B pressed -> request 0x4E"); }
        wasYB = yb;
        if (g_tenReq.exchange(0)) {
            char rp[128];
            addon::rematch::Command("tenshin fill", rp, sizeof(rp));
            const int rc = TenshinReqCall(a + 0x1060);
            uint32_t w1 = 0;
            Rd(a + 0x1068, w1);
            Log("TenshinInput: fc1a20(state+0x1060,0x4E,1) rc=%d word %08X->%08X fill=%s", rc, w0, w1, rp);
        }
    }
    {   // spirit pin: keep the spawned guardian-spirit chr on the player (no-op unless `rm spirit pin`)
        static int n = 0;
        if ((++n & 3) == 0) {
            char rp[200];
            addon::rematch::Command("spirit tick", rp, sizeof(rp));
        }
    }
    if (const int bm = g_tenBody.load()) {
        if (g_tenBodyTick++ < 240) {
            uint8_t b = 0xFF, a0 = 0xFF;
            uint64_t p50 = 0, q10 = 0;
            Rd(a + 0xB440, b);
            Rd(a + 0x50, p50);
            if (p50) { Rd(p50 + 0x10, q10); Rd(p50 + 0xA0, a0); }
            if (bm >= 2 && b) { uint8_t z = 0; WrBytes(a + 0xB440, &z, 1); }
            if (bm >= 3 && p50 && a0 != 1) { uint8_t one = 1; WrBytes(p50 + 0xA0, &one, 1); }
            if ((g_tenBodyTick % 6) == 1) Log("TenshinBody[%d] t=%d B440=%u slotA0=%u [[st+50]+10]=%llX", bm, g_tenBodyTick, (unsigned)b, (unsigned)a0, (unsigned long long)q10);
        }
    }
    if (const uint32_t mv = g_tenMove.exchange(0)) {
        uint8_t b0 = 0xFF, b1 = 0xFF;
        uint64_t p50 = 0, q10 = 0;
        Rd(a + 0xB440, b0);
        Rd(a + 0x50, p50);
        if (p50) Rd(p50 + 0x10, q10);
        const int rc = TenshinMoveCall(a, mv);
        Rd(a + 0xB440, b1);
        uint64_t q10b = 0;
        if (p50) Rd(p50 + 0x10, q10b);
        Log("TenshinMove: 66C4A8(state=%llX, %X) rc=%d state+B440 %u->%u [[state+50]+10] %llX->%llX", (unsigned long long)a, mv, rc, (unsigned)b0, (unsigned)b1,
            (unsigned long long)q10, (unsigned long long)q10b);
    }
    if (g_tenGo.load() > 0 && g_tenGo.fetch_sub(1) > 0) {
        uint32_t w = 0;
        if (Rd(a + 0x1068, w)) {
            uint32_t a68 = 0;
            Rd(a + 0xA68, a68);
            const uint32_t fm = g_tenForm.load();
            if (fm) WrU32(a + 0xA68, fm);
            const uint32_t sf = g_tenSetForm.load();
            if (sf) {
                uint64_t t1d8a = 0, t1d8b = 0;
                Rd(a + 0x1D8, t1d8a);
                WrU32(a + 0xA68, 0);  // 74C608 returns early (al=0, no pack install) when the new id == [state+A68] (opus RE, pc #47700)
                const int r1 = TenshinSetFormCall(a, sf, 0);
                const int ret1 = g_tenSfRet & 0xFF;
                const int r2 = TenshinSetFormCall(a, sf, 1);
                uint32_t a68b = 0;
                Rd(a + 0xA68, a68b);
                Rd(a + 0x1D8, t1d8b);
                Log("TenshinInput: SetForm(%X) 74C2FC fault=%d ret=%d 74C128 fault=%d A68 %X -> %X state+1D8 %llX -> %llX", sf, r1, ret1, r2, a68, a68b, (unsigned long long)t1d8a,
                    (unsigned long long)t1d8b);
            }
            Log("TenshinInput: A68 state=%llX %X -> %X", (unsigned long long)a, a68, fm ? fm : a68);
            WrU32(a + 0x1068, w | 0x4000);
            Log("TenshinInput: bit 0xE set state=%llX word %08X->%08X", (unsigned long long)a, w, w | 0x4000);
        }
    }
}

// ---- [Spirit] FormKey (notes/soulcore-transform-model.md, branch pc/soulcore-formkey) ----
// 1) log: when the soul-core summon chr is loaded, its body key 0x300000000+([chr]>>4), 1175400(key,3), [[chr+50]+8/+10], same for the player.
// 2) swap (once per arm): wait until the key's pack is loaded (133440 +2C8, 5 s), arm the 0x11793D0 call-site override (74C698 / 74C438)
//    to return that key once, then call the game's SetForm 74C2FC(state, X) with X != [state+A68]. All game calls are SEH-guarded.
std::atomic<int> g_fkOn{0};            // ini [Spirit] FormKey=1 or `rm spirit formkey on`
std::atomic<int> g_fkSwap{0};          // 1 = one swap armed (cleared after one use)
std::atomic<uint32_t> g_fkX{0};        // form id for 74C2FC (0 = auto: old [state+A68] after clearing A68, else 0x6E73)
std::atomic<uint64_t> g_fkMgr{0};      // spirit mgr seen at the 2916C5C entry
std::atomic<uint32_t> g_fkMgrId{0xFFFFFFFF};
std::atomic<uint64_t> g_fkLogged{0};   // spirit chr already logged
std::atomic<uint64_t> g_fkState{0};    // player input state seen at 2E6C34
std::atomic<uint64_t> g_fkPendKey{0};  // swap waiting for this key's pack
double g_fkPendT = 0;
int g_fkPendPolls = 0;
volatile uint64_t g_fkCell = 0;        // read by the call-site stubs: nonzero = return it once
volatile LONG g_fkHits = 0;
uintptr_t G_PTAB = 0;
// #54190: the Summoning Seal never runs 2916C5C (pc2 #54173). Its yokai is a plain 65A8A8 chr spawn (ally #54100): desc[0] = chr id
// (0x5129B), first motion from 65ADA6. FormKeySealDesc keeps the last desc[0]; FormKeySealNote (rematch SpiritSpawnNote) hands over the chr.
std::atomic<uint64_t> g_fkSealChr{0};
std::atomic<uint32_t> g_fkSealDesc{0}, g_fkSealMotion{0};
double g_fkSealT = -1;  // pc #56910: time of the last seal spawn hit (FormHide=4 falls back to it when FormSlot0 never captures, e.g. DOWN core A2ACB)
// ally #54309: a type-3 chr pack has no kind-3 model (1175400 = 0), but the seal chr carries its own model in slot 1 ([[chr+50]+10]).
// [Spirit] FormModel=1: keep the form's own key; only the model getter call 74C79D (call 1175400 in SetForm, result -> [[chr+50]+8+i*8]
// at 74C7B1) returns that seal model once, then the game's own Living Artifact request (0x4E, as Y+B) runs the transform.
std::atomic<int> g_fkModelMode{0};
volatile uint64_t g_fkMCell = 0;
volatile LONG g_fkMHits = 0;
double g_fkMArmT = 0;
uint64_t g_fkMState = 0, g_fkMVal = 0;
// pc #54384: [Spirit] FormPart. The LA body is part 47 of the parts container X=chara+0x9F0, loaded by F3478 at +F38AE
// (call F3D78(part, id, chara, 0); id = LA-row [row+0x10]; FED9E0 skips the reload when id == cached [part+0x20]).
// The hook logs every part-47 load of the player; FormPart=1: while FormModel is armed, the id becomes the seal yokai's body id
// [[schr+E8]+248]. FormPart=2: also rebuild the parts once (F3478(chara+9F0, 0)) if LA started and no part-47 load came.
std::atomic<int> g_fkPartMode{0};
std::atomic<uint64_t> g_fkMSchr{0};
volatile LONG g_fkPartHits = 0, g_fkPartSwaps = 0;
void* g_fkPartOrig = nullptr;
uint32_t FkBodyId(uint64_t chr) {  // 0xF26C4: [[chr+560]+4] if chr+560 set, else [[chr+E8]+248]
    uint64_t e8 = 0, a560 = 0;
    uint32_t v = 0;
    if (!chr || !Rd(chr + 0xE8, e8) || !e8) return 0;
    Rd(e8 + 0x248, v);
    if (Rd(chr + 0x560, a560) && a560) Rd(a560 + 4, v);
    return v;
}
uint32_t FkSealBody(uint64_t schr) {  // pc #54384: [[schr+E8]+248]
    uint64_t e8 = 0;
    uint32_t v = 0;
    return schr && Rd(schr + 0xE8, e8) && e8 && Rd(e8 + 0x248, v) ? v : 0;
}
// pc #54368: [Spirit] FormAppear. Appearance block blk=[chr+560] (ctor 919600: +0 chr id, +4 body id read by F26C4, +8 -1, +C record
// 0x530 bytes, +804 load state, +814 dirty bytes). ED584 reloads the parts when byte +814 != 0; the player writer 484EB8 copies the record
// then sets +814=0x01010101 (4858EC). FormAppear=1: once per seal spawn, the seal yokai's body id [[schr+E8]+248] goes into the player's
// [blk+4] and +814=0x01010101. `rm spirit formkey appear off` puts the old id back (and +814 again) on the next game-thread input call.
// pc #54571 v2 (pc2 static trace, dump899): blk state machine EE310 (called from ED584 each update):
//   0 -(dl && +80E==0: byte +814=1, EE7B4(chrId)?2:1)-> 1 | 1: ED79A consumes +814 (2C4450 part load, then clears byte +814), EE310 84D540 -> 2
//   3: waits the 3 handles at +6D8 | 4 -> 5 | 6 (player steady state): +80E||+80F -> 8, else EE7B4 -> 0xB | 8 -> 0xA (EDFB6) | 0xA -> 0 (EDE94)
// The only writer of 1 is EE553 (state 0). The game's own reload request is 846B3C(obj,id,force) (called by 484EB8 at 485720): it sets
// blk bytes +810=1 +80F=1 +811/+812=1; 20943B4 is a bare `+80F=1` setter. So the v2 kick (FormAppearKick, after the id write):
//   1 = set +80F/+810/+811/+812 like 846B3C (the game walks 6->8->0xA->0->1 and consumes +814), 2 = force +804=1 (skips 0/8/0xA), 0 = none.
// Only the FIRST seal chr of each arm is used (later seal figures like EE49E are skipped). Unproven: that the part load reads [blk+4]
// (2C4450 gets the slot kind from the record via 903620; the body may come only from the record at +C).
std::atomic<int> g_fkAppearMode{0};
std::atomic<int> g_fkAppKick{1};
std::atomic<int> g_fkAppRestore{0};
std::atomic<uint64_t> g_fkAppSeal{0};  // first seal chr of this arm (0 = not yet applied)
uint64_t g_fkAppBlk = 0;
uint32_t g_fkAppOld = 0, g_fkAppNew = 0;
// pc #54739 / cx-pc2 #54650: FormAppear=3 also writes blk+0 (the chr id EE7B4 keys the model request on) = seal chrId [schr]>>4 before the kick
uint32_t g_fkAppOld0 = 0, g_fkAppNew0 = 0;
double g_fkAppT = 0;
int g_fkAppStep = 0;
uint32_t g_fkAppLastSt = 0xFFFFFFFF;
void FkAppearLog(const char* tag, uint64_t chr) {
    uint64_t e8 = 0, blk = 0;
    uint32_t key = 0, row = 0, b0 = 0, b4 = 0, b8 = 0, st = 0, dirty = 0, r[4] = {};
    uint8_t fl[5] = {};  // +80E..+812
    Rd(chr, key);
    if (Rd(chr + 0xE8, e8) && e8) Rd(e8 + 0x248, row);
    if (Rd(chr + 0x560, blk) && blk) {
        Rd(blk, b0);
        Rd(blk + 4, b4);
        Rd(blk + 8, b8);
        Rd(blk + 0x804, st);
        Rd(blk + 0x814, dirty);
        for (int i = 0; i < 5; ++i) Rd(blk + 0x80E + i, fl[i]);
        for (int i = 0; i < 4; ++i) Rd(blk + 0xC + 4ull * i, r[i]);
    }
    Log("FormKey APPEAR %s chr=%llX chrId=[chr]>>4=%X F26C4=%X [chr+560]=%llX [[chr+560]+4]=%X [[chr+E8]+248]=%X blk+0=%X blk+8=%X state+804=%u dirty+814=%08X +80E..812=%u%u%u%u%u rec+C[0..3]=%X %X %X %X t=%.3f",
        tag, (unsigned long long)chr, key >> 4, FkBodyId(chr), (unsigned long long)blk, b4, row, b0, b8, st, dirty, fl[0], fl[1], fl[2], fl[3], fl[4],
        r[0], r[1], r[2], r[3], Now());
}
bool FkAppearWrite(uint64_t blk, uint32_t id, int kick, uint32_t id0 = 0) {
    __try {
        if (id0 && !WrU32(blk, id0)) return false;
        if (!WrU32(blk + 4, id) || !WrU32(blk + 0x814, 0x01010101)) return false;
        if (kick == 1) {  // 846B3C's reload request: +810=1, +80F=1, +811/+812=1
            const uint8_t one = 1;
            return WrBytes(blk + 0x810, &one, 1) && WrBytes(blk + 0x80F, &one, 1) && WrBytes(blk + 0x811, &one, 1) && WrBytes(blk + 0x812, &one, 1);
        }
        if (kick == 2) return WrU32(blk + 0x804, 1);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
void FkAppearApply(uint64_t pchr, uint64_t schr) {
    uint64_t blk = 0;
    const uint32_t sb = FkSealBody(schr);
    uint32_t schrKey = 0;
    Rd(schr, schrKey);
    const uint64_t first = g_fkAppSeal.load();
    if (first && first != schr) {
        Log("FormKey APPEAR skip: later seal chr=%llX chrId=%X (first seal chr=%llX already applied)", (unsigned long long)schr, schrKey >> 4, (unsigned long long)first);
        return;
    }
    if (!pchr || !Rd(pchr + 0x560, blk) || !blk) { Log("FormKey APPEAR skip: player has no appearance block [chr+560]"); return; }
    if (!sb || sb == 0xFFFFFFFF) { Log("FormKey APPEAR skip: seal body id [[schr+E8]+248]=%X", sb); return; }
    uint32_t cur = 0;
    Rd(blk + 4, cur);
    if (cur == sb) { Log("FormKey APPEAR skip: player [blk+4] already %X", sb); return; }
    g_fkAppSeal.store(schr);
    uint32_t cur0 = 0;
    Rd(blk, cur0);
    if (g_fkAppBlk != blk) { g_fkAppBlk = blk; g_fkAppOld = cur; g_fkAppOld0 = cur0; }
    const int kick = g_fkAppKick.load();
    const uint32_t id0 = g_fkAppearMode.load() == 3 ? schrKey >> 4 : 0;
    FkAppearLog("player before write", pchr);
    const bool ok = FkAppearWrite(blk, sb, kick, id0);
    g_fkAppNew = sb;
    g_fkAppNew0 = id0;
    if (id0) Log("FormKey APPEAR3 blk+0 %X -> %X (seal chrId) before kick", cur0, id0);
    g_fkAppT = Now();
    g_fkAppStep = 0;
    g_fkAppLastSt = 0xFFFFFFFF;
    Log("FormKey APPEAR WRITE first seal chr=%llX chrId=%X -> player blk=%llX [blk+4] %X -> %X, +814=01010101, kick=%d (%s) ok=%d (restore id %X)",
        (unsigned long long)schr, schrKey >> 4, (unsigned long long)blk, cur, sb, kick,
        kick == 1 ? "+80F/+810/+811/+812=1" : kick == 2 ? "+804=1" : "none", ok, g_fkAppOld);
    FkAppearLog("player after write", pchr);
}
void FkAppearTick(uint64_t pchr) {  // game thread (2E6C34 input hook)
    if (g_fkAppT) {
        const double dt = Now() - g_fkAppT;
        uint64_t blk = 0;
        uint32_t st = 0xFFFFFFFF;
        if (Rd(pchr + 0x560, blk) && blk) Rd(blk + 0x804, st);
        if (st != g_fkAppLastSt) {  // every state change in the first 5 s
            g_fkAppLastSt = st;
            char t[48];
            sprintf_s(t, "player state change +%.3f s", dt);
            FkAppearLog(t, pchr);
        }
        static const double marks[3] = {0.1, 1.0, 3.0};
        static const char* const names[3] = {"player +0.1 s", "player +1 s", "player +3 s"};
        if (g_fkAppStep < 3 && dt >= marks[g_fkAppStep]) FkAppearLog(names[g_fkAppStep++], pchr);
        if (dt > 5.0) g_fkAppT = 0;
    }
    if (g_fkAppRestore.exchange(0) && g_fkAppBlk) {
        const bool ok = FkAppearWrite(g_fkAppBlk, g_fkAppOld, g_fkAppKick.load(), g_fkAppNew0 ? g_fkAppOld0 : 0);
        if (g_fkAppNew0) Log("FormKey APPEAR3 RESTORE blk+0 %X -> %X", g_fkAppNew0, g_fkAppOld0);
        g_fkAppNew0 = 0;
        Log("FormKey APPEAR RESTORE blk=%llX [blk+4] %X -> %X kick=%d ok=%d", (unsigned long long)g_fkAppBlk, g_fkAppNew, g_fkAppOld, g_fkAppKick.load(), ok);
        g_fkAppBlk = 0;
        g_fkAppSeal.store(0);
        g_fkAppT = Now();
        g_fkAppStep = 0;
        g_fkAppLastSt = 0xFFFFFFFF;
        FkAppearLog("player after restore", pchr);
    }
}
uint64_t FkPlayer();
// pc #54739 / cx-pc2 #54695: [Spirit] PartProbe=1 logs every part-id staging F3D78(slot, id, chr, flag) from all 10 call sites in F3478
// (site = return RVA - 5) and every chr-level part apply 2C4450(chr, kind, state+3) from its 8 callers, for every chr (player, seal yokai, ...).
std::atomic<int> g_ppOn{0};
volatile LONG g_ppF3Hits = 0, g_ppC4Hits = 0, g_ppLines = 0;
void* g_ppF3Orig = nullptr;
void* g_ppC4Orig = nullptr;
// pc2 #54828: the seal yokai 5129B stages exactly one real part, kind 0 id 0x40244A, through the generic F35AD loop (all its other slots -1).
// [Spirit] FormSlot0 (pc #54830, needs FormKey=1; installs the F3D78 call-site hooks even with PartProbe=0). While the seal is armed (its chr staged
// a kind-0 id), at F3D78 for the PLAYER chr: 1 = the first kind-0 slot (lowest slot address) gets the seal id, other slots unchanged;
// 2 = the same, plus the other kind-0 slots and kinds 1/2/3/D/E get -1 (hide the human). 0.3 s after the capture every player slot seen gets
// cached [slot+20] = -1 (the game's own reload trigger, FED9E0 skips when id == cached) and the parts are rebuilt once (F3478(chr+9F0,0)).
// Restore: 60 s after the capture, LA end, or pipe `rm spirit formkey slot0 off` - swap off, cached -1 again, rebuild (original ids reload).
// Pipe `rm spirit formkey slot0 <n>` sets the mode (re-arms for the next seal chr).
std::atomic<int> g_fs0Mode{0};
std::atomic<uint32_t> g_fs0Id{0};
std::atomic<int> g_fs0Active{0}, g_fs0Restore{0};
std::atomic<uint64_t> g_fs0CapChr{0}, g_fs0DoneChr{0};
volatile LONG g_fs0Swaps = 0, g_fs0Clears = 0, g_fs0Lines = 0, g_fs0N = 0;
double g_fs0CapT = 0;
uint64_t g_fs0Slot[64];
uint32_t g_fs0Kind[64];
std::atomic<uint64_t> g_fs0FirstK0{0};
uint64_t g_fs0SlotChr = 0;  // pc #56639: the player chr the noted slots belong to
void Fs0Note(uint64_t slot, uint32_t kind, uint64_t chr) {  // remember every player part slot (for the cached reset); first kind-0 = lowest address
    // inside the player chr block (chr+9F0 parts, e.g. chr+A10); ally #55049: the player also stages slots elsewhere (#33 1447453B0) which the
    // F3478 rebuild never re-stages, and the old global-lowest rule picked that one (0 swaps)
    if (chr != g_fs0SlotChr) { g_fs0SlotChr = chr; g_fs0N = 0; g_fs0FirstK0.store(0); }  // pc #56639: the noted slots belong to one player chr
    const LONG n = g_fs0N;
    for (LONG i = 0; i < n && i < 64; ++i)
        if (g_fs0Slot[i] == slot) return;
    if (n >= 64) return;
    g_fs0Kind[n] = kind;
    g_fs0Slot[n] = slot;
    InterlockedIncrement(&g_fs0N);
    if (kind == 0 && slot > chr && slot < chr + 0x40000) {
        uint64_t f = g_fs0FirstK0.load();
        while ((!f || slot < f) && !g_fs0FirstK0.compare_exchange_weak(f, slot)) {}
    }
    Log("FormSlot0 player slot #%ld %llX kind=%X%s", n, (unsigned long long)slot, kind, kind == 0 && g_fs0FirstK0.load() == slot ? " (first kind-0 so far)" : "");
}
const char* PpWho(uint64_t chr, uint64_t pl) {
    if (!chr) return "null";
    if (chr == pl) return "PLAYER";
    if (chr == g_fkSealChr.load() || chr == g_fkAppSeal.load() || chr == g_fkMSchr.load()) return "SEAL";
    return "other";
}
void PpLogF3(uint64_t slot, uint32_t id, uint64_t chr, uint64_t r9, uint64_t ret) {
    const LONG n = InterlockedIncrement(&g_ppF3Hits);
    if (InterlockedIncrement(&g_ppLines) > 4000) return;
    const uint64_t pl = FkPlayer();
    uint32_t kind = 0, cached = 0, key = 0;
    Rd(slot, kind);
    Rd(slot + 0x20, cached);
    if (chr) Rd(chr, key);
    Log("PartProbe F3D78 #%ld site=+%llX who=%s chr=%llX chrId=%X slot=%llX kind[slot]=%X cached[slot+20]=%X id=%X flag=%u t=%.3f", n,
        (unsigned long long)(ret - g_base - 5), PpWho(chr, pl), (unsigned long long)chr, key >> 4, (unsigned long long)slot, kind, cached, id,
        (unsigned)(r9 & 0xFF), Now());
}
// pc #54925: [Spirit] SlotWatch=1 - hardware write watch on a staged player kind-0 slot (DR0 = byte [slot+2F8] pending flag, DR1 = dword
// [slot+40] pending id) on every thread for 4 s; the VEH records each writer (RIP after the write + 6 unwound frames). Armed once at boot
// (the first real player kind-0 staging, chrId 0x64 = the normal flow) and once after the FormSlot0 apply. The writer that CLEARS +2F8 = commit.
std::atomic<int> g_swMode{0};
struct SwHit { uint64_t addr, fr[7]; uint32_t v40; uint8_t f2f8; double t; };
SwHit g_swHits[96];
volatile LONG g_swN = 0, g_swBusy = 0;
volatile uint64_t g_swSlot = 0;
PVOID g_swVeh = nullptr;
LONG CALLBACK SwVeh(EXCEPTION_POINTERS* e) {
    if (e->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP) return EXCEPTION_CONTINUE_SEARCH;
    CONTEXT* c = e->ContextRecord;
    const uint64_t dr6 = c->Dr6, slot = g_swSlot;
    if (!(dr6 & 3) || !slot) return EXCEPTION_CONTINUE_SEARCH;
    const LONG i = InterlockedIncrement(&g_swN) - 1;
    if (i < 96) {
        SwHit& h = g_swHits[i];
        h.addr = (dr6 & 1) ? slot + 0x2F8 : slot + 0x40;
        h.t = Now();
        Rd(slot + 0x40, h.v40);
        Rd(slot + 0x2F8, h.f2f8);
        for (int f = 0; f < 7; ++f) h.fr[f] = 0;
        CONTEXT k = *c;
        for (int f = 0; f < 7; ++f) {
            h.fr[f] = k.Rip;
            DWORD64 ib = 0;
            PRUNTIME_FUNCTION rf = RtlLookupFunctionEntry(k.Rip, &ib, nullptr);
            if (!rf) {
                uint64_t r = 0;
                if (!Rd(k.Rsp, r)) break;
                k.Rip = r;
                k.Rsp += 8;
            } else {
                PVOID hd = nullptr;
                DWORD64 ef = 0;
                RtlVirtualUnwind(UNW_FLAG_NHANDLER, ib, k.Rip, rf, &k, &hd, &ef, nullptr);
            }
            if (!k.Rip) break;
        }
    }
    c->Dr6 = 0;
    return EXCEPTION_CONTINUE_EXECUTION;
}
int SwSetAll(bool on, uint64_t slot) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return -1;
    THREADENTRY32 te{};
    te.dwSize = sizeof(te);
    const DWORD pid = GetCurrentProcessId(), me = GetCurrentThreadId();
    int n = 0;
    for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
        if (te.th32OwnerProcessID != pid || te.th32ThreadID == me) continue;
        HANDLE h = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
        if (!h) continue;
        if (SuspendThread(h) != (DWORD)-1) {
            CONTEXT c{};
            c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
            if (GetThreadContext(h, &c)) {
                c.Dr7 &= ~0x00FF000Full;
                if (on) {
                    c.Dr0 = slot + 0x2F8;
                    c.Dr1 = slot + 0x40;
                    c.Dr7 |= 1ull | 4ull | (1ull << 16) | (1ull << 20) | (3ull << 22);  // L0 L1; RW0=write LEN0=1 byte; RW1=write LEN1=4 bytes
                } else {
                    c.Dr0 = c.Dr1 = 0;
                }
                if (SetThreadContext(h, &c)) ++n;
            }
            ResumeThread(h);
        }
        CloseHandle(h);
    }
    CloseHandle(snap);
    return n;
}
DWORD WINAPI SwThread(LPVOID tagp) {
    const char* tag = (const char*)tagp;
    const uint64_t slot = g_swSlot;
    Sleep(2);  // let the arming F3D78 call finish its own staging writes
    if (!g_swVeh) g_swVeh = AddVectoredExceptionHandler(1, &SwVeh);
    InterlockedExchange(&g_swN, 0);
    uint32_t v40 = 0;
    uint8_t f = 0;
    Rd(slot + 0x40, v40);
    Rd(slot + 0x2F8, f);
    const int n = SwSetAll(true, slot);
    Log("SlotWatch %s ARMED slot=%llX on %d threads ([slot+2F8]=%u [slot+40]=%X) t=%.3f", tag, (unsigned long long)slot, n, f, v40, Now());
    Sleep(4000);
    const int m = SwSetAll(false, slot);
    const LONG hits = g_swN;
    Log("SlotWatch %s DISARMED (%d threads) hits=%ld", tag, m, hits);
    for (LONG i = 0; i < hits && i < 96; ++i) {
        const SwHit& h = g_swHits[i];
        char fr[256];
        int o = 0;
        fr[0] = 0;
        for (int k = 0; k < 7 && h.fr[k] && o < 200; ++k) {
            const uint64_t a = h.fr[k];
            if (a >= g_base && a < g_base + 0x8000000) o += sprintf_s(fr + o, sizeof(fr) - o, " +%llX", (unsigned long long)(a - g_base));
            else o += sprintf_s(fr + o, sizeof(fr) - o, " %llX", (unsigned long long)a);
        }
        Log("SlotWatch %s hit #%ld write %s -> now +2F8=%u +40=%X t=%.3f rip+stack:%s", tag, i, h.addr == slot + 0x2F8 ? "+2F8" : "+40", h.f2f8, h.v40, h.t, fr);
    }
    g_swSlot = 0;
    InterlockedExchange(&g_swBusy, 0);
    return 0;
}
void SwArm(uint64_t slot, const char* tag) {
    if (!g_swMode.load() || !slot || InterlockedCompareExchange(&g_swBusy, 1, 0) != 0) return;
    g_swSlot = slot;
    HANDLE t = CreateThread(nullptr, 0, &SwThread, (LPVOID)tag, 0, nullptr);
    if (t) CloseHandle(t);
    else { g_swSlot = 0; InterlockedExchange(&g_swBusy, 0); }
}
// pc #54959: [Spirit] SlotProbe=1 - for 3 s after the FormSlot0 apply, sample the player's first kind-0 slot (and the seal's kind-0 slot for
// contrast) every ~10 ms on a side thread: state [slot+18] (0 idle 1 requested 2/3/4 building 5 = no asset), +2F8 staged, +2F9 defer, +40 staged
// id, +20 cached id, +48 asset key (from 406668), +10 live resource, +328 visual handle; a line on every change. Plus 406668([G+58], id) with
// G = [+45B9E30] (the part/asset DB lookup 3FD610 uses at +3FE11F) for the seal id and the player's original kind-0 id: rax 0 => state 5, nothing loads.
std::atomic<int> g_spMode{0};
std::atomic<uint64_t> g_spSealSlot{0};
std::atomic<uint32_t> g_spOrigId{0};
volatile LONG g_spBusy = 0;
uint64_t SpLookup(uint32_t id, uint32_t* key0) {
    uint64_t g = 0, db = 0;
    *key0 = 0;
    if (!Rd(g_base + 0x45B9E30, g) || !g || !Rd(g + 0x58, db) || !db) return 0;
    uint64_t r = 0;
    __try {
        r = ((uint64_t(*)(uint64_t, uint32_t))(g_base + 0x406668))(db, id);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 1;
    }
    if (r) Rd(r + 0x3C, *key0);
    return r;
}
void SpLookupLog(const char* tag) {
    const uint32_t ids[2] = {g_fs0Id.load(), g_spOrigId.load()};
    for (int i = 0; i < 2; ++i) {
        if (!ids[i] || ids[i] == 0xFFFFFFFF) continue;
        uint32_t k = 0;
        const uint64_t r = SpLookup(ids[i], &k);
        Log("SlotProbe %s 406668(DB, %s id %X) = %llX%s key[+3C]=%X t=%.3f", tag, i ? "player orig" : "seal", ids[i], (unsigned long long)r,
            r == 0 ? " NULL -> state 5 (asset not in DB)" : r == 1 ? " (fault)" : "", k, Now());
    }
}
struct SpSnap { uint32_t st, v40, v20, v48; uint8_t f8, f9; uint64_t p10, p328; };
bool SpRead(uint64_t slot, SpSnap& s) {
    s = {};
    return slot && Rd(slot + 0x18, s.st) && Rd(slot + 0x40, s.v40) && Rd(slot + 0x20, s.v20) && Rd(slot + 0x48, s.v48) && Rd(slot + 0x2F8, s.f8) &&
           Rd(slot + 0x2F9, s.f9) && Rd(slot + 0x10, s.p10) && Rd(slot + 0x328, s.p328);
}
DWORD WINAPI SpThread(LPVOID) {
    const uint64_t slots[2] = {g_fs0FirstK0.load(), g_spSealSlot.load()};
    const char* nm[2] = {"PLAYER", "SEAL"};
    SpSnap last[2];
    bool have[2] = {false, false};
    int lines = 0;
    const double t0 = Now();
    Log("SlotProbe start player slot=%llX seal slot=%llX seal id=%X player orig id=%X t=%.3f", (unsigned long long)slots[0], (unsigned long long)slots[1],
        g_fs0Id.load(), g_spOrigId.load(), t0);
    SpLookupLog("start");
    int samples = 0, st5 = 0;
    while (Now() - t0 < 3.0) {
        for (int i = 0; i < 2; ++i) {
            SpSnap s;
            if (!SpRead(slots[i], s)) continue;
            if (i == 0) { ++samples; if (s.st == 5) ++st5; }
            if (!have[i] || memcmp(&s, &last[i], sizeof(s)) != 0) {
                if (++lines <= 250)
                    Log("SlotProbe %s slot=%llX state[+18]=%u +2F8=%u +2F9=%u +40=%X +20=%X +48=%X +10=%llX +328=%llX dt=%.3f", nm[i], (unsigned long long)slots[i],
                        s.st, s.f8, s.f9, s.v40, s.v20, s.v48, (unsigned long long)s.p10, (unsigned long long)s.p328, Now() - t0);
                last[i] = s;
                have[i] = true;
            }
        }
        Sleep(10);
    }
    SpLookupLog("end");
    {   // ally #55089: the player slot commits a new 40244A obj but the body stays human - dump both slot objects (player vs seal) to diff
        for (int i = 0; i < 2; ++i) {
            uint64_t o = 0;
            if (!slots[i] || !Rd(slots[i] + 0x10, o) || !o) { Log("SlotProbe OBJ %s [slot+10]=0", nm[i]); continue; }
            char h[0x40 * 3 + 4];
            for (uint64_t off = 0; off < 0x400; off += 0x40) {
                HexSafe(h, sizeof(h), o + off, 0x40);
                Log("SlotProbe OBJ %s %llX+%03llX %s", nm[i], (unsigned long long)o, (unsigned long long)off, h);
            }
        }
    }
    Log("SlotProbe done: %d player samples, %d in state 5, final player state=%u +40=%X +10=%llX lines=%d", samples, st5, last[0].st, last[0].v40,
        (unsigned long long)last[0].p10, lines);
    InterlockedExchange(&g_spBusy, 0);
    return 0;
}
void SpArm() {
    if (!g_spMode.load() || InterlockedCompareExchange(&g_spBusy, 1, 0) != 0) return;
    HANDLE t = CreateThread(nullptr, 0, &SpThread, nullptr, 0, nullptr);
    if (t) CloseHandle(t);
    else InterlockedExchange(&g_spBusy, 0);
}
uint64_t HkPpF3D78(uint64_t slot, uint32_t id, uint64_t chr, uint64_t r9) {
    if (g_swMode.load() && id != 0xFFFFFFFF && chr) {
        static bool bootArmed = false;
        uint32_t kind = 0xFFFFFFFF, key = 0;
        Rd(slot, kind);
        Rd(chr, key);
        if (!bootArmed && kind == 0 && (key >> 4) == 0x64) { bootArmed = true; SwArm(slot, "boot"); }
    }
    if (g_ppOn.load()) PpLogF3(slot, id, chr, r9, (uint64_t)_ReturnAddress());
    if (const int m = g_fs0Mode.load(); m && chr) {
        uint32_t kind = 0xFFFFFFFF;
        Rd(slot, kind);
        if (chr == g_fkSealChr.load() || chr == g_fkAppSeal.load()) {
            if (kind == 0 && id != 0xFFFFFFFF && chr != g_fs0DoneChr.load() && g_fs0CapChr.exchange(chr) != chr) {
                g_fs0Id.store(id);
                g_spSealSlot.store(slot);
                g_fs0CapT = Now();
                g_fs0Active.store(1);
                Log("FormSlot0 CAPTURE seal chr=%llX kind-0 id=%X mode=%d (player slots known %ld, first kind-0 %llX) t=%.3f", (unsigned long long)chr, id, m,
                    g_fs0N, (unsigned long long)g_fs0FirstK0.load(), g_fs0CapT);
            }
        } else if (chr == FkPlayer()) {
            Fs0Note(slot, kind, chr);
            if (const uint32_t to = g_fs0Id.load(); to && g_fs0Active.load() && m != 9) {  // 9 = capture only (pc2 FormHide control: no player swap)
                uint32_t nid = id;
                if (kind == 0 && slot == g_fs0FirstK0.load()) {
                    nid = to;
                    if (id != to && id != 0xFFFFFFFF) g_spOrigId.store(id);
                }
                else if (m == 2 && (kind <= 3 || kind == 0xD || kind == 0xE)) nid = 0xFFFFFFFF;
                if (nid != id) {
                    InterlockedIncrement(nid == to ? &g_fs0Swaps : &g_fs0Clears);
                    if (InterlockedIncrement(&g_fs0Lines) <= 300)
                        Log("FormSlot0 player slot=%llX kind=%X id %X -> %X (mode %d) t=%.3f", (unsigned long long)slot, kind, id, nid, m, Now());
                    id = nid;
                }
            }
        }
    }
    return ((uint64_t(*)(uint64_t, uint32_t, uint64_t, uint64_t))g_ppF3Orig)(slot, id, chr, r9);
}
uint64_t HkPp2C4450(uint64_t chr, uint32_t kind, uint32_t st, uint64_t r9) {
    if (g_ppOn.load()) {
        const LONG n = InterlockedIncrement(&g_ppC4Hits);
        if (InterlockedIncrement(&g_ppLines) <= 4000) {
            const uint64_t pl = FkPlayer();
            uint32_t key = 0, c20 = 0, c1fc = 0;
            Rd(chr, key);
            Rd(chr + 0x20, c20);
            Rd(chr + 0x1FC, c1fc);
            void* bt[8] = {};
            const USHORT nb = RtlCaptureStackBackTrace(1, 8, bt, nullptr);
            char st2[160];
            int o = 0;
            st2[0] = 0;
            for (USHORT k = 0; k < nb && o < 140; ++k) {
                const uint64_t a = (uint64_t)bt[k];
                if (a >= g_base && a < g_base + 0x8000000) o += sprintf_s(st2 + o, sizeof(st2) - o, " +%llX", (unsigned long long)(a - g_base));
            }
            Log("PartProbe 2C4450 #%ld stack:%s", n, st2);
            Log("PartProbe 2C4450 #%ld site=+%llX who=%s chr=%llX chrId=%X kind(edx)=%X r8d=%X [chr+20]=%X [chr+1FC]=%X%s t=%.3f", n,
                (unsigned long long)((uint64_t)_ReturnAddress() - g_base - 5), PpWho(chr, pl), (unsigned long long)chr, key >> 4, kind, st, c20, c1fc,
                c1fc == kind ? " (skip: same)" : "", Now());
        }
    }
    return ((uint64_t(*)(uint64_t, uint32_t, uint32_t, uint64_t))g_ppC4Orig)(chr, kind, st, r9);
}
void PartProbeInstall() {
    static std::atomic<int> done{0};
    if (done.exchange(1)) return;
    if (!g_base) g_base = g_img.base;
    static const uint32_t kF3[] = {0xF35AD, 0xF38E9, 0xF396C, 0xF39A9, 0xF39D7, 0xF3BA0, 0xF3C8B, 0xF3CBE, 0xF3D62};  // +F38AE = HkPart47 (FormKey)
    static const uint32_t kC4[] = {0xED7CC, 0xEDB56, 0xEDB71, 0x902DD0, 0x902E40, 0x206F805, 0x206F830, 0x206F85B};
    int nf = 0, nc = 0;
    auto one = [&](uint32_t rva, uint32_t tgt, void* fn, void** orig, const char* name) {
        uint8_t* p = (uint8_t*)(g_base + rva);
        uint8_t b[5] = {};
        int32_t rel = 0;
        if (Copy(b, (uint64_t)p, 5)) memcpy(&rel, b + 1, 4);
        if (b[0] != 0xE8 || (uint64_t)(p + 5 + rel) != g_base + tgt) { Log("PartProbe: +%X is not 'call %X' - skipped", rva, tgt); return false; }
        const char* w = nullptr;
        void* o = nullptr;
        if (!InstallCallRetargetAt(p, name, fn, &o, &w)) { Log("PartProbe: +%X failed: %s", rva, w ? w : "?"); return false; }
        *orig = o;
        return true;
    };
    for (uint32_t r : kF3) nf += one(r, 0xF3D78, (void*)&HkPpF3D78, &g_ppF3Orig, "call:PartProbeF3D78");
    for (uint32_t r : kC4) nc += one(r, 0x2C4450, (void*)&HkPp2C4450, &g_ppC4Orig, "call:PartProbe2C4450");
    Log("PartProbe: F3D78 hooked on %d/9 sites (+F38AE via FormKey part-47 hook), 2C4450 on %d/8 callers", nf, nc);
}
uint64_t HkPart47(uint64_t part, uint32_t id, uint64_t chr, uint64_t r9) {
    if (g_ppOn.load()) PpLogF3(part, id, chr, r9, (uint64_t)_ReturnAddress());
    const uint32_t in = id;
    if (g_fkOn.load()) {
        const uint64_t pl = FkPlayer();
        const LONG n = InterlockedIncrement(&g_fkPartHits);
        const uint64_t schr = g_fkMSchr.load();
        const bool armed = g_fkMVal != 0;
        const uint32_t sb = FkSealBody(schr);
        if (armed && g_fkPartMode.load() && chr && chr == pl && sb && sb != 0xFFFFFFFF && id != 0xFFFFFFFF && id != sb) {
            id = sb;
            InterlockedIncrement(&g_fkPartSwaps);
        }
        static uint32_t lastIn = 0xFFFFFFFE;
        if (n <= 40 || in != lastIn || id != in) {
            uint32_t cached = 0;
            Rd(part + 0x20, cached);
            Log("FormKey PART47 #%ld part=%llX chr=%llX player=%d id=%X%s%X cached[part+20]=%X armed=%d FormPart=%d F26C4(player)=%X F26C4(seal)=%X sealBody[[schr+E8]+248]=%X schr=%llX t=%.3f",
                n, (unsigned long long)part, (unsigned long long)chr, chr && chr == pl, in, id != in ? " -> REPLACED " : " kept ", id, cached, armed,
                g_fkPartMode.load(), FkBodyId(pl), FkBodyId(schr), sb, (unsigned long long)schr, Now());
            lastIn = in;
        }
    }
    return ((uint64_t(*)(uint64_t, uint32_t, uint64_t, uint64_t))g_fkPartOrig)(part, id, chr, r9);
}
bool FkRebuildParts(uint64_t chr) {
    __try {
        ((void (*)(uint64_t, uint64_t))(g_base + 0xF3478))(chr + 0x9F0, 0);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
void Fs0Reload(uint64_t pchr, const char* why) {  // the game's reload trigger on every player slot, then one parts rebuild
    const LONG n = g_fs0N < 64 ? g_fs0N : 64;
    const uint32_t m1 = 0xFFFFFFFF;
    int w = 0;
    for (LONG i = 0; i < n; ++i)
        if (WrBytes(g_fs0Slot[i] + 0x20, &m1, 4)) ++w;
    const LONG s0 = g_fs0Swaps, c0 = g_fs0Clears;
    const bool ok = FkRebuildParts(pchr);
    Log("FormSlot0 %s: cached[slot+20]=-1 on %d/%ld slots, rebuild F3478(chr+9F0,0) ok=%d swaps %ld -> %ld clears %ld -> %ld t=%.3f", why, w, n, ok, s0,
        g_fs0Swaps, c0, g_fs0Clears, Now());
    FkAppearLog(why, pchr);
}
// pc #54948: [Spirit] FormApply - after the FormSlot0 swap + rebuild, run the chr-level part apply 2C4450(obj, 0, -1) the way 902C00 applies
// the seal yokai's own part object (902E40: rcx = [slot+10] = part object made by 902C00 from the slot, edx = 2C68B4 kind (0 for 40244A),
// r8d = -1). 2C4450 skips when [obj+1FC] == kind (except chr ids 83DB1 / 909C3), so [obj+1FC] is set to -1 first. FormApply=1: target = the
// player's first kind-0 slot object [slot+10] (player chr if that is null); 2: target = the player chr. Two tries: 0.1 s and 1.0 s after the
// rebuild. Logs the slot state, target id ([obj]>>4), +1FC before/after and [slot+10] after.
std::atomic<int> g_faMode{0};
std::atomic<int> g_foxMode{0};  // ally #55149 FormObjFix
std::atomic<int> g_fzsMode{0};  // pc #55150 FormZeroSlot
bool FaCall(uint64_t obj) {
    __try {
        ((void (*)(uint64_t, uint32_t, uint32_t))(g_base + 0x2C4450))(obj, 0, 0xFFFFFFFFu);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
void FaApply(uint64_t pchr, int round) {
    const uint64_t slot = g_fs0FirstK0.load();
    uint64_t obj = 0;
    uint32_t st = 0, v40 = 0, v20 = 0;
    if (slot) { Rd(slot + 0x10, obj); Rd(slot + 0x18, st); Rd(slot + 0x40, v40); Rd(slot + 0x20, v20); }
    const int m = g_faMode.load();
    const uint64_t tgt = m == 2 || !obj ? pchr : obj;
    uint32_t key = 0, k1fc = 0, a1fc = 0;
    Rd(tgt, key);
    Rd(tgt + 0x1FC, k1fc);
    const bool w = WrU32(tgt + 0x1FC, 0xFFFFFFFFu);
    const bool ok = FaCall(tgt);
    Rd(tgt + 0x1FC, a1fc);
    uint64_t obj2 = 0;
    if (slot) Rd(slot + 0x10, obj2);
    Log("FormApply #%d mode=%d slot=%llX state[+18]=%u +40=%X +20=%X obj[+10]=%llX -> target=%llX (%s) id[t]>>4=%X +1FC %X -> -1 (w%d) -> call 2C4450(t,0,-1) ok=%d +1FC after=%X obj after=%llX t=%.3f",
        round, m, (unsigned long long)slot, st, v40, v20, (unsigned long long)obj, (unsigned long long)tgt, tgt == pchr ? "player chr" : "slot obj", key >> 4,
        k1fc, w, ok, a1fc, (unsigned long long)obj2, Now());
}
// pc #55034: [Spirit] FormCommit=1 - the seal yokai's kind-0 slot is committed by the per-frame appearance tick 3FD37C(chr, dl force, r8b)
// -> 3FD3E8(chr+9F0) -> 3FD610(slot) state machine (yokai stack +3FD482 <- +3FD3E2 <- +F2D07). The player's staged slot never gets there
// (pc2 #55031: 0 writes to +2F8 in 4 s). F2D02 is gated by chr+106 / [chr+AB84]==-1 / chr+886 / chr+A02 / chr+B313. FormCommit: (a) all 20
// 3FD37C call sites are retargeted to log the chr walk (first 80 distinct chrs + every player call, site, dl, r8b); (b) for ~1 s after the
// FormSlot0 kick, call 3FD37C(player, 1, 0) once per game frame and log the first kind-0 slot [+18]/+2F8/+2F9/+40/+20/+10/+328 before/after.
std::atomic<int> g_fcMode{0};
void* g_fcOrig = nullptr;
volatile LONG g_fcLines = 0, g_fcPlayerCalls = 0, g_fcCalls = 0, g_fcDistinct = 0;
uint64_t g_fcSeen[80] = {};
std::atomic<int> g_fcWindow{0};
void FcGate(const char* tag, uint64_t c) {
    uint8_t b106 = 0, b886 = 0, bA02 = 0, bB313 = 0, b9D4 = 0, b109 = 0, bA03 = 0;
    uint32_t ab84 = 0, key = 0;
    float f8e4 = 0;
    Rd(c, key); Rd(c + 0x106, b106); Rd(c + 0xAB84, ab84); Rd(c + 0x886, b886); Rd(c + 0xA02, bA02); Rd(c + 0xB313, bB313);
    Rd(c + 0x9D4, b9D4); Rd(c + 0x109, b109); Rd(c + 0xA03, bA03); Rd(c + 0x8E4, f8e4);
    Log("FormCommit gate %s chr=%llX id=%X F2D02 gate: +106=%u +AB84=%X +886=%u +A02=%u +B313=%u (all 0/-1 -> F2D02 skipped) +9D4(dl)=%u 3FD37C: +109=%u +8E4=%.3f +A03=%u",
        tag, (unsigned long long)c, key >> 4, b106, ab84, b886, bA02, bB313, b9D4, b109, f8e4, bA03);
}
uint64_t HkFc3FD37C(uint64_t chr, uint64_t dl, uint64_t r8, uint64_t r9) {
    const uint64_t site = (uint64_t)_ReturnAddress() - g_base - 5;
    InterlockedIncrement(&g_fcCalls);
    const uint64_t pl = FkPlayer();
    const bool isPl = chr && chr == pl;
    bool fresh = false;
    if (!isPl && g_fcDistinct < 80) {
        bool seen = false;
        const LONG n = g_fcDistinct;
        for (LONG i = 0; i < n; ++i) if (g_fcSeen[i] == chr) { seen = true; break; }
        if (!seen) { const LONG k = InterlockedIncrement(&g_fcDistinct) - 1; if (k < 80) { g_fcSeen[k] = chr; fresh = true; } }
    }
    if (isPl) InterlockedIncrement(&g_fcPlayerCalls);
    const bool sealish = chr && (chr == g_fkSealChr.load() || chr == g_fkAppSeal.load() || chr == g_fs0CapChr.load());
    if ((fresh || (isPl && (g_fcPlayerCalls <= 20 || g_fcWindow.load())) || sealish) && InterlockedIncrement(&g_fcLines) <= 600) {
        uint32_t key = 0;
        Rd(chr, key);
        Log("FormCommit walk site=+%llX chr=%llX id=%X %s dl=%u r8b=%u calls=%ld playerCalls=%ld t=%.3f", (unsigned long long)site, (unsigned long long)chr,
            key >> 4, isPl ? "PLAYER" : sealish ? "SEAL" : "other", (unsigned)(dl & 0xFF), (unsigned)(r8 & 0xFF), g_fcCalls, g_fcPlayerCalls, Now());
    }
    return ((uint64_t(*)(uint64_t, uint64_t, uint64_t, uint64_t))g_fcOrig)(chr, dl, r8, r9);
}
void FormCommitInstall() {
    static std::atomic<int> done{0};
    if (done.exchange(1)) return;
    if (!g_base) g_base = g_img.base;
    static const uint32_t kS[] = {0xF2D02, 0x2BD04B, 0x3FC663, 0x485A70, 0x60370B, 0x603782, 0x66AB6A, 0x10C46A5, 0x10C4712, 0x1C0A659,
                                  0x1C0F6D2, 0x1F5D682, 0x200388B, 0x2052E9F, 0x208B0E1, 0x238C945, 0x239441B, 0x23947B1, 0x23C419E, 0x23C41FC};
    int n = 0;
    char skipped[256];
    int o = 0;
    skipped[0] = 0;
    for (uint32_t rva : kS) {
        uint8_t* p = (uint8_t*)(g_base + rva);
        uint8_t b[5] = {};
        int32_t rel = 0;
        if (Copy(b, (uint64_t)p, 5)) memcpy(&rel, b + 1, 4);
        if (b[0] != 0xE8 || (uint64_t)(p + 5 + rel) != g_base + 0x3FD37C) { if (o < 230) o += sprintf_s(skipped + o, sizeof(skipped) - o, " +%X(%02X)", rva, b[0]); continue; }
        const char* w = nullptr;
        void* orig = nullptr;
        if (!InstallCallRetargetAt(p, "call:FormCommit3FD37C", (void*)&HkFc3FD37C, &orig, &w)) { Log("FormCommit: +%X failed: %s", rva, w ? w : "?"); continue; }
        g_fcOrig = orig;
        ++n;
    }
    Log("FormCommit: 3FD37C hooked on %d/20 call sites; not a direct call:%s", n, o ? skipped : " none");
}
struct FcSnap { uint32_t st, v40, v20; uint8_t f8, f9; uint64_t p10, p328; };
void FcRead(uint64_t s, FcSnap& x) {
    x = {};
    if (!s) return;
    Rd(s + 0x18, x.st); Rd(s + 0x40, x.v40); Rd(s + 0x20, x.v20); Rd(s + 0x2F8, x.f8); Rd(s + 0x2F9, x.f9); Rd(s + 0x10, x.p10); Rd(s + 0x328, x.p328);
}
bool FcCall(uint64_t chr) {
    __try {
        ((uint64_t(*)(uint64_t, uint64_t, uint64_t))(g_base + 0x3FD37C))(chr, 1, 0);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
void FcTick(uint64_t pchr, double dt) {  // game thread, inside the kick window
    static int frames = 0, faults = 0;
    static uint64_t lastCap = 0;
    const uint64_t cap = g_fs0CapChr.load();
    if (cap != lastCap) {
        lastCap = cap; frames = 0; faults = 0;
        FcGate("player", pchr);
        if (cap) FcGate("seal", cap);
    }
    if (dt < 0.35 || dt > 1.35 || frames >= 120) {
        if (g_fcWindow.load() && dt > 1.35) {
            g_fcWindow.store(0);
            FcSnap a;
            FcRead(g_fs0FirstK0.load(), a);
            Log("FormCommit done: %d forced 3FD37C(player,1,0) calls, %d faults, player calls through the 20 sites so far %ld; final slot state[+18]=%u +2F8=%u +2F9=%u +40=%X +20=%X +10=%llX +328=%llX t=%.3f",
                frames, faults, g_fcPlayerCalls, a.st, a.f8, a.f9, a.v40, a.v20, (unsigned long long)a.p10, (unsigned long long)a.p328, Now());
        }
        return;
    }
    g_fcWindow.store(1);
    const uint64_t s = g_fs0FirstK0.load();
    FcSnap b, a;
    FcRead(s, b);
    const bool ok = FcCall(pchr);
    if (!ok) ++faults;
    FcRead(s, a);
    ++frames;
    if (frames <= 8 || memcmp(&a, &b, sizeof(a)) != 0 || !ok)
        Log("FormCommit #%d 3FD37C(player,1,0) ok=%d slot=%llX state %u->%u +2F8 %u->%u +2F9 %u->%u +40 %X->%X +20 %X->%X +10 %llX->%llX +328 %llX->%llX dt=%.3f", frames, ok,
            (unsigned long long)s, b.st, a.st, b.f8, a.f8, b.f9, a.f9, b.v40, a.v40, b.v20, a.v20, (unsigned long long)b.p10, (unsigned long long)a.p10,
            (unsigned long long)b.p328, (unsigned long long)a.p328, dt);
}
// pc #55111: [Spirit] FormAttach=1 - FormCommit showed the forced 3FD37C(player,1,0) commits the staged slot through the state-0 fast path only
// (+20=40244A, new [slot+10], state stays 0), so 903088 (state 1: mesh create/attach, +3FE010) and 2C5FBC (state 2, +3FDE7C) never run for the
// player. FormAttach: for dt 0.35..2.5 s after capture call 3FD37C(player,1,0) once per frame; once the slot holds the seal id (+20) with no
// stage pending, copy the seal slot's load request [+60..+7F] ({id,+68,+70,+78}, what state 1 hands 3FE9D0 / 903088) into the player slot and set
// state [+18]=1, so the next ticks take the slow path 1 -> 2 -> 3 -> 4. Logs every player slot change and the 903088 / 2C5FBC calls in the window.
std::atomic<int> g_fatMode{0};
std::atomic<int> g_fatWin{0};
void* g_fat903Orig = nullptr;
void* g_fat2C5Orig = nullptr;
volatile LONG g_fatLines = 0;
uint64_t HkFat903088(uint64_t slot, uint64_t rdx, uint64_t r8, uint64_t r9, uint64_t a5) {
    const bool pl = slot && slot == g_fs0FirstK0.load();
    if ((pl || g_fatWin.load()) && InterlockedIncrement(&g_fatLines) <= 200) {
        uint32_t rid = 0;
        Rd(r9, rid);
        Log("FormAttach 903088 slot=%llX%s rdx=%llX chr(r8)=%llX req.id=%X t=%.3f", (unsigned long long)slot, pl ? " PLAYER" : "", (unsigned long long)rdx,
            (unsigned long long)r8, rid, Now());
    }
    // pc #55177 FormAttach=2/3/4 (on b75c4d66): probe the 903088 inputs for player vs seal (406668 entry kind e[+4], chr+560 appearance blk),
    // =2 zero player [slot+10] before 903088 so it must create, =4 hand 903088 the seal chr (seal chr+560 blk) for the player slot,
    // =3 after 903088 point player [slot+10] at the seal slot's obj (borrow seal obj).
    const int fm = g_fatMode.load();
    const uint64_t sc = g_fs0CapChr.load();
    if ((pl || (sc && r8 == sc)) && InterlockedIncrement(&g_fatLines) <= 200) {
        uint32_t rid = 0, ek = 0, ekind = 0;
        Rd(r9, rid);
        const uint64_t e = SpLookup(rid, &ek);
        if (e > 1) Rd(e + 4, ekind);
        uint64_t b560 = 0, p10 = 0;
        Rd(r8 + 0x560, b560);
        Rd(slot + 0x10, p10);
        Log("FormAttach probe %s slot=%llX chr=%llX [chr+560]=%llX 406668(%X)=%llX e[+4]=%X key[+3C]=%X +10=%llX mode=%d t=%.3f", pl ? "PLAYER" : "SEAL",
            (unsigned long long)slot, (unsigned long long)r8, (unsigned long long)b560, rid, (unsigned long long)e, ekind, ek, (unsigned long long)p10, fm, Now());
    }
    if (pl && fm == 2) {
        const bool ok = [&] { const uint64_t z = 0; return WrBytes(slot + 0x10, &z, 8); }();
        Log("FormAttach=2 player [slot+10] zeroed before 903088 ok=%d", ok);
    }
    if (pl && fm == 4 && sc) {
        Log("FormAttach=4 903088 player slot gets seal chr %llX instead of %llX", (unsigned long long)sc, (unsigned long long)r8);
        r8 = sc;
    }
    const uint64_t r = ((uint64_t(*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t))g_fat903Orig)(slot, rdx, r8, r9, a5);
    if (pl && fm == 3) {
        uint64_t so = 0;
        const uint64_t ss = g_spSealSlot.load();
        if (ss) Rd(ss + 0x10, so);
        const bool ok = so && WrBytes(slot + 0x10, &so, 8);
        Log("FormAttach=3 player [slot+10] -> seal obj %llX ok=%d", (unsigned long long)so, ok);
    }
    if ((pl || (sc && r8 == sc)) && InterlockedIncrement(&g_fatLines) <= 200) {
        uint64_t p10 = 0;
        uint32_t st = 0;
        Rd(slot + 0x10, p10);
        Rd(slot + 0x18, st);
        Log("FormAttach 903088 %s returned %llX -> +10=%llX state=%u t=%.3f", pl ? "PLAYER" : "SEAL", (unsigned long long)r, (unsigned long long)p10, st, Now());
    }
    return r;
}
uint64_t HkFat2C5FBC(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    if (g_fatWin.load() && InterlockedIncrement(&g_fatLines) <= 200)
        Log("FormAttach 2C5FBC (state 2) rcx=%llX rdx=%llX r8=%llX r9=%llX t=%.3f", (unsigned long long)a, (unsigned long long)b, (unsigned long long)c,
            (unsigned long long)d, Now());
    return ((uint64_t(*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t))g_fat2C5Orig)(a, b, c, d, e, f);
}
void FormAttachInstall() {
    static std::atomic<int> done{0};
    if (done.exchange(1)) return;
    if (!g_base) g_base = g_img.base;
    auto one = [&](uint32_t rva, uint32_t tgt, void* fn, void** orig, const char* name) {
        uint8_t* p = (uint8_t*)(g_base + rva);
        uint8_t b[5] = {};
        int32_t rel = 0;
        if (Copy(b, (uint64_t)p, 5)) memcpy(&rel, b + 1, 4);
        if (b[0] != 0xE8 || (uint64_t)(p + 5 + rel) != g_base + tgt) { Log("FormAttach: +%X is not 'call %X' - skipped", rva, tgt); return false; }
        const char* w = nullptr;
        if (!InstallCallRetargetAt(p, name, fn, orig, &w)) { Log("FormAttach: +%X failed: %s", rva, w ? w : "?"); return false; }
        return true;
    };
    const bool a = one(0x3FE010, 0x903088, (void*)&HkFat903088, &g_fat903Orig, "call:FormAttach903088");
    const bool b = one(0x3FDE7C, 0x2C5FBC, (void*)&HkFat2C5FBC, &g_fat2C5Orig, "call:FormAttach2C5FBC");
    Log("FormAttach: hooks 903088@+3FE010=%d 2C5FBC@+3FDE7C=%d", a, b);
}
void FatTick(uint64_t pchr, double dt) {  // game thread, inside the kick window
    static uint64_t lastCap = 0;
    static int frames = 0, faults = 0, forced = 0;
    static FcSnap last;
    static bool have = false;
    const uint64_t cap = g_fs0CapChr.load();
    const uint64_t s = g_fs0FirstK0.load(), ss = g_spSealSlot.load();
    if (cap != lastCap) { lastCap = cap; frames = faults = forced = 0; have = false; }
    if (dt < 0.35 || dt > 2.5 || frames >= 400) {
        if (g_fatWin.exchange(0) && dt > 2.5) {
            FcSnap a;
            FcRead(s, a);
            Log("FormAttach done: %d forced ticks, %d faults, state-1 forced=%d; final player slot state=%u +20=%X +40=%X +10=%llX +328=%llX t=%.3f", frames, faults,
                forced, a.st, a.v20, a.v40, (unsigned long long)a.p10, (unsigned long long)a.p328, Now());
        }
        return;
    }
    g_fatWin.store(1);
    if (!FcCall(pchr)) ++faults;
    ++frames;
    FcSnap a;
    FcRead(s, a);
    if (!forced && s && ss && a.v20 == g_fs0Id.load() && a.f8 == 0 && a.st == 0) {
        uint8_t mine[0x20] = {}, seal[0x20] = {};
        Copy(mine, s + 0x60, 0x20);
        const bool rs = Copy(seal, ss + 0x60, 0x20);
        uint32_t sst = 0;
        Rd(ss + 0x18, sst);
        const bool w1 = rs && WrBytes(s + 0x60, seal, 0x20);
        const bool w2 = w1 && WrU32(s + 0x18, 1);
        forced = 1;
        Log("FormAttach FORCE state 1: player slot %llX req[+60] was id=%X +68=%X +70=%llX +78=%X -> seal slot %llX (state %u) id=%X +68=%X +70=%llX +78=%X; copy=%d state=1 w=%d dt=%.3f",
            (unsigned long long)s, *(uint32_t*)mine, *(uint32_t*)(mine + 8), (unsigned long long)*(uint64_t*)(mine + 0x10), *(uint32_t*)(mine + 0x18),
            (unsigned long long)ss, sst, *(uint32_t*)seal, *(uint32_t*)(seal + 8), (unsigned long long)*(uint64_t*)(seal + 0x10), *(uint32_t*)(seal + 0x18), w1, w2, dt);
        FcRead(s, a);
    }
    if (!have || memcmp(&a, &last, sizeof(a)) != 0) {
        if (InterlockedIncrement(&g_fatLines) <= 200)
            Log("FormAttach tick #%d player slot state=%u +2F8=%u +2F9=%u +40=%X +20=%X +10=%llX +328=%llX dt=%.3f", frames, a.st, a.f8, a.f9, a.v40, a.v20,
                (unsigned long long)a.p10, (unsigned long long)a.p328, dt);
        last = a;
        have = true;
    }
}
// pc #55327 [Spirit] FormHide=1 - hide+puppet fallback step 1: 0.5 s after the FormSlot0 seal capture call the game's own SetHideModel A550E8(chr, 1)
// on the player (writes [chr+B440]=1; the part code 3FFFEC/40343C/406EDC and 8591B read that byte; Tenshin end 1AB4C5C uses it) and show it again
// with A550E8(chr, 0) 20 s later. Logs [chr+B440] before/after. 2 = also pin the seal chr position [+110] to the player's every frame while hidden.
std::atomic<int> g_fhMode{0};
// pc #55492: read-only end observation. Native LA end clears bit 1 of ext+188
// (Tenshin controller ext+178, flags +10); actor removal unlinks actor+E0 from the 16-bucket table.
std::atomic<int> g_fhUntilEnd{0};
struct FormHideEndWatch {
    uint64_t player = 0, seal = 0, actorGlobal = 0;
    bool active = false, laGuard = false, laSeen = false, sealSeen = false;
    double nextPoll = 0;
    const char* ended = nullptr;
};
FormHideEndWatch g_fhEnd;

bool FormHideCodeMatches(uint64_t rva, const uint8_t* want, size_t n) {
    uint8_t bytes[128] = {}; // shellWriter guard is 75 bytes (#56115: 64 made mode 4 never install)
    return n <= sizeof(bytes) && Copy(bytes, g_base + rva, n) && memcmp(bytes, want, n) == 0;
}
// Unknown is -1, never 'absent'. Only a complete, bounded walk can establish absence.
int FormHideSealPresent(uint64_t global, uint64_t seal) {
    uint64_t mgr = 0, buckets = 0;
    if (!global || !Rd(global, mgr) || !mgr || !Rd(mgr + 0xBA0, buckets) || !buckets) return -1;
    unsigned budget = 4096;
    for (unsigned i = 0; i < 16; ++i) {
        uint64_t node = 0, count = 0;
        if (!Rd(buckets + i * 16, node) || !Rd(buckets + i * 16 + 8, count) || count > budget) return -1;
        budget -= (unsigned)count;
        for (uint64_t n = 0; n < count; ++n) {
            if (!node) return -1;
            if (node == seal + 0xE0) return 1;  // no dereference of the borrowed seal actor
            if (!Rd(node, node)) return -1;
        }
        if (node) return -1;  // count/list mismatch or cycle; do not infer despawn
    }
    return 0;
}
void FormHideEndBegin(uint64_t player, uint64_t seal) {
    g_fhEnd = {};
    if (!g_fhUntilEnd.load()) return;
    g_fhEnd.player = player;
    g_fhEnd.seal = seal;
    g_fhEnd.active = true;
    static const uint8_t endHead[] = {0x48,0x8B,0xC4,0x48,0x89,0x58,0x08,0x48,0x89,0x70,0x10,0x57,0x48,0x81,0xEC,0xC0,0,0,0,0x80,0x61,0x10,0xFD};
    static const uint8_t beginBit[] = {0x24,0xEF,0x33,0xD2,0x0C,0x02,0x88,0x41,0x10};
    g_fhEnd.laGuard = FormHideCodeMatches(0x1AB4C5C, endHead, sizeof(endHead)) && FormHideCodeMatches(0x843040, beginBit, sizeof(beginBit));
    // Verify the removal wrapper including its RIP-relative manager load, bucket offset and unlink-call target.
    static const uint8_t removeHead[] = {0x48,0x83,0xEC,0x28,0x48,0x8B,0xD1,0x48,0x8B,0x0D,0x52,0x5B,0x48,0x04,
        0x48,0x85,0xC9,0x74,0x0C,0x48,0x81,0xC1,0x98,0x0B,0,0,0xE8,0x11,0,0,0};
    if (FormHideCodeMatches(0x2BF7E8, removeHead, sizeof(removeHead))) {
        int32_t rel = 0;
        if (Rd(g_base + 0x2BF7F2, rel)) g_fhEnd.actorGlobal = g_base + 0x2BF7F6 + rel;
    }
    Log("FormHideUntilEnd begin player=%llX seal=%llX LAguard=%d actorTableGuard=%d (unobserved signals fall back to 20s)",
        (unsigned long long)player, (unsigned long long)seal, g_fhEnd.laGuard, g_fhEnd.actorGlobal != 0);
}
const char* FormHideEndReason(uint64_t player, uint64_t seal, double hideT) {
    const double now = Now();
    if (!g_fhUntilEnd.load()) return now - hideT >= 20.0 ? "timer" : nullptr;
    if (!g_fhEnd.active || g_fhEnd.player != player || g_fhEnd.seal != seal) return "end-context-change";
    if (g_fhEnd.ended) return g_fhEnd.ended;
    if (now >= g_fhEnd.nextPoll) {
        g_fhEnd.nextPoll = now + 0.1;
        uint64_t ext = 0;
        uint8_t flags = 0;
        const bool laRead = g_fhEnd.laGuard && Rd(player + 0x3A0, ext) && ext && Rd(ext + 0x188, flags);
        if (laRead && (flags & 2)) {
            if (!g_fhEnd.laSeen) Log("FormHideUntilEnd observed LA active player=%llX", (unsigned long long)player);
            g_fhEnd.laSeen = true;
        } else if (g_fhEnd.laSeen) {
            g_fhEnd.ended = laRead ? "LA-ended" : "LA-state-unreadable";
        } else {
            const int present = FormHideSealPresent(g_fhEnd.actorGlobal, seal);
            if (present == 1) {
                if (!g_fhEnd.sealSeen) Log("FormHideUntilEnd observed seal in actor table seal=%llX", (unsigned long long)seal);
                g_fhEnd.sealSeen = true;
            } else if (g_fhEnd.sealSeen) g_fhEnd.ended = present == 0 ? "seal-despawn" : "seal-table-unreadable";
        }
    }
    if (!g_fhEnd.ended && !g_fhEnd.laSeen && !g_fhEnd.sealSeen && now - hideT >= 20.0) g_fhEnd.ended = "unobserved-end-fallback-20s";
    return g_fhEnd.ended;
}
bool FormHideEndHolding(uint64_t player, uint64_t seal) {
    return g_fhUntilEnd.load() && g_fhMode.load() && g_fhEnd.active && !g_fhEnd.ended && g_fhEnd.player == player && g_fhEnd.seal == seal &&
        (g_fhEnd.laSeen || g_fhEnd.sealSeen);
}
// pc #55665: pin the seal chr onto the player's spot every tick while hidden: position [+110] (16 B) and the 3x3 rotation rows at
// [+FormPinRot] (default 0x120: +120 right, +130 up, +140 forward; ally chr dumps #55310) when every row of both chrs is a unit vector,
// so the yokai turns with the stick. LA can outlive its seal: in end-driven mode never write onto a removed actor.
std::atomic<int> g_fhRotOff{0x120};
// pc #55842 [Spirit] FormPinWarp=1 (default): +110 alone is re-driven every frame from the physics controller [[chr+3B0]+30] (ally: 5129B
// 1.9 m off after 2034 +110 pins). Move it like the game's own warp: CharacterSetPositionYaw 1C27010 = 669AB8(chr, pos, 0) (sets +110 via
// 3578E0, the controller [[chr+3B0]+30] pos, history +260/+2B0, +340) + rotation rows +120..+14C + [[chr+3B0]+8]+D8 |= 11h. 0 = old +110 write.
std::atomic<int> g_fhWarp{1};
static bool FormPinWarpCall(uint64_t cap, const float* pos) {
    static const uint8_t want[29] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x57,
                                     0x48, 0x83, 0xEC, 0x30, 0x41, 0x8A, 0xE8, 0x48, 0x8B, 0xFA, 0x48, 0x8B, 0xD9};
    static int guard = -1;
    if (guard < 0) {
        uint8_t b[sizeof(want)] = {};
        guard = Copy(b, g_base + 0x669AB8, sizeof(b)) && memcmp(b, want, sizeof(b)) == 0;
        Log("FormPinWarp: 669AB8 guard %s", guard ? "ok" : "differs - falling back to the +110 write");
    }
    if (!guard) return false;
    alignas(16) float p[4] = {pos[0], pos[1], pos[2], 1.0f};  // 3578E0 / 669C40 use movaps
    __try {
        ((void (*)(uint64_t, float*, uint8_t))(g_base + 0x669AB8))(cap, p, 0);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        guard = 0;
        Log("FormPinWarp: 669AB8 faulted on chr %llX - disabled", (unsigned long long)cap);
        return false;
    }
    return true;
}
std::atomic<int> g_fh4Mask{0x7F};  // pc #56435 [Spirit] FH4Mask bits: 1 B440, 2 B444, 4 root+3A8, 8 root+248, 16 shell46 alpha, 32 FormPin, 64 pin warp
bool FormPin(uint64_t pchr, uint64_t cap) {
    if (!(g_fh4Mask.load() & 32)) return true;
    if (g_fhUntilEnd.load() && FormHideSealPresent(g_fhEnd.actorGlobal, cap) != 1) return false;
    float pos[4] = {};
    if (!Copy(pos, pchr + 0x110, 16)) return false;
    uint64_t ag = g_fhEnd.actorGlobal;  // only call into the game on a chr that is provably still in the actor table
    if (!ag) {  // FormHideUntilEnd off: same RIP-relative actor manager load as FormHideEndBegin, opcode-checked
        static const uint8_t mov[3] = {0x48, 0x8B, 0x0D};
        int32_t rel = 0;
        if (FormHideCodeMatches(0x2BF7EF, mov, 3) && Rd(g_base + 0x2BF7F2, rel)) ag = g_base + 0x2BF7F6 + rel;
    }
    const bool warped = g_fhWarp.load() && (g_fh4Mask.load() & 64) && FormHideSealPresent(ag, cap) == 1 && FormPinWarpCall(cap, pos);
    if (!warped && !WrBytes(cap + 0x110, pos, 16)) return false;
    static uint64_t loggedCap = 0;
    float m[12] = {}, sm[12] = {};
    const int ro = g_fhRotOff.load();
    if (ro > 0 && Copy(m, pchr + ro, sizeof(m)) && Copy(sm, cap + ro, sizeof(sm))) {
        bool unit = true;
        for (int r = 0; r < 3; ++r) {
            const float* a = m + r * 4; const float* b = sm + r * 4;
            const float n = a[0] * a[0] + a[1] * a[1] + a[2] * a[2], sn = b[0] * b[0] + b[1] * b[1] + b[2] * b[2];
            unit = unit && n > 0.98f && n < 1.02f && sn > 0.98f && sn < 1.02f;
        }
        const bool rot = unit && WrBytes(cap + ro, m, sizeof(m));
        uint64_t comp = 0, xf = 0;
        if (rot && warped && Rd(cap + 0x3B0, comp) && comp && Rd(comp + 8, xf) && xf) {  // 1C27010's transform-dirty mark after the rows
            uint32_t fl = 0;
            if (Rd(xf + 0xD8, fl)) { fl |= 0x11; WrBytes(xf + 0xD8, &fl, 4); }
        }
        if (loggedCap != cap) {
            loggedCap = cap;
            Log("FormPin first: rot +%X player fwd=%.3f %.3f %.3f seal fwd=%.3f %.3f %.3f copied=%d warp=%d pos=%.2f %.2f %.2f", ro,
                m[8], m[9], m[10], sm[8], sm[9], sm[10], rot, warped, pos[0], pos[1], pos[2]);
        }
    }
    return true;
}
// pc #56910 [Spirit] FormFollowSeal: traveling soul cores (d-pad DOWN) move their yokai away from the player, so FormPin (seal -> player)
// would hold it in place. 1 = every frame warp the hidden player onto the seal (669AB8 on the player + its rotation rows), camera keeps
// following the player, who reappears where the yokai stops. 2 = auto: seal bound from the spawn hit (no FormSlot0 capture) -> follow; else no pin for the first FormFollowProbeMs after the seal is bound;
// seal moved >= FormFollowCm (default 400, was 150) from its first position -> follow, else pin as before. 0 = off (pin only).
std::atomic<int> g_fhFollow{0}, g_fhFollowProbeMs{600}, g_fhFollowCm{400};
bool FormFollow(uint64_t pchr, uint64_t cap) {
    uint64_t ag = g_fhEnd.actorGlobal;  // never read a removed seal (same actor-table lookup as FormPin)
    if (!ag) {
        static const uint8_t mov[3] = {0x48, 0x8B, 0x0D};
        int32_t rel = 0;
        if (FormHideCodeMatches(0x2BF7EF, mov, 3) && Rd(g_base + 0x2BF7F2, rel)) ag = g_base + 0x2BF7F6 + rel;
    }
    if (FormHideSealPresent(ag, cap) != 1) return false;
    float pos[4] = {};
    if (!Copy(pos, cap + 0x110, 16) || !std::isfinite(pos[0]) || !std::isfinite(pos[1]) || !std::isfinite(pos[2])) return false;
    const bool warped = g_fhWarp.load() && FormPinWarpCall(pchr, pos);
    if (!warped && !WrBytes(pchr + 0x110, pos, 16)) return false;
    float m[12] = {}, sm[12] = {};
    const int ro = g_fhRotOff.load();
    bool rot = false;
    if (ro > 0 && Copy(sm, cap + ro, sizeof(sm)) && Copy(m, pchr + ro, sizeof(m))) {
        bool unit = true;
        for (int r = 0; r < 3; ++r) {
            const float* a = m + r * 4; const float* b = sm + r * 4;
            const float n = a[0] * a[0] + a[1] * a[1] + a[2] * a[2], sn = b[0] * b[0] + b[1] * b[1] + b[2] * b[2];
            unit = unit && n > 0.98f && n < 1.02f && sn > 0.98f && sn < 1.02f;
        }
        rot = unit && WrBytes(pchr + ro, sm, sizeof(sm));
        uint64_t comp = 0, xf = 0;
        if (rot && warped && Rd(pchr + 0x3B0, comp) && comp && Rd(comp + 8, xf) && xf) {
            uint32_t fl = 0;
            if (Rd(xf + 0xD8, fl)) { fl |= 0x11; WrBytes(xf + 0xD8, &fl, 4); }
        }
    }
    static uint64_t loggedCap = 0;
    static double nextLog = 0;
    const double now = Now();
    if (loggedCap != cap || now >= nextLog) {
        Log("FormFollow %s: player %llX -> seal %llX pos=%.1f %.1f %.1f warp=%d rot=%d t=%.3f", loggedCap != cap ? "first" : "at",
            (unsigned long long)pchr, (unsigned long long)cap, pos[0], pos[1], pos[2], warped, rot, now);
        loggedCap = cap; nextLog = now + 1;
    }
    return true;
}
// pc #55469: mode 3 writes only the guarded hide byte, without SetHideModel's fade or seal-position pinning.
// Keep its saved byte separate from the native modes; never invoke native unhide for a direct-byte hide.
// pc #55704 [Spirit] FormHidePress=1 (mode 3): hide on the d-pad LEFT (seal) press edge so the throw is never drawn; the capture ~1.3 s
// later adopts the hide. No capture within 2.5 s = not a seal -> restore. FormHideExtra=1: a second seal chr loaded while hidden
// (EE49E, ~0.5 s after the capture) gets the same hide byte, restored at the end only if it is still in the actor table.
std::atomic<int> g_fhPress{0}, g_fhExtra{1};
// pc #55814: pc2's pipe pad never reaches XInputGetState, so PadButtons() misses LEFT. Also fire the press-hide on the seal ACTION: the
// player's current action id = dword [[ctrl+0xE8]], ctrl = [[pchr+0x3B0]+8] (358528 inlined; Training frame_probe_game.h), read every frame
// from the 2E6C34 input tick. A change to a seal-use id hides on that frame. Ids: [Spirit] FormHideSealAct (hex list, up to 4) plus one
// learned live: the action that started 0.6..2.0 s before a seal capture (nearest to the ~1.27 s press->capture gap). FormHideActLog=1 logs
// every player action change (20/s cap) so the seal id shows in the log.
std::atomic<uint32_t> g_fhActIds[4];
std::atomic<int> g_fhActN{0}, g_fhActLog{1}, g_fhActLearn{1};
std::atomic<uint32_t> g_fhActLearned{0};
uint32_t PlayerActId(uint64_t pchr) {
    uint64_t ai = 0, ctrl = 0, p = 0;
    uint32_t id = 0xFFFFFFFF;
    if (!pchr || !Rd(pchr + 0x3B0, ai) || !ai || !Rd(ai + 8, ctrl) || !ctrl || !Rd(ctrl + 0xE8, p)) return 0xFFFFFFFF;
    if (!p) return 0xFFFF;
    return Rd(p, id) ? id : 0xFFFFFFFF;
}
bool FhIsSealAct(uint32_t id) {
    if (id == 0xFFFF || id == 0xFFFFFFFF || id == 0) return false;
    if (id == g_fhActLearned.load()) return true;
    for (int i = 0, n = g_fhActN.load(); i < n; ++i)
        if (g_fhActIds[i].load() == id) return true;
    return false;
}
// Returns true on the frame the player's action changes to a seal-use id. Keeps a 16-entry start history for the learner.
bool FhSealActTick(uint64_t pchr, uint64_t cap, double capT, double now) {
    static uint32_t last = 0xFFFFFFFF;
    static uint64_t lastChr = 0;
    static struct { uint32_t id; double t; } hist[16] = {};
    static int hn = 0;
    static double learnedFor = -1;
    static int64_t sec = -1;
    static int inSec = 0;
    const uint32_t id = PlayerActId(pchr);
    if (pchr != lastChr) { lastChr = pchr; last = id; hn = 0; return false; }
    bool edge = false;
    if (id != last && id != 0xFFFFFFFF) {
        const uint32_t prev = last;
        last = id;
        hist[hn % 16] = {id, now};
        ++hn;
        edge = FhIsSealAct(id);
        if (g_fhActLog.load()) {
            if ((int64_t)now != sec) { sec = (int64_t)now; inSec = 0; }
            if (++inSec <= 20 || edge) Log("FormHide=3 player action 0x%X -> 0x%X%s t=%.3f", prev, id, edge ? " (seal action)" : "", now);
        }
    } else if (id == 0xFFFFFFFF) {
        last = id;
    }
    if (g_fhActLearn.load() && cap && capT != learnedFor && now - capT < 1.0) {
        learnedFor = capT;
        int best = -1;
        double bestD = 9;
        bool knownNear = false;  // pc #56639: FastThrow ends the throw early, so an attack nearer 1.27 s got learned (0x7ED); a known seal act wins
        for (int k = 0; k < 16 && k < hn; ++k) {
            const auto& h = hist[(hn - 1 - k) % 16];
            if (capT - h.t >= 0 && capT - h.t <= 2.5 && FhIsSealAct(h.id)) knownNear = true;
        }
        for (int k = 0; k < 16 && k < hn && !knownNear; ++k) {
            const auto& h = hist[(hn - 1 - k) % 16];
            const double gap = capT - h.t;
            if (gap < 0.6 || gap > 2.0 || h.id == 0xFFFF) continue;
            const double d = gap > 1.27 ? gap - 1.27 : 1.27 - gap;
            if (d < bestD) { bestD = d; best = (hn - 1 - k) % 16; }
        }
        if (best >= 0) {
            const uint32_t cand = hist[best].id;
            const bool known = FhIsSealAct(cand);
            if (!known && !g_fhActLearned.load()) g_fhActLearned.store(cand);
            Log("FormHide=3 seal action learn: capture %llX, action 0x%X started %.3f s before it (%s) learned=0x%X", (unsigned long long)cap, cand,
                capT - hist[best].t, known ? "already known" : "new", g_fhActLearned.load());
        } else {
            Log("FormHide=3 seal action learn: capture %llX, no player action start 0.6..2.0 s before it", (unsigned long long)cap);
        }
    }
    return edge;
}
// pc #55953 / ally #55928: experimental full-blend + root-multiplier hold.
// Keep the native +240 fade controller intact (FE37A0 consumes its +248).
struct Fh4State {
    uint64_t owner = 0, seal = 0, root = 0;
    double started = 0, capT = -1, nextLog = 0, nextShellLog = 0;
    uint8_t hide = 0;
    float blend = 0, multiplier = 0, fade = 1;
    uint64_t sealRoot = 0;  // pc #56435: seal's model root, refreshed by the tick at 10 Hz (hooks used to walk the actor table per call)
    double nextSealPoll = 0;
    bool fromSpawn = false;  // pc #56910: seal bound from the spawn hit, not a FormSlot0 capture
    int follow = -1;  // pc #56910 FormFollowSeal: -1 undecided (auto probe), 0 pin seal to player, 1 player follows seal
    float sealP0[4] = {};
    double followT0 = 0;
};
Fh4State g_fh4;
// pc #56938 (human #56933): no attacks or items while FormHide=4 owns the player (press/capture -> restore). [Spirit] FormBlockPad = XInput
// button mask dropped from the game's XInputGetState for that window (default 0xFF4F = every button except Start, Back and R3: attacks,
// items, dodge/sprint, d-pad, guard), FormBlockRT=1 zeroes both triggers, FormBlockLS=1 zeroes the left stick (no walking; pc #56964 /
// human #56935: no actions at all, camera free -> right stick untouched). The IAT slot is taken on the FIRST form start, not at load, so this hook sits outside EngineFix's PadLatency/VPad hooks
// and also masks the harness pipe pad. Keyboard (DirectInput) is not masked. 0 = off.
std::atomic<int> g_fbMask{0xFF4F}, g_fbRT{1}, g_fbLS{1};
std::atomic<bool> g_fbActive{false};
std::atomic<long> g_fbDropped{0};
std::atomic<ULONGLONG> g_fbSince{0};  // safety: never mask longer than 30 s even if the restore never runs
using FbXGet = DWORD(WINAPI*)(DWORD, void*);
FbXGet g_fbPrev = nullptr;
struct FbPad { WORD buttons; BYTE lt, rt; SHORT lx, ly, rx, ry; };
struct FbState { DWORD packet; FbPad pad; };
DWORD WINAPI HkFormBlockPad(DWORD user, void* st) {
    const DWORD r = g_fbPrev ? g_fbPrev(user, st) : ERROR_DEVICE_NOT_CONNECTED;
    if (r == ERROR_SUCCESS && st && g_fbActive.load(std::memory_order_relaxed) && GetTickCount64() - g_fbSince.load(std::memory_order_relaxed) < 30000) {
        FbState* s = (FbState*)st;
        const WORD m = (WORD)g_fbMask.load(std::memory_order_relaxed);
        const bool trig = g_fbRT.load(std::memory_order_relaxed), ls = g_fbLS.load(std::memory_order_relaxed);
        if ((s->pad.buttons & m) || (trig && (s->pad.rt || s->pad.lt)) || (ls && (s->pad.lx || s->pad.ly))) g_fbDropped.fetch_add(1, std::memory_order_relaxed);
        s->pad.buttons &= (WORD)~m;
        if (trig) { s->pad.rt = 0; s->pad.lt = 0; }
        if (ls) { s->pad.lx = 0; s->pad.ly = 0; }
    }
    return r;
}
void** FbXInputSlot() {
    uint8_t* base = (uint8_t*)g_img.base;
    __try {
        auto nt = (IMAGE_NT_HEADERS64*)(base + ((IMAGE_DOS_HEADER*)base)->e_lfanew);
        const IMAGE_DATA_DIRECTORY& dd = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (!dd.VirtualAddress) return nullptr;
        for (auto d = (IMAGE_IMPORT_DESCRIPTOR*)(base + dd.VirtualAddress); d->Name; ++d) {
            const char* dll = (const char*)(base + d->Name);
            if (_strnicmp(dll, "xinput", 6) != 0 || !d->OriginalFirstThunk || !d->FirstThunk) continue;
            const bool ord2 = _strnicmp(dll, "xinput1_", 8) == 0;
            auto names = (IMAGE_THUNK_DATA64*)(base + d->OriginalFirstThunk);
            auto iat = (void**)(base + d->FirstThunk);
            for (int i = 0; names[i].u1.AddressOfData; ++i) {
                const ULONGLONG e = names[i].u1.AddressOfData;
                if (IMAGE_SNAP_BY_ORDINAL64(e)) { if (ord2 && IMAGE_ORDINAL64(e) == 2) return &iat[i]; }
                else if (!strcmp(((IMAGE_IMPORT_BY_NAME*)(base + e))->Name, "XInputGetState")) return &iat[i];
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    return nullptr;
}
void FormBlockSet(bool on) {
    if (!g_fbMask.load() && !g_fbRT.load() && !g_fbLS.load()) return;
    static bool hooked = false, tried = false;
    if (on && !tried) {
        tried = true;
        void** slot = FbXInputSlot();
        DWORD old = 0, t = 0;
        if (slot && *slot && VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) {
            g_fbPrev = (FbXGet)*slot;  // published before the swap so the hook never calls a null original
            InterlockedExchangePointer(slot, (void*)&HkFormBlockPad);
            VirtualProtect(slot, sizeof(void*), old, &t);
            hooked = true;
        }
        Log("FormBlockPad: %s mask=%04X triggers=%d leftStick=%d", hooked ? "XInputGetState hooked (outermost)" : "no XInputGetState import - off", g_fbMask.load(), g_fbRT.load(), g_fbLS.load());
    }
    if (!hooked || g_fbActive.exchange(on) == on) return;
    if (on) { g_fbDropped.store(0); g_fbSince.store(GetTickCount64()); Log("FormBlockPad: all actions + walking blocked (camera free) t=%.3f", Now()); }
    else Log("FormBlockPad: released, %ld blocked pad reads t=%.3f", g_fbDropped.load(), Now());
}
// pc #56435: per-second cost counters (hook calls, real writes, QPC ticks inside our code, game ticks = frames)
struct Fh4Perf { std::atomic<long> parts{0}, mult{0}, alpha{0}, writes{0}, ticks{0}; std::atomic<long long> qpc{0}; };
Fh4Perf g_fh4Perf;
struct Fh4Timer { long long t0 = (long long)Qpc(); ~Fh4Timer() { g_fh4Perf.qpc.fetch_add((long long)Qpc() - t0); } };
// Heap fields: plain SEH write, and only when the value differs (no VirtualQuery/VirtualProtect, no needless store).
template <class T>
static bool Fh4Set(uint64_t a, T v) {
    T cur{};
    if (!Rd(a, cur)) return false;
    if (cur == v) return true;
    __try { *(volatile T*)a = v; } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    g_fh4Perf.writes.fetch_add(1);
    return true;
}
SRWLOCK g_fh4Lock = SRWLOCK_INIT;
std::atomic<bool> g_fh4Ready{false}, g_fh4Fault{false};
void* g_fh4PartsOrig = nullptr;
void* g_fh4MultiplierOrig = nullptr;
void* g_fh4AlphaOrig = nullptr;

// All helpers below run with g_fh4Lock held. No stale-root restore on replacement.
void Fh4Restore(uint64_t player) {
    FormBlockSet(false);  // input comes back even when the hide restore below has to retry
    bool ok = true;
    if (player == g_fh4.owner && player == FkPlayer()) {
        uint64_t root = 0; float current = 0; uint8_t h = 0;
        if (!Rd(player + 0x20, root)) ok = false;
        else if (root && root == g_fh4.root) {
            if (!Rd(root + 0x3A8, current)) ok = false;
            else if (current == 0 && !Fh4Set(root + 0x3A8, g_fh4.multiplier)) ok = false;
            if (!Rd(root + 0x248, current)) ok = false;
            else if (current == 0 && !Fh4Set(root + 0x248, g_fh4.fade)) ok = false;
        }
        if (!Rd(player + 0xB440, h)) ok = false;
        else if (h == 1 && !Fh4Set(player + 0xB440, g_fh4.hide)) ok = false;
        if (!Rd(player + 0xB444, current)) ok = false;
        else if (current == 1.f && !Fh4Set(player + 0xB444, g_fh4.blend)) ok = false;
    }
    Log("FormHide=4 restore owner=%llX current=%llX root=%llX ok=%d",
        (unsigned long long)g_fh4.owner, (unsigned long long)player, (unsigned long long)g_fh4.root, ok);
    if (!ok) { g_fh4Fault.store(true); return; } // retain backup for retry
    g_fh4 = {}; g_fhEnd.active = false;
}
bool Fh4ClampRoot(uint64_t chr, uint64_t root, bool nativeFresh) {
    uint64_t actual = 0;
    if (!chr || chr != g_fh4.owner || chr != FkPlayer() || !root ||
        !Rd(chr + 0x20, actual) || actual != root) return false;
    // #56154: the seal chr is captured ~1.2 s after the press, before its model root exists;
    // a missing/unknown seal is the end-reason's job, so only refuse a real shared root here.
    if (g_fh4.sealRoot && g_fh4.sealRoot == root) return false;
    float k8 = 0;
    if (!Rd(root + 0x3A8, k8) || !std::isfinite(k8)) return false;
    // ally #56104: k6=[root+248]=0 also hides slots 6/16-19 (dark throw layer, head, weapon) that k8=0 leaves.
    float k6 = 0;
    if (!Rd(root + 0x248, k6) || !std::isfinite(k6)) return false;
    if (!g_fh4.root) { g_fh4.root = root; g_fh4.multiplier = k8; g_fh4.fade = k6; }
    else if (g_fh4.root != root) return false; // do not adopt a new model while holding the old one
    else {
        if (nativeFresh || k8 != 0) g_fh4.multiplier = k8;
        if (k6 != 0) g_fh4.fade = k6;
    }
    const int m = g_fh4Mask.load();
    return (!(m & 4) || Fh4Set(root + 0x3A8, 0.f)) && (!(m & 8) || Fh4Set(root + 0x248, 0.f));
}
// pc #56223: the seal yokai fades itself out ~5 s after its spawn: its own model root fade [[seal+20]+248] goes 1 -> 0 at +5.0..+5.5 s
// (ally trace, B440/B444/+3A8 of the seal never change), then it asks for despawn at +5.7 s (held by SealDefer). Nobody was visible from
// then until the SealDefer deadline. [Spirit] FormSealKeep=1 (default) holds the seal root +248 at 1.0 while FormHide=4 owns a captured
// seal that is still in the actor table, with its own root (never the player's).
std::atomic<int> g_fhSealKeep{1};
void Fh4KeepSeal() {
    if (!g_fhSealKeep.load() || !g_fh4.owner || !g_fh4.seal) return;
    if (FormHideSealPresent(g_fhEnd.actorGlobal, g_fh4.seal) != 1) return;
    uint64_t sr = 0; float k6 = 0;
    if (!Rd(g_fh4.seal + 0x20, sr) || !sr || sr == g_fh4.root || !Rd(sr + 0x248, k6) || !std::isfinite(k6) || k6 == 1.f) return;
    const float one = 1.f;
    if (WrBytes(sr + 0x248, &one, 4)) {
        static uint64_t logged = 0;
        if (logged != g_fh4.seal && k6 < 0.99f) { logged = g_fh4.seal; Log("FormSealKeep: seal %llX root %llX fade %.3f -> 1 (held until the form ends) t=%.3f",
            (unsigned long long)g_fh4.seal, (unsigned long long)sr, k6, Now()); }
    }
}
bool Fh4Apply(uint64_t chr) {
    if (!chr || chr != g_fh4.owner || chr != FkPlayer()) return false;
    uint64_t root = 0;
    if (!Rd(chr + 0x20, root) || !Fh4ClampRoot(chr, root, false)) return false;
    const int m = g_fh4Mask.load();
    return (!(m & 1) || Fh4Set(chr + 0xB440, (uint8_t)1)) && (!(m & 2) || Fh4Set(chr + 0xB444, 1.f));
}
void HkFh4Parts(uint64_t chr) {
    g_fh4Perf.parts.fetch_add(1);
    if (!g_fh4.owner || chr != g_fh4.owner) { ((void(*)(uint64_t))g_fh4PartsOrig)(chr); return; }  // pc #56435: no lock for other actors
    {
    Fh4Timer tm;
    AcquireSRWLockExclusive(&g_fh4Lock);
    if (g_fh4.owner && chr == g_fh4.owner && !Fh4Apply(chr)) g_fh4Fault.store(true);
    ReleaseSRWLockExclusive(&g_fh4Lock);
    }
    // Before EFC30: b=1 makes ordinary alpha=0; k8=0 makes both special branches zero.
    ((void(*)(uint64_t))g_fh4PartsOrig)(chr);
    Fh4Timer tm;
    AcquireSRWLockExclusive(&g_fh4Lock);
    if (g_fh4.owner && chr == g_fh4.owner && !Fh4Apply(chr)) g_fh4Fault.store(true);
    ReleaseSRWLockExclusive(&g_fh4Lock);
}
void HkFh4Multiplier(uint64_t chr, uint64_t root) {
    ((void(*)(uint64_t, uint64_t))g_fh4MultiplierOrig)(chr, root);
    g_fh4Perf.mult.fetch_add(1);
    if (!g_fh4.owner || (chr != g_fh4.owner && chr != g_fh4.seal)) return;  // the seal call feeds FormSealKeep
    Fh4Timer tm;
    AcquireSRWLockExclusive(&g_fh4Lock);
    if (g_fh4.owner && g_fh4.seal && chr == g_fh4.seal) Fh4KeepSeal();
    if (g_fh4.owner && chr == g_fh4.owner && !Fh4ClampRoot(chr, root, true)) g_fh4Fault.store(true);
    ReleaseSRWLockExclusive(&g_fh4Lock);
}
// #56049: B444 is a crossfade TO the fixed part-46 armour shell, not invisibility.
// EFC30 writes this derived alpha every update. Clamp its output at the actual
// writer, without editing model pointers, equipment state, or the fade controller.
bool Fh4ClampShell(uint64_t parts, uint64_t context) {
    uint64_t chr = 0, root = 0, active = 0, part = 0, render = 0, sealRoot = 0;
    uint32_t enabled = 0, handle = 0; uint8_t hide = 0; float blend = 0, alpha = 0;
    if (!Rd(parts, chr) || !chr || chr != g_fh4.owner || chr != FkPlayer() || parts != chr + 0x9F0 ||
        !Rd(chr + 0x20, root) || root != g_fh4.root || !root || context != root ||
        !Rd(chr + 0xB440, hide) || hide != 1 || !Rd(chr + 0xB444, blend) || blend != 1.f) return false;
    sealRoot = g_fh4.sealRoot;
    if (sealRoot && sealRoot == root) return false; // #56154: unloaded seal root is not a fault
    if (!Rd(parts + 0x9F60, active) || !Rd(parts + 0x9F6C, enabled)) return false;
    if (!active || !enabled) active = parts + 0x20; // exact F5358/EFD74 selection
    if (!Rd(active + 0x9590, part)) return false;
    if (!part) return true; // no shell loaded
    if (!Rd(part + 0x20, render)) return false;
    if (!render) return true;
    if (render == root || render == sealRoot || !Rd(part, handle) || !Rd(render + 0xC, alpha) || !std::isfinite(alpha)) return false;
    // Four native type-5 models from F3CD6..F3D62; never clamp an arbitrary FormBody replacement.
    const uint32_t id = handle >> 4;
    if (id != 0x58C2A4 && id != 0x76D58 && id != 0x6CCDE0 && id != 0x295A89) return false;
    // Recheck identity immediately before the write; keep no attachment pointer across frames.
    uint64_t check = 0;
    if (!Rd(active + 0x9590, check) || check != part || !Rd(part + 0x20, check) || check != render) return false;
    const bool ok = !(g_fh4Mask.load() & 16) || Fh4Set(render + 0xC, 0.f);
    const double now = Now();
    if (now >= g_fh4.nextShellLog) {
        Log("FormHide=4 shell46 id=%X part=%llX render=%llX nativeAlpha=%.6f zero=%d t=%.3f",
            id, (unsigned long long)part, (unsigned long long)render, alpha, ok, now);
        g_fh4.nextShellLog = now + 1;
    }
    return ok;
}
void HkFh4Alpha(uint64_t parts, uint8_t flag, uint64_t context) {
    // Preserve native updates and all arguments, including calls on unrelated actors.
    ((void(*)(uint64_t, uint8_t, uint64_t))g_fh4AlphaOrig)(parts, flag, context);
    g_fh4Perf.alpha.fetch_add(1);
    if (!g_fh4.owner || parts != g_fh4.owner + 0x9F0) return;
    Fh4Timer tm;
    AcquireSRWLockExclusive(&g_fh4Lock);
    uint64_t chr = 0;
    if (g_fh4Ready.load() && g_fhMode.load() == 4 && !g_fh4Fault.load() &&
        g_fh4.owner && Rd(parts, chr) && chr == g_fh4.owner && !Fh4ClampShell(parts, context))
        g_fh4Fault.store(true);
    ReleaseSRWLockExclusive(&g_fh4Lock);
    // No retained alpha override: after release the next native EFC30 call
    // computes the ordinary value. The clamped field is an output, not fade state.
}
void FormHide4Install() {
    if (g_fhMode.load() != 4 || g_fh4Ready.load()) return;
    static bool attempted = false;
    if (attempted) return;
    attempted = true;
    // Check the actual offset-bearing instructions, never fake a known executable.
    static const uint8_t g0[] = {0x48,0x8B,0x81,0x60,0x9F,0x00,0x00,0x48,0x85,0xC0,0x75,0x05,0x48,0x8D,0x41,0x20,0xC3,0x83,0xB9,0x6C,0x9F,0x00,0x00,0x00,0x75,0xF6,0xEB,0xF0};
    if (!FormHideCodeMatches(0xF5358, g0, sizeof(g0))) { Log("FormHide=4 layout guard +F5358 failed"); return; }
    static const uint8_t g1[] = {0x48,0x8B,0x53,0x20,0x48,0x85,0xD2,0x74,0x2A,0xF3,0x0F,0x10,0x83,0x44,0xB4,0x00,0x00,0x0F,0x2F,0xC6,0x73,0x04,0xF3,0x0F,0x5C,0xF0,0xF3,0x0F,0x11,0x72,0x0C};
    if (!FormHideCodeMatches(0x401985, g1, sizeof(g1))) { Log("FormHide=4 layout guard +401985 failed"); return; }
    static const uint8_t g2[] = {0xF3,0x0F,0x10,0xB7,0x48,0x02,0x00,0x00,0xF3,0x44,0x0F,0x10,0x87,0xA8,0x03,0x00,0x00};
    if (!FormHideCodeMatches(0xEFD5A, g2, sizeof(g2))) { Log("FormHide=4 layout guard +EFD5A failed"); return; }
    static const uint8_t g3[] = {0x49,0x8B,0x50,0x10,0x48,0x85,0xD2,0x74,0x3D,0x4C,0x8B,0x4A,0x20,0x4D,0x85,0xC9,0x74,0x34,0x49,0x8B,0x43,0x08,0x8B,0x08,0x41,0x89,0x49,0x0C};
    if (!FormHideCodeMatches(0xFDE6A2, g3, sizeof(g3))) { Log("FormHide=4 layout guard +FDE6A2 failed"); return; }
    static const uint8_t g4[] = {0x48,0x89,0x5C,0x24,0x10,0x57,0x48,0x83,0xEC,0x20,0x80,0xB9,0xD4,0x09,0x00,0x00,0x00,0x40,0x8A,0xFA,0x48,0x8B,0xD9,0x88,0x91,0x40,0xB4,0x00,0x00};
    if (!FormHideCodeMatches(0xA550E8, g4, sizeof(g4))) { Log("FormHide=4 layout guard +A550E8 failed"); return; }
    static const uint8_t g5[] = {0xF3,0x41,0x0F,0x10,0x8B,0x44,0xB4,0x00,0x00,0x48,0x8D,0x45,0x20,0xF3,0x0F,0x10};
    if (!FormHideCodeMatches(0xEFC99, g5, sizeof(g5))) { Log("FormHide=4 layout guard +EFC99 failed"); return; }
    static const uint8_t g6[] = {0xF3,0x0F,0x11,0x8E,0x44,0xB4,0x00,0x00};
    if (!FormHideCodeMatches(0x403E99, g6, sizeof(g6))) { Log("FormHide=4 layout guard +403E99 failed"); return; }
    static const uint8_t g7[] = {0x48,0x8B,0xC4,0x48,0x89,0x58,0x08,0x48,0x89,0x70,0x10,0x57,0x48,0x83,0xEC,0x50,0x80,0xB9,0x08,0x01,0x00,0x00,0x00,0x48,0x8B,0xF2,0x0F,0x29,0x70,0xE8};
    if (!FormHideCodeMatches(0x567568, g7, sizeof(g7))) { Log("FormHide=4 layout guard +567568 failed"); return; }
    static const uint8_t g8[] = {0xF3,0x0F,0x11,0xB6,0xA8,0x03,0x00,0x00};
    if (!FormHideCodeMatches(0x56778C, g8, sizeof(g8))) { Log("FormHide=4 layout guard +56778C failed"); return; }
    static const uint8_t g9[] = {0x48,0x8B,0xD6,0x48,0x8B,0xCF,0xE8,0x63,0x53,0x40,0x00,0x48,0x8B,0xD6,0x48,0x8B};
    if (!FormHideCodeMatches(0x1621FA, g9, sizeof(g9))) { Log("FormHide=4 layout guard +1621FA failed"); return; }
    static const uint8_t shellWriter[] = {0x48,0x8B,0x83,0x60,0x9F,0x00,0x00,0x48,0x85,0xC0,0x74,0x09,0x83,0xBB,0x6C,0x9F,0x00,0x00,0x00,0x75,0x04,0x48,0x8D,0x43,0x20,0x48,0x8B,0x88,0x90,0x95,0x00,0x00,0x48,0x85,0xC9,0x74,0x76,0x4C,0x8B,0x41,0x20,0x4D,0x85,0xC0,0x74,0x6D,0x0F,0x28,0xC6,0xF3,0x0F,0x59,0x45,0x20,0xF3,0x41,0x0F,0x59,0xC0,0xF3,0x41,0x0F,0x11,0x40,0x0C,0x8B,0x01,0xC1,0xE8,0x04,0x3D,0xA4,0xC2,0x58,0x00};
    if (!FormHideCodeMatches(0xEFD74, shellWriter, sizeof(shellWriter))) { Log("FormHide=4 shell guard +EFD74 failed"); return; }
    static const uint8_t shellCall[] = {0x48,0x8B,0x7C,0x24,0x38,0x41,0x8A,0xD4,0x4C,0x8B,0xC7,0x48,0x8B,0xCB,0xE8,0x7B,0x9F,0xFF,0xFF,0x48,0x8B,0x0B};
    if (!FormHideCodeMatches(0xF5CA2, shellCall, sizeof(shellCall))) { Log("FormHide=4 shell guard +F5CA2 failed"); return; }
    static const uint8_t shellOwner[] = {0x4C,0x8B,0x19,0x49,0x8B,0xF8,0x0F,0x29,0x70,0xD8,0x40,0x8A,0xF2,0x0F,0x29,0x78,0xC8,0x48,0x8B,0xD9,0x44,0x0F,0x29,0x40,0xB8};
    if (!FormHideCodeMatches(0xEFC41, shellOwner, sizeof(shellOwner))) { Log("FormHide=4 shell guard +EFC41 failed"); return; }
    static const uint32_t calls[] = {0x403495,0x403EC6,0x1A219A6,0x1C3687E,0x2052EA7,0x2055969,0x162200,0xF5CB0};
    for (uint32_t rva : calls) {
        const uint64_t target = rva == 0xF5CB0 ? 0xEFC30 : rva == 0x162200 ? 0x567568 : 0x40169C;
        uint8_t b[5] = {}; int32_t rel = 0;
        if (!Copy(b, g_base + rva, 5)) return;
        memcpy(&rel, b + 1, 4);
        if (b[0] != 0xE8 || g_base + rva + 5 + rel != g_base + target) {
            Log("FormHide=4 call guard +%X failed", rva); return;
        }
    }
    // Set originals before installing any live retarget; partial installation never arms writes.
    g_fh4PartsOrig = (void*)(g_base + 0x40169C);
    g_fh4MultiplierOrig = (void*)(g_base + 0x567568);
    g_fh4AlphaOrig = (void*)(g_base + 0xEFC30);
    unsigned n = 0;
    for (uint32_t rva : calls) {
        const bool mult = rva == 0x162200, alpha = rva == 0xF5CB0;
        const char* why = nullptr; void* original = nullptr;
        if (!InstallCallRetargetAt((uint8_t*)(g_base + rva),
                alpha ? "call:FormHide4ShellAlpha" : mult ? "call:FormHide4Multiplier" : "call:FormHide4Parts",
                alpha ? (void*)&HkFh4Alpha : mult ? (void*)&HkFh4Multiplier : (void*)&HkFh4Parts, &original, &why)) {
            Log("FormHide=4 hook +%X failed (%s)", rva, why ? why : "unknown"); break;
        }
        ++n;
    }
    g_fh4Ready.store(n == 8);
    Log("FormHide=4 hooks=%u/8 ready=%d (B440=1 B444=1 root+3A8=0 shell46+C=0)", n, g_fh4Ready.load());
}
// pc #56436 [Spirit] FastThrow=<x> (0 = off): the seal throw is the player's actions 0x28FD (~1.0 s, native play rate 2.2) then 0x28FB;
// the yokai spawns from that motion's timeline ~1.2 s after the press. Poking the play-rate fields [chr+40]+324.. directly is undone every
// frame (ally live test: layer refresh recomputes them), so all 18 calls of ComputeMotionSpeed 134E68(ctrl, layer, flag) -> xmm0
// (notes/boss-speed-lever-22389.md, [ctrl+E0] = actor) are retargeted: while the player's current action is in FastThrowActs (default
// 28FD,28FB) the rate is multiplied by FastThrow, so the throw is over in a few frames and the timeline still spawns the yokai.
std::atomic<float> g_ftMul{0};
// pc #57162 (human #57147/#57150): [Spirit] SealTailSpeed=<x> (default 3, <=1 off). SealEndOnHit no longer despawns the seal at its last hit
// burst; the seal actor's motion rate (same 134E68 hooks) is multiplied by x from that point until its natural end/despawn, so the yokai
// plays its whole animation, only faster, and the form then ends on seal-despawn as before.
// pc #57226 (human #57218/#57219): [Spirit] SealSpeed=<x> (default 1.4, <=1 off) = the bound seal's motion rate x from bind/spawn until it
// despawns, SealTailSpeed default 4, and at the last hit the rate ramps linearly from SealSpeed to SealTailSpeed over SealTailRampMs (250)
// instead of a step jump, so the whole move is a bit faster and the end smooth.
std::atomic<float> g_tailMul{4.f};
std::atomic<float> g_sealMul{1.4f};
std::atomic<float> g_tailRampMs{250.f};
std::atomic<double> g_tailT0{0};
std::atomic<uint64_t> g_tailSeal{0};
float SealRateMul(uint64_t a) {  // 1 = leave the rate alone
    const float base = g_sealMul.load() > 1.f ? g_sealMul.load() : 1.f;
    const uint64_t ts = g_tailSeal.load(std::memory_order_relaxed);
    if (ts && a == ts) {
        const float t = g_tailMul.load() > 1.f ? g_tailMul.load() : base, ms = g_tailRampMs.load();
        const double e = ms > 0.f ? (Now() - g_tailT0.load()) * 1000.0 / ms : 1.0;
        return e >= 1.0 ? t : e <= 0.0 ? base : base + (t - base) * (float)e;
    }
    return g_fh4.owner && a == g_fh4.seal ? base : 1.f;
}
std::atomic<uint32_t> g_ftActs[4];
std::atomic<int> g_ftN{0};
void* g_ftOrig = nullptr;
float HkMotionSpeed(uint64_t ctrl, uint8_t layer, uint8_t flag) {
    const float r = ((float (*)(uint64_t, uint8_t, uint8_t))g_ftOrig)(ctrl, layer, flag);
    if (!(r > 0.f) || r > 50.f) return r;
    uint64_t a = 0;
    if (!Rd(ctrl + 0xE0, a) || !a) return r;
    if (const float sm = SealRateMul(a); sm != 1.f) {
        static uint64_t loggedA = 0; static float loggedM = 0; static double loggedT = 0;
        if (a != loggedA || (std::fabs(sm - loggedM) >= 0.5f && Now() - loggedT > 0.05) || (sm != loggedM && Now() - loggedT > 2) || (sm != loggedM && sm == g_tailMul.load())) {
            loggedA = a; loggedM = sm; loggedT = Now();
            Log("SealSpeed: seal %llX rate %.3f x%.2f t=%.3f", (unsigned long long)a, r, sm, Now());
        }
        return r * sm;
    }
    const float m = g_ftMul.load();
    if (m <= 1.f || a != FkPlayer()) return r;
    const uint32_t id = PlayerActId(a);
    bool hit = false;
    for (int i = 0, n = g_ftN.load(); i < n; ++i) hit = hit || g_ftActs[i].load() == id;
    if (!hit) return r;
    static uint32_t loggedId = 0; static double loggedT = 0;
    if (id != loggedId || Now() - loggedT > 5) { loggedId = id; loggedT = Now(); Log("FastThrow: player action 0x%X layer %u rate %.3f -> %.3f t=%.3f", id, layer, r, r * m, Now()); }
    return r * m;
}
void FastThrowInstall() {
    const int pct = IniInt(L"Spirit", L"FastThrow", 0);
    { wchar_t tb[32]; GetPrivateProfileStringW(L"Spirit", L"SealTailSpeed", L"4", tb, 32, IniPath()); g_tailMul.store((float)wcstod(tb, nullptr)); }
    { wchar_t tb[32]; GetPrivateProfileStringW(L"Spirit", L"SealSpeed", L"1.4", tb, 32, IniPath()); g_sealMul.store((float)wcstod(tb, nullptr)); }
    g_tailRampMs.store((float)IniInt(L"Spirit", L"SealTailRampMs", 250));
    const bool tail = g_tailMul.load() > 1.f && IniInt(L"Spirit", L"SealEndOnHit", 0) > 0;
    const bool sspeed = g_sealMul.load() > 1.f && IniInt(L"Spirit", L"FormHide", 0) == 4;
    if (pct <= 1 && !tail && !sspeed) return;
    wchar_t ab[128] = {};
    GetPrivateProfileStringW(L"Spirit", L"FastThrowActs", L"28FD,28FB", ab, 128, IniPath());
    int n = 0;
    for (wchar_t* q = ab; *q && n < 4;) {
        while (*q == L',' || *q == L' ') ++q;
        if (!*q) break;
        wchar_t* e = nullptr;
        const unsigned long v = wcstoul(q, &e, 16);
        if (e == q) break;
        if (v) g_ftActs[n++].store((uint32_t)v);
        q = e;
    }
    g_ftN.store(n);
    static const uint8_t pro[] = {0x48,0x8B,0xC4,0x48,0x89,0x58,0x08,0x48,0x89,0x68,0x10,0x48,0x89,0x70,0x18,0x48,0x89,0x78,0x20};
    if (!FormHideCodeMatches(0x134E68, pro, sizeof(pro))) { Log("FastThrow: 134E68 prologue differs - off"); return; }
    static const uint32_t calls[] = {0x133C3F,0x20DCCB,0x20DF26,0x20DF3F,0x4F3827,0x66CB7C,0x66CD36,0x66D883,0x66D8F4,0x66DEC0,0x66EC3C,0x673B2C,
                                     0x67B1DE,0x6860F5,0x6861B8,0x9B8133,0x9B819F,0x201821A};
    for (uint32_t rva : calls) {  // verify every site first; never install a partial set
        uint8_t b[5] = {}; int32_t rel = 0;
        if (!Copy(b, g_base + rva, 5)) { Log("FastThrow: +%X unreadable - off", rva); return; }
        memcpy(&rel, b + 1, 4);
        if (b[0] != 0xE8 || g_base + rva + 5 + rel != g_base + 0x134E68) { Log("FastThrow: call guard +%X failed - off", rva); return; }
    }
    g_ftOrig = (void*)(g_base + 0x134E68);
    unsigned k = 0;
    for (uint32_t rva : calls) {
        const char* why = nullptr; void* original = nullptr;
        if (!InstallCallRetargetAt((uint8_t*)(g_base + rva), "call:FastThrow134E68", (void*)&HkMotionSpeed, &original, &why)) {
            Log("FastThrow: hook +%X failed (%s)", rva, why ? why : "?"); break;
        }
        ++k;
    }
    g_ftMul.store(pct > 1 ? (float)pct : 0.f);
    Log("FastThrow: x%d on actions %d ids, hooks %u/18; SealSpeed x%.2f%s; SealTailSpeed x%.2f%s ramp %.0f ms", pct, n, k, g_sealMul.load(),
        sspeed ? "" : " (off)", g_tailMul.load(), tail ? "" : " (off)", g_tailRampMs.load());
    if (!sspeed) g_sealMul.store(0.f);
    if (k != 18) { g_tailMul.store(0.f); g_sealMul.store(0.f); }  // partial hook set: no tail speedup, SealEndOnHit falls back to the despawn
}
// pc #56515 [Spirit] OrbSkip=1: the seal item throws an orb object (322468 per-frame update, notes/soulcore-orb-56515.md) that shows
// effect 0x6254D and flies until the ground test 6A3BB0 (call 322DD9) hits; only then does the landing object burst and spawn the yokai
// (+0.67 s on ally, +2.6 s on pc). Two call retargets inside 322468, both only for the seal orb (orb effect param O+0x644 == 0x6254D):
// 322DD9 ground test returns 1 on the first flying tick (lands next to the player), 322958 skips creating the orb effect (zero handle,
// the game's own failure result at 29F0E7). Ally live /proc test #56515: no 0x6254D, burst +0.13 s, yokai +0.41 s (was +0.67 s).
void* g_orbLandOrig = nullptr;
void* g_orbFxOrig = nullptr;
std::atomic<uint32_t> g_orbHits{0};
uint64_t HkOrbGround(uint64_t o, uint64_t edx, uint64_t r8, uint64_t r9) {
    uint32_t fx = 0;
    if (o > 0x10000 && Rd(o + 0x644, fx) && fx == 0x6254D) {
        if (g_orbHits.fetch_add(1) < 4) Log("OrbSkip: seal orb %llX lands on its first tick t=%.3f", (unsigned long long)o, Now());
        return 1;
    }
    return ((uint64_t (*)(uint64_t, uint64_t, uint64_t, uint64_t))g_orbLandOrig)(o, edx, r8, r9);
}
uint64_t HkOrbFx(uint64_t ctrl, uint64_t out, uint64_t fx, uint64_t r9, uint64_t s5, uint64_t s6, uint64_t s7, uint64_t s8) {
    if ((uint32_t)fx == 0x6254D && out > 0x10000) {
        *(volatile uint32_t*)out = 0;
        Log("OrbSkip: orb effect 0x6254D skipped (ctrl %llX) t=%.3f", (unsigned long long)ctrl, Now());
        return out;
    }
    return ((uint64_t (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t))g_orbFxOrig)(ctrl, out, fx, r9, s5, s6, s7, s8);
}
void OrbSkipInstall() {
    if (IniInt(L"Spirit", L"OrbSkip", 0) <= 0) return;
    static const uint8_t land[] = {0x81,0xC2,0x32,0x05,0x00,0x00,0xE8,0xD2,0x0D,0x38,0x00,0x84,0xC0,0x74,0x21};
    static const uint8_t fx[] = {0x48,0x8B,0xCF,0xE8,0xDB,0xC5,0xF7,0xFF,0x8B,0x08,0x89,0x4F,0x78};
    if (!FormHideCodeMatches(0x322DD3, land, sizeof(land)) || !FormHideCodeMatches(0x322955, fx, sizeof(fx))) {
        Log("OrbSkip: 322468 call bytes differ (not this game build) - off"); return;
    }
    const char* why = nullptr;
    if (!InstallCallRetargetAt((uint8_t*)(g_base + 0x322DD9), "call:OrbSkipGround", (void*)&HkOrbGround, &g_orbLandOrig, &why)) {
        Log("OrbSkip: ground hook failed (%s) - off", why ? why : "?"); return;
    }
    if (!InstallCallRetargetAt((uint8_t*)(g_base + 0x322958), "call:OrbSkipFx", (void*)&HkOrbFx, &g_orbFxOrig, &why))
        Log("OrbSkip: orb effect hook failed (%s) - orb lands at once but its effect may flash", why ? why : "?");
    Log("OrbSkip: on (seal orb lands on its first tick, orb effect 0x6254D off)");
}
// 0 = pin seal to player, 1 = follow the seal, 2 = neither (auto probe running). Runs under g_fh4Lock.
int Fh4FollowDecide(double now) {
    const int m = g_fhFollow.load();
    if (m <= 0) return 0;
    if (m == 1) return 1;
    if (g_fh4.follow >= 0) return g_fh4.follow;
    // pc #57103: auto misfired on LEFT 5129B (lunge moved the seal 151 cm in 0.32 s vs 150 -> the player was warped = "teleport"). A seal
    // FormSlot0 captured (LEFT) is probed against FormFollowCm (default now 400); a spawn-hit seal (DOWN A2ACB) follows at once.
    if (g_fh4.fromSpawn) {  // no FormSlot0 kind-0 capture = not the LEFT (5129B) body: traveling core (DOWN A2ACB), follow at once
        g_fh4.follow = 1;
        Log("FormFollowSeal auto: seal bound from the spawn hit (no FormSlot0 capture) -> FOLLOW");
        return 1;
    }
    float p[4] = {};
    if (!Copy(p, g_fh4.seal + 0x110, 16)) return 2;
    if (g_fh4.followT0 <= 0) { memcpy(g_fh4.sealP0, p, sizeof(p)); g_fh4.followT0 = now; return 2; }
    const float dx = p[0] - g_fh4.sealP0[0], dy = p[1] - g_fh4.sealP0[1], dz = p[2] - g_fh4.sealP0[2];
    const float d = sqrtf(dx * dx + dy * dy + dz * dz);
    if (d >= (float)g_fhFollowCm.load()) g_fh4.follow = 1;
    else if ((now - g_fh4.followT0) * 1000.0 >= g_fhFollowProbeMs.load()) g_fh4.follow = 0;
    else return 2;
    Log("FormFollowSeal auto: seal moved %.0f cm in %.2f s -> %s", d, now - g_fh4.followT0, g_fh4.follow ? "FOLLOW (player rides the seal)" : "pin");
    return g_fh4.follow;
}
void FormHide4Tick(uint64_t player) {
    static bool leftWas = false;
    static uint64_t attempted = 0;
    static double attemptedT = -1;
    if (g_fhMode.load() != 4 && !g_fh4.owner) return;
    uint64_t cap = g_fs0CapChr.load(); double capT = g_fs0CapT; const double now = Now();
    if (g_fh4.owner && g_fh4.fromSpawn) { cap = g_fh4.seal; capT = g_fh4.capT; }  // keep the spawn-hit seal (FormSlot0 never captures it)
    else if (g_fhFollow.load() && g_fh4.owner && !g_fh4.seal) {  // pc #56910: no kind-0 capture for some cores (DOWN A2ACB) -> use the spawn hit
        const uint64_t sc = g_fkSealChr.load(); const double st = g_fkSealT;
        if (sc && st >= g_fh4.started - .05 && !(cap && capT >= g_fh4.started - .05) && now - st >= 0.3) {
            Log("FormHide=4 seal from spawn hit (no FormSlot0 capture) chr=%llX desc=%X t=%.3f", (unsigned long long)sc, g_fkSealDesc.load(), now);
            cap = sc; capT = st; g_fh4.fromSpawn = true;
        }
    }
    {   // pc #56435: 1 line/s cost report (ticks/s ~ game frames/s)
        static double nextPerf = 0;
        g_fh4Perf.ticks.fetch_add(1);
        if (now >= nextPerf) {
            LARGE_INTEGER fq; QueryPerformanceFrequency(&fq);
            if (nextPerf > 0)
                Log("FH4perf ticks=%ld parts=%ld mult=%ld alpha=%ld writes=%ld ours=%.0fus holding=%d mask=%X t=%.3f", g_fh4Perf.ticks.exchange(0),
                    g_fh4Perf.parts.exchange(0), g_fh4Perf.mult.exchange(0), g_fh4Perf.alpha.exchange(0), g_fh4Perf.writes.exchange(0),
                    g_fh4Perf.qpc.exchange(0) * 1e6 / (double)fq.QuadPart, g_fh4.owner != 0, g_fh4Mask.load(), now);
            nextPerf = now + 1;
        }
    }
    const bool left = (PadButtons(nullptr, nullptr) & 4) != 0;
    const bool edge = g_fhPress.load() &&
        ((left && !leftWas && g_fhPress.load() != 2) || FhSealActTick(player, cap, capT, now));
    leftWas = left;
    AcquireSRWLockExclusive(&g_fh4Lock);
    if (g_fh4.owner) {
        const char* reason = nullptr;
        if (g_fhMode.load() != 4) reason = "mode-change";
        else if (player != g_fh4.owner) reason = "player-change";
        else if (g_fh4Fault.load()) reason = "apply-fault";
        else if (!g_fh4.seal) {
            if (cap && capT >= g_fh4.started - .05) {
                g_fh4.seal = attempted = cap; g_fh4.capT = attemptedT = capT;
                g_fh4.started = now; FormHideEndBegin(player, cap);
                FormHideEndReason(player, cap, now);
            } else if (now - g_fh4.started > 2.5) reason = "press-no-seal";
        } else if (cap != g_fh4.seal || capT != g_fh4.capT) reason = "capture-change";
        else reason = FormHideEndReason(player, cap, g_fh4.started);
        if (reason) {
            Log("FormHide=4 end %s", reason); Fh4Restore(player);
            // pc #57103: the FormSlot0 capture outlives the form (cleared only at 60 s). A spawn-hit form (DOWN A2ACB) never consumed it, so
            // the same tick re-began a form on the stale LEFT seal -> hidden + locked 20 s. Every end consumes whatever capture is current.
            attempted = g_fs0CapChr.load(); attemptedT = g_fs0CapT;
        }
        else {
            if (g_fh4.seal && now >= g_fh4.nextSealPoll) {  // pc #56435: hooks read this cache instead of walking the actor table per call
                uint64_t sr = 0;
                g_fh4.sealRoot = FormHideSealPresent(g_fhEnd.actorGlobal, g_fh4.seal) == 1 && Rd(g_fh4.seal + 0x20, sr) ? sr : 0;
                g_fh4.nextSealPoll = now + 0.1;
            }
            if (!Fh4Apply(player)) g_fh4Fault.store(true);
            if (g_fh4.seal) {
                Fh4Timer tm;
                const int fd = Fh4FollowDecide(now);
                if (fd == 1) FormFollow(player, g_fh4.seal);
                else if (fd == 0) FormPin(player, g_fh4.seal);
                Fh4KeepSeal();
            }  // timed: FH4perf ours= includes the pin + warp
        }
    } else if (g_fhMode.load() == 4 && g_fh4Ready.load() && player &&
               (edge || (cap && now >= capT && now - capT < 2.5 && (cap != attempted || capT != attemptedT) &&
                         FormHideSealPresent(g_fhEnd.actorGlobal, cap) != 0))) {  // pc #57103: only a fresh capture of a live seal begins a form
        if (Rd(player + 0xB440, g_fh4.hide) && Rd(player + 0xB444, g_fh4.blend) && std::isfinite(g_fh4.blend)) {
            g_fh4.owner = player; g_fh4.started = now; g_fh4Fault.store(false);
            FormBlockSet(true);
            if (!edge) {
                g_fh4.seal = attempted = cap; g_fh4.capT = attemptedT = capT;
                FormHideEndBegin(player, cap); FormHideEndReason(player, cap, now);
            }
            if (!Fh4Apply(player)) { g_fh4Fault.store(true); Fh4Restore(player); }
            else Log("FormHide=4 begin player=%llX seal=%llX root=%llX t=%.3f",
                (unsigned long long)player, (unsigned long long)g_fh4.seal, (unsigned long long)g_fh4.root, now);
        }
    }
    if (g_fh4.owner && player == g_fh4.owner && now >= g_fh4.nextLog) {
        uint8_t h = 255; float b = -1, k6 = -1, k8 = -1;
        Rd(player + 0xB440, h); Rd(player + 0xB444, b);
        uint64_t root = 0;
        if (Rd(player + 0x20, root) && root && root == g_fh4.root) {
            Rd(root + 0x248, k6); Rd(root + 0x3A8, k8);
        }
        float sp[4] = {}, pp[4] = {};
        if (g_fh4.seal) Copy(sp, g_fh4.seal + 0x110, 16);
        Copy(pp, player + 0x110, 16);
        Log("FormHide=4 hold B440=%u B444=%.6f k6=%.6f k8=%.6f age=%.3f fault=%d player=%.0f %.0f %.0f seal=%.0f %.0f %.0f follow=%d",
            (unsigned)h, b, k6, k8, now - g_fh4.started, g_fh4Fault.load(), pp[0], pp[1], pp[2], sp[0], sp[1], sp[2], g_fh4.follow);
        g_fh4.nextLog = now + 1;
    }
    ReleaseSRWLockExclusive(&g_fh4Lock);
}

void FormHideDirectTick(uint64_t pchr) {
    static uint64_t owner = 0, seal = 0, attempted = 0, extra = 0, extraDone = 0, sealAtHide = 0;  // sealAtHide: g_fkSealChr when the seal was bound
    static double attemptedT = -1, hideT = 0, pressT = 0;
    static uint8_t saved = 0, extraSaved = 0;
    static int pins = 0;
    static bool wasLeft = false;
    const int mode = g_fhMode.load();
    const uint64_t cap = g_fs0CapChr.load();
    const double capT = g_fs0CapT;
    const double now = Now();
    if (!owner && mode != 3) return;
    static const uint8_t want[29] = {
        0x48, 0x89, 0x5C, 0x24, 0x10, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x80, 0xB9, 0xD4, 0x09, 0x00, 0x00, 0x00,
        0x40, 0x8A, 0xFA, 0x48, 0x8B, 0xD9, 0x88, 0x91, 0x40, 0xB4, 0x00, 0x00
    };
    auto guarded = [&]() {
        uint8_t b[sizeof(want)] = {};
        return Copy(b, g_base + 0xA550E8, sizeof(b)) && memcmp(b, want, sizeof(b)) == 0;
    };
    bool leftEdge = false, actEdge = false;
    if (g_fhPress.load() && mode == 3) {
        const bool left = (PadButtons(nullptr, nullptr) & 0x0004) != 0;  // XINPUT_GAMEPAD_DPAD_LEFT
        const bool padEdge = left && !wasLeft;
        wasLeft = left;
        if (padEdge) Log("FormHide=3 pad LEFT edge (XInput) t=%.3f", now);
        leftEdge = padEdge && g_fhPress.load() != 2;  // FormHidePress=2: seal action only, the pad edge is logged for timing
        actEdge = FhSealActTick(pchr, cap, capT, now);  // pc #55814: pad-independent
    }
    if (owner) {
        if (pchr != owner) {
            Log("FormHide=3 abandon: player changed %llX -> %llX; no stale-pointer restore", (unsigned long long)owner, (unsigned long long)pchr);
            owner = 0;
            g_fhEnd.active = false;
            return;
        }
        const char* reason = nullptr;
        if (!seal) {  // press-hide waiting for the seal capture
            if (mode == 3 && cap && capT >= pressT - 0.05) {
                seal = attempted = cap;
                sealAtHide = g_fkSealChr.load();
                attemptedT = capT;
                hideT = now;
                pins = 0;
                FormHideEndBegin(pchr, cap);
                FormHideEndReason(pchr, cap, hideT);
                Log("FormHide=3 press-hide adopted seal=%llX %.3f s after the press t=%.3f", (unsigned long long)cap, capT - pressT, now);
                return;
            }
            reason = mode != 3 ? "mode-change" : now - pressT > 2.5 ? "press-no-seal-2.5s" : nullptr;
        } else {
            reason = mode != 3 ? "mode-change" : cap != seal || capT != attemptedT ? "capture-change" : FormHideEndReason(pchr, seal, hideT);
        }
        if (!reason) {
            if (!seal) return;
            if (FormPin(pchr, seal) && (++pins == 1 || pins % 600 == 0)) Log("FormHide=3 pin #%d t=%.3f", pins, now);
            const uint64_t sc = g_fkSealChr.load();
            if (g_fhExtra.load() && sc && sc != seal && sc != pchr && sc != sealAtHide && sc != extraDone && guarded()) {  // only a chr loaded after binding
                extraDone = sc;
                uint8_t eb = 0xFF, ea = 0xFF;
                const uint8_t hide = 1;
                const bool ok = Rd(sc + 0xB440, eb) && WrBytes(sc + 0xB440, &hide, 1) && Rd(sc + 0xB440, ea) && ea == 1;
                if (ok) { extra = sc; extraSaved = eb; }
                Log("FormHide=3 EXTRA seal chr=%llX hide byte %u->%u ok=%d t=%.3f", (unsigned long long)sc, (unsigned)eb, (unsigned)ea, ok, now);
            }
            return;
        }
        uint8_t current = 0xFF, after = 0xFF;
        if (!guarded() || !Rd(owner + 0xB440, current)) return;  // keep ownership so a later tick can retry
        if (extra) {  // only touch the extra chr while it is provably still a live actor
            const int present = FormHideSealPresent(g_fhEnd.actorGlobal, extra);
            uint8_t ec = 0xFF;
            const bool eok = present == 1 && Rd(extra + 0xB440, ec) && ec == 1 && WrBytes(extra + 0xB440, &extraSaved, 1);
            Log("FormHide=3 EXTRA restore chr=%llX present=%d byte=%u ok=%d", (unsigned long long)extra, present, (unsigned)ec, eok);
            extra = 0;
        }
        if (current != 1) {
            Log("FormHide=3 restore skipped (%s): byte changed externally to %u; saved=%u", reason, (unsigned)current, (unsigned)saved);
            owner = 0;
            g_fhEnd.active = false;
            return;
        }
        const bool ok = WrBytes(owner + 0xB440, &saved, 1) && Rd(owner + 0xB440, after) && after == saved;
        Log("FormHide=3 RESTORE (%s) player=%llX saved=%u byte=%u->%u ok=%d pins=%d t=%.3f", reason, (unsigned long long)owner,
            (unsigned)saved, (unsigned)current, (unsigned)after, ok, pins, now);
        if (ok) { owner = 0; g_fhEnd.active = false; }
        return;
    }
    const bool press = (leftEdge || actEdge) && pchr;
    if (!press && (mode != 3 || !pchr || !cap || now < capT || (attempted == cap && attemptedT == capT))) return;
    if (!press) { attempted = cap; attemptedT = capT; }
    if (!guarded()) { Log("FormHide=3: full 29-byte A550E8 guard differs - skipped"); return; }
    uint8_t before = 0xFF, after = 0xFF;
    if (!Rd(pchr + 0xB440, before)) { Log("FormHide=3: hide-byte read failed - skipped"); return; }
    const uint8_t hide = 1;
    const bool wrote = WrBytes(pchr + 0xB440, &hide, 1);
    if (wrote) {  // retain the backup even if the readback fails
        owner = pchr;
        saved = before;
        hideT = now;
        pins = 0;
        extra = 0;
        if (press) {
            seal = 0;
            pressT = now;
        } else {
            seal = cap;
            sealAtHide = g_fkSealChr.load();
            FormHideEndBegin(pchr, cap);
            FormHideEndReason(pchr, cap, hideT);  // observe the initial live state immediately
        }
    }
    const bool ok = wrote && Rd(pchr + 0xB440, after) && after == hide;
    Log("FormHide=3 APPLY (%s) player=%llX saved=%u byte=%u->%u ok=%d seal=%llX untilEnd=%d t=%.3f", press ? (actEdge ? "seal action" : "seal press") : "capture",
        (unsigned long long)pchr, (unsigned)before, (unsigned)before, (unsigned)after, ok, (unsigned long long)(press ? 0 : cap), g_fhUntilEnd.load(), now);
}
void FormHideTick(uint64_t pchr) {
    static uint64_t done = 0, undone = 0;
    static double hideT = 0;
    static int pins = 0;
    const uint64_t cap = g_fs0CapChr.load();
    if (!pchr || !cap) return;
    if (g_fhUntilEnd.load() && g_fhEnd.active && g_fhEnd.player != pchr) {
        Log("FormHideUntilEnd abandon: native player changed; no unhide on replacement player");
        g_fhEnd.active = false;
        undone = done;
        return;
    }
    auto call = [&](uint8_t h) {
        static const uint8_t want[16] = {0x48, 0x89, 0x5C, 0x24, 0x10, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x80, 0xB9, 0xD4, 0x09, 0x00, 0x00};
        uint8_t b[16] = {};
        if (!Copy(b, g_base + 0xA550E8, 16) || memcmp(b, want, 16) != 0) { Log("FormHide: A550E8 bytes differ - skipped"); return false; }
        uint8_t before = 0xFF, after = 0xFF;
        Rd(pchr + 0xB440, before);
        bool ok = true;
        __try {
            ((void (*)(uint64_t, uint8_t))(g_base + 0xA550E8))(pchr, h);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            ok = false;
        }
        Rd(pchr + 0xB440, after);
        Log("FormHide A550E8(player %llX, %u) ok=%d [chr+B440] %u->%u seal chr %llX t=%.3f", (unsigned long long)pchr, h, ok, before, after,
            (unsigned long long)cap, Now());
        return ok;
    };
    if (done != cap) {  // pc #55665: hide on the capture frame (was +0.5 s, the throw showed)
        done = cap;
        hideT = Now();
        pins = 0;
        if (call(1)) {
            FormHideEndBegin(pchr, cap);
            FormHideEndReason(pchr, cap, hideT);
        }
    }
    if (done == cap && undone != cap) {
        if (const char* reason = FormHideEndReason(pchr, cap, hideT)) {
            const bool ok = call(0);
            if (ok || !g_fhUntilEnd.load()) { undone = cap; g_fhEnd.active = false; }
            Log("FormHide: unhide (%s) ok=%d, %d seal pins", reason, ok, pins);
        } else if (g_fhMode.load() >= 2) {
            if (FormPin(pchr, cap)) ++pins;
        }
    }
}
// pc #55633 [Spirit] SealLife=1: why the seal chr despawns ~5 s after capture. (a) 2BFC90 (actor registry unlink, rdx = actor) logs every
// actor removed 0..40 s after a capture with a deep return chain, marking the captured / spawn seal chrs; (b) snapshots the captured seal chr
// [0,C000) and its [+3A0] ext [0,2000) at capture +0.3/+1.3/+2.3/+3.3/+4.3 s and logs 4-byte fields that move like a clock (float +-0.5..1.5/s,
// steady; or int +20..200/s). SealKeepOff=<hex>[,<hex>..] (offsets into the seal chr, 'e' prefix = ext) + SealKeepSec: hold those floats at their
// capture +0.5 s value until capture + SealKeepSec (default 25).
std::atomic<int> g_slMode{0};
uint32_t g_slKeepOff[8] = {};
bool g_slKeepExt[8] = {};
int g_slKeepN = 0;
double g_slKeepSec = 25;
double g_slScanAt = 0.3;  // SealLifeScanAtMs: first clock-scan sample after capture
std::atomic<int> g_slLoop{0};
uint32_t g_slLoopMotion = 0;
double g_slLoopAt = 4.0, g_slLoopEvery = 4.0;
void HkSealUnlink(uint64_t, uint64_t actor, uint64_t, uint64_t, const uint64_t* sp) {
    if (!g_slMode.load() || !actor) return;
    const uint64_t cap = g_fs0CapChr.load();
    const double t = Now(), dt = t - g_fs0CapT;
    if (!cap || dt < 0 || dt > 40) return;
    const bool isCap = actor == cap, isSeal = actor == g_fkSealChr.load() || actor == g_fkAppSeal.load();
    static std::atomic<int> n{0};
    if (!isCap && !isSeal && n.load() >= 30) return;
    if (n.fetch_add(1) >= 60) return;
    uint32_t key = 0;
    Rd(actor, key);
    char via[600] = "";
    size_t vl = 0;
    int found = 0;
    for (int k = 0; k < 400 && found < 14 && vl + 40 < sizeof(via); ++k) {
        const uint64_t v = StackArg(sp, k);
        if (!LooksLikeRet(v)) continue;
        vl += (size_t)snprintf(via + vl, sizeof(via) - vl, " +%llX(fn %X)", (unsigned long long)(v - g_base), FnStart(v));
        ++found;
    }
    Log("SealLife UNLINK actor=%llX key>>4=%X cap=%d seal=%d dtCap=%.3f via%s t=%.3f", (unsigned long long)actor, key >> 4, isCap, isSeal, dt,
        vl ? via : " -", t);
}
// SealDefer=1 (pc #55633): the seal chr leaves through the despawn queue: 681044(mgr+BD0, actor) (callers: wrapper 681E9C at 681EB6, and
// E462E1), drained by 5BC9A0 -> 2BF7E8 unlink. Both E8 sites are retargeted; while capture < SealKeepSec a request for the captured seal chr is
// held (not queued) and re-issued by SealLifeTick at the deadline or on a new capture. Other actors pass straight through.
std::atomic<int> g_sdMode{0};
uint64_t g_seoForce = 0;  // SealEndOnIdle: this seal's own despawn request passes SealDefer
void (*g_sdOrig)(uint64_t, uint64_t) = nullptr;
uint64_t g_sdHeld = 0, g_sdList = 0;
int g_sdSkips = 0;
double g_sdHeldT = 0;
void HkDespawnEnq(uint64_t list, uint64_t actor) {
    const uint64_t cap = g_fs0CapChr.load();
    if (actor && actor == cap) {
        const double dt = Now() - g_fs0CapT;
        if (g_sdMode.load() && dt >= 0 && dt < g_slKeepSec && actor != g_seoForce) {
            if (!g_sdHeld) {
                g_sdHeldT = Now();
                Log("SealDefer HOLD despawn request seal=%llX list=%llX caller=+%llX dtCap=%.3f", (unsigned long long)actor, (unsigned long long)list,
                    (unsigned long long)((uint64_t)_ReturnAddress() - g_base), dt);
            }
            g_sdHeld = actor;
            g_sdList = list;
            ++g_sdSkips;
            return;
        }
        Log("SealDefer pass despawn request seal=%llX caller=+%llX dtCap=%.3f", (unsigned long long)actor, (unsigned long long)((uint64_t)_ReturnAddress() - g_base), dt);
    }
    g_sdOrig(list, actor);
}
void SealDeferRelease(const char* why) {
    if (!g_sdHeld || !g_sdOrig) return;
    const uint64_t a = g_sdHeld, l = g_sdList;
    g_sdHeld = 0;
    int ok = 1;
    __try { g_sdOrig(l, a); } __except (EXCEPTION_EXECUTE_HANDLER) { ok = 0; }
    Log("SealDefer RELEASE (%s) seal=%llX queued ok=%d held %.1f s, %d requests swallowed", why, (unsigned long long)a, ok, Now() - g_sdHeldT, g_sdSkips);
    g_sdSkips = 0;
}
// Queue the seal's despawn through 681E9C (the game's own wrapper: [4745348]+BD0 -> 681044, natural caller +681EBB), byte-checked.
// Returns 1 ok, 0 fault, -1 bytes not found (off). A SealDefer hold lets this request through (g_seoForce).
int SealQueueDespawn(uint64_t cap) {
    static int bytesOk = -1;
    if (bytesOk < 0) {
        static const uint8_t want[] = {0x48, 0x83, 0xEC, 0x28, 0x48, 0x8B, 0xD1, 0x48, 0x8B, 0x0D};
        uint8_t b[sizeof(want)] = {};
        bytesOk = Copy(b, g_base + 0x681E9C, sizeof(b)) && !memcmp(b, want, sizeof(want));
        if (!bytesOk) Log("SealEnd: 681E9C bytes not found in this update - off");
    }
    if (!bytesOk) return -1;
    int ok = 1;
    g_seoForce = cap;
    __try { ((void (*)(uint64_t))(g_base + 0x681E9C))(cap); } __except (EXCEPTION_EXECUTE_HANDLER) { ok = 0; }
    g_seoForce = 0;
    return ok;
}
// SealEndOnIdle=1 (pc #56896, ally trace #56896): the captured seal yokai plays its attack action(s) and, for some cores, then idles until the
// game's own despawn request. Ibaraki 5129B: action C53 for 4.87 s, then 0x1 (idle) 0.84 s, then despawn. A2ACB: C53 -> C56 -> C57 (multi-part)
// and despawn with no idle. Action id = PlayerActId(seal) ([[[[chr+3B0]+8]+E8]]). On the first attack -> idle (0x1) edge we queue the despawn
// ourselves through 681E9C (the game's wrapper: [4745348]+BD0 -> 681044, same path as the natural caller +681EBB), so FormHide=4 ends on
// seal-despawn. Never on a hit: only when the action itself becomes idle. A held SealDefer request is released instead.
std::atomic<int> g_seoMode{0};
void SealEndOnIdleTick() {
    if (!g_seoMode.load()) return;
    static uint64_t lastCap = 0;
    static bool seenAtk = false, fired = false;
    static uint32_t lastId = 0;
    const uint64_t cap = g_fs0CapChr.load();
    if (cap != lastCap) { lastCap = cap; seenAtk = false; fired = false; lastId = 0; }
    if (!cap || fired) return;
    const uint32_t id = PlayerActId(cap);
    if (id == 0xFFFF || id == 0xFFFFFFFF || id == 0) return;
    if (id != 1) { seenAtk = true; lastId = id; return; }
    if (!seenAtk) return;
    fired = true;
    const double dt = Now() - g_fs0CapT;
    if (g_sdHeld == cap) {
        Log("SealEndOnIdle seal=%llX action 0x%X -> idle 0x1 dtCap=%.3f: releasing held SealDefer request", (unsigned long long)cap, lastId, dt);
        SealDeferRelease("idle");
        return;
    }
    const int ok = SealQueueDespawn(cap);
    if (ok < 0) return;
    Log("SealEndOnIdle seal=%llX action 0x%X -> idle 0x1 dtCap=%.3f: despawn queued via 681E9C ok=%d t=%.3f", (unsigned long long)cap, lastId, dt, ok, Now());
}
// SealEndOnHit=1 (pc #57014, human #57010): end the form right after the seal's LAST hit window closes, not at the idle edge: the recovery
// pose sits inside the attack action (Ibaraki C53 = 4.9 s, its last hit closes at 2.57 s). Hit windows = attack collision objects in the
// pool [45C6F38]+0x20+i*8 (0xFA0 slots, obj+8 = owner actor; freed slot = window end, Training FRAMEPROBE) owned by the seal or by an actor
// spawned through 65A8A8 after it (Ibaraki throws a helper EE49E that owns the first burst). A "burst" = the pool going from none to some of
// those colliders. The gaps between bursts (0.7 s Ibaraki, 1.2 s A2ACB) are as long as the recovery, so a quiet-time rule cannot tell them
// apart; instead each core has a burst count: [SealHits] <chrId hex>=N (seeded 5129B=2, A2ACB=2, ally trace #57014). When burst N closes we
// queue the despawn (681E9C). A core with no count is learned: the bursts of a form that ended by itself are written to [SealHits], so its
// next form ends on the hit; until then SealEndOnIdle stays the fallback.
std::atomic<int> g_sehMode{0};
uint64_t g_sehKid[8] = {};
double g_sehKidT[8] = {};
void SealEndOnHitNoteSpawn(uint64_t chr) {  // 65A8A8 spawn path (FormKeySealNote), any chr near the player
    if (!g_sehMode.load() || !chr) return;
    static int n = 0;
    for (int i = 0; i < 8; ++i) if (g_sehKid[i] == chr) return;
    g_sehKid[n % 8] = chr; g_sehKidT[n % 8] = Now(); ++n;
}
int SealHitsFor(uint32_t id) {
    wchar_t k[16]; swprintf_s(k, L"%X", id);
    const int n = (int)GetPrivateProfileIntW(L"SealHits", k, 0, IniPath());
    return n ? n : id == 0x5129B ? 2 : id == 0xA2ACB ? 2 : 0;  // built-in seeds (ally trace #57014) when the ini has no [SealHits] line
}
// pc #57269 (human #57262): [Spirit] SealExitFx=1 - exit transition instead of the sped-up tail: at the last-hit close the dark appear
// effects the seal yokai shows when it first spawns (Ibaraki 5129B spawn hit: 63056 63047 6304A 6304D 63050 63053 6307E 63027, from its motion
// timeline 2C72D0 -> 2C6CE0 -> EffectCreate 4CE90C) are spawned on the player through the same wrapper 2C6CE0 the way the Ki pulse 844313 does
// it (P = 0x58 block from 48E97C: +0 bone id, +10 offset, +20 rotation, +30 scale; rcx = effect manager [45B9E38], [rsp+20] = owner chr),
// then after SealExitFxMs the seal is despawned (681E9C) under it and the player restored inside. SealExitFxId = comma list (hex),
// SealExitFxBone = attach bone id (default 0), SealExitFxOwner 0 = player, 1 = seal. SealSpeed still applies; the tail speedup is skipped.
std::atomic<int> g_sxfOn{0};
uint32_t g_sxfIds[12] = {};
int g_sxfN = 0, g_sxfBone = 0, g_sxfOwner = 0, g_sxfMs = 150;
void SealExitFxLoad() {
    g_sxfOn.store(IniInt(L"Spirit", L"SealExitFx", 0));
    wchar_t b[256] = {};
    GetPrivateProfileStringW(L"Spirit", L"SealExitFxId", L"63056,63047,6304A,6304D,63050,63053,6307E,63027", b, 256, IniPath());
    g_sxfN = 0;
    for (wchar_t* q = b; *q && g_sxfN < 12;) {
        while (*q == L',' || *q == L' ') ++q;
        if (!*q) break;
        wchar_t* e = nullptr;
        const unsigned long v = wcstoul(q, &e, 16);
        if (e == q) break;
        if (v) g_sxfIds[g_sxfN++] = (uint32_t)v;
        q = e;
    }
    g_sxfBone = IniInt(L"Spirit", L"SealExitFxBone", 0);
    g_sxfOwner = IniInt(L"Spirit", L"SealExitFxOwner", 0);
    g_sxfMs = IniInt(L"Spirit", L"SealExitFxMs", 150);
}
int SealExitFxSpawn(uint64_t owner) {  // game thread; returns effects created
    static int bytesOk = -1;
    if (bytesOk < 0) {
        static const uint8_t w1[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x57, 0x48, 0x83, 0xEC, 0x40};
        static const uint8_t w2[] = {0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x83, 0x21, 0x00, 0x48, 0x8B};
        uint8_t b1[sizeof(w1)] = {}, b2[sizeof(w2)] = {};
        bytesOk = Copy(b1, g_base + 0x2C6CE0, sizeof(b1)) && !memcmp(b1, w1, sizeof(w1)) && Copy(b2, g_base + 0x48E97C, sizeof(b2)) && !memcmp(b2, w2, sizeof(w2));
        if (!bytesOk) Log("SealExitFx: 2C6CE0/48E97C bytes not found in this update - off");
    }
    if (!bytesOk || !owner) return 0;
    uint64_t mgr = 0;
    if (!Rd(g_base + 0x45B9E38, mgr) || !mgr) return 0;
    int made = 0;
    for (int i = 0; i < g_sxfN; ++i) {
        alignas(16) uint8_t P[0x60] = {};
        alignas(16) uint8_t out[0x20] = {};
        __try {
            ((void (*)(void*))(g_base + 0x48E97C))(P);
            *(int32_t*)P = g_sxfBone;
            ((void* (*)(uint64_t, void*, uint32_t, void*, uint64_t, uint64_t))(g_base + 0x2C6CE0))(mgr, out, g_sxfIds[i], P, owner, 0);
            ++made;
        } __except (EXCEPTION_EXECUTE_HANDLER) { Log("SealExitFx: effect %X faulted", g_sxfIds[i]); }
    }
    return made;
}
void SealEndOnHitTick() {
    if (!g_sehMode.load()) return;
    static uint64_t sxfSeal = 0;
    static double sxfAt = 0;
    // Session = one FormHide=4 form (from the press, before the seal exists): A2ACB is bound from its spawn hit only ~0.3 s after it spawns and
    // its first burst can already be over by then (ally #57014 run 3), so bursts are counted from the press for the seal and every actor
    // spawned (65A8A8) since, and the bound seal only picks the core's burst count.
    static double sess = 0;
    static uint64_t seal = 0;
    static int bursts = 0, want = 0;
    static bool open = false, fired = false;
    static uint32_t sid = 0;
    static std::vector<uint64_t> pool;
    const bool on = g_fh4.owner != 0;  // not g_fh4.started: it is reset when the seal gets captured
    if (on != (sess > 0)) {
        if (seal && sid && !fired && bursts > 0) {  // form over without our end: learn this core's burst count
            const int had = SealHitsFor(sid);
            if (bursts > had) {
                wchar_t k[16], v[16]; swprintf_s(k, L"%X", sid); swprintf_s(v, L"%d", bursts);
                WritePrivateProfileStringW(L"SealHits", k, v, IniPath());
                Log("SealEndOnHit learn core %X: %d hit bursts (was %d) -> [SealHits]", sid, bursts, had);
            }
        }
        if (const uint64_t ts = g_tailSeal.exchange(0)) Log("SealTailSpeed: form over, seal %llX back to normal speed t=%.3f", (unsigned long long)ts, Now());
        sess = on ? Now() : 0; seal = 0; sid = 0; want = 0; bursts = 0; open = false; fired = false; sxfSeal = 0;
    }
    if (sxfSeal && Now() >= sxfAt) {
        const uint64_t sx = sxfSeal; sxfSeal = 0;
        const int ok = SealQueueDespawn(sx);
        Log("SealExitFx: seal %llX despawn queued under the exit fx ok=%d t=%.3f", (unsigned long long)sx, ok, Now());
    }
    if (!sess || fired) return;
    const double now = Now();
    if (!seal && g_fh4.seal) {
        seal = g_fh4.seal;
        uint32_t k = 0; Rd(seal, k); sid = k >> 4; want = SealHitsFor(sid);
        Log("SealEndOnHit seal=%llX core %X bound %.3f s after the press, %d bursts seen so far: %d hit bursts known%s", (unsigned long long)seal, sid, now - sess,
            bursts, want, want ? " - ends when the last one closes" : " - learning (idle fallback)");
    }
    uint64_t pb = 0;
    if (!Rd(g_base + 0x45C6F38, pb) || !pb) return;
    pool.resize(0xFA0);
    if (!Copy(pool.data(), pb + 0x20, 0xFA0 * 8)) return;
    int n = 0;
    for (int i = 0; i < 0xFA0; ++i) {
        const uint64_t o = pool[i];
        if (!o) continue;
        uint64_t own = 0;
        if (!Rd(o + 8, own) || !own || own == g_fh4.owner) continue;
        bool mine = seal && own == seal;
        for (int k = 0; !mine && k < 8; ++k) mine = g_sehKid[k] == own && g_sehKidT[k] >= sess - 0.05;
        if (mine) ++n;
    }
    if (n && !open) { open = true; ++bursts; Log("SealEndOnHit core %X burst %d opens (%d colliders) %.3f s after the press", sid, bursts, n, now - sess); }
    bool fire = false;
    if (!n && open) {
        open = false;
        Log("SealEndOnHit core %X burst %d closes %.3f s after the press (want %d)", sid, bursts, now - sess, want);
        fire = seal && want && bursts >= want;
    } else if (!n && !open && seal && want && bursts >= want) fire = true;  // every burst was over before the seal got bound
    if (!fire) return;
    fired = true;
    if (g_sdHeld == seal) { Log("SealEndOnHit: releasing held SealDefer request"); SealDeferRelease("last-hit"); return; }
    if (g_sxfOn.load() > 0 && g_sxfN > 0) {  // pc #57269: shadow exit instead of the tail
        const uint64_t ow = g_sxfOwner == 1 ? seal : FkPlayer();
        const int made = SealExitFxSpawn(ow);
        sxfSeal = seal; sxfAt = now + g_sxfMs / 1000.0;
        Log("SealEndOnHit seal=%llX core %X last hit burst %d closed %.3f s after the press: exit fx %d/%d on %s %llX bone %d, despawn in %d ms t=%.3f",
            (unsigned long long)seal, sid, bursts, now - sess, made, g_sxfN, g_sxfOwner == 1 ? "seal" : "player", (unsigned long long)ow, g_sxfBone, g_sxfMs, now);
        return;
    }
    if (g_tailMul.load() > 1.f) {  // pc #57162: no early cut - the rest of the animation plays at x speed until the seal's natural end
        g_tailT0.store(now);
        g_tailSeal.store(seal);
        Log("SealEndOnHit seal=%llX core %X last hit burst %d closed %.3f s after the press: tail speedup x%.2f (ramp from x%.2f over %.0f ms) until its natural end t=%.3f",
            (unsigned long long)seal, sid, bursts, now - sess, g_tailMul.load(), g_sealMul.load() > 1.f ? g_sealMul.load() : 1.f, g_tailRampMs.load(), now);
        return;
    }
    const int ok = SealQueueDespawn(seal);
    Log("SealEndOnHit seal=%llX core %X last hit burst %d closed %.3f s after the press: despawn queued via 681E9C ok=%d t=%.3f", (unsigned long long)seal, sid,
        bursts, now - sess, ok, now);
}
struct SlSnap { std::vector<uint8_t> b; std::vector<uint8_t> ok; };
void SlRead(SlSnap& s, uint64_t base, size_t n) {
    s.b.assign(n, 0);
    s.ok.assign(n / 0x1000, 0);
    for (size_t p = 0; p < n / 0x1000; ++p) s.ok[p] = base && Copy(s.b.data() + p * 0x1000, base + p * 0x1000, 0x1000);
}
void SlAnalyse(const char* nm, const SlSnap* s, int ns, const double* ts) {
    int hits = 0;
    const size_t n = s[0].b.size();
    for (size_t o = 0; o + 4 <= n && hits < 60; o += 4) {
        bool okAll = true;
        for (int i = 0; i < ns; ++i) okAll = okAll && s[i].ok[o / 0x1000];
        if (!okAll) continue;
        float f[8]; int32_t v[8];
        for (int i = 0; i < ns; ++i) { memcpy(&f[i], &s[i].b[o], 4); memcpy(&v[i], &s[i].b[o], 4); }
        bool fl = true, in = true;
        double rmin = 1e9, rmax = -1e9, imin = 1e18, imax = -1e18;
        for (int i = 1; i < ns; ++i) {
            const double dtt = ts[i] - ts[i - 1];
            if (!std::isfinite(f[i]) || !std::isfinite(f[i - 1]) || fabsf(f[i]) > 1e6f) fl = false;
            else { const double r = (f[i] - f[i - 1]) / dtt; rmin = std::min(rmin, r); rmax = std::max(rmax, r); }
            const double ri = (double)(v[i] - v[i - 1]) / dtt; imin = std::min(imin, ri); imax = std::max(imax, ri);
        }
        fl = fl && ((rmin > 0.4 && rmax < 1.6) || (rmax < -0.4 && rmin > -1.6)) && rmax - rmin < 0.3;
        in = !fl && ((imin >= 20 && imax <= 200) || (imax <= -20 && imin >= -200)) && imax - imin < 0.35 * fabs(imax);
        if (!fl && !in) continue;
        ++hits;
        if (fl) Log("SealLife CLOCK %s+%zX float %.3f %.3f %.3f %.3f %.3f rate=%.3f..%.3f/s", nm, o, f[0], f[1], f[2], ns > 3 ? f[3] : 0.f, ns > 4 ? f[4] : 0.f, rmin, rmax);
        else Log("SealLife CLOCK %s+%zX int %d %d %d %d %d rate=%.0f..%.0f/s", nm, o, v[0], v[1], v[2], ns > 3 ? v[3] : 0, ns > 4 ? v[4] : 0, imin, imax);
    }
    Log("SealLife scan %s done: %d clock-like fields", nm, hits);
}
void SealLifeTick() {
    SealEndOnHitTick();
    SealEndOnIdleTick();
    if (g_sdHeld && (g_sdHeld != g_fs0CapChr.load() || Now() - g_fs0CapT >= g_slKeepSec)) SealDeferRelease(g_sdHeld != g_fs0CapChr.load() ? "capture-change" : "deadline");
    if (!g_slMode.load() && !g_slKeepN && !g_slLoop.load()) return;
    static uint64_t cap0 = 0;
    static double capT0 = -1;
    static int ns = 0;
    static SlSnap chr[5], ext[5];
    static double ts[5];
    static uint64_t extP = 0;
    static float keep[8];
    static bool kept = false;
    const uint64_t cap = g_fs0CapChr.load();
    const double capT = g_fs0CapT, now = Now();
    if (cap != cap0 || capT != capT0) { cap0 = cap; capT0 = capT; ns = 0; extP = 0; kept = false; }
    if (!cap) return;
    const double dt = now - capT;
    if (g_slMode.load() && ns < 5 && dt >= g_slScanAt + ns) {
        if (ns == 0) Rd(cap + 0x3A0, extP);
        if (FormHideSealPresent(g_fhEnd.actorGlobal ? g_fhEnd.actorGlobal : 0, cap) == 0) { Log("SealLife: seal gone before sample %d", ns); ns = 5; return; }
        SlRead(chr[ns], cap, 0xC000);
        SlRead(ext[ns], extP, 0x2000);
        ts[ns] = now;
        if (++ns == 5) { Log("SealLife: 5 samples seal=%llX ext=%llX t0=%.3f", (unsigned long long)cap, (unsigned long long)extP, ts[0]); SlAnalyse("chr", chr, 5, ts); if (extP) SlAnalyse("ext", ext, 5, ts); }
    }
    // SealLoop=1: the seal chr goes when its summon motion ends (run 1: no clock field; unlink via the 5BC9A0 queue at +5.9 s). Re-issue
    // its motion (SealLoopMotion, default the spawn motion) with the game's 66C4A8(chr, id, 0, 0.0f) at capture + SealLoopAt, then every
    // SealLoopEvery, while it is still registered and capture < SealKeepSec.
    if (g_slLoop.load() && dt >= g_slLoopAt && dt < g_slKeepSec) {
        static double next = 0;
        static uint64_t loopCap = 0;
        if (loopCap != cap) { loopCap = cap; next = 0; }
        if (now >= next) {
            next = now + g_slLoopEvery;
            const uint32_t mv = g_slLoopMotion ? g_slLoopMotion : g_fkSealMotion.load();
            const int present = FormHideSealPresent(g_fhEnd.actorGlobal, cap);
            int rc = -1;
            if (mv && present != 0) rc = TenshinMoveCall(cap, mv);
            Log("SealLoop 66C4A8(seal %llX, %X) present=%d rc=%d dt=%.2f", (unsigned long long)cap, mv, present, rc, dt);
        }
    }
    if (g_slKeepN && dt >= 0.5 && dt < g_slKeepSec) {
        uint64_t e = 0;
        Rd(cap + 0x3A0, e);
        if (!kept) {
            for (int i = 0; i < g_slKeepN; ++i) { const uint64_t a = (g_slKeepExt[i] ? e : cap) + g_slKeepOff[i]; keep[i] = 0; Rd(a, keep[i]); Log("SealKeep hold %s+%X = %.3f", g_slKeepExt[i] ? "ext" : "chr", g_slKeepOff[i], keep[i]); }
            kept = true;
        }
        static double nextLog = 0;
        const bool lg = now >= nextLog;
        if (lg) nextLog = now + 2;
        for (int i = 0; i < g_slKeepN; ++i) {
            const uint64_t a = (g_slKeepExt[i] ? e : cap) + g_slKeepOff[i];
            float cur = 0;
            if (!Rd(a, cur)) continue;
            if (lg) Log("SealKeep %s+%X cur=%.3f -> %.3f dt=%.1f", g_slKeepExt[i] ? "ext" : "chr", g_slKeepOff[i], cur, keep[i], dt);
            WrBytes(a, &keep[i], 4);
        }
    }
}
void FkSlot0Tick(uint64_t pchr) {  // game thread
    SealLifeTick();
    FormHideDirectTick(pchr);  // also restores a mode-3 hide after mode/capture changes
    FormHide4Tick(pchr);
    if (const int mode = g_fhMode.load(); mode && mode != 3 && mode != 4) FormHideTick(pchr);  // pc #55327: native modes unchanged
    static uint64_t kicked = 0;
    const uint64_t cap = g_fs0CapChr.load();
    if (g_fs0Active.load() && cap && kicked != cap && g_fs0Mode.load() != 9 && Now() - g_fs0CapT >= 0.3) {
        kicked = cap;
        if (g_fzsMode.load()) {  // ally/pc #55150 [Spirit] FormZeroSlot=1: zero the player kind-0 slot +2C0..+2F7 (902C00 copies it into obj +190.. / +2A4..)
            const uint64_t ps = g_fs0FirstK0.load();
            char hb[0x38 * 3 + 4];
            HexSafe(hb, sizeof(hb), ps + 0x2C0, 0x38);
            static const uint8_t z[0x38] = {};
            const bool ok = ps && WrBytes(ps + 0x2C0, z, sizeof(z));
            Log("FormZeroSlot player slot %llX +2C0..+2F7 zeroed ok=%d (was %s)", (unsigned long long)ps, ok, hb);
        }
        Fs0Reload(pchr, "player after FormSlot0 apply");
        SwArm(g_fs0FirstK0.load(), "after-apply");
        SpArm();
    }
    {   // ally #55149 [Spirit] FormObjFix=1: 0.6 s after the capture (the player kind-0 slot has committed its own 40244A obj by then), copy the
        // seal obj's fields that differ onto the player obj: +004 dword (P=1 S=0), +190..+1AF (slot +2C0/+2D0 transform block, P 24.8/20.1/16.1 S 0),
        // +2A4 (slot +2F0), +2AC dword. 2 = also +370..+397 (P 30.0/-4.0/2, S 0/+4.0/-1). Logs before/after.
        static uint64_t fx = 0;
        if (g_foxMode.load() && g_fs0Active.load() && cap && fx != cap && Now() - g_fs0CapT >= 0.6) {
            fx = cap;
            uint64_t po = 0, so = 0;
            const uint64_t ps = g_fs0FirstK0.load(), ss = g_spSealSlot.load();
            if (ps) Rd(ps + 0x10, po);
            if (ss) Rd(ss + 0x10, so);
            uint32_t pid = 0, sid = 0;
            Rd(po, pid);
            Rd(so, sid);
            if (!po || !so || (pid >> 4) != (sid >> 4)) {
                Log("FormObjFix skipped: player obj=%llX id %X, seal obj=%llX id %X (need both, same id)", (unsigned long long)po, pid >> 4, (unsigned long long)so, sid >> 4);
            } else {
                struct { uint32_t off, len; } rg[] = {{0x4, 4}, {0x190, 0x20}, {0x2A4, 4}, {0x2AC, 4}, {0x370, 0x28}};
                const int n = g_foxMode.load() >= 2 ? 5 : 4;
                char hb[0x28 * 3 + 4], ha[0x28 * 3 + 4];
                for (int i = 0; i < n; ++i) {
                    uint8_t buf[0x28];
                    HexSafe(hb, sizeof(hb), po + rg[i].off, rg[i].len);
                    const bool ok = Copy(buf, so + rg[i].off, rg[i].len) && WrBytes(po + rg[i].off, buf, rg[i].len);
                    HexSafe(ha, sizeof(ha), po + rg[i].off, rg[i].len);
                    Log("FormObjFix player obj %llX +%03X ok=%d before %s after %s", (unsigned long long)po, rg[i].off, ok, hb, ha);
                }
            }
        }
    }
    {   // pc #54948 FormApply: 0.1 s and 1.0 s after the kick (kick = capture + 0.3 s)
        static uint64_t fa1 = 0, fa2 = 0;
        const double dt = Now() - g_fs0CapT;
        if (g_faMode.load() && g_fs0Active.load() && cap && kicked == cap) {
            if (fa1 != cap && dt >= 0.4) { fa1 = cap; FaApply(pchr, 1); }
            if (fa2 != cap && dt >= 1.3) { fa2 = cap; FaApply(pchr, 2); }
        }
        if (g_fcMode.load() && g_fs0Active.load() && cap && kicked == cap) FcTick(pchr, dt);  // pc #55034 FormCommit
        if (g_fatMode.load() && g_fs0Active.load() && cap && kicked == cap) FatTick(pchr, dt);  // pc #55111 FormAttach
    }
    const bool timeout = g_fs0Active.load() && Now() - g_fs0CapT > 60.0 && !FormHideEndHolding(pchr, cap);
    if (g_fs0Restore.exchange(0) || timeout) {
        if (g_fs0Active.exchange(0)) {
            g_fs0DoneChr.store(cap);
            // pc #56639: mode 9 is capture only (nothing swapped), so nothing to reload; the 60 s reload wrote -1 into 64 cached slots after a
            // shrine/area reload had freed them (crash at title: pointer 27A'FFFFFFFF).
            if (g_fs0Mode.load() != 9 && pchr == g_fs0SlotChr) Fs0Reload(pchr, timeout ? "player after FormSlot0 restore (60 s)" : "player after FormSlot0 restore");
            else Log("FormSlot0 restore%s: capture cleared, no slot reload (mode %d) t=%.3f", timeout ? " (60 s)" : "", g_fs0Mode.load(), Now());
        }
        g_fs0Id.store(0);
        g_fs0CapChr.store(0);
        kicked = 0;
    }
}
uint64_t FkPack(uint64_t key) {
    uint64_t m = 0;
    if (!Rd(g_base + 0x45B9E60, m) || !m) return 0;
    __try {
        return ((uint64_t(*)(uint64_t, uint64_t))(g_base + 0x133440))(m, key);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}
uint64_t FkModel(uint64_t key, int& fault) {
    fault = 0;
    __try {
        return ((uint64_t(*)(uint64_t, uint32_t))(g_base + 0x1175400))(key, 3);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        fault = 1;
        return 0;
    }
}
uint64_t FkGameKey(uint32_t id) {  // 0x134314(-, id) = 0x300000000 + id with the game's optional remap (SetForm 74C675)
    __try {
        return ((uint64_t(*)(uint64_t, uint32_t))(g_base + 0x134314))(0, id);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}
uint64_t FkRec(uint32_t id) {
    uint64_t ctx = 0, db = 0;
    if (!Rd(g_base + 0x45B9E30, ctx) || !ctx || !Rd(ctx + 0x3F0, db) || !db) return 0;
    __try {
        return ((uint64_t(*)(uint64_t, uint32_t))(g_base + 0x3748E0))(db, id);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}
uint64_t FkPlayer() {
    uint64_t pt = 0, pl = 0;
    return G_PTAB && Rd(G_PTAB, pt) && pt && Rd(pt + 0xCD * 0x18, pl) ? pl : 0;
}
// pc #55219 [Spirit] EquipDiff=1, log only: which player structure changes when one armor piece is equipped. A side thread snapshots every
// 250 ms: player chr 0..0x10000 (incl. the chr+9F0 part slots and the part-46 slot at ~chr+9F90), appearance blk [chr+560] 0..0x840, and the
// first 0x400 of every object pointer found at slot+10 for the 64 slots after chr+A10 (stride 0x340). Trigger = blk dword +814 goes 0 -> !0 or
// the record blk+C..+53C changes. before = snapshot 0.5 s before the trigger, after = 2 s after it; noise = offsets that also differ between two
// pre-trigger snapshots (0.75 s / 0.5 s before). Logs every non-noise differing qword (cap 500 lines per trigger, 3 triggers).
struct EdSnap { uint64_t chr, blk; double t; uint8_t c[0x10000]; uint8_t b[0x840]; uint64_t obj[64]; uint8_t o[64][0x400]; };
static EdSnap* g_edRing = nullptr;  // 4 entries
std::atomic<int> g_edMode{0};
static void EdTake(EdSnap& s, uint64_t chr) {
    s.chr = chr;
    s.t = Now();
    s.blk = 0;
    Rd(chr + 0x560, s.blk);
    if (!Copy(s.c, chr, sizeof(s.c))) memset(s.c, 0, sizeof(s.c));
    if (!s.blk || !Copy(s.b, s.blk, sizeof(s.b))) memset(s.b, 0, sizeof(s.b));
    for (int i = 0; i < 64; ++i) {
        s.obj[i] = 0;
        Rd(chr + 0xA10 + (uint64_t)i * 0x340 + 0x10, s.obj[i]);
        if (!s.obj[i] || !Copy(s.o[i], s.obj[i], 0x400)) memset(s.o[i], 0, 0x400);
    }
}
static uint32_t EdRecHash(const EdSnap& s) {
    uint32_t h = 2166136261u;
    for (int i = 0xC; i < 0x53C; ++i) h = (h ^ s.b[i]) * 16777619u;
    return h;
}
static int EdDiff(const char* tag, const uint8_t* a, const uint8_t* b, const uint8_t* n0, const uint8_t* n1, size_t len, uint64_t base, int budget) {
    int lines = 0, noisy = 0, diffs = 0;
    for (size_t off = 0; off + 8 <= len; off += 8) {
        if (!memcmp(a + off, b + off, 8)) continue;
        if (memcmp(n0 + off, n1 + off, 8)) { ++noisy; continue; }
        ++diffs;
        if (lines < budget) {
            uint64_t x, y;
            memcpy(&x, a + off, 8);
            memcpy(&y, b + off, 8);
            Log("EquipDiff %s +%05zX (%llX): %016llX -> %016llX", tag, off, (unsigned long long)(base + off), (unsigned long long)x, (unsigned long long)y);
            ++lines;
        }
    }
    Log("EquipDiff %s: %d changed qwords (%d logged), %d noisy skipped", tag, diffs, lines, noisy);
    return lines;
}
DWORD WINAPI EdThread(LPVOID) {
    g_edRing = (EdSnap*)VirtualAlloc(nullptr, sizeof(EdSnap) * 6, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!g_edRing) { Log("EquipDiff: no memory"); return 0; }
    EdSnap* after = g_edRing + 4;
    EdSnap* tmp = g_edRing + 5;
    int n = 0, triggers = 0;
    uint32_t lastDirty = 0, lastHash = 0;
    bool haveHash = false;
    Log("EquipDiff: armed (snapshot every 250 ms, trigger = blk+814 0->!0 or record change)");
    while (triggers < 3) {
        Sleep(250);
        const uint64_t chr = FkPlayer();
        if (!chr) { n = 0; haveHash = false; continue; }
        EdSnap& cur = g_edRing[n % 4];
        EdTake(cur, chr);
        ++n;
        uint32_t dirty = 0;
        memcpy(&dirty, cur.b + 0x814, 4);
        const uint32_t h = EdRecHash(cur);
        // ally #55240 run: removing the chest piece fired neither trigger (the player wears a fixed appearance), so also trigger on any chr+A10
        // slot obj change, or by hand: the file EquipDiff.go next to the ASI (deleted when seen; 'touch' it right before the change)
        static uint64_t lastObjSum = 0;
        uint64_t objSum = 0;
        for (int i = 0; i < 64; ++i) objSum = objSum * 31 + cur.obj[i];
        wchar_t gof[MAX_PATH];
        swprintf_s(gof, L"%sEquipDiff.go", ModDir());
        const bool manual = GetFileAttributesW(gof) != INVALID_FILE_ATTRIBUTES && DeleteFileW(gof);
        const bool slotTrig = n > 4 && lastObjSum && objSum != lastObjSum;
        lastObjSum = objSum;
        if (manual) Log("EquipDiff: manual trigger file seen");
        const bool trig = n > 4 && (manual || slotTrig || (cur.blk && ((dirty && !lastDirty) || (haveHash && h != lastHash))));
        const uint32_t ld = lastDirty, lh = lastHash;
        lastDirty = dirty;
        lastHash = h;
        haveHash = cur.blk != 0;
        if (!trig) continue;
        ++triggers;
        // ring: cur = n-1, before = n-3 (0.5 s), noise pair = n-4 (0.75 s) vs n-3
        EdSnap& before = manual ? cur : g_edRing[(n - 3) % 4];
        EdSnap& old = manual ? g_edRing[(n - 2) % 4] : g_edRing[(n - 4) % 4];
        Log("EquipDiff TRIGGER #%d%s chr=%llX blk=%llX +814 %08X -> %08X rec hash %08X -> %08X t=%.3f (before t=%.3f)", triggers,
            manual ? " (manual, after = +12 s)" : slotTrig ? " (slot obj change)" : "", (unsigned long long)chr,
            (unsigned long long)cur.blk, ld, dirty, lh, h, cur.t, before.t);
        Sleep(manual ? 12000 : 2000);  // manual: 12 s to make the change in the menu and close it
        EdTake(*after, chr);
        if (after->chr != before.chr) { Log("EquipDiff: player chr changed, skipped"); continue; }
        int budget = 500;
        budget -= EdDiff("CHR", before.c, after->c, old.c, before.c, sizeof(before.c), chr, budget);
        budget -= EdDiff("BLK", before.b, after->b, old.b, before.b, sizeof(before.b), before.blk, budget > 0 ? budget : 0);
        for (int i = 0; i < 64 && budget > 0; ++i) {
            if (before.obj[i] != after->obj[i]) {
                Log("EquipDiff SLOT %d (+%X) obj %llX -> %llX", i, 0xA10 + i * 0x340, (unsigned long long)before.obj[i], (unsigned long long)after->obj[i]);
                --budget;
            } else if (before.obj[i]) {
                char tg[32];
                snprintf(tg, sizeof(tg), "SLOT%d.OBJ", i);
                budget -= EdDiff(tg, before.o[i], after->o[i], old.o[i], before.o[i], 0x400, before.obj[i], budget);
            }
        }
        (void)tmp;
        n = 0;  // rebuild the baseline ring
    }
    Log("EquipDiff: done (%d triggers)", triggers);
    return 0;
}
void EdStart() {
    if (!g_base) g_base = g_img.base;
    if (!G_PTAB) G_PTAB = Site("g:PlayerTab");
    HANDLE t = CreateThread(nullptr, 0, &EdThread, nullptr, 0, nullptr);
    if (t) CloseHandle(t);
}
struct FkChr {
    uint64_t chr, M, m8, m10, key, gkey, model, pack;
    uint32_t w0, idx;
    int loaded, fault;
};
void FkRead(uint64_t chr, FkChr& o) {
    memset(&o, 0, sizeof(o));
    o.chr = chr;
    o.loaded = -1;
    if (!chr) return;
    Rd(chr, o.w0);
    o.key = 0x300000000ull + (o.w0 >> 4);
    o.gkey = FkGameKey(o.w0 >> 4);
    if (Rd(chr + 0x50, o.M) && o.M) {
        Rd(o.M + 8, o.m8);
        Rd(o.M + 0x10, o.m10);
        Rd(o.M + 0xA0, o.idx);
    }
    o.pack = FkPack(o.key);
    uint8_t l = 0;
    if (o.pack && Rd(o.pack + 0x2C8, l)) o.loaded = l;
    o.model = FkModel(o.key, o.fault);
}
void FkLogChr(const char* who, const FkChr& c) {
    Log("FormKey %s chr=%llX [chr]=%X id=[chr]>>4=%X key=%llX gamekey(134314)=%llX pack=%llX loaded=%d model=1175400(key,3)=%llX fault=%d M=[chr+50]=%llX [M+8]=%llX [M+10]=%llX idxA0=%u match8=%d match10=%d",
        who, (unsigned long long)c.chr, c.w0, c.w0 >> 4, (unsigned long long)c.key, (unsigned long long)c.gkey, (unsigned long long)c.pack, c.loaded,
        (unsigned long long)c.model, c.fault, (unsigned long long)c.M, (unsigned long long)c.m8, (unsigned long long)c.m10, c.idx & 0xFF,
        c.model && c.model == c.m8, c.model && c.model == c.m10);
}
// 2916C5C entry (rcx = spirit mgr): creator inputs [mgr+0x10] and rec+0x38 charId (logged when mgr/id changes)
void HkFormKeySpawn(uint64_t a, uint64_t, uint64_t, uint64_t, const uint64_t*) {
    if (!g_fkOn.load() || a < 0x10000) return;
    uint32_t id = 0xFFFFFFFF;
    Rd(a + 0x10, id);
    const bool chg = g_fkMgr.exchange(a) != a;
    if (g_fkMgrId.exchange(id) == id && !chg) return;
    const uint64_t rec = id != 0xFFFFFFFF ? FkRec(id) : 0;
    uint32_t charId = 0;
    if (rec) Rd(rec + 0x38, charId);
    g_fkLogged.store(0);  // a new summon: log its chr again once it is loaded
    Log("FormKey creator 2916C5C mgr=%llX [mgr+10]=%u(0x%X) rec=%llX charId[rec+38]=%u(0x%X) t=%.3f", (unsigned long long)a, id, id,
        (unsigned long long)rec, charId, charId, Now());
}
int FkSetFormCall(uint64_t st, uint32_t x, int& ret) {
    __try {
        ret = ((uint8_t(*)(uint64_t, uint32_t))(g_base + 0x74C2FC))(st, x);
        return 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 1;
    }
}
void FkSwap(uint64_t st, uint64_t key) {
    uint64_t pchr = 0, M = 0, m10a = 0, m10b = 0, d8a = 0, d8b = 0;
    uint32_t a68 = 0, a68b = 0;
    Rd(st + 0xE0, pchr);
    if (pchr && Rd(pchr + 0x50, M) && M) Rd(M + 0x10, m10a);
    Rd(st + 0xA68, a68);
    Rd(st + 0x1D8, d8a);
    uint32_t x = g_fkX.load();
    if (!x) x = a68 ? a68 : 0x6E73;
    if (x == a68) WrU32(st + 0xA68, 0);  // 74C608 returns early when id == [state+A68]
    g_fkHits = 0;
    g_fkCell = key;
    Log("FormKey swap called 74C2FC(state=%llX, X=%X) key=%llX A68 was %X chr=[state+E0]=%llX [[chr+50]+10]=%llX state+1D8=%llX", (unsigned long long)st, x,
        (unsigned long long)key, a68, (unsigned long long)pchr, (unsigned long long)m10a, (unsigned long long)d8a);
    int ret = -1;
    const int fault = FkSetFormCall(st, x, ret);
    const uint64_t left = g_fkCell;
    g_fkCell = 0;  // disarm (the stub clears it itself when it fires)
    if (M) Rd(M + 0x10, m10b);
    Rd(st + 0xA68, a68b);
    Rd(st + 0x1D8, d8b);
    if (fault && x == a68) WrU32(st + 0xA68, a68);
    Log("FormKey swap ret al=%d fault=%d overrideHits=%ld cellLeft=%llX A68 %X -> %X [[chr+50]+10] %llX -> %llX state+1D8 %llX -> %llX", ret & 0xFF, fault, g_fkHits,
        (unsigned long long)left, a68, a68b, (unsigned long long)m10a, (unsigned long long)m10b, (unsigned long long)d8a, (unsigned long long)d8b);
}
// pc #56639: the player chr changed (title, area load): forget everything bound to the old one. Never call the game with the old actors.
void FormPlayerChange(uint64_t pchr) {
    static uint64_t last = 0;
    if (pchr == last) return;
    const uint64_t old = last;
    last = pchr;
    if (!old) return;
    const uint64_t held = g_sdHeld;
    g_sdHeld = 0; g_sdSkips = 0;
    g_fs0Active.store(0); g_fs0CapChr.store(0); g_fs0Id.store(0); g_fs0Restore.store(0);
    Log("FormReset player chr %llX -> %llX: dropped held seal %llX, FormSlot0 capture cleared, FormHide=4 owner %llX (ends as player-change)",
        (unsigned long long)old, (unsigned long long)pchr, (unsigned long long)held, (unsigned long long)g_fh4.owner);
}
// pc #56639: the human stayed invincible after a FormHide=4 form. The player's i-frame flag is ctrl+EA0 (Training frame meter, pc #35618;
// dodge sets it ~0.34 s). [Spirit] FormInvulnFix=1 (default): for 10 s after the form ends, an EA0 that stays set > 0.6 s is cleared.
// Also logs EA0 during/after the form and a diff of ctrl 0..0x1000 / chr 0..0xC000 (before the form vs. 1 s and 2 s after, stable values only).
std::atomic<int> g_fiFix{1};
struct FiSnap { uint8_t c[0x1000]; uint8_t p[0xC000]; bool ok; };
static FiSnap* g_fiSnap = nullptr;  // [0] before, [1] end+1 s, [2] end+2 s
static void FiTake(FiSnap& s, uint64_t ctrl, uint64_t chr) { s.ok = Copy(s.c, ctrl, sizeof(s.c)) && Copy(s.p, chr, sizeof(s.p)); }
static void FiDiff(const char* nm, const uint8_t* b, const uint8_t* a1, const uint8_t* a2, size_t n, int& lines) {
    for (size_t o = 0; o + 4 <= n && lines < 120; o += 4) {
        uint32_t vb, v1, v2;
        memcpy(&vb, b + o, 4); memcpy(&v1, a1 + o, 4); memcpy(&v2, a2 + o, 4);
        if (v1 != v2 || v1 == vb) continue;
        ++lines;
        Log("FormInvuln diff %s+%zX before %08X after %08X", nm, o, vb, v1);
    }
}
void FormInvulnTick(uint64_t ctrl, uint64_t pchr) {
    static bool was = false;
    static double endT = -1, stuck = -1, nextLog = 0;
    static int snaps = 0;
    const double now = Now();
    uint8_t e = 0;
    Rd(ctrl + 0xEA0, e);
    if (g_fh4.owner && g_fh4.owner == pchr) {
        if (!was) {
            was = true;
            if (!g_fiSnap) g_fiSnap = new (std::nothrow) FiSnap[3];
            if (g_fiSnap) FiTake(g_fiSnap[0], ctrl, pchr);
            snaps = 1;
            Log("FormInvuln form start ctrl=%llX EA0=%u act=0x%X", (unsigned long long)ctrl, e, PlayerActId(pchr));
        } else if (now >= nextLog) {
            Log("FormInvuln during form EA0=%u act=0x%X", e, PlayerActId(pchr));
        }
        if (now >= nextLog) nextLog = now + 2;
        return;
    }
    if (was) { was = false; endT = now; stuck = -1; nextLog = 0; Log("FormInvuln form end EA0=%u act=0x%X", e, PlayerActId(pchr)); }
    if (endT < 0) return;
    const double dt = now - endT;
    if (dt > 10) { endT = -1; return; }
    if (g_fiSnap && snaps == 1 && dt >= 1.0) { FiTake(g_fiSnap[1], ctrl, pchr); snaps = 2; }
    if (g_fiSnap && snaps == 2 && dt >= 2.0) {
        FiTake(g_fiSnap[2], ctrl, pchr); snaps = 3;
        int lines = 0;
        if (g_fiSnap[0].ok && g_fiSnap[1].ok && g_fiSnap[2].ok) {
            FiDiff("ctrl", g_fiSnap[0].c, g_fiSnap[1].c, g_fiSnap[2].c, sizeof(g_fiSnap[0].c), lines);
            FiDiff("chr", g_fiSnap[0].p, g_fiSnap[1].p, g_fiSnap[2].p, sizeof(g_fiSnap[0].p), lines);
        }
        Log("FormInvuln diff done: %d stable changed dwords (snap ok %d%d%d)", lines, g_fiSnap[0].ok, g_fiSnap[1].ok, g_fiSnap[2].ok);
    }
    if (now >= nextLog) { nextLog = now + 1; Log("FormInvuln after form +%.1f s EA0=%u act=0x%X", dt, e, PlayerActId(pchr)); }
    if (!e) { stuck = -1; return; }
    if (stuck < 0) { stuck = now; return; }
    if (now - stuck > 0.6 && g_fiFix.load()) {
        const uint8_t z = 0;
        const bool ok = WrBytes(ctrl + 0xEA0, &z, 1);
        Log("FormInvuln: ctrl+EA0 stuck at %u for %.2f s after the form - cleared ok=%d act=0x%X", e, now - stuck, ok, PlayerActId(pchr));
        stuck = -1;
    }
}
// 2E6C34 entry (rcx = input state; game thread): spirit chr check + the pending swap
void HkFormKeyInputBody(uint64_t a) {
    uint64_t pchr = 0;
    Rd(a + 0xE0, pchr);
    const uint64_t pl = FkPlayer();
    if (!pchr || !pl || pchr != pl) return;  // pc #56639: only the live player (playerChr=0 at title/load ran this for every char, every frame)
    FormPlayerChange(pchr);
    FkAppearTick(pchr);
    FkSlot0Tick(pchr);
    FormInvulnTick(a, pchr);
    if (g_fkState.exchange(a) != a) Log("FormKey player state=%llX chr=[state+E0]=%llX playerChr=%llX", (unsigned long long)a, (unsigned long long)pchr, (unsigned long long)pl);
    const uint64_t mgr = g_fkMgr.load();
    uint64_t inst = 0, schr = 0, sst = 0;
    if (mgr && Rd(mgr + 0x128, inst) && inst) {
        Rd(inst + 0x30, schr);
        Rd(inst, sst);
    }
    bool seal = false;
    uint32_t sealDesc = 0;
    if (!(schr && sst == 2)) {  // no guardian spirit: the seal's 65A8A8 spawn
        if (const uint64_t c = g_fkSealChr.load()) {
            schr = c;
            sst = 2;
            seal = true;
            sealDesc = g_fkSealDesc.load();
        }
    }
    if (schr && sst == 2 && g_fkLogged.exchange(schr) != schr) {
        FkChr s, p;
        FkRead(schr, s);
        FkRead(pchr, p);
        uint32_t id = 0xFFFFFFFF, charId = 0;
        if (!seal) Rd(mgr + 0x10, id);
        const uint64_t rec = id != 0xFFFFFFFF ? FkRec(id) : 0;
        if (rec) Rd(rec + 0x38, charId);
        if (seal)
            Log("FormKey seal chr loaded (683F74/65A8A8 path) chr=%llX desc[0]=%X motion=%X t=%.3f", (unsigned long long)schr, sealDesc, g_fkSealMotion.load(), Now());
        else
            Log("FormKey summon loaded mgr=%llX inst=%llX state=%llu [mgr+10]=%u(0x%X) rec=%llX charId[rec+38]=%u(0x%X) t=%.3f", (unsigned long long)mgr,
                (unsigned long long)inst, (unsigned long long)sst, id, id, (unsigned long long)rec, charId, charId, Now());
        FkLogChr("spirit", s);
        FkLogChr("player", p);
        FkAppearLog(seal ? "seal" : "spirit", schr);
        FkAppearLog("player", pchr);
        if (seal && !g_fkAppSeal.load() && g_ppOn.load()) {  // pc2 #4045 run: the title screen used all 4000 lines; fresh budget at the first seal chr
            Log("PartProbe: budget reset at first seal chr (so far F3D78 %ld, 2C4450 %ld calls)", g_ppF3Hits, g_ppC4Hits);
            InterlockedExchange(&g_ppLines, 0);
        }
        if (seal && g_fkAppearMode.load()) FkAppearApply(pchr, schr);
        // ally #54204: the seal figures shared the player's model pointer. If the chr key is the player's own, use desc[0] as the body id.
        uint64_t dkey = 0, dpack = 0;
        uint8_t dl = 0;
        if (seal && sealDesc) {
            dkey = FkGameKey(sealDesc);
            if (!dkey) dkey = 0x300000000ull + sealDesc;
            dpack = FkPack(dkey);
            if (dpack) Rd(dpack + 0x2C8, dl);
            int f = 0;
            const uint64_t dm = FkModel(dkey, f);
            Log("FormKey seal desc key=%llX pack=%llX loaded=%u model=1175400(key,3)=%llX fault=%d sameAsPlayerKey=%d", (unsigned long long)dkey,
                (unsigned long long)dpack, dl, (unsigned long long)dm, f, (s.gkey ? s.gkey : s.key) == (p.gkey ? p.gkey : p.key));
        }
        if (seal && g_fkModelMode.load() && g_fkSwap.load()) {
            if (s.m10 && s.m10 != p.m8 && s.m10 != p.m10) {
                g_fkSwap.store(0);
                g_fkMHits = 0;
                g_fkMVal = s.m10;
                g_fkMState = a;
                g_fkMArmT = Now();
                g_fkMCell = s.m10;
                g_fkMSchr.store(schr);
                g_fkPartHits = 0;
                g_fkPartSwaps = 0;
                Log("FormKey PART armed FormPart=%d: seal chr=%llX sealBody[[schr+E8]+248]=%X F26C4(seal)=%X F26C4(player)=%X", g_fkPartMode.load(),
                    (unsigned long long)schr, FkSealBody(schr), FkBodyId(schr), FkBodyId(pchr));
                g_tenReq.store(1);  // the game's own LA transform request on the next 2E6C34 call
                Log("FormKey MODEL armed: 74C79D returns seal [[chr+50]+10]=%llX once (player M+8=%llX M+10=%llX); LA request 0x4E queued",
                    (unsigned long long)s.m10, (unsigned long long)p.m8, (unsigned long long)p.m10);
            } else {
                g_fkLogged.store(0);  // look at the next seal spawn (the 2nd figure)
                Log("FormKey MODEL: seal chr has no own slot-1 model (M+10=%llX) - waiting for the next spawn", (unsigned long long)s.m10);
            }
        } else if (!g_fkAppearMode.load() && g_fkSwap.exchange(0)) {  // FormAppear replaces the key swap
            uint64_t key = s.gkey ? s.gkey : s.key;
            if (dkey && (key == (p.gkey ? p.gkey : p.key) || !s.w0)) key = dkey;
            g_fkPendKey.store(key);
            g_fkPendT = Now();
            g_fkPendPolls = 0;
            Log("FormKey swap armed key=%llX: waiting for its pack (+2C8), timeout 5 s", (unsigned long long)key);
        }
    }
    if (g_fkMVal) {  // model mode. ally #54340: the LA transform does not pass 74C79D; it switches the player's active slot ([M+A0] 0 -> 2) to
        // the LA model. So while armed, put the seal yokai's model into the ACTIVE slot [M+8+idx*8] (idx != 0) and keep it there; restore at idx 0.
        static uint64_t wSlot = 0, wOld = 0;
        static uint8_t lastIx = 0xFF;
        static double lastRe = 0;
        uint64_t M = 0;
        uint8_t ix = 0;
        if (Rd(pchr + 0x50, M) && M) Rd(M + 0xA0, ix);
        const uint64_t slot = M && ix ? M + 8 + 8ull * ix : 0;
        uint64_t cur = 0;
        if (slot) Rd(slot, cur);
        static double ixOnT = 0;
        static bool rebuilt = false;
        if (!ix) ixOnT = 0;
        else if (!ixOnT) { ixOnT = Now(); rebuilt = false; }
        if (ix && !rebuilt && g_fkPartMode.load() >= 2 && g_fkPartSwaps == 0 && ixOnT && Now() - ixOnT > 0.3) {
            rebuilt = true;
            const LONG h0 = g_fkPartHits;
            const bool ok = FkRebuildParts(pchr);
            Log("FormKey PART forced rebuild F3478(chr+9F0,0) ok=%d part47 loads %ld -> %ld swaps=%ld t+%.2f s", ok, h0, g_fkPartHits, g_fkPartSwaps,
                Now() - g_fkMArmT);
        }
        if (ix != lastIx) {
            uint32_t a68 = 0;
            Rd(a + 0xA68, a68);
            Log("FormKey MODEL player idxA0 %u -> %u slot=%llX cur=%llX A68=%X t+%.2f s", lastIx, ix, (unsigned long long)slot, (unsigned long long)cur, a68,
                Now() - g_fkMArmT);
            lastIx = ix;
        }
        if (slot && cur != g_fkMVal) {
            if (wSlot != slot) { wSlot = slot; wOld = cur; }
            const bool ok = WrBytes(slot, (const void*)&g_fkMVal, 8);
            if (Now() - lastRe > 1.0) {
                lastRe = Now();
                Log("FormKey MODEL WRITE active slot idx=%u [%llX] %llX -> %llX ok=%d t+%.2f s", ix, (unsigned long long)slot, (unsigned long long)cur,
                    (unsigned long long)g_fkMVal, ok, Now() - g_fkMArmT);
            }
        }
        const bool ended = wSlot && !ix;
        if (ended || Now() - g_fkMArmT > 90.0) {
            if (wSlot && wOld) {
                uint64_t v = 0;
                if (Rd(wSlot, v) && v == g_fkMVal) WrBytes(wSlot, &wOld, 8);
            }
            Log("FormKey MODEL %s: restored [%llX] <- %llX, hits74C79D=%ld part47Loads=%ld part47Swaps=%ld", ended ? "LA ended" : "TIMEOUT 90 s",
                (unsigned long long)wSlot, (unsigned long long)wOld, g_fkMHits, g_fkPartHits, g_fkPartSwaps);
            g_fkMSchr.store(0);
            if (g_fs0Active.load()) g_fs0Restore.store(1);
            wSlot = wOld = 0;
            lastIx = 0xFF;
            g_fkMCell = 0;
            g_fkMVal = 0;
        }
    }
    if (const uint64_t key = g_fkPendKey.load()) {
        const uint64_t pack = FkPack(key);
        uint8_t l = 0;
        if (pack) Rd(pack + 0x2C8, l);
        ++g_fkPendPolls;
        if (l) {
            g_fkPendKey.store(0);
            Log("FormKey pack ready key=%llX pack=%llX after %d polls %.2f s", (unsigned long long)key, (unsigned long long)pack, g_fkPendPolls, Now() - g_fkPendT);
            FkSwap(a, key);
        } else if (Now() - g_fkPendT > 5.0) {
            g_fkPendKey.store(0);
            Log("FormKey swap TIMEOUT key=%llX pack=%llX loaded=%u after %d polls: not called", (unsigned long long)key, (unsigned long long)pack, l, g_fkPendPolls);
        }
    }
}
void HkFormKeyInput(uint64_t a, uint64_t, uint64_t, uint64_t, const uint64_t*) {
    if (!g_fkOn.load()) return;
    __try {
        HkFormKeyInputBody(a);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_fkCell = 0;
        g_fkPendKey.store(0);
        Log("FormKey: exception in the input hook (swap disarmed; the game continues)");
    }
}

}  // namespace

void TenshinReq() { g_tenReq.store(1); }  // next 0x2E6C34 call (game thread): fill gauge + fc1a20(state+0x1060, 0x4E, 1)
void TenshinForm(uint32_t v) { g_tenForm.store(v); }
void TenshinPreload(uint32_t id, bool request, char* out, size_t n) {
    static const uint32_t kForms[] = {0x1909, 0x5FFF, 0x6E73, 0x5455, 0x5095, 0x1C17, 0x2D3F, 0x72B1, 0x5E1B, 0x27C, 0xD2F, 0x17D6, 0x4E6, 0x2552, 0x1302, 0x33C9};
    size_t o = 0;
    auto add = [&](const char* f, auto... a) { if (o < n) o += (size_t)snprintf(out + o, n - o, f, a...); };
    for (uint32_t f : kForms) {
        if (id && f != id) continue;
        __try {
            const uint32_t key = ((uint32_t(*)(uint32_t))(g_base + 0x11793F0))(f);
            if (request) ((void (*)(uint32_t, uint32_t))(g_base + 0x1A8B990))(key, 0);
            void* tbl = *(void**)(g_base + 0x45B9E60);
            uint8_t* e = ((uint8_t * (*)(void*, uint64_t)) (g_base + 0x133440))(tbl, 0x600000000ull + key);
            add(" %X:key=%X,E=%p,2C8=%d,m20=%llX,a18=%llX,m28=%llX", f, key, (void*)e, e ? (int)e[0x2C8] : -1, e ? *(unsigned long long*)(e + 0x20) : 0ull, e ? *(unsigned long long*)(e + 0x18) : 0ull, e ? *(unsigned long long*)(e + 0x28) : 0ull);
        } __except (EXCEPTION_EXECUTE_HANDLER) { add(" %X:(fault)", f); }
    }
    Log("TenshinPreload(req=%d): %s", request ? 1 : 0, out);
}
// ally #47612: load a form's type-6 body pack the way the game does: 3F9C10(mgr=*(base+45B9E60), key=0x600000000+idx, ctx=29667D8()), then [133440(mgr,key)+2C8] != 0
void TenshinLoad(uint32_t form, char* out, size_t n) {
    static const struct { uint32_t f, idx; } kIdx[] = {{0x5455, 0x99}, {0x72B1, 0x10F}, {0x5E1B, 0x10}, {0x5FFF, 0x5D}, {0x6E73, 0x19F}, {0x1909, 0x184}, {0x17D6, 0x100}, {0x1C17, 0x131},
                                                      {0x2552, 0x139}, {0x2D3F, 0x1EA}, {0x33C9, 0xFD}, {0x4E6, 0x110}, {0x27C, 0x158}, {0xD2F, 0xC5}, {0x1302, 0xD}, {0x5095, 0x154}};
    size_t o = 0;
    auto add = [&](const char* f, auto... a) { if (o < n) o += (size_t)snprintf(out + o, n - o, f, a...); };
    for (auto& k : kIdx) {
        if (form && k.f != form) continue;
        __try {
            void* mgr = *(void**)(g_base + 0x45B9E60);
            const uint64_t key = 0x600000000ull + k.idx;
            void* ctx = ((void* (*)())(g_base + 0x29667D8))();
            ((void (*)(void*, uint64_t, void*))(g_base + 0x3F9C10))(mgr, key, ctx);
            uint8_t* e = ((uint8_t * (*)(void*, uint64_t)) (g_base + 0x133440))(mgr, key);
            add(" %X(idx %X):E=%s,2C8=%d", k.f, k.idx, e ? "y" : "n", e ? (int)e[0x2C8] : -1);
        } __except (EXCEPTION_EXECUTE_HANDLER) { add(" %X:(fault)", k.f); }
    }
    Log("TenshinLoad: %s", out);
}
void TenshinSlots(uint64_t ext, char* out, size_t n) {
    size_t o = 0;
    auto add = [&](const char* f, auto... a) { if (o < n) o += (size_t)snprintf(out + o, n - o, f, a...); };
    __try {
        uint8_t* tblp = (uint8_t*)(g_base + 0x45B3E30);  // [rip+3E6DBB5] at 74C274
        uint8_t* tbl = *(uint8_t**)(*(uint8_t**)tblp + 0x68);
        add("setidx=%d;", *(int32_t*)(ext + 0x590 + 0x1EE0));
        for (int s = 0; s < 0x11; ++s) {
            uint16_t* w = ((uint16_t * (*)(uint64_t, int)) (g_base + 0xF65D8))(ext + 0x590, s);
            const uint32_t id = w ? *w : 0;
            uint8_t* d = id ? ((uint8_t * (*)(void*, uint64_t)) (g_base + 0xF4DBC))(tbl, id) : nullptr;
            add(" s%X=%X/%X", s, id, d ? *(uint32_t*)(d + 0x58) : 0xFFFFFFFFu);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { add(" (fault)"); }
    Log("TenshinSlots: %s", out);
}
void TenshinBody(int m) { g_tenBody.store(m); g_tenBodyTick = 0; }
void TenshinSetForm(uint32_t v) { g_tenSetForm.store(v); }
void TenshinMove(uint32_t v) { g_tenMove.store(v); }
void TenshinGo() { g_tenGo.store(24); }  // ~6 calls per state: every input state gets the bit once (the first one is not the player's)
void FormKeySealDesc(uint32_t id) { g_fkSealDesc.store(id); }
void FormKeySealNote(uint64_t chr, uint32_t motion, float dist) {
    if (dist >= 0 && dist < 30.f) SealEndOnHitNoteSpawn(chr);
    // ally #57462: the DOWN core (A2ACB) spawns ONE chr and the game reuses the same chr address on the next use, so a plain
    // 'same chr' skip dropped every DOWN after the first (press-no-seal, human saw the yokai thrown). Skip only a repeat note of the same spawn.
    if (!g_fkOn.load() || !chr || dist < 0 || dist >= 15.f || (g_fkSealChr.load() == chr && Now() - g_fkSealT < 1.0)) return;
    g_fkSealMotion.store(motion);
    g_fkSealT = Now();
    g_fkSealChr.store(chr);
    Log("FormKey seal spawn hit (65A8A8 -> 66C4A8 from 65ADA6) chr=%llX desc[0]=%X motion=%X dist=%.1f swapArmed=%d", (unsigned long long)chr,
        g_fkSealDesc.load(), motion, dist, g_fkSwap.load());
}
bool FormKeyInstall() {
    static std::atomic<int> done{0};
    static bool ok = false;
    if (done.exchange(1)) return ok;
    if (!g_base) g_base = g_img.base;
    G_PTAB = Site("g:PlayerTab");
    OnEntry("ScProbe_GSpiritWork", &HkFormKeySpawn);
    OnEntry("ScProbe_TenshinInput", &HkFormKeyInput);
    if (g_slMode.load()) OnEntry("SealLife_2BFC90", &HkSealUnlink);
    if (g_sdMode.load()) {
        for (uint32_t rva : {0x681EB6u, 0xE462E1u}) {
            void* o = nullptr;
            const char* w = nullptr;
            uint8_t b[5] = {};
            int32_t rel = 0;
            if (!Copy(b, g_base + rva, 5) || b[0] != 0xE8 || (memcpy(&rel, b + 1, 4), g_base + rva + 5 + rel) != g_base + 0x681044) {
                Log("SealDefer: call %X does not target 681044 - skipped", rva);
                continue;
            }
            if (InstallCallRetargetAt((uint8_t*)(g_base + rva), "call:DespawnEnqueue", (void*)&HkDespawnEnq, &o, &w)) {
                g_sdOrig = (void (*)(uint64_t, uint64_t))o;
                Log("SealDefer: call %X -> hook", rva);
            } else Log("SealDefer: call %X retarget failed: %s", rva, w ? w : "?");
        }
    }
    const char* why = nullptr;
    const bool grp = InstallGroup("SC", &why);  // installs only the SC hooks registered so far (InvLog=0: just these two)
    if (grp) Log("FormKey: group SC installed (2916C5C spawn + 2E6C34 input, independent of InvLog)");
    else Log("FormKey: group SC OFF - %s", why ? why : "?");
    int n = 0;
    static const uint32_t kCalls[] = {0x74C698, 0x74C438};  // the only two callers of 0x11793D0 (SetForm / its online sibling)
    for (uint32_t rva : kCalls) {
        uint8_t* p = (uint8_t*)(g_base + rva);
        uint8_t b[5] = {};
        int32_t rel = 0;
        if (Copy(b, (uint64_t)p, 5)) memcpy(&rel, b + 1, 4);
        if (b[0] != 0xE8 || (uint64_t)(p + 5 + rel) != g_base + 0x11793D0) {
            Log("FormKey: call +%X is not 'call 11793D0' - key override not installed there", rva);
            continue;
        }
        const char* w = nullptr;
        if (InstallCallKeyOverride(p, "call:FormKey11793D0", &g_fkCell, &g_fkHits, &w)) ++n;
        else Log("FormKey: key override at +%X failed: %s", rva, w ? w : "?");
    }
    Log("FormKey: key override on %d/2 call sites of 0x11793D0", n);
    {   // model mode: SetForm's model getter call (74C79D call 1175400 -> 74C7B1 write)
        uint8_t* p = (uint8_t*)(g_base + 0x74C79D);
        uint8_t b[5] = {};
        int32_t rel = 0;
        if (Copy(b, (uint64_t)p, 5)) memcpy(&rel, b + 1, 4);
        const char* w = nullptr;
        if (b[0] != 0xE8 || (uint64_t)(p + 5 + rel) != g_base + 0x1175400) Log("FormKey: +74C79D is not 'call 1175400' - model override not installed");
        else if (InstallCallKeyOverride(p, "call:FormKeyModel1175400", &g_fkMCell, &g_fkMHits, &w)) Log("FormKey: model override on +74C79D (FormModel=%d)", g_fkModelMode.load());
        else Log("FormKey: model override at +74C79D failed: %s", w ? w : "?");
    }
    {   // pc #54384: part-47 loader call in F3478 (+F38AE call F3D78)
        uint8_t* p = (uint8_t*)(g_base + 0xF38AE);
        uint8_t b[5] = {};
        int32_t rel = 0;
        if (Copy(b, (uint64_t)p, 5)) memcpy(&rel, b + 1, 4);
        const char* w = nullptr;
        if (b[0] != 0xE8 || (uint64_t)(p + 5 + rel) != g_base + 0xF3D78) Log("FormKey: +F38AE is not 'call F3D78' - part-47 hook not installed");
        else if (InstallCallRetargetAt(p, "call:FormPart47F3D78", (void*)&HkPart47, &g_fkPartOrig, &w)) Log("FormKey: part-47 hook on +F38AE (FormPart=%d)", g_fkPartMode.load());
        else Log("FormKey: part-47 hook at +F38AE failed: %s", w ? w : "?");
    }
    if (g_ppOn.load() || g_fs0Mode.load() || g_swMode.load()) PartProbeInstall();
    if (g_fcMode.load()) FormCommitInstall();
    if (g_fatMode.load()) FormAttachInstall();
    FormHide4Install();
    FastThrowInstall();
    OrbSkipInstall();
    ok = grp && n > 0;
    return ok;
}
void FormKeyCmd(const char* line, char* reply, size_t n) {
    const char* p = strstr(line, "formkey");
    p = p ? p + 7 : "";
    while (*p == ' ') ++p;
    if (!strncmp(p, "on", 2)) {
        const uint32_t x = (uint32_t)strtoul(p + 2, nullptr, 16);
        g_fkX.store(x);
        const bool ok = FormKeyInstall();
        g_fkOn.store(1);
        g_fkSwap.store(1);
        g_fkLogged.store(0);
        Log("FormKey on (pipe): swap armed once, X=%X%s, hooks %s", x, x ? "" : " (auto)", ok ? "ok" : "FAILED");
    } else if (!strncmp(p, "appear", 6)) {
        const char* q = p + 6;
        while (*q == ' ') ++q;
        if (!strncmp(q, "off", 3)) {
            g_fkAppearMode.store(0);
            g_fkAppRestore.store(1);
        } else if (!strncmp(q, "on", 2)) {
            g_fkAppearMode.store(1);
            g_fkAppSeal.store(0);  // re-arm: the next first seal chr is used
            g_fkLogged.store(0);
        } else if (!strncmp(q, "kick", 4)) {
            g_fkAppKick.store(atoi(q + 4));
        }
        Log("FormKey appear (pipe): FormAppear=%d kick=%d restorePending=%d blk=%llX old=%X new=%X", g_fkAppearMode.load(), g_fkAppKick.load(), g_fkAppRestore.load(),
            (unsigned long long)g_fkAppBlk, g_fkAppOld, g_fkAppNew);
    } else if (!strncmp(p, "slot0", 5)) {
        const char* q = p + 5;
        while (*q == ' ') ++q;
        if (!strncmp(q, "off", 3)) g_fs0Restore.store(1);
        else {
            g_fs0Mode.store(atoi(q));
            g_fs0DoneChr.store(0);  // re-arm on the current seal chr
            if (g_fs0Mode.load()) PartProbeInstall();
        }
        Log("FormSlot0 (pipe): mode=%d active=%d id=%X restorePending=%d", g_fs0Mode.load(), g_fs0Active.load(), g_fs0Id.load(), g_fs0Restore.load());
    } else if (!strncmp(p, "fhmode", 6)) {  // pc #56435: live FormHide mode for a same-launch FormHide=0 vs 4 A/B
        g_fhMode.store(atoi(p + 6));
        FormHide4Install();
        Log("FormHide mode (pipe) = %d", g_fhMode.load());
    } else if (!strncmp(p, "follow", 6)) {  // pc #56910: live [Spirit] FormFollowSeal
        g_fhFollow.store(atoi(p + 6));
        Log("FormFollowSeal (pipe) = %d", g_fhFollow.load());
    } else if (!strncmp(p, "fh4mask", 7)) {  // pc #56435: live A/B of the FormHide=4 writes (bits as [Spirit] FH4Mask)
        g_fh4Mask.store((int)strtol(p + 7, nullptr, 0));
        Log("FormHide=4 mask (pipe) = %X", g_fh4Mask.load());
    } else if (!strncmp(p, "off", 3)) {
        g_fs0Restore.store(1);
        g_fkOn.store(0);
        g_fkSwap.store(0);
        g_fkPendKey.store(0);
        g_fkCell = 0;
        Log("FormKey off (pipe)");
    }
    snprintf(reply, n, "ok formkey on=%d swapArmed=%d X=%X mgr=%llX state=%llX pendingKey=%llX lastHits=%ld slot0 mode=%d active=%d id=%X swaps=%ld clears=%ld", g_fkOn.load(), g_fkSwap.load(), g_fkX.load(),
             (unsigned long long)g_fkMgr.load(), (unsigned long long)g_fkState.load(), (unsigned long long)g_fkPendKey.load(), g_fkHits, g_fs0Mode.load(),
             g_fs0Active.load(), g_fs0Id.load(), g_fs0Swaps, g_fs0Clears);
}

void Register(bool boss, bool rm, bool inv) {
    g_base = g_img.base;
    g_scProbes.store(IniInt(L"Spirit", L"Probes", 0));
    g_imgEnd = g_img.end;
    LARGE_INTEGER fq;
    QueryPerformanceFrequency(&fq);
    g_qpcSec = 1.0 / (double)fq.QuadPart;
    G_NET = Site("g:NetMgr");
    G_PM = Site("g:PlayMode");
    G_CH = Site("g:CharMgr");
    G_INV = Site("g:Inventory");
    G_DATA = Site("g:DataDb");
    G_MS = Site("g:MissionMgr");
    G_QUEUE = Site("g:MissionQueue");
    F_CLR0 = Site("MissionClearedGetSlot");
    F_CLR1 = Site("MissionClearedGet_37AFC0");
    F_UNL = Site("MissionUnlockedGet");
    g_logOn = boss || rm;
    if (IniInt(L"Spirit", L"FormKey", 0)) { g_fkOn.store(1); g_fkSwap.store(1); }
    g_fkPartMode.store(IniInt(L"Spirit", L"FormPart", 0));  // pc #54384: part-47 body id swap while FormModel is armed
    g_fkAppearMode.store(IniInt(L"Spirit", L"FormAppear", 0));  // pc #54368: seal body id -> player appearance [blk+4] + reload
    g_fkAppKick.store(IniInt(L"Spirit", L"FormAppearKick", 1));
    g_ppOn.store(IniInt(L"Spirit", L"PartProbe", 0));
    g_spMode.store(IniInt(L"Spirit", L"SlotProbe", 0));  // pc #54959: 3 s state sampler of the player kind-0 slot + 406668 DB lookup
    g_swMode.store(IniInt(L"Spirit", L"SlotWatch", 0));  // pc #54925: HW write watch on the staged player kind-0 slot (+2F8/+40)
    g_fs0Mode.store(IniInt(L"Spirit", L"FormSlot0", 0));  // pc2 #54828: player kind-0 part id -> seal's kind-0 id + one rebuild  // pc #54739: log every F3D78 / 2C4450 part load  // pc #54571: 1 = 846B3C reload flags, 2 = +804=1, 0 = none
    g_faMode.store(IniInt(L"Spirit", L"FormApply", 0));  // pc #54948: 2C4450(obj,0,-1) after the FormSlot0 swap (1 = slot obj, 2 = player chr)
    g_foxMode.store(IniInt(L"Spirit", L"FormObjFix", 0));  // ally #55149: copy the seal obj fields onto the player kind-0 obj after the commit
    g_fhUntilEnd.store(IniInt(L"Spirit", L"FormHideUntilEnd", 0) != 0);
    g_fhSealKeep.store(IniInt(L"Spirit", L"FormSealKeep", 1));
    { wchar_t fb[32]; GetPrivateProfileStringW(L"Spirit", L"FormBlockPad", L"0xFF4F", fb, 32, IniPath()); g_fbMask.store((int)(wcstol(fb, nullptr, 0) & 0xFFFF)); }
    g_fbRT.store(IniInt(L"Spirit", L"FormBlockRT", 1));
    g_fbLS.store(IniInt(L"Spirit", L"FormBlockLS", 1));
    g_fhPress.store(IniInt(L"Spirit", L"FormHidePress", 0));  // pc #55704
    g_fhExtra.store(IniInt(L"Spirit", L"FormHideExtra", 1));
    g_fhActLog.store(IniInt(L"Spirit", L"FormHideActLog", 1));  // pc #55814
    g_fhActLearn.store(IniInt(L"Spirit", L"FormHideActLearn", 1));
    {
        wchar_t ab[128] = {};
        // Ally #55835: 0x28FD preceded both seal captures; action-only press 2 applied the hide at +36 ms.
        // This fixes first-press action detection; the human-body visibility failure is a separate, still-open issue.
        GetPrivateProfileStringW(L"Spirit", L"FormHideSealAct", L"28FD", ab, 128, IniPath());
        int n = 0;
        for (wchar_t* p = ab; *p && n < 4;) {
            while (*p == L',' || *p == L' ') ++p;
            if (!*p) break;
            wchar_t* e = nullptr;
            const unsigned long v = wcstoul(p, &e, 16);
            if (e == p) break;
            if (v) g_fhActIds[n++].store((uint32_t)v);
            p = e;
        }
        g_fhActN.store(n);
    }
    g_fhRotOff.store(IniInt(L"Spirit", L"FormPinRot", 0x120));  // pc #55665: 0 = position-only pin
    g_fhWarp.store(IniInt(L"Spirit", L"FormPinWarp", 1));
    g_fhFollow.store(IniInt(L"Spirit", L"FormFollowSeal", 0));  // pc #56910
    g_fhFollowProbeMs.store(IniInt(L"Spirit", L"FormFollowProbeMs", 600));
    g_fhFollowCm.store(IniInt(L"Spirit", L"FormFollowCm", 400));  // pc #55842: 1 = move via the game's warp setter 669AB8
    g_slMode.store(IniInt(L"Spirit", L"SealLife", 0));  // pc #55633
    {
        wchar_t kb[128] = {};
        GetPrivateProfileStringW(L"Spirit", L"SealKeepOff", L"", kb, 128, IniPath());
        for (wchar_t* p = kb; *p && g_slKeepN < 8;) {
            while (*p == L',' || *p == L' ') ++p;
            if (!*p) break;
            const bool ext = *p == L'e' || *p == L'E';
            if (ext) ++p;
            wchar_t* e = nullptr;
            const unsigned long v = wcstoul(p, &e, 16);
            if (e == p) break;
            g_slKeepExt[g_slKeepN] = ext;
            g_slKeepOff[g_slKeepN++] = (uint32_t)v;
            p = e;
        }
        g_slKeepSec = IniInt(L"Spirit", L"SealKeepSec", 25);
        g_slLoop.store(IniInt(L"Spirit", L"SealLoop", 0));
        g_sdMode.store(IniInt(L"Spirit", L"SealDefer", 0));
    g_fiFix.store(IniInt(L"Spirit", L"FormInvulnFix", 1));  // pc #56639
        g_seoMode.store(IniInt(L"Spirit", L"SealEndOnIdle", 0));
        g_sehMode.store(IniInt(L"Spirit", L"SealEndOnHit", 0));
        SealExitFxLoad();
        g_slScanAt = IniInt(L"Spirit", L"SealLifeScanAtMs", 300) / 1000.0;
        GetPrivateProfileStringW(L"Spirit", L"SealLoopMotion", L"0", kb, 128, IniPath());
        g_slLoopMotion = (uint32_t)wcstoul(kb, nullptr, 16);
        g_slLoopAt = IniInt(L"Spirit", L"SealLoopAtMs", 4000) / 1000.0;
        g_slLoopEvery = IniInt(L"Spirit", L"SealLoopEveryMs", 4000) / 1000.0;
    }
    g_fhMode.store(IniInt(L"Spirit", L"FormHide", 0));
    g_fh4Mask.store(IniInt(L"Spirit", L"FH4Mask", 0x7F));  // 1=native hide, 2=+pin seal pos, 3=guarded direct byte hide/restore without fade, 4=full blend + root multiplier hold
    g_fzsMode.store(IniInt(L"Spirit", L"FormZeroSlot", 0));  // pc #55150: zero player kind-0 slot +2C0..+2F7 before the rebuild
    if (IniInt(L"Spirit", L"EquipDiff", 0)) { g_edMode.store(1); EdStart(); }  // pc #55219: log what changes in the player chr/blk/slot objs on an equip change
    g_fatMode.store(IniInt(L"Spirit", L"FormAttach", 0));  // pc #55111: forced commit + seal request copy + state 1 -> slow path 903088/2C5FBC attach
    g_fcMode.store(IniInt(L"Spirit", L"FormCommit", 0));  // pc #55034: 3FD37C walk log + forced 3FD37C(player,1,0) per frame for ~1 s after the kick
    g_fkModelMode.store(IniInt(L"Spirit", L"FormModel", 0));  // ally #54309: model-only override + real LA transform  // [Spirit] FormKey=1: log + one swap; main installs via FormKeyInstall
    // FIGHT: always (the rematch gates and verify_all read it)
    OnEntry("BossBattleMode", &HkBegin);
    OnEntry("BossBattleModeEnd", &HkEnd);
    OnEntry("Character::Create", &HkCreate);
    if (boss || rm) {
        OnEntry("SetMissionFlag", &HkFlag);
        OnEntry("ShowBossGauge", &HkGauge);
        OnEntry("SetMissionClearFlag", &HkClear);
        static const char* const kRew[] = {"Character::AddDropTable", "Character::AddDropFixItem", "Character::AddDropKeyItemDirectGet",
                                           "Character::ForceDropAmurita", "Character::ForceDropItem", "Character::ForceDropItemAndGet",
                                           "Character::EnemyForceKillWithItemDrop", "Item::AddItem", "Item::AddFixItem", "Item::AddMagicItem",
                                           "Item::AddItemNoNotice", "Item::AddEquipment", "Item::AddEquipmentBySlot", "Item::AddEquipmentPreset",
                                           "Player::AddGuardianSpirit", "Player::AddGuardianSpiritNoNotice", "Player::AddGuardianSpiritWithTimer",
                                           "Player::AddSkill", "Player::AddSkillDojo", "Player::AddMoney", "Mission::SetReward",
                                           "Mission::GetGameClearRewardNormal", "Mission::GetGameClearRewardHard"};
        static const Handler kRewH[] = {&HkRew<0>, &HkRew<1>, &HkRew<2>, &HkRew<3>, &HkRew<4>, &HkRew<5>, &HkRew<6>, &HkRew<7>,
                                        &HkRew<8>, &HkRew<9>, &HkRew<10>, &HkRew<11>, &HkRew<12>, &HkRew<13>, &HkRew<14>, &HkRew<15>,
                                        &HkRew<16>, &HkRew<17>, &HkRew<18>, &HkRew<19>, &HkRew<20>, &HkRew<21>, &HkRew<22>};
        for (int i = 0; i < 23; ++i) OnEntry(kRew[i], kRewH[i]);
        OnEntry("GameClearRewardGrant", &HkGrant);
    }
    if (rm) {
        g_rm = true;
        OnEntry("MissionLoadStep", &HkTick);
        OnEntry("EmakiMissionRequest", &HkEmaki);
        OnEntry("MissionSetupEmaki", &HkSetupEmaki);
        OnEntry("MissionRequestById", &HkById);
        OnEntry("MissionRequestEnqueue", &HkEnqueue);
        OnEntry("MissionStartFromSave", &HkStartSave);
        OnEntry("MissionReplayMenu_Setup", &HkReplaySetup);
        OnEntry("MissionReplayMenuOpen_1DDA258", &HkReplayOpen);
        OnEntry("UI::OpenMissionReplayMenu", &HkReplayNative);
        OnEntry("MissionSelectListBuild", &HkSelectList);
        OnEntry("BattleScrollTop_Select", &HkScrollTop);
        OnEntry("EmakiItemUse_MenuHandler", &HkItemUse);
        OnEntry("MissionReplayMenu_Decide", &HkItemUse2);
        OnEntry("MissionRecordGet", &HkRecordGet);
        OnEntry("MissionClearedGetSlot", &HkCleared0);
        OnEntry("MissionClearedGet_37AFC0", &HkCleared1);
        OnEntry("MissionUnlockedGet", &HkUnlocked);
        OnEntry("EmakiUniqueDefeatFlag_Serialize", &HkUniqueFlag);
        OnEntry("ClearMapSetEntry", &HkClearMap);
        OnEntry("TrophyUnlock_body", &HkTrophy);
        OnEntry("MissionResultUpdate", &HkResult);
        OnEntry("RetryGadget_ResurrectPlayers", &HkResurrect);
        OnEntry("SetDecideRetry_impl", &HkDecideRetry);
        OnEntry("Mission::SetMissionResultClearType", &HkEnd2<0>);
        OnEntry("UI::SetGameOverUIDeadType", &HkEnd2<1>);
        OnEntry("UI::SetGameOverUIFailType", &HkEnd2<2>);
        OnEntry("Player::SetRetryPointIDAndSave", &HkEnd2<3>);
        OnEntry("Player::NoGameOver", &HkEnd2<4>);
        OnEntry("BuildSaveFilePath", &HkSavePath);
    }
    if (inv) {
        OnEntry("InventoryAddItem", &HkInvAdd);
        OnEntry("EffectCreate", &HkSc<0>);
        OnEntry("ScProbe_1320F2C", &HkSc<1>);
        OnEntry("ScProbe_23CBF9C", &HkSc<2>);
        OnEntry("ScProbe_GSpiritReq", &HkSc<3>);
        OnEntry("ScProbe_GSpiritWork", &HkSc<4>);
        OnEntry("ScProbe_AsyncLoad", &HkSc<5>);
        OnEntry("ScProbe_MotionReq", &HkSc<6>);
        // pc #56435: the log-only probes cost FPS (~100 log lines/s); [Spirit] Probes=1 installs them, 0 (default) leaves them out entirely
        if (g_scProbes.load()) OnEntry("ScProbe_TenshinInput", &HkTenshinInput);
        if (g_scProbes.load()) OnEntry("ScProbe_786C84", &HkTenshinReq);
        if (g_scProbes.load()) { void* o = nullptr; const char* w = nullptr; if (InstallCallRetargetAt((uint8_t*)(g_base + 0x74C27F), "call:RefreshFormItemDef", (void*)&HkItemDefForm, &o, &w)) g_itemDefOrig = (void* (*)(void*, uint64_t))o; else Log("TenshinForm: itemdef hook failed: %s", w ? w : "?"); }
        if (g_scProbes.load()) OnEntry("ScProbe_SpiritActive", &HkSpiritActive);
        OnEntry("SealProbe_65A8A8", &HkSeal<0>);  // functional: desc[0] for FormKey; its log part is Probes-gated
        if (g_scProbes.load()) {
            OnEntry("ScProbe_B611BC", &HkCoreProbe<0>);
            OnEntry("ScProbe_29EF38", &HkCoreProbe<1>);
            OnEntry("SealProbe_68157C", &HkSeal<1>);
            OnEntry("SealProbe_5BA648", &HkSeal<2>);
            OnEntry("SealProbe_322468", &HkSeal<3>);
            OnEntry("SealProbe_3203EC", &HkSeal<4>);
            OnEntry("SealProbe_3578E0", &HkChrCreated);
        }
    }
}

}  // namespace bosslog

namespace fight {
// 'Leave arena' (user 20261018): the native boss mode was ended outside the script's BossBattleModeEnd: clear the fight state as HkEnd does
void ForceEnd(const char* why) {
    if (bosslog::g_logOn) Log("BossLog: fight state cleared (%s)", why);
    bosslog::g_endAt.store(-1e9);  // no post-fight grace: there was no fight
    bosslog::g_active.store(false);
    InterlockedExchange(&g_active, 0);
}
void LastFightPlacements(char* buf, size_t n) {
    AcquireSRWLockShared(&bosslog::g_lastLock);
    snprintf(buf, n, "%s", bosslog::g_lastPlac);
    ReleaseSRWLockShared(&bosslog::g_lastLock);
}
bool PlacementSeenRecently(uint32_t id, double withinSec) {
    const double t = Now();
    const uint32_t rn = bosslog::g_ringN.load();
    for (uint32_t i = rn > (uint32_t)bosslog::kRing ? rn - (uint32_t)bosslog::kRing : 0; i < rn; ++i) {
        const auto& e = bosslog::g_ring[i % bosslog::kRing];
        if (e.id.load() == id && t - e.t.load() <= withinSec) return true;
    }
    return false;
}
double Now() {
    static const LARGE_INTEGER f0 = [] {
        LARGE_INTEGER x;
        QueryPerformanceCounter(&x);
        return x;
    }();
    static const double sec = [] {
        LARGE_INTEGER fq;
        QueryPerformanceFrequency(&fq);
        return 1.0 / (double)fq.QuadPart;
    }();
    LARGE_INTEGER q;
    QueryPerformanceCounter(&q);
    return (double)(q.QuadPart - f0.QuadPart) * sec;
}
}  // namespace fight
}  // namespace addon
