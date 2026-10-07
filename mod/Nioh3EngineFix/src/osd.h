// osd.h - in-game overlay of Nioh3EngineFix: FPS counter (real engine fps vs presented fps incl. frame generation,
// frame time, VRR game-speed factor) and a startup status popup. Drawn into the game's own frame from a hook on the
// bottom-most IDXGISwapChain::Present / Present1 (system dxgi.dll; DXVK's dxgi under Proton), so it shows under
// gamescope, on every presented frame (generated ones too), with no external files. See notes\osd-design.md.
// The mod menu (menu.h) is drawn through the same hook with the primitives at the bottom of this header.
#pragma once
#include <cstdint>

namespace osd {

enum class State : uint8_t { Off = 0, On, WineSkipped, Failed };

struct StartupInfo {
    const char* version = "";                     // MOD_VERSION
    void (*log)(const char* fmt, ...) = nullptr;  // dllmain's Log (thread-safe enough: one fputs + fflush per line)
    const wchar_t* iniPath = nullptr;             // Nioh3EngineFix.ini ([Overlay], [General] StartupPopup)
    uint8_t* base = nullptr;                      // Nioh3.exe image base
    bool knownBuild = false;                      // v2.02: engine readouts (G, config slots) are only read then
    int applied = 0, skipped = 0;                 // patch groups
    const char* skippedNames = nullptr;           // optional, e.g. "W2c L1"
    const char* wine = nullptr;                   // wine_get_version() or nullptr
    bool vrr = false;                             // VRR group applied
    State l1 = State::Off, f1 = State::Off;
};

// Copies the info, reads the ini and starts a background thread that waits until the engine renders, then hooks
// Present. Returns immediately; never throws. Does nothing when FpsCounter, StartupPopup and the mod menu are all off.
void Start(const StartupInfo& info);

// ---- shared with the mod menu (menu.cpp); valid only on the Present-hook thread (under the overlay lock) ----------
struct OverlaySettings {
    int fpsCounter = 1, frameTime = 1, speed = 1, popup = 1;
    int pos = 0;  // 0 TopLeft, 1 TopRight, 2 BottomLeft, 3 BottomRight
    float scale = 1.0f, popupSeconds = 8.0f, popupDelay = 3.0f, brightness = 1.0f;
};
OverlaySettings& Overlay();
const char* PositionName(int pos);
// FPS-counter text scale for a back buffer of height H (Scale x auto factor), as used by the counter itself
float CounterScale(unsigned H);

// ---- frame generation (FG): the game's setting plus what the Present hook measures (v2.02 config slots, see
// notes\settings.md and notes\frame-generation.md). Present-hook thread only (startup popup + mod menu). ------------
struct FgStatus {
    int setting = -1;         // FRAME_GENERATION 0x45BB008: 0 OFF, 1 DLSS, 2 FSR, 3 XeSS (-1 = unknown build)
    int upscaler = -1;        // RESOLUTION_SCALING 0x45BAFF8: 0 OFF, 1 DLSS, 2 FSR, 3 XESS
    bool measured = false;    // real (engine) and presented frame rates are known
    bool active = false;      // presented / real > 1.4: generated frames reach the screen
    bool presenter = false;   // the FidelityFX "FSR Presenter" thread presented within the last 2 s
    double realFps = 0, presentFps = 0;
    double sinceSwitch = 1e9; // seconds since the mod menu changed the setting (the game rebuilds its swapchain)
};
FgStatus FrameGen();
const char* FgTypeName(int v);  // "OFF", "DLSS", "FSR", "XeSS"; nullptr when out of range
// One line: why FG is not running although the setting is on; nullptr when it runs, is off, or is still measuring.
const char* FgReason(const FgStatus& s);
// mod menu: the setting was just changed (the reason line says "switching" while the game rebuilds its swapchain)
void NoteFgSwitch();

constexpr int kGlyphW = 8, kGlyphH = 16;  // font cell in texels
// append a solid rectangle / a text run (pixels, top-left origin; colours 0xAABBGGRR) to this frame's quads
void DrawRect(float x, float y, float w, float h, uint32_t abgr);
float DrawText(float x, float y, const char* text, uint32_t abgr, float scale);  // returns the end x

}  // namespace osd
