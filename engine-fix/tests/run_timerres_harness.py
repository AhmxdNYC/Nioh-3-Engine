"""run_timerres_harness.py - TR offline tests (src/timerres.cpp) + SkipDebugString wiring. Not a game test.

    py -I tests/run_timerres_harness.py [path/to/Nioh3.dump.exe]

1. Builds tests/timerres_harness.cpp + src/timerres.cpp into build/trtest and runs it: the exe imports WINMM itself, so TR hooks
   its own import slots like Nioh3.exe's. Switch off = calls pass + counted; on = 1 ms held (NtQueryTimerResolution <= 1 ms,
   Sleep(1) < 3 ms), pairs skipped; begin/end stay balanced per thread across switch flips (4 threads x 20000 pairs while the
   switch flips); after the hold is released winmm's refcount is 0 (an extra end is refused); cost per skipped pair.
2. Dump: Nioh3.exe imports WINMM timeBeginPeriod / timeEndPeriod (normal import, slots 38E2B80 / next), every caller is one of
   the three HID reader threads (AEF900 / AF07D0 / AF14E0), and AF07D0's loop is timeBeginPeriod(g) ; Sleep(4) ; timeEndPeriod(g)
   with g_HidTimerPeriod 44B68F0 = 1.
3. Source wiring: build.bat / CMakeLists build timerres.cpp, dllmain starts it and logs its stats line, the menu has the TR row,
   the ini documents TR and SkipDebugString, Log() skips OutputDebugStringA only with SkipDebugString and no debugger.
Must end ALL PASSED.
"""
import pathlib
import re
import struct
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
REPO = ROOT.parent.parent
DUMP = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path(r"re\Nioh3.dump.exe")
OUT = ROOT / "build" / "trtest"
OUT.mkdir(parents=True, exist_ok=True)
fails = []


def check(c, msg):
    if not c:
        fails.append(msg)
    return c


# ---- 1. harness ----
setup = REPO / "tools" / "msvc" / "setup_x64.bat"
if not setup.exists():  # git worktree: tools\ is not tracked, use the main checkout's portable MSVC
    setup = pathlib.Path(r"tools\msvc\setup_x64.bat")
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
cmd = pre + (
    'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /permissive- /DUNICODE /D_UNICODE "{t}\\timerres_harness.cpp" "{s}\\timerres.cpp" '
    '/Fo"{o}\\\\" /Fe"{o}\\timerres_harness.exe" /link kernel32.lib winmm.lib'
).format(o=OUT, t=ROOT / "tests", s=ROOT / "src")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-4000:], r.stderr[-2000:])
    sys.exit("compile failed")
warn = [l for l in r.stdout.splitlines() if "warning" in l]
check(not warn, "compiler warnings: %s" % warn[:3])
r = subprocess.run([str(OUT / "timerres_harness.exe")], capture_output=True, text=True, timeout=300)
print(r.stdout.strip())
check(r.returncode == 0 and "ALL PASSED" in r.stdout, "harness failed")

# ---- 2. dump: the game's WINMM timer-period imports and their only callers ----
img = DUMP.read_bytes()
pe = struct.unpack_from("<I", img, 0x3C)[0]
imp = struct.unpack_from("<I", img, pe + 24 + 112 + 8)[0]
slots = {}
d = imp
while True:
    oft, _, _, name, ft = struct.unpack_from("<IIIII", img, d)
    if not name:
        break
    dll = img[name:img.index(b"\0", name)].decode().lower()
    if dll == "winmm.dll":
        i = 0
        while True:
            e = struct.unpack_from("<Q", img, oft + 8 * i)[0]
            if not e:
                break
            if not e >> 63:
                fn = img[e + 2:img.index(b"\0", e + 2)].decode()
                slots[fn] = ft + 8 * i
            i += 1
    d += 20
check("timeBeginPeriod" in slots and "timeEndPeriod" in slots, "dump: WINMM timeBeginPeriod / timeEndPeriod imported")
print("dump: WINMM slots", {k: hex(v) for k, v in slots.items() if k.startswith("time")})
text = (0x1000, 0x38E0000)  # .text of v2.02 (callers are FF 15 rel32 to the slot)
hid = [(0xAEF900, 0xAF07D0), (0xAF07D0, 0xAF08E6), (0xAF14E0, 0xAF14E0 + 4214)]
for fn in ("timeBeginPeriod", "timeEndPeriod"):
    s = slots.get(fn)
    if not s:
        continue
    callers = []
    for m in re.finditer(rb"\xFF\x15", img[text[0]:text[1]]):
        a = text[0] + m.start()
        if a + 6 + struct.unpack_from("<i", img, a + 2)[0] == s:
            callers.append(a)
    print("dump: %s called from %s" % (fn, [hex(c) for c in callers]))
    check(len(callers) >= 3 and all(any(lo <= c < hi for lo, hi in hid) for c in callers),
          "dump: every %s caller is a HID reader thread" % fn)
# AF07D0 loop: mov ecx,[g] ; call [timeBeginPeriod] ; mov ecx,4 ; call [Sleep] ; mov ecx,[g] ; call [timeEndPeriod]
seg = img[0xAF07D0:0xAF08E6]
m = re.search(rb"\x8B\x0D(....)\xFF\x15(....)\xB9\x04\x00\x00\x00\xFF\x15(....)\x8B\x0D(....)\xFF\x15(....)", seg, re.S)
if check(m is not None, "dump: AF07D0 begin(g) ; Sleep(4) ; end(g) loop shape"):
    a = 0xAF07D0 + m.start()
    g1 = a + 6 + struct.unpack_from("<i", img, a + 2)[0]
    print("dump: AF07D0 loop at %#x, period global %#x = %d" % (a, g1, struct.unpack_from("<I", img, g1)[0]))
    check(g1 == 0x44B68F0 and struct.unpack_from("<I", img, g1)[0] == 1, "dump: g_HidTimerPeriod 44B68F0 = 1")

# ---- 3. source wiring ----
src = (ROOT / "src" / "dllmain.cpp").read_text(encoding="utf-8", errors="replace")
menu_cpp = (ROOT / "src" / "menu.cpp").read_text(encoding="utf-8", errors="replace")
menu_h = (ROOT / "src" / "menu.h").read_text(encoding="utf-8", errors="replace")
ini = (ROOT / "Nioh3EngineFix.ini").read_text(encoding="utf-8", errors="replace")
bat = (ROOT / "build.bat").read_text(encoding="utf-8", errors="replace")
cmk = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8", errors="replace")
check("timerres.obj" in bat and bat.count("timerres.obj") == 2, "build.bat compiles + links timerres.obj")
check("src/timerres.cpp" in cmk, "CMakeLists lists timerres.cpp")
check("timerres::Start(ti)" in src and "timerres::StatusLine" in src and "mh.exp[menu::kExpTR].enable = timerres::Switch()" in src,
      "dllmain starts TR, logs its line, wires the menu switch")
check("kExpTR" in menu_h and 'L"TR"' in menu_cpp and "kXTR" in menu_cpp, "menu row TR")
check(re.search(r"^TR = 0\s*$", ini, re.M) is not None, "ini: TR = 0 documented")
check(re.search(r"^SkipDebugString = 0\s*$", ini, re.M) is not None, "ini: SkipDebugString = 0 documented")
check("if (!g_skipOds || IsDebuggerPresent()) OutputDebugStringA(line);" in src and src.count("OutputDebugStringA(") == 1,
      "Log(): the only OutputDebugStringA is gated by SkipDebugString / debugger")
check('c.skipOds = I(L"Debug", L"SkipDebugString", c.skipOds);' in src and "g_skipOds = cfg.skipOds != 0;" in src,
      "SkipDebugString read and applied before the first log line")

if fails:
    print("FAILED:")
    for f in fails:
        print("  -", f)
    sys.exit(1)
print("ALL PASSED")
