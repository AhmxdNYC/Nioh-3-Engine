r"""build_vcloud.py - STAGE (never install) the VCLOUD replacement: shader_dx12\0x5f17ca9d.file record 7 (CS b76874bb,
DrawSky async cloud ray march), notes\gpu-volumetrics-rewrite.md.

    py -I extract_vcloud.py                                   # vcloud\extract\vcloud_cs7.{dxbc,ll} (+ the other 7 permutations)
    py -I ..\gi\ll2hlsl.py extract\vcloud_cs7.ll extract\vcloud_cs7_dec.hlsl
    py -I flatten.py extract\vcloud_cs7_dec.hlsl src\lm4.hlsl # if-convert the 4 light marches (exact)
    py -I hoist.py src\lm4.hlsl src\lm4h2.hlsl --split 7      # issue their samples in 2 batches (8 + 6)
    copy src\lm4h2.hlsl src\vc7_src.hlsl ; py -I ..\gi\mkfast.py vc7    # pinned build (bit-exact proof input)
    py -I build_vcloud.py                                     # both variants -> vcloud\out\{u,p}\

Variants (same restructure, same interface):
  u  src\lm4h2.hlsl as is (dxc fast-math may reassociate: max rel. diff 1.3e-3 on extreme synthetic scenes,
     fp16-equal on 99.9 %; the driver re-fuses FMAs anyway). Fewest instructions -> the Windows A/B candidate.
  p  gi\out\vc7_fast.hlsl = + 726 max(x, x) pins (vc7_pins.txt): bit-exact to the shipped DXIL on the interpreter
     (verify.py --kind vcloud, 512 hold-out seeds, 100 % blocks). Pins fold away under RADV/ACO (Ally), but the AMD
     Windows compiler folds them late (+17 % VALU on gfx1102), so on Windows u is the faster of the two.
Compile/interface rules as build_sweep.py / fog\build_fog.py (RDTS dxc, cs_6_0, -O3, validator 1.5, -Zs; SFI0 / ISG1 /
OSG1 / PSV0 identical, dumpbin interface identical up to the float4[4] vs row_major float4x4 spelling). The record is
CXDK-wrapped: header sizes rebuilt, engine tail kept byte-identical; the other 7 records byte-identical.
"""
import os, shutil, sys
HERE = os.path.dirname(os.path.abspath(__file__))
GS = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(GS, "sweep")); sys.path.insert(0, GS)
import build_sweep as B  # noqa: E402
import idrk  # noqa: E402
sys.modules.pop("s2g", None)
from s2g import Group  # noqa: E402  (sweep\s2g.py: CXDK-aware)
sys.path.insert(0, os.path.join(GS, "fog"))
import build_fog as F  # noqa: E402  (its _norm: reflection-spelling tolerant iface)

FILE = "0x5f17ca9d.file"
REC = 7
KEY = "b76874bb17bdfc10a0739b74191c68a9"
SRC = {"u": os.path.join(HERE, "src", "lm4h2.hlsl"), "p": os.path.join(HERE, "src", "lm4h2_pinned.hlsl"),
       "pfu": os.path.join(HERE, "src", "pf.hlsl"), "pfp": os.path.join(HERE, "src", "pf_pinned.hlsl")}


def main():
    which = sys.argv[1:] or ["u", "p", "pfu", "pfp"]
    bak = os.path.join(GS, "backup", FILE)
    os.makedirs(os.path.dirname(bak), exist_ok=True)
    if not os.path.exists(bak):
        shutil.copy2(os.path.join(B.SD, FILE), bak)
    orig = open(bak, "rb").read()
    g = Group(orig)
    od = g.dxbc(REC)
    assert od[4:20].hex() == KEY, "record %d digest %s - game updated?" % (REC, od[4:20].hex())
    B.BUILD = os.path.join(HERE, "build")
    os.makedirs(B.BUILD, exist_ok=True)
    op = os.path.join(B.BUILD, "vc7_orig.dxbc")
    open(op, "wb").write(od)
    orig_ll = B.dumpbin(op)
    for v in which:
        new, fast_ll = B.compile_record("vc7" + v, REC, SRC[v], "cs_6_0", [], False)
        _check(od, new, orig_ll, fast_ll)
        p2 = g.replace({REC: new})
        g.verify(p2, {REC: new})
        packed = idrk.pack(p2, orig)
        d = os.path.join(HERE, "out", v)
        os.makedirs(d, exist_ok=True)
        open(os.path.join(d, FILE), "wb").write(packed)
        open(os.path.join(d, KEY + ".dxil"), "wb").write(new)
        print("vc7%s: %s %d B sha256 %s | record digest %s | psoswap %s.dxil sha256 %s" % (
            v, FILE, len(packed), B.sha(packed), new[4:20].hex(), KEY, B.sha(new)))
    print("original %s sha256 %s" % (FILE, B.sha(orig)))


def _check(od, new, orig_ll, fast_ll):
    _, _, opart = idrk.dxbc_parts(od, 0)
    _, _, npart = idrk.dxbc_parts(new, 0)
    assert sorted(opart) == sorted(npart), (sorted(opart), sorted(npart))
    for part in ("SFI0", "ISG1", "OSG1", "PSV0"):
        a = od[opart[part][0]:opart[part][0] + opart[part][1]]
        b = new[npart[part][0]:npart[part][0] + npart[part][1]]
        assert a == b, "%s differs" % part
    io = [F._norm(l) for l in B.iface(orig_ll)]
    inew = [F._norm(l) for l in B.iface(fast_ll)]
    if io != inew:
        for a, b in zip(io, inew):
            if a != b:
                print("  orig:", a); print("  fast:", b)
        raise SystemExit("interface differs")


if __name__ == "__main__":
    main()
    print("STAGED (not installed).")
