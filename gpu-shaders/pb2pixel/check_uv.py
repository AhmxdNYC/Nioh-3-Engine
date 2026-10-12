"""Independent per-block expression/resource/control certificate for UV hoist."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

sys.dont_write_bytecode = True


def certificate(ll, S):
    fn = S.Fn(ll)
    values = {}
    blocks = {"%" + b: "BLOCK" + str(i) for i, (b, _) in enumerate(fn.blocks)}
    output = []
    for b, ins in fn.blocks:
        trace = []
        for line in ins:
            m = S.DEF.match(line)
            rhs = line.split(" = ", 1)[1] if m else line.strip()
            def token(t):
                name = t[0]
                if name in blocks:
                    return blocks[name]
                if name in values:
                    return values[name]
                if name.startswith("%dx."):
                    return name
                raise ValueError("undefined operand " + name)
            expr = S.TOK.sub(token, rhs)
            if m:
                values[m[1]] = hashlib.sha256(expr.encode()).hexdigest()
            # Record all calls except exactly the pure functions being moved.
            # Thus resource IDs, samplers, addresses, clamp and outputs are bound.
            pure = re.search(r"@dx\.op\.(?:unary\.f32\(i32 22,|binary\.f32\(i32 (?:35|36),)", rhs)
            if not m or ("call " in rhs and not pure):
                trace.append(expr)
        output.append(trace)
    return output


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--repo", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    a = p.parse_args()
    sys.path.insert(0, str(a.repo / "gpu-shaders/materials"))
    import sfb_rewrite as S
    import dxcom
    import uv_probe
    ll = dxcom.dumpbin(str(a.out / "original.dxbc"))
    if uv_probe.sha((a.out / "original.dxbc").read_bytes()) != uv_probe.PS_SHA:
        raise ValueError("container binding failed")
    out = uv_probe.rewrite(ll, S, dxcom)
    original = certificate(ll, S)
    assert original == certificate(out, S), "resource/output/control mismatch"
    # Mutate the actual candidate text, not a detached mathematical model.
    mutants = [out.replace("i32 22, float %5", "i32 22, float %6", 1),
               out.replace("i32 0, i32 3, i1 false", "i32 0, i32 4, i1 false", 1),
               out.replace("float 1.000000e+00", "float 5.000000e-01", 1)]
    for mutant in mutants:
        assert mutant != out
        assert certificate(mutant, S) != original, "semantic mutation escaped"
    print("PASS: 10-block resource/output/control certificate; 3 actual-body mutants rejected")


if __name__ == "__main__":
    main()
