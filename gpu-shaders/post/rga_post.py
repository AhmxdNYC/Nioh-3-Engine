r"""rga_post.py -- offline AMD driver compile (RGA, PAL DX12 path, gfx1103 = ROG Ally 780M) of shipped vs replacement
runtime-post pixel shaders: VGPRs, waves/SIMD, VMEM ops and memory waits (s_waitcnt vmcnt/loadcnt). The Ally runs
vkd3d-proton + ACO, so these numbers are a proxy (same ISA family, different compiler).
Usage: py -I rga_post.py [name ...]  -> out\rga_post.txt
"""
import os, re, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import build_post as B  # noqa: E402
RGA = os.path.join(B.ROOT if hasattr(B, "ROOT") else os.path.join(HERE, "..", ".."), "tools", "rdts",
                   "RadeonDeveloperToolSuite-2026-05-28-1806", "rga.exe")
VS = {"mbtile": "d8fc9faf", "mbgather": "d8fc9faf", "dofgather": "523efaab", "compose_ca": "d21b4b07",
      "compose_ca2": "d21b4b07"}
RS = '#define RS "DescriptorTable(CBV(b0, numDescriptors=14)), DescriptorTable(SRV(t0, numDescriptors=32)), ' \
     'DescriptorTable(Sampler(s0, numDescriptors=16))"\n'
GPSO = """# schemaVersion
1.0
# InputLayoutNumElements
0
# InputLayout
{ }
#PrimitiveTopologyType
D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE
# NumRenderTargets
1
# RTVFormats
{ DXGI_FORMAT_R16G16B16A16_FLOAT }
"""


def isa_stats(path, vs, chip="gfx1103"):
    tmp = tempfile.mkdtemp(prefix="rgapost_")
    open(os.path.join(tmp, "rs.hlsl"), "w").write(RS)
    open(os.path.join(tmp, "p.gpso"), "w").write(GPSO)
    r = subprocess.run([RGA, "-s", "dx12", "--offline", "-c", chip, "--vs-blob", vs, "--ps-blob", path,
                        "--rs-hlsl", os.path.join(tmp, "rs.hlsl"), "--rs-macro", "RS", "--gpso", os.path.join(tmp, "p.gpso"),
                        "--isa", os.path.join(tmp, "isa.txt"), "-a", os.path.join(tmp, "a.csv")],
                       capture_output=True, text=True, cwd=tmp)
    f = [x for x in os.listdir(tmp) if "pixel" in x and x.endswith(".txt") or ("_ps" in x and "isa" in x)]
    if not f:
        f = [x for x in os.listdir(tmp) if "isa" in x and ("ps" in x.lower() or "pixel" in x.lower())]
    if not f:
        raise SystemExit("rga failed:\n" + r.stdout[-1500:] + r.stderr[-800:] + str(os.listdir(tmp)))
    t = open(os.path.join(tmp, f[0]), encoding="utf-8", errors="replace").read()
    ins = [l.split("//")[0].strip() for l in t.splitlines()]
    ins = [i for i in ins if re.match(r"^[sv]_|^image_|^buffer_|^global_|^exp|^ds_", i)]
    m = re.search(r"vgpr_count\(?\s*[:=]?\s*(\d+)", t) or re.search(r"VGPRs?\D+(\d+)", t)
    vg = int(m.group(1)) if m else -1
    wave = 64 if re.search(r"wave_size\D+64|wave64", t) else 32
    vmem = sum(1 for i in ins if re.match(r"^(image_|buffer_load|global_load|tbuffer_load)", i))
    waits = sum(1 for i in ins if re.match(r"^s_waitcnt(_vmcnt| vmcnt|_loadcnt)", i) or (i.startswith("s_waitcnt") and "vmcnt" in i))
    return dict(vgpr=vg, wave=wave, ins=len(ins), vmem=vmem, vmwaits=waits)


def main():
    names = sys.argv[1:] or list(B.SHADERS)
    out = []
    for n in names:
        try:
            orig, new, outp, probs = B.build(n)
        except FileNotFoundError:
            continue
        vs = B.find_original(VS[n])
        a = isa_stats(B.find_original(B.SHADERS[n][0]), vs)
        b = isa_stats(outp, vs)
        line = "%-12s shipped: %s | replacement: %s" % (n, a, b)
        print(line)
        out.append(line)
    open(os.path.join(HERE, "out", "rga_post.txt"), "w").write("\n".join(out) + "\n")


if __name__ == "__main__":
    main()
