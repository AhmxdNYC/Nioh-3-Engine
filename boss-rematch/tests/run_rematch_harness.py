r"""run_rematch_harness.py - offline tests of Nioh3BossRematch (no game).

    py -I tests/run_rematch_harness.py

1. Builds tests/rematch_harness.cpp (rematch_core.h + the real hooks.cpp with stubbed sites) with MSVC /W4 and runs it on the real
   catalogue: catalogue / verified merge, E1 map redirect + CRC restore, E lifecycle (retry, exit, timeout), entry thunk with two
   handlers, chaining onto another module's hook, the pump retarget.
2. Wiring: build.bat compiles every src/*.cpp; the ini ships Enable = 0, DirectStart = 0, ShowUnverified = 0, Pipe = 0 and the
   [Debug] loggers at 0; the catalogue is in sync with tools/rematch_catalog.py; sites_gen.h in sync with tools/derive_sites.py.
"""
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
REPO = ROOT.parent.parent
OUT = ROOT / "build" / "harness"
fails = []


def check(c, m):
    if not c:
        fails.append(m)


OUT.mkdir(parents=True, exist_ok=True)
setup = None
for c in (REPO / "tools" / "msvc" / "setup_x64.bat", pathlib.Path(r"tools\msvc\setup_x64.bat")):
    if c.exists():
        setup = c
        break
cmd = ('call "{s}" >nul && cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /permissive- /DUNICODE /D_UNICODE "{t}\\rematch_harness.cpp" '
       '/Fo"{o}\\\\" /Fe"{o}\\rematch_harness.exe" /link kernel32.lib user32.lib').format(s=setup, t=ROOT / "tests", o=OUT)
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
warn = [l for l in r.stdout.splitlines() if "warning" in l]
check(r.returncode == 0, "harness build failed:\n" + r.stdout[-3000:])
check(not warn, "warnings:\n" + "\n".join(warn[:20]))
if r.returncode == 0:
    r = subprocess.run([str(OUT / "rematch_harness.exe"), str(ROOT / "Nioh3BossRematch_bosses.tsv")], capture_output=True, text=True, timeout=120)
    print(r.stdout.strip())
    check(r.returncode == 0 and "ALL PASSED" in r.stdout, "harness failed")
bb = (ROOT / "build.bat").read_text(encoding="utf-8")
for f in (ROOT / "src").glob("*.cpp"):
    check(f.stem in bb, "build.bat does not build %s" % f.name)
ini = (ROOT / "Nioh3BossRematch.ini").read_text(encoding="utf-8")
for k in ("Enable = 0", "DirectStart = 0", "ShowUnverified = 0", "Pipe = 0", "BossLog = 0", "RematchLog = 0", "InvLog = 0", "HiddenDuels = 0", "FightNow = 0", "FightNowWarp = 0", "ArenaCapture = 0"):
    check(re.search(r"(?m)^" + re.escape(k) + r"\s*$", ini) is not None, "ini default: " + k)
r = subprocess.run([sys.executable, "-I", str(REPO / "tools" / "rematch_catalog.py"), "--check"], capture_output=True, text=True)
check(r.returncode == 0, "catalogue out of date: " + r.stdout.strip())
r = subprocess.run([sys.executable, "-I", str(ROOT / "tools" / "derive_sites.py"), "--check"], capture_output=True, text=True)
check(r.returncode == 0, "sites out of date: " + r.stdout.strip())
if fails:
    print("FAILED:")
    for f in fails:
        print("  " + f)
    sys.exit(1)
print("run_rematch_harness: ALL PASSED")
