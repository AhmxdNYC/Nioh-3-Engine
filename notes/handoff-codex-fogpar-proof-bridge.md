# Fogpar retained proof-input bridge

2026-10-11 14:51, OpenAI Codex. Offline artifact comparison only.

`py -I tools/bench/audit_fogpar_proof_bridge.py` PASS. The missing main-checkout
inputs are retained in `D:/nioh3-wt/allygpu/gpu-shaders`, which the audit reads
without editing. No rebuild, staging copy, device replacement or installation.
The `--donor` argument allows another read-only copy with the same pinned hashes.

- Current packed fogpar record #235 equals retained `gi/out/vf235_par.dxil`
  and `fog/out/psoswap/641524ee74b197c7d8f1535066bb2c1a.dxil` byte-for-byte.
  Record SHA256: `c259803c1774a723054b1e0caf99cc8c5fc9d4a341d61f7c53d3928121d56e7a`.
- RDTS dxc `-dumpbin` of the extracted packed record equals retained
  `gi/out/vf235_par.ll` after the builder's exact normalization: omit empty
  lines and lines beginning with `;`, normalize CRLF. All 3,170 remaining lines
  match. LL SHA256: `c120d993bcfdd962c6908e3c9027b3ce8addf2b0d9b33f8ff1e626c6be50f375`.
  Normalized body SHA256: `6436f9f08a17d2eaa37e8881e97134125b4c6db0a4fbd27eec4adfe8c10bbade`.
- Tool SHA256: `ae4bf100b8c64ab03cdc0ee5996d5f212e96c59d6d37340359342d714ac6c004`.
  Truncated/appended/alternate-onlypar LL rejection controls all pass;
  comment-header control passes. The `onlypar` name is not a substitute for
  `par`: its retained DXIL hash differs and its LL body is rejected.

This closes the current packed-to-retained-proof-input comparison. Archived
coverage/holdout reports name `vf235_par.ll` but do not record its input hash;
this audit cannot cryptographically bind their historical execution to this
retained file. Their previously reported bit-exact results were not rerun.
No fresh equivalence, shader-execution, image-equality or performance verdict.
Main's missing standalone/LL files remain missing; the audit deliberately
does not restore staging while device queues are outstanding.

Final phone inbox empty; no sender=user steering. No local Nioh3/PresentMon/
Ghidra process observed, hence no local test/restoration to finish. The temporary
extraction was cleaned by TemporaryDirectory. Existing shaders, pinned builds,
other writers/worktrees, saves/settings/hardware/controller/holds unchanged.
No Claude invocation or auto-wake. Stable1.7.61, FO NO-GO and guard PR1 unmerged.

Next independent unit: default-OFF read-only training observer on owned
`codex-training-pump` c297a20, following its ledger prerequisites. Consume
fresh actionable device evidence first. Whole-default A/B and runtime/player
acceptance remain external; no new shader device queue during baseline testing.
