r"""extract_cull.py - dump records of the culling shader groups (read-only from the game folder) as DXBC + dxc -dumpbin.

    py -I extract_cull.py                 # default set: MPC #22 #0 #23 #28-#31 #24 #26 #27, MIC #0, MshLod #0-#6
    py -I extract_cull.py 0x29ee0902.file 23 28
"""
import os, subprocess, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "vendor"))
import s2g  # noqa: E402

SD = r"C:\Program Files (x86)\Steam\steamapps\common\Nioh3\shader_dx12"
DXC = os.path.join(HERE, "..", "..", "tools", "rdts", "RadeonDeveloperToolSuite-2026-05-28-1806", "utils", "dx12", "dxc", "dxc.exe")
OUT = os.path.join(HERE, "extract")
DEFAULT = {"0x29ee0902.file": [22, 0, 23, 28, 29, 30, 31, 24, 26, 27],
           "0x17ad25bb.file": [0],
           "0x8d9282e6.file": [0, 1, 2, 3, 4, 5, 6]}


def main():
    os.makedirs(OUT, exist_ok=True)
    want = DEFAULT if len(sys.argv) < 2 else {sys.argv[1]: [int(x) for x in sys.argv[2:]]}
    for fn, recs in want.items():
        g = s2g.Group(open(os.path.join(SD, fn), "rb").read())
        print(fn, g.name, len(g.ents), "records")
        for k in recs:
            d = g.dxbc(k)
            p = os.path.join(OUT, "%s_%d.dxbc" % (g.name, k))
            open(p, "wb").write(d)
            ll = subprocess.run([DXC, "-dumpbin", p], capture_output=True, text=True, check=True).stdout
            open(p[:-5] + ".ll", "w", encoding="utf-8", newline="\n").write(ll)
            print("  #%d %d B digest %s" % (k, len(d), d[4:20].hex()))


if __name__ == "__main__":
    main()
