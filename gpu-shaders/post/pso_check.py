r"""pso_check.py -- build pso_check.cpp (MSVC from tools\msvc) and create, on this PC's D3D12 device, a graphics PSO
from the game's VS + shipped PS and VS + replacement PS for every runtime-post replacement. Must end "ALL PSOS OK".
Usage: py -I pso_check.py
"""
import os, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import build_post as B  # noqa: E402
import rga_post as R  # noqa: E402

ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
BUILD = os.path.join(HERE, "out", "psocheck")


def main():
    os.makedirs(BUILD, exist_ok=True)
    exe = os.path.join(BUILD, "pso_check.exe")
    setup = os.path.join(ROOT, "tools", "msvc", "setup_x64.bat")
    cmd = ('call "%s" >nul && cl /nologo /O2 /EHsc /W4 "%s" /Fo"%s\\\\" /Fe"%s" /link d3d12.lib' %
           (setup, os.path.join(HERE, "pso_check.cpp"), BUILD, exe))
    r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
    if r.returncode:
        print(r.stdout[-3000:], r.stderr[-1000:])
        raise SystemExit("compile failed")
    ok = True
    for n, (dig, _) in B.SHADERS.items():
        out = os.path.join(B.OUT, open(B.find_original(dig), "rb").read()[4:20].hex() + ".dxbc")
        p = subprocess.run([exe, B.find_original(R.VS[n]), B.find_original(dig), out], capture_output=True, text=True)
        print(n + ":\n" + p.stdout.rstrip())
        ok &= p.returncode == 0
    print("ALL PSOS OK" if ok else "PSO CREATION FAILED")


if __name__ == "__main__":
    main()
