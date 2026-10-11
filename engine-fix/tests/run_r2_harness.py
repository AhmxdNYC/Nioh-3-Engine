"""run_r2_harness.py - gpuopt R2a/R2b/R1 rewrites on a real D3D12 device of this PC. Not a game test.

    py -I tests/run_r2_harness.py

Builds tests/r2_harness.cpp with the real src/gpuopt.cpp, src/gpuopt_thunks.asm and src/gpuprof.cpp (MSVC: cl on
PATH or ..\\..\\tools\\msvc\\setup_x64.bat) into build/r2test and runs it. See r2_harness.cpp for the checks.
"""
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "build" / "r2test"
OUT.mkdir(parents=True, exist_ok=True)
setup = ROOT.parent.parent / "tools" / "msvc" / "setup_x64.bat"
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
src = ROOT / "src"
cmd = pre + (
    'ml64 /nologo /c /W3 /Fo"{o}\\gpuopt_thunks.obj" "{s}\\gpuopt_thunks.asm" >nul && '
    'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /DUNICODE /D_UNICODE "{t}" "{s}\\gpuopt.cpp" "{s}\gimerge.cpp" "{s}\\gpuprof.cpp" "{s}\psoswap.cpp" '
    '"{s}\\skipfx.cpp" "{s}\\mlprobe.cpp" "{s}\\mpccheck.cpp" '
    '"{o}\\gpuopt_thunks.obj" /Fo"{o}\\\\" /Fe"{o}\\r2_harness.exe" /link d3d12.lib d3dcompiler.lib dxgi.lib kernel32.lib user32.lib'
).format(o=OUT, s=src, t=ROOT / "tests" / "r2_harness.cpp")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-4000:], r.stderr[-2000:])
    sys.exit("compile failed")
rc = 0
for mode in ("1",):
    rc |= subprocess.run([str(OUT / "r2_harness.exe"), mode]).returncode
sys.exit(rc)
