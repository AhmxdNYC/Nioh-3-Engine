r"""run_tour_harness.py - Tour mode offline tests (src/tour_core.h, src/tour.cpp). Not a game test.

    py -I tests/run_tour_harness.py [path/to/Nioh3.dump.exe]

1. Builds tests/tour_harness.cpp + src/tour.cpp (MSVC) into build/tourtest and runs it: TourList parsing, resume file
   (crash point / crash load / stall / second pass), farthest-point sampling, frame-time p50/p99, save-path match, and the real
   thunk bytes: tick thunk passes 5 integer args (one on the stack) and 4 xmm args through while the tick function runs; SB1
   blocked = al 1 + *err 0 + writer not called, SB2 blocked = al 1; switch off = the original runs with every argument.
2. Dump: tour.cpp's head-byte checks (kHeads) equal the dump; TT1/SB1-SB3 are E8 calls to the expected callees; the save-file
   APIs the guard hooks are imported by name from KERNEL32; the user-save writer 5B7BB4 is called only from 5B6F37.
3. Source wiring: dllmain starts tour::Start, group TR is check-only, build.bat / CMakeLists build tour.cpp, the ini documents
   the keys (Tour = 0 by default), the built-in list and tools\tour lists name existing missions (notes\missions.csv).
Must end ALL PASSED.
"""
import csv
import json
import pathlib
import re
import struct
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
REPO = ROOT.parent.parent
DUMP = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path(r"re\Nioh3.dump.exe")
OUT = ROOT / "build" / "tourtest"
OUT.mkdir(parents=True, exist_ok=True)
fails = []


def check(c, msg):
    if not c:
        fails.append(msg)
    return c


# ---- 1. harness ----
setup = REPO / "tools" / "msvc" / "setup_x64.bat"
if not setup.exists():
    setup = pathlib.Path(r"tools\msvc\setup_x64.bat")
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
cmd = pre + (
    'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /permissive- /DUNICODE /D_UNICODE "{t}\\tour_harness.cpp" "{s}\\tour.cpp" '
    '/Fo"{o}\\\\" /Fe"{o}\\tour_harness.exe" /link kernel32.lib user32.lib'
).format(o=OUT, t=ROOT / "tests", s=ROOT / "src")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-4000:], r.stderr[-2000:])
    sys.exit("compile failed")
warn = [l for l in r.stdout.splitlines() if "warning" in l]
check(not warn, "compiler warnings: %s" % warn[:3])
r = subprocess.run([str(OUT / "tour_harness.exe")], capture_output=True, text=True, timeout=300)
print(r.stdout.strip())
check(r.returncode == 0 and "ALL PASSED" in r.stdout, "harness failed")

# ---- 2. dump ----
img = DUMP.read_bytes()
src = (ROOT / "src" / "tour.cpp").read_text(encoding="utf-8")
heads = re.findall(r"\{(k\w+), (\d+), \{([0-9xA-Fa-f, ]+)\}\}", src)
consts = dict((n, int(v, 16)) for n, v in re.findall(r"constexpr uint32_t (k\w+) = (0x[0-9A-Fa-f]+)", src))
consts.update((n, int(v, 16)) for n, v in re.findall(r", (k\w+) = (0x[0-9A-Fa-f]+)", src))
check(len(heads) == 8, "kHeads: %d entries parsed (want 8)" % len(heads))
for name, n, bs in heads:
    rva = consts.get(name)
    want = bytes(int(x, 16) for x in bs.split(","))[: int(n)]
    check(rva is not None and img[rva:rva + len(want)] == want, "head bytes of %s (%s) differ from the dump" % (name, rva and hex(rva)))


def e8_target(site):
    if img[site] != 0xE8:
        return None
    return site + 5 + struct.unpack_from("<i", img, site + 1)[0]


for s, c in (("kTickSite", "kTickCallee"), ("kSb1Site", "kSb1Callee"), ("kSb2Site", "kSbBackupCallee"), ("kSb3Site", "kSbBackupCallee")):
    check(s in consts and c in consts and e8_target(consts[s]) == consts[c], "%s is not an E8 call to %s" % (s, c))
# rip-relative globals named in the heads: FtPost / TickCallee load g_FastTravelSeq
for fn in ("kFtPost", "kTickCallee"):
    a = consts[fn]
    disp = struct.unpack_from("<i", img, a + 10)[0]
    check(a + 14 + disp == consts["kFtSeqPtr"], "%s does not load g_FastTravelSeq %X" % (fn, consts["kFtSeqPtr"]))
# callers of the user-save writer: only 5B6F37 (scan the executable sections for E8 / E9 rel32 to 5B7BB4)
pe = struct.unpack_from("<I", img, 0x3C)[0]
nsec = struct.unpack_from("<H", img, pe + 6)[0]
opt = struct.unpack_from("<H", img, pe + 20)[0]
callers = []
for i in range(nsec):
    o = pe + 24 + opt + i * 40
    va, vs, ch = struct.unpack_from("<I", img, o + 12)[0], struct.unpack_from("<I", img, o + 8)[0], struct.unpack_from("<I", img, o + 36)[0]
    if not ch & 0x20000000:
        continue
    for op in (b"\xE8", b"\xE9"):
        j = img.find(op, va, va + vs)
        while j != -1:
            if j + 5 + struct.unpack_from("<i", img, j + 1)[0] == consts["kSb1Callee"]:
                callers.append(j)
            j = img.find(op, j + 1, va + vs)
check(callers == [consts["kSb1Site"]], "SaveFileWriteAtomic callers %s (want only 5B6F37)" % [hex(c) for c in callers])
# KERNEL32 imports by name for the guard
imp = struct.unpack_from("<I", img, pe + 24 + 112 + 8)[0]
names = set()
d = imp
while True:
    oft, _, _, nm, ft = struct.unpack_from("<IIIII", img, d)
    if not nm:
        break
    dll = img[nm:img.index(b"\0", nm)].decode().upper()
    if dll.startswith("KERNEL32"):
        k = oft
        while True:
            e = struct.unpack_from("<Q", img, k)[0]
            if not e:
                break
            if not e >> 63:
                names.add(img[e + 2:img.index(b"\0", e + 2)].decode())
            k += 8
    d += 20
for api in ("CreateFileW", "MoveFileExW", "MoveFileW", "DeleteFileW", "CopyFileW", "SetFileAttributesW"):
    check(api in names, "KERNEL32!%s not imported by name" % api)
check("CreateFile2" not in names and "ReplaceFileW" not in names, "another file-write API is imported (CreateFile2 / ReplaceFileW)")

# ---- 3. wiring ----
dm = (ROOT / "src" / "dllmain.cpp").read_text(encoding="utf-8")
check('#include "tour.h"' in dm and "tour::Start(ti);" in dm, "dllmain does not start tour")
check('if (!strcmp(g, "TR")) return false;' in dm, "group TR not check-only in dllmain")
pj = json.loads((ROOT / "patches.json").read_text(encoding="utf-8"))
sites = {s["id"]: s for s in pj["sites"]}
for sid, rva in (("TT1", 0x56529C), ("SB1", 0x5B6F37), ("SB2", 0x1FDF2D0), ("SB3", 0x1FDF62A)):
    check(sid in sites and int(sites[sid]["rva"], 16) == rva and sites[sid]["kind"] == "check" and sites[sid]["group"] == "TR",
          "patches.json %s" % sid)
bb = (ROOT / "build.bat").read_text(encoding="utf-8")
check("src\\tour.cpp" in bb and "build\\tour.obj" in bb, "build.bat does not build / link tour.cpp")
check("src/tour.cpp" in (ROOT / "CMakeLists.txt").read_text(encoding="utf-8"), "CMakeLists misses tour.cpp")
ini = (ROOT / "Nioh3EngineFix.ini").read_text(encoding="utf-8")
for k in ("Tour = 0", "TourDwellSec", "TourPointsPerStage", "TourList", "TourPasses", "TourLoadTimeoutSec"):
    check(re.search(r"^\s*" + re.escape(k.split(" ")[0]) + r"\s*=", ini, re.M) is not None, "ini misses %s" % k)
check(re.search(r"^\s*Tour\s*=\s*0\s*$", ini, re.M) is not None, "ini default is not Tour = 0")
# mission keys
mcsv = REPO / "notes" / "missions.csv"
rows = list(csv.DictReader(mcsv.open(encoding="utf-8")))
keys = {int(x["mission_key_u16(+158)"]): x for x in rows}
builtin = [int(k) for k in re.findall(r'^\s*"(\d+)\s+#', src, re.M)]
check(len(builtin) == 15, "built-in list has %d entries (want 15)" % len(builtin))
for k in builtin:
    check(k in keys and keys[k]["class"] == "main", "built-in key %d is not a main mission" % k)
for lf in sorted((REPO / "tools" / "tour").glob("tourlist_*.txt")):
    for ln in lf.read_text(encoding="utf-8").splitlines():
        ln = ln.split("#")[0].strip()
        if not ln:
            continue
        k = int(ln.split("@")[0], 0)
        check(k in keys and keys[k]["name_en"], "%s: key %d is not a named mission" % (lf.name, k))

if fails:
    for f in fails:
        print("FAIL:", f)
    sys.exit(1)
print("ALL PASSED (harness + dump + wiring)")
