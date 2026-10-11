r"""run_gimerge_harness.py - GIMerge (gimerge.cpp) offline logic: MaskUnion brute force + hold/merge/replay on a fake list. Not a game test.

    py -I tests/run_gimerge_harness.py

Builds tests/gimerge_harness.cpp with the real src/gimerge.cpp (MSVC: cl on PATH or ..\..\tools\msvc\setup_x64.bat,
same flags as build.bat: /O2 /MT /W4 /permissive- /std:c++17) into build/gmtest and runs it. See gimerge_harness.cpp.
"""
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "build" / "gmtest"
OUT.mkdir(parents=True, exist_ok=True)
setup = ROOT.parent.parent / "tools" / "msvc" / "setup_x64.bat"
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
cmd = pre + (
    'cl /nologo /O2 /MT /GS /EHsc /std:c++17 /W4 /permissive- /DUNICODE /D_UNICODE "{t}" "{s}" '
    '/Fo"{o}\\\\" /Fe"{e}" /link kernel32.lib user32.lib'
).format(o=OUT, s=ROOT / "src" / "gimerge.cpp", e=OUT / "gimerge_harness.exe",
         t=ROOT / "tests" / "gimerge_harness.cpp")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
print(r.stdout[-6000:], r.stderr[-2000:])
if r.returncode:
    sys.exit("compile failed")
sys.exit(subprocess.run([str(OUT / "gimerge_harness.exe")]).returncode)
