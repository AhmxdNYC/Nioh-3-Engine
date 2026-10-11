// rebar.h - RB: Resizable BAR / Smart Access Memory detection + a one-time "turn it on" notice (1.7.56;
// notes\rebar-fix-v3.md, notes\user-settings.md).
//
// Why: the engine keeps a 700 MB UPLOAD ring (every per-draw / per-dispatch constant buffer) and the meshlet scene buffers
// (instances, LOD root lists, offsets; written by the CPU every frame) in UPLOAD heaps. With Resizable BAR / SAM the driver
// backs them with VRAM; without it they sit in system RAM and the GPU reads them over PCIe. Measured on pc2 (RX 6650 XT in
// a slow ~0.8 GB/s chipset slot, 1080p HIGH): ReBAR OFF -> ON = GPU time 32.5 -> 24.0 ms at spawn, 27.2 -> 19.1 ms at a busier
// spot, but measured fps only +6 % there because the CPU became the limit. Size on a normal x16 PC is unmeasured. It is a
// BIOS setting the mod cannot change, so the mod tells the player once.
//
// Detection (background thread, once, ~40 s after the game first renders): the game's adapter by LUID (engine device,
// v2.02) or the hardware adapter with the most dedicated VRAM; D3D12 ARCHITECTURE.UMA and OPTIONS16.GPUUploadHeapSupported
// from that device; DXGI QueryVideoMemoryInfo(NON_LOCAL) sampled 10 x every 2 s, median. Verdict "UPLOAD heaps in system
// memory" = discrete (not UMA) AND GPUUploadHeapSupported not TRUE AND non-local usage >= 600 MB (the engine's UPLOAD +
// READBACK heaps alone are 800 MB; with ReBAR the RX 9070 and pc2 show 160-212 MB, without it pc2 shows 900-1200 MB).
// Skipped (log line only): Wine / Proton (vkd3d-proton places UPLOAD in host-visible VRAM itself; the Ally is UMA anyway),
// UMA / APUs, [Profiler] VSForceSysUpload A/B runs (the layout is forced), hands-free test sessions ([Debug] Tour or
// VirtualPad on: no toast, the log line stays).
//
// Notice: one log line "RB: Resizable BAR / Smart Access Memory is off: ..." and an OSD toast (osd::ShowNotice, needs the
// overlay's Present hook, i.e. FPS counter / popup / menu on). The toast stays 25 s (or until the mod menu is opened while it
// shows) and is then never shown again: the marker file Nioh3EngineFix.rebarnotice next to the ASI is written once it was on
// screen. Delete the marker to see it again. [General] ReBarNotice: 1 (default) = check + notice, 0 = never check.
#pragma once
#include <cstdint>

namespace rebar {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;     // [General] ReBarNotice, [Debug] Tour / VirtualPad, [Profiler] MemStats / VSForceSysUpload
    const wchar_t* markerPath = nullptr;  // Nioh3EngineFix.rebarnotice (written after the toast was shown)
    uintptr_t base = 0;                   // Nioh3.exe image base
    bool knownBuild = false;              // v2.02: the engine's D3D12 device is read for the adapter LUID
    const char* wine = nullptr;           // wine_get_version() or nullptr
};

// Copies the info, reads the ini and starts the detection thread when wanted. Returns at once; never throws.
void Start(const StartInfo& info);

// Detection result for other modules (any thread): -1 not decided yet / not run, 0 UPLOAD heaps in VRAM (ReBAR on, UMA,
// Wine), 1 UPLOAD heaps in system memory (discrete GPU without ReBAR).
int UploadInSystemMemory();

}  // namespace rebar
