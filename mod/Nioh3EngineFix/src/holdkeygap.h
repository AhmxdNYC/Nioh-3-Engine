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
// Mode 1 (gap fill on the key scan 0x20DFC0) was the first theory and is DISPROVEN live (254k scans, 0 gaps); kept off.
// Mode 2 = trace / gate probe for RE (logs HKGT/HKGG/HKGK/HKGH lines).
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
