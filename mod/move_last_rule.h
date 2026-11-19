// GitHub #126: which action below 0x4E20 counts as an attack for 'Repeat last move' (move_toggle.h MlAttack). Ids >= 0x4E20 keep the old rule.
// Some bosses attack with low ids (Giant Toad 210803: 0xC61-0xCA3, 0x1770), but every boss also has low-id steps / turns / guards whose
// game-data row carries an attack block or hitbox rows and so is kind 'A' (Nobuharu / Shingen 0x3FA-0x3FD, Toad 0x1E-0x21: motions 80-83;
// Toad 0xC52-0xC5A: motions 30-37). Data rule, no boss list (#87625), checked on the 4 extracted files (Yoritomo, Nobuharu, Shingen, Toad):
//   kind 'A', its motion is an attack motion (1000..29999: 0..999 are the shared locomotion / guard set on every boss, >= 30000 hit
//   reactions and Ki break, as N3CatLabel), and its facts row says attack, grab, hits or unblockable (a motion-6901 recovery with only a
//   hitbox row is not).
#pragma once
#include "boss_move_facts.h"
enum { N3_ATTACK_MOTION_MIN = 1000, N3_ATTACK_MOTION_END = 30000 };
static inline int N3LowIdAttack(char kind, unsigned motion, const N3MoveFact* f) {
    return kind == 'A' && motion >= N3_ATTACK_MOTION_MIN && motion < N3_ATTACK_MOTION_END && f && (f->attack || f->grab || f->hits || f->great);
}
