// #35870 Ki press probe, offline.
//   py -m ziglang cc -target x86_64-windows-gnu tests/frame_meter_kipress_test.c -o build/frame_meter_kipress_test.exe && build\frame_meter_kipress_test.exe
#include <stdio.h>
#undef NDEBUG
#include <assert.h>
#include "../src/frame_meter_kipress.h"

static int Run(KpRun* s, int frames, int r1At, KpEv* ev, int cap) {
    int n = 0;
    for (int f = 0; f < frames; ++f) {
        KpEv tmp[4];
        int k = KpStep(s, 1, f == r1At, 20.f - f, 53.9f, (float)f, 1000.f + f, 0, 25160, tmp, 4);
        for (int i = 0; i < k && n < cap; ++i) ev[n++] = tmp[i];
    }
    KpEv tmp[4];
    int k = KpStep(s, 0, 0, 0.f, 0.f, 0.f, 1055.f, 1, 25160, tmp, 4);
    for (int i = 0; i < k && n < cap; ++i) ev[n++] = tmp[i];
    return n;
}

int main(void) {
    KpRun s; KpEv ev[16];
    KpReset(&s);
    int n = Run(&s, 23, -1, ev, 16);
    assert(n == 2 && ev[0].kind == KP_OPEN && ev[1].kind == KP_CLOSE && ev[1].frame == 23 && ev[1].dKi == 55.f);

    const int taps[3] = {2, 11, 21};
    for (int t = 0; t < 3; ++t) {
        KpReset(&s);
        n = Run(&s, 23, taps[t], ev, 16);
        int sawR = 0, sawA = 0;
        for (int i = 0; i < n; ++i) {
            if (ev[i].kind == KP_R1) { assert(ev[i].frame == taps[t]); sawR = 1; }
            if (ev[i].kind == KP_AFTER) { assert(ev[i].frame == taps[t] + 6 && ev[i].dKi == 6.f); sawA = 1; }
        }
        assert(sawR);
        assert(sawA == (taps[t] + 6 < 23));
        assert(ev[n - 1].kind == KP_CLOSE && ev[n - 1].dKi == 55.f);
    }

    KpReset(&s);
    KpEv one;
    assert(KpStep(&s, 0, 1, 0, 0, 0, 1153.f, 0, 2000, &one, 1) == 0);
    printf("frame_meter_kipress_test: all passed\n");
    return 0;
}
