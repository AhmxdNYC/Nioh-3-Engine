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

// smooth direction arrow (#38929): a filled triangle pointing along dir (inputhist::Dir: 1 up, then clockwise in 45 degree steps),
// scan-converted into 1 px high spans so it is crisp at any size instead of a chunky bitmap
inline void Arrow(int dir, float bx, float by, float size, uint32_t color) {
    const float PI = 3.14159265f, a = (float)(dir - 1) * PI * 0.25f;
    const float dx = std::sin(a), dy = -std::cos(a), px = -dy, py = dx;  // forward and perpendicular unit vectors
    const float R = size * 0.46f, cx = bx + size * 0.5f, cy = by + size * 0.5f;
    const float vx[3] = {cx + dx * R, cx - dx * R * 0.7f + px * R * 0.85f, cx - dx * R * 0.7f - px * R * 0.85f};
    const float vy[3] = {cy + dy * R, cy - dy * R * 0.7f + py * R * 0.85f, cy - dy * R * 0.7f - py * R * 0.85f};
    const int y0 = (int)std::floor(std::min(vy[0], std::min(vy[1], vy[2]))), y1 = (int)std::ceil(std::max(vy[0], std::max(vy[1], vy[2])));
    for (int y = y0; y < y1; ++y) {
        const float sy = (float)y + 0.5f;
        float lo = 1e9f, hi = -1e9f;
        for (int i = 0; i < 3; ++i) {
            const int j = (i + 1) % 3;
            if ((vy[i] <= sy) == (vy[j] <= sy)) continue;
            const float x = vx[i] + (sy - vy[i]) * (vx[j] - vx[i]) / (vy[j] - vy[i]);
            lo = std::min(lo, x); hi = std::max(hi, x);
        }
        if (hi > lo) osd::DrawRect(std::floor(lo + 0.5f), (float)y, std::max(1.0f, std::floor(hi + 0.5f) - std::floor(lo + 0.5f)), 1.0f, color);
    }
}

// a face-button glyph: the controller display's lit sprite for that button, fitted into a size x size box (padviz::Layer: same call
// the controller picture uses; opacity 1 = the pressed look incl. the pressed-colour tint)
inline void FaceGlyph(int skinStyle, int spriteId, float bx, float by, float size) {
    const padviz::Skin& k = padviz::SkinFor(skinStyle);
    const padviz::SpriteRect& r = k.s[spriteId];
    if (!r.w) return;
    const float s = size / (float)std::max(r.w, r.h);
    const float ox = bx + (size - (float)r.w * s) * 0.5f - r.cx * s, oy = by + (size - (float)r.h * s) * 0.5f - r.cy * s;
    padviz::Layer(k, spriteId, ox, oy, s, 0.97f);  // #38919: resting (unlit) sprite, no pressed tint
}

// draws the panel with its top-left corner at (x, y) (right = false) or its top-right corner at (x, y) (right = true).
// Returns the text scale used (the caller sets the quad group's shader scale to it), 0 = nothing drawn.
// the panel may grow down to kBottomFrac of the screen height: below that sit the game's own bottom HUD widgets (items / ninjutsu
// shortcuts, boss HP bar), so a long history never covers them. Depends only on the screen size and the panel's top, so the panel
// height stays fixed while inputs change.
constexpr float kBottomFrac = 0.80f;
inline float Draw(unsigned W, unsigned H, float x, float y, bool right, uint64_t nowMs, int style) {
    using namespace inputhist;
    (void)W;
    const float sc = std::max(0.5f, std::floor((float)H / 1080.0f * 1.5f * 8.0f) / 8.0f);
    const float cw = (float)osd::kGlyphW * sc, th = (float)osd::kGlyphH * sc;
    const float rowH = std::floor(22.0f * sc), pad = std::floor(6.0f * sc), icon = std::floor(rowH * 0.8f);
    const int fit = (int)std::floor(((float)H * kBottomFrac - y - 2.0f * pad) / rowH);
    const int rows = std::clamp(std::min((int)g_rows, fit), kMinRows, kMaxRows);
    const float numW = 2.0f * cw, chipW = 2.0f * cw + pad, slotW = std::max(icon, chipW) + std::floor(pad * 0.5f);
    const float w = pad + numW + std::floor(pad * 1.5f) + icon + pad + (float)kSlots * slotW + pad;
    const float h = (float)rows * rowH + 2.0f * pad;
    if (right) x -= w;

    menu_style::Panel(x, y, w, h, sc, menu_style::kPanelBgSide, 1.0f, false, false);  // #38452: no gold top line, #38501: no outline

    Entry e[kMaxRows];
    const int n = Snapshot(e, rows);
    static const char* const kLabelXbox[4] = {"LB", "RB", "LT", "RT"};
    static const char* const kLabelPs[4] = {"L1", "R1", "L2", "R2"};
    const char* const* label = style ? kLabelPs : kLabelXbox;
    static const int kFace[4] = {padviz::S_A, padviz::S_B, padviz::S_X, padviz::S_Y};
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
