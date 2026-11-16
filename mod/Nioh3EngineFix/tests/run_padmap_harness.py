r"""run_padmap_harness.py - PadListGuard map-miss cave offline test (src/padguard_cave.h, GitHub #96). Not a game test.

    py -I tests/run_padmap_harness.py [path/to/Nioh3.dump.exe]

Builds tests/padmap_harness.cpp (MSVC) and runs it: dump checks of the at() miss path and its 9 pad-walk calls, then the
map-miss cave from BuildMapMiss: a listed caller gets a zeroed value, any other caller reaches the original throw.
Must end ALL PASSED.
"""
import os
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
REPO = ROOT.parent.parent
DUMP = sys.argv[1] if len(sys.argv) > 1 else r"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
OUT = ROOT / "build" / "padmaptest"
OUT.mkdir(parents=True, exist_ok=True)

setup = pathlib.Path(os.environ.get("N3_MSVC_SETUP", str(REPO / "tools" / "msvc" / "setup_x64.bat")))
if not setup.exists():
    setup = pathlib.Path(r"C:/Users/Ahmad Hamza/nioh3-engine/tools/msvc/setup_x64.bat")
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
cmd = pre + (
    'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /permissive- /DUNICODE /D_UNICODE "{t}\\padmap_harness.cpp" '
    '/Fo"{o}\\\\" /Fe"{o}\\padmap_harness.exe" /link kernel32.lib user32.lib'
).format(o=OUT, t=ROOT / "tests")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-4000:], r.stderr[-2000:])
    sys.exit("compile failed")
fails = []
if [l for l in r.stdout.splitlines() if "warning" in l]:
    fails.append("compiler warnings:\n" + r.stdout)
r = subprocess.run([str(OUT / "padmap_harness.exe"), DUMP], capture_output=True, text=True, timeout=120)
print(r.stdout[-8000:])
if not (r.returncode == 0 and "ALL OK" in r.stdout):
    fails.append("harness failed (exit %s)" % r.returncode)
if fails:
    print("FAILED:")
    for f in fails:
        print("  - " + f)
    sys.exit(1)
print("ALL PASSED")
