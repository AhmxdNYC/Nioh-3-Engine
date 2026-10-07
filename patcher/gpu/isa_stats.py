"""isa_stats.py - disassemble dumped PSO code objects with rga and summarise the ISA.

    py isa_stats.py <shaders_dir>          (dir written by rgp_report.py --dump-shaders)

Writes <shaders_dir>/isa/<pso>.txt and <shaders_dir>/isa_stats.csv. rga cannot open paths with
spaces, so each ELF is staged in a space-free temp dir first.
"""
import csv, glob, os, re, shutil, subprocess, sys, tempfile
from collections import Counter

RGA = r"C:\Users\Ahmad Hamza\nioh3-engine\tools\rdts\RadeonDeveloperToolSuite-2026-05-28-1806\rga.exe"
KEYS = {
    "sample": r"^image_sample|^image_gather",
    "img_load": r"^image_load|^image_store|^image_atomic",
    "buf_mem": r"^buffer_|^global_|^flat_|^scratch_",
    "bvh_rt": r"^image_bvh|bvh",
    "wmma": r"^v_wmma|^v_swmmac",
    "lds": r"^ds_",
    "export": r"^export|^exp ",
    "trans": r"^v_(exp|log|rcp|rsq|sqrt|sin|cos)_",
    "sendmsg": r"^s_sendmsg",
    "branch": r"^s_cbranch|^s_branch",
}


def stage_names(txt):
    return sorted(set(re.findall(r"^(_amdgpu_\w+?_main\w*):", txt, re.M)))


def main(d):
    out_isa = os.path.join(d, "isa"); os.makedirs(out_isa, exist_ok=True)
    tmp = tempfile.mkdtemp(prefix="rgaisa_")
    rows = []
    for elf in sorted(glob.glob(os.path.join(d, "*.elf"))):
        pso = os.path.basename(elf).split("_")[1]
        dst = os.path.join(out_isa, pso + ".txt")
        if not os.path.exists(dst):
            src = os.path.join(tmp, "x.elf"); shutil.copy(elf, src)
            for f in glob.glob(os.path.join(tmp, "*isa*")):
                os.remove(f)
            subprocess.run([RGA, "-s", "bin", "--isa", os.path.join(tmp, "isa.txt"), src],
                           capture_output=True, text=True)
            parts = sorted(glob.glob(os.path.join(tmp, "*isa*.txt")))
            with open(dst, "w", encoding="utf-8") as o:
                for p in parts:
                    o.write(f"//// {os.path.basename(p)}\n" + open(p, encoding="utf-8", errors="replace").read())
        txt = open(dst, encoding="utf-8", errors="replace").read()
        ops = [l.strip().split()[0] for l in txt.splitlines()
               if l.startswith("\t") or (l.startswith("  ") and l.strip() and not l.strip().startswith("//"))]
        c = Counter()
        for op in ops:
            for k, rx in KEYS.items():
                if re.search(rx, op):
                    c[k] += 1
        rows.append(dict(pso=pso, stages=" ".join(stage_names(txt)), n_inst=len(ops), **{k: c[k] for k in KEYS}))
    with open(os.path.join(d, "isa_stats.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=["pso", "stages", "n_inst", *KEYS]); w.writeheader(); w.writerows(rows)
    shutil.rmtree(tmp, ignore_errors=True)
    print(f"{len(rows)} PSOs -> {os.path.join(d, 'isa_stats.csv')}")


if __name__ == "__main__":
    main(sys.argv[1])
