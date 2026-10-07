r"""disasm_all.py -- disassemble every SM4/5 container from inventory.py (out/blobs/*.dxbc) with the system
D3DCompiler_47 (D3DDisassemble), DXIL ones with dxc -dumpbin, and write out/asm/<digest16>.asm plus a one-line-per-shader
summary out/summary.txt (stage, resources by name, cbuffer vars, instruction / sample / loop counts, r5 time).
Usage: py -I disasm_all.py
"""
import csv, ctypes, os, re, subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(HERE, "out")
DXC = os.path.join(ROOT, "tools", "msvc", "Windows Kits", "10", "bin", "10.0.22621.0", "x64", "dxc.exe")

d3dc = ctypes.WinDLL("D3DCompiler_47.dll")


def blob_text(out):
    vt = ctypes.cast(ctypes.cast(out, ctypes.POINTER(ctypes.c_void_p))[0], ctypes.POINTER(ctypes.c_void_p))
    get_ptr = ctypes.WINFUNCTYPE(ctypes.c_void_p, ctypes.c_void_p)(vt[3])
    get_size = ctypes.WINFUNCTYPE(ctypes.c_size_t, ctypes.c_void_p)(vt[4])
    release = ctypes.WINFUNCTYPE(ctypes.c_ulong, ctypes.c_void_p)(vt[2])
    t = ctypes.string_at(get_ptr(out), get_size(out)).decode("latin1")
    release(out)
    return t


def disasm(data):
    out = ctypes.c_void_p()
    hr = d3dc.D3DDisassemble(data, len(data), 0, None, ctypes.byref(out))
    if hr != 0:
        return None
    return blob_text(out)


def summarize(txt):
    lines = txt.splitlines()
    res = []
    for l in lines:
        m = re.match(r"//\s+(\S+)\s+(sampler|texture|UAV|cbuffer|tbuffer)\s+\S+\s+\S+\s+(\S+)\s+\S+\s+\d+", l)
        if m:
            res.append("%s:%s" % (m.group(3), m.group(1)))
    cbv = re.findall(r"//\s+\w+\s+(\w+(?:\[\d+\])?);\s+// Offset", txt)
    body = [l.strip() for l in lines if l.strip() and not l.startswith("//") and not l.strip().startswith("dcl_")]
    ins = [l for l in body if not l.startswith(("ps_", "vs_", "cs_", "gs_", "hs_", "ds_"))]
    samples = sum(1 for l in ins if re.match(r"(sample|gather|ld|ld_indexable|ldms|ld_structured|ld_raw|ld_uav)", l))
    loops = sum(1 for l in ins if l.startswith("loop"))
    tg = re.search(r"dcl_thread_group (\d+), (\d+), (\d+)", txt)
    return dict(res=" ".join(res), cbv=" ".join(cbv[:24]), ins=len(ins), fetch=samples, loops=loops,
                tg="x".join(tg.groups()) if tg else "")


def main():
    inv = list(csv.DictReader(open(os.path.join(OUT, "inventory.csv"))))
    os.makedirs(os.path.join(OUT, "asm"), exist_ok=True)
    seen = set()
    rows = []
    for r in inv:
        d16 = r["digest"][:16]
        if d16 in seen:
            continue
        seen.add(d16)
        data = open(os.path.join(OUT, "blobs", d16 + ".dxbc"), "rb").read()
        if r["kind"] == "dxil":
            p = subprocess.run([DXC, "-dumpbin", os.path.join(OUT, "blobs", d16 + ".dxbc")], capture_output=True, text=True)
            txt = p.stdout
            s = dict(res="", cbv="", ins=txt.count("\n  %") + txt.count(" = call "), fetch=txt.count("@dx.op.sample") +
                     txt.count("@dx.op.textureLoad") + txt.count("@dx.op.bufferLoad"), loops=0, tg="")
            m = re.search(r"numthreads\((\d+), (\d+), (\d+)\)", txt) or re.search(r"!\{i32 (\d+), i32 (\d+), i32 (\d+)\}", txt)
            s["tg"] = "x".join(m.groups()) if m else ""
            s["res"] = " ".join(sorted(set(re.findall(r"; (\w+)\s+(?:texture|sampler|cbuffer|UAV)", txt))))[:300]
        else:
            txt = disasm(data) or ""
            s = summarize(txt)
        open(os.path.join(OUT, "asm", d16 + ".asm"), "w").write(txt)
        rows.append((float(r["r5_us"]), r, s))
    rows.sort(key=lambda x: (-x[0], x[1]["stage"], x[1]["rva"]))
    with open(os.path.join(OUT, "summary.txt"), "w") as f:
        for us, r, s in rows:
            f.write("%7.1f %-4s %-3s %s rva=%s %6d B ins=%d fetch=%d loops=%d tg=%s | %s | %s\n" % (
                us, r["kind"], r["stage"], r["digest"][:16], r["rva"], int(r["size"]), s["ins"], s["fetch"], s["loops"],
                s["tg"], s["res"], s["cbv"]))
    print(open(os.path.join(OUT, "summary.txt")).read())


if __name__ == "__main__":
    main()
