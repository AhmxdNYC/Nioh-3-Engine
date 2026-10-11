// osd.h - in-game overlay of Nioh3EngineFix: FPS counter (real engine fps vs presented fps incl. frame generation,
// frame time, VRR game-speed factor) and a startup status popup. Drawn into the game's own frame from a hook on the
// bottom-most IDXGISwapChain::Present / Present1 (system dxgi.dll; DXVK's dxgi under Proton), so it shows under
// gamescope, on every presented frame (generated ones too), with no external files. See notes\osd-design.md.
// The mod menu (menu.h) is drawn through the same hook with the primitives at the bottom of this header.
#pragma once
#include <cstddef>
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

// TG (dllmain): called on the Present-hook thread after every stats window (~0.5 s) with the rendered fps (the game's
// own frames, FG excluded) and the engine's update ticks/s. Set before Start.
void SetRateSink(void (*fn)(double rendered, double ticks));
// QPC of the newest hooked Present on any swapchain (0 = none yet); any thread.
int64_t LastPresentQpc();
// 1.7.57: thread id that presents the game's real frames (not the FSR presenter); 0 = none seen yet. Any thread.
unsigned long GamePresentTid();
// 1.7.56 one-time notice toast (rebar.cpp): up to 4 lines (first orange, second white, rest grey), shown once the startup
// popup is gone, for `seconds`. Any thread; a second call is ignored. NoticeDone(): it was on screen for its full time, or
// the mod menu was opened while it showed (= dismissed).
void ShowNotice(const char* const* lines, int n, float seconds);
bool NoticeDone();

// ---- shared with the mod menu (menu.cpp); valid only on the Present-hook thread (under the overlay lock) ----------
struct OverlaySettings {
    int fpsCounter = 1, frameTime = 1, speed = 1, popup = 1;
    int pos = 0;  // 0 TopLeft, 1 TopRight, 2 BottomLeft, 3 BottomRight
    // distance from that corner's screen edges, in pixels at 1080p (scaled by height/1080). offUser = [Overlay] OffsetX/OffsetY
    // (or the menu) set it; otherwise the corner default applies (TopLeft sits below the game's own HUD bars).
    int offX = 0, offY = 0;
    bool offUser = false;
    float scale = 1.0f, popupSeconds = 8.0f, popupDelay = 3.0f, brightness = 1.0f;
    bool maxLatencyAuto = false;  // ini value was -1 (auto by OS); Save keeps -1 while the value is unchanged
    int maxLatency = 0;  // [Latency] MaxFrameLatency: 0 = the game's value, 1..16 = SetMaximumFrameLatency(N)
};
OverlaySettings& Overlay();
// MaxFrameLatency status of the presenting swapchain (Present-hook thread)
struct LatencyStatus {
    int want = 0;   // setting (0 = game)
    int game = 0;   // the game's own value (GetMaximumFrameLatency before we changed it), 0 = not known yet
    int cur = 0;    // current value on the swapchain
    int state = 0;  // 0 not checked yet, 1 waitable swapchain (applied), 2 not applicable (no waitable flag / no IDXGISwapChain2)
};
LatencyStatus Latency();
const char* PositionName(int pos);
// effective overlay offsets (pixels at 1080p): the user's, or the default of the current corner
int OffsetX();
int OffsetY();
int DefaultOffsetY(int pos);  // TopLeft 160 (below the game's health / ki HUD, ~15 % of 1080), others 0
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
    double realFps = 0, presentFps = 0;  // rendered frames/s (the game's own presents) and displayed frames/s (incl. generated)
    double tickFps = 0;                  // engine update ticks/s (G+0x610)
    double sinceSwitch = 1e9; // seconds since the mod menu changed the setting (the game rebuilds its swapchain)
    // 1.7.13: kFgOff / kFgRunning / kFgPaused (game menu, loading, cutscene: the presenter passes frames through) /
    // kFgNotRunning (setting on, no presenter thread) / kFgStarting (measuring, or switching) / kFgUnknown
    int state = 5;
    int pauseReason = 0;      // menu::GamePauseReason(): 0 none, 1 game menu open, 2 game paused (stop flags), 3 loading
    double gameThreadFps = 0, presenterFps = 0;  // presents per second of the game thread / of the FSR presenter thread
};
enum : int { kFgOff = 0, kFgRunning = 1, kFgPaused = 2, kFgNotRunning = 3, kFgStarting = 4, kFgUnknown = 5 };
// One status line for the menu: "ON - running (60 -> 120 fps)" etc. Returns the colour class: 0 grey, 1 green, 2 orange, 3 yellow.
int FgStateText(const FgStatus& s, char* buf, size_t n);
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
