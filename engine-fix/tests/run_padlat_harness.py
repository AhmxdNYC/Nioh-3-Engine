"""run_padlat_harness.py - PadLatency offline tests (src/padlat_core.h, src/padlat.cpp). Not a game test.

    py -I tests/run_padlat_harness.py [path/to/Nioh3.dump.exe]

1. Builds tests/padlat_harness.cpp + src/padlat.cpp (MSVC: cl on PATH or ..\\..\\tools\\msvc\\setup_x64.bat) into build/pltest and
   runs it: change classes, synthetic 60 Hz frame timeline (stock + IK order) through the real tracker / ring / matcher with exact
   expected input->present and input->GPU-done, poll interval, expiry, ring under 4 producers, the real XInputGetState hook on a fake
   IAT slot (pass-through identical, counters, cost per call).
2. Dump: the game's pad APIs are XINPUT1_4 #2 (XInputGetState, the hooked slot) + #3 (SetState), DINPUT8 (keyboard / mouse) and
   HID.DLL (Sony pads on the game's own reader threads); no RawInput / GameInput / Windows.Gaming.Input import or name string.
3. Source wiring: PadLatency turns the TS probes on, padlat::Start gets the TS ring before osd::Start, osd feeds OnPresent and
   keeps its Present hook for PadLatency, build.bat / CMakeLists build padlat.cpp, ini documents the keys.
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
OUT = ROOT / "build" / "pltest"
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
    'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /permissive- /DUNICODE /D_UNICODE "{t}\\padlat_harness.cpp" "{s}\\padlat.cpp" '
    '/Fo"{o}\\\\" /Fe"{o}\\padlat_harness.exe" /link kernel32.lib'
).format(o=OUT, t=ROOT / "tests", s=ROOT / "src")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-4000:], r.stderr[-2000:])
    sys.exit("compile failed")
warn = [l for l in r.stdout.splitlines() if "warning" in l]
check(not warn, "compiler warnings: %s" % warn[:3])
r = subprocess.run([str(OUT / "padlat_harness.exe")], capture_output=True, text=True, timeout=300)
print(r.stdout.strip())
check(r.returncode == 0 and "ALL PASSED" in r.stdout, "harness failed")

# ---- 2. the game's input imports ----
img = DUMP.read_bytes()
pe = struct.unpack_from("<I", img, 0x3C)[0]
imp = struct.unpack_from("<I", img, pe + 24 + 112 + 8)[0]
dlls = {}
d = imp
while True:
    oft, _, _, name_rva, ft = struct.unpack_from("<IIIII", img, d)
    if name_rva == 0:
        break
    dll = img[name_rva:img.index(b"\0", name_rva)].decode("ascii").upper()
    th = oft or ft
    names = []
    i = 0
    while True:
        ent = struct.unpack_from("<Q", img, th + 8 * i)[0]
        if ent == 0:
            break
        names.append(("#%d" % (ent & 0xFFFF)) if ent >> 63 else img[ent + 2:img.index(b"\0", ent + 2)].decode("ascii"))
        if dll.startswith("XINPUT") and ent >> 63 and ent & 0xFFFF == 2:
            dlls.setdefault("_slot", ft + 8 * i)
        i += 1
    dlls[dll] = names
    d += 20
xin = {k: v for k, v in dlls.items() if k.startswith("XINPUT")}
check(list(xin) == ["XINPUT1_4.DLL"] and sorted(xin["XINPUT1_4.DLL"]) == ["#2", "#3"], "XInput imports %s" % xin)
check(dlls.get("_slot") == 0x38E2C50, "XInputGetState IAT slot %s (menu.cpp kRvaXInputIat 0x38E2C50)" % hex(dlls.get("_slot", 0)))
allnames = {n for k, v in dlls.items() if k != "_slot" for n in v}
for bad in ("GetRawInputData", "GetRawInputBuffer", "RegisterRawInputDevices", "RoGetActivationFactory", "GameInputCreate"):
    check(bad not in allnames, "the game imports %s - PadLatency would miss that path" % bad)
for s in (b"GameInput", b"Windows.Gaming.Input", b"GetRawInputData", b"RegisterRawInputDevices"):
    check(s not in img and s.decode().encode("utf-16le") not in img, "name string %s in the dump (dynamic load?)" % s)
check("DirectInput8Create" in dlls.get("DINPUT8.DLL", []) and "HidD_GetAttributes" in dlls.get("HID.DLL", []), "DInput8 / HID imports")
print("dump: pad APIs = XINPUT1_4 #2/#3 (IAT 0x38E2C50), DINPUT8 (kb/mouse), HID.DLL (Sony pads); no RawInput / GameInput / WGI")

# ---- 3. wiring ----
src = lambda n: (ROOT / "src" / n).read_text(encoding="utf-8")  # noqa: E731
dm, osd = src("dllmain.cpp"), src("osd.cpp")
check('if (!c.ts && padlat::Wanted(f)) c.ts = 1, c.tsForPad = true;' in dm, "dllmain: PadLatency does not turn TS on")
i_pl, i_osd = dm.find("padlat::Start(li)"), dm.find("osd::Start(si)")
check(0 < i_pl < i_osd, "dllmain: padlat::Start must run before osd::Start")
check("li.tsRing = reinterpret_cast<const volatile int64_t*>(lat_ring);" in dm and "exDone[menu::kExpTS]" in dm[dm.find("PadLatency (padlat.cpp"):i_pl],
      "dllmain: TS ring handed over only when TS is installed")
check("padlat::OnPresent(g_ctx.q, now);" in osd and "!padlat::WantsPresentHook()" in osd, "osd: OnPresent feed / Present hook gate")
bb = (ROOT / "build.bat").read_text(encoding="utf-8")
check("src\\padlat.cpp" in bb and "build\\padlat.obj" in bb.split("link /nologo")[1], "build.bat")
check("src/padlat.cpp" in (ROOT / "CMakeLists.txt").read_text(encoding="utf-8"), "CMakeLists.txt")
ini = (ROOT / "Nioh3EngineFix.ini").read_text(encoding="utf-8")
check(re.search(r"^PadLatency = 0$", ini, re.M) and re.search(r"^PadLatencyReportSec = 30$", ini, re.M), "ini keys")
core = src("padlat_core.h")
check("kTriggerThreshold = 30" in core and "kLeftDeadzone = 7849, kRightDeadzone = 8689" in core, "XInput thresholds")
print("wiring: TS on for PadLatency, start order, TS ring hand-off, osd feed + hook gate, build files, ini keys")

if fails:
    print("FAILED (%d):" % len(fails))
    for f in fails:
        print("  - " + f)
    sys.exit(1)
print("PADLAT: ALL PASSED")
