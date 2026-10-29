"""run_extraperf_harness.py - offline tests of the Extra performance options (src/extraperf.cpp / .asm, 1.7.59). Not a game test.

    py -I tests/run_extraperf_harness.py [path/to/Nioh3.dump.exe]

1. Builds tests/extraperf_harness.cpp + .asm with src/extraperf.cpp + .asm and runs it: every render-config field per level
   (min / scale / constant), no compounding when the engine does not re-fill a field, switching off restores the engine value,
   nothing outside the fields written; the GI cave (scale x level, floor 0.1), the meshlet LOD cave (rdx passed through, x level,
   cap 10, no compounding, restore), the GPU particle caves (global = register x level, the register and rax kept); presets.
2. Dump: GIS1-4 call GiScaleLookup 187A50 from GiRenderScalesApply with ecx = 0 / ebx / 2 / 3; EPL1 calls 2353C0 with
   rcx = [rdi+18h]; EPF1 / EPF2 store xmm1 / xmm2 (view+2650h / +2654h) to 4566008 / 4566000, the globals the effect DL
   recorders read; the RCO site is still right after ApplyGraphicsSettings; ApplyVolumetricQuality / ApplyAmbientOcclusion /
   ApplyMotionBlurQuality write the fields as extraperf.h says; the RenderConfig ctor types (ints 0x40C / 0x410 / 0xB44 /
   0x12F4 / 0x13DC, floats 0xAF4 / 0xAF8).
3. Source wiring: build.bat / CMakeLists, dllmain groups / targets / install / stats line, menu rows, ini keys, live_keys.
Must end ALL PASSED.
"""
import pathlib
import struct
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
REPO = ROOT.parent.parent
DUMP = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path(r"C:\nioh3-engine\re\Nioh3.dump.exe")
OUT = ROOT / "build" / "eptest"
OUT.mkdir(parents=True, exist_ok=True)
fails = []


def check(c, msg):
    if not c:
        fails.append(msg)
    return c


setup = REPO / "tools" / "msvc" / "setup_x64.bat"
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
cmd = pre + (
    'ml64 /nologo /c /W3 /Fo"{o}\\ep_caves.obj" "{s}\\extraperf.asm" && '
    'ml64 /nologo /c /W3 /Fo"{o}\\ep_probe.obj" "{t}\\extraperf_harness.asm" && '
    'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /permissive- /DUNICODE /D_UNICODE "{t}\\extraperf_harness.cpp" "{s}\\extraperf.cpp" '
    '"{o}\\ep_caves.obj" "{o}\\ep_probe.obj" /Fo"{o}\\\\" /Fe"{o}\\extraperf_harness.exe" /link kernel32.lib'
).format(o=OUT, t=ROOT / "tests", s=ROOT / "src")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-4000:], r.stderr[-2000:])
    sys.exit("compile failed")
warn = [l for l in r.stdout.splitlines() if "warning" in l]
check(not warn, "compiler warnings: %s" % warn[:3])
r = subprocess.run([str(OUT / "extraperf_harness.exe")], capture_output=True, text=True, timeout=120)
print(r.stdout.strip())
check(r.returncode == 0 and "ALL PASSED" in r.stdout, "harness failed")

img = DUMP.read_bytes()


def call_target(at):
    assert img[at] == 0xE8
    return at + 5 + int.from_bytes(img[at + 1:at + 5], "little", signed=True)


def rip_store(at):  # F3 0F 11 modrm disp32 (8 bytes)
    return at + 8 + int.from_bytes(img[at + 4:at + 8], "little", signed=True)


def i32(at):
    return struct.unpack_from("<i", img, at)[0]


for at in (0x18745A, 0x187464, 0x187471, 0x18747F):
    check(call_target(at) == 0x187A50, "GIS %X does not call GiScaleLookup" % at)
check(img[0x187458:0x18745A] == bytes.fromhex("33C9"), "GIS1 ecx = 0 changed")
check(img[0x187469:0x18746E] == bytes.fromhex("B902000000") and img[0x187476:0x18747B] == bytes.fromhex("B903000000"), "GIS3/4 ecx")
check(img[0x187A50:0x187A54] == bytes.fromhex("4883EC78"), "GiScaleLookup prologue changed")
check(call_target(0x233A49) == 0x2353C0 and img[0x233A42:0x233A49] == bytes.fromhex("488B4F18488BD6"), "EPL1 changed")
check(rip_store(0x4A5B76) == 0x4566008 and img[0x4A5B76:0x4A5B7A] == bytes.fromhex("F30F110D"), "EPF1 is not movss [4566008], xmm1")
check(rip_store(0x4A5B7E) == 0x4566000 and img[0x4A5B7E:0x4A5B82] == bytes.fromhex("F30F1115"), "EPF2 is not movss [4566000], xmm2")
check(img[0x4A5B18:0x4A5B30] == bytes.fromhex("F30F109154260000F30F108950260000F30F10814C260000"), "xmm2/xmm1/xmm0 sources changed")
check(img[0x39B7C1:0x39B7D0] == bytes.fromhex("488B06488BCE498B17FF90B8040000"), "RCO1 site changed")
check(call_target(0x39B7BC) == 0x39A39C, "39B7BC is not ApplyGraphicsSettings")
# appliers write what extraperf.h says
av = img[0x39B080:0x39B0E8]
for off in (0x13DC, 0x7B4):
    check(struct.pack("<I", off) in av, "ApplyVolumetricQuality no longer touches S+%X" % off)
# VOLUMETRIC LIGHTEST (cold branch 1995D41, reached by je at 39B08A): ScatterScale x const, X = Z = 0 - what XFOG 3 writes
check(img[0x39B08A:0x39B08C] == bytes.fromhex("0F84") and i32(0x39B08C) + 0x39B090 == 0x1995D41, "LIGHTEST branch moved")
check(img[0x1995D4D:0x1995D77] == bytes.fromhex("F3410F108198130000F30F590576E42302F3410F1181981300004183A1DC130000004183A1E013000000"),
      "VOLUMETRIC LIGHTEST no longer zeroes S+1398 / 13DC / 13E0")
check(struct.unpack_from("<f", img, 0x1995D5E + 0x223E476)[0] == 0.0, "LIGHTEST ScatterScale factor is not 0.0")
check(bytes.fromhex("C7818C18000001000000") in img[0x39B4A4:0x39B545] and bytes.fromhex("83A18C18000000") in img[0x39B4A4:0x39B545],
      "ApplyMotionBlurQuality S+188C writes changed")
check(struct.pack("<I", 0x12F4) in img[0x39B20C:0x39B330], "ApplyAmbientOcclusion S+12F4")
# RenderConfig ctor defaults (types)
ctor = img[0x6C1ED4:0x6C1ED4 + 19000]
check(bytes.fromhex("C7800C04000020000000") in ctor and bytes.fromhex("C7801004000000010000") in ctor, "ctor cloud ray samples 32 / 256 (int)")
check(bytes.fromhex("C783F80A00000000FA43") in ctor, "ctor ShadowEraseFarStart float 500")

src = ROOT / "src"
dm = (src / "dllmain.cpp").read_text(encoding="utf-8")
for s in ('extraperf::Wanted();  // 1.7.59: Extra performance rides on RCO', '"GIS") || !strcmp(g, "EPL") || !strcmp(g, "EPF")',
          "extraperf::Configure(ep)", "extraperf::SetGiInstalled", "extraperf::SetLodInstalled", "extraperf::SetFxInstalled",
          "extraperf::SetRcoInstalled", "extraperf::StatusLine", "gen::Target::caveFxDraw: return",
          "kLastTarget = (int)gen::Target::caveRDWake"):
    check(s in dm, "dllmain.cpp lacks: " + s)
check("extraperf::ApplyS(S)" in (src / "samelook.cpp").read_text(encoding="utf-8"), "samelook RcoApplyC does not call ApplyS")
mc = (src / "menu.cpp").read_text(encoding="utf-8")
for s in ('"(XPRE)"', '"(XCLD)"', '"(XFOG)"', '"(XGI)"', '"(XSHD)"', '"(XSHF)"', '"(XLOD)"', '"(XTER)"', '"(XPRT)"', '"(XAO)"', '"(XPOST)"',
          "kEp9 - kEp0 + 1 == extraperf::kNumOpts", 'add(L"ExtraPerf", k,', "extraperf::SetLevel(id - kEp0, q.value)",
          '"Extra performance (changes the look)"'):
    check(s in mc, "menu.cpp lacks: " + s)
bb = (ROOT / "build.bat").read_text(encoding="utf-8")
check("src\\extraperf.cpp" in bb and "src\\extraperf.asm" in bb and "build\\extraperf_caves.obj" in bb, "build.bat")
check("src/extraperf.cpp src/extraperf.asm" in (ROOT / "CMakeLists.txt").read_text(encoding="utf-8"), "CMakeLists.txt")
ini = (ROOT / "Nioh3EngineFix.ini").read_text(encoding="utf-8", errors="replace")
for k in ("[ExtraPerf]", "Enabled = 1", "Preset = 0", "XCLD = 0", "XFOG = 0", "XGI = 0", "XSHD = 0", "XSHF = 0", "XLOD = 0",
          "XTER = 0", "XPRT = 0", "XAO = 0", "XPOST = 0"):
    check(k in ini, "ini lacks " + k)
lk = (REPO / "tools" / "testmatrix" / "live_keys.py").read_text(encoding="utf-8")
check('"XCLD": "ExtraPerf.XCLD"' in lk and '"XPRE": "ExtraPerf.Preset"' in lk, "live_keys.py does not list the Extra performance rows as live")

print("\n".join("FAIL: " + f for f in fails) if fails else "ALL PASSED")
sys.exit(1 if fails else 0)
