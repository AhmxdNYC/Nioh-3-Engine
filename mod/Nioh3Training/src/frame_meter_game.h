// Practice frame meter (SF6 style), game side. Included by training.c after frame_probe_game.h. State machine: frame_meter.h (tests/frame_meter_test.c).
// Input: the FRAMEPROBE signals only (frame_probe_game.h): the boss action id seen by the pump, open hit windows (attack collision still in its pool
// slot) and hits created since the last tick, with the GreatAttack bit (hit +0x13C bit 10 = unblockable, notes/yoritomo-hit-source-22305.md).
// No game memory is read or written beyond what the probe already does. Generic: the first live Training boss slot, no per-boss table.
// Clock: GAME time (user #22575): the game's own step per update tick from FpGameStep (frame_probe_game.h), so one cell is 1/60 s of game time at
// any frame rate; it also stops while the mod menu pauses the game (MENU PAUSE), the F9 freeze or a load.
// Drawing: a click-through topmost layered GDI window over the game (the same fallback renderer as the Rematch standalone menu and the fade window),
// so no Present hook and no other ASI is involved. Hidden while the mod menu is open, the game is not the foreground window or no data yet.
// Ini (Nioh3Training.ini [Practice]): FrameMeter = 0/1 (default 0, the Practice hub row 'Frame meter'), FrameMeterY = top of the meter in % of the
// screen height; -1 (default) anchors above the lower player HUD, slightly left.
#include "frame_meter.h"
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
static int g_cfg_ifp;  // [Debug] IFrameProbe 1: log-only probe of the player's state bits (notes/iframes.md)
static void FmLoadIni(const char* ini) {
    g_cfg_ifp = (int)GetPrivateProfileIntA("Debug", "IFrameProbe", 0, ini);
    g_cfg_fm_y = (int)GetPrivateProfileIntA("Practice", "FrameMeterY", -1, ini); if (g_cfg_fm_y < -1 || g_cfg_fm_y > 90) g_cfg_fm_y = -1;
    Log("FRAME METER: [Practice] FrameMeter %d, PlayerFrameMeter %d, KiWindow %d, FrameMeterY %d (-1 = above player HUD; otherwise top percent)", g_pst.v[PS_FRAME_METER], g_pst.v[PS_PLAYER_METER], g_pst.v[PS_KI_WINDOW], g_cfg_fm_y);
}
// Player KiTick K+0x74 (status = [actor+0x3A0], K = status+0x98). Read only. 1 while the 30-tick window is counting down.
static __attribute__((noinline)) int FmPlayerKiOpen(uint64_t actor) {
    uint64_t st = 0; float kt = -1.f;
    if (!actor || !Rd64((uintptr_t)actor + 0x3A0, &st) || !st) return 0;
    __try { kt = *(volatile float*)(st + 0x98 + 0x74); } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    return FmKiWindowOpen(kt);
}
// KiTick +0x5C and current Ki. Read only. 1 on the tick a pulse lands (see FmKiPulseStep).
// v2: window K+0x74, pool K+0x5C, Ki status+0xAC (see FmKiPulseStep2).
static __attribute__((noinline)) int FmPlayerKiLanded(uint64_t actor, float* ktOut, float* kiOut, float* t74Out) {
    static uint64_t who = 0;
    static FmPulseWatch w;
    uint64_t st = 0;
    float kt = 0.f, ki = 0.f, t74 = -1.f, pt74, ppool, pki;
    int landed = 0;
    if (!actor || actor != who) { who = actor; memset(&w, 0, sizeof w); }
    if (ktOut) *ktOut = 0.f;
    if (kiOut) *kiOut = 0.f;
    if (t74Out) *t74Out = -1.f;
    if (!actor || !Rd64((uintptr_t)actor + 0x3A0, &st) || !st) { memset(&w, 0, sizeof w); return 0; }
    __try {
        kt = *(volatile float*)(st + 0x98 + 0x5C);
        t74 = *(volatile float*)(st + 0x98 + 0x74);
        ki = *(volatile float*)(st + 0xAC);
    } __except (EXCEPTION_EXECUTE_HANDLER) { memset(&w, 0, sizeof w); return 0; }
    if (ktOut) *ktOut = kt;
    if (kiOut) *kiOut = ki;
    if (t74Out) *t74Out = t74;
    pt74 = w.t74; ppool = w.pool; pki = w.ki;
    landed = FmKiPulseStep2(&w, t74, kt, ki);
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
// #35176 hurtbox side. The collision pool [45C6F38]+0x20+i*8 (0xFA0 slots) holds attack and damage (hurtbox) collision objects:
// obj+0 = shape (shape+0x10 = category mask, damage = 0x3FFFC0, attack = 0x440003F), obj+8 = owner actor, obj+0xD4 flags, obj+0xD8 part,
// obj+0xB0 = def (def+0x14 = type). The contact handler 117A6D0 never hits a damage collision whose def type is 0x17 or 0x19, and an object
// that is not in the pool cannot be touched at all. Read only. Fills a summary of the player's damage collisions; returns their count (-1 = no pool).
typedef struct { int n, off; uint32_t d4or, d4and, types; uint64_t m18or; char sig[480]; } FmHurt;
static __attribute__((noinline)) int FmHurtScan(uint64_t actor, FmHurt* h) {
    uint64_t pool = 0; int o = 0;
    memset(h, 0, sizeof *h); h->d4and = 0xFFFFFFFFu;
    if (!actor || !Rd64(B + 0x45C6F38, &pool) || !pool) return -1;
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
// [Debug] IFrameProbe 1: one log line whenever the player's action id or a candidate state word changes (ctrl+0xA8 / +0x1060 flag qwords,
// the ctrl+0xEA0..0xEB2 state bytes, actor+0xABC0 / +0xADB1). Dodge in Practice, then read which bit follows the dodge window. Capped.
static __attribute__((noinline)) void FmIFrameProbe(uint64_t actor, uint64_t ctrl, uint32_t act, double t) {
    static uint64_t pa8, p1060, pb0, pb1, pb2; static uint32_t pact; static int lines, told, hlines; static char psig[480];
    uint64_t a8 = 0, x1060 = 0, b0 = 0, b1 = 0, b2 = 0;
    if (!g_cfg_ifp || !actor || !ctrl) return;
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
        if (act >= 10000 && act < 10200 && stlines < 2500 && Rd64(actor + 0x3A0, &st) && st) {
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
        if (act >= 10000 && act < 10200 && obLines < 2500 && Rd64(B + 0x45C6F38, &pool) && pool) {
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
// FpTick (game thread, FpLock held), once per pump tick.
static void FmFeed(double gdt) {
    const int wantB = g_pst.v[PS_FRAME_METER], wantP = g_pst.v[PS_PLAYER_METER] || g_pst.v[PS_KI_WINDOW];
    if ((!wantB && !wantP) || !g_pst_live) { g_fm_show = 0; g_fmp_show = 0; g_fm_ki_show = 0; g_fm_slot = -1; g_fm_last = 0; return; }
    if (!g_fm_thread && InterlockedExchange(&g_fm_thread, 1) == 0) { HANDLE t = CreateThread(0, 0, FmThread, 0, 0, 0); if (t) CloseHandle(t); else Log("FRAME METER: draw thread failed (%lu)", GetLastError()); }
    double dt = gdt;
    if (dt < 0) dt = 0; if (dt > 150) dt = 150;
    const uint8_t* gs = GameState();
    if (((g_menu_frozen || g_dbg_frozen) && !g_fz_over) || (gs && gs[2])) dt = 0;  // a Practice frame step runs one frame through every stop request
    g_fm_clock += dt;
    // player bar (user #22796): same sampler, same clock, fed from the player's g_fp entry
    if (!wantP) g_fmp_show = 0;
    else if (g_fp[FP_PL].valid) {
        int active = g_fp[FP_PL].pulse, ub = g_fp[FP_PL].pulseUb, grab = g_fp[FP_PL].pulseGrab;
        for (int w = 0; w < FP_WIN; ++w) if (g_fpw[w].used && g_fpw[w].slot == FP_PL && g_fpw[w].act == g_fp[FP_PL].run.act) { active = 1; if (g_fpw[w].great) ub = 1; if (g_fpw[w].grab) grab = 1; }
        AcquireSRWLockExclusive(&g_fm_lock);
        FmSampleEx(&g_fmp, g_fm_clock, g_fp[FP_PL].run.act, active, ub, grab);
        {
            static uint64_t hbActor; static int hbBase, hbM18;  // hurtbox count seen outside i-frames for this actor (max seen); attack bits seen in its masks
            FmHurt h; const int hn = FmHurtScan(g_fp[FP_PL].a, &h);
            if (g_fp[FP_PL].a != hbActor) { hbActor = g_fp[FP_PL].a; hbBase = 0; hbM18 = 0; }
            if (hn > hbBase && !h.off) hbBase = hn;
            if (hn > 0 && (h.m18or & 0x440003Full)) hbM18 = 1;
            FmMarkNoDmg(&g_fmp, g_fm_clock, FmPlayerNoDmg(g_fp[FP_PL].a, g_fp[FP_PL].ctrl) || FmHurtOff(&h, hn, hbBase, hbM18));
        }
        FmIFrameProbe(g_fp[FP_PL].a, g_fp[FP_PL].ctrl, g_fp[FP_PL].run.act, g_fm_clock);
        if (g_pst.v[PS_KI_WINDOW]) {
            static int wasOpen;
            float pkt = 0.f, pki = 0.f, pt74 = -1.f;
            const int open = FmPlayerKiOpen(g_fp[FP_PL].a);
            int landed = FmPlayerKiLanded(g_fp[FP_PL].a, &pkt, &pki, &pt74);
            const int edge = open && !wasOpen;
            wasOpen = open;
            FmMarkKiWin(&g_fmp, g_fm_clock, open);
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
    if (!wantB) { g_fm_ki_show = 0; g_fm_show = 0; g_fm_slot = -1; for (int i = 0; i < N3T_SLOTS; ++i) g_fp[i].pulse = g_fp[i].pulseUb = g_fp[i].pulseGrab = 0; InterlockedIncrement(&g_fm_ver); return; }
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
// ---- drawing (own thread) ----
#include "frame_meter_count.h"
#define FM_DISPLAY_CELLS 120 // SF6-sized pips; retain the full 360-cell sampler history.
#define FM_KEY RGB(255, 0, 255)
static HFONT g_fm_font; static int g_fm_fontPx;
static COLORREF FmColor(uint8_t st) {
    switch (st) {
    case FM_NEUTRAL: return RGB(9,12,17);
    case FM_PENDING: return RGB(105,117,134);
    case FM_STARTUP: return RGB(80,210,72);
    case FM_ACTIVE: return RGB(255,142,91);
    case FM_ACTIVE_UB: return RGB(155,18,39);
    case FM_GRAB: return RGB(205,210,220);
    case FM_RECOVERY: return RGB(35,157,220);
    default: return RGB(5,7,10);
    }
}
static void FmFill(HDC dc, int x, int y, int w, int h, COLORREF c) { RECT r = {x, y, x + w, y + h}; HBRUSH b = CreateSolidBrush(c); FillRect(dc, &r, b); DeleteObject(b); }
static int FmGeom(int W, int H, int* cw, int* barH, int* lineH, int* pad) {
    const float k = H > 0 ? (float)H / 1080.0f : 1.0f;
    *lineH = (int)(20*k); if (*lineH<15) *lineH=15;
    *pad = (int)(2*k); if (*pad<2) *pad=2;
    *cw = FmCellWidth(W,*lineH*3,*pad,FM_DISPLAY_CELLS);
    *barH = (int)(14*k); if (*barH<10) *barH=10;
    return *pad*2 + *barH;
}
static int FmReadoutHeight(int barH, int pad) { return g_pst.v[PS_FRAME_READOUT] ? barH+pad : 0; }
static int FmKiShown(void) { return g_pst.v[PS_FRAME_KI] && g_pst.v[PS_FRAME_METER] && g_fm_show && g_fm_ki_show; }
static int FmPlayerRow(void) { return (g_pst.v[PS_PLAYER_METER] || g_pst.v[PS_KI_WINDOW]) && g_fmp_show; }
static int FmPanels(void) { return (g_pst.v[PS_FRAME_METER] && g_fm_show ? 1 : 0) + (FmPlayerRow() ? 1 : 0) + FmKiShown(); }
#define FM_KIWIN_RGB RGB(16,44,168)
#define FM_NODMG_RGB RGB(255,206,48)  // #35176 i-frame stripe (gold), top third of the cell
static void FmLabel(HDC dc, int x, int y, const char* text, COLORREF color) {
    SetTextColor(dc,color); TextOutA(dc,x,y,text,(int)strlen(text));
}
static void FmCheck(HDC dc, int x, int y, int w, int h) {
    const int thick = h / 3 > 3 ? h / 3 : 3;
    const int x0 = x + w / 5, y0 = y + h / 2, x1 = x + w / 2, y1 = y + (h * 4) / 5, x2 = x + (w * 4) / 5, y2 = y + h / 5;
    HPEN dark = CreatePen(PS_SOLID, thick + 2, RGB(8, 10, 16));
    HPEN light = CreatePen(PS_SOLID, thick, RGB(245, 248, 255));
    HGDIOBJ old = SelectObject(dc, dark);
    MoveToEx(dc, x0, y0, 0); LineTo(dc, x1, y1); LineTo(dc, x2, y2);
    SelectObject(dc, light);
    MoveToEx(dc, x0, y0, 0); LineTo(dc, x1, y1); LineTo(dc, x2, y2);
    SelectObject(dc, old); DeleteObject(dark); DeleteObject(light);
}
static void FmPanel(HDC dc, int w, int h, int H, int oy, const FmMeter* mp, const char* who, const char* side, int kiStripe, int noDmg) {
    const FmMeter m=*mp;
    int cw,barH,lineH,pad; const int ph=FmGeom(w,H,&cw,&barH,&lineH,&pad); (void)h;
    const int px=barH-2>9?barH-2:9;
    if (!g_fm_font || g_fm_fontPx!=px) {
        if(g_fm_font) DeleteObject(g_fm_font);
        g_fm_fontPx=px; g_fm_font=CreateFontW(-px,0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,FIXED_PITCH,L"Consolas");
    }
    int used=FM_CELLS;
    while(used>0 && (FmViewCell(&m,used-1)&0x7F)==FM_EMPTY) --used;
    const int viewStart=used>FM_DISPLAY_CELLS?used-FM_DISPLAY_CELLS:0;
    const int barW=cw*FM_DISPLAY_CELLS,gutter=lineH*3,x0=(w-barW-gutter)/2+gutter,by=oy+pad;
    FmFill(dc,x0-gutter-pad,oy,barW+gutter+pad*2,ph,RGB(16,21,30));
    HGDIOBJ of=SelectObject(dc,g_fm_font); SetBkMode(dc,TRANSPARENT);
    // Clip long actor names/freeze hints to the strip instead of spilling outside the panel.
    int saved=SaveDC(dc); IntersectClipRect(dc,x0-gutter,oy,x0+barW,oy+ph);
    (void)who; // Compact default has no actor/title line.
    FmLabel(dc,x0-gutter,by,side,RGB(240,243,249));
    FmFill(dc,x0-1,by-1,barW+2,barH+2,RGB(41,50,65));
    for(int i=0;i<FM_DISPLAY_CELLS;++i) {
        const uint8_t c=FmViewCell(&m,viewStart+i);
        const int win=kiStripe && FmViewKiWin(&m,viewStart+i);
        FmFill(dc,x0+i*cw,by,cw>3?cw-2:cw>1?cw-1:cw,barH,win?FM_KIWIN_RGB:FmColor(c&0x7F));
        if(!win && (c&0x7F)==FM_ACTIVE_UB) FmFill(dc,x0+i*cw,by+barH/2,cw>3?cw-2:1,1,RGB(255,210,216));
        if(c&FM_EDGE) FmFill(dc,x0+i*cw,by-2,1,barH+4,RGB(235,240,248));
        if(noDmg && FmViewNoDmg(&m,viewStart+i)) FmFill(dc,x0+i*cw,by,cw>3?cw-2:cw>1?cw-1:cw,barH/3>2?barH/3:2,FM_NODMG_RGB);
    }
    // Count each visible colour run; action boundaries and the Ki window also start a new run.
    // Centre within its LAST coloured cell, excluding the separator.
    for(int first=0;first<FM_CELLS;) {
        const uint8_t st=FmViewCell(&m,first)&0x7F;
        const int win=kiStripe && FmViewKiWin(&m,first);
        int end=first+1;
        while(end<FM_CELLS) {
            const uint8_t next=FmViewCell(&m,end);
            const int nwin=kiStripe && FmViewKiWin(&m,end);
            if((next&0x7F)!=st || (next&FM_EDGE) || nwin!=win) break;
            ++end;
        }
        if(st!=FM_EMPTY && end>viewStart && end<=viewStart+FM_DISPLAY_CELLS) {
            char n[12]; snprintf(n,sizeof n,"%d",end-first);
            const int left=x0+(end-viewStart-1)*cw;
            const int fillW=cw>3?cw-2:cw>1?cw-1:cw;
            RECT cell={left,by,left+fillW,by+barH},textBox;
            HFONT font=FmFitCount(dc,n,cell,px,&textBox);
            if(font) {
                HGDIOBJ normal=SelectObject(dc,font);
                int clip=SaveDC(dc); IntersectClipRect(dc,cell.left,cell.top,cell.right,cell.bottom);
                for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx)
                    if(dx || dy) FmLabel(dc,textBox.left+dx,textBox.top+dy,n,RGB(2,3,5));
                FmLabel(dc,textBox.left,textBox.top,n,
                    RGB(255,255,255));
                RestoreDC(dc,clip); SelectObject(dc,normal); DeleteObject(font);
            }
        }
        first=end;
    }
    if(kiStripe) {
        for(int i=0;i<FM_DISPLAY_CELLS;++i) if(FmViewKiPulse(&m,viewStart+i))
            FmCheck(dc,x0+i*cw,by,cw>3?cw-2:cw,barH);
    }
    RestoreDC(dc,saved); SelectObject(dc,of);
}
// Shared legend below both rows; wraps at smaller resolutions.
static int FmLegendCount(void) { return 11 + (g_pst.v[PS_KI_WINDOW] ? 1 : 0) + FmKiShown() + (FmPlayerRow() ? 1 : 0); }
static int FmLegendHeight(int w,int H) {
    if(!g_pst.v[PS_FRAME_LEGEND]) return 0;
    int cw,bh,lh,pad; FmGeom(w,H,&cw,&bh,&lh,&pad);
    const int n=FmLegendCount();
    int cols=cw*FM_DISPLAY_CELLS/(lh*6); if(cols<1) cols=1; if(cols>n) cols=n;
    return ((n+cols-1)/cols)*lh+pad*2;
}
static void FmLegend(HDC dc,int w,int H,int y) {
    if(!g_pst.v[PS_FRAME_LEGEND]) return;
    int cw,bh,lh,pad; FmGeom(w,H,&cw,&bh,&lh,&pad);
    const int bw=cw*FM_DISPLAY_CELLS,gutter=lh*3,x=(w-bw-gutter)/2+gutter;
    const char* names[]={"Startup","Active","Recovery","Unblockable","Grab","No hit","Pending","Empty","Action start","Total frames","Stun/adv --","Ki Pulse","Ki debt","No damage (i-frame)"};
    COLORREF colors[]={FmColor(FM_STARTUP),FmColor(FM_ACTIVE),FmColor(FM_RECOVERY),FmColor(FM_ACTIVE_UB),FmColor(FM_GRAB),FmColor(FM_NEUTRAL),FmColor(FM_PENDING),FmColor(FM_EMPTY),RGB(235,240,248),RGB(214,222,234),RGB(145,159,181),FM_KIWIN_RGB,RGB(138,92,255),FM_NODMG_RGB};
    int use[14]; for(int i=0;i<11;++i) use[i]=1; use[11]=g_pst.v[PS_KI_WINDOW]?1:0; use[12]=FmKiShown()?1:0; use[13]=FmPlayerRow()?1:0;
    int idx[14], n=0; for(int i=0;i<14;++i) if(use[i]) idx[n++]=i;
    int cols=bw/(lh*6); if(cols<1) cols=1; if(cols>n) cols=n;
    const int pitch=bw/cols;
    FmFill(dc,x-gutter-pad,y,bw+gutter+pad*2,FmLegendHeight(w,H),RGB(16,21,30));
    HGDIOBJ old=SelectObject(dc,g_fm_font); SetBkMode(dc,TRANSPARENT);
    FmLabel(dc,x-gutter,y+pad,"KEY",RGB(145,159,181));
    for(int i=0;i<n;++i) {
        const int xx=x+(i%cols)*pitch,yy=y+pad+(i/cols)*lh;
        FmFill(dc,xx,yy+3,8,8,colors[idx[i]]); FmLabel(dc,xx+12,yy,names[idx[i]],RGB(183,194,211));
    }
    SelectObject(dc,old);
}
static void FmPaint(HDC dc, int w, int h, int H) {
    static FmMeter m,mp; static FmKi ki; int showKi; char who[48]; AcquireSRWLockShared(&g_fm_lock);
    m=g_fm; mp=g_fmp; ki=g_fm_ki; showKi=FmKiShown(); snprintf(who,sizeof who,"%s",g_fm_who); ReleaseSRWLockShared(&g_fm_lock);
    FmFill(dc,0,0,w,h,FM_KEY);
    int cw,barH,lineH,pad; const int ph=FmGeom(w,H,&cw,&barH,&lineH,&pad);
    // Clip long readouts/key labels as well as cells to the centered panel bounds.
    const int panelW=cw*FM_DISPLAY_CELLS+lineH*3+pad*2;
    const int panelLeft=(w-cw*FM_DISPLAY_CELLS-lineH*3)/2-pad;
    const int panelClip=SaveDC(dc);
    IntersectClipRect(dc,panelLeft,0,panelLeft+panelW,h);
    int oy=FmReadoutHeight(barH,pad);
    if(oy) {
        const FmSum* q=FmPlayerRow() ? &mp.cur : &m.cur;
        char start[24]="--",active[192]="--",recovery[24]="--",total[24]="--",text[320];
        if(q->hits) {
            snprintf(start,sizeof start,"%d",q->startup);
            snprintf(total,sizeof total,"%d%s",q->total,q->done?"":"+");
            snprintf(recovery,sizeof recovery,"%d%s",q->recovery,q->done?"":"+");
            int used=0; active[0]=0;
            for(int i=0;i<q->hits && i<FM_MAXW && used<(int)sizeof active-24;++i)
                used+=snprintf(active+used,sizeof active-used,"%s%d-%d",i?",":"",q->ws[i]+1,q->we[i]>q->ws[i]?q->we[i]:q->ws[i]+1);
        }
        // Separate header above both pip rows. Top actor's active windows are 1-based.
        // Independent snapshots cannot measure advantage.
        snprintf(text,sizeof text,"Startup %s  Active %s  Recovery %s  Total %s  Advantage --",start,active,recovery,total);
        int bw=cw*FM_DISPLAY_CELLS,gutter=lineH*3,x=(w-bw-gutter)/2+gutter;
        FmFill(dc,x-gutter-pad,0,bw+gutter+pad*2,oy,RGB(16,21,30));
        HFONT font=CreateFontW(-(barH-2),0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,FIXED_PITCH,L"Consolas");
        HGDIOBJ old=SelectObject(dc,font); SetBkMode(dc,TRANSPARENT);
        FmLabel(dc,x,0,text,RGB(245,248,255)); SelectObject(dc,old); DeleteObject(font);
    }
    if(FmPlayerRow()) { FmPanel(dc,w,h,H,oy,&mp,"PLAYER","PLAYER",g_pst.v[PS_KI_WINDOW],1); oy+=ph; }
    if(g_pst.v[PS_FRAME_METER] && g_fm_show) { FmPanel(dc,w,h,H,oy,&m,who,"BOSS",0,0); oy+=ph; }
    if(showKi) {
        int used=FM_CELLS; while(used>0 && (FmViewCell(&m,used-1)&0x7F)==FM_EMPTY) --used;
        const int start=used>FM_DISPLAY_CELLS?used-FM_DISPLAY_CELLS:0;
        const int bw=cw*FM_DISPLAY_CELLS,gutter=lineH*3,x=(w-bw-gutter)/2+gutter,by=oy+pad;
        FmFill(dc,x-gutter-pad,oy,bw+gutter+pad*2,ph,RGB(16,21,30));
        HGDIOBJ old=SelectObject(dc,g_fm_font); SetBkMode(dc,TRANSPARENT);
        FmLabel(dc,x-gutter,by,g_fm_ki_label,RGB(240,243,249));
        FmFill(dc,x-1,by-1,bw+2,barH+2,RGB(41,50,65));
        for(int i=0;i<FM_DISPLAY_CELLS;++i) FmFill(dc,x+i*cw,by,cw>3?cw-2:cw>1?cw-1:cw,barH,FmKiView(&ki,&m,start+i)?RGB(138,92,255):FmColor(FM_EMPTY));
        SelectObject(dc,old); oy+=ph;
    }
    FmLegend(dc,w,H,oy);
    RestoreDC(dc,panelClip);
}
static BOOL CALLBACK FmFind(HWND h, LPARAM lp) { (void)lp;
    DWORD pid; GetWindowThreadProcessId(h, &pid); if (pid != GetCurrentProcessId() || !IsWindowVisible(h) || h == g_fm_wnd || h == g_fd_wnd || GetWindow(h, GW_OWNER)) return TRUE;
    RECT r; GetClientRect(h, &r); if (r.right < 400 || r.bottom < 300) return TRUE;
    g_fm_game = h; return FALSE;
}
static int g_fm_H;  // game client height (font / bar scale)
static LRESULT CALLBACK FmProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCHITTEST) return HTTRANSPARENT;
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); RECT r; GetClientRect(h, &r);
        HDC mem = CreateCompatibleDC(dc); HBITMAP bmp = CreateCompatibleBitmap(dc, r.right, r.bottom); HGDIOBJ ob = SelectObject(mem, bmp);
        __try { FmPaint(mem, r.right, r.bottom, g_fm_H); } __except (EXCEPTION_EXECUTE_HANDLER) { }
        BitBlt(dc, 0, 0, r.right, r.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, ob); DeleteObject(bmp); DeleteDC(mem); EndPaint(h, &ps); return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}
static DWORD WINAPI FmThread(LPVOID p) {
    (void)p;
    WNDCLASSEXW wc = {sizeof wc}; wc.lpfnWndProc = FmProc; wc.hInstance = GetModuleHandleW(0); wc.lpszClassName = L"Nioh3TrainingFrameMeter";
    RegisterClassExW(&wc);
    g_fm_wnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, wc.lpszClassName, L"", WS_POPUP, 0, 0, 100, 100, 0, 0, wc.hInstance, 0);
    if (!g_fm_wnd) { Log("FRAME METER: overlay window failed (%lu), meter not drawn", GetLastError()); return 0; }
    SetLayeredWindowAttributes(g_fm_wnd, FM_KEY, 235, LWA_COLORKEY | LWA_ALPHA);
    Log("FRAME METER: overlay window ready");
    int shown = 0; LONG lastVer = -1; RECT last = {0};
    for (;;) {
        Sleep(33);
        MSG msg; while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        if (!g_fm_game || !IsWindow(g_fm_game)) { g_fm_game = 0; EnumWindows(FmFind, 0); }
        const int have = g_fm_show || g_fmp_show;
        const int gameOk = g_fm_game && !IsIconic(g_fm_game);
        const int fg = gameOk && GetForegroundWindow() == g_fm_game;
        const int menu = GateMenuOpen();
        const int want = (g_pst.v[PS_FRAME_METER] || g_pst.v[PS_PLAYER_METER] || g_pst.v[PS_KI_WINDOW]) && g_pst_live && FmPanels() && gameOk && fg && !menu;
        if (have) {
            static int toldOn, toldWhy;
            if (want && !toldOn) { toldOn = 1; Log("FRAME METER: on screen"); }
            else if (!want && !toldWhy) {
                toldWhy = 1;
                Log("FRAME METER: hidden live %d panels %d game %d foreground %d menu %d", (int)g_pst_live, FmPanels(), gameOk ? 1 : 0, fg ? 1 : 0, menu ? 1 : 0);
            }
        }
        if (!want) { if (shown) { ShowWindow(g_fm_wnd, SW_HIDE); shown = 0; } continue; }
        RECT cr; GetClientRect(g_fm_game, &cr); POINT o = {0, 0}; ClientToScreen(g_fm_game, &o);
        int cw, barH, lineH, pad; g_fm_H = cr.bottom;
        const int rowH = FmGeom(cr.right, cr.bottom, &cw, &barH, &lineH, &pad);
        const int hh = rowH * FmPanels() + FmReadoutHeight(barH,pad) + FmLegendHeight(cr.right,cr.bottom);
        const int top = o.y + FmPositionY(cr.bottom, hh, g_cfg_fm_y);
        const int left = o.x + (g_cfg_fm_y < 0 ? FmPositionX(cr.right) : 0);
        RECT want_r = {left, top, left + cr.right, top + hh};
        if (!shown || memcmp(&want_r, &last, sizeof last)) {
            SetWindowPos(g_fm_wnd, HWND_TOPMOST, want_r.left, want_r.top, want_r.right - want_r.left, hh, SWP_NOACTIVATE | SWP_SHOWWINDOW);
            last = want_r; shown = 1; lastVer = -1;
        }
        const LONG v = g_fm_ver; if (v != lastVer) { lastVer = v; InvalidateRect(g_fm_wnd, 0, FALSE); }
    }
}
