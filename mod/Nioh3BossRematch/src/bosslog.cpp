// bosslog.cpp - BossLog / RematchLog / InvLog (moved from Nioh3EngineFix 1.7.40 into the add-on; log-only, game behaviour unchanged)
// and the FIGHT state the rematch uses (fight count, in-fight flag, placements created before the last BossBattleMode).
// [Debug] BossLog / RematchLog / InvLog in Nioh3BossRematch.ini (all 0 by default). Every hook: the handler runs, then the
// original instructions, with all argument registers and xmm0-5 intact (hooks.cpp). Sites resolve by pattern / native name.
#include "addon.h"

#include <intrin.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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
// Unproven: that ED584 rebuilds the body from blk+4 (it may read the body only from the record at +C; the log dumps both).
std::atomic<int> g_fkAppearMode{0};
std::atomic<int> g_fkAppRestore{0};
uint64_t g_fkAppBlk = 0;
uint32_t g_fkAppOld = 0, g_fkAppNew = 0;
double g_fkAppT = 0;
void FkAppearLog(const char* tag, uint64_t chr) {
    uint64_t e8 = 0, blk = 0;
    uint32_t key = 0, row = 0, b0 = 0, b4 = 0, b8 = 0, st = 0, dirty = 0, r[4] = {};
    Rd(chr, key);
    if (Rd(chr + 0xE8, e8) && e8) Rd(e8 + 0x248, row);
    if (Rd(chr + 0x560, blk) && blk) {
        Rd(blk, b0);
        Rd(blk + 4, b4);
        Rd(blk + 8, b8);
        Rd(blk + 0x804, st);
        Rd(blk + 0x814, dirty);
        for (int i = 0; i < 4; ++i) Rd(blk + 0xC + 4ull * i, r[i]);
    }
    Log("FormKey APPEAR %s chr=%llX chrId=[chr]>>4=%X F26C4=%X [chr+560]=%llX [[chr+560]+4]=%X [[chr+E8]+248]=%X blk+0=%X blk+8=%X state+804=%u dirty+814=%08X rec+C[0..3]=%X %X %X %X t=%.3f",
        tag, (unsigned long long)chr, key >> 4, FkBodyId(chr), (unsigned long long)blk, b4, row, b0, b8, st, dirty, r[0], r[1], r[2], r[3], Now());
}
bool FkAppearWrite(uint64_t blk, uint32_t id) {
    __try {
        return WrU32(blk + 4, id) && WrU32(blk + 0x814, 0x01010101);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
void FkAppearApply(uint64_t pchr, uint64_t schr) {
    uint64_t blk = 0;
    const uint32_t sb = FkSealBody(schr);
    if (!pchr || !Rd(pchr + 0x560, blk) || !blk) { Log("FormKey APPEAR skip: player has no appearance block [chr+560]"); return; }
    if (!sb || sb == 0xFFFFFFFF) { Log("FormKey APPEAR skip: seal body id [[schr+E8]+248]=%X", sb); return; }
    uint32_t cur = 0;
    Rd(blk + 4, cur);
    if (cur == sb) { Log("FormKey APPEAR skip: player [blk+4] already %X", sb); return; }
    if (g_fkAppBlk != blk) { g_fkAppBlk = blk; g_fkAppOld = cur; }
    const bool ok = FkAppearWrite(blk, sb);
    g_fkAppNew = sb;
    g_fkAppT = Now();
    Log("FormKey APPEAR WRITE player blk=%llX [blk+4] %X -> %X, +814=01010101 ok=%d (restore id %X)", (unsigned long long)blk, cur, sb, ok, g_fkAppOld);
    FkAppearLog("player after write", pchr);
}
void FkAppearTick(uint64_t pchr) {  // game thread (2E6C34 input hook)
    if (g_fkAppT && Now() - g_fkAppT > 1.0) {
        g_fkAppT = 0;
        FkAppearLog("player +1 s", pchr);
    }
    if (g_fkAppRestore.exchange(0) && g_fkAppBlk) {
        const bool ok = FkAppearWrite(g_fkAppBlk, g_fkAppOld);
        Log("FormKey APPEAR RESTORE blk=%llX [blk+4] %X -> %X ok=%d", (unsigned long long)g_fkAppBlk, g_fkAppNew, g_fkAppOld, ok);
        g_fkAppBlk = 0;
        FkAppearLog("player after restore", pchr);
    }
}
uint64_t FkPlayer();
uint64_t HkPart47(uint64_t part, uint32_t id, uint64_t chr, uint64_t r9) {
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
// 2E6C34 entry (rcx = input state; game thread): spirit chr check + the pending swap
void HkFormKeyInputBody(uint64_t a) {
    uint64_t pchr = 0;
    Rd(a + 0xE0, pchr);
    const uint64_t pl = FkPlayer();
    if (!pchr || (pl && pchr != pl)) return;  // only the local player's state
    FkAppearTick(pchr);
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
    if (!g_fkOn.load() || !chr || dist < 0 || dist >= 15.f || g_fkSealChr.load() == chr) return;
    g_fkSealMotion.store(motion);
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
            g_fkLogged.store(0);
        }
        Log("FormKey appear (pipe): FormAppear=%d restorePending=%d blk=%llX old=%X new=%X", g_fkAppearMode.load(), g_fkAppRestore.load(),
            (unsigned long long)g_fkAppBlk, g_fkAppOld, g_fkAppNew);
    } else if (!strncmp(p, "off", 3)) {
        g_fkOn.store(0);
        g_fkSwap.store(0);
        g_fkPendKey.store(0);
        g_fkCell = 0;
        Log("FormKey off (pipe)");
    }
    snprintf(reply, n, "ok formkey on=%d swapArmed=%d X=%X mgr=%llX state=%llX pendingKey=%llX lastHits=%ld", g_fkOn.load(), g_fkSwap.load(), g_fkX.load(),
             (unsigned long long)g_fkMgr.load(), (unsigned long long)g_fkState.load(), (unsigned long long)g_fkPendKey.load(), g_fkHits);
}

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
    if (IniInt(L"Spirit", L"FormKey", 0)) { g_fkOn.store(1); g_fkSwap.store(1); }
    g_fkPartMode.store(IniInt(L"Spirit", L"FormPart", 0));  // pc #54384: part-47 body id swap while FormModel is armed
    g_fkAppearMode.store(IniInt(L"Spirit", L"FormAppear", 0));  // pc #54368: seal body id -> player appearance [blk+4] + reload
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
        OnEntry("ScProbe_TenshinInput", &HkTenshinInput);
        OnEntry("ScProbe_786C84", &HkTenshinReq);
        { void* o = nullptr; const char* w = nullptr; if (InstallCallRetargetAt((uint8_t*)(g_base + 0x74C27F), "call:RefreshFormItemDef", (void*)&HkItemDefForm, &o, &w)) g_itemDefOrig = (void* (*)(void*, uint64_t))o; else Log("TenshinForm: itemdef hook failed: %s", w ? w : "?"); }
        OnEntry("ScProbe_SpiritActive", &HkSpiritActive);
        OnEntry("ScProbe_B611BC", &HkCoreProbe<0>);
        OnEntry("ScProbe_29EF38", &HkCoreProbe<1>);
        OnEntry("SealProbe_65A8A8", &HkSeal<0>);
        OnEntry("SealProbe_68157C", &HkSeal<1>);
        OnEntry("SealProbe_5BA648", &HkSeal<2>);
        OnEntry("SealProbe_322468", &HkSeal<3>);
        OnEntry("SealProbe_3203EC", &HkSeal<4>);
        OnEntry("SealProbe_3578E0", &HkChrCreated);
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
