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
#include "hint_shapes.h"

#include "inputhist_dpad.h"

namespace inputhist_draw {

constexpr int kSlots = 6;  // button glyphs per row

using hint_shapes::Arrow;
using hint_shapes::ArrowSd;  // existing geometry-check callers

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

// Same approved shapes as the menu hints; keep history scale and colours unchanged.
using hint_shapes::GlyphAt;
using hint_shapes::GlyphStr;
inline void ShoulderCap(int style, int b, float bx, float cy, float icon) {
    hint_shapes::ShoulderCap(style, b, bx, cy, icon, menu_style::kBumperBg, 0xFFEBECF0u, 0.72f);  // trigger only slightly taller than the 0.62-icon bumper
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
        const float cy = ry + rowH * 0.5f;
        const float ax = x + pad + numW + std::floor(pad * 1.5f), ay = cy - icon * 0.5f;
        if (e[i].dir == N) { const float gh = std::floor(icon * 0.6f); GlyphAt('N', std::floor(ax + (icon - gh * 0.78f) * 0.5f), std::floor(ay + (icon - gh) * 0.5f), gh * 0.78f, gh, std::max(1.5f, icon * 0.1f), menu_style::kText); }
        else Arrow(e[i].dir, ax, ay, icon, menu_style::kText);
        float bx = ax + icon + pad;
        int used = 0;
        if(e[i].dpad) { inputhist_dpad::Draw(style,e[i].dpad,bx+(slotW-icon)*0.5f,cy-icon*0.5f,icon);bx+=slotW;++used; }
        for (int b = 0; b < 4 && used < kSlots; ++b)
            if (e[i].btn & (1 << b)) {
                FaceGlyph(style, kFace[b], bx + (slotW - icon) * 0.5f, cy - icon * 0.5f, icon);
                bx += slotW;
                ++used;
            }
        for (int b = 0; b < 4 && used < kSlots; ++b)
            if (e[i].btn & (16 << b)) {
                ShoulderCap(style, b, bx + (slotW - hint_shapes::Snap(1.5f * icon)) * 0.5f, cy, icon);
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
