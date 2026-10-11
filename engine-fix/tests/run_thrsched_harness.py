"""run_thrsched_harness.py - TSTATE census + RTP offline test (src/thrsched.cpp). Not a game test.

    py -I tests/run_thrsched_harness.py

1. Builds tests/thrsched_harness.cpp + src/thrsched.cpp into build/thrtest and runs it: 5 named busy threads share ONE logical
   processor; the census must see ~1/5 CPU and >= 50 % READY for [App]Game with RTP off, RTP on must raise [App]Game and the
   dispatcher to ABOVE_NORMAL (they then take the CPU, the workers starve), RTP off must restore the old priorities.
2. Source wiring: build.bat / CMakeLists build thrsched.cpp, dllmain starts it and logs the RTP line, the menu has the RTP row
   (kExpRTP, ini key RTP), osd exports GamePresentTid, the ini documents RTP / RTP_Level / ThreadState / Focus.
Must end ALL PASSED.
"""
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
REPO = ROOT.parent.parent
OUT = ROOT / "build" / "thrtest"
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
    'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /permissive- /DUNICODE /D_UNICODE "{t}\\thrsched_harness.cpp" "{s}\\thrsched.cpp" '
    '/Fo"{o}\\\\" /Fe"{o}\\thrsched_harness.exe" /link kernel32.lib'
).format(o=OUT, t=ROOT / "tests", s=ROOT / "src")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-4000:], r.stderr[-2000:])
    sys.exit("compile failed")
warn = [l for l in r.stdout.splitlines() if "warning" in l]
check(not warn, "compiler warnings: %s" % warn[:3])
busy = subprocess.run(["tasklist", "/fi", "imagename eq Nioh3.exe"], capture_output=True, text=True).stdout
if "Nioh3.exe" in busy:  # 5 spinners on one CPU for ~40 s would disturb a running game measurement
    sys.exit("Nioh3 is running - run this harness when the game is closed")
r = subprocess.run([str(OUT / "thrsched_harness.exe")], capture_output=True, text=True, timeout=300)
print(r.stdout.strip())
check(r.returncode == 0 and "ALL PASSED" in r.stdout, "harness failed")

src = lambda n: (ROOT / "src" / n).read_text(encoding="utf-8", errors="replace")
bb = (ROOT / "build.bat").read_text(encoding="utf-8", errors="replace")
cm = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8", errors="replace")
ini = (ROOT / "Nioh3EngineFix.ini").read_text(encoding="utf-8", errors="replace")
dm, mc, mh, oh = src("dllmain.cpp"), src("menu.cpp"), src("menu.h"), src("osd.h")
check("src\\thrsched.cpp" in bb and "build\\thrsched.obj" in bb, "build.bat compiles + links thrsched")
check("src/thrsched.cpp" in cm, "CMakeLists has thrsched.cpp")
check("thrsched::Start(ti)" in dm and "thrsched::RtpStatusLine" in dm and "kExpRTP" in dm, "dllmain starts thrsched, logs RTP, wires the row")
check("kExpRTP" in mh and "kXRTP" in mc and 'L"RTP"' in mc and "(RTP)" in mc, "menu row RTP")
check("GamePresentTid" in oh, "osd exports GamePresentTid")
for k in ("RTP = 0", "RTP_Level = 1", "ThreadState = 0", "ThreadStateUs = 100000", "Focus = 0"):
    check(("\n" + k) in ini.replace("\r\n", "\n"), "ini documents " + k)

print("\n".join("FAIL: " + f for f in fails) if fails else "ALL PASSED")
sys.exit(1 if fails else 0)
