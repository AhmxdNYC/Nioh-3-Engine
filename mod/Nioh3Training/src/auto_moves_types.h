// pc2 #23907: the runtime move record (auto_moves.h fills it; move_catalog.h and Nioh3BossRematch read it through Nioh3Training_AutoMoves).
#pragma once
#include <stdint.h>
enum { AM_NSUB = 16, AM_ROW = 0x150, AM_HB = 192, AM_MAXMOVES = 2048, AM_MAXROWS = 64 };
// kind: 'A' attack-like (attack block, hit window, GreatAttack or own hitbox rows; listed like kind 0 static rows), 'M' movement / other, 'I' idle.
typedef struct {
    uint16_t id, motion, next, row;
    uint8_t block;     // attack block +0x13C / +0x142 set
    uint8_t great;     // u16 +0x04 bit 0x2000 = GreatAttack (unblockable)
    uint8_t attack;    // block || hit window || great (the static-row rule of tools/add_static_boss_rows.py / gen_boss_move_facts.py)
    uint8_t fact;      // listed in boss_move_facts.h style: block || own hitbox rows || great
    uint8_t hits;      // archive hit count when record_known; otherwise 0 = unknown (nwin retains old parser estimate)
    uint8_t nwin;      // raw hit windows (before the spawn / untimed rule)
    uint8_t nrows, untimed, spawns;  // own hitbox rows, untimed (spawned-object) rows, non-ambient spawn events
    uint8_t link;      // next is a real move (not idle, not self): a sequence link, as in boss_sequences.h
    uint8_t loop;      // the next action repeats itself (looping middle part)
    char kind;
    uint32_t anim_hash; // action row +0xF0; with id/motion identifies archive metadata, never id alone
    uint8_t record_known, zero_power; // a known zero is not missing; raw archive base values, not final HP damage
    char damage[96]; // slash-separated hit groups; pipe-separated alternatives retained verbatim
    // human #44217: part of a grab (catch + throw), auto_moves.h AmGrabs. Sits in what was tail padding, so sizeof(AmMove) and the
    // Nioh3Training_AutoMoves ABI are unchanged (an older Training leaves it 0 = no grab).
    uint8_t grab;
} AmMove;
#ifdef __cplusplus
static_assert(sizeof(AmMove) == 124, "AmMove ABI (Nioh3Training_AutoMoves) changed");
#else
_Static_assert(sizeof(AmMove) == 124, "AmMove ABI (Nioh3Training_AutoMoves) changed");
#endif
typedef struct { int n; uint32_t size, off[AM_NSUB], len[AM_NSUB]; AmMove m[AM_MAXMOVES]; } AmTable;

