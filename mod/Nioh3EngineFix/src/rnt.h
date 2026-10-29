// rnt.h - per-render-node timer. [Profiler] NodeTimer = 0 by default. Measurement only.
// The engine already times a render entry in FrameRenderViews 113204 when entry+0x40 is set.
// This stores a record there. It does not patch code and it does not hook 1113B4 or 113204.
// F11 (only while NodeTimer = 1) arms the current graph. Retail leaves +0x40 null.
#pragma once
#include <cstdint>

namespace rnt {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;
    uintptr_t base = 0;
    bool knownBuild = false;
};

void Start(const StartInfo& info);

// One engine stats record. Layout matches 113204 / 1113B4: lock +0, max +0x10, min +0x18,
// count +0x20, mean double +0x28, 10-sample ring +0x30. Never freed.
struct Rec {
    alignas(8) uint8_t b[0x88];
};

struct Slot {
    char name[48];
    uint32_t execRva;
    uint32_t typeHash;
};

// Walk a render-entry list (next at +0x38). Write +0x40 only when it is null.
// imageEnd is one past the module. Returns how many new records were stored.
int ArmEntries(void* first, Rec* recs, Slot* slots, int cap, uintptr_t imageBase, uintptr_t imageEnd, int* stoppedEarly);

bool BytesMatch(const uint8_t* image, size_t imageBytes);

double MsFromTicks(double ticks, double ticksPerMs);

int FormatPass(char* dst, int dstN, double ms, double maxMs, uint64_t count, const char* name, uint32_t execRva);

}  // namespace rnt
