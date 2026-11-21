// #127085 style meter drawing, split out of style_meter_game.h so tests/style_meter_demo.c renders the real frames offline.
// Needs before it: windows.h, style_meter.h, style_meter_fx.h, style_atlas_gen.h, wincodec.h (COBJMACROS), g_sm_png / g_sm_png_end,
// int g_cfg_sm_scale, Log(fmt, ...).
#ifndef STYLE_METER_DRAW_H
#define STYLE_METER_DRAW_H
static const GUID SM_CLSID_WICImagingFactory = {0xcacaf262, 0x9370, 0x4615, {0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0x0a}};
static const GUID SM_IID_IWICImagingFactory = {0xec5ec8a9, 0xc395, 0x4314, {0x9c, 0x77, 0x54, 0xd7, 0xa9, 0x35, 0xff, 0x70}};
static const GUID SM_GUID_PBGRA = {0x6fddc324, 0x4e03, 0x4bfe, {0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x10}};
static HDC g_sm_atlas_dc; static HBITMAP g_sm_atlas_bmp;
// #127157 per-letter horizontal shift (atlas px) from the sprite box centre to the centre of its solid ink (alpha > 200), measured
// from the decoded atlas: SS / SSS have a faint slash trail on one side, so box-centred they looked off-centre over the bar.
static float g_sm_letter_dx[SM_RANKS];
#ifndef SM_INK_MASS_W
#define SM_INK_MASS_W 1.0f
#endif
// #127175 bar fill colour per rank (D grey .. SSS red). #127238 letters are now cream like the words, so the colours that used to be
// read from the old letter sprites are kept here instead.
static const uint32_t g_sm_rank_rgb[SM_RANKS] = {0xD8DEE6u, 0xD7DDE5u, 0xE6AD50u, 0xE58037u, 0xE64430u, 0xE64431u, 0xE63C2Au};
// #127259 the bar fills in the letters' colour (cream, style/word_letters.py FILL 236,232,222), the same at every rank.
// SM_BAR_RANK_RGB 1 brings back the per-rank colours above.
#ifndef SM_BAR_RANK_RGB
#define SM_BAR_RANK_RGB 0
#endif
#define SM_LETTER_RGB 0xECE8DEu
static int g_sm_letter_ink[SM_RANKS][4];  // #127353 solid-ink box (sprite px: x0 y0 x1 y1), used to keep katana cuts on the letter
static void SmLetterInk(const uint8_t* px, UINT w) {
    for (int i = 0; i < SM_RANKS; ++i) {
        const int* L = sm_atlas_letter[i]; int lo = -1, hi = -1;
        int* B = g_sm_letter_ink[i]; B[0] = L[2]; B[1] = L[3]; B[2] = -1; B[3] = -1;
        for (int y = 0; y < L[3]; ++y) for (int x = 0; x < L[2]; ++x)
            if (px[((size_t)(L[1] + y) * w + (size_t)(L[0] + x)) * 4 + 3] > 200) { if (x < B[0]) B[0] = x; if (y < B[1]) B[1] = y; if (x > B[2]) B[2] = x; if (y > B[3]) B[3] = y; }
        if (B[2] < 0) { B[0] = 0; B[1] = 0; B[2] = L[2] - 1; B[3] = L[3] - 1; }
        for (int x = 0; x < L[2]; ++x) for (int y = 0; y < L[3]; ++y)
            if (px[((size_t)(L[1] + y) * w + (size_t)(L[0] + x)) * 4 + 3] > 200) { if (lo < 0) lo = x; hi = x; break; }
        // #127187 optical centre: the ink's weight centre (alpha-weighted mean x of the solid ink); SM_INK_MASS_W blends back toward the box centre.
        // Box-only left A / D looking right of the bar and C left of it (open or slanted shapes carry their weight off-centre).
        double sw = 0, sx = 0;
        for (int y = 0; y < L[3]; ++y) for (int x = 0; x < L[2]; ++x) {
            const unsigned al = px[((size_t)(L[1] + y) * w + (size_t)(L[0] + x)) * 4 + 3]; if (al > 200) { sw += al; sx += (double)al * x; }
        }
        const float boxC = (float)(lo + hi) / 2.f, massC = sw > 0 ? (float)(sx / sw) : boxC;
        g_sm_letter_dx[i] = lo < 0 ? 0.f : (float)L[2] / 2.f - (boxC * (1.f - SM_INK_MASS_W) + massC * SM_INK_MASS_W);
    }
}
// #127168 the letter sprites have a soft glow painted around them (human: no glow). Pixels of a letter below SM_HALO_CUT alpha are the
// halo: fade them out with a steep ramp (cut..cut+48) so the solid letter keeps an anti-aliased edge. Premultiplied, so RGB scales too.
#ifndef SM_HALO_CUT
#define SM_HALO_CUT 150
#endif
static void SmLetterDehalo(uint8_t* px, UINT w) {
    for (int i = 0; i < SM_RANKS; ++i) {
        const int* L = sm_atlas_letter[i];
        for (int y = 0; y < L[3]; ++y) for (int x = 0; x < L[2]; ++x) {
            uint8_t* p = &px[((size_t)(L[1] + y) * w + (size_t)(L[0] + x)) * 4];
            const int a = p[3]; if (a >= SM_HALO_CUT + 48) continue;
            const int na = a <= SM_HALO_CUT ? 0 : (a - SM_HALO_CUT) * 255 / 48;
            if (na >= a) continue;
            for (int k = 0; k < 3; ++k) p[k] = a ? (uint8_t)(p[k] * na / a) : 0;
            p[3] = (uint8_t)na;
        }
    }
}
// Decode the embedded atlas into a premultiplied BGRA top-down DIB (the AlphaBlend source). Returns 1 on success.
static int SmAtlasLoad(void) {
    typedef HRESULT (WINAPI *CoInitFn)(LPVOID, DWORD);
    typedef HRESULT (WINAPI *CoCreateFn)(REFCLSID, LPUNKNOWN, DWORD, REFIID, LPVOID*);
    HMODULE ole = LoadLibraryW(L"ole32.dll");
    CoInitFn coInit = ole ? (CoInitFn)(void*)GetProcAddress(ole, "CoInitializeEx") : 0;
    CoCreateFn coCreate = ole ? (CoCreateFn)(void*)GetProcAddress(ole, "CoCreateInstance") : 0;
    if (!coInit || !coCreate) { Log("STYLE METER: ole32 not available, no sprites"); return 0; }
    coInit(0, COINIT_MULTITHREADED);
    IWICImagingFactory* f = 0; IWICStream* st = 0; IWICBitmapDecoder* dec = 0; IWICBitmapFrameDecode* fr = 0; IWICFormatConverter* cv = 0;
    UINT w = 0, h = 0; int ok = 0; HRESULT hr;
    const DWORD size = (DWORD)(g_sm_png_end - g_sm_png);
    hr = coCreate(&SM_CLSID_WICImagingFactory, 0, CLSCTX_INPROC_SERVER, &SM_IID_IWICImagingFactory, (LPVOID*)&f);
    if (SUCCEEDED(hr)) hr = IWICImagingFactory_CreateStream(f, &st);
    if (SUCCEEDED(hr)) hr = IWICStream_InitializeFromMemory(st, (BYTE*)g_sm_png, size);
    if (SUCCEEDED(hr)) hr = IWICImagingFactory_CreateDecoderFromStream(f, (IStream*)st, 0, WICDecodeMetadataCacheOnDemand, &dec);
    if (SUCCEEDED(hr)) hr = IWICBitmapDecoder_GetFrame(dec, 0, &fr);
    if (SUCCEEDED(hr)) hr = IWICImagingFactory_CreateFormatConverter(f, &cv);
    if (SUCCEEDED(hr)) hr = IWICFormatConverter_Initialize(cv, (IWICBitmapSource*)fr, &SM_GUID_PBGRA, WICBitmapDitherTypeNone, 0, 0.0, WICBitmapPaletteTypeCustom);
    if (SUCCEEDED(hr)) hr = IWICFormatConverter_GetSize(cv, &w, &h);
    if (SUCCEEDED(hr) && w == SM_ATLAS_W && h == SM_ATLAS_H) {
        BITMAPINFO bi; memset(&bi, 0, sizeof bi);
        bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = (LONG)w; bi.bmiHeader.biHeight = -(LONG)h;
        bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
        void* px = 0; HDC scr = GetDC(0);
        g_sm_atlas_bmp = CreateDIBSection(scr, &bi, DIB_RGB_COLORS, &px, 0, 0);
        g_sm_atlas_dc = CreateCompatibleDC(scr); ReleaseDC(0, scr);
        if (g_sm_atlas_bmp && px && g_sm_atlas_dc) {
            hr = IWICFormatConverter_CopyPixels(cv, 0, w * 4, w * 4 * h, (BYTE*)px);
            if (SUCCEEDED(hr)) { SelectObject(g_sm_atlas_dc, g_sm_atlas_bmp); GdiFlush(); SmLetterDehalo((uint8_t*)px, w); SmLetterInk((const uint8_t*)px, w); ok = 1; }
        }
    } else if (SUCCEEDED(hr)) hr = E_FAIL;
    if (cv) IWICFormatConverter_Release(cv);
    if (fr) IWICBitmapFrameDecode_Release(fr);
    if (dec) IWICBitmapDecoder_Release(dec);
    if (st) IWICStream_Release(st);
    if (f) IWICImagingFactory_Release(f);
    if (!ok) { if (g_sm_atlas_dc) DeleteDC(g_sm_atlas_dc); if (g_sm_atlas_bmp) DeleteObject(g_sm_atlas_bmp); g_sm_atlas_dc = 0; g_sm_atlas_bmp = 0; }
    Log("STYLE METER: sprite atlas %s (%lu bytes embedded, %ux%u, hr %08lX)", ok ? "decoded" : "NOT decoded, text fallback", size, w, h, (unsigned long)hr);
    return ok;
}
// Layout in window pixels for client height H (1080 = the sprite design size x SM_LETTER_H_1080P / tallest letter).
typedef struct { float k; int w, h, pad; } SmGeo;
// #127175 the bar: SM_BAR_HK x the sprite gauge height, drawn in code (rank colour fill, shaded, quarter ticks, bright tip, moving sheen)
#ifndef SM_BAR_HK
#define SM_BAR_HK 1.7f   // #127273 thinner (was 2.2) to match the letter
#endif
// #127273 bar length as a share of the sprite gauge width (human: smaller, in proportion to the rank letter)
#ifndef SM_BAR_WK
#define SM_BAR_WK 0.72f
#endif
// #127148 the rank letter is drawn at SM_LETTER_PCT % of the word / gauge scale (it was too big); [StyleMeter] scale still scales all.
#ifndef SM_LETTER_PCT
#define SM_LETTER_PCT 60
#endif
#define SM_LK ((float)SM_LETTER_PCT / 100.f)
// #127242 the rank word under the letter is off (human: remove the words, bring the ranks closer to the meter). 1 brings the word row back.
#ifndef SM_SHOW_WORD
#define SM_SHOW_WORD 0
#endif
// tallest letter / word in the atlas: the letter and word rows are always this tall, so the word and gauge never move with the rank (#127152)
static void SmAtlasMax(int* lw, int* lh, int* ww, int* wh) {
    *lw = *lh = *ww = *wh = 0;
    for (int i = 0; i < SM_RANKS; ++i) { if (sm_atlas_letter[i][2] > *lw) *lw = sm_atlas_letter[i][2]; if (sm_atlas_letter[i][3] > *lh) *lh = sm_atlas_letter[i][3];
        if (sm_atlas_word[i][2] > *ww) *ww = sm_atlas_word[i][2]; if (sm_atlas_word[i][3] > *wh) *wh = sm_atlas_word[i][3]; }
}
static SmGeo SmGeom(int H) {
    SmGeo g; int lw = 0, lh = 0, ww = 0, wh = 0;
    for (int i = 0; i < SM_RANKS; ++i) { if (sm_atlas_letter[i][2] > lw) lw = sm_atlas_letter[i][2]; if (sm_atlas_letter[i][3] > lh) lh = sm_atlas_letter[i][3];
        if (sm_atlas_word[i][2] > ww) ww = sm_atlas_word[i][2]; if (sm_atlas_word[i][3] > wh) wh = sm_atlas_word[i][3]; }
    g.k = (float)H / 1080.f * (float)g_cfg_sm_scale / 100.f * (float)SM_LETTER_H_1080P / (float)lh;
    g.pad = (int)(8 * g.k) + 2;
    const int gw = sm_atlas_gauge_bg[2] > ww ? sm_atlas_gauge_bg[2] : ww;
    const float lwk = lw * SM_LK;
    g.w = (int)((lwk > gw ? lwk : (float)gw) * g.k) + 2 * g.pad;
    g.h = SM_SHOW_WORD ? (int)((lh * SM_LK + wh + sm_atlas_gauge_bg[3] * SM_BAR_HK) * g.k) + 4 * g.pad
                       : (int)((lh * SM_LK + sm_atlas_gauge_bg[3] * SM_BAR_HK) * g.k) + 2 * g.pad + g.pad / 3;  // letter, small gap, bar
    return g;
}
static void SmBlit(HDC dst, int x, int y, const int* r, int srcW, float k) {
    const int dw = (int)(srcW * k + 0.5f), dh = (int)(r[3] * k + 0.5f);
    if (dw <= 0 || dh <= 0 || srcW <= 0) return;
    BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    AlphaBlend(dst, x, y, dw, dh, g_sm_atlas_dc, r[0], r[1], srcW, r[3], bf);
}
// #127085 one frame of the meter with motion + pop-ups. The canvas is the old meter box (SmGeom) plus a margin on every side for the
// bounce / shake and a pop-up column under the gauge; the old box sits at (mx, my) so the meter does not move on screen.
typedef struct {
    int rank;              // 0..6
    float gauge;           // eased fill 0..1000
    float scale;           // rank-up bounce (1 = rest)
    float dx, dy;          // shake, 1080p pixels
    float glow;            // 0..1 glow pulse (S and up)
    SmPop pops[SM_POPS]; int npops;  // p[0] oldest
    double now; int lifeMs;
    double upMs; int prevRank;   // #127299 ms since the shown rank stepped up (<0 / large = none), the rank before it
} SmDrawSt;
// #127289 pop-up text size as a share of pf (human: in proportion with the shorter bar, #127273 SM_BAR_WK)
#ifndef SM_POP_K
#define SM_POP_K 0.72f
#endif
typedef struct { SmGeo g; int w, h, mx, my, pf, popH, pp; } SmCanvas;  // pp = pop-up font px
static SmCanvas SmCanvasGeom(int H, int pops) {
    SmCanvas c; c.g = SmGeom(H);
    int lw = 0, lh = 0;
    for (int i = 0; i < SM_RANKS; ++i) { if (sm_atlas_letter[i][2] > lw) lw = sm_atlas_letter[i][2]; if (sm_atlas_letter[i][3] > lh) lh = sm_atlas_letter[i][3]; }
    const float letterPx = lh * c.g.k;
    c.pf = (int)(letterPx * 0.12f + 0.5f); if (c.pf < 10) c.pf = 10;
    c.pp = (int)(c.pf * SM_POP_K + 0.5f); if (c.pp < 9) c.pp = 9;
    c.popH = pops ? (int)(c.pp * 1.25f) * SM_POPS + c.g.pad : 0;
    c.mx = (int)(lw * c.g.k * 0.34f) + (int)(10.f * (float)H / 1080.f) + 2;
    c.my = (int)(lh * c.g.k * 0.34f) + (int)(10.f * (float)H / 1080.f) + 2;
    c.w = c.g.w + 2 * c.mx; c.h = c.g.h + c.my + c.popH + c.my / 2;
    return c;
}
// premultiplied 'over' of one text coverage mask (tpx R channel, white-on-black GDI text) at (ox, oy), colour col, opacity a
static void SmMaskOver(uint32_t* px, int W, int H, const uint32_t* tpx, int TW, int TH, int ox, int oy, uint32_t col, float a) {
    if (a <= 0.f) return;
    const unsigned cr = (col >> 16) & 0xFF, cg = (col >> 8) & 0xFF, cb = col & 0xFF;
    for (int y = 0; y < TH; ++y) {
        const int yy = oy + y; if (yy < 0 || yy >= H) continue;
        for (int x = 0; x < TW; ++x) {
            const unsigned cov = tpx[y * TW + x] & 0xFF; if (!cov) continue;
            const int xx = ox + x; if (xx < 0 || xx >= W) continue;
            const unsigned sa = (unsigned)(cov * a + 0.5f); if (!sa) continue;
            uint32_t* d = &px[yy * W + xx]; const uint32_t dv = *d; const unsigned inv = 255 - sa;
            const unsigned r = cr * sa / 255 + ((dv >> 16) & 0xFF) * inv / 255, g = cg * sa / 255 + ((dv >> 8) & 0xFF) * inv / 255,
                           b = cb * sa / 255 + (dv & 0xFF) * inv / 255, al = sa + (dv >> 24) * inv / 255;
            *d = (al > 255 ? 255u : al) << 24 | (r > 255 ? 255u : r) << 16 | (g > 255 ? 255u : g) << 8 | (b > 255 ? 255u : b);
        }
    }
}
static void SmBlitA(HDC dst, int x, int y, const int* r, int srcW, float k, int alpha) {
    const int dw = (int)(srcW * k + 0.5f), dh = (int)(r[3] * k + 0.5f);
    if (dw <= 0 || dh <= 0 || srcW <= 0 || alpha <= 0) return;
    BLENDFUNCTION bf = {AC_SRC_OVER, 0, (BYTE)(alpha > 255 ? 255 : alpha), AC_SRC_ALPHA};
    AlphaBlend(dst, x, y, dw, dh, g_sm_atlas_dc, r[0], r[1], srcW, r[3], bf);
}
// premultiplied 'over' of one pixel
static inline void SmPxOver(uint32_t* d, unsigned r, unsigned g, unsigned b, unsigned a) {
    if (!a) return; const uint32_t dv = *d; const unsigned inv = 255 - a;
    const unsigned rr = r * a / 255 + ((dv >> 16) & 0xFF) * inv / 255, gg = g * a / 255 + ((dv >> 8) & 0xFF) * inv / 255,
                   bb = b * a / 255 + (dv & 0xFF) * inv / 255, al = a + (dv >> 24) * inv / 255;
    *d = (al > 255 ? 255u : al) << 24 | (rr > 255 ? 255u : rr) << 16 | (gg > 255 ? 255u : gg) << 8 | (bb > 255 ? 255u : bb);
}
static inline unsigned SmMix(unsigned a, unsigned b, float t) { return (unsigned)(a + ((float)b - (float)a) * t + 0.5f); }
// #127175 the style bar: dark track with a thin rim, fill in the rank's own letter colour (shaded top to bottom), quarter ticks,
// a bright tip at the fill edge and a soft sheen that sweeps along the fill (period SM_SHEEN_MS). gauge 0..1000, now in ms.
#ifndef SM_SHEEN_MS
#define SM_SHEEN_MS 1600.0
#endif
static void SmBar(uint32_t* px, int W, int H, int x0, int y0, int bw, int bh, float gauge, uint32_t rgb, double now) {
    unsigned cr = (rgb >> 16) & 0xFF, cg = (rgb >> 8) & 0xFF, cb = rgb & 0xFF;
    if (!rgb) { cr = 0xF5; cg = 0xCD; cb = 0x8B; }
    {   // lift dark / grey letters so the fill reads on a dark track
        const unsigned mx = cr > cg ? (cr > cb ? cr : cb) : (cg > cb ? cg : cb);
        if (mx > 0 && mx < 220) { cr = cr * 220 / mx; cg = cg * 220 / mx; cb = cb * 220 / mx; }
    }
    const float f = gauge < 0.f ? 0.f : gauge > 1000.f ? 1.f : gauge / 1000.f;
    const int fw = (int)(bw * f + 0.5f), tip = bh / 2 + 2;
    const float sheenX = (float)(fmod(now, SM_SHEEN_MS) / SM_SHEEN_MS) * (float)(fw + 2 * bh) - (float)bh;
    for (int y = 0; y < bh; ++y) {
        const int yy = y0 + y; if (yy < 0 || yy >= H) continue;
        const float v = bh > 1 ? (float)y / (float)(bh - 1) : 0.f;            // 0 top .. 1 bottom
        const int rim = y == 0 || y == bh - 1;
        for (int x = 0; x < bw; ++x) {
            const int xx = x0 + x; if (xx < 0 || xx >= W) continue;
            uint32_t* d = &px[yy * W + xx];
            if (rim || x == 0 || x == bw - 1) { SmPxOver(d, 200, 200, 210, 150); continue; }
            if (x >= fw) { SmPxOver(d, 18, 18, 24, 190); }
            else {
                const float shade = v < 0.4f ? 1.f + (0.4f - v) * 0.9f : 1.f - (v - 0.4f) * 0.55f;   // bright upper band, darker bottom
                unsigned r = (unsigned)(cr * shade), g = (unsigned)(cg * shade), b = (unsigned)(cb * shade);
                if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
                const float ds = (float)x - sheenX; const float sh = ds > -bh && ds < bh ? 1.f - (ds < 0 ? -ds : ds) / (float)bh : 0.f;
                if (sh > 0.f) { r = SmMix(r, 255, sh * 0.45f); g = SmMix(g, 255, sh * 0.45f); b = SmMix(b, 255, sh * 0.45f); }
                if (x >= fw - tip) { const float t = (float)(x - (fw - tip)) / (float)tip; r = SmMix(r, 255, t * 0.85f); g = SmMix(g, 255, t * 0.85f); b = SmMix(b, 255, t * 0.85f); }
                SmPxOver(d, r, g, b, 245);
            }
            for (int q = 1; q < 4; ++q) if (x == bw * q / 4) SmPxOver(d, 0, 0, 0, 150);   // quarter ticks
        }
    }
}
// #127299 premultiplied white flash over what is already drawn in a rect (keeps alpha: only lit pixels brighten)
static void SmFlashRect(uint32_t* px, int w, int h, int x0, int y0, int rw, int rh, float f) {
    if (f <= 0.f) return;
    if (f > 1.f) f = 1.f;
    for (int yy = y0 < 0 ? 0 : y0; yy < y0 + rh && yy < h; ++yy) for (int xx = x0 < 0 ? 0 : x0; xx < x0 + rw && xx < w; ++xx) {
        uint32_t* d = &px[yy * w + xx]; const unsigned a = *d >> 24; if (!a) continue;
        unsigned r = (*d >> 16) & 0xFF, g = (*d >> 8) & 0xFF, b = *d & 0xFF;
        r += (unsigned)((a - (r > a ? a : r)) * f); g += (unsigned)((a - (g > a ? a : g)) * f); b += (unsigned)((a - (b > a ? a : b)) * f);
        *d = a << 24 | (r > a ? a : r) << 16 | (g > a ? a : g) << 8 | (b > a ? a : b);
    }
}
// #127299 sword-slash streak across the letter: a thin bright cut sweeping lower-left -> upper-right, tapered tail, fades at the end
static void SmSlash(uint32_t* px, int w, int h, float cx, float cy, float lw, float lh, float s, float unit, float tilt) {
    const float ax = cx - lw * 0.9f, ay = cy + lh * tilt * 1.2f, bx = cx + lw * 0.9f, by = cy - lh * tilt * 1.2f;  // #127338 hairline, a touch past the letter
    const float dx = bx - ax, dy = by - ay, len2 = dx * dx + dy * dy, len = sqrtf(len2);
    if (len2 <= 1.f) return;
    const float head = s * 1.25f, tail = head - 0.45f, wmax = 1.8f * unit + 0.6f, fade = s < 0.7f ? 1.f : (1.f - s) / 0.3f;
    const int x0 = (int)(ax - wmax - 2), x1 = (int)(bx + wmax + 2), y0 = (int)((by < ay ? by : ay) - wmax - 2), y1 = (int)((ay > by ? ay : by) + wmax + 2);
    for (int yy = y0 < 0 ? 0 : y0; yy <= y1 && yy < h; ++yy) for (int xx = x0 < 0 ? 0 : x0; xx <= x1 && xx < w; ++xx) {
        const float p = ((xx - ax) * dx + (yy - ay) * dy) / len2;
        if (p < tail || p > head || p < 0.f || p > 1.f) continue;
        const float d = fabsf((xx - ax) * dy - (yy - ay) * dx) / len;
        const float wd = wmax * (p - tail) / (head - tail), gw = wd + 2.f * unit;
        if (d >= gw) continue;
        if (d >= wd) { const float a = (1.f - (d - wd) / (gw - wd)) * 0.25f * fade; if (a > 0.f) SmPxOver(&px[yy * w + xx], 255, 214, 160, (unsigned)(a * 255.f)); continue; }
        const float a = (1.f - 0.5f * d / wd) * fade;
        if (a <= 0.f) continue;
        SmPxOver(&px[yy * w + xx], 255, 253, 246, (unsigned)(a * 255.f));
    }
}
// #127304 parry sparks: hot streaks flung out of the impact point, pulled down by gravity, white -> orange -> red as they cool.
// Deterministic per rank (seed) so the same rank-up always looks the same. e = ms since impact.
static void SmSparks(uint32_t* px, int w, int h, float cx, float cy, float e, int n, unsigned seed, float unit, int embers) {
    for (int i = 0; i < n; ++i) {
        const float life = embers ? 420.f + 220.f * SmRand01(seed, i, 1) : 260.f + 200.f * SmRand01(seed, i, 1);
        const float t = (e - (embers ? 40.f * SmRand01(seed, i, 5) : 0.f)) / 1000.f; if (t < 0.f || t * 1000.f >= life) continue;
        float vx, vy, g;
        if (embers) {  // rising embers: drift up and sideways from the bottom of the letter
            vx = (SmRand01(seed, i, 2) - 0.5f) * 140.f; vy = -(90.f + 150.f * SmRand01(seed, i, 3)); g = -60.f;
        } else {        // sparks: mostly sideways fan (a blade-on-blade spray), some up
            const float a = (SmRand01(seed, i, 2) - 0.5f) * 2.6f + (i & 1 ? 3.14159f : 0.f);
            const float v = 380.f + 420.f * SmRand01(seed, i, 3); vx = cosf(a) * v; vy = sinf(a) * v * 0.75f - 120.f; g = 900.f;
        }
        const float ox = embers ? (SmRand01(seed, i, 4) - 0.5f) * 80.f : 0.f, oy = embers ? 30.f : 0.f;
        const float x = cx + (ox + vx * t) * unit, y = cy + (oy + vy * t + 0.5f * g * t * t) * unit;
        const float cvx = vx, cvy = vy + g * t, tl = embers ? 0.02f : 0.05f;  // streak length = velocity x tl
        const float x2 = x - cvx * tl * unit, y2 = y - cvy * tl * unit;
        const float u = t * 1000.f / life, al = (1.f - u * u);
        unsigned r, gg, b;
        if (embers) { r = 255; gg = (unsigned)(150 - 90 * u); b = (unsigned)(40 - 30 * u); }
        else if (u < 0.35f) { r = 255; gg = (unsigned)(250 - 110 * u / 0.35f); b = (unsigned)(220 - 170 * u / 0.35f); }
        else { r = 255; gg = (unsigned)(140 - 90 * (u - 0.35f) / 0.65f); b = (unsigned)(50 - 30 * (u - 0.35f) / 0.65f); }
        const float len = sqrtf((x - x2) * (x - x2) + (y - y2) * (y - y2)); const int steps = (int)len + 1;
        for (int k = 0; k <= steps; ++k) {
            const float q = (float)k / steps, px0 = x2 + (x - x2) * q, py0 = y2 + (y - y2) * q;
            const int ix = (int)px0, iy = (int)py0; if (ix < 0 || iy < 0 || ix + 1 >= w || iy + 1 >= h) continue;
            const unsigned a = (unsigned)(al * (0.35f + 0.65f * q) * 255.f);  // brighter head, faint tail
            SmPxOver(&px[iy * w + ix], r, gg, b, a);
            SmPxOver(&px[iy * w + ix + 1], r, gg, b, a * 3 / 4); SmPxOver(&px[(iy + 1) * w + ix], r, gg, b, a * 3 / 4);
        }
    }
}
// #127304 ki shockwave ring: thin pale-blue ring expanding out of the letter, thinning and fading
static void SmRing(uint32_t* px, int w, int h, float cx, float cy, float e, float r0, float unit) {
    const float dur = 360.f; if (e < 0.f || e >= dur) return;
    const float u = e / dur, R = r0 * (0.5f + 0.75f * (1.f - (1.f - u) * (1.f - u))) /* #127364 stays near the letter */, th = (5.f * (1.f - u) + 1.f) * unit, al = (1.f - u) * 0.9f;
    const int x0 = (int)(cx - R - th - 1), x1 = (int)(cx + R + th + 1), y0 = (int)(cy - R * 0.55f - th - 1), y1 = (int)(cy + R * 0.55f + th + 1);
    for (int yy = y0 < 0 ? 0 : y0; yy <= y1 && yy < h; ++yy) for (int xx = x0 < 0 ? 0 : x0; xx <= x1 && xx < w; ++xx) {
        const float dx = xx - cx, dy = (yy - cy) / 0.55f, d = fabsf(sqrtf(dx * dx + dy * dy) - R);  // flattened ellipse: a ring on the ground plane
        if (d >= th) continue;
        SmPxOver(&px[yy * w + xx], 205, 232, 255, (unsigned)((1.f - d / th) * al * 255.f));
    }
}
// ---- #127315 a different rank-up motion per rank (human: "sort of reused same transition") -----------------------------
// Pixel helpers work on the canvas after GdiFlush; letters are drawn first, so the rect holds only letter pixels.
static void SmTintRect(uint32_t* px, int w, int h, int x0, int y0, int rw, int rh, uint32_t rgb, float f) {
    if (f <= 0.f) return;
    if (f > 1.f) f = 1.f;
    const unsigned tr = (rgb >> 16) & 0xFF, tg = (rgb >> 8) & 0xFF, tb = rgb & 0xFF;
    for (int yy = y0 < 0 ? 0 : y0; yy < y0 + rh && yy < h; ++yy) for (int xx = x0 < 0 ? 0 : x0; xx < x0 + rw && xx < w; ++xx) {
        uint32_t* d = &px[yy * w + xx]; const unsigned a = *d >> 24; if (!a) continue;
        const float r = (float)((*d >> 16) & 0xFF), g = (float)((*d >> 8) & 0xFF), b = (float)(*d & 0xFF);
        const unsigned nr = (unsigned)(r + (tr * a / 255.f - r) * f), ng = (unsigned)(g + (tg * a / 255.f - g) * f), nb = (unsigned)(b + (tb * a / 255.f - b) * f);
        *d = a << 24 | (nr > a ? a : nr) << 16 | (ng > a ? a : ng) << 8 | (nb > a ? a : nb);
    }
}
// ink-brush reveal: clear everything right of a ragged front; the band just behind the front is wet ink (dark red)
static void SmBrushRect(uint32_t* px, int w, int h, int x0, int y0, int rw, int rh, float front, float unit) {
    const float band = 12.f * unit;
    for (int yy = y0 < 0 ? 0 : y0; yy < y0 + rh && yy < h; ++yy) {
        const float jf = front + (SmRand01(0xB2u, (unsigned)(yy / 3), 0) - 0.5f) * 14.f * unit;
        for (int xx = x0 < 0 ? 0 : x0; xx < x0 + rw && xx < w; ++xx) {
            uint32_t* d = &px[yy * w + xx]; if (!(*d >> 24)) continue;
            if (xx > jf) { *d = 0; continue; }
            if (xx > jf - band) { const unsigned a = *d >> 24; const float f = 0.85f * (1.f - (jf - xx) / band);
                const unsigned r = (unsigned)(((*d >> 16) & 0xFF) * (1 - f) + 150 * a / 255.f * f), g = (unsigned)(((*d >> 8) & 0xFF) * (1 - f) + 18 * a / 255.f * f), b = (unsigned)((*d & 0xFF) * (1 - f) + 24 * a / 255.f * f);
                *d = a << 24 | r << 16 | g << 8 | b; }
        }
    }
}
// cut in two along the slash line through (cx, cy): the two halves slide apart along the cut and fade
static void SmSplitRect(uint32_t* px, int w, int h, int x0, int y0, int rw, int rh, float cx, float cy, float tilt, float dist, float fade) {
    if (rw <= 0 || rh <= 0) return;
    if (fade > 1.f) fade = 1.f;
    if (fade < 0.f) fade = 0.f;  // a negative fade cast to unsigned drew the halves solid white
    uint32_t* tmp = (uint32_t*)malloc((size_t)rw * rh * 4); if (!tmp) return;
    for (int yy = 0; yy < rh; ++yy) for (int xx = 0; xx < rw; ++xx) {
        const int X = x0 + xx, Y = y0 + yy; uint32_t v = 0;
        if (X >= 0 && Y >= 0 && X < w && Y < h) { v = px[Y * w + X]; px[Y * w + X] = 0; }
        tmp[yy * rw + xx] = v;
    }
    float dx = (float)rw * 1.5f, dy = -(float)rh * 2.f * tilt; const float l = sqrtf(dx * dx + dy * dy); dx /= l; dy /= l;
    for (int yy = 0; yy < rh; ++yy) for (int xx = 0; xx < rw; ++xx) {
        const uint32_t v = tmp[yy * rw + xx]; const unsigned a = v >> 24; if (!a) continue;
        const float rx = x0 + xx - cx, ry = y0 + yy - cy, side = (rx * dy - ry * dx) > 0 ? 1.f : -1.f;  // which side of the cut
        const int X = (int)(x0 + xx - side * (dx * dist + dy * dist * 0.25f)), Y = (int)(y0 + yy - side * (dy * dist - dx * dist * 0.25f));
        if (X < 0 || Y < 0 || X >= w || Y >= h) continue;
        SmPxOver(&px[Y * w + X], (unsigned)(((v >> 16) & 0xFF) * 255 / a), (unsigned)(((v >> 8) & 0xFF) * 255 / a), (unsigned)((v & 0xFF) * 255 / a), (unsigned)(a * fade));
    }
    free(tmp);
}
// #127348 vertical split along a near-vertical cut through (cx, cy) (x = cx + (y - cy) * k): the left half slides left, the right half
// right, both fall (dy) and fade. Used by the A overhead cut.
static void SmSplitV(uint32_t* px, int w, int h, int x0, int y0, int rw, int rh, float cx, float cy, float k, float dx, float dy, float fade) {
    if (rw <= 0 || rh <= 0) return;
    if (fade > 1.f) fade = 1.f;
    if (fade <= 0.f) fade = 0.f;
    uint32_t* tmp = (uint32_t*)malloc((size_t)rw * rh * 4); if (!tmp) return;
    for (int yy = 0; yy < rh; ++yy) for (int xx = 0; xx < rw; ++xx) {
        const int X = x0 + xx, Y = y0 + yy; uint32_t v = 0;
        if (X >= 0 && Y >= 0 && X < w && Y < h) { v = px[Y * w + X]; px[Y * w + X] = 0; }
        tmp[yy * rw + xx] = v;
    }
    if (fade > 0.f) for (int yy = 0; yy < rh; ++yy) for (int xx = 0; xx < rw; ++xx) {
        const uint32_t v = tmp[yy * rw + xx]; const unsigned a = v >> 24; if (!a) continue;
        const float side = (x0 + xx) > cx + (y0 + yy - cy) * k ? 1.f : -1.f;
        const int X = (int)(x0 + xx + side * dx), Y = (int)(y0 + yy + dy * (side > 0 ? 1.f : 0.8f));
        if (X < 0 || Y < 0 || X >= w || Y >= h) continue;
        SmPxOver(&px[Y * w + X], (unsigned)(((v >> 16) & 0xFF) * 255 / a), (unsigned)(((v >> 8) & 0xFF) * 255 / a), (unsigned)((v & 0xFF) * 255 / a), (unsigned)(a * fade));
    }
    free(tmp);
}
// shatter: the letter breaks into small tiles flung out from its centre, falling and fading (t = ms)
static void SmShatterRect(uint32_t* px, int w, int h, int x0, int y0, int rw, int rh, float t, float unit) {
    if (rw <= 0 || rh <= 0) return;
    uint32_t* tmp = (uint32_t*)malloc((size_t)rw * rh * 4); if (!tmp) return;
    for (int yy = 0; yy < rh; ++yy) for (int xx = 0; xx < rw; ++xx) {
        const int X = x0 + xx, Y = y0 + yy; uint32_t v = 0;
        if (X >= 0 && Y >= 0 && X < w && Y < h) { v = px[Y * w + X]; px[Y * w + X] = 0; }
        tmp[yy * rw + xx] = v;
    }
    const int ts = (int)(7.f * unit) + 2; const float ts_ = t / 1000.f, fade = 1.f - t / 420.f; if (fade <= 0.f) { free(tmp); return; }
    for (int ty = 0; ty < rh; ty += ts) for (int tx = 0; tx < rw; tx += ts) {
        const unsigned id = (unsigned)(ty / ts * 97 + tx / ts);
        const float ox = tx + ts / 2.f - rw / 2.f, oy = ty + ts / 2.f - rh / 2.f, ol = sqrtf(ox * ox + oy * oy) + 1.f;
        const float sp = (180.f + 260.f * SmRand01(0x5Au, id, 1)) * unit;
        const float vx = ox / ol * sp + (SmRand01(0x5Au, id, 2) - 0.5f) * 120.f * unit, vy = oy / ol * sp - 160.f * unit * SmRand01(0x5Au, id, 3);
        const int mx = (int)(vx * ts_), my = (int)(vy * ts_ + 0.5f * 900.f * unit * ts_ * ts_);
        for (int yy = ty; yy < ty + ts && yy < rh; ++yy) for (int xx = tx; xx < tx + ts && xx < rw; ++xx) {
            const uint32_t v = tmp[yy * rw + xx]; const unsigned a = v >> 24; if (!a) continue;
            const int X = x0 + xx + mx, Y = y0 + yy + my; if (X < 0 || Y < 0 || X >= w || Y >= h) continue;
            const float heat = 1.f - fade;  // tiles glow orange as they fly
            const unsigned r = (unsigned)(((v >> 16) & 0xFF) * 255 / a), g = (unsigned)((((v >> 8) & 0xFF) * 255 / a) * (1 - heat * 0.6f)), b = (unsigned)(((v & 0xFF) * 255 / a) * (1 - heat * 0.85f));
            SmPxOver(&px[Y * w + X], r, g, b, (unsigned)(a * fade));
        }
    }
    free(tmp);
}
// ink splatter: dark red drops that appear as the brush passes them and fade
static void SmSplatter(uint32_t* px, int w, int h, int x0, int y0, int rw, int rh, float front, float t, float unit) {
    const float al = t < 300.f ? 1.f : 1.f - (t - 300.f) / 400.f; if (al <= 0.f) return;
    for (int i = 0; i < 20; ++i) {
        const float x = x0 - rw * 0.25f + rw * 1.5f * SmRand01(0x1Bu, i, 1), y = y0 - rh * 0.2f + rh * 1.4f * SmRand01(0x1Bu, i, 2);
        if (x > front) continue;
        const float r = (1.5f + 4.f * SmRand01(0x1Bu, i, 3)) * unit;
        for (int yy = (int)(y - r); yy <= (int)(y + r); ++yy) for (int xx = (int)(x - r); xx <= (int)(x + r); ++xx) {
            if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
            const float d = sqrtf((xx - x) * (xx - x) + (yy - y) * (yy - y)); if (d > r) continue;
            SmPxOver(&px[yy * w + xx], 130, 12, 18, (unsigned)(al * (d > r - 1 ? (r - d) : 1.f) * 230));
        }
    }
}
// #127338 "cooler, still minimal and detailed": a fine light glint that sweeps once across the letter's own pixels (lower-left ->
// upper-right), like light running along a polished blade. Alpha untouched, so nothing is drawn outside the letter. s = 0..1.
static void SmGlint(uint32_t* px, int w, int h, int x0, int y0, int rw, int rh, float s) {
    if (s <= 0.f || s >= 1.f || rw <= 0 || rh <= 0) return;
    const float span = (float)rw + (float)rh * 0.6f, band = (float)rw * 0.07f + 1.5f, pos = -band + (span + 2.f * band) * s;
    for (int yy = y0 < 0 ? 0 : y0; yy < y0 + rh && yy < h; ++yy) for (int xx = x0 < 0 ? 0 : x0; xx < x0 + rw && xx < w; ++xx) {
        uint32_t* d = &px[yy * w + xx]; const unsigned a = *d >> 24; if (a < 8) continue;
        const float q = fabsf((float)(xx - x0) + (float)(y0 + rh - yy) * 0.6f - pos); if (q >= band) continue;
        const float f = (1.f - q / band) * (1.f - q / band) * 0.9f;
        unsigned r = (*d >> 16) & 0xFF, g = (*d >> 8) & 0xFF, b = *d & 0xFF;
        r += (unsigned)((a - (r > a ? a : r)) * f); g += (unsigned)((a - (g > a ? a : g)) * f); b += (unsigned)((a - (b > a ? a : b)) * f);
        *d = a << 24 | (r > a ? a : r) << 16 | (g > a ? a : g) << 8 | (b > a ? a : b);
    }
}
// #127343 katana draw-cut: one hairline arc (a slice of a big circle, like a blade's swing) through (px0, py0), rising left -> right.
// s = 0..1 head travel; the tail trails behind and the whole cut fades by 'fade'. Thin, bright, a faint cold edge.
// #127346 ang = tangent angle at the letter centre (negative = rising right); the arc always bows upward like a swing
// #127353 cuts are clipped to the cut letter's box (+ a small margin) and fade out toward its edges, so they never run out of the UI
static int g_sm_cut_clip[4], g_sm_cut_clip_on;
static void SmCutClip(const int* rc, int rank, float m) {
    if (!rc || rank < 0 || rank >= SM_RANKS) { g_sm_cut_clip_on = 0; return; }
    const int* L = sm_atlas_letter[rank]; const int* B = g_sm_letter_ink[rank]; const float k = L[2] > 0 ? (float)(rc[2] - 2) / L[2] : 1.f;
    const float x0 = rc[0] + B[0] * k, y0 = rc[1] + B[1] * k, x1 = rc[0] + (B[2] + 1) * k, y1 = rc[1] + (B[3] + 1) * k, mx = (x1 - x0) * m, my = (y1 - y0) * m;
    g_sm_cut_clip[0] = (int)(x0 - mx); g_sm_cut_clip[1] = (int)(y0 - my); g_sm_cut_clip[2] = (int)(x1 + mx); g_sm_cut_clip[3] = (int)(y1 + my); g_sm_cut_clip_on = 1;
}
static void SmArcCutA(uint32_t* px, int w, int h, float px0, float py0, float lw, float lh, float s, float fadeIn, float unit, float ang) {
    if (fadeIn <= 0.f || s <= 0.f) return;
    const float R = lh * 3.2f, tx = cosf(ang), ty = sinf(ang);
    const float ccx = px0 - ty * R, ccy = py0 + tx * R;           // centre below the cut: the arc bows upward like a swing
    const float phiP = atan2f(py0 - ccy, px0 - ccx), span = (lw * 1.5f) / R;  // #127353 short: the cut stays on the letter
    const float p0 = phiP - span * 0.5f, head = p0 + span * (s > 1.f ? 1.f : s), tail = head - span * 0.55f;
    const float wmax = 1.6f * unit + 0.6f, edge = wmax + 1.5f * unit;
    const int x0 = (int)(px0 - lw * 0.8f), x1 = (int)(px0 + lw * 0.8f), y0 = (int)(py0 - lh * 0.6f), y1 = (int)(py0 + lh * 0.6f);  // #127353 never far outside the letter
    int cx0 = x0, cx1 = x1, cy0 = y0, cy1 = y1; const float ef = 10.f * unit + 1.f;
    if (g_sm_cut_clip_on) { cx0 = g_sm_cut_clip[0]; cy0 = g_sm_cut_clip[1]; cx1 = g_sm_cut_clip[2]; cy1 = g_sm_cut_clip[3]; }
    for (int yy = cy0 < 0 ? 0 : cy0; yy <= cy1 && yy < h; ++yy) for (int xx = cx0 < 0 ? 0 : cx0; xx <= cx1 && xx < w; ++xx) {
        const float dx = xx - ccx, dy = yy - ccy, d = fabsf(sqrtf(dx * dx + dy * dy) - R); if (d >= edge) continue;
        float eb = (float)(xx - cx0); if (cx1 - xx < eb) eb = (float)(cx1 - xx); if (yy - cy0 < eb) eb = (float)(yy - cy0); if (cy1 - yy < eb) eb = (float)(cy1 - yy);
        const float fade = fadeIn * (eb < ef ? eb / ef : 1.f);  // taper to nothing at the clip edge
        const float phi = atan2f(dy, dx); if (phi > head || phi < tail || phi < p0) continue;
        const float q = (phi - tail) / (head - tail);              // 0 tail .. 1 head: blade gets thicker toward the head
        const float wd = wmax * (0.25f + 0.75f * q) * (0.45f + 0.55f * fade);  // lingering cut thins instead of turning grey
        if (d >= wd) { if (d < wd + 1.5f * unit) SmPxOver(&px[yy * w + xx], 190, 215, 255, (unsigned)((1.f - (d - wd) / (1.5f * unit)) * 0.3f * q * fade * 255.f)); continue; }
        SmPxOver(&px[yy * w + xx], 255, 255, 255, (unsigned)((1.f - 0.3f * d / wd) * (0.7f + 0.3f * q) * sqrtf(fade) * 255.f));  // #127353 bright white, not grey
    }
}
static void SmArcCut(uint32_t* px, int w, int h, float px0, float py0, float lw, float lh, float s, float fade, float unit) {
    SmArcCutA(px, w, h, px0, py0, lw, lh, s, fade, unit, -0.379f);  // rising right (the B draw-cut)
}
// one katana swing: travel 0..1, then the cut lingers and fades (s 1..1.6)
static void SmSwing(uint32_t* px, int w, int h, float cx, float cy, float lw, float lh, float s, float unit, float ang) {
    if (s <= 0.f || s >= 1.6f) return;
    SmArcCutA(px, w, h, cx, cy, lw, lh, s, s < 1.f ? 1.f : 0.9f * (1.f - (s - 1.f) / 0.6f), unit, ang);
}
static inline float SmClamp01(float v) { return v < 0.f ? 0.f : v > 1.f ? 1.f : v; }
// #127364 A lightning helpers -------------------------------------------------------------------------------------------
static void SmDot(uint32_t* px, int w, int h, float x, float y, float rad, unsigned r, unsigned g, unsigned b, float al) {
    for (int yy = (int)(y - rad - 1); yy <= (int)(y + rad + 1); ++yy) for (int xx = (int)(x - rad - 1); xx <= (int)(x + rad + 1); ++xx) {
        if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
        const float d = sqrtf((xx - x) * (xx - x) + (yy - y) * (yy - y)); if (d > rad + 1.f) continue;
        SmPxOver(&px[yy * w + xx], r, g, b, (unsigned)(al * (d > rad ? rad + 1.f - d : 1.f) * 255.f));
    }
}
static void SmLineGlow(uint32_t* px, int w, int h, float x0, float y0, float x1, float y1, float th, float al) {
    const float l = sqrtf((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0)); const int n = (int)l + 1;
    for (int k = 0; k <= n; ++k) { const float q = (float)k / n, x = x0 + (x1 - x0) * q, y = y0 + (y1 - y0) * q;
        SmDot(px, w, h, x, y, th * 2.2f, 150, 200, 255, al * 0.12f); SmDot(px, w, h, x, y, th, 245, 250, 255, al); }
}
// thin jagged bolt from (x0, y0) down to (x1, y1) with one short fork; seed changes the shape on every flicker
static void SmBolt(uint32_t* px, int w, int h, float x0, float y0, float x1, float y1, unsigned seed, float unit, float al) {
    const int N = 7; float lx = x0, ly = y0; const float L = y1 - y0;
    for (int i = 1; i <= N; ++i) {
        const float q = (float)i / N, j = i == N ? 0.f : (SmRand01(0xB0u + seed, i, 1) - 0.5f) * 0.28f * L;
        const float nx = x0 + (x1 - x0) * q + j, ny = y0 + L * q;
        SmLineGlow(px, w, h, lx, ly, nx, ny, 0.8f * unit + 0.3f, al);
        if (i == 3) { const float fx = nx + (SmRand01(0xB0u + seed, 9, 2) > 0.5f ? 1.f : -1.f) * 0.18f * L, fy = ny + 0.22f * L;
            SmLineGlow(px, w, h, nx, ny, fx, fy, 0.5f * unit + 0.2f, al * 0.7f); }
        lx = nx; ly = ny;
    }
}
// burn away: pixels closer than a noisy radius to the strike point are gone; a thin ember band glows at the burning edge. s = 0..1
static void SmBurnRect(uint32_t* px, int w, int h, int x0, int y0, int rw, int rh, float bx, float by, float s, float unit) {
    const float maxd = sqrtf((float)rw * rw + (float)rh * rh), front = s * maxd * 1.05f, band = 5.f * unit + 1.f;
    for (int yy = y0 < 0 ? 0 : y0; yy < y0 + rh && yy < h; ++yy) for (int xx = x0 < 0 ? 0 : x0; xx < x0 + rw && xx < w; ++xx) {
        uint32_t* d = &px[yy * w + xx]; const unsigned a = *d >> 24; if (!a) continue;
        const float n = (SmRand01(0xBAu, (unsigned)(xx / 3), (unsigned)(yy / 3)) - 0.5f) * 0.25f * maxd;
        const float dd = sqrtf((xx - bx) * (xx - bx) + (yy - by) * (yy - by)) + n;
        if (dd < front) { *d = 0; continue; }
        if (dd < front + band) { const float f = 1.f - (dd - front) / band;  // ember: orange-white edge
            const unsigned r = a, g = (unsigned)((150.f + 90.f * f) * a / 255.f), b = (unsigned)(60.f * f * a / 255.f);
            *d = a << 24 | r << 16 | g << 8 | b; }
    }
}
// small electric arcs that jump across the letter's ink box, flickering
static void SmCrackle(uint32_t* px, int w, int h, const int* rc, int rank, float t, float unit) {
    const int* L = sm_atlas_letter[rank]; const int* B = g_sm_letter_ink[rank]; const float k = L[2] > 0 ? (float)(rc[2] - 2) / L[2] : 1.f;
    const float ix = rc[0] + B[0] * k, iy = rc[1] + B[1] * k, iw = (B[2] - B[0]) * k, ih = (B[3] - B[1]) * k;
    const unsigned fr = (unsigned)(t / 45.f); const float al = 1.f - SmClamp01((t - 300.f) / 340.f);
    for (int a = 0; a < 2; ++a) { if (SmRand01(0xCAu + fr, a, 0) < 0.35f) continue;
        float x = ix + iw * SmRand01(0xCAu + fr, a, 1), y = iy + ih * SmRand01(0xCAu + fr, a, 2);
        for (int sg = 0; sg < 3; ++sg) { const float nx = x + (SmRand01(0xCAu + fr, a * 7 + sg, 3) - 0.5f) * 0.3f * iw, ny = y + (SmRand01(0xCAu + fr, a * 7 + sg, 4) - 0.5f) * 0.3f * ih;
            if (nx < ix || nx > ix + iw || ny < iy || ny > iy + ih) break;
            SmLineGlow(px, w, h, x, y, nx, ny, 0.45f * unit + 0.2f, al * 0.85f); x = nx; y = ny; }
    }
}
typedef struct { HDC mem; uint32_t* px; int w, h; float mid, rowBottom, kl, unit; } SmLetterCtx;
static void SmLetterCentre(const SmLetterCtx* x, int r, float* cx, float* cy) {
    const int* L = sm_atlas_letter[r]; *cx = x->mid + g_sm_letter_dx[r] * x->kl; *cy = x->rowBottom - L[3] * x->kl / 2.f;
}
// draw letter r at its rest place + (dx, dy), scale s, alpha; returns its rect
static void SmLetterAt(const SmLetterCtx* x, int r, float dx, float dy, float s, float alpha, int* rc) {
    const int* L = sm_atlas_letter[r]; float cx, cy; SmLetterCentre(x, r, &cx, &cy);
    const float k = x->kl * s; const int lx = (int)(cx + dx - L[2] * k / 2.f + 0.5f), ly = (int)(cy + dy - L[3] * k / 2.f + 0.5f);
    if (alpha > 0.f) SmBlitA(x->mem, lx, ly, L, L[2], k, (int)(255.f * (alpha > 1.f ? 1.f : alpha)));
    if (rc) { rc[0] = lx; rc[1] = ly; rc[2] = (int)(L[2] * k) + 2; rc[3] = (int)(L[3] * k) + 2; }
}
// dt = ms since the shown rank stepped up from prev to r. Returns 0 when no transition is running (caller draws the letter at rest).
// #127338 impact time per rank (ms into its motion): a short crisp pause there, then a glint runs across the new letter
static const float sm_up_hit_t[SM_RANKS] = {150, 150, 320, 300, 160, 210, 350};
// #127364 no shared effects: only the C clang and the B draw-cut freeze on the hit
static const float sm_up_hold_ms[SM_RANKS] = {0, SM_UP_HOLD, SM_UP_HOLD, 0, 0, 0, 0};
static int SmRankUpDraw(const SmLetterCtx* x, int prev, int r, float dt0) {
    if (dt0 < 0.f || dt0 >= SM_UP_MS || prev < 0 || prev >= SM_RANKS || prev == r || r < 1 || r >= SM_RANKS) return 0;
    uint32_t* px = x->px; const int w = x->w, h = x->h; const float u = x->unit; int rc[4] = {0, 0, 0, 0}, ro[4];
    const float T = sm_up_hit_t[r], HO = sm_up_hold_ms[r], dt = dt0 < T ? dt0 : dt0 < T + HO ? T : dt0 - HO;
    float cx, cy; SmLetterCentre(x, r, &cx, &cy); const float S = x->kl * sm_atlas_letter[r][3];  // letter height in canvas px
    switch (r) {
    case 1: {  // C: parry clang - the new letter strikes in from the right, knocks the old one away, sparks at the contact point
        const float hit = 150.f, e = dt - hit;
        const float nd = dt < hit ? (1.f - dt / hit) * (1.f - dt / hit) * 0.9f * S : (e < 120.f ? 0.07f * S * sinf(e / 120.f * 3.14159f) : 0.f);
        const float push = -0.8f * S * SmClamp01(1.f - nd / (0.8f * S));  // #127338 the C shoves the D aside as it lands (no overlap)
        if (dt < hit) SmLetterAt(x, prev, push, 0, 1, 1, 0);
        else SmLetterAt(x, prev, push - e * 0.9f * u, e * e * 0.0009f * u, 1, 1.f - e / 260.f, 0);
        SmLetterAt(x, r, nd, 0, 1, 1, rc); GdiFlush();
        if (e >= 0.f) { SmTintRect(px, w, h, rc[0], rc[1], rc[2], rc[3], 0xFFFFFFu, 0.75f * (1.f - e / 180.f));
            SmSparks(px, w, h, (float)rc[0] + 4.f * u, cy, e, 36, 0xC1u, u, 0); }
        break; }
    case 2: {  // #127343 B: katana draw-cut - one fast curved blade arc through the old letter, a beat of stillness with the cut line
               //      glowing on it, then the halves slip apart along the cut and fall away; the B settles in behind with the glint
        const float swing = 80.f, still = 260.f;
        SmLetterAt(x, prev, 0, 0, 1, dt < still ? 1.f : 0.f, ro); GdiFlush();
        float pcx, pcy; SmLetterCentre(x, prev, &pcx, &pcy);
        if (dt >= still) { SmLetterAt(x, prev, 0, 0, 1, 1, ro); GdiFlush();
            const float e = dt - still; SmSplitRect(px, w, h, ro[0], ro[1], ro[2], ro[3], pcx, pcy, 0.185f, e * e * 0.0025f * u + e * 0.12f * u, 1.f - e / 200.f); }
        const float na = SmClamp01((dt - still - 50.f) / 120.f);
        SmLetterAt(x, r, 0, 0, 1, na, rc); GdiFlush();
        SmCutClip(ro, prev, 0.1f);
        SmArcCut(px, w, h, pcx, pcy, (float)ro[2], (float)ro[3], dt / swing, dt < swing ? 1.f : dt < still ? 0.9f : 0.9f * (1.f - (dt - still) / 120.f), u);
        SmCutClip(0, 0, 0);
        if (dt >= swing && dt < swing + 140.f) SmSparks(px, w, h, pcx + ro[2] * 0.6f, pcy - ro[3] * 0.22f, dt - swing, 8, 0xB2u, u, 0);
        break; }
    case 3: {  // #127364 A: lightning - a thin forked bolt strikes the B from just above, the B burns away from the strike point
               //      with an ember edge, and the A crackles in with small electric arcs over its ink
        const float strike = 60.f, burn = 260.f;
        float pcx, pcy; SmLetterCentre(x, prev, &pcx, &pcy);
        const float bx = pcx + 0.05f * S, by = pcy - 0.15f * S;  // strike point on the B
        if (dt < strike + burn) { SmLetterAt(x, prev, 0, 0, 1, 1, ro); GdiFlush();
            if (dt >= strike) SmBurnRect(px, w, h, ro[0], ro[1], ro[2], ro[3], bx, by, (dt - strike) / burn, u); }
        const float na = SmClamp01((dt - strike - 120.f) / 110.f);
        if (na > 0.f) { SmLetterAt(x, r, 0, 0, 1, na, rc); GdiFlush();
            SmTintRect(px, w, h, rc[0], rc[1], rc[2], rc[3], 0xB4DCFFu, 0.6f * (1.f - SmClamp01((dt - strike - 120.f) / 300.f)));
            if (dt < 640.f) SmCrackle(px, w, h, rc, r, dt, u); }
        const int on = dt < 50.f || (dt >= 75.f && dt < 120.f) || (dt >= 150.f && dt < 175.f);  // bolt flickers three times
        if (on) SmBolt(px, w, h, bx - 0.08f * S, pcy - 0.68f * S, bx, by, (unsigned)(dt < 75.f ? 1 : dt < 150.f ? 2 : 3), u, dt < 50.f ? 1.f : 0.7f);
        break; }
    case 4: {  // S: ki burst - the old letter is drawn into a shrinking ring of ki, then the S bursts out blue-white with a shockwave
        const float gather = 160.f, e = dt - gather;
        if (dt < gather) { const float q = dt / gather;
            SmLetterAt(x, prev, 0, 0, 1.f - 0.8f * q, 1.f - q, 0); GdiFlush();
            SmRing(px, w, h, cx, cy, (1.f - q) * 220.f, S * 0.7f, u);
            break; }
        const float sc = e < 110.f ? 0.3f + 0.88f * (e / 110.f) : e < 260.f ? 1.18f - 0.18f * ((e - 110.f) / 150.f) : 1.f;
        SmLetterAt(x, r, 0, 0, sc, 1, rc); GdiFlush();
        SmTintRect(px, w, h, rc[0], rc[1], rc[2], rc[3], 0x96CDFFu, 0.85f * (1.f - e / 420.f));
        SmRing(px, w, h, cx, cy, e, S * 0.62f, u);
        break; }
    case 5: {  // #127364 SS: afterimage dash - the S whips out to the left trailing red afterimages, the SS whips in from the right
               //      with its own trail and snaps to rest; the trails collapse into it
        const float out = 110.f, in = 170.f, D = 0.42f * S;
        if (dt < out) { const float q = dt / out;
            for (int k = 4; k >= 1; --k) { const float qk = SmClamp01((dt - 14.f * k) / out); SmLetterAt(x, prev, -D * qk * qk, 0, 1, (1.f - q) * 0.22f * (5 - k) / 4.f, 0); }
            GdiFlush(); SmLetterAt(x, prev, -D * q * q, 0, 1, 1.f - q, 0); }
        const float e = dt - out * 0.6f; if (e < 0.f) break;
        int nt = 0;
        for (int k = 4; k >= 1; --k) { const float ek = SmClamp01((e - 18.f * k) / in), ok = (1.f - ek) * (1.f - ek) * (1.f - ek) * D;
            const float al = (1.f - SmClamp01((e - in) / 120.f)) * 0.3f * (5 - k) / 4.f; if (al > 0.f) { SmLetterAt(x, r, ok, 0, 1, al, 0); ++nt; } }
        GdiFlush();
        if (nt) { int tr[4]; SmLetterAt(x, r, 0, 0, 1, 0, tr); SmTintRect(px, w, h, tr[0], tr[1], tr[2] + (int)D + 2, tr[3], g_sm_rank_rgb[5], 0.85f); }
        const float ei = SmClamp01(e / in), off = (1.f - ei) * (1.f - ei) * (1.f - ei) * D;
        SmLetterAt(x, r, off, 0, 1, SmClamp01(e / 60.f), rc);
        break; }
    default: {  // SSS: demon shatter - the old letters burst into glowing shards, the SSS rises red-hot out of the embers,
                //      triple cut + shockwave, cooling to cream
        if (dt < 420.f) { SmLetterAt(x, prev, 0, 0, 1, 1, ro); GdiFlush(); SmShatterRect(px, w, h, ro[0], ro[1], ro[2], ro[3], dt, u); }
        const float e = dt - 150.f; if (e < 0.f) break;
        const float rise = 1.f - SmClamp01(e / 240.f);
        SmLetterAt(x, r, 0, rise * rise * 0.8f * S, 1, SmClamp01(e / 90.f), rc); GdiFlush();
        SmTintRect(px, w, h, rc[0], rc[1], rc[2], rc[3], 0xFF4A1Eu, 0.9f * SmClamp01(1.f - (e - 120.f) / 480.f));
        if (0) { const float f = e - 200.f; static const float tl[3] = {-0.38f, 0.38f, -0.05f};  // #127364 swings / ring removed (unique per rank)
            SmCutClip(rc, r, 0.1f);
            for (int k = 0; k < 3; ++k) SmSwing(px, w, h, cx, cy, (float)rc[2], (float)rc[3], (f - 70.f * k) / 80.f, u, tl[k]);
            SmCutClip(0, 0, 0); }
        SmSparks(px, w, h, cx, cy + S * 0.35f, e, 30, 0xE3BE25u, u, 1);
        break; }
    }
    // #127364 no shared hit flash / glint: each rank's motion above is its whole effect
    return 1;
}
// Draws into mem / px (w x h, cleared here). Sprites when the atlas decoded, else the plain text fallback (no motion on the fallback letter).
static void SmCompose(HDC mem, uint32_t* px, const SmCanvas* c, const SmDrawSt* st) {
    const int w = c->w, h = c->h; const SmGeo* g = &c->g;
    memset(px, 0, (size_t)w * h * 4);
    int rank = st->rank; if (rank < 0) rank = 0; if (rank >= SM_RANKS) rank = SM_RANKS - 1;
    const float unit = (float)c->pf / 36.f;  // 1080p pixel -> canvas pixel (pf is 36 at 1080p, scale 100)
    const SmUpFx up = SmRankUp(st->upMs);
    const SmUpStyle* us = &sm_up_style[st->rank < 0 ? 0 : st->rank > 6 ? 6 : st->rank];  // #127304 per-rank rank-up
    const int sx = (int)((st->dx + up.shx * us->shake) * unit), sy = (int)((st->dy + up.shy * us->shake) * unit);
    const int right = c->mx + g->w - g->pad + sx; int y = c->my + g->pad + sy;
    int gaugeBottom = y, barX = 0, barY = 0, barW = 0, barH = 0;
    // #127144 one centre line for letter, word, gauge and pop-ups: the middle of the box (as wide as the widest letter / word / gauge, SmGeom)
    const float mid = (float)right - (float)(g->w - 2 * g->pad) / 2.f;
    if (g_sm_atlas_dc) {
        const int* L = sm_atlas_letter[rank]; const int* W = sm_atlas_word[rank];
        int mlw, mlh, mww, mwh; SmAtlasMax(&mlw, &mlh, &mww, &mwh);
        // #127152 fixed rows: letter row = tallest letter, word row = tallest word; each letter sits on the row's bottom line
        const float kl = g->k * SM_LK, rowL = mlh * kl, cx = mid + g_sm_letter_dx[rank] * kl * st->scale, cy = y + rowL - L[3] * kl / 2.f;
        // #127149 no glow halo behind the letter (human did not like it)
        // #127315 rank-up: each rank has its own motion (SmRankUpDraw); otherwise the letter at rest
        const SmLetterCtx lc = {mem, px, w, h, mid, y + rowL, kl, unit};
        if (!SmRankUpDraw(&lc, st->prevRank, rank, (float)st->upMs)) {
            const float ks = kl * st->scale;
            SmBlitA(mem, (int)(cx - L[2] * ks / 2.f + 0.5f), (int)(cy - L[3] * ks / 2.f + 0.5f), L, L[2], ks, 255);
        }
        if (SM_SHOW_WORD) {
            y += (int)rowL + g->pad;
            SmBlitA(mem, (int)(mid - W[2] * g->k / 2.f + 0.5f), y + (int)((mwh - W[3]) * g->k / 2.f), W, W[2], g->k, 255); y += (int)(mwh * g->k) + g->pad;
        } else {
            (void)W; (void)mwh;
            y += (int)rowL + g->pad / 3;  // #127242 no word: the letter sits just above the bar
        }
        barX = (int)(mid - sm_atlas_gauge_bg[2] * SM_BAR_WK * g->k / 2.f + 0.5f); barY = y;
        barW = (int)(sm_atlas_gauge_bg[2] * SM_BAR_WK * g->k + 0.5f); barH = (int)(sm_atlas_gauge_bg[3] * g->k * SM_BAR_HK + 0.5f); if (barH < 4) barH = 4;
        gaugeBottom = y + barH + g->pad;
    } else {  // no sprites (WIC failed): plain text, so the meter still shows something
        HFONT fn = CreateFontW(-(int)(200 * g->k), 0, 0, 0, FW_BOLD, TRUE, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Georgia");
        HGDIOBJ of = SelectObject(mem, fn); SetBkMode(mem, TRANSPARENT); SetTextColor(mem, RGB(230, 60, 40));
        RECT tr = {0, y, right, c->my + g->h}; DrawTextA(mem, sm_rank_name[rank], -1, &tr, DT_RIGHT | DT_TOP | DT_SINGLELINE);
        SelectObject(mem, of); DeleteObject(fn); GdiFlush();
        for (int i = 0; i < w * h; ++i) if (px[i] & 0xFFFFFF) px[i] |= 0xFF000000u;  // GDI text leaves alpha 0: opaque where drawn
        const int gy = c->my + g->h - g->pad - (int)(6 * g->k) - 1, gw = (int)(440 * g->k), gx = right - gw, fw = (int)(gw * st->gauge / 1000.f);
        for (int yy = gy; yy < gy + (int)(5 * g->k) + 1 && yy < h; ++yy) for (int xx = gx; xx < right; ++xx) if (xx >= 0 && yy >= 0) px[yy * w + xx] = xx < gx + fw ? 0xF5CDB98Bu : 0x50505050u;
        gaugeBottom = c->my + g->h;
    }
    GdiFlush();
    if (barW > 0) SmBar(px, w, h, barX, barY, barW, barH, st->gauge, SM_BAR_RANK_RGB ? g_sm_rank_rgb[rank] : SM_LETTER_RGB, st->now);
    // #127364 no shared bar flash on rank-up
    if (st->npops > 0 && c->popH > 0) {  // newest on top, older lines under it
        const int TW = w, TH = (int)(c->pp * 1.25f);
        BITMAPINFO bi; memset(&bi, 0, sizeof bi);
        bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = TW; bi.bmiHeader.biHeight = -TH;
        bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
        uint32_t* tpx = 0; HDC tdc = CreateCompatibleDC(mem);
        HBITMAP tb = tdc ? CreateDIBSection(tdc, &bi, DIB_RGB_COLORS, (void**)&tpx, 0, 0) : 0;
        HFONT fn = CreateFontW(-c->pp, 0, 0, 0, FW_HEAVY, TRUE, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Georgia");
        if (tb && tpx && fn) {
            HGDIOBJ ob = SelectObject(tdc, tb), of = SelectObject(tdc, fn);
            SetBkMode(tdc, TRANSPARENT); SetTextColor(tdc, RGB(255, 255, 255));
            int line = 0;
            for (int i = st->npops - 1; i >= 0; --i) {
                float in = 1.f; const float a = SmPopAlpha(&st->pops[i], st->now, st->lifeMs, &in);
                if (a <= 0.f) continue;
                char t[48], tb2[24]; const SmPop* pp = &st->pops[i];  // #127171 value first, then the bonus if it has one
                snprintf(t, sizeof t, "%s +%.0f", pp->label, pp->pts); tb2[0] = 0;
                if (pp->bonus > 0.5f) snprintf(tb2, sizeof tb2, " +%.0f", pp->bonus);
                SIZE s1 = {0, 0}, s2 = {0, 0}; GetTextExtentPoint32A(tdc, t, (int)strlen(t), &s1);
                if (tb2[0]) GetTextExtentPoint32A(tdc, tb2, (int)strlen(tb2), &s2);
                const int x0 = (TW - (int)(s1.cx + s2.cx)) / 2;  // whole line stays centred
                const int ox = (int)(mid - TW / 2.f) + (int)((1.f - in) * c->pp * 0.8f), oy = gaugeBottom + line * TH;
                for (int part = 0; part < 2; ++part) {  // #127179 bonus in its own colour
                    const char* str = part ? tb2 : t; if (!str[0]) continue;
                    memset(tpx, 0, (size_t)TW * TH * 4);
                    RECT r = {x0 + (part ? (int)s1.cx : 0), 0, TW, TH}; DrawTextA(tdc, str, -1, &r, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX); GdiFlush();
                    SmMaskOver(px, w, h, tpx, TW, TH, ox + 2, oy + 2, 0x000000, a * 0.75f);   // shadow
                    SmMaskOver(px, w, h, tpx, TW, TH, ox, oy, part ? 0xE6EAF0u : 0xFFD27Au, a);  // value warm gold, bonus white-silver (#127191)
                }
                ++line;
            }
            SelectObject(tdc, of); SelectObject(tdc, ob);
        }
        if (fn) DeleteObject(fn);
        if (tb) DeleteObject(tb);
        if (tdc) DeleteDC(tdc);
    }
}
#endif
