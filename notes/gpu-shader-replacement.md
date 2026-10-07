# GPU shader replacement — GHDM depth min/max downsample

Status 2026-10-06: **replacement built and staged, not installed.** Nothing in the game folder has been
modified. Everything is in `nioh3-engine/gpu-shaders/`.

## 1. shader_dx12 format and pipeline → file mapping

**Container `*.file` ("IDRK0000")**: 0x38-byte header (u64 file size, u64 compressed size, u64
uncompressed size, u32 0, u32 resource hash = file name, u32 type hash 0x7bcd279f, u32 flags 0x00100000,
u64 0). Then zlib chunks of 16 KiB uncompressed, each `u32 zsize + zlib stream ("78 9c")`, terminated by
`u32 0`.
- No checksum, no signature, no encryption.
- 24 of the 3897 files use flags 0x00400000 with an extra header; 2 of those don't parse yet. Not needed here.

**Payload "_S2G7000"**: one shader *group* (technique) with all its permutations.

| Offset | Field |
|---|---|
| +0x08 | u32 total payload size |
| +0x10 | group name (e.g. `GHDM`, `RefProbe-Cll…`, `VFSCAT-…`) |
| +0x98 | u32 offset of the record table: (u32 offset, u32 size) per shader, relative to the blob base |
| +0xB0 | u32 blob base |
| +0xB4 | u32 blob bytes |

- Before the blobs: the engine's binding/name tables, plus 16 bytes per shader. Those 16 bytes look like a permutation key; they don't match MD5 of the DXBC or the DXIL.
- Records are either plain DXBC (compute groups such as GHDM) or `"CXDK2000" | u32 dxbcSize | u32 tailSize | DXBC | engine reflection tail` (graphics groups).
- The DXBC is standard DXIL (SM 6.x, validator 1.5) with reflection (STAT) and an ILDN debug name, so `dxc -dumpbin` shows full resource names and cbuffer layouts.

**Mapping RGP → file (exact):**
- PAL's code-object metadata `.api_shader_hash` is the D3D12 DXBC container digest: the second u64 holds digest bytes 0..7 and the first holds bytes 8..15.
- `gpu-shaders/idrk.py index` hashes all 36,312 DXBCs into `dxil_index.csv`.
- `patcher/gpu/rgp_report.py --dump-shaders` now writes the `file/group#index` of every hot pipeline into `pso_map.csv`.

The target pipeline:
- RGP PSO `e6429cd12f46fbbb`, api_shader_hash `[0x191c842a88bd36cb, 0xa208cf9e529c42b6]`
- = DXBC digest `b6429c529ecf08a2cb36bd882a841c19`
- = **`shader_dx12\0x0104840c.file`, group `GHDM`, variant 0** (of 4: Texture2D / +SrcLevel / Texture2DArray / TextureCubeArray).
- `rga -s dx12` on the original DXIL reproduces the captured ISA: 9 VGPRs, 2× image_load, 2× image_sample_lz in serial loops.
- It runs right after the shadow-map draws (`SM-*`, `SMTREE2-*`), and its output is 512² (64×64 groups of 8×8 threads). With the 4096² shadow map from game-data that is an integer 8×8 footprint: 64 serial loads per thread.

**Original semantics** (reconstructed from the DXIL; names come from its reflection):
```
cbuffer $Globals { uint nSrcOffsetSize[4]; uint nDstOffsetSize[4]; uint nIsReverseZ; }   // 120 B
SamplerState __smptSrc : s8;  Texture2D<float4> tSrc : t8;  RWTexture2D<float4> uDst : u0;  [numthreads(8,8,1)]
if tid < dstSize:
  if srcSize % dstSize == 0:  r = srcSize/dstSize; acc = reduce(tSrc.Load(srcOff + tid*r + (x,y)))  over r.x*r.y
  else: n = ceil(srcSize/dstSize); acc = reduce(SampleLevel at a UV grid clamped to (srcSize-0.5)/texSize)
  reduce = MIN (init 2.0) if nIsReverseZ else MAX (init -2.0);   uDst[dstOff + tid] = acc.xxxx
```
Each loop iteration issues one fetch and waits for it (`s_wait_samplecnt 0` / `s_wait_loadcnt 0`), so the
shader is latency-bound: 504 µs + 81 µs + 70 µs per frame.

## 2. Is it validated / can it be overridden?
- **Loader** (Ghidra, RVA 0xa300b0, called through the resource-type resolver for 0x7bcd279f at 0x10e50f0) builds `<root>../../shader_dx12/0x%08x.file`. A missing file falls back to `PB2Unknown.file`. It is a plain loose-file load through the KTGL file system.
- The IDRK header has no checksum field, and the S2G group has only offsets and sizes.
- No hash or size table for shader_dx12 was found in the exe strings or in the package index.
- **No on-disk PSO cache:** `%LOCALAPPDATA%\KoeiTecmo\NIOH3` holds only config and saves. `PreloadPSOMode` builds PSOs at startup from these files. The AMD driver cache is keyed by bytecode, so a new shader just compiles once.
- D3D12 itself only checks the DXIL signature. The replacement is compiled and signed with the Microsoft-signed `dxc.exe` + `dxil.dll` 1.8.2502 bundled with RDTS (`tools\rdts\…\utils\dx12\dxc\`), which Smart App Control allows. It is compiled `-validator-version 1.5`, the same as the shipped shader.
- So **replacing the loose file should work**. That is untested in-game; the A/B is the test.

## 3. Replacement shader
`gpu-shaders/GHDM_fast.hlsl`:
- **Same interface as the original:** same registers (b0, s8, t8, u0, space 0), same `$Globals` layout (120 B), same resource names, cs_6_0, numthreads 8×8×1, same output.
- **Integer path:** `GatherRed` (2×2 texels per fetch), 8 independent gathers per loop iteration (an 8×4 texel tile), clamped so duplicate texels never leave the footprint. min/max are idempotent, so the result is **bit-identical**. For the 8×8 case that is 2 round-trips instead of 64. A batched-`Load` fallback handles footprints with width or height 1.
- **Non-integer path:** the exact original UV formula, with 4 `SampleLevel` taps per iteration.
- **Driver ISA** (`rga -s dx12 -c gfx1201`): 8 `image_gather4_lz` issued back-to-back before one wait; 44 VGPRs, no LDS, no scratch.
- Expected cost is bounded by bandwidth: 4096² × 4 B = 64 MiB ≈ 0.1 ms. So about **504 → 100–150 µs** for the big call, with a smaller gain on the 81/70 µs calls. Roughly **0.4–0.5 ms per frame** in total; to be measured.
- Not done: a GPU bit-exactness test (it would need a D3D12 host; no native binaries here). Gather-at-texel-corner is a standard, exact technique, but the visual A/B below is the real check.

Files:
- `idrk.py`: unpack / pack / index IDRK + S2G (packs with the original header fields, 16 KiB zlib chunks).
- `build_ghdm.py`:
  - backs up the original to `backup\0x0104840c.file` on first run;
  - compiles, splices variant 0 and fixes the S2G sizes/offsets (other variants stay byte-identical);
  - re-packs to `replacement\0x0104840c.file` (14,429 B vs 12,534 B) and verifies all records.
- `apply_ghdm.ps1` (also `-Revert` / `-Status`):
  - SHA-256 checked;
  - refuses if Nioh3 is running or the game file is unknown (e.g. after a game update).
- `extract/GHDM_*.ll`: original DXIL disassembly. `dxil_index.csv`: all game DXBCs, plus the replacement's digest `e238b6d0c327a3f6267b4d989f91b777`.

## 4. A/B plan (orchestrator)
1. Baseline (current files, game closed): `apply_ghdm.ps1 -Status` → `ORIGINAL`. Arm the RGP daemon, launch, go to the same shrine spot, run PresentMon for 60 s, then `RGP_TRIGGER`. Take a screenshot of an area with strong shadows (and a few tree shadows).
2. Close the game. `powershell -ExecutionPolicy Bypass -File gpu-shaders\apply_ghdm.ps1` → `FAST (replacement)`.
3. Same launch, spot and settings: PresentMon 60 s, RGP capture, same screenshots. The first launch may hitch once while the new PSO compiles.
4. Analysis: `rgp_report.py <trace> --dump-shaders`. In `pso_map.csv` the GHDM dispatch should now show digest `e238b6d0…` (`GHDM-FAST(replacement)`). Compare that segment (was 504 µs) and the frame time.
5. Visual check: shadows (cascade transitions, trees) and anything that culls against the shadow Hi-Z. Missing or flickering shadows would mean a semantic mismatch → revert.
6. If the game crashes at load or the shadows break: `apply_ghdm.ps1 -Revert`. Steam "verify integrity" also restores the file.

## 5. If file replacement does not work: next-best routes
- **Native D3D12 hook** on a PC without Smart App Control, or a handheld: hook `ID3D12Device::CreateComputePipelineState` and swap bytecode whose DXBC digest equals `b6429c52…`. The same staged DXIL works unchanged. OptiScaler already proxies `dxgi.dll` here; an OptiScaler plugin or ReShade add-on could do the swap without a new loader.
- Memory patching (`patcher\Mem.fsx`) is not a good fit: the PSO is already compiled when the game is running. Replacing the bytecode buffer before PSO creation is possible but racy.
- The same file route now works for **any** game shader, because every hot RGP pipeline maps to `file/group#index` automatically. Candidates from `notes/gpu-profile.md`:
  - `ESM` filtering (240 µs);
  - `WDO-PtSrbVsm` (366 µs, serial loop);
  - the second GHDM call sites;
  - `RefProbe-*` lighting: big, so riskier.
