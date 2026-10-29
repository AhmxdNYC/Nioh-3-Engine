// menu_style.h - one palette and a few drawing helpers shared by every in-game mod menu panel drawn through the OSD
// (EngineFix menu, the hosted Boss Rematch / Practice / Training side panel, corner notes). Drawing only: no state, no input.
// Colours are 0xAABBGGRR like osd::DrawRect / DrawText. Every helper takes a fade factor (0..1) for the short open fade-in.
#pragma once
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "osd.h"
#include "hint_shapes.h"

namespace menu_style {

// ---- palette: dark slate panel, warm gold accent (matches the game's own UI gold) -------------------------------------
constexpr uint32_t kPanelBg     = 0x73140F0C;  // one shared translucency for every mod panel (#38305)
constexpr uint32_t kPanelBgSide = 0x73140F0C;  // side panel: lighter so the arena shows through
constexpr uint32_t kTitleBg     = 0xF01C1A16;  // title bar
constexpr uint32_t kFooterBg    = 0x90000000;  // footer band (hint line)
constexpr uint32_t kBorder      = 0x70506070;  // 1 px outline
constexpr uint32_t kAccent      = 0xFF5AC8F0;  // gold: top line, selected stripe, section headers
constexpr uint32_t kAccentSoft  = 0x705AC8F0;  // gold wash behind the selected row (human #44361: stronger, readable while hovering)
constexpr uint32_t kDivider     = 0x38FFFFFF;
constexpr uint32_t kTitleText   = 0xFF8CDCF8;  // pale gold
constexpr uint32_t kText        = 0xFFF0F0F0, kTextDim = 0xFF8C8C8C, kTextMuted = 0xFFB4B4B4;
constexpr uint32_t kOnBg  = 0xF0F0F0F0, kOnText  = 0xFF141414;   // human #44361: [ON] = bright white pill, dark text (no green)
constexpr uint32_t kOffBg = 0x80201C1A, kOffText = 0xFFB4B4B4;   // [OFF] = dark pill, dim text
constexpr uint32_t kRptBg = 0xF01E8CD2, kRptText = 0xFFFFFFFF;   // amber pill (repeat)
constexpr uint32_t kUnkBg = 0xA0303030, kUnkText = 0xFF8C8C8C;   // unknown / unavailable
constexpr uint32_t kChipBg = 0xE0DCDCDC, kChipText = 0xFF141414; // button prompt key caps
constexpr uint32_t kTrack = 0x50FFFFFF;                          // slider track

inline uint32_t Fade(uint32_t c, float a) {
    if (a >= 1.0f) return c;
    if (a <= 0.0f) return c & 0x00FFFFFFu;
    return (c & 0x00FFFFFFu) | ((uint32_t)((float)(c >> 24) * a) << 24);
}

// fade factor for the panel: 0 -> 1 over 140 ms after the panel was (re)opened (a gap of > 120 ms without a draw = reopened)
inline float OpenFade(uint64_t nowMs) {
    static uint64_t last = 0, openedAt = 0;
    if (!last || nowMs - last > 120) openedAt = nowMs;
    last = nowMs;
    const float t = (float)(nowMs - openedAt) / 140.0f;
    return t >= 1.0f ? 1.0f : t < 0.0f ? 0.0f : 0.35f + 0.65f * t;
}

// panel body with a 1 px outline and a 2 px gold line on top; rounded corners, radius osd::PanelRadius(s) (#38505)
inline float PanelR(float s) { return osd::PanelRadius(s); }
inline void Panel(float x, float y, float w, float h, float s, uint32_t bg, float a, bool accentLine = true, bool border = true) {
    const float px = s < 1.0f ? 1.0f : (float)(int)(s * 0.75f + 0.5f), r = PanelR(s);
    osd::DrawRoundRect(x, y, w, h, r, Fade(bg, a));
    if (border) osd::DrawRoundFrame(x, y, w, h, r, px, Fade(kBorder, a));
    if (accentLine) osd::DrawRoundBand(x, y, w, h, r, 0.0f, 2 * px, Fade(kAccent, a));
}
// a band (title bar / footer) of rows [row0, row0 + rows) inside a Panel(x, y, w, h, s), inset `side` px, clipped to its corners
inline void PanelBand(float x, float y, float w, float h, float s, float row0, float rows, float side, uint32_t c) {
    osd::DrawRoundBand(x, y, w, h, PanelR(s), row0, rows, c, side);
}

// selected row: soft gold wash plus a solid stripe on the left edge
// human #44361: plus a 1 px gold line above and below, so the hovered row is unmistakable next to the [ON] / [OFF] pills
inline void SelectBar(float x, float y, float w, float h, float s, float a) {
    const float px = s < 2.0f ? 1.0f : (float)(int)(s / 2);
    osd::DrawRect(x, y, w, h, Fade(kAccentSoft, a));
    osd::DrawRect(x, y, w, px, Fade(kAccent, a));
    osd::DrawRect(x, y + h - px, w, px, Fade(kAccent, a));
    osd::DrawRect(x, y, s < 1.0f ? 3.0f : 4.0f * s, h, Fade(kAccent, a));
}

inline void Divider(float x, float y, float w, float s, float a) {
    osd::DrawRect(x, y, w, s < 2.0f ? 1.0f : (float)(int)(s / 2), Fade(kDivider, a));
}

// pill around a short value (ON / OFF / RPT ...) whose text starts at tx; cw = glyph width, lh = line height
inline void Pill(float tx, float y, const char* t, uint32_t bg, uint32_t fg, float cw, float lh, float s, float a) {
    const float w = (float)strlen(t) * cw;
    osd::DrawRect(tx - cw * 0.5f, y - s, w + cw, lh - s, Fade(bg, a));
    osd::DrawText(tx, y, t, Fade(fg, a), s);
}

// pill colours for a value token; false = not a pill value
// human #44361: ON / OFF are shown as the text markers [ON] / [OFF] (the state reads without colour); other values unchanged
inline const char* PillText(const char* v) {
    return !strcmp(v, "ON") ? "[ON]" : !strcmp(v, "OFF") || !strcmp(v, "off") ? "[OFF]" : v;
}
inline bool PillColors(const char* v, uint32_t& bg, uint32_t& fg) {
    if (!strcmp(v, "[ON]")) { bg = kOnBg; fg = kOnText; return true; }
    if (!strcmp(v, "[OFF]")) { bg = kOffBg; fg = kOffText; return true; }
    if (!strcmp(v, "[REPEAT]")) { bg = kRptBg; fg = kRptText; return true; }
    if (!strcmp(v, "[?]")) { bg = kUnkBg; fg = kUnkText; return true; }
    if (!strcmp(v, "ON") || !strcmp(v, "IDLE") || !strcmp(v, "Stand still (idle)") || !strcmp(v, "Stand still + drill")) { bg = kOnBg; fg = kOnText; return true; }
    if (!strcmp(v, "OFF") || !strcmp(v, "off") || !strcmp(v, "CPU") || !strcmp(v, "Fight normally")) { bg = kOffBg; fg = kOffText; return true; }
    if (!strcmp(v, "RPT") || !strcmp(v, "REPEAT") || !strcmp(v, "IDLE (waiting)") || !strcmp(v, "Stand still (getting ready)")) { bg = kRptBg; fg = kRptText; return true; }
    if (!strcmp(v, "?") || !strcmp(v, "CPU (idle failed)") || !strcmp(v, "Fight normally (stand still failed)")) { bg = kUnkBg; fg = kUnkText; return true; }
    return false;
}

// horizontal slider: track + filled part + a small knob; frac 0..1
inline void Slider(float x, float y, float w, float lh, float frac, float s, float a) {
    frac = frac < 0 ? 0 : frac > 1 ? 1 : frac;
    const float th = s < 1.0f ? 2.0f : 3.0f * s * 0.75f, ty = y + (lh - th) * 0.5f - s;
    osd::DrawRect(x, ty, w, th, Fade(kTrack, a));
    if (frac > 0) osd::DrawRect(x, ty, w * frac, th, Fade(kAccent, a));
    const float kw = 3.0f * s, kh = lh * 0.6f;
    osd::DrawRect(x + w * frac - kw * 0.5f, y + (lh - kh) * 0.5f - s, kw, kh, Fade(kText, a));
}

// ---- button prompts (#13537). Every prompt is one or more badges of the same height, centred on the text line, then a muted
// label. Badges: face buttons A / B / X / Y = round badge in the controller colour with a white (Y: dark) letter; the D-pad pairs
// "LR" / "UD" = a light key cap with two drawn shaft-and-head arrows (no '<' '>' characters); anything else (LB, RB, Start, Enter, Esc, PgUp ...)
// = a light rounded key cap with dark text. Keys in one prompt are separated by spaces: Hint(x, y, "LB RB", "Page", ...).
// All edges are snapped to whole pixels and the rounded shapes are built from non-overlapping rows, so they stay crisp and the open
// fade does not show seams. y is the text top of the line (as for osd::DrawText); s = text scale, cw = glyph width (kGlyphW * s).
constexpr uint32_t kBtnA = 0xFF3DA63D, kBtnB = 0xFF2C34D8, kBtnX = 0xFFD86F2E, kBtnY = 0xFF00B4F0;  // Xbox green / red / blue / yellow
constexpr uint32_t kCapBg = 0xF0DCDCDC, kCapText = 0xFF141414, kHintLabel = 0xFFC8C8C8;
constexpr uint32_t kBumperBg = 0xF0585858, kBumperText = 0xFFF0F0F0;

inline float Snap(float v) { return (float)(int)(v + 0.5f); }

// filled rounded rectangle; corner rows are separate 1 px slices (no overlap). Same shared helper as the panels (osd.h).
inline void RoundRect(float x, float y, float w, float h, float r, uint32_t c) { osd::DrawRoundRect(x, y, w, h, r, c); }

inline float BadgeH(float s) { return Snap((float)osd::kGlyphH * s); }
// shoulder button (LB / RB / LT / RT): dark bumper, strongly rounded top, flat-ish bottom, light text (#13537)
// PlayStation symbol in the round badge box (x, top, d = diameter), built from 1 px rects like the rest of this file
inline void PsSymbol(float x, float top, float d, char k, uint32_t c) {
    x = Snap(x); top = Snap(top); d = Snap(d);
    const float m = Snap(d * 0.28f), sz = d - 2.0f * m, t = sz < 9.0f ? 1.0f : 2.0f;  // symbol box, stroke
    const float bx = x + m, by = top + m;
    if (k == 'A') {  // Cross: two diagonals
        for (int i = 0; i < (int)sz; ++i) {
            osd::DrawRect(bx + (float)i, by + (float)i, t, 1.0f, c);
            osd::DrawRect(bx + sz - t - (float)i, by + (float)i, t, 1.0f, c);
        }
    } else if (k == 'B') {  // Circle: ring from rows
        const float r = sz * 0.5f;
        for (int i = 0; i < (int)sz; ++i) {
            const float dy = r - ((float)i + 0.5f), o = r - std::sqrt(std::fmax(0.0f, r * r - dy * dy));
            const float ri = r - t, in = std::fabs(dy) < ri ? r - std::sqrt(std::fmax(0.0f, ri * ri - dy * dy)) : r;
            if (in > o + 0.5f) {  // row crosses the hole: two end strokes
                osd::DrawRect(bx + Snap(o), by + (float)i, std::fmax(1.0f, Snap(in - o)), 1.0f, c);
                osd::DrawRect(bx + sz - Snap(in), by + (float)i, std::fmax(1.0f, Snap(in - o)), 1.0f, c);
            } else osd::DrawRect(bx + Snap(o), by + (float)i, sz - 2.0f * Snap(o), 1.0f, c);
        }
    } else if (k == 'X') {  // Square outline
        osd::DrawRect(bx, by, sz, t, c); osd::DrawRect(bx, by + sz - t, sz, t, c);
        osd::DrawRect(bx, by + t, t, sz - 2.0f * t, c); osd::DrawRect(bx + sz - t, by + t, t, sz - 2.0f * t, c);
    } else {  // Triangle outline: two slanted sides and the base
        const float h = sz, half = sz * 0.5f;
        for (int i = 0; i < (int)h; ++i) {
            const float e = half * ((float)i + 0.5f) / h;
            osd::DrawRect(bx + half - e, by + (float)i, t, 1.0f, c);
            osd::DrawRect(bx + half + e - t, by + (float)i, t, 1.0f, c);
        }
        osd::DrawRect(bx, by + h - t, sz, t, c);
    }
}
// [Menu] ButtonStyle: 0 = Xbox letters, 1 = PlayStation symbols (A = Cross, B = Circle, X = Square, Y = Triangle); set by the menu
inline volatile int g_btnStyle = 0;
// inline row text that names a button (A: open, B): PlayStation names under the PlayStation style
inline const char* PadWord(const char* xbox, const char* ps) { return g_btnStyle ? ps : xbox; }
inline bool IsBumper(const char* k, size_t n) { return n == 2 && (k[0] == 'L' || k[0] == 'R') && (k[1] == 'B' || k[1] == 'T'); }
inline void Bumper(float x, float y, const char* k, float cw, float s, float a) {
    (void)cw;
    const float h = BadgeH(s);
    const int b = (k[0] == 'R' ? 1 : 0) + (k[1] == 'T' ? 2 : 0);
    hint_shapes::ShoulderCap(g_btnStyle, b, Snap(x), Snap(y) + h * 0.5f, h,
                            Fade(kBumperBg, a), Fade(kBumperText, a));
}
// width of one badge for key k (n chars)
inline float BadgeW(const char* k, size_t n, float cw, float s) {
    if (n == 1 && strchr("ABXY", k[0])) return BadgeH(s);                                        // round face button
    if (n == 2 && (!strncmp(k, "LR", 2) || !strncmp(k, "UD", 2))) return Snap(cw * 2.5f);     // D-pad pair
    if (IsBumper(k, n)) return Snap(3.0f * cw);                                                  // shoulder button
    return Snap((float)n * cw + cw);                                                            // key cap
}
inline void Badge(float x, float y, const char* k, size_t n, float cw, float s, float a) {
    const float h = BadgeH(s), top = Snap(y), w = BadgeW(k, n, cw, s);
    if (n == 1 && strchr("ABXY", k[0])) {
        if (g_btnStyle) {  // PlayStation: dark round badge, coloured symbol (Cross blue, Circle red, Square pink, Triangle green)
            RoundRect(x, top, w, h, h * 0.5f, Fade(0xF0382E28, a));
            PsSymbol(x, top, h, k[0], Fade(k[0] == 'A' ? 0xFFF0A050 : k[0] == 'B' ? 0xFF5050F0 : k[0] == 'X' ? 0xFFC888F0 : 0xFF80D060, a));
            return;
        }
        const uint32_t bg = k[0] == 'A' ? kBtnA : k[0] == 'B' ? kBtnB : k[0] == 'X' ? kBtnX : kBtnY;
        RoundRect(x, top, w, h, h * 0.5f, Fade(bg, a));
        const char t[2] = {k[0], 0};
        osd::DrawText(Snap(x + (w - cw) * 0.5f), Snap(top + (h - (float)osd::kGlyphH * s) * 0.5f + 0.5f * s), t,  // caps sit on font rows 3..11
                      Fade(k[0] == 'Y' ? kCapText : 0xFFFFFFFFu, a), s);
        return;
    }
    if (IsBumper(k, n)) { Bumper(x, y, k, cw, s, a); return; }
    RoundRect(x, top, w, h, h * 0.3f, Fade(kBumperBg, a));
    if (n == 2 && (!strncmp(k, "LR", 2) || !strncmp(k, "UD", 2))) {
        // Same shaft + chevron as input history, two square glyphs within the existing badge width.
        const float gap = Snap(cw * 0.15f), size = std::floor((w - gap) * 0.5f);
        const float x0 = x + (w - 2.0f * size - gap) * 0.5f, cy = top + (h - size) * 0.5f;
        hint_shapes::Arrow(k[0] == 'L' ? 7 : 1, x0, cy, size, Fade(kBumperText, a));
        hint_shapes::Arrow(k[0] == 'L' ? 3 : 5, x0 + size + gap, cy, size, Fade(kBumperText, a));
        return;
    }
    char t[16];
    snprintf(t, sizeof t, "%.*s", (int)(n < 15 ? n : 15), k);
    osd::DrawText(Snap(x + cw * 0.5f), top, t, Fade(kBumperText, a), s);
}

// width of a whole prompt incl. its trailing gap
inline float HintW(const char* keys, const char* label, float cw, float s) {
    float w = 0;
    for (const char* p = keys; *p;) {
        while (*p == ' ') ++p;
        const char* e = p;
        while (*e && *e != ' ') ++e;
        if (e > p) w += BadgeW(p, (size_t)(e - p), cw, s) + Snap(cw * 0.25f);
        p = e;
    }
    return w + Snap(cw * 0.35f) + (float)strlen(label) * cw + Snap(cw * 2.0f);
}
// one prompt: badges + label; returns the x after it (with the gap to the next prompt)
inline float Hint(float x, float y, const char* keys, const char* label, float cw, float lh, float s, float a) {
    (void)lh;
    x = Snap(x);
    for (const char* p = keys; *p;) {
        while (*p == ' ') ++p;
        const char* e = p;
        while (*e && *e != ' ') ++e;
        if (e > p) { Badge(x, y, p, (size_t)(e - p), cw, s, a); x += BadgeW(p, (size_t)(e - p), cw, s) + Snap(cw * 0.25f); }
        p = e;
    }
    x += Snap(cw * 0.35f);
    osd::DrawText(x, Snap(y), label, Fade(kHintLabel, a), s);
    return x + (float)strlen(label) * cw + Snap(cw * 2.0f);
}
// draws prompts left to right from x while they fit before `right`; stops at the first that does not
struct HintItem { const char* keys; const char* label; };
inline void HintRow(float x, float y, float right, const HintItem* items, int n, float cw, float lh, float s, float a) {
    for (int i = 0; i < n; ++i) {
        if (x + HintW(items[i].keys, items[i].label, cw, s) - Snap(cw * 2.0f) > right) break;
        x = Hint(x, y, items[i].keys, items[i].label, cw, lh, s, a);
    }
}

}  // namespace menu_style
