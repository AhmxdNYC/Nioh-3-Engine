"""run_flp_load_harness.py - FLP file-loader priority vs level load (src/thrsched.cpp, #103349/#103350). Not a game test.
    py -I tests/run_flp_load_harness.py
Builds tests/flp_load_harness.cpp + src/thrsched.cpp and runs it (two idle named loader threads, a fake G+58Ah load
byte, affinity 2 CPUs): loaders are lowered only in the field and restored within 600 ms when a load starts, never
lowered while the flag is unreadable. Light load (no spinners), safe while the game runs. Must end ALL PASSED.
"""
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
REPO = ROOT.parent.parent
OUT = ROOT / "build" / "flptest"
OUT.mkdir(parents=True, exist_ok=True)
setup = REPO / "tools" / "msvc" / "setup_x64.bat"
if not setup.exists():  # git worktree: use the main checkout's portable MSVC
    setup = pathlib.Path(r"C:\nioh3-engine\tools\msvc\setup_x64.bat")
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
cmd = pre + (
    'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /permissive- /DUNICODE /D_UNICODE "{t}\\flp_load_harness.cpp" "{s}\\thrsched.cpp" '
    '/Fo"{o}\\\\" /Fe"{o}\\flp_load_harness.exe" /link kernel32.lib'
).format(o=OUT, t=ROOT / "tests", s=ROOT / "src")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-4000:], r.stderr[-2000:])
    sys.exit("compile failed")
warn = [l for l in r.stdout.splitlines() if "warning" in l]
r = subprocess.run([str(OUT / "flp_load_harness.exe")], capture_output=True, text=True, timeout=120)
print(r.stdout.strip())
ok = r.returncode == 0 and "ALL PASSED" in r.stdout and not warn
if warn:
    print("compiler warnings:", warn[:3])
print("ALL PASSED" if ok else "FAILED")
sys.exit(0 if ok else 1)
