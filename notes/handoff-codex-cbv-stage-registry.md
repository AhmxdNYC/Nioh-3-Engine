# CBV producer discovery: stage-record registry seam

2026-10-11, OpenAI Codex. Offline inert-image provenance only.

Result: the stage-record getters do not construct a layout locally. They
dispatch a record handle through the object at global RVA `4BD0A38`, vtable
slot `+10`. The concrete registry implementation is the next producer seam;
no positive-size CBV invariant or native cache defect is established.

Native VS getter `303465C` loads `[group+30]`, then its first pointer, and
tail-jumps to `3033124`. That helper reads the indexed eight-byte handle,
passes it in RDX, zeroes R8D (kind zero), and tail-jumps through registry
vtable `+10`. It neither tests group+30 for null nor validates the index.
Those remain caller contracts; this is not evidence of bad runtime inputs.

The seven adjacent helpers at `3033044..3033104` use the same global,
indexed handle load and virtual slot, but supply kinds `7,5,3,4,2,6,1`.
They are distinct entrypoints, not all aliases of `3033124`. Existing symbol
descriptions calling that address the shared body for every stage are too
broad. Discovery reads of the surrounding getter thunks agree with the
stage-array routing; only the VS thunk is pinned by this audit. Kind numbers
must not be substituted for the Syn binder's stage indices without proof.

Native `30345DC` returns the previous global object in RAX and replaces it
with RCX. Its three instructions contain no local constructor, ownership
release or invalidation call. It does not prove whether callers provide
invalidation elsewhere. A targeted CALL/JMP/Jcc byte-reference search for
`30345DC` returned zero matches; it cannot cover indirect calls or address
taking. Do not infer that the setter is unreachable.

Verification: `py -I -B tools/bench/audit_cbv_stage_registry.py` PASS:
three SHA256-pinned windows (382 bytes total), 62 instruction checks, and
memory-only truncated/corrupted rejection controls. No previous passed suite
repeated, game or Ghidra access, build, deployment or performance claim.
The surrounding discovery scan was bounded to `3033000..3034800`; it supplies
no exhaustive producer coverage claim.

Next: locate pointer references to `30345DC` / the initialization writer of
`4BD0A38`, resolve its concrete vtable+10 implementation, then trace the
returned stage object's layout+38/+40/+44 writes. The link from that returned
object to the bound layout also needs proof. Continue the positive declared
entry or independent generation-invalidating guarantee there.

Stable1.7.61, guard PR1 unmerged and FO NO-GO remain unchanged. No local
game/capture at entry or checkpoint; no restoration needed. Phone inbox
empty; controller/holds, other worktrees and device-pinned builds untouched.
