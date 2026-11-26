// menurepeat.h - hold-to-repeat in the game's own menus (TASK #137071, human #137069).
// The game moves its menu cursor once per press: holding the D-pad, the left stick, a bumper or a trigger does not keep
// it moving. While the game's own "a menu is open" signal is on (menu.cpp PauseDetect: the getters its frame generation
// pauses on, or the stop flags of the pause menu) and the mod menu is closed, a held button gets a one-poll release
// after the first delay and then once per period, so the game sees a fresh press each time - the same delay/rate as the
// mod menu's own repeat (400 ms, then every 90 ms at Normal). Nothing is added: the pad only ever reports what the
// player holds, minus a short release. Off in gameplay (signal off), so held D-pad / bumpers there are untouched.
// Universal: no per-menu list. Header-only, no Windows types, so tests/menurepeat_test.cpp checks it offline.
#pragma once
#include <cstdint>

namespace menurepeat {

// bits in the caller's "raw" word: XInput wButtons (low 16) plus the menu.cpp extras (B_SU..B_RT)
constexpr uint32_t kDUp = 0x0001, kDDown = 0x0002, kDLeft = 0x0004, kDRight = 0x0008, kLB = 0x0100, kRB = 0x0200;
constexpr uint32_t kSU = 1u << 16, kSD = 1u << 17, kSL = 1u << 18, kSR = 1u << 19, kLT = 1u << 20, kRT = 1u << 21;
constexpr uint32_t kRepeatable = kDUp | kDDown | kDLeft | kDRight | kLB | kRB | kSU | kSD | kSL | kSR | kLT | kRT;
constexpr int kBits = 22;

// [Menu] GameMenuRepeatSpeed: 0 Slow, 1 Normal (= mod menu), 2 Fast
constexpr int kDelayMs = 400;
inline int PeriodMs(int speed) { return speed <= 0 ? 150 : speed >= 2 ? 50 : 90; }
inline const char* SpeedName(int speed) { return speed <= 0 ? "Slow" : speed >= 2 ? "Fast" : "Normal"; }

struct Bit {
    uint64_t downSince = 0;  // 0 = not held (or held from before the menu opened and not yet seen)
    uint64_t nextRep = 0;
    uint64_t relUntil = 0;   // a release window is running until this time
    bool pressSeen = true;   // the game saw the button pressed again since the last release
};
struct State {
    Bit b[kBits];
    bool wasActive = false;
    uint32_t repeats = 0;    // releases injected (log / tests)
};

// One pad poll. raw = what the player holds now; active = a game menu is open and the mod menu is closed.
// Returns the bits to report as released for THIS poll (0 = pass the pad through unchanged).
inline uint32_t Step(State& s, uint32_t raw, bool active, uint64_t t, int speed) {
    if (!active) {
        if (s.wasActive)
            for (Bit& x : s.b) x = Bit{};
        s.wasActive = false;
        return 0;
    }
    if (!s.wasActive) {  // menu just opened: a button already held counts from now (full first delay, never an instant repeat)
        for (Bit& x : s.b) x = Bit{};
        s.wasActive = true;
    }
    const uint64_t period = (uint64_t)PeriodMs(speed), pulse = period / 3 < 34 ? period / 3 : 34;
    uint32_t release = 0;
    for (int i = 0; i < kBits; ++i) {
        const uint32_t m = 1u << i;
        if (!(kRepeatable & m)) continue;
        Bit& x = s.b[i];
        if (!(raw & m)) {
            x = Bit{};
            continue;
        }
        if (!x.downSince) {
            x.downSince = t ? t : 1;
            x.nextRep = t + kDelayMs;
            continue;
        }
        if (x.relUntil) {
            if (t < x.relUntil) {
                release |= m;
                continue;
            }
            x.relUntil = 0;  // window over: this poll reports the button pressed again
            x.pressSeen = true;
            continue;
        }
        if (t >= x.nextRep && x.pressSeen) {
            x.relUntil = t + (pulse ? pulse : 1);  // this poll is always released, so even one poll per window works
            x.pressSeen = false;
            x.nextRep = t + period;
            ++s.repeats;
            release |= m;
        }
    }
    return release;
}

}  // namespace menurepeat
