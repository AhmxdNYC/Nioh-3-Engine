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
#include <cstdio>

namespace holdkeygap {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;  // [General] HoldKeyGap (default 0)
    uintptr_t base = 0;                // Nioh3.exe image base
    bool knownBuild = false;
    // GH #154: last held presses go to the report's "last mod state" (crashreport::SetState). Null = log lines only.
    void (*setState)(const char* key, const char* value) = nullptr;
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
//
// A gap of 0.25 s or more is not added (pause, or the input object was idle). Shorter gaps are real hitches and count
// in full. There is no per-count cap: GH #149 capped the whole hold at count/30, then count/15. On a 120 setting that
// cap is 4 units of 1/60 s per counted frame (66.67 ms). GH #150's log is that cap saturating ("frames 6 stock 6 real
// 24 (frame 66.67 ms)") while the game still ran at ~63 fps, so a hold whose counter lagged real time stayed under 12.
inline float NextHeldSec(float heldSec, uint32_t c, uint32_t lastCnt, float d, float dt) {
    if (c == 0) return 0.0f;
    if (c < lastCnt || lastCnt == 0) heldSec = (float)c * (dt > 0.0f ? dt : 1.0f / 60.0f);  // new press
    else if (d > 0.0f && d < 0.25f) heldSec += d;
    return heldSec;
}
// Held getter result: max(stock, real hold in 1/60 s). Stock when the press is not tracked yet or the count went back.
// Truncates: rounding a fraction up changed steady-fps counts and let 10.2/60 s reach 12 (regression review, GH #150).
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


// ---- GH #154 per-press record (diagnostic only, never changes the getter result) ----
// One record per held press of a tracked button, filled by the held getter and the per-frame tick, summarised when the
// press ends. kMaHeld is the held count the martial-art key asks for (key cond arg, header comment above); it is only
// used to stamp WHEN the stock and the fixed value first reached it.
constexpr int kMaHeld = 12;
struct PressStat {
    int calls = 0;          // held getter calls during the press
    int stockCalls = 0;     // calls answered stock because the press was not tracked yet / count went back
    int maxStock = 0, maxReal = 0;
    float stockAt = -1.0f;  // real seconds held when stock first reached kMaHeld (-1 = never)
    float fixAt = -1.0f;    // same for the value the fix returned
    float maxStep = 0.0f;   // longest real frame (s) inside the press
    float skipped = 0.0f;   // real seconds not counted (gaps >= 0.25 s)
};
inline void NoteCall(PressStat& p, int stock, int ret, float heldNow, bool fallback) {
    ++p.calls;
    if (fallback) ++p.stockCalls;
    if (stock > p.maxStock) p.maxStock = stock;
    if (ret > p.maxReal) p.maxReal = ret;
    if (stock >= kMaHeld && p.stockAt < 0.0f) p.stockAt = heldNow;
    if (ret >= kMaHeld && p.fixAt < 0.0f) p.fixAt = heldNow;
}
inline void NoteStep(PressStat& p, float d) {
    if (d > 0.0f && d < 0.25f) { if (d > p.maxStep) p.maxStep = d; }
    else if (d >= 0.25f && d <= 1.0f) p.skipped += d;
}
// A press ended between two ticks: the count went to 0 or restarted below the last one.
inline bool PressEnded(uint32_t c, uint32_t lastCnt) { return lastCnt != 0 && (c == 0 || c < lastCnt); }
// Worth a line: held at least 100 ms real (taps are not the issue) and not a menu-long hold.
inline bool PressWorthLog(float heldSec) { return heldSec >= 0.1f && heldSec <= 3.0f; }
// Fits a report state value (95 chars with the "inN " prefix). Example:
// "b2 231ms f14 12@fix 197 stock - | max 7/13 | calls 9/0 | step 21 skip 0" = button 2 held 231 ms real over 14 counted
// frames; the fixed value reached 12 at 197 ms held, stock never; max stock 7 / fixed 13; 9 getter calls, 0 of them answered
// stock because the press was not tracked yet; longest frame 21 ms; 0 ms of the hold not counted (gaps >= 250 ms).
inline int FormatPress(char* out, unsigned cap, int button, float heldSec, uint32_t frames, const PressStat& p) {
    char fx[16], st[16];
    if (p.fixAt >= 0.0f) snprintf(fx, sizeof fx, "%d", (int)(p.fixAt * 1000.0f + 0.5f)); else snprintf(fx, sizeof fx, "-");
    if (p.stockAt >= 0.0f) snprintf(st, sizeof st, "%d", (int)(p.stockAt * 1000.0f + 0.5f)); else snprintf(st, sizeof st, "-");
    return snprintf(out, cap, "b%d %dms f%u %d@fix %s stock %s | max %d/%d | calls %d/%d | step %d skip %d",
                    button, (int)(heldSec * 1000.0f + 0.5f), frames, kMaHeld, fx, st, p.maxStock, p.maxReal, p.calls, p.stockCalls,
                    (int)(p.maxStep * 1000.0f + 0.5f), (int)(p.skipped * 1000.0f + 0.5f));
}

}  // namespace holdkeygap
