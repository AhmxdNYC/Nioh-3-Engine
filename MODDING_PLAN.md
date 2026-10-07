# Nioh 3 engine optimization — plan

Owner: Ahmad. Goal: make Nioh 3 run *much* better on this PC by changing the engine itself,
not Windows settings. Full permission to work unattended (2026-10-06).

## Target machine
Ryzen 5 7600X (6c/12t) · 32 GB DDR5-4800 · RX 9070 (RDNA4, FSR4) · 2560x1440 · game on C: (Samsung 980 NVMe).
Smart App Control ENFORCING → no freshly-built unsigned DLLs. Signed binaries (OptiScaler, AMD/Intel DLLs) load fine.

## Known engine facts (research, 2026-10-06)
- Team Ninja Katana engine, DX12-only, Feb 2026 release. Online co-op; Steam lists no DRM/anti-cheat (verify on disk).
- **Camera judder whenever FPS is between 60 and 120** — camera/physics on a fixed tick with no render interpolation.
  Community workaround = lock 60 or 120. Cutscenes capped at 60.
- Most expensive settings: Global Illumination >> Effects > AO, background mesh, grass, volumetric clouds.
- No native TAA; upscaler mandatory (DLSS / custom FSR3.1 / XeSS). OptiScaler wiki: on AMD feed XeSS → FSR4.

## Workstreams
1. **Recon** — exe protection (SteamStub? packed?), anti-cheat files, config/save locations, engine module layout.
2. **Safety** — backup config + saves (+ exe hash) before any modded launch. MODLOG.md records every change + how to undo.
3. **Baseline** — fixed scene, PresentMon CSV (avg, 1% low, frametime variance, GPUBusy). Not over-measured: one 30 s capture per variant.
3b. **Runtime shader-stutter test** — game precompiles 17,639 PSOs at boot, but verify: watch
   `%LOCALAPPDATA%\AMD\DxcCache` growth + frametime spikes during gameplay. Growth/spikes → use SCSKiller.
4. **Upscaler** — OptiScaler 0.9.4 (`dxgi.dll`, signed) with XeSS input → FSR4.
5. **Settings sweep** — A/B each expensive setting with PresentMon GPUBusy, pick the efficient frontier.
6. **Engine RE (the main event)**
   - Static: Ghidra headless (20 GB heap), strings/xrefs → main loop, frame limiter, delta-time, camera update.
   - Dynamic: external patcher (`patcher/Mem.fsx`, dotnet fsi + Iced) — AOB scan, code caves, hooks; no DLL.
   - Fix A: camera judder → interpolate camera at render rate between fixed ticks (AC Rogue precedent: ~6x less jerk).
   - Fix B: GPU hot spots → RGP capture, find dominant passes (GI), patch quality constants not exposed in menus.
   - Fix C: CPU main thread → ETW/wpr sampling, find hot functions, look for wasteful work.
7. **Package** — one-click `apply.cmd` / `revert.cmd`, notes in this folder.

## Rules
- Patched sessions = offline mode (online co-op untouched). Never touch anti-cheat/DRM.
- Kill processes by exact PID. Stop after 3 identical failures and change approach.
- Decompiled output stays in `C:\Users\Ahmad Hamza\nioh3-engine\re\` (never published).
