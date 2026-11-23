// Last 20 fights for the Report. No game access: training.c / the unit test fill the clock and the tick.
#ifndef N3_FIGHTLOG_H
#define N3_FIGHTLOG_H
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { FL_CAP = 20, FL_NAME = 48 };
enum { FL_BOSSMATCH = 1, FL_PRACTICE = 2, FL_NORMAL = 3 };
enum { FL_OPEN = 0, FL_WIN = 1, FL_LOSS = 2, FL_QUIT = 3 };

typedef struct FlClock {
    int y, mo, d, h, mi, s;
} FlClock;

typedef struct FlRec {
    int mode, result;
    uint64_t token;
    char name[FL_NAME];
    char boss_id[32];
    uint32_t character_id;
    uint32_t t0, t1;
    FlClock start, end;
} FlRec;

typedef struct FlLog {
    FlRec rec[FL_CAP];
    int count;  // 0..FL_CAP, how many slots hold a fight
    uint64_t serial;
    int next;   // next slot to write; also the oldest slot once full
    int open;   // slot of the fight that has not ended, or -1
} FlLog;

static inline void FlInit(FlLog* L) {
    memset(L, 0, sizeof *L);
    L->open = -1;
}

static inline void FlCopyName(char* d, const char* s) {
    if (!s || !s[0]) s = "boss";
    const char* cut = strstr(s, " (");
    if (cut && cut > s) snprintf(d, FL_NAME, "%.*s", (int)(cut - s), s);
    else snprintf(d, FL_NAME, "%s", s);
}

static inline void FlClose(FlLog* L, int result, uint32_t now, const FlClock* end) {
    if (!L || L->open < 0) return;
    FlRec* r = &L->rec[L->open];
    if (r->result != FL_OPEN) { L->open = -1; return; }
    r->result = result;
    r->t1 = now;
    if (end) r->end = *end;
    L->open = -1;
}

// A new fight closes an unfinished one as quit (the player left it for another).
static inline void FlStart(FlLog* L, int mode, const char* name, uint32_t now, const FlClock* st) {
    if (!L) return;
    if (L->open >= 0) FlClose(L, FL_QUIT, now, st);
    FlRec* r = &L->rec[L->next];
    memset(r, 0, sizeof *r);
    r->token = ++L->serial;
    r->mode = mode;
    r->result = FL_OPEN;
    r->t0 = now;
    if (st) r->start = *st;
    FlCopyName(r->name, name);
    L->open = L->next;
    L->next = (L->next + 1) % FL_CAP;
    if (L->count < FL_CAP) L->count++;
}

// Identity is captured at start, never read from the currently selected menu row later.
static inline void FlIdentify(FlLog* L, const char* id, uint32_t character_id) {
    if (!L || L->open < 0) return;
    FlRec* r = &L->rec[L->open];
    snprintf(r->boss_id, sizeof r->boss_id, "%s", id && id[0] ? id : "unknown");
    for (char* p = r->boss_id; *p; ++p) if ((unsigned char)*p < 32 || *p == '|') *p = '_';
    r->character_id = character_id;
}

static inline int FlEnd(FlLog* L, int result, uint32_t now, const FlClock* end) {
    if (!L || L->open < 0) return 0;
    FlClose(L, result, now, end);
    return 1;
}

// A late native clear can correct only the exact most-recent attempt, never a loss or newer round.
static inline int FlConfirmWin(FlLog* L, uint64_t token, uint32_t now, const FlClock* end) {
    if (!L || !L->count || !token) return 0;
    FlRec* r = &L->rec[(L->next + FL_CAP - 1) % FL_CAP];
    if (r->token != token || (r->result != FL_OPEN && r->result != FL_QUIT)) return 0;
    if (r->result == FL_OPEN) FlClose(L, FL_WIN, now, end);
    else r->result = FL_WIN; // retain the original boss-mode end time
    return 1;
}

static inline const char* FlModeName(int m) {
    if (m == FL_PRACTICE) return "Practice";
    if (m == FL_BOSSMATCH) return "Boss Match";
    if (m == FL_NORMAL) return "normal boss";
    return "?";
}
static inline const char* FlResultName(int r) {
    if (r == FL_WIN) return "win";
    if (r == FL_LOSS) return "loss";
    if (r == FL_QUIT) return "quit";
    return "in progress";
}

static inline void FlDur(uint32_t t0, uint32_t t1, char* b, int n) {
    uint32_t sec = (t1 - t0) / 1000u;
    if (sec >= 3600u) snprintf(b, (size_t)n, "%uh %um %us", sec / 3600u, (sec % 3600u) / 60u, sec % 60u);
    else if (sec >= 60u) snprintf(b, (size_t)n, "%um %us", sec / 60u, sec % 60u);
    else snprintf(b, (size_t)n, "%us", sec);
}

// Oldest first. Returns bytes written, not counting the trailing 0.
static inline int FlFormat(const FlLog* L, char* buf, int cap) {
    if (!buf || cap <= 0) return 0;
    buf[0] = 0;
    if (!L || L->count <= 0) {
        snprintf(buf, (size_t)cap, "  (no fights this session)\r\n");
        return (int)strlen(buf);
    }
    const int start = (L->count == FL_CAP) ? L->next : 0;
    int used = 0;
    for (int i = 0; i < L->count; ++i) {
        const FlRec* r = &L->rec[(start + i) % FL_CAP];
        char dur[32], endb[32];
        if (r->result == FL_OPEN) snprintf(dur, sizeof dur, "still going");
        else FlDur(r->t0, r->t1, dur, (int)sizeof dur);
        if (r->result == FL_OPEN || r->end.y == 0) snprintf(endb, sizeof endb, "-");
        else snprintf(endb, sizeof endb, "%04d-%02d-%02d %02d:%02d:%02d", r->end.y, r->end.mo, r->end.d, r->end.h, r->end.mi, r->end.s);
        char row[384];
        int n = snprintf(row, sizeof row,
            "  %d. %s | %s | boss_id %s | character_id %u | start %04d-%02d-%02d %02d:%02d:%02d | end %s | %s | %s\r\n",
            i + 1, r->name, FlModeName(r->mode), r->boss_id[0] ? r->boss_id : "unknown", r->character_id,
            r->start.y, r->start.mo, r->start.d, r->start.h, r->start.mi, r->start.s,
            endb, dur, FlResultName(r->result));
        if (n < 0 || n >= (int)sizeof row || used + n >= cap) break;
        memcpy(buf + used, row, (size_t)n + 1);
        used += n;
    }
    return used;
}
#endif
