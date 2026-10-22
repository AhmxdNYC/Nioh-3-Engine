# Downloaded mods vs Nioh3EngineFix (2026-10-15)
Extracted to D:\nioh3-wt\dl-check\a (nioh3fps) and \b (opt36). Nothing executed or installed; DLL read as strings only.

## 1. nioh3fps 0.3.0 (github yogesh1239/nioh3fps), zip of 6 files
What it is: a native dinput8.dll proxy (129 KB, MinHook, MIT) plus nioh3fps.ini. It does one thing: "lockstep", one game
update per rendered frame, scaled to real time, so the game is smooth at any fps below 120. Core is decrypted at runtime
("unique decrypted core"), so I cannot read its logic beyond strings. Keys F10 toggle, F9 marker, Ctrl+F10 timing CSV.
Self-disables online/co-op. Has crash minidump handler, flight recorder, log nioh3fps.log, Collect-Nioh3FpsLogs.bat.
Settings: Enabled, StartEnabled, Lockstep, HeartbeatSource (take/xell_entry/xell_exit), SafeMode, ToggleKey/DumpKey/MarkerKey,
TimingMode (Raw/Smooth), AvgFrames, SmoothK, ScaleMin 0.5, ScaleMax 2.0, LogLevel.
- SAME as ours: the goal is our VRR (variable-dt, real-time game speed below the cap) and our online suspension (VRROnline auto).
  Ours is also tied to the limiter (L1) and has TG (logic tick governor) on top; ScaleMin/Max ~ our VRR clamp.
- NEW to us: Raw vs Smooth (multi-frame averaged) dt option; a hotkey to flip the fix live and a marker key; the flight-recorder
  CSV dump; "heartbeat" source choice (XeLL entry/exit); it patches about 27 GetFps call sites (strings "GetFps <rva>"),
  more than the sites we use. Worth checking whether it covers consumers VRR misses (its list is a good cross-check).
- CONFLICT: HIGH if both timing fixes run. Both rewrite dt / GetFps consumers, so the game gets the correction twice (wrong
  speed, jitter). Its dinput8.dll is its own hook, not an ASI loader: it will not load Nioh3EngineFix.asi, and it collides
  with Ultimate ASI Loader's dinput8.dll (same file name). Needs its own dinput8.dll; ours runs as version.dll on the PC, so the
  files can sit side by side, but the hooks still overlap. README says "FG may jitter". Closed-source core, cannot audit it.
- Recommendation: do NOT install together with our VRR. Only for a separate A/B with Nioh3EngineFix VRR=0 (and TG/L1 timing
  keys off), our ASI still loaded via version.dll, to compare smoothness against our VRR. Not for the Ally (Proton needs
  dinput8 override and we already own that name).

## 2. Nexus #36 "Optimization modification and FG x4 unlock" (rar, 895 bytes)
Byte-identical to our mods\opt36-default.rar (SHA256 4CE2E3D0...). Already surveyed in community and fps-unlock notes.
It is one file: "config - qualidade.xml", a copy of the game's own config.xml (goes to %LOCALAPPDATA%\KoeiTecmo\NIOH3).
No DLL, no code. Values: BORDERLESS, VSYNC OFF, 2560x1440, REN_RAT 100, REN_FPS 120, DYNAMIC_FPS OFF, FRAME_GENERATION OFF
(DLSS_FRAME_GEN_MULI 2X), RESOLUTION_SCALING FSR, sharpness 1.0, HIGH shadows/GI HIGHEST/AO HIGH/model/meshlet/grass HIGH,
MOTION_BLUR/CA/VIGNETTE/DOF/LENS_FLARE/MACH_BAND OFF, BLOOM+SSS+NOISE_FILTER ON, EVENT_FPS 30, CPU_SPEC_CLASS STANDARD.
- SAME: it is only game settings; our mod reads/writes the same keys (FRAME_GENERATION via the menu, with .bak). Not a
  performance patch. The "FG x4 unlock" is only in the Nexus page text, not in this file (FG is OFF, multiplier 2X).
- NEW: nothing technical. The only slightly new idea is CPU_SPEC_CLASS, EVENT_FPS=30 and POSTEFFECT_RESOLUTION as a lower
  load preset, which are all in-game options we can set ourselves.
- CONFLICT: none with the ASI. Risk is only that overwriting config.xml replaces the user's resolution/monitor/HDR and
  FG state (our FG-marker logic and Proton FG crash warning assume the user's own file). 1440p/120 is hard-coded.
- Recommendation: skip. Do not ship or copy it; no value over our own presets.

## Overall
nioh3fps is the only item of interest, as a competitor/cross-check for VRR (GetFps site list, Raw/Smooth averaging idea).
Do not co-install with our timing patches. opt36 is a plain config file, ignore.
