r"""run_rematch_harness.py - boss rematch offline tests (src/rematch.cpp, ASI 1.7.40). Not a game test.

    py -I tests/run_rematch_harness.py

1. Builds tests/rematch_harness.cpp (includes src/rematch.cpp) with MSVC /W4 into build/rematchtest and runs it on a fake image:
   thunk chaining onto an earlier (bosslog-like) hook of the same entry (order, args, result, only the rel32 rewritten), probe
   refusals, catalogue parsing (+ the real Nioh3EngineFix_bosses.tsv), every s.2 gate, the cleared-mission scan and the sub-page
   rows, R-a with (0, 0, 6), HiddenDuels un-hide + restore on menu close / mission exit / abort / timeout, refusals (fight, busy
   menu), R-b direct start (MissionRequestById(id, slot byte, 0, 16-aligned {101,1000000,10100,0,0,0}) -> a slot with mode 2;
   refusals: not cleared / unverified / queue full / not taken).
2. Wiring: dllmain starts rematch::Start after bosslog::Start, build.bat / CMakeLists build rematch.cpp, build.bat copies the
   catalogue, the ini documents [Rematch] Enable = 0 / DirectStart = 0, the catalogue is in sync with tools/rematch_catalog.py.
3. Dump pin (re\Nioh3.dump.exe): the request MissionRequestById builds (223965C..22396B6), the replay confirm's inline slot write
   (F1FE98..) and its params (F1FDD4..) are still the bytes the R-b equivalence was read from (notes\session-2026-10-08-gamedata.md).
Must end ALL PASSED.
"""
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
REPO = ROOT.parent.parent
OUT = ROOT / "build" / "rematchtest"
OUT.mkdir(parents=True, exist_ok=True)
fails = []


def check(c, msg):
    if not c:
        fails.append(msg)
    return c


setup = REPO / "tools" / "msvc" / "setup_x64.bat"
if not setup.exists():
    setup = pathlib.Path(r"C:\Users\Ahmad Hamza\nioh3-engine\tools\msvc\setup_x64.bat")
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
cmd = pre + (
    'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /permissive- /DUNICODE /D_UNICODE "{t}\\rematch_harness.cpp" '
    '/Fo"{o}\\\\" /Fe"{o}\\rematch_harness.exe" /link kernel32.lib user32.lib'
).format(o=OUT, t=ROOT / "tests")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-4000:], r.stderr[-2000:])
    sys.exit("compile failed")
warn = [l for l in r.stdout.splitlines() if "warning" in l]
check(not warn, "compiler warnings: %s" % warn[:3])
r = subprocess.run([str(OUT / "rematch_harness.exe"), str(ROOT / "Nioh3EngineFix_bosses.tsv")], capture_output=True, text=True, timeout=300)
out = r.stdout.strip()
print("\n".join(l for l in out.splitlines() if not l.startswith("  log:"))[-6000:])
check(r.returncode == 0 and "ALL PASSED" in out, "harness failed")

dm = (ROOT / "src" / "dllmain.cpp").read_text(encoding="utf-8")
ib, ir = dm.find("bosslog::Start(bi)"), dm.find("rematch::Start(ri)")
check(ib > 0 and ir > ib, "dllmain: rematch::Start must come after bosslog::Start")
check("ri.pauseReason = &menu::GamePauseReason" in dm, "dllmain: pauseReason wired")
bb = (ROOT / "build.bat").read_text(encoding="utf-8")
check("src\\rematch.cpp" in bb and "build\\rematch.obj" in bb and "Nioh3EngineFix_bosses.tsv" in bb, "build.bat: rematch.cpp / obj / catalogue copy")
check("src/rematch.cpp" in (ROOT / "CMakeLists.txt").read_text(encoding="utf-8"), "CMakeLists: rematch.cpp")
ini = (ROOT / "Nioh3EngineFix.ini").read_text(encoding="utf-8")
sec = ini[ini.find("[Rematch]"):]
check("[Rematch]" in ini and re.search(r"(?m)^Enable = 0", sec) and re.search(r"(?m)^DirectStart = 0", sec)
      and re.search(r"(?m)^HiddenDuels = 0", sec), "ini: [Rematch] Enable / HiddenDuels / DirectStart = 0")
r = subprocess.run([sys.executable, "-I", str(REPO / "tools" / "rematch_catalog.py"), "--check"], capture_output=True, text=True)
print(r.stdout.strip())
check(r.returncode == 0, "catalogue out of date (py -I tools/rematch_catalog.py)")

# 3. R-b equivalence pin: both request writers unchanged in the dump
DUMP = pathlib.Path(r"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe")
if check(DUMP.exists(), "dump not found: %s" % DUMP):
    import struct
    d = DUMP.read_bytes()
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    ns, osz = struct.unpack_from("<H", d, pe + 6)[0], struct.unpack_from("<H", d, pe + 20)[0]
    secs = [struct.unpack_from("<IIII", d, pe + 24 + osz + i * 40 + 8) for i in range(ns)]

    def at(rva, n):
        for vs, va, rs, ra in secs:
            if va <= rva < va + max(vs, rs):
                return d[ra + rva - va: ra + rva - va + n]
        return b""
    pins = {
        # MissionRequestById: movaps [r14] params, +0x10 u16 0, +0x14 u64 2, +0x1C 0, +0x20 0, +0x24 1, +0x28 u16 1, +0x38 0, +0x3C 0,
        # +0 slot, +8 region, +4 id, +0xC stage, call MissionRequestEnqueue
        0x223965C: "41 0f 28 06 48 8d 55 a0 f2 41 0f 10 4e 10 48 83 4d d0 ff 0f 29 45 e0 f2 0f 11 4d f0 66 44 89 65 b0 48 c7 45 b4 02 00 00 00 "
                   "44 88 65 bc 44 89 65 c0 c7 45 c4 01 00 00 00 66 c7 45 c8 01 00 44 89 65 d8 66 44 89 65 dc 44 88 7d a0 40 88 75 a8 89 7d a4 "
                   "89 5d ac e8 86 fe ff ff",
        # F1FD80 (replay confirm, result 0): the same fields written straight into queue slot r8 (45BC420 + r8 * 0x58)
        0xF1FE98: "49 6b c8 58 48 8d 15 7d c5 69 03 44 88 34 11 89 74 11 04 40 88 7c 11 08 44 89 7c 11 0c 45 33 ff 66 44 89 7c 11 10 48 c7 44 "
                  "11 14 02 00 00 00 44 88 7c 11 1c",
        # F1FD80 params on its stack: {101h, 1000000h, 10100h, 0, ...}
        0xF1FDD4: "c7 44 24 30 01 01 00 00 c7 44 24 34 00 00 00 01 48 c7 44 24 38 00 01 01 00",
    }
    for rva, hx in pins.items():
        want = bytes.fromhex(hx)
        check(at(rva, len(want)) == want, "R-b pin: dump bytes at 0x%X changed" % rva)
    print("R-b pins: %d request-writer byte runs checked against the dump" % len(pins))

print()
if fails:
    print("FAILED (%d):" % len(fails))
    for f in fails:
        print("  - " + f)
    sys.exit(1)
print("ALL PASSED")
