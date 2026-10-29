"""run_cbs_bench.py - CBS micro-benchmark (tests/cbs_bench.cpp): memcpy vs CbuStreamCopy into a mapped D3D12 UPLOAD buffer on this PC.
ShaderVisibleDescRingAlloc 1192B0 bytes from re\\Nioh3.dump.exe, relocated into the test process. Not a game test.

    py -I tests/run_cbvupload_harness.py [trace.bin ...]

trace.bin = a CbvRecord capture (Nioh3EngineFix_cbvtrace.bin from a game run with [Profiler] CbvRecord = N). Builds
tests/cbvupload_harness.cpp + src/cbvupload.cpp (MSVC: cl on PATH or ..\\..\\tools\\msvc\\setup_x64.bat) into
build/cbsbench and runs it. Must end ALL PASSED. See cbvupload_harness.cpp.
"""
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "build" / "cbsbench"
OUT.mkdir(parents=True, exist_ok=True)
REPO = ROOT.parent.parent
DUMP = REPO / "re" / "Nioh3.dump.exe"
if not DUMP.exists():  # worktrees: the dump lives in the main checkout only
    DUMP = pathlib.Path(r"C:\nioh3-engine\re\Nioh3.dump.exe")
setup = REPO / "tools" / "msvc" / "setup_x64.bat"
if not setup.exists():
    setup = pathlib.Path(r"C:\nioh3-engine\tools\msvc\setup_x64.bat")
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
cmd = pre + (
    'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /DCBU_HARNESS "{t}\\cbs_bench.cpp" "{s}\\cbvupload.cpp" "{s}\\cbvstage.cpp" '
    '/Fo"{o}\\\\" /Fe"{o}\\cbs_bench.exe" /link kernel32.lib d3d12.lib dxgi.lib'
).format(o=OUT, s=ROOT / "src", t=ROOT / "tests")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-6000:], r.stderr[-2000:])
    sys.exit("compile failed")
sys.exit(subprocess.run([str(OUT / "cbs_bench.exe"), str(DUMP)] + sys.argv[1:]).returncode)
