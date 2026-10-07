# Nioh 3 static analysis (Ghidra), `re\out`

Source: `re\Nioh3.dump.exe`, a runtime memory-layout dump of the SteamStub-decrypted Nioh3.exe (Katana engine, x64, DX12).
Image base 0x7FF74F410000. **Every address in these files is an RVA** (subtract 0x7FF74F410000 from the `FUN_7ff7...` / `DAT_7ff7...`
names that Ghidra prints). Because this is a runtime dump, statically initialised pointers in `.data`/`.rdata` hold real absolute
addresses, and `.data` holds the **live values at dump time** (the settings below were read straight out of it).

## Files

| file | contents |
|---|---|
| `functions.tsv` | `rva size name callers`: 237,573 functions. Starts come from `.pdata`, so they are reliable. `callers` counts direct call references only, so virtual-call and callback targets show 0. |
| `strings.tsv` | `rva enc funcs via string`: 29,574 ASCII/UTF-16 strings found by a full scan of `.rdata/.data/_RDATA`, not only the strings Ghidra defined. 17,358 have xrefs. `funcs` lists functions that reference the string directly (`D0x..` means a data location). `via` lists pointer slots holding the string's address, written `slot=funcs`, or `slot>tableBase=funcs` when code indexes a table from its base. Config keys, option lists and script-binding names are reached only through `via`. |
| `imports.tsv` | `library name iat_rva nrefs calling_functions`: 485 imports. `T0x..` is a jmp-thunk, followed by the thunk's callers. |
| `anchors.md` | The 15 Nioh3Fix sites. Each has its containing function (RVA and size), 1-level callers, disassembly around the site, and the full decompilation. |
| `README.md` | this file |

Scripts are in `re\scripts`: `N3PreAnalysis.java`, `N3AddLeaRefs.java`, `N3Export.java`, `N3Strings.java`, `N3Query.java`, and `pe_util.py`.
`pe_util.py` works offline without Ghidra: `py re\scripts\pe_util.py func 0x5af50a` gives the `.pdata` function bounds with chained unwind entries resolved, and `... imports` lists the imports.
Logs are in `re\logs`.
(`spinscan.txt` in this folder was written by a different agent.)

## Query tool (for other agents)

```
re\decomp.cmd 0x5af50a                      :: decompile the function containing the RVA (header lists callers/callees)
re\decomp.cmd asm:0x5af50a:12               :: disassemble +-12 instructions, showing refs, strings and IAT names
re\decomp.cmd x:0x45baf54                   :: every reference TO an RVA (READ/WRITE/CALL + containing function)
re\decomp.cmd c:0x5af50a                    :: call sites of the containing function
re\decomp.cmd 0x5af50a asm:0x5af50a c:0x5af50a   :: batch several queries to pay the startup cost once
```
- A query takes about 5–15 s, including JVM start. It runs with `-readOnly -noanalysis` and a 4 GB heap (`GHIDRA_HEADLESS_MAXMEM`, which you can override), so it is safe to run while the game is running.
- **Locking:** Ghidra takes an exclusive project lock even for read-only opens (tested: a second process fails with `LockException: Unable to lock project`).
  `decomp.cmd` serializes callers with a mutex directory `re\ghidra_proj\.n3query.mutex` and waits up to 10 min. If a query was killed, delete that dir.
  Do not open the project in the Ghidra GUI while agents are querying it.
- Set `N3Q_OUT=<path>` to keep the output file. When no Ghidra function covers an address, it falls back to `.pdata` and creates the function in memory; nothing is saved.
- For string lookups, grep `out\strings.tsv` instead of using `s:`. `s:` only sees strings Ghidra defined.

## How it was built, and the time it took

1. `analyzeHeadless -import` with an 18 GB heap. The pre-script disabled Decompiler Parameter ID, x86 Constant Reference Analyzer, Aggressive Instruction Finder, Embedded Media, Condense Filler and Scalar Operand References.
   **Auto-analysis took 1,903 s (about 32 min)**, plus about 1 min for import. The biggest costs were "Create Address Tables – One Time" (867 s), Stack (286 s), Disassemble Entry Points (276 s) and Decompiler Switch Analysis (231 s).
   Peak memory was about 6.3 GB committed. The machine still ran out of commit memory at 13:46 because the game and other tools were also running; the analysis process survived. **Use 8 GB or less for any further runs.**
2. **Gotcha:** with the constant-reference analyzer off, x64 `LEA reg,[rip+X]` gets **no references**. These instructions are how MSVC loads string, table and global addresses. Before the fix, only 391 strings had xrefs.
   `N3AddLeaRefs.java` (58 s) walks all 12.97 M instructions and adds 126,493 LEA data references. The fix is saved in the project.
3. `N3Export.java` writes `functions.tsv` and `imports.tsv` (about 10 s). `N3Strings.java` writes `strings.tsv` (about 10 s). `anchors.md` was generated from `decomp.cmd` output.
- The project is `re\ghidra_proj\Nioh3.gpr` (1.1 GB). It has no RTTI or class names: the binary is stripped, and RTTI analysis did not run or found nothing. The Microsoft demangler named a few CRT functions.

## Findings: frame pacing / main loop / delta time

**Game time is tied to the frame rate (fixed step per frame).** Logic does not integrate measured wall-clock dt.
- The frame-timing object is a static at RVA **0x4759090**, and `*(u64*)0x4759910` points to it.
  - `+0x614`: active target FPS (30, 60 or 120).
  - `+0x618`: requested target FPS for the next frame.
  - `+0x61c`/`+0x620`: a second FPS pair. It is also filled from REN_FPS and is used as the divider `+0x614/+0x61c`, stored at `param_3+0x1e6`.
  - `+0x624`: time scale (copied from `+0x628`; forced to 0 when the pause flags `+0x58b`/`+0x58c` are set).
  - `+0x62c`: fractional 60 Hz frame accumulator.
  - Live values in the dump: 60/60/60/60, scale 1.0.
- The per-frame update is `0x5ad5f8`, called from `0x5acee8`. It applies `+0x618` to `+0x614`, re-reads the cap, and computes **dt60 = scale·60/targetFPS**.
  It feeds dt60 into the game-mode update (`0x8c8e54`, `0x5ab644`), then runs the dynamic-FPS monitor `0x5af410`.
- `0x46484` returns `scale*60/targetFPS` and is the delta getter used everywhere (39 callers). `0x46594` returns the target FPS (93 callers).
  As a consequence, when the GPU or CPU cannot hold the target, **the game slows down** rather than taking bigger steps. Changing `+0x614` to a value other than 30, 60 or 120 changes game speed for every system that assumes these steps.
- `0x5af604` is the REN_FPS cap: `{30,60,120}[settings[9]]`. **Only 30, 60 and 120 exist.** There is no 144 or uncapped option in the descriptor table.
- `0x5af410` and `0x5af4e4` are the **dynamic-FPS governor**. They keep a 10-sample ring of frame timings, store the average at `+0x7d8+0x74`, and step the target 120→60→30 or back up, only when DYNAMIC_FPS is on (`0x45baf58`).
  The Nioh3Fix hooks at 0x5af50a and 0x5af5c2 sit inside this function, on the DYNAMIC_FPS check and the commit of the new target.
- **No CPU-side frame limiter loop was found.** QPC has 49 callers and none of them is a sleep-until-deadline limiter. The cap appears to come from present and VSync, the DXGI frame-latency waitable object, Reflex/XeLL sleep, and the FFX frame-interpolation swapchain. That pacing path is:
  - `0x4a529c` (render submit, called through vtable `0x577e50`): when a per-frame flag is set (a byte returned by `0x247708`, at `[rsp+0x2564]`), it **busy-polls** in a tight loop at
    **0x4a5348–0x4a5357**: `pause; WaitForSingleObject(h, 0); jnz loop`.
    `h = (*(*(0x45ba588)+0x4278))->vtbl[0x108/8 = 33]()`. Slot 33 matches `IDXGISwapChain2::GetFrameLatencyWaitableObject`; this is likely, not proven.
    So the frame-latency wait burns a core instead of blocking. This is a candidate for CPU and power savings: make EDX non-zero at 0x4a534a, either INFINITE or a bounded timeout. `xor edx,edx` is only 2 bytes, so this needs a mid-function hook or a code cave, not an in-place patch.
    The same function then calls `xellSleep` (Intel XeLL; INTEL_XELL is OFF at dump time) and runs Streamline/Reflex markers.
  - `0x6b8e28` is `slReflexSleep` ("Reflex_Sleep"). `0x6b9014` is the PCL_PresentStart marker. `0x30b9cc` formats the Reflex latency report (simDeltaUs, renderDeltaUs, presentDeltaUs and so on).
  - `0x370f41c` waits on an FFX/FSR3 frame-interpolation fence. It either **spin-polls `ID3D12Fence::GetCompletedValue`** or uses SetEventOnCompletion with WaitForSingleObject. The choice depends on the byte at `0x4bd2580`, which is written by `0x39a39c`.
    The byte is **1 (event wait) in the dump**, so it does not spin with the current settings (FRAME_GENERATION=FSR). Callers are `0x37778b4` and nearby functions, which are the FFX swapchain present threads.
- Timers: `0x30bca40` is a 30 ms periodic waitable-timer heartbeat thread at THREAD_PRIORITY_TIME_CRITICAL, created by `0x30bc7f8`. It is not the frame limiter.
  `timeBeginPeriod` and `timeEndPeriod` are called only by the HID/DirectInput reader threads `0xaef900`, `0xaf07d0` and `0xaf14e0`, which also run at TIME_CRITICAL priority with period `0x44b68f0`.
  No ntdll imports, so no NtDelayExecution. `Sleep` has 361 callers, mostly `Sleep(0)` and `Sleep(n)` backoff in the job system (for example `0x207200`) and in loaders.
- Job system / spin waits: `0x206be4` spins on `TryEnterCriticalSection` plus `SwitchToThread` for up to 5000 ticks. The task names are `[App]*Thread` strings such as PreUpdateThread `0xe6f930`, GameModeDrawThread `0xe6f8f0`, StageUpdateThread `0xfb2760`, UpdateStageEffectThread `0x100d360`, UpdateAmbientEffectThread `0x1016fa0`, PreUpdateMenuThread `0x1138fb0`, UpdateScreenLayoutThread `0x1305450`, OceanThread%d `0x79f62c` and KTGLSoftBodyWorker `0xc4e260`.
- Script-side 30 Hz logic: the binding names `Shot::Is30Fps`, `Shot::IsSecondHalfUpdate30Fps`, `Shot::SetIsDelayStageHitOn30Fps` and `Groupthink::GetNumberToBeAddedToTimerByFPS` show that some gameplay (projectiles, AI) runs at 30 Hz on alternating frames.
  Also present: `Time::GetTimeStep`, `g_timeStep`, `fTimeStep`, `DeltaTime` (post-FX shaders), and `MinimumDeltaShaderTime`.

## Findings: config.xml (settings)

- The descriptor table is at **0x45c8570**: 60 entries × 0x30 bytes, `{char* key; char** options; ...; int count @+0x24; int min @+0x28; int max @+0x2c}`. It is built once by `0x1fe14cc`.
  The XML path format is `//%s/text()`, used by the reader and writer `0x1fe4244` and `0x1fe5d8c`, under root `settings`.
- **Live values: int array at 0x45baf30, where index = table order.** Verified, for example REN_FPS = `0x45baf54` = index 9 and DYNAMIC_FPS = `0x45baf58`. Values at dump time:
  BORDERLESS, VSYNC ON, 2560x1440 output, REN_REZ 1664x936 (REN_RAT 65%), **REN_FPS 60, DYNAMIC_FPS ON**, PRESET STANDARD, SHADOW STANDARD, AF 4, AO STANDARD, GI STANDARD, SSR STANDARD,
  MODEL_QUAL HIGH, MODEL_TEX STANDARD, MODEL_DRAW MANY, VOLUMETRIC STANDARD, MOTION_QUALITY HIGH, LANDSCAPE NORMAL, **MESHLET_QUALITY HIGH**, GRASS HIGH, **EVENT_FPS 30**,
  POSTEFFECT_RESOLUTION STANDARD, MOTION_BLUR STANDARD, DYNAMIC_RESOLUTION 60FPS (MAX 65, MIN 50), RESOLUTION_SCALING FSR, **FRAME_GENERATION FSR**, REFLEX OFF, XELL OFF, **CPU_SPEC_CLASS STANDARD**.
- Option ranges: REN_FPS {30,60,120}; EVENT_FPS {30,60}; CPU_SPEC_CLASS {LOW,STANDARD,HIGH}; MESHLET_QUALITY {LIGHT,STANDARD,HIGH}; MOTION_QUALITY {LIGHT,STANDARD,HIGH};
  LANDSCAPE_QUALITY {LIGHTEST..ULTRA}; GI {LIGHTEST..HIGHEST} (6 levels); DYNAMIC_RESOLUTION {OFF,30..120FPS}; DLSS_FRAME_GEN_MULI {2X,3X,4X}; NVIDIA_REFLEX {OFF,ON,ON_BOOST}.
  Out-of-range values: getters such as `0x5af604` clamp to the first option.
- **Orphan or hidden keys:** `MIPDOWNLEVEL`, `MIPDOWNLEVEL_PLAYER/_CHARA/_STAGE/_RENDERTARGET`, `DEBUG_JUMP`, `REMOVE_MAP_CLOUD`, `DISP_DAMAGE_HEAL`, `DEBUG_DAMAGE_PC` and `DEBUG_DAMAGE_ENEMY_NPC` sit in the key-string pool (0x3a9a780–0x3a9a9f0) but are **not** in the 60-entry table.
  They have zero xrefs, so they look compiled out of retail. Putting them in config.xml will most likely do nothing.
- Other debug strings that are still referenced: render-parameter names in `0x265e814` (`RadenDebugMaxLevel`, `DebugDisplayType*`, `SamplerFeedbackDebugMode`, `OBOROHeightMapDebug*`, `MeshletGeometryDensityDebug` `0x2438ed8`) and `EnableDebugHistgram/ExposureMask/BloomSoftKneeCurve` (`0x300af10`, `0x3447980`).
  The debug FPS overlay string `"FPS     %2.2f(%4.2f)"` (0x4099fb0) has no code xref.

## Findings: camera / FOV / resolution anchors (see anchors.md)

- 0x8781f8 is not a FOV function. It is in the **WndProc** `0x878054`, in the resize and display-mode path: it reads DISP_MODE `0x45baf30`, MONITOR_ID, and the output size at `0x45bb020`/`0x45bb024` (width/height).
  0x28a6198 is an **aspect-ratio scale** (16:9 / 21:9 / 32:9 reference ÷ current aspect) used for UI/HUD and camera. These are ultrawide hooks.
- Camera-related types and bindings: `Game::Camera::CameraModeFps` (typeid string), and `kids::placeable::camera::Commit{SuperSampling,VolumetricFog,PSSR,OutlineHighlight,SaturationFader,...}` (registration functions near 0x247xxxx and 0x249cxxx).
- Resolution anchors `0x1fde3e8` and `0x1fde504` are small getters on the SCR_REZ/REN_REZ tables (settings indices 6 and 7).
- The LOD anchors are spread over 0x0de8d0–0x39a86c. The containing functions and callers are in anchors.md. `0x39a39c` (2 LOD sites) also writes the FFX fence-wait mode byte `0x4bd2580`, so it looks like a "graphics settings apply" function.

## KATANA / engine identification

- 24 strings contain "katana", nearly all gameplay (Katanaduka "sword grave": `Online::Katanaduka*`, `katanaduka_position_*`). The engine-side hit is `kids::E_KATANA_SCENEDATABASE_STACK_ID` inside a mangled name at 0x455aba0.
  The engine namespaces are **`kids::`** (1,016 strings: scene, automaton/AnimGraph, placeable, module script bindings) and **`ktgl`** (423 strings: Koei Tecmo Game Library, for example `KTGLSoftBodyWorker` and `ktglkids_scl_`).
- Middleware: NVIDIA Streamline (`sl.interposer.dll` provides D3D12CreateDevice and CreateDXGIFactory*, called from `0x2ee300c` and `0x2ee1bc0`), AMD FidelityFX FSR3 upscaler and frame interpolation (`FFX_*`, region 0x3715xxx–0x377xxxx), Intel XeSS, XeSS-FG and XeLL,
  AMD AGS, MediaFoundation (movies), XAudio2 and WASAPI (audio thread `0x1a7131c`), Steam API.

## Gotchas
- The `FUN_...` names are absolute addresses. Subtract 0x7FF74F410000.
- Small leaf functions have no `.pdata` entry, shown as `pdataStart=none`. Ghidra still has them.
- `.pdata` chained entries: function bodies are split, for example 0xde8d0 lives in a fragment of 0xddc70. The query tool resolves this.
- RTTI and class names are absent. Vtables and callbacks show up as `DATA` references in `c:` output.
- The Ghidra project cannot be opened twice. Always use `decomp.cmd` (mutex) or stop all queries first.
