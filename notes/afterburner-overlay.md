# MSI Afterburner + RTSS overlay for Nioh 3

Status (2026-10-06 19:05): **installed, configured, running**. RTSS has hooked Nioh3.exe, and the Afterburner OSD shows
`FPS | ms (frametime) | GPU1 °C | GPU1 % | CPU %`.

## Installed
| App | Path | Version | Signature |
|---|---|---|---|
| MSI Afterburner | `C:\Program Files (x86)\MSI Afterburner\MSIAfterburner.exe` | 4.6.6.16757 | Valid, MICRO-STAR INTERNATIONAL CO., LTD. |
| RTSS | `C:\Program Files (x86)\RivaTuner Statistics Server\RTSS.exe` | 7.3.7 build 28314 (the exe reports 7.3.5.28314) | Valid, MICRO-STAR INTERNATIONAL CO., LTD. |

- Installers are in `C:\Users\Ahmad Hamza\Downloads\Afterburner\`. msi.com returns 403 from this PC, so they came from Guru3D's official mirror (ftp.nluug.nl). The SHA256 values match the winget manifests.
- Both apps need admin rights (`requireAdministrator`), so each launch triggers a UAC prompt unless it is started from an elevated shell.

## Configuration (edited files; backups next to them)
- RTSS `Profiles\Global` (seeded from `ProfileTemplates\Global`, backup `Global.bak`):
  - `[OSD] EnableOSD=1`
  - `[Framerate] Limit=0, LimitTime=0, SyncDisplay=0, SyncLimiter=0`. No limiter and no scanline sync.
- Afterburner `MSIAfterburner.cfg` (root, backup `.bak`): `StartMinimized=1`. `HideIntegratedGpus` was set to 1, then set back to 0.
- Afterburner `Profiles\MSIAfterburner.cfg` (backups `.bak` and `.bak2`):
  - `Sources=+Framerate,+Frametime,+...`. Framerate and Frametime were not monitored by default.
  - `[Source X] ShowInOSD=1` for: Framerate, Frametime, GPU1 temperature, GPU1 usage, CPU usage.
  - GPU1 = RX 9070. GPU2 = the integrated AMD Radeon Graphics, which is not shown.
- No overclock, fan or voltage settings were touched.
- Verified through the `MAHMSharedMemory` and `RTSSSharedMemoryV2` shared memory, with no UI interaction.

## Use
- RTSS and Afterburner must be running before Nioh3.exe starts. The benchmark's relaunches pick the overlay up automatically.
- To relaunch after a reboot (elevated): start `RTSS.exe` first, then `MSIAfterburner.exe`. Afterburner now starts minimized.
- Toggle hotkey: **none set by default**. To add one, open Afterburner Settings > On-Screen Display > "Toggle On-Screen Display". Or turn the OSD off in RTSS ("Show On-Screen Display").
- To change the OSD items: Afterburner Settings > Monitoring, then tick or untick "Show in On-Screen Display" for each item.
