"""run_gp_harness.py - GP (GPU pass profiler) on a real D3D12 device of this PC. Not a game test.

    py -I tests/run_gp_harness.py

Builds tests/gp_harness.cpp with the real src/gpuopt.cpp, src/gpuopt_thunks.asm and src/gpuprof.cpp (MSVC: cl on
PATH or ..\\..\\tools\\msvc\\setup_x64.bat) into build/gptest and runs it. See gp_harness.cpp for the checks.
"""
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "build" / "gptest"
OUT.mkdir(parents=True, exist_ok=True)
setup = ROOT.parent.parent / "tools" / "msvc" / "setup_x64.bat"
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
src = ROOT / "src"
cmd = pre + (
    'ml64 /nologo /c /W3 /Fo"{o}\\gpuopt_thunks.obj" "{s}\\gpuopt_thunks.asm" >nul && '
    'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /DUNICODE /D_UNICODE "{t}" "{s}\\gpuopt.cpp" "{s}\gimerge.cpp" "{s}\\gpuprof.cpp" "{s}\psoswap.cpp" '
    '"{s}\\skipfx.cpp" "{s}\\mlprobe.cpp" "{s}\\mpccheck.cpp" '
    '"{o}\\gpuopt_thunks.obj" /Fo"{o}\\\\" /Fe"{o}\\gp_harness.exe" /link d3d12.lib d3dcompiler.lib dxgi.lib kernel32.lib user32.lib'
).format(o=OUT, s=src, t=ROOT / "tests" / "gp_harness.cpp")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-4000:], r.stderr[-2000:])
    sys.exit("compile failed")
rc = 0
for mode in ("1", "2", "3", "4", "5"):   # 1 = marker profiler, 2 = GP2 per-PSO attribution, 3 = GP2 sampled + coarse, 4 = GQ gap census, 5 = MLP + MpcCheck
    rc |= subprocess.run([str(OUT / "gp_harness.exe"), mode]).returncode
sys.exit(rc)
