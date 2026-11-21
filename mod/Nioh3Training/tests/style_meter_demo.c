// #127085 offline preview of the style meter pop-ups + motion with the real draw code (src/style_meter_draw.h): a scripted climb D -> SSS
// with every bonus kind, rendered at 1080p over a dark background, written as PPM frames (ffmpeg makes the video). Not shipped.
// Build (from mod/Nioh3Training):
//   py -m ziglang cc -target x86_64-windows-gnu -O1 -fms-extensions tests/style_meter_demo.c -o build/style_meter_demo.exe -lgdi32 -lmsimg32 -lole32 -lwindowscodecs
//   build/style_meter_demo.exe OUTDIR   (writes OUTDIR/f0000.ppm ...)
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#define COBJMACROS
#include <wincodec.h>
#include "../src/style_meter.h"
#include "../src/style_meter_fx.h"
#include "../src/style_atlas_gen.h"
__asm__(".section .rdata,\"dr\"\n\t.p2align 4\n\t.globl g_sm_png\ng_sm_png:\n\t.incbin \"style/rank_atlas.png\"\n\t.globl g_sm_png_end\ng_sm_png_end:\n\t.byte 0\n\t.text\n");
extern const unsigned char g_sm_png[], g_sm_png_end[];
static int g_cfg_sm_scale = 100;
static void Log(const char* fmt, ...) { va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap); putchar('\n'); }
#include "../src/style_meter_draw.h"

typedef struct { double t; const char* label; float pts, bonus; } Ev;  // #127171 value, then bonus
int main(int argc, char** argv) {
    const char* out = argc > 1 ? argv[1] : "build/demo";
    CreateDirectoryA(out, 0);
    if (!SmAtlasLoad()) { printf("atlas failed\n"); return 1; }
    const int H = 1080, VW = 1000, VH = 1000;                 // video crop around the meter (right side of a 1920x1080 screen)
    const SmCanvas c = SmCanvasGeom(H, 1);
    const int rx = 1920 * 1690 / 1920, cy = 1080 * 380 / 1080;
    const int left = rx - c.g.w - c.mx, top = cy - SM_LETTER_H_1080P / 2 - c.g.pad - c.my;
    const int cropX = 1920 - VW - 20, cropY = 0;
    static const Ev ev[] = {
        {600, "Perfect Ki", 30, 15}, {1000, "Ninjutsu", 20}, {1700, "Weapon Swap", 12, 25}, {1720, "Counter", 10, 30}, {2500, "Soul Core", 40},
        {2550, "Style Switch", 15}, {3400, "Clean Dodge", 45}, {3420, "Ki Pulse", 30}, {4300, "Ninjutsu", 20}, {4320, "Ninjutsu", 20},
        {5000, "Weapon Swap", 14, 25}, {5050, "Counter", 9, 30}, {5070, "Soul Core", 40},
    };
    static const double rankT[] = {0, 1300, 2200, 3000, 3800, 4600, 5300};  // D C B A S SS SSS
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = c.w; bi.bmiHeader.biHeight = -c.h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    uint32_t* px = 0; HDC mem = CreateCompatibleDC(0); HBITMAP bmp = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, (void**)&px, 0, 0); SelectObject(mem, bmp);
    uint8_t* fr = (uint8_t*)malloc((size_t)VW * VH * 3);
    SmPopQ q; memset(&q, 0, sizeof q); size_t ei = 0; int lastRank = 0; double rankUpT = -1e18, shakeT = -1e18; float eased = 0; SmShownBar shownBar = {-1, 0.f}; int upFrom = -1;
    const double dt = 1000.0 / 30.0;
    for (int n = 0; n < 7 * 30; ++n) {
        const double t = n * dt;
        int rank = 0; for (int r = 0; r < SM_RANKS; ++r) if (t >= rankT[r]) rank = r;
        const double r0 = rankT[rank], r1 = rank + 1 < SM_RANKS ? rankT[rank + 1] : 7000;
        float gauge = (float)((t - r0) / (r1 - r0) * 1000.0); if (rank == SM_RANKS - 1) gauge = (float)((t - r0) / 1000.0 * 1000.0);  // #127273 SSS fills to the end like the game (cap = full) if (gauge > 1000.f) gauge = 1000.f;
        float big = 0;
        while (ei < sizeof ev / sizeof ev[0] && ev[ei].t <= t) { SmPopPush2(&q, ev[ei].label, ev[ei].pts, ev[ei].bonus, t, SM_POP_LIFE_MS); big += ev[ei].pts + ev[ei].bonus; ++ei; }
        if (big >= SM_SHAKE_PTS) shakeT = t;
        (void)lastRank;
        SmPopPrune(&q, t, SM_POP_LIFE_MS);
        SmDrawSt st; memset(&st, 0, sizeof st);
        { const int was = shownBar.rank; if (SmBarStep(&shownBar, rank, gauge, dt)) { rankUpT = t; upFrom = was; } }  // #127253 same fill-to-the-end step as the game
        st.rank = shownBar.rank; st.now = t; st.lifeMs = SM_POP_LIFE_MS; eased = shownBar.g; st.gauge = eased;
        st.scale = 1.f; st.upMs = t - rankUpT; st.prevRank = upFrom; SmShake(t - shakeT, &st.dx, &st.dy); st.glow = SmGlow(rank, t);
        st.npops = q.n; memcpy(st.pops, q.p, sizeof st.pops);
        SmCompose(mem, px, &c, &st); GdiFlush();
        for (int y = 0; y < VH; ++y) for (int x = 0; x < VW; ++x) {
            const int sxp = cropX + x, syp = cropY + y;
            // background: dark blue-grey vignette like a night stage
            const float vx = (sxp - 1300.f) / 1100.f, vy = (syp - 500.f) / 900.f; float l = 1.f - (vx * vx + vy * vy); if (l < 0) l = 0;
            float br = 18 + 30 * l, bg = 22 + 34 * l, bb = 30 + 44 * l;
            const int cx = sxp - left, cyy = syp - top;
            if (cx >= 0 && cx < c.w && cyy >= 0 && cyy < c.h) {
                const uint32_t p = px[cyy * c.w + cx]; const float a = (p >> 24) / 255.f;
                br = ((p >> 16) & 0xFF) + br * (1 - a); bg = ((p >> 8) & 0xFF) + bg * (1 - a); bb = (p & 0xFF) + bb * (1 - a);
            }
            uint8_t* o = fr + ((size_t)y * VW + x) * 3; o[0] = (uint8_t)(br > 255 ? 255 : br); o[1] = (uint8_t)(bg > 255 ? 255 : bg); o[2] = (uint8_t)(bb > 255 ? 255 : bb);
        }
        char path[MAX_PATH]; snprintf(path, sizeof path, "%s/f%04d.ppm", out, n);
        FILE* f = fopen(path, "wb"); if (!f) { printf("cannot write %s\n", path); return 1; }
        fprintf(f, "P6\n%d %d\n255\n", VW, VH); fwrite(fr, 1, (size_t)VW * VH * 3, f); fclose(f);
    }
    printf("demo: %d frames %dx%d in %s (canvas %dx%d, pf %d)\n", 7 * 30, VW, VH, out, c.w, c.h, c.pf);
    return 0;
}
