r"""isa1.py <pso16> [...] -- disassemble single PSO ELFs from the top300 dump with rga into re/scripts_gpu2/out/isa/<pso>.txt"""
import glob, os, shutil, subprocess, sys, tempfile
RGA = r"C:\Users\Ahmad Hamza\nioh3-engine\tools\rdts\RadeonDeveloperToolSuite-2026-05-28-1806\rga.exe"
D = [r"C:\Users\Ahmad Hamza\nioh3-engine\re\scripts_gpu\out\rgp_top300\Nioh3_2026-10-06_19-39-58_shaders",
     r"C:\Users\Ahmad Hamza\nioh3-engine\captures\rgp\Nioh3_2026-10-06_19-39-58_shaders"]
OUT = r"C:\Users\Ahmad Hamza\nioh3-engine\re\scripts_gpu2\out\isa"; os.makedirs(OUT, exist_ok=True)
tmp = tempfile.mkdtemp(prefix="rgaisa_")
for pso in sys.argv[1:]:
    elf = next((e for d in D for e in glob.glob(os.path.join(d, "pso_%s_*.elf" % pso))), None)
    if not elf: print(pso, "no elf"); continue
    src = os.path.join(tmp, "x.elf"); shutil.copy(elf, src)
    for f in glob.glob(os.path.join(tmp, "*isa*")): os.remove(f)
    subprocess.run([RGA, "-s", "bin", "--isa", os.path.join(tmp, "isa.txt"), src], capture_output=True, text=True)
    dst = os.path.join(OUT, pso + ".txt")
    with open(dst, "w", encoding="utf-8") as o:
        for p in sorted(glob.glob(os.path.join(tmp, "*isa*.txt"))):
            o.write("//// %s\n" % os.path.basename(p) + open(p, encoding="utf-8", errors="replace").read())
    print(pso, "->", dst)
shutil.rmtree(tmp, ignore_errors=True)
