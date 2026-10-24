// halfrate.h - opt-in half-rate reuse of far shadows, the GI gather, and the cloud record.
// [HalfRate] HRS / HRG / HRC, each 0 by default. Not part of any ExtraPerf preset.
//
// Skipping the record leaves the previous target in place (the clear is inside the record).
// A camera cut is an eye jump of more than 4 m between CameraBuildLookAtView samples.
#pragma once
#include <cstdint>

namespace halfrate {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;
    uintptr_t base = 0;
    bool knownBuild = false;
};

void Start(const StartInfo& info);
bool Installed();
void OnGamePresent();  // telemetry only; no policy changes
void LogLine();        // cumulative counters, existing log, once per second
int Enabled(int channel);          // 0 shadow, 1 GI, 2 clouds
void SetEnabled(int channel, int on);
const char* StatusLine(char* buf, size_t n);

}  // namespace halfrate
