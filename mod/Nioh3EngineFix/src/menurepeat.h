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
// #137566: only digital buttons repeat. The left stick and the triggers are analog: screens like the map pan / zoom with
// them, and a repeat release there changed the pan speed (PC2 live: 104-108 px/s with repeat vs 46-47 px/s without).
// So the stick and triggers always pass through untouched; the D-pad and bumpers keep the menu cursor moving.
constexpr uint32_t kRepeatable = kDUp | kDDown | kDLeft | kDRight | kLB | kRB;
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

// #137478: a menu that keeps game time running (shrine, blacksmith) raises neither the menu getters nor the stop flags, but it
// takes control from the player: the game's control-off manager (menu.cpp ReadControlOff, manager+0x34 = OR of the active
// reasons' masks) is nonzero. Debounced like the menu signal: on after 120 ms continuous, off after 150 ms clear (one-frame
// script blips never arm the repeat; gameplay has no active reason).
struct Debounce {
    uint64_t since = 0, lastOn = 0;
    bool shown = false;
    bool Update(bool now, uint64_t t) {
        if (now) {
            if (!since) since = t ? t : 1;
            lastOn = t;
        } else {
            since = 0;
        }
        shown = (since && t - since >= 120) || (shown && t - lastOn < 150);
        return shown;
    }
};
// human 2026-11-27: boss fights keep two control-off reasons the whole fight while the player plays (9C mask 1800, 9F mask 2040,
// aggregate 3840 in every Boss Match of the log). Counting them as a menu turned a held left stick into D-pad pulses mid fight and
// fired soul cores / summon seals on their own. Only bits outside these count.
constexpr uint32_t kFightCtlMask = 0x3840u;
inline bool ControlOff(bool readOk, uint32_t aggregate) { return readOk && (aggregate & ~kFightCtlMask) != 0; }

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

// #138311 (human #138299): the left stick works like the D-pad in game menus. The stick itself is never changed (the map
// pans with it, #137566); instead, while a stick direction is held past the same first delay, the matching D-pad button is
// pressed for a short pulse once per period, so the menu cursor keeps moving like a held D-pad. A real D-pad press of the
// same direction wins (no extra pulse). Only the dominant axis counts (no diagonal pulses). Off outside game menus.
// #138487 (human #138485): the game ignores the stick in its lists, so waiting the first delay before the first pulse
// made every new direction stall 0.4 s (flick up -> down = a dead gap). A fresh tilt now pulses at once, like a D-pad
// press, then repeats after the first delay. A direction already held when the menu opens still waits the full delay.
struct StickBit {
    uint64_t downSince = 0, nextPulse = 0, pressUntil = 0;
    bool pressSeen = false;  // the current pulse was reported pressed on at least one poll
    bool relSeen = true;     // a released poll followed the last pulse
};
struct StickState {
    StickBit b[4];           // up, down, left, right
    bool wasActive = false;
    uint32_t stale = 0;      // stick bits held since before the menu opened (no instant pulse until released once)
    uint32_t pulses = 0;
};
// stick = kSU|kSD|kSL|kSR bits already reduced to the dominant axis; dpad = the real D-pad bits held now.
// Returns the D-pad bits to OR into the pad for THIS poll.
inline uint32_t StickStep(StickState& s, uint32_t stick, uint32_t dpad, bool active, uint64_t t, int speed) {
    static const uint32_t kIn[4] = {kSU, kSD, kSL, kSR}, kOut[4] = {kDUp, kDDown, kDLeft, kDRight};
    if (!active || !s.wasActive) {
        for (StickBit& x : s.b) x = StickBit{};
        s.wasActive = active;
        s.stale = active ? stick : 0;
        if (!active) return 0;
    }
    s.stale &= stick;
    const uint64_t period = (uint64_t)PeriodMs(speed), pulse = period / 3 < 34 ? period / 3 : 34;
    uint32_t out = 0;
    for (int i = 0; i < 4; ++i) {
        StickBit& x = s.b[i];
        if (!(stick & kIn[i]) || (dpad & kOut[i])) { x = StickBit{}; continue; }
        if (!x.downSince) {
            x.downSince = t ? t : 1;
            x.nextPulse = t + kDelayMs;
            if (!(s.stale & kIn[i])) {  // #138487: fresh tilt = instant first step
                x.pressUntil = t + (pulse ? pulse : 1);
                x.pressSeen = true;
                ++s.pulses;
                out |= kOut[i];
            }
            continue;
        }
        if (x.pressUntil) {
            if (t < x.pressUntil || !x.pressSeen) { x.pressSeen = true; out |= kOut[i]; continue; }
            x.pressUntil = 0; x.relSeen = false;  // this poll is released
            continue;
        }
        if (!x.relSeen) { x.relSeen = true; }
        if (t >= x.nextPulse && x.relSeen) {
            x.pressUntil = t + (pulse ? pulse : 1);
            x.pressSeen = true;
            x.nextPulse = t + period;
            ++s.pulses;
            out |= kOut[i];
        }
    }
    return out;
}
// dominant-axis reduction of the stick bits (|x| vs |y|)
inline uint32_t StickDominant(uint32_t raw, int lx, int ly) {
    uint32_t st = raw & (kSU | kSD | kSL | kSR);
    const int ax = lx < 0 ? -lx : lx, ay = ly < 0 ? -ly : ly;
    if (ay >= ax) st &= (kSU | kSD); else st &= (kSL | kSR);
    return st;
}

}  // namespace menurepeat
