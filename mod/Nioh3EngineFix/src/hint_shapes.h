// hint_shapes.h - approved input-history and menu-prompt geometry, shared so both stay in sync.
// Drawing only; callers supply colours/fade and sizing. Does not affect the controller-overlay sprite path.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include "osd.h"

namespace hint_shapes {
#include "inputhist_glyphs.inc"
inline float Snap(float v) { return (float)(int)(v + 0.5f); }

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

// Approved solid shoulder caps: wider bumper, taller trigger, one shaped outer corner, no border.
inline void ShoulderCap(int style, int b, float bx, float cy, float icon, uint32_t fill, uint32_t text, float triggerHeight = 1.0f) {  // b: 0 LB 1 RB 2 LT 3 RT
    const bool trig = b >= 2, left = !(b & 1);
    const float w = Snap(1.5f * icon), h = Snap((trig ? triggerHeight : 0.62f) * icon), x0 = Snap(bx), y0 = Snap(cy - h * 0.5f);  // #39172: sized to the face buttons (icon)
    const float r = std::floor(h * (trig ? 0.32f : 0.38f));
    for (int rr = 0; rr < (int)h; ++rr) {  // fill, 1 px rows
        float in = 0.0f;
        if ((float)rr < r) { const float k = (r - ((float)rr + 0.5f)) / r; in = r * (1.0f - std::sqrt(std::max(0.0f, 1.0f - k * k))); }
        in = std::floor(in);
        osd::DrawRect(x0 + (left ? in : 0.0f), y0 + (float)rr, w - in, 1.0f, fill);
    }
    char t[3] = {b & 1 ? 'R' : 'L', 0, 0};
    t[1] = b >= 2 ? (style ? '2' : 'T') : (style ? '1' : 'B');
    const float gh = std::floor(h * (trig ? 0.5f : 0.62f));
    GlyphStr(t, x0 + w * 0.5f, y0 + (h - gh) * 0.5f + h * 0.04f, gh, true, text);
}

}  // namespace hint_shapes
