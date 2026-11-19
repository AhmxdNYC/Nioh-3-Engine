// holdkeygap.h - HoldKeyGap ([General] HoldKeyGap = 0 by default; human #120716 / #120730, notes/hold-re-120730.md).
//
// The move's branch/cancel key scan 0x20DFC0(this, moveRes, t, slot) fires a key at time k when t <= k < t + step, with
// t = current anim time (1/60 s units) and step = [this + 0x40 + slot*4] (60/fps). Each frame tests [t_n, t_n + s_n) but
// time then advances by the next frame's step, so with uneven frame times (s_{n+1} > s_n) a gap [t_n + s_n, t_{n+1}) is
// never tested and a key inside it is skipped (kb-118263: 11.82 -> 12.48 skips the frame-12 martial-art branch of a held
// attack; the move plays out as a heavy swing). Fixed dt tiles exactly, which is why 60 fps never misses.
//
// The fix is a detour on 0x20DFC0 that remembers each (this, slot, moveRes)'s last window end and, when the next call on the
// same move starts after it (a gap smaller than kMaxGap), runs the stock scan once over [prevEnd, t + step) instead: the
// window is only ever widened back to where the last one ended, never shrunk, no move or button ids. Logs "HOLDKEYGAP: ...".
#pragma once
#include <cstdint>

namespace holdkeygap {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;  // [General] HoldKeyGap (default 0)
    uintptr_t base = 0;                // Nioh3.exe image base
    bool knownBuild = false;
};

constexpr uint32_t kFnRva = 0x20DFC0;  // v2.02 key scan (rvas.json)
constexpr unsigned kStolen = 15;      // mov rax,rsp / mov [rax+8],rbx / mov [rax+18],rbp / mov [rax+20],rsi
constexpr float kMaxGap = 4.0f;       // anim frames (1/60 s); larger jumps are seeks / hitstop resumes, left stock

void Start(const StartInfo& info);
long Fills();       // windows widened since start
bool Installed();

// Pure window rule (unit-tested): given the last window end of the same move and this call's t / step, returns the window
// start to use (t when stock).
inline float WindowStart(bool sameMove, float prevEnd, float prevT, float t, float step) {
    if (!sameMove || !(step > 0.0f) || !(prevEnd < t) || !(prevT <= t) || !(t - prevEnd < kMaxGap)) return t;
    return prevEnd;
}

}  // namespace holdkeygap
