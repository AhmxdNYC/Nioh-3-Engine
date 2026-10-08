# Nioh 3 save format: cipher solved (v2.02)

Static + emulated analysis only (Unicorn ran exe functions on `re\Nioh3.dump.exe`; game never launched, live save never touched).
Tool: `re\scripts_re\savecrypt.py` (stdlib only). Verified on a COPY (`D:\nioh3-re\save-work\A.BIN` from backup 20261008-0407):
decrypt -> magic `RNNUSR`, stored checksum == computed checksum (algorithm from `amrita-tools.md` section 2 is correct),
re-encrypt with the keys read from the decoded header -> **byte-identical to the original file**. ~27 s per 9 MiB in pure Python.

## Cipher (exe)
- Writer `FUN 0x5b7f4c` (builds the 0x158 header + encrypts body), CTR `0x5b85e8`, key schedule `0x5b88cc`, constant derivation `0xfbac00`.
- **AES-128, CTR mode, 32-bit big-endian counter in IV bytes 12..15, with a CUSTOM S-box** (256-byte table at RVA `0x3be2b70`; the standard
  S-box/inverse at `0x3be2a70` is NOT used for saves, which is why a stock AES fails). Key schedule/Rcon/MixColumns/ShiftRows are standard.
- Header (0x158 = 344 B): 6-byte magic `RNNUSR`/`RNNSYS`, +0x20 time64 savedAt, +0x28 saveCount, ... then four 16-byte per-save random values
  `c8`@0x49, `b8`@0x59, `a8`@0x69, `k98`@0x79 (from the game's rand(), seeded by time). The whole 344 B is XORed with a **static**
  keystream = CTR(P1key,P1iv) xor CTR(P2key,P2iv) (constants in savecrypt.py; identical for every save, confirmed with different rand).
  After removing it: `c8 = CTR(E, D)(hdr[0x49:0x59])`, `b8` likewise from 0x59 (static key E / IV D), `a8`,`k98` = CTR(key c8, iv b8) of hdr[0x69:], hdr[0x79:].
- Body (0x158 .. end of file) = `CTR(key a8, iv k98, CTR(key c8, iv b8, plaintext))`. Re-encryption with the same four values reproduces the file exactly;
  the game picks fresh random values every save (that is why consecutive saves differ everywhere).
- Plaintext layout after header: checksum seed u32 @0x900190, checksum u32 @0x900194 (over the decrypted buffer, see `amrita-tools.md`).

## Using it
```
py re\scripts_re\savecrypt.py check   COPY.BIN          # magic, checksum, round-trip
py re\scripts_re\savecrypt.py decrypt COPY.BIN COPY.dec  # header decoded + body decoded
py re\scripts_re\savecrypt.py encrypt COPY.dec  OUT.BIN  # recompute is NOT done: fix checksum @0x900194 yourself after editing
```
Editing flow: decrypt, patch bytes, recompute checksum (`savecrypt.checksum`) and store at 0x900194, `encrypt`, write to BOTH `SAVEDATA.BIN` and
`BACKUP.BIN` (identical copies). (`encrypt` does not recompute the checksum.)

## Body structure (partial)
- Body from 0x190 is a tagged stream `{u32 hash, u32 size, data}` but records are **nested** (e.g. hash `8590a652`, 1024 B, is a list of sub-records
  `6067cb8b`/`e30a9fb2`...), so a flat top-level walk drifts. Walking the top level from 400 works only for the first ~0x11249d bytes
  in the two test saves. A proper recursive parser is still to do.
- Candidates checked for the mission flag block (6 slots x 0x400 B): the two 6148-B records `bf0a03c7` and `cf552cec` are `u32 count=256` + 256 x 24-byte
  entries (periodic hash-like pattern), not flag bytes. The 1024-B record `8590a652` is a nested list of 30 sub-entries. **Mission flag block not yet
  located.** Next step: recursive parse, then diff two saves taken before/after one boss kill (same character timeline; the two backups here
  are different timelines: savedAt 2026-05 vs 2026-10 and diverge at 0x11249d).
