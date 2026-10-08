r"""runtime_inventory.py -- inventory a psoswap DumpMode=2 dump (Ally, Proton, MAX settings).
Per blob: container digest (= dump file name), DXIL HASH digest, stage, resource names, cbuffer fields, op counts
(samples / loads / gathers / loops / instructions), r5 RGP time if the digest is in pso_map.csv, and a pass guess.
Disassembly (dxc -dumpbin / D3DDisassemble) is cached next to the dump (<dump>\..\asm\), never in the repo.
Usage: py -I runtime_inventory.py D:\nioh3-re\ally-dump\shaderdump_172\dump  -> out\runtime_inventory.csv
"""
import concurrent.futures as cf, csv, ctypes, os, re, struct, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
DXC = os.path.join(ROOT, "tools", "rdts", "RadeonDeveloperToolSuite-2026-05-28-1806", "utils", "dx12", "dxc", "dxc.exe")
PSO = os.path.join(ROOT, "re", "scripts_gpu", "out", "rgp_top300", "Nioh3_2026-10-06_19-39-58_shaders", "pso_map.csv")
# runtime post passes identified from cbuffer / resource names (Ally dump 172)
ROLE = {"f11afe97": "MB tile max (REWRITTEN)", "4e13e236": "MB gather (REWRITTEN)", "615daafd": "MB velocity decode",
        "76a3c427": "MB neighbour max", "d8fc9faf": "MB VS", "cf792d56": "DOF gather+composite (REWRITTEN)",
        "51dd7477": "DOF CoC/downsample", "b5dfef9e": "DOF CoC/downsample", "e7318142": "DOF", "745a18ff": "DOF",
        "baff8a76": "DOF", "3dd31ae3": "DOF CS", "e3cb5cbe": "DOF CS", "01da6d84": "DOF VS", "523efaab": "DOF VS",
        "04c1052d": "composite + CA (REWRITTEN)", "e6efc8ff": "composite + CA (REWRITTEN)", "278e3bad": "composite",
        "6a305be4": "composite (r5)", "d21b4b07": "composite VS", "17149dca": "bloom downsample", "18a4c2bb": "bloom",
        "b5b4638c": "bloom", "7139b08f": "bloom bright pass", "c5909527": "bloom VS", "796e2cdd": "lens flare",
        "b94af32d": "lens flare", "2c9c30db": "lens flare VS", "df455421": "lens flare VS",
        "92075474": "eye adaptation CS", "a1712355": "eye adaptation CS", "6ee82656": "eye adaptation CS",
        "0cc29779": "tonemap", "9ea0865d": "heat haze/noise", "f3596c47": "heat haze/noise"}
DXIL_INDEX = os.path.join(ROOT, "gpu-shaders", "dxil_index.csv")
EXE_INV = os.path.join(HERE, "out", "inventory.csv")
KNOWN = {"17149dca": "bloom (r5 131us)", "18a4c2bb": "bloom (r5 40us)", "6a305be4": "compose (r5 47us)",
         "df4adc57": "post df4adc57 (r5 86us)", "c5909527": "post VS (bloom)", "d21b4b07": "post VS"}
# resource / cbuffer name -> pass guess (first match wins)
PASS_RULES = [("motion blur", r"(?i)motionblur|\bMB_|velocity|vVelocity|tVelocity|fMotionBlur|nMBSample|MBlur"),
              ("dof", r"(?i)\bdof|DepthOfField|tCoC|fCoC|bokeh|vFocus|FocalLen|fNearBlur|fFarBlur"),
              ("bloom", r"(?i)bloom|glare|\bfBloom|tBloom|BrightPass|vBloom"),
              ("tonemap/compose", r"(?i)tonemap|ToneMap|\bLUT|tLut|colorgrad|vignette|fExposure|tExposure"),
              ("fsr/upscale", r"(?i)ffx|fsr|rcas|upscal"),
              ("taa", r"(?i)\btaa|history|tHistory|jitter")]


def parse_container(b):
    d = b[4:20].hex()
    nparts = struct.unpack_from("<I", b, 28)[0]
    parts, hdig, stage = {}, "", "?"
    for i in range(nparts):
        off = struct.unpack_from("<I", b, 32 + 4 * i)[0]
        fourcc = b[off:off + 4].decode("latin1")
        sz = struct.unpack_from("<I", b, off + 4)[0]
        parts[fourcc] = (off + 8, sz)
        if fourcc == "HASH":
            hdig = b[off + 12:off + 28].hex()
    for fc in ("DXIL", "SHEX", "SHDR"):
        if fc in parts:
            o = parts[fc][0]
            ver = struct.unpack_from("<I", b, o)[0]
            kind = (ver >> 16) & 0xFFFF
            stage = {0: "ps", 1: "vs", 2: "gs", 3: "hs", 4: "ds", 5: "cs", 13: "ms", 14: "as"}.get(kind, "k%d" % kind)
            break
    return d, hdig, stage, sorted(parts)


d3dc = None


def disasm_dxbc(data):
    global d3dc
    if d3dc is None:
        d3dc = ctypes.WinDLL("D3DCompiler_47.dll")
    out = ctypes.c_void_p()
    if d3dc.D3DDisassemble(data, len(data), 0, None, ctypes.byref(out)) != 0:
        return ""
    vt = ctypes.cast(ctypes.cast(out, ctypes.POINTER(ctypes.c_void_p))[0], ctypes.POINTER(ctypes.c_void_p))
    gp = ctypes.WINFUNCTYPE(ctypes.c_void_p, ctypes.c_void_p)(vt[3])
    gs = ctypes.WINFUNCTYPE(ctypes.c_size_t, ctypes.c_void_p)(vt[4])
    t = ctypes.string_at(gp(out), gs(out)).decode("latin1")
    ctypes.WINFUNCTYPE(ctypes.c_ulong, ctypes.c_void_p)(vt[2])(out)
    return t


def get_asm(path, asmdir):
    name = os.path.basename(path)
    ap = os.path.join(asmdir, name + ".txt")
    if os.path.exists(ap):
        return open(ap, encoding="latin1").read()
    if path.endswith(".dxil"):
        t = subprocess.run([DXC, "-dumpbin", path], capture_output=True, text=True, errors="replace").stdout
    else:
        t = disasm_dxbc(open(path, "rb").read())
    open(ap, "w", encoding="latin1", errors="replace").write(t)
    return t


def summarize(t, dxil):
    if dxil:
        res = re.findall(r";\s+(\S+)\s+(sampler|texture|UAV|cbuffer)\s+\S+\s+\S+\s+\S+\s+\S+\s+\d+", t)
        names = ["%s" % n for n, _ in res]
        cbv = re.findall(r";\s+(?:row_major\s+)?\w+\s+(\w+(?:\[\d+\])?);\s+; Offset", t)
        body = [l for l in t.splitlines() if l.startswith("  ") and "=" in l or l.strip().startswith(("call", "store", "br "))]
        samples = len(re.findall(r"@dx\.op\.sample\w*\.", t))
        loads = len(re.findall(r"@dx\.op\.(?:textureLoad|bufferLoad|rawBufferLoad)\.", t))
        gathers = len(re.findall(r"@dx\.op\.textureGather\w*\.", t))
        loops = len(re.findall(r"!llvm\.loop", t))
        ins = len(body)
    else:
        res = re.findall(r"//\s+(\S+)\s+(sampler|texture|UAV|cbuffer|tbuffer)\s+\S+\s+\S+\s+\S+\s+\d+", t)
        names = [n for n, _ in res]
        cbv = re.findall(r"//\s+(?:row_major\s+)?\w+\s+(\w+(?:\[\d+\])?);\s+// Offset", t)
        lines = [l.strip() for l in t.splitlines() if l.strip() and not l.startswith("//") and not l.strip().startswith("dcl_")]
        samples = sum(1 for l in lines if re.match(r"sample", l))
        loads = sum(1 for l in lines if re.match(r"ld", l))
        gathers = sum(1 for l in lines if re.match(r"gather", l))
        loops = sum(1 for l in lines if l.startswith("loop"))
        ins = len(lines)
    return names, cbv, samples, loads, gathers, loops, ins


def r5_times():
    t = {}
    if not os.path.exists(PSO):
        return t
    for r in csv.DictReader(open(PSO)):
        for m in re.finditer(r"(\w+):([0-9a-f]{32})", r["shaders(stage:dxbc_digest=file/group#index)"]):
            k = m.group(2)
            t.setdefault(k, [0.0, set()])
            t[k][0] += float(r["seg_dur_us"])
            t[k][1].add(r["seg"])
    return t


def main():
    dump = sys.argv[1]
    asmdir = os.path.join(os.path.dirname(os.path.abspath(dump)), "asm")
    os.makedirs(asmdir, exist_ok=True)
    files = sorted(os.path.join(dump, f) for f in os.listdir(dump) if f.endswith((".dxil", ".dxbc")))
    dx = [f for f in files if f.endswith(".dxil")]
    with cf.ThreadPoolExecutor(12) as ex:
        list(ex.map(lambda f: get_asm(f, asmdir), dx))
    times = r5_times()
    disk = {}
    if os.path.exists(DXIL_INDEX):
        for r in csv.DictReader(open(DXIL_INDEX)):
            disk[r["hash_digest"]] = disk[r["container_digest"]] = "disk:%s/%s" % (r["file"], r["group"])
    exe = set()
    if os.path.exists(EXE_INV):
        exe = {r["digest"] for r in csv.DictReader(open(EXE_INV))}
    rows = []
    for f in files:
        b = open(f, "rb").read()
        d, hd, stage, parts = parse_container(b)
        t = get_asm(f, asmdir)
        names, cbv, samples, loads, gathers, loops, ins = summarize(t, f.endswith(".dxil"))
        text = " ".join(names + cbv)
        guess = ""
        for k in (d[:8], hd[:8]):
            if k in KNOWN:
                guess = KNOWN[k]
        if not guess:
            for g, rx in PASS_RULES:
                if re.search(rx, text):
                    guess = g
                    break
        r5 = times.get(hd) or times.get(d) or [0.0, set()]
        src = disk.get(d) or disk.get(hd) or ("exe" if d in exe else "runtime")
        role = ROLE.get(d[:8], "")
        rows.append(dict(file=os.path.basename(f), digest=d, hash=hd, stage=stage, source=src, role=role, size=len(b),
                         ins=ins, samples=samples,
                         loads=loads, gathers=gathers, loops=loops, r5_us="%.1f" % r5[0], r5_segs=" ".join(sorted(r5[1])),
                         guess=guess, resources=" ".join(names), cbuffer=" ".join(cbv[:40])))
    out = os.path.join(HERE, "out", "runtime_inventory.csv")
    with open(out, "w", newline="") as fo:
        w = csv.DictWriter(fo, fieldnames=list(rows[0]))
        w.writeheader()
        w.writerows(rows)
    import collections
    print("%d blobs -> %s" % (len(rows), out))
    print(collections.Counter((r["source"].split(":")[0], r["stage"], r["file"][-4:]) for r in rows))


if __name__ == "__main__":
    main()
