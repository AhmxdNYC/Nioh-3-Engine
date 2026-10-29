// Offline check of src/arena_scan.h (the in-process arena derivation) against tools/auto_arena.py: prints the rows as the TSV columns 1-14.
//   python3 -m ziglang cc -O2 -o /tmp/arena_scan_test mod/Nioh3Training/tests/arena_scan_test.c -lm && /tmp/arena_scan_test <game dir>
#include <stdio.h>
#define AS_LOG(...) (fprintf(stderr, __VA_ARGS__), fputc('\n', stderr))
#include "../src/arena_scan.h"
static void Row(const AsRow* r, void* ctx) {
    (void)ctx;
    printf("0x%04X\t%u\t0x%02X\t%u\t%u\t%.0f\t%.0f\t%.0f\t%.1f\t%.0f\t%.0f\t%.0f\t%s\t%s\n", r->mission, r->start, r->world, r->chr, r->placement,
           r->p[0], r->p[1], r->p[2], r->p[3], r->b[0], r->b[1], r->b[2], r->kind, r->id);
}
int main(int argc, char** argv) { return AsScan(argc > 1 ? argv[1] : ".", Row, 0) < 0; }
