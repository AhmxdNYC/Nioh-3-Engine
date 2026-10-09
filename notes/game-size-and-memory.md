# Nioh 3: how many GB could the game use less (disk and memory), same look and play? (2026-10-09)

Research only. Nothing in the game folder was deleted, moved or changed; the game was not launched.

**Bottom line: realistically about 0.1-0.2 GB of disk and about 0.75-1 GB of transient RAM on the Ally. Nothing else is both safe and invisible.**
The install is 135.8 GB (Steam `SizeOnDisk`; 127 GiB by `du`), and 99.9 % of it is the game's streamed art, which cannot be shrunk without changing the look.

## 1. Table (item | GB saved | device | safe? | undo)

| Item | GB saved | Device | Safe? | Undo / Steam verify |
|---|---|---|---|---|
| `package\*.fdata` (148 files, 112.6 GB) + `package\data\*.file` (15 files, 4.5 GB): textures 63.3, meshlet models 39.0, streamed assets 4.75, models 3.4 | 0 | all | NO: the loader reads them by KTID; 0 duplicate textures, all BCn, no lossless repack (notes\asset-audit.md) | n/a |
| `archive\` (archive_00..FF.bin, 14.4 GB, 4,824 scripts/anim/UI blobs) | 0 | all | NO: needed for missions/UI | n/a |
| Language / voice packs | 0 | all | Not separable. Steam has ONE content depot (3681011, 135.8 GB) plus a 0.3 MB one; `UserConfig.language` only picks text. The 23 language text files are KB-sized. Voice/audio sits inside rdb AssetData (`ASRS`, 365 entries, 1.5 GB stored) with no per-language split we could find. Splitting needs an rdb rewrite (unproven) | Steam verify restores |
| `shader_dx12\` (705 MB, 0x*.file) | 0 | all | NO: the exe loads `shader_dx12\0x%08x.file`; a missing file falls back to PB2Unknown (visible breakage). Our replacements live here too | restore `gpu-shaders\backup\` |
| NVIDIA-only DLLs: `nvngx_dlss.dll` 29 MB, `nvngx_dlssg.dll` 7.2 MB | **0.036** | Ally/Linux, any AMD PC | Probably, UNTESTED: only used when DLSS is chosen; game-shipped, so Verify re-downloads | Steam verify, or copy back |
| `libxess.dll` 75 MB, `libxess_fg.dll` 22 MB, `libxell.dll` 0.4 MB (Intel) | **0.097** | AMD | NOT proven: the exe lists XeSS as upscaler/FG option 3 and loads `libxess_fg.dll` when FG=XeSS; may be a static import (import table not checked) | Steam verify |
| Streamline `sl.*.dll` (6 files, 2.4 MB) | 0 (skip) | AMD | NO: `sl.interposer.dll` is in the live path on AMD (OptiScaler/fakenvapi, IAT hook) | n/a |
| Our `*.bak` (3 asi baks 2.2 MB, 4 ini baks ~0.1 MB), `Nioh3EngineFix.log` 0.2 MB | 0.003 | PC | Yes (ours, not Steam files) | none needed |
| `%LOCALAPPDATA%\KoeiTecmo\NIOH3\dump\core_dump_1006155402.dmp` | 0.033 | PC | Yes (2026-10-06 crash, analysed in crash-2526D1.md). Keep Savedata_backup (19 MB) | none |
| Repo-side on C: `captures\` 1.5 GB (rgp 275 MB, tm 319 MB, sweep 206 MB, ~10 MB PNGs), `backups\` 82 MB | up to ~1.0 | PC dev machine | Yes, but this is NOT the game's size; keep digests, drop raw rgp/tm | git history / re-capture |
| D: `nioh3-re` (2.2 GB ghidra project, 1.2 GB assets, 0.46 GB archive, 0.46 GB save-backups) | not game | PC dev machine | Tooling, keep | n/a |
| Steam `shadercache\3681010` | 0 (4 KB) | PC | n/a | n/a |
| Memory: LM load-memory sweeper (`[Memory] LM`, ini currently **0**, built, no Ally A/B yet) | **0.75-1.1 transient** (design ceiling, estimate, not measured) | Ally (15.3 GiB shared RAM/VRAM, zram) | Low risk; self-test turns it off if MEM_RESET does nothing | `LM = 0` |
| Memory: lower MODEL_TEX_QUAL / texture streaming budget | about 1-2 (VRAM) | Ally | NO: changes sharpness (breaks the same-look rule) | n/a |
| Memory: shrink the 6 GiB engine arena | unknown | all | NO: 52 pools sum to exactly 6145 MiB; risks "Allocator allocate fail" | n/a |

**Total realistic: disk about 0.04 GB (NVIDIA DLLs, untested) + 0.04 GB (our baks/dump) = 0.1-0.2 GB (more only if the XeSS DLLs are proven removable); memory about 0.75-1 GB transient on the Ally only. Steady-state memory saving is about 0.**

## 2. Disk evidence
- `du`: `package` 112.6 GB fdata + 4.5 GB `package\data`, `archive` 14.4 GB, `shader_dx12` 0.7 GB, exe 75 MB, DLLs about 0.25 GB. C: has 91 GB free.
- rdb index (`D:\nioh3-re\rdb_index.tsv`, 315k rows), stored bytes by type: StreamingTexContext 63.3 GB (39,195), StreamingMeshletModelData 39.0 GB (11,856), StreamAssetDataFile 4.75 GB (15 = the `package\data` files), ModelData 3.4, TexContext 3.0, G1AFile 2.6, AssetData 1.5, all others under 1 GB.
- Compression: 192,902 entries zlib, 53,490 stored uncompressed (mostly already-compressed BCn streaming mips). Recompressing would save little and the loader reads the per-entry flags, so a repack is a loader-format project; no evidence it pays.
- notes\asset-audit.md: 0.5 % duplicate vertices, 0 duplicate textures, all BCn with full mips, collision 1 % of triangles; no lossless asset win.
- Steam: `appmanifest_3681010.acf` lists only depots 3681011 and 3681013, no optional language/voice depot, so Steam offers no smaller install. Any file Steam knows, once deleted, is restored by Verify.
- DLL dependence: runtime selection by `FRAME_GENERATION` (1 DLSS, 2 FSR, 3 XeSS); Streamline is active on AMD through OptiScaler. No evidence shows the game starting without the NVIDIA/Intel DLLs; one rename-and-launch test would settle it, but 0.13 GB is not worth it.

## 3. Memory evidence
PC session `captures\playsession\20261008-2059-session\modlog_run1.log` (45 `stats mem:` lines, 22+ min, RX 9070, MAX, LM off):

| Metric | t 0.5 min | t 5 min | t 22 min | Meaning |
|---|---|---|---|---|
| process private/commit | 12,963 MB | 18,719 | 18,772 (range 18.3-19.1 GB) | 6.1 GiB arena + about 12 GB driver/D3D commit; flat after the load |
| working set | 5,274 MB | 6,600 | 6,857 (peak 6,862) | arena pages being touched; bounded by 6.1 GiB, +1.6 GB total |
| VRAM local used / budget | 4,509 / 15,417 | 10,034 | 10,035 (9.5-10.2 GB) | texture streaming fills what the budget allows |
| non-local VRAM | 176 | 193 | 193 | flat |
| handles | 2,637 | 2,869 | 2,869 | flat; the waitable-handle leak (memory-leaks.md §5.3) is not visible here |

- Growth: none after the load (memory-leaks.md: Ally GPU memory flat 6812 +/- 3 MB over 5.5 min, RAM+swap about +1 MB/min, 70-min fps drift +0.1/h). The problem is capacity, not a leak.
- Ally (`captures\ally\1725`, MemStats): working set 4.7 -> 5.7 GB and VRAM local 2.4 -> 5.8 GB within 2 min of load, non-local about 0.95 GB, sys RAM available about 5 GB. So game WS + VRAM is about 11-12 GB of the one 15.3 GiB pool; MangoHud saw a +0.75-0.85 GB swap burst at the load peak.
- What LM can save (notes\load-memory.md §4): freed stage pools (250-1450 MB each) stay committed and dirty because nothing decommits before shutdown; on the Ally the kernel swaps them out on the next load. LM (MEM_RESET after 2 stable sweeps) drops them instead, designed to avoid 40-100 % of the 0.75-1.1 GB burst. It does not lower steady state. Status: design and snippet; ini `LM = 0`; the saving is an estimate until the Ally A/B.
- Texture streaming budget: this is where VRAM goes (63 GB streamable, about 4-5 GB resident on the Ally). Shrinking it changes sharpness, so it is excluded by AGENT_BRIEF.

## 4. Recommendations
1. Do nothing on disk; the game is 99.9 % irreducible art. If C: space is needed, clear our own raw captures/dumps (about 1 GB).
2. Run the LM A/B on the Ally (load-memory.md §6): the only look-neutral memory win (up to 1 GB less swap per load).
3. Leave the DLSS/XeSS DLLs alone (0.13 GB, unproven, Verify restores).
