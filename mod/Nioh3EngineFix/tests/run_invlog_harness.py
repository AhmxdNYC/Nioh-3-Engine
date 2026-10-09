r"""run_invlog_harness.py - InvLog offline tests (src/bosslog.cpp, [Debug] InvLog). Not a game test.

    py -I tests/run_invlog_harness.py

1. Builds tests/invlog_harness.cpp (includes src/bosslog.cpp) with MSVC /W4 into build/invlogtest and runs it: the real Install()
   on a stand-in InventoryAddItem with the real prologue (refusal on other bytes, result and args intact through the hook), the
   log line fields, the 30 lines/s limit + summary, the return-address and source helpers.
2. Wiring: kInvHooks bytes == patches.json BL31 orig == the dump at 54D324; ini documents [Debug] InvLog = 0.
Must end ALL PASSED.
"""
import json
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "build" / "invlogtest"
OUT.mkdir(parents=True, exist_ok=True)
DUMP = pathlib.Path(r"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe")
fails = []


def check(c, msg):
    if not c:
        fails.append(msg)
    return c


setup = ROOT.parent.parent / "tools" / "msvc" / "setup_x64.bat"
if not setup.exists():
    setup = pathlib.Path(r"C:\Users\Ahmad Hamza\nioh3-engine\tools\msvc\setup_x64.bat")
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
cmd = pre + (
    'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /permissive- /DUNICODE /D_UNICODE "{t}\\invlog_harness.cpp" '
    '/Fo"{o}\\\\" /Fe"{o}\\invlog_harness.exe" /link kernel32.lib user32.lib'
).format(o=OUT, t=ROOT / "tests")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-4000:], r.stderr[-2000:])
    sys.exit("compile failed")
warn = [l for l in r.stdout.splitlines() if "warning" in l]
check(not warn, "compiler warnings: %s" % warn[:3])
r = subprocess.run([str(OUT / "invlog_harness.exe")], capture_output=True, text=True, timeout=120)
print(r.stdout.strip()[-3000:])
check(r.returncode == 0 and "ALL PASSED" in r.stdout, "harness failed")

src = (ROOT / "src" / "bosslog.cpp").read_text(encoding="utf-8")
m = re.search(r'\{"InventoryAddItem", 0x([0-9A-F]+), (\d+), \{([^}]*)\}', src)
if check(m is not None, "kInvHooks entry not found"):
    rva, n = int(m.group(1), 16), int(m.group(2))
    b = bytes(int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{2})", m.group(3)))
    doc = json.loads((ROOT / "patches.json").read_text(encoding="utf-8"))
    st = next((s for s in doc["sites"] if s["id"] == "BL31"), None)
    check(st is not None and int(st["rva"], 16) == rva and st["len"] == n and bytes.fromhex(st["orig"]) == b, "patches.json BL31 != kInvHooks")
    if DUMP.exists():
        check(DUMP.read_bytes()[rva:rva + n] == b, "dump bytes at 0x%X != kInvHooks" % rva)
ini = (ROOT / "Nioh3EngineFix.ini").read_text(encoding="utf-8")
check(re.search(r"(?m)^InvLog = 0", ini) is not None, "ini: [Debug] InvLog = 0")

print()
if fails:
    print("FAILED (%d):" % len(fails))
    for f in fails:
        print("  - " + f)
    sys.exit(1)
print("ALL PASSED")
