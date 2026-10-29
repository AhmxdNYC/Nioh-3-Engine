"""run_samelook_harness.py - offline tests of the same-look package (src/samelook.cpp / .asm, 1.7.49). Not a game test.

    py -I tests/run_samelook_harness.py [path/to/Nioh3.dump.exe]

1. Builds tests/samelook_harness.cpp + src/samelook.cpp and runs it: VSMA decisions over simulated tile bursts (marked tiles,
   valid refreshes, the front and duplicates always drawn; no waiting tile skipped VSMA_K visits in a row; K = 1 / switch off /
   throttle param >= 0 = stock) and the render-config knob writes.
2. Dump: the three VSMA call sites call 1F8340 / 1F7960 / 1F96E0, the batch loop keeps rbx = tile, r14 = page manager,
   edi = budget up to the bind call, 1F8094 computes the same tile index, 8F041C reads node param 37B3FAE9; RCO1 is
   `call [rax+4B8h]` with rcx = rsi right after ApplyGraphicsSettings.
3. Source wiring: build.bat / CMakeLists, dllmain groups / targets / rows / stats line, menu rows, ini keys.
Must end ALL PASSED.
"""
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
REPO = ROOT.parent.parent
DUMP = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path(r"C:\nioh3-engine\re\Nioh3.dump.exe")
OUT = ROOT / "build" / "sltest"
OUT.mkdir(parents=True, exist_ok=True)
fails = []


def check(c, msg):
    if not c:
        fails.append(msg)
    return c


setup = REPO / "tools" / "msvc" / "setup_x64.bat"
if not setup.exists():
    setup = pathlib.Path(r"C:\nioh3-engine\tools\msvc\setup_x64.bat")
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
cmd = pre + (
    'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /permissive- /DUNICODE /D_UNICODE "{t}\\samelook_harness.cpp" "{s}\\samelook.cpp" "{s}\\extraperf.cpp" '
    '/Fo"{o}\\\\" /Fe"{o}\\samelook_harness.exe" /link kernel32.lib'
).format(o=OUT, t=ROOT / "tests", s=ROOT / "src")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-4000:], r.stderr[-2000:])
    sys.exit("compile failed")
warn = [l for l in r.stdout.splitlines() if "warning" in l]
check(not warn, "compiler warnings: %s" % warn[:3])
r = subprocess.run([str(OUT / "samelook_harness.exe")], capture_output=True, text=True, timeout=120)
print(r.stdout.strip())
check(r.returncode == 0 and "ALL PASSED" in r.stdout, "harness failed")

img = DUMP.read_bytes()


def call_target(at):
    assert img[at] == 0xE8
    return at + 5 + int.from_bytes(img[at + 1:at + 5], "little", signed=True)


check(call_target(0x538F62) == 0x1F8340, "VSMA1 does not call VsmClipmapRender")
check(call_target(0x1F8AB2) == 0x1F7960, "VSMA2 does not call VsmTilesPerFrameBudget")
check(call_target(0x1F8AF2) == 0x1F96E0, "VSMA3 does not call VsmTileBindViewport")
check(img[0x1F8A8B:0x1F8A91] == bytes.fromhex("488BD3498BCE"), "1F8168 is not called with (r14, rbx)")
check(img[0x1F8AB7:0x1F8AC2] == bytes.fromhex("4C8BC3498BD5498BCE8BF8"), "edi = budget / r8 = rbx / rcx = r14 changed")
seg = img[0x1F8AC2:0x1F8AF2]
for bad in (b"\x8B\xF8", b"\x8B\xFA", b"\x48\x8B\xD8", b"\x4C\x8B\xF0"):  # nothing in between re-assigns edi / rbx / r14
    check(bad not in seg, "a write to edi / rbx / r14 between the budget call and the bind call: %s" % bad.hex())
check(img[0x1F8094:0x1F80C6].find(bytes.fromhex("0FAF81E8000400034208 0FAF81E40004000342 04".replace(" ", ""))) > 0,
      "1F8094 tile index formula changed")
check(bytes.fromhex("BAE9FAB337") in img[0x8F041C:0x8F0450], "8F041C does not read param 37B3FAE9")
check(img[0x1F95B8:0x1F95C4] == bytes.fromhex("488B88303D0000E8586E6F00"), "1F95BF throttle call (view+3D30 table) changed")
check(img[0x39B7C1:0x39B7D0] == bytes.fromhex("488B06488BCE498B17FF90B8040000"), "RCO1 site changed")
check(call_target(0x39B7BC) == 0x39A39C, "39B7BC is not ApplyGraphicsSettings")

src = (ROOT / "src")
dm = (src / "dllmain.cpp").read_text(encoding="utf-8")
for s in ('"VSMA")) return samelook::VsmaIni()', '"RCO")) return samelook::RcoIni()', "samelook::Configure(sl)",
          "samelook::SetVsmaTargets", "samelook::SetRcoInstalled", "samelook::StatusLine", "menu::kExpVSMA", "menu::kExpCLDR",
          "gen::Target::caveRco: return"):
    check(s in dm, "dllmain.cpp lacks: " + s)
mc = (src / "menu.cpp").read_text(encoding="utf-8")
for s in ('L"VSMA", L"LPVR", L"SKYR", L"FOGT", L"CLDR"', '"(VSMA)"', '"(CLDR)"', "kXPCM - kXW6 + 1 == kNumExp", "e == kExpVSMA"):
    check(s in mc, "menu.cpp lacks: " + s)
check("samelook::OnPresent()" in (src / "osd.cpp").read_text(encoding="utf-8"), "osd.cpp does not count presents")
bb = (ROOT / "build.bat").read_text(encoding="utf-8")
check("src\\samelook.cpp" in bb and "src\\samelook.asm" in bb and "build\\samelook_caves.obj" in bb, "build.bat")
check("src/samelook.cpp src/samelook.asm" in (ROOT / "CMakeLists.txt").read_text(encoding="utf-8"), "CMakeLists.txt")
ini = (ROOT / "Nioh3EngineFix.ini").read_text(encoding="utf-8", errors="replace")
for k in ("VSMA = 0", "LPVR = 0", "SKYR = 0", "FOGT = 0", "CLDR = 0", "VSMA_K = 2", "VSMA_Front = 2", "VSMA_First = 0", "LPVR_N = 2"):
    check(k in ini, "ini lacks " + k)

print("\n".join("FAIL: " + f for f in fails) if fails else "ALL PASSED")
sys.exit(1 if fails else 0)
