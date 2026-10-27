// inputhist_draw.h - draws the SF6-style input history panel (data: inputhist.h). Called from osd.cpp BuildOverlay inside the pad quad
// group (padviz.h controller display): that group binds the current ButtonStyle skin's sprite atlas, so the face-button glyphs are the
// controller display's own resting sprites drawn through padviz::Layer (same code path, same atlas, same Xbox / PlayStation skin, same
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

// direction arrow (#38929, restored dpad design): the original arrow = chevron head on a stem, rotated per direction (inputhist::Dir:
// 1 up, then clockwise in 45 degree steps), drawn as an anti-aliased signed-distance shape with rounded corners. Every pixel gets
// coverage alpha; runs of equal alpha are merged into one DrawRect.
inline float ArrowSd(float u, float v) {  // local space, forward = +u, unit = size; rounded head triangle united with a capsule stem
    const float r = 0.07f;                 // corner rounding
    const float tx = 0.50f - r * 2.0f, bx = -0.04f, hw = 0.42f - r;  // triangle shrunk by r, then re-grown below
    // edges of the head triangle: tip (tx,0), base corners (bx, +-hw)
    const float ex = tx - bx, ey = hw, el = std::sqrt(ex * ex + ey * ey);
    const float d1 = ((u - bx) * ey - std::fabs(v) * ex) / el;  // distance to the slanted edge (negative inside)
    const float d2 = bx - u;                                      // base edge
    const float head = std::max(d1, d2) - r;
    const float su = std::min(std::max(u, -0.46f), 0.02f);      // stem capsule from -0.46 to 0.02
    const float stem = std::sqrt((u - su) * (u - su) + v * v) - 0.115f;
    return std::min(head, stem);
}

inline void Arrow(int dir, float bx, float by, float size, uint32_t color) {
    const float PI = 3.14159265f, a = (float)(dir - 1) * PI * 0.25f;
    const float fx = std::sin(a), fy = -std::cos(a);  // forward unit vector (screen space)
    const float cx = bx + size * 0.5f, cy = by + size * 0.5f, inv = 1.0f / size;
    const uint32_t alpha0 = color >> 24, rgb = color & 0x00FFFFFFu;
    const int n = (int)std::ceil(size);
    for (int y = 0; y < n; ++y) {
        int runStart = 0, runQ = 0;
        for (int x = 0; x <= n; ++x) {
            int q = 0;
            if (x < n) {
                const float dx = (bx + (float)x + 0.5f) - cx, dy = (by + (float)y + 0.5f) - cy;
                const float sd = ArrowSd((dx * fx + dy * fy) * inv, (-dx * fy + dy * fx) * inv) * size;  // pixels
                q = (int)std::lround(std::min(1.0f, std::max(0.0f, 0.5f - sd)) * 8.0f);
            }
            if (q != runQ || x == n) {
                if (runQ > 0) {
                    const uint32_t al = alpha0 * (uint32_t)runQ / 8u;
                    osd::DrawRect(bx + (float)runStart, by + (float)y, (float)(x - runStart), 1.0f, (al << 24) | rgb);
                }
                runStart = x; runQ = q;
            }
        }
    }
}

// a face-button glyph: the controller display's lit sprite for that button, fitted into a size x size box (padviz::Layer: same call
// the controller picture uses; the resting sprite in its own colours, never tinted by the pressed-colour setting, so it matches the
// controller display's resting look)
inline void FaceGlyph(int skinStyle, int spriteId, float bx, float by, float size) {
    const padviz::Skin& k = padviz::SkinFor(skinStyle);
    const padviz::SpriteRect& r = k.s[spriteId];
    if (!r.w) return;
    const float s = size / (float)std::max(r.w, r.h);
    const float ox = bx + (size - (float)r.w * s) * 0.5f - r.cx * s, oy = by + (size - (float)r.h * s) * 0.5f - r.cy * s;
    padviz::Layer(k, spriteId, ox, oy, s, 0.97f, 0, 0, 1, 1, 1, false);  // #38919: resting (unlit) sprite, no pressed tint
}

// draws the panel with its top-left corner at (x, y) (right = false) or its top-right corner at (x, y) (right = true).
// Returns the text scale used (the caller sets the quad group's shader scale to it), 0 = nothing drawn.
// the panel may grow down to kBottomFrac of the screen height: below that sit the game's own bottom HUD widgets (items / ninjutsu
// shortcuts, boss HP bar), so a long history never covers them. Depends only on the screen size and the panel's top, so the panel
// height stays fixed while inputs change.
constexpr float kBottomFrac = 0.63f;  // #38945: stop above the 3 triangle markers (~0.665) and the consumables (~0.78)
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
