// user #14291: Boss moves page split into sub menus (one page per move group) so a move is easy to find (Yoritomo has 173).
// Groups come from the catalogue only (mod/boss_moves.h kind + label, and the live Ki-break ids of notes/kibreak-deathstance-9895.md):
//   Attacks          kind A (hit seen), S (wind-up), L (combo link) from the Training logs
//   Unnamed attacks  not seen in a fight yet, game data says attack-family motion (static action table)
//   Twitches         kind T (short state change, ends at once when forced)
//   Ki break         the live-logged Ki-break ids (4E4C start, 4E37/4E38 upright pose, 4E3E/4E40/FA6 hunched / bare crouch) and
//                    every static-table row with the same motion family (Ki-break motion 300xx / 301xx). Which attacks CAUSE a
//                    Ki break is not known (no data), so those stay in Attacks.
//   Movement and reactions  kind M / I (steps, turns, system states, other hit reactions) and idle / initialization
//   Unsorted         everything else (not classified yet)
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../../boss_moves.h"

enum { kMcAttack, kMcUnnamedAtk, kMcTwitch, kMcKiBreak, kMcMovement, kMcUnsorted, kMcCount };

static inline const char* N3MoveCatName(int c) {
    return c == kMcAttack ? "Attacks" : c == kMcUnnamedAtk ? "Unnamed attacks" : c == kMcTwitch ? "Twitches" : c == kMcKiBreak ? "Ki break"
         : c == kMcMovement ? "Movement and reactions" : c == kMcUnsorted ? "Unsorted" : "?";
}
// one short line per group (footer of the group row)
static inline const char* N3MoveCatHelp(int c) {
    return c == kMcAttack ? "Attacks seen in fights: hits, wind-ups and combo links. Best for drills."
         : c == kMcUnnamedAtk ? "Attacks from the game's move table that were not seen in a fight yet (no name yet). May not play from Stand still."
         : c == kMcTwitch ? "Twitches: short state changes that end at once when forced (about 0.05-0.15 s)."
         : c == kMcKiBreak ? "Moves during a Ki break (break start, poses, recovery). Moves that CAUSE a Ki break are not known yet: they stay in Attacks."
         : c == kMcMovement ? "Steps, turns, recoveries, hit reactions and system states (plus idle / initialization, always ON)."
         : c == kMcUnsorted ? "Moves not sorted yet (no kind recorded and no attack / movement motion in the game data)."
         : "";
}
// Ki-break ids seen live on Yoritomo (chr 16102), notes/kibreak-deathstance-9895.md (#9895 / #9937). nullptr = no live note.
static inline const char* N3KiNote(uint32_t chr, uint32_t id) {
    if (chr != 16102u) return nullptr;
    switch (id) {
        case 0x4E4C: return "Ki break start (seen live)";
        case 0x4E38: return "Ki-break pose, upright wings out (good, seen live)";
        case 0x4E37: return "Ki-break pose, upright (seen live)";
        case 0x4E3E: return "Ki break, hunched pose (seen live)";
        case 0x4E40: return "Ki break, hunched 'death' pose (seen live)";
        case 0xFA6: return "Ki break, bare crouch (seen live)";
        default: return nullptr;
    }
}
// motion number of a static-table "Ki-break motion NNNNN" label, 0 if none
static inline int N3KiMotion(const char* label) {
    const char* p = label ? strstr(label, "Ki-break motion ") : nullptr;
    return p ? atoi(p + 16) : 0;
}
static inline int N3MoveCat(const N3Move* m) {
    if (N3MoveProtected(m->id)) return kMcMovement;
    const int km = N3KiMotion(m->label);
    if (N3KiNote(m->chr, m->id) || km / 100 == 300 || km / 100 == 301) return kMcKiBreak;
    if (m->kind == 'A' || m->kind == 'S' || m->kind == 'L') return kMcAttack;
    if (m->kind == 'T') return kMcTwitch;
    if (m->kind == 'M' || m->kind == 'I') return kMcMovement;
    if (m->label && strstr(m->label, "attack-family")) return kMcUnnamedAtk;
    return kMcUnsorted;
}
