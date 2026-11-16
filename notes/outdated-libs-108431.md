# Outdated bundled libraries in Nioh3 (human #108431, 2026-11-16)
Tool: `py -I notes/lib_age_scan.py FILE` (version/copyright stamps, oldest first). DLL versions via PE FileVersion.

## Nioh3.exe (on disk, 77.8 MB; mostly encrypted on disk, only ~38k plain strings)
- zlib inflate 1.1.4 (1995-2002) — asset unpacking; see notes/stream-workers-re-108311.md. OLDEST.
- libVorbis 1.3.7 "Xiph.Org libVorbis I 20200704" (Jul 2020) — audio decode; current 1.3.7 is still latest, NOT outdated.
- boost (typeid names only; version not stamped).
- Everything else is encrypted on disk -> a full list needs the same scan on a memory dump of the running game.

## Shipped DLLs
- dxcompiler.dll 1.6.2106.3 = DXC June 2021 (current 1.8.25xx). Candidate if the game compiles shaders at runtime (check PSO/shader-compile path).
- amd_ags 6.3, FidelityFX 2.3/4.x, XeSS 2.0.2, Streamline 2.9, DLSS 310.4 — all 2025-era, current.
- dxgi.dll 0.9.4 / dinput8.dll = mod loaders (OptiScaler/ours), not game libs.

## Next
- Run lib_age_scan on a runtime dump of Nioh3.exe at the next game test (no extra launch needed).
- Check whether dxcompiler.dll is actually loaded/used by the game during shader warm-up.
