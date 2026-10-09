r"""run_rematch_harness.py - boss rematch offline tests (src/rematch.cpp, ASI 1.7.40). Not a game test.

    py -I tests/run_rematch_harness.py

1. Builds tests/rematch_harness.cpp (includes src/rematch.cpp) with MSVC /W4 into build/rematchtest and runs it on a fake image:
   thunk chaining onto an earlier (bosslog-like) hook of the same entry (order, args, result, only the rel32 rewritten), probe
   refusals, catalogue parsing (+ the real Nioh3EngineFix_bosses.tsv), every s.2 gate, the cleared-mission scan and the sub-page
   rows, R-a with (0, 0, 6), HiddenDuels un-hide + restore on menu close / mission exit / abort / timeout, refusals (fight, busy
   menu, R-b not built).
2. Wiring: dllmain starts rematch::Start after bosslog::Start, build.bat / CMakeLists build rematch.cpp, build.bat copies the
   catalogue, the ini documents [Rematch] Enable = 0 / DirectStart = 0, the catalogue is in sync with tools/rematch_catalog.py.
Must end ALL PASSED.
"""
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
REPO = ROOT.parent.parent
OUT = ROOT / "build" / "rematchtest"
OUT.mkdir(parents=True, exist_ok=True)
fails = []


def check(c, msg):
    if not c:
        fails.append(msg)
    return c


setup = REPO / "tools" / "msvc" / "setup_x64.bat"
if not setup.exists():
    setup = pathlib.Path(r"C:\Users\Ahmad Hamza\nioh3-engine\tools\msvc\setup_x64.bat")
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
cmd = pre + (
    'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /permissive- /DUNICODE /D_UNICODE "{t}\\rematch_harness.cpp" '
    '/Fo"{o}\\\\" /Fe"{o}\\rematch_harness.exe" /link kernel32.lib user32.lib'
).format(o=OUT, t=ROOT / "tests")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-4000:], r.stderr[-2000:])
    sys.exit("compile failed")
warn = [l for l in r.stdout.splitlines() if "warning" in l]
check(not warn, "compiler warnings: %s" % warn[:3])
r = subprocess.run([str(OUT / "rematch_harness.exe"), str(ROOT / "Nioh3EngineFix_bosses.tsv")], capture_output=True, text=True, timeout=300)
out = r.stdout.strip()
print("\n".join(l for l in out.splitlines() if not l.startswith("  log:"))[-6000:])
check(r.returncode == 0 and "ALL PASSED" in out, "harness failed")

dm = (ROOT / "src" / "dllmain.cpp").read_text(encoding="utf-8")
ib, ir = dm.find("bosslog::Start(bi)"), dm.find("rematch::Start(ri)")
check(ib > 0 and ir > ib, "dllmain: rematch::Start must come after bosslog::Start")
check("ri.pauseReason = &menu::GamePauseReason" in dm, "dllmain: pauseReason wired")
bb = (ROOT / "build.bat").read_text(encoding="utf-8")
check("src\\rematch.cpp" in bb and "build\\rematch.obj" in bb and "Nioh3EngineFix_bosses.tsv" in bb, "build.bat: rematch.cpp / obj / catalogue copy")
check("src/rematch.cpp" in (ROOT / "CMakeLists.txt").read_text(encoding="utf-8"), "CMakeLists: rematch.cpp")
ini = (ROOT / "Nioh3EngineFix.ini").read_text(encoding="utf-8")
sec = ini[ini.find("[Rematch]"):]
check("[Rematch]" in ini and re.search(r"(?m)^Enable = 0", sec) and re.search(r"(?m)^DirectStart = 0", sec)
      and re.search(r"(?m)^HiddenDuels = 0", sec), "ini: [Rematch] Enable / HiddenDuels / DirectStart = 0")
r = subprocess.run([sys.executable, "-I", str(REPO / "tools" / "rematch_catalog.py"), "--check"], capture_output=True, text=True)
print(r.stdout.strip())
check(r.returncode == 0, "catalogue out of date (py -I tools/rematch_catalog.py)")

print()
if fails:
    print("FAILED (%d):" % len(fails))
    for f in fails:
        print("  - " + f)
    sys.exit(1)
print("ALL PASSED")
