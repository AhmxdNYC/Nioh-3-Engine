// #127085 style meter drawing, split out of style_meter_game.h so tests/style_meter_demo.c renders the real frames offline.
// Needs before it: windows.h, style_meter.h, style_meter_fx.h, style_atlas_gen.h, wincodec.h (COBJMACROS), g_sm_png / g_sm_png_end,
// int g_cfg_sm_scale, Log(fmt, ...).
#ifndef STYLE_METER_DRAW_H
#define STYLE_METER_DRAW_H
static const GUID SM_CLSID_WICImagingFactory = {0xcacaf262, 0x9370, 0x4615, {0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0x0a}};
static const GUID SM_IID_IWICImagingFactory = {0xec5ec8a9, 0xc395, 0x4314, {0x9c, 0x77, 0x54, 0xd7, 0xa9, 0x35, 0xff, 0x70}};
static const GUID SM_GUID_PBGRA = {0x6fddc324, 0x4e03, 0x4bfe, {0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x10}};
static HDC g_sm_atlas_dc; static HBITMAP g_sm_atlas_bmp;
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
            if (SUCCEEDED(hr)) { SelectObject(g_sm_atlas_dc, g_sm_atlas_bmp); GdiFlush(); ok = 1; }
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
static SmGeo SmGeom(int H) {
    SmGeo g; int lw = 0, lh = 0, ww = 0, wh = 0;
    for (int i = 0; i < SM_RANKS; ++i) { if (sm_atlas_letter[i][2] > lw) lw = sm_atlas_letter[i][2]; if (sm_atlas_letter[i][3] > lh) lh = sm_atlas_letter[i][3];
        if (sm_atlas_word[i][2] > ww) ww = sm_atlas_word[i][2]; if (sm_atlas_word[i][3] > wh) wh = sm_atlas_word[i][3]; }
    g.k = (float)H / 1080.f * (float)g_cfg_sm_scale / 100.f * (float)SM_LETTER_H_1080P / (float)lh;
    g.pad = (int)(8 * g.k) + 2;
    const int gw = sm_atlas_gauge_bg[2] > ww ? sm_atlas_gauge_bg[2] : ww;
    g.w = (int)((lw > gw ? lw : gw) * g.k) + 2 * g.pad;
    g.h = (int)((lh + wh + sm_atlas_gauge_bg[3]) * g.k) + 4 * g.pad;
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
} SmDrawSt;
typedef struct { SmGeo g; int w, h, mx, my, pf, popH; } SmCanvas;
static SmCanvas SmCanvasGeom(int H, int pops) {
    SmCanvas c; c.g = SmGeom(H);
    int lw = 0, lh = 0;
    for (int i = 0; i < SM_RANKS; ++i) { if (sm_atlas_letter[i][2] > lw) lw = sm_atlas_letter[i][2]; if (sm_atlas_letter[i][3] > lh) lh = sm_atlas_letter[i][3]; }
    const float letterPx = lh * c.g.k;
    c.pf = (int)(letterPx * 0.12f + 0.5f); if (c.pf < 10) c.pf = 10;
    c.popH = pops ? (int)(c.pf * 1.25f) * SM_POPS + c.g.pad : 0;
    c.mx = (int)(lw * c.g.k * 0.10f) + (int)(10.f * (float)H / 1080.f) + 2;
    c.my = (int)(lh * c.g.k * 0.10f) + (int)(10.f * (float)H / 1080.f) + 2;
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
// Draws into mem / px (w x h, cleared here). Sprites when the atlas decoded, else the plain text fallback (no motion on the fallback letter).
static void SmCompose(HDC mem, uint32_t* px, const SmCanvas* c, const SmDrawSt* st) {
    const int w = c->w, h = c->h; const SmGeo* g = &c->g;
    memset(px, 0, (size_t)w * h * 4);
    int rank = st->rank; if (rank < 0) rank = 0; if (rank >= SM_RANKS) rank = SM_RANKS - 1;
    const float unit = (float)c->pf / 36.f;  // 1080p pixel -> canvas pixel (pf is 36 at 1080p, scale 100)
    const int sx = (int)(st->dx * unit), sy = (int)(st->dy * unit);
    const int right = c->mx + g->w - g->pad + sx; int y = c->my + g->pad + sy;
    int gaugeBottom = y;
    if (g_sm_atlas_dc) {
        const int* L = sm_atlas_letter[rank]; const int* W = sm_atlas_word[rank];
        const float lw0 = L[2] * g->k, lh0 = L[3] * g->k, cx = right - lw0 / 2.f, cy = y + lh0 / 2.f;
        if (st->glow > 0.f) {  // halo: a slightly larger, faint copy of the letter behind it
            const float kg = g->k * st->scale * 1.08f;
            SmBlitA(mem, (int)(cx - L[2] * kg / 2.f), (int)(cy - L[3] * kg / 2.f), L, L[2], kg, (int)(st->glow * 120.f));
        }
        const float ks = g->k * st->scale;
        SmBlitA(mem, (int)(cx - L[2] * ks / 2.f + 0.5f), (int)(cy - L[3] * ks / 2.f + 0.5f), L, L[2], ks, 255);
        y += (int)lh0 + g->pad;
        SmBlitA(mem, right - (int)(W[2] * g->k + 0.5f), y, W, W[2], g->k, 255); y += (int)(W[3] * g->k) + g->pad;
        const int gx = right - (int)(sm_atlas_gauge_bg[2] * g->k + 0.5f);
        SmBlitA(mem, gx, y, sm_atlas_gauge_bg, sm_atlas_gauge_bg[2], g->k, 255);
        const int fw = (int)((float)sm_atlas_gauge_fill[2] * st->gauge / 1000.f + 0.5f);
        if (fw > 0) SmBlitA(mem, gx, y, sm_atlas_gauge_fill, fw, g->k, 255);
        gaugeBottom = y + (int)(sm_atlas_gauge_bg[3] * g->k) + g->pad;
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
    if (st->npops > 0 && c->popH > 0) {  // newest on top, older lines under it
        const int TW = w, TH = (int)(c->pf * 1.25f);
        BITMAPINFO bi; memset(&bi, 0, sizeof bi);
        bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = TW; bi.bmiHeader.biHeight = -TH;
        bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
        uint32_t* tpx = 0; HDC tdc = CreateCompatibleDC(mem);
        HBITMAP tb = tdc ? CreateDIBSection(tdc, &bi, DIB_RGB_COLORS, (void**)&tpx, 0, 0) : 0;
        HFONT fn = CreateFontW(-c->pf, 0, 0, 0, FW_HEAVY, TRUE, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Georgia");
        if (tb && tpx && fn) {
            HGDIOBJ ob = SelectObject(tdc, tb), of = SelectObject(tdc, fn);
            SetBkMode(tdc, TRANSPARENT); SetTextColor(tdc, RGB(255, 255, 255));
            int line = 0;
            for (int i = st->npops - 1; i >= 0; --i) {
                float in = 1.f; const float a = SmPopAlpha(&st->pops[i], st->now, st->lifeMs, &in);
                if (a <= 0.f) continue;
                char t[48]; snprintf(t, sizeof t, "%s +%.0f", st->pops[i].label, st->pops[i].pts);
                memset(tpx, 0, (size_t)TW * TH * 4);
                RECT r = {0, 0, TW - c->g.pad, TH}; DrawTextA(tdc, t, -1, &r, DT_RIGHT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX); GdiFlush();
                const int ox = sx - c->mx + (int)((1.f - in) * c->pf * 0.8f), oy = gaugeBottom + line * TH;
                SmMaskOver(px, w, h, tpx, TW, TH, ox + 2, oy + 2, 0x000000, a * 0.75f);   // shadow
                SmMaskOver(px, w, h, tpx, TW, TH, ox, oy, 0xFFD27A, a);                   // warm gold
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
