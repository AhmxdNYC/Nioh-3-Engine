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

#include "inputhist_glyphs.inc"

constexpr int kSlots = 6;  // button glyphs per row

// direction arrow (#38929, restored dpad design): the original arrow = chevron head on a stem, rotated per direction (inputhist::Dir:
// 1 up, then clockwise in 45 degree steps), drawn as an anti-aliased signed-distance shape with rounded corners. Every pixel gets
// coverage alpha; runs of equal alpha are merged into one DrawRect.
inline float Seg(float u, float v, float ax, float ay, float bx, float by) {  // distance to segment a-b
    const float px = u - ax, py = v - ay, dx = bx - ax, dy = by - ay;
    const float t = std::min(1.0f, std::max(0.0f, (px * dx + py * dy) / (dx * dx + dy * dy)));
    return std::sqrt((px - dx * t) * (px - dx * t) + (py - dy * t) * (py - dy * t));
}
// #39098/#39102 approved look: round-capped shaft with a chevron head (the first arrows, smooth), no filled triangle
inline float ArrowSd(float u, float v) {
    const float r = 0.075f;
    const float shaft = Seg(u, v, -0.38f, 0.0f, 0.38f, 0.0f);
    const float h1 = Seg(u, v, 0.38f, 0.0f, 0.08f, 0.30f), h2 = Seg(u, v, 0.38f, 0.0f, 0.08f, -0.30f);
    return std::min(shaft, std::min(h1, h2)) - r;
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
    // #39159: no grey ring - the dark button fill is extended over the sprite's ring (annulus 0.82R..R repainted in the disc colour)
    const float R = (float)std::max(r.w, r.h) * s * 0.5f, cx = bx + size * 0.5f, cy = by + size * 0.5f, ri = R * 0.82f;
    const int n = (int)std::ceil(R) + 1;
    for (int y = -n; y <= n; ++y) {
        int runStart = 0, runQ = 0;
        for (int x = -n; x <= n + 1; ++x) {
            int q = 0;
            if (x <= n) {
                const float d = std::sqrt(((float)x + 0.5f) * ((float)x + 0.5f) + ((float)y + 0.5f) * ((float)y + 0.5f));
                const float cov = std::min(1.0f, std::max(0.0f, R + 0.5f - d)) * std::min(1.0f, std::max(0.0f, d - ri + 0.5f));
                q = (int)std::lround(cov * 8.0f);
            }
            if (q != runQ || x == n + 1) {
                if (runQ > 0) osd::DrawRect(cx + (float)runStart, cy + (float)y, (float)(x - runStart), 1.0f, ((0xF8u * (uint32_t)runQ / 8u) << 24) | 0x001C1D22u);
                runStart = x; runQ = q;
            }
        }
    }
}

// stroke glyphs (approved art v7): digits, N and cap labels are unit-box segment sets drawn with the same anti-aliased rounded stroke as
// the arrows (one weight everywhere). Runs of equal alpha merge into one DrawRect.
struct Seg4 { float x0, y0, x1, y1; };
inline void StrokeSegs(const Seg4* sg, int n, float bx, float by, float w, float h, float lw, uint32_t color) {
    const uint32_t alpha0 = color >> 24, rgb = color & 0x00FFFFFFu;
    const int pad = (int)std::ceil(lw) + 1, nw = (int)std::ceil(w) + 2 * pad, nh = (int)std::ceil(h) + 2 * pad;
    for (int y = 0; y < nh; ++y) {
        int runStart = 0, runQ = 0;
        for (int x = 0; x <= nw; ++x) {
            int q = 0;
            if (x < nw) {
                const float px = (float)(x - pad) + 0.5f, py = (float)(y - pad) + 0.5f;
                float dmin = 1e9f;
                for (int i = 0; i < n; ++i) dmin = std::min(dmin, Seg(px, py, sg[i].x0 * w, sg[i].y0 * h, sg[i].x1 * w, sg[i].y1 * h));
                q = (int)std::lround(std::min(1.0f, std::max(0.0f, 0.5f - (dmin - lw * 0.5f))) * 8.0f);
            }
            if (q != runQ || x == nw) {
                if (runQ > 0) osd::DrawRect(bx - (float)pad + (float)runStart, by - (float)pad + (float)y, (float)(x - runStart), 1.0f, ((alpha0 * (uint32_t)runQ / 8u) << 24) | rgb);
                runStart = x; runQ = q;
            }
        }
    }
}
inline void GlyphAt(char c, float bx, float by, float w, float h, float lw, uint32_t color) {
    for (const GlyphDef& g : kGlyphs)
        if (g.ch == c) { StrokeSegs((const Seg4*)g.seg, g.n, bx, by, w, h, lw, color); return; }
}
// string of stroke glyphs: total width tw = n*gw + (n-1)*gap; centred on cx (center = true) or starting at cx
inline float GlyphsW(int n, float gw, float gap) { return (float)n * gw + (float)(n - 1) * gap; }
inline void GlyphStr(const char* t, float cx, float top, float gh, bool center, uint32_t color) {
    const int n = (int)strlen(t);
    const float gw = gh * 0.58f, gap = gh * 0.25f, lw = std::max(1.25f, gh * 0.125f);
    float x = center ? cx - GlyphsW(n, gw, gap) * 0.5f : cx;
    for (int i = 0; i < n; ++i, x += gw + gap) GlyphAt(t[i], std::floor(x + 0.5f), std::floor(top + 0.5f), gw, gh, lw, color);
}

// bumper / trigger cap (art v7): same shoulder shape, trigger just taller; outer top corner rounded, thin light outline, stroke label.
inline void ShoulderCap(int style, int b, float bx, float cy, float icon) {  // b: 0 LB 1 RB 2 LT 3 RT
    namespace ms = menu_style;
    const bool trig = b >= 2, left = !(b & 1);
    const float w = ms::Snap(1.5f * icon), h = ms::Snap(trig ? icon : 0.62f * icon), x0 = ms::Snap(bx), y0 = ms::Snap(cy - h * 0.5f);  // #39172: sized to the face buttons (icon)
    const float r = std::floor(h * (trig ? 0.32f : 0.38f));
    for (int rr = 0; rr < (int)h; ++rr) {  // fill, 1 px rows
        float in = 0.0f;
        if ((float)rr < r) { const float k = (r - ((float)rr + 0.5f)) / r; in = r * (1.0f - std::sqrt(std::max(0.0f, 1.0f - k * k))); }
        in = std::floor(in);
        osd::DrawRect(x0 + (left ? in : 0.0f), y0 + (float)rr, w - in, 1.0f, ms::kBumperBg);
    }
    char t[3] = {b & 1 ? 'R' : 'L', 0, 0};
    t[1] = b >= 2 ? (style ? '2' : 'T') : (style ? '1' : 'B');
    const float gh = std::floor(h * (trig ? 0.5f : 0.62f));
    GlyphStr(t, x0 + w * 0.5f, y0 + (h - gh) * 0.5f + h * 0.04f, gh, true, 0xFFEBECF0u);
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
    const float numW = 2.0f * cw, chipW = 2.0f * cw + pad, slotW = std::max(std::max(icon, chipW), std::ceil(1.5f * icon)) + std::floor(pad * 0.5f);
    const float w = pad + numW + std::floor(pad * 1.5f) + icon + pad + (float)kSlots * slotW + pad;
    const float h = (float)rows * rowH + 2.0f * pad;
    if (right) x -= w;

    menu_style::Panel(x, y, w, h, sc, menu_style::kPanelBgSide, 1.0f, false, false);  // #38452: no gold top line, #38501: no outline

    Entry e[kMaxRows];
    const int n = Snapshot(e, rows);
    static const int kFace[4] = {padviz::S_A, padviz::S_B, padviz::S_X, padviz::S_Y};
    static const int kShoulder[4] = {padviz::S_LB, padviz::S_RB, padviz::S_LT, padviz::S_RT};  // inputhist::btn bits 4..7 (LB RB LT RT / L1 R1 L2 R2)
    const float top = y + pad;
    for (int i = 0; i < n; ++i) {
        const float ry = top + (float)i * rowH;
        if (i & 1) osd::DrawRect(x + 2.0f, ry, w - 4.0f, rowH, 0x0CFFFFFFu);  // #38452: alternating row only slightly lighter
        const uint64_t t1 = i == 0 ? nowMs : e[i - 1].t0;
        char buf[8];
        snprintf(buf, sizeof buf, "%d", Frames(e[i].t0, t1));
        const float ty = ry + std::floor((rowH - th) * 0.5f);
        const float nx = x + pad + (float)(2 - (int)strlen(buf)) * cw;
        (void)nx; (void)ty;
        { const float gh = std::floor(th * 0.62f); GlyphStr(buf, x + pad + (float)(2 - (int)strlen(buf)) * cw * 1.0f + 0.0f, ry + std::floor((rowH - gh) * 0.5f), gh, false, menu_style::kText); }
        const float ax = x + pad + numW + std::floor(pad * 1.5f), ay = ry + std::floor((rowH - icon) * 0.5f);
        if (e[i].dir == N) { const float gh = std::floor(icon * 0.6f); GlyphAt('N', std::floor(ax + (icon - gh * 0.78f) * 0.5f), std::floor(ay + (icon - gh) * 0.5f), gh * 0.78f, gh, std::max(1.5f, icon * 0.1f), menu_style::kText); }
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
                ShoulderCap(style, b, bx, ay + icon * 0.5f, icon);
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
