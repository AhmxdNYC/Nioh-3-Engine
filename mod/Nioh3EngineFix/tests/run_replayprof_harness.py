"""run_replayprof_harness.py - RPF replay-thread profiler offline test (src/replayprof.cpp + src/replayprof.asm). Not a game test.

    py -I tests/run_replayprof_harness.py [path/to/Nioh3.dump.exe]

1. Builds tests/replayprof_harness.cpp (+ .asm fake dispatcher loop) with src/replayprof.cpp/.asm into build/rpftest and
   runs it: thunk argument / return / stack forwarding, replay-thread filter, nested calls, per-op / per-group accounting,
   idle vs in-handler waits, on/off restore byte-identical, Report() values, probe cost per token / per D3D12 call.
2. v2.02 dump: the byte runs RPF verifies at run time (dispatcher loop 1A7C455 -> table 3C02220 and return 1A7C467, the
   loop's wait 1A7C3FC = call [KERNEL32!WaitForSingleObject import], sub-range replays 11575D / 495F26) and every table
   entry == the handler RVA in src/replayprof_names.h.
3. Source wiring: build.bat / CMakeLists build replayprof, dllmain starts it and routes Tuning.RPF, ini documents
   ReplayProf = 0, live_keys.py lists Tuning.RPF.
Must end ALL PASSED.
"""
import pathlib
import re
import struct
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
REPO = ROOT.parent.parent
OUT = ROOT / "build" / "rpftest"
OUT.mkdir(parents=True, exist_ok=True)
DUMP = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path(r"C:\nioh3-engine\re\Nioh3.dump.exe")
fails = []


def check(c, msg):
    print(("ok: " if c else "FAIL: ") + msg)
    if not c:
        fails.append(msg)
    return c


setup = REPO / "tools" / "msvc" / "setup_x64.bat"
if not setup.exists():  # git worktree: tools\msvc is not tracked, use the main checkout's portable MSVC
    setup = pathlib.Path(r"C:\nioh3-engine\tools\msvc\setup_x64.bat")
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
cmd = pre + (
    'ml64 /nologo /c /W3 /Fo"{o}\\rp.obj" "{s}\\replayprof.asm" && '
    'ml64 /nologo /c /W3 /Fo"{o}\\rph.obj" "{t}\\replayprof_harness.asm" && '
    'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /permissive- /DUNICODE /D_UNICODE "{t}\\replayprof_harness.cpp" '
    '"{s}\\replayprof.cpp" "{o}\\rp.obj" "{o}\\rph.obj" /Fo"{o}\\\\" /Fe"{o}\\replayprof_harness.exe" /link kernel32.lib'
).format(o=OUT, t=ROOT / "tests", s=ROOT / "src")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-6000:], r.stderr[-2000:])
    sys.exit("compile failed")
warn = [l for l in r.stdout.splitlines() if "warning" in l]
check(not warn, "compiler warnings: %s" % warn[:3])
r = subprocess.run([str(OUT / "replayprof_harness.exe")], capture_output=True, text=True, timeout=300)
print("\n".join(l for l in r.stdout.splitlines() if not l.startswith("ok: ")))
check(r.returncode == 0 and "ALL PASSED" in r.stdout, "harness")

# ---- 2. v2.02 dump (raw offset == RVA) ----
if DUMP.exists():
    d = DUMP.read_bytes()
    ib = 0x7FF74F410000  # image base the dump's absolute pointers use (Ghidra base)
    rel = lambda at, end: end + struct.unpack_from("<i", d, at)[0]
    TAB = 0x3C02220
    check(d[0x1A7C455:0x1A7C458] == bytes.fromhex("4C8D05") and rel(0x1A7C458, 0x1A7C45C) == TAB and
          d[0x1A7C45C:0x1A7C467] == bytes.fromhex("488D55D0488BCF41FF14C0"), "dispatcher loop 1A7C455: lea r8,[3C02220] ... call [r8+rax*8], ret 1A7C467")
    check(d[0x11575D:0x115760] == bytes.fromhex("488D3D") and rel(0x115760, 0x115764) == TAB, "sub-range replay 11575D: lea rdi,[3C02220]")
    check(d[0x495F26:0x495F29] == bytes.fromhex("4C8D05") and rel(0x495F29, 0x495F2D) == TAB, "replay 495F26: lea r8,[3C02220]")
    # import slot of KERNEL32!WaitForSingleObject
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    imp_rva = struct.unpack_from("<I", d, pe + 24 + 112 + 8)[0]  # DataDirectory[1]
    slot = None
    o = imp_rva
    while True:
        oft, _, _, name, ft = struct.unpack_from("<IIIII", d, o)
        if not name:
            break
        dll = d[name:d.index(b"\0", name)].decode()
        if dll.lower() == "kernel32.dll":
            k = 0
            while True:
                v = struct.unpack_from("<Q", d, oft + 8 * k)[0]
                if not v:
                    break
                if not v >> 63 and d[v + 2:d.index(b"\0", v + 2)] == b"WaitForSingleObject":
                    slot = ft + 8 * k
                k += 1
        o += 20
    check(slot is not None and d[0x1A7C3FC:0x1A7C3FE] == bytes.fromhex("FF15") and rel(0x1A7C3FE, 0x1A7C402) == slot,
          "dispatcher wait 1A7C3FC: call [KERNEL32!WaitForSingleObject] (slot %s), ret 1A7C402" % (hex(slot) if slot else None))
    names = (ROOT / "src" / "replayprof_names.h").read_text(encoding="utf-8")
    rvas = [int(x, 16) for x in re.findall(r'\{"\w+", 0x([0-9A-F]+)\},  // [0-9A-F]{2}', names)]
    tab = struct.unpack_from("<185Q", d, TAB)
    check(len(rvas) == 185 and all(tab[i] - ib == rvas[i] for i in range(185)), "token table == the 185 named handler RVAs")
else:
    check(False, "dump %s not found" % DUMP)

# ---- 3. wiring ----
src = lambda n: (ROOT / "src" / n).read_text(encoding="utf-8", errors="replace")
bb = (ROOT / "build.bat").read_text(encoding="utf-8", errors="replace")
cm = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8", errors="replace")
ini = (ROOT / "Nioh3EngineFix.ini").read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")
dm = src("dllmain.cpp")
lk = (REPO / "tools" / "testmatrix" / "live_keys.py")
lk = lk.read_text(encoding="utf-8") if lk.exists() else ""
check("src\\replayprof.cpp" in bb and "src\\replayprof.asm" in bb and "build\\replayprof.obj" in bb and "build\\replayprof_thunks.obj" in bb,
      "build.bat compiles and links replayprof (.cpp + .asm)")
check("src/replayprof.cpp" in cm and "src/replayprof.asm" in cm, "CMakeLists lists replayprof.cpp / .asm")
check('#include "replayprof.h"' in dm and "replayprof::Start(rpi);" in dm, "dllmain starts replayprof")
check("if (replayprof::SetTuning(key, val, reply, n)) return true;" in dm, "LiveTuning routes Tuning.RPF")
check("\nReplayProf = 0\n" in ini, "ini documents ReplayProf = 0")
check('"RPF"' in lk, "live_keys.py: Tuning.RPF live-tuning")
print("ALL PASSED" if not fails else "FAILED:\n  " + "\n  ".join(fails))
sys.exit(1 if fails else 0)
