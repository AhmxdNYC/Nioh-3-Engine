// human #126496 / #126502: DMC-style "style meter" (rank D, C, B, A, S, SS, SSS + a gauge inside the rank). Pure state machine, no game access
// (tests/style_meter_test.c). Game glue: style_meter_game.h (fed once per FRAMEPROBE pump tick, game clock in ms).
// Generic (human rule #87625): no per-boss or per-move table. Events and their base points:
//   hit     the player's hit took HP off an enemy                  cfg.hit   (x variety factor, below)
//   blocked the player's hit connected but the enemy guarded it (no HP damage): blockPct % of a hit, same use / variety rules, counts
//           in the combo (game glue: SmBlkQ below decides which hits were guarded)
//   dodge   an enemy hit window opened and closed while the player had i-frames in it and took no damage   cfg.dodge
//   deflect the same, during the universal Samurai deflect (game glue: the move's action-row skill word is in [StyleMeter] DeflectSkill)
//                                                                                                              cfg.deflect
//   counter (avoid) the same, during any other skill move (martial art / weapon counter / ninjutsu / guardian spirit: the game's own
//           skill marks) cfg.dodge, shown as Counter; one counter per move between this and the hit counter below (game glue)
//           (game glue decides both signals; the rule here is only the priority: other skill -> counter, deflect -> deflect, else dodge)
//   ki      a Ki pulse landed within cfg.kiHitMs of a landed hit     cfg.ki   (human #126744: a pulse after a whiff scores 0; kiHitMs 0 = no gate)
//   startup (#126821) a NEW use's points x (1 + startupPct % x how far its first hit window opened between startupMinF and startupMaxF, 60 fps
//           frames, from the frame meter's own run: run.hit[0]). Slow, risky startups earn more; unknown startup = x1.
//   counter (#126843) + cfg.counter once per use when the player had i-frames while an enemy hit window was open in the same action run
//           (an invincible counter / attack through an attack), then landed the hit.
//   skill   (#126850 soul core, #126854 ninjutsu) cfg.soul / cfg.nin, only in combat (a landed hit within cfg.combatMs): no out-of-fight farming.
//   style   (#126843 Samurai <-> Ninja switch) cfg.style, only in combat and only once per landed hit between switches (no switch spam).
//   damaged the player lost HP: one rank down and the gauge empty (rank D with an empty gauge = the meter is idle / hidden)
// Variety: every new "use" (player action id + action run) is compared with the last SM_HIST uses; each earlier use of the same action id
// multiplies the points by repeatPct % (floor repeatFloorPct %). Further hits of the same use (multi-hit, several enemies) get multiPct % of that.
// Multi-hit cap: per use only the first multiHitCap hits (default 4, 0 = no cap) score and count toward the combo; further hits of the same
// use score 0, add nothing, do not refresh the decay delay and do not raise the combo (s->hits still counts every real hit). The summoned
// guardian spirit's hits are one use of their own (SmSummonHits), capped the same way.
// The game glue passes a use key, not the raw run: back-to-back segments of one martial art (same skill word) are one use (SmChainStep).
// Loop penalty: the ordered act ids of the player's uses are tracked; a trailing loop of period 1 (A A A ..) or period 2 (A B A B ..) that
// already completed loopReps cycles (default 3: the 4th A in a row, the 7th use of A B A B A B A) makes every further use of it score 0 for
// all its hits and take loopPts (default 10) off the gauge once per use (draining like decay: below 0 drops a rank, at D it ends idle;
// the decay delay is not refreshed). Any other move breaks the loop; an unknown act (SM_ACT_NONE / 0) breaks it too. loopReps 0 = off.
// The avoid events are moves in the same sequence (human 2026-11-25: "3 deflects in a row, the 3rd gets no points AND you lose -10"):
// a clean dodge / deflect is one step with a pseudo act id (SM_LOOP_DODGE / SM_LOOP_DEFLECT); an avoid counter is a step of its own
// skill move (act / run, the same use as that move's hits). A looped one scores 0 and costs loopPts like a looped attack use. Bonuses
// that belong to a use (counter, startup, weapon switch, guardian spirit / martial art, combo feed, Burst Break hit) are not paid for a
// looped use; Ki pulses, ninjutsu, soul core and style switches are not in the sequence.
// Move variety (human 2026-11-25): a run keeps the set of distinct player moves that scored (SmVarietyMove: the act id of every new player
// use that scored a hit or a blocked hit, not looped / penalized uses, not unknown ids, not the summoned spirit's hits; plus pseudo ids
// SM_VAR_DODGE .. SM_VAR_KI for a scored dodge / deflect / avoid counter / Ki pulse, fed by the game glue). Each time the distinct count
// reaches varStep, 2 x varStep, .. up to varMax, the run pays varBase, then a step of varInc that grows by varGrow each milestone
// (defaults 5 moves +20, 10 +30, 15 +43, 20 +59, .., 60 +295). The set does not reset with rank decay / an idle meter; the game glue resets it (SmVarietyReset) at a boss fight start, player
// death and a load.
// Decay: decayDelayMs after the last scoring event the gauge drains decayPerS points per second, +decayRankPct % per rank above D;
// draining below 0 drops a rank (gauge refilled from that rank's top), at D it ends empty (idle).
// Promotion: rankPts[r] points fill rank r; the overflow carries into the next rank. SSS caps at a full gauge.
#ifndef STYLE_METER_H
#define STYLE_METER_H
#include <stdint.h>

#define SM_RANKS 7
#define SM_HIST 8
#define SM_HIT_PTS 20  // #127568 human Main PC tuning (was 10)
#define SM_DODGE_PTS 45  // #127568 (was 30)
#define SM_DEFLECT_PTS 23  // [StyleMeter] DeflectPts: human 2026-11-25 half a dodge (was 45) so other defensive options are worth using
#define SM_DEFLECT_SKILL 0x67D2u  // [StyleMeter] DeflectSkill: action-row skill word of the universal Samurai deflect (live 2026-11-23: one word
                                  // on 15 directional deflect actions 25503..25621, 276 uses, never an attack frame; weapon counters have their own)
#define SM_DEFLECT_SKILLS 8
#define SM_KI_PTS 30  // #127568 (was 20)
#define SM_KI_PERFECT_PTS 15  // #127075 extra on a perfect Ki pulse (the frame meter's teal check)
#define SM_NIN_GAP_MS 500     // #127065 one ninjutsu per throw: gauge drops closer than this are the same throw
#define SM_REPEAT_PCT 80  // #127568 (was 60)
#define SM_REPEAT_FLOOR_PCT 40  // #127568 (was 10)
#define SM_MULTI_PCT 100  // #127568 (was 50)
#define SM_DECAY_DELAY_MS 500   // #127568 human tuning (#126945 1.5 s, then 0.5 s); was 4000,  // human #126785: was 2500
#define SM_DECAY_PER_S 30   // #127568 (was 12);   // human #126785: harder to lose rank (2.5x ranks drain at the old 12/s)
#define SM_DECAY_RANK_PCT 50  // #127568 (was 35)
#define SM_DECAY_RAMP_PCT 150 // #127568 (was 0);     // human #126972: +N % drain per idle second after the delay (fast fade after a combo ends); 0 = flat
#define SM_COMBAT_MS 8000
#define SM_KI_HIT_MS 3000        // human #126744: whiffs must not raise the meter (Ki pulse only counts in a combo that landed)
#define SM_DAMAGE_GRACE_MS 1500  // live #126502: one multi-hit enemy combo = one rank drop
#define SM_STARTUP_MIN_F 15   // #126821: startups at or below this many 60 fps frames get no bonus
#define SM_STARTUP_MAX_F 60   // ... at or above this the full bonus
#define SM_STARTUP_PCT 100    // full bonus: points x2
#define SM_COUNTER_PTS 30
#define SM_NIN_PTS 20
#define SM_SOUL_PTS 40
#define SM_STYLE_PTS 15
#define SM_SWAP_PTS 50      // #127057 weapon swap attack (human 2026-11-25: doubled from 25, hard to do mid combat)
#define SM_SWAP_MS 600      // a move that starts within this of a weapon slot change is the swap attack (old rule, SmSwapHit)
#define SM_SWITCH_MS 3000   // [StyleMeter] WeaponSwitchMs: a hit with another weapon within this of the last hit is a weapon switch
#define SM_GS_PTS 25        // #127213 guardian spirit skill (Spirit Force gauge spent at a move start)
#define SM_GS_MS 1000       // ... the gauge drop must come within this of the move start
#define SM_COMBO_MS 3000        // human: combo = landed hits each within this of the previous one; getting hit breaks it
#define SM_COMBO_STEP 10        // ... a bonus every this many hits in one combo
#define SM_COMBO_BASE 25        // ... first milestone's bonus, each later one +SM_COMBO_INC, never above SM_COMBO_MAX (25, 40, 55, 70, 85, 100, 100..)
#define SM_COMBO_INC 15
#define SM_COMBO_MAX 100
#define SM_GS_HOLD_MS 1500      // human: a guardian spirit spend in combat holds the drain this long (the summoned spirit's first hit comes ~1.5 s later)
#define SM_GS_SUMMON_MS 8000  // human: hits by the summoned guardian spirit (R2 + Triangle / RT + Y) count this long after its Spirit Force spend
#define SM_GS_DROP 1.f      // ... by at least this much below the value just before the move
#define SM_MA_PTS SM_NIN_PTS // #127295 martial art: same as the ninjutsu bonus (ini MartialArtPts unset = NinjutsuPts)
#define SM_SKILL_NONE_ID 0xBA71u  // #127295 action-row skill word the game's AttackSkill test (cond 0x2F, 0x2EA15B) treats as none
#define SM_ACT_NONE 0xFFFFFFFFu
#define SM_LOOP_REPS 2      // [StyleMeter] LoopReps: cycles of a 1- or 2-move loop allowed before each further use costs points (0 = off, 2..7; human 2026-11-25: A A or A B A B is fine, the next repeat costs)
#define SM_LOOP_DODGE 0xFFFFFF01u    // loop sequence pseudo act ids of the avoid events (no real action id is this high)
#define SM_LOOP_DEFLECT 0xFFFFFF02u
#define SM_LOOP_COUNTER 0xFFFFFF03u  // an avoid counter with no move key (SmOnAvoidCounter; the game glue passes the move)
#define SM_LOOP_PTS 10      // [StyleMeter] LoopPenaltyPts: points taken off per looped use
#define SM_MULTI_CAP 4      // [StyleMeter] MultiHitCap: hits per use that score / count toward the combo (0 = no cap)
#define SM_BURST_PTS 40     // human: [StyleMeter] BurstBreakPts, a boss's red (burst / GreatAttack) attack interrupted by your hit during its wind-up
#define SM_BURST_MS 1500    // ... BurstBreakMs: the boss's burst action must end within this of the hit that broke it
#define SM_BURST_CTR_ACT 1095u  // ... BurstCounterAct: the player's burst counter action id (live 2026-11-25: act 0x447 every time, 1 f startup,
                               // pad B+RT; its hit lands after the red attack is out, the boss then goes to its burst-broken action). 0 = off
#define SM_BURST_AVOID_MS 300  // an avoid (dodge / deflect / counter) this soon after a burst-counter Burst Break is that same event (not paid)
#define SM_VAR_STEP 5         // [StyleMeter] VarietyStep: a bonus every this many distinct moves in one run
#define SM_VAR_BASE 20        // ... VarietyBonus: first milestone's bonus (0 = off)
#define SM_VAR_INC 10         // ... VarietyBonusStep: the 2nd milestone pays this much more than the 1st
#define SM_VAR_GROW 3         // ... VarietyBonusGrow: each later step grows by this much (20, 30, 43, 59, 78, 100, .. 295 at 60 moves)
#define SM_VAR_MAX 60         // ... VarietyMax: no milestone past this many distinct moves (at most SM_VAR_IDS)
#define SM_VAR_IDS 64         // distinct move ids remembered per run
// pseudo move ids for non-hit moves (variety set only; real act ids are small)
#define SM_VAR_DODGE 0xFFFFFF01u
#define SM_VAR_DEFLECT 0xFFFFFF02u
#define SM_VAR_COUNTER 0xFFFFFF03u
#define SM_VAR_KI 0xFFFFFF04u
#define SM_BLOCK_PCT 50       // [StyleMeter] BlockPct: a guarded (blocked) hit is worth this % of a normal hit (human: 'half the points'; 0 = off)

static const char* const sm_rank_name[SM_RANKS] = {"D", "C", "B", "A", "S", "SS", "SSS"};
// Words of the approved sprite set (Temp/rank126376/sprites/sprites.py RANKS).
static const char* const sm_rank_word[SM_RANKS] = {"Dull", "Cunning", "Bold", "Ardent", "Asura", "Sword Saint", "Heavenly"};
// human #126755 too easy (SS in ~40 hits), #126766 4x, #126785 4x too hard (C after 52 hits): 2.5x {40, 60, 80, 100, 130, 160, 200}
static const float sm_rank_pts_default[SM_RANKS] = {120.f, 180.f, 130.f, 180.f, 250.f, 350.f, 500.f};  // human 2026-11-25: D and C twice as long (was 60 90; #127568 was 100 150 200 250 325 400 500)

typedef struct {
    float hit, dodge, ki;                 // base points per event
    float deflect;                        // a clean avoid with the deflect signal, not a skill move
    int repeatPct, repeatFloorPct, multiPct;
    int decayDelayMs; float decayPerS; int decayRankPct; int decayRampPct;
    int damageGraceMs;                    // further damage within this window after a drop costs no extra rank
    float kiPerfect;                      // #127075 extra points for a perfect Ki pulse (on top of ki)
    int kiHitMs;                          // a Ki pulse scores only this long after the last landed hit (0 = always)
    int combatMs;                         // the meter stays up this long after the last event when no boss fight is live
    int startupMinF, startupMaxF, startupPct;  // #126821 startup scaling
    float counter, nin, soul, style;      // #126843 / #126854 / #126850 bonus points
    float swap; int swapMs;               // #127057 weapon swap attack bonus / window
    int switchMs;                         // weapon switch: the last hit with the other weapon must be this recent
    float gs; int gsMs;                   // #127213 guardian spirit skill bonus / drop window from the move start
    int gsSummonMs;                       // human: summoned guardian spirit hits count this long after the summon's gauge spend (0 = off)
    int comboMs, comboStep, comboBase, comboInc, comboMax;  // human: combo window / milestone size / milestone bonus (0 base = no bonus)
    float ma;                             // #127295 martial art bonus (default = nin)
    int loopReps; float loopPts; int multiHitCap;  // loop penalty cycles (0 = off) / points per looped use; hits per use that score (0 = all)
    float burst; int burstMs, burstSkill; // human: Burst Break bonus / hit -> action end window / 1 = only skill / guardian spirit hits count
    uint32_t burstCtrAct;                 // the player's burst counter action id (its hit breaks a red attack that is already out; 0 = off)
    int varStep, varBase, varInc, varMax, varGrow; // human: move variety milestone size / first bonus / bonus step / last milestone (distinct moves)
    int blockPct;                         // a hit the enemy guarded is worth this % of a hit (0 = blocked hits score nothing)
    float rankPts[SM_RANKS];
} SmCfg;

typedef struct { int inWin, sawIfr, hit, sawDeflect, sawSkill; } SmDodge;
enum { SM_AV_NONE = 0, SM_AV_DODGE = 1, SM_AV_DEFLECT = 2, SM_AV_COUNTER = 3 };

typedef struct {
    int rank;               // 0 = D .. 6 = SSS
    float pts;              // points inside the rank, 0 .. rankPts[rank]
    int active;             // 0 = idle (D, empty): hidden
    double lastScore;       // game ms of the last scoring event (decay delay)
    double lastEvent;       // game ms of the last event of any kind (combat timeout)
    uint32_t hist[SM_HIST]; int histN, histPos;
    uint32_t useAct; uint64_t useRun; float useFactor; int useOn;
    SmDodge dodge;
    double lastHit;         // game ms of the last landed hit (Ki pulse gate)
    double lastDamaged;     // game ms of the last rank drop (damage grace), <0 = none
    uint32_t hitsAtStyle;   // s->hits at the last scored style switch (one switch bonus per landed hit)
    uint32_t hits, dodges, kis, damaged;   // counters (log / tests)
    uint32_t counters, nins, souls, styles;
    uint32_t soulsScored, soulsAtStyle;  // #126913 soul core uses that scored / soulsScored at the last scored style switch
    double lastStyleT;      // #127065 game ms of the last scored style switch (a first switch counts even from idle)
    uint32_t kiPerfects;
    double lastSwap; int swapPaid;   // #127057 game ms of the last weapon slot change / its swap attack already scored
    uint32_t swaps, swapHits;
    int hitSlot; double hitSlotT;    // weapon slot of the last landed hit (-1 unknown) / its game ms (weapon switch bonus)
    uint32_t gsUses, gsHits;  // #127213 guardian spirit skill moves seen / scored
    uint32_t maUses, maHits;  // #127295 martial art moves seen / scored
    double lastSoul;        // game ms of the last scored soul core use (a switch inside that form counts, #126913)
    uint32_t deflects;      // clean avoids that were deflects
    uint32_t counterAvoids; // clean avoids during a skill move that counted as that move's counter
    uint32_t combo, comboBest, comboBonuses;  // human: landed hits in the current combo / longest combo / milestone bonuses paid
    double comboT;          // game ms of the combo's last hit
    uint32_t lpA, lpB; int lpN1, lpN2;   // loop penalty: act of the last / the one before use, trailing period-1 / period-2 run lengths (uses)
    uint64_t lpRun; int lpOn, lpPen, lpHits;  // the current player use (own run key), it is a looped use, its hits so far (multi-hit cap)
    uint32_t countedHits, cappedHits, loopPens; float loopLost;  // hits that scored (combo feed) / over the cap / looped uses / points they cost
    uint32_t bursts;        // human: Burst Breaks (a boss's red attack interrupted in its wind-up)
    uint32_t blocked;       // hits the enemy guarded that scored (also counted in hits)
    uint32_t varIds[SM_VAR_IDS]; int varN;  // human: distinct moves that scored this run (SmVarietyMove)
    uint32_t varBonuses; int varMile; float varLastPts, varPts;  // variety milestones paid / the last one (distinct moves, bonus) / all bonus points
} SmState;

static inline void SmCfgDefaults(SmCfg* c) {
    c->hit = (float)SM_HIT_PTS; c->dodge = (float)SM_DODGE_PTS; c->deflect = (float)SM_DEFLECT_PTS; c->ki = (float)SM_KI_PTS; c->kiPerfect = (float)SM_KI_PERFECT_PTS;
    c->repeatPct = SM_REPEAT_PCT; c->repeatFloorPct = SM_REPEAT_FLOOR_PCT; c->multiPct = SM_MULTI_PCT;
    c->decayDelayMs = SM_DECAY_DELAY_MS; c->decayPerS = (float)SM_DECAY_PER_S; c->decayRankPct = SM_DECAY_RANK_PCT; c->decayRampPct = SM_DECAY_RAMP_PCT;
    c->startupMinF = SM_STARTUP_MIN_F; c->startupMaxF = SM_STARTUP_MAX_F; c->startupPct = SM_STARTUP_PCT;
    c->counter = (float)SM_COUNTER_PTS; c->nin = (float)SM_NIN_PTS; c->soul = (float)SM_SOUL_PTS; c->style = (float)SM_STYLE_PTS; c->swap = (float)SM_SWAP_PTS; c->swapMs = SM_SWAP_MS; c->switchMs = SM_SWITCH_MS;
    c->gs = (float)SM_GS_PTS; c->gsMs = SM_GS_MS; c->gsSummonMs = SM_GS_SUMMON_MS;
    c->comboMs = SM_COMBO_MS; c->comboStep = SM_COMBO_STEP; c->comboBase = SM_COMBO_BASE; c->comboInc = SM_COMBO_INC; c->comboMax = SM_COMBO_MAX;
    c->burst = (float)SM_BURST_PTS; c->burstMs = SM_BURST_MS; c->burstSkill = 0; c->burstCtrAct = SM_BURST_CTR_ACT;
    c->ma = c->nin;  // #127295 the martial art bonus follows the ninjutsu bonus unless set
    c->loopReps = SM_LOOP_REPS; c->loopPts = (float)SM_LOOP_PTS; c->multiHitCap = SM_MULTI_CAP;
    c->combatMs = SM_COMBAT_MS; c->kiHitMs = SM_KI_HIT_MS; c->damageGraceMs = SM_DAMAGE_GRACE_MS;
    c->blockPct = SM_BLOCK_PCT;
    c->varStep = SM_VAR_STEP; c->varBase = SM_VAR_BASE; c->varInc = SM_VAR_INC; c->varMax = SM_VAR_MAX; c->varGrow = SM_VAR_GROW;
    for (int i = 0; i < SM_RANKS; ++i) c->rankPts[i] = sm_rank_pts_default[i];
}
// Clamp ini values into sane ranges (no zero-size rank, no negative points).
static inline void SmCfgClamp(SmCfg* c) {
    if (c->hit < 0) c->hit = 0; if (c->dodge < 0) c->dodge = 0; if (c->deflect < 0) c->deflect = 0; if (c->ki < 0) c->ki = 0; if (c->kiPerfect < 0) c->kiPerfect = 0;
    if (c->repeatPct < 0) c->repeatPct = 0; if (c->repeatPct > 100) c->repeatPct = 100;
    if (c->repeatFloorPct < 0) c->repeatFloorPct = 0; if (c->repeatFloorPct > 100) c->repeatFloorPct = 100;
    if (c->multiPct < 0) c->multiPct = 0; if (c->multiPct > 100) c->multiPct = 100;
    if (c->decayDelayMs < 0) c->decayDelayMs = 0; if (c->decayPerS < 0) c->decayPerS = 0; if (c->decayRankPct < 0) c->decayRankPct = 0; if (c->decayRampPct < 0) c->decayRampPct = 0;
    if (c->combatMs < 0) c->combatMs = 0;
    if (c->damageGraceMs < 0) c->damageGraceMs = 0;
    if (c->kiHitMs < 0) c->kiHitMs = 0;
    if (c->startupMinF < 0) c->startupMinF = 0; if (c->startupMaxF <= c->startupMinF) c->startupMaxF = c->startupMinF + 1;
    if (c->startupPct < 0) c->startupPct = 0; if (c->startupPct > 500) c->startupPct = 500;
    if (c->counter < 0) c->counter = 0; if (c->nin < 0) c->nin = 0; if (c->soul < 0) c->soul = 0; if (c->style < 0) c->style = 0;
    if (c->swap < 0) c->swap = 0; if (c->swapMs < 0) c->swapMs = 0; if (c->swapMs > 5000) c->swapMs = 5000; if (c->switchMs < 0) c->switchMs = 0; if (c->switchMs > 30000) c->switchMs = 30000;
    if (c->gs < 0) c->gs = 0; if (c->gsMs < 0) c->gsMs = 0; if (c->gsMs > 5000) c->gsMs = 5000;
    if (c->gsSummonMs < 0) c->gsSummonMs = 0; if (c->gsSummonMs > 30000) c->gsSummonMs = 30000;
    if (c->comboMs < 200) c->comboMs = 200; if (c->comboMs > 20000) c->comboMs = 20000; if (c->comboStep < 2) c->comboStep = 2; if (c->comboStep > 1000) c->comboStep = 1000;
    if (c->comboBase < 0) c->comboBase = 0; if (c->comboInc < 0) c->comboInc = 0; if (c->comboMax < c->comboBase) c->comboMax = c->comboBase;
    if (c->ma < 0) c->ma = 0;
    if (c->loopReps < 0) c->loopReps = 0; if (c->loopReps == 1) c->loopReps = 2; if (c->loopReps > 7) c->loopReps = 7;
    if (c->loopPts < 0) c->loopPts = 0; if (c->loopPts > 1000) c->loopPts = 1000; if (c->multiHitCap < 0) c->multiHitCap = 0; if (c->multiHitCap > 64) c->multiHitCap = 64;
    if (c->burst < 0) c->burst = 0; if (c->burstMs < 100) c->burstMs = 100; if (c->burstMs > 10000) c->burstMs = 10000; c->burstSkill = c->burstSkill ? 1 : 0;
    if (c->blockPct < 0) c->blockPct = 0; if (c->blockPct > 100) c->blockPct = 100;
    if (c->varStep < 1) c->varStep = 1; if (c->varStep > SM_VAR_IDS) c->varStep = SM_VAR_IDS; if (c->varBase < 0) c->varBase = 0; if (c->varInc < 0) c->varInc = 0; if (c->varGrow < 0) c->varGrow = 0;
    if (c->varMax < 0) c->varMax = 0; if (c->varMax > SM_VAR_IDS) c->varMax = SM_VAR_IDS;
    for (int i = 0; i < SM_RANKS; ++i) if (!(c->rankPts[i] >= 1.f)) c->rankPts[i] = 1.f;
}
static inline void SmReset(SmState* s) {
    uint8_t* p = (uint8_t*)s; for (unsigned i = 0; i < sizeof *s; ++i) p[i] = 0;
    s->useAct = SM_ACT_NONE; s->lastScore = s->lastEvent = s->lastHit = s->lastSoul = s->lastSwap = s->lastStyleT = s->hitSlotT = s->comboT = -1e18; s->lastDamaged = -1; s->hitSlot = -1;
    s->lpA = s->lpB = SM_ACT_NONE;
}
static inline float SmGauge(const SmState* s, const SmCfg* c) {
    if (!s->active) return 0.f;
    const float g = s->pts / c->rankPts[s->rank];
    return g < 0.f ? 0.f : g > 1.f ? 1.f : g;
}
static inline void SmAdd(SmState* s, const SmCfg* c, float pts) {
    if (pts <= 0.f) return;
    s->active = 1; s->pts += pts;
    while (s->rank < SM_RANKS - 1 && s->pts >= c->rankPts[s->rank]) { s->pts -= c->rankPts[s->rank]; s->rank++; }
    if (s->rank == SM_RANKS - 1 && s->pts > c->rankPts[s->rank]) s->pts = c->rankPts[s->rank];
}
// Points lost (loop penalty): drains like SmTick's decay (below 0 drops a rank, refilled from that rank's top; at D it ends empty / idle).
// Does not touch lastScore. Nothing to lose when idle.
static inline void SmSub(SmState* s, const SmCfg* c, float pts) {
    if (!(pts > 0.f) || !s->active) return;
    s->pts -= pts;
    while (s->pts < 0.f) {
        if (s->rank == 0) { s->pts = 0.f; s->active = 0; s->useOn = 0; break; }
        s->rank--; s->pts += c->rankPts[s->rank];
    }
}
// Loop penalty, once per new player use with act id `act`: 1 = the use continues a period-1 (A A A ..) or period-2 (A B A B ..) loop that
// already completed loopReps cycles (period 1: more than loopReps uses in a row, period 2: more than 2 x loopReps). Unknown act breaks it.
static inline int SmLoopStep(SmState* s, const SmCfg* c, uint32_t act) {
    if (act == SM_ACT_NONE || act == 0) { s->lpA = s->lpB = SM_ACT_NONE; s->lpN1 = s->lpN2 = 0; return 0; }
    const int n1 = act == s->lpA ? s->lpN1 + 1 : 1;
    const int n2 = s->lpA == SM_ACT_NONE || act == s->lpA ? 1 : act == s->lpB && s->lpN2 >= 2 ? s->lpN2 + 1 : 2;
    s->lpN1 = n1; s->lpN2 = n2; s->lpB = s->lpA; s->lpA = act;
    return c->loopReps > 0 && (n1 > c->loopReps || n2 > 2 * c->loopReps);
}
static inline void SmLoopPay(SmState* s, const SmCfg* c) { s->loopPens++; s->loopLost += c->loopPts; SmSub(s, c, c->loopPts); }
// A player use (act, run key): the first call of a new run steps the loop (and pays a looped use's penalty); returns 1 = it is looped.
static inline int SmLoopUse(SmState* s, const SmCfg* c, uint32_t act, uint64_t run) {
    if (!s->lpOn || run != s->lpRun) {
        s->lpOn = 1; s->lpRun = run; s->lpHits = 0; s->lpPen = SmLoopStep(s, c, act);
        if (s->lpPen) SmLoopPay(s, c);
    }
    return s->lpPen;
}
// A one-shot move with no use of its own (pseudo act id): one step; returns 1 = looped (penalty paid). The current use is untouched, so
// the further hits of an attack that a dodge happened in stay that attack's use.
static inline int SmLoopEvent(SmState* s, const SmCfg* c, uint32_t act) {
    const int pen = SmLoopStep(s, c, act);
    if (pen) SmLoopPay(s, c);
    return pen;
}
// Move variety: a new run (fight start / death / load). The meter itself is untouched.
static inline void SmVarietyReset(SmState* s) { s->varN = 0; s->varMile = 0; s->varLastPts = 0.f; }
// Move variety: move `id` scored (a real act id or SM_VAR_*). A new distinct move that brings the count to a milestone (every varStep, up to
// varMax) pays the bonus (added to the gauge, refreshes the decay delay) and returns it; else 0 (s->varMile = the count it paid at).
static inline float SmVarietyMove(SmState* s, const SmCfg* c, double t, uint32_t id) {
    if (id == SM_ACT_NONE || id == 0 || s->varN >= c->varMax || s->varN >= SM_VAR_IDS) return 0.f;
    for (int i = 0; i < s->varN; ++i) if (s->varIds[i] == id) return 0.f;
    s->varIds[s->varN++] = id;
    if (c->varStep <= 0 || s->varN % c->varStep) return 0.f;
    const int k = s->varN / c->varStep - 1;  // 0 = the first milestone; the step itself grows by varGrow each milestone
    const float b = (float)(c->varBase + k * c->varInc + c->varGrow * k * (k - 1) / 2);
    if (!(b > 0.f) || c->varBase <= 0) return 0.f;  // VarietyBonus 0 = off (the count still runs)
    SmAdd(s, c, b); s->lastEvent = s->lastScore = t;
    s->varBonuses++; s->varMile = s->varN; s->varLastPts = b; s->varPts += b;
    return b;
}
// Variety factor for a NEW use of action id `act` (1.0 for an unknown id: no data, no penalty).
static inline float SmVariety(const SmState* s, const SmCfg* c, uint32_t act) {
    if (act == SM_ACT_NONE || act == 0) return 1.f;
    int n = 0; for (int i = 0; i < s->histN; ++i) if (s->hist[i] == act) n++;
    float f = 1.f; for (int i = 0; i < n; ++i) f *= (float)c->repeatPct / 100.f;
    const float fl = (float)c->repeatFloorPct / 100.f;
    return f < fl ? fl : f;
}
// #126821 startup multiplier: startupF = 60 fps frames from the action start to its first hit window (< 0 = unknown -> 1).
static inline float SmStartupMult(const SmCfg* c, float startupF) {
    if (!(startupF >= 0.f) || c->startupPct <= 0) return 1.f;
    float k = (startupF - (float)c->startupMinF) / (float)(c->startupMaxF - c->startupMinF);
    k = k < 0.f ? 0.f : k > 1.f ? 1.f : k;
    return 1.f + k * (float)c->startupPct / 100.f;
}
// One hit landed. act = the player's action id at the hit, run = a key of that action run (a new run of the same id = a new use).
// startupF: that run's startup (frames, < 0 unknown); counter: 1 = the run had i-frames inside an enemy hit window (bonus once per use).
// summon = 1: a summoned guardian spirit hit (SmSummonHits does its own cap; no loop tracking, so its hits between the player's do not
// split or repeat the player's use).
// pct: % of the hit's value (100 = a hit that took HP, blockPct = a guarded hit).
static inline float SmOnHitUsePct(SmState* s, const SmCfg* c, double t, uint32_t act, uint64_t run, float startupF, int counter, int summon, int pct) {
    float f, bonus = 0.f;
    if (!summon) {  // loop penalty / multi-hit cap, per player use (own run key)
        SmLoopUse(s, c, act, run);
        if (s->lpPen || (c->multiHitCap > 0 && s->lpHits >= c->multiHitCap)) {  // scores 0, no decay refresh, no combo
            if (!s->lpPen) s->cappedHits++;
            s->hits++; s->lastEvent = s->lastHit = t; return 0.f;
        }
        s->lpHits++;
    }
    if (s->useOn && act == s->useAct && run == s->useRun) f = s->useFactor * (float)c->multiPct / 100.f;
    else {
        f = SmVariety(s, c, act) * SmStartupMult(c, startupF);
        if (counter && c->counter > 0.f) { bonus = c->counter; s->counters++; }
        const int newUse = !s->useOn || run != s->useRun;  // a chained segment (same use key, new act) is not a new move
        s->useOn = 1; s->useAct = act; s->useRun = run; s->useFactor = f;
        if (!summon && newUse) SmVarietyMove(s, c, t, act);  // human: move variety (the glue reads varBonuses for the banner)
        if (act != SM_ACT_NONE && act != 0) { s->hist[s->histPos] = act; s->histPos = (s->histPos + 1) % SM_HIST; if (s->histN < SM_HIST) s->histN++; }
    }
    const float p = (c->hit * f + bonus) * (float)pct / 100.f;
    SmAdd(s, c, p); s->hits++; s->lastEvent = s->lastHit = t; if (p > 0.f) s->lastScore = t;
    s->countedHits++;
    return p;
}
// Multi-segment moves: one martial art can play as several back-to-back action runs, each a new act id. Switch Glaive Infinite Retribution
// (game log 2026-11-25): 25351 lead-in (3 hits) -> 25352 spin (2..15 hits) -> 25353 finisher (0..3 hits), all skill word 0x42B, each run
// starting on the tick the last one ended. With the run as the use key the multi-hit cap allowed 4 per segment (up to 10 combo per
// use). SmChainStep, once per tick with the player's run, returns the use key to pass as `run` to SmOnHit*: a run that starts directly
// after the last one (no tick between, no idle / unknown act) whose action row is a skill row with the same skill word as the last run's,
// and whose act id is not yet in the chain, keeps the chain's key (same use: one cap, one loop step); anything else starts a new use
// keyed by its own run. A repeat of an act in the chain (A B C A) is a new use, so re-casting the same martial art back to back still
// counts. skill = SmSkillWord's result (1 skill row, 0 not, -1 unreadable this tick): a contiguous run stays in the chain while its row
// is unreadable, decided once on the first readable tick (or as a new use after SM_CHAIN_WAIT ticks unreadable).
#define SM_CHAIN_ACTS 8
#define SM_CHAIN_WAIT 30
#define SM_CHAIN_NOSKILL 0xFFFFFFFFu
typedef struct { uint64_t run, key; uint32_t skw, prevSkw; int n, pending, wait; uint32_t acts[SM_CHAIN_ACTS]; } SmChain;
static inline void SmChainReset(SmChain* ch) {
    ch->run = ch->key = 0; ch->skw = ch->prevSkw = SM_CHAIN_NOSKILL; ch->n = ch->pending = ch->wait = 0;
}
static inline uint64_t SmChainStep(SmChain* ch, uint64_t run, uint32_t act, int skill, uint32_t skw) {
    if (!run || act == SM_ACT_NONE || act == 0) { SmChainReset(ch); return run; }
    if (run != ch->run) {
        const int contig = ch->run != 0 && ch->skw != SM_CHAIN_NOSKILL;
        int seen = 0; for (int i = 0; i < ch->n; ++i) if (ch->acts[i] == act) seen = 1;
        ch->prevSkw = ch->skw; ch->run = run; ch->skw = SM_CHAIN_NOSKILL; ch->wait = 0;
        if (contig && !seen && ch->n < SM_CHAIN_ACTS) { ch->pending = 1; ch->acts[ch->n++] = act; }  // tentatively the same use
        else { ch->pending = 0; ch->key = run; ch->acts[0] = act; ch->n = 1; }
    }
    if (skill >= 0) {
        const uint32_t w = skill > 0 ? skw : SM_CHAIN_NOSKILL;
        if (ch->pending && w != ch->prevSkw) { ch->key = run; ch->acts[0] = act; ch->n = 1; }  // not the same move: a new use
        ch->pending = 0; ch->skw = w;
    } else if (ch->pending && ++ch->wait > SM_CHAIN_WAIT) { ch->pending = 0; ch->key = run; ch->acts[0] = act; ch->n = 1; }
    return ch->key;
}
static inline float SmOnHitUse(SmState* s, const SmCfg* c, double t, uint32_t act, uint64_t run, float startupF, int counter, int summon) { return SmOnHitUsePct(s, c, t, act, run, startupF, counter, summon, 100); }
static inline float SmOnHitPct(SmState* s, const SmCfg* c, double t, uint32_t act, uint64_t run, float startupF, int counter, int pct) { return SmOnHitUsePct(s, c, t, act, run, startupF, counter, 0, pct); }
static inline float SmOnHitEx(SmState* s, const SmCfg* c, double t, uint32_t act, uint64_t run, float startupF, int counter) { return SmOnHitUsePct(s, c, t, act, run, startupF, counter, 0, 100); }
// A hit the enemy guarded: blockPct % of the same hit (variety, multi-hit, startup and counter rules unchanged). It is a landed hit for the
// Ki pulse gate / combat / style switch (s->hits), and s->blocked. blockPct 0 = nothing happens (the hit is not counted at all).
static inline float SmOnBlockedHit(SmState* s, const SmCfg* c, double t, uint32_t act, uint64_t run, float startupF, int counter) {
    if (c->blockPct <= 0) return 0.f;
    s->blocked++;
    return SmOnHitPct(s, c, t, act, run, startupF, counter, c->blockPct);
}
// Guarded-hit detection (game glue feeds it from the hit apply 327DEC, which calls HP damage 787268 once and Ki damage 844604 once per hit,
// both with the same hit object). One entry per (hit object, victim): SM_BLK_HP = the hit took HP (raw > 0), SM_BLK_HP0 = the HP damage
// call came with 0, SM_BLK_KI = the hit did Ki damage. An entry is decided one tick after it was first noted (so both calls of one hit are
// in): blocked = Ki damage and no HP damage. A hit that did HP damage is never a blocked hit (no double count). A 0 HP call without Ki
// damage is NOT a guard: live log 2026-11-25, 225 of 229 such 'blocked' hits were the player's own deflect (skill 0x67D2) / burst counter
// (act 0x447) touching the attacker (Ki 0), each also scored as Deflect / Dodge; the real guards (boss in its guard action) had Ki 120-146.
#define SM_BLK_Q 64
enum { SM_BLK_KI = 1, SM_BLK_HP0 = 2, SM_BLK_HP = 4 };
typedef struct { uint64_t hit, vic; float ki; uint8_t fl, age; } SmBlkEv;
typedef struct { SmBlkEv e[SM_BLK_Q]; int n; uint32_t dropped; } SmBlkQ;
static inline void SmBlkNote(SmBlkQ* q, uint64_t hit, uint64_t vic, int fl, float ki) {
    if (!hit || !vic || !fl) return;
    for (int i = 0; i < q->n; ++i) if (q->e[i].hit == hit && q->e[i].vic == vic) { q->e[i].fl |= (uint8_t)fl; if (ki > 0.f) q->e[i].ki += ki; return; }
    if (q->n >= SM_BLK_Q) { q->dropped++; return; }
    SmBlkEv* e = &q->e[q->n++]; e->hit = hit; e->vic = vic; e->fl = (uint8_t)fl; e->ki = ki > 0.f ? ki : 0.f; e->age = 0;
}
// Once per tick: decides the entries noted before this tick, ages the new ones. Returns the number of blocked hits; the first maxOut are
// copied to out (log). Decided entries are removed.
static inline int SmBlkTake(SmBlkQ* q, SmBlkEv* out, int maxOut) {
    int nb = 0, k = 0;
    for (int i = 0; i < q->n; ++i) {
        SmBlkEv e = q->e[i];
        if (!e.age) { e.age = 1; q->e[k++] = e; continue; }
        if (!(e.fl & SM_BLK_HP) && (e.fl & SM_BLK_KI) && e.ki > 0.f) { if (out && nb < maxOut) out[nb] = e; nb++; }
    }
    q->n = k;
    return nb;
}
static inline float SmOnHit(SmState* s, const SmCfg* c, double t, uint32_t act, uint64_t run) { return SmOnHitEx(s, c, t, act, run, -1.f, 0); }
// In combat = a hit landed within combatMs (skills / style switches outside a fight score nothing and do not wake the meter).
static inline int SmInCombat(const SmState* s, const SmCfg* c, double t) { return t - s->lastHit <= (double)c->combatMs; }
// #126850 soul core / #126854 ninjutsu used. kind 0 = ninjutsu, 1 = soul core.
static inline float SmOnSkill(SmState* s, const SmCfg* c, double t, int kind) {
    if (kind) s->souls++; else s->nins++;
    if (!SmInCombat(s, c, t)) return 0.f;
    const float p = kind ? c->soul : c->nin;
    if (p <= 0.f) return 0.f;
    if (kind) { s->soulsScored++; s->lastSoul = t; }
    SmAdd(s, c, p); s->lastEvent = s->lastScore = t; return p;
}
// #127057 weapon swap attack: the active weapon slot changed (the game's GetActiveWeaponSlot) ...
// Weapon switch (any way of switching: flash / swap attack, the quick-swap button, ...): a landed hit with a different weapon slot than the
// last landed hit, while that hit is recent (switchMs, 3 s: fluid switching, not a switch between fights). Once per switch: the next hit with the same weapon is a normal hit. slot < 0 =
// unknown (nothing scores, nothing is remembered).
static inline float SmWeaponHit(SmState* s, const SmCfg* c, double t, int slot) {
    if (slot < 0) return 0.f;
    const int sw = s->hitSlot >= 0 && slot != s->hitSlot && t - s->hitSlotT <= (double)c->switchMs;
    s->hitSlot = slot; s->hitSlotT = t;
    if (!sw || c->swap <= 0.f) return 0.f;
    s->swapHits++;
    SmAdd(s, c, c->swap); s->lastEvent = s->lastScore = t; return c->swap;
}
static inline void SmOnSwap(SmState* s, double t) { s->swaps++; s->lastSwap = t; s->swapPaid = 0; }
// ... and a move that started within swapMs of that change landed a hit: one bonus per slot change (swap spam without hits scores nothing).
static inline float SmSwapHit(SmState* s, const SmCfg* c, double t, double runStart) {
    if (s->swapPaid || c->swap <= 0.f) return 0.f;
    const double d = runStart - s->lastSwap;
    if (d < -(double)c->swapMs || d > (double)c->swapMs) return 0.f;
    s->swapPaid = 1; s->swapHits++;
    SmAdd(s, c, c->swap); s->lastEvent = s->lastScore = t; return c->swap;
}
// #126843 Samurai <-> Ninja style switch: scores in combat, once per landed hit since the last scored switch.
// #126913 mid soul core: a switch within combatMs of a scored soul core use also scores, once per such use (the form itself may land no
// hit before the switch, and BR's FormStyleSwitch replays a press made inside the form at its end).
static inline float SmOnStyle(SmState* s, const SmCfg* c, double t) {
    s->styles++;
    if (c->style <= 0.f) return 0.f;
    const int byHit = SmInCombat(s, c, t) && s->hits != s->hitsAtStyle;
    const int bySoul = s->soulsScored != s->soulsAtStyle && t - s->lastSoul <= (double)c->combatMs;
    // #127065: the mode switch has its own attack, so a switch from an idle / empty meter scores too: one per combatMs with no hit between
    const int byIdle = t - s->lastStyleT > (double)c->combatMs;
    if (!byHit && !bySoul && !byIdle) return 0.f;
    s->hitsAtStyle = s->hits; s->soulsAtStyle = s->soulsScored; s->lastStyleT = t;
    SmAdd(s, c, c->style); s->lastEvent = s->lastScore = t; return c->style;
}
// Clean dodge / deflect: still counted when looped (it happened), but a looped one scores 0 and costs loopPts (no decay refresh).
static inline float SmOnDodge(SmState* s, const SmCfg* c, double t) {
    s->dodges++; s->lastEvent = t;
    if (SmLoopEvent(s, c, SM_LOOP_DODGE)) return 0.f;
    SmAdd(s, c, c->dodge); s->lastScore = t; return c->dodge;
}
static inline float SmOnDeflect(SmState* s, const SmCfg* c, double t) {
    s->deflects++; s->lastEvent = t;
    if (SmLoopEvent(s, c, SM_LOOP_DEFLECT)) return 0.f;
    SmAdd(s, c, c->deflect); s->lastScore = t; return c->deflect;
}
// Counter by avoiding: dodge points; count = 1 when this move has not been counted as a counter yet (the hit counter may have been first).
// act / run = the skill move (a loop step of that use, shared with its hits: a looped move's avoid scores 0, the penalty is paid once).
static inline float SmOnAvoidCounterUse(SmState* s, const SmCfg* c, double t, int count, uint32_t act, uint64_t run) {
    if (count) s->counterAvoids++;
    s->lastEvent = t;
    if (SmLoopUse(s, c, act, run)) return 0.f;
    SmAdd(s, c, c->dodge); s->lastScore = t; return c->dodge;
}
static inline float SmOnAvoidCounter(SmState* s, const SmCfg* c, double t, int count) {
    if (count) s->counterAvoids++;
    s->lastEvent = t;
    if (SmLoopEvent(s, c, SM_LOOP_COUNTER)) return 0.f;
    SmAdd(s, c, c->dodge); s->lastScore = t; return c->dodge;
}
// #127065 / #127075 ninjutsu gate: n gauge drops this tick -> 1 if it is a new throw. One per throw (drops within SM_NIN_GAP_MS of the
// last one are the same throw, e.g. two +20 10 ms apart, act 34000 -> 34052), and only in Ninja mode (style 1; -1 = unknown is allowed).
static inline int SmNinGate(double* lastT, double t, int n, int style) {
    if (n <= 0 || style == 0) return 0;
    const int fresh = t - *lastT >= SM_NIN_GAP_MS;
    *lastT = t;  // a chain of drops keeps extending the same throw
    return fresh;
}
// #127213 guardian spirit skill gate (same idea as SmNinGate: a gauge spend, no act ids). Fed once per tick with the player move key `run`
// and the Spirit Force gauge `cur` (< 0 = unreadable). On a new move the value of the tick before it is the base; a drop of >= SM_GS_DROP
// below the base within winMs of the move start marks the move as a guardian spirit skill, once per move. A refill (cur above the base)
// raises the base, so it never counts. Returns 1 on the tick the move is marked.
typedef struct { uint64_t run; double t0; float prev, base; int on, gs, paid; } SmGsMove;
static inline void SmGsReset(SmGsMove* g) { g->run = 0; g->t0 = -1e18; g->prev = g->base = -1.f; g->on = g->gs = g->paid = 0; }
static inline int SmGsStep(SmGsMove* g, double t, uint64_t run, float cur, int winMs) {
    if (!g->on || run != g->run) { g->on = 1; g->run = run; g->t0 = t; g->base = g->prev; g->gs = g->paid = 0; }
    int mark = 0;
    if (cur < 0.f) g->base = -1.f;  // unreadable inside the move: no base, no mark this move
    else if (g->base >= 0.f) {
        if (cur > g->base) g->base = cur;
        else if (!g->gs && g->base - cur >= SM_GS_DROP && t - g->t0 <= (double)winMs) { g->gs = 1; mark = 1; }
    }
    g->prev = cur;
    return mark;
}
// ... and the marked move scored points (movePts = its own points so far): the bonus once per move.
static inline float SmGsPay(SmState* s, const SmCfg* c, SmGsMove* g, double t, float movePts) {
    if (!g->gs || g->paid || !(movePts > 0.f) || c->gs <= 0.f) return 0.f;
    g->paid = 1; s->gsHits++;
    SmAdd(s, c, c->gs); s->lastEvent = s->lastScore = t; return c->gs;
}
// human: summoned guardian spirit (the attack that sends the spirit out, R2 + Triangle on PlayStation). Its hits come from the spirit's
// own actor (same team as the player), so they are not the player's hits. A guardian spirit gauge spend opens a window of gsSummonMs;
// each hit by a same-team actor inside it scores like a hit of the summon move (one use: variety, multi-hit, keeps the meter from
// draining) and the guardian spirit bonus once per summon, unless that move already paid it with the player's own hits.
typedef struct { double t0; uint64_t run; uint32_t act; int paid; int hitsN; } SmSummon;  // hitsN: hits so far (multi-hit cap)
static inline void SmSummonReset(SmSummon* m) { m->t0 = -1e18; m->run = 0; m->act = 0; m->paid = 1; m->hitsN = 0; }
static inline void SmSummonOpen(SmSummon* m, double t, uint64_t run, uint32_t act) { m->t0 = t; m->run = run; m->act = act; m->paid = 0; m->hitsN = 0; }
static inline int SmSummonLive(const SmSummon* m, const SmCfg* c, double t) { return c->gsSummonMs > 0 && t - m->t0 >= 0.0 && t - m->t0 <= (double)c->gsSummonMs; }
static inline float SmSummonHits(SmState* s, const SmCfg* c, SmSummon* m, double t, int n, float* bonus) {
    float p = 0.f; *bonus = 0.f;
    if (n <= 0 || !SmSummonLive(m, c, t)) return 0.f;
    for (int i = 0; i < n && i < 64; ++i) {
        if (c->multiHitCap > 0 && m->hitsN >= c->multiHitCap) { s->hits++; s->cappedHits++; s->lastEvent = s->lastHit = t; continue; }  // over the cap
        m->hitsN++; p += SmOnHitUse(s, c, t, m->act, m->run, -1.f, 0, 1);
    }
    if (!m->paid && c->gs > 0.f) { m->paid = 1; s->gsHits++; SmAdd(s, c, c->gs); s->lastEvent = s->lastScore = t; *bonus = c->gs; }
    return p;
}
// #127295 martial art, from the game's own action data (no act ids, no input): the current action's 336-byte file row
// R = [[ctrl+0xE8]+0x20] has a skill word at dword R+0xDC. The game's state test 0x2E9948 cond 0x2F (-> hit +0x13C bit 6,
// Collision::Damage::Hit::IsAttackAttackSkill) is true when that dword's bit 31 is clear and its low u16 is not 0xBA71.
// MartialArtPts unset (< 0) = NinjutsuPts.
static inline int SmSkillRow(uint32_t w) { return !(w >> 31) && (w & 0xFFFFu) != SM_SKILL_NONE_ID; }
static inline float SmMaPtsResolve(int ini, float nin) { return ini < 0 ? nin : (float)ini; }
// Per move: `skill` = 1 the move's row is a skill row, 0 not, -1 unreadable; `other` = this move is already another skill bonus
// (guardian spirit gauge drop / ninjutsu gauge spend: those rows carry the skill word too). Returns 1 on the tick the move is marked.
typedef struct { uint64_t run; int on, ma, other, paid; } SmMaMove;
static inline void SmMaReset(SmMaMove* m) { m->run = 0; m->on = m->ma = m->other = m->paid = 0; }
static inline int SmMaStep(SmMaMove* m, uint64_t run, int skill, int other) {
    if (!m->on || run != m->run) { m->on = 1; m->run = run; m->ma = m->other = m->paid = 0; }
    if (other) m->other = 1;
    if (skill > 0 && !m->ma) { m->ma = 1; return 1; }
    return 0;
}
// ... paid once per move on its first scored points (movePts = the move's own points so far), never on a guardian spirit / ninjutsu move.
static inline float SmMaPay(SmState* s, const SmCfg* c, SmMaMove* m, double t, float movePts) {
    if (!m->ma || m->other || m->paid || !(movePts > 0.f) || c->ma <= 0.f) return 0.f;
    m->paid = 1; s->maHits++;
    SmAdd(s, c, c->ma); s->lastEvent = s->lastScore = t; return c->ma;
}
// human: Burst Break. A boss's red attack (the game's GreatAttack / unblockable flag, u16 row +0x04 bit 0x2000 of its current action row) is
// interrupted by the player's hit (or the summoned guardian spirit's) during its wind-up. One SmBurst per boss slot, stepped once per tick
// with that slot's current action run:
//   valid  the slot has a boss with a readable action run (0 = gone / replaced: forgotten, no award)
//   key    run key (start frame << 32 | act), act = its action id
//   row    the current run's row has GreatAttack: 1 yes, 0 no, -1 unread this tick (sticky: one read of 1 marks the run)
//   next   the row's next-action id (u16 +0xE0; SM_ACT_NONE / 0xFFFF / 0 = none): the wind-up flowing into that action is not a break
//   great  the frame probe saw an unblockable collider created in this run (run.great): the attack came out, too late to break it
//   hit    0 none, 1 a hit landed on this boss this tick, 2 the same by a skill move / guardian spirit (pts = its points, for the pop-up),
//          3 the same by the player's burst counter (BurstCounterAct): it beats the red attack while it is out, so it counts even with great
// A hit counts while the run is a burst run and !great (a burst-counter hit: any time in the burst run). When the run ends (key changes) the hits of that same tick still belong to the old
// run (the hit is what ended it). Award when the old run was a burst run, got a counted hit, never created its unblockable collider, ended
// within burstMs of its last counted hit, and the new action is not its continuation (the new row is a burst row too, or it is the old row's
// next action); a burst-counter hit waives the collider rule. Each candidate (burst run with a counted hit) that ends returns SM_BB_MISS or
// SM_BB_AWARD with *o filled for the log.
// Burst counter vs dodge: the counter has i-frames inside the boss's hit window, so the dodge detector sees a clean avoid a few ticks after the
// counter hit (live: hit, then "dodge +45", then the action change). SmBurstAvoid decides that avoid is the Burst Break's own (no Dodge!).
enum { SM_BB_NONE = 0, SM_BB_AWARD = 1, SM_BB_MISS = 2 };
enum { SM_BB_HIT = 1, SM_BB_HIT_SKILL = 2, SM_BB_HIT_CTR = 3 };
typedef struct { uint64_t key; uint32_t act, next; int on, burst, great, hit, hits, ctr, held; double hitT; float hitPts; } SmBurst;
typedef struct { uint32_t act, newAct, next; int great, cont, hitKind, hits, late, noSkill, ctr, held; double ago; float hitPts; } SmBurstEnd;
static inline void SmBurstReset(SmBurst* b) { b->key = 0; b->act = b->next = SM_ACT_NONE; b->on = b->burst = b->great = b->hit = b->hits = b->ctr = b->held = 0; b->hitT = -1e18; b->hitPts = 0.f; }
static inline void SmBurstHit(SmBurst* b, int hit, float pts, double t) {
    if (!hit || !b->burst || (b->great && hit < SM_BB_HIT_CTR)) return;
    if (hit > b->hit) b->hit = hit;
    if (hit == SM_BB_HIT_CTR) b->ctr++;
    b->hits++; b->hitT = t; b->hitPts = pts;
}
static inline int SmBurstNextOk(uint32_t n) { return n != SM_ACT_NONE && n != 0xFFFFu && n != 0; }
static inline int SmBurstStep(SmBurst* b, const SmCfg* c, double t, int valid, uint64_t key, uint32_t act, int row, uint32_t next, int great, int hit, float pts, SmBurstEnd* o) {
    int r = SM_BB_NONE;
    if (!valid) { SmBurstReset(b); return r; }
    if (b->on && key != b->key) {  // the old run ended this tick: this tick's hits are its own
        SmBurstHit(b, hit, pts, t); hit = 0;
        if (b->burst && b->hit) {
            o->act = b->act; o->newAct = act; o->next = b->next; o->great = b->great; o->hitKind = b->hit; o->hits = b->hits; o->ago = t - b->hitT; o->hitPts = b->hitPts;
            o->ctr = b->ctr; o->held = b->held;
            o->cont = row > 0 || (SmBurstNextOk(b->next) && (act & 0xFFFFu) == (b->next & 0xFFFFu));
            o->late = o->ago > (double)c->burstMs; o->noSkill = c->burstSkill && b->hit < 2;
            r = (!o->great || o->ctr) && !o->cont && !o->late && !o->noSkill ? SM_BB_AWARD : SM_BB_MISS;
        }
        b->on = 0;
    }
    if (!b->on) { SmBurstReset(b); b->on = 1; b->key = key; b->act = act; }
    if (row > 0) b->burst = 1;
    if (SmBurstNextOk(next) && !SmBurstNextOk(b->next)) b->next = next;
    if (great) b->great = 1;  // a collider and a hit in one tick: the attack is out, the hit is too late
    SmBurstHit(b, hit, pts, t);
    return r;
}
// The hit kind of a player hit on a boss: the burst counter action, else a skill move (2) or a plain hit (1).
static inline int SmBurstHitKind(const SmCfg* c, uint32_t act, int skill) { return c->burstCtrAct && act == c->burstCtrAct ? SM_BB_HIT_CTR : skill ? SM_BB_HIT_SKILL : SM_BB_HIT; }
// An avoid (SM_AV_*) seen this tick, after this tick's SmBurstStep: 1 = it is the burst counter's own avoid, not scored now. Held (b->held =
// avoid) while the live burst run has a burst-counter hit: SmBurstStep then returns it in o->held, and the glue pays it after all when that
// run ends with SM_BB_MISS (no Burst Break: the avoid was real). Also 1 within SM_BURST_AVOID_MS after a burst-counter award (ctrAwardT).
static inline int SmBurstAvoid(SmBurst* b, double ctrAwardT, double t, int avoid) {
    if (!avoid) return 0;
    if (b && b->on && b->burst && b->ctr && !b->held) { b->held = avoid; return 1; }
    return t >= ctrAwardT && t - ctrAwardT <= (double)SM_BURST_AVOID_MS;
}
static inline float SmOnBurst(SmState* s, const SmCfg* c, double t) {
    s->bursts++;
    if (c->burst <= 0.f) return 0.f;
    SmAdd(s, c, c->burst); s->lastEvent = s->lastScore = t; return c->burst;
}
// #127075 Ki pulse with the perfect flag: perfect adds kiPerfect on top when the pulse scores at all.
static inline float SmOnKiEx(SmState* s, const SmCfg* c, double t, int perfect) {
    s->kis++;
    if (c->kiHitMs > 0 && !(t - s->lastHit <= (double)c->kiHitMs)) return 0.f;
    float p = c->ki;
    if (perfect && c->kiPerfect > 0.f) { p += c->kiPerfect; s->kiPerfects++; }
    SmAdd(s, c, p); s->lastEvent = s->lastScore = t; return p;
}
static inline float SmOnKi(SmState* s, const SmCfg* c, double t) {
    s->kis++;
    if (c->kiHitMs > 0 && !(t - s->lastHit <= (double)c->kiHitMs)) return 0.f;  // pulse after a whiff: no points, no decay reset
    SmAdd(s, c, c->ki); s->lastEvent = s->lastScore = t; return c->ki;
}
static inline void SmOnDamaged(SmState* s, const SmCfg* c, double t) {
    s->damaged++; s->lastEvent = t; s->combo = 0;  // getting hit breaks the combo
    if (s->dodge.inWin) s->dodge.hit = 1;
    if (s->lastDamaged >= 0 && t - s->lastDamaged < (double)c->damageGraceMs) { s->pts = 0.f; return; }  // same combo: no second drop
    s->lastDamaged = t;
    if (s->rank > 0) s->rank--;
    s->pts = 0.f; s->active = s->rank > 0;
    s->useOn = 0; s->useAct = SM_ACT_NONE;
}
// human: combo. n landed hits this tick (the player's and the summoned spirit's). A gap over comboMs starts a new combo; every comboStep-th
// hit of one combo pays a milestone bonus (comboBase, +comboInc each later milestone, capped at comboMax). Returns the bonus paid;
// *mile = the combo count of the last milestone reached this tick (0 = none).
static inline float SmComboMilestone(const SmCfg* c, uint32_t combo) {
    if (c->comboStep <= 0 || combo == 0 || combo % (uint32_t)c->comboStep) return 0.f;
    const int k = (int)(combo / (uint32_t)c->comboStep) - 1; const int b = c->comboBase + k * c->comboInc;
    return (float)(b > c->comboMax ? c->comboMax : b);
}
static inline float SmComboHits(SmState* s, const SmCfg* c, double t, int n, uint32_t* mile) {
    float bonus = 0.f; *mile = 0;
    for (int i = 0; i < n && i < 64; ++i) {
        if (s->combo && t - s->comboT > (double)c->comboMs) s->combo = 0;
        s->combo++; s->comboT = t;
        if (s->combo > s->comboBest) s->comboBest = s->combo;
        const float b = SmComboMilestone(c, s->combo);
        if (b > 0.f) { bonus += b; *mile = s->combo; s->comboBonuses++; }
    }
    if (bonus > 0.f) { SmAdd(s, c, bonus); s->lastEvent = s->lastScore = t; }
    return bonus;
}
// the combo ended (gap over comboMs): the count drops to 0. Per tick.
static inline void SmComboTick(SmState* s, const SmCfg* c, double t) { if (s->combo && t - s->comboT > (double)c->comboMs) s->combo = 0; }
// Per tick: idle decay. t = game ms now, dt = game ms since the last tick.
static inline void SmTick(SmState* s, const SmCfg* c, double t, double dt) {
    if (!s->active || dt <= 0) return;
    double idle = t - s->lastScore - (double)c->decayDelayMs;
    if (idle <= 0) return;
    const double idleAll = idle;  // ms idle past the delay: the ramp grows with it (#126972)
    if (idle > dt) idle = dt;
    float d = c->decayPerS * (1.f + (float)s->rank * (float)c->decayRankPct / 100.f) * (float)(idle / 1000.0);
    if (c->decayRampPct > 0) d *= 1.f + (float)c->decayRampPct / 100.f * (float)(idleAll / 1000.0);
    s->pts -= d;
    while (s->pts < 0.f) {
        if (s->rank == 0) { s->pts = 0.f; s->active = 0; s->useOn = 0; break; }
        s->rank--; s->pts += c->rankPts[s->rank];
    }
}
// Dodge detector, once per tick: enemyActive = an enemy hit window is open now, ifr = the player has i-frames now, tookHit = the player lost HP
// this tick. 1 on the tick the window closes when i-frames were seen inside it and no damage came in.
// deflectNow / skillNow = the deflect signal / the player's move is a skill, this tick. Returns SM_AV_* on the tick the window closes clean
// (i-frames seen, no damage): a skill move anywhere in the window = counter, else the deflect signal = deflect, else dodge.
static inline int SmDodgeStepEx(SmDodge* d, int enemyActive, int ifr, int tookHit, int deflectNow, int skillNow) {
    int award = SM_AV_NONE;
    if (enemyActive) {
        if (!d->inWin) { d->inWin = 1; d->sawIfr = 0; d->hit = 0; d->sawDeflect = 0; d->sawSkill = 0; }
        if (ifr) d->sawIfr = 1;
        if (deflectNow) d->sawDeflect = 1;
        if (skillNow) d->sawSkill = 1;
        if (tookHit) d->hit = 1;
    } else if (d->inWin) {
        if (tookHit) d->hit = 1;  // the damage of the last frame of the window lands on the close tick
        if (deflectNow) d->sawDeflect = 1;
        if (skillNow) d->sawSkill = 1;
        award = d->sawIfr && !d->hit ? (d->sawSkill ? SM_AV_COUNTER : d->sawDeflect ? SM_AV_DEFLECT : SM_AV_DODGE) : SM_AV_NONE;
        d->inWin = 0;
    }
    return award;
}
static inline int SmDodgeStep(SmDodge* d, int enemyActive, int ifr, int tookHit) { return SmDodgeStepEx(d, enemyActive, ifr, tookHit, 0, 0) != 0; }
// Visible: something to show, and a boss fight is live or the last event is recent.
static inline int SmVisible(const SmState* s, const SmCfg* c, double t, int fight) {
    return s->active && (fight || t - s->lastEvent < (double)c->combatMs);
}
// #127003 move logger: one record per player move (action run), hit or miss. Pure (formatting only) so the test covers it.
typedef struct {
    uint32_t act; double ms;      // action id, run length (game ms)
    int windows, landed;          // hit windows the move opened (frame probe) / hits that landed on an enemy during it
    float startupF;               // first hit window, 60 fps frames (-1 = no window: a non-attack or a whiff with no collider)
    int style;                    // 0 Samurai, 1 Ninja, -1 unknown (chr +0xA95C)
    uint16_t btn; uint8_t lt, rt; // XInput buttons / triggers held when the move started (stance / weapon swap / guardian are pad combos)
    int soulForm;                 // a soul core form started (BR FormUseTick / seal use) within SM_MOVE_SOUL_MS before the move
    int nin, great, enemy;        // ninjutsu gauge spent during it / GreatAttack collider / an enemy hit window was open at its start
    int slot, swapped;            // #127057 active weapon slot at its start (-1 unknown) / it was a weapon swap attack (scored)
    float sf0, sfMin, sfMax;      // #127213 Spirit Force gauge just before the move / lowest during it / its max (sfMax <= 0 = unread)
    int gs;                       // #127213 marked as a guardian spirit skill (gauge drop at its start)
    int skillRd, ma; uint32_t skill;  // #127295 action-row skill word read / it scored as a martial art / the word (R+0xDC)
    int posOk; float p0[2], b0[2], moved, turn, dist0, dist1;  // human (Displacement!): player / nearest boss at the start (x, z cm), farthest
                                  // from the start, bearing change around the boss, distance to it at the start / at the last tick
} SmMoveRec;
#define SM_MOVE_SOUL_MS 20000
static inline int SmPadStr(char* o, int n, uint16_t b, uint8_t lt, uint8_t rt) {
    static const struct { uint16_t m; const char* s; } k[] = { {0x1000,"A"},{0x2000,"B"},{0x4000,"X"},{0x8000,"Y"},{0x0100,"LB"},{0x0200,"RB"},
        {0x0040,"LS"},{0x0080,"RS"},{0x0001,"Up"},{0x0002,"Down"},{0x0004,"Left"},{0x0008,"Right"},{0x0010,"Start"},{0x0020,"Back"} };
    int w = 0; o[0] = 0;
    for (unsigned i = 0; i < sizeof k / sizeof k[0] && w < n - 1; ++i) if (b & k[i].m) w += snprintf(o + w, n - w, "%s%s", w ? "+" : "", k[i].s);
    if (lt > 64 && w < n - 1) w += snprintf(o + w, n - w, "%sLT", w ? "+" : "");
    if (rt > 64 && w < n - 1) w += snprintf(o + w, n - w, "%sRT", w ? "+" : "");
    if (!w) w = snprintf(o, n, "-");
    return w;
}
static inline int SmMoveFmt(char* o, int n, const SmMoveRec* m) {
    char pad[96], su[24]; SmPadStr(pad, sizeof pad, m->btn, m->lt, m->rt);
    if (m->startupF < 0) snprintf(su, sizeof su, "-"); else snprintf(su, sizeof su, "%.0f f", m->startupF);
    char sf[64]; if (m->sfMax > 0.f) snprintf(sf, sizeof sf, " | spirit %.1f->%.1f/%.1f%s", m->sf0, m->sfMin, m->sfMax, m->gs ? " guardian spirit" : ""); else sf[0] = 0;
    char sk[48]; if (!m->skillRd) sk[0] = 0; else if (SmSkillRow(m->skill)) snprintf(sk, sizeof sk, " | skill 0x%X%s", m->skill, m->ma ? " martial art" : ""); else snprintf(sk, sizeof sk, " | skill -");
    char sl[32]; if (m->slot >= 0) snprintf(sl, sizeof sl, " | slot %d%s", m->slot, m->swapped ? " swap attack" : ""); else sl[0] = 0;
    char ps[80]; if (m->posOk) snprintf(ps, sizeof ps, " | moved %.2f m around boss %.0f deg dist %.2f->%.2f m", m->moved / 100.f, m->turn, m->dist0 / 100.f, m->dist1 / 100.f); else ps[0] = 0;
    return snprintf(o, n, "act 0x%X (%u) %.0f ms %.0f f | %s | windows %d landed %d startup %s | %s | pad %s%s%s%s%s%s%s%s%s",
        m->act, m->act, m->ms, m->ms * 0.06, m->landed > 0 ? "HIT" : m->windows > 0 ? "MISS" : "no attack", m->windows, m->landed,
        su, m->style == 0 ? "Samurai" : m->style == 1 ? "Ninja" : "style ?",
        pad, m->soulForm ? " | soul core form" : "", m->nin ? " | ninjutsu" : "", m->great ? " | great" : "", m->enemy ? " | enemy attacking" : "", sl, sf, sk, ps);
}
#endif
