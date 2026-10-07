r"""rga_stats.py - compile DXIL/DXBC blobs with RGA (offline DX12 driver) for gfx1103 (ROG Ally 780M) and gfx1201
(RX 9070) and summarise the ISA: VGPRs/SGPRs/LDS/scratch, waves per SIMD, instruction and memory-op counts, and
how many times the shader stalls on a memory counter (full waits vs partial waits).

    py -I rga_stats.py name=file.dxil [name=file2.dxil ...] [--out DIR] [--chips gfx1103,gfx1201]
"""
import argparse, os, re, shutil, subprocess, sys, tempfile

RGA = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools", "rdts",
                   "RadeonDeveloperToolSuite-2026-05-28-1806", "rga.exe")
VGPR_FILE = {"gfx1103": 1024, "gfx1201": 1536}     # per SIMD, wave32 registers


def waves_per_simd(chip, vgprs, wave):
    gran = 8 if wave == 32 else 8
    alloc = (vgprs + gran - 1) // gran * gran
    regs = VGPR_FILE[chip] // (2 if wave == 64 else 1)
    return min(16, regs // max(alloc, 1))


def analyse(isa_text):
    lines = [l.strip() for l in isa_text.splitlines()]
    ins = [l.split("//")[0].strip() for l in lines if l and re.match(r"^[a-z_][a-z0-9_]*\s", l + " ") and not l.startswith(";")
           and not l.startswith("shader") and not l.startswith("asic") and not l.startswith("type(")
           and not re.match(r"^(sgpr_count|vgpr_count|wave_size|label_)", l)]
    ins = [i for i in ins if i]
    op = [i.split()[0] for i in ins]
    st = {}
    st["instructions"] = len(op)
    st["vmem_loads"] = sum(1 for o in op if re.match(r"^(buffer_load|tbuffer_load|global_load|image_load|image_sample|image_gather)", o))
    st["samples"] = sum(1 for o in op if o.startswith("image_sample") or o.startswith("image_gather"))
    st["valu"] = sum(1 for o in op if o.startswith("v_"))
    st["salu"] = sum(1 for o in op if o.startswith("s_") and not o.startswith("s_wait") and not o.startswith("s_delay"))
    st["branches"] = sum(1 for o in op if o.startswith("s_cbranch") or o == "s_branch")
    # memory waits: gfx11 s_waitcnt vmcnt(N) / gfx12 s_wait_loadcnt N, s_wait_samplecnt N
    full = part = 0
    for i in ins:
        m = re.match(r"^s_waitcnt\s+.*vmcnt\((\d+)\)", i)
        if m:
            full += m.group(1) == "0"; part += m.group(1) != "0"
            continue
        m = re.match(r"^s_wait_(loadcnt|samplecnt|bvhcnt)\s+(0x[0-9a-fA-F]+|\d+)", i)
        if m:
            v = int(m.group(2), 0)
            full += v == 0; part += v != 0
    st["mem_waits_full"] = full
    st["mem_waits_partial"] = part
    m = re.search(r"vgpr_count\((\d+)\)", isa_text); st["vgprs"] = int(m.group(1)) if m else -1
    m = re.search(r"sgpr_count\((\d+)\)", isa_text); st["sgprs"] = int(m.group(1)) if m else -1
    m = re.search(r"wave_size\((\d+)\)", isa_text); st["wave"] = int(m.group(1)) if m else 64
    st["scratch"] = sum(1 for o in op if o.startswith("scratch_") or ("buffer_store" in o and "scratch" in o))
    return st


def run(name, blob, chips, outdir):
    tmp = tempfile.mkdtemp(prefix="rga_")
    src = os.path.join(tmp, "s.dxil")
    shutil.copy(blob, src)
    res = {}
    for c in chips:
        isa = os.path.join(tmp, "isa.txt")
        r = subprocess.run([RGA, "-s", "dx12", "--offline", "-c", c, "--cs-blob", src, "--isa", isa,
                            "-a", os.path.join(tmp, "a.txt")], capture_output=True, text=True)
        f = [x for x in os.listdir(tmp) if x.endswith(".isa") or ("isa" in x and x.endswith(".txt") and x != "a.txt")]
        f = [x for x in f if c in x] or f
        if not f:
            print(r.stdout[-2000:], r.stderr[-2000:]); raise SystemExit("rga failed for %s %s" % (name, c))
        text = open(os.path.join(tmp, f[0]), encoding="utf-8", errors="replace").read()
        if outdir:
            os.makedirs(outdir, exist_ok=True)
            open(os.path.join(outdir, "%s.%s.isa" % (name, c)), "w", encoding="utf-8").write(text)
        st = analyse(text)
        st["waves_per_simd"] = waves_per_simd(c, st["vgprs"], st["wave"])
        res[c] = st
        for x in os.listdir(tmp):
            if x != "s.dxil":
                os.remove(os.path.join(tmp, x))
    shutil.rmtree(tmp, ignore_errors=True)
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("items", nargs="+")
    ap.add_argument("--out", default=None)
    ap.add_argument("--chips", default="gfx1103,gfx1201")
    a = ap.parse_args()
    chips = a.chips.split(",")
    cols = ["vgprs", "waves_per_simd", "sgprs", "instructions", "valu", "vmem_loads", "samples", "branches",
            "mem_waits_full", "mem_waits_partial", "scratch"]
    print("| shader | chip | " + " | ".join(cols) + " |")
    print("|---|---|" + "---|" * len(cols))
    for it in a.items:
        name, path = it.split("=", 1)
        r = run(name, path, chips, a.out)
        for c in chips:
            print("| %s | %s | %s |" % (name, c, " | ".join(str(r[c][k]) for k in cols)))


if __name__ == "__main__":
    main()
