"""run_sbm_harness.py - SBM scene-buffer mirror (src/scenemirror.cpp) on a real D3D12 device of this PC. Not a game test.

    py -I tests/run_sbm_harness.py

Builds tests/sbm_harness.cpp with the real src/scenemirror.cpp (MSVC: cl on PATH or ..\\..\\tools\\msvc\\setup_x64.bat)
into build/sbmtest and runs it three times: own COPY queue, game DIRECT queue, and the late-write check.
"""
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "build" / "sbmtest"
OUT.mkdir(parents=True, exist_ok=True)
setup = ROOT.parent.parent / "tools" / "msvc" / "setup_x64.bat"
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
src = ROOT / "src"
cmd = pre + (
    'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /DUNICODE /D_UNICODE "{t}" "{s}\\scenemirror.cpp" '
    '/Fo"{o}\\\\" /Fe"{o}\\sbm_harness.exe" /link d3d12.lib dxgi.lib kernel32.lib user32.lib'
).format(o=OUT, s=src, t=ROOT / "tests" / "sbm_harness.cpp")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-4000:], r.stderr[-2000:])
    sys.exit("compile failed")
rc = 0
for args in (["1"], ["0"], ["1", "late"], ["1", "bw"], ["0", "bw"]):
    rc |= subprocess.run([str(OUT / "sbm_harness.exe")] + args).returncode
sys.exit(rc)
