// paddebug_test.cpp - pc #97485 PadDebug row: Record/Read round-trip, off = no writes, block reasons, stale feed, own polling.
#include "../src/paddebug.h"
#include <cstdio>
#include <cstring>
static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)
int main() {
    char a[96], b[96];
    paddebug::g_on = 0;
    paddebug::Record(1, 77, 0x1000, 1, 2, 3, 4, 5, 6, false, 1000);
    CHECK(paddebug::Read().t == 0);  // off: nothing stored
    paddebug::Format(paddebug::Read(), 5000, -1, a, sizeof a, b, sizeof b);
    CHECK(strstr(a, "this session")); CHECK(!strcmp(b, "Mod blocking pad: no"));
    paddebug::g_on = 1;
    paddebug::Record(0, 0x40010005u, 0x1010, -32768, 32767, 100, -100, 255, 0, false, 1000);
    paddebug::RecordBlock(0);
    paddebug::Format(paddebug::Read(), 1200, 0, a, sizeof a, b, sizeof b);
    CHECK(!strcmp(a, "P0 #5 B1010 LT255 RT0 L-32768,32767 R100,-100")); CHECK(!strcmp(b, "Mod blocking pad: no"));
    paddebug::RecordBlock(paddebug::kMenuOpen | paddebug::kLatch);
    paddebug::Format(paddebug::Read(), 1200, 1, a, sizeof a, b, sizeof b);
    CHECK(!strcmp(b, "Mod blocking pad: yes (menu open, release latch, FormBlockPad)"));
    paddebug::Record(2, 9, 0, 0, 0, 0, 0, 0, 0, true, 1000);  // own polling: EF hides nothing from a game that is not reading
    paddebug::Format(paddebug::Read(), 1200, 0, a, sizeof a, b, sizeof b);
    CHECK(!strncmp(a, "P2* #9 ", 7)); CHECK(!strcmp(b, "Mod blocking pad: no"));
    paddebug::Format(paddebug::Read(), 9000, 1, a, sizeof a, b, sizeof b);  // stale feed
    CHECK(strstr(a, "1.5 s")); CHECK(!strcmp(b, "Mod blocking pad: yes (FormBlockPad)"));
    char tiny[8]; paddebug::Format(paddebug::Read(), 1200, 1, tiny, sizeof tiny, tiny, sizeof tiny); CHECK(strlen(tiny) < 8);
    // widest line fits the 58-column panel
    paddebug::Record(3, 65535, 0xFFFF, -32768, -32768, -32768, -32768, 255, 255, true, 1000);
    paddebug::Format(paddebug::Read(), 1200, 0, a, sizeof a, b, sizeof b);
    CHECK(strlen(a) <= 58);
    printf(fails ? "paddebug_test: %d FAILED\n" : "paddebug_test: all passed\n", fails);
    return fails ? 1 : 0;
}
