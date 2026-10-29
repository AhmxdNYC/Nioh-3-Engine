r"""run_bootnotice_harness.py - SkipBootNotice offline tests (src/bootnotice.cpp). Not a game test.

    py -I tests/run_bootnotice_harness.py [path/to/Nioh3.dump.exe]

1. Builds tests/bootnotice_harness.cpp + src/bootnotice.cpp (MSVC) and runs it: ini modes (default off for normal play,
   on with Tour / VirtualPad, explicit 0/1/2), only the two 5-byte sites change, the BN1 thunk resets the string instead of
   queueing the notice (log-only mode calls the original), the BN2 thunk turns "too heavy" into 0 and passes 0 through,
   refusal when the bytes differ.
2. Dump: the harness's site bytes equal the dump, BN1/BN2 call E6DA30 / 1FE3D38, the notice text id 0x3C4ABF1 is used
   only at 1C35372 (the notice has no other caller), and E6DA30's last call before its epilogue is StringReset 310C18 on its
   string argument (what the skip does instead).
Must end ALL PASSED.
"""
import pathlib
import struct
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
REPO = ROOT.parent.parent
DUMP = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path(r"C:\nioh3-engine\re\Nioh3.dump.exe")
OUT = ROOT / "build" / "bootnoticetest"
OUT.mkdir(parents=True, exist_ok=True)
fails = []


def check(c, msg):
    if not c:
        fails.append(msg)
    return c


setup = REPO / "tools" / "msvc" / "setup_x64.bat"
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
cmd = pre + (
    'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /permissive- /DUNICODE /D_UNICODE "{t}\\bootnotice_harness.cpp" "{s}\\bootnotice.cpp" '
    '/Fo"{o}\\\\" /Fe"{o}\\bootnotice_harness.exe" /link kernel32.lib user32.lib'
).format(o=OUT, t=ROOT / "tests", s=ROOT / "src")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-4000:], r.stderr[-2000:])
    sys.exit("compile failed")
check(not [l for l in r.stdout.splitlines() if "warning" in l], "compiler warnings:\n" + r.stdout)
r = subprocess.run([str(OUT / "bootnotice_harness.exe")], capture_output=True, text=True, timeout=120)
print(r.stdout[-6000:])
check(r.returncode == 0 and "ALL OK" in r.stdout, "harness failed")

d = DUMP.read_bytes()
src = (ROOT / "tests" / "bootnotice_harness.cpp").read_text()
seq = bytes.fromhex("B9F1ABC403E888F796FE488BD0488D4DB0E8ECAF6DFEBE01000000488BD0448AC6E8988623FF44386F550F85AD010000E891E93A0084C00F84A0010000")
check(d[0x1C35372:0x1C35372 + len(seq)] == seq, "sequencer bytes differ from the dump")
for site, callee in ((0x1C35393, 0xE6DA30), (0x1C353A2, 0x1FE3D38)):
    rel = struct.unpack_from("<i", d, site + 1)[0]
    check(d[site] == 0xE8 and site + 5 + rel == callee, "site %X does not call %X" % (site, callee))
hits = []
i = d.find(bytes.fromhex("F1ABC403"))
while i >= 0:
    hits.append(i)
    i = d.find(bytes.fromhex("F1ABC403"), i + 1)
check(hits == [0x1C35373], "text id 0x3C4ABF1 immediates: %s" % [hex(h) for h in hits])
# E6DA30 tail: ... lea/mov rcx,<param_2 home>; call 310C18 just before the cookie check / epilogue
body = d[0xE6DA30:0xE6DA30 + 2974]
calls = [k for k in range(len(body) - 5) if body[k] == 0xE8 and 0xE6DA30 + k + 5 + struct.unpack_from("<i", body, k + 1)[0] == 0x310C18]
check(len(calls) >= 1, "E6DA30 never calls StringReset 310C18")
check("0xB9, 0xF1, 0xAB, 0xC4, 0x03" in src, "harness lost the sequencer bytes")

if fails:
    print("FAILED:\n  " + "\n  ".join(fails))
    sys.exit(1)
print("ALL PASSED")
