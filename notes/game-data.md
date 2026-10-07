# Nioh 3 game data (archives) — what is tunable beyond config.xml

Status: first pass complete (2026-10-06, game-data agent). Game folder untouched (read-only); nothing injected.

## 1. Tooling — RDBExplorer works headlessly (via a Python port)

- Repo: `data\RDBExplorer\` (MrIkso/RDBExplorer, cloned --depth 1). It is a **WinForms** app (net10.0-windows, OpenTK, HexBox).
  It has no CLI; the apphost .exe would be blocked by Smart App Control anyway.
- Instead the core readers were ported to stdlib Python (byte-for-byte the same logic as
  `Core/Formats/RDBReader.cs`, `RDXReader.cs`, `Core/ArchiveExploler.cs:GetEntryData`, `LayeredFile/LFMBParser.cs`,
  `ObjectDatabaseFile/KidsObjDbParser.cs`). Name/type databases are read straight from the clone
  (`Utils/TypeIDHelper.cs`, `Databases/rdb_names.csv`, `kidstypeinfodb.yml`, `all_properties.csv`).
- Scripts (keep out of the extract dir):
  - `data\tools\n3rdb.py` — RDB/RDX/fdata lister + extractor, and LFM `archive_NN.bin` lister/extractor.
  - `data\tools\n3kod.py` — `.kidsobjdb` (KOD_) dumper with type/property names.

Exact commands (Git Bash; `py -I` = isolated mode):
```
cd "C:/Users/Ahmad Hamza/nioh3-engine/data"
P="C:/Program Files (x86)/Steam/steamapps/common/Nioh3/package"
py -I tools/n3rdb.py list "$P" > rdb_list.tsv              # 310,594 entries in ~3 s (root 241,590 + system 69,004)
py -I tools/n3rdb.py get  "$P" system 908ab6e2 out.kidsobjdb  # one entry by KTID
py -I tools/n3rdb.py dump "$P" system sel.txt extract/system  # KTIDs listed in col 1 of sel.txt
py -I tools/n3rdb.py lfm  "C:/Program Files (x86)/Steam/steamapps/common/Nioh3/archive" 00 [outdir]
py -I tools/n3kod.py extract/system/0x908ab6e2.kidsobjdb [typefilter] > dumps/x.txt
```
Output `data\rdb_list.tsv` columns: rdb, ktid, type, fsize (uncompressed), csize, container (.fdata), offset, flags, etype, name (if known).

### Archive layout (as found on disk, v2.02)
| Path | What |
|---|---|
| `package\root.rdb/.rdx` + 148 `0x????????.fdata` (111.8 GB) | Main resource DB. RDB = `_DRK` header + one `IDRK` record per file (KTID, type KTID, sizes, location). RDX maps fdata index -> fdata file hash. Each fdata entry = `IDRK` header + zlib chunks (16 KB), flags bits 20-25 = compression (1 zlib, 4 zlib-ext). |
| `package\system.rdb/.rdx` | Engine/system resources (shaders G1S 68,771, render graphs, task graph, system kidsobjdb). |
| `package\data\XX\0x????????.file` (15 files) | "External" entries (RDB location flag 0xC01), path = `data/<ktid&0xFF>/0x<ktid>.file`. |
| `archive\archive_NN.bin` (20) + `lfm_order_NN.bin` + `load_inf_NN.bin` | LFM ("layered file manager") bundles: `LFMB` header + table {offset, usize, csize, compressed}; lfm_order gives each entry's canonical path `archive/XX/<64-hex hash>`. |
| `archive\XX\<62 hex>` (~3,500 loose files) | The same LFM namespace as loose files (dir = first 2 hex of a 256-bit path hash, filename = remaining 62). Game-logic data (param tables, strings). |
| `archive\app_config.bin` | 4 bytes. |

Missing: system.rdx indexes `0x785578fe.fdata` and `0xa9992094.fdata` (63 NameDatabaseFiles, i.e. object-name DBs) which are not on disk — harmless for us.

Other dumpers written this session (all `py -I`, stdlib only):
- `data\tools\n3kgk.py` — `.kidsrender` (`_RGK`) / `.kidstask` (`_TGK`) graph dumper. Node format (RE'd here): `INGK` `0000` u32 size, u32 nodeId, u32 typeKtid, u32 0, u32 ncols, u32 nEdges, u32 edges[nEdges], ncols × (u32 type, u32 count, u32 propHash), then values **packed** (no alignment). Same value type codes as KOD. All 21 graphs parse with 0 errors.
- `data\tools\n3kgk_names.py` — one line per graph node (type + column names).
- `data\tools\typescan.py <pkg> root out.tsv HASH=label ...` — scans all 17,038 root `.kidsobjdb` for objects of given type KTIDs (39 s).
- `data\tools\aobscan.py <exe> "AA ? BB" ...` — AOB resolver (used on `re\Nioh3.dump.exe`).
- Name DBs: RDBExplorer's `all_properties.csv` + `kidstypeinfodb.yml`, plus Kassent's `property.txt` / `property_hashes.csv`
  (copied to `data\tools\kassent_*`, merged into `extra_props.tsv` / `extra_types.tsv`). Coverage after the merge: only 4 unnamed properties and 2 unnamed types left in all system DBs.
- Outputs: `data\rdb_list.tsv` (all 310k entries), `data\lfm_list.tsv` (4,824 LFM entries), `data\typescan_root.tsv`, `data\dumps\*.txt`, extracted files under `data\extract\{system,root,stage,lfm}` (~380 MB).
- Object *names* (NameDatabaseFile) for system DBs live in the 2 missing fdata, so system objects show KTIDs only; stage DBs are unnamed too.

## 2. Loose-file override (LooseFileLoader) — layout + hooks (static analysis, no DLL loaded)

Source is public: github.com/kassent/Nioh3-Plugins `plugins/LooseFileLoader` (`src/ModAssetManager.cpp`, `src/ModHooks.cpp`); the shipped `mods\x\LooseFileLoader-1.3.0\plugins\LooseFileLoader.dll` contains exactly the same AOB strings and log strings, so the source is authoritative.

**Override scheme (what a data mod must look like):**
- Folder: `<Nioh3>\mods\` (files directly in it) or `<Nioh3>\mods\<ModName>\` (exactly one level; no deeper recursion).
- File name stem = the asset's **file KTID** as `0x1234ABCD` or `1234ABCD` (8 hex digits, case-insensitive); the extension is ignored (`0x62550860.kidsobjdb` is fine).
- Content = the **raw, uncompressed asset bytes** — exactly what `n3rdb.py get/dump` writes (no IDRK header, no zlib).
- Conflicts: files in `mods\` root win over sub-folders; sub-folders are ordered case-insensitively by folder name, first wins; logged as "Mod override conflict".
- Applies only to **RDB-managed assets** (`package\root|system.rdb`, i.e. everything in `data\rdb_list.tsv`): the hook keys on file KTID at deserialize time. The LFM layer (`archive\archive_NN.bin`, `archive\XX\<hash>`) is **not** covered.
- Index is built once at plugin init (game start). Assets loaded at boot (system.rdb render graphs, task graph, system kidsobjdb) need a game restart; stage assets are re-read on stage load.
- Requires a plugin loader that calls the export `nioh3_plugin_initialize` (Kassent's NIOH3-Plugin-Loader `dinput8.dll` → `<Nioh3>\plugins\*.dll`). `mods\x\Nioh3ModLoader-1.0.6\dinput8.dll` (balfa's NiohNativeModLoader) is the alternative loader.

**Hooks (all patterns resolve to exactly 1 hit in `re\Nioh3.dump.exe`, raw offset == RVA; IDA address = 0x140000000 + RVA):**
| What | AOB | RVA in 2.02 dump | How it is used |
|---|---|---|---|
| DeserializeAsset call site | `FF 93 B0 ? ? ? 48 8D 4D ? 49 89 45` | `0x315C74` (`call [rbx+0xB0]`, inside fn `0x3159A0`) | 6 bytes NOPed, safetyhook **mid-hook**: rcx = asset handler, rdx = loading ctx (`+gameAsset`), r8 = AssetReader, r9 = arg. Gets file KTID via GetFileKtIdFromRes; if an override exists it swaps `assetReader->streamReader` for a file reader, sets `assetFileSize`, `archiveFileOffset=0`, then calls `handler->Deserialize` (vtbl +0xB0) itself and puts the result in rax. |
| GetArchiveInfo | `E8 ? ? ? ? 85 C0 0F 85 ? ? ? ? 4C 8B 7E` | call @ `0x5DE334` → fn `0x2DFB7C` | Inline hook; for overridden streamed assets rewrites `ArchiveInfo.filePath` to the loose file (streaming textures/meshlets). |
| RegisterAssetHandler | `E8 ? ? ? ? 84 C0 0F 84 ? ? ? ? 83 65 ? 00 48 8D 15` | call @ `0x26F5E69` → fn `0x3246E9C` | Inline hook; optional: disables streaming for `StreamingTexContext` (0xAD57EBBA, handler `+0x14` bool) when `DisableStreamingLoading=1`. |
| GetResIdByFileKtid | `E8 ? ? ? ? 8B D0 48 8B CB E8 ? ? ? ? BB` | → `0x609F74` | helper |
| GetResItemById | `E8 ? ? ? ? 49 8B 95 ? ? ? ? 4C 8B C0 E8` | → `0x7AFE94` | helper |
| GetResIDFromRes | `E8 ? ? ? ? 8B C8 44 8B C6` | → `0x4A7A30` | helper |
| GetFileKtIdFromRes | `E8 ? ? ? ? 49 8D AF ? ? ? ? 8B D0` | → `0xFAE850` | helper (AssetIdManager) |
| GetResHandlerFromType | `E8 ? ? ? ? 45 33 C9 4C 89 4D` | → `0x2386FC` | helper |
| g_gameMain | `48 8B 15 ? ? ? ? 0F 57 D2` | global `0x4759910` | GameManager*; `+0x530` ArchiveManager, `+0x210` AssetManager |
g_gameMain matching Kassent's 2.0.2.0 default (0x4759910) confirms our dump is the same build their analysis targets.

**Nioh3FileLoaderLogger 1.0.1 (`hook.asi`, ASI loader)**: hooks the same asset-load function
`40 55 56 57 41 54 41 55 41 56 41 57 48 81 EC … 4D 8B F0` → fn **`0x3159A0`** (the function containing the deserialize call above) and
`40 53 48 83 EC ? 48 8B D9 E8 ? ? ? ? 44 8B D0` → fn **`0xFAE850`** (= GetFileKtIdFromRes). Logs `[VFS] Hash: 0x%08X | Type: %s (%s)` per asset load (KTID + type name, e.g. AssetData/StreamAssetDataFile). Use: find which KTIDs a scene actually loads (e.g. which of the 7 root render graphs is live).

**Replicating on this PC without an unsigned DLL (Smart App Control):** the override is a pure in-process data swap, so the external patcher would need code injection (a mid-hook at `0x315C74` with a trampoline that reads our file). Cheaper alternatives for *static* data, in order:
1. Memory-poke after load: system objects (task graph, Param::Parallel, Physics::Scene, View) live at stable heap addresses after boot; the external patcher can locate them by scanning for the object's column signature and write the value (same technique as the config/Nioh3Fix pokes). Works for values read every frame/stage; not for values only consumed at creation (e.g. buffer sizes).
2. RDBExplorer-style injection (append a new IDRK block to the fdata, patch the RDB location) — only touches data files, no code, but modifies the game folder (back up `root.rdb`/`system.rdb` + the touched fdata; Steam verify restores).
3. On handhelds/other PCs: LooseFileLoader layout above.

## 3. Integrity / safety assessment
- No signature or hash check of asset content was found: RDB entries carry only KTIDs/sizes/offsets (no CRC/hash field); the exe has no anti-cheat (no EAC/BattlEye/nProtect strings) and no "checksum/integrity/tamper" strings; LooseFileLoader feeds arbitrary bytes into the deserializers and RDBExplorer's append-and-repoint injection is used by released localization/model mods.
- Steam: files are only checked on "Verify integrity" or replaced by updates. Any repack/inject must back up the originals (`backups\`); a game update will overwrite or invalidate them (KTIDs are stable across patches, offsets are not).
- Online co-op: there is no anti-cheat, but render/LOD/foliage data is client-local. Do **not** change gameplay-affecting data (physics `Physics::SceneParam` timestep/iterations, AI/behaviour, collision) for online play; keep perf mods to visuals/culling, and test offline first.
- Loose overrides are reversible (delete the file); fdata injection is reversible by restoring the backed-up `.rdb` + truncating the fdata (or Steam verify).

## 4. What tunable perf data exists

### 4.0 Big picture
- **The mapping from menu quality to parameters is code, not data.** No config key string (MODEL_DRAW, SHADOW_QUAL, MESHLET_QUALITY …) appears in any extracted file or in the LFM layer.
  - The quality-like fields of `CommonViewSetting` (ModelLODLevelOffset, RadenParmanentResidentLevelNum, OBOROLODClumpSettingValue, VirtualShadowMapTileLifeSpan, GlobalMipLodBias, LODLevel, SuperSamplingResolutionRatio) are not set by any shipped database. The only CommonViewSetting in the data is an editor/preview object.
  - So the presets live in code. This agrees with settings.md, where `fn_2353C0` picks a column of a per-stage LOD table.
  - Editing data therefore changes the **base values** that the code then scales.
- The LFM layer (`archive\`) holds UI data (KSCL layouts, G2A/TMG animation packs, sprite tables), game scripts (bytecode, magic `2011-04-29…`) and game tables. None of it relates to rendering or performance.
- The data that matters lives in `.kidsobjdb` (KOD_) object databases and in two graph files. All of it sits in the RDB, so LooseFileLoader can override any of it by KTID.

### 4.1 System scene DB `system 0x908ab6e2.kidsobjdb` (291 KB): threads, physics, frame parameter [loaded at boot]
| Object (KTID) | Fields | Notes |
|---|---|---|
| `Physics::Scene` f43035be | **MaxThreadCount 2**, MaxDynamicEntityCount 2200, MaxKinematicEntityCount 150, MaxJointCount 8000 | The task graph's `Physics::Simulate` node runs this scene (PhysicsSceneObjectNameHash f43035be), so the physics solver uses 2 threads. Candidate value: 4. The PC has 12 hardware threads and the PhysUpdate pool is 4. |
| `Physics::SceneParam` 7dc63959 | TimeStep 0.016667, IterationMax 10, SubStepCount 1, Tolerance 0.0005 | Affects gameplay. Do not touch for online play. |
| `Param::Frame` 581d17f1 | **FramePerSecond 60** | Referenced by the task graph's FrameSync, Behavior::Update and UpdatePlaceable nodes (FrameParamObjectNameHash). Probably the ktgl time base behind the 60-step timing model (research.md §1). REN_FPS probably overwrites it at runtime. **Flag for the frame-timing work and verify live.** |
| 15× `Param::Parallel` | ParallelUnitNum 32 (13 objects) or **0** (2710f411, 3ad90e00); ParallelType 0 or 1 | How finely each task node splits its jobs (see 4.2). |
| 6× `3D::ViewParam` | MaxLightNum 1024, ParallelSortLightUnitNum 128, ParallelSortPrimitiveUnitNum 128, sort methods | Sort parallelism per view. |
| `OcclusionCullingManager` ×2 | OccluderUnitArrayCapacity 16 | |
| 8× `Camera::Perspective`, 2× Ortho | Engine default camera and post-effect parameters | Each environment overrides these (4.4). |
| `UserInterface::StandardManager` | NumberOfPad 2, Keyboard 1, Mouse 0 | |

### 4.2 Frame task graph `system 0xbd13a52e.kidstask` (3.9 KB, 33 nodes) [loaded at boot]
Node order:
1. FrameSync
2. KeyTrigger / Trigger
3. **Behavior::Update** (Parallel 2710f411 = **0 units**)
4. Physics::Simulate
5. UpdateLevelEvent (32)
6. EvaluateScript (0)
7. Behavior::Update #2 (0)
8. UpdatePlaceable (32)
9. ApplicationCallback
10. BeginRegisterLight
11. 5× RegisterLightByListContainer (32 each)
12. RegisterWindByOctreeManager
13. EndRegisterLight / BeginRegisterPrimitive
14. FloatingOrigin apply
15. 7× RegisterScenePlaceableByListContainer (six with 32 units, one with 0; two use ParallelType 1)
16. FloatingOrigin unapply
17. EndRegisterPrimitive
18. CommitToNextRender

Findings:
- The ktgl-level game-object update (`Behavior::Update`, which runs twice) and `EvaluateScript` run with **ParallelUnitNum 0**, i.e. not split (serial). That matches the profile of one hot main thread.
- Setting them to 32 is a *high-risk experiment*: the game-object update order may not be thread-safe, which would mean crashes or desync. Try it offline only, and only after CPU profiling shows these nodes on the main thread.
- The render-registration nodes already use 32 units. The exception is RegisterScenePlaceable 3ad90e00, which uses 0.

### 4.3 System view DBs [loaded at boot]: capacities
Which view is which:
- `system 0x62550860` is the main 3D view. It is the only one with ClusterNum [64,64,16], SDF clusters and GPU culling setup.
- `0x57283dfc` and `0xd01e3b99` are other 3D views.
- `0x34b0110d` and `0x639428d9` are small views or UI views.

Main-view values:

| Field | Value |
|---|---|
| TempBufferSize | 0x1B71B00 |
| MaxDisplaysetNum | 16192 |
| ShadowmapWidth / Height | 4096 / 4096 |
| PlaneShadowmap | 1024 |
| CubeShadowmap | 256 |
| EffectDisplaylistMaxPrimNum | 7600 (cap on effect primitives) |
| EffectDisplaylistMaxGPUEffectObjectNum | 1024 |
| EffectParallelLightNum | 4 |
| ClusterMaxLight / Spot / Decal | 2048 / 2048 / 4096 |
| MaxHairNum | 128 |
| OnCreate*ShadowMapperNumPerView | 10 |

These are capacities, not per-frame work. Lowering them saves memory, not time.
- Exception: EffectDisplaylistMaxPrimNum is a hard cap on effect primitives. Lowering it acts as a crude VISUAL_EFFECT limiter, at the risk of missing VFX.

### 4.4 Environment DBs: 137 named, 8,595 per-area camera presets [loaded per stage]
- The names are in `data\stage_names.tsv`, kind=env.
- Each environment DB (about 9 MB, e.g. `0x396b9210`) holds 62 `Camera::Perspective` presets plus fog and skylight objects.

Performance-relevant fields and their shipped distribution:

| Field | Values (count) | Side |
|---|---|---|
| VolumetricFogResolutionX / Z | X: 160 (all). Z: 64 (7964), 32 (631) | GPU froxel grid. The VOLUMETRIC setting probably scales it |
| VolumetricFogFarDistance | 60000, 5000, 240000, 30000 … | GPU |
| VolumetricFogCustomSMResolution | 1024 / 0 / 512 | GPU |
| CloudRayTraceNum / CloudCircleRayTraceNum | 16 (all) | GPU |
| SSAORayNum / SSAOStepNum | 5/2 (most), 8/6, 10 | GPU |
| RLRMaxRaySamples (SSR) | 32, 24, 0, 64, 16, 80, 250 | GPU |
| ShadowCascadeStart / 2Start / 3Start | 1800 / 7000 / 12000 typical | CPU+GPU (shadow casters per cascade) |
| ShadowEraseFarEnd | 20000 (7608), 50000, 100000 | **CPU** (culling and submission distance for shadow casters) plus GPU |
| ShadowmapPlanarResolution / Cubic | 512–4096 / 256–1024 | GPU (local lights) |
| ShadowSampleNum | 4 / 6 | GPU |
| TerrainTessellationMaxDistance | 4000 (7514), 5000, 2000 | GPU |
| LPVIterationNum 9, SDFRaymarchNum 8, S4SamplingNum 16/25 | | GPU |

- Most of these are GPU-side, and this PC's GPU has headroom, so leave them alone.
- Only `ShadowEraseFarEnd` and the cascade splits matter for the CPU.
- Overriding them means shipping a 9 MB file per environment. Poking the live camera objects in memory is cheaper.

### 4.5 Landscape: 33 `LandscapeComposition` objects [loaded per stage]
- Each is mapped to its stage in `data\stage_dbmap.tsv`.
- `LODDistanceScale` is the distance multiplier for terrain quadtree LOD. LANDSCAPE_QUALITY probably scales on top of it. Values: 1.3 (20 objects) or 1.49 (13 objects).
- NearFarBlendingOffset ranges from 1304 to 15000.
- The files are small (1–2 KB), so loose overrides are cheap.
- Lowering LODDistanceScale to about 1.0 trims the terrain patch count (CPU quadtree work plus GPU tessellation). Expected gain: small.

### 4.6 Procedural placement: foliage, rock and prop scatter on 27 maps [loaded per stage] ← best CPU candidate
- Each open-field map (S00_EDO, S01_SEN_01..04, S02_HEI_01..04, S03_KOD_01, S04_BAK_01..04, S90_DLC1_01..04, A01_HAZAMA, …) has one `3D::Placeable::ProceduralPlacement` object.
- The mapping to maps is in `data\stage_dbmap.tsv`. Per-map values and file sizes are in the table in §7.
- This is the system behind costs in the style of GRASS_DENSITY and MODEL_DRAW:
  - CPU: placement updates plus per-instance culling (`fn_31EF94`).
  - GPU: the instances themselves.

Scalar fields (shipped values):

| Field | Values |
|---|---|
| ViewDistance (cm) | 60000 on 21 maps; also 50000 / 40000 / 35000 / 30000 |
| UpdateDistance | 100000 |
| FreeDistance | 120000 |
| NumCells | 512² |
| NumBlocks | 64² on 25 maps |
| InstanceCullingDistance | 5000 on 17 maps, otherwise 0 |
| ShadowAbsolutelyRenderDistanceH | 9000 on 16 maps, otherwise 0 |
| AbsolutelyRenderDistanceH | 0 / 6000 / 1000 |

Per-element arrays:
- `LODDistanceArray`: 2000 up to 20000 and beyond.
- `ModelViewDistanceArray`: e.g. 21052, 10000, 5000; -1 means unlimited.
- `ModelInstanceNumLimitArray`: 50000 for each element.
- `LODTransitionTimeArray` and `ElementRADENLODBiasGroupArray`.

Knobs that go beyond the menu:
- Scale `ViewDistance`, `ModelViewDistanceArray`, `LODDistanceArray` and `ShadowAbsolutelyRenderDistanceH` by roughly 0.6–0.75.
- Cap `ModelInstanceNumLimitArray`.

The StaticGrass object (`0x6e02bfd0`, one file) has FarClip 15000, LODFarClipRate 0.7, LODDensityRate 0.3 and MaxBlocks 512.

### 4.7 Octree / HLOD: 151 stage Octree DBs
- `HLOD0DistanceThreshold` and the `HLOD2DistanceThreshold*` fields are 0 everywhere, and `HLODDepth` is -1. HLOD must be driven elsewhere (code or model data), so there is nothing to tune here.
- `MaxPlaceables` (1024–9555) is a capacity.

### 4.8 Render graphs: 20 `.kidsrender` files
- **Full deferred pipelines:** system `0x102dd706` (431 nodes) and root `0x05c37956`, `0xce279af4`, `0xd813e276`.
  - They contain OBOROExport, VirtualShadowMapPass, RaytracingReflection ×2, MeshletResolveLODPass ×3, SSAO, VolumetricFogScatter and RLR.
- **Variants without OBORO (some also without VSM):** `0x0bc5547f`, `0x1e710c81`, `0x20e55b61`, `0x92b508c4`, `0x8698a511`. They are probably for interiors, events or lower GI settings.
- Which graph is live is unknown. Find out with the FileLoaderLogger hook at `0x3159A0` or a memory read.
- The nodes are wiring only, with no quality numbers. They hold render targets, shader hashes, `BeginCheckSkip` blocks with CheckSkipType codes, and MaxParallelLightNum 3/4.
- Passes could be removed here, but that is high-risk, and the menu already toggles the expensive ones. Not recommended.

### 4.9 Other
- **Render-target table** `system 0x93fb3280` (784 RTs): sizes of the fixed render targets only; screen-sized ones are -1. Examples: a 4096-wide CubeArray with 240 slices, and OBORO probe atlases (128² cube arrays with N slices). VRAM only.
- **Shader data:** `system 0x403a0f7a.bin` (48 MB) and five more `_DVS2000` files hold shader-variant/PSO data. Not tunable.
- **Master index:** `MotorCommonSetting` (`root 0x6d011726`, object 61ad16cf) maps stage, environment, event and character names to DB KTIDs. Dumped to `data\stage_names.tsv`.
- **Stage settings:** each `MotorStageSetting` lists its ObjectDatabases. `data\stage_dbmap.tsv` maps the Octree, Landscape and ProcPlacement DBs to stage names.
- **Character settings:** 1,953 `MotorCharacterSetting` objects (one per character), not yet inspected. They are a candidate for animation and cloth LOD. Fields to look at: `Model.ClothModulesLODSimulateMode`, `LevelOfDetailLow/MidPartsDistance`, `ShadowClipDistance`, `MotionMatchInterval`. See the next steps.

- Character/model data: `MotorCharacterSetting` → per-character `Model::WorldPQ` objects carry per-model flags: ShadowCastFlag, IsCastVirtualShadow, RADENLODBiasGroup, IsDisabledOBOROSDF, FrustumViewCullingType. There are no per-character LOD distances in data. Animation LOD (MOTION_QUALITY) is code; Nioh3Fix pokes it.

## 5. Ranked data-level optimizations to try

Context: the game is CPU-bound (CPU about 10.7 ms vs GPU about 7 ms), so CPU relief ranks first. All items are offline-first and reversible.

| # | Change | Where (KTID / rdb) | Delivery | Expected | Risk |
|---|---|---|---|---|---|
| 1 | **Physics solver threads** `Physics::Scene.MaxThreadCount` 2 → 4 | system `0x908ab6e2` obj f43035be | Loose file (boot-time) or memory poke before the first stage load | Small to medium CPU gain in fights or physics-heavy scenes (ragdolls, debris) | Low to medium. Thread count only, simulation unchanged |
| 2 | **Foliage/prop scatter distances** on open-field maps. Scale ×0.6–0.75: ProceduralPlacement `ViewDistance`, `ModelViewDistanceArray`, `LODDistanceArray`, `ShadowAbsolutelyRenderDistanceH`. Optionally cap `ModelInstanceNumLimitArray` | 27 root DBs, `data\stage_dbmap.tsv` | Loose file per map (0.3–6.6 MB) | Medium CPU gain (instance update, culling, submission) plus GPU, in open fields where the CPU p95 spikes. Goes further than GRASS LOWEST | Low. Visual pop-in only |
| 3 | **Shadow-caster distance** in environment camera presets: `ShadowEraseFarEnd` 20000 → 12000–15000 and the cascade starts | 137 environment DBs (~9 MB each) | Memory poke of live camera objects (loose files are too big) | Small to medium CPU gain (fewer shadow casters submitted) | Low. Distant shadows fade earlier |
| 4 | **Terrain LOD** `LandscapeComposition.LODDistanceScale` 1.3/1.49 → 1.0 | 33 root DBs (1–2 KB) | Loose file | Small | Low |
| 5 | **Effect primitive cap** `View.EffectDisplaylistMaxPrimNum` 7600 → 3800 (the value of the second view) | system `0x62550860` | Loose file (boot-time) | Caps the worst VFX spikes | Medium. Effects can be dropped |
| 6 | **Task parallelism**: `Param::Parallel` 2710f411 (Behavior::Update ×2, EvaluateScript) 0 → 32. Also RegisterScenePlaceable 3ad90e00 0 → 32 | system `0x908ab6e2` | Loose file (boot-time) | Potentially large main-thread relief, **if** those nodes are hot | **High**. Thread-safety unknown. Crash/desync risk; offline only |
| 7 | GPU *spending* (we have headroom): raise VolumetricFogResolutionZ, RLRMaxRaySamples, SSAORay/StepNum, ShadowmapPlanarResolution | Environment DBs | Memory poke | Better image quality, not fps | Low |
| — | Not worth it: Octree HLOD (unused), render-graph pass removal, RT/buffer capacities, LFM/UI data, Physics::SceneParam (gameplay) | | | | |

**Before any of these, verify the menu does not overwrite the value.** Load a stage, then read the live object (external patcher memory read: find the KOD object in memory by its column signature) and compare it with the shipped value at two menu settings. If the code overwrites the field (for example the per-quality columns of `fn_2353C0`), patch the code-side table instead.

Delivery on this PC (Smart App Control blocks LooseFileLoader):
- Boot-time system objects: a memory poke after boot, before entering a stage, works for values read every frame (ParallelUnitNum, MaxThreadCount if it is read when the solver starts). Values consumed only at creation time need the file route: fdata injection with backups.
- Per-stage objects: poke after stage load, or inject.

## 6. Next steps
1. Confirm which render graph and env DB are live in a given area. Options: hook/read `0x3159A0` the way the logger does, or read the ArchiveManager at `g_gameMain(0x4759910)+0x530`.
2. Write a "KOD object finder" for the external patcher: search process memory for the column header triplets of a known object (e.g. Physics::Scene: `05 00 00 00 01 00 00 00 <hash MaxThreadCount>`), or walk the asset manager. Then A/B items 1, 2 and 3 with PresentMon (CPUBusy p50/p95).
3. RE the consumer of `Param::Parallel.ParallelUnitNum`, to learn what 0 means (serial, or auto?) before trying item 6.
4. Check whether `Param::Frame.FramePerSecond` is rewritten by REN_FPS, for the frame-timing agent.

## 7. Appendix: ProceduralPlacement per map (distances in cm; size = raw asset bytes = loose-override file size)

```
ktid	size	map	ViewDist	InstCull	ShadowAbsH	AbsH	nElem	LODDist(min/max)	ModelViewDist(max)	InstLimit(max)
01f83b4b	6626396	S90_DLC1_03_BASE	60000	5000	9000	0	242	300.0/20000.0	21052	50000
069a56fe	5578508	S00_EDO_BASE	60000	5000	9000	0	197	300.0/20000.0	25000	50000
144c874a	1959708	S00_EDO_07_BASE	50000	0	0	1000	179	300.0/20000.0	21052	50000
23f35067	6626684	S02_HEI_02_BASE	60000	5000	9000	0	212	300.0/20000.0	20000	50000
2dba4659	6557712	S00_EDO_LAST_BASE	30000	5000	9000	0	6	3000.0/6000.0	10000	50000
38b5a9a6	6626152	S04_BAK_03_BASE	60000	5000	9000	0	242	300.0/20000.0	21052	50000
3d66592c	6618704	S90_DLC1_02_BASE	60000	5000	9000	0	213	300.0/20000.0	20000	50000
3e4ac879	6591900	S01_SEN_03_BASE	40000	5000	7500	0	84	300.0/18000.0	40000	50000
749d2e68	6626396	S02_HEI_03_BASE	60000	5000	9000	0	242	300.0/20000.0	21052	50000
78d4770d	6601436	S90_DLC1_01_BASE	60000	5000	9000	0	150	300.0/18000.0	30000	50000
895f87a7	6612024	S04_BAK_04_BASE	60000	5000	9000	0	147	300.0/20000.0	20000	50000
8ef4a67a	6589520	S01_SEN_04_BASE	35000	5000	9000	0	82	300.0/18000.0	35000	50000
9761eda4	6614640	S04_BAK_01_BASE	60000	5000	9000	0	179	300.0/20000.0	30000	50000
9aae7d9e	5526356	S01_SEN_03_BASE	60000	0	0	6000	57	1000.0/18000.0	5000	50000
9cf70c77	6593564	S01_SEN_01_BASE	60000	0	0	6000	94	300.0/18000.0	20000	50000
af8c027c	6590420	A02_TITLE	60000	0	0	6000	90	300.0/18000.0	20000	50000
c5470c69	6612492	S02_HEI_04_BASE	60000	5000	9000	0	147	300.0/20000.0	20000	50000
c6712491	6590420	S03_KOD_01_BASE	60000	0	0	6000	90	300.0/18000.0	20000	50000
c68a1d6a	6583672	S90_DLC1_04_BASE	60000	5000	9000	0	61	1000.0/20000.0	0	50000
d3497266	6613960	S02_HEI_01_BASE	60000	5000	9000	0	150	300.0/18000.0	20000	50000
d66a8a03	6588232	A01_HAZAMA	60000	0	0	6000	82	300.0/18000.0	15000	50000
dad6d9f0	6586876	S01_SEN_04_HITOKOTO	50000	0	5000	0	78	300.0/18000.0	20000	50000
e7333246	6590420	S90_MOV_EDO_ALL	60000	0	0	6000	90	300.0/18000.0	20000	50000
e80bcba5	6593412	S04_BAK_02_BASE	60000	5000	9000	0	86	300.0/18000.0	20000	50000
eb17997e	6590420	A02_CHARA_CREATE	60000	0	0	6000	90	300.0/18000.0	20000	50000
eb585b9f	6576464	S01_SEN_04_BASE	60000	0	0	6000	59	1000.0/18000.0	5000	50000
eda0ea78	6591700	S01_SEN_02_BASE	40000	5000	9000	0	83	300.0/65749.921875	40000	50000
```
Most ProcPlacement files are about 6.6 MB, mostly ResourceMap/DensityMap arrays; an override must rewrite the whole file. ModelInstanceNumLimitArray is 50000 for every element on every map, so it is effectively uncapped. S01_SEN_02 has one element with LOD distance 65750.
