# Fogpar input-binding preflight passed; full proof reserved

Lead follow-up 2026-10-11: added `--preflight-only` to perform hash/snapshot,
fresh DXC disassembly and identity controls without executing interpretation
seeds. Actual run PASS_INPUT_BINDING_ONLY,19 input/tool files,4.42s; all original
hashes unchanged. Truncated/appended/alternate LL rejected; comment-only header
accepted. The stock UTF-8 BOM comparison fix executed successfully. AST and
comparator self-tests PASS. Manifest `scratch/fogpar-input-binding-20261011.json`,
SHA256 `1a69e11450245ae80130ceaa30fbf2aeedd31bf72105431a057203abe2230167`.
The manifest has **zero equivalence passes**: this is no bit-exact, native GPU,
image-equality or performance claim. Full seed/mutation runs remain reserved.

2026-10-11, OpenAI Codex. Private offline unit in `codex-fogpar-validation`.

`tools/bench/verify_fogpar_bound.py` prepares an input-bound fresh equivalence
run against packed record #235 and the retained `vf235_par.ll`. It snapshots
the exact input bytes, eight harness/parser sources, DXC and its two DLLs into
a disposable directory. The manifest records original paths, SHA256s, Python
and NumPy binary hashes, seeds, environment, per-block execution counts and
results; it contains no shader bytes, shader text, generated scene or output
values. Original file hashes are checked again after execution.

- Candidate record SHA256:
  `c259803c1774a723054b1e0caf99cc8c5fc9d4a341d61f7c53d3928121d56e7a`.
- Candidate normalized LL body SHA256:
  `6436f9f08a17d2eaa37e8881e97134125b4c6db0a4fbd27eec4adfe8c10bbade`.
- Stock record SHA256:
  `63e2aadb479ce45ccc29f625deaffe03fed4bfeb72628401c586d179ff858fa4`.
- Stock retained LL SHA256:
  `03357c63e1f97418917347dccb8d97bf1440e4524006dd4fae0f529d05fa17b9`.
- Stock normalized dump body SHA256:
  `e396bcb95555be28622a68513897c581ee2186495472ca06a1e092d4077c0cdd`.

Main staging and `D:/nioh3-wt/allygpu/gpu-shaders` remain read-only. The runner
pins the full stock/candidate pack hashes and verifies that only record #235
changes. Both retained standalone candidate DXIL files must equal that exact
record. Fresh DXC `-dumpbin` bodies must match the retained interpreter inputs.
The stock LL begins with UTF-8 BOM `EF BB BF` immediately before its first
comment; the stock comparison removes that marker only. Execution uses the
unchanged, byte-frozen stock LL. Candidate normalization is unchanged from
`build_fogpar.py`.

The existing interpreter's `verify.run_one` supplies write/ULP statistics and
stock block coverage. An additional comparator checks every UAV resource,
per-thread/address ordered writes, masks and float32 bits, plus final contents.
This distinguishes signed zero and represented NaN payloads that the old ULP
comparator treats as equal. Python/interpreter NaN representation still limits
claims about native GPU NaN payloads. Identity controls reject truncated,
appended and alternate `onlypar` bodies and accept an added comment header.
Two frozen-LL output mutations must also fail equivalence: replacing the first
store component with one, and removing the output store.

Queued reproducible commands, from this worktree:

```powershell
py -I tools/bench/verify_fogpar_bound.py --self-test
py -I tools/bench/verify_fogpar_bound.py --smoke --jobs 2 --output notes/fogpar-bound-smoke.json
py -I tools/bench/verify_fogpar_bound.py --jobs 8 --output notes/fogpar-bound-proof.json
```

The full run uses coverage seeds `[0,768)`, holdout `[768,2304)`, realistic
moderate `[0,64)`, and realistic heavy `[0,32)`, all float32 with zero ULP
tolerance. Each pass has a 1,200-second timeout. Both coverage and holdout must
hit every original block; smoke results cannot satisfy that gate. Existing
archived coverage/holdout logs are not reused as fresh results.

**Checkpoint:** the root reserved this PC for the user's Boss Rematch recording
before any interpreter seeds ran. No proof batch, compiler or process remains
running. Two early smoke attempts stopped at stock BOM comparison in under one
second; source inspection corrected the transport-marker handling, but the
corrected runner has not been executed against shaders. These attempts are not
fresh equivalence evidence. The root must release its recording hold before
the queued commands run.

Light validation PASS: Python AST parsing; strict UAV comparator detects signed
zero, represented NaN-payload, mask, resource and address changes; normalized
body comparator accepts comments and rejects truncated/appended synthetic
inputs. `--self-test` reproduces the comparator controls without accessing
shader inputs or DXC; the same controls are required by every proof run. These
checks perform no shader interpretation.

Remaining gates: execute and review the smoke/full manifests after release,
then native GPU shader execution, profiler-off paired timings, moving-scene
visual comparison and restoration on authorized devices. No device queue,
shader rebuild/replacement, native compilation, claimed FPS gain, game/Ghidra/
bridge/Discord/controller/hold operation or public artifact was performed.
