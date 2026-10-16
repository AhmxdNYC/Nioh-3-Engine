# Yamagata variant to character asset key

2026-10-11 root static verification. This closes the data association left uncertain in `training-yamagata-move-selection.md`; it does not create an action-ID catalogue or authorize a resource/native call.

The complete retained archive_03 scan covered1295 files/14,810,167 bytes. The primary enemy table `data/extract/lfm/archive_03/C4/E1CBAE0841F7ECE87BD581BA0998F242FE69C4449DB57A473D072E0EB85FAC` is943,314 bytes, SHA256 `1ff95adee8d431e7102fadc39d9ba219951cb259d32c8e2d17576369bfb0e34c`. Header count1022, records920 bytes beginning at8.

| Primary row | Character scalar/key+64 | Parameter+8 | Asset keys+248 and+254 | Name hash |
|---|---:|---:|---|---|
| 60, fileoffsetD7A8 | 307259 | 22178 | CF276D56 | CN_MASAKAGE |
| 6, fileoffset1598 | 598115 | 20878 | 84806F6C | CE_MASAKAGE_00 |

`re/scripts_re/kidshash.py` independently reproduces both character-name hashes. Both rows share text5732529 and behavior-key fields710932/187449, but have distinct model keys and other parameters. The R1CBC placement65092 maps to307259, so its CN_MASAKAGE data association is concrete. The later Mikatagahara598115 variant is not interchangeable just because its displayed name matches.

The companion268-row/20-byte table `archive_03/BC/A7FA73E0E7E39E96BF82A0BFEA29999A9FB14B46D21127CE38F3AA7523802F`, SHA256 `27cdd2ace340835b60bc09be7c92af50382e289527085e592f5dd6dcac2faec8`, also contains these exact character keys at row69/72 column3. This corroborates the data relation; the native meaning of every neighboring field is not newly established.

The old enemy-parameter note's claim that+248/+254 never match character-name hashes was too broad: its referenced stage_names first column contains **loader database IDs**, not the character-name hash. Correct that comparison instead of interpreting the absence as evidence of different assets.

The CN loader56A3D77E points at settings82AE0FD7, which has78 action-object hashes. Those are **not78 attack command IDs**. Missing sceneA0D00ABE and action mapping remain unresolved; only real variant-bound action request IDs can become move-picker entries. The proven native preload input is still scalar307259, as documented in `training-yamagata-arena-preload.md`; never substitute CF276D56 for it.
