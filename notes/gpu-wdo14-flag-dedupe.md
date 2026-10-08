# WDO#14 VSM page-flag dedupe (remaining-work 3.5) - built, proven offline, staged OFF

Target: `shader_dx12\0x1667d3a0.file` group `WDO-PtSrbSsmVsm` record **#14** (sun-shadow resolve at render resolution,
PSO 4001317a, container digest `8a013bc7...`; 120 us PC in r5, 0.7-1.1 ms Ally; design in `gpu-shader-sweep.md` s5.1).
The Ally runs the same record (`captures/ally/gp2_L1_full.md` row 32).

## What changes
Every PCF tap of the virtual shadow map loads its page flag `tVirtualShadowMapFlags[idx]` (`Buffer<uint>`, read-only
SRV) at clip level L, and for resident pages again at level L+1 (cross-fade). 43 flag loads in the DXIL: 1 in the
clip-level search loop, 2x2 kernel (4 + 4), 4x4 kernel (16 + 16), 1-tap kernel (1 + 1).
- Neighbouring taps nearly always fall in the same page. The cross-fade taps of one kernel use the *same* L+1 index
  every time (centre coordinate), so after the first one they always repeat.
- Rewrite (DXIL level, no HLSL decompile needed): two per-thread caches (level L chain, level L+1 chain) of
  `(valid, idx, val)`; each guarded load becomes `if (valid && idx == cache.idx) val = cache.val; else val = Load(idx)`.
  The load stays where it was, in its own block (skipped by the whole wave when every lane hits); nothing else moves.
- 36 of 43 loads are guarded (not: the search loop, and the first load of each chain in each kernel).
- Output-identical by construction: equal index into a read-only SRV gives the equal dword.

## Tooling (pixel-shader port of the proof pipeline)
The note asked for a PS port of the DXIL->HLSL decompiler. Not needed: the materials pipeline already works on any
pixel shader at the IR level (`dxcom.py` assemble/sign, `psim.py` pixel interpreter), so the rewrite is done there.
| file | role |
|---|---|
| `gpu-shaders/vsm/flag_dedupe.py` | the rewrite: chains keyed by the clip-level term of the index; cache carried as SSA phis (iterated dominance frontiers, trivial/dead phis removed); reuses `materials/sfb_rewrite.Fn` |
| `gpu-shaders/vsm/verify_vsm.py` | lint (same dx.op multiset; every guarded load has the template) + psim bit-exact check on scenes built per `$Globals` field (mP2W ~ identity, 1..9 clip levels, VSM matrices into (0,1), page grid 4..64, PCF step from sub-texel to pages apart, cross-fade bytes, 2x2/4x4/1-tap kernels in turn, resident rates 0.1..0.95) |
| `re/scripts_gpu/build_wdo14.py` | build + proof + stage (pattern of `build_wdo28.py`); outputs `re/scripts_gpu/out/wdo14/` |
| `gpu-shaders/apply_wdo14.ps1` | Windows install / exact revert / status (sha-checked, self-backup) |
| `ally_shaders.sh` key `wdo14` | Ally install / revert / status (same hashes) |

## Proof (build_wdo14.py, all must pass or nothing is staged)
- Record #14 digest = v2.02 original; its dumpbin = committed `re/scripts_gpu/out/WDO_14.ll`.
- Container validated + signed by dxil.dll; parts SFI0 ISG1 OSG1 PSV0 STAT ILDN byte-identical to the original (no
  RTS0 in the shipped container: the root signature comes from the engine, unchanged). dumpbin of the result = the
  rewritten text. DXBC 29431 -> 31123 B, new digest `6db7f007ae259aedf07796b6032d9add`.
- Lint: 36 guarded loads match the template; dx.op calls 693, same multiset.
- psim, 120 scenes x 600 px: **BIT-EXACT**; every guarded load both hit and missed (123k hits / 40k misses).
- Repack: the other 114 records byte-identical, header unchanged. Deterministic (two builds, same sha256).
- File sha256: original `36bdb485...272a7c`, replacement `60e2646c...328259`.

## ISA (RGA offline DX12 driver; not RADV, see the caveat)
| chip | VGPR | waves/SIMD | inst | branches |
|---|---|---|---|---|
| gfx1103 (Ally class) orig -> fast | 40 -> 48 | 12 -> 10 | 3327 -> 3595 | 206 -> 242 |
| gfx1201 (PC) orig -> fast | 40 -> 44 | 16 -> 16 | 3706 -> 4085 | 207 -> 243 |
- Every variant tried costs gfx1103 the same 48 VGPRs: level-L chain only, L+1 only, both, and caching only bit 0
  (`bitval`, i1 in SGPRs). So the full version, which removes the most loads, is the one staged.
- Static VMEM count is unchanged (the loads are skipped at run time, not removed).
- **Risk:** on the AMD Windows compiler, Ally-class occupancy drops 12 -> 10 waves. RADV/ACO allocates differently and
  cannot be checked offline here (item 3.4). The Ally A/B decides. A level-L-only build (`flag_dedupe.rewrite(...,
  offsets={0})`) is the fallback if the full one loses.
- Not done (outside page-flag scope): the cross-fade `SampleCmpLevelZero` also repeats with identical operands on every
  tap of a kernel. Deduplicating it is the next step if this one wins.

## Test (game closed)
PC: `powershell -NoProfile -ExecutionPolicy Bypass -File gpu-shaders\apply_wdo14.ps1` (`-Revert`, `-Status`).
Ally: copy `ally_shaders.sh` + `replacement/0x1667d3a0.file`, then `./ally_shaders.sh apply wdo14` / `revert wdo14`.
Matrix id `S_wdo14` (`tools/testmatrix/matrix.json`, checklist regenerated). If the game rejects the record, the
sun shadow is missing or flat: revert. Expected: about 0.2-0.4 ms Ally, if TA throughput, not latency, limits the pass.
