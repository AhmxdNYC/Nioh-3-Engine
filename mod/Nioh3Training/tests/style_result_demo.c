// Offline preview of the style meter fight result card (src/style_result_draw.h) with the real draw code: three sample fights rendered at
// 1080p over a dark background, written as PPM frames (card fade-in + grade letter motion, then the settled card). Not shipped.
// Build (from mod/Nioh3Training):
//   py -m ziglang cc -target x86_64-windows-gnu -O1 -fms-extensions tests/style_result_demo.c -o build/style_result_demo.exe -lgdi32 -lmsimg32 -lole32 -lwindowscodecs
//   build/style_result_demo.exe OUTDIR   (writes OUTDIR/<fight>_<frame>.ppm)
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#define COBJMACROS
#include <wincodec.h>
#include "../src/style_meter.h"
#include "../src/style_meter_fx.h"
#include "../src/style_atlas_gen.h"
#include "../src/style_result.h"
__asm__(".section .rdata,\"dr\"\n\t.p2align 4\n\t.globl g_sm_png\ng_sm_png:\n\t.incbin \"style/rank_atlas.png\"\n\t.globl g_sm_png_end\ng_sm_png_end:\n\t.byte 0\n\t.text\n");
extern const unsigned char g_sm_png[], g_sm_png_end[];
static int g_cfg_sm_scale = 100;
static void Log(const char* fmt, ...) { va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap); putchar('\n'); }
#include "../src/style_meter_draw.h"
#include "../src/style_result_draw.h"

// Build a result the same way the game does: SrTick over a scripted meter.
static SrResult Fight(const float* secAt, const float* gaugeAt, int hold, SrCount n, int moves, int ma) {
    SmCfg c; SmCfgDefaults(&c); SmState s; SmReset(&s); SrFight f; SrReset(&f);
    SrBossHit(&f, &s, 1);
    for (int r = 0; r < SM_RANKS; ++r) { s.rank = r; s.active = 1; s.pts = gaugeAt[r] * c.rankPts[r]; for (double t = 0; t < secAt[r] * 1000.0; t += 16) SrTick(&f, &s, &c, 16); }
    f.dead[0] = 1;
    SrResult res = SrFinishEx(&f, &s, hold, 0); res.n = n; res.penalty = n.damaged * SR_HIT_PENALTY / 100.f; res.rawAvg = res.avg; res.score = res.rawScore - res.penalty; if (res.score < 0.f) res.score = 0.f; res.grade = (int)res.score; if (res.grade > SM_RANKS - 1) res.grade = SM_RANKS - 1; res.n.displaces = n.dodges / 4; res.n.crouchCancels = n.dodges / 5; res.n.flux1 = n.kis / 4; res.n.flux2 = n.kis / 9; res.n.deflects = n.dodges / 3; res.moves = moves; res.maMoves = ma; res.inputs = (uint32_t)(f.ms / 1000.0 * 4.2); res.apm = f.ms > 0 ? (float)(res.inputs * 60000.0 / f.ms) : 0.f;
    return res;
}
int main(int argc, char** argv) {
    const char* out = argc > 1 ? argv[1] : "build/result_demo";
    CreateDirectoryA(out, 0);
    if (!SmAtlasLoad()) { printf("atlas failed\n"); return 1; }
    static const float tA[SM_RANKS] = {8, 14, 30, 50, 40, 12, 3}, gA[SM_RANKS] = {.5f, .5f, .6f, .6f, .5f, .5f, .4f};   // a good fight: ~A
    static const float tB[SM_RANKS] = {70, 10, 5, 5, 5, 10, 40}, gB[SM_RANKS] = {0, .5f, .5f, .5f, .5f, .5f, .9f};      // bursty: held back
    static const float tC[SM_RANKS] = {4, 4, 4, 5, 6, 8, 160}, gC[SM_RANKS] = {.5f, .5f, .5f, .5f, .5f, .5f, 1.f};      // SSS
    const SrCount nA = {187, 14, 41, 12, 3, 6, 5, 2, 9, 3, 1, 11}, nB = {120, 4, 12, 1, 9, 1, 0, 1, 2, 0, 0, 0}, nC = {260, 30, 60, 33, 0, 14, 8, 4, 22, 9, 3, 25};
    struct { const char* name; SrResult r; } fights[] = {
        {"good", Fight(tA, gA, SR_HOLD_PCT, nA, 34, 4)}, {"bursty", Fight(tB, gB, SR_HOLD_PCT, nB, 12, 0)}, {"sss", Fight(tC, gC, SR_HOLD_PCT, nC, 41, 7)},
    };
    {   // human 2026-11-26: page 2 sample breakdowns
        static const float e[3][SM_PC_N] = {{2400, 610, 180, 40, 420, 230, 160, 330, 260, 0, 40, 75, 100, 60, 25, 300, 280, 135, 200, 162, 390},
                                            {1500, 90, 30, 0, 120, 20, 0, 120, 0, 0, 40, 0, 0, 40, 0, 200, 24, 0, 54, 60},
                                            {3900, 1250, 470, 0, 900, 230, 560, 1500, 500, 160, 160, 75, 450, 440, 0, 900, 1200, 310, 300, 324, 1350}};
        static const float l[3][SM_PL_N] = {{900, 380, 40}, {2400, 1100, 120}, {350, 0, 0}};
        for (int fi = 0; fi < 3; ++fi) { SrResult* r = &fights[fi].r; r->earnedTotal = r->lostTotal = 0.f;
            for (int i = 0; i < SM_PC_N; ++i) { r->n.earned[i] = e[fi][i]; r->earnedTotal += e[fi][i]; }
            for (int i = 0; i < SM_PL_N; ++i) { r->n.lost[i] = l[fi][i]; r->lostTotal += l[fi][i]; } }
    }
    fights[0].r.n.blockCounters = 3; fights[2].r.n.blockCounters = 6;  // human 2026-11-28
    // human 2026-11-26: boss names on the title + the place in the boss's best scores (sss: no name known = the old title)
    snprintf(fights[0].r.name, sizeof fights[0].r.name, "Demon of Pride"); fights[0].r.place = 1;
    snprintf(fights[1].r.name, sizeof fights[1].r.name, "Anayama Nobutada & Baba Nobuharu"); fights[1].r.place = 3;
    snprintf(fights[2].r.name, sizeof fights[2].r.name, "Tokugawa Kunimatsu"); snprintf(fights[2].r.note, sizeof fights[2].r.note, "#1 best, build 1.8.48, 2026-11-26 21:05");  // a saved card viewed from the menu
    const int H = 1080; const SrGeo g = SrGeom(H, SR_SCALE_PCT);
    const int VW = g.w + 80, VH = g.h + 80;
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = g.w; bi.bmiHeader.biHeight = -g.h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    uint32_t* px = 0; HDC mem = CreateCompatibleDC(0); HBITMAP bmp = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, (void**)&px, 0, 0); SelectObject(mem, bmp);
    uint32_t* base = (uint32_t*)malloc((size_t)g.w * g.h * 4); uint8_t* fr = (uint8_t*)malloc((size_t)VW * VH * 3);
    for (unsigned fi = 0; fi < sizeof fights / sizeof fights[0]; ++fi) {
        char line[700]; SrFmt(line, sizeof line, &fights[fi].r); printf("%s: %s\n", fights[fi].name, line);
        SrComposeBase(mem, px, &g, &fights[fi].r); GdiFlush(); memcpy(base, px, (size_t)g.w * g.h * 4);
        for (int n = 0; n <= 40; ++n) {
            const double age = n * 1000.0 / 30.0;
            SrComposeFrame(mem, px, base, &g, &fights[fi].r, age);
            const float a = SrCardAlpha(age, SR_SHOW_MS, -1.0);
            for (int y = 0; y < VH; ++y) for (int x = 0; x < VW; ++x) {
                const float vx = (x - VW / 2.f) / VW, vy = (y - VH / 3.f) / VH; float l = 1.f - 1.5f * (vx * vx + vy * vy); if (l < 0) l = 0;
                float br = 40 + 50 * l, bgc = 44 + 46 * l, bb = 40 + 40 * l;
                const int cx = x - 40, cy = y - 40;
                if (cx >= 0 && cx < g.w && cy >= 0 && cy < g.h) {
                    const uint32_t p = px[cy * g.w + cx]; const float pa = (p >> 24) / 255.f * a;
                    br = ((p >> 16) & 0xFF) * a + br * (1 - pa); bgc = ((p >> 8) & 0xFF) * a + bgc * (1 - pa); bb = (p & 0xFF) * a + bb * (1 - pa);
                }
                uint8_t* o = fr + ((size_t)y * VW + x) * 3; o[0] = (uint8_t)(br > 255 ? 255 : br); o[1] = (uint8_t)(bgc > 255 ? 255 : bgc); o[2] = (uint8_t)(bb > 255 ? 255 : bb);
            }
            char path[MAX_PATH]; snprintf(path, sizeof path, "%s/%s_%02d.ppm", out, fights[fi].name, n);
            FILE* f = fopen(path, "wb"); if (!f) { printf("cannot write %s\n", path); return 1; }
            fprintf(f, "P6\n%d %d\n255\n", VW, VH); fwrite(fr, 1, (size_t)VW * VH * 3, f); fclose(f);
        }
        {   // page 2 (points breakdown), settled
            SrComposeBreakdown(mem, px, &g, &fights[fi].r); GdiFlush();
            for (int y = 0; y < VH; ++y) for (int x = 0; x < VW; ++x) {
                float br = 50, bgc = 52, bb = 48; const int cx = x - 40, cy = y - 40;
                if (cx >= 0 && cx < g.w && cy >= 0 && cy < g.h) { const uint32_t q = px[cy * g.w + cx]; const float pa = (q >> 24) / 255.f;
                    br = ((q >> 16) & 0xFF) + br * (1 - pa); bgc = ((q >> 8) & 0xFF) + bgc * (1 - pa); bb = (q & 0xFF) + bb * (1 - pa); }
                uint8_t* o = fr + ((size_t)y * VW + x) * 3; o[0] = (uint8_t)(br > 255 ? 255 : br); o[1] = (uint8_t)(bgc > 255 ? 255 : bgc); o[2] = (uint8_t)(bb > 255 ? 255 : bb);
            }
            char path[MAX_PATH]; snprintf(path, sizeof path, "%s/%s_pts.ppm", out, fights[fi].name);
            FILE* f = fopen(path, "wb"); if (!f) { printf("cannot write %s\n", path); return 1; }
            fprintf(f, "P6\n%d %d\n255\n", VW, VH); fwrite(fr, 1, (size_t)VW * VH * 3, f); fclose(f);
        }
    }
    printf("result demo: card %dx%d, frames in %s\n", g.w, g.h, out);
    return 0;
}
