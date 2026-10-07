r"""build_sweep.py - STAGE (never install) the shader-sweep replacements.

    py -I build_sweep.py              build every item below
    py -I build_sweep.py mpc          one item

For each item: read the game file read-only (first run copies it to gpu-shaders\backup\), check every target record's
container digest (= Steam v2.02), compile each *.hlsl with the Microsoft-signed dxc bundled with RDTS (validator 1.5,
same as the shipped shaders, -Zs so the container carries an ILDN part like the originals), check the interface against
the shipped record, splice all of the item's records into ONE group (s2g.py; all other records byte-identical, the
CXDK interface tail kept), re-pack and write gpu-shaders\replacement\<file>. Also writes build\<item>_orig.ll /
_fast.ll (single-record items) or build\<item>_<k>_orig.ll / _fast.ll (multi-record items) for the verifiers and rga.

One game file = one item: records of the same .file that are replaced together go into the same item, so two items
never write the same replacement file. Items with write=False are compiled and interface-checked only (kept for
their standalone proofs), they do not produce a replacement file.

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
CULL = os.path.join(GS, "culling")

# item -> dict(files that carry the records, records {index: (original container digest, hlsl)}, target, extra dxc
#              args, write: produce gpu-shaders\replacement\<file>)
ITEMS = {
    # meshlet-culling prefix-sum chain (notes\gpu-culling-rewrite.md): #23 computes the whole chain when cnt <= 1024
    # (else the shipped block-local sums via an LDS scan, the sweep's MPC23_fast algorithm); #28-#31 return early
    # when cnt <= 1024 (else shipped code). Proven chain-level by gpu-shaders\culling\verify_culling.py.
    "mpc": dict(files=["0x29ee0902.file"], target="cs_6_0", extra=[], write=True, records={
        23: ("34078a288341dd83cbe0e7da50a6367a", os.path.join(CULL, "MPC23_chain.hlsl")),
        28: ("4225b9ffcea28ebd44af839681b0d4e0", os.path.join(CULL, "MPC28_chain.hlsl")),
        29: ("6b87b43e989867072653c56649a53fff", os.path.join(CULL, "MPC29_chain.hlsl")),
        30: ("3ecb247d53b412aabd8f9a96033ce992", os.path.join(CULL, "MPC30_chain.hlsl")),
        31: ("de4d71b0118bbfa7b6b478eeb6cc7730", os.path.join(CULL, "MPC31_chain.hlsl"))}),
    # the sweep's standalone MPC#23 (record-exact on its own); superseded by "mpc" (same game file), kept for its proof
    "mpc23": dict(files=["0x29ee0902.file"], target="cs_6_0", extra=[], write=False, records={
        23: ("34078a288341dd83cbe0e7da50a6367a", os.path.join(HERE, "MPC23_fast.hlsl"))}),
    # meshlet LOD chain (notes\gpu-culling-rewrite.md): MshLod#3's per-thread serial pad loop spread over the lanes.
    # Proven by gpu-shaders\culling\verify_culling.py mshlod.
    "mshlod": dict(files=["0x8d9282e6.file"], target="cs_6_0", extra=[], write=True, records={
        3: ("a1426af02a72961478ba0a7c40ae2db6", os.path.join(CULL, "MshLod3_fast.hlsl"))}),
    "toonedge2": dict(files=["0x9c481f79.file", "0x47bc5cb4.file"], target="cs_6_0", extra=[], write=True, records={
        2: ("318fa766bb0f9ad624957b9f1da0e576", os.path.join(HERE, "TOONEDGE2_fast.hlsl"))}),
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


def stem(item, k, multi):
    return "%s_%d" % (item, k) if multi else item


def compile_record(item, k, src, target, extra, multi):
    out = os.path.join(BUILD, stem(item, k, multi) + "_fast.dxil")
    r = subprocess.run([DXC, "-T", target, "-E", "main", "-O3", "-validator-version", "1.5", "-Zs", "-Fd", BUILD + os.sep,
                        "-Fo", out, src] + extra, capture_output=True, text=True)
    if r.returncode:
        print(r.stdout, r.stderr)
        raise SystemExit("compile failed: %s #%d" % (item, k))
    for f in os.listdir(BUILD):
        if f.endswith(".pdb"):
            os.remove(os.path.join(BUILD, f))
    new = open(out, "rb").read()
    fast_ll = dumpbin(out)
    open(os.path.join(BUILD, stem(item, k, multi) + "_fast.ll"), "w", encoding="utf-8", newline="\n").write(fast_ll)
    return new, fast_ll


def check_iface(item, k, od, new, orig_ll, fast_ll):
    _, _, opart = idrk.dxbc_parts(od, 0)
    _, _, npart = idrk.dxbc_parts(new, 0)
    assert sorted(opart) == sorted(npart), (item, k, sorted(opart), sorted(npart))
    for part in ("SFI0", "ISG1", "OSG1", "PSV0"):
        a = od[opart[part][0]:opart[part][0] + opart[part][1]]
        b = new[npart[part][0]:npart[part][0] + npart[part][1]]
        assert a == b, "%s #%d: %s differs" % (item, k, part)
    io, inew = iface(orig_ll), iface(fast_ll)
    if io != inew:
        for a, b in zip(io, inew):
            if a != b:
                print("  orig:", a); print("  fast:", b)
        raise SystemExit("%s #%d: interface differs" % (item, k))


def build(item):
    it = ITEMS[item]
    recs = it["records"]
    multi = len(recs) > 1
    os.makedirs(BUILD, exist_ok=True)
    os.makedirs(os.path.join(GS, "backup"), exist_ok=True)
    os.makedirs(os.path.join(GS, "replacement"), exist_ok=True)
    compiled = {k: compile_record(item, k, src, it["target"], it["extra"], multi) for k, (_, src) in recs.items()}
    res = []
    for fn in it["files"]:
        bak = os.path.join(GS, "backup", fn)
        if not os.path.exists(bak):
            shutil.copy2(os.path.join(SD, fn), bak)
        orig = open(bak, "rb").read()
        g = s2g.Group(orig)
        new = {}
        for k, (odig, _) in recs.items():
            od = g.dxbc(k)
            assert od[4:20].hex() == odig, "%s #%d digest %s != %s - game updated?" % (fn, k, od[4:20].hex(), odig)
            op_path = os.path.join(BUILD, stem(item, k, multi) + "_orig.dxbc")
            open(op_path, "wb").write(od)
            orig_ll = dumpbin(op_path)
            open(os.path.join(BUILD, stem(item, k, multi) + "_orig.ll"), "w", encoding="utf-8", newline="\n").write(orig_ll)
            nb, fast_ll = compiled[k]
            check_iface(item, k, od, nb, orig_ll, fast_ll)
            new[k] = nb
            print("  %s %s #%d: %d -> %d B DXBC, container digest %s -> %s; parts/SFI0/ISG1/OSG1/PSV0/bindings identical"
                  % (fn, g.name, k, len(od), len(nb), odig, idrk.dxbc_parts(nb, 0)[1].hex()))
        if not it["write"]:
            print("    (record-only item: no replacement file written)")
            continue
        newp = g.replace(new)
        g.verify(newp, new)
        repl = idrk.pack(newp, orig)
        assert idrk.unpack(repl) == newp
        dst = os.path.join(GS, "replacement", fn)
        open(dst, "wb").write(repl)
        res.append((fn, sha(orig), sha(repl), len(orig), len(repl)))
        print("    %d other records byte-identical" % (len(g.ents) - len(new)))
        print("    original    sha256 %s (%d B)" % (sha(orig), len(orig)))
        print("    replacement sha256 %s (%d B) -> %s" % (sha(repl), len(repl), dst))
    return res


if __name__ == "__main__":
    for it in (sys.argv[1:] or list(ITEMS)):
        print("==", it)
        build(it)
    print("STAGED (not installed).")
