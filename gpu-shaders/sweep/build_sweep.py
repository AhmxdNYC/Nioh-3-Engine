r"""build_sweep.py - STAGE (never install) the shader-sweep replacements.

    py -I build_sweep.py              build every item below
    py -I build_sweep.py mpc23        one item

For each item: read the game file read-only (first run copies it to gpu-shaders\backup\), check the target record's
container digest (= Steam v2.02), compile the *_fast.hlsl with the Microsoft-signed dxc bundled with RDTS
(validator 1.5, same as the shipped shaders, -Zs so the container carries an ILDN part like the originals), check the
interface against the shipped record, splice it in (s2g.py; all other records byte-identical, the CXDK interface tail
kept), re-pack and write gpu-shaders\replacement\<file>. Also writes build\<item>_orig.ll / _fast.ll (dxc -dumpbin)
for verify_sweep.py and rga.

Interface check (all must be identical to the shipped record): DXBC part list, SFI0 (feature flags), ISG1/OSG1
(signatures), PSV0 (resource bindings, numthreads, ...), the dumpbin header (cbuffer layout, resource names, types,
registers) and the [numthreads] metadata.
"""
import hashlib, os, re, shutil, subprocess, sys
HERE = os.path.dirname(os.path.abspath(__file__))
GS = os.path.dirname(HERE)
sys.path.insert(0, HERE)
sys.path.insert(0, GS)
import idrk, s2g  # noqa: E402

SD = r"C:\Program Files (x86)\Steam\steamapps\common\Nioh3\shader_dx12"
DXC = os.path.join(GS, "..", "tools", "rdts", "RadeonDeveloperToolSuite-2026-05-28-1806", "utils", "dx12", "dxc", "dxc.exe")
BUILD = os.path.join(HERE, "build")

# item -> (files that carry the record, record index, original container digest, hlsl, target, extra dxc args)
ITEMS = {
    "mpc23": (["0x29ee0902.file"], 23, "34078a288341dd83cbe0e7da50a6367a", "MPC23_fast.hlsl", "cs_6_0", []),
    "toonedge2": (["0x9c481f79.file", "0x47bc5cb4.file"], 2, "318fa766bb0f9ad624957b9f1da0e576", "TOONEDGE2_fast.hlsl", "cs_6_0", []),
}


def sha(b):
    return hashlib.sha256(b).hexdigest()


def dumpbin(path):
    return subprocess.run([DXC, "-dumpbin", path], capture_output=True, text=True, check=True).stdout


def iface(ll):
    out = []
    for l in ll.splitlines():
        if l.startswith("target datalayout"):
            break
        if l.startswith(";") and "shader hash" not in l and "shader debug name" not in l and "additional functionality" not in l:
            out.append(l.rstrip())
    nt = re.findall(r"!\{i32 4, !(\d+)\}", ll)
    md = dict(re.findall(r"^!(\d+) = (.*)$", ll, re.M))
    out.append("numthreads " + (md.get(nt[0], "?") if nt else "-"))
    return out


def build(item):
    files, k, odig, src, target, extra = ITEMS[item]
    os.makedirs(BUILD, exist_ok=True)
    os.makedirs(os.path.join(GS, "backup"), exist_ok=True)
    os.makedirs(os.path.join(GS, "replacement"), exist_ok=True)
    out = os.path.join(BUILD, item + "_fast.dxil")
    r = subprocess.run([DXC, "-T", target, "-E", "main", "-O3", "-validator-version", "1.5", "-Zs", "-Fd", BUILD + os.sep,
                        "-Fo", out, os.path.join(HERE, src)] + extra, capture_output=True, text=True)
    if r.returncode:
        print(r.stdout, r.stderr)
        raise SystemExit("compile failed: " + item)
    for f in os.listdir(BUILD):
        if f.endswith(".pdb"):
            os.remove(os.path.join(BUILD, f))
    new = open(out, "rb").read()
    _, ndig, npart = idrk.dxbc_parts(new, 0)
    fast_ll = dumpbin(out)
    open(os.path.join(BUILD, item + "_fast.ll"), "w", encoding="utf-8", newline="\n").write(fast_ll)
    res = []
    for fn in files:
        bak = os.path.join(GS, "backup", fn)
        if not os.path.exists(bak):
            shutil.copy2(os.path.join(SD, fn), bak)
        orig = open(bak, "rb").read()
        g = s2g.Group(orig)
        od = g.dxbc(k)
        assert od[4:20].hex() == odig, "%s #%d digest %s != %s - game updated?" % (fn, k, od[4:20].hex(), odig)
        op_path = os.path.join(BUILD, item + "_orig.dxbc")
        open(op_path, "wb").write(od)
        orig_ll = dumpbin(op_path)
        open(os.path.join(BUILD, item + "_orig.ll"), "w", encoding="utf-8", newline="\n").write(orig_ll)
        _, _, opart = idrk.dxbc_parts(od, 0)
        assert sorted(opart) == sorted(npart), (sorted(opart), sorted(npart))
        for part in ("SFI0", "ISG1", "OSG1", "PSV0"):
            a = od[opart[part][0]:opart[part][0] + opart[part][1]]
            b = new[npart[part][0]:npart[part][0] + npart[part][1]]
            assert a == b, "%s: %s differs" % (item, part)
        io, inew = iface(orig_ll), iface(fast_ll)
        if io != inew:
            for a, b in zip(io, inew):
                if a != b:
                    print("  orig:", a); print("  fast:", b)
            raise SystemExit("%s: interface differs" % item)
        newp = g.replace({k: new})
        g.verify(newp, {k: new})
        repl = idrk.pack(newp, orig)
        assert idrk.unpack(repl) == newp
        dst = os.path.join(GS, "replacement", fn)
        open(dst, "wb").write(repl)
        res.append((fn, sha(orig), sha(repl), len(orig), len(repl)))
        print("  %s %s #%d: %d -> %d B DXBC, container digest %s -> %s; %d other records byte-identical; "
              "parts/SFI0/ISG1/OSG1/PSV0/bindings identical" % (fn, g.name, k, len(od), len(new), odig, ndig.hex(),
                                                                len(g.ents) - 1))
        print("    original    sha256 %s (%d B)" % (sha(orig), len(orig)))
        print("    replacement sha256 %s (%d B) -> %s" % (sha(repl), len(repl), dst))
    return res


if __name__ == "__main__":
    for it in (sys.argv[1:] or list(ITEMS)):
        print("==", it)
        build(it)
    print("STAGED (not installed).")
