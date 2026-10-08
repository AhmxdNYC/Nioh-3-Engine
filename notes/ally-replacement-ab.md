# Ally 1.7.5 GP2: replacement shaders on vs off (RADV)

Logs: `captures/ally/nioh3_ally_modlog_175-L1_2052.log` (all on) vs `...L2_2112.log` (all off). Script: `re/scripts_gpu/gp2_ab.py ON OFF [--win N]`.

## Why the hashes were "unknown"
GP2/psoswap key a shader by the **DXBC container digest** (container header bytes 4..20, `psoswap.cpp ParseBlob`, FNV-64 only for unsigned
containers), not the DXIL HASH-part digest. The old index held the originals but never indexed the **new** containers of our replacements.
`gp2_digest.py` now indexes both digests of every record of the game's `shader_dx12` (scanned from the real bytes), the Ally dump contents,
all `replacement\*.file` (new containers paired to their originals by container position vs `backup\`) and `post\out\*.dxbc`
(`--rebuild`, ~7 s; 37k digests). `reps` labels say ORIGINAL replaced / OUR REPLACEMENT.

## Hash map (the pairs from the digests)
| ON hash (ours) | OFF hash (original) | file / record | rewrite |
|---|---|---|---|
| 3afc8fd74d54ef75ec0a3f70482123ab (56776 B) | 4b4c3cbe6724e06841b7ed69d7472a46 (26404 B) | 0xd070067d.file #121 RefProbe | gi #121 diffuse probe GI gather |
| 4d6740bf811e14127bc9ab8cefc885a5 (39152 B, async) | 641524ee74b197c7d8f1535066bb2c1a (35920 B) | 0xa9910db8.file #235 VFSCAT | fog VFSCAT |
| 8986d40f4c6b26c6185083b0737d3976 (10628 B) | 318fa766bb0f9ad624957b9f1da0e576 (6668 B) | 0x9c481f79 / 0x47bc5cb4 #2 | toonedge2 outline pass |
| b76874bb17bdfc10a0739b74191c68a9 (both runs) | same | 0x5f17ca9d.file VCLOUD (DrawSky) | NOT a replacement |
| 9945c0635ca06e3267abf943ae64a1cc (ps) | 32e8deff99183945b94bc3be679bfa95 (ps) | 0x1fd07cc0.file #28 | wdo28 |
| (not swapped on the Ally) b549cd6e20cf (ours, absent) | 6ae59bd0ddf1220dc8907954b1f215b4 (47632 B) | 0xd070067d.file #414 | gi #414 reflection trace |

b76874bb is the unmodified VCLOUD async cs; it only looks 1.26 vs 0.51 ms in the last window because its interval overlaps the slower
async fog cs (pipelined interval, contention), mean over 6 windows +0.10 ms.
#414: the original 6ae59bd0 still runs with everything ON, so the Ally's installed gi file only has the older #121-only replacement
(ally_shaders.sh lists superseded gi hashes); #414 was never measured there.

## GP2 ms per frame, mean of the last 6 ON gameplay windows of each log (spans 66.8 ms ON vs 64.2 ms OFF)
| rewrite | ON ms | OFF ms | delta | verdict |
|---|---|---|---|---|
| gi #121 (3afc vs 4b4c) | 4.029 | 3.135 | **+0.894** | DROP (slower on RADV, in all 6 windows; last window 4.82 vs 3.35) |
| fog VFSCAT async (4d67 vs 6415) | 3.914 | 3.585 | **+0.329** | DROP (also stretches overlapping async work such as VCLOUD) |
| toonedge2 (8986 vs 318f) | 0.544 | <0.40 (not in OFF top-40; cutoff 0.40) | >= +0.14 | DROP |
| wdo28 #28 (9945 ps; 32e8 variants still orig) | ~0.15 x1 window + 0.889 orig x5 | 1.075 | ~ -0.03 | neutral, keep (inside noise) |
| gi #414 | not swapped on the Ally (6ae5 orig runs: 2.18 vs 2.36, noise) | | n/a | no data; keep OFF until the Ally has the new file and it is measured |
| ghdm, esm, mpc, mshlod, post-v1 (5 dxbc) | below the top-40 cutoff (0.40-0.42 ms) in both logs | | unresolved | keep (ghdm measured +3..11 % fps earlier in MODLOG); re-measure with a larger GP2 top-N |

Noise floor: untouched shaders differ by about +-0.03 ms between the two runs (VS/PS rows), so 0.3 ms+ deltas are real; scenes are not frame-identical.
Sum of the three losers is about +1.8 ms of the +2.6 ms span difference (rest: async contention); matches the 0.4-0.6 fps loss.

## Caveat: stamp overhead
Stamps ON cost a lot (menu span 30.4 ms ON vs 16.7 ms OFF per the A/B window; in gameplay ~64-67 ms spans). Absolute ms are inflated and
pipelined intervals overlap, so use deltas of the same shader/key, not sums. The conclusion is a ranking, so confirm with the fps A/B per item
(`ally_shaders.sh apply|revert gi|fog|toonedge2`).

## Recommendation
Revert `gi`, `fog`, `toonedge2` on the Ally (RGA/Windows-compiler estimates do not hold on RADV); keep ghdm, esm, wdo28, mpc, mshlod, post-v1 until
a per-item fps A/B says otherwise. Rebuild gi/fog for RADV (fewer registers / no merged early-exit) before re-testing.
