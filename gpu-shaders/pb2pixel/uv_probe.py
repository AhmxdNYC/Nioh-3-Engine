"""Exact-input, pure UV subgraph hoist experiment. Private artifacts only."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
BODY_SHA = "f01abd800dbbe594ffe931f3f373db599abb749695e031b0064a9825ceeeeabd"
PS_SHA = "0205c80d9035cd8de989cd758a3cf987eda459156ed2bdd9f6224c5ead258268"


def sha(x):
    return hashlib.sha256(x).hexdigest()


def rewrite(ll, S, dxcom):
    if sha(dxcom.body(ll).encode()) != BODY_SHA:
        raise ValueError("unbound original module")
    fn = S.Fn(ll)
    # Same scalar input, same exact opcode/operands; no float reassociation.
    groups = [(88, 176, 272), (89, 177, 273), (111, 199, 295),
              (112, 200, 296), (113, 201, 297), (114, 202, 298)]
    repl = {"%v" + str(n): "%v" + str(g[0]) for g in groups for n in g[1:]}
    selected = {"%v" + str(n) for g in groups for n in g}
    definitions = {S.DEF.match(x).group(1): x for _, ins in fn.blocks
                   for x in ins if S.DEF.match(x)}
    def sub(x):
        return S.TOK.sub(lambda m: repl.get(m[0], m[0]), x)
    for g in groups:
        rhs = [sub(definitions["%v" + str(n)]).split(" = ", 1)[1] for n in g]
        if len(set(rhs)) != 1:
            raise ValueError("UV opcode/operand equivalence failed")
    lifted = [definitions["%v" + str(g[0])] for g in groups]
    for _, ins in fn.blocks:
        ins[:] = [sub(x) for x in ins
                  if not (S.DEF.match(x) and S.DEF.match(x).group(1) in selected)]
    entry = fn.blocks[0][1]
    at = next(i for i, x in enumerate(entry) if x.startswith("  %v6 = ")) + 1
    entry[at:at] = lifted
    return fn.emit()


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--repo", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--isa", action="store_true")
    a = p.parse_args()
    sys.path[:0] = [str(a.repo / "gpu-shaders/materials")]
    import dxcom
    import sfb_rewrite as S
    original = a.out / "original.dxbc"
    if sha(original.read_bytes()) != PS_SHA:
        raise ValueError("original container mismatch")
    ll = dxcom.dumpbin(str(original))
    out = rewrite(ll, S, dxcom)
    candidate = a.out / "uv-hoist.dxbc"
    candidate.write_bytes(dxcom.ll_to_container(out))
    actual = dxcom.dumpbin(str(candidate))
    if dxcom.body(actual) != dxcom.body(out):
        raise ValueError("assembled body differs")
    po, pn = dxcom.parts(original.read_bytes()), dxcom.parts(candidate.read_bytes())
    for key in ("ISG1", "OSG1", "PSV0", "SFI0"):
        if po.get(key) != pn.get(key):
            raise ValueError("metadata differs: " + key)
    # Resource op sequence must remain identical modulo SSA renumbering: counts
    # alone are NOT a whole-output proof; the exact rewrite guard binds operands.
    ops = lambda t: re.findall(r"@dx\.op\.([\w.]+)\(", t.split("define void @main()", 1)[1].split("\n}", 1)[0])
    from collections import Counter
    delta = Counter(ops(ll))
    delta.subtract(ops(actual))
    if {k: v for k, v in delta.items() if v} != {"unary.f32": 4, "binary.f32": 8}:
        raise ValueError("unexpected opcode delta")
    (a.out / "uv-hoist.ll").write_text(actual, encoding="utf-8")
    result = dict(original_sha256=PS_SHA, candidate_sha256=sha(candidate.read_bytes()),
                  original_body_sha256=BODY_SHA, candidate_body_sha256=sha(dxcom.body(actual).encode()),
                  removed_pure_ops=12, changed_resource_operations=0,
                  native_admitted=False, proof="Exact bound pure subgraph substitution; codegen gate pending")
    if a.isa:
        import rga_ps
        tempfile.tempdir = str(a.out.resolve())
        vs = a.repo / "gpu-shaders/meshvs/work/1559714c.dxbc"
        st, isa = rga_ps.stats(str(vs), str(candidate), "gfx1103")
        (a.out / "uv-hoist.gfx1103.isa.txt").write_text(isa)
        result.update(stats=st, isa_sha256=sha(isa.encode()))
    (a.out / "uv-hoist.json").write_text(json.dumps(result, indent=2))
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
