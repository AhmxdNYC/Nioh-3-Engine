// user #14462 / #14304: decisions for the per-slot idle hold of a summoned Training boss (training.c SphTick + aggro.h AggroTick). Pure, offline
// tested (tests/slot_hold_test.c).
// Live 22:35:55-22:42 (Boss A summoned from the menu, Yoritomo, sequence 0x4E21 > 0x4E22): picking the sequence released the slot hold
// ('SUMPASSIVE slot A: AI on (repeat move picked)'), the boss's own AI then fought with its whole move set (0x4E23, 0x4E5C, 0x4E60, 0x4E62,
// 0x4E7C ... 'follows up on its own'), i.e. random moves instead of idle + the picked sequence. A Training boss with the AI held still plays
// a repeat: the select hook (35EF04) runs at its idle / action-end transitions with the AI off and MvRepTick forces the head after the gap
// (live 23:39-23:48 'Training here' boss, AI held: only the sequence parts played). So a repeat no longer releases the hold; only
// 'Fight normally' for that boss or the end of the session does. 'Aggro me' ON switches the AI on only while the boss walks up (locomotion only,
// move_toggle.h MvChaseFilter), then off again.
#ifndef SLOT_HOLD_H
#define SLOT_HOLD_H
#include <stdint.h>
#define SPH_TRAINING (-1)
// Release reason, 0 = keep holding. ms = SPH_TRAINING (idle until the user picks) or a SummonPassiveMs timer; elapsed = ms since the hold began.
static const char* SphReleaseWhy(int ms, uint32_t elapsed, int sess, int trActive, int fight) {
    if (ms != SPH_TRAINING) return elapsed >= (uint32_t)ms ? "passive time over" : 0;
    if (sess && !trActive) return "Training session ended";
    if (fight) return "this boss set to Fight normally";
    return 0;  // #14462: a repeat move / sequence is played with the AI held (no 'repeat move picked' release)
}
// Held slot boss, every 100 ms: 1 = switch the AI on (aggro walk-up), 0 = switch it off (hold), -1 = leave it.
static int SphAiAction(int chasing, int aiOn) {
    if (aiOn < 0) return -1;
    if (chasing) return aiOn == 0 ? 1 : -1;
    return aiOn == 1 ? 0 : -1;
}
// Aggro walk-up of a held slot boss with a repeat armed: start beyond range + 1 m (hysteresis), keep walking until inside range,
// never start while its sequence still plays (busy). 1 = walk up (its AI state goes into g_mv_chase this tick).
static int SphChaseWanted(int wasChasing, float d, float range, int busy) {
    if (wasChasing) return d > range;
    return !busy && d > range + 100.0f;
}
#endif
