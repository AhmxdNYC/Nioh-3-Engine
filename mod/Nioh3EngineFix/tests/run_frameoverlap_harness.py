"""run_frameoverlap_harness.py - FO frame overlap offline test (src/frameoverlap.cpp). Not a game test.

    py -I tests/run_frameoverlap_harness.py

1. Builds tests/frameoverlap_harness.cpp + src/frameoverlap.cpp (/DFO_TEST) and runs it: a fake GfxDevice, a fake replay
   thread replaying a sequence-checked token stream (a token overwritten before replay breaks the sequence), a fake present
   thread, fake sub-streams, a real D3D12 device / queue for the forward fence. Phases: 40 overlapped frames; a frame larger than
   a stream half (stock fallback, then overlap again); live switch off / on; a previous-generation 0xA7 handle while TAIL is
   pending; (fo7) the stock Present modelled in its real order (back-buffer index read before its present spin, hand-off to
   the present thread) with the KtglCache locks (+CF8h shared by the 0x95 handler and a tile-job thread, +CC0h / +CD0h in the
   frame end) and a hang detector: chain jump + forced wrong prediction resynced before the frame replays (0 frames rendered
   into a non-current back buffer), a chain that will not rotate (session OFF), TAIL-issued session OFF, the fo6pc sequence,
   watchdog (5.6 s stall -> restore -> pure stock), partial restore (FoPresent gone, frame-begin hooks still patched),
   re-arm; after every switch: no lock held, no TAIL pending, full stream / sub-stream extents.
2. Source wiring: build.bat / CMakeLists build frameoverlap.cpp, dllmain starts it, routes Tuning keys and logs its status line,
   the ini documents FrameOverlap = 0 and FO_Pct, live_keys.py lists Tuning.FO / Tuning.FO_Pct.
Must end ALL PASSED.
"""
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
REPO = ROOT.parent.parent
OUT = ROOT / "build" / "fotest"
OUT.mkdir(parents=True, exist_ok=True)
fails = []


def check(c, msg):
    if not c:
        fails.append(msg)
    return c


setup = REPO / "tools" / "msvc" / "setup_x64.bat"
if not setup.exists():  # git worktree: tools\msvc is not tracked, use the main checkout's portable MSVC
    setup = pathlib.Path(r"C:\nioh3-engine\tools\msvc\setup_x64.bat")
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
cmd = pre + (
    'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /permissive- /DUNICODE /D_UNICODE /DFO_TEST "{t}\\frameoverlap_harness.cpp" '
    '"{s}\\frameoverlap.cpp" /Fo"{o}\\\\" /Fe"{o}\\frameoverlap_harness.exe" /link kernel32.lib d3d12.lib dxgi.lib'
).format(o=OUT, t=ROOT / "tests", s=ROOT / "src")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-4000:], r.stderr[-2000:])
    sys.exit("compile failed")
warn = [l for l in r.stdout.splitlines() if "warning" in l]
check(not warn, "compiler warnings: %s" % warn[:3])
r = subprocess.run([str(OUT / "frameoverlap_harness.exe")], capture_output=True, text=True, timeout=600, cwd=str(ROOT))
print(r.stdout[-6000:])
check(r.returncode == 0 and "ALL PASSED" in r.stdout, "harness failed")
# the HandleSlotIsNotFreed stub may only write rax / rcx / rdx / r8 (+ flags), exactly like the original 58C434
try:
    from capstone import Cs, CS_ARCH_X86, CS_MODE_64
    from capstone.x86 import X86_OP_REG
    stub = (OUT / "notfreed_stub.bin").read_bytes()
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = True
    allowed = {"rax", "eax", "al", "rcx", "rdx", "r8", "r8b", "rip", "rflags", "rsp"}
    bad = []
    for ins in md.disasm(stub, 0):
        _, written = ins.regs_access()
        names = {ins.reg_name(x) for x in written}
        if names - allowed:
            bad.append("%x %s %s writes %s" % (ins.address, ins.mnemonic, ins.op_str, names - allowed))
        print("  stub: %3x %-6s %s" % (ins.address, ins.mnemonic, ins.op_str))
    check(not bad and len(stub) > 40, "stub writes only rax/rcx/rdx/r8: %s" % bad)
except ImportError:
    check(False, "capstone not installed (py -m pip install capstone)")

src = lambda n: (ROOT / "src" / n).read_text(encoding="utf-8", errors="replace")
bb = (ROOT / "build.bat").read_text(encoding="utf-8", errors="replace")
cm = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8", errors="replace")
ini = (ROOT / "Nioh3EngineFix.ini").read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")
dm = src("dllmain.cpp")
lk = (REPO / "tools" / "testmatrix" / "live_keys.py").read_text(encoding="utf-8")
check("src\\frameoverlap.cpp" in bb and "build\\frameoverlap.obj" in bb, "build.bat compiles and links frameoverlap")
check("src/frameoverlap.cpp" in cm, "CMakeLists lists frameoverlap.cpp")
check('#include "frameoverlap.h"' in dm and "frameoverlap::Start(foi);" in dm, "dllmain starts frameoverlap")
check("if (frameoverlap::SetTuning(key, val, reply, n)) return true;" in dm, "LiveTuning routes FO / FO_Pct")
check("frameoverlap::StatusLine(" in dm, "dllmain logs the FO status line")
check("\nFrameOverlap = 0\n" in ini and "\nFO_Pct = 25\n" in ini, "ini documents FrameOverlap = 0 and FO_Pct = 25")
check('"FO"' in lk and '"FO_Pct"' in lk, "live_keys.py: Tuning.FO / Tuning.FO_Pct live-tuning")
for f in fails:
    print("FAIL:", f)
print("ALL PASSED" if not fails else "FAILED (%d)" % len(fails))
sys.exit(1 if fails else 0)
