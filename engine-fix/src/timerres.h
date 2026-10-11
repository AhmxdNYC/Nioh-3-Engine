// timerres.h - TR: hold the 1 ms Windows timer resolution once instead of the game's per-loop flapping (1.7.45,
// default off). notes\game-logic-audit.md section 3.
//
// The game's HID pad-reader threads (HidReaderThread0/1/2 AEF900 / AF07D0 / AF14E0, TIME_CRITICAL) wrap every
// sleep of their poll loop in timeBeginPeriod(g_HidTimerPeriod = 1) / timeEndPeriod(1), e.g. AF07D0:
//     for (;;) { poll; timeBeginPeriod(1); Sleep(4); timeEndPeriod(1); }
// That is ~200-500 timer-resolution changes per second (each one a NtSetTimerResolution call when it changes the
// process's minimum), and since Windows 10 2004 the resolution is per process: between timeEndPeriod and the next
// timeBeginPeriod every other game thread's Sleep(n) / timed wait can fall back to the default 15.6 ms granularity
// unless something else in the process holds a period (the main thread's Sleep(1) resource waits 111064 / 1110C8,
// the WinMain pump's timed MsgWait, loader polls). Not measured yet: the TR stats line gives the call rate.
//
// TR holds one timeBeginPeriod(1) for the whole session and turns the game's own begin/end pairs (period >= 1) into
// no-ops, balanced per thread, so the resolution is 1 ms all the time and nothing flaps. Same Sleep(4) duration for
// the HID threads (they asked for 1 ms); other threads only ever get the 1 ms the HID threads already set most of the
// time. Under Wine/Proton timeBeginPeriod changes nothing (the counters still show the game's call rate).
//   [Patches] TR = 0 | 1 | 2   (0 = nothing; 1 = hooks + hold ON; 2 = hooks installed, switch OFF = stock calls,
//                               counters only). Live menu row; the stats line prints the call rate either way.
#pragma once
#include <cstdint>

namespace timerres {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;
    uintptr_t base = 0;  // Nioh3.exe image base (its WINMM import slots get the hooks)
};
// Reads [Patches] TR and installs the two IAT hooks (no-op with TR = 0). Call after the image is decrypted.
void Start(const StartInfo& info);
// Live switch (nullptr when not installed): 0 = the game's calls go through, 1 = hold 1 ms + skip them.
volatile uint32_t* Switch();
// Brings the hold in line with the switch (the hooks do it too; the stats tick calls this for idle HID threads).
void Sync();
// One status line for the 30 s stats log ("" when not installed). secs = seconds since the previous call.
void StatusLine(char* buf, unsigned n, unsigned secs);

}  // namespace timerres
