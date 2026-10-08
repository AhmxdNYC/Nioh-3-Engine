# Exe-embedded / off-disk post shaders + PSO bytecode swap (psoswap)

2026-10-07. Inputs: `notes\gpu-shader-sweep.md` §5.2, `notes\invisible-gpu.md` §4a, r5 RGP `pso_map.csv`, `re\Nioh3.dump.exe`. Game not run.

Ally cost and the item-3.3 verdict (2026-10-08, GP2 1.7.5): `notes\gpu-post-runtime.md` §7.

## 1. Where the "~1.4 ms exe post chain" really lives
- **Nioh3.exe** has 271 DXBC containers (258 unique) in .rdata: 139 SM5 CS (GPU particles, hair, fluid), 77 SM5 PS, 5 VS,
  3 GS, 2 SM4 PS, and 32 DXIL (embedded FSR3 upscaler + frame interpolation). Inventory with r5 times:
  `gpu-shaders\post\out\inventory.csv`, `summary.txt` (rebuild: `py -I gpu-shaders\post\inventory.py`, `disasm_all.py`).
- In the exe and costly in r5 (µs, PC): 449cf0ae TAA-style filter 247, 7c5acaaf 4-target copy 224, 8d25b0cd `far_z` 107,
  29ecb6e4 72, 0cbad2cb CustomDepthUpscale 66, 5fccb795 depth copy 51, particle CS 5e771e53 100 (+ ~10 more particle kernels).
  a2bd0e86 (881) is the shared full-screen VS: that time belongs to its pixel shaders.
- **Not in the exe:** bloom 17149dca (131) / 18a4c2bb, compose 6a305be4 (47), df4adc57 (86), their VSes c5909527 / d21b4b07,
  and the MB / DOF passes. The full 16-byte digests are in no file of the game folder (every exe/dll incl. OptiScaler's
  dxgi.dll, all 3,897 `shader_dx12` files decoded, raw search anywhere in the bytes, not only after "DXBC").
  The 24 `.file`s that `dxil_index.csv` skips are IDRK files with flags 0x4000 (10-byte chunk headers in place of the u32 size;
  decode by scanning for the zlib streams). Each holds 3 big DXIL libraries (RT), none of the post shaders. **Conclusion:** these PSOs are built from bytecode made at run time
  (the exe imports `DxcCreateInstance`; `dxcompiler.dll` ships with the game). Only a PSO-creation hook can see them.
- **2493f9d9 + d4974586** (r5 seg 389, 186 µs, the "3.7 % compute with no fetches") are in `amd_fidelityfx_upscaler_dx12.dll`
  = FSR4 itself, not game post. Leave them alone.

## 2. Rewrites (part 2): none shipped, and why
- 449cf0ae (TAA filter, ps_5_0, 118 slots): 5-tap cross from t0 at clamped UVs, velocity t2, history t1 = 7 fetches, all
  distinct. When `vCustomParams.x > 0` it also does 5 stencil `gather4` (12 distinct texels, overlapping 2×2 footprints).
  - A pixel shader has no LDS, so the TOONEDGE2 tiling template does not apply.
  - 3 gathers in place of 5 would rely on how the hardware rounds texel selection from interpolated UVs. That cannot be
    proven identical offline.
  - Quad sharing (QuadReadAcross) needs SM6, so the shared VS a2bd0e86 would have to become DXIL too. Its coordinates also
    differ in the last bit (neighbour's interpolated UV vs `v2 + dx`).
  - The ~60 ALU ops are not the limiter.
- The copies (7c5acaaf, 5fccb795, d787dbd0, 9–10 slots) are bandwidth-bound draws. A PSO swap cannot turn a draw into
  `CopyResource`.
- So there is no output-identical, measurable win in the exe-resident post set. The expensive targets (MB 10 samples, DOF,
  bloom, compose) need their bytecode first. That is step 4 below.

## 3. psoswap (ASI module, default OFF): `mod\Nioh3EngineFix\src\psoswap.{h,cpp}`
- Hooks the device returned by `D3D12CreateDevice` (IAT slot of `sl.interposer.dll` or `d3d12.dll`; by name or ordinal).
  On that device it hooks `CreateGraphicsPipelineState`, `CreateComputePipelineState`, `CreatePipelineState` (stream) and
  `CreatePipelineLibrary` → `LoadGraphics/Compute/Pipeline`.
- A replacement is `<Folder>\<key>.dxbc|.dxil|.cso|.bin`. key = container digest, or for DXIL the HASH-part digest (= the
  r5/RGP digest); either name works.
- Per PSO it swaps only the matching stages, keeps the caller's desc untouched and drops CachedPSO. Same stage is required,
  and a PSO is never mixed DXBC+DXIL.
- If the runtime refuses the swapped PSO: the original desc is used, the key is disabled and the event is logged.
- Exe-resident shaders are logged once (`exe <stage> <key> <size> rva=`). `LogAll=1` logs every other blob with its module
  (`heap` = built at run time). DumpMode 1 = exe blobs, 2 = all blobs, written to `<Folder>\dump\`.
- **Test** (real D3D12 device on this PC, no game): `py -I mod\Nioh3EngineFix\tests\run_psoswap_harness.py` → `ALL PSOSWAP CHECKS OK`.
  - Swaps tested: the IAT hook (ordinal import), CS swap, stream swap, pipeline-library swap, DXIL swap keyed by the HASH digest.
  - Fallbacks tested: no file, VS-for-CS file, garbage file, runtime-rejected replacement (original output, key disabled), DumpMode 2.
  - Graphics PSOs share the code path but were not dispatched in the harness. Builds clean with the ASI flags (/W4).
- **Integration (ASI src owner)**, details in the psoswap.h header:
  1. Add psoswap.cpp to build.bat and CMakeLists.
  2. In dllmain Worker, early (after the Proton FG guard, before the SteamStub wait): `psoswap::Start({&Log, iniPath, wine})`.
  3. In the osd InstallThread: `if (psoswap::GetState() != psoswap::State::Off) psoswap::InstallNow();` (late fallback).
  4. Optionally add `StatusLine` to the stats line.
  5. Add the ini section `[ShaderSwap] Enabled=0, Folder=Nioh3EngineFix_shaders, DumpMode=0, LogAll=0`.

## 4. Next steps
1. After integration, do one PC run with `[ShaderSwap] Enabled=1, DumpMode=2, LogAll=1` and no replacement files. psoswap
   only observes in this mode, the PSOs stay unchanged. Then grep the log for `hash=17149dca` / `6a305be4` / `df4adc57` and the MB/DOF pixel shaders
   (`ext ps … heap`). Their blobs are in `Nioh3EngineFix_shaders\dump\`.
2. Disassemble the dumps. If they are DXIL, the `gpu-shaders\sweep` interpreter proof flow applies directly (sim.py
   covers CS; PS needs the pixel-shader port noted in gpu-shader-sweep.md). Then rewrite MB (10 taps) and DOF first and
   ship the replacements as `<hash>.dxil` in the psoswap folder.
