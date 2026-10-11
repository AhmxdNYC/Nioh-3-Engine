r"""run_skipfx_harness.py - SkipFx (skipfx.cpp, [Debug] SkipFx diagnostic) offline logic: PSO / marker matching, live reload,
ack file, ClearMode on a fake list with a fake psoswap map. Not a game test.

    py -I tests/run_skipfx_harness.py

Builds tests/skipfx_harness.cpp with the real src/skipfx.cpp (MSVC: cl on PATH or ..\..\tools\msvc\setup_x64.bat, same flags as
build.bat) into build/sfxtest and runs it.
"""
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "build" / "sfxtest"
OUT.mkdir(parents=True, exist_ok=True)
setup = ROOT.parent.parent / "tools" / "msvc" / "setup_x64.bat"
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
cmd = pre + (
    'cl /nologo /O2 /MT /GS /EHsc /std:c++17 /W4 /permissive- /DUNICODE /D_UNICODE "{t}" "{s}" '
    '/Fo"{o}\\\\" /Fe"{e}" /link kernel32.lib user32.lib'
).format(o=OUT, s=ROOT / "src" / "skipfx.cpp", e=OUT / "skipfx_harness.exe",
         t=ROOT / "tests" / "skipfx_harness.cpp")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
print(r.stdout[-6000:], r.stderr[-2000:])
if r.returncode:
    sys.exit("compile failed")
sys.exit(subprocess.run([str(OUT / "skipfx_harness.exe")]).returncode)
