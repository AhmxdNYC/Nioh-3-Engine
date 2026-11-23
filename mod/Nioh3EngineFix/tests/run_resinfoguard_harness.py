r"""run_resinfoguard_harness.py - ResInfoGuard offline test (src/resinfoguard.cpp, GitHub #145). Not a game test.

    py -I tests/run_resinfoguard_harness.py [path/to/Nioh3.dump.exe]

Builds the harness and checks the v2.02 header-mismatch branch (2E0384) is retargeted onto the
reader's own false return (xor al, al at 1912378) only when the assert cold block is intact.
Must end ALL PASSED.
"""
import os
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
REPO = ROOT.parent.parent
DUMP = sys.argv[1] if len(sys.argv) > 1 else r"C:\nioh3-engine\re\Nioh3.dump.exe"
OUT = ROOT / "build" / "resinfoguardtest"
OUT.mkdir(parents=True, exist_ok=True)

setup = pathlib.Path(os.environ.get("N3_MSVC_SETUP", str(REPO / "tools" / "msvc" / "setup_x64.bat")))
if not setup.exists():
    setup = pathlib.Path(r"C:/nioh3-engine/tools/msvc/setup_x64.bat")
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
cmd = pre + (
    'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /permissive- /DUNICODE /D_UNICODE "{t}\\resinfoguard_harness.cpp" "{s}\\resinfoguard.cpp" '
    '/Fo"{o}\\\\" /Fe"{o}\\resinfoguard_harness.exe" /link kernel32.lib user32.lib'
).format(o=OUT, t=ROOT / "tests", s=ROOT / "src")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-4000:], r.stderr[-2000:])
    sys.exit("compile failed")
fails = []
if [ln for ln in r.stdout.splitlines() if "warning" in ln.lower()]:
    fails.append("compiler warnings:\n" + r.stdout)
r = subprocess.run([str(OUT / "resinfoguard_harness.exe"), DUMP], capture_output=True, text=True, timeout=60)
print(r.stdout[-4000:])
if r.stderr:
    print(r.stderr[-2000:])
if not (r.returncode == 0 and "ALL OK" in r.stdout):
    fails.append("harness failed (exit %s)" % r.returncode)
if fails:
    print("FAILED:")
    for item in fails:
        print("  - " + item)
    sys.exit(1)
print("ALL PASSED")
