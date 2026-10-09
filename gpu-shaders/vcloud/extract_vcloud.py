r"""extract_vcloud.py - dump the shipped VCLOUD compute records (shader_dx12\0x5f17ca9d.file, group VCLOUD, 8 CXDK
records = 8 compute permutations; record 7 = CS b76874bb, the one GP2 sees on PC/pc2/Ally) to vcloud\extract\."""
import os, struct, sys
HERE = os.path.dirname(os.path.abspath(__file__))
GS = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(GS, "sweep")); sys.path.insert(0, GS)
import build_sweep as B  # noqa: E402
import idrk  # noqa: E402
sys.modules.pop("s2g", None); sys.path.insert(0, os.path.join(GS, "gi"))
import s2g as S  # noqa: E402
FILE = "0x5f17ca9d.file"
out = os.path.join(HERE, "extract"); os.makedirs(out, exist_ok=True)
p = idrk.unpack(open(os.path.join(B.SD, FILE), "rb").read())
tab, base, ents = S.layout(p)
for k, (o, s) in enumerate(ents):
    rec = p[base + o:base + o + s]
    assert rec[:8] == b"CXDK2000"
    n = struct.unpack_from("<I", rec, 8)[0]
    dx = rec[16:16 + n]
    fn = os.path.join(out, "vcloud_cs%d.dxbc" % k)
    open(fn, "wb").write(dx)
    open(fn[:-5] + ".ll", "w", encoding="utf-8").write(B.dumpbin(fn))
    print(k, dx[4:20].hex(), n)
