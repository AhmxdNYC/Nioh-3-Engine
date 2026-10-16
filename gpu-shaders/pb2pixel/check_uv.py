"""Independent per-block expression/resource/control certificate for UV hoist."""
import argparse
import hashlib
from pathlib import Path
import re
import random
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
    actual_path = a.out / "uv-hoist.dxbc"
    if actual_path.exists():
        actual = dxcom.dumpbin(str(actual_path))
        assert dxcom.body(actual) == dxcom.body(out), "actual candidate differs from exact rewrite"
        out = actual
    original = certificate(ll, S)
    assert original == certificate(out, S), "resource/output/control mismatch"
    # Mutate the actual candidate text, not a detached mathematical model.
    mutants = [out.replace("i32 22, float %5", "i32 22, float %6", 1),
               out.replace("i32 0, i32 3, i1 false", "i32 0, i32 4, i1 false", 1),
               out.replace("float 1.000000e+00", "float 5.000000e-01", 1)]
    for mutant in mutants:
        assert mutant != out
        assert certificate(mutant, S) != original, "semantic mutation escaped"
    if actual_path.exists():
        import verify_sfb as V
        A, B = V.psim.PShader(ll, "original"), V.psim.PShader(out, "candidate")
        coverage = [0] * len(B.m.blocks)
        for scene in range(4):
            # Reassembly drops debug resource names. Bind both programs to the
            # ORIGINAL scene: debug names must not change sampler oracle identity.
            sca, ua = V.scene_for(ll, random.Random(1000 + scene))
            scb, ub = V.scene_for(ll, random.Random(1000 + scene))
            pixels = V.pixels_for(ll, random.Random(1000 + scene), 64)
            ra, _ = A.run_pixels(sca, pixels)
            rb, _ = B.run_pixels(scb, pixels, coverage)
            assert ra == rb, "finite full-output model mismatch"
            assert [x.writes for x in ua] == [x.writes for x in ub]
        print("Finite shared-binding model PASS: 256 pixels; blocks covered=" + str(sum(x > 0 for x in coverage)) + "/" + str(len(coverage)))
    print("PASS: 10-block resource/output/control certificate; 3 actual-body mutants rejected; actual signed candidate checked=" + str(actual_path.exists()))


if __name__ == "__main__":
    main()
