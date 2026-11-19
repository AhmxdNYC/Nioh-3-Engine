"""run_fwp_harness.py - FWP frame-wait park offline test (src/framewaitpark.h, #101247). Not a game test.

    py -I tests/run_fwp_harness.py

1. Builds tests/fwp_harness.cpp (includes src/framewaitpark.h, the code caveFwp calls) into build/fwptest and runs it: a worker
   clears bit 0 of a fake job after 5..3000 us; mode 2 (stock shape) vs mode 1 (park). Never returns while bit 0 is set;
   for waits >= 1 ms (past the 200 us spin) the park halves the caller CPU cycles; >= 300 us it kicks 10x less. Prints the added wake latency.
2. Source wiring: patches.json FWP site/deps, caveFwp in caves.asm, dllmain reads [Patches] FWP (default 0), the ini and the
   shipped defaults document FWP = 0.
Must end ALL PASSED.
"""
import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
REPO = ROOT.parent.parent
OUT = ROOT / "build" / "fwptest"
OUT.mkdir(parents=True, exist_ok=True)
fails = []


def check(c, msg):
    if not c:
        fails.append(msg)
    return c


setup = REPO / "tools" / "msvc" / "setup_x64.bat"
if not setup.exists():  # git worktree: tools\msvc is not tracked, use the main checkout's portable MSVC
    setup = pathlib.Path(r"C:\Users\Ahmad Hamza\nioh3-engine\tools\msvc\setup_x64.bat")
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
cmd = pre + (
    'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /permissive- /DUNICODE /D_UNICODE "{t}\\fwp_harness.cpp" '
    '/Fo"{o}\\\\" /Fe"{o}\\fwp_harness.exe" /link kernel32.lib'
).format(o=OUT, t=ROOT / "tests")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-3000:], r.stderr[-2000:])
    print("FAILED: harness build")
    sys.exit(1)
r = subprocess.run([str(OUT / "fwp_harness.exe")], capture_output=True, text=True, timeout=300)
print(r.stdout)
check(r.returncode == 0 and "ALL PASSED" in r.stdout, "harness: %s" % (r.stdout.strip().splitlines()[-1:] or ["no output"]))

doc = json.loads((ROOT / "patches.json").read_text(encoding="utf-8"))
site = [s for s in doc["sites"] if s.get("id") == "FWP"]
check(len(site) == 1 and site[0]["target"] == "caveFwp" and site[0]["kind"] == "jmp" and site[0]["len"] == 7, "patches.json FWP site")
check(sum(1 for d in doc["deps"] if d.get("group") == "FWP") == 3, "patches.json FWP deps (3)")
asm = (ROOT / "src" / "caves.asm").read_text(encoding="utf-8")
check("caveFwp PROC FRAME" in asm and "jmp     qword ptr [fwp_resume]" in asm, "caves.asm caveFwp")
dll = (ROOT / "src" / "dllmain.cpp").read_text(encoding="utf-8")
check('c.fwp = I(L"Patches", L"FWP", 0)' in dll, "dllmain reads [Patches] FWP default 0")
check('if (!strcmp(g, "FWP")) return cfg.fwp != 0;' in dll, "dllmain group switch")
check("case gen::Target::caveFwp" in dll, "dllmain cave target")
ini = (ROOT / "Nioh3EngineFix.ini").read_text(encoding="utf-8")
check("\nFWP = 0" in ini.replace("\r\n", "\n"), "ini documents FWP = 0")
check('{"Patches", "FWP", "0"}' in (ROOT / "src" / "menu_shipped_defaults.h").read_text(encoding="utf-8"), "shipped default FWP 0")

if fails:
    print("FAILED:")
    for f in fails:
        print("  -", f)
    sys.exit(1)
print("ALL PASSED")
