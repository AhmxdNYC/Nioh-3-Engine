r"""run_aichainguard_harness.py - AiChainGuard offline test (src/aichainguard.cpp, GitHub #95 / #30). Not a game test.

    py -I tests/run_aichainguard_harness.py [path/to/Nioh3.dump.exe]

Builds tests/aichainguard_harness.cpp + src/aichainguard.cpp (MSVC) and runs it: the game's own vector<ptr>::assign bytes from
the dump (310698..31075A) in a fake image, source list ending at a PAGE_NOACCESS page (#95: 190 rows, 114 readable).
Stock faults at +31074A (child process); guard installs only on matching bytes and ini != 0; with the guard the copy ends
after 114 rows with the rows intact; intact lists and other faults are untouched. Must end ALL PASSED.
"""
import os
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
REPO = ROOT.parent.parent
DUMP = sys.argv[1] if len(sys.argv) > 1 else r"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
OUT = ROOT / "build" / "aichainguardtest"
OUT.mkdir(parents=True, exist_ok=True)

setup = pathlib.Path(os.environ.get("N3_MSVC_SETUP", str(REPO / "tools" / "msvc" / "setup_x64.bat")))
if not setup.exists():
    setup = pathlib.Path(r"C:/Users/Ahmad Hamza/nioh3-engine/tools/msvc/setup_x64.bat")
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
cmd = pre + (
    'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /permissive- /DUNICODE /D_UNICODE "{t}\\aichainguard_harness.cpp" "{s}\\aichainguard.cpp" '
    '/Fo"{o}\\\\" /Fe"{o}\\aichainguard_harness.exe" /link kernel32.lib user32.lib'
).format(o=OUT, t=ROOT / "tests", s=ROOT / "src")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-4000:], r.stderr[-2000:])
    sys.exit("compile failed")
fails = []
if [l for l in r.stdout.splitlines() if "warning" in l]:
    fails.append("compiler warnings:\n" + r.stdout)
r = subprocess.run([str(OUT / "aichainguard_harness.exe"), DUMP], capture_output=True, text=True, timeout=120)
print(r.stdout[-8000:])
if not (r.returncode == 0 and "ALL OK" in r.stdout):
    fails.append("harness failed (exit %s)" % r.returncode)
if fails:
    print("FAILED:")
    for f in fails:
        print("  - " + f)
    sys.exit(1)
print("ALL PASSED")
