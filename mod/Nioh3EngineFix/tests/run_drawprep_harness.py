"""run_drawprep_harness.py - DQ / P1E7 caves (src/drawprep.asm) against the original v2.02 bytes on this PC. Not a game test.

    py -I tests/run_drawprep_harness.py

Assembles the real src/drawprep.asm and tests/drawprep_harness.asm, builds tests/drawprep_harness.cpp (MSVC: cl on PATH or
..\\..\\tools\\msvc\\setup_x64.bat) into build/dptest and runs it. Must end ALL PASSED. See drawprep_harness.cpp.
"""
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "build" / "dptest"
OUT.mkdir(parents=True, exist_ok=True)
setup = ROOT.parent.parent / "tools" / "msvc" / "setup_x64.bat"
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
cmd = pre + (
    'ml64 /nologo /c /W3 /Fo"{o}\\drawprep_caves.obj" "{s}\\drawprep.asm" >nul && '
    'ml64 /nologo /c /W3 /Fo"{o}\\drawprep_harness_asm.obj" "{t}\\drawprep_harness.asm" >nul && '
    'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 "{t}\\drawprep_harness.cpp" "{o}\\drawprep_caves.obj" "{o}\\drawprep_harness_asm.obj" '
    '/Fo"{o}\\\\" /Fe"{o}\\drawprep_harness.exe" /link kernel32.lib'
).format(o=OUT, s=ROOT / "src", t=ROOT / "tests")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-4000:], r.stderr[-2000:])
    sys.exit("compile failed")
sys.exit(subprocess.run([str(OUT / "drawprep_harness.exe")]).returncode)
