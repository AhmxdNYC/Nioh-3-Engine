// menu_style.h - one palette and a few drawing helpers shared by every in-game mod menu panel drawn through the OSD
// (EngineFix menu, the hosted Boss Rematch / Practice / Training side panel, corner notes). Drawing only: no state, no input.
// Colours are 0xAABBGGRR like osd::DrawRect / DrawText. Every helper takes a fade factor (0..1) for the short open fade-in.
#pragma once
#include <cstdint>
#include <cstring>
#include "osd.h"

namespace menu_style {

// ---- palette: dark slate panel, warm gold accent (matches the game's own UI gold) -------------------------------------
constexpr uint32_t kPanelBg     = 0xD8140F0C;  // body: near-black slate, translucent
constexpr uint32_t kPanelBgSide = 0xC0140F0C;  // side panel: lighter so the arena shows through
constexpr uint32_t kTitleBg     = 0xF01C1A16;  // title bar
constexpr uint32_t kFooterBg    = 0x90000000;  // footer band (hint line)
constexpr uint32_t kBorder      = 0x70506070;  // 1 px outline
constexpr uint32_t kAccent      = 0xFF5AC8F0;  // gold: top line, selected stripe, section headers
constexpr uint32_t kAccentSoft  = 0x385AC8F0;  // gold wash behind the selected row
constexpr uint32_t kDivider     = 0x38FFFFFF;
constexpr uint32_t kTitleText   = 0xFF8CDCF8;  // pale gold
constexpr uint32_t kText        = 0xFFF0F0F0, kTextDim = 0xFF8C8C8C, kTextMuted = 0xFFB4B4B4;
constexpr uint32_t kOnBg  = 0xF03C8C30, kOnText  = 0xFFFFFFFF;   // green pill
constexpr uint32_t kOffBg = 0xC0403A38, kOffText = 0xFFB4B4B4;   // grey pill
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

// panel body with a 1 px outline and a 2 px gold line on top
inline void Panel(float x, float y, float w, float h, float s, uint32_t bg, float a) {
    const float px = s < 1.0f ? 1.0f : (float)(int)(s * 0.75f + 0.5f);
    osd::DrawRect(x, y, w, h, Fade(bg, a));
    osd::DrawRect(x, y, w, px, Fade(kBorder, a));
    osd::DrawRect(x, y + h - px, w, px, Fade(kBorder, a));
    osd::DrawRect(x, y + px, px, h - 2 * px, Fade(kBorder, a));
    osd::DrawRect(x + w - px, y + px, px, h - 2 * px, Fade(kBorder, a));
    osd::DrawRect(x, y, w, 2 * px, Fade(kAccent, a));
}

// selected row: soft gold wash plus a solid stripe on the left edge
inline void SelectBar(float x, float y, float w, float h, float s, float a) {
    osd::DrawRect(x, y, w, h, Fade(kAccentSoft, a));
    osd::DrawRect(x, y, s < 1.0f ? 2.0f : 3.0f * s, h, Fade(kAccent, a));
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
inline bool PillColors(const char* v, uint32_t& bg, uint32_t& fg) {
    if (!strcmp(v, "ON") || !strcmp(v, "IDLE")) { bg = kOnBg; fg = kOnText; return true; }
    if (!strcmp(v, "OFF") || !strcmp(v, "off") || !strcmp(v, "CPU")) { bg = kOffBg; fg = kOffText; return true; }
    if (!strcmp(v, "RPT") || !strcmp(v, "IDLE (waiting)")) { bg = kRptBg; fg = kRptText; return true; }
    if (!strcmp(v, "?") || !strcmp(v, "CPU (idle failed)")) { bg = kUnkBg; fg = kUnkText; return true; }
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

// one button prompt: key cap + label; returns the x after it (with a gap)
inline float Hint(float x, float y, const char* key, const char* label, float cw, float lh, float s, float a) {
    const float kw = (float)strlen(key) * cw;
    osd::DrawRect(x, y - s, kw + cw, lh - s, Fade(kChipBg, a));
    osd::DrawText(x + cw * 0.5f, y, key, Fade(kChipText, a), s);
    x += kw + cw * 1.5f;
    osd::DrawText(x, y, label, Fade(kTextMuted, a), s);
    return x + (float)strlen(label) * cw + cw * 1.5f;
}
// width in glyph columns of a Hint(key, label) incl. its trailing gap
inline float HintCols(const char* key, const char* label) { return (float)strlen(key) + 3.0f + (float)strlen(label); }

}  // namespace menu_style
