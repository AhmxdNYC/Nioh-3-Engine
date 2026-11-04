// #65732: the arena scan marks the worlds whose mission scripts strip items / gear (Nioh3Training_KeepGear level 2). Runs on the game archives.
//   py -m ziglang cc -O2 -o kg_world_test.exe mod/Nioh3Training/tests/kg_world_test.c && kg_world_test.exe <game dir>
#include <windows.h>
#include <stdio.h>
#define AS_LOG(...) ((void)0)
#include "../src/arena_scan.h"
static int nrow, ndf, nother;
static void Row(const AsRow* r, void* ctx) { (void)ctx; ++nrow; if (as_kg_worlds[r->world]) { if (r->world == 0xDF) ++ndf; else ++nother; } }
int main(int argc, char** argv) {
    if (AsScan(argc > 1 ? argv[1] : ".", Row, 0) < 0) { printf("FAIL: scan\n"); return 1; }
    int nw = 0; for (int w = 0; w < 256; ++w) if (as_kg_worlds[w]) { printf("item/gear-script world 0x%02X\n", w); ++nw; }
    printf("%d rows; %d rows in world 0xDF flagged, %d rows in other flagged worlds; %d flagged worlds\n", nrow, ndf, nother, nw);
    if (!as_kg_worlds[0xDF]) { printf("FAIL: world 0xDF (tutorial / dojo) not flagged\n"); return 1; }
    printf("PASS\n"); return 0;
}
