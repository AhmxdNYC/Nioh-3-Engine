// inputhist_draw.h - draws the SF6-style input history panel (data: inputhist.h). Called from osd.cpp BuildOverlay inside the pad quad
// group (padviz.h controller display): that group binds the current ButtonStyle skin's sprite atlas, so the face-button glyphs are the
// controller display's own lit sprites drawn through padviz::Layer (same code path, same atlas, same Xbox / PlayStation skin, same
// pressed-colour setting) - not a copy. Newest row on top; per row: frames held (left, 60 Hz frames, 99 max), direction arrow
// (8-way, "N" = neutral), buttons held (right). Panel look and text scale follow the mod menu side panel (menu_style.h, menu_geom
// glyph metrics). The panel height is fixed by the row count, so nothing shifts while inputs change. Drawing only.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "inputhist.h"
#include "padviz.h"
#include "menu_style.h"

namespace inputhist_draw {

constexpr int kSlots = 6;  // button glyphs per row

// 7x7 arrow bitmaps: orthogonal up and diagonal up-right; the other six are rotations of these
inline const char* const kUp[7] = {"...#...", "..###..", ".#####.", "...#...", "...#...", "...#...", "...#..."};
inline const char* const kUR[7] = {"..#####", ".....##", "....#.#", "...#..#", "..#...#", ".#.....", "#......"};

inline bool Cell(int dir, int r, int c) {  // dir: inputhist::Dir (not N)
    const bool diag = (dir & 1) == 0;       // UR, DR, DL, UL are even
    const int quarter = diag ? (dir - 2) / 2 : (dir - 1) / 2;  // clockwise quarter turns from the base bitmap
    for (int q = 0; q < quarter; ++q) {  // rotating the bitmap cw: new(r, c) = old(6 - c, r)  =>  read old at the inverse
        const int nr = 6 - c, nc = r;
        r = nr; c = nc;
    }
    return (diag ? kUR[r] : kUp[r])[c] == '#';
}

inline void Arrow(int dir, float bx, float by, float size, uint32_t color) {
    const float u = std::max(1.0f, std::floor(size / 7.0f));
    const float ox = bx + std::floor((size - 7.0f * u) * 0.5f), oy = by + std::floor((size - 7.0f * u) * 0.5f);
    for (int r = 0; r < 7; ++r)
        for (int c = 0; c < 7; ++c)
            if (Cell(dir, r, c)) osd::DrawRect(ox + (float)c * u, oy + (float)r * u, u, u, color);
}

// a face-button glyph: the controller display's lit sprite for that button, fitted into a size x size box (padviz::Layer: same call
// the controller picture uses; opacity 1 = the pressed look incl. the pressed-colour tint)
inline void FaceGlyph(int skinStyle, int spriteId, float bx, float by, float size) {
    const padviz::Skin& k = padviz::SkinFor(skinStyle);
    const padviz::SpriteRect& r = k.s[spriteId];
    if (!r.w) return;
    const float s = size / (float)std::max(r.w, r.h);
    const float ox = bx + (size - (float)r.w * s) * 0.5f - r.cx * s, oy = by + (size - (float)r.h * s) * 0.5f - r.cy * s;
    padviz::Layer(k, spriteId, ox, oy, s, 1.0f);
}

// draws the panel with its top-left corner at (x, y) (right = false) or its top-right corner at (x, y) (right = true).
// Returns the text scale used (the caller sets the quad group's shader scale to it), 0 = nothing drawn.
inline float Draw(unsigned W, unsigned H, float x, float y, bool right, uint64_t nowMs, int style) {
    using namespace inputhist;
    (void)W;
    const int rows = std::clamp((int)g_rows, kMinRows, kMaxRows);
    const float sc = std::max(0.5f, std::floor((float)H / 1080.0f * 1.5f * 8.0f) / 8.0f);
    const float cw = (float)osd::kGlyphW * sc, th = (float)osd::kGlyphH * sc;
    const float rowH = std::floor(22.0f * sc), pad = std::floor(6.0f * sc), icon = std::floor(rowH * 0.8f);
    const float numW = 2.0f * cw, chipW = 2.0f * cw + pad, slotW = std::max(icon, chipW) + std::floor(pad * 0.5f);
    const float w = pad + numW + std::floor(pad * 1.5f) + icon + pad + (float)kSlots * slotW + pad;
    const float h = (float)rows * rowH + 2.0f * pad;
    if (right) x -= w;

    menu_style::Panel(x, y, w, h, sc, menu_style::kPanelBgSide, 1.0f, false);  // #38452: no gold top line

    Entry e[kMaxRows];
    const int n = Snapshot(e, rows);
    static const char* const kLabelXbox[4] = {"LB", "RB", "LT", "RT"};
    static const char* const kLabelPs[4] = {"L1", "R1", "L2", "R2"};
    const char* const* label = style ? kLabelPs : kLabelXbox;
    static const int kFace[4] = {padviz::S_A_LIT, padviz::S_B_LIT, padviz::S_X_LIT, padviz::S_Y_LIT};
    const uint32_t chipBg = 0xE0000000u | padviz::PackBgr(padviz::g_pressRGB);
    const float top = y + pad;
    for (int i = 0; i < n; ++i) {
        const float ry = top + (float)i * rowH;
        if (i & 1) osd::DrawRect(x + 2.0f, ry, w - 4.0f, rowH, 0x0CFFFFFFu);  // #38452: alternating row only slightly lighter
        const uint64_t t1 = i == 0 ? nowMs : e[i - 1].t0;
        char buf[8];
        snprintf(buf, sizeof buf, "%d", Frames(e[i].t0, t1));
        const float ty = ry + std::floor((rowH - th) * 0.5f);
        const float nx = x + pad + (float)(2 - (int)strlen(buf)) * cw;
        osd::DrawText(nx, ty, buf, menu_style::kText, sc);
        const float ax = x + pad + numW + std::floor(pad * 1.5f), ay = ry + std::floor((rowH - icon) * 0.5f);
        if (e[i].dir == N) osd::DrawText(ax + std::floor((icon - cw) * 0.5f), ty, "N", menu_style::kTextDim, sc);
        else Arrow(e[i].dir, ax, ay, icon, menu_style::kText);
        float bx = ax + icon + pad;
        int used = 0;
        for (int b = 0; b < 4 && used < kSlots; ++b)
            if (e[i].btn & (1 << b)) {
                FaceGlyph(style, kFace[b], bx, ay, icon);
                bx += slotW;
                ++used;
            }
        for (int b = 0; b < 4 && used < kSlots; ++b)
            if (e[i].btn & (16 << b)) {
                const float cx = bx, cy = ry + std::floor((rowH - th - 2.0f * sc) * 0.5f);
                osd::DrawRect(cx, cy, chipW, th + 2.0f * sc, chipBg);
                osd::DrawText(cx + std::floor(pad * 0.5f), cy + sc, label[b], menu_style::kChipText, sc);
                bx += slotW;
                ++used;
            }
    }
    return sc;
}

// panel height in pixels for the current row count (osd.cpp uses it to stack below / above other overlays)
inline float PanelHeight(unsigned H) {
    const float sc = std::max(0.5f, std::floor((float)H / 1080.0f * 1.5f * 8.0f) / 8.0f);
    const float rowH = std::floor(22.0f * sc), pad = std::floor(6.0f * sc);
    return (float)std::clamp((int)inputhist::g_rows, inputhist::kMinRows, inputhist::kMaxRows) * rowH + 2.0f * pad;
}

}  // namespace inputhist_draw
