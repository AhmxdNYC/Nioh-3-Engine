// inputhist.h - SF6-style INPUT HISTORY data (human #38224/#38275/#38280): a short ring of "held state" entries, each one the direction
// (8-way + neutral) and the buttons held, with the time it started. The drawing is in inputhist_draw.h (osd.cpp).
// Read-only on the game's input: padviz::Feed (menu.cpp PadFeed, the raw XInputGetState result before any menu masking) calls Push() on
// the game input thread; the Present thread reads with Snapshot(). Nothing here writes game state, save data or files; the only
// persistence is the [Menu] ini keys the menu already saves (InputDisplay / InputDisplaySide / InputDisplayRows). Header-only,
// no dependencies (so tests/inputhist_test.cpp can build it alone).
#pragma once
#include <atomic>
#include <cmath>
#include <cstdint>

namespace inputhist {

// ---- settings (menu.cpp: ini + menu rows) ----
inline volatile int g_on = 0;    // default OFF
inline volatile int g_side = 0;  // 0 left (below the FPS overlay), 1 right
inline volatile int g_rows = 20; // visible history rows, kMinRows..kMaxRows (human: longer history, was 12 / max 24)
constexpr int kMinRows = 4, kMaxRows = 32;

constexpr int kCap = 40;  // ring capacity (>= kMaxRows + 1 so the oldest visible row always has its end time)

enum Dir : uint8_t { N = 0, U, UR, R, DR, D, DL, L, UL };  // numpad-style order, clockwise from up
enum Btn : uint16_t { bA = 1, bB = 2, bX = 4, bY = 8, bLB = 16, bRB = 32, bLT = 64, bRT = 128 };

struct Entry {
    uint8_t dir = N;
    uint16_t btn = 0;
    uint8_t dpad = 0; // raw XInput directional buttons, separate from left-stick movement
    uint64_t t0 = 0;  // ms when this state started
};

inline Entry g_ring[kCap];
inline int g_n = 0;       // entries stored (<= kCap)
inline int g_next = 0;    // next write slot
inline std::atomic_flag g_lock = ATOMIC_FLAG_INIT;
inline void Lock() { while (g_lock.test_and_set(std::memory_order_acquire)) {} }
inline void Unlock() { g_lock.clear(std::memory_order_release); }

// stick deadzone for the direction (left stick, radial, of 32767) - the D-pad wins when it is pressed
constexpr int kStickDead = 14000;
constexpr int kTrigDown = 100;  // analogue trigger value (0..255) counted as pressed

inline uint8_t DirOf(uint32_t buttons, int lx, int ly) {
    int h = 0, v = 0;  // +h right, +v up
    if (buttons & 0x0008) ++h;
    if (buttons & 0x0004) --h;
    if (buttons & 0x0001) ++v;
    if (buttons & 0x0002) --v;
    if (!h && !v && (long long)lx * lx + (long long)ly * ly >= (long long)kStickDead * kStickDead) {
        const double a = std::atan2((double)ly, (double)lx) * 180.0 / 3.14159265358979;  // 0 = right, 90 = up
        const int oct = ((int)std::floor((a + 22.5) / 45.0) + 8) % 8;                    // 0 R, 1 UR, 2 U, 3 UL, 4 L, 5 DL, 6 D, 7 DR
        static const uint8_t k[8] = {R, UR, U, UL, L, DL, D, DR};
        return k[oct];
    }
    static const uint8_t t[3][3] = {{DL, D, DR}, {L, N, R}, {UL, U, UR}};  // [v + 1][h + 1]
    return t[v + 1][h + 1];
}
inline uint16_t BtnOf(uint32_t buttons, int lt, int rt) {
    uint16_t m = 0;
    if (buttons & 0x1000) m |= bA;
    if (buttons & 0x2000) m |= bB;
    if (buttons & 0x4000) m |= bX;
    if (buttons & 0x8000) m |= bY;
    if (buttons & 0x0100) m |= bLB;
    if (buttons & 0x0200) m |= bRB;
    if (lt >= kTrigDown) m |= bLT;
    if (rt >= kTrigDown) m |= bRT;
    return m;
}

// one accepted pad sample (XInput layout); a new entry starts whenever the direction or the held buttons change
inline void Push(uint32_t buttons, int lx, int ly, int lt, int rt, uint64_t nowMs) {
    const uint8_t d = DirOf(0, lx, ly);
    const uint8_t dp = (uint8_t)(buttons & 15);
    const uint16_t b = BtnOf(buttons, lt, rt);
    Lock();
    const int last = (g_next + kCap - 1) % kCap;
    if (g_n == 0 || g_ring[last].dir != d || g_ring[last].btn != b || g_ring[last].dpad != dp) {
        g_ring[g_next] = {d, b, dp, nowMs};
        g_next = (g_next + 1) % kCap;
        if (g_n < kCap) ++g_n;
    }
    Unlock();
}

// newest first; out[i].t0 = start; returns the count (<= max). Durations: the caller uses the next newer entry's t0 (or now).
inline int Snapshot(Entry* out, int max) {
    Lock();
    const int n = g_n < max ? g_n : max;
    for (int i = 0; i < n; ++i) out[i] = g_ring[(g_next + kCap - 1 - i) % kCap];
    Unlock();
    return n;
}

// frames (60 Hz game frames) between two ms stamps, 1..99 for the two-digit column
inline int Frames(uint64_t t0, uint64_t t1) {
    const uint64_t dt = t1 > t0 ? t1 - t0 : 0;
    const uint64_t f = (dt * 60 + 500) / 1000;
    return f > 99 ? 99 : f < 1 ? 1 : (int)f;
}

}  // namespace inputhist
