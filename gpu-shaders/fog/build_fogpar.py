r"""build_fogpar.py - STAGE (never install) the 'onlypar' volumetric-fog variant (item `fogpar`, notes\ally-gpu-budget.md s6).

VFSCAT #235 (shader_dx12\0xa9910db8.file, CS 641524ee) with ONLY the merged early-exit VSM clip-level search of the
2026-10-07 rewrite (notes\gpu-fog-rewrite.md item 1). No light-list prefetch, no speculative spot loads, no tail
reorder: those cost VGPRs / bandwidth and made the full rewrite +0.33..0.54 ms slower on the Ally (RADV).

    py -I mkfog.py --no-cl --no-tail ; move vf235_src.hlsl -> vf235_par_src.hlsl ; git checkout vf235_src.hlsl
    py -I ..\gi\mkfast.py (build('vf235', tag='par') with src = vf235_par_src.hlsl, pins = vf235_pins.txt)
    py -I ..\gi\verify.py ..\..\re\scripts_gpu\out\VFSCAT_235.ll ..\gi\out\vf235_par.ll --kind vfscat ...
    py -I build_fogpar.py

Outputs (both untracked, same bytes for record #235):
  gpu-shaders\replacement\0xa9910db8.file        file-swap pack (ally_shaders.sh / apply_fog.ps1 item fogpar)
  gpu-shaders\fog\out\psoswap\641524ee74b197c7d8f1535066bb2c1a.dxil   ASI [ShaderSwap] file (no game file touched)
The packaged DXIL must dump to exactly gi\out\vf235_par.ll, the file the bit-exact proof ran on.
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import build_fog as F  # noqa: E402

PAR_HLSL = os.path.join(F.GI, "out", "vf235_par.hlsl")
PAR_LL = os.path.join(F.GI, "out", "vf235_par.ll")
KEY = "641524ee74b197c7d8f1535066bb2c1a"


def main():
    F.RECORDS = {235: (KEY, PAR_HLSL)}
    F.BUILD = os.path.join(HERE, "build_par")
    F.main()
    # the record that went into the pack, re-read from the pack itself
    p = F.idrk.unpack(open(os.path.join(F.GS, "replacement", F.FILE), "rb").read())
    tab, base, ents = F.S.layout(p)
    o, s = ents[235]
    rec = p[base + o:base + o + s]
    tmp = os.path.join(F.BUILD, "vf235_par_packed.dxil")
    open(tmp, "wb").write(rec)
    packed_ll = F.B.dumpbin(tmp)
    proven = open(PAR_LL, encoding="utf-8").read()
    assert _body(packed_ll) == _body(proven), "packed #235 != proven vf235_par.ll"
    d = os.path.join(HERE, "out", "psoswap")
    os.makedirs(d, exist_ok=True)
    dst = os.path.join(d, KEY + ".dxil")
    open(dst, "wb").write(rec)
    print("    psoswap file %s (%d B, sha256 %s)" % (dst, len(rec), F.B.sha(rec)))
    print("    packed #235 dumpbin body == gi\\out\\vf235_par.ll (the proof input)")


def _body(ll):
    """dumpbin text minus the comment header (source path / hash lines differ between dxc runs on different paths)"""
    return [l for l in ll.replace("\r\n", "\n").split("\n") if l and not l.startswith(";")]


if __name__ == "__main__":
    main()
    print("STAGED (not installed).")
