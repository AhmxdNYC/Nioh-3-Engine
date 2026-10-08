# Wo Long 2 demo assessment (2026-10-08)

## Verdict
**Not actionable now; no engine comparison was possible. Revisit only if a build is already on disk or the full game ships (Mar 2027).**

## What / where
- Title: **Wo Long 2: Wings of Ember** (Team Ninja / Koei Tecmo). Full game: 4 Mar 2027, PS5, Xbox/Game Pass, Switch 2, Steam + Microsoft Store.
- Steam main app: **4507990** (store.steampowered.com/app/4507990). Pre-purchase $69.99; PC requirements still "TBD".
- Demo: free time-limited **alpha demo**, 16-30 Sep 2026 (Changban field, 3-player co-op, WIP character creator, Fledgling Phoenix Helmet reward). **It went offline on 30 Sep 2026**, so it is effectively delisted.
- Demo app ID: **not found** (Steam appdetails for 4507990 has no `demos` entry now; SteamDB returns 403 to scripts). Check https://steamdb.info/app/4507990/ in a browser, tab "Related Apps"/"Demo", for the demo appid.
- Engine: not stated by any source I read; Wo Long 1 and DW:O are Katana-family, so likely, but unverified.
- Sources: Dexerto, Gematsu (2026/09 wo-long-2 article), Steam store page and news post.

## Licensed / installed on this PC?
No. Steam libraries: C:\Program Files (x86)\Steam, D:\SteamLibrary (no manifests), F: absent. Manifests present: Nioh 3 (3681010), Avatar Legends, Machine Party, Keep It Up! Demo, Steamworks Redist. No appmanifest for 4507990 or a demo; the string 4507990 is absent from localconfig.vdf, config/*.vdf and appcache/packageinfo.vdf. Nothing was run or modified.

## Steps for the user (only if they want to try; account-side, I did not run them)
1. Find the demo appid on SteamDB (browser) for 4507990, "Related apps".
2. Steam must be online (not offline mode) and not in use on the ROG Ally. In Steam: Console (steam://open/console) -> `app_license_request <demo_appid>`; then `steam://install/<demo_appid>`.
3. Expect this to fail: after 30 Sep the demo's free package is normally removed, and depots may be unavailable even with a license. If a license was already claimed during 16-30 Sep it will appear in the Library and installs normally.
4. Only if licensed: DepotDownloader with the account is acceptable. No third-party re-uploads or cracked builds.

## Comparison plan once installed (read-only, never launch)
- exe strings: ktgl::/ktul::, KIDSTS/KIDSTaskScheduler, SynergisticDispatcher, KTGLGraph; build date (PE timestamp).
- D3D12 vs DX11 imports; FSR/DLSS/XeSS/Streamline dll versions vs Nioh 3 (DLSS4, FSR3, XeSS2).
- shader_dx12 container format and magic/version; config.xml keys.
- Worth comparing: async-compute queue slots on AMD, barrier batching (gpu-barriers.md), job-system worker sizing vs core count, upscaler integration (FSR3 frame gen pacing), quality-table tiers.
