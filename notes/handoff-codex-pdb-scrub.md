# Private PDB-path scrub tooling - 2026-10-11

Branch `codex-pdb-scrub` based on combined candidate `5736bdb04373456c35c81d2a0e603cd370313c9e`. No mod/src, version, defaults, build or runtime changes. Native PC benchmark aborted before launch at its free-space guard; no runtime result is claimed. Raw private 1.7.65 remains untouched, pending lead-owned smoke/integration.

Read the public handoff's exact rule in main `notes/handoff-claude-public-repo.md`: replace the built ASI's PDB path with the bare filename plus zero padding; "only the debug-directory string changes; code bytes stay identical." Nothing was written to the public repo or a shared bin.

## Tool and focused proof

`release/scrub_pdb.py INPUT OUTPUT` supports PE32/PE32+ RSDS CodeView records. It maps/validates PE debug-directory/payload bounds and rejects missing/unsupported CodeView formats, malformed/truncated paths, mismatched raw/RVA pointers, overlapping payloads and paths inside executable or declared-code bytes. Opaque non-CodeView records stay unchanged (the actual ASI has types 12 and 13 alongside RSDS). All records validate before any output creation.

Input is read-only. A distinct, non-existing output is required, including resolved aliases; no in-place rewrite or overwrite. Each pathname storage region receives the UTF-8 bare `.pdb` filename and zero padding. File length, RSDS GUID/age, all bytes outside those regions and all executable/code bytes are proved identical before output. Output is read back; a failed output write/readback removes only the newly created output. Common Windows/macOS/Linux home-path markers are scanned across the whole result in ASCII and UTF-16 LE/BE; a remaining marker aborts before output creation. Diagnostics/proof omit input paths and full PDB pathnames.

`py -I release/tests/test_scrub_pdb.py`: **8 tests PASS**, with PE32/64, multi-RSDS and opaque-record preservation, UTF-8 filename/idempotence, 19 malformed variants, overlapping/executable/code payload refusal, three residual-home-path encodings, nonoverwrite/in-place refusal, malformed-file noncreation, successful-copy source preservation and path-resolution errors without personal-path diagnostics. Focused reruns followed the added code-section, UTF-16 BE and resolution-error coverage. No mod harnesses/full suite or native builds were repeated.

## Actual private 1.7.65 copy

Output: `D:/nioh3-re/candidates/1.7.65/Nioh3EngineFix.asi`.
Proof: `D:/nioh3-re/candidates/1.7.65/Nioh3EngineFix.pdb-scrub-proof.json` (metadata only).

- Raw/source SHA256, verified unchanged: `cee81fbbfc76df810660de0d5ea9e7a701a651c4bbf407c2a1938e9d5b88d5df`.
- Scrubbed-copy SHA256: `997c1c0e92bab3e936c033deeca4e53b8b37dd3a3866fb2e2c31eb1cb0c85537`.
- Both files: **1,755,136 bytes**. **83 changed bytes**, all within the single **84-byte RSDS pathname storage at offsets 1446468..1446551**. Result stores `Nioh3EngineFix.pdb` plus zeros.
- **1,136,640 executable/code bytes identical**, every other byte identical; original GUID bytes `0dcecc8c8c06be42bf783672ec77673b` and age **1** preserved. Common home-path markers absent.

The final tool reproduces the saved copy and complete proof exactly from the unchanged raw input. This is a privacy/byte-equivalence gate, not a native smoke result, signature validation or general secret detector. Root owns private transfer/smoke and any later public release; do not publish these handoffs or private history. No game, bridge, public write, main merge or push in this unit.
