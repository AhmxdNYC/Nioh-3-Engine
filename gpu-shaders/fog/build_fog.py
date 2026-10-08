r"""build_fog.py - STAGE (never install) the volumetric-fog replacement: VFSCAT #235 ("VFSCAT-CllLpvSfbVsm",
froxel scattering, 160x90x64) in shader_dx12\0xa9910db8.file.

    py -I mkfog.py                 # 1. fog\vf235_src.hlsl from the decompile of the shipped DXIL (restructure)
    py -I ..\gi\mkfast.py vf235    # 2. + pins (fog\vf235_pins.txt) -> gi\out\vf235_fast.{hlsl,dxil,ll}
    py -I build_fog.py             # 3. compile, interface-check, splice, re-pack -> gpu-shaders\replacement\0xa9910db8.file

Same compile/interface rules as gi\build_gi.py (sweep\build_sweep.py functions: RDTS dxc, validator 1.5, -Zs; DXBC
parts / SFI0 / ISG1 / OSG1 / PSV0 / dumpbin interface equal to the shipped record, only the reflection layout type
name "dx.alignment.legacy." vs "hostlayout." tolerated). Splice with gi\s2g.py; every other record byte-identical.
Proof of equivalence: gi\verify.py --kind vfscat (notes\gpu-fog-rewrite.md).
"""
import os, re, shutil, sys
HERE = os.path.dirname(os.path.abspath(__file__))
GS = os.path.dirname(HERE)
GI = os.path.join(GS, "gi")
sys.path.insert(0, os.path.join(GS, "sweep"))
import build_sweep as B  # noqa: E402
sys.path.insert(0, GI)
sys.modules.pop("s2g", None)
import s2g as S  # noqa: E402  (gi\s2g.py)
import idrk  # noqa: E402

FILE = "0xa9910db8.file"
RECORDS = {235: ("641524ee74b197c7d8f1535066bb2c1a", os.path.join(GI, "out", "vf235_fast.hlsl"))}
BUILD = os.path.join(HERE, "build")
_iface = B.iface


def _norm(l):
    """reflection-comment spellings that differ only by the HLSL type annotation, not by layout: the decompile
    declares a structured-buffer member `row_major float4x4 m` as `float4 m[4]` (same 64 bytes at the same offset;
    dxc 1.8 would split row_major matrix-row reads into 4 scalar loads), and the struct type name then loses its
    "dx.alignment.legacy." prefix. Element stride, member offsets, bindings and PSV0 are still compared exactly."""
    l = l.replace("hostlayout.", "dx.alignment.legacy.")
    l = re.sub(r"^(;\s+)row_major float4x4 (\w+);", r"\1float4 \2[4];", l)
    l = re.sub(r"^(;\s+struct )dx\.alignment\.legacy\.(struct\.)", r"\1\2", l)
    return re.sub(r"\s+;", " ;", l)


B.iface = lambda ll: [_norm(l) for l in _iface(ll)]


def main():
    B.BUILD = BUILD
    os.makedirs(BUILD, exist_ok=True)
    os.makedirs(os.path.join(GS, "backup"), exist_ok=True)
    os.makedirs(os.path.join(GS, "replacement"), exist_ok=True)
    bak = os.path.join(GS, "backup", FILE)
    if not os.path.exists(bak):
        shutil.copy2(os.path.join(B.SD, FILE), bak)
    orig = open(bak, "rb").read()
    p = idrk.unpack(orig)
    tab, base, ents = S.layout(p)
    new = {}
    for k, (odig, src) in RECORDS.items():
        o, s = ents[k]
        od = p[base + o:base + o + s]
        assert od[4:20].hex() == odig, "#%d digest %s != %s - game updated?" % (k, od[4:20].hex(), odig)
        op_path = os.path.join(BUILD, "vf%d_orig.dxbc" % k)
        open(op_path, "wb").write(od)
        orig_ll = B.dumpbin(op_path)
        nb, fast_ll = B.compile_record("vf", k, src, "cs_6_0", [], True)
        B.check_iface("fog", k, od, nb, orig_ll, fast_ll)
        new[k] = nb
        print("  %s #%d: %d -> %d B DXBC, container digest %s -> %s; parts/SFI0/ISG1/OSG1/PSV0/bindings identical"
              % (FILE, k, len(od), len(nb), odig, idrk.dxbc_parts(nb, 0)[1].hex()))
    newp = S.splice(p, new)
    repl = idrk.pack(newp, orig)
    assert idrk.unpack(repl) == newp
    dst = os.path.join(GS, "replacement", FILE)
    open(dst, "wb").write(repl)
    print("    %d other records byte-identical" % (len(ents) - len(new)))
    print("    original    sha256 %s (%d B)" % (B.sha(orig), len(orig)))
    print("    replacement sha256 %s (%d B) -> %s" % (B.sha(repl), len(repl), dst))


if __name__ == "__main__":
    main()
    print("STAGED (not installed).")
