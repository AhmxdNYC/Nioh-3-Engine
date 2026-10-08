r"""run_prefetch_harness.py - PF trace prefetch (prefetch.cpp) through readprobe.cpp's read hooks. Not a game test.

    py -I tests/run_prefetch_harness.py      -> "PREFETCH HARNESS: ALL OK"

Builds tests/prefetch_harness.cpp (it #includes src/readprobe.cpp + src/prefetch.cpp; MSVC: cl on PATH or ..\..\tools\msvc\setup_x64.bat,
same flags as build.bat) into build/pftest and runs it. Site bytes against the real exe: py -I re\scripts_load\ls_sites.py.
"""
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "build" / "pftest"
OUT.mkdir(parents=True, exist_ok=True)
setup = ROOT.parent.parent / "tools" / "msvc" / "setup_x64.bat"
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
cmd = pre + (
    'cl /nologo /O2 /MT /GS /EHsc /std:c++17 /W4 /permissive- /DUNICODE /D_UNICODE "{t}" '
    '/Fo"{o}\\\\" /Fe"{e}" /link kernel32.lib'
).format(o=OUT, e=OUT / "prefetch_harness.exe", t=ROOT / "tests" / "prefetch_harness.cpp")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
print(r.stdout[-6000:], r.stderr[-2000:])
if r.returncode:
    sys.exit("compile failed")
sys.exit(subprocess.run([str(OUT / "prefetch_harness.exe")]).returncode)
