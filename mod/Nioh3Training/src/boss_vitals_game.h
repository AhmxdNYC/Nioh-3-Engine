// user #14320 / #14323 / #14324: per-boss HP / Ki controls, game side. Included by training.c after the KiBreakNoFinisher block (g_nf, g_nf_off)
// and aggro.h; BvTick runs on the game thread (h_pump, after TrainTick). Decisions: boss_vitals.h.
// Fields (the same ones the boss HP / Ki logs read, e.g. 'KIBREAK watch boss .. HP 51712/51712 Ki 1040/1040'):
//   HP  = [[actor+0x3A0]+0x58] cur / +0x50 max (Character::GetHpRate, = the player's PlayerHp fields). Write: native HP set 66C188(hp+0x38, v)
//         (CMD bosshp, the NOFIN reserve / survive-at-1 path; proven live on bosses), read back; if it did not stick, the direct field write the
//         player's infinite health uses (PlayerRefill: [[actor+0x3A0]+0x58] = v).
//   Ki  = [[actor+0x3A0]+0xAC] cur / +0xB0 max (floats). Write: KiSetCur 2F88D0(st+0x98, v) (NfTick / KiBreakBareFix use it outside a hit).
//   Ki broken = StateTest 0x23 on [actor+0x3B0], or KiIsExhausted 844FE4(st+0x98), or Ki <= 0: Ki is then never written (break untouched).
// Bosses: Training slot A / B (g_slot_act, the Training session boss is Boss A when slot A is free, WatchTick). UI threads only read g_bv_live.
#include "boss_vitals.h"
static BvCfg g_bv[BV_SLOTS]; static volatile LONG g_bv_inited;
static volatile LONG g_bv_req[BV_SLOTS]; static DWORD g_bv_kiWait[BV_SLOTS];
static struct { volatile int live; volatile uint32_t chr; volatile uint64_t hc, hm; volatile float kc, km; } g_bv_live[BV_SLOTS];
static volatile LONG g_bv_logs; static DWORD g_bv_t;
static void BvInit(void) { if (InterlockedCompareExchange(&g_bv_inited, 1, 0) == 0) for (int s = 0; s < BV_SLOTS; ++s) BvDefaults(&g_bv[s]); }
static void BvLog(const char* fmt, ...) { if (InterlockedIncrement(&g_bv_logs) > 300) return; char m[400]; va_list ap; va_start(ap, fmt); vsnprintf(m, sizeof m, fmt, ap); va_end(ap); Log("%s", m); }
static uint64_t BvActor(int s) {
    if (s < 0 || s >= N3T_SLOTS) return 0;  // user #14309: every boss letter (N3T_SLOTS = BV_SLOTS)
    const uint64_t a = g_slot_act[s];
    return a && SlotAlive(s) && ActorInTable(a) ? a : 0;
}
static int BvNfSlot(uint64_t a) { for (int i = 0; i < NF_N; ++i) if (a && g_nf[i].a == a) return i; return -1; }
static __attribute__((noinline)) int BvWriteHp(int s, uint64_t a, uint64_t st, uint64_t target, uint64_t max, const char* why) {
    const int nf = BvNfSlot(a); const int64_t off = nf >= 0 ? g_nf_off[nf] : 0; int keep = 0;
    const uint64_t raw = BvHpRaw(target, max, off, &keep);
    uint64_t back = 0; int how = 1;
    __try { ((void (*)(uint64_t, uint64_t))(B + 0x66C188))(st + 0x38, raw); back = *(volatile uint64_t*)(st + 0x58); } __except (EXCEPTION_EXECUTE_HANDLER) { back = ~0ull; }
    if (back != raw) { how = 2; __try { *(volatile uint64_t*)(st + 0x58) = raw; back = *(volatile uint64_t*)(st + 0x58); } __except (EXCEPTION_EXECUTE_HANDLER) { back = ~0ull; } }
    if (nf >= 0 && off > 0 && !keep) g_nf_off[nf] = 0;  // the hidden reserve did not fit under max: dropped (NfTick re-adds it at <= 20 %)
    BvLog("BOSSVITALS Boss %c %p: HP -> %llu / %llu (%s, %s%s)%s", 'A' + s, (void*)a, target, max, why, how == 1 ? "native HP set" : "direct field write",
          off > 0 ? (keep ? ", NOFIN reserve kept" : ", NOFIN reserve dropped") : "", back == raw ? "" : " - WRITE DID NOT STICK");
    return back == raw;
}
// Ki ceiling loss (#22592 Reset fight 'WRITE DID NOT STICK'): KiSetCur 2F88D0 clamps to max - [K+0x88] (K = st+0x98, so st+0x120), and the native
// regen 2F83A0..2F8400 stops at the same ceiling. It is 0 at spawn and grows with the player's hits (live: 2.9 after a few light hits on Hino-enma,
// regen then stuck at 1661.1 / 1664; cleared by hand -> regen back to 1664). A refill / set above the ceiling clears it (= the spawn state) first.
static __attribute__((noinline)) int BvWriteKi(int s, uint64_t a, uint64_t st, float v, float max, const char* why) {
    float back = -1.0f, loss = 0.0f; int cleared = 0;
    __try {
        volatile float* lp = (volatile float*)(st + 0x98 + 0x88); loss = *lp;
        if (BvKiCeilClear(v, max, loss)) { *lp = 0.0f; cleared = 1; }
        ((void (*)(uint64_t, float))(B + 0x2F88D0))(st + 0x98, v); back = *(volatile float*)(st + 0xAC);
    } __except (EXCEPTION_EXECUTE_HANDLER) { back = -1.0f; }
    const int ok = back > v - 0.5f && back < v + 0.5f;
    char cl[80] = ""; if (cleared) snprintf(cl, sizeof cl, ", Ki ceiling loss %.1f cleared", loss);
    BvLog("BOSSVITALS Boss %c %p: Ki -> %.1f / %.1f (%s%s), read back %.1f%s", 'A' + s, (void*)a, v, max, why, cl, back, ok ? "" : " - WRITE DID NOT STICK");
    return ok;
}
static __attribute__((noinline)) void BvSlotTick(int s, DWORD now) {
    BvCfg* c = &g_bv[s];
    const uint64_t a = BvActor(s);
    uint64_t st = 0, ai = 0, hc = 0, hm = 0; float kc = 0, km = 0; int dead = 0, broken = 0;
    if (!a || !Rd64(a + 0x3A0, &st) || !st || !Rd64(a + 0x3B0, &ai) || !ai) {
        g_bv_live[s].live = 0; if (InterlockedExchange(&g_bv_req[s], 0)) BvLog("BOSSVITALS Boss %c: no live boss, request dropped", 'A' + s); return; }
    __try {
        hm = *(volatile uint64_t*)(st + 0x50); hc = *(volatile uint64_t*)(st + 0x58); dead = *(volatile int32_t*)(st + 0x48) == 1;
        kc = *(volatile float*)(st + 0xAC); km = *(volatile float*)(st + 0xB0);
        broken = KbState(ai, 0x23) || ((uint8_t (*)(uint64_t))(B + 0x844FE4))(st + 0x98) || kc <= 0.0f;
    } __except (EXCEPTION_EXECUTE_HANDLER) { g_bv_live[s].live = 0; return; }
    const int nf = BvNfSlot(a), window = nf >= 0 && g_nf[nf].win == 1;
    const int64_t off = nf >= 0 ? g_nf_off[nf] : 0;
    const uint64_t shown = off > 0 ? (hc > (uint64_t)off ? hc - (uint64_t)off : 0) : hc;  // HP the bar shows (NOFIN hidden reserve, #9319)
    g_bv_live[s].hc = shown; g_bv_live[s].hm = hm; g_bv_live[s].kc = kc; g_bv_live[s].km = km; g_bv_live[s].chr = g_slot_chr[s]; g_bv_live[s].live = 1;
    if (dead || !hm) { if (InterlockedExchange(&g_bv_req[s], 0)) BvLog("BOSSVITALS Boss %c %p: boss is dead, request dropped (no revive)", 'A' + s, (void*)a); return; }
    LONG req = InterlockedExchange(&g_bv_req[s], 0);
    uint64_t ht = 0; float kt = 0;
    if ((req & BV_REQ_HP) && !window) BvWriteHp(s, a, st, BvHpTarget(hm, c->hpPct), hm, "Set boss HP %");
    else if (req & BV_REQ_HP) BvLog("BOSSVITALS Boss %c %p: HP set skipped (KiBreakNoFinisher HP-0 window owns the HP)", 'A' + s, (void*)a);
    else if (BvHpHold(c, dead, window, shown, hm, &ht)) BvWriteHp(s, a, st, ht, hm, "infinite health");
    if (req & BV_REQ_KI) {
        if (!broken && !window) { BvWriteKi(s, a, st, BvKiTarget(km, c->kiPct), km, "Set boss Ki %"); g_bv_kiWait[s] = 0; }
        else {  // Ki broken: the break plays out; the set waits (BV_KI_WAIT_MS) and is then dropped
            if (!g_bv_kiWait[s]) { g_bv_kiWait[s] = now ? now : 1; BvLog("BOSSVITALS Boss %c %p: Ki set waits for the Ki break to end", 'A' + s, (void*)a); }
            if (now - g_bv_kiWait[s] < BV_KI_WAIT_MS) InterlockedOr(&g_bv_req[s], BV_REQ_KI);
            else { g_bv_kiWait[s] = 0; BvLog("BOSSVITALS Boss %c %p: Ki set dropped (still Ki broken after %d ms)", 'A' + s, (void*)a, BV_KI_WAIT_MS); }
        }
    } else if (BvKiHold(c, dead, window, broken, kc, km, &kt)) BvWriteKi(s, a, st, kt, km, "infinite Ki");
}
static void BvTick(void) {
    BvInit();
    const DWORD now = GetTickCount();
    if (now - g_bv_t < 50) return;  // 20 Hz: a hit shows on the bar for at most ~50 ms with infinite health / Ki
    g_bv_t = now;
    if (GameState() && GameState()[2]) return;  // loading
    for (int s = 0; s < N3T_SLOTS; ++s) BvSlotTick(s, now);  // also keeps g_bv_live fresh for the menu rows
}
// C API for Boss Rematch's per-boss page. slot 0 = Boss A, 1 = Boss B.
// Get: 1 = a live boss in the slot (hc/hm = HP the bar shows, kc/km = Ki), 0 = empty (cfg still filled), -1 = bad slot. cfg = {infHp, infKi, hpPct, kiPct}.
N3T_API int Nioh3Training_BossVitalsGet(int slot, uint64_t* hc, uint64_t* hm, float* kc, float* km, int* cfg) {
    BvInit();
    if (slot < 0 || slot >= BV_SLOTS) return -1;
    if (cfg) { cfg[0] = g_bv[slot].infHp; cfg[1] = g_bv[slot].infKi; cfg[2] = g_bv[slot].hpPct; cfg[3] = g_bv[slot].kiPct; }
    const int live = slot < N3T_SLOTS && g_bv_live[slot].live && SlotAlive(slot);
    if (hc) *hc = live ? g_bv_live[slot].hc : 0;
    if (hm) *hm = live ? g_bv_live[slot].hm : 0;
    if (kc) *kc = live ? g_bv_live[slot].kc : 0;
    if (km) *km = live ? g_bv_live[slot].km : 0;
    return live;
}
// Set: what = BV_INF_HP / BV_INF_KI (value 1 / 0, -1 flips), BV_HP_PCT / BV_KI_PCT (value = %, clamped; applied once on the game thread and the new
// infinite level), BV_REFILL (both levels 100 %, both applied). Returns the new value (REFILL: 1), -1 = bad slot / what. Session only (not saved).
N3T_API int Nioh3Training_BossVitalsSet(int slot, int what, int value) {
    BvInit();
    if (slot < 0 || slot >= BV_SLOTS) return -1;
    BvCfg* c = &g_bv[slot]; int r = -1;
    switch (what) {
    case BV_INF_HP: r = c->infHp = value < 0 ? !c->infHp : value != 0; break;
    case BV_INF_KI: r = c->infKi = value < 0 ? !c->infKi : value != 0; break;
    case BV_HP_PCT: r = c->hpPct = BvClampPct(value, 1); InterlockedOr(&g_bv_req[slot], BV_REQ_HP); break;
    case BV_KI_PCT: r = c->kiPct = BvClampPct(value, 0); g_bv_kiWait[slot] = 0; InterlockedOr(&g_bv_req[slot], BV_REQ_KI); break;
    case BV_REFILL: c->hpPct = 100; c->kiPct = 100; g_bv_kiWait[slot] = 0; InterlockedOr(&g_bv_req[slot], BV_REQ_HP | BV_REQ_KI); r = 1; break;
    default: return -1;
    }
    InterlockedExchange(&g_bv_logs, 0);
    Log("BOSSVITALS Boss %c: %s -> %d (infinite health %d at %d %%, infinite Ki %d at %d %%)", 'A' + slot,
        what == BV_INF_HP ? "infinite health" : what == BV_INF_KI ? "infinite Ki" : what == BV_HP_PCT ? "set HP %" : what == BV_KI_PCT ? "set Ki %" : "refill",
        r, c->infHp, c->hpPct, c->infKi, c->kiPct);
    return r;
}
