# PB2 UV-hoist: one-record pack and captured Windows PSO gate

2026-10-12. Private offline/compiler work; no game launch, installation,
window, queue, draw, render-target allocation or default-setting change.

## Exact private pack

`gpu-shaders/pb2pixel/native_pack.py` requires the full original file, original
PS and signed candidate hashes from `pb2-pixel-uv-hoist.md`. It freshly dumps
both bodies, binds the candidate to the exact rewrite, verifies the independent
resource/output/control certificate and unchanged ISG1/OSG1/PSV0/SFI0 chunks,
and requires DXIL validation/signing to leave the candidate bytes unchanged.

Output: `D:/nioh3-re/pb2pixel-20261011/private-pack/0xb4751b0f.file`.
SHA256 `aad7d12180f65800a1ded41dd6022eb5997eb1a947c7e18a5bcbb85af295ed88`.
Original file SHA256
`c32b5ae870a80f68f8fced65e85ff4306a81ba2ab5dddb6983aff80c652c817a`.
Candidate PS SHA256
`bbd47920e47762776bf232ac49b06f4dd344a35e81b9cb80b14c1ca4bd9b2b34`.

Exactly record 1 of two changes. Record 0 is byte-identical; the replaced
record's complete auxiliary tail is identical. Only allowed payload sizes and
table offset/size entries change in the group header; outer IDRK properties
remain identical. Replacing the PS back yields the byte-identical original
uncompressed payload. Wrong-slot and corrupt-candidate negatives reject.
Packed size 22,725→20,856 bytes is not a runtime performance claim.
All game source bytes and journal hashes remained unchanged afterward.

This is one duplicate-container alternative, not evidence that the actual
live material draw selects this file; the same original PS occurs elsewhere.
No other duplicate container was packed or installed.

## Exact captured graphics PSO admission: PASS

Reused the pinned `pb2guard/pipeline_extract.py` journal parser and checksums.
The actual journal SHA256 remains
`f1845f5f3d61f7f7d59538cb187fef57df433f6e3f3fced9c852b75cec97c40d`.
All eight recorded complete 656-byte descriptors, original VS
`5a7d10634d334e35ed62a5706fa784c905793f8d8a20a60539be9abd0ea970cc`,
original PS `0205c80d9035cd8de989cd758a3cf987eda459156ed2bdd9f6224c5ead258268`
and recorded root
`dc65a1fc5ca7cb3fbcd100895637c8eab667904e6f613625597bad8890d0031c`
are bound. No synthetic root/PSO replaces these inputs. Selected PSO, SHDR and
RSIG checksums and conflicting duplicates are verified by the reused parser;
unrelated journal checksums are not claimed verified.

The previous pinned VS helper is adapted at exactly two checked statements:
VS becomes fixed original, and PS alternates original/candidate. Remaining
descriptor state and pointer/length rejection checks are unchanged. The
extractor's previous VS candidate is overwritten only in private helper input;
the new receipt explicitly identifies replacement stage PS. Helpers build with
MSVC /W4 /WX, and execution is bounded to 120 seconds.

Actual result: eight root creations and sixteen original/candidate PSO
creations all S_OK, zero failures, no draws. Adapter AMD Radeon RX 9070,
vendor1002/device7550, driver query9007201289044972. Compiler input snapshots
remain byte-identical after execution.

Private receipt: `D:/nioh3-re/pb2pixel-20261011/pack-native.json`.
Binding SHA256
`2759e94e04a978bcd9be76e663c7062da51bad0b80fee8028db68ef07aa73245`.
Adapted helper source SHA256
`fd09f3b225e674780d268fc322cf207351d3a69ebf80fca8282256547907eb8c`;
helper executable SHA256
`4663d95ec74e9c42f0dfa8f22dd7cadc7511e02e1bac164ee6d3af25f4450a00`.

## Remaining gates

This proves Windows compiler acceptance for the eight recorded states, not
actual rendering or performance. Proton/RADV compiler acceptance and codegen,
exact selected live container, native visual/motion checks and matched A/B
remain open. Existing VS-candidate acceptance does not transfer to this PS
candidate or to a combined VS+PS installation. Lead owns any native proposal,
current-entry backup/rollback and device-local lease; this tool does not install.

Reproduce from the isolated worktree:

```powershell
python -B gpu-shaders/pb2pixel/native_pack.py --repo 'C:/Users/Ahmad Hamza/nioh3-engine' --out D:/nioh3-re/pb2pixel-20261011 --admit
```
