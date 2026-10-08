# DW: Origins assessment (decision research, 2026-10-07)

## Verdict
**Not worth it now. Revisit only if the game gets installed for free.**

## 1. Installed?
No. Steam libraries are `C:\Program Files (x86)\Steam`, `D:\SteamLibrary`, and `F:\SteamLibrary`. `F:` doesn't exist. Only Nioh 3 (manifest 3681010), SF6, DBFZ, Avatar Legends, CoD HQ and RoR2 are installed. No static analysis was possible (no exe, no dlls, no config.xml). Nothing was modified.

## 2. Same engine? (web evidence only)
Yes, by public sources. Both games use Koei Tecmo's in-house Katana engine. Wikipedia, DSOGaming and Automaton describe DW:O as running on the same engine as Wo Long and Rise of the Ronin. The KTGL/ktul/KIDS strings are therefore very likely present, but **unverified**. Not checked: the shader container format, D3D12 vs DX11, and the FSR/DLSS/XeSS dll versions.

## 3. Performance reputation
- **DW:O:** Steam Deck Verified. FSR3 distortion on Deck was patched. Mid-range PCs get about 100-120 fps at 1440p. It needs a decent CPU for 60+ fps. Some reports of stutter and crashes, a G-Sync/FreeSync bug, and stutter when fps falls under the cap. SteamDeckHQ praises it as smooth and stable even in big battles (the Deck and PC numbers are Koei's design target: huge crowds, simple shading).
- **Nioh 3:** DSOGaming reports no major shader stutter. It needs 6+ cores, and about 8 for 120 fps. DLSS4, FSR3 and XeSS2 are supported. Camera-pan stutter is a known complaint. The Steam Deck OLED struggles (800p + FSR Balanced is the suggested setting). The ROG Ally X is OK at 1080p.
- No Digital Foundry analysis of either game turned up in the search.

## 4. What could transfer
| Candidate | Value | Notes |
|---|---|---|
| Job-system / thread config | Low | Ally is GPU-bound (99%), so CPU fixes are fps-neutral there. |
| Async compute / barriers | Low-med, speculative | Can't be learned without a D3D12 capture of DW:O. The same code lives in Nioh 3's own exe, so we can already read it. |
| FSR integration cost | Low | Both are FSR3. Nioh 3's own exe is the ground truth. |
| Quality-setting tables | Low | DW:O has a different workload (crowds, simpler shading). Its tables don't map. |
| Shader variants | Low | Same-family shaders would differ in content. |

The core problem is the Ally GPU (pixel shading and passes). DW:O is lighter-shaded, so it proves nothing about Nioh 3's GPU cost. Nioh 3's own decomp and profiling are better evidence.

## 5. Effort vs value
Buying and installing DW:O costs about 40+ GB and money. The static comparison is about 1 agent-day (strings, dlls, shader container, config keys, plus a D3D12 capture for barriers/async compute). The expected gain is small. At best it would confirm already-known engine patterns, and it might show an FSR/async-compute setting that DW:O enables and Nioh 3 doesn't, but that can't be known without owning it.

**If done anyway, compare these three things:**
1. Its D3D12 pass and barrier structure and async-compute use on AMD (PIX/RGP capture).
2. Its config.xml / graphics-option keys vs Nioh 3's (hidden quality knobs).
3. FSR/XeSS dll versions and its upscaler integration (mip bias, jitter, reactive mask).
