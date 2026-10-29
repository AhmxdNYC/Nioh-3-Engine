"""run_cbf_bench.py - CBF copy-cost micro-benchmark (tests/cbf_bench.cpp): GPU time of system-memory -> VRAM copies of the
game's per-draw constant pattern on the DIRECT queue (CBF_Queue = 0) and a COPY queue (CBF_Queue = 1), one batched copy per
submission vs one 3 KB copy per CBV (the CbvMirror pattern). No game; run it with the game closed (it measures the GPU).

    py -I tests/run_cbf_bench.py [MB per frame (default 11)] [submissions per frame (default 16)]

Builds into build/cbfbench (MSVC: cl on PATH or ..\\..\\tools\\msvc\\setup_x64.bat) and runs cbf_bench.exe.
"""
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "build" / "cbfbench"
OUT.mkdir(parents=True, exist_ok=True)
REPO = ROOT.parent.parent
setup = REPO / "tools" / "msvc" / "setup_x64.bat"
if not setup.exists():  # worktrees: the portable MSVC lives in the main checkout only
    setup = pathlib.Path(r"C:\nioh3-engine\tools\msvc\setup_x64.bat")
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
cmd = pre + (
    'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 "{t}\\cbf_bench.cpp" '
    '/Fo"{o}\\\\" /Fe"{o}\\cbf_bench.exe" /link kernel32.lib d3d12.lib dxgi.lib'
).format(o=OUT, t=ROOT / "tests")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-6000:], r.stderr[-2000:])
    sys.exit("compile failed")
sys.exit(subprocess.run([str(OUT / "cbf_bench.exe")] + sys.argv[1:]).returncode)
