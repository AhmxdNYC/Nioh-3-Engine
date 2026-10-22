// padviz.h - live controller INPUT DISPLAY (user #22952/#22956): a small pad graphic whose buttons, sticks and triggers light up as the
// player presses them. Header-only like menu_style.h. Two halves:
//   * Feed(): menu.cpp PadFeed (the game's XInputGetState IAT hook, or the menu's own polling) stores the raw pad state BEFORE the menu
//     masks anything, so the graphic shows what the hand does, not what the game is allowed to see. Game input thread.
//   * Draw(): osd.cpp BuildOverlay (Present-hook thread) draws it as its own quad group (own text scale, so the face-button letters
//     stay crisp at any size) with the same primitives as the menu badges (menu_style.h): Xbox A/B/X/Y or PlayStation Cross/Circle/
//     Square/Triangle follow [Menu] ButtonStyle (menu_style::g_btnStyle). Settings: [Menu] ControllerDisplay / ControllerDisplaySize /
//     ControllerDisplayOpacity (menu.cpp reads the ini and the menu rows change the globals below live). Nothing is drawn while off or
//     when the game has not polled a pad for 1.5 s (pad unplugged / loading).
// Layout is in font-texel units (kW x kH) multiplied by the group scale. Colours 0xAABBGGRR like osd::DrawRect.
#pragma once
#include <cmath>
#include <cstdint>
#include "menu_style.h"

namespace padviz {

enum : uint32_t {  // XInput wButtons bits
    kUp = 0x0001, kDown = 0x0002, kLeft = 0x0004, kRight = 0x0008, kStart = 0x0010, kBack = 0x0020, kL3 = 0x0040, kR3 = 0x0080,
    kLB = 0x0100, kRB = 0x0200, kA = 0x1000, kB = 0x2000, kX = 0x4000, kY = 0x8000
};
constexpr int kW = 160, kH = 104;  // graphic size in texel units (x scale = pixels)

struct State {
    uint32_t buttons = 0;
    int lx = 0, ly = 0, rx = 0, ry = 0;  // -32768..32767, +y = up
    int lt = 0, rt = 0;                   // 0..255
};

// ---- settings (menu.cpp: ini + menu rows) ----
inline volatile int g_on = 0;
inline volatile int g_size100 = 100;      // size x100: 50..400
inline volatile int g_opacity100 = 80;    // opacity x100: 20..100

// ---- feed ----
inline volatile uint32_t g_buttons = 0;
inline volatile int g_lx = 0, g_ly = 0, g_rx = 0, g_ry = 0, g_lt = 0, g_rt = 0, g_slot = 0;
inline volatile uint64_t g_at = 0;  // ms of the last accepted feed (0 = never)

// one XInputGetState result for pad `slot`. The newest of the lowest-numbered live slot wins (the game polls all four while it scans
// for the active pad; an unplugged slot is never fed), so two pads on one console do not flicker the graphic.
inline void Feed(int slot, uint32_t buttons, int lx, int ly, int rx, int ry, int lt, int rt, uint64_t nowMs) {
    if (!g_on) return;
    if (slot > g_slot && g_at && nowMs - g_at <= 500) return;
    g_slot = slot;
    g_buttons = buttons; g_lx = lx; g_ly = ly; g_rx = rx; g_ry = ry; g_lt = lt; g_rt = rt;
    g_at = nowMs;
}
inline bool Live(uint64_t nowMs) { return g_on && g_at && nowMs - g_at <= 1500; }
inline State Snapshot() {
    State s;
    s.buttons = g_buttons; s.lx = g_lx; s.ly = g_ly; s.rx = g_rx; s.ry = g_ry; s.lt = g_lt; s.rt = g_rt;
    return s;
}

// group text scale: pixels per texel, 0.5 steps (1080p: size 1 -> 1, 2 -> 2), 0.5 .. 8
inline float Scale(unsigned H) {
    float s = (float)H / 1080.0f * (float)g_size100 / 100.0f;
    s = std::floor(s * 2.0f + 0.5f) / 2.0f;
    return s < 0.5f ? 0.5f : s > 8.0f ? 8.0f : s;
}
inline float Opacity() { return (float)g_opacity100 / 100.0f; }

// ---- drawing ----
constexpr uint32_t kBody = 0xB0140F0C, kUnlit = 0xC0302A26, kWell = 0xD0100C0A, kUnlitText = 0xFF787878,
                   kLitCap = 0xF05AC8F0;  // lit bumper: gold cap + white label (dark label on a light cap outlines into a blob, "LB" unreadable)
constexpr uint32_t kPsFill = 0xC0201C18, kPsFillLit = 0xF0685A4E;

inline void Disc(float ox, float oy, float s, float cx, float cy, float d, uint32_t c) {
    menu_style::RoundRect(ox + (cx - d * 0.5f) * s, oy + (cy - d * 0.5f) * s, d * s, d * s, d * s * 0.5f, c);
}
inline void Box(float ox, float oy, float s, float x, float y, float w, float h, uint32_t c) {
    osd::DrawRect(menu_style::Snap(ox + x * s), menu_style::Snap(oy + y * s), menu_style::Snap(w * s), menu_style::Snap(h * s), c);
}

inline void Stick(float ox, float oy, float s, float cx, float cy, int ax, int ay, bool click, float a) {
    using namespace menu_style;
    const float dx = (float)ax / 32768.0f, dy = (float)ay / 32768.0f;
    const bool active = std::sqrt(dx * dx + dy * dy) > 0.24f;
    Disc(ox, oy, s, cx, cy, 28, Fade(kWell, a));
    if (active) Disc(ox, oy, s, cx, cy, 28, Fade(kAccentSoft, a));
    const uint32_t k = click ? 0xFFFFFFFFu : active ? kAccent : 0xFF8C8C8Cu;
    Disc(ox, oy, s, cx + dx * 7.0f, cy - dy * 7.0f, 14, Fade(k, a));
}

// one face button. k = 'A' 'B' 'X' 'Y' (A = bottom: Xbox A / PlayStation Cross, B = right: Circle, X = left: Square, Y = top: Triangle)
inline void Face(float ox, float oy, float s, float cx, float cy, char k, bool on, int style, float a) {
    using namespace menu_style;
    const float x = ox + (cx - 8.0f) * s, y = oy + (cy - 8.0f) * s;
    if (style) {
        const uint32_t sym = k == 'A' ? 0xFFF0A050 : k == 'B' ? 0xFF5050F0 : k == 'X' ? 0xFFC888F0 : 0xFF80D060;
        Disc(ox, oy, s, cx, cy, 16, Fade(on ? kPsFillLit : kPsFill, a));
        PsSymbol(x, y, 16.0f * s, k, Fade(on ? sym : (sym & 0x00FFFFFFu) | 0x58000000u, a));
        return;
    }
    const uint32_t bg = k == 'A' ? kBtnA : k == 'B' ? kBtnB : k == 'X' ? kBtnX : kBtnY;
    Disc(ox, oy, s, cx, cy, 16, Fade(on ? bg : kUnlit, a));
    const char t[2] = {k, 0};
    osd::DrawText(Snap(x + 4.0f * s), Snap(y + 0.5f * s), t, Fade(on ? (k == 'Y' ? kCapText : 0xFFFFFFFFu) : kUnlitText, a), s);
}

inline void Bump(float ox, float oy, float s, float x, float y, bool right, bool on, int style, float a) {
    using namespace menu_style;
    RoundRect(ox + x * s, oy + y * s, 44.0f * s, 16.0f * s, 4.0f * s, Fade(on ? kLitCap : kBumperBg, a));
    const char* t = style ? (right ? "R1" : "L1") : (right ? "RB" : "LB");
    osd::DrawText(Snap(ox + (x + 14.0f) * s), Snap(oy + y * s), t, Fade(on ? 0xFFFFFFFFu : 0xFFB4B4B4u, a), s);
}

inline void Trig(float ox, float oy, float s, float x, int v, int style, float a) {
    using namespace menu_style;
    (void)style;
    Box(ox, oy, s, x, 0, 34, 8, Fade(kBody, a));
    const float w = 34.0f * (float)(v < 0 ? 0 : v > 255 ? 255 : v) / 255.0f;
    if (w >= 1.0f) Box(ox, oy, s, x, 0, w, 8, Fade(kAccent, a));
}

// draws the whole graphic with its top-left at (x, y) pixels; s = Scale(H); a = opacity; style 0 Xbox / 1 PlayStation
inline void DrawState(const State& p, float x, float y, float s, float a, int style) {
    using namespace menu_style;
    const uint32_t b = p.buttons;
    Trig(x, y, s, 10, p.lt, style, a);
    Trig(x, y, s, 116, p.rt, style, a);
    Bump(x, y, s, 6, 10, false, (b & kLB) != 0, style, a);
    Bump(x, y, s, 110, 10, true, (b & kRB) != 0, style, a);
    RoundRect(x, y + 28.0f * s, (float)kW * s, (float)(kH - 28) * s, 12.0f * s, Fade(kBody, a));
    Stick(x, y, s, 36, 54, p.lx, p.ly, (b & kL3) != 0, a);
    Stick(x, y, s, 102, 82, p.rx, p.ry, (b & kR3) != 0, a);
    // D-pad: dim cross, each lit arm over it
    const float dpx = 62, dpy = 82;
    Box(x, y, s, dpx - 4, dpy - 12, 8, 24, Fade(kUnlit, a));
    Box(x, y, s, dpx - 12, dpy - 4, 24, 8, Fade(kUnlit, a));
    if (b & kUp) Box(x, y, s, dpx - 4, dpy - 12, 8, 9, Fade(kAccent, a));
    if (b & kDown) Box(x, y, s, dpx - 4, dpy + 3, 8, 9, Fade(kAccent, a));
    if (b & kLeft) Box(x, y, s, dpx - 12, dpy - 4, 9, 8, Fade(kAccent, a));
    if (b & kRight) Box(x, y, s, dpx + 3, dpy - 4, 9, 8, Fade(kAccent, a));
    // Back / Start
    RoundRect(x + 68.0f * s, y + 47.0f * s, 12.0f * s, 6.0f * s, 3.0f * s, Fade((b & kBack) ? kAccent : kUnlit, a));
    RoundRect(x + 86.0f * s, y + 47.0f * s, 12.0f * s, 6.0f * s, 3.0f * s, Fade((b & kStart) ? kAccent : kUnlit, a));
    Face(x, y, s, 122, 37, 'Y', (b & kY) != 0, style, a);
    Face(x, y, s, 122, 71, 'A', (b & kA) != 0, style, a);
    Face(x, y, s, 105, 54, 'X', (b & kX) != 0, style, a);
    Face(x, y, s, 139, 54, 'B', (b & kB) != 0, style, a);
}

// live entry: false = nothing drawn
inline bool Draw(float x, float y, unsigned H, uint64_t nowMs) {
    if (!Live(nowMs)) return false;
    DrawState(Snapshot(), x, y, Scale(H), Opacity(), menu_style::g_btnStyle ? 1 : 0);
    return true;
}

}  // namespace padviz
