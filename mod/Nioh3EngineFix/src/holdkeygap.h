// holdkeygap.h - HoldKeyGap ([General] HoldKeyGap = 0 by default; human #120716 / #120730, notes/hold-re-120730.md).
//
// Mode 3 = THE FIX (held attack -> martial art "heavy first" at high fps under load). The MA branch key of a held windup
// (key time ~frame 12) has the input condition "button held >= 12" (key cond: button 1, type 0, arg 12; checked by
// 0x20CF68 -> input vtable +0xB0 0x13507C -> 0x1350B4 -> held getter vtable +0x140 = 0x901BA4). The held getter returns
// frames_held * 60 / fps_setting ([settings+0x614], e.g. 120). Below the fps setting (CPU load, uneven frames) that
// under-counts the real hold: at ~90 fps a 220 ms hold reads 11, the key fails once and the MA waits for the next key
// (~frame 28, ~510 ms) - the player sees a heavy swing first. At 60 setting == real fps, so it never misses.
// Mode 3 patches the held getter of the input class (vtable slot, verified == 0x901BA4) to return
// max(stock, real seconds held * 60), real seconds tracked per input object and button once per frame. No button or move ids.
// Live (Main PC, keyboard, REN_FPS 120 + 12 CPU burners, 16 holds): OFF 7/16 and 8/16 long windups, ON 0/16.
//
// Mode 1 (gap fill on the key scan 0x20DFC0) was the first theory and is DISPROVEN live (254k scans, 0 gaps); it rewrites the
// key-scan step, do not use. Mode 2 = trace / gate probe for RE (logs HKGT/HKGG/HKGK/HKGH lines), do not use.
#pragma once
#include <cstdint>

namespace holdkeygap {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;  // [General] HoldKeyGap (default 0)
    uintptr_t base = 0;                // Nioh3.exe image base
    bool knownBuild = false;
};

constexpr uint32_t kScanRva = 0x20DFC0;  // v2.02 key scan (rvas.json)
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

// Mode 3 pure rules (unit-tested, used by holdkeygap.cpp). Real seconds a button is held after one frame: c = the game's
// frames-held count now, lastCnt = at the previous frame, d = real seconds since the previous frame, dt = EMA frame time.
constexpr float kMinHeldFps = 15.0f;  // slowest real frame rate the held clock follows (GH #149)
inline float NextHeldSec(float heldSec, uint32_t c, uint32_t lastCnt, float d, float dt) {
    if (c == 0) return 0.0f;
    if (c < lastCnt || lastCnt == 0) heldSec = (float)c * (dt > 0.0f ? dt : 1.0f / 60.0f);  // new press
    else if (d > 0.0f && d < 0.25f) heldSec += d;
    // At most kMinHeldFps worth per counted frame (pauses). Was 30 fps: below 30 real fps (1% lows, CPU-bound PCs) a
    // 200 ms hold capped at 5 frames * 33 ms = 10 of 12 and the martial art still started late (GH #149).
    if (heldSec > (float)c / kMinHeldFps) heldSec = (float)c / kMinHeldFps;
    return heldSec;
}
// Held getter result: max(stock, real hold in 1/60 s). Stock when the press is not tracked yet or the count went back.
inline int HeldReal(int stock, float heldSec, uint32_t c, uint32_t lastCnt) {
    if (c == 0 || lastCnt == 0 || c < lastCnt) return stock;
    const float real = heldSec * 60.0f * (float)c / (float)lastCnt;
    if (!(real > (float)stock + 0.5f) || !(real < 100000.0f)) return stock;
    const int r = (int)real;
    return r > stock ? r : stock;
}

// Tracker slot for a new input object (GH #147): first free slot (n < cap), else the entry idle longest if it has not
// been ticked for at least staleTicks (an input object freed by a load / respawn / despawned enemy), else -1. Before #147
// the table never recycled: after 32 input objects of the patched class the player's new input object (after a load or
// respawn) went untracked and the fix silently fell back to stock for the rest of the session.
inline int PickTracker(const int64_t* lastTick, int n, int cap, int64_t now, int64_t staleTicks) {
    if (n < cap) return n;
    int best = -1;
    int64_t bestTick = 0;
    for (int i = 0; i < cap; ++i) {
        if (now - lastTick[i] < staleTicks) continue;
        if (best < 0 || lastTick[i] < bestTick) { best = i; bestTick = lastTick[i]; }
    }
    return best;
}

}  // namespace holdkeygap
