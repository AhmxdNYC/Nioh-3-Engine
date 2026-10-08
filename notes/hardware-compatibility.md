# Hardware compatibility: NVIDIA, Intel Arc, other AMD GPUs, Intel CPUs (2026-10-08, ASI 1.7.33)

Read-only code review (no game run, no build). Question: does the mod work, and help, on hardware we cannot test
(NVIDIA, Intel Arc, AMD cards other than the RX 9070 / 780M, Intel CPUs)? Sources: every key of
`mod\Nioh3EngineFix\Nioh3EngineFix.ini`, `src\*` (all modules, `caves.asm`, `drawprep.asm`, `gpuopt_thunks.asm`),
`release\profiles.json` + `package.py`, the shader pack in `gpu-shaders\` (GHDM, ESM, wdo14/28, fog, GI, culling,
sweep, materials) with their build/apply scripts.

## Verdict

- **No cross-vendor risk found in any default-ON feature or any shipped profile entry, so no code change.** Every
  feature that is ON by default or in a profile either patches game CPU code only, or uses D3D12/DXGI the way the
  spec says. None of them depends on the GPU vendor.
- **No vendor SDK anywhere.** There are no AGS/ADL/NVAPI/IGCL calls, no Anti-Lag hooks and no DXGI VendorId branches in
  the mod. The only vendor branch involved is the game's own: `226A490 cmp [rbp+70h],10DEh` (async slot 0, see AS0).
- **No AVX, AVX-512, BMI, MONITORX/MWAITX or TSX in any cave.** Every cave uses only baseline x64 (SSE2,
  `prefetcht0`, `pause`, `lock`, `mfence`, `rep stos`). There are three exceptions, and they all use SSE4.1:
  - MQ (`insertps`/`extractps`): guarded with `IsProcessorFeaturePresent(PF_SSE4_1)`.
  - AABB (`blendps`) and VRR `MenuQuantize` (`roundss`): **not guarded, but default off.**

  Any CPU that can run Nioh 3 has SSE4.1 (Intel since 2008, AMD since 2011).
- **No shader uses wave intrinsics or assumes wave size.** We grepped every replacement shader for
  `Wave*`/`QuadRead*`/`WaveSize`. "wave" only appears in comments.
  - All groupshared use is behind `GroupMemoryBarrierWithGroupSync`.
  - The only implicit-lockstep code is in the *shipped* MshLod#3: a 32-thread group that relies on one wave. Our
    replacement adds a group barrier, so it is the safer one on Intel SIMD8/16.
  - DXIL is vendor-neutral. Bit-exactness is proven offline (DXIL interpreter, or by construction for GHDM). Tuning
    (VGPR/occupancy via RGA) was done for gfx1103/gfx1201 only.
- **The game behaves differently on NVIDIA, and the mod does not depend on it.** On NVIDIA the engine keeps async slot 0
  on (`KIDS_ASync_LLCulling_Effect`). That moves effect recording (~6-7 % of the main thread on PC) from `[App]Game`
  into a KIDS job (notes\particles.md). NVIDIA drivers `<= 0x1C92` get all async slots off (226A571).
  - Effect: the main-thread profile on NVIDIA differs from our RX 9070 numbers, so CPU gains will be a little
    different there.
  - No patch site depends on it. A site that NVIDIA does not execute is simply inert.
- **Hash/build checks.**
  - The ASI refuses every build except v2.02 (PE timestamp 0x6AA7C106) unless `AllowUnknownBuild = 1`, and checks
    every site byte-for-byte.
  - Shader files: `build_*.py` asserts the original record digest, `apply_*.ps1` only overwrites the known v2.02
    original (sha256), and `package.py` pins the shipped GHDM sha.
  - **Gap:** the zip's manual step (copy `optional\shader_dx12\0x0104840c.file`) has no check. After a game update
    that changes that file, copying it would put back the old v2.02 file. This is a build risk, not a vendor risk.
    USER_GUIDE's "Optional shader" section doesn't say "v2.02 only". Suggested: add one line there, and/or have the ASI
    hash `shader_dx12\0x0104840c.file` at start and warn when it is our replacement on a non-v2.02 build.

## Classes

- **(a)** vendor-neutral: patches game CPU code / Windows APIs only.
- **(b)** D3D12/DXGI, vendor-neutral, spec-correct.
- **(c)** vendor/driver-specific assumption.
- **(d)** Proton/Wine only.

Defaults are listed as base ini / PC profile / ALLY profile.

## Table: every key

| Feature / keys | Default | Class | NVIDIA / Intel Arc / other AMD? | Expected benefit there (why) | Risk + recommended guard |
|---|---|---|---|---|---|
| `[General]` Enabled, Log, AllowUnknownBuild, DecryptWaitSeconds | 1,1,0,60 | a | yes | — | AllowUnknownBuild is a build risk, not a vendor risk |
| Overlay + StartupPopup (`[Overlay]` all keys, StartupPopup*) | on | b | yes. DXBC SM5.0 shaders, own root signature/PSO; PRESENT→RT→PRESENT barriers on the swapchain's own DIRECT queue (`GetDevice(ID3D12CommandQueue)`, fallback = game queue on the same device); formats RGBA8/BGRA8/RGB10A2/FP16/sRGB; HDR from SetColorSpace1 | — (display only) | (1) "FG FPS"/FG status logic knows the FSR presenter thread by name. With DLSS-G or XeSS-FG it falls back to present-rate ratios, so the label may be wrong. It can show "DLSS FG selected but no generated frames reach the screen - try FSR" falsely (cosmetic). (2) Learns the DXGI vtable from a throw-away device on the *default* adapter. On iGPU+dGPU laptops that may be the iGPU: vtable is the same dxgi.dll class; on failure the overlay just stays off. Guard: none needed; suggest wording DLSS/XeSS FG as "unverified" instead of "try FSR" |
| `[Menu]` all keys (Key, Gesture, Tap*, HoldButton, PauseHint*, KeyTripleTap, Scale) | on | a | yes (dinput8/XInput hooks, engine flags) | — | none |
| `[Menu] FrameGenToggle` | 1 | a/b | yes. Calls the game's own `FrameGenSettingWrite`, which re-checks `IsFrameGenTypeAvailable` (DLSS-G = Streamline caps, XeSS = game flag, FSR = always) | same as the game's own option | none (unavailable types are skipped and the game's setter refuses them) |
| AutoContinueLoad | 0/0/0 | a | yes | skips tip page anywhere | none |
| W1, W2 (write-combined upload memory) | 1 | a | yes. D3D12 UPLOAD heaps are write-combined on every vendor | **yes, on any CPU-limited PC**: main-thread time; slower CPUs gain more | none |
| VRR, VRROnline, `[VRR]` Tier*/Min*/Max*/MaxFps | 1, -1 | a | yes | **yes, anyone below the fps cap** (game time stops slowing down) | MenuQuantize (default 0) uses `roundss` without an SSE4.1 check; add `IsProcessorFeaturePresent(37)` like MQ |
| W3, R1, L1 (+ `[Tuning]` W3/R1/L1_*) | 1 | a | yes (L1 needs Win10 1803+ high-res timer, checks itself) | CPU power/heat, laptops/handhelds; fps-neutral when GPU-bound | Intel hybrid / dual-CCD AMD: a sleeping thread may wake on another core (scheduler), same as any sleep; not a correctness issue |
| WK | 1 | a | yes | fewer futile wakes, power | none |
| WorkerCap | 0/0/10 | a | yes. Lowers the game's `clamp(logical,6,32)` pool base; logs and skips when logical ≤ cap (e.g. 8-thread Claw) | handhelds only | none |
| P12 | 0 | a | yes. The game's own ≤12-thread rule (Intel 6c/12t, Ryzen 5) | measured none on 7600X | none |
| **AS0** | 0 everywhere | **c** | **NVIDIA: no-op** (game already enables slot 0). **Intel Arc: forces an async path KT never ships on Intel.** Other AMD: same as Ally/PC (untested beyond them) | AMD only, PC measured neutral | Possible hang/corruption on Intel. Default off, so no code change. Guard if it ever ships: apply only when the game's adapter VendorId is 0x1002 (skip 0x10DE as a no-op, refuse 0x8086) |
| W3WineSleep, L1OnWine, FGOnWine | 1,0,0 | d | Proton on any GPU | Proton only | none |
| FG (F1: FSR FG presenter sleeps) | 1 | a | yes. Patches the FidelityFX FG presenter code inside Nioh3.exe; runs whenever *FSR* FG is used (FSR FG works on NVIDIA/Intel) | ~0.2 core with FSR FG; **nothing with DLSS-G / XeSS-FG** (that code never runs) | none |
| W6, GF (+ W6_*, GF_TimeoutMs) | 0 / 1 prov / 1 | a/b | yes. GF uses `ID3D12Fence::SetEventOnCompletion` (spec) | CPU power; PC +4 % (one window, provisional) | none |
| B, PP (+PP_Period) | 0 / 1 prov / 0 | a | yes | main thread | **B is not a vendor risk but a correctness question.** profiles.json flags "suspected draw loss", and the README says "B dropped on PC (-0.1 %)", yet the PC profile still has B = 1 (provisional). Resolve before a final PC zip |
| MQ | 0 / 1 prov / 1 prov | a | yes; SSE4.1 checked at install | scheduler threads | none |
| FPC, M1, M2, J1, X1, E1, S1, PT1, DL, CpuStats, RWb, GL1, CP, SPF, P1E7, DQ, DS, TG, IK, TS, WakeStats, ED, WG (+ their `[Tuning]` keys) | 0 | a | yes | CPU-limited systems; size depends on the CPU's cache/memory latency (prefetch distances were tuned on Zen 4) | LK/J1/M2 (locked loads → plain loads) rely on x86-TSO and a Dekker screen. That argument holds on Intel identically, but it was only run on Zen 4. S1's PAUSE costs more cycles on recent Intel cores; it delays a spin exit by < 0.1 µs |
| AABB | 0 | a | yes | game-update thread | `blendps` (SSE4.1) without a CPUID check; add one like MQ before shipping it |
| LK | 0 | a | yes | see above | see above |
| WL (frame-latency handle leak) | 1 | b | yes. DXGI returns a new duplicated handle per `GetFrameLatencyWaitableObject` call; the cave closes the previous one only if the value differs | stops a handle leak on any Windows GPU | DLSS-G / XeSS-FG proxy swapchains are untested. A proxy that returns one cached handle is safe (same-value skip); one that cycles several owned handles would be closed under it (unlikely). Guard if seen: menu row "Close leaked handles" off. Log counter `stats/s WL` |
| SPR | 0 | a | yes | measurement | none |
| PJ, PJ_MaxMB, PJ_QuietMs | 0 | b | yes. Same `ID3D12Device` create calls; CachedPSO blobs are dropped, so no driver-blob replay | first-use PSO hitches on any driver that has a shader cache (NVIDIA, Intel, AMD) | Journal class = OS/UMA/SM, **not vendor**: after a GPU swap the old journal is replayed (only useless/failed creates, never a wrong PSO). Cheap improvement: add VendorId to `DevClass` |
| LC, LW, LoadOnlyCpuFixes | 0 | a | yes | loading, only where loaders are the bottleneck | none |
| EP | -1 | a (auto = d) | yes; auto = on only under Proton | Proton only (Windows events cost 0.6 µs) | none |
| SR, SH | -1, 0 | d | refused on native Windows; Proton on any GPU | Proton loading (Ally 47→38 s) | none |
| RC (+RCMB/RCKB/RCMaxReadKB), AQ (+AQThreads), PF (+PFThreads/LeadMB/Traces/MatchPct), TSY | 0 | a | yes | device-bound loads (HDD/slow SSD) | fixed 4 helper threads (AQ/PF) and TrySync 3..8 can oversubscribe 4-6-thread CPUs; all off by default |
| GIMerge, GIMergeQualify, `[Debug]` GIProbe* | 0 | b | yes. Matches shaders by digest (shader_dx12 files are shared by all vendors), rewrites the stencil ref in the game's own CB before the GPU reads it, self-disables | GPU-bound; the empty group-1 pass is cheap with any wave width, so similar small gain | none |
| `[Latency]` MaxFrameLatency | 0 / 0 / 1 | b | yes (`IDXGISwapChain2::SetMaximumFrameLatency`, only on waitable chains) | latency/fps trade | with NVIDIA Reflex on, leave 0 |
| `[GpuOpt]` Barriers, MergeAcrossBinds, DropDuplicateUav, DropUndoPairs, MaxBatch, DeferThreads, AnyCaller, Trace*, InstallDelaySec; TileBatch | 0 | b | yes (result-neutral merges per the barrier/tile spec) | PC (RDNA4) measured **-7 %**; unknown elsewhere | off; keep off |
| `[GpuOpt]` R2a, R2b, R1 | 0 | **c + d** | forced off on Windows. Under Proton they would also run on NVIDIA's Vulkan driver, though they were designed around RADV (`COMMON` → ALL_COMMANDS); R2b/R1 are not spec-exact | RADV only | default off. Guard if they ever ship: also require VendorId 0x1002 under Proton |
| `[Profiler]` MemStats, TileStats, GPUProf* | 0 | b | yes. Timestamps only on DIRECT/COMPUTE lists with per-queue `GetTimestampFrequency`; MemStats picks the adapter by LUID | measurement | MemStats' "first hardware adapter" fallback may report the iGPU on a laptop (stats only) |
| `[Profiler]` LoadStats, LoadGates, ReadProbe, SP (+ SP keys) | 0 | a | yes | measurement | none |
| `[ShaderSwap]` Enabled, Folder, DumpMode, LogAll | 0 | b | yes (DXBC/DXIL by digest; failure → original desc) | — | none |
| `[Memory]` LM (+LM_*) | 0 | a | yes (self-test) | RAM-tight systems | none |
| `[Debug]` BossLog, RematchLog | 0 | a | yes | — | none |

## Shader pack

| Shader (file) | Shipped? | Class | Wave/LDS | Same image on NVIDIA/Intel? | Benefit there | Risk / guard |
|---|---|---|---|---|---|---|
| **GHDM** fast depth min/max (`0x0104840c.file`, record 0 of 4; records are Texture2D / +SrcLevel / Array / Cube, not per vendor) | **yes, PC + ALLY "kept"** (optional manual copy) | b | none. cs_6_0, 8x8, no groupshared, no wave ops | **Yes, by construction.** Only min/max (idempotent, order-free); Gather at texel corners `(base+1)/size` puts the sample 0.5 texel from any footprint edge, and D3D requires ≥ 8-bit sub-texel precision; the non-integer path keeps the exact UV math. Never tested on a real GPU bit-for-bit, even on AMD (visual A/B only) | Gain measured on **RDNA3 (Ally +6.4-8.7 %) and RDNA4 (PC +3 %, ~+11 % with FG) only**. The original's serial one-fetch-per-iteration loop is latency-bound on any GPU, so expect a smaller but positive saving where GPU-bound; nothing when CPU-bound | build: digest assert, apply/package sha pins; manual copy unchecked and the USER_GUIDE section doesn't say "v2.02 only" (see Verdict) |
| ESM gather blur (`0x1ca8bb39`) | no (apply_esm) | b | none | near-identical: equal "up to the texture unit's sub-texel weight rounding", which is vendor-specific | model only | judge per vendor by screenshot |
| wdo14 VSM flag dedupe (`0x1667d3a0`), wdo28 blur (`0x1fd07cc0`) | no | b | per-thread only | wdo14 bit-exact (psim); wdo28 see its note | AMD occupancy tuned (48 VGPR, 12→10 waves on Ally) | other compilers allocate differently: may be neutral/slower |
| fog vf235 (`0xa9910db8`), GI rp121/rp414 (`0xd070067d`) | no | b | LDS from the original, barriers kept | DXIL-interpreter bit-exact | unroll/if-convert raises register pressure; RDNA-modelled only | may be slower on other archs; A/B per vendor |
| MPC#23/28-31 chain (`0x29ee0902`), MshLod#3 (`0x8d9282e6`), TOONEDGE2 (`0x9c481f79`/`0x47bc5cb4`) | no | b | LDS + group barriers; no wave-size assumption | proven chain-level (verify_culling) | small | none (MshLod replacement is *more* portable than the shipped one) |
| materials trvs/pbvs load dedupe | no | b | — | identical (repeat loads removed) | tiny | none |

## Side findings (not vendor-related)

- The committed `release\profiles\Nioh3EngineFix.{PC,ALLY}.ini` are stale: they have `SR = 0`, but the base ini and
  profiles.json say `-1`. `package.py` regenerates them before zipping, so the zips are right. Re-run
  `make_profiles.py` before committing them again.
- The B entry in the PC profile contradicts the README (see the table).

## Summary for other hardware

- **NVIDIA / Intel Arc / other AMD GPU on Windows:** everything shipped works. Expected gains:
  - CPU fixes (W1/W2, VRR, W6/GF, MQ, spin/sleep fixes) help any CPU-limited PC.
  - The GHDM shader gives a small GPU saving where GPU-bound (measured on AMD only).
  - The FG presenter fix helps only with FSR frame generation.
- **Loading fixes:**
  - EP/SR are Proton-only. On native Windows the file path is already cheap.
  - Under Proton they work with any GPU, including NVIDIA on Linux.
- **Intel CPUs:** no instruction-set or core-count assumption in default features. Hybrid P/E chips are handled by the
  game's own pool sizing.
- **Keep off on non-AMD:** AS0 (Intel Arc risk), `[GpuOpt]` R2a/R2b/R1 (RADV-specific), Barriers (measured loss on
  RDNA4).
