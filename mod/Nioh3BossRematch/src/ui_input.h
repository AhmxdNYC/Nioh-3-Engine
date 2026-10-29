// Hook-owned per-controller state. UI thread hands off only atomics (open,
// owner, latch epoch); the XInput hook owns these fields. Tested without a game.
#pragma once
#include <cstdint>

namespace addon_ui_input {
constexpr uint16_t kGesture = 0x0020 | 0x0001;  // View + D-pad Up
inline uint16_t NavigationEdges(uint16_t edges) { return edges & 0x330F; }  // D-pad directions, LB/RB, A/B
inline bool OwnsInput(int owner, unsigned slot) { return owner < 0 || owner == (int)slot; }
struct Pad {
    uint16_t previous = 0;
    uint64_t since = 0;
    bool holding = false, fired = false, latch = false;
    unsigned epoch = 0;
    struct Result { uint16_t edges; bool toggle, blank; };
    Result Feed(uint16_t buttons, bool nonNeutral, uint64_t now, bool open, bool gesture, unsigned currentEpoch, uint16_t directions = 0) {
        const uint16_t nav = (uint16_t)(buttons | directions);
        const uint16_t edges = (uint16_t)(nav & ~previous);
        previous = nav;
        if (epoch != currentEpoch) { epoch = currentEpoch; latch = true; }
        if (!nonNeutral) latch = false;
        bool toggle = false;
        if (gesture && (buttons & kGesture) == kGesture) {
            if (!holding) { holding = true; fired = false; since = now; }
            else if (!fired && now - since >= 1000) { fired = true; toggle = true; latch = true; }
        } else { holding = fired = false; }
        // No menu action until the opening/closing buttons have been released.
        return {(uint16_t)(open && !latch ? edges : 0), toggle, open || latch};
    }
};
}  // namespace addon_ui_input
