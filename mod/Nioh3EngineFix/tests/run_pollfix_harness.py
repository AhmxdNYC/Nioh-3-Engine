"""run_pollfix_harness.py - PollFix (src/thrsched.cpp, #108505). Not a game test.
    py -I tests/run_pollfix_harness.py
Builds tests/pollfix_harness.cpp + src/thrsched.cpp and runs it: a fake image with the real v2.02 FileLoad.Poll / FileLoad
bytes (tests/fpw_fixture.h); install, thunk = native unlock, unwind, back-off only on idle looping passes. Must end ALL PASSED.
"""
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
REPO = ROOT.parent.parent
OUT = ROOT / "build" / "pollfixtest"
OUT.mkdir(parents=True, exist_ok=True)
setup = REPO / "tools" / "msvc" / "setup_x64.bat"
if not setup.exists():  # git worktree: use the main checkout's portable MSVC
    setup = pathlib.Path(r"C:\Users\Ahmad Hamza\nioh3-engine\tools\msvc\setup_x64.bat")
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
cmd = pre + (
    'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /permissive- /DUNICODE /D_UNICODE "{t}\\pollfix_harness.cpp" "{s}\\thrsched.cpp" '
    '/Fo"{o}\\\\" /Fe"{o}\\pollfix_harness.exe" /link kernel32.lib'
).format(o=OUT, t=ROOT / "tests", s=ROOT / "src")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-4000:], r.stderr[-2000:])
    sys.exit("compile failed")
warn = [l for l in r.stdout.splitlines() if "warning" in l]
r = subprocess.run([str(OUT / "pollfix_harness.exe")], capture_output=True, text=True, timeout=120)
print(r.stdout.strip())
r2 = subprocess.run([str(OUT / "pollfix_harness.exe"), "off"], capture_output=True, text=True, timeout=120)
print(r2.stdout.strip())
ok = r.returncode == 0 and "ALL PASSED" in r.stdout and r2.returncode == 0 and "ALL PASSED" in r2.stdout and not warn
if warn:
    print("compiler warnings:", warn[:3])
print("ALL PASSED" if ok else "FAILED")
sys.exit(0 if ok else 1)
