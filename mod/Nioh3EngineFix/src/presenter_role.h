// presenter_role.h - is the thread that presents the real swapchain the game, or a frame-generation pacer?
//
// GitHub #133/#134 (1.8.24): with NVIDIA DLSS Frame Generation (Streamline, plugins loaded by the driver under hashed
// names such as 1B0_E658703.dll) the real swapchain is presented by "nv.sl.dlss_g.thread.pacer". The swapchain would
// not name its queue (GetDevice(ID3D12CommandQueue) = E_NOINTERFACE behind a dxgi.dll proxy), so the overlay drew its
// first frame on the GAME's captured DIRECT queue from the pacer thread; the device was removed 16 ms later (#133,
// game fatal 0xE0000001) or the Steam overlay read a null pointer in the same Present (#134). The FSR presenter has had
// the same guard since 1.7.x, but it is found by thread name, and the DLSS pacer's name was still empty on its first
// Present in #133. The stack tells regardless of names: the game's presents come from Nioh3.exe code, a pacer thread
// starts in middleware and never runs game code.
#pragma once
#include <cstdint>

namespace presenter_role {

enum Role { Unknown = 0, Game = 1, Foreign = 2 };

struct Range {
    uintptr_t lo = 0, hi = 0;
    bool Has(uintptr_t a) const { return a >= lo && a < hi; }
};

// frames: return addresses of the presenting thread, innermost first. exe = the game image. start[]: the modules a
// thread's walk ends in when it reached the thread start (ntdll.dll, kernel32.dll).
// Game: any frame is game code. Foreign: the walk is complete (its last frame is a thread-start module) and no frame
// is game code. Unknown otherwise (empty or truncated walk): the caller keeps the old behaviour.
inline Role Classify(const uintptr_t* frames, int n, Range exe, const Range* start, int nStart) {
    if (!frames || n <= 0 || exe.hi <= exe.lo) return Unknown;
    for (int i = 0; i < n; ++i)
        if (exe.Has(frames[i])) return Game;
    for (int s = 0; s < nStart; ++s)
        if (start[s].hi > start[s].lo && start[s].Has(frames[n - 1])) return Foreign;
    return Unknown;
}

}  // namespace presenter_role
