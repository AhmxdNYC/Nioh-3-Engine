// bosslog.cpp - BossLog / RematchLog / InvLog (moved from Nioh3EngineFix 1.7.40 into the add-on; log-only, game behaviour unchanged)
// and the FIGHT state the rematch uses (fight count, in-fight flag, placements created before the last BossBattleMode).
// [Debug] BossLog / RematchLog / InvLog in Nioh3BossRematch.ini (all 0 by default). Every hook: the handler runs, then the
// original instructions, with all argument registers and xmm0-5 intact (hooks.cpp). Sites resolve by pattern / native name.
#include "addon.h"

#include <intrin.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>

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

}  // namespace

void Register(bool boss, bool rm, bool inv) {
    g_base = g_img.base;
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
    if (inv) OnEntry("InventoryAddItem", &HkInvAdd);
}

}  // namespace bosslog

namespace fight {
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
