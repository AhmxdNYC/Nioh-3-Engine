# ktgl string hash (static, 2026-10-08; game not run)

**Algorithm (verified 3203/3203 LFM script name-table pairs):** 32-bit, case-sensitive, seed 0, signed chars:
`h = h ^ (h*64 + (h>>2) - 0x61C88647 + c)` per byte (boost::hash_combine / 0x9E3779B9 style). Code: `re\scripts_re\ktgl_hash.py`.
Found at EXE RVA 0x13D170 (string branch, `iVar1==1`), reached from the script-native binding registration (0x94D80 -> 0x95204 -> 0x2E3334 -> 0x13D170) and the generic name-id struct {kind 1=char*, 2=ptr+len, 3=u16}. Not kidshash (31^i), not FNV/CRC/murmur/djb2 (tested).
Stage-name hashes in `data\stage_names.tsv` use a different function (A01_HAZAMA -> b885b26b, ktgl gives 219a73e7); rdb name_hash is kidshash.

## Sound::PlayBossBGM
Wrapper 0x23AD8A4 only unpacks the arg (0x2E5574) and stores it at `soundMgr+0xB4` (0xB8=0) via 0x29057EC; no string compare there. The id is the hash of a cue/BGM name; scripts carry `[hash, 0x7c0b0783, 0]` (0x7c0b0783 = type tag, not a hash of BGM/Sound/Boss/State variants tried).
The 43 BGM ids occur as raw words in archive_03 BGM-box/area tables (e.g. 0efcc25f in `...CBACDA3C...DE30B05...`, record `{hash,0x7c0b0783,...}`), but **no cue-name strings exist on disk** (no sound bank in the extracted data; KTGLSoundBank resources not in rdb index), so ids remain unresolved.

## Resolution attempts
- Hashed every ASCII token (+ split/stem/case variants) of the EXE, strings.tsv, text_en/ja, data/*, all extracted LFM/root/stage/system files: 0 hits against 43 BGM ids.
- Combinatorial prefix/suffix (bgm_/BGM_BOSS_/...) x stage, chara, boss, text names: 0 real hits over ~119k words x 38 prefixes x 31 suffixes (~1.4e8 strings; the single hit bgm_event_xdccb0_end is a chance 32-bit collision).
- Gadget constants in `Gadget::*` are mostly numeric placement ids (e.g. 0x0103xxxx, 733e2361-type in boss-arenas.csv), present as raw u32 in archive_03 placement tables; only a minority are hashes; none matched a name. Placement ids are not ktgl hashes of strings in the data.

Next: get the sound bank (KTGLSoundBank header/body resource hashes at EXE 0x3909C90/0x3909CB0, missing system fdata 0x785578fe / 0xa9992094) or runtime-dump the cue registry.
