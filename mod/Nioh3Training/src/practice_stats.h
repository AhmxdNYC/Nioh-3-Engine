// user #22389: Practice stats (Player / Boss options in the Practice menu). Pure decisions, no game access (tests/practice_stats_test.c).
// Game glue: practice_stats_game.h. Shared with Boss Rematch's menu (practice_stats_menu.h).
// Settings (saved in Nioh3Training.ini [Practice], every one defaults OFF / 100 % except NegKiMeter / FrameMeterReadout / KiWindow = ON):
//   PS_PLAYER_INF_KI   PlayerInfiniteKi    0/1    player Ki (stamina) refilled to max whenever it drops (st+0xAC = st+0xB0, Player::RecoverStamina 5FB5C8)
//   PS_DMG_DEALT       DamageDealtPct      10..1000 HP damage of hits whose attacker is the player (x0.1 .. x10) x pct / 100
//   PS_DMG_TAKEN       DamageTakenPct      0..1000 HP damage of hits whose attacker is a Training boss (x0 .. x10) x pct / 100 ('Damage boss deals')
//   PS_BOSS_KI_DRAIN   BossKiDrainPerSec   0..1000 Training bosses lose this much Ki per second (0 = off; floor Ki 0.5 = one hit from a break)
//   PS_BOSS_SPEED     BossSpeedPct        10..300 Training/Rematch boss animation speed (ComputeMotionSpeed 134E68 hook, notes/boss-speed-lever-22389.md)
//   PS_PLAYER_SPEED   PlayerSpeedPct      10..300 player animation speed (same 134E68 hook, [ctrl+0xE0] == the player)
//   PS_PLAYER_INF_NIN PlayerInfiniteNinjutsu 0/1  player ninjutsu gauges (st+0x36AC+k*0x30 cur / +0x36B0 max, k=0..2) kept full
//   PS_PLAYER_INF_SOUL PlayerInfiniteSoulCore 0/1 (pc #57367) soul-core uses are not used up: a soul core is an inventory item (count u32 at
//                                         record +4, records inv+0x1E0AD0+i*0xE8); a count that drops by 1 during the seal throw (player action
//                                         0x28FD/0x28FB/0x28AF + 2 s) is put back. Works everywhere (not only in Practice), other items untouched.
//   PS_FRAME_METER    FrameMeter          0/1    on-screen boss frame meter (frame_meter.h / frame_meter_game.h; read-only, uses the FRAMEPROBE hook)
//   PS_PLAYER_METER   PlayerFrameMeter    0/1    second bar under the boss meter for the PLAYER's own attacks (user #22796, notes/player-frame-meter-22796.md)
//   PS_KI_DEALT       KiDamageDealtPct    0..1000 Ki damage of hits whose attacker is the player (x0 .. x10) x pct / 100 (user #22588; KiDamage 844604 at 329E9D)
//   PS_ONE_HIT_KI     OneHitKiBreak       0/1    a player hit on a Training boss empties its Ki in that same hit = the native Ki break (user #22588)
//   PS_HOLD_KI_BREAK  HoldKiBreak         0/1    a Ki-broken Training boss stays broken (break timers kept from running out, AI held off) until OFF / it dies (user #22918)
//   PS_TIME_OF_DAY    TimeOfDayMin        -1 / 0..1410 (ini, minutes; 30-min steps) sky time of day; -1 = Stock (no write). Internal value 0 = Stock,
//                                         1..48 = (v - 1) * 30 min (PstTodFromIni / PstTodToIni). user #22942, notes/training-time-of-day-22942.md
//   PS_WEATHER        Weather             0..5   0 Stock, 1 Clear, 2 Overcast, 3 Rain, 4 Storm, 5 Snow (sky/env fields of the active RenderConfig, user #22942)
//   PS_KI_LOG         KiLog               0/1    Ki-break logger: every 100 ms while a Training boss is Ki-broken, its Ki pool fields + the Ki floor, and every
//                                         Ki-damage hit on it (user #23013, notes/human-boss-negative-ki-23013.md design 1). Log only, writes nothing.
//   PS_HOLD_KI_V2     HoldKiBreakV2       0/1    Hold Ki break method (needs HoldKiBreak ON): 0 = timer hold (#22918), 1 = Ki-pool hold (design 2 of #23013)
//   PS_KI_FLOOR       KiFloorDepth        0..1000 % of the stock negative Ki floor for Training bosses (100 = stock; design 3 of #23013)
//   PS_NEG_KI_METER   NegKiMeter          0/1    (default OFF since #38077) thin bar under a Training boss's Ki bar while its Ki or K+0x60 pool is below 0 (user #23400,
//                                         negki_meter.h / negki_meter_game.h; read only)
//   PS_KI_WINDOW      KiWindow            0/1    (default OFF since #38077) dark blue on the player frame-meter row while KiTick K+0x74 counts down from about 30
//                                         (#34558, about 0.4 s after the attack starts). Gameplay / Overlays can turn it off. Not the boss Ki-debt strip.
//   PS_FRAME_IFR      FrameMeterIFrames   0/1    (default ON, human #37322) own "I-FR" row under the PLAYER row showing only the i-frame (no damage) cells in gold;
//                                         OFF = the gold stripe on the PLAYER row as before.
//   PS_FRAME_PROW     FrameMeterPlayerRow 0/1    (default ON, human #38180) draw the PLAYER row; OFF keeps the I-FR row (only i-frames)
//   PS_KEY_*          FrameMeterKey<Startup|Active|Recovery|Unblockable|Grab|Other|Timing|Ki|IFrames> 0/1 (default ON) one legend (KEY) entry group each:
//                     Other = No hit / Pending / Empty, Timing = Action start / Total frames / Stun, Ki = Ki Pulse / Ki debt / Perfect, IFrames = the 4 I FR entries
#pragma once
#include <stdint.h>
enum { PS_PLAYER_INF_KI = 0, PS_DMG_DEALT = 1, PS_DMG_TAKEN = 2, PS_BOSS_KI_DRAIN = 3, PS_BOSS_SPEED = 4, PS_PLAYER_INF_NIN = 5, PS_PLAYER_SPEED = 6, PS_FRAME_METER = 7, PS_KI_DEALT = 8, PS_ONE_HIT_KI = 9, PS_PLAYER_METER = 10, PS_HOLD_KI_BREAK = 11, PS_TIME_OF_DAY = 12, PS_WEATHER = 13, PS_KI_LOG = 14, PS_HOLD_KI_V2 = 15, PS_KI_FLOOR = 16, PS_NEG_KI_METER = 17, PS_FRAME_LEGEND = 18, PS_FRAME_READOUT = 19, PS_FRAME_KI = 20, PS_KI_WINDOW = 21, PS_FRAME_IFR = 22,
       // human #38180 frame-meter visibility: PLAYER row + one key (legend) entry group each, all default ON
       PS_FRAME_PROW = 23, PS_KEY_STARTUP = 24, PS_KEY_ACTIVE = 25, PS_KEY_RECOVERY = 26, PS_KEY_UNBLOCK = 27, PS_KEY_GRAB = 28, PS_KEY_OTHER = 29, PS_KEY_TIMING = 30, PS_KEY_KI = 31, PS_KEY_IFR = 32,
       PS_FRAME_STYLE = 33, PS_FRAME_SIZE = 34, PS_PLAYER_INF_SOUL = 35, PS_N = 36 }; // append-only ABI: 0 Row (default), 1 Ring
static inline int PstIsToggle(int what) { return (what >= PS_FRAME_PROW && what <= PS_FRAME_STYLE) || what == PS_PLAYER_INF_KI || what == PS_PLAYER_INF_NIN || what == PS_PLAYER_INF_SOUL || what == PS_FRAME_METER || what == PS_ONE_HIT_KI || what == PS_PLAYER_METER || what == PS_HOLD_KI_BREAK || what == PS_KI_LOG || what == PS_HOLD_KI_V2 || what == PS_NEG_KI_METER || what == PS_FRAME_LEGEND || what == PS_FRAME_KI || what == PS_FRAME_READOUT || what == PS_KI_WINDOW || what == PS_FRAME_IFR; }
typedef struct { int v[PS_N]; } PstCfg;
static inline int PstDefault(int what) { return what == PS_DMG_DEALT || what == PS_DMG_TAKEN || what == PS_KI_DEALT || what == PS_BOSS_SPEED || what == PS_PLAYER_SPEED || what == PS_KI_FLOOR ? 100 : what == PS_FRAME_LEGEND || what == PS_FRAME_KI || what == PS_FRAME_READOUT || what == PS_FRAME_IFR || (what >= PS_FRAME_PROW && what <= PS_KEY_IFR) ? 1 : 0; }  // #38077: NegKiMeter + KiWindow overlays default OFF; style defaults Row
static inline void PstDefaults(PstCfg* c) { for (int i = 0; i < PS_N; ++i) c->v[i] = PstDefault(i); }
enum { PS_TOD_STEPS = 48 };  // 00:00 .. 23:30 in 30-min steps (internal 1..48; 0 = Stock)
enum { PS_WX_STOCK = 0, PS_WX_CLEAR = 1, PS_WX_OVERCAST = 2, PS_WX_RAIN = 3, PS_WX_STORM = 4, PS_WX_SNOW = 5, PS_WX_N = 6 };
static inline int PstClamp(int what, int v) {
    const int sp = what == PS_BOSS_SPEED || what == PS_PLAYER_SPEED;
    const int lo = sp || what == PS_DMG_DEALT ? 10 : 0;
    const int hi = what == PS_FRAME_SIZE ? 2 : PstIsToggle(what) ? 1 : what == PS_TIME_OF_DAY ? PS_TOD_STEPS : what == PS_WEATHER ? PS_WX_N - 1 : sp ? 300 : 1000;
    return v < lo ? lo : v > hi ? hi : v;
}
// Left / Right steps of the menu sliders (A on an ON/OFF row flips it).
static inline int PstStep(int what, int v, int dir) {
    if(what == PS_FRAME_SIZE) return dir ? PstClamp(what,v+(dir>0?1:-1)) : (PstClamp(what,v)+1)%3;
    static const int dmg[] = {0, 10, 25, 50, 75, 100, 125, 150, 200, 300, 500, 1000};  // PS_DMG_DEALT starts at 10 (PstClamp); PS_KI_DEALT / PS_DMG_TAKEN at 0
    static const int drain[] = {0, 5, 10, 20, 30, 50, 100, 200, 500, 1000};  // Ki per second (a boss's max Ki is ~1000)
    static const int spd[] = {10, 25, 50, 75, 100, 125, 150, 200, 300};
    static const int depth[] = {0, 50, 100, 150, 200, 300, 500, 1000};  // Ki floor depth, % of the stock floor
    const int* st = what == PS_BOSS_KI_DRAIN ? drain : what == PS_BOSS_SPEED || what == PS_PLAYER_SPEED ? spd : what == PS_KI_FLOOR ? depth : dmg;
    const int n = what == PS_BOSS_KI_DRAIN ? (int)(sizeof drain / sizeof drain[0]) : what == PS_BOSS_SPEED || what == PS_PLAYER_SPEED ? (int)(sizeof spd / sizeof spd[0]) : what == PS_KI_FLOOR ? (int)(sizeof depth / sizeof depth[0]) : (int)(sizeof dmg / sizeof dmg[0]);
    if (PstIsToggle(what)) return dir > 0 ? 1 : dir < 0 ? 0 : !v;
    if (what == PS_TIME_OF_DAY || what == PS_WEATHER) return PstClamp(what, PstClamp(what, v) + (dir > 0) - (dir < 0));  // one step (30 min / one preset); Stock at the left end
    v = PstClamp(what, v);
    if (dir > 0) { for (int i = 0; i < n; ++i) if (st[i] > v) return st[i]; return st[n - 1]; }
    if (dir < 0) { for (int i = n - 1; i >= 0; --i) if (st[i] < v) return PstClamp(what, st[i]); return PstClamp(what, st[0]); }
    return v;
}
// Scaled HP damage of one hit. Only positive damage is scaled (0 / negative = heal or no-op, untouched); 100 % = unchanged; rounds to nearest;
// a positive hit at a non-zero % never rounds down to 0; never overflows int.
static inline int PstScaleDmg(int dmg, int pct) {
    if (dmg <= 0 || pct == 100) return dmg;
    if (pct <= 0) return 0;
    const int64_t r = ((int64_t)dmg * pct + 50) / 100;
    return r < 1 ? 1 : r > 0x7FFFFFFF ? 0x7FFFFFFF : (int)r;
}
// Ki drain for one tick of dtMs at rate Ki per second: new Ki (floor 0.5, so a drain alone never makes the 'bare' Ki-0 break KiBreakBareFix cancels).
// Returns 1 = write *out. Never on a Ki-broken / dead boss, in the KiBreakNoFinisher HP-0 window, or while that boss's infinite Ki holds it (infKi wins).
static inline int PstKiDrain(int rate, int dtMs, int dead, int window, int broken, int infKi, float cur, float max, float* out) {
    if (rate <= 0 || dtMs <= 0 || dead || window || broken || infKi || !(max > 0) || !(cur > 0.5f)) return 0;
    if (dtMs > 500) dtMs = 500;  // a long hitch (load, menu) does not drain a big chunk at once
    float t = cur - (float)rate * (float)dtMs / 1000.0f;
    if (t < 0.5f) t = 0.5f;
    if (!(t < cur - 0.001f)) return 0;
    *out = t; return 1;
}
// Player infinite Ki: 1 = write max this tick (what Player::RecoverStamina 5FB5C8 does). Sanity bounds guard a garbage read.
static inline int PstPlayerKiRefill(int on, float cur, float max) {
    if (!on || !(max > 0.0f) || max > 100000.0f || !(cur >= -100000.0f && cur <= 100000.0f)) return 0;
    return cur < max - 0.5f;
}
// Boss speed: factor applied to a motion play rate (pct 100 = unchanged). Garbage native rates (NaN / negative / huge) are passed through untouched,
// and a hit-stop 0 stays 0.
static inline float PstSpeedScale(int pct, float native) {
    if (pct == 100 || !(native > 0.0f) || native > 100.0f) return native;
    return native * (float)PstClamp(PS_PLAYER_SPEED, pct) / 100.0f;
}
// Player infinite ninjutsu: 1 = write cur = max for this gauge (same sanity bounds as the Ki refill; max <= 0 means the slot is unused).
static inline int PstNinjutsuFill(int on, float cur, float max) {
    if (!on || !(max > 0.0f) || max > 100000.0f || !(cur >= -100000.0f && cur <= 100000.0f)) return 0;
    return cur < max;
}
// Damage multiplier of one hit by ATTACKER side (hit apply 327DEC: attacker = 325F40(hit) = owner character of [hit+0x198], victim = [hit+0x1C8]).
// Player's hits on someone else -> dealt; a Training boss's hits -> boss; anyone else (allies, NPCs, the player hurting itself, unknown attacker) -> 100.
static inline int PstDmgPct(int atkPlayer, int atkBoss, int vicPlayer, int dealt, int boss) {
    if (atkPlayer) return vicPlayer ? 100 : dealt;
    return atkBoss ? boss : 100;
}
// user #22588 Ki damage of one hit (KiDamage 844604(K = victim st+0x98, xmm1 = amount) at 329E9D in the hit apply 327DEC; KiDamage = KiSetCur(cur - amount)).
// Scaled only when the PLAYER is the attacker and the victim is not the player (same sides as PstDmgPct 'Damage I deal'). Only a positive amount is
// scaled (0 / negative / NaN untouched); 100 % = unchanged; 0 % = no Ki damage.
// One-hit Ki break (oneHit, victim a Training boss, not blocked = not already exhausted / broken / dead / in the HP-0 window / its infinite Ki ON):
// the amount becomes cur + max(scaled, 1), so this same hit takes the Ki from > 0 to <= 0 (to minus the hit's own Ki damage, at least -1) = the
// native Ki-break transition of 327DEC (Ki <= 0 test 2A3758 before / after this call). A hit whose Ki damage already empties the bar is untouched.
static inline float PstScaleKi(float amt, int pct) {
    if (!(amt > 0.0f) || pct == 100) return amt;
    if (pct <= 0) return 0.0f;
    if (pct > 1000) pct = 1000;
    return amt * (float)pct / 100.0f;
}
// user #23924 the HP multipliers also scale Ki ('spirit') damage: the Ki percentage of one hit. Player -> someone else: Ki damage I deal x Damage I deal
// (both sliders, 100 % = neutral); a Training boss -> the player: Damage boss deals; anyone else x1. Capped at x10 like the sliders.
static inline int PstKiPct(int atkPlayer, int atkBoss, int vicPlayer, int kiDealt, int dealt, int boss) {
    if (atkPlayer) { if (vicPlayer) return 100; const int64_t p = (int64_t)kiDealt * dealt / 100; return p > 1000 ? 1000 : (int)p; }
    return atkBoss && vicPlayer ? boss : 100;
}
static inline float PstKiDmg(int atkPlayer, int vicPlayer, int vicBoss, int pct, int oneHit, int blocked, float cur, float amt) {
    if (!atkPlayer || vicPlayer || !(amt == amt)) return amt;
    const float a = PstScaleKi(amt, pct);
    if (!oneHit || !vicBoss || blocked || !(cur > 0.0f) || cur > 100000.0f || a < 0.0f || a >= cur) return a;
    return cur + (a > 1.0f ? a : 1.0f);
}
// user #22918 Hold Ki break, one Training boss per tick. broken = the boss is natively Ki-broken (state 0x23 or KiIsExhausted 844FE4);
// held = this boss is already being held. Start only on a natural break (never forces one); keep while broken; release when the toggle goes OFF,
// the session ends (live 0), the boss dies, or the game itself clears the break (e.g. a grapple / finisher ended it): the next break is held again.
enum { PS_HB_NONE = 0, PS_HB_START = 1, PS_HB_KEEP = 2, PS_HB_RELEASE = 3 };
static inline int PstHoldBreakStep(int on, int live, int dead, int broken, int held) {
    if (held) return on && live && !dead && broken ? PS_HB_KEEP : PS_HB_RELEASE;
    return on && live && !dead && broken ? PS_HB_START : PS_HB_NONE;
}
// A running break countdown (seconds, counting down to 0 = recover) kept at least 'hold' s. Only a RUNNING timer (0 < t) is topped up: an idle timer
// (0 or -1) is never started, so the hold never invents a break phase the game did not begin. Garbage (NaN / huge) is left alone.
static inline int PstHoldTimer(float t, float hold, float* out) {
    if (!(t > 0.0f) || t > 1000.0f || !(hold > 0.0f) || t >= hold) return 0;
    *out = hold; return 1;
}
// user #22918 live (padviz test 04:24, one-hit break on an idle boss): the break the player sees is only Ki <= 0 ('23=0 exhausted 0 ki0 1': no state 0x23, no 844FE4 timer),
// the native Ki regen lifts Ki above 0 again ~0.3 s later ('Ki recovered ... Ki 2.2'). The first version only looked at state 0x23 / the timers, so it never started
// (no 'hold Ki break' line at all). Broken = state 0x23 || exhausted || (sane Ki max and Ki <= 0), the same definition as the NOFIN gold bar.
// held = already held: the regen between two ticks may lift Ki a little above 0 (we put it back), that is still the hold; a jump over 25 % of max is a refill by the
// game / Reset fight / the boss Ki slider and ends the hold.
static inline int PstHoldIsBroken(int s23, int exhausted, float kc, float km, int held) {
    if (s23 || exhausted) return 1;
    if (!(km > 0.0f && km <= 100000.0f)) return 0;
    return kc <= 0.0f || (held && kc <= km * 0.25f);
}
// Ki-zero hold: while held and Ki is above 0 (regen), put it back to the lowest value seen since the break started (<= 0, so a negative stun-extend pool is kept).
static inline int PstHoldKi(float kc, float low, float* out) { if (!(kc > 0.0f) || kc > 100000.0f) return 0; *out = low < 0.0f ? low : 0.0f; return 1; }
// user #23013 Ki floor depth (design 3). KiFloor 2F89C8() = tuning 0x7E3D of the global table ([45B9E30]+0x230, lookup 608530): int [rec+0x10] *
// float [rec+0x18], 0.0 when the record is missing; no actor argument. KiSetCur 2F88D0 clamps the new Ki to >= it, KiDamage 844604 clamps K+0x60 to >= it
// while exhausted. For a Training boss's K the floor becomes stock * pct / 100 (a missing / non-negative stock floor uses -max as the base);
// 100 % = stock bit for bit, anyone else = stock. Garbage max / stock (NaN, huge) = stock.
static inline float PstKiFloor(float stock, float max, int pct, int boss) {
    if (!boss || pct == 100 || !(stock == stock) || stock < -1000000.0f || stock > 1000000.0f) return stock;
    pct = pct < 0 ? 0 : pct > 1000 ? 1000 : pct;
    float base = stock;
    if (!(base < 0.0f)) { if (!(max > 0.0f && max <= 100000.0f)) return stock; base = -max; }
    return base * (float)pct / 100.0f;
}
// user #23013 Hold Ki break v2 (design 2): Ki pool writes for one tick of a held boss. want = the floor; returns a bit mask of what to write:
// 1 = cur (KiSetCur to the floor; only when cur is above it), 2 = K+0x60 = floor (only when above it), 4 = K+0x58 = 0, 8 = K+0x5C = 0.
static inline int PstHoldV2Writes(float cur, float k60, float k58, float k5c, float floorv) {
    if (!(floorv == floorv) || floorv > 0.0f || floorv < -1000000.0f) return 0;
    int m = 0;
    if (cur == cur && cur > floorv) m |= 1;
    if (k60 == k60 && k60 > floorv) m |= 2;
    if (k58 != 0.0f) m |= 4;
    if (k5c != 0.0f) m |= 8;
    return m;
}
// Release: give the AI back only if the hold switched it off (aiWas = 1 when held), it is still off now, and the boss is alive.
static inline int PstHoldAiRestore(int weTurnedOff, int aiNow, int dead) { return weTurnedOff && aiNow == 0 && !dead; }
// Boss Match (#43283): a CPU fight must not keep a Practice dummy mutation. The stored [Practice] value is what the
// player chose and what an overlay save writes. The effective value is what the hooks apply. Read-only overlays
// (Ki log, frame meter, negative-Ki meter) stay as stored. Gating never writes the ini.
static inline int PstBossMatchMutates(int what) {
    return what == PS_ONE_HIT_KI || what == PS_HOLD_KI_BREAK || what == PS_HOLD_KI_V2 || what == PS_BOSS_KI_DRAIN || what == PS_KI_FLOOR || what == PS_BOSS_SPEED;
}
static inline int PstBossMatchNeutral(int what) { return what == PS_KI_FLOOR || what == PS_BOSS_SPEED ? 100 : 0; }
static inline int PstBossMatchEffective(int what, int stored, int bossMatch) {
    return bossMatch && PstBossMatchMutates(what) ? PstBossMatchNeutral(what) : stored;
}
static inline int PstBossMatchSaved(int what, int stored, int bossMatch) { (void)what; (void)bossMatch; return stored; }
// user #22942 Time of day / Weather (notes/training-time-of-day-22942.md). S = the active RenderConfig ([[B+0x4751490]]->+0x28 = GetActiveSessionObj
// 164188), the r8 of RenderFrameSettingsApply 39B570; the game does not advance the clock (TimeSpanPerDay ~1e20 s, AutoDateTimeStepType 0 every frame).
// SkylightDateTime S+0xB0 = fraction of a day (0.5 = noon), read by SkySunMoonDirections 5EBD08 as max(+B0, 0); it is not clamped above 1, so keep < 1.
// ini TimeOfDayMin (-1 = Stock, else minutes 0..1439, rounded down to 30 min) <-> internal 0 (Stock) / 1..48.
static inline int PstTodFromIni(int min) { if (min < 0) return 0; if (min > 1439) min = 1439; return min / 30 + 1; }
static inline int PstTodToIni(int v) { v = PstClamp(PS_TIME_OF_DAY, v); return v ? (v - 1) * 30 : -1; }
static inline int PstTodMinutes(int v) { return PstTodToIni(v); }  // -1 = Stock
static inline float PstTodFraction(int v) { const int m = PstTodMinutes(v); return m < 0 ? -1.0f : (float)m / 1440.0f; }
// ini <-> internal for every PS_* (identity except time of day); the ini default of a key = PstToIni(what, PstDefault(what)).
static inline int PstFromIni(int what, int ini) { return what == PS_TIME_OF_DAY ? PstTodFromIni(ini) : PstClamp(what, ini); }
static inline int PstToIni(int what, int v) { return what == PS_TIME_OF_DAY ? PstTodToIni(v) : v; }
static inline const char* PstWeatherName(int w) {
    static const char* const n[PS_WX_N] = {"Stock", "Clear", "Overcast", "Rain", "Storm", "Snow"};
    return w >= 0 && w < PS_WX_N ? n[w] : "?";
}
// Weather fields of S (re/out/renderconfig_fields.tsv; the storm targets are the ones the native storm lerp 39BED0 writes on the same S).
enum { PS_EF_CLOUDY, PS_EF_TEMP, PS_EF_HUMID, PS_EF_PRECIP, PS_EF_FOG, PS_EF_HAZE, PS_EF_CLEAR, PS_EF_RAINDROP, PS_EF_FOGSCATTER, PS_EF_GUST, PS_EF_GUSTINT, PS_EF_TREEWIND, PS_EF_N };
static inline uint32_t PstEnvFieldOff(int f) {
    static const uint32_t o[PS_EF_N] = {0x7C, 0x80, 0x84, 0x88, 0x8C, 0x90, 0x98, 0x15C8, 0x13C8, 0xF70, 0xF84, 0x1968};
    return f >= 0 && f < PS_EF_N ? o[f] : 0;
}
// Value of field f for weather preset w: 1 = write *out, 0 = leave the stock value (Stock writes nothing). Values from the shipped preset ranges
// (Cloudy 0.3..1, Precipitation 0..0.25, Temperature -50..20) and the 39BED0 storm targets (fog key-light scatter 12, gust 750 / 550, tree wind x2).
static inline int PstWeatherValue(int w, int f, float* out) {
    float v = 0.0f; int set = 0;
    switch (w) {
    case PS_WX_CLEAR:
        set = f == PS_EF_CLOUDY || f == PS_EF_CLEAR || f == PS_EF_FOG || f == PS_EF_HAZE || f == PS_EF_PRECIP || f == PS_EF_RAINDROP;
        v = f == PS_EF_CLOUDY ? 0.3f : f == PS_EF_CLEAR ? 1.0f : 0.0f; break;
    case PS_WX_OVERCAST: case PS_WX_RAIN: case PS_WX_STORM: case PS_WX_SNOW:
        set = f == PS_EF_CLOUDY || f == PS_EF_CLEAR || f == PS_EF_HUMID || f == PS_EF_PRECIP || f == PS_EF_RAINDROP;
        v = f == PS_EF_CLOUDY ? 1.0f : f == PS_EF_HUMID ? 0.7f : 0.0f;
        if (w != PS_WX_OVERCAST && f == PS_EF_PRECIP) v = w == PS_WX_STORM ? 0.25f : 0.2f;
        if (w != PS_WX_OVERCAST && f == PS_EF_RAINDROP) v = 1.0f;
        if (w == PS_WX_STORM && (f == PS_EF_FOGSCATTER || f == PS_EF_GUST || f == PS_EF_GUSTINT || f == PS_EF_TREEWIND)) {
            set = 1; v = f == PS_EF_FOGSCATTER ? 12.0f : f == PS_EF_GUST ? 750.0f : f == PS_EF_GUSTINT ? 550.0f : 2.0f; }
        if (w == PS_WX_SNOW && f == PS_EF_TEMP) { set = 1; v = -20.0f; }
        break;
    default: break;
    }
    if (set) *out = v;
    return set;
}
// Display sky scope (#38871): 0 = no memory access (unknown/loading/non-active object),
// 1 = release our previous override only (no player or event scene), 2 = apply selected overrides.
// There is deliberately no Practice-session input. eventScene also means unknown event-signature state.
static inline int PstEnvScope(int gameStateKnown, int loading, int activeObject, int playerPresent, int eventScene) {
    if (!gameStateKnown || loading || !activeObject) return 0;
    return playerPresent && !eventScene ? 2 : 1;
}
// One overridden float of S, written every frame after the native fill. Stock is remembered so switching back restores it even if the game does NOT
// re-fill S each frame (open question of the plan): stock = the value seen whenever it differs from what we wrote last frame (= the game wrote it).
// Release: the first frame without an override; restores stock only if S still holds our last write (else the game already re-filled it).
typedef struct { float stock, last; uint8_t wrote, haveStock; uint32_t refills, frames; } PstEnvField;
static inline float PstEnvApply(PstEnvField* e, float cur, float want) {
    if (!e->wrote || cur != e->last) { e->stock = cur; e->haveStock = 1; if (e->wrote) e->refills++; }
    if (e->wrote) e->frames++;
    e->last = want; e->wrote = 1;
    return want;
}
static inline int PstEnvRelease(PstEnvField* e, float cur, float* out) {
    const int r = e->wrote && e->haveStock && cur == e->last && cur != e->stock;
    if (r) *out = e->stock;
    e->wrote = 0; e->refills = e->frames = 0;
    return r;
}
// Sanity of a read: a float S field is finite and within a loose bound (garbage pointer guard; the shipped values are within -50..1000).
static inline int PstEnvSane(float v) { return v == v && v > -100000.0f && v < 100000.0f; }
