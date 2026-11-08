#pragma once
// Queued soul-core press (#83466): a d-pad press made while the previous seal is still in its ghost exit (0.1-1.7 s after the form
// exit, before "despawned by itself") reaches the game masked or while it refuses a second seal, so the throw is swallowed.
// The press is queued instead, and replayed as a short d-pad pulse (through the XInputGetState hook) the first frame the old seal is
// gone and the form block is released. The old seal is NOT despawned early: its tail hits belong to the previous core.
#include <windows.h>
#include <atomic>
#include <cstdint>
namespace pressq {
constexpr ULONGLONG kWindowMs = 3000;  // a queued press older than this is dropped (the player has moved on)
constexpr ULONGLONG kPulseMs = 120;    // same length as the harness press
constexpr ULONGLONG kRetryMs = 450;    // no seal spawn within this long after a pulse: pulse again
constexpr int kMaxTries = 4;
enum class Step { Idle, Wait, Pulse, Done, Expired, GaveUp };
struct State {
    std::atomic<int> dir{0};  // 1 LEFT, 2 DOWN, 0 nothing queued
    std::atomic<ULONGLONG> queuedAt{0}, pulseAt{0};
    std::atomic<int> tries{0};
};
inline void Queue(State& s, int dir, ULONGLONG now) {
    s.pulseAt.store(0); s.tries.store(0); s.queuedAt.store(now); s.dir.store(dir);  // the newest press replaces an older queued one
}
inline void Clear(State& s) { s.dir.store(0); s.pulseAt.store(0); }
// game thread, every tick. thrown = the new seal's spawn hit was seen (g_pickLearned after the press).
template<class BeforePulse>
inline Step Tick(State& s, ULONGLONG now, bool oldSealAlive, bool blocked, bool thrown, BeforePulse beforePulse) {
    if (!s.dir.load()) return Step::Idle;
    if (thrown) { Clear(s); return Step::Done; }
    if (now - s.queuedAt.load() > kWindowMs) { Clear(s); return Step::Expired; }
    const ULONGLONG p = s.pulseAt.load();
    if (p) {
        if (now - p <= kRetryMs) return Step::Wait;
        if (s.tries.load() >= kMaxTries) { Clear(s); return Step::GaveUp; }
        s.pulseAt.store(0);  // no spawn after the last pulse: pulse again below
    }
    if (oldSealAlive || blocked) return Step::Wait;
    beforePulse(s.tries.load() == 0);  // prepare the logical press BEFORE the hook can expose its input
    s.tries.fetch_add(1); s.pulseAt.store(now);
    return Step::Pulse;
}
inline Step Tick(State& s, ULONGLONG now, bool oldSealAlive, bool blocked, bool thrown) {
    return Tick(s, now, oldSealAlive, blocked, thrown, [](bool) {});
}
// XInput hook thread: d-pad bit to OR into the pad state right now (0 = none).
inline WORD Buttons(const State& s, ULONGLONG now) {
    const int d = s.dir.load();
    const ULONGLONG p = s.pulseAt.load();
    if (!d || !p || now < p || now - p >= kPulseMs) return 0;
    return d == 1 ? 0x0004 : 0x0002;  // XINPUT_GAMEPAD_DPAD_LEFT / DPAD_DOWN
}
}
