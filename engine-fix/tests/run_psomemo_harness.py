"""run_psomemo_harness.py - PCM graphics-PSO lookup memo offline test (src/psomemo.cpp). Not a game test.

    py -I tests/run_psomemo_harness.py

1. Builds tests/psomemo_harness.cpp + src/psomemo.cpp (/DPSOMEMO_TEST) into build/pmtest and runs it: 2M lookups against a
   model of PsoCacheFindGfx 11976C (skewed keys, equal-hash collisions, runtime inserts, two clear + refill with new PSOs);
   every return value and all 0x90 copied bytes must equal the model's, and most lookups must be served by the memo.
2. Source wiring: build.bat / CMakeLists build psomemo.cpp, dllmain starts it, logs its line, wires the PCM row and keeps
   group PCM out of the byte-patch batch; the menu has the PCM row; the ini documents PCM.
Must end ALL PASSED.
"""
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
REPO = ROOT.parent.parent
OUT = ROOT / "build" / "pmtest"
OUT.mkdir(parents=True, exist_ok=True)
fails = []


def check(c, msg):
    if not c:
        fails.append(msg)
    return c


setup = REPO / "tools" / "msvc" / "setup_x64.bat"
if not setup.exists():  # git worktree: tools\ is not tracked, use the main checkout's portable MSVC
    setup = pathlib.Path(r"tools\msvc\setup_x64.bat")
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
cmd = pre + (
    'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /permissive- /DUNICODE /D_UNICODE /DPSOMEMO_TEST "{t}\\psomemo_harness.cpp" '
    '"{s}\\psomemo.cpp" /Fo"{o}\\\\" /Fe"{o}\\psomemo_harness.exe" /link kernel32.lib'
).format(o=OUT, t=ROOT / "tests", s=ROOT / "src")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-4000:], r.stderr[-2000:])
    sys.exit("compile failed")
warn = [l for l in r.stdout.splitlines() if "warning" in l]
check(not warn, "compiler warnings: %s" % warn[:3])
r = subprocess.run([str(OUT / "psomemo_harness.exe")], capture_output=True, text=True, timeout=300)
print(r.stdout.strip())
check(r.returncode == 0 and "ALL PASSED" in r.stdout, "harness failed")

src = lambda n: (ROOT / "src" / n).read_text(encoding="utf-8", errors="replace")
bb = (ROOT / "build.bat").read_text(encoding="utf-8", errors="replace")
cm = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8", errors="replace")
ini = (ROOT / "Nioh3EngineFix.ini").read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")
dm, mc, mh = src("dllmain.cpp"), src("menu.cpp"), src("menu.h")
check("src\\psomemo.cpp" in bb and "build\\psomemo.obj" in bb, "build.bat compiles + links psomemo")
check("src/psomemo.cpp" in cm, "CMakeLists has psomemo.cpp")
check("psomemo::Start(" in dm and "psomemo::StatusLine" in dm and "kExpPCM" in dm and 'strcmp(g, "PCM")' in dm,
      "dllmain starts PM, logs it, wires the row, keeps group PCM out of the batch")
check("kExpPCM" in mh and "kXPCM" in mc and 'L"PCM"' in mc and "(PCM)" in mc, "menu row PM")
check("\nPCM = 0" in ini, "ini documents PCM = 0")

print("\n".join("FAIL: " + f for f in fails) if fails else "ALL PASSED")
sys.exit(1 if fails else 0)
