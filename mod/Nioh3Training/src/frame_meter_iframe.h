// #35411 empty-area i-frame probe, pure part (no game access; tests/frame_meter_iframe_test.c). Game side: frame_meter_game.h.
// 1. Predicate bits: the defender-side "cannot be damaged / no damage / no hit" tests of the hit and damage pipeline, evaluated on the
//    PLAYER every tick without any enemy or hit (notes/iframes.md, section "Empty-area probe").
// 2. A window tracker: for a block of dwords (actor / ctrl / status), which dwords turn to a new value after the action start and go back
//    before it ends (a "window"), or turn once and stay until the end ("open"). Fast-changing words (transforms, timers) are "noisy".
// 3. The stripe rule: a predicate bit lights the gold "No damage" stripe only after it has been seen OFF once for this actor, so a bit that
//    is always on (a wrong reading) cannot paint every cell.
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    FMI_CANNOT0 = 1u << 0,   // 2A3264(ctrl, hit type 0) = "cannot be damaged" (validity 78AC18 -> 2A3238)
    FMI_CANNOT4 = 1u << 1,   // same, hit type 4 (EA1 does not count)
    FMI_CANNOT9 = 1u << 2,   // same, hit type 9 (EAE counts)
    FMI_R29AE0  = 1u << 3,   // 481290(ctrl): action record dword +0x18 bit 29 and ctrl+0xAE0 != 0
    FMI_A8B20   = 1u << 4,   // 866490(ctrl): ctrl+0xA8 bit 20 and 481290
    FMI_NODMGACT= 1u << 5,   // action record qword +0 bit 35 (326C94: no damage when not guarding)
    FMI_S48     = 1u << 6,   // status+0x48 == 1 (326C94: no damage for attack rows with bit 1)
    FMI_S2DC8   = 1u << 7,   // status+0x2DC8 bit 31
    FMI_EA0     = 1u << 8,
    FMI_EA1     = 1u << 9,
    FMI_EAE     = 1u << 10,
    FMI_X15D8   = 1u << 11,  // [ctrl+0x15D8]+2
    FMI_ISNODMG = 1u << 12,  // IsNoDamage 2A343C without its buff vcall: actor+0xADB1 | ctrl+0xEB0 (only clears the reaction, 326C94 +0x29E)
    FMI_NOHIT   = 1u << 13,  // actor+0xABC0 | ctrl+0xEB1 (contact handler 117A6D0 returns)
    FMI_COND4   = 1u << 14,  // 876130(ctrl) = condition 4: status+0x2DC8 bit 12 | ctrl+0xEAA (78AD8C / 2A34F8 use it)
    FMI_ARMOR   = 1u << 15,  // 2A3764(ctrl): status+0x2DC8 bit 49 | ctrl+0xEAB (326C94 sets dmg+0x284)
    FMI_GUARDX  = 1u << 16,  // 2A36FC without its game-mode call: status+0x2DC8 bit 11 | bit 48 | ctrl+0xEA5
    FMI_HURTOFF = 1u << 17,  // hurtbox candidate (FmHurtOff)
    FMI_NBITS   = 18
};
// Bits that, if set, mean a hit on the player would do no damage or not connect (the stripe set). S48 depends on the attack row, ISNODMG
// only removes the reaction, COND4 / ARMOR / GUARDX change the reaction or guard path: logged, not drawn.
#define FMI_STRIPE_MASK (FMI_CANNOT0 | FMI_NODMGACT | FMI_NOHIT | FMI_HURTOFF | FMI_EA0 | FMI_EA1 | FMI_S2DC8 | FMI_X15D8 | FMI_A8B20)
static const char* const g_fmi_names[FMI_NBITS] = {"cannot0", "cannot4", "cannot9", "r29ae0", "a8b20", "nodmgact", "s48", "s2dc8b31", "ea0", "ea1",
    "eae", "x15d8", "isnodmg", "nohit", "cond4", "armor", "guardx", "hurtoff"};

// Stripe rule. `seenOff` keeps the bits seen OFF at least once for this actor (reset it when the actor changes).
static inline int FmiStripe(uint32_t mask, uint32_t* seenOff) {
    *seenOff |= ~mask & ((1u << FMI_NBITS) - 1);
    return (mask & *seenOff & FMI_STRIPE_MASK) != 0;
}

// Window tracker, one entry per dword of a sampled block.
typedef struct { uint32_t base, last, first; uint16_t nchg, f1, f2; } FmiWord;
enum { FMI_NOISY = 4 };  // more than this many changes in one action = noisy (a transform, a timer)
static inline void FmiTrackReset(FmiWord* w, const uint32_t* cur, int n) {
    for (int i = 0; i < n; ++i) { w[i].base = w[i].last = w[i].first = cur[i]; w[i].nchg = w[i].f1 = w[i].f2 = 0; }
}
static inline void FmiTrackStep(FmiWord* w, const uint32_t* cur, int n, int frame) {
    const uint16_t f = (uint16_t)(frame < 0 ? 0 : frame > 0xFFFF ? 0xFFFF : frame);
    for (int i = 0; i < n; ++i) {
        if (cur[i] == w[i].last) continue;
        if (w[i].nchg < 0xFFFF) w[i].nchg++;
        if (w[i].nchg == 1) { w[i].f1 = f; w[i].first = cur[i]; }
        w[i].f2 = f; w[i].last = cur[i];
    }
}
// 1 = window (changed at f1 >= 1, back to the start value at f2), 2 = open (one change at f1 >= 1, still changed at the end), 0 = nothing / noisy.
static inline int FmiTrackKind(const FmiWord* w) {
    if (w->nchg == 0 || w->nchg > FMI_NOISY || w->f1 < 1) return 0;
    if (w->last == w->base && w->nchg >= 2) return 1;
    if (w->nchg == 1) return 2;
    return 0;
}
static inline int FmiTrackNoisy(const FmiWord* w, int n) { int k = 0; for (int i = 0; i < n; ++i) k += w[i].nchg > FMI_NOISY; return k; }
// Appends " <tag><off>=<base>><first>@<f1>-<f2>" (window) or " <tag><off>=<base>><first>@<f1>-" (open) for each candidate. Returns the
// number appended; *pos is advanced; stops when `max` entries are written or the buffer is nearly full.
static inline int FmiTrackFormat(const FmiWord* w, int n, char tag, uint32_t offBase, char* out, int cap, int* pos, int max) {
    int k = 0;
    for (int i = 0; i < n && k < max; ++i) {
        const int kind = FmiTrackKind(&w[i]);
        if (!kind) continue;
        if (*pos > cap - 48) break;
        char tmp[64]; int m;
        if (kind == 1) m = snprintf(tmp, sizeof tmp, " %c%X=%X>%X@%u-%u", tag, (unsigned)(offBase + i * 4), w[i].base, w[i].first, w[i].f1, w[i].f2);
        else m = snprintf(tmp, sizeof tmp, " %c%X=%X>%X@%u-", tag, (unsigned)(offBase + i * 4), w[i].base, w[i].first, w[i].f1);
        if (m <= 0 || *pos + m >= cap) break;
        memcpy(out + *pos, tmp, (size_t)m + 1); *pos += m; k++;
    }
    return k;
}
// The game frame (1/60 s of game time) of a sample at `t` ms in an action that started at `t0` ms.
static inline int FmiFrame(double t, double t0) { const double d = t - t0; return d <= 0 ? 0 : (int)(d * 60.0 / 1000.0 + 1e-6); }
// Compact names of the set bits ("cannot0|nodmgact"), "-" when none.
static inline void FmiNames(uint32_t m, char* out, int cap) {
    int o = 0; out[0] = 0;
    for (int b = 0; b < FMI_NBITS; ++b) if (m >> b & 1) { const int k = snprintf(out + o, cap - o, "%s%s", o ? "|" : "", g_fmi_names[b]); if (k <= 0 || o + k >= cap) break; o += k; }
    if (!o) snprintf(out, cap, "-");
}
