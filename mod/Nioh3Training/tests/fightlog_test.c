// Ring of the last 20 fights. No game.
#include <stdio.h>
#include <string.h>
#undef NDEBUG
#include <assert.h>
#include "../src/fightlog.h"

static FlClock C(int h, int mi, int s) {
    FlClock c; c.y = 2026; c.mo = 11; c.d = 2; c.h = h; c.mi = mi; c.s = s; return c;
}

int main(void) {
    FlLog L; FlInit(&L);
    char buf[2048];
    assert(FlFormat(&L, buf, (int)sizeof buf) > 0);
    assert(strstr(buf, "no fights"));
    assert(FlEnd(&L, FL_WIN, 1000, 0) == 0);

    FlClock a = C(12, 1, 0), b = C(12, 2, 9);
    FlStart(&L, FL_PRACTICE, "Imagawa Yoshimoto (Edo)", 1000, &a);
    assert(L.open >= 0 && FlFormat(&L, buf, (int)sizeof buf) && strstr(buf, "in progress") && strstr(buf, "Imagawa Yoshimoto"));
    assert(!strstr(buf, "(Edo)"));
    assert(FlEnd(&L, FL_WIN, 1000 + 69000, &b) == 1);
    assert(FlEnd(&L, FL_WIN, 2000, &b) == 0);
    FlFormat(&L, buf, (int)sizeof buf);
    assert(strstr(buf, "Practice") && strstr(buf, "win") && strstr(buf, "1m 9s"));
    assert(strstr(buf, "start 2026-11-02 12:01:00") && strstr(buf, "end 2026-11-02 12:02:09"));

    FlStart(&L, FL_BOSSMATCH, "boss", 5000, &a);
    FlStart(&L, FL_NORMAL, "Saito Toshimitsu", 8000, &b);  // the open Boss Match is closed as quit
    FlFormat(&L, buf, (int)sizeof buf);
    assert(strstr(buf, "Boss Match") && strstr(buf, "quit"));
    assert(strstr(buf, "normal boss") && strstr(buf, "in progress"));
    assert(FlEnd(&L, FL_LOSS, 9000, &b) == 1);
    FlFormat(&L, buf, (int)sizeof buf);
    assert(strstr(buf, "loss"));

    FlInit(&L);
    for (int i = 0; i < 21; ++i) {
        char name[16]; snprintf(name, sizeof name, "B%d", i);
        FlClock s = C(1, 0, i % 60);
        FlStart(&L, FL_NORMAL, name, (uint32_t)(i * 1000), &s);
        FlEnd(&L, FL_WIN, (uint32_t)(i * 1000 + 1000), &s);
    }
    assert(L.count == 20 && L.open < 0);
    FlFormat(&L, buf, (int)sizeof buf);
    assert(!strstr(buf, "B0 |"));
    assert(strstr(buf, "B1 |") && strstr(buf, "B20 |"));
    assert(strstr(buf, "  1. B1 |") && strstr(buf, "  20. B20 |"));
    printf("fightlog_test ok\n");
    return 0;
}
