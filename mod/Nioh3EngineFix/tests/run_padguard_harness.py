r"""run_padguard_harness.py - PadListGuard offline tests (src/padguard.cpp, GitHub #79 / #55). Not a game test.

    py -I tests/run_padguard_harness.py [path/to/Nioh3.dump.exe]

1. tests/test_padguard.py against the dump: table == dump == patches.json, `cmp node, head ; je exit` loop heads, no branch
   into the replaced bytes, node register dead at every exit, no walk of the four hosts missing.
2. Builds tests/padguard_harness.cpp + src/padguard.cpp (MSVC) and runs it: fake image with the dump's bytes, Start() per ini /
   byte case; every site with node = 0 / head / other, stock vs patched (stop address, all GPRs, flags), counter + log line,
   rate limit, chained unwind from every jump of every cave.
Must end ALL PASSED.
"""
import os
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
REPO = ROOT.parent.parent
DUMP = sys.argv[1] if len(sys.argv) > 1 else r"C:\nioh3-engine\re\Nioh3.dump.exe"
OUT = ROOT / "build" / "padguardtest"
OUT.mkdir(parents=True, exist_ok=True)
fails = []


def check(c, msg):
    if not c:
        fails.append(msg)
    return c


r = subprocess.run([sys.executable, "-I", str(ROOT / "tests" / "test_padguard.py"), DUMP], capture_output=True, text=True)
print(r.stdout[-6000:], r.stderr[-2000:])
check(r.returncode == 0, "test_padguard.py failed")

setup = pathlib.Path(os.environ.get("N3_MSVC_SETUP", str(REPO / "tools" / "msvc" / "setup_x64.bat")))
if not setup.exists():
    setup = pathlib.Path(r"C:/nioh3-engine/tools/msvc/setup_x64.bat")
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
cmd = pre + (
    'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /permissive- /DUNICODE /D_UNICODE "{t}\\padguard_harness.cpp" "{s}\\padguard.cpp" '
    '/Fo"{o}\\\\" /Fe"{o}\\padguard_harness.exe" /link kernel32.lib user32.lib'
).format(o=OUT, t=ROOT / "tests", s=ROOT / "src")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-4000:], r.stderr[-2000:])
    sys.exit("compile failed")
check(not [l for l in r.stdout.splitlines() if "warning" in l], "compiler warnings:\n" + r.stdout)
r = subprocess.run([str(OUT / "padguard_harness.exe")], capture_output=True, text=True, timeout=180)
print(r.stdout[-8000:])
check(r.returncode == 0 and "ALL OK" in r.stdout, "harness failed")

if fails:
    print("FAILED:")
    for f in fails:
        print("  - " + f)
    sys.exit(1)
print("ALL PASSED")
