# lodu / fogpar staged provenance checkpoint

2026-10-11, OpenAI Codex. Offline read-only artifact unit; no device queue,
game/Ghidra launch, installation, shader rebuild or controller operation.

`py -I tools/bench/audit_gpu_candidates.py` PASS. The audit reads retained
originals and staged packs, verifies full SHA256s against the documented
variants, and compares every record. It uses the mesh builder's CXDK-aware
parser for lodu and the GI plain-DXBC parser for fogpar.

| Candidate | Actual staged record change | Provenance result |
|---|---|---|
| lodu | #2 only; 31 other records byte-identical | cull4 manifest and ZIP CRC valid; packed record equals transport record and retained `work/lod2_dup.dxbc` |
| fogpar | #235 only; 362 other records byte-identical | full pack matches documented fogpar hash; packed record has documented digest and full standalone-record SHA256 |

- lodu pack SHA256: `5f7f3b120b909c74c0238d7af3f4f3cd43299840b21a5a49e195545ea54150f1`.
  Record digest `d5a1a069857feee3a6589f1c788aca52`; record SHA256
  `6dea76aedc33e740a9ffee1b9bda6e1eef2bdcbffd82eb86f0bab897d00d11f6`.
- fogpar pack SHA256: `81007b3f2bc066e4f4951b9bc4bac37be29bdb934efbc9fe83275d65b767334e`.
  Record digest `5473bfb797f181002f6cd88547d207e4`; record SHA256
  `c259803c1774a723054b1e0caf99cc8c5fc9d4a341d61f7c53d3928121d56e7a`.

Important staging distinctions:

- `replacement/cull4/0x8d9282e6.file` is lodu, but the flat
  `replacement/0x8d9282e6.file` is still mshlod (SHA256
  `214ac371d68ff4e258526bd6f1d7cd044347bc8c417410966ae6400a02986c24`).
  Windows apply_cull4 selects the correct subdirectory; flat-file consumers
  must check/select the lodu pack explicitly. Do not overwrite flat staging
  or device files while another test owner/queue uses them. Never combine
  mshlod and lodu in one item list.
- `gi/out/vf235_par.ll` and the documented fog ShaderSwap DXIL file are absent
  in this checkout. The correct record remains inside the staged fogpar pack;
  the ASI-only installation path is not currently staged. Archived proof logs
  alone cannot establish a fresh packed-to-proof-input comparison here.

Existing proof reports were inspected, not rerun: lodu 400 scenes x64 threads
x3 guards, 67/67 blocks, three caught mutations; fogpar coverage/holdout
reports PASS with bit-exact writes and 137/137 blocks. No new equivalence or
performance claim follows. Runtime shader execution, paired profiler-off frame
timing, moving scene/image-noise comparisons and restoration remain device
gates per the GPU cost audit. At checkpoint, phone1877 (sender pc2) reports both private guard1765 installation-only title-menu runs PASS, PCM0/default and separate PCM2 switch-off, installed ASI matching kit229, PresentMon frames, graceful exits and entry saves/ASI/INI/log/config restoration. Archive bridge230 remains unverified. No field or FPS gain claim; an in-run pre-flush CSV check initially printed FAIL, which the device report supersedes using flushed CSV/OSD evidence.

Checkpoint: no local game/capture observed; phone1877 contains no sender=user steering. Stable 1.7.61, FO NO-GO and controller holds remain unchanged.
Next independent unit: verify archive230 against reported default/PCM2 installation, frame evidence, graceful closure and restoration before deciding guard merge. Alternate independent proof: close or bound the native first active CBV layout's positive-size dirty-slot invariant. A later fog unit may recover the missing
proof-input comparison without replacing pinned test files.
