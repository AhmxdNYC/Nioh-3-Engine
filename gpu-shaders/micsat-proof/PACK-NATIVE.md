# Safe MIC pack binding and bounded native recipe

Offline pack gate **PASS**. No game file was modified and no native test ran.
This is the reviewed safe-index candidate only, not the earlier speculative
SAT rewrite. Native admission remains the lead's decision.

Target: `shader_dx12/0x17ad25bb.file`, record0 of3; payload record offset572,
DXBC offset588. Full-file identities:

- Required original: `1c89c8e765015265b57781f913e9e7eab881790fdc82a3301844837b41337fdf`.
- Packed candidate: `f8fac97a05ff5dab599e33da804851ef9f55239bf74832b407e8bc1eb58074cb` (41121 bytes).
- Original DXBC: `3566a0efaaeac7724d0354c6a79bd0007ff15db33629f23b0555a324a80aeffc`, digest `e31c389f6d4a38de7b1b784b87c710d2`.
- Safe DXBC: `9a64454f4f89d6571c01cbaf38229aca08ec66e2f0a206fc969a77a9e4409c66`, digest `ed9a06c39e23b641d422411cc4f76144`.

Private ignored artifacts, relative to this directory:

- `out/safe/pack/candidate/0x17ad25bb.file`
- `out/safe/pack/original/0x17ad25bb.file` — exact original compressed bytes for rollback.
- `out/safe/pack/receipt.json` — full per-record hashes and checker binding.

`safe_pack.py` checks the complete original-file hash and unique target digest,
changes exactly record0, preserves records1/2 byte-for-byte and all384 auxiliary
tail bytes, preserves unrelated group/container metadata, decompresses the
repacked file, and reverses the replacement back to the complete original
payload. Six negatives are rejected: wrong slot, changed original file, changed
candidate, unrelated header, other record, and changed target auxiliary tail.
The game file and DXBC candidate remain unchanged after artifact creation.

    py -I -B gpu-shaders/micsat-proof/safe_pack.py

## Recipe for a future lead-authorized smoke

1. Use the existing game-lock, process-closed and entry-state backup/restore
   harness. Verify the target device's installed **full-file** original hash,
   private backup hash and candidate artifact hash above. Any mismatch means
   stop; do not overwrite another candidate or merge an unreviewed group.
2. Change only this one shader group for the candidate arm. Keep default picture,
   resolution, camera route, save and other candidate settings identical. Do not
   combine this with a boss/training transition or another shader experiment.
3. Use a short known field scene: baseline then candidate, approximately20–30
   seconds of the same camera motion and disocclusion route per arm once ready.
   Exclude startup/compilation from any timing. Confirm actual candidate shader
   usage through available digest/PSO evidence; absent usage evidence is
   inconclusive, not a smoke pass. Stop on device errors, hangs, missing geometry,
   stale visibility, flicker or changed culling output.
4. After closing the game, restore the exact saved original file through the
   existing harness and verify its full SHA256. Verify the normal entry-state
   save/INI/binary restoration receipts too. If installed bytes differ from both
   expected candidate and original, preserve the discrepancy for inspection;
   do not claim successful rollback or overwrite an unrelated writer's state.

A visual/functional smoke cannot establish performance or universal culling
equivalence. Advance only to a separately bounded paired A/B after actual shader
usage and picture checks pass. Offline VGPR/wait reductions are motivation, not
native FPS results. No other device, public payload or default installation is
authorized by this note.
