"""run_jt_harness.py - JT (job-timing probe) without the game. Not a game test.

    py -I tests/run_jt_harness.py

Builds tests/jt_harness.cpp + tests/jt_harness.asm with the real src/jt.cpp and src/jt.asm (MSVC: cl on PATH or
..\\..\\tools\\msvc\\setup_x64.bat) into build/jttest and runs it. See jt_harness.cpp for the checks.
"""
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "build" / "jttest"
OUT.mkdir(parents=True, exist_ok=True)
setup = ROOT.parent.parent / "tools" / "msvc" / "setup_x64.bat"
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
src = ROOT / "src"
tst = ROOT / "tests"
cmd = pre + (
    'ml64 /nologo /c /W3 /Fo"{o}\\jt_caves.obj" "{s}\\jt.asm" >nul && '
    'ml64 /nologo /c /W3 /Fo"{o}\\jt_harness_asm.obj" "{t}\\jt_harness.asm" >nul && '
    'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /DUNICODE /D_UNICODE "{t}\\jt_harness.cpp" "{s}\\jt.cpp" '
    '"{o}\\jt_caves.obj" "{o}\\jt_harness_asm.obj" /Fo"{o}\\\\" /Fe"{o}\\jt_harness.exe" /link kernel32.lib'
).format(o=OUT, s=src, t=tst)
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-4000:], r.stderr[-2000:])
    sys.exit("compile failed")
sys.exit(subprocess.run([str(OUT / "jt_harness.exe")]).returncode)
