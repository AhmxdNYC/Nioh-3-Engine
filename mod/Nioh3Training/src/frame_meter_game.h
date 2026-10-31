// Practice frame meter (SF6 style), game side. Included by training.c after frame_probe_game.h. State machine: frame_meter.h (tests/frame_meter_test.c).
// Input: the FRAMEPROBE signals only (frame_probe_game.h): the boss action id seen by the pump, open hit windows (attack collision still in its pool
// slot) and hits created since the last tick, with the GreatAttack bit (hit +0x13C bit 10 = unblockable, notes/yoritomo-hit-source-22305.md).
// No game memory is read or written beyond what the probe already does. Generic: the first live Training boss slot, no per-boss table.
// Clock: GAME time (user #22575): the game's own step per update tick from FpGameStep (frame_probe_game.h), so one cell is 1/60 s of game time at
// any frame rate; it also stops while the mod menu pauses the game (MENU PAUSE), the F9 freeze or a load.
// Drawing: a click-through topmost layered GDI window over the game (the same fallback renderer as the Rematch standalone menu and the fade window),
// so no Present hook and no other ASI is involved. Visible with the mod menu open; hidden when unfocused, loading, or without data.
// Ini (Nioh3Training.ini [Practice]): FrameMeter = 0/1 (default 0, the Practice hub row 'Frame meter'), FrameMeterY = top of the meter in % of the
// screen height; -1 (default) anchors above the lower player HUD, slightly left.
#include "frame_meter.h"
#include "frame_meter_iframe.h"
#include "frame_meter_kipress.h"
#include "frame_meter_ki.h"
#include "frame_meter_position.h"
#include <string.h>
static __attribute__((noinline)) int NkmRead(uint64_t a,float* cur,float* mx,float* k60,int* dead);
static int FmHumanBoss(uint64_t actor,uint32_t chr); // definition after automatic binding, no per-boss list
static const char* const g_fm_ki_label="KI DEBT";
static FmKi g_fm_ki; static volatile LONG g_fm_ki_show;
static volatile LONG g_fz_hold, g_fz_over, g_fz_frames, g_fz_steps; static int g_fz_vk_freeze, g_fz_vk_step;  // practice_freeze_game.h (Practice freeze + frame advance)
static const char* FzKeyName(int vk, char* b, int n);
static FmMeter g_fm, g_fmp; static volatile LONG g_fmp_show; static SRWLOCK g_fm_lock = SRWLOCK_INIT;
static volatile LONG g_fm_ver, g_fm_show, g_fm_thread; static int g_fm_slot = -1; static double g_fm_clock, g_fm_last; static int g_cfg_fm_y = -1;
static HWND g_fm_wnd, g_fm_game; static uint64_t g_fm_actor; static uint32_t g_fm_chr; static char g_fm_who[48] = "Boss A";
// Label of the followed boss: "Boss B - <name>" (name from the arena table row of its character id, when there is one).
static void FmWho(int slot, uint32_t chr, char* out, int n) {
    const char* nm = 0;
    for (int i = 0; i < g_nrows && chr; ++i) if (g_rows[i].chr == chr && g_rows[i].name[0]) { nm = g_rows[i].name; break; }
    if (nm) snprintf(out, n, "Boss %c - %s", 'A' + slot, nm); else snprintf(out, n, "Boss %c", 'A' + slot);
}
static DWORD WINAPI FmThread(LPVOID p);
static int g_cfg_ifbyte;  // [Debug] IFrameBytes 1: log the outside-model i-frame bytes per change (notes/frame-meter-lanes-37199.md)
static int g_cfg_ifp;  // [Debug] IFrameProbe 1: log-only probe of the player's state bits (notes/iframes.md)
static int g_cfg_kipress;  // [Debug] KiPressProbe 1: log R1 against the blue bar (notes/ki-press-probe-35849.md)
static int g_cfg_fm_unfocused;  // [Debug] FrameMeterUnfocused 1: keep drawing when the game is not the front window. Default 0.
static int g_cfg_fm_drawcost; // [Debug] FrameMeterDrawCost 1: opt-in CPU paint/upload timing, per style.
static void FmLoadIni(const char* ini) {
    g_cfg_ifp = (int)GetPrivateProfileIntA("Debug", "IFrameProbe", 0, ini);
    g_cfg_ifbyte = (int)GetPrivateProfileIntA("Debug", "IFrameBytes", 0, ini) ? 1 : 0;
    if (g_cfg_ifbyte) Log("IFRAMEEA on: [Debug] IFrameBytes 1, log-only (ctrl+EAA/EA0/EA5/EA7/EA8/EB0 and status+2DC8 bit 12)");
    g_cfg_kipress = (int)GetPrivateProfileIntA("Debug", "KiPressProbe", 0, ini);
    g_cfg_fm_unfocused = (int)GetPrivateProfileIntA("Debug", "FrameMeterUnfocused", 0, ini) ? 1 : 0;
    g_cfg_fm_drawcost = (int)GetPrivateProfileIntA("Debug", "FrameMeterDrawCost", 0, ini) ? 1 : 0;
    g_fm_ki_perfect_only = (int)GetPrivateProfileIntA("Practice", "KiWindowPerfectOnly", 1, ini) ? 1 : 0;
    g_cfg_fm_y = (int)GetPrivateProfileIntA("Practice", "FrameMeterY", -1, ini); if (g_cfg_fm_y < -1 || g_cfg_fm_y > 90) g_cfg_fm_y = -1;
    Log("FRAME METER: [Practice] FrameMeter %d, PlayerFrameMeter %d, KiWindow %d, FrameMeterY %d (-1 = above player HUD; otherwise top percent)", g_pst.v[PS_FRAME_METER], g_pst.v[PS_PLAYER_METER], g_pst.v[PS_KI_WINDOW], g_cfg_fm_y);
    if (g_cfg_fm_unfocused) Log("FRAME METER: [Debug] FrameMeterUnfocused 1, still drawing when the game is not the front window");
    if (g_cfg_kipress) Log("KIPRESS on. Bar frame 0 is the tick the blue bar opens. R1 is XInput bit 0x0200.");
}
// Player KiTick K+0x74 (status = [actor+0x3A0], K = status+0x98). Read only. 1 while the 30-tick window is counting down.
static __attribute__((noinline)) int FmPlayerKiOpen(uint64_t actor) {
    uint64_t st = 0; float kt = -1.f;
    if (!actor || !Rd64((uintptr_t)actor + 0x3A0, &st) || !st) return 0;
    __try { kt = *(volatile float*)(st + 0x98 + 0x74); } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    return FmKiWindowOpen(kt);
}
static __attribute__((noinline)) int FmKpCtrl(uint64_t actor, uint64_t ctrl, uint64_t* x1060, int* same) {
    uint64_t a = 0;
    *x1060 = 0; *same = 0;
    if (!ctrl) return 0;
    __try { *x1060 = *(volatile uint64_t*)(ctrl + 0x1060); a = *(volatile uint64_t*)(ctrl + 0xE0); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    *same = a == actor;
    return 1;
}
// KiTick +0x5C and current Ki. Read only. 1 on the tick a pulse lands (see FmKiPulseStep).
// v2: window K+0x74, pool K+0x5C, Ki status+0xAC (see FmKiPulseStep2).
static __attribute__((noinline)) int FmPlayerKiLanded(uint64_t actor, float* ktOut, float* kiOut, float* t74Out) {
    static uint64_t who = 0;
    static FmPulseWatch w;
    static float pt7c = -1.f, pstored = 0.f;
    uint64_t st = 0;
    float kt = 0.f, ki = 0.f, t74 = -1.f, t7c = -1.f, stored = 0.f, pt74, ppool, pki;
    int landed = 0;
    if (!actor || actor != who) { who = actor; memset(&w, 0, sizeof w); pt7c = -1.f; }
    if (ktOut) *ktOut = 0.f;
    if (kiOut) *kiOut = 0.f;
    if (t74Out) *t74Out = -1.f;
    if (!actor || !Rd64((uintptr_t)actor + 0x3A0, &st) || !st) { memset(&w, 0, sizeof w); return 0; }
    __try {
        kt = *(volatile float*)(st + 0x98 + 0x5C);
        t74 = *(volatile float*)(st + 0x98 + 0x74);
        ki = *(volatile float*)(st + 0xAC);
        t7c = *(volatile float*)(st + 0x98 + 0x7C);
        stored = *(volatile float*)(st + 0x98 + 0x58);
    } __except (EXCEPTION_EXECUTE_HANDLER) { memset(&w, 0, sizeof w); return 0; }
    if (ktOut) *ktOut = kt;
    if (kiOut) *kiOut = ki;
    if (t74Out) *t74Out = t74;
    pt74 = w.t74; ppool = w.pool; pki = w.ki;
    landed = FmKiPulseStep2(&w, t74, kt, ki);
    {
        const int grade = FmKiPulseGrade(pt74, t74, pki, ki, 1.f, pt7c);
        if (grade) {
            uint64_t ai = 0, ctrl = 0, x1060 = 0; int same = 0, cok = 0;
            if (Rd64((uintptr_t)actor + 0x3B0, &ai) && ai && Rd64(ai + 8, &ctrl) && ctrl) cok = FmKpCtrl(actor, ctrl, &x1060, &same);
            Log("KI PULSE grade %s: window %.2f -> %.2f, after-time %.2f -> %.2f, stored %.1f -> %.1f, Ki %.1f -> %.1f (+%.1f), pool %.1f, x1060 %016llX bit19 %d (%s)",
                grade == 2 ? "perfect" : "full", pt74, t74, pt7c, t7c, pstored, stored, pki, ki, ki - pki, ppool,
                (unsigned long long)x1060, (int)((x1060 >> 0x19) & 1), !cok ? "ctrl unread" : same ? "ctrl ok" : "ctrl+0xE0 != actor");
        }
    }
    pt7c = t7c; pstored = stored;
    if (landed) Log("KI PULSE land: window %.1f -> %.1f, pool %.1f -> %.1f, Ki %.1f -> %.1f", pt74, t74, ppool, kt, pki, ki);
    return landed;
}
// #35176 i-frames (notes/iframes.md). Read only. 1 while a flag the game's own hit / damage gates test is set:
//   actor+0xABC0 (contact handler 117A6D0 returns: no hit), ctrl+0xEB1 (Character::SetNoHitInvincible, 117A7B6: no hit),
//   ctrl+0xEB0 (Character::SetNoDamageInvincible) and actor+0xADB1 (NoDamageMode) read by IsNoDamage 2A343C: no damage.
// ctrl = [[actor+0x3B0]+8]. Dodge i-frames are probably hurtbox-based and NOT covered by these flags (unproven; the probe below is for that).
static __attribute__((noinline)) int FmPlayerNoDmg(uint64_t actor, uint64_t ctrl) {
    int r = 0;
    if (!actor || !ctrl) return 0;
    __try {
        r = *(volatile uint8_t*)(actor + 0xABC0) || *(volatile uint8_t*)(actor + 0xADB1) || *(volatile uint8_t*)(ctrl + 0xEB0) || *(volatile uint8_t*)(ctrl + 0xEB1);
    } __except (EXCEPTION_EXECUTE_HANDLER) { r = 0; }
    return r;
}
// human #37361: bits for the I-FR row from the probe bytes (ctrl+0xEAA = COND4 / outside reverser byte, ctrl+0xEA5 = guard, ctrl+0xEA7). Read only.
static __attribute__((noinline)) int FmPlayerFlagBits(uint64_t ctrl) {
    int r = 0;
    if (!ctrl) return 0;
    __try {
        if (*(volatile uint8_t*)(ctrl + 0xEAA)) r |= FMF_EAA;
        if (*(volatile uint8_t*)(ctrl + 0xEA5)) r |= FMF_GUARD;
        if (*(volatile uint8_t*)(ctrl + 0xEA7)) r |= FMF_EA7;
    } __except (EXCEPTION_EXECUTE_HANDLER) { r = 0; }
    return r;
}
// #35176 hurtbox side. The collision pool [45C6F38]+0x20+i*8 (0xFA0 slots) holds attack and damage (hurtbox) collision objects:
// obj+0 = shape (shape+0x10 = category mask, damage = 0x3FFFC0, attack = 0x440003F), obj+8 = owner actor, obj+0xD4 flags, obj+0xD8 part,
// obj+0xB0 = def (def+0x14 = type). The contact handler 117A6D0 never hits a damage collision whose def type is 0x17 or 0x19, and an object
// that is not in the pool cannot be touched at all. Read only. Fills a summary of the player's damage collisions; returns their count (-1 = no pool).
typedef struct { int n, off; uint32_t d4or, d4and, types; uint64_t m18or; char sig[480]; } FmHurt;
static __attribute__((noinline)) int FmHurtScan(uint64_t actor, FmHurt* h) {
    uint64_t pool = 0; int o = 0;
    memset(h, 0, sizeof *h); h->d4and = 0xFFFFFFFFu;
    if (!actor || !Rd64(B + Grva(0x45C6F38), &pool) || !pool) return -1;
    __try {
        for (int i = 0; i < 0xFA0; ++i) {
            const uint64_t obj = *(volatile uint64_t*)(pool + 0x20 + (uint64_t)i * 8);
            if (!obj || *(volatile uint64_t*)(obj + 8) != actor) continue;
            const uint64_t shape = *(volatile uint64_t*)obj;
            if (!shape) continue;
            const uint32_t m10 = *(volatile uint32_t*)(shape + 0x10);
            if (!(m10 & 0x3FFFC0u)) continue;
            const uint64_t m18 = *(volatile uint64_t*)(shape + 0x18);  // collides-with mask (0xB9B38 / 0xA11340)
            h->m18or |= m18;
            const uint32_t d4 = *(volatile uint32_t*)(obj + 0xD4), d8 = *(volatile uint32_t*)(obj + 0xD8);
            const uint64_t def = *(volatile uint64_t*)(obj + 0xB0);
            const int type = def ? *(volatile int32_t*)(def + 0x14) : -1;
            h->n++; h->d4or |= d4; h->d4and &= d4;
            if (type == 0x17 || type == 0x19) h->off++;
            if (type >= 0 && type < 32) h->types |= 1u << type;
            if (o < (int)sizeof h->sig - 64) o += snprintf(h->sig + o, sizeof h->sig - o, " %d:%X/%X/%X/%llX/t%d", i, d4, d8, m10, (unsigned long long)m18, type);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
    if (!h->n) h->d4and = 0;
    return h->n;
}
// Candidate i-frame from the hurtbox (UNPROVEN as the dodge mechanism, see notes/iframes.md): the player owns hurtbox objects normally,
// so "none left" or "every one has a no-hit def type" means attacks cannot connect. `base` = the count seen while idle (>0).
// m18seen = the hurtboxes of this actor were seen colliding with attack categories before (so a mask without them is a change, not the norm).
static int FmHurtOff(const FmHurt* h, int n, int base, int m18seen) { return n >= 0 && base > 0 && (n == 0 || h->off == n || (m18seen && n > 0 && !(h->m18or & 0x440003Full))); }
// #35176 contact-handler probe (log only, [Debug] IFrameProbe 1). 117A6D0(attack obj, damage obj) is the attack-vs-hurtbox contact handler
// (callers 117A49D/4E6/574/57F; hookcheck entry: must preserve []). Entry = mov r11,rsp / push rbp / push rdi / push r14 / push r15 /
// lea rbp,[r11-1E8h] (16 bytes, no rip-relative operand). The detour calls the original first, then counts player contacts and whether the
// hit queue [[47452E0]+0x5910] grew (hit queued) or not (rejected in 117A6D0). Runs on the physics thread: Interlocked counters only.
typedef void (*FmCtFn)(uint64_t, uint64_t);
static FmCtFn o_fmct; static volatile LONG g_fmct_hooked, g_fmct_calls, g_fmct_pl, g_fmct_plq, g_fmct_lines; static volatile uint64_t g_fmct_player, g_fmct_atkOwner, g_fmct_atkObj;
static volatile uint32_t g_fmct_act; static volatile double g_fmct_t;
typedef uint64_t (*FmHistFn)(uint64_t, uint64_t);
// The inputs of the reject exits of 117A6D0 (notes/iframes.md), read BEFORE the original runs. Read only; the hit-history test 2B44C4 is a pure list walk.
static __attribute__((noinline)) void FmCtInputs(uint64_t atk, uint64_t dmg, uint64_t def, char* o, int n) {
    uint64_t ai = 0, ctrl = 0, H = 0, row = 0, dd = 0, ao = 0, hist = 0; int dtype = -1, hcnt = -1, side_d = -1, side_a = -1;
    uint32_t r44 = 0, r48 = 0, r4c = 0, d4 = 0; int8_t r21 = 0; uint8_t abc0 = 0, adb1 = 0, eb1 = 0, eb0 = 0; uint8_t r92 = 0;
    __try {
        ao = *(volatile uint64_t*)(atk + 8);
        abc0 = *(volatile uint8_t*)(def + 0xABC0); adb1 = *(volatile uint8_t*)(def + 0xADB1);
        ai = *(volatile uint64_t*)(def + 0x3B0); if (ai) ctrl = *(volatile uint64_t*)(ai + 8);
        if (ctrl) { eb1 = *(volatile uint8_t*)(ctrl + 0xEB1); eb0 = *(volatile uint8_t*)(ctrl + 0xEB0); }
        { const uint64_t sd = *(volatile uint64_t*)(def + 0xB0); if (sd) side_d = *(volatile int32_t*)(sd + 0x10); }
        if (ao && !(*(volatile uint8_t*)ao & 0xF)) { const uint64_t sa = *(volatile uint64_t*)(ao + 0xB0); if (sa) side_a = *(volatile int32_t*)(sa + 0x10); }
        dd = *(volatile uint64_t*)(dmg + 0xB0); if (dd) dtype = *(volatile int32_t*)(dd + 0x14);
        d4 = *(volatile uint32_t*)(dmg + 0xD4);
        H = *(volatile uint64_t*)(atk + 0xB0);
        if (H) {
            row = *(volatile uint64_t*)H; hcnt = *(volatile int32_t*)(H + 0xE4);
            if (row) { r44 = *(volatile uint32_t*)(row + 0x44); r48 = *(volatile uint32_t*)(row + 0x48); r4c = *(volatile uint16_t*)(row + 0x4C); r21 = *(volatile int8_t*)(row + 0x21); r92 = *(volatile uint8_t*)(row + 0x92); }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { snprintf(o, n, " read-fault"); return; }
    if (ai && H) { __try { hist = ((FmHistFn)(B + Grva(0x2B44C4)))(ai, atk) & 0xFF; } __except (EXCEPTION_EXECUTE_HANDLER) { hist = 0xEE; } }
    snprintf(o, n, " abc0 %u eb1 %u eb0 %u adb1 %u side %d/%d dtype %d d4 %X row %p r48 %X r44 %X r4c %X r21 %d r92 %u hcnt %d hist %u",
        abc0, eb1, eb0, adb1, side_d, side_a, dtype, d4, (void*)row, r48, r44, r4c, (int)r21, r92, hcnt, (unsigned)hist);
}
__attribute__((used)) static void FmCtDetour(uint64_t atk, uint64_t dmg) {
    const uint64_t pl = g_fmct_player; int mine = 0; uint32_t q0 = 0, q1 = 0; uint64_t mgr = 0, ao = 0;
    InterlockedIncrement(&g_fmct_calls);
    if (pl && atk && dmg) {
        __try { ao = *(volatile uint64_t*)(atk + 8); mine = *(volatile uint64_t*)(dmg + 8) == pl && ao != pl; } __except (EXCEPTION_EXECUTE_HANDLER) { mine = 0; }
        if (mine && Rd64(B + Grva(0x47452E0), &mgr) && mgr) { __try { q0 = *(volatile uint32_t*)(mgr + 0x5910); } __except (EXCEPTION_EXECUTE_HANDLER) { mgr = 0; } }
    }
    char in[400]; in[0] = 0;
    if (mine && g_fmct_lines < 1500) FmCtInputs(atk, dmg, pl, in, sizeof in);
    o_fmct(atk, dmg);
    if (mine) {
        if (mgr) { __try { q1 = *(volatile uint32_t*)(mgr + 0x5910); } __except (EXCEPTION_EXECUTE_HANDLER) { q1 = q0; } }
        InterlockedIncrement(&g_fmct_pl); if (q1 != q0) InterlockedIncrement(&g_fmct_plq);
        g_fmct_atkOwner = ao; g_fmct_atkObj = atk;
        if (in[0] && InterlockedIncrement(&g_fmct_lines) <= 1500)
            Log("IFRAMECX t %.1f act %u queued %d atk %p obj %p%s", g_fmct_t, g_fmct_act, q1 != q0, (void*)ao, (void*)atk, in);
    }
}
static void FmCtInstall(void) {
    static const uint8_t pro[16] = {0x4C, 0x8B, 0xDC, 0x55, 0x57, 0x41, 0x56, 0x41, 0x57, 0x49, 0x8D, 0xAB, 0x18, 0xFE, 0xFF, 0xFF};
    if (g_fmct_hooked) return;
    if (memcmp((void*)(B + Grva(0x117A6D0)), pro, sizeof pro)) { g_fmct_hooked = 2; Log("IFRAMECT hook 117A6D0: bytes differ, probe off"); return; }
    o_fmct = (FmCtFn)Hook(0x117A6D0, 16, (void*)FmCtDetour); g_fmct_hooked = 1;
    Log("IFRAMECT hook 117A6D0: on (log only, [Debug] IFrameProbe 1)");
}
// #35176 damage-pipeline probe (log only, IFrameProbe 1). 78A7F0(dmg) = evaluate a queued hit: 78AC18 (can be damaged) + dmg+0x14C, then 326C94
// (damage / reaction), else dmg+0x2A0 = 1 (no damage). Its entry has a rel32 call at +9, so the trampoline re-creates the first 3 instructions
// and calls 78AC18 by absolute address, then jumps back to 78A7FE. hookcheck entry 78A7F0: 1 direct caller (78A4B5), must preserve [].
// After the original returns, one IFRAMEDM line per hit on the player: the result fields and the defender inputs the no-damage paths read.
typedef void (*FmDmFn)(uint64_t);
static FmDmFn o_fmdm; static volatile LONG g_fmdm_hooked, g_fmdm_lines;
__attribute__((used)) static void FmDmDetour(uint64_t dmg) {
    const uint64_t pl = g_fmct_player; uint64_t def = 0, ctrl = 0, st = 0, stat = 0, rec = 0, rq = 0; uint32_t rec18 = 0, stat48 = 0, s2dc8 = 0, id = 0;
    uint8_t ea0 = 0, ea1 = 0, eae = 0; uint64_t ae0 = 0, a8 = 0;
    int mine = 0;
    __try {
        def = *(volatile uint64_t*)(dmg + 0x1C8);
        mine = pl && def == pl;
        if (mine) {
            const uint64_t ai = *(volatile uint64_t*)(def + 0x3B0); ctrl = ai ? *(volatile uint64_t*)(ai + 8) : 0;
            stat = *(volatile uint64_t*)(def + 0x3A0);
            if (stat) { stat48 = *(volatile uint32_t*)(stat + 0x48); s2dc8 = *(volatile uint32_t*)(stat + 0x2DC8); }
            if (ctrl) {
                st = *(volatile uint64_t*)(ctrl + 0xE8); if (st) { id = *(volatile uint32_t*)st; rec = *(volatile uint64_t*)(st + 0x20); }
                if (rec) { rq = *(volatile uint64_t*)rec; rec18 = *(volatile uint32_t*)(rec + 0x18); }
                ea0 = *(volatile uint8_t*)(ctrl + 0xEA0); ea1 = *(volatile uint8_t*)(ctrl + 0xEA1); eae = *(volatile uint8_t*)(ctrl + 0xEAE);
                ae0 = *(volatile uint64_t*)(ctrl + 0xAE0); a8 = *(volatile uint64_t*)(ctrl + 0xA8);
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { mine = 0; }
    o_fmdm(dmg);
    if (!mine || InterlockedIncrement(&g_fmdm_lines) > 1500) return;
    uint8_t nod = 0, f14c = 0, f282 = 0, f286 = 0, f28e = 0, f29e = 0, f2ae = 0; int32_t r1e0 = -9, r1e4 = -9, r1ec = -9, r1fc = -9; uint64_t row0 = 0, atk = 0;
    __try {
        nod = *(volatile uint8_t*)(dmg + 0x2A0); f14c = *(volatile uint8_t*)(dmg + 0x14C); f282 = *(volatile uint8_t*)(dmg + 0x282); f286 = *(volatile uint8_t*)(dmg + 0x286);
        f28e = *(volatile uint8_t*)(dmg + 0x28E); f29e = *(volatile uint8_t*)(dmg + 0x29E); f2ae = *(volatile uint8_t*)(dmg + 0x2AE);
        r1e0 = *(volatile int32_t*)(dmg + 0x1E0); r1e4 = *(volatile int32_t*)(dmg + 0x1E4); r1ec = *(volatile int32_t*)(dmg + 0x1EC); r1fc = *(volatile int32_t*)(dmg + 0x1FC);
        atk = *(volatile uint64_t*)(dmg + 0x198);
        { const uint64_t r = *(volatile uint64_t*)(dmg + 0x190); if (r) row0 = *(volatile uint64_t*)r; }
    } __except (EXCEPTION_EXECUTE_HANDLER) { }
    Log("IFRAMEDM t %.1f dmg %p act %u state-id %u nodmg %u f14c %u 1e0 %d 1e4 %d 1ec %d 1fc %d guard282 %u 286 %u 28e %u 29e %u 2ae %u atk %p row0 %llX | rec0 %016llX (bit35 %u) rec18 %X stat48 %u s2dc8 %X ea0 %u ea1 %u eae %u ae0 %llX a8 %016llX",
        g_fmct_t, (void*)dmg, g_fmct_act, id, nod, f14c, r1e0, r1e4, r1ec, r1fc, f282, f286, f28e, f29e, f2ae, (void*)atk, (unsigned long long)row0,
        (unsigned long long)rq, (unsigned)(rq >> 35 & 1), rec18, stat48, s2dc8, ea0, ea1, eae, (unsigned long long)ae0, (unsigned long long)a8);
}
static void FmDmInstall(void) {
    static const uint8_t pro[14] = {0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0xD9, 0xE8, 0x1A, 0x04, 0x00, 0x00};
    if (g_fmdm_hooked) return;
    uint8_t* site = (uint8_t*)(B + Grva(0x78A7F0));
    if (memcmp(site, pro, sizeof pro)) { g_fmdm_hooked = 2; Log("IFRAMEDM hook 78A7F0: bytes differ, probe off"); return; }
    uint8_t* tr = (uint8_t*)VirtualAlloc(0, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tr) { g_fmdm_hooked = 2; return; }
    int k = 0;
    memcpy(tr, pro, 9); k = 9;                                                             // push rbx / sub rsp,20h / mov rbx,rcx
    tr[k++] = 0x48; tr[k++] = 0xB8; *(uint64_t*)(tr + k) = (uint64_t)(B + Grva(0x78AC18)); k += 8; // mov rax, 78AC18
    tr[k++] = 0xFF; tr[k++] = 0xD0;                                                        // call rax
    tr[k++] = 0xFF; tr[k++] = 0x25; *(uint32_t*)(tr + k) = 0; k += 4; *(uint64_t*)(tr + k) = (uint64_t)(B + Grva(0x78A7FE)); k += 8;  // jmp [rip] -> 78A7FE
    o_fmdm = (FmDmFn)tr;
    DWORD old; VirtualProtect(site, 14, PAGE_EXECUTE_READWRITE, &old);
    uint8_t pt[14]; pt[0] = 0x48; pt[1] = 0xB8; *(uint64_t*)(pt + 2) = (uint64_t)(void*)FmDmDetour; pt[10] = 0xFF; pt[11] = 0xE0; pt[12] = 0x90; pt[13] = 0x90;
    memcpy(site, pt, 14);
    VirtualProtect(site, 14, old, &old); FlushInstructionCache(GetCurrentProcess(), site, 14);
    g_fmdm_hooked = 1; Log("IFRAMEDM hook 78A7F0: on (log only, [Debug] IFrameProbe 1)");
}
// #35391: one IFRAMEHP line per HP-damage call on the player (NfHpDamage at 329890, IFrameProbe 1, max 1500): raw damage, after the Practice
// multiplier, applied (after the Infinite-health cut), HP before (hp+0x20 = status+0x58). hit = the same object IFRAMEDM prints as 'dmg'.
static void FmIfHpLog(uint64_t hp, int raw, int scaled, int applied, uint64_t hit) {
    static volatile LONG n; uint64_t me = PlayerChar(), st = 0; int64_t cur = -1, mx = -1;
    if (!g_cfg_ifp || !hp || !me || !Rd64(me + 0x3A0, &st) || !st || st + 0x38 != hp) return;
    if (InterlockedIncrement(&n) > 1500) return;
    __try { cur = *(volatile int64_t*)(hp + 0x20); mx = *(volatile int64_t*)(hp + 0x18); } __except (EXCEPTION_EXECUTE_HANDLER) { }
    Log("IFRAMEHP t %.1f act %u hit %p raw %d scaled %d applied %d hp-before %lld/%lld class %s", g_fmct_t, g_fmct_act, (void*)hit, raw, scaled, applied,
        (long long)cur, (long long)mx, raw <= 0 ? "zero" : applied > 0 ? "damage" : "real-hit-cut-by-infinite-health");
}
// Pump side: one IFRAMECT line per tick with player contacts (max 3000).
static void FmCtTick(uint64_t actor, uint32_t act, double t) {
    static LONG pc, pq; static int lines;
    if (g_fmct_hooked == 0) FmCtInstall();
    if (g_fmdm_hooked == 0) FmDmInstall();
    g_fmct_player = actor; g_fmct_act = act; g_fmct_t = t;
    if (g_fmct_hooked != 1) return;
    const LONG c = g_fmct_pl, q = g_fmct_plq, dc = c - pc, dq = q - pq; pc = c; pq = q;
    if (dc > 0 && lines < 3000) {
        lines++;
        Log("IFRAMECT t %.1f act %u contacts %ld queued %ld rejected %ld atk %p obj %p (all calls %ld)", t, act, dc, dq, dc - dq,
            (void*)g_fmct_atkOwner, (void*)g_fmct_atkObj, g_fmct_calls);
    }
}
// #35176 IFRAMEOB filter: skip the per-tick transform floats seen in log #2331 (shape +0x30..+0x3C, +0x80..+0x9C; object +0x90..+0xAC,
// +0xC0..+0xCC, +0xF0..+0xFC) and any change that looks like a small step of a float (both values normal floats, |delta| < 25 % + 1).
static int FmObNoisy(int shape, int off, uint32_t a, uint32_t b) {
    if (shape ? ((off >= 0x30 && off < 0x40) || (off >= 0x80 && off < 0xA0)) : ((off >= 0x90 && off < 0xB0) || (off >= 0xC0 && off < 0xD0) || off >= 0xF0)) return 1;
    {
        const uint32_t ea = (a >> 23) & 0xFF, eb = (b >> 23) & 0xFF;
        if (ea >= 0x60 && ea <= 0x9F && eb >= 0x60 && eb <= 0x9F) {
            float fa, fb, d; memcpy(&fa, &a, 4); memcpy(&fb, &b, 4); d = fb - fa; if (d < 0) d = -d;
            if (fa < 0) fa = -fa;
            if (d < 0.25f * fa + 1.0f) return 1;
        }
    }
    return 0;
}
// #35411 empty-area probe (notes/iframes.md "Empty-area probe"). FmiEval runs every player tick while the player meter is on (also without
// IFrameProbe, because the stripe uses it): the defender-side "no damage" predicates of the hit / damage pipeline, evaluated on the player
// with no enemy and no hit. Game functions called (all checked in the disassembly: loads, compares and calls to each other only, no store
// except their own stack frame): 2A3264 (cannot be damaged, hit type in edx), 481290, 866490, 876130 (condition 4), 2A3764. Everything else
// is re-implemented from the decompile with plain loads. NOT evaluated: the buff vcall of IsNoDamage (ctrl vtbl +0x138 -> F6878 -> +0x208 =
// F68D8, 2.6 KB frame, not proven pure) and the rest of the defence state 2A34F8 (vcalls). The probe logs the vtable slots once instead.
// Entry bytes of the called functions (from re/Nioh3.dump.exe). If one differs (another game version), no game function is called.
static int FmiCodeOk(void) {
    static int ok = -1;
    if (ok >= 0) return ok;
    static const uint8_t k264[11] = {0x48, 0x83, 0xEC, 0x28, 0x44, 0x8B, 0xDA, 0x4C, 0x8B, 0xD1, 0xE8};
    static const uint8_t k290[14] = {0x48, 0x8B, 0x81, 0xE8, 0x00, 0x00, 0x00, 0x45, 0x33, 0xC0, 0x48, 0x85, 0xC0, 0x74};
    static const uint8_t k490[10] = {0x48, 0x83, 0xEC, 0x28, 0x8B, 0x81, 0xA8, 0x00, 0x00, 0x00};
    static const uint8_t k130[13] = {0x48, 0x8B, 0x81, 0xE0, 0x00, 0x00, 0x00, 0x48, 0x8B, 0x90, 0xA0, 0x03, 0x00};
    static const uint8_t k764[13] = {0x48, 0x8B, 0x81, 0xE0, 0x00, 0x00, 0x00, 0x48, 0x8B, 0x90, 0xA0, 0x03, 0x00};
    __try {
        ok = !memcmp((void*)(B + Grva(0x2A3264)), k264, sizeof k264) && !memcmp((void*)(B + Grva(0x481290)), k290, sizeof k290) && !memcmp((void*)(B + Grva(0x866490)), k490, sizeof k490)
            && !memcmp((void*)(B + Grva(0x876130)), k130, sizeof k130) && !memcmp((void*)(B + Grva(0x2A3764)), k764, sizeof k764);
    } __except (EXCEPTION_EXECUTE_HANDLER) { ok = 0; }
    Log("IFRAMEPR predicate functions 2A3264/481290/866490/876130/2A3764: %s", ok ? "bytes match, called read-only" : "bytes differ, only the re-implemented bits are used");
    return ok;
}
typedef uint64_t (*FmiFn1)(uint64_t);
typedef uint64_t (*FmiFn2)(uint64_t, int);
typedef struct { uint32_t mask, r18, s48; uint64_t a8, ae0, ea0, eb0, s2dc8, rec, q0; uint16_t next, recAct; uint8_t abc0, adb1; int ok, called; } FmiState;
static __attribute__((noinline)) void FmiEval(uint64_t actor, uint64_t ctrl, int hurtOff, FmiState* s) {
    uint64_t st = 0, stat = 0, x15 = 0, e0 = 0; uint8_t x15b = 0;
    memset(s, 0, sizeof *s);
    if (!actor || !ctrl) return;
    __try {
        e0 = *(volatile uint64_t*)(ctrl + 0xE0);
        stat = *(volatile uint64_t*)(actor + 0x3A0);
        st = *(volatile uint64_t*)(ctrl + 0xE8);
        if (st) s->rec = *(volatile uint64_t*)(st + 0x20);
        if (s->rec) { s->q0 = *(volatile uint64_t*)s->rec; s->r18 = *(volatile uint32_t*)(s->rec + 0x18); s->recAct = *(volatile uint16_t*)(s->rec + 0x0C); s->next = *(volatile uint16_t*)(s->rec + 0xE0); }
        if (stat) { s->s48 = *(volatile uint32_t*)(stat + 0x48); s->s2dc8 = *(volatile uint64_t*)(stat + 0x2DC8); }
        s->a8 = *(volatile uint64_t*)(ctrl + 0xA8); s->ae0 = *(volatile uint64_t*)(ctrl + 0xAE0);
        s->ea0 = *(volatile uint64_t*)(ctrl + 0xEA0); s->eb0 = *(volatile uint64_t*)(ctrl + 0xEB0);
        x15 = *(volatile uint64_t*)(ctrl + 0x15D8); if (x15) x15b = *(volatile uint8_t*)(x15 + 2);
        s->abc0 = *(volatile uint8_t*)(actor + 0xABC0); s->adb1 = *(volatile uint8_t*)(actor + 0xADB1);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
    s->ok = 1;
    const uint8_t* ea = (const uint8_t*)&s->ea0; const uint8_t* eb = (const uint8_t*)&s->eb0;  // ea[k] = ctrl+0xEA0+k, eb[k] = ctrl+0xEB0+k
    uint32_t m = 0;
    if (s->q0 >> 35 & 1) m |= FMI_NODMGACT;
    if (s->s48 == 1) m |= FMI_S48;
    if (s->s2dc8 >> 31 & 1) m |= FMI_S2DC8;
    if (ea[0]) m |= FMI_EA0;
    if (ea[1]) m |= FMI_EA1;
    if (ea[0xE]) m |= FMI_EAE;
    if (x15b) m |= FMI_X15D8;
    if (s->adb1 || eb[0]) m |= FMI_ISNODMG;
    if (s->abc0 || eb[1]) m |= FMI_NOHIT;
    if ((s->s2dc8 >> 11 & 1) || (s->s2dc8 >> 48 & 1) || ea[5]) m |= FMI_GUARDX;
    if (hurtOff) m |= FMI_HURTOFF;
    (void)e0; (void)stat; (void)st;
    { static int once; if (!once) { once = 1; FmiCodeOk(); Log("IFRAMEPR: game functions are not called from the pump"); } }
    s->called = 0;
    s->mask = m;
}
// IFRAMEWN: per action run, the dwords of actor (0..0xB300), ctrl (0..0x1700) and status [actor+0x3A0] (0..0x3400) that turn on after the start
// and back before the end (window) or turn once and stay (open). One line set at the end of every run. Read only.
enum { FMI_NA = 0xB300 / 4, FMI_NC = 0x1700 / 4, FMI_NS = 0x3400 / 4 };
static FmiWord g_fmi_wa[FMI_NA], g_fmi_wc[FMI_NC], g_fmi_ws[FMI_NS];
static uint32_t g_fmi_ca[FMI_NA], g_fmi_cc[FMI_NC], g_fmi_cs[FMI_NS];
static __attribute__((noinline)) int FmiReadBlock(uint64_t p, uint32_t* out, int n) {
    if (!p) return 0;
    __try { for (int i = 0; i < n; ++i) out[i] = *(volatile uint32_t*)(p + (uint64_t)i * 4); } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    return 1;
}
static void FmiWindows(uint64_t actor, uint64_t ctrl, uint32_t act, double t) {
    static uint32_t pact = 0xFFFFFFFF; static double t0; static uint64_t pactor; static int have, lines, okA, okC, okS;
    uint64_t stat = 0; Rd64(actor + 0x3A0, &stat);
    const int a = FmiReadBlock(actor, g_fmi_ca, FMI_NA), c = FmiReadBlock(ctrl, g_fmi_cc, FMI_NC), s = FmiReadBlock(stat, g_fmi_cs, FMI_NS);
    if (act != pact || actor != pactor || a != okA || c != okC || s != okS) {
        if (have && pact != 0xFFFFFFFF && lines < 1500) {
            const int frames = FmiFrame(t, t0);
            if (frames >= 3) {
                char b[960]; int o = snprintf(b, sizeof b, "IFRAMEWN t %.1f act %u frames %d noisy %d/%d/%d:", t0, pact, frames,
                    okA ? FmiTrackNoisy(g_fmi_wa, FMI_NA) : -1, okC ? FmiTrackNoisy(g_fmi_wc, FMI_NC) : -1, okS ? FmiTrackNoisy(g_fmi_ws, FMI_NS) : -1);
                int n = 0;
                if (okC) n += FmiTrackFormat(g_fmi_wc, FMI_NC, 'c', 0, b, sizeof b, &o, 40 - n);
                if (okS) n += FmiTrackFormat(g_fmi_ws, FMI_NS, 's', 0, b, sizeof b, &o, 40 - n);
                if (okA) n += FmiTrackFormat(g_fmi_wa, FMI_NA, 'a', 0, b, sizeof b, &o, 40 - n);
                if (!n) snprintf(b + o, sizeof b - o, " none");
                lines++; Log("%s", b);
            }
        }
        okA = a; okC = c; okS = s; pact = act; pactor = actor; t0 = t; have = a || c || s;
        if (a) FmiTrackReset(g_fmi_wa, g_fmi_ca, FMI_NA);
        if (c) FmiTrackReset(g_fmi_wc, g_fmi_cc, FMI_NC);
        if (s) FmiTrackReset(g_fmi_ws, g_fmi_cs, FMI_NS);
        return;
    }
    const int f = FmiFrame(t, t0);
    if (a) FmiTrackStep(g_fmi_wa, g_fmi_ca, FMI_NA, f);
    if (c) FmiTrackStep(g_fmi_wc, g_fmi_cc, FMI_NC, f);
    if (s) FmiTrackStep(g_fmi_ws, g_fmi_cs, FMI_NS, f);
}
// IFRAMEPR: one line per tick while the player is in a non-looping action (record +0xE0 next != own id), max 150 per run and 6000 in total:
// frame since the action start, the predicate mask, and the raw inputs that changed since the previous tick (all of them on frame 0).
static void FmiLogTick(uint64_t ctrl, uint32_t act, double t, const FmiState* s) {
    static uint32_t pact = 0xFFFFFFFF; static double t0; static int perRun, lines, told; static FmiState p;
    if (!told) {
        told = 1; uint64_t vt = 0, c0 = 0, c138 = 0, e0 = 0; Rd64(ctrl, &vt); if (vt) { Rd64(vt + 0xC0, &c0); Rd64(vt + 0x138, &c138); } Rd64(ctrl + 0xE0, &e0);
        Log("IFRAMEPR start: ctrl vtbl %llX (+C0 %llX, +138 %llX; static base class 38FE698 / 2E9934 / F6878) ctrl+E0 == player %d, calls %d. Bits: 0 cannot0 1 cannot4 2 cannot9 3 r29ae0 4 a8b20 5 nodmgact 6 s48 7 s2dc8b31 8 ea0 9 ea1 10 eae 11 x15d8 12 isnodmg 13 nohit 14 cond4 15 armor 16 guardx 17 hurtoff; stripe set %X",
            (unsigned long long)(vt ? vt - B : 0), (unsigned long long)(c0 ? c0 - B : 0), (unsigned long long)(c138 ? c138 - B : 0), e0 == g_fmct_player, s->called, (unsigned)FMI_STRIPE_MASK);
    }
    const int start = act != pact;
    if (start) { pact = act; t0 = t; perRun = 0; }
    if (!s->ok || act == 0xFFFF || (s->rec && s->next == (uint16_t)act) || perRun >= 150 || lines >= 6000) { p = *s; return; }
    char b[700], nm[200]; int o; FmiNames(s->mask, nm, sizeof nm);
    o = snprintf(b, sizeof b, "IFRAMEPR t %.1f act %u f %d m %05X %s", t, act, FmiFrame(t, t0), s->mask, nm);
    if (start || perRun == 0) o += snprintf(b + o, sizeof b - o, " | rec %p q0 %016llX r18 %08X next %u recact %u", (void*)s->rec, (unsigned long long)s->q0, s->r18, s->next, s->recAct);
    if (start || perRun == 0 || s->a8 != p.a8) o += snprintf(b + o, sizeof b - o, " a8=%llX", (unsigned long long)s->a8);
    if (start || perRun == 0 || s->ae0 != p.ae0) o += snprintf(b + o, sizeof b - o, " ae0=%llX", (unsigned long long)s->ae0);
    if (start || perRun == 0 || s->ea0 != p.ea0) o += snprintf(b + o, sizeof b - o, " ea0=%016llX", (unsigned long long)s->ea0);
    if (start || perRun == 0 || s->eb0 != p.eb0) o += snprintf(b + o, sizeof b - o, " eb0=%016llX", (unsigned long long)s->eb0);
    if (start || perRun == 0 || s->s2dc8 != p.s2dc8) o += snprintf(b + o, sizeof b - o, " s2dc8=%llX", (unsigned long long)s->s2dc8);
    if (start || perRun == 0 || s->s48 != p.s48) o += snprintf(b + o, sizeof b - o, " s48=%u", s->s48);
    if (start || perRun == 0 || s->abc0 != p.abc0 || s->adb1 != p.adb1) snprintf(b + o, sizeof b - o, " abc0=%u adb1=%u", s->abc0, s->adb1);
    perRun++; lines++; p = *s;
    Log("%s", b);
}
// #37169 i-frame byte probe ([Debug] IFrameBytes 1, log only, no game call). Outside model (#37168): C = *(*(char+0x3B0)+8) is the action controller,
// u8(C+0xEAA) = i-frames, status+0x2DC8 bit 12 = same flag, C+0xEA5 auto-guard, C+0xEA7 / 0xEA8 deflect guard, C+0xEB0 super armor.
// One IFRAMEEA line whenever the action id or any of the values changes; f = frames since the action id changed (60 per second, game clock).
static __attribute__((noinline)) void FmIFrameBytes(uint64_t actor, uint64_t ctrl, uint32_t act, double t) {
    static uint32_t pact = 0xFFFFFFFF; static double t0; static int lines, told; static uint64_t pv = ~0ull;
    static const int off[6] = {0xEAA, 0xEA0, 0xEA5, 0xEA7, 0xEA8, 0xEB0};
    if (!actor || !ctrl || lines >= 30000) return;
    uint8_t v[7] = {0}; uint64_t stat = 0, s2 = 0, p3b0 = 0, c2 = 0;
    __try {
        for (int i = 0; i < 6; ++i) v[i] = *(volatile uint8_t*)(ctrl + off[i]);
        stat = *(volatile uint64_t*)(actor + 0x3A0); if (stat) s2 = *(volatile uint64_t*)(stat + 0x2DC8);
        p3b0 = *(volatile uint64_t*)(actor + 0x3B0); if (p3b0) c2 = *(volatile uint64_t*)(p3b0 + 8);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
    v[6] = (uint8_t)(s2 >> 12 & 1);
    if (!told) { told = 1; Log("IFRAMEEA start: C via actor+3B0 -> +8 = %llX, probe ctrl = %llX, %s. Fields: eaa ea0 ea5 ea7 ea8 eb0 b12 (status+2DC8 bit 12)", (unsigned long long)c2, (unsigned long long)ctrl, c2 == ctrl ? "SAME" : "DIFFERENT"); }
    uint64_t raw = 0; for (int i = 0; i < 7; ++i) raw |= (uint64_t)v[i] << (8 * i);
    const int start = act != pact;
    if (start) { pact = act; t0 = t; }
    if (!start && raw == pv) return;
    pv = raw; lines++;
    const double d = t - t0; const int f = d <= 0 ? 0 : (int)(d * 60.0 / 1000.0 + 1e-6);
    Log("IFRAMEEA t %.1f act %u f %d eaa %u ea0 %u ea5 %u ea7 %u ea8 %u eb0 %u b12 %u%s", t, act, f, v[0], v[1], v[2], v[3], v[4], v[5], v[6], start ? " START" : "");
}
// [Debug] IFrameProbe 1: one log line whenever the player's action id or a candidate state word changes (ctrl+0xA8 / +0x1060 flag qwords,
// the ctrl+0xEA0..0xEB2 state bytes, actor+0xABC0 / +0xADB1). Dodge in Practice, then read which bit follows the dodge window. Capped.
// #35411: also IFRAMEPR (per tick predicates) and IFRAMEWN (per run window scan). The older IFRAMEST / IFRAMEOB dumps need IFrameProbe 2.
static __attribute__((noinline)) void FmIFrameProbe(uint64_t actor, uint64_t ctrl, uint32_t act, double t, const FmiState* is) {
    static uint64_t pa8, p1060, pb0, pb1, pb2; static uint32_t pact; static int lines, told, hlines; static char psig[480];
    uint64_t a8 = 0, x1060 = 0, b0 = 0, b1 = 0, b2 = 0;
    if (!g_cfg_ifp || !actor || !ctrl) return;
    FmCtTick(actor, act, t);
    FmiLogTick(ctrl, act, t, is);
    FmiWindows(actor, ctrl, act, t);
    {   // hurtbox objects: one IFRAMEHB line when the set or a field changes
        FmHurt h; const int n = FmHurtScan(actor, &h);
        if (n >= 0 && strcmp(h.sig, psig) && hlines < 3000) {
            hlines++; memcpy(psig, h.sig, sizeof psig);
            Log("IFRAMEHB t %.1f act %u n %d off %d d4or %X d4and %X m18or %llX types %X:%s", t, act, n, h.off, h.d4or, h.d4and, (unsigned long long)h.m18or, h.types, h.sig);
        }
    }
    {   // backup (#35176 log #2325): during dodge actions (10000-10199) log every u32 that changed since the previous tick in
        // status [actor+0x3A0] +0x80..+0x400 and +0x2DC0..+0x2E00, and ctrl +0x7F0..+0x820. One IFRAMEST line per tick with changes.
        enum { ST_A = 0x80 / 4, ST_B = 0x400 / 4, ST2_A = 0x2DC0 / 4, ST2_B = 0x2E00 / 4, CT_A = 0x7F0 / 4, CT_B = 0x820 / 4 };
        static uint32_t ps[ST_B], ps2[ST2_B - ST2_A], pc[CT_B - CT_A]; static int have, stlines; static uint32_t stAct;
        uint32_t cs[ST_B], cs2[ST2_B - ST2_A], cc[CT_B - CT_A]; uint64_t st = 0; int ok = 0;
        if (g_cfg_ifp >= 2 && act >= 10000 && act < 10200 && stlines < 2500 && Rd64(actor + 0x3A0, &st) && st) {
            __try {
                for (int k = ST_A; k < ST_B; ++k) cs[k] = *(volatile uint32_t*)(st + (uint64_t)k * 4);
                for (int k = ST2_A; k < ST2_B; ++k) cs2[k - ST2_A] = *(volatile uint32_t*)(st + (uint64_t)k * 4);
                for (int k = CT_A; k < CT_B; ++k) cc[k - CT_A] = *(volatile uint32_t*)(ctrl + (uint64_t)k * 4);
                ok = 1;
            } __except (EXCEPTION_EXECUTE_HANDLER) { ok = 0; }
        }
        if (!ok) have = 0;
        else {
            if (have && stAct == act) {
                char b[900]; int o = 0, nch = 0;
                for (int k = ST_A; k < ST_B && o < (int)sizeof b - 48; ++k) if (cs[k] != ps[k]) { float f; memcpy(&f, &cs[k], 4); o += snprintf(b + o, sizeof b - o, " s%X=%X(%.3g)", k * 4, cs[k], f); nch++; }
                for (int k = 0; k < ST2_B - ST2_A && o < (int)sizeof b - 48; ++k) if (cs2[k] != ps2[k]) { o += snprintf(b + o, sizeof b - o, " s%X=%X", (ST2_A + k) * 4, cs2[k]); nch++; }
                for (int k = 0; k < CT_B - CT_A && o < (int)sizeof b - 48; ++k) if (cc[k] != pc[k]) { float f; memcpy(&f, &cc[k], 4); o += snprintf(b + o, sizeof b - o, " c%X=%X(%.3g)", (CT_A + k) * 4, cc[k], f); nch++; }
                if (nch) { stlines++; Log("IFRAMEST t %.1f act %u%s", t, act, b); }
            } else { stlines++; Log("IFRAMEST t %.1f act %u start (baseline taken)", t, act); }
            memcpy(ps, cs, sizeof ps); memcpy(ps2, cs2, sizeof ps2); memcpy(pc, cc, sizeof pc); have = 1; stAct = act;
        }
    }
    {   // #35176 IFRAMEOB v2 (log #2331): per dodge tick, the integer / flag-like u32s that changed in the player's first 4 hurtbox objects
        // (obj +0..+0x100) and shapes (+0..+0x100). Transform floats are skipped (FmObNoisy). Also the object that obj+0x10 points to
        // (+0..+0x40) whenever that pointer or its content changes. Max 150 lines per dodge start, 2500 in total.
        enum { NW = 0x100 / 4, NO = 4, PW = 0x40 / 4 };
        static uint32_t po[NO][NW], psh[NO][NW], ppt[NO][PW]; static uint64_t pobj[NO], pptr[NO]; static int obLines, perDodge; static uint32_t obAct;
        uint64_t pool = 0;
        if (g_cfg_ifp >= 2 && act >= 10000 && act < 10200 && obLines < 2500 && Rd64(B + Grva(0x45C6F38), &pool) && pool) {
            uint64_t objs[NO] = {0}, ptr[NO] = {0}; int no = 0;
            uint32_t co[NO][NW] = {{0}}, csh[NO][NW] = {{0}}, cpt[NO][PW] = {{0}}; int ok = 1;
            __try {
                for (int i = 0; i < 0xFA0 && no < NO; ++i) {
                    const uint64_t obj = *(volatile uint64_t*)(pool + 0x20 + (uint64_t)i * 8);
                    if (!obj || *(volatile uint64_t*)(obj + 8) != actor) continue;
                    const uint64_t sh = *(volatile uint64_t*)obj;
                    if (!sh || !(*(volatile uint32_t*)(sh + 0x10) & 0x3FFFC0u)) continue;
                    objs[no] = obj;
                    for (int k = 0; k < NW; ++k) { co[no][k] = *(volatile uint32_t*)(obj + (uint64_t)k * 4); csh[no][k] = *(volatile uint32_t*)(sh + (uint64_t)k * 4); }
                    no++;
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) { ok = 0; }
            for (int j = 0; ok && j < no; ++j) {  // separate guard: a stale pointer must not drop the whole sample
                ptr[j] = (uint64_t)co[j][4] | ((uint64_t)co[j][5] << 32);
                if (ptr[j] < 0x10000 || ptr[j] > 0x7FFFFFFFFFFFull) { ptr[j] = 0; continue; }
                __try { for (int k = 0; k < PW; ++k) cpt[j][k] = *(volatile uint32_t*)(ptr[j] + (uint64_t)k * 4); } __except (EXCEPTION_EXECUTE_HANDLER) { ptr[j] = 0; }
            }
            if (ok) {
                char b2[1000]; int o = 0, nch = 0;
                if (act != obAct) perDodge = 0;
                for (int j = 0; j < no && o < (int)sizeof b2 - 80; ++j) {
                    if (objs[j] != pobj[j] || act != obAct) { o += snprintf(b2 + o, sizeof b2 - o, " [%d new]", j); nch++; }
                    else for (int k = 0; k < NW && o < (int)sizeof b2 - 40; ++k) {
                        if (co[j][k] != po[j][k] && !FmObNoisy(0, k * 4, po[j][k], co[j][k])) { o += snprintf(b2 + o, sizeof b2 - o, " o%d+%X=%X", j, k * 4, co[j][k]); nch++; }
                        if (csh[j][k] != psh[j][k] && !FmObNoisy(1, k * 4, psh[j][k], csh[j][k])) { o += snprintf(b2 + o, sizeof b2 - o, " h%d+%X=%X", j, k * 4, csh[j][k]); nch++; }
                    }
                    if (ptr[j] && (ptr[j] != pptr[j] || memcmp(cpt[j], ppt[j], sizeof cpt[j]))) {
                        o += snprintf(b2 + o, sizeof b2 - o, " p%d=%llX:", j, (unsigned long long)ptr[j]);
                        for (int k = 0; k < PW && o < (int)sizeof b2 - 12; ++k) o += snprintf(b2 + o, sizeof b2 - o, "%s%X", k ? "," : "", cpt[j][k]);
                        nch++;
                    }
                }
                if (nch && perDodge < 150) { perDodge++; obLines++; Log("IFRAMEOB t %.1f act %u n %d%s", t, act, no, b2); }
                for (int j = 0; j < NO; ++j) { pobj[j] = j < no ? objs[j] : 0; pptr[j] = j < no ? ptr[j] : 0; }
                memcpy(po, co, sizeof po); memcpy(psh, csh, sizeof psh); memcpy(ppt, cpt, sizeof ppt); obAct = act;
            }
        }
    }
    __try {
        a8 = *(volatile uint64_t*)(ctrl + 0xA8); x1060 = *(volatile uint64_t*)(ctrl + 0x1060);
        b0 = *(volatile uint64_t*)(ctrl + 0xEA0); b1 = *(volatile uint64_t*)(ctrl + 0xEA8); b2 = *(volatile uint64_t*)(ctrl + 0xEB0);
        b1 = (b1 & 0x0000FFFFFFFFFFFFull) | ((uint64_t)*(volatile uint8_t*)(actor + 0xABC0) << 48) | ((uint64_t)*(volatile uint8_t*)(actor + 0xADB1) << 56);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
    if (act == pact && a8 == pa8 && x1060 == p1060 && b0 == pb0 && b1 == pb1 && b2 == pb2) return;
    pact = act; pa8 = a8; p1060 = x1060; pb0 = b0; pb1 = b1; pb2 = b2;
    if (lines >= 3000) { if (!told) { told = 1; Log("IFRAME probe cap 3000"); } return; }
    lines++;
    Log("IFRAME t %.1f act %u a8 %016llX x1060 %016llX ea0 %016llX ea8 %016llX (byte 6 = actor+ABC0, byte 7 = actor+ADB1) eb0 %016llX (EB4 = part mask) nodmg %d",
        t, act, (unsigned long long)a8, (unsigned long long)x1060, (unsigned long long)b0, (unsigned long long)b1, (unsigned long long)b2, FmPlayerNoDmg(actor, ctrl));
}
int FmIFrameProbeOn(void) { return g_cfg_ifp || g_cfg_ifbyte; }
int FmKiPressOn(void) { return g_cfg_kipress; }
// R1 through the game's XInput slot, so a VirtualPad tap is visible. Bit 0x0200.
static int FmPadRB(void) {
    typedef unsigned long (__stdcall *Fn)(unsigned long, void*);
    Fn fn = 0; uint64_t slot = 0;
    if (Rd64(B + Grva(0x38E2C50), &slot) && slot > 0x10000ull) fn = (Fn)slot;
    if (!fn) {
        static Fn raw; static int tried;
        if (!tried) { tried = 1; HMODULE x = LoadLibraryA("xinput1_4.dll"); if (!x) x = LoadLibraryA("xinput1_3.dll"); raw = x ? (Fn)GetProcAddress(x, "XInputGetState") : 0; }
        fn = raw;
    }
    if (!fn) return 0;
    uint8_t st[32]; memset(st, 0, sizeof st);
    __try { if (fn(0, st) != 0) return 0; return (*(unsigned short*)(st + 4) & 0x0200) != 0; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
// human #37394: PADBTN line whenever the real pad's buttons / triggers / stick change, next to the IFRAMEEA move lines ([Debug] IFrameBytes 1). Log only.
static __attribute__((noinline)) void FmPadLog(double t) {
    typedef unsigned long (__stdcall *Fn)(unsigned long, void*);
    static Fn raw; static int tried, lines; static uint64_t pv = ~0ull;
    if (lines >= 30000) return;
    if (!tried) { tried = 1; HMODULE x = LoadLibraryA("xinput1_4.dll"); if (!x) x = LoadLibraryA("xinput1_3.dll"); raw = x ? (Fn)GetProcAddress(x, "XInputGetState") : 0; }
    Fn fn = raw; uint64_t slot = 0;
    if (Rd64(B + Grva(0x38E2C50), &slot) && slot > 0x10000ull) fn = (Fn)slot;
    if (!fn) return;
    uint8_t st[32]; memset(st, 0, sizeof st);
    __try { if (fn(0, st) != 0) return; } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
    const unsigned btn = *(unsigned short*)(st + 4); const int lt = st[6], rt = st[7]; const int lx = *(short*)(st + 8), ly = *(short*)(st + 10);
    const int sx = lx > 12000 ? 1 : lx < -12000 ? -1 : 0, sy = ly > 12000 ? 1 : ly < -12000 ? -1 : 0;
    const uint64_t key = (uint64_t)btn | (uint64_t)(lt > 100) << 16 | (uint64_t)(rt > 100) << 17 | (uint64_t)(sx + 1) << 18 | (uint64_t)(sy + 1) << 20;
    if (key == pv) return;
    pv = key; lines++;
    static const char* const nm[16] = {"Up","Down","Left","Right","Start","Back","L3","R3","L1","R1","","","A","B","X","Y"};
    char b[160]; int o = 0; b[0] = 0;
    for (int i = 0; i < 16; ++i) if ((btn >> i & 1) && nm[i][0]) o += snprintf(b + o, sizeof b - o, "%s ", nm[i]);
    if (lt > 100) o += snprintf(b + o, sizeof b - o, "L2 ");
    if (rt > 100) o += snprintf(b + o, sizeof b - o, "R2 ");
    Log("PADBTN t %.1f %sstick %+d,%+d", t, b, sx, sy);
}
// last/seq/from at 0/4/8, pad at 12, ring ids at 16, caller RVAs at 80. The stub below uses those offsets.
static struct { volatile uint32_t last, seq, from, pad; volatile uint32_t ring[16]; volatile uint32_t callers[16]; } g_kp_fx;
static volatile int g_kp_frame = -1;
static int g_kp_fx_hooked;
// Records R8D and the return RVA at 0x3EAB1C, then runs the original. The on-disk exe is packed, so the check is the bytes in the running image.
static void FmFxInstall(void) {
    static const uint8_t pro[14] = {0x40,0x53,0x48,0x83,0xEC,0x40,0x48,0x8B,0x44,0x24,0x78,0x48,0x8B,0xDA};
    uint8_t* site = (uint8_t*)(B + Grva(0x3EAB1C));
    int32_t rel; uint8_t* stub; uint8_t* tr; int at;
    if (g_kp_fx_hooked) return;
    if (memcmp(site, pro, sizeof pro)) { g_kp_fx_hooked = 2; Log("KIPRESS fx hook 3EAB1C: bytes differ, effect id off"); return; }
    rel = *(int32_t*)(site + 0x24);
    if (site[0x23] != 0xE8 || (uint32_t)(Grva(0x3EAB44) + rel) != Grva(0x4CE90C)) { g_kp_fx_hooked = 2; Log("KIPRESS fx hook 3EAB1C: call is not 4CE90C, effect id off"); return; }
    stub = (uint8_t*)VirtualAlloc(0, 128, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!stub) { g_kp_fx_hooked = 2; Log("KIPRESS fx hook: no stub page"); return; }
    at = 0;
    stub[at++] = 0x49; stub[at++] = 0xBA; *(uint64_t*)(stub + at) = (uint64_t)&g_kp_fx; at += 8; // mov r10, &g_kp_fx
    stub[at++] = 0x45; stub[at++] = 0x89; stub[at++] = 0x02;                                     // mov [r10], r8d
    stub[at++] = 0x48; stub[at++] = 0x8B; stub[at++] = 0x04; stub[at++] = 0x24;                   // mov rax, [rsp] return address
    stub[at++] = 0x49; stub[at++] = 0xBB; *(uint64_t*)(stub + at) = (uint64_t)B; at += 8;         // mov r11, image base
    stub[at++] = 0x4C; stub[at++] = 0x29; stub[at++] = 0xD8;                                       // sub rax, r11
    stub[at++] = 0x41; stub[at++] = 0x89; stub[at++] = 0x42; stub[at++] = 0x08;                   // mov [r10+8], eax
    stub[at++] = 0x41; stub[at++] = 0xBB; *(uint32_t*)(stub + at) = 1; at += 4;                   // mov r11d, 1
    stub[at++] = 0xF0; stub[at++] = 0x45; stub[at++] = 0x0F; stub[at++] = 0xC1; stub[at++] = 0x5A; stub[at++] = 0x04; // lock xadd [r10+4], r11d
    stub[at++] = 0x41; stub[at++] = 0x83; stub[at++] = 0xE3; stub[at++] = 0x0F;                   // and r11d, 15
    stub[at++] = 0x47; stub[at++] = 0x89; stub[at++] = 0x44; stub[at++] = 0x9A; stub[at++] = 0x10; // mov [r10+r11*4+16], r8d
    stub[at++] = 0x43; stub[at++] = 0x89; stub[at++] = 0x44; stub[at++] = 0x9A; stub[at++] = 0x50; // mov [r10+r11*4+80], eax
    stub[at++] = 0xFF; stub[at++] = 0x25; *(uint32_t*)(stub + at) = 0; at += 4;                   // jmp qword [rip]
    tr = (uint8_t*)Hook(0x3EAB1C, 14, stub);
    *(uint64_t*)(stub + at) = (uint64_t)tr;
    g_kp_fx_hooked = 1;
    Log("KIPRESS fx hook 3EAB1C: bytes match, effect id is R8D, caller is the return RVA, calls 4CE90C");
}
// 0x3EA2C8 parses the row through 0x1399b0, and that parse advances a cursor, so this hook only reads.
// Record i is (countA-countB)+i in the array at [[row+8]+0x18]. Type word at +4, value dword at +0.
// Final size is fields 2-4 times the effect context +0x170/+0x174/+0x178 when its type nibble is 2, 6 or 7.
// The first 12 bytes are mov rax,rsp / save rbx,rdi / push rbp, with no rip-relative byte, so they can run from the stub.
static uint64_t g_kirow_save[4];
static int g_kirow_hooked;
static float KiRowF(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }
static int KiRowFields(uint64_t row, uint32_t* ty, uint32_t* val, uint32_t* nout, uint32_t* baseOut, uint64_t* objOut) {
    uint64_t obj = 0, p8 = 0, p20 = 0, data = 0;
    uint16_t countA = 0, countB = 0, base = 0;
    int i, n;
    *nout = 0; *baseOut = 0; *objOut = 0;
    for (i = 0; i < 10; ++i) { ty[i] = 0; val[i] = 0; }
    if (!row || !Rd64(row + 8, &obj) || obj < 0x10000) return 0;
    *objOut = obj;
    if (!Rd64(obj + 8, &p8) || p8 < 0x10000) return 0;
    if (!Rd64(obj + 0x18, &data) || data < 0x10000) return 0;
    if (!Rd64(obj + 0x20, &p20) || p20 < 0x10000) return 0;
    __try {
        countB = *(volatile uint16_t*)(p8 + 0xA6);
        countA = *(volatile uint16_t*)p20;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    if (countA < countB || countB > 64) return 0;
    base = (uint16_t)(countA - countB);
    *baseOut = base;
    *nout = countB;
    n = countB > 10 ? 10 : (int)countB;
    __try {
        for (i = 0; i < n; ++i) {
            uint64_t rp = data + (uint64_t)(uint16_t)(base + (uint16_t)i) * 8u;
            val[i] = *(volatile uint32_t*)rp;
            ty[i] = *(volatile uint16_t*)(rp + 4);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    return 1;
}
static void KiRowScale(float* m0, float* m1, float* m2, int* gate) {
    uint64_t ctx = 0;
    uint32_t nib;
    *m0 = *m1 = *m2 = 1.f;
    *gate = 0;
    if (!Rd64(B + Grva(0x45BA228), &ctx) || ctx < 0x10000) return;
    __try {
        nib = *(volatile uint32_t*)ctx & 0xFu;
        if (nib == 2 || nib == 6 || nib == 7) {
            *m0 = *(volatile float*)(ctx + 0x170);
            *m1 = *(volatile float*)(ctx + 0x174);
            *m2 = *(volatile float*)(ctx + 0x178);
            *gate = (int)nib;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { *m0 = *m1 = *m2 = 1.f; *gate = 0; }
}
static void KiRowLog(uint64_t row) {
    static int lines, told, busy;
    float t74, s0, s1, s2, a0, a1, f7, f8, f9, m0, m1, m2;
    uint32_t ty[10], val[10], n = 0, base = 0;
    uint64_t obj = 0;
    int i, r1, perfect, gate, ok;
    if (busy) return;
    busy = 1;
    for (i = 0; i < 10; ++i) { ty[i] = 0; val[i] = 0; }
    t74 = -1.f;
    __try {
        uint64_t a = g_fp[FP_PL].valid ? g_fp[FP_PL].a : 0, st = 0;
        if (a && Rd64(a + 0x3A0, &st) && st) t74 = *(volatile float*)(st + 0x98 + 0x74);
    } __except (EXCEPTION_EXECUTE_HANDLER) { t74 = -1.f; }
    ok = KiRowFields(row, ty, val, &n, &base, &obj);
    KiRowScale(&m0, &m1, &m2, &gate);
    r1 = FmPadRB();
    perfect = r1 && FmKiPerfect(t74);
    if (lines >= 240) { if (!told) { told = 1; Log("KIROW cap 240"); } busy = 0; return; }
    lines++;
    if (!ok) {
        Log("KIROW perfect %d t74 %.2f r1 %d row %llX obj %llX UNREAD",
            perfect, t74, r1, (unsigned long long)row, (unsigned long long)obj);
        busy = 0;
        return;
    }
    s0 = KiRowF(val[2]); s1 = KiRowF(val[3]); s2 = KiRowF(val[4]);
    a0 = KiRowF(val[5]); a1 = KiRowF(val[6]);
    f7 = KiRowF(val[7]); f8 = KiRowF(val[8]); f9 = KiRowF(val[9]);
    Log("KIROW perfect %d t74 %.2f r1 %d n %u base %u row %llX id %u f1 %u ty %u %u %u %u %u %u %u %u %u %u sz %.4f %.4f %.4f ang %.4f %.4f f7 %.4f f8 %.4f f9 %.4f sc %d %.4f %.4f %.4f fin %.4f %.4f %.4f",
        perfect, t74, r1, n, base, (unsigned long long)row, val[0], val[1],
        ty[0], ty[1], ty[2], ty[3], ty[4], ty[5], ty[6], ty[7], ty[8], ty[9],
        s0, s1, s2, a0, a1, f7, f8, f9,
        gate, m0, m1, m2, s0 * m0, s1 * m1, s2 * m2);
    busy = 0;
}
static void FmRowInstall(void) {
    static const uint8_t pro[12] = {0x48,0x8B,0xC4,0x48,0x89,0x58,0x10,0x48,0x89,0x78,0x18,0x55};
    uint8_t* site = (uint8_t*)(B + Grva(0x3EA2C8));
    uint8_t* stub; int at; DWORD old;
    if (g_kirow_hooked) return;
    if (memcmp(site, pro, 12)) {
        g_kirow_hooked = 2;
        Log("KIROW hook 3EA2C8: bytes differ %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
            site[0], site[1], site[2], site[3], site[4], site[5], site[6], site[7], site[8], site[9], site[10], site[11]);
        return;
    }
    stub = (uint8_t*)VirtualAlloc(0, 160, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!stub) { g_kirow_hooked = 2; Log("KIROW hook: no stub page"); return; }
    at = 0;
    stub[at++] = 0x49; stub[at++] = 0xBA; *(uint64_t*)(stub + at) = (uint64_t)g_kirow_save; at += 8;
    stub[at++] = 0x49; stub[at++] = 0x89; stub[at++] = 0x0A;
    stub[at++] = 0x49; stub[at++] = 0x89; stub[at++] = 0x52; stub[at++] = 0x08;
    stub[at++] = 0x4D; stub[at++] = 0x89; stub[at++] = 0x42; stub[at++] = 0x10;
    stub[at++] = 0x4D; stub[at++] = 0x89; stub[at++] = 0x4A; stub[at++] = 0x18;
    stub[at++] = 0x48; stub[at++] = 0x83; stub[at++] = 0xEC; stub[at++] = 0x28;
    stub[at++] = 0x49; stub[at++] = 0x8B; stub[at++] = 0x0A;
    stub[at++] = 0x48; stub[at++] = 0xB8; *(uint64_t*)(stub + at) = (uint64_t)KiRowLog; at += 8;
    stub[at++] = 0xFF; stub[at++] = 0xD0;
    stub[at++] = 0x48; stub[at++] = 0x83; stub[at++] = 0xC4; stub[at++] = 0x28;
    stub[at++] = 0x49; stub[at++] = 0xBA; *(uint64_t*)(stub + at) = (uint64_t)g_kirow_save; at += 8;
    stub[at++] = 0x49; stub[at++] = 0x8B; stub[at++] = 0x0A;
    stub[at++] = 0x49; stub[at++] = 0x8B; stub[at++] = 0x52; stub[at++] = 0x08;
    stub[at++] = 0x4D; stub[at++] = 0x8B; stub[at++] = 0x42; stub[at++] = 0x10;
    stub[at++] = 0x4D; stub[at++] = 0x8B; stub[at++] = 0x4A; stub[at++] = 0x18;
    memcpy(stub + at, pro, 12); at += 12;
    stub[at++] = 0xFF; stub[at++] = 0x25; *(uint32_t*)(stub + at) = 0; at += 4;
    *(uint64_t*)(stub + at) = (uint64_t)(site + 12); at += 8;
    VirtualProtect(site, 12, PAGE_EXECUTE_READWRITE, &old);
    site[0] = 0x48; site[1] = 0xB8; *(uint64_t*)(site + 2) = (uint64_t)stub; site[10] = 0xFF; site[11] = 0xE0;
    VirtualProtect(site, 12, old, &old);
    FlushInstructionCache(GetCurrentProcess(), site, 12);
    g_kirow_hooked = 1;
    Log("KIROW hook 3EA2C8 on. 10 row fields plus scaled size. perfect is R1 held and timer 0.05 to 4.");
}
// seq at 0. Each slot is 32 bytes at offset 8: return RVA, DX, RCX, R8.
static struct { volatile uint32_t seq, pad; struct { volatile uint32_t ret, id; volatile uint64_t rcx, r8; } slot[8]; } g_kp_buff;
static int g_kp_buff_hooked;
static int g_kp_tail;
static void FmBuffInstall(void) {
    static const uint8_t pro[13] = {0x48,0x8B,0xC4,0x48,0x89,0x58,0x18,0x66,0x89,0x50,0x10,0x55,0x56};
    uint8_t* site = (uint8_t*)(B + Grva(0x3EFB90));
    uint8_t* stub; uint8_t* tr; int at;
    if (g_kp_buff_hooked) return;
    if (memcmp(site, pro, sizeof pro)) { g_kp_buff_hooked = 2; Log("KIPRESS buff hook 3EFB90: bytes differ, buff log off"); return; }
    stub = (uint8_t*)VirtualAlloc(0, 128, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!stub) { g_kp_buff_hooked = 2; Log("KIPRESS buff hook: no stub page"); return; }
    at = 0;
    stub[at++] = 0x49; stub[at++] = 0xBA; *(uint64_t*)(stub + at) = (uint64_t)&g_kp_buff; at += 8;
    stub[at++] = 0x48; stub[at++] = 0x8B; stub[at++] = 0x04; stub[at++] = 0x24;
    stub[at++] = 0x49; stub[at++] = 0xBB; *(uint64_t*)(stub + at) = (uint64_t)B; at += 8;
    stub[at++] = 0x4C; stub[at++] = 0x29; stub[at++] = 0xD8;
    stub[at++] = 0x41; stub[at++] = 0xBB; *(uint32_t*)(stub + at) = 1; at += 4;
    stub[at++] = 0xF0; stub[at++] = 0x45; stub[at++] = 0x0F; stub[at++] = 0xC1; stub[at++] = 0x1A;
    stub[at++] = 0x41; stub[at++] = 0x83; stub[at++] = 0xE3; stub[at++] = 0x07;
    stub[at++] = 0x41; stub[at++] = 0xC1; stub[at++] = 0xE3; stub[at++] = 0x05;
    stub[at++] = 0x41; stub[at++] = 0x83; stub[at++] = 0xC3; stub[at++] = 0x08;
    stub[at++] = 0x43; stub[at++] = 0x89; stub[at++] = 0x04; stub[at++] = 0x1A;
    stub[at++] = 0x43; stub[at++] = 0x89; stub[at++] = 0x54; stub[at++] = 0x1A; stub[at++] = 0x04;
    stub[at++] = 0x4B; stub[at++] = 0x89; stub[at++] = 0x4C; stub[at++] = 0x1A; stub[at++] = 0x08;
    stub[at++] = 0x4F; stub[at++] = 0x89; stub[at++] = 0x44; stub[at++] = 0x1A; stub[at++] = 0x10;
    stub[at++] = 0xFF; stub[at++] = 0x25; *(uint32_t*)(stub + at) = 0; at += 4;
    tr = (uint8_t*)Hook(0x3EFB90, 13, stub);
    *(uint64_t*)(stub + at) = (uint64_t)tr;
    g_kp_buff_hooked = 1;
    Log("KIPRESS buff hook 3EFB90: bytes match, DX is the id, R8 is the actor candidate");
}
/* The only test of pulse-object +0x13C bit 0x2000. 11-byte test plus the 6-byte je. */
static struct { volatile uint32_t seq, pad; struct { volatile uint64_t comp; volatile uint32_t f13c, pad2; } slot[8]; } g_kp_flag;
static int g_kp_flag_hooked;
static void FmFlagInstall(void) {
    static const uint8_t pro[17] = {
        0x41,0xF7,0x86,0x3C,0x01,0x00,0x00,0x00,0x20,0x00,0x00,
        0x0F,0x84,0x4E,0x02,0x00,0x00};
    uint8_t* site = (uint8_t*)(B + Grva(0x2362ED7));
    uint8_t* stub; int at; DWORD old; int je_at, rel;
    if (g_kp_flag_hooked) return;
    if (memcmp(site, pro, sizeof pro)) {
        g_kp_flag_hooked = 2;
        Log("KIFLAG hook 2362ED7: bytes differ, flag log off");
        return;
    }
    stub = (uint8_t*)VirtualAlloc(0, 160, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!stub) { g_kp_flag_hooked = 2; Log("KIFLAG hook: no stub page"); return; }
    at = 0;
    stub[at++] = 0x49; stub[at++] = 0xBA; *(uint64_t*)(stub + at) = (uint64_t)&g_kp_flag; at += 8;
    stub[at++] = 0xB8; *(uint32_t*)(stub + at) = 1; at += 4;
    stub[at++] = 0xF0; stub[at++] = 0x41; stub[at++] = 0x0F; stub[at++] = 0xC1; stub[at++] = 0x02;
    stub[at++] = 0x83; stub[at++] = 0xE0; stub[at++] = 0x07;
    stub[at++] = 0xC1; stub[at++] = 0xE0; stub[at++] = 0x04;
    stub[at++] = 0x83; stub[at++] = 0xC0; stub[at++] = 0x08;
    stub[at++] = 0x4D; stub[at++] = 0x89; stub[at++] = 0x34; stub[at++] = 0x02;
    stub[at++] = 0x45; stub[at++] = 0x8B; stub[at++] = 0x9E; *(uint32_t*)(stub + at) = 0x13C; at += 4;
    stub[at++] = 0x45; stub[at++] = 0x89; stub[at++] = 0x5C; stub[at++] = 0x02; stub[at++] = 0x08;
    memcpy(stub + at, pro, 11); at += 11;
    je_at = at;
    stub[at++] = 0x0F; stub[at++] = 0x84; *(uint32_t*)(stub + at) = 0; at += 4;
    stub[at++] = 0xFF; stub[at++] = 0x25; *(uint32_t*)(stub + at) = 0; at += 4;
    *(uint64_t*)(stub + at) = (uint64_t)(site + 17); at += 8;
    rel = at - (je_at + 6);
    *(uint32_t*)(stub + je_at + 2) = (uint32_t)rel;
    stub[at++] = 0xFF; stub[at++] = 0x25; *(uint32_t*)(stub + at) = 0; at += 4;
    *(uint64_t*)(stub + at) = (uint64_t)(B + Grva(0x2363136)); at += 8;
    VirtualProtect(site, 17, PAGE_EXECUTE_READWRITE, &old);
    site[0] = 0xFF; site[1] = 0x25; *(uint32_t*)(site + 2) = 0;
    *(uint64_t*)(site + 6) = (uint64_t)stub;
    site[14] = 0x90; site[15] = 0x90; site[16] = 0x90;
    VirtualProtect(site, 17, old, &old);
    FlushInstructionCache(GetCurrentProcess(), site, 17);
    g_kp_flag_hooked = 1;
    Log("KIFLAG hook 2362ED7 on. Logs f13c bit 0x2000. It does not change a model.");
}
static struct { volatile uint32_t seq, pad; struct { volatile uint32_t kind, a, id; volatile uint64_t rcx; } slot[16]; } g_kp_x;
static int g_kp_r1_hooked, g_kp_pick_hooked;
static void (*o_kir1)(uint64_t, uint64_t);
static void (*o_kipick)(uint64_t, uint64_t);
static void KxPush(uint32_t kind, uint32_t a, uint32_t id, uint64_t rcx) {
    uint32_t i = (uint32_t)InterlockedExchangeAdd((volatile LONG*)&g_kp_x.seq, 1);
    g_kp_x.slot[i & 15].rcx = rcx;
    g_kp_x.slot[i & 15].a = a;
    g_kp_x.slot[i & 15].id = id;
    g_kp_x.slot[i & 15].kind = kind;
}
static void KiR1Enter(uint64_t rcx, uint64_t rdx) {
    KxPush(1, 0, 0, rcx);
    o_kir1(rcx, rdx);
}
static void KiPickEnter(uint64_t rcx, uint64_t rdx) {
    uint32_t bit80 = 0, id = 0;
    __try {
        uint64_t act = *(volatile uint64_t*)(rcx + 0xE0);
        uint64_t st = act ? *(volatile uint64_t*)(act + 0x3A0) : 0;
        if (st) bit80 = (*(volatile uint8_t*)(st + 0x2DC8) & 0x80) ? 1 : 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) { bit80 = 2; }
    if (bit80 == 1) id = (rdx & 0xFFu) ? 0xA605Bu : 0x73E5Au;
    KxPush(2, (bit80 << 8) | (uint32_t)(rdx & 0xFFu), id, rcx);
    o_kipick(rcx, rdx);
}
static void FmR1Install(void) {
    static const uint8_t pro[15] = {0x48,0x8B,0xC4,0x48,0x89,0x58,0x08,0x48,0x89,0x70,0x10,0x48,0x89,0x78,0x18};
    if (g_kp_r1_hooked) return;
    if (memcmp((void*)(B + Grva(0x23644F0)), pro, sizeof pro)) { g_kp_r1_hooked = 2; Log("KIR1 hook 23644F0: bytes differ"); return; }
    o_kir1 = (void (*)(uint64_t, uint64_t))Hook(0x23644F0, 15, (void*)KiR1Enter);
    g_kp_r1_hooked = 1;
    Log("KIR1 hook 23644F0 on. Buff ids still come from the 3EFB90 log.");
}
static void FmPickInstall(void) {
    static const uint8_t pro[15] = {0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xEC,0x40};
    if (g_kp_pick_hooked) return;
    if (memcmp((void*)(B + Grva(0x845148)), pro, sizeof pro)) { g_kp_pick_hooked = 2; Log("KIPICK hook 845148: bytes differ"); return; }
    o_kipick = (void (*)(uint64_t, uint64_t))Hook(0x845148, 15, (void*)KiPickEnter);
    g_kp_pick_hooked = 1;
    Log("KIPICK hook 845148 on. bit80 set picks A605B or 73E5A from the grade byte.");
}
static int FmBit19(void) {
    uint64_t ctrl = g_fp[FP_PL].valid ? g_fp[FP_PL].ctrl : 0;
    int b = -1;
    if (ctrl > 0x10000ull) {
        __try { b = (int)((*(volatile uint64_t*)(ctrl + 0x1060) >> 0x19) & 1); }
        __except (EXCEPTION_EXECUTE_HANDLER) { b = -1; }
    }
    return b;
}
// 0x3EC0F4 and 0x660B98 use the entry stack. The stolen bytes run only after the logger returns.
// 0x4CE90C does not call out. It stores every effect in a ring. See KifxDrain.
static uint64_t g_nm_start[5], g_nm_obj[5];
static int g_nm_start_hooked, g_nm_obj_hooked;
static float KiNowT74(void) {
    float t = -1.f;
    __try {
        uint64_t a = g_fp[FP_PL].valid ? g_fp[FP_PL].a : 0, st = 0;
        if (a && Rd64(a + 0x3A0, &st) && st) t = *(volatile float*)(st + 0x98 + 0x74);
    } __except (EXCEPTION_EXECUTE_HANDLER) { t = -1.f; }
    return t;
}
static void KiHex32(uint64_t p, char* out) {
    int i;
    for (i = 0; i < 32; ++i) out[i * 2] = out[i * 2 + 1] = '0';
    out[64] = 0;
    if (p < 0x10000) return;
    __try {
        for (i = 0; i < 32; ++i) sprintf(out + i * 2, "%02X", ((volatile uint8_t*)p)[i]);
    } __except (EXCEPTION_EXECUTE_HANDLER) { out[0] = 0; }
}
static void KiStartLog(uint64_t entry, uint64_t state, uint64_t r8, uint64_t r9) {
    static int lines, told, busy;
    float t74; int r1, perfect; uint32_t buff = 0, from; uint64_t owner = 0;
    (void)state; (void)r8; (void)r9;
    if (busy) return;
    busy = 1;
    from = (uint32_t)(g_nm_start[4] - B);
    t74 = KiNowT74();
    r1 = FmPadRB();
    perfect = r1 && FmKiPerfect(t74);
    __try {
        if (entry > 0x10000) { owner = *(volatile uint64_t*)(entry + 8); buff = *(volatile uint32_t*)(entry + 0x74); }
    } __except (EXCEPTION_EXECUTE_HANDLER) { owner = 0; buff = 0; }
    if (lines >= 160) { if (!told) { told = 1; Log("KISTART cap 160"); } busy = 0; return; }
    lines++;
    Log("KISTART perfect %d t74 %.2f r1 %d fx 40060 buff %u owner %llX from %X",
        perfect, t74, r1, buff, (unsigned long long)owner, from);
    busy = 0;
}
static int KiNamedSlot(uint32_t id) {
    if (id == 1000u) return 0;
    if (id == 10500u) return 1;
    if (id == 10510u) return 2;
    if (id == 0x9C7Cu) return 3;
    return -1;
}
static void KiAscii(uint64_t p, char* out, int n) {
    int i;
    out[0] = 0;
    if (p < 0x10000ull || n < 1) return;
    __try {
        for (i = 0; i < n; ++i) {
            unsigned char c = ((volatile uint8_t*)p)[i];
            if (c == 0) { out[i] = 0; return; }
            if (c < 32 || c > 126) { out[0] = 0; return; }
            out[i] = (char)c;
        }
        out[n] = 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) { out[0] = 0; }
}
static int g_nm_row_done[4];
static void KiDumpDef(uint32_t id) {
    int slot = KiNamedSlot(id), i;
    uint64_t def = 0, g = 0, table = 0, p0 = 0, p8 = 0;
    typedef uint64_t (*Fn)(uint64_t, uint32_t);
    char raw[0x68 * 2 + 1], name[32];
    uint32_t flags = 0, d20 = 0;
    int res = 0, color = 0, b34 = 0, tags[4];
    float sz = 0.f, f14 = 0.f, f18 = 0.f, f1c = 0.f;
    if (slot < 0 || g_nm_row_done[slot]) return;
    raw[0] = 0;
    name[0] = 0;
    for (i = 0; i < 4; ++i) tags[i] = 0;
    if (!Rd64(B + Grva(0x45B9E30), &g) || g < 0x10000ull || !Rd64(g + 0x30, &table) || table < 0x10000ull) return;
    __try { def = ((Fn)(B + Grva(0x1613D8)))(table, id); } __except (EXCEPTION_EXECUTE_HANDLER) { def = 0; }
    g_nm_row_done[slot] = 1;
    if (def > 0x10000ull) {
        __try {
            volatile uint8_t* b = (volatile uint8_t*)def;
            for (i = 0; i < 0x68; ++i) sprintf(raw + i * 2, "%02X", b[i]);
            raw[0x68 * 2] = 0;
            flags = *(volatile uint32_t*)(def + 0x0C);
            sz = *(volatile float*)(def + 0x10);
            f14 = *(volatile float*)(def + 0x14);
            f18 = *(volatile float*)(def + 0x18);
            f1c = *(volatile float*)(def + 0x1C);
            d20 = *(volatile uint32_t*)(def + 0x20);
            res = *(volatile int*)(def + 0x28);
            color = *(volatile int*)(def + 0x30);
            b34 = *(volatile uint8_t*)(def + 0x34);
            for (i = 0; i < 4; ++i) tags[i] = *(volatile int*)(def + 0x58 + (uint64_t)i * 4u);
            p0 = *(volatile uint64_t*)def;
            p8 = *(volatile uint64_t*)(def + 8);
        } __except (EXCEPTION_EXECUTE_HANDLER) { raw[0] = 0; }
        KiAscii(p0, name, 24);
        if (!name[0]) KiAscii(p8, name, 24);
    }
    if (!name[0]) { name[0] = '-'; name[1] = 0; }
    Log("KIROWDEF id %u def %llX flags %X sz %.3f f14 %.3f f18 %.3f f1c %.3f d20 %X res %d color %d b34 %u tag %d %d %d %d name %s raw %s",
        id, (unsigned long long)def, flags, sz, f14, f18, f1c, d20, res, color, (unsigned)b34,
        tags[0], tags[1], tags[2], tags[3], name, raw);
}
static void KiObjLog(uint64_t obj, uint64_t idreg, uint64_t r8, uint64_t r9) {
    static int lines, told, busy;
    float t74; int r1, perfect; uint32_t id, from; uint64_t row = 0; char hex[68];
    typedef uint64_t (*Fn)(uint32_t);
    (void)obj; (void)r8; (void)r9;
    if (busy) return;
    busy = 1;
    id = (uint32_t)idreg;
    from = (uint32_t)(g_nm_obj[4] - B);
    t74 = KiNowT74();
    r1 = FmPadRB();
    perfect = r1 && FmKiPerfect(t74);
    __try { row = ((Fn)(B + Grva(0x66101C)))(id); } __except (EXCEPTION_EXECUTE_HANDLER) { row = 0; }
    KiHex32(row, hex);
    if (lines >= 80) { if (!told) { told = 1; Log("KIOBJ cap 80"); } busy = 0; return; }
    lines++;
    Log("KIOBJ perfect %d t74 %.2f r1 %d id %X row %llX from %X bytes %s",
        perfect, t74, r1, id, (unsigned long long)row, from, hex);
    busy = 0;
}
static void KiCallStub(uint8_t* stub, int* at, uint64_t* save, void* fn) {
    stub[(*at)++] = 0x49; stub[(*at)++] = 0xBA; *(uint64_t*)(stub + *at) = (uint64_t)save; *at += 8;
    stub[(*at)++] = 0x49; stub[(*at)++] = 0x89; stub[(*at)++] = 0x0A;
    stub[(*at)++] = 0x49; stub[(*at)++] = 0x89; stub[(*at)++] = 0x52; stub[(*at)++] = 0x08;
    stub[(*at)++] = 0x4D; stub[(*at)++] = 0x89; stub[(*at)++] = 0x42; stub[(*at)++] = 0x10;
    stub[(*at)++] = 0x4D; stub[(*at)++] = 0x89; stub[(*at)++] = 0x4A; stub[(*at)++] = 0x18;
    stub[(*at)++] = 0x48; stub[(*at)++] = 0x8B; stub[(*at)++] = 0x04; stub[(*at)++] = 0x24;
    stub[(*at)++] = 0x49; stub[(*at)++] = 0x89; stub[(*at)++] = 0x42; stub[(*at)++] = 0x20;
    stub[(*at)++] = 0x48; stub[(*at)++] = 0x83; stub[(*at)++] = 0xEC; stub[(*at)++] = 0x68;
    stub[(*at)++] = 0x0F; stub[(*at)++] = 0x29; stub[(*at)++] = 0x44; stub[(*at)++] = 0x24; stub[(*at)++] = 0x20;
    stub[(*at)++] = 0x0F; stub[(*at)++] = 0x29; stub[(*at)++] = 0x4C; stub[(*at)++] = 0x24; stub[(*at)++] = 0x30;
    stub[(*at)++] = 0x0F; stub[(*at)++] = 0x29; stub[(*at)++] = 0x54; stub[(*at)++] = 0x24; stub[(*at)++] = 0x40;
    stub[(*at)++] = 0x0F; stub[(*at)++] = 0x29; stub[(*at)++] = 0x5C; stub[(*at)++] = 0x24; stub[(*at)++] = 0x50;
    stub[(*at)++] = 0x49; stub[(*at)++] = 0x8B; stub[(*at)++] = 0x0A;
    stub[(*at)++] = 0x49; stub[(*at)++] = 0x8B; stub[(*at)++] = 0x52; stub[(*at)++] = 0x08;
    stub[(*at)++] = 0x4D; stub[(*at)++] = 0x8B; stub[(*at)++] = 0x42; stub[(*at)++] = 0x10;
    stub[(*at)++] = 0x4D; stub[(*at)++] = 0x8B; stub[(*at)++] = 0x4A; stub[(*at)++] = 0x18;
    stub[(*at)++] = 0x48; stub[(*at)++] = 0xB8; *(uint64_t*)(stub + *at) = (uint64_t)fn; *at += 8;
    stub[(*at)++] = 0xFF; stub[(*at)++] = 0xD0;
    stub[(*at)++] = 0x0F; stub[(*at)++] = 0x28; stub[(*at)++] = 0x44; stub[(*at)++] = 0x24; stub[(*at)++] = 0x20;
    stub[(*at)++] = 0x0F; stub[(*at)++] = 0x28; stub[(*at)++] = 0x4C; stub[(*at)++] = 0x24; stub[(*at)++] = 0x30;
    stub[(*at)++] = 0x0F; stub[(*at)++] = 0x28; stub[(*at)++] = 0x54; stub[(*at)++] = 0x24; stub[(*at)++] = 0x40;
    stub[(*at)++] = 0x0F; stub[(*at)++] = 0x28; stub[(*at)++] = 0x5C; stub[(*at)++] = 0x24; stub[(*at)++] = 0x50;
    stub[(*at)++] = 0x48; stub[(*at)++] = 0x83; stub[(*at)++] = 0xC4; stub[(*at)++] = 0x68;
    stub[(*at)++] = 0x49; stub[(*at)++] = 0xBA; *(uint64_t*)(stub + *at) = (uint64_t)save; *at += 8;
    stub[(*at)++] = 0x49; stub[(*at)++] = 0x8B; stub[(*at)++] = 0x0A;
    stub[(*at)++] = 0x49; stub[(*at)++] = 0x8B; stub[(*at)++] = 0x52; stub[(*at)++] = 0x08;
    stub[(*at)++] = 0x4D; stub[(*at)++] = 0x8B; stub[(*at)++] = 0x42; stub[(*at)++] = 0x10;
    stub[(*at)++] = 0x4D; stub[(*at)++] = 0x8B; stub[(*at)++] = 0x4A; stub[(*at)++] = 0x18;
}
static void KiPatchSite(uint8_t* site, int n, uint8_t* stub) {
    DWORD old; int i;
    uint8_t p[32];
    memset(p, 0x90, sizeof p);
    p[0] = 0x48; p[1] = 0xB8; *(uint64_t*)(p + 2) = (uint64_t)stub; p[10] = 0xFF; p[11] = 0xE0;
    VirtualProtect(site, n, PAGE_EXECUTE_READWRITE, &old);
    memcpy(site, p, n);
    VirtualProtect(site, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), site, n);
    (void)i;
}
static void KiFinish(uint8_t* stub, int* at, uint8_t* site, int n, const uint8_t* pro) {
    memcpy(stub + *at, pro, n); *at += n;
    stub[(*at)++] = 0xFF; stub[(*at)++] = 0x25; *(uint32_t*)(stub + *at) = 0; *at += 4;
    *(uint64_t*)(stub + *at) = (uint64_t)(site + n);
}
typedef struct { volatile uint32_t ms, id, from, ready; volatile uint64_t rcx, sp30; } KifxSlot;
static struct { volatile uint32_t seq; uint32_t pad; KifxSlot slot[4096]; } g_kifx;
static int g_kifx_hooked, g_kifx_lines, g_kifx_told;
static struct { int live; uint32_t t0, cursor; float t74; int b19, n, capped; } g_kifx_win[8];
static void FmFxRingInstall(void) {
    static const uint8_t proD[29] = {0x48,0x8B,0xC4,0x55,0x53,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8D,0xA8,0x88,0xFC,0xFF,0xFF,0x48,0x81,0xEC,0x38,0x04,0x00,0x00};
    uint8_t* site; uint8_t* stub; int at;
    if (g_kifx_hooked || !g_cfg_kipress) return;
    site = (uint8_t*)(B + Grva(0x4CE90C));
    if (memcmp(site, proD, 29)) { g_kifx_hooked = 2; Log("KIFX hook 4CE90C: bytes differ"); return; }
    if ((char*)&g_kifx.slot[0] - (char*)&g_kifx != 8 || sizeof(g_kifx.slot[0]) != 32) { g_kifx_hooked = 2; Log("KIFX hook: slot layout differs"); return; }
    if (!(stub = (uint8_t*)VirtualAlloc(0, 512, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE))) { g_kifx_hooked = 2; Log("KIFX hook: no stub page"); return; }
    at = 0;
    stub[at++] = 0x4C; stub[at++] = 0x8B; stub[at++] = 0x1C; stub[at++] = 0x24;
    stub[at++] = 0x49; stub[at++] = 0xBA; *(uint64_t*)(stub + at) = (uint64_t)B; at += 8;
    stub[at++] = 0x4D; stub[at++] = 0x29; stub[at++] = 0xD3;
    stub[at++] = 0x41; stub[at++] = 0x53;
    stub[at++] = 0x51;
    stub[at++] = 0x52;
    stub[at++] = 0x41; stub[at++] = 0x50;
    stub[at++] = 0x41; stub[at++] = 0x51;
    stub[at++] = 0x48; stub[at++] = 0x83; stub[at++] = 0xEC; stub[at++] = 0x20;
    stub[at++] = 0x48; stub[at++] = 0xB8; *(uint64_t*)(stub + at) = (uint64_t)GetTickCount; at += 8;
    stub[at++] = 0xFF; stub[at++] = 0xD0;
    stub[at++] = 0x48; stub[at++] = 0x83; stub[at++] = 0xC4; stub[at++] = 0x20;
    stub[at++] = 0x41; stub[at++] = 0x59;
    stub[at++] = 0x41; stub[at++] = 0x58;
    stub[at++] = 0x5A;
    stub[at++] = 0x59;
    stub[at++] = 0x41; stub[at++] = 0x5B;
    stub[at++] = 0x50;
    stub[at++] = 0x49; stub[at++] = 0xBA; *(uint64_t*)(stub + at) = (uint64_t)&g_kifx; at += 8;
    stub[at++] = 0xB8; *(uint32_t*)(stub + at) = 1; at += 4;
    stub[at++] = 0xF0; stub[at++] = 0x41; stub[at++] = 0x0F; stub[at++] = 0xC1; stub[at++] = 0x02;
    stub[at++] = 0x50;
    stub[at++] = 0x25; *(uint32_t*)(stub + at) = 0x0FFF; at += 4;
    stub[at++] = 0xC1; stub[at++] = 0xE0; stub[at++] = 0x05;
    stub[at++] = 0x83; stub[at++] = 0xC0; stub[at++] = 0x08;
    stub[at++] = 0x45; stub[at++] = 0x89; stub[at++] = 0x5C; stub[at++] = 0x02; stub[at++] = 0x08;
    stub[at++] = 0x45; stub[at++] = 0x89; stub[at++] = 0x44; stub[at++] = 0x02; stub[at++] = 0x04;
    stub[at++] = 0x49; stub[at++] = 0x89; stub[at++] = 0x4C; stub[at++] = 0x02; stub[at++] = 0x10;
    stub[at++] = 0x4C; stub[at++] = 0x8B; stub[at++] = 0x5C; stub[at++] = 0x24; stub[at++] = 0x40;
    stub[at++] = 0x4D; stub[at++] = 0x89; stub[at++] = 0x5C; stub[at++] = 0x02; stub[at++] = 0x18;
    stub[at++] = 0x44; stub[at++] = 0x8B; stub[at++] = 0x5C; stub[at++] = 0x24; stub[at++] = 0x08;
    stub[at++] = 0x45; stub[at++] = 0x89; stub[at++] = 0x1C; stub[at++] = 0x02;
    stub[at++] = 0x44; stub[at++] = 0x8B; stub[at++] = 0x1C; stub[at++] = 0x24;
    stub[at++] = 0x41; stub[at++] = 0x83; stub[at++] = 0xC3; stub[at++] = 0x01;
    stub[at++] = 0x45; stub[at++] = 0x89; stub[at++] = 0x5C; stub[at++] = 0x02; stub[at++] = 0x0C;
    stub[at++] = 0x58;
    stub[at++] = 0x58;
    KiFinish(stub, &at, site, 29, proD);
    KiPatchSite(site, 29, stub); g_kifx_hooked = 1;
    Log("KIFX hook 4CE90C on. Every effect is kept. Lines run from 0.5 s before each R1 press to 1.5 s after.");
}
static const char* KifxGrade(int b19) { return b19 < 0 ? "unread" : b19 ? "perfect" : "normal"; }
static void KifxOnPress(float t74) {
    uint32_t now = GetTickCount(), seq = g_kifx.seq;
    int b19 = FmBit19(), i, slot = 0;
    for (i = 0; i < 8; ++i) if (!g_kifx_win[i].live) { slot = i; break; }
    if (i == 8) for (i = 1; i < 8; ++i) if ((int)(now - g_kifx_win[i].t0) > (int)(now - g_kifx_win[slot].t0)) slot = i;
    g_kifx_win[slot].live = 1;
    g_kifx_win[slot].t0 = now;
    g_kifx_win[slot].t74 = t74;
    g_kifx_win[slot].b19 = b19;
    g_kifx_win[slot].n = 0;
    g_kifx_win[slot].capped = 0;
    g_kifx_win[slot].cursor = seq > 4096u ? seq - 4096u : 0;
    Log("KIFX press bit19 %d t74 %.2f %s", b19, t74, KifxGrade(b19));
}
static void KifxDrain(void) {
    uint32_t now = GetTickCount(), seq = g_kifx.seq, from;
    int w;
    static int wrapTold;
    static uint32_t defSeen;
    if (g_kifx_hooked != 1) return;
    from = defSeen;
    if (seq - from > 4096u) from = seq - 4096u;
    while (from != seq) {
        KifxSlot* s = &g_kifx.slot[from & 4095u];
        if (s->ready != from + 1u) break;
        if (KiNamedSlot(s->id) >= 0) KiDumpDef(s->id);
        from++;
    }
    defSeen = from;
    for (w = 0; w < 8; ++w) {
        uint32_t guard = 0;
        if (!g_kifx_win[w].live) continue;
        if (seq - g_kifx_win[w].cursor > 4096u) {
            g_kifx_win[w].cursor = seq - 4096u;
            if (!wrapTold) { wrapTold = 1; Log("KIFX ring wrapped. Effects older than 4096 calls were dropped."); }
        }
        while (g_kifx_win[w].cursor != seq && guard < 4096u) {
            uint32_t i = g_kifx_win[w].cursor, ms, id, caller;
            uint64_t rcx, sp30;
            KifxSlot* s = &g_kifx.slot[i & 4095u];
            int dt;
            guard++;
            if (s->ready != i + 1u) break;
            ms = s->ms; id = s->id; caller = s->from; rcx = s->rcx; sp30 = s->sp30;
            g_kifx_win[w].cursor = i + 1u;
            dt = (int)(ms - g_kifx_win[w].t0);
            if (dt < -500 || dt > 1500) continue;
            if (g_kifx_win[w].n >= 400 || g_kifx_lines >= 2400) {
                if (!g_kifx_win[w].capped) { g_kifx_win[w].capped = 1; if (!g_kifx_told) { g_kifx_told = 1; Log("KIFX cap 400 a press, 2400 a run"); } }
                continue;
            }
            g_kifx_win[w].n++;
            g_kifx_lines++;
            Log("KIFX id %u from %X rcx %llX sp30 %llX dt %d bit19 %d t74 %.2f %s",
                id, caller, (unsigned long long)rcx, (unsigned long long)sp30, dt,
                g_kifx_win[w].b19, g_kifx_win[w].t74, KifxGrade(g_kifx_win[w].b19));
        }
        if ((int)(now - g_kifx_win[w].t0) > 1500) {
            Log("KIFX press done n %d bit19 %d t74 %.2f %s",
                g_kifx_win[w].n, g_kifx_win[w].b19, g_kifx_win[w].t74, KifxGrade(g_kifx_win[w].b19));
            g_kifx_win[w].live = 0;
        }
    }
}
static void FmNameInstall(void) {
    static const uint8_t proS[15] = {0x48,0x8B,0xC4,0x48,0x89,0x58,0x18,0x57,0x48,0x81,0xEC,0xD0,0x00,0x00,0x00};
    static const uint8_t proO[24] = {0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x6C,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57,0x41,0x56,0x41,0x57,0x48,0x83,0xEC,0x30};
    uint8_t* site; uint8_t* stub; int at;
    if (!g_nm_start_hooked) {
        site = (uint8_t*)(B + Grva(0x3EC0F4));
        if (memcmp(site, proS, 15)) { g_nm_start_hooked = 2; Log("KISTART hook 3EC0F4: bytes differ"); }
        else if (!(stub = (uint8_t*)VirtualAlloc(0, 512, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE))) { g_nm_start_hooked = 2; Log("KISTART hook: no stub page"); }
        else {
            at = 0; KiCallStub(stub, &at, g_nm_start, (void*)KiStartLog); KiFinish(stub, &at, site, 15, proS);
            KiPatchSite(site, 15, stub); g_nm_start_hooked = 1;
            Log("KISTART hook 3EC0F4 on. Logs buff id, owner, and perfect on the 40060 start path.");
        }
    }
    if (!g_nm_obj_hooked) {
        site = (uint8_t*)(B + Grva(0x660B98));
        if (memcmp(site, proO, 24)) { g_nm_obj_hooked = 2; Log("KIOBJ hook 660B98: bytes differ"); }
        else if (!(stub = (uint8_t*)VirtualAlloc(0, 512, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE))) { g_nm_obj_hooked = 2; Log("KIOBJ hook: no stub page"); }
        else {
            int je1, je2, jmp;
            at = 0;
            stub[at++] = 0x81; stub[at++] = 0xFA; *(uint32_t*)(stub + at) = 0xA605B; at += 4;
            je1 = at; stub[at++] = 0x0F; stub[at++] = 0x84; *(uint32_t*)(stub + at) = 0; at += 4;
            stub[at++] = 0x81; stub[at++] = 0xFA; *(uint32_t*)(stub + at) = 0x73E5A; at += 4;
            je2 = at; stub[at++] = 0x0F; stub[at++] = 0x84; *(uint32_t*)(stub + at) = 0; at += 4;
            jmp = at; stub[at++] = 0xE9; *(uint32_t*)(stub + at) = 0; at += 4;
            *(uint32_t*)(stub + je1 + 2) = (uint32_t)(at - (je1 + 6));
            *(uint32_t*)(stub + je2 + 2) = (uint32_t)(at - (je2 + 6));
            KiCallStub(stub, &at, g_nm_obj, (void*)KiObjLog);
            *(uint32_t*)(stub + jmp + 1) = (uint32_t)(at - (jmp + 5));
            KiFinish(stub, &at, site, 24, proO);
            KiPatchSite(site, 24, stub); g_nm_obj_hooked = 1;
            Log("KIOBJ hook 660B98 on. Logs A605B and 73E5A with the 66101C row.");
        }
    }
}
static void FmKiPressTick(void) {
    static KpRun run; static int lines, told; static uint32_t seen;
    float t74 = -1.f, pool = 0.f, stored = 0.f, ki = 0.f; int end80 = 0; int ok = 0;
    const uint64_t a = g_fp[FP_PL].valid ? g_fp[FP_PL].a : 0;
    uint64_t st = 0; uint32_t seq, from; int open;
    if (!g_kp_fx_hooked) FmFxInstall();
    if (!g_kp_buff_hooked) FmBuffInstall();
    if (!g_kp_flag_hooked) FmFlagInstall();
    if (!g_kp_r1_hooked) FmR1Install();
    if (!g_kp_pick_hooked) FmPickInstall();
    if (!g_kirow_hooked) FmRowInstall();
    FmNameInstall();
    FmFxRingInstall();
    if (a && Rd64(a + 0x3A0, &st) && st) {
        __try {
            t74 = *(volatile float*)(st + 0x98 + 0x74);
            pool = *(volatile float*)(st + 0x98 + 0x5C);
            stored = *(volatile float*)(st + 0x98 + 0x58);
            ki = *(volatile float*)(st + 0xAC);
            end80 = *(volatile uint8_t*)(st + 0x98 + 0x80);
            ok = 1;
        } __except (EXCEPTION_EXECUTE_HANDLER) { ok = 0; }
    }
    if (ok) {
    open = FmKiWindowOpen(t74);
    KpEv ev[4];
    const int n = KpStep(&run, open, FmPadRB(), t74, pool, stored, ki, end80, g_fp[FP_PL].run.act, ev, 4);
    g_kp_frame = run.wasOpen ? run.frame - 1 : -1;
    seq = g_kp_fx.seq; from = seen; if (seq - from > 16) from = seq - 16;
    while (from != seq && g_kp_frame >= 0) {
        uint32_t id = g_kp_fx.ring[from & 15];
        uint32_t who = g_kp_fx.callers[from & 15];
        from++;
        if (lines >= 160) { if (!told) { told = 1; Log("KIPRESS cap 160"); } break; }
        lines++; Log("KIPRESS fx %u from %X frame %d t74 %.2f", id, who, g_kp_frame, t74);
    }
    seen = seq;
    for (int i = 0; i < n; ++i) {
        const unsigned fx = g_kp_fx.last;
        const unsigned who = g_kp_fx.from;
        if (ev[i].kind == KP_R1) KifxOnPress(ev[i].t74);
        if (lines >= 160) { if (!told) { told = 1; Log("KIPRESS cap 160"); } break; }
        lines++;
        if (ev[i].kind == KP_OPEN) Log("KIPRESS open act %u", ev[i].act);
        else if (ev[i].kind == KP_R1) Log("KIPRESS r1 frame %d t74 %.2f pool %.1f stored %.1f Ki %.1f fx %u from %X", ev[i].frame, ev[i].t74, ev[i].pool, ev[i].stored, ev[i].ki, fx, who);
        else if (ev[i].kind == KP_AFTER) Log("KIPRESS after frame %d dKi %.1f pool %.1f end80 %u fx %u from %X", ev[i].frame, ev[i].dKi, ev[i].pool, (unsigned)ev[i].end80, fx, who);
        else if (ev[i].kind == KP_CLOSE) { g_kp_tail = 20; Log("KIPRESS close frame %d dKi %.1f pool %.1f end80 %u fx %u from %X", ev[i].frame, ev[i].dKi, ev[i].pool, (unsigned)ev[i].end80, fx, who); }
    }
    }
    if (g_kp_frame < 0 && g_kp_tail > 0) g_kp_tail--;
    {
        static uint32_t bseen;
        uint32_t bseq = g_kp_buff.seq, bfrom = bseen;
        const int watch = g_kp_frame >= 0 || g_kp_tail > 0;
        if (bseq - bfrom > 8) bfrom = bseq - 8;
        while (bfrom != bseq) {
            uint32_t ret = g_kp_buff.slot[bfrom & 7].ret, id = g_kp_buff.slot[bfrom & 7].id;
            uint64_t rcx = g_kp_buff.slot[bfrom & 7].rcx, r8 = g_kp_buff.slot[bfrom & 7].r8;
            uint32_t a89c = 0, c13c = 0, a13c = 0; float bt = -1.f; int got = 0;
            bfrom++;
            if (watch) {
                __try {
                    if (rcx > 0x10000ull) c13c = *(volatile uint32_t*)(rcx + 0x13C);
                    if (r8 > 0x10000ull) { a89c = *(volatile uint32_t*)(r8 + 0x89C); a13c = *(volatile uint32_t*)(r8 + 0x13C);
                        uint64_t bst = 0; if (Rd64(r8 + 0x3A0, &bst) && bst) bt = *(volatile float*)(bst + 0x98 + 0x74); }
                    got = 1;
                } __except (EXCEPTION_EXECUTE_HANDLER) { got = 0; }
                if (lines >= 160) { if (!told) { told = 1; Log("KIPRESS cap 160"); } break; }
                lines++;
                Log("KIPRESS buff id %u from %X rcx %llX r8 %llX a89c %X c13c %X a13c %X t74 %.2f %s",
                    id, ret, (unsigned long long)rcx, (unsigned long long)r8, a89c, c13c, a13c, bt,
                    !got ? "unread" : (bt > 0.05f && bt <= 4.f) ? "perfect" : (bt > 0.05f && bt <= 31.f) ? "normal" : "closed");
                if (ret == Grva(0x23645EA) || ret == Grva(0x2364652) || ret == Grva(0x8448D0)) {
                    int b19 = FmBit19();
                    if (lines < 160) {
                        lines++;
                        Log("KIR1 buff id %u from %X bit19 %d t74 %.2f %s",
                            id, ret, b19, bt, b19 < 0 ? "unread" : b19 ? "perfect" : "normal");
                    }
                }
            }
        }
        bseen = bseq;
    }
    {
        static uint32_t fseen;
        uint32_t fseq = g_kp_flag.seq, ffrom = fseen;
        int b19 = FmBit19();
        if (fseq - ffrom > 8) ffrom = fseq - 8;
        while (ffrom != fseq) {
            uint64_t comp = g_kp_flag.slot[ffrom & 7].comp;
            uint32_t f13c = g_kp_flag.slot[ffrom & 7].f13c;
            const int bit = (f13c & 0x2000) != 0;
            const int watch = g_kp_frame >= 0 || g_kp_tail > 0 || (t74 > 0.05f && t74 <= 31.f) || bit;
            ffrom++;
            if (!watch) continue;
            if (lines >= 160) { if (!told) { told = 1; Log("KIPRESS cap 160"); } break; }
            lines++;
            Log("KIFLAG f13c %X bit2000 %d comp %llX bit19 %d t74 %.2f %s",
                f13c, bit, (unsigned long long)comp, b19, t74,
                b19 < 0 ? "unread" : b19 ? "perfect" : "normal");
        }
        fseen = fseq;
    }
    {
        static uint32_t xseen;
        uint32_t xseq = g_kp_x.seq, xfrom = xseen;
        int b19 = FmBit19();
        const int watch = g_kp_frame >= 0 || g_kp_tail > 0 || (t74 > 0.05f && t74 <= 31.f);
        if (xseq - xfrom > 16) xfrom = xseq - 16;
        while (xfrom != xseq) {
            uint32_t kind = g_kp_x.slot[xfrom & 15].kind;
            uint32_t xa = g_kp_x.slot[xfrom & 15].a;
            uint32_t id = g_kp_x.slot[xfrom & 15].id;
            uint64_t rcx = g_kp_x.slot[xfrom & 15].rcx;
            xfrom++;
            if (!watch) continue;
            if (lines >= 160) { if (!told) { told = 1; Log("KIPRESS cap 160"); } break; }
            lines++;
            if (kind == 1)
                Log("KIR1 call 23644F0 rcx %llX bit19 %d t74 %.2f %s",
                    (unsigned long long)rcx, b19, t74, b19 < 0 ? "unread" : b19 ? "perfect" : "normal");
            else
                Log("KIPICK grade %u bit80 %u id %X rcx %llX bit19 %d t74 %.2f %s",
                    xa & 0xFFu, xa >> 8, id, (unsigned long long)rcx, b19, t74,
                    (xa >> 8) == 2 ? "unread" : (xa >> 8) != 1 ? "skip" : b19 < 0 ? "unread" : b19 ? "perfect" : "normal");
        }
        xseen = xseq;
    }
}
/* One dump of the 7 buff records. Map pointer is *(T+0x20), not the inline address. */
static void FmBuffTableDump(void) {
    static int done;
    static const uint16_t ids[7] = {900, 822, 496, 2, 0x15E, 0x1B2, 0x114};
    uint64_t G = 0, T = 0, map = 0, begin = 0, end = 0, arr = 0;
    uint32_t count = 0;
    int nent, n;
    if (done) return;
    if (!Rd64(B + Grva(0x45B9E30), &G) || G < 0x10000ull) return;
    if (!Rd64(G + 0x7A8, &T) || T < 0x10000ull) return;
    __try {
        map = *(volatile uint64_t*)(T + 0x20);
        arr = *(volatile uint64_t*)T;
        if (map < 0x10000ull || arr < 0x10000ull) return;
        begin = *(volatile uint64_t*)(map + 8);
        end = *(volatile uint64_t*)(map + 0x10);
        count = *(volatile uint32_t*)(arr + 4);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
    if (begin < 0x10000ull || end < begin) return;
    nent = (int)((end - begin) / 8);
    if (nent <= 0 || nent > 200000) { done = 1; Log("KIBUFF table unreadable entries %d", nent); return; }
    done = 1;
    Log("KIBUFF table count %u entries %d", count, nent);
    for (n = 0; n < 7; ++n) {
        uint32_t idx = 0xFFFFFFFFu;
        uint16_t kind = 0;
        uint8_t raw[0x2C];
        char hex[0x2C * 2 + 1];
        int e, h;
        memset(raw, 0, sizeof raw);
        __try {
            for (e = 0; e < nent; ++e) {
                uint16_t key = *(volatile uint16_t*)(begin + (uint64_t)e * 8u);
                if (key == ids[n]) { idx = *(volatile uint32_t*)(begin + (uint64_t)e * 8u + 4); break; }
            }
            if (idx < count) {
                uint8_t* rec = (uint8_t*)(arr + 8 + (uint64_t)idx * 0x2C);
                for (h = 0; h < 0x2C; ++h) raw[h] = rec[h];
                kind = *(volatile uint16_t*)(rec + 0x2A);
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) { idx = 0xFFFFFFFEu; }
        for (h = 0; h < 0x2C; ++h) sprintf(hex + h * 2, "%02X", raw[h]);
        hex[0x2C * 2] = 0;
        Log("KIBUFF id %u idx %u kind %u raw %s", (unsigned)ids[n], idx, (unsigned)kind, hex);
    }
}
/* Arm the effect hooks even when KiPressProbe is off. The last run logged KIROW and never logged KIEC. */
static void FmArmHooks(void) {
    static int once;
    if (once) return;
    once = 1;
    Log("KIARM switch %d. KISTART, KIOBJ, KIPICK, and KIR1 arm either way. KIFX arms only when the switch is on.", g_cfg_kipress);
    FmNameInstall();
    FmPickInstall();
    FmR1Install();
}
// FpTick (game thread, FpLock held), once per pump tick.
static void FmFeed(double gdt) {
    const int wantB = g_pst.v[PS_FRAME_METER], wantP = g_pst.v[PS_PLAYER_METER] || g_pst.v[PS_KI_WINDOW];
    const int probe = g_cfg_ifp != 0 || g_cfg_ifbyte != 0;
    const int kpress = g_cfg_kipress != 0;
    FmArmHooks();
    const int scope = FmScope(wantB, wantP, g_pst_live != 0);
    { static int pScope = -1; if (scope != pScope) { pScope = scope; Log("FRAME METER: scope %d (0 off, 1 session, 2 player row with no boss)", scope); } }
    if (scope == 0 && !probe && !kpress) { g_fm_show = 0; g_fmp_show = 0; g_fm_ki_show = 0; g_fm_slot = -1; g_fm_last = 0; return; }
    if (scope && !g_fm_thread && InterlockedExchange(&g_fm_thread, 1) == 0) { HANDLE t = CreateThread(0, 0, FmThread, 0, 0, 0); if (t) CloseHandle(t); else Log("FRAME METER: draw thread failed (%lu)", GetLastError()); }
    if (probe && !g_pst_live) { static int told; if (!told) { told = 1; Log("IFRAMEPR: logging in the field with no Practice session"); } }
    double dt = gdt;
    if (dt < 0) dt = 0; if (dt > 150) dt = 150;
    const uint8_t* gs = GameState();
    if (((g_menu_frozen || g_dbg_frozen) && !g_fz_over) || (gs && gs[2])) dt = 0;  // a Practice frame step runs one frame through every stop request
    g_fm_clock += dt;
    // player bar (user #22796): same sampler, same clock, fed from the player's g_fp entry
    if (!wantP) g_fmp_show = 0;
    if (probe && !g_fp[FP_PL].valid) { static int toldP; if (!toldP) { toldP = 1; Log("IFRAMEPR: probe on, no player actor yet"); } }
    if (!g_kirow_hooked) FmRowInstall();
    if (kpress) FmNameInstall();
    if (kpress) FmFxRingInstall();
    if (kpress) FmBuffTableDump();
    if (kpress && g_fp[FP_PL].valid) FmKiPressTick();
    if (kpress) KifxDrain();
    if ((wantP || probe) && g_fp[FP_PL].valid) {
        static FmiState fmiS;
        int active = g_fp[FP_PL].pulse, ub = g_fp[FP_PL].pulseUb, grab = g_fp[FP_PL].pulseGrab;
        for (int w = 0; w < FP_WIN; ++w) if (g_fpw[w].used && g_fpw[w].slot == FP_PL && g_fpw[w].act == g_fp[FP_PL].run.act) { active = 1; if (g_fpw[w].great) ub = 1; if (g_fpw[w].grab) grab = 1; }
        AcquireSRWLockExclusive(&g_fm_lock);
        FmSampleEx(&g_fmp, g_fm_clock, g_fp[FP_PL].run.act, active, ub, grab);
        {
            static uint64_t hbActor, hbActor2; static int hbBase, hbM18;  // hurtbox count seen outside i-frames for this actor (max seen); attack bits seen in its masks
            FmHurt h; const int hn = FmHurtScan(g_fp[FP_PL].a, &h);
            if (g_fp[FP_PL].a != hbActor) { hbActor = g_fp[FP_PL].a; hbBase = 0; hbM18 = 0; }
            if (hn > hbBase && !h.off) hbBase = hn;
            if (hn > 0 && (h.m18or & 0x440003Full)) hbM18 = 1;
            const int hoff = FmHurtOff(&h, hn, hbBase, hbM18);
            static uint32_t seenOff;  // #35411 predicate bits seen OFF for this actor (FmiStripe)
            if (g_fp[FP_PL].a != hbActor2) { hbActor2 = g_fp[FP_PL].a; seenOff = 0; }
            FmiEval(g_fp[FP_PL].a, g_fp[FP_PL].ctrl, hoff, &fmiS);
            const int pred = fmiS.ok && FmiStripe(fmiS.mask, &seenOff);
            FmMarkNoDmg(&g_fmp, g_fm_clock, ((FmPlayerNoDmg(g_fp[FP_PL].a, g_fp[FP_PL].ctrl) || hoff || pred) ? FMF_NODMG : 0) | FmPlayerFlagBits(g_fp[FP_PL].ctrl));
        }
        if (g_cfg_ifp) FmIFrameProbe(g_fp[FP_PL].a, g_fp[FP_PL].ctrl, g_fp[FP_PL].run.act, g_fm_clock, &fmiS);
        if (g_cfg_ifbyte) FmPadLog(g_fm_clock);
        if (g_cfg_ifbyte) FmIFrameBytes(g_fp[FP_PL].a, g_fp[FP_PL].ctrl, g_fp[FP_PL].run.act, g_fm_clock);
        {  // #38492/#38843: the Ki window is always sampled for the PLAYER row so the perfect part (teal) shows; KiWindow adds the whole blue bar
            static int wasOpen;
            float pkt = 0.f, pki = 0.f, pt74 = -1.f;
            const int open = FmPlayerKiOpen(g_fp[FP_PL].a);
            int landed = FmPlayerKiLanded(g_fp[FP_PL].a, &pkt, &pki, &pt74);
            const int edge = open && !wasOpen;
            wasOpen = open;
            static int wasRB;
            const int rb = FmPadRB();
            const int r1Edge = rb && !wasRB;
            wasRB = rb;
            static int prevPerfWin;
            // The press often ends the window on the same tick, so the previous sample's perfect window counts too.
            const int perfect = FmKiPressPerfect(open, r1Edge, pt74) || (r1Edge && prevPerfWin);
            const int perfWin = open && FmKiPerfect(pt74);  // #38492: the perfect window itself (timer in (0.05, 4]), press or not
            FmMarkKiWin(&g_fmp, g_fm_clock, open ? (perfect || perfWin ? 2 : 1) : 0);
            if ((perfect || perfWin) && !FmLiveRun(&g_fmp) && FmSealPerfectEnd(&g_fmp))
                Log("KI cyan: %s at timer %.2f sealed the last cell", perfect ? "R1" : "window", pt74);
            prevPerfWin = open && FmKiPerfect(pt74);
            if (perfect) {  // a perfect press shows the check on the PLAYER row at that moment (frozen bar: on its last open cell)
                landed = 1;
                if (!FmLiveRun(&g_fmp)) FmSealPerfectCheck(&g_fmp);
            }
            if (!landed && edge && FmKiPulseRecent(&g_fmp, FmCellOf(g_fm_clock), 45)) landed = 1;
            FmMarkKiPulse(&g_fmp, g_fm_clock, landed);
            {
                static uint32_t prevAct;
                static float prevKt, prevKi;
                static int prevBlue, sampleLogs, capTold;
                const uint32_t act = g_fp[FP_PL].run.act;
                const int jump = pkt > prevKt + 1.f || pkt < prevKt - 1.f || pki > prevKi + 15.f || pki < prevKi - 15.f;
                const int watch = (act >= 6200u && act <= 6299u) || pkt > 0.f || open;
                if (watch && (act != prevAct || jump || open != prevBlue)) {
                    if (sampleLogs < 400) {
                        sampleLogs++;
                        Log("KI PULSE sample act %u timer %.1f window %.2f Ki %.1f blue %d check %d", act, pkt, pt74, pki, open, landed);
                    } else if (!capTold) {
                        capTold = 1;
                        Log("KI PULSE sample cap 400");
                    }
                }
                prevAct = act; prevKt = pkt; prevKi = pki; prevBlue = open;
            }
        }
        ReleaseSRWLockExclusive(&g_fm_lock);
        g_fmp_show = 1;
    }
    g_fp[FP_PL].pulse = g_fp[FP_PL].pulseUb = g_fp[FP_PL].pulseGrab = 0;
    if (!wantB || scope == 2) { g_fm_ki_show = 0; g_fm_show = 0; g_fm_slot = -1; for (int i = 0; i < N3T_SLOTS; ++i) g_fp[i].pulse = g_fp[i].pulseUb = g_fp[i].pulseGrab = 0; InterlockedIncrement(&g_fm_ver); return; }
    int valid[N3T_SLOTS], busy[N3T_SLOTS];
    for (int i = 0; i < N3T_SLOTS; ++i) {
        valid[i] = g_fp[i].valid; busy[i] = g_fp[i].valid && (g_fp[i].pulse || g_fp[i].pulseUb);
        for (int w = 0; w < FP_WIN && valid[i] && !busy[i]; ++w) if (g_fpw[w].used && g_fpw[w].slot == i && g_fpw[w].act == g_fp[i].run.act) busy[i] = 1;
    }
    const int s = FmPickSlot(g_fm_slot, N3T_SLOTS, valid, busy);  // user #22918: the boss that is attacking, not always the first slot
    if (s < 0) { g_fm_ki_show = 0; g_fm_slot=-1; for (int i = 0; i < N3T_SLOTS; ++i) g_fp[i].pulse = g_fp[i].pulseUb = g_fp[i].pulseGrab = 0; InterlockedIncrement(&g_fm_ver); return; }  // no boss: the last picture stays
    int active = g_fp[s].pulse, ub = g_fp[s].pulseUb, grab = g_fp[s].pulseGrab;
    for (int i = 0; i < N3T_SLOTS; ++i) g_fp[i].pulse = g_fp[i].pulseUb = g_fp[i].pulseGrab = 0;
    // an open window of this boss is active only while it belongs to the CURRENT action: a window opened by the previous action (its hitbox still
    // open when the action id changes) is closed for the meter, so an idle/move action after an attack gets no fake active/recovery run
    for (int w = 0; w < FP_WIN; ++w) if (g_fpw[w].used && g_fpw[w].slot == s && g_fpw[w].act == g_fp[s].run.act) { active = 1; if (g_fpw[w].great) ub = 1; if (g_fpw[w].grab) grab = 1; }
    AcquireSRWLockExclusive(&g_fm_lock);
    if (s != g_fm_slot || g_fp[s].a != g_fm_actor || g_fp[s].chr != g_fm_chr) {  // another slot, or a new boss (retry / respawn / swap) in the same slot: fresh meter
        FmReset(&g_fm); FmKiReset(&g_fm_ki); g_fm_ki_show=0; g_fm_slot = s; g_fm_actor = g_fp[s].a; g_fm_chr=g_fp[s].chr;
        FmWho(s, g_fp[s].chr, g_fm_who, sizeof g_fm_who);
        Log("FRAME METER: following %s (chr %u)", g_fm_who, g_fp[s].chr);
    }
    float kiCur=0,kiMax=0,kiSecondary=0; int kiDead=0;
    const int kiRead=g_pst.v[PS_FRAME_KI] && NkmRead(g_fp[s].a,&kiCur,&kiMax,&kiSecondary,&kiDead);
    const int kiNegative=kiRead && !kiDead && NkmNeg(kiCur,kiSecondary,0)<0.0f;
    g_fm_ki_show=FmKiEligible(g_pst.v[PS_FRAME_KI],FmHumanBoss(g_fp[s].a,g_fp[s].chr),kiRead,kiDead,kiCur,kiSecondary);
    FmKiSample(&g_fm_ki,&g_fm,g_fm_clock,g_fp[s].run.act,kiNegative);
    FmSampleEx(&g_fm, g_fm_clock, g_fp[s].run.act, active, ub, grab);
    ReleaseSRWLockExclusive(&g_fm_lock);
    InterlockedIncrement(&g_fm_ver); g_fm_show = 1;
}
// ---- drawing (own thread; offline-testable) ----
#include "frame_meter_draw.h"
static BOOL CALLBACK FmFind(HWND h, LPARAM lp) { (void)lp;
    DWORD pid; GetWindowThreadProcessId(h, &pid); if (pid != GetCurrentProcessId() || !IsWindowVisible(h) || h == g_fm_wnd || h == g_fd_wnd || GetWindow(h, GW_OWNER)) return TRUE;
    RECT r; GetClientRect(h, &r); if (r.right < 400 || r.bottom < 300) return TRUE;
    g_fm_game = h; return FALSE;
}
static int g_fm_H;  // game client height (font / bar scale)
// #38961: per-pixel alpha. Draw with GDI into a 32-bit DIB, then set alpha per pixel: colour key = clear, panel background = the
// mod-panel translucency (menu_style kPanelBg 0x73), everything else (bars, text) opaque.
static void FmRender(HWND wnd) {
    LARGE_INTEGER begin={0},painted={0};
    if(g_cfg_fm_drawcost)QueryPerformanceCounter(&begin);
    RECT r; GetClientRect(wnd, &r);
    const int w = r.right, h = r.bottom;
    if (w <= 0 || h <= 0) return;
    HDC scr = GetDC(0), mem = CreateCompatibleDC(scr);
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    uint32_t* px = 0;
    HBITMAP bmp = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, (void**)&px, 0, 0);
    if (bmp && px) {
        HGDIOBJ ob = SelectObject(mem, bmp);
        n3_stroke_layered=1;
        __try { FmPaint(mem, w, h, g_fm_H); } __except (EXCEPTION_EXECUTE_HANDLER) { }
        n3_stroke_layered=0;
        GdiFlush();
        FmFinalizeAlpha(px,w,h);
        if(g_cfg_fm_drawcost)QueryPerformanceCounter(&painted);
        POINT src = {0, 0}; SIZE sz = {w, h};
        BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        UpdateLayeredWindow(wnd, scr, 0, &sz, mem, &src, 0, &bf, ULW_ALPHA);
        SelectObject(mem, ob);
    }
    if (bmp) DeleteObject(bmp);
    DeleteDC(mem); ReleaseDC(0, scr);
    if(g_cfg_fm_drawcost && painted.QuadPart) {
        LARGE_INTEGER end,freq;QueryPerformanceCounter(&end);QueryPerformanceFrequency(&freq);
        const int style=g_pst.v[PS_FRAME_STYLE]?1:0;
        static unsigned count[2];static double total[2],paint[2],peak[2];
        const double elapsed=1000.*(end.QuadPart-begin.QuadPart)/freq.QuadPart;
        ++count[style];total[style]+=elapsed;paint[style]+=1000.*(painted.QuadPart-begin.QuadPart)/freq.QuadPart;
        if(elapsed>peak[style])peak[style]=elapsed;
        if(count[style]==1 || count[style]%30==0)
            Log("FRAME METER DRAW %s %dx%d n %u paint %.3f total %.3f peak %.3f ms (CPU incl layered upload; not FPS)",
                style?"Ring":"Row",w,h,count[style],paint[style]/count[style],total[style]/count[style],peak[style]);
    }
}
static LRESULT CALLBACK FmProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCHITTEST) return HTTRANSPARENT;
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps; BeginPaint(h, &ps); EndPaint(h, &ps); return 0;  // drawn by FmRender (UpdateLayeredWindow)
    }
    return DefWindowProcW(h, msg, wp, lp);
}
static DWORD WINAPI FmThread(LPVOID p) {
    (void)p;
    WNDCLASSEXW wc = {sizeof wc}; wc.lpfnWndProc = FmProc; wc.hInstance = GetModuleHandleW(0); wc.lpszClassName = L"Nioh3TrainingFrameMeter";
    RegisterClassExW(&wc);
    g_fm_wnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, wc.lpszClassName, L"", WS_POPUP, 0, 0, 100, 100, 0, 0, wc.hInstance, 0);
    if (!g_fm_wnd) { Log("FRAME METER: overlay window failed (%lu), meter not drawn", GetLastError()); return 0; }
    Log("FRAME METER: overlay window ready");
    int shown = 0; LONG lastVer = -1; RECT last = {0}; PstCfg lastCfg = {0};
    for (;;) {
        Sleep(33);
        MSG msg; while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        if (!g_fm_game || !IsWindow(g_fm_game)) { g_fm_game = 0; EnumWindows(FmFind, 0); }
        const int have = g_fm_show || g_fmp_show;
        const int gameOk = g_fm_game && !IsIconic(g_fm_game);
        const int front = gameOk && GetForegroundWindow() == g_fm_game;
        const int fg = front || (g_cfg_fm_unfocused && gameOk);
        const int menu = GateMenuOpen();
        const uint8_t* lgs = GameState(); const int loading = lgs && lgs[2];  // #38144: no meter over a loading screen
        const int scope = FmScope(g_pst.v[PS_FRAME_METER], g_pst.v[PS_PLAYER_METER] || g_pst.v[PS_KI_WINDOW], g_pst_live != 0);
        const int want = scope && FmPanels() && gameOk && fg && !loading;  // #38920: stays visible while the mod menu is open
        if (have) {
            static int toldOn, toldWhy;
            if (want && !toldOn) { toldOn = 1; Log("FRAME METER: on screen"); }
            else if (!want && !toldWhy) {
                toldWhy = 1;
                Log("FRAME METER: hidden live %d panels %d game %d foreground %d menu %d unfocused %d", (int)g_pst_live, FmPanels(), gameOk ? 1 : 0, front ? 1 : 0, menu ? 1 : 0, g_cfg_fm_unfocused);
            }
        }
        if (!want) { if (shown) { ShowWindow(g_fm_wnd, SW_HIDE); shown = 0; } continue; }
        RECT cr; GetClientRect(g_fm_game, &cr); POINT o = {0, 0}; ClientToScreen(g_fm_game, &o);
        int cw, barH, lineH, pad; g_fm_H = cr.bottom;
        const int rowH = FmGeom(cr.right, cr.bottom, &cw, &barH, &lineH, &pad);
        const int hh = FmDrawHeight(cr.right,cr.bottom);
        (void)rowH;
        const int top = o.y + FmPositionY(cr.bottom, hh, g_cfg_fm_y);
        const int left = o.x + (g_cfg_fm_y < 0 ? FmPositionX(cr.right) : 0);
        RECT want_r = {left, top, left + cr.right, top + hh};
        if (!shown || memcmp(&want_r, &last, sizeof last)) {
            SetWindowPos(g_fm_wnd, HWND_TOPMOST, want_r.left, want_r.top, want_r.right - want_r.left, hh, SWP_NOACTIVATE | SWP_SHOWWINDOW);
            last = want_r; shown = 1; lastVer = -1;
        }
        const LONG v = g_fm_ver;
        if (v != lastVer || memcmp(&lastCfg, &g_pst, sizeof lastCfg)) {
            lastVer = v; lastCfg = g_pst; FmRender(g_fm_wnd);
        }
    }
}
