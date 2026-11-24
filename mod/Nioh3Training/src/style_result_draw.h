// Style meter fight result card: drawn by the style meter's draw thread in its own click-through layered window, centred near the top of the
// game window when a boss fight ends (style_result.h). Also used by tests/style_result_demo.c to render the card offline.
// Needs before it: style_meter_draw.h (atlas, SmMaskOver, SmLetterCtx, SmRankUpDraw), style_result.h.
#ifndef STYLE_RESULT_DRAW_H
#define STYLE_RESULT_DRAW_H

#define SR_CARD_W 640           // 1080p design size (scaled by the client height and StyleMeterScale)
#define SR_CARD_H 664
#define SR_FADE_IN_MS 250
#define SR_FADE_OUT_MS 350
#define SR_ARM_MS 800           // a confirm press counts only after the card has been up this long (button mashing at the kill)
#define SR_GOLD 0xC9B37Eu
#define SR_LABEL 0xB9BEC6u
#define SR_VALUE 0xF2E6C8u
// Time-at-rank bar colours, D .. SSS: dull to hot, all distinct (the meter's own bar colours repeat at the top ranks).
static const uint32_t sr_rank_rgb[SM_RANKS] = {0x7E7A73u, 0xB0703Cu, 0x9CA5B0u, 0xE0AE2Eu, 0xB8D4F0u, 0xE6C8EAu, 0xE8559Cu};  // the meter's rank colours, a little darker for the bar

typedef struct { int w, h; float k; } SrGeo;
static SrGeo SrGeom(int clientH, int scalePct) {
    SrGeo g; g.k = (float)clientH / 1080.f * (float)scalePct / 100.f;
    if (g.k < 0.3f) g.k = 0.3f;
    g.w = (int)(SR_CARD_W * g.k + 0.5f); g.h = (int)(SR_CARD_H * g.k + 0.5f); return g;
}
// premultiplied 'over' of a solid colour on a rect
static void SrFill(uint32_t* px, int W, int H, int x0, int y0, int rw, int rh, uint32_t rgb, unsigned a) {
    for (int y = y0 < 0 ? 0 : y0; y < y0 + rh && y < H; ++y) for (int x = x0 < 0 ? 0 : x0; x < x0 + rw && x < W; ++x)
        SmPxOver(&px[y * W + x], (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF, a);
}
// One line of text (GDI coverage mask -> premultiplied over, with a soft shadow). align 0 left of x, 1 centred on x, 2 right of x.
static void SrText(HDC ref, uint32_t* px, int W, int H, const char* s, int x, int y, int fpx, int weight, int italic, int align, uint32_t col) {
    if (!s || !s[0] || fpx < 6) return;
    const int TH = (int)(fpx * 1.35f) + 2, TW = W;
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = TW; bi.bmiHeader.biHeight = -TH;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    uint32_t* tpx = 0; HDC tdc = CreateCompatibleDC(ref);
    HBITMAP tb = tdc ? CreateDIBSection(tdc, &bi, DIB_RGB_COLORS, (void**)&tpx, 0, 0) : 0;
    HFONT fn = CreateFontW(-fpx, 0, 0, 0, weight, italic, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Georgia");
    if (tb && tpx && fn) {
        HGDIOBJ ob = SelectObject(tdc, tb), of = SelectObject(tdc, fn);
        SetBkMode(tdc, TRANSPARENT); SetTextColor(tdc, RGB(255, 255, 255));
        SIZE sz = {0, 0}; GetTextExtentPoint32A(tdc, s, (int)strlen(s), &sz);
        const int ox = align == 1 ? x - (int)sz.cx / 2 : align == 2 ? x - (int)sz.cx : x;
        memset(tpx, 0, (size_t)TW * TH * 4);
        RECT r = {0, 0, TW, TH}; DrawTextA(tdc, s, -1, &r, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX); GdiFlush();
        const int sh = fpx / 14 + 1;
        SmMaskOver(px, W, H, tpx, TW, TH, ox + sh, y + sh, 0x000000, 0.8f);
        SmMaskOver(px, W, H, tpx, TW, TH, ox, y, col, 1.f);
        SelectObject(tdc, of); SelectObject(tdc, ob);
    }
    if (fn) DeleteObject(fn);
    if (tb) DeleteObject(tb);
    if (tdc) DeleteDC(tdc);
}
static void SrClock(char* o, int n, double ms) { const int s = (int)(ms / 1000.0 + 0.5); snprintf(o, n, "%d:%02d", s / 60, s % 60); }
// The static part of the card (panel, texts, bar): everything but the grade letter.
static void SrComposeBase(HDC ref, uint32_t* px, const SrGeo* g, const SrResult* r) {
    const int W = g->w, H = g->h; const float k = g->k;
#define SRX(v) ((int)((v) * k + 0.5f))
    memset(px, 0, (size_t)W * H * 4);
    // panel: dark see-through body, darker top band, gold rim
    SrFill(px, W, H, 0, 0, W, H, 0x0E0C0Bu, 222);
    SrFill(px, W, H, 0, 0, W, SRX(58), 0x000000u, 70);
    SrFill(px, W, H, 0, 0, W, SRX(2) > 1 ? SRX(2) : 1, 0xA89A6Cu, 255); SrFill(px, W, H, 0, H - (SRX(2) > 1 ? SRX(2) : 1), W, SRX(2) > 1 ? SRX(2) : 1, 0xA89A6Cu, 255);
    SrFill(px, W, H, 0, 0, SRX(2) > 1 ? SRX(2) : 1, H, 0xA89A6Cu, 255); SrFill(px, W, H, W - (SRX(2) > 1 ? SRX(2) : 1), 0, SRX(2) > 1 ? SRX(2) : 1, H, 0xA89A6Cu, 255);
    SrFill(px, W, H, SRX(8), SRX(8), W - SRX(16), 1, 0xA89A6Cu, 90); SrFill(px, W, H, SRX(8), H - SRX(8), W - SRX(16), 1, 0xA89A6Cu, 90);
    SrText(ref, px, W, H, r->bosses > 1 ? "BOSS FIGHT  STYLE RANK" : "BOSS STYLE RANK", W / 2, SRX(17), SRX(23), FW_BOLD, 0, 1, SR_GOLD);
    SrFill(px, W, H, SRX(40), SRX(58), W - SRX(80), 1, 0xA89A6Cu, 140);
    // score line under the letter
    char b[160], t[24]; SrClock(t, sizeof t, r->ms);
    snprintf(b, sizeof b, "Average %.2f / 7     Peak %s     Fight %s", r->avg, sm_rank_name[r->peak], t);
    SrText(ref, px, W, H, b, W / 2, SRX(266), SRX(21), FW_NORMAL, 0, 1, SR_VALUE);
    if (r->n.damaged == 0) snprintf(b, sizeof b, "No hits taken: no penalty");
    else snprintf(b, sizeof b, "Style %.2f  \x96 %.2f for %u hit%s taken", r->rawAvg, r->penalty, r->n.damaged, r->n.damaged == 1 ? "" : "s");
    SrText(ref, px, W, H, b, W / 2, SRX(296), SRX(17), FW_BOLD, 0, 1, r->n.damaged == 0 ? 0x9FD49Au : 0xE08A7Au);
    if (r->grade < r->byAvg)
        snprintf(b, sizeof b, "Average reached %s, but under %d%% of the fight was spent at %s or higher", sm_rank_name[r->byAvg], r->hold, sm_rank_name[r->byAvg]);
    else if (r->grade < SM_RANKS - 1)
        snprintf(b, sizeof b, "Next: average %d.00 with %d%% of the fight at %s or higher", r->grade + 1, r->hold, sm_rank_name[r->grade + 1]);
    else snprintf(b, sizeof b, "Heavenly");
    SrText(ref, px, W, H, b, W / 2, SRX(320), SRX(16), FW_NORMAL, 1, 1, r->grade < r->byAvg ? 0xE6A050u : SR_LABEL);
    // time at each rank: one bar split by share, then a legend row
    const int bx = SRX(40), bw = W - 2 * bx, by = SRX(352), bh = SRX(16) > 4 ? SRX(16) : 4;
    SrFill(px, W, H, bx - 1, by - 1, bw + 2, bh + 2, 0x000000u, 200);
    { float acc = 0.f; int x0 = bx;
      for (int i = 0; i < SM_RANKS; ++i) { acc += r->pctAt[i]; const int x1 = bx + (int)(bw * (acc > 100.f ? 100.f : acc) / 100.f + 0.5f);
          if (x1 > x0) { SrFill(px, W, H, x0, by, x1 - x0, bh, sr_rank_rgb[i], 255); SrFill(px, W, H, x0, by, x1 - x0, bh / 3, 0xFFFFFFu, 40); }
          x0 = x1; } }
    for (int i = 0; i < SM_RANKS; ++i) {
        const int cx = bx + bw * (2 * i + 1) / (2 * SM_RANKS);
        snprintf(b, sizeof b, "%s %.0f%%", sm_rank_name[i], r->pctAt[i]);
        SrText(ref, px, W, H, b, cx, by + bh + SRX(6), SRX(15), r->pctAt[i] >= 0.5f ? FW_BOLD : FW_NORMAL, 0, 1, r->pctAt[i] >= 0.5f ? g_sm_rank_rgb[i] : 0x6A6E74u);
    }
    SrFill(px, W, H, SRX(40), SRX(408), W - SRX(80), 1, 0xA89A6Cu, 140);
    // stats: two columns, label left / value right
    char v[14][40]; const char* lab[14] = {"Different moves", "Martial arts", "Ninjutsu", "Soul cores", "Guardian spirit", "Weapon switches", "Style switches",
                                           "Hits landed", "Counters", "Dodges", "Deflects", "Ki pulses", "Hits taken", "Best combo"};
    snprintf(v[0], 40, "%d", r->moves);
    if (r->n.maHits) snprintf(v[1], 40, "%u (%d different)", r->n.maHits, r->maMoves); else snprintf(v[1], 40, "0");
    snprintf(v[2], 40, "%u", r->n.nins); snprintf(v[3], 40, "%u", r->n.souls); snprintf(v[4], 40, "%u", r->n.gsHits); snprintf(v[5], 40, "%u", r->n.swapHits);
    snprintf(v[6], 40, "%u", r->n.styles);
    snprintf(v[7], 40, "%u", r->n.hits); snprintf(v[8], 40, "%u", r->n.counters + r->n.counterAvoids); snprintf(v[9], 40, "%u", r->n.dodges); snprintf(v[10], 40, "%u", r->n.deflects);
    if (r->n.kiPerfects) snprintf(v[11], 40, "%u (%u perfect)", r->n.kis, r->n.kiPerfects); else snprintf(v[11], 40, "%u", r->n.kis);
    snprintf(v[12], 40, "%u", r->n.damaged); snprintf(v[13], 40, "%u hits", r->comboBest);
    for (int i = 0; i < 14; ++i) {
        if (!lab[i][0]) continue;
        const int col = i / 7, row = i % 7, y = SRX(422 + row * 28);
        const int lx = SRX(col ? 334 : 40), rx = SRX(col ? 600 : 306);
        SrText(ref, px, W, H, lab[i], lx, y, SRX(18), FW_NORMAL, 0, 0, SR_LABEL);
        SrText(ref, px, W, H, v[i], rx, y, SRX(18), FW_BOLD, 0, 2, i == 12 && r->n.damaged == 0 ? 0x9FD49Au : i == 12 ? 0xE08A7Au : SR_VALUE);
    }
    SrFill(px, W, H, W / 2, SRX(424), 1, SRX(7 * 28 - 8), 0xA89A6Cu, 70);
    SrFill(px, W, H, SRX(40), SRX(622), W - SRX(80), 1, 0xA89A6Cu, 90);
    SrText(ref, px, W, H, "Press A or Enter to close", W / 2, SRX(630), SRX(15), FW_NORMAL, 1, 1, 0x8A8F96u);
#undef SRX
}
// One card frame: the grade letter on its own transparent layer (the meter's rank-up motions tint / clear rects around the letter and
// assume nothing is under it), then that layer over the base. ageMs = ms since the card appeared: the meter's motion for the grade plays once.
static void SrComposeFrame(HDC mem, uint32_t* px, const uint32_t* base, const SrGeo* g, const SrResult* r, double ageMs) {
    const int n = g->w * g->h;
    memset(px, 0, (size_t)n * 4);
    if (!g_sm_atlas_dc) SrText(mem, px, g->w, g->h, sm_rank_name[r->grade], g->w / 2, (int)(80 * g->k), (int)(160 * g->k), FW_HEAVY, 1, 1, 0xE64430u);
    else {
        int mlw, mlh, mww, mwh; SmAtlasMax(&mlw, &mlh, &mww, &mwh);
        const float unit = g->k, kl = 180.f * g->k / (float)mlh, mid = g->w / 2.f, rowBottom = 72.f * g->k + mlh * kl;
        const SmLetterCtx lc = {mem, px, g->w, g->h, mid, rowBottom, kl, unit};
        const float dt = (float)(ageMs - SR_FADE_IN_MS * 0.6);
        if (!(r->grade >= 1 && SmRankUpDraw(&lc, r->grade - 1, r->grade, dt))) {
            const int* L = sm_atlas_letter[r->grade];
            const float cx = mid + g_sm_letter_dx[r->grade] * kl, cy = rowBottom - L[3] * kl / 2.f;
            SmBlitA(mem, (int)(cx - L[2] * kl / 2.f + 0.5f), (int)(cy - L[3] * kl / 2.f + 0.5f), L, L[2], kl, 255);
        }
    }
    GdiFlush();
    for (int i = 0; i < n; ++i) {  // premultiplied: out = letter + base x (1 - letter alpha)
        const uint32_t l = px[i], b = base[i]; const unsigned inv = 255 - (l >> 24);
        if (inv == 255) { px[i] = b; continue; }
        const unsigned a = (l >> 24) + (b >> 24) * inv / 255, rr = ((l >> 16) & 0xFF) + ((b >> 16) & 0xFF) * inv / 255,
                       gg = ((l >> 8) & 0xFF) + ((b >> 8) & 0xFF) * inv / 255, bb = (l & 0xFF) + (b & 0xFF) * inv / 255;
        px[i] = (a > 255 ? 255u : a) << 24 | (rr > 255 ? 255u : rr) << 16 | (gg > 255 ? 255u : gg) << 8 | (bb > 255 ? 255u : bb);
    }
}
// Card opacity 0..1 at ageMs. showMs <= 0: no timeout. closeMs >= 0: ms since the player closed it (fades out).
static float SrCardAlpha(double ageMs, int showMs, double closeMs) {
    if (ageMs < 0) return 0.f;
    float a = ageMs < SR_FADE_IN_MS ? (float)(ageMs / SR_FADE_IN_MS) : 1.f;
    if (showMs > 0) { if (ageMs >= showMs) return 0.f; if (ageMs > showMs - SR_FADE_OUT_MS) { const float b = (float)((showMs - ageMs) / SR_FADE_OUT_MS); if (b < a) a = b; } }
    if (closeMs >= 0) { if (closeMs >= SR_FADE_OUT_MS) return 0.f; const float b = 1.f - (float)(closeMs / SR_FADE_OUT_MS); if (b < a) a = b; }
    return a;
}
// Confirm press edge: down now, up on the previous poll. armed = the card has been up SR_ARM_MS and the button was seen released since.
static int SrConfirmEdge(int down, int* wasDown, int* sawUp) {
    const int edge = down && !*wasDown && *sawUp;
    if (!down) *sawUp = 1;
    *wasDown = down; return edge;
}
#endif
