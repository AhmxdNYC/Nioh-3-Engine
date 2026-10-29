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
// user #22349: a boss with game-data facts (mod/boss_move_facts.h, tools/gen_boss_move_facts.py; Yoritomo now) has its attacks (the two
// attack groups above) split by the game's data instead: '1 hit', '2 hits' ... (hit windows of the move's own hitbox rows), 'Unblockable'
// (GreatAttack flag; shown only there, the row says its hit count) and 'Other attacks (hits unknown)' (no own hitbox rows / projectiles:
// not guessed). Unsafe moves stay in no group; twitches / animations keep their own group; Sequences stay. Other bosses: unchanged.
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../../boss_moves.h"
#include "../../boss_move_facts.h"

enum { kMcAttack, kMcUnnamedAtk, kMcTwitch, kMcKiBreak, kMcMovement, kMcUnsorted, kMcCount };
// user #22349: game-data attack groups. kMcCount itself is the Sequences row (training_bosses_menu.h kMcSeq), so these start after it.
// human #44217: kMcGrab = the moves of a grab (catch + throw, auto_moves.h AmGrabs); appended so the other group numbers stay put.
enum { kMcHitMax = 12, kMcHit1 = kMcCount + 1, kMcGreat = kMcHit1 + kMcHitMax, kMcHitUnknown, kMcGrab, kMcAll };  // kMcHit1 + n - 1 = 'n hits'
// menu order: attacks first (by hits, then Grabs, Unblockable, unknown), then the old groups (a boss without facts uses Attacks / Unnamed)
static const int kMcOrder[] = {kMcHit1, kMcHit1 + 1, kMcHit1 + 2, kMcHit1 + 3, kMcHit1 + 4, kMcHit1 + 5, kMcHit1 + 6, kMcHit1 + 7, kMcHit1 + 8,
                               kMcHit1 + 9, kMcHit1 + 10, kMcHit1 + 11, kMcGrab, kMcGreat, kMcHitUnknown, kMcAttack, kMcUnnamedAtk, kMcTwitch,
                               kMcKiBreak, kMcMovement, kMcUnsorted};
static_assert(sizeof kMcOrder / sizeof kMcOrder[0] == kMcAll - 1, "every group (not Sequences) once");
static inline int N3MoveHitCat(int c) { return c >= kMcHit1 && c < kMcHit1 + kMcHitMax; }

static inline const char* N3MoveCatName(int c) {
    static const char* const hit[kMcHitMax] = {"1 hit", "2 hits", "3 hits", "4 hits", "5 hits", "6 hits", "7 hits", "8 hits", "9 hits", "10 hits",
                                               "11 hits", "12 or more hits"};
    if (N3MoveHitCat(c)) return hit[c - kMcHit1];
    if (c == kMcGreat) return "Unblockable";
    if (c == kMcGrab) return "Grabs";
    if (c == kMcHitUnknown) return "Attacks: hit count unknown";
    return c == kMcAttack ? "Attacks" : c == kMcUnnamedAtk ? "Unnamed attacks" : c == kMcTwitch ? "Non-attack animations" : c == kMcKiBreak ? "Ki break"
         : c == kMcMovement ? "Movement and reactions" : c == kMcUnsorted ? "Unsorted" : "?";
}
// one short line per group (footer of the group row)
static inline const char* N3MoveCatHelp(int c) {
    if (N3MoveHitCat(c)) return "Attacks grouped by available hit counts, including linked moves where known. Hits can miss or be blocked; the label is not damage taken.";
    if (c == kMcGreat) return "Attacks marked unblockable in game data. Known hit counts appear beside each move.";
    if (c == kMcGrab) return "Grabs from game data: the grab start, the catch check and the throw. Only lands on a caught player.";
    if (c == kMcHitUnknown) return "Hit count unavailable. Shared hitboxes or projectiles may have no count. Unknown does not mean zero hits.";
    return c == kMcAttack ? "Attacks seen in fights: hits, wind-ups and combo links. Best for drills."
         : c == kMcUnnamedAtk ? "Attacks from the game's move table that were not seen in a fight yet (no name yet). May not play from Stand still."
         : c == kMcTwitch ? "Non-attack states: poses, steps, turns, and reactions. For testing animations; some end immediately when repeated."
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
    if (!N3MoveSelectable(m)) return -1;  // user #14655: unsafe (kills the boss) moves are in no group, so no menu lists them
    if (N3MoveProtected(m->id)) return kMcTwitch;
    // user #14664: attacks first (recorded fights, then the game table's attack-family motions); everything else safe = one group
    int c = kMcTwitch;
    if (m->kind == 'A' || m->kind == 'S' || m->kind == 'L') c = kMcAttack;
    else if (N3MoveAttack(m)) c = kMcUnnamedAtk;
    if (!N3MoveFactsBoss(m->chr)) return c;
    // user #22349: this boss's attacks by game data (Unblockable wins: a great move is listed there only). An unclassified move (kind 0)
    // whose action row has an attack block is an attack too (Yoritomo 0x4E55 / 0x4E59 / 0x4E5E); seen twitches / movement stay twitches.
    const N3MoveFact* f = N3MoveFactFind(m->chr, m->id);
    if (c == kMcTwitch && !(m->kind == 0 && f && f->attack)) return c;
    if (f && f->grab) return kMcGrab;  // human #44217: a grab part (catch / throw) is a Grab whatever its hit rows say
    if (f && f->great) return kMcGreat;
    if (f && f->hits) return kMcHit1 + (f->hits < kMcHitMax ? f->hits : kMcHitMax) - 1;
    return kMcHitUnknown;
}
// user #22349: '2 hits, unblockable' / 'unblockable, hits unknown' for an Unblockable row ("" otherwise: the group name says the hits)
static inline const char* N3MoveHitTag(const N3Move* m, char* b, size_t n) {
    const N3MoveFact* f = N3MoveCat(m) == kMcGreat ? N3MoveFactFind(m->chr, m->id) : nullptr;
    if (!f) { if (n) b[0] = 0; return b; }
    if (f->hits) snprintf(b, n, "%u hit%s, unblockable", (unsigned)f->hits, f->hits == 1 ? "" : "s");
    else snprintf(b, n, "unblockable, hits unknown");
    return b;
}

// human #44241: players name a boss move by type + number ("1Hit3", "2Hit4", "Grab3", "Unblockable8"). N = the 1-based rank of the move's id
// among the boss's moves of the same group, sorted by id, so it is the same on every machine. "" for moves outside the attack groups.
static inline const char* N3MoveLabelId(const N3Move* m, char* b, size_t n) {
    if (n) b[0] = 0;
    const int c = N3MoveCat(m);
    const bool hit = N3MoveHitCat(c);
    if (!(hit || c == kMcGrab || c == kMcGreat || c == kMcHitUnknown)) return b;
    int rank = 1;
    for (int i = 0; i < N3_MOVE_COUNT; ++i) {
        const N3Move* o = &kN3Moves[i];
        if (o->chr == m->chr && o->id < m->id && N3MoveCat(o) == c) ++rank;
    }
    if (hit) snprintf(b, n, "%d%s%d", c - kMcHit1 + 1, c == kMcHit1 + kMcHitMax - 1 ? "+Hit" : "Hit", rank);
    else snprintf(b, n, "%s%d", c == kMcGrab ? "Grab" : c == kMcGreat ? "Unblockable" : "Atk?", rank);
    return b;
}
