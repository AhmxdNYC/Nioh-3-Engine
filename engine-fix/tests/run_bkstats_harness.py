"""run_bkstats_harness.py - BKS back-end census + KL kick limit offline test (src/bkstats.cpp). Not a game test.

    py -I tests/run_bkstats_harness.py

1. Builds tests/bkstats_harness.cpp + src/bkstats.cpp into build/bktest and runs it against a fake v2.02 image (the three
   layout-check byte runs) and a fake GfxDevice driven by a fake engine thread (25 ms frames, 19 ms recording with the
   dispatcher starved half of it, 6 ms drain, known per-frame engine timers). The report lines must give back the known
   values; KL must write stock/n, clamp at 16, re-assert after an engine rewrite, restore the engine value on 0 and refuse
   out-of-range values.
2. Source wiring: build.bat / CMakeLists build bkstats.cpp, dllmain starts it and routes Tuning keys to it, the ini documents
   Backend and KL_Div (default 0), live_keys.py lists Tuning.KL_Div as live-tuning.
Must end ALL PASSED.
"""
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
REPO = ROOT.parent.parent
OUT = ROOT / "build" / "bktest"
OUT.mkdir(parents=True, exist_ok=True)
fails = []


def check(c, msg):
    if not c:
        fails.append(msg)
    return c


setup = REPO / "tools" / "msvc" / "setup_x64.bat"
if not setup.exists():  # git worktree: tools\msvc is not tracked, use the main checkout's portable MSVC
    setup = pathlib.Path(r"tools\msvc\setup_x64.bat")
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
cmd = pre + (
    'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /permissive- /DUNICODE /D_UNICODE "{t}\\bkstats_harness.cpp" '
    '"{s}\\bkstats.cpp" /Fo"{o}\\\\" /Fe"{o}\\bkstats_harness.exe" /link kernel32.lib'
).format(o=OUT, t=ROOT / "tests", s=ROOT / "src")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-4000:], r.stderr[-2000:])
    sys.exit("compile failed")
warn = [l for l in r.stdout.splitlines() if "warning" in l]
check(not warn, "compiler warnings: %s" % warn[:3])
r = subprocess.run([str(OUT / "bkstats_harness.exe")], capture_output=True, text=True, timeout=120)
print("\n".join(l for l in r.stdout.splitlines() if not l.startswith("  log:") or "BKS: " in l))
check(r.returncode == 0 and "ALL PASSED" in r.stdout, "harness failed")

src = lambda n: (ROOT / "src" / n).read_text(encoding="utf-8", errors="replace")
bb = (ROOT / "build.bat").read_text(encoding="utf-8", errors="replace")
cm = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8", errors="replace")
ini = (ROOT / "Nioh3EngineFix.ini").read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")
dm = src("dllmain.cpp")
lk = (REPO / "tools" / "testmatrix" / "live_keys.py").read_text(encoding="utf-8")
check("src\\bkstats.cpp" in bb and "build\\bkstats.obj" in bb, "build.bat compiles and links bkstats")
check("src/bkstats.cpp" in cm, "CMakeLists lists bkstats.cpp")
check('#include "bkstats.h"' in dm and "bkstats::Start(bki);" in dm, "dllmain starts bkstats")
check("if (bkstats::SetTuning(key, val, reply, n)) return true;" in dm, "LiveTuning routes KL_Div")
check("\nBackend = 0\n" in ini and "\nKL_Div = 0\n" in ini, "ini documents Backend = 0 and KL_Div = 0")
check('"KL_Div"' in lk, "live_keys.py: Tuning.KL_Div live-tuning")
print("ALL PASSED" if not fails else "FAILED:\n  " + "\n  ".join(fails))
sys.exit(1 if fails else 0)
