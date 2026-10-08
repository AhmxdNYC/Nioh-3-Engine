r"""run_loadmem_harness.py - LM (loadmem.cpp) sweep logic on a synthetic heap. Not a game test.

    py -I tests/run_loadmem_harness.py

Builds tests/loadmem_harness.cpp with the real src/loadmem.cpp (MSVC: cl on PATH or ..\..\tools\msvc\setup_x64.bat,
same flags as build.bat: /O2 /MT /W4 /permissive- /std:c++17) into build/lmtest and runs it. See loadmem_harness.cpp.
"""
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "build" / "lmtest"
OUT.mkdir(parents=True, exist_ok=True)
setup = ROOT.parent.parent / "tools" / "msvc" / "setup_x64.bat"
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
cmd = pre + (
    'cl /nologo /O2 /MT /GS /EHsc /std:c++17 /W4 /permissive- /DUNICODE /D_UNICODE "{t}" "{s}" '
    '/Fo"{o}\\\\" /Fe"{e}" /link kernel32.lib user32.lib'
).format(o=OUT, s=ROOT / "src" / "loadmem.cpp", e=OUT / "loadmem_harness.exe",
         t=ROOT / "tests" / "loadmem_harness.cpp")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
print(r.stdout[-6000:], r.stderr[-2000:])
if r.returncode:
    sys.exit("compile failed")
sys.exit(subprocess.run([str(OUT / "loadmem_harness.exe")]).returncode)
