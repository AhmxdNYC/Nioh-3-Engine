r"""build_gi.py - STAGE (never install) the GI-gather replacement: RefProbe #121 (diffuse probe GI) and #414
(reflection probe trace) in shader_dx12\0xd070067d.file ("RefProbe-CllPtSfbSplsSslsSsmVsm", 517 compute records).

    py -I mkfast.py rp121        # 1. regenerate out\rp121_fast.hlsl from the shipped DXIL (decompile+collapse+pins)
    py -I mk414.py && py -I mkfast.py rp414   #  and out\rp414_fast.hlsl (restructure + pins; compiled with -HV 2018)
    py -I build_gi.py            # 2. compile, interface-check, splice, re-pack -> gpu-shaders\replacement\0xd070067d.file

Compiles and interface-checks with sweep\build_sweep.py's functions (same dxc, validator 1.5, -Zs; DXBC parts / SFI0 /
ISG1 / OSG1 / PSV0 / dumpbin interface must equal the shipped record; the only tolerated text difference is the
LLVM type name of the $Globals layout in the reflection comment, "dx.alignment.legacy." (shipped dxc) vs
"hostlayout." (RDTS dxc), same members/offsets). The splice uses gi\s2g.py because this group's record table is not
in storage order (sweep's s2g.Group requires that); every other record is moved byte-identically and checked.
Proof of equivalence: verify.py (notes\gpu-gi-rewrite.md).
"""
import os, shutil, sys
HERE = os.path.dirname(os.path.abspath(__file__))
GS = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(GS, "sweep"))
import build_sweep as B  # noqa: E402
sys.path.insert(0, HERE)
sys.modules.pop("s2g", None)
import s2g as S  # noqa: E402  (gi\s2g.py)
import idrk  # noqa: E402

FILE = "0xd070067d.file"
# record: (shipped container digest, replacement source, extra dxc args)
RECORDS = {121: ("4b4c3cbe6724e06841b7ed69d7472a46", os.path.join(HERE, "out", "rp121_fast.hlsl"), []),
           414: ("6ae59bd0ddf1220dc8907954b1f215b4", os.path.join(HERE, "out", "rp414_fast.hlsl"), ["-HV", "2018"])}
BUILD = os.path.join(HERE, "build")
_iface = B.iface
B.iface = lambda ll: [l.replace("hostlayout.", "dx.alignment.legacy.") for l in _iface(ll)]


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
    for k, (odig, src, extra) in RECORDS.items():
        o, s = ents[k]
        od = p[base + o:base + o + s]
        assert od[4:20].hex() == odig, "#%d digest %s != %s - game updated?" % (k, od[4:20].hex(), odig)
        op_path = os.path.join(BUILD, "rp%d_orig.dxbc" % k)
        open(op_path, "wb").write(od)
        orig_ll = B.dumpbin(op_path)
        nb, fast_ll = B.compile_record("rp", k, src, "cs_6_0", extra, True)
        B.check_iface("gi", k, od, nb, orig_ll, fast_ll)
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
