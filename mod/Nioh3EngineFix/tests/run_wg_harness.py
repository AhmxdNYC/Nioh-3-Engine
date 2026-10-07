"""run_wg_harness.py - WG (wake gate) stress test on this PC. Not a game test.

    py -I tests/run_wg_harness.py [--negative]

Extracts the real WG source from src/dllmain.cpp (the state structs/globals, WgIndex, WgWake, WgSleep) into
build/wgtest/*.inc, compiles tests/wg_harness.cpp with MSVC (same toolchain lookup as build.bat: cl on PATH or
..\\..\\tools\\msvc\\setup_x64.bat) and runs it. The harness models the engine's job pools with the v2.02 layout (slot =
workers + i*458h, [slot] = WorkerThread with +8 event and +10h bit0, [slot+440h] = pool, [pool+34h] = id), 8 workers,
3 submitters pushing bursts with a wake-one each, nested submissions from workers; the stock model is free of lost
wake-ups. Pass = every job completes in WG off / WG on (MaxPending 1 and 2) with an unbounded window, and the pending
counters return to 0. --negative builds a broken gate (skips whatever the pending count is) to show the oracle catches
a stall.
"""
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = (ROOT / "src" / "dllmain.cpp").read_text(encoding="utf-8").splitlines()
OUT = ROOT / "build" / "wgtest"
OUT.mkdir(parents=True, exist_ok=True)


def find(pat):
    for i, l in enumerate(SRC):
        if re.match(pat, l):
            return i
    sys.exit("marker not found: " + pat)


g0, g1 = find(r"^constexpr LONG kWgWaiting"), find(r"^volatile LONG64 g_wgSkipped")
e0, e1 = find(r"^// ---- WG \(1\.6\.0\): wake gate\. Worker slot"), find(r"^// ---- DS \(1\.6\.0\): replaces")
glob = "\n".join(SRC[g0:g1 + 1]) + "\n"
ext = "\n".join(SRC[e0:e1]) + "\n"
if "--negative" in sys.argv:
    ext = ext.replace("if (ps.pending >= g_wgMax) {", "if (ps.pending >= 0) {")
(OUT / "wg_globals.inc").write_text(glob, encoding="utf-8")
(OUT / "wg_extract.inc").write_text(ext, encoding="utf-8")
harness = ROOT / "tests" / "wg_harness.cpp"
setup = ROOT.parent.parent / "tools" / "msvc" / "setup_x64.bat"
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
cmd = pre + 'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /I "%s" "%s" /Fo"%s\\\\" /Fe"%s"' % (
    OUT, harness, OUT, OUT / "wg_harness.exe")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode:
    print(r.stdout, r.stderr)
    sys.exit("compile failed")
sys.exit(subprocess.run([str(OUT / "wg_harness.exe")]).returncode)
